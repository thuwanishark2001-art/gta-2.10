#!/usr/bin/env bash
# Restores a backup made by backup.sh over the current database.
#   restore.sh <file.sql.gz> --yes
# Stop the game server first (docker compose stop server) so nothing writes
# while the restore runs. Env: DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD
set -euo pipefail

: "${DB_HOST:=db}" "${DB_PORT:=3306}" "${DB_NAME:?}" "${DB_USER:?}" "${DB_PASSWORD:?}"
file="${1:-}"
if [ -z "$file" ] || [ ! -f "$file" ]; then
    echo "usage: restore.sh <backup.sql.gz> --yes" >&2; exit 2
fi
if [ "${2:-}" != "--yes" ]; then
    echo "This replaces every table in $DB_NAME with the contents of $file."
    echo "Run again with --yes to do it."; exit 2
fi
gunzip -t "$file"
export MYSQL_PWD="$DB_PASSWORD"
gunzip -c "$file" | mariadb --skip-ssl -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" "$DB_NAME"
echo "restored $DB_NAME from $(basename "$file")"
