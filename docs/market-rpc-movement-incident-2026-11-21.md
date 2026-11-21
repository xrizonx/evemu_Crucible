# Crucible market blank / five-minute remote call / unresponsive ship (21 Nov 2026)

## User reports
- Market window opens, but no market data is displayed
- A generic first error (exact text, method and traceback unavailable)
- Client says a remote call has been outstanding for five minutes
- Ship does not respond to navigation or throttle commands

These symptoms are not sufficient to claim one confirmed production root cause:
SQL latency, market seed coverage, the RPC dispatch loop and login-warp state
were independently inspected. All changes below require a new server build,
the relevant DB migration and live reproduction.

## Defects corrected on the audit branch

1. **Main-loop starvation:** `EntityList::Process` calls `Client::ProcessNet`
   before ticking each system. Previously, `ProcessNet` drained one
   client's entire receive queue, potentially starving every other client
   and ship-movement tick. It now processes at most 64 packets per client
   per loop and sends queued Destiny notifications before returning.
   Dispatches taking over 1s are logged.
2. **Synchronous SQL stall:** `DBcore` previously used a fixed 4096-byte
   result-query buffer while passing `vsnprintf`'s untruncated length to
   `mysql_real_query`. The result-query path now uses a dynamically
   sized SQL buffer. Database socket connect/read/write waits are bounded
   (10/30/30 seconds), successful result queries over 1s log a brief
   diagnostic prefix, and reconnect retries are limited to one attempt
   on genuinely lost connections. `Reconnect` now checks for
   `Connected` explicitly; an `Error` enum is not success.
   **Caveat:** these socket timeouts do not guarantee a total wall-clock
   deadline for every DB query.
3. **Cached market failures:** `GetOrders`, `GetNewPriceHistory` and
   `GetOldPriceHistory` no longer cache `None` after a transient SQL
   failure. A later refresh can try the database again. Missing
   market-group cache data can likewise be retried.
4. **Market result safety:** The market-group filter index key is no
   longer shared as an owned packed-row field. Null market-group text is
   safe. Per-type order queries no longer narrow 32-bit type IDs to
   `uint16`.
5. **Slow best-ask queries:** New composite indexes are provided for
   region, station and solar-system market lookups. The existing
   single-column `solarSystem` migration remains intact.
6. **Login-warp lock:** A login-return warp is not a player-initiated
   capacitor-consuming action. Rejecting it because of low capacitor
   left `LoginWarp` set and could block later navigation commands.
   The automatic return warp now skips the player cap check; invalid
   return vectors clear the login-warp flag.

## Apply and validate

1. Back up the database, then apply the normal SQL migration:
   `sql/migrations/20261121070000-market-order-rpc-indexes.sql`.
   The new indexes preserve orders but can take time and resources to
   build on a populated server. Schedule for low traffic.
2. Build the new audit-branch server and redeploy/restart it (a CI green
   result alone does not replace the running server executable).
3. Run **read-only** diagnostics from
   `sql/diagnostics/2026-11-21-market-rpc-movement-readonly.sql` against
   the active game database. Confirm the selected region has live
   `mktOrders`, `invMarketGroups` is populated and all three composite
   indexes exist.
4. Reproduce opening the market, selecting an item, refreshing orders,
   asking for price history, undocking, setting throttle, orbiting and
   warping after logging directly into space. Check a fresh login with
   a ship that has low capacitor.
5. Inspect server log times around the incident for
   `Slow result query`, `Slow packet dispatch`,
   `MarketMgr`, `MARKET__DB_ERROR`, `DBCore query failed`,
   `Client::ProcessNet`, `LoginWarp` and `Destiny`.
   Collect the client's exact first error and corresponding server
   timestamps. A five-minute *client* wait does not identify which
   remote service failed by itself.

## Verification boundaries

Cross-platform source-level guard: `tools/verify_market_movement_rpc.py`.
The MariaDB CI job runs the migration against a seeded order table and
executes a market query explain. Native Linux/Windows, sanitizer, Docker,
and other existing jobs remain required. No live Crucible gameplay,
end-to-end packet captures or production SQL execution plans were
available to verify this particular user's incident.

**Rollout:** audit branch only; do not merge to `master` without
separate instruction.
