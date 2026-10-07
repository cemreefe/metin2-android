// Platform port: every OS/backend seam the engine core calls lives here.
// Each adapter (android, web) provides these functions; the core never
// includes platform SDK headers directly.
#pragma once

// M2_PORT marks a build running on the platform-port layer (Android, web,
// ...anything that is not the original Win32 client). All the per-platform
// blocks below are enabled by it.
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#define M2_PORT 1
#endif

// C sources (eterPack/md5.c) get -include windows.h too; the port API is
// C++ only, so C translation units just see the M2_PORT marker.
#ifdef __cplusplus
#include <cstddef>
#include <cstdarg>

namespace M2Plat
{
	struct SRect { long left, top, right, bottom; };
	enum ELogLevel
	{
		LOG_DEBUG = 0,
		LOG_INFO,
		LOG_WARN,
		LOG_ERROR,
	};

	void LogV(ELogLevel level, const char* tag, const char* fmt, va_list args);
	void Log(ELogLevel level, const char* tag, const char* fmt, ...);

	// Graphics surface ------------------------------------------------------------

	// Called by the GL device from Present. Adapters swap buffers / hand a new
	// surface to the EGL context; the web adapter is a no-op because the browser
	// composites the canvas at the end of each requestAnimationFrame callback.
	bool PresentFrame();

	// Physical pixel size of the drawing surface (NOT the logical UI size).
	int SurfaceWidth();
	int SurfaceHeight();

	// Filesystem ------------------------------------------------------------------

	// Strip drive letters, map '\\' -> '/', lowercase. Pure function; identical on
	// every platform.
	void NormalizePath(const char* in, char* out, size_t outSize);

	// access(2) with a normalized-path fallback.
	int FileAccess(const char* path, int mode);

	// Copy a file the adapter holds outside the filesystem (web: lazy pack
	// data) to its normalized path. False if the adapter does not know it.
	bool MaterializeFile(const char* path);

	// Audio output owned by the adapter instead of the audio library's own
	// device (web: AudioWorklet). render fills interleaved stereo floats.
	// False when the adapter has none and the library should open a device.
	typedef void (*AudioRenderFn)(float* out, unsigned frames, void* user);
	bool OpenAudioOutput(unsigned sampleRate, AudioRenderFn render, void* user);

	// Root for per-user data (saves, logs). Android: external files dir;
	// web: a MEMFS/IDBFS mount point.
	const char* DataDir();

	// TTF the GDI font shim should load.
	const char* FontFilePath();

	// UI / device ---------------------------------------------------------------

	void SetKeyboardVisible(bool visible, float focusBottomFraction);
	void SetGameControlsVisible(bool visible);
	// Tell the adapter which screen regions (logical pixels) are engine UI, so
	// platform chrome (e.g. the touch joystick) does not eat touches there.
	void SetTouchBlockers(const SRect* rects, int count, int width, int height);
	void RestartApp();

	// Environment lookup; adapters map it onto getenv or page config.
	// Returns value length or 0.
	int GetEnv(const char* name, char* out, size_t outSize);

	// Translate a platform-native key code into the engine's DIK_* / VK_*
	// codes (0 = not mappable). The key queue carries platform codes.
	int KeyToDIK(int platKey);
	int KeyToVK(int platKey);

	// Read a platform debug property (Android: system property; web: URL query /
	// localStorage). Returns the value length or 0.
	int GetDebugProperty(const char* name, char* out, size_t outSize);

	// Portable helpers implemented in m2platform.cpp (NormalizePath, FileAccess)
	// plus per-adapter files. A default weak implementation is provided for
	// adapters that do not need a piece.
}

#endif /* __cplusplus */
