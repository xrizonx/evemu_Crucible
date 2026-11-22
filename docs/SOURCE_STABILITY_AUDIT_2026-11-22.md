# EVEmu Crucible — targeted crash and stability source audit
**Date:** 2026-11-22  
**Repository:** xrizonx/evemu_Crucible  
**Branch:** `fix/login-system-boot-crash-2026-11-21`  
**Proposed integration:** [PR #52](https://github.com/xrizonx/evemu_Crucible/pull/52)  
**Status:** Source patches and regression cases committed. **Compilation, CI, and live-server stability are unverified.**

## Scope and limits

The 2026-11-22 review prioritized C++ paths associated with the observed Windows and Linux `IncRef()` / `DecRef()` use-after-free diagnostics and character login, station/corporation RPCs, notification serialization, system entities, and client teardown. It also sampled source for ships/modules, fleet, core networking, inventory, markets and persistence interfaces.

Build manifests list approximately **249 eve-server C++ source files, 28 eve-core files, and 50 eve-common entries** (some generated protocol sources among the common entries). This review **did not open and analyze every source/header/generated packet/SQL file**. An exhaustive audit is not claimed. Source review alone is not a substitute for compilation, ASan/UBSan, Windows crash dumps, or login/runtime verification.

## Confirmed defects corrected in this pass

| Area | Previous defect | Correction |
| --- | --- | --- |
| `python/PyRep.cpp` | `PyObject::Clone()` shallow-copied owning `PyRep*` fields, leading to duplicate releases | Deep-clone type and argument objects; null-safe destructor; self-assignment guards in tuple/list/dict/object-ex |
| `python/PyPacket.cpp` | Notification decode released multiple *borrowed* nested tuples and streams as well as the outer owning packet, including on success | A single `OwnedPyRep` owns the outer packet; nested references remain borrowed; only retained `args` outlives decode |
| `services/Callable.h`, `Service.h`, `BoundService.h` | Member-function pointers were cast between incompatible signatures; handler and bound OID allocations were not reclaimed | Type-safe callback binding; virtual base destructors; handler/OID cleanup |
| `system/SystemManager.cpp` | `RemoveEntity()` left deleted entities in `m_entities` | Erase the main entity lookup entry and guard anomaly manager |
| `system/SystemManager.cpp` | Duplicate client registration incremented counters; unsigned player count could wrap on repeated removal | Reject duplicates, verify exact pointer, prevent underflow |
| `EntityList.cpp` | Corporation-member map stored the first member but failed to append others; a duplicate session could remove the real player's map entry | Correct membership insert, reject duplicate mapping, and clean up exact client pointers in all corp entries |
| `Client.cpp` | Partially initialized logins could leave client pointers registered in entity/system lists; station guest-list dereference could be null | Guard partial teardown and station exit; reset item-factory owner pointer; null checks on loaded logout |
| `corporation/CorpRegistryBound.cpp`, `CorpRegistryService.cpp` | Cached sparse office header lists leaked; delayed release could invalidate a replacement cache entry | Create headers only when needed; release callbacks check expected pointer |
| `network/StreamPacketizer.cpp` | An unaligned 32-bit length was read through a typed buffer iterator; disconnect cleanup left incomplete frames | Read frame lengths via byte-safe `memcpy`; clear partial input on buffer reset |
| `ship/modules/ModuleManager.cpp` | Module activation dereferenced absent ship/destiny managers during docked/transition states | Validate the ship entity and destination before activation |
| `fleet/FleetService.cpp` | Grid/system queries dereferenced absent ship entities/bubbles and truncated system IDs from uint32 to uint16 | Guard nulls and use 32-bit system IDs |
| `Client.cpp` | Exceptions during service dispatch could leave `m_canThrow` enabled for later events | Scope exit restores exception flag |
| `python/PyPacket.cpp` | Default call cloning, packet type access and outgoing payload could dereference null or produce null wire elements | Guard absent argument tuple/type and explicitly encode None |
| `memory/RefPtr.h` | Previous atomic refcount migration passed `std::atomic` objects directly to C varargs diagnostic formatting | Explicit integer loads in reference-count diagnostics |

## Existing hotfixes preserved

Prior PR work addresses `RefPtr` self-assignment and release-before-retain, `PyResult` aliasing, nested `MachoBindObject` tuple/result references, `invbroker::GetInventoryFromId` optional passive decoding and error handling, character login stage guards and station transition safety. No database tables, characters, ships, items, or client wire-format layout were intentionally changed in this pass.

## Regression cases added, but NOT executed

Extended the existing `src/eve-test/marshal/EVEMarshalTest.cpp` executable with:
- a `PyObject` clone that survives destruction of its source;
- tuple/list/dict/`PyObjectEx` self-assignment;
- notification decode with malformed nested payloads and valid retained arguments;
- notification clone metadata and independent argument lifetime;
- two adjacent framed packets with an unaligned second 32-bit frame length and clearing an incomplete frame;
- empty `PyCallStream` clone and null outbound packet payload encoding.

Use the repository's existing multi-platform CMake/GitHub Actions build and CTest matrix before merging. No CI result, test execution, runtime reproduction or debug-symbolized stack is available for this audit.

## Remaining high-risk concerns (not fixed / not verified)

1. **Intrusive lifetime races:** Atomic `RefObject` counts do not make already-dangling raw pointers safe. `EVEBoundObject::Release()` still calls `delete this`; service manager and callers must not reenter a released bound object.
2. **Factory global context:** `sItemFactory.SetUsingClient()` is a global mutable client pointer. Audit concurrency/worker use before claiming cross-thread safety; scoped setters alone do not make global state thread-local.
3. **Other ship and fleet null paths:** Several `GetShipSE()->...` and `GetMyInventory()->...` calls remain outside the guarded methods, especially in fleet broadcasts, module loading, ship state encoding and station unloading.
4. **Python dictionary equality:** `PyDict` equality comparator currently appears to compare only object hashes, rather than key equality. Collisions could cause incorrect lookups; changing semantics requires protocol regression tests.
5. **Network resource limits:** `StreamPacketizer` handles incomplete frame lengths but may retain data for an excessive claimed packet length. Establish a consistent framing limit before accepting untrusted input.
6. **SQL/data integrity:** Character-starting-station, corporate office and inventory relationships may still contain bad records. No full SQL audit or migration has been performed.
7. **No symbolized crash stack:** A native debug-symbolized stack, ASan/UBSan run, and new Windows/Linux binary are necessary to attribute any remaining use-after-free.

## Acceptance criteria

Build C++ on Windows x86-64 and Linux (including Docker where supported), run existing and added CTest regressions, reproduce account login for a character docked in a station and newly created character, then exercise inventory, corp-office binding, notification processing and reconnect/logout without fatal reference-count diagnostics or client exceptions. Until these are observed, treat the patch as **best-effort, unverified**.
