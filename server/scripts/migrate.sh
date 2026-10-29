#!/usr/bin/env bash
# Applies server/sql/migrations/*.sql in order, once each.
# Applied versions are recorded in schema_migrations.
# Env: DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD, MIGRATIONS_DIR
set -euo pipefail

: "${DB_HOST:=db}" "${DB_PORT:=3306}" "${DB_NAME:?}" "${DB_USER:?}" "${DB_PASSWORD:?}"
: "${MIGRATIONS_DIR:=/migrations}"
export MYSQL_PWD="$DB_PASSWORD"
# DB_SSL=1 for a remote database; the local compose network doesn't need it.
if command -v mariadb >/dev/null; then MYSQL_BIN=mariadb; else MYSQL_BIN=mysql; fi
SSL_OPT=()
if [ "${DB_SSL:-0}" != 1 ] && [ "$MYSQL_BIN" = mariadb ]; then SSL_OPT=(--skip-ssl); fi
sql() { "$MYSQL_BIN" "${SSL_OPT[@]}" -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" --protocol=TCP -N -B "$DB_NAME" "$@"; }

for i in $(seq 1 30); do sql -e "SELECT 1" >/dev/null 2>&1 && break; echo "waiting for database ($i)"; sleep 2; done

sql -e "CREATE TABLE IF NOT EXISTS schema_migrations (
            version    VARCHAR(64) NOT NULL PRIMARY KEY,
            applied_at DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP)"

for f in $(ls "$MIGRATIONS_DIR"/*.sql | sort); do
    v=$(basename "$f" .sql)
    if [ -n "$(sql -e "SELECT version FROM schema_migrations WHERE version='$v'")" ]; then
        continue
    fi
    echo "applying $v"
    # Each file runs in one client session; schema_migrations is only updated if it succeeds.
    sql < "$f"
    sql -e "INSERT INTO schema_migrations (version) VALUES ('$v')"
done
echo "migrations up to date"
