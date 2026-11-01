package com.samp.mobile.game;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Build;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * The game itself comes from the player's own GTA: San Andreas install, not
 * from this APK:
 * <ul>
 * <li>Engine libraries (libGTASA.so and what it links against) are copied
 *     once from the installed game into this app's private storage, marked
 *     read-only, and loaded from there.</li>
 * <li>Game data the engine reads from "the APK" is read straight from the
 *     installed game's package through {@link #assets(Context)}.</li>
 * </ul>
 * The client's hooks target one GTA:SA version per architecture (the
 * addresses in app/src/main/cpp/samp are per ABI), so only that version is
 * accepted. tools/port-libgtasa moves the addresses to another version;
 * when it does, update {@link #requiredVersion()} and {@link #KNOWN_GOOD}
 * for the ported ABI only.
 */
public final class GameFiles {
    private static final String TAG = "GameFiles";

    public static final String GTA_PACKAGE = "com.rockstargames.gtasa";
    /** Game version the native addresses are written for, per ABI. */
    private static final String REQUIRED_VERSION_ARM64 = "2.10";
    private static final String REQUIRED_VERSION_ARMV7 = "2.10";

    /** SHA-256 of the libraries the hooks were written against. */
    private static final String[][] KNOWN_GOOD = {
        { "arm64-v8a/libGTASA.so", "12527bfdb63889c02d24366629e808cb446d354a2b380348bd661da4edc6af11" },
        { "arm64-v8a/libSCAnd.so", "523e9326cb911ca71227c0e5d40d4b8e30e4b1062fa3ddbc6a5db4b1c886d880" },
        { "armeabi-v7a/libGTASA.so", "798751ae5cc0f3b7a65f63b70e23879bc1d4c6c28af914f539fcb47ee553adb7" },
        { "armeabi-v7a/libSCAnd.so", "d3b6a78c6d2267bb045af2fd0108b3b332c64f6c0e6431aeae36e1573373542c" },
    };

    public enum State { READY, NEEDS_COPY, NOT_INSTALLED, WRONG_VERSION }

    public static final class Status {
        public final State state;
        public final String installedVersion;

        Status(State state, String installedVersion) {
            this.state = state;
            this.installedVersion = installedVersion;
        }
    }

    private static AssetManager sGameAssets;

    private GameFiles() {}

    public static String requiredVersion() {
        return android.os.Process.is64Bit() ? REQUIRED_VERSION_ARM64 : REQUIRED_VERSION_ARMV7;
    }

    /** "2.10" accepts 2.10 and 2.10.x, not 2.100. */
    static boolean versionMatches(String installed, String required) {
        return installed.equals(required) || installed.startsWith(required + ".");
    }

    public static String abi() {
        return android.os.Process.is64Bit() ? "arm64-v8a" : "armeabi-v7a";
    }

    /** Libraries to load, in order (each one's dependencies come first). */
    static List<String> libraries() {
        if (android.os.Process.is64Bit()) return Arrays.asList("libSCAnd.so", "libGTASA.so");
        // The 32-bit engine also links against Immersion's haptics library.
        return Arrays.asList("libImmEmulatorJ.so", "libSCAnd.so", "libGTASA.so");
    }

    static File libDir(Context context) {
        return new File(context.getFilesDir(), "gtasa/" + abi());
    }

    private static File marker(Context context) {
        return new File(libDir(context), "copied-from.txt");
    }

    public static Status check(Context context) {
        PackageInfo info;
        try {
            info = context.getPackageManager().getPackageInfo(GTA_PACKAGE, 0);
        } catch (PackageManager.NameNotFoundException e) {
            return new Status(State.NOT_INSTALLED, null);
        }
        String version = info.versionName == null ? "" : info.versionName;
        if (!versionMatches(version, requiredVersion())) return new Status(State.WRONG_VERSION, version);

        // Copied before from this exact install?
        String expected = markerText(info);
        try {
            if (expected.equals(readText(marker(context)))) {
                for (String lib : libraries()) {
                    if (!new File(libDir(context), lib).isFile()) return new Status(State.NEEDS_COPY, version);
                }
                return new Status(State.READY, version);
            }
        } catch (IOException ignored) {
            // no marker yet
        }
        return new Status(State.NEEDS_COPY, version);
    }

    private static String markerText(PackageInfo info) {
        long code = Build.VERSION.SDK_INT >= 28 ? info.getLongVersionCode() : info.versionCode;
        return info.versionName + " " + code + " " + info.lastUpdateTime + "\n";
    }

    /**
     * Copies the engine libraries out of the installed game. Returns true if
     * every copied library matches a known build. Throws if one is missing.
     */
    public static boolean copyLibraries(Context context) throws Exception {
        PackageInfo info = context.getPackageManager().getPackageInfo(GTA_PACKAGE, 0);
        ApplicationInfo app = info.applicationInfo;
        File dir = libDir(context);
        if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException("can't create " + dir);
        marker(context).delete();

        boolean allKnown = true;
        for (String lib : libraries()) {
            File target = new File(dir, lib);
            File tmp = new File(dir, lib + ".tmp");
            tmp.delete();
            if (!copyFromInstall(app, lib, tmp)) throw new IOException(lib + " wasn't found in " + GTA_PACKAGE);
            String hash = sha256(tmp);
            boolean known = isKnown(abi() + "/" + lib, hash);
            allKnown &= known || lib.equals("libImmEmulatorJ.so");
            Log.i(TAG, lib + " sha256 " + hash + (known ? " (known build)" : " (not a known build)"));
            target.setWritable(true);
            target.delete();
            if (!tmp.renameTo(target)) throw new IOException("can't write " + target);
            // Android 14 refuses dynamically loaded code that is writable.
            target.setReadOnly();
        }
        writeText(marker(context), markerText(info));
        return allKnown;
    }

    /** Loads the engine libraries copied by {@link #copyLibraries}. */
    public static void loadLibraries(Context context) {
        File dir = libDir(context);
        for (String lib : libraries()) {
            System.load(new File(dir, lib).getAbsolutePath());
        }
    }

    /** The installed game's assets, or this app's own if it can't be opened. */
    public static synchronized AssetManager assets(Context context) {
        if (sGameAssets == null) {
            try {
                sGameAssets = context.createPackageContext(GTA_PACKAGE, 0).getAssets();
            } catch (PackageManager.NameNotFoundException e) {
                Log.e(TAG, "GTA: San Andreas isn't installed; using this app's assets");
                return context.getAssets();
            }
        }
        return sGameAssets;
    }

    // ---- helpers -----------------------------------------------------------

    private static boolean isKnown(String name, String hash) {
        for (String[] entry : KNOWN_GOOD) {
            if (entry[0].equals(name) && entry[1].equals(hash)) return true;
        }
        return false;
    }

    /** Extracted native libraries if the installer extracted them, otherwise lib/<abi>/ inside the APK and its splits. */
    private static boolean copyFromInstall(ApplicationInfo app, String lib, File out) throws IOException {
        if (app.nativeLibraryDir != null) {
            File extracted = new File(app.nativeLibraryDir, lib);
            if (extracted.isFile()) {
                try (InputStream in = new FileInputStream(extracted)) {
                    copy(in, out);
                }
                return true;
            }
        }
        List<String> apks = new ArrayList<>();
        apks.add(app.sourceDir);
        if (app.splitSourceDirs != null) apks.addAll(Arrays.asList(app.splitSourceDirs));
        String entryName = "lib/" + abi() + "/" + lib;
        for (String apk : apks) {
            try (ZipFile zip = new ZipFile(apk)) {
                ZipEntry entry = zip.getEntry(entryName);
                if (entry == null) continue;
                try (InputStream in = zip.getInputStream(entry)) {
                    copy(in, out);
                }
                return true;
            }
        }
        return false;
    }

    private static void copy(InputStream in, File out) throws IOException {
        try (OutputStream os = new FileOutputStream(out)) {
            byte[] buffer = new byte[1 << 16];
            int n;
            while ((n = in.read(buffer)) > 0) os.write(buffer, 0, n);
        }
    }

    private static String sha256(File file) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try (InputStream in = new FileInputStream(file)) {
            byte[] buffer = new byte[1 << 16];
            int n;
            while ((n = in.read(buffer)) > 0) digest.update(buffer, 0, n);
        }
        StringBuilder hex = new StringBuilder();
        for (byte b : digest.digest()) hex.append(String.format("%02x", b));
        return hex.toString();
    }

    private static String readText(File file) throws IOException {
        try (InputStream in = new FileInputStream(file)) {
            byte[] data = new byte[(int) Math.min(file.length(), 4096)];
            int n = in.read(data);
            return new String(data, 0, Math.max(n, 0), "UTF-8");
        }
    }

    private static void writeText(File file, String text) throws IOException {
        try (OutputStream os = new FileOutputStream(file)) {
            os.write(text.getBytes("UTF-8"));
        }
    }
}
