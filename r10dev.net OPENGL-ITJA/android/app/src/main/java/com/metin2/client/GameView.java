package com.metin2.client;

import android.content.Context;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

public class GameView extends SurfaceView {
    boolean mTextInputActive;

    public GameView(Context context) {
        super(context);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    private int mPointerId = -1;
    private boolean mPinching;
    private float mPinchSpan;
    private JoystickView mJoystick;

    // A wheel delta of 120 is one notch of the desktop mouse wheel, which the camera reads as
    // one zoom step; spreading the fingers this far apart zooms in by one step.
    // Matches CMSApplication::TOUCH_WHEEL; its x argument is the wheel delta.
    private static final int ACTION_WHEEL = 18;
    private static final int WHEEL_NOTCH = 120;
    private static final float PINCH_PIXELS_PER_NOTCH = 90.0f;
    private static final float PINCH_SLOP = 12.0f;

    void setJoystick(JoystickView joystick) {
        mJoystick = joystick;
    }

    // The engine follows a single pointer: the first finger that lands on the game view.
    // Other fingers (e.g. one on the joystick) are handled by their own views.
    // Two fingers on the game view pinch instead, which zooms the camera.
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        int index = event.getActionIndex();
        switch (action) {
        case MotionEvent.ACTION_DOWN:
        case MotionEvent.ACTION_POINTER_DOWN:
            if (pinchPointers(event) >= 2) {
                if (!mPinching) {
                    mPinching = true;
                    mPinchSpan = span(event);
                    releaseTouch();
                }
                return true;
            }
            if (mPinching || mPointerId != -1 || isJoystickPointer(event, index))
                return true;
            mPointerId = event.getPointerId(index);
            NativeLib.touchEvent(MotionEvent.ACTION_DOWN, event.getX(index), event.getY(index));
            return true;
        case MotionEvent.ACTION_MOVE: {
            if (mPinching) {
                if (pinchPointers(event) >= 2) {
                    float newSpan = span(event);
                    float delta = newSpan - mPinchSpan;
                    if (Math.abs(delta) >= PINCH_SLOP) {
                        mPinchSpan = newSpan;
                        NativeLib.touchEvent(ACTION_WHEEL, delta * WHEEL_NOTCH / PINCH_PIXELS_PER_NOTCH, 0.0f);
                    }
                }
                return true;
            }
            int i = event.findPointerIndex(mPointerId);
            if (i >= 0)
                NativeLib.touchEvent(MotionEvent.ACTION_MOVE, event.getX(i), event.getY(i));
            return true;
        }
        case MotionEvent.ACTION_UP:
        case MotionEvent.ACTION_POINTER_UP:
            if (mPinching) {
                // Stay in pinch mode until every finger is gone, so lifting one of them does not
                // turn the remaining one into a camera drag.
                if (action == MotionEvent.ACTION_UP || event.getPointerCount() <= 1)
                    mPinching = false;
                return true;
            }
            if (isJoystickPointer(event, index))
                return true;
            if (event.getPointerId(index) == mPointerId) {
                NativeLib.touchEvent(MotionEvent.ACTION_UP, event.getX(index), event.getY(index));
                mPointerId = -1;
            }
            return true;
        case MotionEvent.ACTION_CANCEL:
            mPinching = false;
            releaseTouch();
            return true;
        }
        return true;
    }

    // Fingers that steer the joystick must not pinch: holding the stick and dragging the
    // camera with a second finger has to keep working. Split touch delivery usually keeps
    // those pointers out of this view's events, but not on every device.
    private boolean isJoystickPointer(MotionEvent event, int index) {
        if (mJoystick == null || mJoystick.getVisibility() != VISIBLE)
            return false;
        float x = event.getX(index) + getLeft(), y = event.getY(index) + getTop();
        return x >= mJoystick.getLeft() && x < mJoystick.getRight()
            && y >= mJoystick.getTop() && y < mJoystick.getBottom();
    }

    private int pinchPointers(MotionEvent event) {
        if (mJoystick != null && mJoystick.isActive())
            return 0;
        int count = 0;
        for (int i = 0; i < event.getPointerCount(); ++i)
            if (!isJoystickPointer(event, i))
                ++count;
        return count;
    }

    private float span(MotionEvent event) {
        int first = -1;
        for (int i = 0; i < event.getPointerCount(); ++i) {
            if (isJoystickPointer(event, i))
                continue;
            if (first == -1) {
                first = i;
                continue;
            }
            return (float) Math.hypot(event.getX(first) - event.getX(i), event.getY(first) - event.getY(i));
        }
        return mPinchSpan;
    }

    void releaseTouch() {
        if (mPointerId == -1)
            return;
        mPointerId = -1;
        NativeLib.touchEvent(MotionEvent.ACTION_CANCEL, 0, 0);
    }

    @Override
    public boolean onCheckIsTextEditor() {
        return mTextInputActive;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        if (!mTextInputActive)
            return null;
        outAttrs.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_ACTION_SEND;
        return new BaseInputConnection(this, false) {
            @Override
            public boolean deleteSurroundingText(int beforeLength, int afterLength) {
                for (int i = 0; i < beforeLength; ++i) {
                    sendKey(KeyEvent.KEYCODE_DEL);
                }
                return true;
            }

            @Override
            public boolean performEditorAction(int actionCode) {
                sendKey(KeyEvent.KEYCODE_ENTER);
                return true;
            }

            private void sendKey(int keyCode) {
                sendKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, keyCode));
                sendKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, keyCode));
            }
        };
    }
}
