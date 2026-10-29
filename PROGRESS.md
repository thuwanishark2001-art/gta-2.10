# Kingmakers RP: progress

Kingmakers RP is an original, free, non-commercial roleplay server built on the
SA:MP 0.3.7 mobile client in this repo. No Play Store release and no
monetization: the GTA:SA engine and data are Rockstar's and are not covered by
this repo's MIT license.

## Task list

### Phase 0: audit
- [x] Read repo structure, connection flow and build requirements
- [x] `./gradlew assembleDebug` builds (see [docs/BUILD.md](docs/BUILD.md))
- [x] Remove committed signing keystores and hard-coded signing passwords
- [x] Remove the third-party Firebase config (`google-services.json`) and Firebase deps
- [x] Replace third-party server IPs with a configurable Kingmakers endpoint
- [x] Plan and risks (below)
- [ ] Decide how players supply their own game files (see Risk 1)
- [ ] Decide whether to purge old secrets from git history (see Risk 2)

### Phase 1: server foundation (`/server`)
- [ ] open.mp server + Pawn gamemode skeleton, config via env/file
- [ ] MySQL (mysql plugin) + migrations in `/server/sql`
- [ ] docker-compose for local dev (server + MySQL)
- [ ] Accounts: register/login, bcrypt, sessions, anti-spam, ban/mute/kick
- [ ] Characters: several per account, `First_Last` names, skin, spawn, stats
- [ ] Test plan: Pawn unit tests + scripted login against the dockerized server

### Phase 2: core RP systems
- [ ] Economy, jobs (taxi, trucker, miner, pizza, mechanic), vehicles, property,
      inventory, factions + MDT, admin/helper + reports + logs, RP chat commands

### Phase 3: mobile UX
- [ ] Login, character select, inventory, phone, speedometer, HUD (documented RPCs)

### Phase 4: quality and ops
- [ ] Tests, server-side anti-cheat, logging, backups, deploy script, docs

---

## Phase 0 report (2026-10-29)

### Repository structure

| Path | What it is |
|---|---|
| `app/src/main/cpp/samp/` | The SA:MP client, built as `libsamp.so` (C++20, ~440 files). `net/` is networking and RPC handlers, `game/` + `game_sa/` wrap the engine, `gui/` is the ImGui-based in-game UI, `java/` is the JNI bridge, `voice_new/` is voice chat, `vendor/` holds RakNet, ImGui, inih, BASS, armhook. |
| `app/src/main/cpp/opus/` | Opus codec for voice chat. |
| `app/src/main/java/` | Android layer: `com.samp.mobile.game.SAMP` (the launcher activity), `game/ui/*` (Java HUD, dialogs, keyboard, loading screen), `com.nvidia.devtech` + `com.wardrumstudios` (the engine's Android glue). |
| `app/src/main/jniLibs/` | Prebuilt `.so` files: **Rockstar's `libGTASA.so` and `libSCAnd.so`**, OpenAL, LuaJIT, MonetLoader (Lua plugin loader), `libImmEmulatorJ.so`. |
| `app/src/main/assets/` | **Rockstar game data** (`Text/*.gxt`, `data/mainV1.scm`, `handling.cfg`, `socialclub/`), fonts, client `settings.ini`. |
| `app/src/main/res/` | Android layouts and images for the Java HUD and dialogs. |
| `dumps_libGTASA_32and64/` | Symbol dumps of `libGTASA.so` (offset reference for hooks). |
| `.safedk/` | Leftovers from an ad-SDK scanner. Not used by the build. |

### How the client connects

1. `SAMP` activity starts, the engine loads `libGTASA.so`, then `libsamp.so`
   hooks it (shadowhook + armhook, offsets for GTA:SA Android **2.10**).
2. `CSettings` reads `<storage>/SAMP/settings.ini`: `[client] name`,
   `password`, `serverid`.
3. `main.cpp` picks a host/port from `servers.h` by `serverid` and creates
   `CNetGame`, which opens a RakNet client connection (UDP).
4. Handshake is standard SA:MP 0.3.7: `ID_AUTH_KEY` challenge, then
   `RPC_ClientJoin` with netgame version, nickname, challenge and
   `SAMP_VERSION`. So any 0.3.7-compatible server (SA-MP 0.3.7 or open.mp)
   accepts it.
5. RPCs: 42 core handlers registered in `net/netrpc.cpp` and 80 script RPCs
   in `net/scriptrpc.cpp` (dialogs, textdraws, 3D labels, objects, etc.). The
   client sends the usual sync packets and RPCs (spawn, death, chat, command,
   dialog response, damage).
6. **Custom channel:** packet id `251` is handled by
   `CNetGame::Packet_CustomRPC` with a `uint32` sub-id. Only sub-id `99`
   exists today (a texture/version check). This is the hook for our own
   Kingmakers RPCs in Phase 3; the server side needs a packet-level plugin
   (open.mp component or Pawn.RakNet) to send them.
7. Java UI is driven from native code through `java/jniutil.cpp`
   (`showDialog`, `updateHudInfo(health, armour, weapon, ammo, clip, money,
   wanted)`, `showHud`, `setTab`, loading screen, keyboard).

### What it needs to build

JDK 17+, Android SDK 34, NDK 26.2.11394342, CMake 3.22.1, Gradle 8.9,
AGP 8.4.0. Full steps in [docs/BUILD.md](docs/BUILD.md).

### Build fixes made in Phase 0

- Dropped dead or blocked Maven repos (JCenter, AppLovin, Aliyun, Splunk,
  Sonatype snapshots). Added Google's Maven Central mirror.
- Removed unused dependencies (Firebase, Volley, Glide, Retrofit, PRDownloader,
  image slider, ini4j, 7-zip, navigation, lifecycle). Only one dead file used
  any of them (`launcher/other/UnZipCallback.java`, removed).
- `paranoid-gradle-plugin` was bundled into the app as a runtime dependency
  (dragging in Kotlin and logback). Replaced with `compileOnly paranoid-core`
  for the `@Obfuscate` annotation.
- Set `ANDROID_STL=c++_shared` (required by shadowhook's prefab package).
- Fixed three wrong-case `#include`s that only worked on Windows.
- Removed the manifest entry for `launcher.activity.MainActivity`, a class that
  doesn't exist.

**Tested:** `./gradlew clean assembleDebug` succeeds; the APK contains
`libsamp.so` for both ABIs and is signed with the standard debug key.
**Not tested:** running it on a device or emulator (no Android device here).

### Secrets and third-party data cleanup

- `app/src/main/key.jks`, `keys/debi.jks` and the plaintext keystore
  passwords in `app/build.gradle` are removed. Release signing now reads a
  git-ignored `keystore.properties`.
- `app/google-services.json` (another project's Firebase config) and the
  Google Services plugin are removed. The native Crashlytics header stays; it
  is a no-op when Firebase isn't present.
- `servers.h` no longer contains other servers' IPs. It now has one
  `KM_SERVER_HOST`/`KM_SERVER_PORT`, defaulting to `10.0.2.2:7777` and
  overridable from CMake.

---

## Plan

1. **Phase 1 is server-only and independent of the client.** Build
   `/server` with open.mp (0.3.7-compatible, actively maintained, Linux
   Docker friendly), a Pawn gamemode split into modules
   (`accounts`, `characters`, `admin`, ...), the `mysql` plugin (R41+),
   `samp-bcrypt` for passwords, env-driven config and docker-compose.
   Test with a scripted bot client or the APK on an emulator.
2. **Client rebranding (start of Phase 3):** package id
   (`com.samp.mobile` to a Kingmakers id), app name, icon, splash/loading
   screen, strings. Keep README credits and LICENSE.
3. **Custom RPCs (Phase 3):** extend `Packet_CustomRPC` (packet 251) with
   documented Kingmakers sub-ids for HUD stats, inventory, phone, character
   select. Server side via Pawn.RakNet or an open.mp component.
4. **Permissions trim:** drop `INSTALL_PACKAGES`, `REQUEST_INSTALL_PACKAGES`,
   `RECORD_AUDIO` unless voice chat stays, and reassess
   `MANAGE_EXTERNAL_STORAGE`.

## Risks

1. **The APK currently ships Rockstar's files.** `libGTASA.so`, `libSCAnd.so`
   and the game data in `assets/` are compiled into every APK. Sharing that
   APK would break the "players supply their own copy" rule. Fix: strip
   them from the repo and APK, and on first launch copy them from the
   player's own installed GTA:SA (Android, version 2.10, which the hook
   offsets require). Needs a decision before any APK is shared.
2. **Secrets stay in git history.** Removing files on a branch doesn't erase
   old commits. The keystore isn't ours, so the practical fix is to never use
   it, plus optionally rewrite history (force-push, breaks forks).
3. **Engine version lock.** All hooks target GTA:SA Android 2.10. Players
   with a different version will crash.
4. **Closed prebuilts.** `libmonetloader.so`, `libImmEmulatorJ.so` and
   `libbass.so` are binaries of unknown or proprietary license. MonetLoader
   also lets players run Lua mods, which is a cheating vector.
5. **Custom RPCs need a packet-level server plugin.** Pawn alone can't send
   packet 251; Pawn.RakNet (SA-MP) or an open.mp component is required.
6. **No device testing in CI.** The build is verified; gameplay needs a
   real Android device or an ARM emulator with the player's game files.
