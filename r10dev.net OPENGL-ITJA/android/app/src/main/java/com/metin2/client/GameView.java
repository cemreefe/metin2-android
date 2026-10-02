package com.metin2.client;

import android.content.Context;
import android.text.InputType;
import android.view.KeyEvent;
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
