# Porting the client to GTA:SA 2.11

The Play Store now ships GTA: San Andreas **2.11.311**. The client was
written for **2.10**. This is what moving to 2.11 involves, based on the
2.11 arm64 engine from a test phone (analysed locally, never committed).

## What changed in the game

| | 2.10 | 2.11.311 |
|---|---|---|
| Engine library | `libGTASA.so` (+ `libSCAnd.so`) | `libGame.so` (+ `libopenal.so`, `libVendor_mpg123.so`, `libc++_shared.so`) |
| Compiler | clang 5 (2017) | clang 21 with LTO and PGO |
| Symbols | full `.symtab` and DWARF debug info | stripped: exported symbols only |
| Java ↔ engine | NVIDIA `NvEventQueueActivity`, Wardrum `WarMedia`/`WarGamepad`/… (4 JNI exports) | `com.rockstargames.oswrapper` (38 JNI exports) |
| Input | NVEvent queue | inside oswrapper; `NVEventGetNextEvent` is gone |
| Audio | Java (`WarMedia`) | native OpenAL + mpg123 |

## Blocker: where the game data lives

2.11's APKs (`base.apk`, `split_config.arm64_v8a.apk`) hold no game
data: no `texdb`, `gta3.img`, scripts or audio. The launcher activity is
`com.rockstargames.gtasa.DownloaderActivity`. On first run it downloads
the data with Play Asset Delivery (`downloadAssetPacks`, `getPackLocation`),
and Play Core stores those packs in the game's private storage
(`files/assetpacks`).

Android doesn't let another app read that folder, and `adb` can't
either without root. The client's model, "use the player's own
install", reaches the engine code but not the data in 2.11, unless some of
it comes as install-time packs, which become readable split APKs.
`adb shell pm path com.rockstargames.gtasa` on a phone answers that.

Also in 2.11: a Flutter UI (`libflutter.so`, `libapp.so`), Sentry crash
reporting, and Play licensing (`CHECK_LICENSE`).

## Milestone 1: the 2.11 engine runs in the launcher (2026-11-01)

On a test phone (Honor, Android 16, GTA:SA 2.11.311), the launcher runs
the player's own 2.11 engine through the new oswrapper host
(`docs/OSWRAPPER-2.11.md`). It got through the main menu, a new game and
the intro cutscene, and into Grove Street: HUD, radar, peds, traffic,
touch controls, and backgrounding and resuming. There's no multiplayer
client yet, and no sound.

What it took, found with on-device diagnostics (`cpp/kmdiag`: log copy,
crash reports, a game-thread hang watchdog, all saved to Downloads):

- Pass only `data_main`, through the game's AssetManager.
- `cpp/km211` patches for the missing audio packs:
  - `ZIPFile::Find` on a missing archive returns "not found". The first
    crash was a null `ZIPFile` in `CAEMP3TrackLoader::GetDataStream`.
  - `IsAmbienceTrackActive`, `IsCutsceneTrackActive` and `IsRadioOn` report
    false, and the cutscene preload doesn't wait. The game thread was
    sleeping forever in `StopAmbienceTrack` under the intro.

Sound needs the audio packs, which other apps can't read.

## Milestone 2: the multiplayer client runs on 2.11 (2026-11-01)

On the same phone, `SAMP211` (the 2.11 host plus libsamp, PR #12) starts
the client and shows SA-MP's connecting view over Santa Maria Beach,
drawn by the 2.11 engine. The client's chat and debug overlay are up, it
holds 30 fps, and it retries the server every 6 s without crashing. It
can't join yet: the build points at a placeholder server address.

What it took beyond the address port, in test order:
1. `setStorageRoot` is a libsamp native, so it's called after loading
   libsamp. The early crash left no log, so kmdiag now records Java
   exceptions too, and the session is marked at the start of `onCreate`.
2. Without SA-MP's `main.scm`, Rockstar's story script ran the intro
   mission. `CTheScripts::Process` now runs only while `CGame::Initialise`
   is on the stack. That first pass creates the player, and without it
   the camera crashed.
3. That first pass also fades to black for the intro, so the client
   fades back in after `CGame::Initialise`.
4. `CJavaWrapper` called Java through `GetEnv` without checking it, which
   leaves the `JNIEnv` unset on an unattached thread. It now attaches the
   thread first. The two runtime crashes before this change haven't come
   back.

Still open: the server address, the SA-MP texture database (`samp`), the
cosmetic patches marked TODO 2.11, and sound.

## Milestone 3: playing on a real server (2026-11-01)

On the same phone, the client joins the Kingmakers server on a VPS
(Mumbai), registers, logs in, spawns a character and plays: walking,
streaming cars, the Kingmakers HUD and panels, owner commands over RCON,
reconnecting after a dropped connection, and in-car internet radio.

Fixed on the way, in test order:
1. **Crashes in ART's fault handler** after connecting: kmdiag gave the UI
   and game threads one shared static alternate signal stack, so two
   threads taking a signal at once (ART uses SIGSEGV for implicit null and
   suspend checks) overwrote each other. Each thread now gets its own. This
   was also behind the earlier "~5 s after connect" crashes.
2. **Crash when a vehicle streamed in** (SIGILL at libGame+0):
   `AllVehicles__ProcessControl_hook` found the original function in a
   table of 2.10 vtable offsets. It now reads each class's original from
   its vtable slot before hooking.
3. **Invisible player**: the server's skin 2 is defined only by SA-MP's data
   pack, which the client doesn't ship. The client falls back to male01
   (7) instead of CJ (model 0, no body without clothes); the server offers
   only stock skins (migration 007). The client also reads the engine's own
   model table instead of swapping its GOT slot.
4. **No HUD**: the game's HUD was hidden for a Java HUD that only showed
   with `hud = true`. The Java HUD is now the default.
5. **`/rcon`** went to the gamemode as a command; it's now sent as RCON
   (packet 201), like the PC client.
6. **Reconnects** used the in-game (character) name; they now use the
   login name.
7. **Blip colours**: `GetRadarTraceColour` is inlined into
   `DrawEntityBlip`/`DrawCoordBlip`, so SA-MP colour codes weren't
   translated. Both functions are hooked to translate the trace colour for
   the draw.
8. **FPS**: `DoGameState` capped `RsGlobal.maxFPS` at 30; now 60.
9. **Quitting from the menu** (`GamePlatformServices.quit`) now disconnects
   from the server instead of timing out.

Sound: the engine's packs can't be read, so the client's BASS stream player
carries server audio (`/radio`, `/playstream`).

Still open: SA-MP's data pack (`samp` texture database, `samp.img`
objects, extra skins, font); hiding the menu map's legend text and zone
name; the aim-while-walking constant in `CTaskSimpleUseGun::SetMoveAnim`;
and a check in game that sprite icons aren't drawn black (`/testblips`).

## What carries over

`tools/port-libgtasa/port.py port` on the arm64 build:

- **641 of 685 addresses move automatically.** 634 are function entry
  points and data found by name. 7 come from fallbacks, listed separately
  for in-game checking:
  - 3 renamed functions: the text type changed from `unsigned short*` to
    `short*`, same ABI.
  - 2 statics found through the code that uses them.
  - 2 patch sites matched by their surrounding instructions.
- About 300 hooks by symbol name carry over, except 4 (below).

## What needs a person

- **About 35 instruction patches inside functions** (most in `patches.cpp`:
  radar, map menu, vehicle `ProcessControl`, `RemoveDriver`, render
  setup). The new compiler rearranged these functions, so each patch has
  to be re-derived: work out what it changes and find that spot in the 2.11
  code.
- **GOT redirects that no longer exist:** `Points` (multitouch) and the
  model-info stores. 2.10 code read these through the GOT, so the client
  could point them at its own copies. 2.11 addresses them directly, so the
  client has to patch each use or adopt the engine's own storage instead.
- **Changed signatures:** `DoGameState()` now takes a float, and
  `NvFOpen(path, mode, bool, bool)` is now `NvFOpen(path)`.
  `MainMenuScreen::OnExit` and `NVEventGetNextEvent` are gone.
- **About 6 statics** the stripped build no longer names (`deviceChip`,
  `StorageRootBuffer`, `glAlphaFuncQCOM`, two RenderWare pipeline-init
  functions…). Each has to be found by reading the code that uses it.
- **The Java host layer.** This is the biggest part. The client's
  launcher replaces the game's own Java classes with its copies of the
  2.10 ones. 2.11's engine talks to `com.rockstargames.oswrapper.*`
  instead, so those have to be rewritten against the 2.11 APK's
  `classes.dex`, along with input (multitouch) and audio setup.
- **The launcher** has to load `libGame.so` and its new dependencies
  rather than `libGTASA.so` + `libSCAnd.so`.
- **Struct layouts can't be checked.** The build is stripped, so
  `port.py structs` has nothing to compare against. A recompile with
  LTO doesn't change layouts by itself, but any class Rockstar edited
  would only show up in game.

## Not an option

Shipping or downloading a 2.10 APK for players. It's Rockstar's game, and
the client only ever uses the player's own installed copy.
