package com.samp.mobile.game;

import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * Saves this app's log after a game session that didn't end cleanly, so a
 * player without a computer can send it in. A session is marked when the
 * game starts and unmarked when it closes normally. On the next launch, a
 * leftover mark means it crashed, and the log (this app's own lines, which
 * Android keeps across the restart) goes to Downloads.
 */
public final class CrashLog {
    private static final String TAG = "CrashLog";
    private static final String MARK = "session-running";

    private CrashLog() {}

    public static void sessionStarted(Context context) {
        try {
            new File(context.getFilesDir(), MARK).createNewFile();
        } catch (IOException e) {
            Log.w(TAG, "can't mark session", e);
        }
    }

    public static void sessionEnded(Context context) {
        new File(context.getFilesDir(), MARK).delete();
    }

    private static final String LAST_SESSION = "kingmakers-last-session.txt";

    /**
     * Saves this session's log so far to Downloads/kingmakers-last-session.txt,
     * replacing the previous one, so a test that got stuck without crashing
     * still has a log. Call when the game goes to the background; runs on
     * its own thread.
     */
    public static void saveSessionLog(Context context) {
        final Context app = context.getApplicationContext();
        new Thread(() -> {
            try {
                if (Build.VERSION.SDK_INT >= 29) {
                    // Our own earlier copy: delete it so the name stays the same.
                    app.getContentResolver().delete(MediaStore.Downloads.EXTERNAL_CONTENT_URI,
                        MediaStore.Downloads.DISPLAY_NAME + "=?", new String[] { LAST_SESSION });
                }
                write(app, LAST_SESSION);
            } catch (Exception e) {
                Log.w(TAG, "can't save the session log", e);
            }
        }, "SessionLog").start();
    }

    /**
     * If the last session crashed, saves the log and returns where it went
     * (for the player to see); otherwise returns null.
     */
    public static String saveIfLastSessionCrashed(Context context) {
        File mark = new File(context.getFilesDir(), MARK);
        if (!mark.exists()) return null;
        mark.delete();
        String name = "kingmakers-log-" + new SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(new Date()) + ".txt";
        try {
            return write(context, name);
        } catch (Exception e) {
            Log.e(TAG, "can't save the log", e);
            return null;
        }
    }

    /** Writes logcat (empty on phones that hide it) and the kmdiag file to Downloads/name. */
    private static String write(Context context, String name) throws Exception {
        Process logcat = new ProcessBuilder("logcat", "-d", "-v", "threadtime", "-b", "main,system,crash")
            .redirectErrorStream(true).start();
        File diag = Diag.file(context);
        InputStream logcatIn = logcat.getInputStream();
        InputStream all = diag.isFile()
            ? new java.io.SequenceInputStream(logcatIn, new java.io.SequenceInputStream(
                new java.io.ByteArrayInputStream("\n--------- kmdiag session file\n".getBytes()),
                new java.io.FileInputStream(diag)))
            : logcatIn;
        try (InputStream in = all) {
            if (Build.VERSION.SDK_INT >= 29) {
                ContentValues values = new ContentValues();
                values.put(MediaStore.Downloads.DISPLAY_NAME, name);
                values.put(MediaStore.Downloads.MIME_TYPE, "text/plain");
                Uri uri = context.getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
                if (uri == null) throw new IOException("can't create " + name + " in Downloads");
                try (OutputStream out = context.getContentResolver().openOutputStream(uri)) {
                    copy(in, out);
                }
                return Environment.DIRECTORY_DOWNLOADS + "/" + name;
            }
            File file = new File(context.getExternalFilesDir(null), name);
            try (OutputStream out = new FileOutputStream(file)) {
                copy(in, out);
            }
            return file.getAbsolutePath();
        }
    }

    private static void copy(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1 << 16];
        int n;
        while ((n = in.read(buffer)) > 0) out.write(buffer, 0, n);
    }
}
