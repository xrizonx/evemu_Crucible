# Memory-safety and CPU responsiveness audit — 20 November 2026

**Scope:** EVEmu Crucible branch `audit/pending-client-tests-db-cleaner-safety-2026-11-20`.
This record is a **targeted source review and instrumented-test plan**, not a certification
that all server code is leak-free or scales to any given player count. No game-client load
test or instrumented long-running multiplayer workload has been completed for this audit.

## Findings addressed on this branch

| Area | Prior defect | Change / intended invariant |
| --- | --- | --- |
| Active NPC spawn-bubble enumeration | `SystemManager::GetSpawnBubbles()` looped over a nonempty map without incrementing its iterator: a permanent hang / full-core spin | Iterate the map exactly once with a range-based loop; preserve all returned key/pointer pairs |
| Unloading solar systems | `BubbleManager::ClearSystemBubbles()` removed references from the global list and maps but did not free the `new SystemBubble` allocations | Destroy retired bubbles after `SystemManager::UnloadSystem()` has already destroyed their entities; clear non-owning marker pointers to prevent double destruction |
| Unload cost | Calling `std::list::remove` for every bubble scanned the entire owning list repeatedly | Use a single owning-list pass; processing all bubbles is linear in total bubble count |
| Manager cleanup | `BubbleManager::clear()` freed objects without clearing the list and its lookup indexes | Clear all indexes and saved spawn IDs, making repeat cleanup avoid previously freed pointers |
| Aborted pre-login connections | `Client::~Client()` returned if `m_loaded` was false, leaking its allocated session and two destiny queues, and retaining partial bound-service references | Release partial-session allocations and bound resources without executing full authenticated logout |
| Inbound network packets | `Client::ProcessNet()` discarded its freshly decoded `PyPacket*` after dispatch without deleting the wrapper | Use `std::unique_ptr<PyPacket>` to reclaim incoming packets on success, ordinary failure and exceptions; keep dispatch handler behavior unchanged |
| Regression checks | Relevant cleanup/performance contracts were not asserted | Extend `tools/verify_subsystems_audit.py` with targeted source invariants |

These changes do **not** change spawn rules, ship movement, market data, SQL schemas,
session protocol, login policy, or NPC mission definitions. Functional preservation
still requires runtime regression testing before a release.

## Automated verification

The existing GitHub Actions build tests Linux, Windows/UCRT64 and Docker packaging,
plus NPC buy-seed MariaDB integration and the source-invariant checks. A new Linux
`ASan/UBSan memory and UB smoke` job builds instrumented C++ and runs registered
CTest tests under:

- `-fsanitize=address,undefined`
- `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`
- `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`

A passing sanitizer job would only cover the code paths exercised by CTest, which
may be much smaller than a real online-world session. A CI green build is not a
heap-leak, race-freedom or scalability guarantee. ThreadSanitizer and real workload
profiling are separate tasks.

## Unresolved / requires more evidence

1. `BubbleManager::RemoveEmpty()` unlinks periodic empty bubbles without deleting
   their allocations. Before reclaiming those objects at runtime, audit the
   non-owning references in `SpawnMgr`, anomaly and dungeon managers, and the
   associated mission/respawn timers. Removing them prematurely could introduce
   use-after-free bugs or suppress existing respawns. Unload-time reclamation
   is handled separately.
2. `EntityList::Process()` processes clients and loaded systems on its main
   loop, so a single expensive callback can delay all systems. Do not add
   parallel processing until shared-world state, per-client queues and database
   interactions are made thread-safe and tested.
3. `SystemBubble::SyncPos()` generates one update per entity per pilot. This
   scales with both pilot and entity counts; optimize only after profiling
   packet sizes and observing stable ordering/ownership semantics.
4. Lower-level `PyRep` reference-counting, outbound `PyPacket::Encode()`
   ownership and partial decode failures need dedicated instrumented tests.
   Changing the global ownership convention without that evidence risks double
   frees and use-after-free. Long-run retention from item caches, failed system
   boots, image/network workers and DB connections has **not** been exhaustively
   reviewed. No absolute absence-of-leaks claim.
5. No measured p95/p99 server tick latency or client interaction response
   latency is available across differently populated solar systems.

## Required before calling the server leak-free or scale-tested

Use isolated worlds with test accounts and no production data. Capture at least
three repeated, controlled workloads for baseline and candidate versions:

| Scenario | Representative shape | Required observations |
| --- | --- | --- |
| Small | 1 active system, 1–5 clients, ordinary belts and market | Warm/cold RSS, CPU per tick, login/logout and docking response |
| Medium | 10–25 active systems, dozens of clients, NPC respawns | 1 Hz tick delays, memory delta after repeated system unloads, DB latency |
| Large | 50–100+ active systems, dense NPC/player bubbles | p50/p95/p99 tick and RPC latency, peak/steady-state RSS, database waits and CPU saturation |
| Churn | Repeated incomplete login, disconnect, warp, system unload/reload cycles | ASan/LSan/Valgrind diagnostics, stable post-GC/idle RSS and no stale-pointer crashes |

Do not use only an increasing player count as a proxy for density: one very busy
bubble and 100 sparsely populated systems stress different paths. Profile CPU
with `perf` or Callgrind on Linux and attribute samples to `EntityList::Process`,
`SystemManager::ProcessTic`, bubble scans, `DestinyManager`, NPC AI, packet
serialization and SQL. Record core count, clock, network conditions, catalog
size and workload seed. Compare the same workloads before and after, including
gameplay invariants (targeting, missions, dynamic spawns, chat, market, docking,
storage and character persistence).

See `doc/valgrind` and the existing `Profiler` for local instrumentation
options. A stable benchmark under ASan also does not prove a production build
has identical performance; measure release binaries separately.
