// Miles Sound System API subset used by MilesLib, implemented on miniaudio for Android.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "../platform/m2platform.h"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_FLAC
#include "miniaudio/miniaudio.h"

#include "mss.h"

#define LOGI(...) M2Plat::Log(M2Plat::LOG_INFO, "Metin2Audio", __VA_ARGS__)

namespace
{
	const int c_iListener = 0x7fff0001;
	const int c_iProvider = 1;
	const int c_iDriver = 1;

	struct Voice
	{
		bool bUsed = false;
		bool bLoaded = false;
		bool bPaused = false;
		bool bSpatial = false;
		ma_decoder kDecoder;
		ma_sound kSound;
		S32 iLoopCount = 1;
		float fVolume = 1.0f;
		float fPan = 0.5f;
		void* pOwned = nullptr;
	};

	ma_engine g_kEngine;
	bool g_bEngine = false;
	bool g_bEngineFailed = false;
	std::vector<Voice*> g_kVoices;
	std::unordered_map<const void*, U32> g_kSizes;
	std::mutex g_kSizesLock;
	const char* g_szError = "No Error";

	AIL_file_open_callback g_pfnOpen = nullptr;
	AIL_file_close_callback g_pfnClose = nullptr;
	AIL_file_seek_callback g_pfnSeek = nullptr;
	AIL_file_read_callback g_pfnRead = nullptr;

	bool EnsureEngine()
	{
		if (g_bEngine)
			return true;
		if (g_bEngineFailed)
			return false;
		ma_result r = ma_engine_init(NULL, &g_kEngine);
		if (r != MA_SUCCESS)
		{
			g_bEngineFailed = true;
			g_szError = "miniaudio engine init failed";
			LOGI("engine init failed: %d", r);
			return false;
		}
		g_bEngine = true;
		LOGI("engine started: %u Hz, %u ch", ma_engine_get_sample_rate(&g_kEngine), ma_engine_get_channels(&g_kEngine));
		return true;
	}

	int NewVoice(bool bSpatial)
	{
		if (!EnsureEngine())
			return 0;
		for (size_t i = 0; i < g_kVoices.size(); ++i)
		{
			if (!g_kVoices[i]->bUsed)
			{
				g_kVoices[i]->bUsed = true;
				g_kVoices[i]->bSpatial = bSpatial;
				return (int)i + 1;
			}
		}
		Voice* pVoice = new Voice;
		pVoice->bUsed = true;
		pVoice->bSpatial = bSpatial;
		g_kVoices.push_back(pVoice);
		return (int)g_kVoices.size();
	}

	Voice* Get(int h)
	{
		if (h <= 0 || h > (int)g_kVoices.size() || !g_kVoices[h - 1]->bUsed)
			return nullptr;
		return g_kVoices[h - 1];
	}

	void Unload(Voice* v)
	{
		if (v->bLoaded)
		{
			ma_sound_uninit(&v->kSound);
			ma_decoder_uninit(&v->kDecoder);
			v->bLoaded = false;
		}
		if (v->pOwned)
		{
			free(v->pOwned);
			v->pOwned = nullptr;
		}
		v->bPaused = false;
	}

	void Release(int h)
	{
		Voice* v = Get(h);
		if (!v)
			return;
		Unload(v);
		v->bUsed = false;
		v->iLoopCount = 1;
		v->fVolume = 1.0f;
		v->fPan = 0.5f;
	}

	bool Load(Voice* v, const void* data, size_t size)
	{
		Unload(v);
		if (!data || !size)
		{
			g_szError = "empty sound data";
			return false;
		}
		ma_decoder_config kDecoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
		if (ma_decoder_init_memory(data, size, &kDecoderConfig, &v->kDecoder) != MA_SUCCESS)
		{
			g_szError = "unsupported sound data";
			return false;
		}
		ma_uint32 uFlags = v->bSpatial ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
		if (ma_sound_init_from_data_source(&g_kEngine, &v->kDecoder, uFlags, NULL, &v->kSound) != MA_SUCCESS)
		{
			ma_decoder_uninit(&v->kDecoder);
			g_szError = "sound init failed";
			return false;
		}
		v->bLoaded = true;
		ma_sound_set_volume(&v->kSound, v->fVolume);
		if (!v->bSpatial)
			ma_sound_set_pan(&v->kSound, v->fPan * 2.0f - 1.0f);
		return true;
	}

	S32 Status(int h)
	{
		Voice* v = Get(h);
		if (!v || !v->bLoaded)
			return SMP_DONE;
		if (ma_sound_is_playing(&v->kSound))
			return SMP_PLAYING;
		return v->bPaused ? SMP_STOPPED : SMP_DONE;
	}

	void Start(int h)
	{
		Voice* v = Get(h);
		if (!v || !v->bLoaded)
			return;
		ma_sound_set_looping(&v->kSound, v->iLoopCount == 0);
		ma_sound_seek_to_pcm_frame(&v->kSound, 0);
		ma_sound_start(&v->kSound);
		v->bPaused = false;
	}

	void Pause(int h)
	{
		Voice* v = Get(h);
		if (!v || !v->bLoaded)
			return;
		ma_sound_stop(&v->kSound);
		v->bPaused = true;
	}

	void Resume(int h)
	{
		Voice* v = Get(h);
		if (!v || !v->bLoaded || !v->bPaused)
			return;
		ma_sound_start(&v->kSound);
		v->bPaused = false;
	}

	void End(int h)
	{
		Voice* v = Get(h);
		if (!v || !v->bLoaded)
			return;
		ma_sound_stop(&v->kSound);
		v->bPaused = false;
	}

	void SetVolume(int h, float fVolume)
	{
		Voice* v = Get(h);
		if (!v)
			return;
		v->fVolume = fVolume;
		if (v->bLoaded)
			ma_sound_set_volume(&v->kSound, fVolume);
	}

	void* Track(void* p, U32 size)
	{
		std::lock_guard<std::mutex> kLock(g_kSizesLock);
		g_kSizes[p] = size;
		return p;
	}

	U32 TrackedSize(const void* p)
	{
		std::lock_guard<std::mutex> kLock(g_kSizesLock);
		auto it = g_kSizes.find(p);
		return it == g_kSizes.end() ? 0 : it->second;
	}

	void* CopyFile(const void* data, U32 size, U32* outSize)
	{
		void* p = malloc(size);
		if (!p)
			return nullptr;
		memcpy(p, data, size);
		*outSize = size;
		return Track(p, size);
	}

	U32 RiffSize(const void* data)
	{
		const unsigned char* b = (const unsigned char*)data;
		if (memcmp(b, "RIFF", 4) != 0)
			return 0;
		return (U32)(b[4] | (b[5] << 8) | (b[6] << 16) | (b[7] << 24)) + 8;
	}

	void* ReadWholeFile(const char* filename, U32* outSize)
	{
		*outSize = 0;
		if (g_pfnOpen)
		{
			U32 uHandle = 0;
			if (!g_pfnOpen(filename, &uHandle))
				return nullptr;
			S32 iSize = g_pfnSeek(uHandle, 0, 2);
			g_pfnSeek(uHandle, 0, 0);
			void* p = iSize > 0 ? malloc(iSize) : nullptr;
			if (p)
				g_pfnRead(uHandle, p, (U32)iSize);
			g_pfnClose(uHandle);
			if (p)
				*outSize = (U32)iSize;
			return p;
		}
		FILE* fp = fopen(filename, "rb");
		if (!fp)
			return nullptr;
		fseek(fp, 0, SEEK_END);
		long lSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		void* p = lSize > 0 ? malloc(lSize) : nullptr;
		if (p && fread(p, 1, lSize, fp) == (size_t)lSize)
			*outSize = (U32)lSize;
		else
		{
			free(p);
			p = nullptr;
		}
		fclose(fp);
		return p;
	}
}

void AIL_startup()
{
	EnsureEngine();
}

void AIL_shutdown()
{
}

void AIL_set_redist_directory(const char*)
{
}

void AIL_set_file_callbacks(AIL_file_open_callback o, AIL_file_close_callback c, AIL_file_seek_callback s, AIL_file_read_callback r)
{
	g_pfnOpen = o;
	g_pfnClose = c;
	g_pfnSeek = s;
	g_pfnRead = r;
}

void* AIL_file_read(const char* filename, int)
{
	U32 uSize = 0;
	void* pData = ReadWholeFile(filename, &uSize);
	if (!pData)
	{
		g_szError = "file not found";
		return nullptr;
	}
	U32* pOut = (U32*)malloc(uSize + sizeof(U32));
	if (!pOut)
	{
		free(pData);
		return nullptr;
	}
	pOut[0] = uSize;
	memcpy(pOut + 1, pData, uSize);
	free(pData);
	Track(pOut + 1, uSize);
	return pOut;
}

S32 AIL_file_type(const void* data, U32 size)
{
	const unsigned char* b = (const unsigned char*)data;
	if (size >= 22 && memcmp(b, "RIFF", 4) == 0 && memcmp(b + 8, "WAVE", 4) == 0)
	{
		// The fmt chunk normally follows the RIFF header directly.
		unsigned uFormat = b[20] | (b[21] << 8);
		return uFormat == 1 || uFormat == 3 ? AILFILETYPE_PCM_WAV : AILFILETYPE_ADPCM_WAV;
	}
	if (size >= 3 && (memcmp(b, "ID3", 3) == 0 || (b[0] == 0xFF && (b[1] & 0xE0) == 0xE0)))
		return AILFILETYPE_MPEG_L3_AUDIO;
	return AILFILETYPE_UNKNOWN;
}

void AIL_WAV_info(const void* data, AILSOUNDINFO* info)
{
	memset(info, 0, sizeof(*info));
	info->initial_ptr = (void*)data;
	info->data_ptr = (void*)data;
	info->data_len = RiffSize(data);
}

S32 AIL_decompress_ADPCM(const AILSOUNDINFO* info, void** out, U32* outSize)
{
	*out = CopyFile(info->initial_ptr, info->data_len, outSize);
	return *out != nullptr;
}

S32 AIL_decompress_ASI(const void* data, U32 size, const char*, void** out, U32* outSize, void*)
{
	*out = CopyFile(data, size, outSize);
	return *out != nullptr;
}

void AIL_mem_free_lock(void* p)
{
	if (!p)
		return;
	std::lock_guard<std::mutex> kLock(g_kSizesLock);
	if (g_kSizes.erase((U32*)p + 1))
		free(p);
	else
	{
		g_kSizes.erase(p);
		free(p);
	}
}

const char* AIL_last_error()
{
	return g_szError;
}

void AIL_set_paused(int paused)
{
	if (!g_bEngine)
		return;
	if (paused)
		ma_engine_stop(&g_kEngine);
	else
		ma_engine_start(&g_kEngine);
}

HDIGDRIVER AIL_open_digital_driver(U32, S32, S32, U32)
{
	return EnsureEngine() ? c_iDriver : 0;
}

void AIL_close_digital_driver(HDIGDRIVER)
{
}

HSAMPLE AIL_allocate_sample_handle(HDIGDRIVER)
{
	return NewVoice(false);
}

void AIL_release_sample_handle(HSAMPLE s) { Release(s); }

void AIL_init_sample(HSAMPLE s)
{
	if (Voice* v = Get(s))
		Unload(v);
}

S32 AIL_set_sample_file(HSAMPLE s, const void* data, S32 size)
{
	Voice* v = Get(s);
	return v && Load(v, data, (size_t)size);
}

S32 AIL_sample_status(HSAMPLE s) { return Status(s); }

void AIL_set_sample_loop_count(HSAMPLE s, S32 count)
{
	if (Voice* v = Get(s))
		v->iLoopCount = count;
}

void AIL_start_sample(HSAMPLE s) { Start(s); }
void AIL_stop_sample(HSAMPLE s) { Pause(s); }
void AIL_resume_sample(HSAMPLE s) { Resume(s); }
void AIL_end_sample(HSAMPLE s) { End(s); }

void AIL_sample_volume_pan(HSAMPLE s, float* volume, float* pan)
{
	Voice* v = Get(s);
	if (volume)
		*volume = v ? v->fVolume : 0.0f;
	if (pan)
		*pan = v ? v->fPan : 0.5f;
}

void AIL_set_sample_volume_pan(HSAMPLE s, float volume, float pan)
{
	Voice* v = Get(s);
	if (!v)
		return;
	v->fPan = pan;
	SetVolume(s, volume);
	if (v->bLoaded)
		ma_sound_set_pan(&v->kSound, pan * 2.0f - 1.0f);
}

S32 AIL_enumerate_3D_providers(HPROENUM* next, HPROVIDER* provider, char** name)
{
	if (*next != HPROENUM_FIRST)
		return 0;
	*next = HPROENUM_NEXT;
	*provider = c_iProvider;
	*name = (char*)"Miles Fast 2D Positional Audio";
	return 1;
}

S32 AIL_open_3D_provider(HPROVIDER) { return EnsureEngine() ? M3D_NOERR : 1; }
void AIL_close_3D_provider(HPROVIDER) {}
H3DPOBJECT AIL_open_3D_listener(HPROVIDER) { return c_iListener; }
void AIL_close_3D_listener(H3DPOBJECT) {}

H3DSAMPLE AIL_allocate_3D_sample_handle(HPROVIDER) { return NewVoice(true); }
void AIL_release_3D_sample_handle(H3DSAMPLE s) { Release(s); }

S32 AIL_set_3D_sample_file(H3DSAMPLE s, const void* data)
{
	Voice* v = Get(s);
	U32 uSize = TrackedSize(data);
	if (!uSize)
	{
		g_szError = "unknown 3D sample buffer";
		return 0;
	}
	return v && Load(v, data, uSize);
}

S32 AIL_3D_sample_status(H3DSAMPLE s) { return Status(s); }

void AIL_set_3D_sample_loop_count(H3DSAMPLE s, S32 count)
{
	if (Voice* v = Get(s))
		v->iLoopCount = count;
}

void AIL_start_3D_sample(H3DSAMPLE s) { Start(s); }
void AIL_stop_3D_sample(H3DSAMPLE s) { Pause(s); }
void AIL_resume_3D_sample(H3DSAMPLE s) { Resume(s); }
void AIL_end_3D_sample(H3DSAMPLE s) { End(s); }

float AIL_3D_sample_volume(H3DSAMPLE s)
{
	Voice* v = Get(s);
	return v ? v->fVolume : 0.0f;
}

void AIL_set_3D_sample_volume(H3DSAMPLE s, float volume) { SetVolume(s, volume); }

void AIL_set_3D_position(H3DPOBJECT obj, float x, float y, float z)
{
	if (obj == c_iListener)
	{
		if (g_bEngine)
			ma_engine_listener_set_position(&g_kEngine, 0, x, y, z);
		return;
	}
	Voice* v = Get(obj);
	if (v && v->bLoaded)
		ma_sound_set_position(&v->kSound, x, y, z);
}

void AIL_set_3D_orientation(H3DPOBJECT obj, float fx, float fy, float fz, float ux, float uy, float uz)
{
	if (obj != c_iListener || !g_bEngine)
		return;
	ma_engine_listener_set_direction(&g_kEngine, 0, fx, fy, fz);
	ma_engine_listener_set_world_up(&g_kEngine, 0, ux, uy, uz);
}

void AIL_set_3D_velocity(H3DPOBJECT, float, float, float, float) {}
void AIL_auto_update_3D_position(H3DPOBJECT, S32) {}
void AIL_update_3D_position(H3DPOBJECT, float) {}

HSTREAM AIL_open_stream(HDIGDRIVER, const char* filename, S32)
{
	U32 uSize = 0;
	void* pData = ReadWholeFile(filename, &uSize);
	if (!pData)
	{
		g_szError = "stream file not found";
		LOGI("stream not found: %s", filename);
		return 0;
	}
	int h = NewVoice(false);
	Voice* v = Get(h);
	if (!v || !Load(v, pData, uSize))
	{
		free(pData);
		Release(h);
		return 0;
	}
	v->pOwned = pData;
	return h;
}

void AIL_close_stream(HSTREAM s) { Release(s); }

S32 AIL_stream_status(HSTREAM s)
{
	return Status(s) == SMP_DONE ? -1 : SMP_PLAYING;
}

void AIL_set_stream_loop_count(HSTREAM s, S32 count)
{
	if (Voice* v = Get(s))
		v->iLoopCount = count;
}

void AIL_start_stream(HSTREAM s) { Start(s); }

void AIL_pause_stream(HSTREAM s, S32 onoff)
{
	if (onoff)
		Pause(s);
	else
		Resume(s);
}

void AIL_stream_volume_levels(HSTREAM s, float* left, float* right)
{
	Voice* v = Get(s);
	if (left)
		*left = v ? v->fVolume : 0.0f;
	if (right)
		*right = v ? v->fVolume : 0.0f;
}

void AIL_set_stream_volume_levels(HSTREAM s, float left, float)
{
	SetVolume(s, left);
}
