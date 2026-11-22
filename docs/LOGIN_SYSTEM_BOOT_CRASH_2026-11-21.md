# Login crash immediately after AnomalyMgr initialization — 21 November 2026

## Reported evidence

After a successful queue check, login and account authentication, the server
starts loading Inari, solar system **30002793**, and stops logging immediately
after:

```text
[CMgr Init] BeltMgr Initialized for Inari(30002793)
[CMgr Init] SpawnMgr Initialized for Inari(30002793)
[CMgr Init] DungeonMgr Initialized for Inari(30002793)
[CMgr Init] AnomalyMgr Initialized for Inari(30002793) with 3 Max Signals ...
```

The message is **not an exception or error**. No exit code, C++ stack
trace, Windows faulting module, or signal number was supplied. The next
`SystemManager::BootSystem` stages are `LoadSystemDynamics`,
`LoadPlayerDynamics`, operational POS initialization, LSC channel
creation, and map loading. A successful CI compile alone does not
reproduce this system-specific crash.

## Platform-neutral safety changes

- The dynamic entity factory now rejects and destroys an NPC when
  `NPC::Load()` fails, rather than registering a partially initialized
  NPC into `SystemManager` and ticking it later.
- Persisted wrecks, cargo containers, and spawn containers validate
  `GetMyInventory()` before loading contents. A missing inventory
  cannot be dereferenced; the item ID is logged and the invalid world
  entity is skipped.
- Operational POS/sovereignty initialization skips null entities and
  missing POS interfaces instead of dereferencing them.
- The in-space login path validates its capsule and ShipSE / DestinyMgr
  before applying login cloak; `SetDestiny` checks system, ship and
  resulting ship-entity references. A failure disconnects only the
  affected client with a meaningful message, instead of crashing all
  clients.
- Portable `SystemBoot` messages identify the exact boot stage.
  Enable `ITEM__TRACE` for the exact item IDs in the dynamic loader
  if the crash remains. No forced NPC or player item deletions.

This resolves **specific unsafe source paths** and makes subsequent
failures diagnosable; the original crash's instruction address cannot
be determined from the last console line alone.

## Windows 11 / UCRT64 deployment and diagnosis

1. Preserve the previous server executable and take a backup of the
   database. Do not run a new binary over an active server.
2. Use a **successful** Windows UCRT64 GCC build from PR #52 and deploy
   the matching executable and runtime DLLs as a set; keep the current
   configuration and SQL data unchanged.
3. Restart and reproduce the character selection into Inari. Record
   the `SystemBoot` log line printed immediately before the exit.
4. If it still terminates, open **Event Viewer > Windows Logs >
   Application** and copy the error entry's *Faulting application*,
   *Faulting module*, *Exception code* and *Fault offset*. Include
   the complete log beginning at `SystemBoot`.
5. For a symbolized stack trace, launch the matching debug/symbol
   build in GDB (if installed) and collect
   `thread apply all bt full` after the fault. The trace needs to
   match the executable commit SHA. Do not send login credentials.
6. Run the **read-only**
   `sql/diagnostics/2026-11-21-inari-login-crash-readonly.sql`
   against the active database to identify missing item types, bad
   entity groups, and stale transient belt-rat rows. Do not delete
   rows unless a specific defect and backup have been confirmed.

## Linux / Docker diagnosis

Deploy the corresponding Linux binary from the same commit. If needed,
run `ulimit -c unlimited` before starting the executable, or under GDB
with `gdb --args ./eve-server` and `thread apply all bt full` at the
fault. Docker needs access to the logs and a core-dump-capable runtime
to preserve the same evidence. Record the image's build commit SHA.

## CI coverage and limits

The existing matrix builds Windows UCRT64 GCC, Linux GCC, Docker and
runs MariaDB integration plus Linux ASan/UBSan. The shared C++ changes
and cross-platform source guards must pass before deployment. The
matrix does **not** have the production Inari SQL inventory or client
connection; a green result is not equivalent to verified live login.
