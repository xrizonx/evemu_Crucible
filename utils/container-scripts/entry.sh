#!/bin/sh
set -eu

# Only the data/config directories need writes. Leave staged assets
# and existing cache files alone during container startup.
install -d /app/etc /app/logs /app/server_cache /app/image_cache
chown evemu:evemu /app/etc /app/logs /app/server_cache /app/image_cache /src/sql
exec setpriv --reuid=evemu --regid=evemu --init-groups -- "$@"
