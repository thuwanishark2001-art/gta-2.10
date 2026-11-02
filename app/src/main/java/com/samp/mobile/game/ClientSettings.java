package com.samp.mobile.game;

import android.content.Context;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;

/**
 * The client's SAMP/settings.ini in the app's external files folder: created
 * from the APK's default on first run, and where the account name lives.
 */
final class ClientSettings {
    private static final String TAG = "ClientSettings";
    /** The name in the bundled settings.ini; every new install starts with it. */
    static final String DEFAULT_NAME = "Nick_Name";

    private ClientSettings() {}

    static File file(Context context) {
        File root = context.getExternalFilesDir(null);
        return new File(root, "SAMP/settings.ini");
    }

    /** Writes the APK's default settings.ini if there is none; never overwrites the player's. */
    static void provision(Context context) {
        File settings = file(context);
        if (settings.isFile()) return;
        File dir = settings.getParentFile();
        if (dir != null && !dir.isDirectory() && !dir.mkdirs()) {
            Log.e(TAG, "can't create " + dir);
            return;
        }
        try (InputStream in = context.getAssets().open("settings.ini");
             OutputStream out = new FileOutputStream(settings)) {
            byte[] buffer = new byte[8192];
            int n;
            while ((n = in.read(buffer)) > 0) out.write(buffer, 0, n);
            Log.i(TAG, "wrote default " + settings);
        } catch (IOException e) {
            Log.e(TAG, "can't write " + settings, e);
        }
    }

    /** The same rule as the server: 3-20 letters, digits or underscores. */
    static boolean isValidName(String name) {
        if (name == null || name.length() < 3 || name.length() > 20) return false;
        for (int i = 0; i < name.length(); i++) {
            char c = name.charAt(i);
            boolean ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
            if (!ok) return false;
        }
        return true;
    }

    /** The account name in [client], or "" if there is none. */
    static String getName(Context context) {
        String section = "";
        for (String line : readLines(context)) {
            String t = line.trim();
            if (t.startsWith("[") && t.endsWith("]")) { section = t; continue; }
            if (!section.equalsIgnoreCase("[client]") || t.startsWith("#")) continue;
            int eq = t.indexOf('=');
            if (eq > 0 && t.substring(0, eq).trim().equalsIgnoreCase("name")) return t.substring(eq + 1).trim();
        }
        return "";
    }

    /** True when the player still has to pick a name. */
    static boolean needsName(Context context) {
        String name = getName(context);
        return !isValidName(name) || name.equals(DEFAULT_NAME);
    }

    /** Sets name = ... in [client], keeping the rest of the file. */
    static boolean setName(Context context, String name) {
        if (!isValidName(name)) return false;
        List<String> lines = readLines(context);
        List<String> out = new ArrayList<>();
        String section = "";
        boolean written = false;
        int clientEnd = -1;
        for (String line : lines) {
            String t = line.trim();
            if (t.startsWith("[") && t.endsWith("]")) {
                if (section.equalsIgnoreCase("[client]") && !written) clientEnd = out.size();
                section = t;
            } else if (section.equalsIgnoreCase("[client]") && !t.startsWith("#")) {
                int eq = t.indexOf('=');
                if (eq > 0 && t.substring(0, eq).trim().equalsIgnoreCase("name")) {
                    if (!written) out.add("name = " + name);
                    written = true;
                    continue;
                }
            }
            out.add(line);
        }
        if (!written) {
            if (clientEnd >= 0) out.add(clientEnd, "name = " + name);
            else if (section.equalsIgnoreCase("[client]")) out.add("name = " + name);
            else { out.add("[client]"); out.add("name = " + name); }
        }
        try {
            Files.write(file(context).toPath(), String.join("\n", out).concat("\n").getBytes(StandardCharsets.UTF_8));
            return true;
        } catch (IOException e) {
            Log.e(TAG, "can't save the name", e);
            return false;
        }
    }

    private static List<String> readLines(Context context) {
        provision(context);
        try {
            return new ArrayList<>(Files.readAllLines(file(context).toPath(), StandardCharsets.UTF_8));
        } catch (IOException e) {
            Log.e(TAG, "can't read " + file(context), e);
            return new ArrayList<>();
        }
    }
}
