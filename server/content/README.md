# Server content

Files in this folder are downloaded by the Kingmakers RP app before the game
starts, and kept on the phone. Changes reach players the next time they open
the app; no new APK is needed. See docs/CONTENT.md.

| File | Used for |
|---|---|
| `logo.png` | the logo on the app's start screen (square, about 512x512) |
| `background.jpg` or `background.png` | the start screen background (landscape, about 1920x1080) |
| `sounds/*.mp3` (or `.ogg`, `.wav`) | `/playsound` and `KM_WELCOME_SOUND` |
| `models/<id>.dff` + `models/<id>.txd` | a custom skin replacing skin `<id>` |

After adding or changing files, run `docker compose restart content` (or
`./scripts/deploy.sh`). Use only letters, digits, `_`, `-`, `.` and `/` in
names. Only add files you made or have the rights to share: never game
files from GTA: San Andreas.
