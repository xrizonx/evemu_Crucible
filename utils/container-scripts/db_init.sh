#!/bin/bash
set -euo pipefail

MARIADB_HOST="${MARIADB_HOST:-db}"
MARIADB_DATABASE="${MARIADB_DATABASE:-evemu}"
MARIADB_PASSWORD="${MARIADB_PASSWORD:-evemu}"
MARIADB_USER="${MARIADB_USER:-evemu}"
MARIADB_PORT="${MARIADB_PORT:-3306}"
DB_WAIT_SECONDS="${DB_WAIT_SECONDS:-60}"

[[ "$MARIADB_PORT" =~ ^[1-9][0-9]{0,4}$ && "$DB_WAIT_SECONDS" =~ ^[1-9][0-9]{0,3}$ ]] \
    || { echo "Invalid MariaDB port or wait timeout." >&2; exit 1; }

echo "Waiting for the EVEmu database..."
deadline=$((SECONDS + DB_WAIT_SECONDS))
until MYSQL_PWD="$MARIADB_PASSWORD" mariadb --protocol=tcp --connect-timeout=2 \
    --host="$MARIADB_HOST" --port="$MARIADB_PORT" --user="$MARIADB_USER" \
    --database="$MARIADB_DATABASE" --execute='SELECT 1' >/dev/null 2>&1; do
    if ((SECONDS >= deadline)); then
        echo "Database unavailable at $MARIADB_HOST:$MARIADB_PORT." >&2
        exit 1
    fi
    sleep 1
done

yaml_quote() { printf "'%s'" "${1//\'/\'\'}"; }
xml_replacement() {
    printf '%s' "$1" | sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g; s/[\\&|]/\\&/g'
}
umask 077
cd /src/sql
config=$(mktemp)
trap 'rm -f "$config"' EXIT
cat > "$config" <<EOF
base-dir: /src/sql/base
db-database: $(yaml_quote "$MARIADB_DATABASE")
db-host: $(yaml_quote "$MARIADB_HOST")
db-pass: $(yaml_quote "$MARIADB_PASSWORD")
db-port: $MARIADB_PORT
db-user: $(yaml_quote "$MARIADB_USER")
log-level: Info
migrations-dir: /src/sql/migrations
dungeon-dir: /src/sql/dungeons
EOF

if [[ "${SEED_MARKET:-FALSE}" == "TRUE" ]]; then
    saturation="${SEED_SATURATION:-80}"
    [[ "$saturation" =~ ^([0-9]|[1-9][0-9]|100)$ ]] || { echo "Invalid SEED_SATURATION." >&2; exit 1; }
    {
        printf 'seed-saturation: %s\nseed-regions:\n' "$saturation"
        IFS=',' read -r -a regions <<< "${SEED_REGIONS:-Derelik,The Citadel,The Forge,Essence}"
        for region in "${regions[@]}"; do
            printf -- '- %s\n' "$(yaml_quote "$region")"
        done
    } >> "$config"
fi

# EVEDBTool reads evedb.yaml from its working directory.
mv -f "$config" evedb.yaml
trap 'rm -f /src/sql/evedb.yaml' EXIT
./evedbtool install
if [[ "${SEED_MARKET:-FALSE}" == "TRUE" ]]; then
    ./evedbtool seed
fi
./evedbtool dungeon apply

if [[ ! -f /app/etc/eve-server.xml ]]; then
    config=$(mktemp /app/etc/eve-server.xml.XXXXXX)
    trap 'rm -f "$config" /src/sql/evedb.yaml' EXIT
    sed -e "s|database_host|$(xml_replacement "$MARIADB_HOST")|g" \
        -e "s|database_username|$(xml_replacement "$MARIADB_USER")|g" \
        -e "s|database_password|$(xml_replacement "$MARIADB_PASSWORD")|g" \
        -e "s|database_name|$(xml_replacement "$MARIADB_DATABASE")|g" \
        -e "s|database_port|$MARIADB_PORT|g" \
        /src/utils/config/eve-server.xml > "$config"
    mv "$config" /app/etc/eve-server.xml
fi
