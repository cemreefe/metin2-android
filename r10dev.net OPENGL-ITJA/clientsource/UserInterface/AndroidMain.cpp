#include "StdAfx.h"
#include "PythonApplication.h"
#include <jni.h>
#include <unistd.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#define LOG_TAG "Metin2Mobile"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern AAssetManager* g_pAssetManager;
int AndroidMain(int argc, char** argv);

extern "C" {

JNIEXPORT void JNICALL Java_com_metin2_client_NativeLib_init(JNIEnv* env, jobject obj, jobject assetManager, jstring dataDir, jint width, jint height)
{
	static bool s_bStarted = false;
	if (s_bStarted)
		return;
	s_bStarted = true;

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

JNIEXPORT void JNICALL Java_com_metin2_client_NativeLib_render(JNIEnv* env, jobject obj)
{
}

JNIEXPORT void JNICALL Java_com_metin2_client_NativeLib_touchEvent(JNIEnv* env, jobject obj, jint action, jfloat x, jfloat y)
{
	CMSApplication::PushTouchEvent(action, (int)x, (int)y);
}

}
