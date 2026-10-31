#!/usr/bin/env bash
# Builds the server image and runs:
#   1. tests/km_tests.pwn against a fresh, migrated MySQL
#   2. a backup, restore and retention check with scripts/backup.sh and restore.sh
#   3. a smoke test: the real gamemode boots, connects to the database and
#      loads factions and properties without query errors
#   4. an end-to-end run of tools/kmbot, a headless SA:MP client, through
#      registration, character creation, spawn, commands and login
# Exit code 0 means everything passed.
set -euo pipefail
cd "$(dirname "$0")/.."

export DB_NAME=km_test DB_USER=km_test DB_PASSWORD=km_test_pw DB_ROOT_PASSWORD=km_test_root
COMPOSE=(docker compose -p kingmakers-test --env-file /dev/null -f docker-compose.yml -f docker-compose.test.yml)
cleanup() { "${COMPOSE[@]}" down -v --remove-orphans >/dev/null 2>&1 || true; }
trap cleanup EXIT

[ -d .deps/omp ] || ./scripts/fetch-deps.sh

# KM_SKIP_BUILD=1 reuses an image you built yourself as kingmakers-test-server.
if [ -z "${KM_SKIP_BUILD:-}" ]; then
    echo "== build"
    "${COMPOSE[@]}" build server
fi

echo "== database + migrations"
"${COMPOSE[@]}" up -d --wait db
"${COMPOSE[@]}" run --rm migrate

echo "== unit + integration tests"
LOG=$(mktemp)
KM_SCRIPT=km_tests "${COMPOSE[@]}" run --rm --no-deps -e KM_SCRIPT=km_tests server >"$LOG" 2>&1 || true
grep -E "KM-TEST|\[db\]" "$LOG"
summary=$(grep -o "KM-TESTS DONE pass=[0-9]* fail=[0-9]*" "$LOG" || true)
if [ -z "$summary" ]; then
    echo "tests did not finish; full log in $LOG"; exit 1
fi
echo "$summary"
case "$summary" in *"fail=0") ;; *) exit 1 ;; esac

echo "== backup and restore"
# Back up, delete a marker row, restore, and check the marker is back.
"${COMPOSE[@]}" run --rm --no-deps --entrypoint bash backup -c '
    set -euo pipefail
    export MYSQL_PWD="$DB_PASSWORD"
    sql() { mariadb --skip-ssl -h db -u "$DB_USER" -N -B "$DB_NAME" -e "$1"; }
    sql "INSERT INTO accounts (username, password_hash, register_ip) VALUES (\"kmtest_backup\", REPEAT(\"x\", 60), \"127.0.0.1\")"
    bash /scripts/backup.sh once
    file=$(ls -1 /backups/km-*.sql.gz | head -n 1)
    sql "DELETE FROM accounts WHERE username = \"kmtest_backup\""
    [ "$(sql "SELECT COUNT(*) FROM accounts WHERE username = \"kmtest_backup\"")" = 0 ]
    if bash /scripts/restore.sh "$file" >/dev/null 2>&1; then echo "restore ran without --yes"; exit 1; fi
    bash /scripts/restore.sh "$file" --yes
    [ "$(sql "SELECT COUNT(*) FROM accounts WHERE username = \"kmtest_backup\"")" = 1 ]
    [ "$(sql "SELECT COUNT(*) FROM schema_migrations")" -ge 5 ]
    sql "DELETE FROM accounts WHERE username = \"kmtest_backup\""
    # retention: keep only BACKUP_KEEP files
    for i in 1 2 3; do sleep 1; BACKUP_KEEP=2 bash /scripts/backup.sh once >/dev/null; done
    [ "$(ls -1 /backups/km-*.sql.gz | wc -l)" = 2 ]
    echo "backup, restore and retention OK"
'

echo "== smoke test: gamemode boots"
KM_SCRIPT=kingmakers "${COMPOSE[@]}" up -d --no-deps server >/dev/null 2>&1
for i in $(seq 1 60); do
    logs=$("${COMPOSE[@]}" logs server 2>&1 || true)
    if echo "$logs" | grep -qE "connection failed|shutting down|query error"; then break; fi
    if echo "$logs" | grep -q "\[db\] connected" && echo "$logs" | grep -q "Kingmakers RP 0" \
        && echo "$logs" | grep -q "\[faction\] factions loaded" && echo "$logs" | grep -q "\[property\] loaded"; then
        echo "gamemode started, connected to the database and loaded factions and properties"
        booted=1; break
    fi
    sleep 2
done
if [ -z "${booted:-}" ]; then
    "${COMPOSE[@]}" logs server | tail -30
    echo "smoke test failed"; exit 1
fi

echo "== end-to-end: kmbot plays through the game as a SA:MP client"
# tools/kmbot is a headless client built from the mobile client's RakNet.
# It registers, creates a character, spawns, runs commands, checks the UI
# protocol, reconnects and logs in.
mkdir -p .deps
if [ ! -x .deps/kmbot ] || [ -n "$(find tools/kmbot ../app/src/main/cpp/samp/vendor/raknet -newer .deps/kmbot -print -quit)" ]; then
    if command -v g++ >/dev/null; then
        ./tools/kmbot/build.sh "$PWD/.deps/kmbot" >/dev/null
    else
        docker run --rm -v "$(cd .. && pwd)":/src -w /src mirror.gcr.io/library/gcc:13 \
            server/tools/kmbot/build.sh /src/server/.deps/kmbot >/dev/null
    fi
fi
server_ip=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$("${COMPOSE[@]}" ps -q server)")
if ! "${COMPOSE[@]}" run --rm --no-deps -v "$PWD/.deps/kmbot:/kmbot:ro" --entrypoint /kmbot server \
        "$server_ip" 7777 kmbot_e2e Bot_Tester; then
    "${COMPOSE[@]}" logs server | tail -30
    echo "end-to-end test failed"; exit 1
fi
