#!/usr/bin/env python3
"""Source-level regression guards for market RPC and movement responsiveness.

These static checks do not substitute for a live client + populated MariaDB
test. The MariaDB CI job separately executes the market index migration.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


client = read("src/eve-server/Client.cpp")
network = client.split("bool Client::ProcessNet()", 1)[1].split(
    "bool Client::SelectCharacter(", 1
)[0]
assert "kMaxPacketsPerTick = 64;" in network
assert "handled < kMaxPacketsPerTick" in network
assert "std::unique_ptr<PyPacket> packet(PopPacket());" in network
assert 'Slow packet dispatch for %s' in network
assert "_SendQueuedUpdates();" in network
# Outgoing packets are queued asynchronously. The mutable per-client queues
# must not be cleared before the outgoing notification owns its own snapshot.
flush = client.split("void Client::_SendQueuedUpdates()", 1)[1].split(
    "void Client::SendNotification(", 1
)[0]
assert flush.count("new PyList(*m_destinyUpdateQueue)") == 2
assert "new PyList(*m_destinyEventQueue)" in flush
assert flush.count("PyDecRef(t);") == 3
assert "m_destinyUpdateQueue->clear();" in flush
pack = client.split("void Client::QueueDestinyUpdate(", 1)[1].split(
    "void Client::_SendQueuedUpdates()", 1
)[0]
assert "paList->AddItem(new PyList(*m_destinyUpdateQueue));" in pack
notification = client.split(
    "void Client::SendNotification(const PyAddress &dest", 1
)[1].split("void Client::DisconnectClient()", 1)[0]
assert "PySafeIncRef(noti.args);" in notification
assert "packet->payload = noti.Encode();" in notification

dbcore = read("src/eve-core/database/dbcore.cpp")
assert "return pStatus == Connected;" in dbcore
assert "MYSQL_OPT_READ_TIMEOUT" in dbcore
assert "MYSQL_OPT_CONNECT_TIMEOUT" in dbcore
assert "MYSQL_OPT_WRITE_TIMEOUT" in dbcore
query_result = dbcore.split("bool DBcore::RunQuery(DBQueryResult &into", 1)[1].split(
    "//query which returns only error status", 1
)[0]
assert "vasprintf(&query, query_fmt, vlist)" in query_result
assert "mysql_store_result(mysql)" in query_result
assert "Slow result query" in query_result
assert "char query[4096]" not in query_result
do_query = dbcore.split("bool DBcore::DoQuery_locked(", 1)[1].split(
    "int32 DBcore::DoEscapeString(", 1
)[0]
assert "const bool connectionLost" in do_query
assert "DoQuery_locked(err, query, querylen, false)" in do_query

market_db_h = read("src/eve-server/market/MarketDB.h")
market_db = read("src/eve-server/market/MarketDB.cpp")
assert "ReadOrders(DBcore& database, uint32 regionID, uint32 typeID" in market_db_h
assert "AND bid IN (%u,%u)" in market_db
assert "row.GetBool(bidColumn)" in market_db
assert "result->SetItem(0, sell);" in market_db
assert "result->SetItem(1, buy);" in market_db
assert "pyrow->SetField((uint32)0, pid->Clone());" in market_db
proxy = read("src/eve-server/market/MarketProxyService.cpp")
orders = proxy.split("PyResult MarketProxyService::GetOrders(", 1)[1].split(
    "PyResult MarketProxyService::PlaceCharOrder(", 1
)[0]
assert "typeID->value() <= 0" in orders
assert "return PyStatic.NewNone();" in orders
assert "sMktMgr.GetOrders(call, typeID->value())" in orders

market_mgr = read("src/eve-server/market/MarketMgr.cpp")
worker = market_mgr.split("    void Run()", 1)[1].split("MarketMgr::MarketMgr()", 1)[0]
assert "DBcore connection;" in worker
assert "MarketDB::ReadOrders(connection," in worker
assert "MarketDB::EncodeOrders(" not in worker and "m_cache" not in worker
assert "m_jobs.size() >= 128" in market_mgr
assert "m_deferredCalls.size() < MaxDeferredCalls" in client
assert "ProcessDeferredCalls();" in network
assert "call->deadline" in network
assert "read->rows.wait_for(std::chrono::seconds(0))" in market_mgr
assert "read->invalidated || OrderClock::now() >= read->expires" in market_mgr
assert "m_orderReader->Invalidate(key);" in market_mgr
assert "mysql_server_end()" not in dbcore
history = market_mgr.split("PyRep *MarketMgr::GetNewPriceHistory(", 1)[1].split(
    "void MarketMgr::SendOnOwnOrderChanged(", 1
)[0]
assert history.count("Price history query failed; leaving cache invalid for retry.") == 2
mgr_h = read("src/eve-server/market/MarketMgr.h")
assert "m_marketGroups = MarketDB::GetMarketGroups();" in mgr_h
assert "if (m_marketGroups == nullptr)" in mgr_h

destiny = read("src/eve-server/system/DestinyManager.cpp")
warp_to = destiny.split("void DestinyManager::WarpTo(", 1)[1].split(
    "void DestinyManager::", 1
)[0]
assert "!mySE->GetPilot()->IsLoginWarping()" in warp_to
assert "mySE->GetPilot()->SetLoginWarpComplete();" in warp_to
init_warp = destiny.split("void DestinyManager::InitWarp()", 1)[1].split(
    "void DestinyManager::", 1
)[0]
assert "mySE->GetPilot()->SetLoginWarpComplete();" in init_warp

index_sql = read("sql/migrations/20261121070000-market-order-rpc-indexes.sql")
for name, columns in [
    ("idx_mkt_rpc_region", "regionID, bid, typeID, price, orderID"),
    ("idx_mkt_rpc_station", "stationID, bid, typeID, price, orderID"),
    ("idx_mkt_rpc_system", "solarSystemID, bid, typeID, price, orderID"),
]:
    assert f"CREATE INDEX {name} ON mktOrders ({columns});" in index_sql
    assert f"DROP INDEX {name} ON mktOrders;" in index_sql
assert "DELETE FROM mktOrders" not in index_sql
assert "TRUNCATE" not in index_sql

print("PASS: market RPC, recovery, SQL timeouts and login-movement safety guards")
