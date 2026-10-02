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
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import java.io.File;

public class MainActivity extends Activity implements SurfaceHolder.Callback {
    private static MainActivity sInstance;
    private static EmbeddedServer sServer;
    private Thread mGameThread;
    private GameView mView;
    private File mDataDir;
    private float mFocusBottom = -1.0f;
    private int mImeHeight;
    private TextView mStatus;
    private ProgressBar mProgress;
    private Button mAction;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);

        sInstance = this;
        mDataDir = getExternalFilesDir(null);
        if (DataInstaller.isInstalled(mDataDir))
            prepareAndStart();
        else
            showInstaller();
    }

    private void showInstaller() {
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setPadding(96, 48, 96, 48);
        mStatus = new TextView(this);
        mStatus.setGravity(Gravity.CENTER);
        mStatus.setTextSize(18);
        mProgress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        mProgress.setMax(1000);
        mAction = new Button(this);
        mAction.setText("Retry download");
        mAction.setVisibility(View.GONE);
        mAction.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                startDownload();
            }
        });
        layout.addView(mStatus);
        layout.addView(mProgress, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));
        layout.addView(mAction);
        setContentView(layout);
        startDownload();
    }

    private void startDownload() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        mAction.setVisibility(View.GONE);
        mStatus.setText("Downloading game data...");
        new Thread(new Runnable() {
            public void run() {
                try {
                    DataInstaller.install(MainActivity.this, mDataDir, new DataInstaller.Progress() {
                        public void onProgress(final long done, final long total) {
                            runOnUiThread(new Runnable() {
                                public void run() {
                                    mStatus.setText(String.format("Downloading game data... %d / %d MB", done >> 20, Math.max(total, 0) >> 20));
                                    if (total > 0)
                                        mProgress.setProgress((int) (done * 1000 / total));
                                }
                            });
                        }
                    });
                    runOnUiThread(new Runnable() {
                        public void run() {
                            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                            prepareAndStart();
                        }
                    });
                } catch (final Exception e) {
                    runOnUiThread(new Runnable() {
                        public void run() {
                            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                            mStatus.setText("Download failed: " + e.getMessage());
                            mAction.setVisibility(View.VISIBLE);
                        }
                    });
                }
            }
        }, "M2DataInstall").start();
    }

    private void prepareAndStart() {
        new Thread(new Runnable() {
            public void run() {
                try {
                    DataInstaller.writeServerProfile(mDataDir);
                } catch (final Exception e) {
                    android.util.Log.e("Metin2Mobile", "server profile: " + e);
                }
                startEmbeddedServer();
                runOnUiThread(new Runnable() {
                    public void run() {
                        mView = new GameView(MainActivity.this);
                        mView.getHolder().addCallback(MainActivity.this);
                        setContentView(mView);
                        mView.requestFocus();
                        mView.getViewTreeObserver().addOnGlobalLayoutListener(new android.view.ViewTreeObserver.OnGlobalLayoutListener() {
                            public void onGlobalLayout() {
                                updateKeyboardShift();
                            }
                        });
                        if (android.os.Build.VERSION.SDK_INT >= 30) {
                            getWindow().getDecorView().setOnApplyWindowInsetsListener(new android.view.View.OnApplyWindowInsetsListener() {
                                public android.view.WindowInsets onApplyWindowInsets(android.view.View v, android.view.WindowInsets insets) {
                                    mImeHeight = insets.getInsets(android.view.WindowInsets.Type.ime()).bottom;
                                    updateKeyboardShift();
                                    return v.onApplyWindowInsets(insets);
                                }
                            });
                        }
                    }
                });
            }
        }, "M2Prepare").start();
    }

    static void setKeyboardVisible(final boolean visible, final float focusBottom) {
        final MainActivity activity = sInstance;
        if (activity == null)
            return;
        activity.runOnUiThread(new Runnable() {
            public void run() {
                InputMethodManager imm = (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
                GameView view = activity.mView;
                view.mTextInputActive = visible;
                activity.mFocusBottom = visible ? focusBottom : -1.0f;
                activity.updateKeyboardShift();
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

    // Slides the game view up so the focused text field stays above the soft keyboard.
    private void updateKeyboardShift() {
        if (mView == null)
            return;
        float shift = 0;
        if (mFocusBottom >= 0) {
            float keyboardTop = mView.getHeight() - mImeHeight;
            if (android.os.Build.VERSION.SDK_INT < 30) {
                android.graphics.Rect visibleFrame = new android.graphics.Rect();
                mView.getWindowVisibleDisplayFrame(visibleFrame);
                keyboardTop = visibleFrame.bottom;
            }
            float focusBottomPx = mFocusBottom * mView.getHeight() + mView.getHeight() * 0.12f;
            shift = Math.max(0, focusBottomPx - keyboardTop);
        }
        mView.setTranslationY(-shift);
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
        NativeLib.touchEvent(event.getAction(), event.getX(), event.getY() - (mView != null ? mView.getTranslationY() : 0));
        return true;
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
    }

    @Override
    public void surfaceChanged(final SurfaceHolder holder, int format, final int width, final int height) {
        if (mGameThread != null)
            return;
        final String dataDir = mDataDir.getAbsolutePath();
        mGameThread = new Thread(new Runnable() {
            public void run() {
                NativeLib.init(getAssets(), holder.getSurface(), dataDir, width, height);
                stopEmbeddedServer();
            }
        }, "Metin2Game");
        mGameThread.start();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
    }

    @Override
    protected void onDestroy() {
        if (isFinishing()) {
            new Thread(new Runnable() {
                public void run() {
                    stopEmbeddedServer();
                }
            }, "M2ServerStop").start();
        }
        super.onDestroy();
    }

    private void startEmbeddedServer() {
        if (!"embedded".equals(BuildConfig.M2_SERVER_MODE))
            return;
        synchronized (MainActivity.class) {
            if (sServer == null)
                sServer = new EmbeddedServer(getApplicationContext(), BuildConfig.M2_AUTH_PORT, BuildConfig.M2_CHANNEL_PORT);
        }
        if (!sServer.start())
            android.util.Log.e("Metin2Mobile", "embedded server failed to start; see files/server/logs");
    }

    private static void stopEmbeddedServer() {
        EmbeddedServer server;
        synchronized (MainActivity.class) {
            server = sServer;
        }
        if (server != null)
            server.stop();
    }
}
