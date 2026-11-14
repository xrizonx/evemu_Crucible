# Upstream Issue Triage — xrizonx/evemu_Crucible

Last reviewed: 2026-11-13  
Working branch: `master`  
Source tracker: [EvEmu-Project/evemu_Crucible/issues](https://github.com/EvEmu-Project/evemu_Crucible/issues)

## Scope

The upstream tracker currently exposes **128 open issue/PR records** across two API pages (100 + 28). The API's issue listing also includes pull requests. This is an inventory of the open records, not a claim that all 128 have received a full source-level audit. The items below are the subset inspected in detail or selected for follow-up.

## Implemented on this fork

| Upstream item | Area | Fork status | Verification still needed |
|---|---|---|---|
| [#139](https://github.com/EvEmu-Project/evemu_Crucible/issues/139) | Turret fire at asteroids can crash | Added null/state/numeric guards to `TurretFormulas::GetToHit()` | In-game asteroid targeting and turret fire |
| [#170](https://github.com/EvEmu-Project/evemu_Crucible/issues/170) | Probe processing iterator invalidation | Fixed safe iteration/removal in `EntityList::Process()` | Repeated probe recovery/removal in-game |
| [#154](https://github.com/EvEmu-Project/evemu_Crucible/issues/154) | Pod-death crash | Added null checks and a client-facing error path for missing station/inventory objects; root cause remains under investigation | Reproduce clone respawn and confirm the server remains stable |
| [#188](https://github.com/EvEmu-Project/evemu_Crucible/issues/188) | Station inventory appears empty after system unload | Reloads inventory when the client marker is stale but the in-memory inventory has been unloaded | Test system unload/return and verify DB-backed hangar contents |
| [#275](https://github.com/EvEmu-Project/evemu_Crucible/issues/275) | NPC belt spawns disappear before warp-in | Implemented the two-part spawn fix from [PR #327](https://github.com/EvEmu-Project/evemu_Crucible/pull/327): security-rating scale correction and immediate warp for NPCs in empty bubbles | Spawn belts across security classes; observe NPC warp-in and combat |
| [#276](https://github.com/EvEmu-Project/evemu_Crucible/issues/276) | Dungeon editor acceleration gate creation | Corrected `AddObject` binding to accept the full nine arguments and nullable optional values | Test adding acceleration gates in the dungeon editor |
| [#319](https://github.com/EvEmu-Project/evemu_Crucible/issues/319) | MySQL reconnect option build compatibility | Replaced legacy `my_bool` usage with `bool` | Confirm all CI platforms pass |

The search-result projection mismatch, defensive DB result conversion, and DB row text bounds checks are additional fork fixes documented in [PATCH_NOTES.md](PATCH_NOTES.md).

## Priority investigation queue

| Priority | Upstream item | Finding so far | Next engineering step |
|---|---|---|---|
| High | [#154](https://github.com/EvEmu-Project/evemu_Crucible/issues/154) Pod-death crash | Historical stack trace reaches `Inventory::LoadContents(this=0x0)` from `Client::LoadStationHangar()`. Added null checks and a client-facing error path; underlying station/inventory lookup failure still needs tracing. | Reproduce clone respawn to the medical station and trace why the station or inventory lookup can fail; verify the guard prevents the crash. |
| High | [#269](https://github.com/EvEmu-Project/evemu_Crucible/issues/269) Scanner crash after NPC kills | Reproduction sequence is specific, but no stack trace is supplied. | Reproduce with wrecks and multiple NPC kills; capture stack trace and inspect scanner result entity lifetimes. |
| Medium | [#39](https://github.com/EvEmu-Project/evemu_Crucible/issues/39) Docking while target-painted causes segfault | Old report; comments mention several historical target-manager fixes, but also later reports of continued crashes. | Re-test current branch with offensive modules active during docking, warp and session changes; inspect target/module cleanup ordering. |
| Medium | [#315](https://github.com/EvEmu-Project/evemu_Crucible/issues/315) Outpost construction crashes | Repro steps exist but reporter supplied no crash log. | Reproduce after anchoring and build timer; capture server stack before modifying construction flow. |
| Medium | [#276](https://github.com/EvEmu-Project/evemu_Crucible/issues/276) Dungeon editor acceleration gate creation | Binding signature has been updated to accept the client’s nine arguments and nullable optional values. | Test the editor interaction and check whether other object types need the same protocol handling. |

## Upstream pull requests worth reviewing

- [PR #327 — belt rat spawn class and warp-in](https://github.com/EvEmu-Project/evemu_Crucible/pull/327): the relevant code changes have been adapted to this fork and are recorded above.
- [PR #332 — client-aligned warp/travel movement](https://github.com/EvEmu-Project/evemu_Crucible/pull/332): closed without merging and changes 75 files. A narrow WARP ball-state serialization correction has been adapted to this fork; the full Destiny physics rewrite remains unported pending review and gameplay tests.
- [PR #326 — broad gameplay fixes](https://github.com/EvEmu-Project/evemu_Crucible/pull/326): touches many unrelated systems. Review individual subsystems and test each change; do not merge the entire PR blindly.
- [PR #323 — persist ship position on logout](https://github.com/EvEmu-Project/evemu_Crucible/pull/323): review save timing, session changes and login warp semantics before adoption.
- [Issue #328 — ship physics claim](https://github.com/EvEmu-Project/evemu_Crucible/issues/328): reporter claims a fix but has not supplied a patch in the issue; no code has been adopted from it.

## Review rules

1. Confirm that the report still applies to this fork before changing code.
2. Prefer a reproducer, stack trace, failing test, or clearly demonstrated source defect.
3. Keep fixes narrowly scoped; avoid importing large unrelated upstream patches.
4. Build on Windows GCC, Linux GCC and Docker CI.
5. Test gameplay separately; compilation alone does not close a gameplay bug.
6. Never run destructive migrations or reset the user's live database as part of issue triage.
7. Update [PATCH_NOTES.md](PATCH_NOTES.md) after each committed fix and record its verification status here.


## 2026-11-14 progress update — Blockade encounter and drone combat

The following work is implemented on `combat-mission-drone-integration` and consolidated in [fork PR #11](https://github.com/xrizonx/evemu_Crucible/pull/11). This is a progress record for this fork; it does not change or close upstream issue records.

| Work item | Implementation/progress | Evidence and remaining verification |
|---|---|---|
| Synthetic Blockade encounter fleet | Expanded the Angel Cartel group 550 test pocket to 24 frigates. | User confirmed 24 ships spawn, move and fire in-game. |
| Kill objective | Explicit 24-kill target; initialized after generic Encounter setup; objective progress remains separate from objective type metadata. | Source path and payload corrected. Still verify the journal counter increments to 24/24 in a live client session. |
| 120-minute mission duration | Special 120-minute accepted duration for synthetic mission 65535 only. | User confirmed the journal now reflects 120 minutes. Other mission timers remain unchanged. |
| Reward persistence | 150,000 ISK reward stored through mission offer updates; old open synthetic offers with missing reward are normalized. | Source/SQL path corrected. Wallet credit still needs an end-to-end live turn-in check. |
| Completion and turn-in | Complete action is gated by required kills; existing agent completion flow pays reward and removes the encounter site/offer. | Source-level path reviewed. Verify Complete unlock, successful turn-in, wallet journal entry and site cleanup in-game. |
| Encounter kill target source | Offer target is derived from the mission pocket's required-kill value, not a generic NPC count. | Source-level correction; verify target against the spawned encounter. |
| Drone combat | Removed duplicate global damage scaling; improved lock-complete activation, approach/engage transitions, invalid target cleanup, target-loss handling and client state/target synchronization. | CI build is the current automated check; dedicated server-side drone AI unit coverage is not present. Gameplay regression testing remains required. |

### CI snapshot for the integrated code

Workflow [#43593340656](https://github.com/xrizonx/evemu_Crucible/actions/runs/43593340656), head `b527d20d17d7dff570ad0dd7b001042593f20589`:

- Docker image build: passed.
- Linux GCC build/test invocation/package: passed.
- Windows 64-bit UCRT64 GCC: still running at the time of this update; the final matrix is not yet fully green.

The run must be checked again after the documentation commit, because a new branch head triggers a new validation run. A green compile is not a substitute for the live mission turn-in and drone gameplay checks above.

### Fork PR housekeeping

- PR #11 is the consolidated integration PR and is the merge candidate after all required CI jobs pass.
- PR #8 (24-frigate encounter/reward), PR #9 (drone combat), and PR #10 (120-minute timer) overlap with the consolidated PR #11. Keep them open until #11 is successfully merged, then close them as superseded with a link to #11; do not independently merge duplicate branches.
- The fork's issue tracker API is disabled in repository metadata (`has_issues=false`), so this Markdown triage file is the maintained progress tracker for this fork. The upstream issue tracker is separate and is not being marked closed by these changes.


## 2026-11-14 follow-up — mission fails to complete after last NPC

**Reported symptom:** Player destroys all hostile NPCs in the Blockade pocket, but the mission remains incomplete / Complete is not offered.

**Code-path finding:** `SpawnMgr::SpawnKilled()` removed the final mission spawn entry and cleaned up the pocket, while offer progress was only persisted inside `RecordMissionKill(killerID)` success. A final NPC death without a resolvable player Client/killer ID could therefore leave the offer's persisted kill progress below its required count even though the mission spawn registry was empty.

**Fix committed on integration branch:**
- Determine whether the final mission spawn entry has been removed independently of per-kill credit.
- When the mission pocket has no remaining spawn entries, find the matching accepted offer by character, agent and offer ID and persist its kill progress to the required objective count.
- Keep ordinary kill progress capped at the target and send the normal mission update to the online character.
- Move the synthetic Blockade reward/24-kill objective initialization after the generic Encounter reset in `Agent::MakeOffer()`.

**Validation still required:** CI for the new head, then live acceptance and 24 NPC kills. Confirm the journal reaches 24/24, the agent's Complete action appears, turn-in marks the offer completed, exactly 150,000 ISK is credited, and the site is removed. Source inspection is not a substitute for this end-to-end test.

**PR overlap:** PR #10 contains the older timer-only implementation and conflicts/overlaps with the consolidated PR #11 on `EVE_Missions.h`, `AgentBound.cpp`, and `MissionDataMgr.cpp`. Keep #10 open only until #11 is merged; then close #10 as superseded rather than merging the divergent branch independently.
