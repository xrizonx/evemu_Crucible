# The Blockade security-agent spawn test

This is an isolated runtime test path on `mission-data-restoration`.

- Every security agent can offer the test encounter, independent of the agent's empire faction and agent level (levels 1-5).
- The synthetic mission ID is `65535`; it is not an imported SDE mission ID.
- The offer has no briefing ID, ISK reward, reward item, or bonus. The UI should be treated as test-only.
- On acceptance, the encounter spawner selects a mission dungeon whose first room contains an Entity group whose name begins with `Asteroid Angel Cartel`.
- The selected template's first-room objects are spawned using the room-object loader. Non-NPC objects are retained as celestials.
- If no matching Angel Cartel template exists, or the template fails to spawn NPCs, acceptance fails instead of silently spawning another faction's rats.
- This test does not implement the Blockade's eight reported steps, reinforcement triggers, loot, or canonical reward values. It tests agent offer selection, mission acceptance, Angel Cartel template selection, and initial-room spawning only.

No SQL is executed by this code path. The research staging record remains separate and is not promoted into `agtMissions`.
