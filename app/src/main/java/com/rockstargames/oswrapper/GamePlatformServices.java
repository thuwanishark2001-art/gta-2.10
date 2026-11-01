package com.rockstargames.oswrapper;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;

/**
 * What the 2.11 engine calls back into (by method name), on the game thread.
 *
 * The launcher has no Rockstar SDK, Flutter UI or Play asset downloads, so:
 * Rockstar account screens complete at once (gate passed, not signed in),
 * HTTP requests fail, there are no movies or user playlists, and the splash
 * screen is the launcher's own. Answers are queued, never made from inside
 * the engine's own call.
 */
public class GamePlatformServices {
    private static final String TAG = "GamePlatformServices";

    public final Activity activity;
    public final GameView view;
    private final String appVersion;

    public GamePlatformServices(Activity activity, GameView view, String appVersion) {
        this.activity = activity;
        this.view = view;
        this.appVersion = appVersion;
    }

    public String getAppVersion() {
        return appVersion;
    }

    /** The engine's language index. 0 is English; the others aren't mapped yet. */
    public int getDeviceLocale() {
        return 0;
    }

    public void quit() {
        Log.i(TAG, "quit");
        activity.runOnUiThread(activity::finish);
    }

    public void openLink(String url) {
        activity.runOnUiThread(() -> {
            try {
                activity.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)));
            } catch (Exception e) {
                Log.w(TAG, "can't open " + url, e);
            }
        });
    }

    // ---- splash: the launcher shows its own --------------------------------

    public void showSplashScreen() {}
    public void hideSplashScreen() {}
    public boolean isSplashScreenVisible() { return false; }
    public void setSplashImage(String name) {}
    public void setSplashText(String text) {}

    // ---- movies: none, so a requested movie ends immediately ----------------

    public void playMovie(String name, boolean skippable) { Log.i(TAG, "playMovie " + name + " (skipped)"); }
    public void pauseMovie(boolean pause) {}
    public void stopMovie() {}
    public boolean isMoviePlaying() { return false; }
    public void setMovieText(String text) {}
    public void setMovieTextScale(int scale) {}

    // ---- user music playlist: not offered -----------------------------------

    public void playlistOpen(String name) {
        GameThread.post(() -> GameNative.implOnPlaylistOpenComplete(false, 0));
    }
    public int playlistCount() { return 0; }
    public boolean playlistIsPlaying() { return false; }
    public void playlistPlay() {}
    public void playlistPause() {}
    public void playlistStop() {}
    public void playlistSetVolume(float volume) {}

    // ---- HTTP (Rockstar cloud, news): fail cleanly ---------------------------

    private static final int HTTP_ERROR_UNAVAILABLE = -1;

    public void httpGet(int id, String url, String[] headerNames, String[] headerValues) { httpFail(id, url); }
    public void httpHead(int id, String url) { httpFail(id, url); }
    public void httpPost(int id, String url, String[] headerNames, String[] headerValues, byte[] body) { httpFail(id, url); }
    public void httpCancel(int id) {}

    private void httpFail(int id, String url) {
        Log.i(TAG, "http " + id + " " + url + " (not offered)");
        GameThread.post(() -> GameNative.implOnHttpRequestError(id, HTTP_ERROR_UNAVAILABLE));
    }

    // ---- Rockstar account: no SDK, so every screen completes at once --------

    public void rockstarShowInitial() {
        GameThread.post(GameNative::implOnRockstarInitialComplete);
    }

    public void rockstarShowGate(int id) {
        Log.i(TAG, "rockstarShowGate " + id + " (passed)");
        GameThread.post(() -> GameNative.implOnRockstarGateComplete(id, true));
    }

    public void rockstarSignIn() {
        GameThread.post(GameNative::implOnRockstarSignInComplete);
    }

    public void rockstarSignOut() {
        GameThread.post(GameNative::implOnRockstarSignOutComplete);
    }

    public void rockstarShowCloudDisabled() {
        GameThread.post(GameNative::implOnRockstarCloudDisabledComplete);
    }

    public void rockstarAccountDeletion() {
        GameThread.post(GameNative::implOnRockstarAccountDeletionComplete);
    }

    public void rockstarFetchId() {}
    public boolean rockstarInTrial() { return false; }
    public void rockstarRequestReview() {}
    public void rockstarSetLocalePriority(String locale) {}
}
