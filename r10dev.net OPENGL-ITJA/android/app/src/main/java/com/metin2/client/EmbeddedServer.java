package com.metin2.client;

import android.content.Context;
import android.system.Os;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;

/**
 * Runs the m2dev server (db -> auth -> channel core) as child processes on
 * 127.0.0.1 for the embedded build profile.
 *
 * Executables ship as lib*.so in nativeLibraryDir (the only exec-allowed
 * location). Runtime data comes from the external pack in
 * files/server/{share,sqlite-seed}; per-process working dirs and the live
 * SQLite databases live in internal storage (symlinks, file locking).
 */
final class EmbeddedServer {
    private static final String TAG = "Metin2Server";
    private static final String HOST = "127.0.0.1";
    private static final int DB_PORT = 9000;
    private static final int START_TIMEOUT_MS = 120000;
    private static final String[] SHARE_DIRS = { "conf", "data", "locale", "mark", "package" };
    private static final String[] DATABASES = { "account", "player", "common", "log", "hotbackup" };

    private static final class Node {
        final String name;
        final String exe;
        final int port;
        final String config;
        Process process;

        Node(String name, String exe, int port, String config) {
            this.name = name;
            this.exe = exe;
            this.port = port;
            this.config = config;
        }
    }

    private final List<Node> mNodes = new ArrayList<Node>();
    private final File mRoot;
    private final File mPack;
    private final File mLogs;
    private final File mSqlite;
    private final String mLibDir;

    EmbeddedServer(Context context, int authPort, int channelPort) {
        mRoot = new File(context.getFilesDir(), "m2server");
        mPack = new File(context.getExternalFilesDir(null), "server");
        mLogs = new File(mPack, "logs");
        mSqlite = new File(mRoot, "sqlite");
        mLibDir = context.getApplicationInfo().nativeLibraryDir;

        mNodes.add(new Node("db", "libm2db.so", DB_PORT, null));
        mNodes.add(new Node("auth", "libm2game.so", authPort,
                "HOSTNAME: auth\nCHANNEL: 1\nPORT: " + authPort + "\nP2P_PORT: " + (authPort + 1000)
                        + "\nAUTH_SERVER: master\n"));
        mNodes.add(new Node("channel1_core1", "libm2game.so", channelPort,
                "HOSTNAME: channel1_1\nCHANNEL: 1\nPORT: " + channelPort + "\nP2P_PORT: " + (channelPort + 1000)
                        + "\nMAP_ALLOW: 1 4 5 6 3 23 43 112 107 67 68 72 208 302 304\n"));
    }

    /** Blocks until every process accepts connections. */
    synchronized boolean start() {
        try {
            if (!new File(mPack, "share/conf/game.txt").isFile()) {
                Log.e(TAG, "server data pack missing: " + mPack + " (run server/tools/push-server-pack.sh)");
                return false;
            }
            mLogs.mkdirs();
            killStale();
            seedDatabases();
            for (Node node : mNodes) {
                File dir = prepareDir(node);
                if (!waitPortFree(node.port)) {
                    Log.e(TAG, node.name + ": port " + node.port + " still in use");
                    return false;
                }
                node.process = launch(node, dir);
                if (!waitListening(node)) {
                    Log.e(TAG, node.name + " did not come up; see " + new File(mLogs, node.name + ".out"));
                    return false;
                }
                Log.i(TAG, node.name + " listening on " + HOST + ":" + node.port);
            }
            return true;
        } catch (Exception e) {
            Log.e(TAG, "embedded server start failed", e);
            return false;
        }
    }

    /** Graceful SIGTERM (cores save players, db flushes its cache), then SIGKILL. */
    synchronized void stop() {
        for (int i = mNodes.size() - 1; i >= 0; --i) {
            Node node = mNodes.get(i);
            int pid = readPid(node);
            if (node.process == null || pid <= 0)
                continue;
            android.os.Process.sendSignal(pid, 15);
            waitExit(node.process, 15000);
            node.process.destroy();
            node.process = null;
            Log.i(TAG, node.name + " stopped");
        }
    }

    private void seedDatabases() throws IOException {
        mSqlite.mkdirs();
        File seed = new File(mPack, "sqlite-seed");
        for (String db : DATABASES) {
            File dst = new File(mSqlite, db + ".sqlite3");
            if (dst.exists())
                continue;
            File src = new File(seed, db + ".sqlite3");
            if (!src.isFile())
                throw new IOException("missing seed database " + src);
            copy(src, dst);
            Log.i(TAG, "seeded " + dst);
        }
    }

    private File prepareDir(Node node) throws Exception {
        File dir = new File(mRoot, node.name);
        dir.mkdirs();
        new File(dir, "log").mkdirs();
        for (String share : SHARE_DIRS) {
            File target = new File(mPack, "share/" + share);
            target.mkdirs();
            symlink(target.getAbsolutePath(), new File(dir, share));
        }
        symlink(new File(mLogs, node.name + ".syslog.log").getAbsolutePath(), new File(dir, "syslog.log"));
        symlink(new File(mLogs, node.name + ".syserr.log").getAbsolutePath(), new File(dir, "syserr.log"));
        if (node.config != null) {
            OutputStream out = new FileOutputStream(new File(dir, "CONFIG"));
            try {
                out.write(node.config.getBytes("UTF-8"));
            } finally {
                out.close();
            }
        }
        return dir;
    }

    private Process launch(Node node, File dir) throws IOException {
        File exe = new File(mLibDir, node.exe);
        File out = new File(mLogs, node.name + ".out");
        List<String> cmd = new ArrayList<String>();
        cmd.add("/system/bin/sh");
        cmd.add("-c");
        cmd.add("exec \"$0\" \"$@\" > \"$M2_LOG\" 2>&1");
        cmd.add(exe.getAbsolutePath());
        if (node.config != null) {
            cmd.add("-I");
            cmd.add(HOST);
        }
        ProcessBuilder pb = new ProcessBuilder(cmd).directory(dir);
        pb.environment().put("M2_SQLITE_DIR", mSqlite.getAbsolutePath());
        pb.environment().put("M2_LOG", out.getAbsolutePath());
        Log.i(TAG, "starting " + node.name + ": " + exe + " in " + dir);
        return pb.start();
    }

    private boolean waitListening(Node node) throws InterruptedException {
        long deadline = System.currentTimeMillis() + START_TIMEOUT_MS;
        while (System.currentTimeMillis() < deadline) {
            if (!isAlive(node.process))
                return false;
            Socket s = new Socket();
            try {
                s.connect(new InetSocketAddress(HOST, node.port), 500);
                return true;
            } catch (IOException ignored) {
            } finally {
                try { s.close(); } catch (IOException ignored) { }
            }
            Thread.sleep(250);
        }
        return false;
    }

    private static boolean waitPortFree(int port) throws InterruptedException {
        for (int i = 0; i < 60; ++i) {
            ServerSocket s = null;
            try {
                s = new ServerSocket();
                s.setReuseAddress(true);
                s.bind(new InetSocketAddress(HOST, port));
                return true;
            } catch (IOException e) {
                Thread.sleep(500);
            } finally {
                if (s != null)
                    try { s.close(); } catch (IOException ignored) { }
            }
        }
        return false;
    }

    /** Processes left over from a previous app instance that was killed. */
    private void killStale() throws InterruptedException {
        for (Node node : mNodes) {
            int pid = readPid(node);
            if (pid <= 0 || !new File("/proc/" + pid + "/cmdline").exists())
                continue;
            String cmdline = readText(new File("/proc/" + pid + "/cmdline"));
            if (cmdline == null || !cmdline.contains(node.exe))
                continue;
            Log.w(TAG, "killing stale " + node.name + " pid " + pid);
            android.os.Process.sendSignal(pid, 15);
            for (int i = 0; i < 50 && new File("/proc/" + pid).exists(); ++i)
                Thread.sleep(200);
            android.os.Process.killProcess(pid);
        }
    }

    private int readPid(Node node) {
        String s = readText(new File(new File(mRoot, node.name), "pid"));
        if (s == null)
            return -1;
        try {
            return Integer.parseInt(s.trim());
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    private static boolean isAlive(Process p) {
        try {
            p.exitValue();
            return false;
        } catch (IllegalThreadStateException e) {
            return true;
        }
    }

    private static void waitExit(Process p, long timeoutMs) {
        long deadline = System.currentTimeMillis() + timeoutMs;
        while (isAlive(p) && System.currentTimeMillis() < deadline) {
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
                return;
            }
        }
    }

    private static void symlink(String target, File link) throws Exception {
        link.delete();
        Os.symlink(target, link.getAbsolutePath());
    }

    private static String readText(File f) {
        if (!f.isFile())
            return null;
        try {
            InputStream in = new FileInputStream(f);
            try {
                byte[] buf = new byte[4096];
                int n = in.read(buf);
                return n <= 0 ? "" : new String(buf, 0, n, "UTF-8");
            } finally {
                in.close();
            }
        } catch (IOException e) {
            return null;
        }
    }

    private static void copy(File src, File dst) throws IOException {
        File tmp = new File(dst.getPath() + ".tmp");
        InputStream in = new FileInputStream(src);
        try {
            OutputStream out = new FileOutputStream(tmp);
            try {
                byte[] buf = new byte[65536];
                int n;
                while ((n = in.read(buf)) > 0)
                    out.write(buf, 0, n);
            } finally {
                out.close();
            }
        } finally {
            in.close();
        }
        if (!tmp.renameTo(dst))
            throw new IOException("rename " + tmp + " -> " + dst);
    }
}
