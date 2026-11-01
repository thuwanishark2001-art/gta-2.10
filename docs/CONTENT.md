# Server content: files the app downloads

The server can send players its own files without a new APK: a logo and
background for the app's start screen, and sounds. The app downloads them
before the game starts and keeps them on the phone, so each file is only
downloaded again when it changes.

**Only add files you made or have the rights to share.** Never put game
files from GTA: San Andreas here. Players use their own copy of the game.

## What you can add

Put files in `server/content/` on the server:

| File | Used for |
|---|---|
| `logo.png` | the logo on the start screen (square, about 512x512) |
| `background.jpg` or `background.png` | the start screen background (landscape, about 1920x1080) |
| `sounds/<name>.mp3` (or `.ogg`, `.wav`) | `/playsound`, and the welcome sound |
| `models/<id>.dff` + `models/<id>.txd` (or `models/<id>/<texture>.png`) | a custom skin (or vehicle) that replaces model `<id>` |

`./scripts/deploy.sh` puts a default Kingmakers RP logo and background into
`content/` (from `server/content-defaults/`) if there isn't one there yet.
To use your own, replace `content/logo.png` or `content/background.jpg`.
Deploying never overwrites them.

Rules for file names:
- Use only letters, digits, `_`, `-`, `.` and `/`. Names with spaces are
  skipped (the content service logs them).
- Each file can be up to 50 MB, and all content together up to 300 MB.
  Keep sounds small: phones on mobile data download everything on first
  launch.

## Adding or changing files

From your computer, copy a file to the VPS. Use your own path to the file:

```
scp welcome.mp3 root@65.20.88.67:/opt/kingmakers/server/content/sounds/
```

Then, on the VPS:

```
cd /opt/kingmakers/server
docker compose restart content
```

Players get the new files the next time they open the app. The app shows
"Downloading server content…" while it does.

To check what the server is offering, open
`http://65.20.88.67:7780/manifest.json` in a browser. It lists each file with
its size and checksum.

## Sounds in the game

- **`/playsound all sounds/welcome.mp3`** plays a sound for everyone.
- **`/playsound <player> sounds/horn.mp3`** plays it for one player.
- **`/stopstream all`** (or `/stopstream <player>`) stops it.

These commands need admin level 2. The app plays the downloaded copy. If a
player doesn't have it yet, the sound streams from the server instead.

**Welcome sound:** to play a sound to every player when they log in, set it
in `server/.env`, then run `./scripts/deploy.sh`:

```
KM_WELCOME_SOUND=sounds/welcome.mp3
```

## Setup (once)

`./scripts/deploy.sh` does this for you:
- starts the `content` service on TCP port 7780
- opens that port in `ufw`
- writes `KM_CONTENT_URL=http://<this server's IP>:7780` into `.env`

If players reach the server at a different address, such as a domain name,
set `KM_CONTENT_URL` in `.env` yourself.

If your provider also has a firewall in its website (for example, a Vultr
Firewall Group), allow **TCP 7780** there too.

The app finds the content service at the server address it was built with
(`kmServerHost` in `app/build.gradle`) on port 7780. To use another port, set
`KM_CONTENT_PORT` in `.env` and build the app with `-PkmContentPort=<port>`.

## How it works

1. **The content service.** In `docker-compose.yml`, `content` is nginx. It
   serves `server/content/` read-only.
2. **The manifest.** At start, `content-server/40-km-manifest.sh` writes
   `manifest.json`, listing each file's path, size and SHA-256.
3. **Downloading.** The app's start screen fetches the manifest
   (`ContentSync.java`). It then:
   - downloads what changed into its own folder
     (`Android/data/com.samp.mobile/files/content/`)
   - checks each file's size and SHA-256 before using it
   - deletes files the server no longer lists

   If the server can't be reached, the app keeps the files it already has
   and starts anyway.
4. **Playing sounds.** When the server plays an audio stream from the
   content service, the client checks for the downloaded copy
   (`audiostream.cpp`). If it has one, it plays that copy instead of
   streaming.

## Custom skins

A custom skin replaces one of the game's skin ids for everyone. The easiest
way to add one is the script, on the VPS:

```
cd /opt/kingmakers/server
./scripts/add-skin.sh 230 /root/skins/myskin.dff /root/skins/myskin.txd
```

Some skin packs ship their textures as `.png` pictures instead of a `.txd`.
For those, give the folder of pictures instead:

```
./scripts/add-skin.sh 230 /root/skins/myskin.dff /root/skins/textures
```

The script:
- reads which textures the model uses, copies each matching picture
  (it matches names loosely: `Body4 (1).png` counts as texture `body4`, and
  a texture named `23 1` is saved as `231.png`)
- lists any it can't find
- restarts the content service

To undo, run `./scripts/add-skin.sh --remove 230`.

By hand, the files are:

```
content/models/230.dff
content/models/230.txd          (or content/models/230/<texture name>.png)
content/models/textures/*.png   (optional: pictures every custom model can use)
```

After changing them by hand, restart the content service. Players get the files the next time they
open the app. Anyone with skin 230 then looks like your custom skin:
`/setskin <player> 230`, or put the id in the character creation lists in
`gamemodes/km/characters.inc`.

What works:
- **Files:** a PC San Andreas skin (`.dff` + `.txd`), the format skin
  sites share.
- **Textures:** a PC `.txd` (DXT1, DXT3, DXT5, 32/24/16-bit, paletted), or
  `.png` pictures named after the textures. The app converts them for the
  phone. Textures from other platforms (PS2, mobile) are skipped.
- **Missing textures:** if the model needs a texture you didn't provide, the
  phone's log says `CustomModels: the model wants texture "<name>"`.
- **Choosing an id:** pick a skin nobody uses, and keep the replacement the
  same kind (a man's skin for a man's id) so it uses the right walk and
  voice. Ids 1-6, 8, 42, 65, 74, 86, 119, 149, 208, 273 and 289 aren't in
  every copy of the game, so they can't be used.
- **If a file is broken:** the stock skin stays. The phone's log says
  `CustomModels: model <id>: <id>.dff didn't load`.
- **Vehicles:** these work the same way (`models/<vehicle id>.dff/.txd`),
  but treat them as experimental. A vehicle model with the wrong parts or
  collision can behave oddly.

Only use skins whose authors allow you to share them, and never files taken
from the game itself.

## How custom models work

`CFileLoader::LoadClumpFile` is the engine function that builds each skin
or vehicle when it streams in. The client hooks it
(`app/src/main/cpp/samp/game/custommodels.cpp`). When a downloaded
`models/<id>.dff` exists, the client:
1. hands that file to the engine instead of the stock model
2. decodes the `.txd` or `.png` textures to RGBA (`txddecode.h`,
   `pngdecode.h`, tested by `app/src/test/txd/run.sh`)
3. serves its textures by name while the model loads, falling back to the
   game's textures for any it doesn't have
