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
- [x] Decide how players supply their own game files: copied from their installed GTA:SA on first launch (see Risk 1)
- [x] Decide whether to purge old secrets from git history: no, the old keys are simply never used (see Risk 2)

### Phase 1: server foundation (`/server`)
- [x] open.mp server + modular Pawn gamemode, config via env (`.env`)
- [x] MariaDB via the MySQL plugin R41-4, migrations in `/server/sql/migrations`
- [x] docker-compose for local dev (database, migration job, server)
- [x] Accounts: register/login, bcrypt, sessions, login limits, anti-spam, ban/mute/kick
- [x] Characters: up to 3 per account, `First_Last` names, gender + outfit, spawn, saved stats
- [x] Tests: `server/scripts/test.sh` (71 checks + boot smoke test)
- [ ] Play through the login and character flow with the APK on a device

### Phase 2: core RP systems
- [x] Economy: cash/bank, ATMs, /pay, /transfer, hourly payday, levels, `money_log`
- [x] Jobs: trucker, pizza, taxi (NPC fares + player calls), miner, mechanic
- [x] Vehicles: dealership, owned vehicles, parking, locks, manual engine, fuel, job and faction vehicles
- [x] Property: houses and 24/7 shops, buy/sell, interiors, locks, shop till
- [x] Inventory: phone (number, SMS, 911, mobile banking), food, medkit, fuel can
- [x] Factions: LSPD, LSFMD, Government; ranks, salaries, duty, radio; police cuffs, charges, arrest, jail, MDT; medic treatment
- [x] Staff: helpers, /report and /helpme with a reports table, staff chat, admin tools, everything in `admin_log`
- [x] RP chat: local, /me, /do, /b, /s, /low, /w, /pm, /r, /o
- [x] Tests: 176 checks + boot test that loads factions and properties
- [ ] Play through every system on a device and fix map coordinates that are off
- [ ] Place the first houses and shops in game (`/createproperty`)

### Phase 3: mobile UX
- [x] UI protocol between server and client, documented in [docs/KMUI.md](docs/KMUI.md)
- [x] HUD stats panel (level and XP bar, bank, job) and speedometer with fuel
- [x] Toasts for job pay, paydays, level ups and SMS
- [x] Phone screen: messages, new SMS, call a taxi, 911
- [x] Login, character select and inventory: already mobile-friendly through the client's existing dialog renderer, so no separate screens
- [ ] Look at all of it on a device and adjust sizes and positions
- [x] Rebrand the client: "Kingmakers RP" name, placeholder gold crown icon and loading screen, other projects' logos removed
- [ ] Replace the placeholder crown with a real logo
- [ ] Change the package id `com.samp.mobile` (together with the game files decision: it moves the app's storage folder)

### Phase 4: quality and ops
- [x] Server-side anti-cheat (weapons, jetpack, health/armour, teleport), logged in `anticheat_log`
- [x] Daily database backups with retention, a restore script, both checked by `test.sh`
- [x] Deploy/update script with a backup before every update
- [x] Log rotation (game server log and container logs)
- [x] Operator guide: [docs/DEPLOY.md](docs/DEPLOY.md)
- [x] Tests: 198 server checks, backup/restore check, boot test, client parser host test
- [ ] Tune the anti-cheat once real players are on (false positives)
- [ ] Off-site copies of the backups (depends on where the server is hosted)

---

## End-to-end test bot, and a login bug it found (2026-10-31)

**The bug.** Registration and login could never complete, ever since
Phase 1. samp-bcrypt 0.4.1 calls back with
`(playerid, <our args>, hashid)` after `bcrypt_hash`, but with
`(playerid, success, <our args>)` after `bcrypt_verify`. The registration
callback expected the verify-style order, so it read the connection
counter as 0, decided the player had changed, and returned without showing
the confirm dialog. The unit tests used no custom arguments, so they
couldn't see it. Fixed in `accounts.inc`, and `km_tests.pwn` now checks
both orders against the real plugin.

**The bot.** `server/tools/kmbot` is a headless SA:MP 0.3.7 client built
from the mobile client's own RakNet, handshake and encryption code (plus a
stub `main.h`). It runs on Linux and plays a fixed scenario against the
real open.mp server:
1. Register (password and confirm), try an invalid character name and get
   it explained, then create a character (name, gender, outfit).
2. Ask for a class and spawn like the client, then get the spawn
   message and the `KM1|hud` UI event.
3. Run `/stats`, `/balance`, an unknown command, `/me`, `/jobs` and `/phone`
   (`KM1|phone`).
4. Reconnect, enter a wrong password and get it explained, log in, pick the
   character from the list, spawn, then `/stats`.

`scripts/test.sh` builds it (host `g++`, or a gcc container) and runs it
inside the compose network after the boot test. Every step passes.

**Movement (follow-up).** The bot now sends on-foot position updates like
the client (the 68-byte `ONFOOT_SYNC_DATA`). It follows the server's spawn
info, teleports and checkpoints. A second scenario, `economy`:
1. Deposits and withdraws at an ATM, and is refused an overdraft.
2. Takes the miner job, walks into the rock checkpoint and mines.
3. Carries the ore to the drop-off and gets the `+$90` toast.
4. Ends with $440 cash and $150 in the bank.

A third scenario, `admin`, logs in to RCON with packet 201 (the test stack
sets an RCON password), makes itself owner, then:
- Property: creates a house, is refused buying it with too little money,
  gets $2,000 from `/givemoney`, buys it, and is moved into the interior and
  back out by `/enter` and `/exit`. It sells for $600.
- Police: joins the LSPD at rank 6, goes on duty at HQ, opens its MDT entry
  (licence and ticket lines present), and goes off duty.
- Reports: files a report, sees it in `/reports`, then takes and closes it.

`test.sh` checks the exact `money_log` and `admin_log` for it.

`test.sh` then checks the exact `money_log` trail. The anti-cheat logged
the bot's 3 km jump to the quarry as a teleport and, as designed, didn't
kick.

**Also learned from it** (no code change needed): open.mp drops dialog
responses to input and password dialogs unless the list item is -1. The
mobile client already sends -1 there. The server's command flood limit (5
per 3 s) works.

## Licences, tickets, vehicle anti-cheat (2026-10-31)

- Migration `006_licences_tickets`: `characters.driving_licence` and a
  `tickets` table.
- `km/licence.inc`: `/drivingtest` costs $500. It's a timed 5-marker route
  in a test car that has to come back with 850+ health, and it fails on
  timeout, death, disconnect or a wrecked car. Also `/licence`,
  `/showlicence`, police `/ticket`, and `/tickets` and `/paytickets`.
  Paying only marks the tickets that were listed, so one written meanwhile
  stays open. The MDT now shows the licence and the number and total of
  unpaid tickets.
- Anti-cheat: a ground vehicle above 330 km/h is reported (aircraft are
  excluded), and vehicle health above 1000 is reset and reported.
- Found while adding this: the MDT query had outgrown its 512-cell buffer
  and would have been cut off in game. It's now 1024 (`MDT_QUERY_SIZE`),
  and a test checks that the longest possible query fits.
- Tests: 214 server checks pass (8 new). The licence office and route
  coordinates are guesses near City Hall and need checking in game.

## Client cleanup (2026-10-31)

- **HUD weapon icons**: the 45 GTA:SA icons are replaced by 11 original
  vector silhouettes. Each GTA weapon id maps to a category (fist, melee,
  pistol, shotgun, SMG, rifle, sniper, heavy, thrown, tool, parachute) in
  `HudManager.weaponIcon`.
- **MonetLoader and LuaJIT removed**: no code loaded them, and the Lua mod
  loader let players run cheats.
- **Permissions**: down from 17 to 8 (`aapt2 dump permissions`): wake lock,
  external storage, internet, vibrate, network and Wi-Fi state, and the
  microphone for voice chat.
- The APK is now 18.6 MB (was 66 MB at the start of this phase).
- Built with `assembleDebug`; the icons were checked in a rendered preview.
  Not run on a device.

## Game files from the player's install (2026-10-31)

Decision: players supply their own GTA: San Andreas. The APK went from
66 MB to 33 MB and no longer contains Rockstar's engine or game data.

**How it works**
- `GameFilesActivity` is now the launcher. It checks that
  `com.rockstargames.gtasa` is installed, at version 2.10. If it isn't,
  it explains what's needed and links to Google Play. The check runs
  again when the player comes back.
- On first launch, and whenever the installed game changes, `GameFiles`
  copies the engine libraries out of the installed game into the app's
  private storage and marks them read-only (Android 14 requires that).
  It handles both extracted libraries and libraries inside the APK or its
  split APKs. The libraries are `libSCAnd.so` and `libGTASA.so`, plus
  `libImmEmulatorJ.so` on 32-bit. Each one's SHA-256 is compared against
  the 2.10 builds the hooks were written for and written to the log.
- `GTASA.onCreate` loads those copies (dependencies first) before the
  engine starts, then `libbass` and `libsamp`.
- Game data the engine reads "from the APK" (`NvAPKFileHelper`, sounds,
  textures) now comes from the installed game's package via
  `createPackageContext(...).getAssets()`. Nothing is copied.
- The manifest declares the GTA:SA package in `<queries>` (needed to see
  it on Android 11+). The game activity is no longer exported.

**What was tested**
- `assembleDebug` builds. The APK contains no `libGTASA.so`,
  `libSCAnd.so`, `libImmEmulatorJ.so` or Rockstar assets. Its launcher is
  `GameFilesActivity`, and the `<queries>` entry is in the merged manifest.

**Untested, needs a device with GTA:SA 2.10 installed**
- The copy itself, and loading the engine from app storage.
- Whether Rockstar's own 2.10 libraries match the bundled ones (the
  hashes). If the bundled ones were patched, the official ones may need
  the same patches, and that will show up as a crash at startup.
- Whether every asset the engine needs is in the Play version's package,
  including Play asset packs.
- The game data in the storage folder (the "cache" players already
  install) is unchanged by this.

## Follow-up (2026-10-31): branding and Phase 2 gaps

- **Client branding.** The app is now called "Kingmakers RP". It uses a
  vector gold crown as a placeholder adaptive icon and loading-screen logo,
  on a dark gold background. The old icon (another project's red "S"),
  the Telegram logo on the loading screen and five other unused
  third-party logos are deleted. The native "TGRP" texts now say
  Kingmakers RP. The APK builds and reports the new label. I didn't
  change the package id, because it decides where the app keeps its files
  and settings; see Phase 3's open items.
- **Leaving while cuffed** now counts as fleeing custody. The player gets a
  "Fled custody" charge and 5 minutes of jail per wanted level (plus 5,
  at most 60), served when they come back. On-duty police are told.
- **Offline members**: `/uninvite First_Last` removes an offline member of
  lower rank, and `/roster` lists every member with their rank and when
  they last played.
- Tests: 206 server checks pass (8 new).

## Phase 4 report (2026-10-31)

**What works**
- Anti-cheat (`km/achooks.inc`, `km/anticheat.inc`, migration
  `005_ops`). The natives that give weapons or move players are wrapped,
  so the server knows what it handed out and when it teleported someone.
  Every second it checks each player. A weapon the server never gave is
  removed, and the third one is a kick. A jetpack is a kick. Health or
  armour above 100 is reset. An on-foot jump of over 100 m in a second
  outside a server teleport is reported to admins only. Findings go to
  `anticheat_log` and to online admins (one alert per player per 30 s).
- Backups: a `backup` compose service dumps the database every 24 hours
  into `data/backups`, keeps 14, and throws away incomplete dumps.
  `scripts/restore.sh` restores one (it needs `--yes`).
- `scripts/deploy.sh` refuses the example passwords, fetches the
  dependencies, builds, backs up the running database, starts everything,
  recreates the game server and waits until the gamemode has loaded.
- Logs: `server.log` starts fresh on every run and the last 10 are kept;
  container logs rotate at 10 MB.
- `docs/DEPLOY.md`: install, update, backup and restore, logs, and what the
  anti-cheat does.

**What was tested**
- `server/scripts/test.sh`: 198 checks pass. New ones cover the
  anti-cheat rules, the native wrappers and the `anticheat_log` row and
  cascade. A new step backs up, deletes a row, restores, checks that it's
  back, checks that a restore without `--yes` is refused, and checks
  retention.
- `deploy.sh` was run three times on a fresh machine state: the first
  install, an update with the pre-update backup, and the log rotation
  after a restart.

**Still broken or untested**
- The anti-cheat thresholds haven't met real players. The teleport check
  only reports, for that reason.
- No vehicle speed or vehicle health checks yet.
- Backups stay on the server's disk until someone copies them elsewhere.

## Phase 3 report (2026-10-31)

**Approach.** The plan was custom RPCs on packet 251, which needs a
packet-level server plugin (Pawn.RakNet). Its release binaries can't be
downloaded through this environment, and it would add another 32-bit
native dependency. Since we ship the client, it can instead recognise
tagged standard messages: UI events are client messages in a reserved
colour (`KM1|event|k=v`), and the UI's buttons send normal commands. No
plugin, stock clients keep working, and the server checks every button
like a typed command. Details in [docs/KMUI.md](docs/KMUI.md).

**What works**
- Server (`km/kmui.inc`): `/kmclient` handshake (accepted before login),
  HUD updates sent only when something changed, toasts, SMS and `/phone`.
  Stock clients get nothing new; `/phone` falls back to the chat text.
- Client native (`cpp/samp/kmui/`): drops UI events from chat, parses
  them, converts the text to UTF-8 and calls Java. It sends `/kmclient 1`
  after joining, reads the vehicle speed every 200 ms, and sends the UI's
  commands from the game thread through a small locked queue.
- Client Java (`game/ui/KmUi.java`): stats panel with a phone button,
  speedometer and fuel bar while in a vehicle, toast queue, phone card.
  It follows the existing HUD's show/hide, and Back closes the phone.

**What was tested**
- `server/scripts/test.sh`: 183 checks pass (7 new for the protocol:
  escaping, length limit, HUD payload, nothing sent to stock clients).
- `./gradlew assembleDebug` succeeds with the new code. The APK contains
  `KmUi`, the `kmSendCommand` JNI export in both ABIs, and the protocol
  strings in `libsamp.so`. JNI method signatures were checked against the
  Java methods by hand.
- `app/src/test/kmui/run.sh`: host test of the client's protocol parser
  (valid events, empty and malformed pairs, non-events) passes.

**Still broken or untested**
- Not run on a device: positions, sizes and the font may need adjusting,
  and the speed scale (x180) is the usual SA:MP one but hasn't been
  checked against the game's own speed.
- Client rebranding is still to do.

## Phase 2 report (2026-10-31)

**What works**
- Migration `004_rp_core` adds factions (seeded with ranks and salaries),
  vehicles, properties, inventory, criminal records, reports and `money_log`,
  plus new character columns (bank, XP, job, faction, wanted, jail, phone).
  Cash and bank can't go negative in the database either.
- New modules in `server/gamemodes/km/`: `chat`, `economy`, `offers`,
  `vehicles`, `jobs`, `property`, `inventory`, `factions`, `support`.
  `server/README.md` documents every command.
- All money moves through `Money_Give`: it refuses overdrafts, saves at
  once and logs a reason, the other character and the balances after.
- Async callbacks check the player's connection counter like Phase 1.
  Purchases create or claim the row first (`... WHERE owner IS NULL`, so two
  buyers can't both get a house) and only charge once that worked; if the
  payment then fails, the row is released again.
- Dialog answers are re-checked on the server (location, ownership, item
  counts), so a spoofed dialog response can't buy from afar.

**What was tested**
- `server/scripts/test.sh`: 176 checks pass (up from 71). New: game rules,
  Phase 2 schema constraints, the gamemode's own character, vehicle, MDT
  and report SQL run against the migrated schema, `Money_Give` /
  `Money_Charge` / inventory checked in the database, the faction and
  property loaders, and account-delete cascades.
- The boot test now also requires factions and properties to load with no
  query errors.
- Gamemode and tests compile with zero warnings.

**Still broken or untested**
- Nothing has been played with a real client yet. The commands, dialogs,
  checkpoints, vehicle spawns and interiors are untested in the world.
- Map coordinates (job sites, routes, dealership, ATMs, jail cell, faction
  cars, interiors) come from common SA:MP references. Some will be a few
  metres off, or inside something, until checked on a device.
- There are no houses or shops yet: admins place them with `/createproperty`.
- Gaps to close later: no driving licences or ticketing yet. Duty status
  isn't saved, so players go off duty when they reconnect. (Cuffs and
  offline members were fixed in the follow-up below.)

## Phase 1 report (2026-10-29)

**What works**
- `docker compose up --build` starts MariaDB 11.4, applies migrations, and
  starts open.mp with the Kingmakers gamemode. Details in
  [server/README.md](server/README.md).
- Accounts: the connecting nickname is the account. Register (password +
  confirm, bcrypt cost 12) or log in. 3 wrong passwords per connection, 10
  per IP per 15 minutes, 2-minute login timeout, one live session per
  account. Sessions and every login attempt are stored.
- Characters: list, create (validated `First_Last`, unique, max 3 enforced
  in SQL), choose gender and outfit, spawn, save on disconnect and every
  5 minutes. Cash is server-authoritative from the start.
- Moderation: `/kick`, `/mute`, `/unmute`, `/ban` (account + IP, timed or
  permanent), `/unban`, `/setadmin`, with rank checks and `admin_log` rows.
  Chat and command flood limits, repeat filter, auto-mute.
- Local IC chat (20 m) so players can talk; full RP chat is Phase 2.

**What was tested**
- `server/scripts/test.sh`: 71 checks pass (name/password validation,
  anti-spam, command parsing, sscanf, schema constraints, capped character
  insert, ban lookup incl. expired/lifted/IP bans, mute expiry in UTC,
  cascades, bcrypt hash/verify), plus a boot test of the real gamemode.
- Gamemode and tests compile with zero warnings.

**Still broken or untested**
- The in-game flow (dialogs, spectate-then-spawn) hasn't been run with a
  real client. It uses standard SA:MP dialogs, which the client supports.
- The plugins are 32-bit and the MySQL plugin is old: it needs MariaDB
  (or MySQL with native passwords) and an IP address instead of a hostname
  (the entrypoint resolves it).
- No password reset or email yet.

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
4. **Permissions trim:** done. Removed install-packages, licence check, boot,
   foreground service, all-files storage and Bluetooth permissions. Kept
   `RECORD_AUDIO` for voice chat.

## Risks

1. **Rockstar's files in the APK: fixed, but untested on a device.** The
   APK no longer contains `libGTASA.so`, `libSCAnd.so`,
   `libImmEmulatorJ.so` or the game data in `assets/`. They come from the
   player's own GTA:SA 2.10 (see the game files report below). Still from
   GTA:SA: the 45 HUD weapon icons
   were replaced too (see the cleanup report). Older commits still contain
   the removed files.
2. **Secrets stay in git history.** Decision: leave history alone. The
   keystore isn't ours and is never used again, so it has no value to us;
   rewriting history would break every fork and clone.
3. **Engine version lock.** All hooks target GTA:SA Android 2.10. Players
   with a different version will crash.
4. **Closed prebuilts.** MonetLoader and LuaJIT are removed: nothing loaded
   them, and the Lua mod loader was a cheating vector. `libImmEmulatorJ.so`
   now comes from the player's GTA:SA. `libbass.so` (proprietary, free for
   non-commercial use) is still bundled.
5. **Custom RPCs need a packet-level server plugin.** Pawn alone can't send
   packet 251; Pawn.RakNet (SA-MP) or an open.mp component is required.
6. **No device testing in CI.** The build is verified; gameplay needs a
   real Android device or an ARM emulator with the player's game files.
