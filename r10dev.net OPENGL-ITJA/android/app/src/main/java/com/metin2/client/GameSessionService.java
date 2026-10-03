package com.metin2.client;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;

// Keeps the process at foreground priority while a game session (and the embedded server)
// is alive, so Android does not reclaim it while the player is in another app.
public class GameSessionService extends Service {
    private static final String CHANNEL_ID = "m2session";
    private static final int NOTIFICATION_ID = 1;

    static void start(Context context) {
        Intent intent = new Intent(context, GameSessionService.class);
        if (Build.VERSION.SDK_INT >= 26)
            context.startForegroundService(intent);
        else
            context.startService(intent);
    }

    static void stop(Context context) {
        context.stopService(new Intent(context, GameSessionService.class));
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Notification.Builder builder;
        if (Build.VERSION.SDK_INT >= 26) {
            NotificationManager manager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
            manager.createNotificationChannel(new NotificationChannel(CHANNEL_ID, "Game session", NotificationManager.IMPORTANCE_LOW));
            builder = new Notification.Builder(this, CHANNEL_ID);
        } else {
            builder = new Notification.Builder(this);
        }
        Intent open = new Intent(this, MainActivity.class).setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent content = PendingIntent.getActivity(this, 0, open, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        builder.setSmallIcon(getApplicationInfo().icon)
            .setContentTitle(getApplicationInfo().loadLabel(getPackageManager()))
            .setContentText("Game running. Tap to return.")
            .setContentIntent(content)
            .setOngoing(true);
        startForeground(NOTIFICATION_ID, builder.build());
        return START_NOT_STICKY;
    }

    /** Swiping the game away kills the process; stop the server first so it saves players. */
    @Override
    public void onTaskRemoved(Intent rootIntent) {
        new Thread(() -> {
            MainActivity.stopEmbeddedServer();
            stopSelf();
            android.os.Process.killProcess(android.os.Process.myPid());
        }, "M2ServerStop").start();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
