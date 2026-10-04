#ifndef __MSS_H__
#define __MSS_H__

// Android implementation of the Miles Sound System API subset the client uses
#ifdef __ANDROID__
typedef unsigned int U32;
typedef int S32;
typedef unsigned short U16;
typedef short S16;
typedef unsigned char U8;
typedef signed char S8;
typedef float F32;
typedef int HPROVIDER;
typedef int H3DPOBJECT;
#define AILCALLBACK
#define FILE_READ_WITH_SIZE 1
typedef int HSAMPLE;
typedef int HSTREAM;
typedef int H3DSAMPLE;
typedef int HDIGDRIVER;
typedef int HPROENUM;
typedef U32 (*AIL_file_open_callback)(char const* filename, U32* file_handle);
typedef void (*AIL_file_close_callback)(U32 file_handle);
typedef S32 (*AIL_file_seek_callback)(U32 file_handle, S32 offset, U32 type);
typedef U32 (*AIL_file_read_callback)(U32 file_handle, void* buffer, U32 bytes);
struct _AILSOUNDINFO;

// Implemented on miniaudio in mss_android.cpp. Sample data handed to the sample functions
// is always a complete WAV/MP3 file image; decoding happens at playback.
void AIL_startup();
void AIL_shutdown();
void AIL_set_redist_directory(const char* dir);
void AIL_set_file_callbacks(AIL_file_open_callback o, AIL_file_close_callback c, AIL_file_seek_callback s, AIL_file_read_callback r);
void* AIL_file_read(const char* filename, int flags);
S32 AIL_file_type(const void* data, U32 size);
void AIL_WAV_info(const void* data, struct _AILSOUNDINFO* info);
S32 AIL_decompress_ADPCM(const struct _AILSOUNDINFO* info, void** out, U32* outSize);
S32 AIL_decompress_ASI(const void* data, U32 size, const char* filename, void** out, U32* outSize, void* callback);
void AIL_mem_free_lock(void* p);
const char* AIL_last_error();
void AIL_android_set_paused(int paused);

HDIGDRIVER AIL_open_digital_driver(U32 rate, S32 bits, S32 channels, U32 flags);
void AIL_close_digital_driver(HDIGDRIVER driver);

HSAMPLE AIL_allocate_sample_handle(HDIGDRIVER driver);
void AIL_release_sample_handle(HSAMPLE s);
void AIL_init_sample(HSAMPLE s);
S32 AIL_set_sample_file(HSAMPLE s, const void* data, S32 size);
S32 AIL_sample_status(HSAMPLE s);
void AIL_set_sample_loop_count(HSAMPLE s, S32 count);
void AIL_start_sample(HSAMPLE s);
void AIL_stop_sample(HSAMPLE s);
void AIL_resume_sample(HSAMPLE s);
void AIL_end_sample(HSAMPLE s);
void AIL_sample_volume_pan(HSAMPLE s, float* volume, float* pan);
void AIL_set_sample_volume_pan(HSAMPLE s, float volume, float pan);

S32 AIL_enumerate_3D_providers(HPROENUM* next, HPROVIDER* provider, char** name);
S32 AIL_open_3D_provider(HPROVIDER provider);
void AIL_close_3D_provider(HPROVIDER provider);
H3DPOBJECT AIL_open_3D_listener(HPROVIDER provider);
void AIL_close_3D_listener(H3DPOBJECT listener);
H3DSAMPLE AIL_allocate_3D_sample_handle(HPROVIDER provider);
void AIL_release_3D_sample_handle(H3DSAMPLE s);
S32 AIL_set_3D_sample_file(H3DSAMPLE s, const void* data);
S32 AIL_3D_sample_status(H3DSAMPLE s);
void AIL_set_3D_sample_loop_count(H3DSAMPLE s, S32 count);
void AIL_start_3D_sample(H3DSAMPLE s);
void AIL_stop_3D_sample(H3DSAMPLE s);
void AIL_resume_3D_sample(H3DSAMPLE s);
void AIL_end_3D_sample(H3DSAMPLE s);
float AIL_3D_sample_volume(H3DSAMPLE s);
void AIL_set_3D_sample_volume(H3DSAMPLE s, float volume);
void AIL_set_3D_position(H3DPOBJECT obj, float x, float y, float z);
void AIL_set_3D_orientation(H3DPOBJECT obj, float fx, float fy, float fz, float ux, float uy, float uz);
void AIL_set_3D_velocity(H3DPOBJECT obj, float dx, float dy, float dz, float magnitude);
void AIL_auto_update_3D_position(H3DPOBJECT obj, S32 enable);
void AIL_update_3D_position(H3DPOBJECT obj, float elapsed);

HSTREAM AIL_open_stream(HDIGDRIVER driver, const char* filename, S32 memory);
void AIL_close_stream(HSTREAM s);
S32 AIL_stream_status(HSTREAM s);
void AIL_set_stream_loop_count(HSTREAM s, S32 count);
void AIL_start_stream(HSTREAM s);
void AIL_pause_stream(HSTREAM s, S32 onoff);
void AIL_stream_volume_levels(HSTREAM s, float* left, float* right);
void AIL_set_stream_volume_levels(HSTREAM s, float left, float right);

#define SMP_DONE 0
#define SMP_PLAYING 1
#define SMP_STOPPED 2
#define SMP_FREE 3

#define SMP_PLAYMULTI 1
#define AIL_FILE_ERROR 0

#define AILFILETYPE_PCM_WAV 1
#define AILFILETYPE_ADPCM_WAV 2
#define AILFILETYPE_MPEG_L3_AUDIO 5
#define AILFILETYPE_MPEG_L2_AUDIO 6
#define AILFILETYPE_MPEG_L1_AUDIO 7
#define AILFILETYPE_RAW_L16 9
#define AILFILETYPE_VOC 12
#define AILFILETYPE_VOX 13
#define AILFILETYPE_AIFF 14
#define AILFILETYPE_XM 15
#define AILFILETYPE_MOD 16
#define AILFILETYPE_S3M 17
#define AILFILETYPE_IT 18
#define AILFILETYPE_SPX 19
#define AILFILETYPE_UNKNOWN 0

typedef struct _AILSOUNDINFO {
    S32 format;
    void* data_ptr;
    U32 data_len;
    U32 rate;
    S32 bits;
    S32 channels;
    U32 samples;
    U32 block_size;
    void* initial_ptr;
} AILSOUNDINFO;

#define HPROENUM_FIRST 0
#define HPROENUM_NEXT 1
#define M3D_NOERR 0

#else
// Windows MSS includes
// #include <mss.h>
#endif

#endif
