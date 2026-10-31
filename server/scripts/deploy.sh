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
