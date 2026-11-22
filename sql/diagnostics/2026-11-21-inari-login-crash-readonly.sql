-- EVEmu login-time crash after AnomalyMgr startup in Inari (30002793).
-- READ ONLY: run against the same database configured for the running server.
-- Does not delete, update or move any item. Copy only results needed for triage.
--
-- 1. Overall dynamic inventory and NPC-group types in the affected system.
SELECT e.itemID, e.itemName, e.ownerID, e.locationID, e.typeID,
       t.groupID, g.categoryID, g.groupName, e.customInfo
FROM entity AS e
LEFT JOIN invTypes AS t ON t.typeID = e.typeID
LEFT JOIN invGroups AS g ON g.groupID = t.groupID
WHERE e.locationID = 30002793
ORDER BY e.itemID
LIMIT 300;

-- 2. Orphaned items whose type or group no longer exists.
SELECT e.itemID, e.typeID, e.ownerID, e.itemName
FROM entity AS e
LEFT JOIN invTypes AS t ON t.typeID = e.typeID
LEFT JOIN invGroups AS g ON g.groupID = t.groupID
WHERE e.locationID = 30002793
  AND (t.typeID IS NULL OR g.groupID IS NULL)
ORDER BY e.itemID;

-- 3. Dynamic entities that the system loader may attempt to restore.
SELECT g.categoryID, g.groupName, COUNT(*) AS recordCount
FROM entity AS e
JOIN invTypes AS t ON t.typeID = e.typeID
JOIN invGroups AS g ON g.groupID = t.groupID
WHERE e.locationID = 30002793
GROUP BY g.categoryID, g.groupID, g.groupName
ORDER BY recordCount DESC, g.categoryID;

-- 4. Historically spawned NPCs which should ordinarily be transient.
-- Do not delete these records by hand without backing up and confirming
-- the server's item ownership and spawn cleanup behavior.
SELECT e.itemID, e.typeID, e.ownerID, e.customInfo
FROM entity AS e
WHERE e.locationID = 30002793
  AND LOWER(COALESCE(e.customInfo, '')) LIKE '%beltrat%'
ORDER BY e.itemID;
