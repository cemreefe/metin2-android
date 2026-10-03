package com.metin2.client;

import android.content.Context;
import android.os.Build;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

final class DataInstaller {
    interface Progress {
        void onProgress(long done, long total);
    }

    private static final String VERSION_FILE = ".m2data_version";
    private static final String BUNDLED_ASSET = "m2data.zip";

    static boolean isInstalled(File dataDir) {
        if (!BuildConfig.M2_DATA_BUNDLED && BuildConfig.M2_DATA_URL.isEmpty())
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

    static void install(android.content.Context context, File dataDir, Progress progress) throws IOException {
        if (BuildConfig.M2_DATA_BUNDLED) {
            android.content.res.AssetFileDescriptor fd = context.getAssets().openFd(BUNDLED_ASSET);
            long total = fd.getLength();
            fd.close();
            extract(context.getAssets().open(BUNDLED_ASSET), total, dataDir, progress);
            return;
        }
        HttpURLConnection conn = (HttpURLConnection) new URL(resolveDataUrl(BuildConfig.M2_DATA_URL)).openConnection();
        conn.setConnectTimeout(15000);
        conn.setReadTimeout(60000);
        try {
            if (conn.getResponseCode() != HttpURLConnection.HTTP_OK)
                throw new IOException("HTTP " + conn.getResponseCode());
            long total = Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                    ? conn.getContentLengthLong() : conn.getContentLength();
            extract(conn.getInputStream(), total, dataDir, progress);
        } finally {
            conn.disconnect();
        }
    }

    private static void extract(InputStream source, final long total, File dataDir, final Progress progress) throws IOException {
        final long[] done = { 0 };
        InputStream counting = new FilterInputStream(new BufferedInputStream(source, 1 << 16)) {
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

    // m2profile.py feeds serverinfo.py: SERVER_IP/PORT_* for the first server, SERVERS for the login list.
    static void writeServerProfile(Context context, File dataDir) throws IOException {
        boolean embeddedBuild = "embedded".equals(BuildConfig.M2_SERVER_MODE);
        List<ServerCatalog.Server> servers = new ArrayList<>();
        boolean profileListed = embeddedBuild || BuildConfig.M2_SERVER_HOST.isEmpty();
        for (ServerCatalog.Server s : ServerCatalog.load(context)) {
            if (s.embedded && !embeddedBuild)
                continue;
            if (s.host.equals(BuildConfig.M2_SERVER_HOST) && s.authPort == BuildConfig.M2_AUTH_PORT)
                profileListed = true;
            servers.add(s);
        }
        if (!profileListed)
            servers.add(0, new ServerCatalog.Server("Dev", BuildConfig.M2_SERVER_HOST, BuildConfig.M2_AUTH_PORT,
                    BuildConfig.M2_CHANNEL_PORT, BuildConfig.M2_GAME_PORT_OFFSET, false));

        StringBuilder list = new StringBuilder("SERVERS = [\n");
        String firstIp = null;
        for (ServerCatalog.Server s : servers) {
            String ip = resolve(s.host);
            if (firstIp == null)
                firstIp = ip;
            list.append("\t(\"").append(s.name).append("\", \"").append(ip).append("\", ")
                    .append(s.authPort).append(", ").append(s.channelPort).append("),\n");
            if (s.gamePortOffset != 0) {
                try {
                    android.system.Os.setenv("M2_GAME_HOST", ip, true);
                    android.system.Os.setenv("M2_GAME_PORT_OFFSET", Integer.toString(s.gamePortOffset), true);
                } catch (android.system.ErrnoException e) {
                    throw new IOException("setenv failed", e);
                }
            }
        }
        list.append("]\n");
        ServerCatalog.Server first = servers.get(0);
        String py = "SERVER_IP = \"" + firstIp + "\"\n"
                + "PORT_AUTH = " + first.authPort + "\n"
                + "PORT_1 = " + first.channelPort + "\n"
                + list;
        OutputStream os = new FileOutputStream(new File(dataDir, "m2profile.py"));
        os.write(py.getBytes());
        os.close();
    }

    private static String resolve(String host) {
        try {
            return java.net.InetAddress.getByName(host).getHostAddress();
        } catch (java.net.UnknownHostException e) {
            return host;
        }
    }
}
