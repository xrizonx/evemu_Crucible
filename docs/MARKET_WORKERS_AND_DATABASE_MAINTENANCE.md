# Market reads and database maintenance

An uncached item order book uses one database statement for both sides. Workers use separate connections and share identical in-flight requests within a region. The game thread owns Python packet creation, cache publication, wallet and inventory changes, and order settlement. An order update invalidates any in-flight snapshot for that region and item. Requests during a region change, saturation or database failure can return no data and can be retried.

| XML setting `market.OrderReadWorkers` | Behavior |
| --- | --- |
| `-1` | Auto-detect logical CPUs and use up to 16 workers |
| `0` | Use synchronous market order reads |
| `1`–`128` | Set the worker count explicitly |

Each active worker can use one additional MariaDB connection. Leave room for the world connection, optional API services and database maintenance. Increasing the worker count can make contention worse on small database hosts. Compare cold and warm order-book requests, concurrent browsing, and movement tick latency before increasing it. History and best-ask requests still run synchronously.

## Database upgrade

Apply the migrations through EVEDBTool during a maintenance window after taking a backup. The history and owner indexes can take time to build on large tables. The cleanup migration archives only attributes with a missing parent in the player-item ID range. Station and static-object IDs are protected. The archive stays available for recovery, including after rollback. Existing overrides are preserved when restoring and reused item IDs are skipped.

Expired orders can still hold escrow or reserved inventory. Historical market data, static client data and seed definitions require their own retention and compatibility checks.

## Docker

The default image installs runtime libraries and the MariaDB client. Build with `--build-arg WITH_GDB=1` if you use `RUN_GDB=TRUE`. The C++ Docker build defaults to two compiler jobs. Use `--build-arg BUILD_JOBS=N` when you have more build memory.

Compose waits for the database health check. Direct container startup also checks the configured host and port, with `DB_WAIT_SECONDS` defaulting to 60. A failed migration stops startup. Existing `eve-server.xml` files retain their database settings.
