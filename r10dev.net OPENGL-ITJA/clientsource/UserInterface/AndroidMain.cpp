#include "StdAfx.h"
#include "PythonApplication.h"
#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>

#define LOG_TAG "Metin2Mobile"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern AAssetManager* g_pAssetManager;
int AndroidMain(int argc, char** argv);

extern "C" {

static bool CreateEGLContext(ANativeWindow* pWindow)
{
	EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (display == EGL_NO_DISPLAY || !eglInitialize(display, NULL, NULL))
	{
		LOGE("eglInitialize failed: 0x%x", eglGetError());
		return false;
	}

	const EGLint aConfigAttribs[] = {
		EGL_RENDERABLE_TYPE, 0x40 /* EGL_OPENGL_ES3_BIT_KHR */,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 16,
		EGL_NONE
	};
	EGLConfig config;
	EGLint iNumConfigs = 0;
	if (!eglChooseConfig(display, aConfigAttribs, &config, 1, &iNumConfigs) || iNumConfigs < 1)
	{
		LOGE("eglChooseConfig failed: 0x%x", eglGetError());
		return false;
	}

	EGLSurface surface = eglCreateWindowSurface(display, config, pWindow, NULL);
	const EGLint aContextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, aContextAttribs);
	if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context))
	{
		LOGE("EGL surface/context setup failed: 0x%x", eglGetError());
		return false;
	}
	return true;
}

JNIEXPORT void JNICALL Java_com_metin2_client_NativeLib_init(JNIEnv* env, jobject obj, jobject assetManager, jobject jSurface, jstring dataDir, jint width, jint height)
{
	static bool s_bStarted = false;
	if (s_bStarted)
		return;
	s_bStarted = true;

	ANativeWindow* pWindow = ANativeWindow_fromSurface(env, jSurface);
	if (!pWindow || !CreateEGLContext(pWindow))
		return;

	g_pAssetManager = AAssetManager_fromJava(env, assetManager);

	const char* c_szDataDir = env->GetStringUTFChars(dataDir, NULL);
	if (chdir(c_szDataDir) != 0)
		LOGE("chdir(%s) failed", c_szDataDir);
	LOGI("Starting Metin2 (%dx%d), data dir: %s", width, height, c_szDataDir);
	env->ReleaseStringUTFChars(dataDir, c_szDataDir);

	if (freopen("stderr.txt", "w", stderr))
		setvbuf(stderr, NULL, _IONBF, 0);

	char szProgram[] = "metin2";
	char* argv[] = { szProgram, NULL };
	AndroidMain(1, argv);

	LOGI("Metin2 main loop exited");
}

JNIEXPORT void JNICALL Java_com_metin2_client_NativeLib_touchEvent(JNIEnv* env, jobject obj, jint action, jfloat x, jfloat y)
{
	CMSApplication::PushTouchEvent(action, (int)x, (int)y);
}

}
