# Graceful shutdown reference-ownership guard (2026-11-22)

**Target:** EVEmu Crucible, Windows and Debian/Linux/Docker.  
**Branch:** `fix/login-system-boot-crash-2026-11-21` (PR #52).  
**Status:** Source changes and a new regression test committed; **native build and live shutdown are not verified**. Automatic GitHub Actions triggers remain disabled.

## User-reported failure

On `docker compose stop server`, the server logged repeat `[RefPtrError] DecRef() on deleted object` and `FATAL` stack traces, with garbage counts (e.g. 3327845616) for the same pointer. A freed object no longer has a valid reference count. Those diagnostics attempted to read the destroyed object's `mDeleted` and `mRefCount` fields. Simply suppressing the log text would still access invalid memory and would not guarantee clean process exit.

## Implementation

- `src/eve-core/memory/RefPtr.h`: `RefObject` registers each live object address in a process-lifetime registry; its destructor unregisters the object. `RefPtr` uses **static** `RetainRef` / `ReleaseRef` entrypoints so the shutdown path can check pointer liveness **without invoking a member function on a freed object**.
- After leaving the main game loop, `eve-server.cpp` enables the shutdown guard. During final cleanup, a mutex protects the live set, which is checked before any object fields are read. A final-release operation removes an address from the registry before running the object's destructor, so nested destructors cannot release it again through `RefPtr`.
- Invalid or zero-count releases during shutdown are **skipped**, and a single aggregate suppression count is logged at the end. The same invalid operations during normal gameplay are still reported by the existing diagnostics.
- Normal gameplay uses the original atomic reference counter path (the registry adds constructor/destructor bookkeeping, but not a mutex on every live gameplay retain/release).
- `ItemFactory::Close()` now empties **all three item caches** (`m_staticItems`, `m_dynamicItems`, `m_items`) before destroying `ItemType` descriptors. The earlier fix only emptied the primary cache.
- `src/eve-test/utils/RefPtrLifetimeTest.cpp` adds an offline regression that deliberately creates a stale intrusive reference after deleting its object, then exercises copy and repeated releases during shutdown.
- The optional local Docker test build (`EVEMU_BUILD_TESTS=ON`) now runs the existing `MarketOrderReaderTest` **and** `RefPtrLifetimeTest`, without using GitHub CI.

**Important limitations:** This guards only `RefPtr` retain/release operations during final shutdown. It does not guarantee safety of arbitrary raw-pointer dereferences, resolve reference ownership errors occurring while the game is running, or prevent the ABA case where a freed address is reused for a new object. A skipped reference indicates there is still an ownership defect to investigate. The registry also introduces one small tracking entry per live `RefObject`. Do not describe the underlying use-after-free as cured without runtime testing.

## Build locally on Debian (no GitHub Actions minutes)

From the local checkout:

```bash
git fetch origin
git switch fix/login-system-boot-crash-2026-11-21
git pull --ff-only origin fix/login-system-boot-crash-2026-11-21

docker compose stop server
# Compile and run local C++ reference-lifetime tests during image build:
docker compose build --build-arg EVEMU_BUILD_TESTS=ON server

docker compose up -d server
docker compose logs --tail=60 server
```

Once the server has finished booting, stop it gracefully and inspect its exit code and final logs:

```bash
docker compose stop -t 60 server
docker inspect server --format '{{.State.ExitCode}}'
docker compose logs --tail=150 server
```

An expected clean shutdown returns exit code **0** and reaches `EVEmu is Offline.`. With the shutdown guard, any dangling `RefPtr` release detected in the final phase is counted in one `Suppressed N stale/underflow reference operations during shutdown.` warning instead of repeating `FATAL` traces.

If the server still crashes or exits non-zero, collect the **entire shutdown log from `Closing connected clients` onward** and the symbolized stack or a local AddressSanitizer build. A stable exit code alone does not prove there are no outstanding ownership defects.

## Windows native build

The same `RefPtr` and shutdown logic applies to Windows x64. Use a locally compiled executable from this branch (not an old release ZIP). Run `RefPtrLifetimeTest` with your normal Windows CMake test build. Windows binaries were not built or tested by this patch.

## Re-enable GitHub CI later

`.github/workflows/build-release.yml` currently retains only `workflow_dispatch`. This patch does not re-enable automatic push, PR or tag builds.
