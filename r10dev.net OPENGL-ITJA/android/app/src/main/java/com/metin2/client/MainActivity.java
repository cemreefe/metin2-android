package com.metin2.client;

import android.app.Activity;
import android.os.Bundle;
import android.content.Context;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.inputmethod.InputMethodManager;
import android.view.SurfaceHolder;
import android.view.Window;
import android.view.WindowManager;

public class MainActivity extends Activity implements SurfaceHolder.Callback {
    private static MainActivity sInstance;
    private Thread mGameThread;
    private GameView mView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);

        sInstance = this;
        mView = new GameView(this);
        mView.getHolder().addCallback(this);
        setContentView(mView);
        mView.requestFocus();
    }

    static void setKeyboardVisible(final boolean visible) {
        final MainActivity activity = sInstance;
        if (activity == null)
            return;
        activity.runOnUiThread(new Runnable() {
            public void run() {
                InputMethodManager imm = (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
                GameView view = activity.mView;
                view.mTextInputActive = visible;
                if (visible) {
                    view.requestFocus();
                    imm.restartInput(view);
                    imm.showSoftInput(view, 0);
                } else {
                    imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
                    imm.restartInput(view);
                }
            }
        });
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN)
            return super.dispatchKeyEvent(event);
        if (event.getAction() == KeyEvent.ACTION_MULTIPLE) {
            String chars = event.getCharacters();
            if (chars != null) {
                for (int i = 0; i < chars.length(); ++i) {
                    NativeLib.keyEvent(KeyEvent.ACTION_DOWN, 0, chars.charAt(i));
                }
            }
            return true;
        }
        if (event.getAction() == KeyEvent.ACTION_DOWN || event.getAction() == KeyEvent.ACTION_UP)
            NativeLib.keyEvent(event.getAction(), keyCode, event.getUnicodeChar());
        return true;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        NativeLib.touchEvent(event.getAction(), event.getX(), event.getY());
        return true;
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
    }

    @Override
    public void surfaceChanged(final SurfaceHolder holder, int format, final int width, final int height) {
        if (mGameThread != null)
            return;
        final String dataDir = getExternalFilesDir(null).getAbsolutePath();
        mGameThread = new Thread(new Runnable() {
            public void run() {
                NativeLib.init(getAssets(), holder.getSurface(), dataDir, width, height);
            }
        }, "Metin2Game");
        mGameThread.start();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
    }
}
