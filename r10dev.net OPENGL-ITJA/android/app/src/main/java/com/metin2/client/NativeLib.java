package com.metin2.client;

import android.view.Surface;

public class NativeLib {
    static {
        System.loadLibrary("metin2_mobile");
    }

    public static native void init(Object assetManager, Surface surface, String dataDir, int width, int height);
    public static native void touchEvent(int action, float x, float y);
    public static native void keyEvent(int action, int keyCode, int unicodeChar);

    static void setKeyboardVisible(boolean visible) {
        MainActivity.setKeyboardVisible(visible);
    }
}
