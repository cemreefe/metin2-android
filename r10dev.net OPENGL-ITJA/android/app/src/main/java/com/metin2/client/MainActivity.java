package com.metin2.client;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
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
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

public class MainActivity extends Activity implements SurfaceHolder.Callback {
    private static MainActivity sInstance;
    private static EmbeddedServer sServer;
    private static MainActivity sCurrent;
    private static int sGeneration;
    private static Thread sGameThread;
    private GameView mView;
    private JoystickView mJoystick;
    private static boolean sGameControlsVisible;
    private File mDataDir;
    private float mFocusBottom = -1.0f;
    private int mImeHeight;
    private TextView mStatus;
    private ProgressBar mProgress;
    private Button mAction;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        synchronized (MainActivity.class) {
            sCurrent = this;
            ++sGeneration;
        }

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        if (android.os.Build.VERSION.SDK_INT >= 28)
            getWindow().getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        getWindow().getDecorView().setBackgroundColor(0xFF000000);
        hideSystemBars();

        sInstance = this;
        mDataDir = getExternalFilesDir(null);
        setRequestedOrientation(isPortraitConfigured(mDataDir)
                ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
                : ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        if (DataInstaller.isInstalled(mDataDir))
            prepareAndStart();
        else
            showInstaller();
    }

    private void showStatusScreen(String actionLabel, View.OnClickListener action) {
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
        mAction.setText(actionLabel);
        mAction.setVisibility(View.GONE);
        mAction.setOnClickListener(action);
        layout.addView(mStatus);
        layout.addView(mProgress, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));
        layout.addView(mAction);
        setContentView(layout);
    }

    private void showInstaller() {
        showStatusScreen(BuildConfig.M2_DATA_BUNDLED ? "Retry" : "Retry download", new View.OnClickListener() {
            public void onClick(View v) {
                startDownload();
            }
        });
        startDownload();
    }

    private void startDownload() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        mAction.setVisibility(View.GONE);
        final String verb = BuildConfig.M2_DATA_BUNDLED ? "Unpacking" : "Downloading";
        mStatus.setText(verb + " game data...");
        new Thread(new Runnable() {
            public void run() {
                try {
                    DataInstaller.install(MainActivity.this, mDataDir, new DataInstaller.Progress() {
                        public void onProgress(final long done, final long total) {
                            runOnUiThread(new Runnable() {
                                public void run() {
                                    mStatus.setText(String.format(verb + " game data... %d / %d MB", done >> 20, Math.max(total, 0) >> 20));
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
                            mStatus.setText(verb + " failed: " + e.getMessage());
                            mAction.setVisibility(View.VISIBLE);
                        }
                    });
                }
            }
        }, "M2DataInstall").start();
    }

    private void prepareAndStart() {
        final boolean embedded = "embedded".equals(BuildConfig.M2_SERVER_MODE);
        if (embedded) {
            showStatusScreen("Retry", new View.OnClickListener() {
                public void onClick(View v) {
                    prepareAndStart();
                }
            });
            mProgress.setIndeterminate(true);
            mStatus.setText("Starting local server...");
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
        new Thread(new Runnable() {
            public void run() {
                try {
                    ServerCatalog.refresh(getApplicationContext());
                    DataInstaller.writeServerProfile(getApplicationContext(), mDataDir);
                } catch (final Exception e) {
                    android.util.Log.e("Metin2Mobile", "server profile: " + e);
                }
                final boolean serverUp = startEmbeddedServer();
                final String failure = serverUp || sServer == null ? "" : sServer.failure();
                DevReporter.uploadPreviousRun(mDataDir);
                final DevReporter.Update update = DevReporter.checkForUpdate();
                runOnUiThread(new Runnable() {
                    public void run() {
                        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                        if (!serverUp) {
                            mProgress.setIndeterminate(false);
                            mStatus.setTextSize(13);
                            mStatus.setText("The local server failed to start.\n\n" + failure);
                            mAction.setVisibility(View.VISIBLE);
                        } else if (update != null)
                            offerUpdate(update);
                        else
                            startGame();
                    }
                });
            }
        }, "M2Prepare").start();
    }

    private void offerUpdate(final DevReporter.Update update) {
        new android.app.AlertDialog.Builder(this)
                .setTitle("Build " + update.build + " is available")
                .setPositiveButton("Update", new android.content.DialogInterface.OnClickListener() {
                    public void onClick(android.content.DialogInterface dialog, int which) {
                        startActivity(new android.content.Intent(android.content.Intent.ACTION_VIEW, android.net.Uri.parse(update.url)));
                        finish();
                    }
                })
                .setNegativeButton("Play build " + BuildConfig.VERSION_CODE, new android.content.DialogInterface.OnClickListener() {
                    public void onClick(android.content.DialogInterface dialog, int which) {
                        startGame();
                    }
                })
                .setCancelable(false)
                .show();
    }

    private void startGame() {
        GameSessionService.start(getApplicationContext());
        mView = new GameView(MainActivity.this);
        mView.getHolder().addCallback(MainActivity.this);
        android.widget.FrameLayout root = new android.widget.FrameLayout(MainActivity.this);
        root.addView(mView);
        mJoystick = new JoystickView(MainActivity.this);
        float dp = getResources().getDisplayMetrics().density;
        android.widget.FrameLayout.LayoutParams stickParams = new android.widget.FrameLayout.LayoutParams((int) (170 * dp), (int) (150 * dp), Gravity.BOTTOM | Gravity.START);
        stickParams.leftMargin = (int) (24 * dp);
        stickParams.bottomMargin = (int) (40 * dp);
        root.addView(mJoystick, stickParams);
        mJoystick.setVisibility(sGameControlsVisible ? View.VISIBLE : View.GONE);
        setContentView(root);
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

    static void setGameControlsVisible(final boolean visible) {
        sGameControlsVisible = visible;
        final MainActivity activity = sInstance;
        if (activity == null)
            return;
        activity.runOnUiThread(new Runnable() {
            public void run() {
                if (activity.mJoystick == null)
                    return;
                if (!visible)
                    activity.mJoystick.release();
                activity.mJoystick.setVisibility(visible ? View.VISIBLE : View.GONE);
            }
        });
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
    protected void onPause() {
        super.onPause();
        if (mJoystick != null)
            mJoystick.release();
        if (mView != null)
            mView.releaseTouch();
        NativeLib.setAudioPaused(true);
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemBars();
        NativeLib.setAudioPaused(false);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus)
            hideSystemBars();
    }

    // Draw under the status/navigation bars and the camera cutout; bars reappear on swipe.
    @SuppressWarnings("deprecation")
    private void hideSystemBars() {
        View decor = getWindow().getDecorView();
        if (android.os.Build.VERSION.SDK_INT >= 30) {
            getWindow().setDecorFitsSystemWindows(false);
            android.view.WindowInsetsController controller = decor.getWindowInsetsController();
            if (controller != null) {
                controller.hide(android.view.WindowInsets.Type.systemBars());
                controller.setSystemBarsBehavior(android.view.WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        }
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
    }

    @Override
    public void surfaceChanged(final SurfaceHolder holder, int format, final int width, final int height) {
        if (sGameThread != null) {
            NativeLib.setSurface(holder.getSurface());
            return;
        }
        final String dataDir = mDataDir.getAbsolutePath();
        sGameThread = new Thread(new Runnable() {
            public void run() {
                NativeLib.init(getAssets(), holder.getSurface(), dataDir, width, height);
                if (sCurrent == MainActivity.this)
                    stopEmbeddedServer();
            }
        }, "Metin2Game");
        sGameThread.start();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (sGameThread != null)
            NativeLib.setSurface(null);
    }

    @Override
    protected void onDestroy() {
        if (isFinishing() && sCurrent == this) {
            sCurrent = null;
            GameSessionService.stop(getApplicationContext());
            final int generation = sGeneration;
            new Thread(new Runnable() {
                public void run() {
                    synchronized (MainActivity.class) {
                        if (generation != sGeneration)
                            return;
                    }
                    stopEmbeddedServer();
                }
            }, "M2ServerStop").start();
        }
        super.onDestroy();
    }

    private boolean startEmbeddedServer() {
        if (!"embedded".equals(BuildConfig.M2_SERVER_MODE))
            return true;
        synchronized (MainActivity.class) {
            if (sServer == null)
                sServer = new EmbeddedServer(getApplicationContext(), BuildConfig.M2_AUTH_PORT, BuildConfig.M2_CHANNEL_PORT);
        }
        if (sServer.start())
            return true;
        android.util.Log.e("Metin2Mobile", "embedded server failed to start; see files/server/logs");
        return false;
    }

    /** display.cfg is written by the in-game display options ("orientation portrait"). */
    private static boolean isPortraitConfigured(File dataDir) {
        if (dataDir == null)
            return false;
        try (BufferedReader reader = new BufferedReader(new FileReader(new File(dataDir, "display.cfg")))) {
            String line;
            while ((line = reader.readLine()) != null) {
                String[] parts = line.trim().split("\\s+");
                if (parts.length == 2 && parts[0].equals("orientation"))
                    return parts[1].equals("portrait");
            }
        } catch (IOException e) {
            return false;
        }
        return false;
    }

    static void restartApp() {
        final MainActivity activity = sInstance;
        if (activity == null)
            return;
        stopEmbeddedServer();
        Intent restart = new Intent(activity, RestartActivity.class);
        restart.putExtra("oldPid", android.os.Process.myPid());
        activity.startActivity(restart);
    }

    static void stopEmbeddedServer() {
        EmbeddedServer server;
        synchronized (MainActivity.class) {
            server = sServer;
        }
        if (server != null)
            server.stop();
    }
}
