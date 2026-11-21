# Belt NPC spawn balance — Crucible / 21 November 2026

**Scope:** ordinary asteroid-belt patrols on
`audit/pending-client-tests-db-cleaner-safety-2026-11-20`.
No mission, anomaly, incursion or explicit DED encounter fleet changes.
No destructive SQL changes.

## Target security progression

The game displays solar-system security to one decimal; the server now uses
that displayed band when enforcing the maximum NPC class.

| System security | Maximum ordinary NPC ship class | Maximum NPC ships per belt patrol |
| --- | --- | --- |
| 1.0–0.8 | Frigate, including advanced frigates | 2 |
| 0.7–0.5 | Cruiser (frigate/destroyer/cruiser possible) | 3 |
| 0.4 | Cruiser (destroyers allowed) | 3 |
| 0.3–0.1 | Battlecruiser | 4 |
| 0.0 and negative trueSec | Battleship | 5 |

For historical compatibility, very high-security belt systems where
`SystemBubble` already suppresses roaming rats still have no belt rats.
"Frigates only" defines the upper hull limit if rats do spawn; it does not
force hostile NPCs to appear in 1.0 systems.

These bands implement the reported 1.0–0.8 and 0.7–0.5 requirements.
The remaining limits follow historical CCP deployment evidence: destroyers
from 0.4, battlecruisers from 0.3, and battleship encounters in nullsec.
Historical lowsec battleship exceptions are not enabled in this first
deterministic policy so that high-tier NPCs cannot invade safer belts.

## Repaired root causes

1. NPC template selection previously did not filter the **individual ship
   class fields** in `npcSpawnClass` (`f`, `af`, `d`, `c`, `ac`,
   `bc`, `bs`, and special classes); thus inappropriate hulls could be
   selected even from an Easy/Fair template. The server now filters every
   candidate against the local security policy and validates that the
   `npcClassGroup`'s actual `invGroups.groupName` matches the expected
   hull. Invalid mappings are skipped with a warning.
2. Imported spawn templates could specify an arbitrarily large number
   of NPCs. Belt patrols now have a strict fleet cap (2/3/4/5) and stronger
   eligible hull classes are prioritized before support ships. Missing
   candidates are skipped, with at most a fallback frigate.
3. A prototype 10% officer chance, additional 8% hauler roll and
   excessive rogue-drone swarm multipliers created frequent, oversized
   special groups. Belt officers are now extremely rare and confined to
   deep nullsec, commanders limited to 0.3 or below, and haulers to
   0.0 or below. The template's 4x/10x swarm expansion no longer applies.
4. A periodic `SystemBubble::ProcessWander` call unconditionally
   re-armed an active belt whenever a player remained present. It now
   only reconciles bubble membership; genuine last-rat kills trigger
   `SpawnKilled` to re-arm the next patrol.
5. The engine previously reported a successful belt group even when no
   NPC entity could load. The spawn routine now reports actual success,
   and refuses duplicate belt groups when NPCs or registry entries exist.
6. `StartRatTimer()` narrowed 300,000 milliseconds into `uint16`,
   shortening configured spawn delays; it now retains 32-bit time.
   `WarpOutSpawn` no longer advances an iterator after erasing it.
7. Ordinary belt rats use the region's configured pirate faction instead
   of an unrequested 5% random rogue-drone replacement. The user-specified
   `RatFaction` configuration override remains supported.

## Tests and deployment

`BeltRatSpawnTest` (CTest) exercises the security boundaries,
allowed/forbidden hulls, incorrect group labels and rare-encounter rules.
`tools/verify_subsystems_audit.py` adds cross-platform guards for bounded
patrol counts and prevention of repeated spawn timers.

1. Back up the game database and existing server executable.
2. Compile and deploy the audit branch after its full Linux/Windows,
   MariaDB, Docker, ASan/UBSan and CTest CI jobs pass.
3. Restart the server to remove previously overspawned in-memory NPCs.
   These code changes do not forcibly delete live NPCs or existing
   game items when applied.
4. Run the read-only
   `sql/diagnostics/2026-11-21-belt-npc-spawn-audit-readonly.sql`
   to identify bad imported NPC groups and oversized SQL templates.
   Do not rewrite base spawn tables on guesswork.
5. Visit several belts each at 0.8, 0.7, 0.5, 0.4, 0.3, 0.1,
   0.0 and negative trueSec with a disposable character. Verify the actual
   ship hull, NPC count, faction, and respawn delay after clearing a
   patrol. Remain in one belt beyond a periodic bubble scan to confirm
   the old group is not duplicated.
6. Capture `SPAWN__WARNING`, `SPAWN__MESSAGE` and `SPAWN__POP`
   if a belt stays empty; wrong/missing imported NPC group data will be
   logged but cannot be reconstructed from code alone.

**Verification limits:** CI policy tests cannot validate live Crucible
client-visible NPC hulls, all region/faction datasets, or exact historical
rarity of commander/officer encounters. Production confirmation needs
actual gameplay logs and SQL diagnostics. Mission encounter design is
intentionally outside this patch.
