# Running the Kingmakers RP server

This covers a single Linux machine (a VPS is fine) running the server stack
from `server/` with Docker Compose. For local development see
`server/README.md`.

## Requirements

- Linux x86-64 with Docker Engine and the Compose v2 plugin
- 1 vCPU and 1 GB of RAM are enough for a small community; MariaDB is the
  main user of memory
- `git`, `curl`, `unzip`
- UDP port 7777 open to players (or whatever `KM_PORT` is set to)

Only the game port needs to be public. The database has no published port
and is reachable only from the other containers.

## First install

```bash
git clone <this repository> kingmakers && cd kingmakers/server
cp .env.example .env
# Edit .env: set DB_PASSWORD and DB_ROOT_PASSWORD to long random values,
# KM_SERVER_NAME, and KM_RCON_PASSWORD if you want RCON.
./scripts/deploy.sh
```

`deploy.sh` refuses to run with the example passwords. It downloads the
pinned dependencies, builds the server image, applies database migrations,
starts everything and waits until the gamemode has loaded.

Then make yourself owner: join, log in as a character, run
`/rcon login <KM_RCON_PASSWORD>` and `/setadmin <your id> 5`. You can clear
`KM_RCON_PASSWORD` afterwards and redeploy to turn RCON off.

The client needs to know the server address: build it with
`-DKM_SERVER_HOST=<your server IP>` (see `docs/BUILD.md`).

## Updating

```bash
cd kingmakers/server
./scripts/deploy.sh --pull
```

This pulls the latest code, takes a database backup, rebuilds and restarts.
New migrations run automatically before the server starts. Players are
disconnected for the few seconds the server takes to restart.

## Backups

The `backup` service writes a compressed dump to `server/data/backups`
every `KM_BACKUP_INTERVAL_HOURS` (default 24) and keeps the newest
`KM_BACKUP_KEEP` (default 14). Dumps are checked for completeness before
they're kept. Copy that folder off the machine regularly: a backup on the
same disk doesn't protect against losing the disk.

Take one now:

```bash
docker compose run --rm --no-deps backup once
```

Restore one (this replaces the whole database):

```bash
docker compose stop server
docker compose run --rm --no-deps --entrypoint bash backup \
    /scripts/restore.sh /backups/km-YYYYMMDD-HHMMSS.sql.gz --yes
docker compose start server
```

`server/scripts/test.sh` checks backup, restore and retention on every run.

## Logs

| What | Where |
|---|---|
| Game server log (chat, connections, `[anticheat]`, `[admin]` lines) | `server/data/logs/server.log`; the previous 10 runs are kept as `server-<time>.log` |
| Container output | `docker compose logs <service>`; rotated at 10 MB, 5 files per service |
| Staff actions | `admin_log` table |
| Money changes | `money_log` table |
| Anti-cheat findings | `anticheat_log` table |

## Anti-cheat

Checked every second for each player:

| Check | Action |
|---|---|
| Weapon the server never gave | Removed; kick on the third time |
| Jetpack | Kick |
| Health or armour above 100 | Set back to 100 and logged |
| On-foot jump of more than 100 m in a second | Logged and shown to admins only (lag and falls can cause it) |
| Ground vehicle above 330 km/h | Logged and shown to admins only |
| Vehicle health above 1000 (repair hack) | Set back to 1000 and logged |
| Money | Can't be cheated: it's kept on the server and the client display is reset |

Admins see findings in chat as `[AC]` lines. Review them in `anticheat_log`
before banning anyone; the teleport check in particular is a hint, not proof.

## Before going public

- The client now takes GTA: San Andreas from the player's own install,
  but that hasn't been tested on a device yet, and the HUD weapon icons
  are still GTA:SA's (see `PROGRESS.md`, Risk 1). Test both before
  sharing the APK.
- Play through the jobs, vehicles and property on a device and fix map
  coordinates that are off.
- Place houses and shops with `/createproperty`.
