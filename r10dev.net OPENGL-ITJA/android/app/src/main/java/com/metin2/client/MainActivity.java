package com.metin2.client;

import android.app.Activity;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.Window;
import android.view.WindowManager;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MainActivity extends Activity {
    private GLSurfaceView mGLView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);

        mGLView = new GLSurfaceView(this);
        mGLView.setEGLContextClientVersion(3);
        mGLView.setRenderer(new Renderer());
        setContentView(mGLView);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        NativeLib.touchEvent(event.getAction(), event.getX(), event.getY());
        return true;
    }

    private class Renderer implements GLSurfaceView.Renderer {
        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            // Width and height will be set in onSurfaceChanged
        }

        public void onSurfaceChanged(GL10 gl, int width, int height) {
            NativeLib.init(MainActivity.this.getAssets(), getExternalFilesDir(null).getAbsolutePath(), width, height);
        }

        public void onDrawFrame(GL10 gl) {
            NativeLib.render();
        }
    }
}
