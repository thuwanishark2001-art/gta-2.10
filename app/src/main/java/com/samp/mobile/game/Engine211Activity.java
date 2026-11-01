package com.samp.mobile.game;

import android.app.Activity;
import android.content.pm.PackageInfo;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.Toast;

import com.nvidia.devtech.NvUtil;
import com.rockstargames.oswrapper.DeviceInfo;
import com.rockstargames.oswrapper.GameNative;
import com.rockstargames.oswrapper.GamePlatformServices;
import com.rockstargames.oswrapper.GameThread;
import com.rockstargames.oswrapper.GameView;

import java.io.File;

/**
 * Runs the player's GTA:SA 2.11 engine (libGame.so) on its own: an engine
 * boot test, without the multiplayer client, which is still being ported
 * (docs/PORT-2.11.md). The startup order follows Rockstar's
 * GameActivityBase (docs/OSWRAPPER-2.11.md).
 */
public class Engine211Activity extends Activity {
    private static final String TAG = "Engine211";
    private static boolean sLoaded;

    private GameView view;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        hideSystemUi();

        synchronized (Engine211Activity.class) {
            if (!sLoaded) {
                GameFiles.loadLibraries(this, GameFiles.Engine.GTA_211);
                sLoaded = true;
            }
        }

        // Paths the engine reads through NvUtil: this app's own storage, so
        // saves and settings stay apart from the real game's.
        File external = getExternalFilesDir(null);
        String root = external != null ? external.getAbsolutePath() : getFilesDir().getAbsolutePath();
        int android = root.indexOf("/Android");
        String base = android > 0 ? root.substring(0, android) : root;
        Log.i(TAG, "STORAGE_ROOT = " + root + " STORAGE_ROOT_BASE = " + base);
        NvUtil.getInstance().setActivity(this);
        NvUtil.getInstance().setAppLocalValue("STORAGE_ROOT", root);
        NvUtil.getInstance().setAppLocalValue("STORAGE_ROOT_BASE", base);

        view = new GameView(this);
        setContentView(view);
        GamePlatformServices services = new GamePlatformServices(this, view, gameVersion());
        GameThread.start(services);

        final DeviceInfo device = new DeviceInfo(this);
        GameThread.post(() -> {
            // Only the install-time pack: the audio packs are downloaded into
            // the game's private storage, which this app can't read.
            GameNative.implOnInitialSetup(device, GameFiles.assets(this),
                new String[] { "data_main" }, new String[] { null });
            GameNative.implOnRockstarSetup("prod", "");
        });
        view.setup();
        GameThread.post(() -> GameNative.implOnActivityCreated(services, GameNative.implIsInitialized()));

        Toast.makeText(this, "GTA:SA 2.11 engine test: multiplayer isn't connected yet.", Toast.LENGTH_LONG).show();
    }

    private String gameVersion() {
        try {
            PackageInfo info = getPackageManager().getPackageInfo(GameFiles.GTA_PACKAGE, 0);
            return info.versionName;
        } catch (Exception e) {
            return GameFiles.VERSION_211;
        }
    }

    private void hideSystemUi() {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemUi();
        view.setGameFocus(hasFocus);
    }

    @Override
    protected void onResume() {
        super.onResume();
        GameThread.post(GameNative::implOnResume);
    }

    @Override
    protected void onPause() {
        GameThread.post(GameNative::implOnPause);
        super.onPause();
    }

    @Override
    public void onLowMemory() {
        super.onLowMemory();
        GameThread.post(GameNative::implOnLowMemory);
    }

    @Override
    public void onBackPressed() {
        GameThread.post(GameNative::implOnBackButtonPressed);
    }

    @Override
    protected void onDestroy() {
        GameThread.post(GameNative::implOnActivityDestroyed);
        GameThread.stop();
        super.onDestroy();
    }
}
