# The Blockade security-agent spawn test

This is an isolated runtime test path on `mission-data-restoration`.

- Every security agent can offer the test encounter, independent of the agent's empire faction and agent level (levels 1-5).
- The synthetic mission ID is `65535`; it is not an imported SDE mission ID.
- The offer has no briefing ID, ISK reward, reward item, or bonus. The UI should be treated as test-only.
- On acceptance, the encounter creates a small, fixed Angel Cartel test fleet directly from inventory-backed NPC types.
- The test fleet contains **12 frigate-class ships only** (group ID `550`). Cruisers and battleships are deliberately excluded to reduce incoming damage and improve survivability when a player first warps into the site.
- NPCs are distributed around the pocket centre rather than stacked at one position.
- This test does not implement the Blockade's eight reported steps, reinforcement triggers, loot, or canonical reward values. It tests agent offer selection, mission acceptance, and initial Angel Cartel frigate spawning only.

No SQL is executed by this code path. The research staging record remains separate and is not promoted into `agtMissions`.
