#include "StdAfx.h"
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

#define LOG_TAG "Metin2Mobile"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern AAssetManager* g_pAssetManager;
int g_iAndroidSurfaceWidth = 0;
int g_iAndroidSurfaceHeight = 0;
int AndroidMain(int argc, char** argv);

static JavaVM* s_pJavaVM = NULL;

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

void AndroidSetGameControlsVisible(bool bVisible)
{
	if (!s_pJavaVM || !s_jSetGameControlsVisible)
		return;
	JNIEnv* env = NULL;
	if (s_pJavaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK || !env)
		return;
	env->CallStaticVoidMethod(s_jNativeLib, s_jSetGameControlsVisible, (jboolean)bVisible);
}

void AndroidSetKeyboardVisible(bool bVisible, float fFocusBottom)
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

// Called by the D3D8 device's Present on the game thread.
bool AndroidPresent()
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

	g_iAndroidSurfaceWidth = width;
	g_iAndroidSurfaceHeight = height;
	g_pAssetManager = AAssetManager_fromJava(env, assetManager);

	const char* c_szDataDir = env->GetStringUTFChars(dataDir, NULL);
	if (chdir(c_szDataDir) != 0)
		LOGE("chdir(%s) failed", c_szDataDir);
	LOGI("Starting Metin2 (%dx%d), data dir: %s", width, height, c_szDataDir);
	env->ReleaseStringUTFChars(dataDir, c_szDataDir);

	InstallCrashHandler();

	if (freopen("stderr.txt", "w", stderr))
		setvbuf(stderr, NULL, _IONBF, 0);

	char szProgram[] = "metin2";
	char* argv[] = { szProgram, NULL };
	AndroidMain(1, argv);

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
	}
	return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL Java_com_metin2_client_NativeLib_keyEvent(JNIEnv* env, jobject obj, jint action, jint keyCode, jint unicodeChar)
{
	CMSApplication::PushKeyEvent(action, keyCode, unicodeChar);
}

JNIEXPORT void JNICALL Java_com_metin2_client_NativeLib_touchEvent(JNIEnv* env, jobject obj, jint action, jfloat x, jfloat y)
{
	CMSApplication::PushTouchEvent(action, (int)x, (int)y);
}

}
