#!/usr/bin/env bash
# Deploys or updates the server on this machine (see ../../docs/DEPLOY.md).
#   deploy.sh            build and (re)start everything with the current code
#   deploy.sh --pull     git pull --ff-only first
# Takes a database backup before restarting, so an update can be rolled back.
set -euo pipefail
cd "$(dirname "$0")/.."

if [ ! -f .env ]; then
    echo "No .env. Copy .env.example to .env and set real passwords first." >&2; exit 1
fi
# Refuse the example passwords.
if grep -qE '^(DB_PASSWORD|DB_ROOT_PASSWORD)=change-me' .env; then
    echo "Change DB_PASSWORD and DB_ROOT_PASSWORD in .env before deploying." >&2; exit 1
fi

if [ "${1:-}" = "--pull" ]; then
    git pull --ff-only
fi

# The content service (docs/CONTENT.md) needs the server's public address.
content_port=$(sed -n 's/^KM_CONTENT_PORT=//p' .env | tail -n 1)
content_port=${content_port:-7780}
if ! grep -qE '^KM_CONTENT_URL=.+' .env; then
    ip=$(hostname -I 2>/dev/null | awk '{ print $1 }')
    if [ -n "$ip" ]; then
        sed -i '/^KM_CONTENT_URL=/d' .env
        echo "KM_CONTENT_URL=http://$ip:$content_port" >> .env
        echo "== set KM_CONTENT_URL=http://$ip:$content_port in .env (change it if players use another address)"
    fi
fi
# Vultr and similar images enable ufw with only SSH open.
if [ "$(id -u)" = 0 ] && command -v ufw >/dev/null && ufw status | grep -q "Status: active"; then
    ufw allow "$content_port"/tcp >/dev/null
fi

# Start-screen art: the defaults, unless you've put your own in content/.
mkdir -p content/sounds content/models
[ -e content/logo.png ] || cp content-defaults/logo.png content/
[ -e content/background.jpg ] || [ -e content/background.png ] || cp content-defaults/background.jpg content/

./scripts/fetch-deps.sh
docker compose build server

# Back up the current database if one is running.
if docker compose ps --status running db 2>/dev/null | grep -q db; then
    echo "== backup before update"
    docker compose run --rm --no-deps backup once
fi

started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
docker compose up -d
# Always restart the game server, so it runs the code just built and the
# check below only sees this run's log.
docker compose up -d --force-recreate --no-deps server
# The content service lists its files at start; restart it to pick up changes.
docker compose restart content >/dev/null
echo "== waiting for the gamemode"
for i in $(seq 1 60); do
    logs=$(docker compose logs --since "$started" server 2>&1 || true)
    if echo "$logs" | grep -q "\[property\] loaded"; then
        echo "server is up:"
        docker compose ps
        exit 0
    fi
    if echo "$logs" | grep -qE "connection failed|shutting down|query error"; then break; fi
    sleep 2
done
docker compose logs --tail 40 server
echo "the server didn't come up; see the log above" >&2
exit 1
