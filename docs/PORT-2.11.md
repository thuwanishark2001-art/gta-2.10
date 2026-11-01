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
