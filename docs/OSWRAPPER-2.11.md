# The 2.11 engine's host interface (`com.rockstargames.oswrapper`)

What `libGame.so` (GTA:SA 2.11.311, arm64) needs from the Java side,
read from the game's own `classes*.dex` and the library's JNI exports.
The Kingmakers launcher reimplements these classes under the same names,
because the engine binds to them by name. Rockstar's code isn't copied:
these are the names, signatures and call order only.

## Libraries

`split_config.arm64_v8a.apk` → `lib/arm64-v8a/`. Load order, dependencies
first:

1. `libc++_shared.so`
2. `libopenal.so`
3. `libVendor_mpg123.so`
4. `libGame.so`

Not needed by the engine: `libflutter.so` and `libapp.so` (Rockstar's
Flutter UI), `libsentry*.so`, `libz.so` and `libdatastore_shared_counter.so`.

## Game data

Asset packs:

| Pack | Delivery | How the engine gets it |
|---|---|---|
| `data_main` | install-time (`split_data_main.apk`, 1.35 GB) | through the `AssetManager` |
| `data_sfx1`, `data_sfx2`, `data_streams` | downloaded on first run | by file path, into the game's private storage, which other apps can't read |

`GameThread.onInitialSetup` reads the intent extras `packNames` and
`packPaths`. They default to `["data_main"]` and `[null]`, where `null`
means "read this pack from the AssetManager". The launcher passes
exactly that, with the GTA package's AssetManager:

```java
createPackageContext("com.rockstargames.gtasa", 0).getAssets()
```

That AssetManager includes install-time pack splits. The audio packs
aren't available, so how the engine behaves without them is the first
thing to check on a phone.

## Natives: `com.rockstargames.oswrapper.GameNative` (all `static`)

```
implIsInitialized()Z
implOnInitialSetup(DeviceInfo, AssetManager, String[] packNames, String[] packPaths)
implOnActivityCreated(GamePlatformServices, boolean initialized)
implOnActivityDestroyed()  implOnPause()  implOnResume()  implOnLowMemory()
implOnSurfaceCreated()  implOnSurfaceChanged(Surface, int w, int h)  implOnSurfaceDestroyed()
implOnDrawFrame(float seconds)
implOnTouchStart(int id, float x, float y)  implOnTouchMove(...)  implOnTouchEnd(...)
implOnBackButtonPressed()  implOnAccelerometerChanged(float, float, float)  implOnNetworkChanged(int)
implOnGamepadConnected(int)  implOnGamepadDisconnected(int)  implOnGamepadResume(int[])
implOnGamepadButtonDown(int, int)  implOnGamepadButtonUp(int, int)  implOnGamepadAxesChanged(int, 6 floats)
implOnHttpRequestResponse(int, int, String, String[], String[])  implOnHttpRequestData(int, byte[])
implOnHttpRequestFinished(int)  implOnHttpRequestError(int, int)
implOnPlaylistOpenComplete(boolean, int)
implOnRockstarSetup(String env, String userId)  implOnRockstarStateChanged(boolean)
implOnRockstarIdChanged(String)  implOnRockstarTicketChanged(String)
implOnRockstarInitialComplete()  implOnRockstarGateComplete(int, boolean)
implOnRockstarSignInComplete()  implOnRockstarSignOutComplete()
implOnRockstarCloudDisabledComplete()  implOnRockstarAccountDeletionComplete()
```

## Callbacks: `GamePlatformServices(GameActivityBase, GameView)`

The engine calls these on the object it gets in `implOnActivityCreated`:

- `getAppVersion()`, `getDeviceLocale()`, `quit()`, `openLink(String)`
- splash: `showSplashScreen()`, `hideSplashScreen()`, `isSplashScreenVisible()`,
  `setSplashImage(String)`, `setSplashText(String)`
- movies: `playMovie(String, boolean)`, `pauseMovie(boolean)`, `stopMovie()`,
  `isMoviePlaying()`, `setMovieText(String)`, `setMovieTextScale(int)`
- user music: `playlistOpen(String)` and the rest of the `playlist*` methods
- HTTP: `httpGet`, `httpHead`, `httpPost`, `httpCancel`
- Rockstar account: `rockstarShowInitial()`, `rockstarShowGate(int)`,
  `rockstarSignIn()`, `rockstarSignOut()`, `rockstarFetchId()`,
  `rockstarInTrial()`, `rockstarRequestReview()`, `rockstarShowCloudDisabled()`,
  `rockstarAccountDeletion()`, `rockstarSetLocalePriority(String)`

The Rockstar calls that open UI expect the matching `implOnRockstar…Complete`
afterwards. The launcher has no Rockstar SDK, so it answers each at once:
gate passed, initial screen done, not signed in.

## Other classes the engine binds to

- **`DeviceInfo(Context)`:** public final fields `cpuFrequency` (int),
  `hardware`, `manufacturer`, `model`, `product` (String), `hasTouchScreen`,
  `hasVibrator`, `isPhone`, `isTvDevice` (boolean) and `osVersion` (int).
- **`com.nvidia.devtech.NvUtil`:** `getInstance()`, and a string key/value
  store: `getAppLocalValue`, `setAppLocalValue`, `hasAppLocalValue`,
  `getParameter`. Before setup the activity sets `STORAGE_ROOT` (the
  external files directory, with a trailing `/`) and `STORAGE_ROOT_BASE`
  (the part before `/Android`).

## Threading and startup

`GameThread` is a single thread named `GameThread`. Every `impl*` call is
queued and run on it. Each loop iteration:

1. drains the queue
2. if the view has focus and `implIsInitialized()`, calls
   `implOnDrawFrame(ms since last frame / 1000f)`

The engine creates its own EGL context on the `Surface`.

`GameActivityBase.onCreate`:

1. `NvUtil.setActivity`, then the `STORAGE_ROOT` and `STORAGE_ROOT_BASE` values
2. `NvAPKFileHelper.setContext`
3. `onActivitySetup()`
4. `Rockstar.setup(...)` (the launcher skips this)
5. hide the system UI, inflate a `GameView` (`SurfaceView`, format set,
   keep-screen-on, focusable)
6. `services = new GamePlatformServices(this, view)`
7. `GameThread.start(services)`
8. `onInitialSetup(this)`
9. `onRockstarSetup(env, userId)`
10. `view.setup(this)`
11. `onActivityCreated(this, view, initialized)`
12. `onActivityReady(initialized)`

## Touch

`GameView` passes each pointer's id and raw view pixel coordinates:
`onTouchStart` on down, `onTouchMove` for every pointer on move, and
`onTouchEnd` on up. On cancel, every active pointer gets `onTouchEnd`.
