package com.metin2.client;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.net.HttpURLConnection;
import java.net.URL;

// Test-build plumbing against the developer's server (m2.devUrl): crash upload and update check.
final class DevReporter {
    static final class Update {
        final int build;
        final String url;

        Update(int build, String url) {
            this.build = build;
            this.url = url;
        }
    }

    private static final String TAG = "Metin2Mobile";
    private static final int TIMEOUT_MS = 10000;

    private DevReporter() {
    }

    static boolean enabled() {
        return BuildConfig.M2_DEV_URL.length() > 0;
    }

    static void uploadPreviousRun(File dataDir) {
        if (!enabled() || !new File(dataDir, "syserr.txt").exists())
            return;
        upload(dataDir, new File(dataDir, "crash.txt").exists() ? "crash" : "log", "");
    }

    /** Posts the current logs under an explicit kind, e.g. a startup that never finished. */
    static void upload(File dataDir, String kind, String note) {
        File crash = new File(dataDir, "crash.txt");
        if (!enabled())
            return;
        boolean crashed = "crash".equals(kind);
        StringBuilder report = new StringBuilder();
        report.append("build ").append(BuildConfig.VERSION_NAME)
                .append(" device ").append(android.os.Build.MANUFACTURER).append(' ').append(android.os.Build.MODEL)
                .append(" android ").append(android.os.Build.VERSION.RELEASE).append('\n');
        if (note.length() > 0)
            report.append(note).append('\n');
        appendTail(report, crash, 64 * 1024);
        appendTail(report, new File(dataDir, "syserr.txt"), 32 * 1024);
        appendTail(report, new File(dataDir, "stderr.txt"), 32 * 1024);
        File serverLogs = new File(new File(dataDir, "server"), "logs");
        appendTail(report, new File(serverLogs, "db.out"), 8 * 1024);
        appendTail(report, new File(serverLogs, "auth.out"), 8 * 1024);
        appendTail(report, new File(serverLogs, "channel1_core1.out"), 8 * 1024);
        appendLogcat(report);
        try {
            HttpURLConnection conn = open("/crash?build=" + BuildConfig.VERSION_CODE + "&kind=" + kind);
            conn.setRequestMethod("POST");
            conn.setDoOutput(true);
            conn.setRequestProperty("Content-Type", "text/plain; charset=utf-8");
            OutputStream out = conn.getOutputStream();
            out.write(report.toString().getBytes("UTF-8"));
            out.close();
            int code = conn.getResponseCode();
            conn.disconnect();
            if (code == 200 && crashed && !crash.renameTo(new File(dataDir, "crash.sent.txt")))
                crash.delete();
        } catch (IOException e) {
            android.util.Log.w(TAG, "crash upload: " + e);
        }
    }

    static Update checkForUpdate() {
        if (!enabled())
            return null;
        try {
            HttpURLConnection conn = open("/latest-" + BuildConfig.M2_UPDATE_CHANNEL + ".txt");
            if (conn.getResponseCode() != 200)
                return null;
            BufferedReader reader = new BufferedReader(new InputStreamReader(conn.getInputStream(), "UTF-8"));
            String line = reader.readLine();
            reader.close();
            conn.disconnect();
            if (line == null)
                return null;
            String[] parts = line.trim().split("\\s+");
            if (parts.length < 2)
                return null;
            int build = Integer.parseInt(parts[0]);
            return build > BuildConfig.VERSION_CODE ? new Update(build, parts[1]) : null;
        } catch (IOException | NumberFormatException e) {
            android.util.Log.w(TAG, "update check: " + e);
            return null;
        }
    }

    private static HttpURLConnection open(String path) throws IOException {
        HttpURLConnection conn = (HttpURLConnection) new URL(BuildConfig.M2_DEV_URL + path).openConnection();
        conn.setConnectTimeout(TIMEOUT_MS);
        conn.setReadTimeout(TIMEOUT_MS);
        return conn;
    }

    private static void appendLogcat(StringBuilder out) {
        out.append("\n===== logcat =====\n");
        try {
            Process proc = Runtime.getRuntime().exec(new String[] { "logcat", "-d", "-v", "threadtime", "-t", "3000" });
            BufferedReader reader = new BufferedReader(new InputStreamReader(proc.getInputStream(), "UTF-8"));
            String line;
            while ((line = reader.readLine()) != null)
                out.append(line).append('\n');
            reader.close();
        } catch (IOException e) {
            out.append("<logcat unavailable: ").append(e).append(">\n");
        }
    }

    private static void appendTail(StringBuilder out, File file, int maxBytes) {
        if (!file.exists())
            return;
        out.append("\n===== ").append(file.getName()).append(" =====\n");
        try {
            RandomAccessFile raf = new RandomAccessFile(file, "r");
            long start = Math.max(0, raf.length() - maxBytes);
            byte[] buf = new byte[(int) (raf.length() - start)];
            raf.seek(start);
            raf.readFully(buf);
            raf.close();
            out.append(new String(buf, "UTF-8"));
        } catch (IOException e) {
            out.append("<unreadable: ").append(e).append(">\n");
        }
    }
}
