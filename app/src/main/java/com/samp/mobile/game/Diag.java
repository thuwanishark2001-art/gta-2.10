package com.samp.mobile.game;

import android.content.Context;
import android.util.Log;

import java.io.File;

/**
 * Native diagnostics (cpp/kmdiag): copies log messages into a file and
 * writes a crash report to it. For phones whose logcat apps can't read.
 * {@link CrashLog} saves the file after a crash.
 */
public final class Diag {
    private static final String TAG = "Diag";
    private static boolean sStarted;

    private Diag() {}

    static File file(Context context) {
        return new File(context.getFilesDir(), "diag-session.log");
    }

    public static synchronized void start(Context context) {
        if (sStarted) return;
        try {
            System.loadLibrary("kmdiag");
            start(file(context).getAbsolutePath());
            sStarted = true;
        } catch (Throwable t) {
            Log.w(TAG, "diagnostics unavailable", t);
        }
    }

    /** Re-installs the crash handler, in case the engine replaced it. */
    public static void reinstall() {
        if (sStarted) installCrashHandler();
    }

    private static native void start(String path);
    private static native void installCrashHandler();
    static native void note(String text);
}
