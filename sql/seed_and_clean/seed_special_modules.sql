-- Supplemental module market seed.
-- Adds published modules plus unpublished Officer, Deadspace (including Pith),
-- Faction and Storyline modules at one selected station.
--
-- Run after the normal market seed. Existing station/type sell orders are
-- preserved; rerunning this script will not create duplicate orders.
--
-- Change these values to the station you want to stock.
USE evemu;
SET @stationid = 60014137; -- Ibaria III - Thukker Mix Warehouse
SET @solarSystemID = 30000053;
SET @regionid = 10000001;

-- Regular published modules receive 550 units.
-- Rare Officer/Deadspace/Faction/Storyline modules receive 10 units.
INSERT INTO mktOrders
    (typeID, ownerID, regionID, stationID, price, volEntered, volRemaining,
     issued, minVolume, duration, solarSystemID, jumps)
SELECT
    it.typeID,
    @stationid,
    @regionid,
    @stationid,
    CASE
        WHEN it.basePrice IS NULL OR it.basePrice <= 0 THEN 100
        ELSE it.basePrice
    END,
    CASE
        WHEN LOWER(COALESCE(mg.metaGroupName, '')) IN
             ('officer', 'deadspace', 'faction', 'storyline') THEN 10
        ELSE 550
    END,
    CASE
        WHEN LOWER(COALESCE(mg.metaGroupName, '')) IN
             ('officer', 'deadspace', 'faction', 'storyline') THEN 10
        ELSE 550
    END,
    132478179209572976,
    1,
    250,
    @solarSystemID,
    1
FROM invTypes AS it
INNER JOIN invGroups AS ig
    ON it.groupID = ig.groupID
LEFT JOIN invMetaTypes AS imt
    ON it.typeID = imt.typeID
LEFT JOIN invMetaGroups AS mg
    ON imt.metaGroupID = mg.metaGroupID
WHERE ig.categoryID = 7
  AND (
      it.published = 1
      OR LOWER(COALESCE(mg.metaGroupName, '')) IN
         ('officer', 'deadspace', 'faction', 'storyline')
  )
AND NOT EXISTS (
    SELECT 1
    FROM mktOrders AS existing
    WHERE existing.stationID = @stationid
      AND existing.typeID = it.typeID
      AND existing.bid = 0
);

-- Report the module sell orders now available at the selected station.
SELECT
    COUNT(*) AS moduleSellOrderCount,
    SUM(volRemaining) AS totalModuleUnits
FROM mktOrders
WHERE stationID = @stationid
  AND bid = 0
  AND typeID IN (
      SELECT it.typeID
      FROM invTypes AS it
      INNER JOIN invGroups AS ig ON it.groupID = ig.groupID
      WHERE ig.categoryID = 7
  );
