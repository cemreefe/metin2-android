package com.metin2.client;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

final class DataInstaller {
    interface Progress {
        void onProgress(long done, long total);
    }

    private static final String VERSION_FILE = ".m2data_version";

    static boolean isInstalled(File dataDir) {
        if (BuildConfig.M2_DATA_URL.isEmpty())
            return true;
        return installedVersion(dataDir) == BuildConfig.M2_DATA_VERSION;
    }

    static int installedVersion(File dataDir) {
        File f = new File(dataDir, VERSION_FILE);
        if (!f.exists())
            return 0;
        try {
            InputStream in = new java.io.FileInputStream(f);
            byte[] buf = new byte[16];
            int n = in.read(buf);
            in.close();
            return n > 0 ? Integer.parseInt(new String(buf, 0, n).trim()) : 0;
        } catch (IOException | NumberFormatException e) {
            return 0;
        }
    }

    static void install(File dataDir, final Progress progress) throws IOException {
        HttpURLConnection conn = (HttpURLConnection) new URL(resolveDataUrl(BuildConfig.M2_DATA_URL)).openConnection();
        conn.setConnectTimeout(15000);
        conn.setReadTimeout(60000);
        if (conn.getResponseCode() != HttpURLConnection.HTTP_OK)
            throw new IOException("HTTP " + conn.getResponseCode());
        final long total = conn.getContentLengthLong();
        final long[] done = { 0 };
        InputStream counting = new FilterInputStream(new BufferedInputStream(conn.getInputStream(), 1 << 16)) {
            private long lastReport;

            @Override
            public int read(byte[] b, int off, int len) throws IOException {
                int n = super.read(b, off, len);
                if (n > 0) {
                    done[0] += n;
                    if (done[0] - lastReport > (1 << 20)) {
                        lastReport = done[0];
                        progress.onProgress(done[0], total);
                    }
                }
                return n;
            }
        };

        String root = dataDir.getCanonicalPath() + File.separator;
        byte[] buf = new byte[1 << 16];
        ZipInputStream zip = new ZipInputStream(counting);
        try {
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                if (entry.getName().equals(VERSION_FILE))
                    continue;
                File out = new File(dataDir, entry.getName());
                if (!out.getCanonicalPath().startsWith(root))
                    throw new IOException("bad zip entry " + entry.getName());
                if (entry.isDirectory()) {
                    out.mkdirs();
                    continue;
                }
                out.getParentFile().mkdirs();
                OutputStream os = new FileOutputStream(out);
                int n;
                while ((n = zip.read(buf)) > 0)
                    os.write(buf, 0, n);
                os.close();
            }
        } finally {
            zip.close();
            conn.disconnect();
        }

        OutputStream os = new FileOutputStream(new File(dataDir, VERSION_FILE));
        os.write(String.valueOf(BuildConfig.M2_DATA_VERSION).getBytes());
        os.close();
        progress.onProgress(total, total);
    }

    // A ".txt" data URL is a pointer file whose first line is the real archive URL,
    // so the archive host can change without rebuilding the APK.
    private static String resolveDataUrl(String url) throws IOException {
        if (!url.endsWith(".txt"))
            return url;
        HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
        conn.setConnectTimeout(15000);
        conn.setReadTimeout(30000);
        try {
            if (conn.getResponseCode() != HttpURLConnection.HTTP_OK)
                throw new IOException("HTTP " + conn.getResponseCode() + " for " + url);
            java.io.BufferedReader r = new java.io.BufferedReader(new java.io.InputStreamReader(conn.getInputStream()));
            String line = r.readLine();
            r.close();
            if (line == null || line.trim().isEmpty())
                throw new IOException("empty data pointer " + url);
            return line.trim();
        } finally {
            conn.disconnect();
        }
    }

    static void writeServerProfile(File dataDir) throws IOException {
        String host = BuildConfig.M2_SERVER_HOST;
        if (BuildConfig.M2_SERVER_MODE.equals("embedded"))
            host = "127.0.0.1";
        String ip = java.net.InetAddress.getByName(host).getHostAddress();
        String py = "SERVER_IP = \"" + ip + "\"\n"
                + "PORT_AUTH = " + BuildConfig.M2_AUTH_PORT + "\n"
                + "PORT_1 = " + BuildConfig.M2_CHANNEL_PORT + "\n";
        OutputStream os = new FileOutputStream(new File(dataDir, "m2profile.py"));
        os.write(py.getBytes());
        os.close();
    }
}
