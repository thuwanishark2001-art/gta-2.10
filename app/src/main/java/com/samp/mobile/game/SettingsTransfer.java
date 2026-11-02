package com.samp.mobile.game;

import android.content.Context;
import android.net.Uri;
import java.io.*;
import java.nio.charset.StandardCharsets;

final class SettingsTransfer {
    private static final int MAX_BYTES = 64 * 1024;

    private SettingsTransfer() {}

    static void exportSettings(Context context, Uri destination) throws IOException {
        ClientSettings.provision(context);
        File source = ClientSettings.file(context);
        if (!source.isFile() || source.length() <= 0 || source.length() > MAX_BYTES)
            throw new IOException("Settings file missing or invalid.");

        try (InputStream in = new FileInputStream(source);
             OutputStream out = context.getContentResolver().openOutputStream(destination, "w")) {
            if (out == null) throw new IOException("Cannot open backup destination.");
            copyLimited(in, out);
            out.flush();
        }
    }

    static void importSettings(Context context, Uri source) throws IOException {
        byte[] bytes;
        try (InputStream in = context.getContentResolver().openInputStream(source);
             ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            if (in == null) throw new IOException("Cannot open selected backup.");
            copyLimited(in, out);
            bytes = out.toByteArray();
        }

        String text = new String(bytes, StandardCharsets.UTF_8);
        if (!validSettings(text))
            throw new IOException("Invalid backup. A valid [client] nickname is required.");

        File target = ClientSettings.file(context);
        File parent = target.getParentFile();
        if (parent == null || (!parent.isDirectory() && !parent.mkdirs()))
            throw new IOException("Cannot access settings folder.");

        File temp = new File(parent, "settings.ini.importing");
        File backup = new File(parent,
                "settings.ini.before-import-" + System.currentTimeMillis() + ".bak");

        try (FileOutputStream out = new FileOutputStream(temp, false)) {
            out.write(bytes);
            out.getFD().sync();
        }

        boolean backedUp = false;
        if (target.exists()) {
            if (!target.renameTo(backup)) {
                temp.delete();
                throw new IOException("Could not back up existing settings.");
            }
            backedUp = true;
        }

        if (!temp.renameTo(target)) {
            temp.delete();
            if (backedUp) backup.renameTo(target);
            throw new IOException("Import failed. Previous settings restored where possible.");
        }
    }

    private static boolean validSettings(String text) {
        if (text == null || text.isEmpty() || text.length() > MAX_BYTES
                || text.indexOf('\0') >= 0) return false;

        String section = "";
        boolean validName = false;

        for (String line : text.split("\\r?\\n")) {
            String t = line.trim();
            if (t.startsWith("[") && t.endsWith("]")) {
                section = t;
                continue;
            }
            if (!section.equalsIgnoreCase("[client]")
                    || t.startsWith("#") || t.startsWith(";")) continue;

            int eq = t.indexOf('=');
            if (eq > 0 && t.substring(0, eq).trim().equalsIgnoreCase("name")) {
                String name = t.substring(eq + 1).trim();
                validName = ClientSettings.isValidName(name)
                        && !ClientSettings.DEFAULT_NAME.equals(name);
            }
        }
        return validName;
    }

    private static void copyLimited(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[4096];
        int total = 0, count;
        while ((count = in.read(buffer)) != -1) {
            total += count;
            if (total > MAX_BYTES) throw new IOException("Backup exceeds 64 KB.");
            out.write(buffer, 0, count);
        }
    }
}
