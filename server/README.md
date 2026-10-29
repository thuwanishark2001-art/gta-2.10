# Kingmakers RP server

An open.mp server with a Pawn gamemode and a MariaDB (MySQL compatible)
database. The mobile client in `../app` connects to it as a standard SA:MP
0.3.7 client.

## Run it locally

Requirements: Docker with Compose v2, `curl`, `git`, `unzip`.

```bash
cd server
cp .env.example .env          # then change the passwords
./scripts/fetch-deps.sh       # downloads pinned open.mp, compiler and plugins into .deps/
docker compose up --build
```

The server listens on UDP 7777. The Android emulator reaches your machine at
`10.0.2.2`, which is the client's default address (see `../docs/BUILD.md`).

Data lives in `server/data/` (database files and logs). Delete it to start over.

## What's in here

| Path | Purpose |
|---|---|
| `gamemodes/kingmakers.pwn` | Gamemode entry point and callbacks |
| `gamemodes/km/*.inc` | Modules: `accounts`, `characters`, `moderation`, `antispam`, `validate`, `cmd`, `db`, `config` |
| `sql/migrations/NNN_*.sql` | Database schema, applied in order by `scripts/migrate.sh` |
| `tests/km_tests.pwn` | Unit tests plus database and bcrypt integration tests |
| `scripts/fetch-deps.sh` | Downloads third-party binaries, pinned by SHA-256 |
| `scripts/entrypoint.sh` | Writes `config.json` and `mysql.ini` from env vars, starts the server |
| `scripts/test.sh` | Runs the test suite and a boot smoke test in Docker |

## Configuration

Set in `.env` (see `.env.example`):

| Variable | Default | Meaning |
|---|---|---|
| `KM_SERVER_NAME` | Kingmakers RP | Name in the server browser |
| `KM_PORT` | 7777 | UDP port |
| `KM_MAX_PLAYERS` | 100 | Player slots (the gamemode is compiled for up to 100) |
| `KM_RCON_PASSWORD` | empty | Empty disables RCON |
| `KM_ANNOUNCE` | false | List on the open.mp master list |
| `DB_NAME`, `DB_USER`, `DB_PASSWORD`, `DB_ROOT_PASSWORD` | | Database credentials |

Gameplay settings (bcrypt cost, login limits, character limit, spawn point,
anti-spam thresholds) are in `gamemodes/km/config.inc`.

## How players get in

1. Connect with any nickname of 3-20 letters, digits or underscores. That
   nickname is the **account** name.
2. New name: choose a password, then type it again. Known name: log in. Three
   wrong passwords kick you; ten failures from one IP in 15 minutes block that
   IP for the rest of the window. Log in within 2 minutes or you're kicked.
3. Pick a character, or create one (up to 3 per account): a `First_Last`
   name, gender, and outfit. The player's in-game name becomes the character
   name.
4. Position, health, armour, cash, interior, world and play time save on
   disconnect and every 5 minutes.

Passwords are hashed with bcrypt (cost 12). Plain passwords are never stored
or logged.

## Admin

Make yourself the first owner: set `KM_RCON_PASSWORD`, join, log in as a
character, then type `/rcon login <password>` and `/setadmin <your id> 5`.

| Level | Commands |
|---|---|
| 1 Moderator | `/kick [player] [reason]`, `/mute [player] [minutes] [reason]`, `/unmute [player]` |
| 2 Admin | `/ban [player] [days, 0 = permanent] [reason]`, `/unban [account]` |
| 5 Owner | `/setadmin [player] [level]` |

Bans cover both the account and the IP. Admins can't act on admins of the
same or higher level. Every action (and every character creation) is written
to the `admin_log` table. Chat floods and repeated messages get warnings, and
the third warning is an automatic 2-minute mute.

## Database

| Table | Contents |
|---|---|
| `accounts` | Username, bcrypt hash, admin/helper level, mute, IPs, timestamps |
| `account_sessions` | One row per login: IP, start, end, end reason |
| `login_attempts` | Every password attempt, for the per-IP limit |
| `characters` | Name (unique), gender, skin, level, cash, position, health, play time |
| `bans` | Account and/or IP, reason, who, expiry, lifted |
| `admin_log` | Every moderation action |
| `schema_migrations` | Which migration files have been applied |

All times are UTC. To change the schema, add a new numbered file in
`sql/migrations/`. Never edit a migration that has already run.

## Tests

```bash
./scripts/test.sh
```

It builds the image, starts a throwaway database, applies the migrations,
runs `tests/km_tests.pwn` (validation, anti-spam, command parsing, schema
constraints, ban lookup, mute expiry, cascades, bcrypt) and then checks that
the real gamemode boots and connects. It exits non-zero on any failure.

Not covered automatically: the in-game dialog flow with a real client. That
needs the APK on a device or emulator.

## Pinned versions

open.mp 1.4.0.2779 (binary reports 1.4.0.2783), Pawn compiler 3.10.11,
MySQL plugin R41-4, samp-bcrypt 0.4.1, sscanf 2.13.8, omp-stdlib at
`17c1b01`, MariaDB 11.4.
