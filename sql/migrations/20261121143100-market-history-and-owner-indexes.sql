-- Cover the wallet's owner-order list and the market's regional history.
-- The bundled schemas have no index for either lookup.
-- +migrate Up
CREATE INDEX idx_mkt_orders_owner ON mktOrders (ownerID);
CREATE INDEX idx_mkt_history_region_type_date ON mktHistory (regionID, typeID, historyDate);
-- +migrate Down
DROP INDEX idx_mkt_history_region_type_date ON mktHistory;
DROP INDEX idx_mkt_orders_owner ON mktOrders;
