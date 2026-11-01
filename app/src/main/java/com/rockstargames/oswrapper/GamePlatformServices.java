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

    private final java.util.HashMap<String, Integer> traced = new java.util.HashMap<>();

    /** Logs the first few calls of each callback; some are polled every frame. */
    private void trace(String name) {
        int n = traced.containsKey(name) ? traced.get(name) : 0;
        traced.put(name, n + 1);
        if (n < 5) Log.i(TAG, name + (n == 4 ? " (further calls not logged)" : ""));
    }

    public String getAppVersion() {
        trace("getAppVersion");
        return appVersion;
    }

    /** The engine's language index. 0 is English; the others aren't mapped yet. */
    public int getDeviceLocale() {
        trace("getDeviceLocale");
        return 0;
    }

    public void quit() {
        trace("quit");
        activity.runOnUiThread(activity::finish);
    }

    public void openLink(String url) {
        trace("openLink");
        activity.runOnUiThread(() -> {
            try {
                activity.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)));
            } catch (Exception e) {
                Log.w(TAG, "can't open " + url, e);
            }
        });
    }

    // ---- splash: the launcher shows its own --------------------------------

    public void showSplashScreen() { trace("showSplashScreen"); }
    public void hideSplashScreen() { trace("hideSplashScreen"); }
    public boolean isSplashScreenVisible() { trace("isSplashScreenVisible"); return false; }
    public void setSplashImage(String name) { trace("setSplashImage"); }
    public void setSplashText(String text) { trace("setSplashText"); }

    // ---- movies: none, so a requested movie ends immediately ----------------

    public void playMovie(String name, boolean skippable) { trace("playMovie"); Log.i(TAG, "playMovie " + name + " (skipped)"); }
    public void pauseMovie(boolean pause) { trace("pauseMovie"); }
    public void stopMovie() { trace("stopMovie"); }
    public boolean isMoviePlaying() { trace("isMoviePlaying"); return false; }
    public void setMovieText(String text) { trace("setMovieText"); }
    public void setMovieTextScale(int scale) { trace("setMovieTextScale"); }

    // ---- user music playlist: not offered -----------------------------------

    public void playlistOpen(String name) {
        trace("playlistOpen");
        GameThread.post(() -> GameNative.implOnPlaylistOpenComplete(false, 0));
    }
    public int playlistCount() { trace("playlistCount"); return 0; }
    public boolean playlistIsPlaying() { trace("playlistIsPlaying"); return false; }
    public void playlistPlay() { trace("playlistPlay"); }
    public void playlistPause() { trace("playlistPause"); }
    public void playlistStop() { trace("playlistStop"); }
    public void playlistSetVolume(float volume) { trace("playlistSetVolume"); }

    // ---- HTTP (Rockstar cloud, news): fail cleanly ---------------------------

    private static final int HTTP_ERROR_UNAVAILABLE = -1;

    public void httpGet(int id, String url, String[] headerNames, String[] headerValues) { trace("httpGet"); httpFail(id, url); }
    public void httpHead(int id, String url) { trace("httpHead"); httpFail(id, url); }
    public void httpPost(int id, String url, String[] headerNames, String[] headerValues, byte[] body) { trace("httpPost"); httpFail(id, url); }
    public void httpCancel(int id) { trace("httpCancel"); }

    private void httpFail(int id, String url) {
        Log.i(TAG, "http " + id + " " + url + " (not offered)");
        GameThread.post(() -> GameNative.implOnHttpRequestError(id, HTTP_ERROR_UNAVAILABLE));
    }

    // ---- Rockstar account: no SDK, so every screen completes at once --------

    public void rockstarShowInitial() {
        trace("rockstarShowInitial");
        GameThread.post(GameNative::implOnRockstarInitialComplete);
    }

    public void rockstarShowGate(int id) {
        Log.i(TAG, "rockstarShowGate " + id + " (passed)");
        GameThread.post(() -> GameNative.implOnRockstarGateComplete(id, true));
    }

    public void rockstarSignIn() {
        trace("rockstarSignIn");
        GameThread.post(GameNative::implOnRockstarSignInComplete);
    }

    public void rockstarSignOut() {
        trace("rockstarSignOut");
        GameThread.post(GameNative::implOnRockstarSignOutComplete);
    }

    public void rockstarShowCloudDisabled() {
        trace("rockstarShowCloudDisabled");
        GameThread.post(GameNative::implOnRockstarCloudDisabledComplete);
    }

    public void rockstarAccountDeletion() {
        trace("rockstarAccountDeletion");
        GameThread.post(GameNative::implOnRockstarAccountDeletionComplete);
    }

    public void rockstarFetchId() { trace("rockstarFetchId"); }
    public boolean rockstarInTrial() { trace("rockstarInTrial"); return false; }
    public void rockstarRequestReview() { trace("rockstarRequestReview"); }
    public void rockstarSetLocalePriority(String locale) { trace("rockstarSetLocalePriority"); }
}
