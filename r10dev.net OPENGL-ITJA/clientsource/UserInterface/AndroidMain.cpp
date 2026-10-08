#include "StdAfx.h"
#include "../MilesLib/mss.h"
#include "PythonApplication.h"
#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unwind.h>
#include <pthread.h>
#include <time.h>
#include <mutex>
#include <vector>

#include <sys/system_properties.h>

#define LOG_TAG "Metin2Mobile"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern AAssetManager* g_pAssetManager;
int M2PortMain(int argc, char** argv);

static JavaVM* s_pJavaVM = NULL;
static int s_iSurfaceWidth = 0;
static int s_iSurfaceHeight = 0;
static char s_szDataDir[512] = "";

// ---- M2Plat port: Android adapter ----

void M2Plat::LogV(ELogLevel level, const char* tag, const char* fmt, va_list args)
{
	android_LogPriority prio = ANDROID_LOG_INFO;
	switch (level)
	{
	case LOG_DEBUG: prio = ANDROID_LOG_DEBUG; break;
	case LOG_WARN:  prio = ANDROID_LOG_WARN; break;
	case LOG_ERROR: prio = ANDROID_LOG_ERROR; break;
	default: break;
	}
	__android_log_vprint(prio, tag && tag[0] ? tag : LOG_TAG, fmt, args);
}

int M2Plat::SurfaceWidth() { return s_iSurfaceWidth; }
int M2Plat::SurfaceHeight() { return s_iSurfaceHeight; }
const char* M2Plat::DataDir() { return s_szDataDir; }
const char* M2Plat::FontFilePath() { return "/system/fonts/Roboto-Regular.ttf"; }

int M2Plat::GetEnv(const char* name, char* out, size_t outSize)
{
	const char* v = getenv(name);
	if (!v || !out || !outSize)
		return 0;
	int n = snprintf(out, outSize, "%s", v);
	return n > 0 ? n : 0;
}

int M2Plat::GetDebugProperty(const char* name, char* out, size_t outSize)
{
	if (!out || !outSize)
		return 0;
	out[0] = 0;
	return __system_property_get(name, out);
}

int M2Plat::KeyToDIK(int keyCode)
{
	static const unsigned char c_aLetterDIK[26] = {
		DIK_A, DIK_B, DIK_C, DIK_D, DIK_E, DIK_F, DIK_G, DIK_H, DIK_I, DIK_J, DIK_K, DIK_L, DIK_M,
		DIK_N, DIK_O, DIK_P, DIK_Q, DIK_R, DIK_S, DIK_T, DIK_U, DIK_V, DIK_W, DIK_X, DIK_Y, DIK_Z };
	static const unsigned char c_aDigitDIK[10] = {
		DIK_0, DIK_1, DIK_2, DIK_3, DIK_4, DIK_5, DIK_6, DIK_7, DIK_8, DIK_9 };

	if (keyCode >= 29 && keyCode <= 54)
		return c_aLetterDIK[keyCode - 29];
	if (keyCode >= 7 && keyCode <= 16)
		return c_aDigitDIK[keyCode - 7];
	if (keyCode >= 131 && keyCode <= 140)
		return DIK_F1 + (keyCode - 131);

	switch (keyCode)
	{
	case 4: case 111: return DIK_ESCAPE;
	case 19: return DIK_UP;
	case 20: return DIK_DOWN;
	case 21: return DIK_LEFT;
	case 22: return DIK_RIGHT;
	case 61: return DIK_TAB;
	case 62: return DIK_SPACE;
	case 66: case 160: return DIK_RETURN;
	case 67: return DIK_BACK;
	case 112: return DIK_DELETE;
	case 59: return DIK_LSHIFT;
	case 60: return DIK_RSHIFT;
	case 113: return DIK_LCONTROL;
	case 114: return DIK_RCONTROL;
	case 57: return DIK_LMENU;
	case 122: return DIK_HOME;
	case 123: return DIK_END;
	}
	return 0;
}

int M2Plat::KeyToVK(int keyCode)
{
	switch (keyCode)
	{
	case 4: case 111: return VK_ESCAPE;
	case 61: return VK_TAB;
	case 66: case 160: return VK_RETURN;
	case 67: return VK_BACK;
	case 19: return VK_UP;
	case 20: return VK_DOWN;
	case 21: return VK_LEFT;
	case 22: return VK_RIGHT;
	case 112: return VK_DELETE;
	case 122: return VK_HOME;
	case 123: return VK_END;
	}
	return 0;
}

static void CrashWrite(int fd, const char* s)
{
	write(fd, s, strlen(s));
}

static void CrashWriteHex(int fd, uintptr_t v)
{
	char buf[19] = "0x";
	for (int i = 0; i < 16; ++i)
		buf[2 + i] = "0123456789abcdef"[(v >> ((15 - i) * 4)) & 0xf];
	buf[18] = 0;
	CrashWrite(fd, buf);
}

struct SCrashUnwind
{
	int fd;
	int depth;
};

static _Unwind_Reason_Code CrashUnwindFrame(struct _Unwind_Context* ctx, void* arg)
{
	SCrashUnwind* st = (SCrashUnwind*)arg;
	uintptr_t pc = _Unwind_GetIP(ctx);
	if (!pc || st->depth >= 64)
		return _URC_END_OF_STACK;
	Dl_info info;
	CrashWrite(st->fd, "#");
	CrashWriteHex(st->fd, (uintptr_t)st->depth++);
	CrashWrite(st->fd, " pc ");
	if (dladdr((void*)pc, &info) && info.dli_fname)
	{
		CrashWriteHex(st->fd, pc - (uintptr_t)info.dli_fbase);
		CrashWrite(st->fd, " ");
		CrashWrite(st->fd, info.dli_fname);
		if (info.dli_sname)
		{
			CrashWrite(st->fd, " (");
			CrashWrite(st->fd, info.dli_sname);
			CrashWrite(st->fd, ")");
		}
	}
	else
		CrashWriteHex(st->fd, pc);
	CrashWrite(st->fd, "\n");
	return _URC_NO_REASON;
}

static struct sigaction s_kOldCrashActions[NSIG];

static void CrashHandler(int sig, siginfo_t* info, void* uctx)
{
	int fd = open("crash.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0)
	{
		CrashWrite(fd, "signal ");
		CrashWriteHex(fd, (uintptr_t)sig);
		CrashWrite(fd, " code ");
		CrashWriteHex(fd, (uintptr_t)info->si_code);
		CrashWrite(fd, " addr ");
		CrashWriteHex(fd, (uintptr_t)info->si_addr);
#if defined(__aarch64__)
		CrashWrite(fd, " abi arm64-v8a\n");
#else
		CrashWrite(fd, " abi x86_64\n");
#endif
		SCrashUnwind st = { fd, 0 };
		_Unwind_Backtrace(CrashUnwindFrame, &st);
		close(fd);
	}
	sigaction(sig, &s_kOldCrashActions[sig], NULL);
	raise(sig);
}

static void InstallCrashHandler()
{
	static const int c_aiSignals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP };
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = CrashHandler;
	sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
	sigemptyset(&sa.sa_mask);
	for (size_t i = 0; i < sizeof(c_aiSignals) / sizeof(c_aiSignals[0]); ++i)
		sigaction(c_aiSignals[i], &sa, &s_kOldCrashActions[c_aiSignals[i]]);
}
static jclass s_jNativeLib = NULL;
static jmethodID s_jSetKeyboardVisible = NULL;
static jmethodID s_jSetGameControlsVisible = NULL;
static jmethodID s_jRestartApp = NULL;

void M2Plat::RestartApp()
{
	if (!s_pJavaVM || !s_jRestartApp)
		return;
	JNIEnv* env = NULL;
	if (s_pJavaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK || !env)
		return;
	env->CallStaticVoidMethod(s_jNativeLib, s_jRestartApp);
}

void M2Plat::SetGameControlsVisible(bool bVisible)
{
	if (!s_pJavaVM || !s_jSetGameControlsVisible)
		return;
	JNIEnv* env = NULL;
	if (s_pJavaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK || !env)
		return;
	env->CallStaticVoidMethod(s_jNativeLib, s_jSetGameControlsVisible, (jboolean)bVisible);
}

static std::mutex s_kTouchBlockerLock;
static std::vector<M2Plat::SRect> s_kTouchBlockers;
static int s_iTouchBlockerWidth = 0, s_iTouchBlockerHeight = 0;

void M2Plat::SetTouchBlockers(const SRect* rects, int count, int iWidth, int iHeight)
{
	std::lock_guard<std::mutex> lock(s_kTouchBlockerLock);
	s_kTouchBlockers.assign(rects, rects + count);
	s_iTouchBlockerWidth = iWidth;
	s_iTouchBlockerHeight = iHeight;
}

void M2Plat::SetKeyboardVisible(bool bVisible, float fFocusBottom)
{
	LOGI("keyboard visible=%d focusBottom=%.2f", (int)bVisible, fFocusBottom);
	if (!s_pJavaVM || !s_jSetKeyboardVisible)
		return;
	JNIEnv* env = NULL;
	if (s_pJavaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK || !env)
		return;
	env->CallStaticVoidMethod(s_jNativeLib, s_jSetKeyboardVisible, (jboolean)bVisible, (jfloat)fFocusBottom);
}

extern "C" {

static EGLDisplay s_eglDisplay = EGL_NO_DISPLAY;
static EGLConfig s_eglConfig = NULL;
static EGLContext s_eglContext = EGL_NO_CONTEXT;
static EGLSurface s_eglSurface = EGL_NO_SURFACE;
static EGLSurface s_eglIdleSurface = EGL_NO_SURFACE;

// The Java side hands the game thread a new window (or none while backgrounded); the game
// thread swaps it in at the next Present so the game loop and its connection keep running.
static pthread_mutex_t s_kWindowLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_kWindowCond = PTHREAD_COND_INITIALIZER;
static ANativeWindow* s_pPendingWindow = NULL;
static ANativeWindow* s_pCurrentWindow = NULL;
static bool s_bWindowPending = false;
static bool s_bGameRunning = false;

static bool CreateEGLContext(ANativeWindow* pWindow)
{
	s_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (s_eglDisplay == EGL_NO_DISPLAY || !eglInitialize(s_eglDisplay, NULL, NULL))
	{
		LOGE("eglInitialize failed: 0x%x", eglGetError());
		return false;
	}

	const EGLint aConfigAttribs[] = {
		EGL_RENDERABLE_TYPE, 0x40 /* EGL_OPENGL_ES3_BIT_KHR */,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 16,
		EGL_NONE
	};
	EGLint iNumConfigs = 0;
	if (!eglChooseConfig(s_eglDisplay, aConfigAttribs, &s_eglConfig, 1, &iNumConfigs) || iNumConfigs < 1)
	{
		LOGE("eglChooseConfig failed: 0x%x", eglGetError());
		return false;
	}

	s_eglSurface = eglCreateWindowSurface(s_eglDisplay, s_eglConfig, pWindow, NULL);
	const EGLint aContextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	s_eglContext = eglCreateContext(s_eglDisplay, s_eglConfig, EGL_NO_CONTEXT, aContextAttribs);
	if (s_eglSurface == EGL_NO_SURFACE || s_eglContext == EGL_NO_CONTEXT || !eglMakeCurrent(s_eglDisplay, s_eglSurface, s_eglSurface, s_eglContext))
	{
		LOGE("EGL surface/context setup failed: 0x%x", eglGetError());
		return false;
	}
	return true;
}

static void ApplyPendingWindow()
{
	pthread_mutex_lock(&s_kWindowLock);
	if (s_bWindowPending)
	{
		ANativeWindow* pWindow = s_pPendingWindow;
		s_pPendingWindow = NULL;
		s_bWindowPending = false;
		if (pWindow && pWindow == s_pCurrentWindow && s_eglSurface != EGL_NO_SURFACE)
		{
			ANativeWindow_release(pWindow);
			pthread_cond_broadcast(&s_kWindowCond);
			pthread_mutex_unlock(&s_kWindowLock);
			return;
		}
		if (s_eglSurface != EGL_NO_SURFACE)
		{
			if (s_eglIdleSurface == EGL_NO_SURFACE)
			{
				const EGLint aPbufferAttribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
				s_eglIdleSurface = eglCreatePbufferSurface(s_eglDisplay, s_eglConfig, aPbufferAttribs);
			}
			if (!eglMakeCurrent(s_eglDisplay, s_eglIdleSurface, s_eglIdleSurface, s_eglContext))
				LOGE("EGL idle surface bind failed: 0x%x", eglGetError());
			eglDestroySurface(s_eglDisplay, s_eglSurface);
			s_eglSurface = EGL_NO_SURFACE;
		}
		if (s_pCurrentWindow)
		{
			ANativeWindow_release(s_pCurrentWindow);
			s_pCurrentWindow = NULL;
		}
		if (pWindow)
		{
			s_eglSurface = eglCreateWindowSurface(s_eglDisplay, s_eglConfig, pWindow, NULL);
			if (s_eglSurface == EGL_NO_SURFACE || !eglMakeCurrent(s_eglDisplay, s_eglSurface, s_eglSurface, s_eglContext))
				LOGE("EGL surface re-attach failed: 0x%x", eglGetError());
			else
				LOGI("EGL surface re-attached");
			s_pCurrentWindow = pWindow;
		}
		else
			LOGI("EGL surface detached; game keeps running in the background");
		pthread_cond_broadcast(&s_kWindowCond);
	}
	pthread_mutex_unlock(&s_kWindowLock);
}

// Called by the D3D8 device's Present on the game thread. extern "C++" because
// this sits inside the extern "C" JNI block (it uses the EGL statics above).
extern "C++" bool M2Plat::PresentFrame()
{
	ApplyPendingWindow();
	if (s_eglSurface == EGL_NO_SURFACE)
	{
		usleep(33000);
		return true;
	}
	return eglSwapBuffers(s_eglDisplay, s_eglSurface) == EGL_TRUE;
}

JNIEXPORT void JNICALL Java_com_metin2_client_NativeLib_init(JNIEnv* env, jobject obj, jobject assetManager, jobject jSurface, jstring dataDir, jint width, jint height)
{
	static bool s_bStarted = false;
	if (s_bStarted)
		return;
	s_bStarted = true;
	s_bGameRunning = true;

	ANativeWindow* pWindow = ANativeWindow_fromSurface(env, jSurface);
	if (!pWindow || !CreateEGLContext(pWindow))
		return;
	s_pCurrentWindow = pWindow;

	s_iSurfaceWidth = width;
	s_iSurfaceHeight = height;
	g_pAssetManager = AAssetManager_fromJava(env, assetManager);

	const char* c_szDataDir = env->GetStringUTFChars(dataDir, NULL);
	snprintf(s_szDataDir, sizeof(s_szDataDir), "%s", c_szDataDir ? c_szDataDir : "");
	if (chdir(c_szDataDir) != 0)
		LOGE("chdir(%s) failed", c_szDataDir);
	LOGI("Starting Metin2 (%dx%d), data dir: %s", width, height, c_szDataDir);
	env->ReleaseStringUTFChars(dataDir, c_szDataDir);

	InstallCrashHandler();

	if (freopen("stderr.txt", "w", stderr))
		setvbuf(stderr, NULL, _IONBF, 0);

	char szProgram[] = "metin2";
	char* argv[] = { szProgram, NULL };
	M2PortMain(1, argv);

	pthread_mutex_lock(&s_kWindowLock);
	s_bGameRunning = false;
	pthread_cond_broadcast(&s_kWindowCond);
	pthread_mutex_unlock(&s_kWindowLock);
	LOGI("Metin2 main loop exited");
}

// Surface lifecycle from the UI thread. Detaching blocks until the game thread has stopped
// using the old window, as surfaceDestroyed requires.
JNIEXPORT void JNICALL Java_com_metin2_client_NativeLib_setSurface(JNIEnv* env, jobject obj, jobject jSurface)
{
	ANativeWindow* pWindow = jSurface ? ANativeWindow_fromSurface(env, jSurface) : NULL;
	pthread_mutex_lock(&s_kWindowLock);
	if (!s_bGameRunning)
	{
		pthread_mutex_unlock(&s_kWindowLock);
		if (pWindow)
			ANativeWindow_release(pWindow);
		return;
	}
	if (s_pPendingWindow)
		ANativeWindow_release(s_pPendingWindow);
	s_pPendingWindow = pWindow;
	s_bWindowPending = true;
	struct timespec deadline;
	clock_gettime(CLOCK_REALTIME, &deadline);
	deadline.tv_sec += 3;
	while (s_bWindowPending && s_bGameRunning)
		if (pthread_cond_timedwait(&s_kWindowCond, &s_kWindowLock, &deadline) != 0)
			break;
	pthread_mutex_unlock(&s_kWindowLock);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*)
{
	s_pJavaVM = vm;
	JNIEnv* env = NULL;
	if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
		return JNI_VERSION_1_6;
	jclass cls = env->FindClass("com/metin2/client/NativeLib");
	if (cls)
	{
		s_jNativeLib = (jclass)env->NewGlobalRef(cls);
		s_jSetKeyboardVisible = env->GetStaticMethodID(s_jNativeLib, "setKeyboardVisible", "(ZF)V");
		s_jSetGameControlsVisible = env->GetStaticMethodID(s_jNativeLib, "setGameControlsVisible", "(Z)V");
		s_jRestartApp = env->GetStaticMethodID(s_jNativeLib, "restartApp", "()V");
	}
	return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL Java_com_metin2_client_NativeLib_keyEvent(JNIEnv* env, jobject obj, jint action, jint keyCode, jint unicodeChar)
{
	CMSApplication::PushKeyEvent(action, keyCode, unicodeChar);
}

JNIEXPORT jboolean JNICALL Java_com_metin2_client_NativeLib_isUiAt(JNIEnv* env, jobject obj, jfloat x, jfloat y)
{
	std::lock_guard<std::mutex> lock(s_kTouchBlockerLock);
	if (s_iSurfaceWidth <= 0 || s_iSurfaceHeight <= 0 || s_iTouchBlockerWidth <= 0 || s_iTouchBlockerHeight <= 0)
		return JNI_FALSE;
	long lx = (long)(x * s_iTouchBlockerWidth / s_iSurfaceWidth);
	long ly = (long)(y * s_iTouchBlockerHeight / s_iSurfaceHeight);
	for (const M2Plat::SRect& r : s_kTouchBlockers)
		if (lx >= r.left && lx < r.right && ly >= r.top && ly < r.bottom)
			return JNI_TRUE;
	return JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_metin2_client_NativeLib_touchEvent(JNIEnv* env, jobject obj, jint action, jfloat x, jfloat y)
{
	CMSApplication::PushTouchEvent(action, (int)x, (int)y);
}

JNIEXPORT void JNICALL Java_com_metin2_client_NativeLib_setAudioPaused(JNIEnv* env, jobject obj, jboolean paused)
{
	AIL_set_paused(paused ? 1 : 0);
}

}
