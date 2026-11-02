package com.samp.mobile.game;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.util.Log;

import androidx.core.content.FileProvider;

import com.samp.mobile.BuildConfig;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;

/**
 * Over-the-air updates: the launcher asks GitHub for the newest release of
 * this app (KM_UPDATE_REPO in app/build.gradle). If its version is higher than
 * this build's, it offers to download the release's .apk and install it over
 * this one. Android only installs it if it's signed with the same key, and the
 * download is checked against the release's size and SHA-256 first.
 *
 * Only release builds (versionName vX.Y.Z, from the Release APK workflow)
 * update; debug and test builds never do.
 */
final class Updater {
    private static final String TAG = "Updater";
    private static final int CONNECT_TIMEOUT_MS = 5000;
    private static final int READ_TIMEOUT_MS = 20000;
    private static final long MAX_APK_BYTES = 200L * 1024 * 1024;

    static final class Release {
        final String version;     // "v1.5.0"
        final String notes;
        final String apkUrl;
        final long apkSize;
        final String sha256;      // lower-case hex, or null if GitHub didn't give one

        Release(String version, String notes, String apkUrl, long apkSize, String sha256) {
            this.version = version;
            this.notes = notes;
            this.apkUrl = apkUrl;
            this.apkSize = apkSize;
            this.sha256 = sha256;
        }
    }

    interface Progress {
        void onProgress(long done, long total);
    }

    /** Checked once per process: a decline lasts until the app is reopened. */
    static volatile boolean checkedThisProcess = false;

    private Updater() {}

    static boolean enabled() {
        return !BuildConfig.KM_UPDATE_REPO.isEmpty() && parse(BuildConfig.VERSION_NAME) != null;
    }

    /** "v1.4.0" or "1.4" -> {1, 4, 0}; null if it isn't a version number. */
    static int[] parse(String version) {
        if (version == null) return null;
        String v = version.trim();
        if (v.startsWith("v") || v.startsWith("V")) v = v.substring(1);
        int dash = v.indexOf('-');
        if (dash >= 0) v = v.substring(0, dash);
        String[] parts = v.split("\\.");
        if (parts.length == 0 || parts.length > 4) return null;
        int[] out = new int[4];
        for (int i = 0; i < parts.length; i++) {
            if (!parts[i].matches("\\d{1,6}")) return null;
            out[i] = Integer.parseInt(parts[i]);
        }
        return out;
    }

    /** True if version a is newer than b. */
    static boolean newer(String a, String b) {
        int[] x = parse(a), y = parse(b);
        if (x == null || y == null) return false;
        for (int i = 0; i < 4; i++) if (x[i] != y[i]) return x[i] > y[i];
        return false;
    }

    /**
     * The newest release (by version number, pre-releases included, drafts
     * not) that has an .apk, if it's newer than this build; else null.
     * Never throws.
     */
    static Release check() {
        if (!enabled()) return null;
        try {
            String json = new String(get("https://api.github.com/repos/" + BuildConfig.KM_UPDATE_REPO + "/releases?per_page=20", 2 * 1024 * 1024),
                StandardCharsets.UTF_8);
            Release best = pickNewest(new JSONArray(json));
            if (best == null || !newer(best.version, BuildConfig.VERSION_NAME)) {
                Log.i(TAG, "up to date (" + BuildConfig.VERSION_NAME + ", newest " + (best == null ? "none" : best.version) + ")");
                return null;
            }
            Log.i(TAG, "update available: " + best.version + " (" + best.apkSize + " bytes)");
            return best;
        } catch (IOException | JSONException e) {
            Log.w(TAG, "update check failed: " + e);
        }
        return null;
    }

    /** From GitHub's release list: the highest version with a usable .apk. */
    static Release pickNewest(JSONArray releases) throws JSONException {
        Release best = null;
        for (int i = 0; i < releases.length(); i++) {
            JSONObject r = releases.getJSONObject(i);
            if (r.optBoolean("draft")) continue;
            String tag = r.optString("tag_name", "");
            if (parse(tag) == null || (best != null && !newer(tag, best.version))) continue;
            JSONArray assets = r.optJSONArray("assets");
            if (assets == null) continue;
            for (int j = 0; j < assets.length(); j++) {
                JSONObject a = assets.getJSONObject(j);
                String name = a.optString("name", "");
                long size = a.optLong("size", 0);
                String url = a.optString("browser_download_url", "");
                if (!name.toLowerCase().endsWith(".apk") || size <= 0 || size > MAX_APK_BYTES || !url.startsWith("https://github.com/")) continue;
                String digest = a.optString("digest", "");
                String sha = digest.startsWith("sha256:") ? digest.substring(7).toLowerCase() : null;
                String notes = r.optString("body", "");
                if (notes.length() > 400) notes = notes.substring(0, 400) + "…";
                best = new Release(tag, notes, url, size, sha);
                break;
            }
        }
        return best;
    }

    static File apkFile(Context context) {
        File dir = context.getExternalCacheDir() != null ? context.getExternalCacheDir() : context.getCacheDir();
        return new File(dir, "update.apk");
    }

    /** Downloads and checks the release's .apk. Throws with a message for the player. */
    static File download(Context context, Release release, Progress progress) throws IOException {
        File out = apkFile(context);
        File tmp = new File(out.getPath() + ".part");
        MessageDigest sha;
        try {
            sha = MessageDigest.getInstance("SHA-256");
        } catch (Exception e) {
            throw new IOException(e);
        }
        HttpURLConnection c = open(release.apkUrl);
        long done = 0;
        try (InputStream in = c.getInputStream(); OutputStream os = new FileOutputStream(tmp)) {
            byte[] buf = new byte[64 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) {
                done += n;
                if (done > release.apkSize) throw new IOException("the download is larger than the release says");
                sha.update(buf, 0, n);
                os.write(buf, 0, n);
                if (progress != null) progress.onProgress(done, release.apkSize);
            }
        } catch (IOException e) {
            tmp.delete();
            throw e;
        } finally {
            c.disconnect();
        }
        if (done != release.apkSize) {
            tmp.delete();
            throw new IOException("the download was cut off");
        }
        if (release.sha256 != null && !hex(sha.digest()).equals(release.sha256)) {
            tmp.delete();
            throw new IOException("the download doesn't match the release");
        }
        if (!sameSigner(context, tmp)) {
            tmp.delete();
            throw new IOException("the update isn't signed with this app's key");
        }
        if (out.exists()) out.delete();
        if (!tmp.renameTo(out)) throw new IOException("can't save the update");
        return out;
    }

    /** The downloaded .apk is this app, signed with the same certificate. */
    @SuppressWarnings("deprecation")
    static boolean sameSigner(Context context, File apk) {
        PackageManager pm = context.getPackageManager();
        int flags = Build.VERSION.SDK_INT >= 28 ? PackageManager.GET_SIGNING_CERTIFICATES : PackageManager.GET_SIGNATURES;
        PackageInfo theirs = pm.getPackageArchiveInfo(apk.getPath(), flags);
        if (theirs == null || !context.getPackageName().equals(theirs.packageName)) return false;
        try {
            PackageInfo ours = pm.getPackageInfo(context.getPackageName(), flags);
            Signature[] a = signatures(ours), b = signatures(theirs);
            return a != null && b != null && a.length > 0 && Arrays.equals(a, b);
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    @SuppressWarnings("deprecation")
    private static Signature[] signatures(PackageInfo info) {
        if (Build.VERSION.SDK_INT >= 28 && info.signingInfo != null) {
            return info.signingInfo.hasMultipleSigners() ? info.signingInfo.getApkContentsSigners()
                : info.signingInfo.getSigningCertificateHistory();
        }
        return info.signatures;
    }

    /** Android 8+: the player has to allow this app to install apps once. */
    static boolean canInstall(Context context) {
        return Build.VERSION.SDK_INT < 26 || context.getPackageManager().canRequestPackageInstalls();
    }

    static Intent allowInstallsIntent(Context context) {
        return new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:" + context.getPackageName()));
    }

    static Intent installIntent(Context context, File apk) {
        Uri uri = FileProvider.getUriForFile(context, context.getPackageName() + ".provider", apk);
        Intent i = new Intent(Intent.ACTION_VIEW);
        i.setDataAndType(uri, "application/vnd.android.package-archive");
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_ACTIVITY_NEW_TASK);
        return i;
    }

    private static HttpURLConnection open(String url) throws IOException {
        // GitHub redirects asset downloads to its storage host; only follow https.
        for (int hops = 0; hops < 5; hops++) {
            HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
            c.setConnectTimeout(CONNECT_TIMEOUT_MS);
            c.setReadTimeout(READ_TIMEOUT_MS);
            c.setInstanceFollowRedirects(false);
            c.setRequestProperty("Accept", url.contains("api.github.com") ? "application/vnd.github+json" : "application/octet-stream");
            c.setRequestProperty("User-Agent", "KingmakersRP-Updater");
            int code = c.getResponseCode();
            if (code == HttpURLConnection.HTTP_OK) return c;
            String next = c.getHeaderField("Location");
            c.disconnect();
            if (code / 100 == 3 && next != null && next.startsWith("https://")) {
                url = next;
                continue;
            }
            throw new IOException("HTTP " + code);
        }
        throw new IOException("too many redirects");
    }

    private static byte[] get(String url, int maxBytes) throws IOException {
        HttpURLConnection c = open(url);
        try (InputStream in = c.getInputStream()) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[16 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) {
                if (out.size() + n > maxBytes) throw new IOException("response too large");
                out.write(buf, 0, n);
            }
            return out.toByteArray();
        } finally {
            c.disconnect();
        }
    }

    private static String hex(byte[] bytes) {
        StringBuilder sb = new StringBuilder(bytes.length * 2);
        for (byte b : bytes) sb.append(String.format("%02x", b & 0xff));
        return sb.toString();
    }
}
