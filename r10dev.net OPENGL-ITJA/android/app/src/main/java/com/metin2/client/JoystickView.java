package com.metin2.client;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

// Floating movement stick in the style of mobile action games: at rest only a small thumb
// dot is shown; touching anywhere in this view's area plants the stick under the thumb and
// dragging walks. It drives the game's arrow-key movement, so walking is camera-relative
// exactly as with the keyboard on desktop.
public class JoystickView extends View {
    private static final int[] KEYS = {
        KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_RIGHT
    };
    private final boolean[] mPressed = new boolean[4];
    private final Paint mRingPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mKnobPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mRestPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final float mRadius;
    private final float mKnobRadius;
    private int mPointerId = -1;
    private float mOriginX, mOriginY, mKnobX, mKnobY;

    public JoystickView(Context context) {
        super(context);
        float dp = context.getResources().getDisplayMetrics().density;
        mRadius = 48 * dp;
        mKnobRadius = 22 * dp;
        mRingPaint.setStyle(Paint.Style.STROKE);
        mRingPaint.setStrokeWidth(2 * dp);
        mRingPaint.setColor(0x60FFFFFF);
        mKnobPaint.setColor(0xA0E0C080);
        mRestPaint.setStyle(Paint.Style.STROKE);
        mRestPaint.setStrokeWidth(2 * dp);
        mRestPaint.setColor(0x50FFFFFF);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (mPointerId == -1) {
            canvas.drawCircle(getWidth() / 2f, getHeight() / 2f, mKnobRadius, mRestPaint);
            return;
        }
        canvas.drawCircle(mOriginX, mOriginY, mRadius, mRingPaint);
        canvas.drawCircle(mKnobX, mKnobY, mKnobRadius, mKnobPaint);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int index = event.getActionIndex();
        switch (event.getActionMasked()) {
        case MotionEvent.ACTION_DOWN:
        case MotionEvent.ACTION_POINTER_DOWN:
            if (mPointerId != -1)
                return true;
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN && NativeLib.isUiAt(getLeft() + event.getX(index), getTop() + event.getY(index)))
                return false;
            mPointerId = event.getPointerId(index);
            mOriginX = mKnobX = event.getX(index);
            mOriginY = mKnobY = event.getY(index);
            break;
        case MotionEvent.ACTION_MOVE: {
            int i = event.findPointerIndex(mPointerId);
            if (i < 0)
                return true;
            float dx = event.getX(i) - mOriginX, dy = event.getY(i) - mOriginY;
            float len = (float) Math.sqrt(dx * dx + dy * dy);
            if (len > mRadius) {
                dx *= mRadius / len;
                dy *= mRadius / len;
                len = mRadius;
            }
            mKnobX = mOriginX + dx;
            mKnobY = mOriginY + dy;
            float dead = mRadius * 0.25f;
            float diag = 0.38f * len;
            setDirections(len > dead && -dy > diag, len > dead && dy > diag, len > dead && -dx > diag, len > dead && dx > diag);
            break;
        }
        case MotionEvent.ACTION_UP:
        case MotionEvent.ACTION_POINTER_UP:
            if (event.getPointerId(index) == mPointerId)
                release();
            return true;
        case MotionEvent.ACTION_CANCEL:
            release();
            return true;
        }
        invalidate();
        return true;
    }

    boolean isActive() {
        return mPointerId != -1;
    }

    void release() {
        mPointerId = -1;
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
