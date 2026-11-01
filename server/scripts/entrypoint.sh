#!/bin/sh
# Writes config.json and mysql.ini from environment variables,
# then starts open.mp. See server/.env.example for the variables.
set -eu

: "${KM_SERVER_NAME:=Kingmakers RP}"
: "${KM_PORT:=7777}"
: "${KM_MAX_PLAYERS:=100}"
: "${KM_RCON_PASSWORD:=}"
: "${KM_ANNOUNCE:=false}"
: "${KM_SCRIPT:=kingmakers}"
: "${KM_WEBSITE:=}"
: "${KM_OWNER_ACCOUNT:=}"
: "${DB_HOST:=db}" "${DB_PORT:=3306}" "${DB_NAME:?}" "${DB_USER:?}" "${DB_PASSWORD:?}"

json_escape() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }

if [ -n "$KM_RCON_PASSWORD" ]; then RCON_ENABLE=true; else RCON_ENABLE=false; fi

cat > config.json <<JSON
{
    "name": "$(json_escape "$KM_SERVER_NAME")",
    "website": "$(json_escape "$KM_WEBSITE")",
    "max_players": $KM_MAX_PLAYERS,
    "announce": $KM_ANNOUNCE,
    "enable_query": true,
    "language": "English",
    "exclude": ["Fixes"],
    "network": { "port": $KM_PORT, "allow_037_clients": true },
    "rcon": { "enable": $RCON_ENABLE, "password": "$(json_escape "$KM_RCON_PASSWORD")", "allow_teleport": false },
    "pawn": {
        "main_scripts": ["$(json_escape "$KM_SCRIPT")"],
        "side_scripts": [],
        "legacy_plugins": ["mysql", "samp_bcrypt"]
    },
    "game": {
        "use_entry_exit_markers": false,
        "use_stunt_bonuses": false,
        "use_player_ped_anims": true,
        "use_manual_engine_and_lights": true,
        "use_chat_radius": false,
        "use_nametag_los": true,
        "nametag_draw_radius": 20.0,
        "death_drop_amount": 0
    },
    "logging": { "enable": true, "file": "logs/server.log", "log_chat": true, "use_timestamp": true }
}
JSON

# The MySQL plugin is statically linked against an old glibc and can't use
# Docker's DNS, so hand it an IP address.
DB_ADDR=$(getent hosts "$DB_HOST" | awk '{ print $1; exit }')
[ -n "$DB_ADDR" ] || DB_ADDR="$DB_HOST"

# The MySQL plugin reads mysql.ini from the server's root directory.
mkdir -p scriptfiles
umask 077
cat > mysql.ini <<INI
hostname = $DB_ADDR
server_port = $DB_PORT
username = $DB_USER
password = $DB_PASSWORD
database = $DB_NAME
auto_reconnect = true
INI

# The owner account (KM_OWNER_ACCOUNT), read by the gamemode at start.
mkdir -p scriptfiles
printf '%s\n' "$KM_OWNER_ACCOUNT" > scriptfiles/owner.cfg

# Start each run with a fresh server.log; keep the last 10.
mkdir -p logs
if [ -s logs/server.log ]; then
    mv logs/server.log "logs/server-$(date -u +%Y%m%d-%H%M%S).log"
fi
ls -1t logs/server-*.log 2>/dev/null | tail -n +11 | xargs -r rm -f

# Volumes are created root-owned; fix that, then drop privileges.
if [ "$(id -u)" = 0 ]; then
    chown -R omp:omp logs scriptfiles config.json mysql.ini
    exec setpriv --reuid=omp --regid=omp --init-groups ./omp-server "$@"
fi
exec ./omp-server "$@"
