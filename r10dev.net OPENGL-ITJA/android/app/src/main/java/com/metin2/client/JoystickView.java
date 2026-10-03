package com.metin2.client;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

// On-screen movement stick. Drives the game's arrow-key movement, so walking is relative to
// the camera exactly as with the keyboard on desktop.
public class JoystickView extends View {
    private static final int[] KEYS = {
        KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_RIGHT
    };
    private final boolean[] mPressed = new boolean[4];
    private final Paint mBasePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mKnobPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private float mKnobX, mKnobY;
    private boolean mActive;

    public JoystickView(Context context) {
        super(context);
        mBasePaint.setColor(0x40FFFFFF);
        mKnobPaint.setColor(0x90E0C080);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        float cx = getWidth() / 2f, cy = getHeight() / 2f, r = Math.min(cx, cy);
        canvas.drawCircle(cx, cy, r, mBasePaint);
        canvas.drawCircle(mActive ? mKnobX : cx, mActive ? mKnobY : cy, r * 0.42f, mKnobPaint);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        float cx = getWidth() / 2f, cy = getHeight() / 2f, r = Math.min(cx, cy);
        switch (event.getActionMasked()) {
        case MotionEvent.ACTION_DOWN:
        case MotionEvent.ACTION_MOVE: {
            float dx = event.getX() - cx, dy = event.getY() - cy;
            float len = (float) Math.sqrt(dx * dx + dy * dy);
            if (len > r) {
                dx *= r / len;
                dy *= r / len;
            }
            mActive = true;
            mKnobX = cx + dx;
            mKnobY = cy + dy;
            float dead = r * 0.25f;
            float diag = 0.38f * len;
            setDirections(len > dead && -dy > diag, len > dead && dy > diag, len > dead && -dx > diag, len > dead && dx > diag);
            break;
        }
        case MotionEvent.ACTION_UP:
        case MotionEvent.ACTION_CANCEL:
            release();
            break;
        }
        invalidate();
        return true;
    }

    void release() {
        mActive = false;
        setDirections(false, false, false, false);
        invalidate();
    }

    private void setDirections(boolean up, boolean down, boolean left, boolean right) {
        boolean[] want = { up, down, left, right };
        for (int i = 0; i < 4; ++i) {
            if (want[i] != mPressed[i]) {
                mPressed[i] = want[i];
                NativeLib.keyEvent(want[i] ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP, KEYS[i], 0);
            }
        }
    }
}
