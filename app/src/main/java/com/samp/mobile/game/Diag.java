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
    private static volatile boolean sStarted;

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
            recordJavaCrashes();
        } catch (Throwable t) {
            Log.w(TAG, "diagnostics unavailable", t);
        }
    }

    /** Java exceptions that end the app go into the file too, then on as before. */
    private static void recordJavaCrashes() {
        final Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((thread, e) -> {
            try {
                java.io.StringWriter text = new java.io.StringWriter();
                e.printStackTrace(new java.io.PrintWriter(text));
                note("\n*** uncaught Java exception on thread " + thread.getName() + "\n" + text + "*** end");
            } catch (Throwable ignored) {
                // never let the report hide the crash
            }
            if (previous != null) previous.uncaughtException(thread, e);
        });
    }

    /** Re-installs the crash handler, in case the engine replaced it. */
    public static void reinstall() {
        if (sStarted) installCrashHandler();
    }

    /** Call from the game thread when it starts. */
    public static void gameThreadStarted() {
        if (sStarted) registerGameThread();
    }

    /** Call from the game thread every loop; silence for 5 s is reported as a hang. */
    public static void heartbeat() {
        if (sStarted) heartbeatNative();
    }

    private static native void registerGameThread();
    @SuppressWarnings("JniMissingFunction")
    private static native void heartbeatNative();
    private static native void start(String path);
    private static native void installCrashHandler();
    static native void note(String text);
}
