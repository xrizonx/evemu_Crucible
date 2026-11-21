-- Crucible belt NPC security/class/quantity audit. READ-ONLY.
-- Run against the active EvE database. Never UPDATE static NPC group mappings
-- without verifying the imported Crucible-era dataset.
--
-- The in-server policy now uses client-displayed security (trueSec rounded to
-- one decimal): 0.8-1.0 frigates, 0.5-0.7 max cruiser, 0.4 max cruiser,
-- 0.1-0.3 max battlecruiser, <=0.0 max battleship.
SELECT ROUND(security, 1) AS displayedSecurity,
       COUNT(*) AS systemCount,
       MIN(security) AS minTrueSec,
       MAX(security) AS maxTrueSec
FROM mapSolarSystems
WHERE security IS NOT NULL
GROUP BY ROUND(security, 1)
ORDER BY displayedSecurity DESC;

-- Verify npcClassGroup's class IDs match the actual faction group names.
-- Ordinary classes: 1/2 frigate; 3 destroyer; 4/5 cruiser;
-- 6 battlecruiser; 7 battleship. Classes 8-14 are special variants.
SELECT ncg.factionID, ncg.shipClass, ncg.groupID,
       COALESCE(ig.groupName, '<MISSING GROUP>') AS actualGroupName
FROM npcClassGroup AS ncg
LEFT JOIN invGroups AS ig ON ig.groupID = ncg.groupID
ORDER BY ncg.factionID, ncg.shipClass;

-- Identify oversized imported templates; the server limits a normal belt
-- patrol to 2-5 actual NPCs rather than blindly spawning this full sum.
SELECT type AS encounterClass, sub AS templateVariant,
       (f + af + d + c + ac + bc + bs + h + o + cf + cd + cc + cbc + cbs)
           AS declaredShipCount,
       f, af, d, c, ac, bc, bs, h, o, cf, cd, cc, cbc, cbs
FROM npcSpawnClass
ORDER BY declaredShipCount DESC, type, sub LIMIT 70;

-- Count faction mappings with no corresponding real invTypes. Such entries
-- must be repaired from verified static data, not randomized ship types.
SELECT ncg.factionID, ncg.shipClass, ncg.groupID,
       COUNT(it.typeID) AS matchingNPCTypeCount
FROM npcClassGroup AS ncg
LEFT JOIN invTypes AS it ON it.groupID = ncg.groupID
GROUP BY ncg.factionID, ncg.shipClass, ncg.groupID
HAVING matchingNPCTypeCount = 0
ORDER BY ncg.factionID, ncg.shipClass;

-- Confirm which pirate faction a belt will use in each region.
SELECT mr.regionID, mr.regionName, mr.ratFactionID
FROM mapRegions AS mr
WHERE mr.ratFactionID IS NOT NULL AND mr.ratFactionID <> 0
ORDER BY mr.regionID;
