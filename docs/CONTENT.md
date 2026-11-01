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

## Not yet: custom skins and vehicles

Custom character skins and vehicle models need the game engine to load new
models. That's the next stage of this feature (adding model archives to the
2.11 engine's streaming). For now, content covers the start screen and
sounds.
