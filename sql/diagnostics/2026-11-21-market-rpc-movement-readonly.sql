-- Read-only operator checks for blank Crucible market windows / RPC delays.
-- USE your EvE database before running (does not modify any rows).
-- Do not treat zero orders as a server bug: check which region was seeded.
SELECT COUNT(*) AS market_group_count FROM invMarketGroups;
SELECT COUNT(*) AS published_market_types
FROM invTypes WHERE published = 1 AND marketGroupID IS NOT NULL;
SELECT regionID, bid, COUNT(*) AS order_count,
       SUM(CASE WHEN volRemaining > 0 THEN 1 ELSE 0 END) AS live_orders
FROM mktOrders GROUP BY regionID, bid ORDER BY order_count DESC LIMIT 30;

-- The market RPC migration should install all three names. Missing indexes
-- indicate that the migration has not yet been applied to this database.
SELECT INDEX_NAME, GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX) AS columns_in_order
FROM information_schema.STATISTICS
WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'mktOrders'
  AND INDEX_NAME IN ('idx_mkt_rpc_region','idx_mkt_rpc_station','idx_mkt_rpc_system')
GROUP BY INDEX_NAME ORDER BY INDEX_NAME;

-- Replace 10000001 with a region that has orders from the result above.
EXPLAIN SELECT price, volRemaining, typeID, orderRange AS `range`,
       orderID, volEntered, minVolume, bid, issued AS issueDate,
       duration, stationID, regionID, solarSystemID, jumps
FROM mktOrders WHERE regionID = 10000001 AND typeID = 34 AND bid = 1;

-- Check for SQL calls currently blocking the database during reproduction.
-- Sensitive query contents are intentionally not displayed.
SELECT ID, USER, DB, COMMAND, TIME AS seconds_running, STATE
FROM information_schema.PROCESSLIST
WHERE COMMAND <> 'Sleep' AND ID <> CONNECTION_ID()
ORDER BY TIME DESC LIMIT 30;
