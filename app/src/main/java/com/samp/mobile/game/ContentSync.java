package com.samp.mobile.game;

import android.content.Context;
import android.util.Log;

import com.samp.mobile.BuildConfig;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Downloads the server's own content (server/content/, see docs/CONTENT.md)
 * into the app's external files folder under content/: a logo, the start
 * screen background and sounds. The server's manifest.json lists each file's
 * size and SHA-256; a file is only put in place after both match, and files
 * the server no longer lists are deleted.
 *
 * The native client plays a sound from this folder instead of streaming it
 * when the server's audio stream URL points into the content service
 * (audiostream.cpp).
 */
final class ContentSync {
    private static final String TAG = "ContentSync";

    static final int MAX_FILES = 500;
    static final long MAX_FILE_BYTES = 50L * 1024 * 1024;
    static final long MAX_TOTAL_BYTES = 300L * 1024 * 1024;
    private static final int CONNECT_TIMEOUT_MS = 5000;
    private static final int READ_TIMEOUT_MS = 15000;
    private static final String INDEX = ".index.json";
    private static final String TMP_DIR = ".tmp";

    /** Letters, digits, _ - . / only; no leading '.' or '/'. */
    private static final Pattern SAFE_PATH = Pattern.compile("[A-Za-z0-9_-][A-Za-z0-9_./-]*");

    interface Progress {
        void onProgress(long doneBytes, long totalBytes, int doneFiles, int totalFiles);
    }

    static final class Result {
        final boolean reachedServer;
        final int downloaded;
        final int removed;
        final String error;

        Result(boolean reachedServer, int downloaded, int removed, String error) {
            this.reachedServer = reachedServer;
            this.downloaded = downloaded;
            this.removed = removed;
            this.error = error;
        }
    }

    static final class Entry {
        final String path;
        final long size;
        final String sha256;

        Entry(String path, long size, String sha256) {
            this.path = path;
            this.size = size;
            this.sha256 = sha256;
        }
    }

    private ContentSync() {}

    /** http://host:port of the content service, from the build's server address. */
    static String baseUrl() {
        return "http://" + BuildConfig.KM_SERVER_HOST + ":" + BuildConfig.KM_CONTENT_PORT;
    }

    static File dir(Context context) {
        return new File(context.getExternalFilesDir(null), "content");
    }

    /** A downloaded file, or null if the server doesn't have it. */
    static File file(Context context, String path) {
        File f = new File(dir(context), path);
        return f.isFile() ? f : null;
    }

    static boolean isSafePath(String path) {
        if (path == null || path.isEmpty() || path.length() > 128) return false;
        if (!SAFE_PATH.matcher(path).matches()) return false;
        if (path.contains("..") || path.contains("//") || path.endsWith("/")) return false;
        for (String part : path.split("/")) {
            if (part.isEmpty() || part.startsWith(".")) return false;
        }
        return true;
    }

    /** Parses and checks manifest.json. Throws on anything unexpected. */
    static List<Entry> parseManifest(String json) throws JSONException {
        JSONObject root = new JSONObject(json);
        JSONArray files = root.getJSONArray("files");
        if (files.length() > MAX_FILES) throw new JSONException("too many files: " + files.length());
        List<Entry> entries = new ArrayList<>();
        Set<String> seen = new HashSet<>();
        long total = 0;
        for (int i = 0; i < files.length(); i++) {
            JSONObject f = files.getJSONObject(i);
            String path = f.getString("path");
            long size = f.getLong("size");
            String sha = f.getString("sha256").toLowerCase();
            if (!isSafePath(path)) throw new JSONException("bad path: " + path);
            if (!seen.add(path.toLowerCase())) throw new JSONException("duplicate path: " + path);
            if (size < 0 || size > MAX_FILE_BYTES) throw new JSONException("bad size for " + path + ": " + size);
            if (!sha.matches("[0-9a-f]{64}")) throw new JSONException("bad sha256 for " + path);
            total += size;
            if (total > MAX_TOTAL_BYTES) throw new JSONException("content is larger than " + MAX_TOTAL_BYTES + " bytes");
            entries.add(new Entry(path, size, sha));
        }
        return entries;
    }

    /** Brings content/ in line with the server. Never throws; a failure keeps the old files. */
    static Result sync(Context context, Progress progress) {
        File dir = dir(context);
        if (!dir.isDirectory() && !dir.mkdirs()) return new Result(false, 0, 0, "can't create " + dir);
        List<Entry> entries;
        try {
            String json = new String(get(baseUrl() + "/manifest.json", 1024 * 1024), StandardCharsets.UTF_8);
            entries = parseManifest(json);
        } catch (IOException | JSONException e) {
            Log.w(TAG, "no content manifest from " + baseUrl() + ": " + e);
            return new Result(false, 0, 0, e.toString());
        }

        Map<String, String> index = readIndex(dir);
        List<Entry> needed = new ArrayList<>();
        long totalBytes = 0;
        for (Entry e : entries) {
            File f = new File(dir, e.path);
            if (e.sha256.equals(index.get(e.path)) && f.isFile() && f.length() == e.size) continue;
            needed.add(e);
            totalBytes += e.size;
        }

        int downloaded = 0;
        long doneBytes = 0;
        String error = null;
        File tmpDir = new File(dir, TMP_DIR);
        if (progress != null && !needed.isEmpty()) progress.onProgress(0, totalBytes, 0, needed.size());
        for (Entry e : needed) {
            try {
                download(e, dir, tmpDir);
                index.put(e.path, e.sha256);
                writeIndex(dir, index);
                downloaded++;
            } catch (IOException ex) {
                Log.w(TAG, "download of " + e.path + " failed: " + ex);
                index.remove(e.path);
                error = ex.toString();
            }
            doneBytes += e.size;
            if (progress != null) progress.onProgress(doneBytes, totalBytes, downloaded, needed.size());
        }
        deleteRecursive(tmpDir);

        // Remove what the server no longer has.
        Set<String> keep = new HashSet<>();
        for (Entry e : entries) keep.add(e.path);
        int removed = removeUnlisted(dir, dir, keep);
        index.keySet().retainAll(keep);
        writeIndex(dir, index);

        Log.i(TAG, "content: " + entries.size() + " files, " + downloaded + " downloaded, " + removed + " removed");
        return new Result(true, downloaded, removed, error);
    }

    private static void download(Entry e, File dir, File tmpDir) throws IOException {
        if (!tmpDir.isDirectory() && !tmpDir.mkdirs()) throw new IOException("can't create " + tmpDir);
        File tmp = new File(tmpDir, "dl-" + System.nanoTime());
        MessageDigest sha;
        try {
            sha = MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException ex) {
            throw new IOException(ex);
        }
        HttpURLConnection c = open(baseUrl() + "/" + e.path);
        long written = 0;
        try (InputStream in = c.getInputStream(); OutputStream out = new FileOutputStream(tmp)) {
            byte[] buf = new byte[64 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) {
                written += n;
                if (written > e.size) throw new IOException(e.path + " is larger than the manifest says");
                sha.update(buf, 0, n);
                out.write(buf, 0, n);
            }
        } catch (IOException ex) {
            tmp.delete();
            throw ex;
        } finally {
            c.disconnect();
        }
        if (written != e.size || !hex(sha.digest()).equals(e.sha256)) {
            tmp.delete();
            throw new IOException(e.path + " doesn't match the manifest");
        }
        File target = new File(dir, e.path);
        File parent = target.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            tmp.delete();
            throw new IOException("can't create " + parent);
        }
        if (target.exists() && !target.delete()) {
            tmp.delete();
            throw new IOException("can't replace " + target);
        }
        if (!tmp.renameTo(target)) {
            tmp.delete();
            throw new IOException("can't move " + e.path + " into place");
        }
    }

    private static HttpURLConnection open(String url) throws IOException {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(CONNECT_TIMEOUT_MS);
        c.setReadTimeout(READ_TIMEOUT_MS);
        c.setInstanceFollowRedirects(false);
        c.setUseCaches(false);
        int code = c.getResponseCode();
        if (code != HttpURLConnection.HTTP_OK) {
            c.disconnect();
            throw new IOException("HTTP " + code + " for " + url);
        }
        return c;
    }

    private static byte[] get(String url, int maxBytes) throws IOException {
        HttpURLConnection c = open(url);
        try (InputStream in = c.getInputStream()) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[16 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) {
                if (out.size() + n > maxBytes) throw new IOException(url + " is too large");
                out.write(buf, 0, n);
            }
            return out.toByteArray();
        } finally {
            c.disconnect();
        }
    }

    private static Map<String, String> readIndex(File dir) {
        Map<String, String> index = new HashMap<>();
        File f = new File(dir, INDEX);
        if (!f.isFile() || f.length() > 1024 * 1024) return index;
        try (InputStream in = new FileInputStream(f)) {
            byte[] data = new byte[(int) f.length()];
            int off = 0, n;
            while (off < data.length && (n = in.read(data, off, data.length - off)) > 0) off += n;
            JSONObject o = new JSONObject(new String(data, 0, off, StandardCharsets.UTF_8));
            JSONArray names = o.names();
            if (names != null) {
                for (int i = 0; i < names.length(); i++) {
                    String k = names.getString(i);
                    if (isSafePath(k)) index.put(k, o.getString(k));
                }
            }
        } catch (IOException | JSONException e) {
            Log.w(TAG, "content index unreadable; checking every file again: " + e);
        }
        return index;
    }

    private static void writeIndex(File dir, Map<String, String> index) {
        File f = new File(dir, INDEX);
        File tmp = new File(dir, INDEX + ".tmp");
        try (OutputStream out = new FileOutputStream(tmp)) {
            out.write(new JSONObject(index).toString().getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) {
            Log.w(TAG, "can't write the content index: " + e);
            return;
        }
        if (!tmp.renameTo(f)) Log.w(TAG, "can't replace the content index");
    }

    private static int removeUnlisted(File root, File dir, Set<String> keep) {
        File[] children = dir.listFiles();
        if (children == null) return 0;
        int removed = 0;
        for (File child : children) {
            if (child.getName().startsWith(".")) continue;
            String rel = root.toURI().relativize(child.toURI()).getPath();
            if (child.isDirectory()) {
                removed += removeUnlisted(root, child, keep);
                String[] left = child.list();
                if (left != null && left.length == 0) child.delete();
            } else if (!keep.contains(rel)) {
                if (child.delete()) removed++;
            }
        }
        return removed;
    }

    private static void deleteRecursive(File f) {
        File[] children = f.listFiles();
        if (children != null) for (File c : children) deleteRecursive(c);
        f.delete();
    }

    private static String hex(byte[] bytes) {
        StringBuilder sb = new StringBuilder(bytes.length * 2);
        for (byte b : bytes) sb.append(String.format("%02x", b & 0xff));
        return sb.toString();
    }
}
