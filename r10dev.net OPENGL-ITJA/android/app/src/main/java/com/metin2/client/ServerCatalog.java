package com.metin2.client;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;

// Login-screen server list: android/servers/servers.json baked into the APK, optionally replaced by a
// newer copy fetched from M2_SERVER_LIST_URL. The baked list is always the fallback.
final class ServerCatalog {
    static final class Server {
        final String name;
        final String host;
        final int authPort;
        final int channelPort;
        final int gamePortOffset;
        final boolean embedded;

        Server(String name, String host, int authPort, int channelPort, int gamePortOffset, boolean embedded) {
            this.name = name;
            this.host = host;
            this.authPort = authPort;
            this.channelPort = channelPort;
            this.gamePortOffset = gamePortOffset;
            this.embedded = embedded;
        }
    }

    private static final String ASSET = "servers.json";
    private static final String CACHE = "servers.json";
    private static final int MAX_BYTES = 64 * 1024;
    private static final Pattern NAME = Pattern.compile("[A-Za-z0-9 _-]{1,24}");
    private static final Pattern HOST = Pattern.compile("[A-Za-z0-9.-]{1,253}");

    private ServerCatalog() {
    }

    static List<Server> load(Context context) {
        String baked;
        try {
            baked = read(context.getAssets().open(ASSET));
        } catch (IOException e) {
            throw new IllegalStateException("missing baked " + ASSET, e);
        }
        String json = baked;
        File cache = new File(context.getFilesDir(), CACHE);
        if (cache.isFile()) {
            try {
                String cached = read(new FileInputStream(cache));
                parse(cached);
                if (version(cached) >= version(baked))
                    json = cached;
            } catch (IOException | JSONException e) {
                android.util.Log.w("Metin2Mobile", "ignoring cached server list: " + e);
            }
        }
        try {
            return parse(json);
        } catch (JSONException e) {
            throw new IllegalStateException("bad baked " + ASSET, e);
        }
    }

    static void refresh(Context context) {
        if (BuildConfig.M2_SERVER_LIST_URL.isEmpty() || !BuildConfig.M2_SERVER_LIST_URL.startsWith("https://"))
            return;
        HttpURLConnection conn = null;
        try {
            conn = (HttpURLConnection) new URL(BuildConfig.M2_SERVER_LIST_URL).openConnection();
            conn.setConnectTimeout(3000);
            conn.setReadTimeout(3000);
            if (conn.getResponseCode() != HttpURLConnection.HTTP_OK)
                return;
            String fetched = read(conn.getInputStream());
            parse(fetched);
            String baked = read(context.getAssets().open(ASSET));
            if (version(fetched) < version(baked))
                return;
            File cache = new File(context.getFilesDir(), CACHE);
            File tmp = new File(context.getFilesDir(), CACHE + ".tmp");
            FileOutputStream out = new FileOutputStream(tmp);
            try {
                out.write(fetched.getBytes("UTF-8"));
            } finally {
                out.close();
            }
            if (!tmp.renameTo(cache))
                throw new IOException("rename " + tmp);
        } catch (IOException | JSONException e) {
            android.util.Log.i("Metin2Mobile", "server list refresh skipped: " + e);
        } finally {
            if (conn != null)
                conn.disconnect();
        }
    }

    static List<Server> parse(String json) throws JSONException {
        JSONArray array = new JSONObject(json).getJSONArray("servers");
        List<Server> servers = new ArrayList<>();
        for (int i = 0; i < array.length(); ++i) {
            JSONObject o = array.getJSONObject(i);
            String name = o.getString("name");
            if (!NAME.matcher(name).matches())
                throw new JSONException("bad server name " + name);
            if (o.optBoolean("embedded", false)) {
                servers.add(new Server(name, "127.0.0.1", BuildConfig.M2_AUTH_PORT, BuildConfig.M2_CHANNEL_PORT, 0, true));
                continue;
            }
            String host = o.getString("host");
            if (!HOST.matcher(host).matches())
                throw new JSONException("bad host " + host);
            servers.add(new Server(name, host, port(o, "authPort"), port(o, "channelPort"),
                    o.has("gamePortOffset") ? port(o, "gamePortOffset") : 0, false));
        }
        if (servers.isEmpty())
            throw new JSONException("empty server list");
        return servers;
    }

    private static int port(JSONObject o, String key) throws JSONException {
        int port = o.getInt(key);
        if (port < 1 || port > 65535)
            throw new JSONException("bad " + key + " " + port);
        return port;
    }

    private static int version(String json) throws JSONException {
        return new JSONObject(json).getInt("version");
    }

    private static String read(InputStream in) throws IOException {
        try {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[4096];
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
                if (out.size() > MAX_BYTES)
                    throw new IOException("server list too large");
            }
            return out.toString("UTF-8");
        } finally {
            in.close();
        }
    }
}
