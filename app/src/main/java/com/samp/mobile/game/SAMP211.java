package com.samp.mobile.game;

import android.os.Bundle;
import android.util.Log;

import com.bytedance.shadowhook.ShadowHook;
import com.samp.mobile.game.ui.CustomKeyboard;

import java.io.UnsupportedEncodingException;

/**
 * The multiplayer client on GTA:SA 2.11: the 2.11 engine host
 * ({@link Engine211Activity}) with libsamp loaded right after the engine,
 * and the client's Java UI ({@link ClientUi}).
 *
 * libsamp calls the methods below by name and signature (java/jniutil.cpp),
 * and every one it looks up must exist, so the list matches SAMP's exactly.
 */
public class SAMP211 extends Engine211Activity implements CustomKeyboard.InputListener {
    private static final String TAG = "SAMP211";
    private static boolean sClientLoaded;

    private ClientUi ui;

    @Override
    protected void onEngineLoaded(String storageRoot) {
        // libsamp's JNI_OnLoad finds libGame.so and installs its hooks, so it
        // has to load after the engine and before the engine starts.
        ShadowHook.init(new ShadowHook.ConfigBuilder().setMode(ShadowHook.Mode.UNIQUE).build());
        provisionClientFiles(storageRoot);
        Log.i(TAG, "loading the client");
        System.loadLibrary("bass");
        System.loadLibrary("samp");
        // A libsamp native, so only once it's loaded. libsamp reads the path
        // later, when the engine calls NvUtilInit.
        setStorageRoot(storageRoot);
        sClientLoaded = true;
        Log.i(TAG, "client loaded");
    }

    /**
     * libsamp reads <storage>/SAMP/settings.ini and stops if it's missing.
     * Put the APK's default there on first run; never overwrite the player's.
     */
    private void provisionClientFiles(String storageRoot) {
        java.io.File settings = new java.io.File(storageRoot, "SAMP/settings.ini");
        if (settings.isFile()) return;
        java.io.File dir = settings.getParentFile();
        if (dir != null && !dir.isDirectory() && !dir.mkdirs()) {
            Log.e(TAG, "can't create " + dir);
            return;
        }
        try (java.io.InputStream in = getAssets().open("settings.ini");
             java.io.OutputStream out = new java.io.FileOutputStream(settings)) {
            byte[] buffer = new byte[8192];
            int n;
            while ((n = in.read(buffer)) > 0) out.write(buffer, 0, n);
            Log.i(TAG, "wrote default " + settings);
        } catch (java.io.IOException e) {
            Log.e(TAG, "can't write " + settings, e);
        }
    }

    @Override
    protected void onGameViewReady() {
        ui = new ClientUi(this, this::sendKmCommand);
        if (!sClientLoaded) return;
        try {
            initializeSAMP();
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "initializeSAMP: " + e.getMessage());
        }
    }

    @Override
    protected boolean showsBootTestNotice() {
        return false;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onBackPressed() {
        if (ui != null && ui.onBack()) return;
        try {
            onEventBackPressed();
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, e.getMessage());
        }
        super.onBackPressed();
    }

    // ---- natives (main.cpp) ---------------------------------------------------

    private static native void setStorageRoot(String root);
    private native void initializeSAMP();
    private native void onInputEnd(byte[] str);
    public native void onEventBackPressed();
    private native void kmSendCommand(byte[] command);

    private void sendKmCommand(String command) {
        try {
            kmSendCommand(command.getBytes("windows-1251"));
        } catch (UnsupportedEncodingException | UnsatisfiedLinkError e) {
            Log.e(TAG, "kmSendCommand: " + e.getMessage());
        }
    }

    @Override
    public void OnInputEnd(String str) {
        try {
            onInputEnd(str.getBytes("windows-1251"));
        } catch (UnsupportedEncodingException | UnsatisfiedLinkError e) {
            Log.e(TAG, "onInputEnd: " + e.getMessage());
        }
    }

    // ---- called by libsamp ------------------------------------------------------

    public void showTab() {}
    public void hideTab() {}
    public void clearTab() {}
    public void setTab(int id, String name, int score, int ping) {}
    public void showLoadingScreen() { ui.showLoadingScreen(); }
    public void hideLoadingScreen() { ui.hideLoadingScreen(); }
    public void setPauseState(boolean pause) { ui.setPauseState(pause); }
    public void showDialog(int id, int type, byte[] caption, byte[] content, byte[] left, byte[] right) {
        ui.showDialog(id, type, caption, content, left, right);
    }
    public void showKeyboard() { ui.showKeyboard(); }
    public void hideKeyboard() { ui.hideKeyboard(); }
    public void exitGame() {
        finishAndRemoveTask();
        System.exit(0);
    }
    public void showEditObject() { ui.showEditObject(); }
    public void hideEditObject() { ui.hideEditObject(); }
    public void updateHudInfo(int health, int armour, int weapon, int ammo, int clip, int money, int wanted) {
        ui.updateHudInfo(health, armour, weapon, ammo, clip, money, wanted);
    }
    public void showHud() { ui.showHud(); }
    public void hideHud() { ui.hideHud(); }
    public void kmHud(int cash, int bank, int level, int xp, int xpMax, byte[] job, int wanted, int fuel) {
        ui.kmHud(cash, bank, level, xp, xpMax, job, wanted, fuel);
    }
    public void kmToast(byte[] text) { ui.kmToast(text); }
    public void kmSms(int from, byte[] text) { ui.kmSms(from, text); }
    public void kmPhone(int hasPhone, int number, int bank) { ui.kmPhone(hasPhone, number, bank); }
    public void kmSpeed(int kmh, int inVehicle) { ui.kmSpeed(kmh, inVehicle); }
}
