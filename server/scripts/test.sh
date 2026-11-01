#!/usr/bin/env bash
# Builds the server image and runs:
#   1. tests/km_tests.pwn against a fresh, migrated MySQL
#   2. a backup, restore and retention check with scripts/backup.sh and restore.sh
#   3. a smoke test: the real gamemode boots, connects to the database and
#      loads factions and properties without query errors
#   4. end-to-end runs of tools/kmbot, a headless SA:MP client: registration,
#      character creation, spawn, commands and login; then ATM banking and a
#      full miner job loop; RCON, property, police and reports as an admin;
#      driving: the licence test, the dealership and a trucker delivery;
#      pizza and taxi shifts; and two players at once (officer arrests
#      suspect; mechanic repairs a customer's car); each checked against the
#      database
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
# KMBOT_RCON matches KM_RCON_PASSWORD in docker-compose.test.yml.
run_bot() { "${COMPOSE[@]}" run --rm --no-deps -e KMBOT_RCON=kmbot-rcon-test -v "$PWD/.deps/kmbot:/kmbot:ro" --entrypoint /kmbot server "$server_ip" 7777 "$@"; }
if ! run_bot kmbot_e2e Bot_Tester basic || ! run_bot kmbot_eco Eco_Tester economy || ! run_bot kmbot_adm Admin_Bot admin \
        || ! run_bot kmbot_drv Driver_Bot driving; then
    "${COMPOSE[@]}" logs server | tail -30
    echo "end-to-end test failed"; exit 1
fi
# The economy run must have left exactly this money trail.
trail=$("${COMPOSE[@]}" exec -T -e MYSQL_PWD="$DB_PASSWORD" db mariadb -u "$DB_USER" -N -B "$DB_NAME" -e \
    "SELECT GROUP_CONCAT(CONCAT(m.reason, ':', m.cash_after, '/', m.bank_after) ORDER BY m.id) FROM money_log m
     JOIN characters c ON c.id = m.character_id WHERE c.name = 'Eco_Tester'")
if [ "$trail" != "deposit:300/200,withdraw:350/150,job:440/150" ]; then
    echo "unexpected money_log for the economy run: $trail"; exit 1
fi
echo "money_log matches the economy run"
trail=$("${COMPOSE[@]}" exec -T -e MYSQL_PWD="$DB_PASSWORD" db mariadb -u "$DB_USER" -N -B "$DB_NAME" -e \
    "SELECT GROUP_CONCAT(CONCAT(m.reason, ':', m.cash_after, '/', m.bank_after) ORDER BY m.id) FROM money_log m
     JOIN characters c ON c.id = m.character_id WHERE c.name = 'Admin_Bot'")
if [ "$trail" != "admin:500/2000,property_buy:500/1000,property_sell:500/1600" ]; then
    echo "unexpected money_log for the admin run: $trail"; exit 1
fi
actions=$("${COMPOSE[@]}" exec -T -e MYSQL_PWD="$DB_PASSWORD" db mariadb -u "$DB_USER" -N -B "$DB_NAME" -e \
    "SELECT GROUP_CONCAT(DISTINCT l.action ORDER BY l.action) FROM admin_log l
     JOIN accounts a ON a.id = l.admin_account_id WHERE a.username = 'kmbot_adm'")
if [ "$actions" != "accept_report,announce,check,close_report,create_property,delete_property,give_money,set_admin,set_faction,set_health,set_skin,set_time,set_weather" ]; then
    echo "unexpected admin_log for the admin run: $actions"; exit 1
fi
echo "money_log and admin_log match the admin run"
# Delivery pay depends on the random drop, so check the sequence, the
# licence and the owned vehicle rather than amounts.
driving=$("${COMPOSE[@]}" exec -T -e MYSQL_PWD="$DB_PASSWORD" db mariadb -u "$DB_USER" -N -B "$DB_NAME" -e \
    "SELECT CONCAT((SELECT GROUP_CONCAT(m.reason ORDER BY m.id) FROM money_log m WHERE m.character_id = c.id),
                   ' licence=', c.driving_licence,
                   ' vehicles=', (SELECT COUNT(*) FROM vehicles v WHERE v.owner_character_id = c.id AND v.model = 462))
     FROM characters c WHERE c.name = 'Driver_Bot'")
if [ "$driving" != "admin,driving_test,vehicle_buy,job licence=1 vehicles=1" ]; then
    echo "unexpected database state after the driving run: $driving"; exit 1
fi
echo "database matches the driving run"

echo "== end-to-end: two players, officer and suspect"
run_bot kmbot_sus Suspect_Bot suspect & suspect_pid=$!
sleep 2
officer_ok=1; run_bot kmbot_cop Officer_Bot officer Suspect_Bot || officer_ok=0
suspect_ok=1; wait "$suspect_pid" || suspect_ok=0
if [ "$officer_ok" != 1 ] || [ "$suspect_ok" != 1 ]; then
    "${COMPOSE[@]}" logs server | tail -30
    echo "two-player test failed"; exit 1
fi
police=$("${COMPOSE[@]}" exec -T -e MYSQL_PWD="$DB_PASSWORD" db mariadb -u "$DB_USER" -N -B "$DB_NAME" -e \
    "SELECT CONCAT('records=', (SELECT COUNT(*) FROM criminal_records r JOIN characters o ON o.id = r.officer_character_id
                                WHERE r.character_id = s.id AND o.name = 'Officer_Bot' AND r.charge = 'Testing the police flow'),
                   ' tickets=', (SELECT COUNT(*) FROM tickets t WHERE t.character_id = s.id AND t.amount = 250 AND t.paid_at IS NULL),
                   ' paid=', (SELECT COUNT(*) FROM money_log m WHERE m.character_id = s.id AND m.reason = 'pay_in' AND m.cash_delta = 100),
                   ' jail=', s.jail_seconds, ' wanted=', s.wanted)
     FROM characters s WHERE s.name = 'Suspect_Bot'")
if [ "$police" != "records=1 tickets=1 paid=1 jail=0 wanted=0" ]; then
    echo "unexpected database state after the two-player run: $police"; exit 1
fi
echo "database matches the two-player run"

echo "== end-to-end: pizza and taxi; mechanic and customer"
if ! run_bot kmbot_job Jobs_Bot jobs; then
    "${COMPOSE[@]}" logs server | tail -30
    echo "jobs run failed"; exit 1
fi
run_bot kmbot_cust Customer_Bot customer & customer_pid=$!
sleep 2
mechanic_ok=1; run_bot kmbot_mech Mechanic_Bot mechanic Customer_Bot || mechanic_ok=0
customer_ok=1; wait "$customer_pid" || customer_ok=0
if [ "$mechanic_ok" != 1 ] || [ "$customer_ok" != 1 ]; then
    "${COMPOSE[@]}" logs server | tail -30
    echo "mechanic run failed"; exit 1
fi
services=$("${COMPOSE[@]}" exec -T -e MYSQL_PWD="$DB_PASSWORD" db mariadb -u "$DB_USER" -N -B "$DB_NAME" -e \
    "SELECT CONCAT('jobs=', (SELECT GROUP_CONCAT(m.details ORDER BY m.id) FROM money_log m JOIN characters c ON c.id = m.character_id WHERE c.name = 'Jobs_Bot'),
                   ' paid=', (SELECT GROUP_CONCAT(CONCAT(m.reason, ':', m.cash_delta)) FROM money_log m JOIN characters c ON c.id = m.character_id WHERE c.name = 'Customer_Bot'),
                   ' earned=', (SELECT GROUP_CONCAT(CONCAT(m.reason, ':', m.cash_delta)) FROM money_log m JOIN characters c ON c.id = m.character_id WHERE c.name = 'Mechanic_Bot'))")
if [ "$services" != "jobs=pizza delivery,taxi fare paid=service_paid:-75 earned=service_earned:75" ]; then
    echo "unexpected database state after the jobs and mechanic runs: $services"; exit 1
fi
echo "database matches the jobs and mechanic runs"
