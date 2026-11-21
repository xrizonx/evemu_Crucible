-- Market order RPCs run on the same server tick loop as ship movement.
-- Cover the region, station and solar-system "best ask" lookups as well
-- as per-item order queries. Existing single-column solarSystem indexes
-- do not efficiently support filtering by bid and grouping by type/price.
-- This migration does not change or delete any market orders.
-- Apply using the normal database migration process before restarting.
-- +migrate Up
CREATE INDEX idx_mkt_rpc_region ON mktOrders (regionID, bid, typeID, price, orderID);
CREATE INDEX idx_mkt_rpc_station ON mktOrders (stationID, bid, typeID, price, orderID);
CREATE INDEX idx_mkt_rpc_system ON mktOrders (solarSystemID, bid, typeID, price, orderID);

-- +migrate Down
DROP INDEX idx_mkt_rpc_system ON mktOrders;
DROP INDEX idx_mkt_rpc_station ON mktOrders;
DROP INDEX idx_mkt_rpc_region ON mktOrders;
