package com.metin2.client;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;

public final class RestartActivity extends Activity {
    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        final int oldPid = getIntent().getIntExtra("oldPid", -1);
        if (oldPid > 0)
            Process.killProcess(oldPid);
        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            Intent launch = getPackageManager().getLaunchIntentForPackage(getPackageName());
            if (launch != null) {
                launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
                startActivity(launch);
            }
            finishAndRemoveTask();
        }, 600);
    }
}
