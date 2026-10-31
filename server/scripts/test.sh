#!/usr/bin/env bash
# Builds the server image and runs:
#   1. tests/km_tests.pwn against a fresh, migrated MySQL
#   2. a smoke test: the real gamemode boots, connects to the database and
#      loads factions and properties without query errors
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

echo "== smoke test: gamemode boots"
KM_SCRIPT=kingmakers "${COMPOSE[@]}" up -d --no-deps server >/dev/null 2>&1
for i in $(seq 1 60); do
    logs=$("${COMPOSE[@]}" logs server 2>&1 || true)
    if echo "$logs" | grep -qE "connection failed|shutting down|query error"; then break; fi
    if echo "$logs" | grep -q "\[db\] connected" && echo "$logs" | grep -q "Kingmakers RP 0" \
        && echo "$logs" | grep -q "\[faction\] factions loaded" && echo "$logs" | grep -q "\[property\] loaded"; then
        echo "gamemode started, connected to the database and loaded factions and properties"; exit 0
    fi
    sleep 2
done
"${COMPOSE[@]}" logs server | tail -30
echo "smoke test failed"; exit 1
