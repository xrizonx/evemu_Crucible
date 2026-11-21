#!/bin/bash
set -euo pipefail

echo "Initializing EVEmu..."
/bin/bash /src/utils/container-scripts/db_init.sh

for file in log.ini MarketBot.xml devtools.raw; do
    if [[ ! -f "/app/etc/$file" ]]; then
        install -m 0644 "/src/utils/config/$file" "/app/etc/$file"
    fi
done

cd /app/bin
if [[ "${RUN_WITH_GDB:-FALSE}" == "TRUE" ]]; then
    command -v gdb >/dev/null || { echo "Build the image with WITH_GDB=1 to use GDB." >&2; exit 1; }
    exec gdb -ex run ./eve-server
fi
exec ./eve-server
