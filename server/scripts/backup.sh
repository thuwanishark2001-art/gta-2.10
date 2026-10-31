#!/usr/bin/env bash
# Database backups: gzipped mariadb-dump files in $BACKUP_DIR, newest
# $BACKUP_KEEP kept.
#   backup.sh once   one backup, then exit
#   backup.sh loop   one backup every $BACKUP_INTERVAL_HOURS (the compose service)
# Env: DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD, BACKUP_DIR,
#      BACKUP_KEEP, BACKUP_INTERVAL_HOURS
set -euo pipefail

: "${DB_HOST:=db}" "${DB_PORT:=3306}" "${DB_NAME:?}" "${DB_USER:?}" "${DB_PASSWORD:?}"
: "${BACKUP_DIR:=/backups}" "${BACKUP_KEEP:=14}" "${BACKUP_INTERVAL_HOURS:=24}"
export MYSQL_PWD="$DB_PASSWORD"

backup_once() {
    mkdir -p "$BACKUP_DIR"
    local name="km-$(date -u +%Y%m%d-%H%M%S).sql.gz"
    local tmp="$BACKUP_DIR/.$name.tmp"
    # --single-transaction gives a consistent snapshot without locking the game.
    mariadb-dump --skip-ssl -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" \
        --single-transaction --quick --no-tablespaces --routines --triggers "$DB_NAME" | gzip -9 > "$tmp"
    # A dump that didn't finish has no completion marker; never keep it.
    if ! gunzip -c "$tmp" | tail -n 1 | grep -q "Dump completed"; then
        rm -f "$tmp"
        echo "backup FAILED: incomplete dump" >&2
        return 1
    fi
    mv "$tmp" "$BACKUP_DIR/$name"
    echo "backup written: $name ($(du -h "$BACKUP_DIR/$name" | cut -f1))"
    ls -1t "$BACKUP_DIR"/km-*.sql.gz | tail -n +"$((BACKUP_KEEP + 1))" | xargs -r rm -f
}

case "${1:-once}" in
    once) backup_once ;;
    loop)
        while true; do
            backup_once || echo "will retry next cycle" >&2
            sleep "$((BACKUP_INTERVAL_HOURS * 3600))"
        done ;;
    *) echo "usage: backup.sh once|loop" >&2; exit 2 ;;
esac
