# Running Kingmakers RP: staff handbook

Staff commands by level, and the owner's common tasks. Each level also has
every command of the levels below it. Type `/help` in game for the list
your rank can use.

## Levels

| Level | Name | Given with |
|---|---|---|
| Helper (1-3) | answers `/helpme` requests | `/sethelper [player] [level]` (Lead) |
| 1 | Moderator | `/setadmin [player] 1` |
| 2 | Admin | `/setadmin [player] 2` |
| 4 | Lead | `/setadmin [player] 4` |
| 5 | Owner | `/setadmin [player] 5`, or the database (below) |

You can only give levels below your own, unless you're logged into RCON
(`/rcon login [password]`, the `KM_RCON_PASSWORD` in `server/.env`).

## Commands

**Helpers and all staff**
- `/reports`: open reports and help requests.
- `/ar [id]`: accept a request.
- `/cr [id]`: close a request.
- `/h [text]`: helper chat.

**Moderator (1)**
- `/a [text]`: admin chat.
- `/kick [player] [reason]`.
- `/mute [player] [minutes] [reason]`, `/unmute [player]`.
- `/freeze [player]`, `/unfreeze [player]`.
- `/goto [player]`, `/gethere [player]`.
- `/check [player]`: account, level, money, admin rank.
- `/ajail [player] [minutes] [reason]`: admin jail. 0 minutes releases them.
- `/togooc`: turn global OOC chat on or off.
- `/spec [player]`, `/specoff`: watch a player. `/specoff` puts you back where you were.

**Admin (2)**
- `/ban [player] [days or 0 = permanent] [reason]`, `/unban [account]`.
- `/veh [model 400-611]`, `/dveh`: spawn and delete admin vehicles.
- `/announce [text]`: big on-screen message for everyone.
- `/sethealth [player] [0-100]`.
- `/setskin [player] [skin]`: only skins every phone has (0-299, not 1-6,
  8, 42, 65, 74, 86, 119, 149, 208, 273, 289).
- `/playstream [player/all] [http(s) url]`, `/stopstream [player/all]`:
  play music or an announcement through players' speakers.

**Lead (4)**
- `/givemoney [player] [amount] [reason]`: paid into the bank. A negative
  amount takes money.
- `/setfaction [player] [faction id, 0 = none] [rank]`.
- `/sethelper [player] [level 0-3]`.
- `/createproperty [interior] [price] [name]`, `/deleteproperty`. Stand at
  the door. `/createproperty` alone lists the interiors. 5 and 6 are 24/7
  shops, which appear on everyone's map and sell phones.
- `/settime [0-23]`, `/setweather [0-20]` (1 sunny, 8 rain, 9 fog,
  19 sandstorm).

**Owner (5)**
- `/setadmin [player] [level 0-5]`.
- `/renameaccount [old] [new]`: the account must be offline. Characters,
  money and rank stay.
- `/testblips`, `/testblips off`: test icons around you, to check how the
  radar draws.

Everything above is recorded in the `admin_log` table.

## Common owner tasks

All of these run on the server over SSH (`ssh root@<server IP>`).

**Update the server** to the latest code (backs up the database first):

```
cd /opt/kingmakers && git pull && cd server && ./scripts/deploy.sh
```

**Make yourself owner:** put your account name in `server/.env` and redeploy.
The name is the one you type in the app, not your character's name.

```
KM_OWNER_ACCOUNT=YourName
```

That account becomes owner (level 5) the next time it logs in.

**Make any account owner** from the database instead:

```
cd /opt/kingmakers/server && docker compose exec db sh -c 'mariadb -uroot -p"$MARIADB_ROOT_PASSWORD" "$MARIADB_DATABASE" -e "UPDATE accounts SET admin_level=5 WHERE username='"'"'NAME'"'"'"'
```

Replace `NAME`, then log out and back in.

**Read the server log:**

```
cd /opt/kingmakers/server && docker compose logs --since 1h server | tail -50
```

In the log, `has left the server (id:reason)` gives the reason as a number:
- `0`: timed out
- `1`: quit
- `2`: kicked or banned

**Backups** are written to `server/data/backups` every 24 hours and before
every deploy. `KM_BACKUP_KEEP` in `.env` sets how many are kept. Restoring
is described in [DEPLOY.md](DEPLOY.md).

**Set up the first shops:** players buy phones at 24/7 shops, and the
server starts with none. At a few shop doors, type:

```
/createproperty 5 5000 24-7 Idlewood
```

## Player commands worth knowing

- **Help:** `/guide` shows the getting-started guide, which new characters
  see on their first spawn. `/help` lists commands.
- **Finding places:** `/gps` (also the client's GPS button) marks a place on
  the map.
- **Jobs:** `/jobs`, `/takejob`, `/work` and `/stopwork` for trucker, pizza,
  taxi, miner, mechanic and bus driver.
- **Radio:** `/radio` (driver) picks an internet radio station for everyone
  in the car.
