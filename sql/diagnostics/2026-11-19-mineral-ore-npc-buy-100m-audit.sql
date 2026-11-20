-- EVEmu Crucible: read-only audit of high-volume NPC mineral and ore buy orders.
-- Does not change player, NPC, market, or wallet data.
-- Target: mineral group 18, asteroid/ore category 25; published market types only.
-- Static seed marker: ownerID=4000000, bid=1, isCorp=0, orderRange=0, duration=250.
-- For prices over 100,000 ISK, the 10 trillion ISK escrow cap reduces volume.
-- To evaluate a particular region, add AND o.regionID = <regionID> to order queries.

-- 1. Quantity and escrow discrepancies for currently seeded target items.
SELECT o.regionID, o.stationID, o.typeID, it.typeName,
       o.price, o.volEntered, o.volRemaining, o.escrow,
       LEAST(100000000,
         GREATEST(1, FLOOR(10000000000000.00 / o.price))) AS expected_seed_quantity
FROM mktOrders AS o
JOIN invTypes AS it ON it.typeID = o.typeID
JOIN invGroups AS ig ON ig.groupID = it.groupID
WHERE o.ownerID = 4000000 AND o.bid = 1 AND o.isCorp = 0
  AND o.orderRange = 0 AND o.duration = 250
  AND it.published = 1 AND it.marketGroupID IS NOT NULL
  AND (ig.groupID = 18 OR ig.categoryID = 25)
  AND o.price > 0
  AND (o.volEntered <> LEAST(100000000,
       GREATEST(1, FLOOR(10000000000000.00 / o.price)))
       OR o.volRemaining > o.volEntered
       OR o.escrow > 10000000000000.00)
ORDER BY o.regionID, o.stationID, o.typeID
LIMIT 250;

-- 2. Coverage and aggregate quantities by group and region.
SELECT o.regionID, ig.groupID, ig.categoryID, ig.groupName,
       COUNT(*) AS seeded_orders, COUNT(DISTINCT o.typeID) AS distinct_types,
       SUM(o.volEntered) AS total_original_units,
       SUM(o.volRemaining) AS total_remaining_units,
       MIN(o.volEntered) AS smallest_seed_order,
       MAX(o.volEntered) AS largest_seed_order
FROM mktOrders AS o
JOIN invTypes AS it ON it.typeID = o.typeID
JOIN invGroups AS ig ON ig.groupID = it.groupID
WHERE o.ownerID = 4000000 AND o.bid = 1 AND o.isCorp = 0
  AND o.orderRange = 0 AND o.duration = 250
  AND (ig.groupID = 18 OR ig.categoryID = 25)
GROUP BY o.regionID, ig.groupID, ig.categoryID, ig.groupName
ORDER BY o.regionID, ig.groupID;

-- 3. Duplicate seeded orders at a station, per type.
SELECT o.regionID, o.stationID, o.typeID, COUNT(*) AS duplicates
FROM mktOrders AS o
JOIN invTypes AS it ON it.typeID = o.typeID
JOIN invGroups AS ig ON ig.groupID = it.groupID
WHERE o.ownerID = 4000000 AND o.bid = 1 AND o.isCorp = 0
  AND o.orderRange = 0 AND o.duration = 250
  AND (ig.groupID = 18 OR ig.categoryID = 25)
GROUP BY o.regionID, o.stationID, o.typeID
HAVING COUNT(*) > 1
ORDER BY duplicates DESC LIMIT 100;

-- 4. Market-listed mineral/ore types with no static NPC buy seed in any region.
-- Configured seed regions and station saturation intentionally limit coverage.
SELECT it.typeID, it.typeName, ig.groupID, ig.groupName, ig.categoryID
FROM invTypes AS it
JOIN invGroups AS ig ON ig.groupID = it.groupID
LEFT JOIN mktOrders AS o ON o.typeID = it.typeID
  AND o.ownerID = 4000000 AND o.bid = 1 AND o.isCorp = 0
  AND o.orderRange = 0 AND o.duration = 250
WHERE it.published = 1 AND it.marketGroupID IS NOT NULL
  AND (ig.groupID = 18 OR ig.categoryID = 25)
  AND o.orderID IS NULL
ORDER BY it.typeID LIMIT 250;


-- 5. Invalid seeded order prices, minimum volumes or escrow.
-- Query 1 intentionally divides by a positive order price; this separate query
-- reports zero/negative/NULL prices that would otherwise escape that audit.
-- Static seeded buy orders require a one-unit minimum; negative escrow and
-- zero entered volume are also invalid. Read-only, no corrective UPDATE.
SELECT o.regionID, o.stationID, o.typeID, it.typeName,
       o.price, o.minVolume, o.volEntered, o.volRemaining, o.escrow
FROM mktOrders AS o
JOIN invTypes AS it ON it.typeID = o.typeID
JOIN invGroups AS ig ON ig.groupID = it.groupID
WHERE o.ownerID = 4000000 AND o.bid = 1 AND o.isCorp = 0
  AND o.orderRange = 0 AND o.duration = 250
  AND it.published = 1 AND it.marketGroupID IS NOT NULL
  AND (ig.groupID = 18 OR ig.categoryID = 25)
  AND (o.price IS NULL OR o.price <= 0
       OR o.minVolume IS NULL OR o.minVolume <> 1
       OR o.volEntered <= 0 OR o.escrow < 0)
ORDER BY o.regionID, o.stationID, o.typeID
LIMIT 250;
