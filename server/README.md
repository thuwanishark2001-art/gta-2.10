# Kingmakers RP server

An open.mp server with a Pawn gamemode and a MariaDB (MySQL compatible)
database. The mobile client in `../app` connects to it as a standard SA:MP
0.3.7 client.

## Run it locally

(For a public server, see `../docs/DEPLOY.md` and use `scripts/deploy.sh`.)

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
| `gamemodes/km/*.inc` | Modules: `accounts`, `characters`, `moderation`, `chat`, `economy`, `offers`, `vehicles`, `jobs`, `property`, `inventory`, `factions`, `support`, plus helpers (`antispam`, `validate`, `rpcalc`, `cmd`, `db`, `util`) |
| `gamemodes/km/config.inc` | Gameplay numbers: pay, prices, limits, timers |
| `gamemodes/km/places.inc` | Fixed map locations: ATMs, gas stations, dealership, hospital, jail, job sites, faction vehicles |
| `gamemodes/km/achooks.inc`, `gamemodes/km/anticheat.inc` | Anti-cheat: wrappers that record what the server gave, and the checks |
| `gamemodes/km/kmui.inc` | Server side of the client UI protocol (`../docs/KMUI.md`) |
| `gamemodes/km/ids.inc` | Job, item, faction-type and property-type ids stored in the database (never renumber) |
| `sql/migrations/NNN_*.sql` | Database schema, applied in order by `scripts/migrate.sh` |
| `tests/km_tests.pwn` | Unit tests plus database and bcrypt integration tests |
| `scripts/fetch-deps.sh` | Downloads third-party binaries, pinned by SHA-256 |
| `scripts/entrypoint.sh` | Writes `config.json` and `mysql.ini` from env vars, rotates the log, starts the server |
| `scripts/backup.sh`, `scripts/restore.sh` | Database dumps (the `backup` service runs them daily) and restore |
| `scripts/deploy.sh` | Install or update on a server (see `../docs/DEPLOY.md`) |
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
| `KM_BACKUP_KEEP` | 14 | Database backups to keep in `data/backups` |
| `KM_BACKUP_INTERVAL_HOURS` | 24 | Hours between backups |

Gameplay settings (bcrypt cost, login limits, character limit, spawn point,
anti-spam thresholds, pay, prices, limits) are in `gamemodes/km/config.inc`.
Map locations are in `gamemodes/km/places.inc`.

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

## Gameplay

`/help` lists the commands a player can use, based on their job, faction and
staff level.

**Chat.** Normal chat is local (20 m). `/me` and `/do` for actions, `/b` local
OOC, `/s` shout, `/low`, `/w` whisper, `/pm` and `/r`, `/o` global OOC (off
until a moderator runs `/togooc`).

**Money.** Cash and bank are server-side only; the client's money display is
reset if it drifts. Every change goes through one function that refuses to go
below zero, saves at once and writes a `money_log` row with a reason.
ATMs: `/balance`, `/deposit`, `/withdraw`. `/pay` hands cash to someone next to
you; `/transfer` moves bank money (at an ATM or with a phone). Every hour of
play is a payday: $150 base pay, faction salary by rank, 0.1% interest (max
$500), and one XP. Level N needs 4 x N paydays.

**Jobs.** `/jobs` marks the sites on the map. `/takejob` at the site, `/work`
to start a shift, `/stopwork` to end it. Pay goes to cash.

| Job | Shift |
|---|---|
| Trucker (Ocean Docks) | Truck deliveries across San Andreas, paid by distance |
| Pizza (Idlewood) | 5 pizzas per load on a Pizzaboy, paid by distance, +50% for fast delivery |
| Taxi (Unity Station) | NPC fares paid by distance; players can `/calltaxi`, drivers `/taxiaccept` |
| Miner (Hunter Quarry) | Mine a rock face, carry the ore to the drop-off, $90 per ore |
| Mechanic (Idlewood) | Tow truck; `/repair` and `/refill` offers to drivers, who `/accept` |

**Vehicles.** Engines are manual (`/engine`, `/lights`). Buy at Coutt and
Schutz (`/buyvehicle`, up to 3, paid from bank then cash). Owned vehicles
appear while you play, at the spot you last `/park`ed; `/lock`, `/myvehicles`,
`/sellvehicle` (50% back). They use fuel: `/refuel` at a gas station or a fuel
can. Plates are `KM-` plus the vehicle id. Job and faction vehicles only start
for their worker or faction.

**Licences and tickets.** `/drivingtest` at the licensing office by City
Hall ($500): drive a test car through 5 markers within 4 minutes and bring
it back undamaged (health 850+). `/licence`, `/showlicence [player]`.
Driving your own car without one is allowed, but you're reminded that
police can fine you. Police write fines with
`/ticket [player] [amount] [reason]` (up to $5,000). Players see them with
`/tickets` and pay them all from the bank with `/paytickets`, at an ATM or
by phone. The MDT shows the licence and unpaid tickets.

**Property.** Lead admins place houses and 24/7 shops with
`/createproperty [interior] [price] [name]` at the door. Players
`/buyproperty` (up to 2), `/enter`, `/exit`, `/lock`, `/sellproperty` (60%
back), `/myproperties`. In a shop, `/shop` sells items; the money goes into
the owner's till (`/till`). Each property's inside is its own virtual world.

**Inventory.** `/inv` to use, give or throw away items: phone, water, burger,
medkit, fuel can. A phone gives you a number (`/number`, `/sms`, `/911`) and
mobile banking.

**Factions.** LSPD, LSFMD and Government are seeded in migration 004, with
six ranks each. A lead admin sets the first leader with
`/setfaction [player] [faction id] [rank]`. Members: `/f` radio, `/duty` at
HQ (uniform and equipment), `/members`, `/quitfaction`. Ranks 5 and 6 can
`/invite` (the player answers `/joinfaction`), `/uninvite` (a player, or a
`First_Last` name for someone offline), `/setrank` and `/roster` (all
members, online or not).
On-duty police and medics share `/d` and get `/911` calls.
Police: `/cuff`, `/uncuff`, `/su [player] [charge]` (record + wanted level),
`/clearwanted` (rank 3+), `/arrest [player] [minutes]` at the LSPD garage,
and `/mdt [player or First_Last]` for wanted level, phone, plates and the
last 10 charges. Jail time keeps counting only while you play. Leaving
the game while cuffed adds a "Fled custody" charge and jail time
(5 minutes per wanted level plus 5), served on return.
Medics: `/heal [player] [price]`. Dying sends you to All Saints ($100 bill).

**Kingmakers client.** The Kingmakers APK announces itself with
`/kmclient` and then gets a stats panel, speedometer, toasts and a phone
screen (`/phone`), driven by the protocol in `../docs/KMUI.md`. Other SA:MP
clients see the normal chat messages instead.

**Help and reports.** `/report` goes to admins, `/helpme` to helpers and
admins, with a 60-second cooldown. Both are saved, so requests sent while no
staff are online are not lost. Staff: `/reports`, `/ar [id]` to take one,
`/cr [id]` to close it (the player is told), `/staff` lists who's online.

## Admin

Make yourself the first owner: set `KM_RCON_PASSWORD`, join, log in as a
character, then type `/rcon login <password>` and `/setadmin <your id> 5`.

| Level | Commands |
|---|---|
| Helper (1-3) | `/reports`, `/ar`, `/cr` for help requests, `/h` staff chat |
| 1 Moderator | `/kick [player] [reason]`, `/mute [player] [minutes] [reason]`, `/unmute [player]`, `/a` admin chat, `/goto`, `/gethere`, `/freeze`, `/unfreeze`, `/check [player]`, `/ajail [player] [minutes, 0 = release] [reason]`, `/togooc`, and all reports |
| 2 Admin | `/ban [player] [days, 0 = permanent] [reason]`, `/unban [account]`, `/veh [model]`, `/dveh` |
| 4 Lead | `/givemoney [player] [amount] [reason]` (bank, negative takes), `/setfaction`, `/sethelper [player] [0-3]`, `/createproperty`, `/deleteproperty` |
| 5 Owner | `/setadmin [player] [level]` |

Bans cover both the account and the IP. Admins can't act on admins of the
same or higher level. Every action (and every character creation) is written
to the `admin_log` table, as are the level 1+ tools above. Chat floods and repeated messages get warnings, and
the third warning is an automatic 2-minute mute.

## Database

| Table | Contents |
|---|---|
| `accounts` | Username, bcrypt hash, admin/helper level, mute, IPs, timestamps |
| `account_sessions` | One row per login: IP, start, end, end reason |
| `login_attempts` | Every password attempt, for the per-IP limit |
| `characters` | Name (unique), gender, skin, level, cash, position, health, play time |
| `bans` | Account and/or IP, reason, who, expiry, lifted |
| `admin_log` | Every staff action |
| `money_log` | Every cash and bank change: deltas, balances after, reason, other character |
| `factions`, `faction_ranks` | Factions, rank names and salaries |
| `vehicles` | Owned vehicles: model, colours, parking spot, fuel, health, lock, price paid |
| `properties` | Houses and businesses: door, interior, price, owner, lock, till |
| `character_items` | Inventory: item id and quantity per character |
| `criminal_records` | Charges from `/su`, shown in the MDT |
| `reports` | `/report` and `/helpme` requests and who handled them |
| `anticheat_log` | Anti-cheat findings and whether the player was kicked |
| `tickets` | Traffic tickets, who wrote them and when they were paid |
| `schema_migrations` | Which migration files have been applied |

All times are UTC. To change the schema, add a new numbered file in
`sql/migrations/`. Never edit a migration that has already run.

## Tests

```bash
./scripts/test.sh
```

It builds the image, starts a throwaway database, applies the migrations,
runs `tests/km_tests.pwn`, backs up and restores the database, and then
checks that the real gamemode boots, connects and loads factions and
properties without query errors. It exits non-zero on any failure.

The test script includes every gamemode module, so it uses the real code:
validation, anti-spam, command parsing, game rules (pay, interest, levels,
fuel, plates), schema constraints (money can't go negative, unique phone
numbers, one-time property claims, report permissions), the gamemode's own
SQL (character load and save, vehicles, MDT, reports) run against the
migrated schema, `Money_Give`/`Money_Charge` and inventory changes checked in
the database and `money_log`, the faction and property loaders, cascades, and
bcrypt.

Not covered automatically: anything that needs a real client in the world
(dialogs, checkpoints, vehicles, interiors, map coordinates). That needs the
APK on a device or emulator.

## Pinned versions

open.mp 1.4.0.2779 (binary reports 1.4.0.2783), Pawn compiler 3.10.11,
MySQL plugin R41-4, samp-bcrypt 0.4.1, sscanf 2.13.8, omp-stdlib at
`17c1b01`, MariaDB 11.4.
