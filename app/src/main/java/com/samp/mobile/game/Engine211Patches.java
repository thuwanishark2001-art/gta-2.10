package com.samp.mobile.game;

import android.util.Log;

/** Engine patches for GTA:SA 2.11 in the launcher (cpp/km211). */
public final class Engine211Patches {
    private Engine211Patches() {}

    /** Applies them; call once, after libGame.so is loaded. */
    public static void applyAll() {
        try {
            System.loadLibrary("km211");
            Log.i("Engine211", "engine patches applied: " + apply());
        } catch (Throwable t) {
            Log.e("Engine211", "engine patches unavailable", t);
        }
    }

    private static native int apply();
}
