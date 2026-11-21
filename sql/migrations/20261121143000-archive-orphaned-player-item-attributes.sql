-- AttributeMap loads these IDs through ItemDB's entity fallback. This
-- range deliberately excludes static objects, offices and asteroids.
-- Run during server maintenance. Base entity tables may still be MyISAM.
-- Keep the archive for recovery and leave existing item overrides untouched.
-- +migrate Up
CREATE TABLE IF NOT EXISTS cleanupOrphanedPlayerItemAttributes LIKE entity_attributes;

INSERT IGNORE INTO cleanupOrphanedPlayerItemAttributes (itemID, attributeID, valueInt, valueFloat)
SELECT a.itemID, a.attributeID, a.valueInt, a.valueFloat
FROM entity_attributes AS a
LEFT JOIN entity AS e ON e.itemID = a.itemID
LEFT JOIN staStations AS s ON s.stationID = a.itemID
LEFT JOIN mapDenormalize AS m ON m.itemID = a.itemID
WHERE a.itemID > 140000000 AND a.itemID < 300000000
  AND e.itemID IS NULL AND s.stationID IS NULL AND m.itemID IS NULL;

DELETE a
FROM entity_attributes AS a
JOIN cleanupOrphanedPlayerItemAttributes AS saved
  ON saved.itemID = a.itemID AND saved.attributeID = a.attributeID
  AND saved.valueInt <=> a.valueInt AND saved.valueFloat <=> a.valueFloat
LEFT JOIN entity AS e ON e.itemID = a.itemID
LEFT JOIN staStations AS s ON s.stationID = a.itemID
LEFT JOIN mapDenormalize AS m ON m.itemID = a.itemID
WHERE a.itemID > 140000000 AND a.itemID < 300000000
  AND e.itemID IS NULL AND s.stationID IS NULL AND m.itemID IS NULL;

-- +migrate Down
-- Leave the archive available. If an ID has since been reused, restoration
-- of its old attributes would put someone else's values on the current item.
INSERT IGNORE INTO entity_attributes (itemID, attributeID, valueInt, valueFloat)
SELECT saved.itemID, saved.attributeID, saved.valueInt, saved.valueFloat
FROM cleanupOrphanedPlayerItemAttributes AS saved
LEFT JOIN entity AS e ON e.itemID = saved.itemID
LEFT JOIN staStations AS s ON s.stationID = saved.itemID
LEFT JOIN mapDenormalize AS m ON m.itemID = saved.itemID
WHERE saved.itemID > 140000000 AND saved.itemID < 300000000
  AND e.itemID IS NULL AND s.stationID IS NULL AND m.itemID IS NULL;
