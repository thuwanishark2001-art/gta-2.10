package com.rockstargames.oswrapper;

import android.content.res.AssetManager;
import android.view.Surface;

/**
 * Entry points into GTA:SA 2.11's engine (libGame.so). The engine exports
 * them as Java_com_rockstargames_oswrapper_GameNative_*, so this class has
 * to keep Rockstar's package and class name. Call only from {@link GameThread}.
 * See docs/OSWRAPPER-2.11.md.
 */
public final class GameNative {
    private GameNative() {}

    public static native boolean implIsInitialized();
    public static native void implOnInitialSetup(DeviceInfo info, AssetManager assets, String[] packNames, String[] packPaths);
    public static native void implOnActivityCreated(GamePlatformServices services, boolean initialized);
    public static native void implOnActivityDestroyed();
    public static native void implOnPause();
    public static native void implOnResume();
    public static native void implOnLowMemory();
    public static native void implOnSurfaceCreated();
    public static native void implOnSurfaceChanged(Surface surface, int width, int height);
    public static native void implOnSurfaceDestroyed();
    public static native void implOnDrawFrame(float seconds);
    public static native void implOnTouchStart(int id, float x, float y);
    public static native void implOnTouchMove(int id, float x, float y);
    public static native void implOnTouchEnd(int id, float x, float y);
    public static native void implOnBackButtonPressed();
    public static native void implOnAccelerometerChanged(float x, float y, float z);
    public static native void implOnNetworkChanged(int type);
    public static native void implOnGamepadConnected(int id);
    public static native void implOnGamepadDisconnected(int id);
    public static native void implOnGamepadResume(int[] ids);
    public static native void implOnGamepadButtonDown(int id, int button);
    public static native void implOnGamepadButtonUp(int id, int button);
    public static native void implOnGamepadAxesChanged(int id, float lx, float ly, float rx, float ry, float lt, float rt);
    public static native void implOnHttpRequestResponse(int id, int status, String url, String[] headerNames, String[] headerValues);
    public static native void implOnHttpRequestData(int id, byte[] data);
    public static native void implOnHttpRequestFinished(int id);
    public static native void implOnHttpRequestError(int id, int error);
    public static native void implOnPlaylistOpenComplete(boolean ok, int count);
    public static native void implOnRockstarSetup(String environment, String userId);
    public static native void implOnRockstarStateChanged(boolean signedIn);
    public static native void implOnRockstarIdChanged(String id);
    public static native void implOnRockstarTicketChanged(String ticket);
    public static native void implOnRockstarInitialComplete();
    public static native void implOnRockstarGateComplete(int id, boolean passed);
    public static native void implOnRockstarSignInComplete();
    public static native void implOnRockstarSignOutComplete();
    public static native void implOnRockstarCloudDisabledComplete();
    public static native void implOnRockstarAccountDeletionComplete();
}
