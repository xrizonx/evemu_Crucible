"""Source-level invariants for the gameplay systems audit.

These checks identify accidental regression of verified source-level fixes.
They do not certify in-game behavior, historical static data or unimplemented
mining drones, contracts, invention or mailing-list administration.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def source(path):
    return (ROOT / path).read_text(encoding="utf-8")

tractor = source("src/eve-server/ship/modules/ActiveModule.cpp")
assert "pilotAlliance != 0 && m_targetSE->GetAllianceID() == pilotAlliance" in tractor
assert "pilotFaction != 0 && m_targetSE->GetWarFactionID() == pilotFaction" in tractor
assert "pilotFleet != 0 && pilotFleet == m_targetSE->GetFleetID()" in tractor
assert "if (startTractor)" in tractor
assert tractor.index("if (startTractor)") > tractor.index("if (!std::isfinite(range) || range <= 0.0f || distance > range)")
assert "m_targetSE->DestinyMgr() != nullptr" in tractor

target = source("src/eve-server/system/TargetManager.cpp")
assert "if (sRef.get() == nullptr || tSE == nullptr)" in target
assert "if (scanRes == 0)" in target
assert "MiningLaser* miningModule = module->GetMiningModule();" in target
assert "if (miningModule != nullptr)" in target
assert "targeter == entry.first && targeter != nullptr" in target

agent = source("src/eve-server/agents/AgentBound.cpp")
start = agent.index("PyResult AgentBound::GetMissionObjectiveInfo(PyCallArgs &call,")
end = agent.index("PyResult AgentBound::GetMyJournalDetails", start)
obj = agent[start:end]
assert "if (!m_agent->HasMission(callerID, offer))" in obj
assert "characterID.value()->value() != callerID" in obj

maildb = source("src/eve-server/mail/MailDB.cpp")
mail = source("src/eve-server/mail/MailMgrService.cpp")
assert "bool MailDB::IsMailRecipient(int32 messageID, uint32 characterID) const" in maildb
assert "m_db.IsMailRecipient(messageID->value(), call.client->GetCharacterID())" in mail
assert "DELETE FROM mailStatus WHERE messageID = %u AND characterID = %u" in maildb
assert "UPDATE mailStatus SET statusMask" in maildb
assert 'UPDATE mailMessage SET statusMask' not in maildb
assert "AND messageID IN (" in maildb and "WHERE characterID = " in maildb
assert "if (bit < 0 || bit > 30)" in maildb
assert "return 0; // never create mailStatus rows with an invalid ID" in maildb

contracts = source("src/eve-server/contract/ContractProxy.cpp")
assert "contractID->value()" in contracts
assert "flagArg == call.byname.end()" in contracts
assert "requestedArg == call.byname.end()" in contracts
assert "sDatabase.DoEscapeString(titleEscaped, title->content());" in contracts
assert "if (plasticWrap.get() == nullptr)" in contracts
assert "SELECT issuerID, status FROM ctrContracts" in contracts
assert "ownerRow.GetUInt(0) != call.client->GetCharacterID()" in contracts
assert "ownerRow.GetInt(1) != 0" in contracts
assert "const bool isPrivate = row.GetBool(11);" in contracts
assert "const uint32 assigneeID = row.GetUInt(12);" in contracts
assert "const uint32 acceptorID = row.GetUInt(11);" in contracts
assert "contractType != 3 || status != 1 || acceptorID != call.client->GetCharacterID()" in contracts

ram = source("src/eve-server/manufacturing/RamProxyService.cpp")
activity = source("src/eve-server/manufacturing/RamMethods.cpp")
assert "if (quantity == 0)\n            continue;" in ram
assert "if (!FactoryDB::CompleteJob(args.jobID," in ram
assert "if (installedItem.get() == nullptr)" in ram
assert "if (!args.cancel && data.activity == EvERam::Activity::Invention)" in ram
# Verify the invention path rejects the job BEFORE payment or installation.
# Test the actual exception, not an implementation comment whose wording can change.
activity_check = activity.split("void RamMethods::ActivityCheck(", 1)[1].split(
    "void RamMethods::JobsCheck(", 1)[0]
invention_case = activity_check.split("case EvERam::Activity::Invention:", 1)[1].split(
    "case EvERam::Activity::ReverseEngineering:", 1)[0]
assert 'throw CustomError("Blueprint invention is not functional on this server yet;' in invention_case
assert 'FactoryDB::InstallJob(' not in invention_case
assert 'AccountService::TransferFunds(' not in invention_case
install = ram.split("PyResult RamProxyService::InstallJob(", 1)[1].split(
    "PyResult RamProxyService::", 1)[0]
assert install.index("sRamMthd.ActivityCheck(call.client, args, bpRef);") < install.index(
    "AccountService::TransferFunds(")
assert install.index("sRamMthd.ActivityCheck(call.client, args, bpRef);") < install.index(
    "FactoryDB::InstallJob(")

drone = source("src/eve-server/npc/DroneAI.cpp")
assert "if (m_pDrone == nullptr)" in drone
assert "IsMiningDrone(m_pDrone) || (pTarget != nullptr && pTarget->IsAsteroidSE())" in drone
assert "asteroid->SetAttribute(AttrQuantity, left);" in drone
assert "ore->MergeTypesInCargo(shipItem.get(), hold);" in drone
mining = drone.split("void DroneAIMgr::ProcessMining()", 1)[1].split("void DroneAIMgr::StopMiningBeam()", 1)[0]
assert mining.index("EvEMining::Orbit(") < mining.index("m_miningTimer.Start(duration);")
assert "if (!flight.inOrbit || MiningSeparation(m_pDrone, target) > range)" in mining
assert "EvEMining::Approach(" in mining and "movement->ApplyMiningDroneStep(" in mining
assert "if (!inRange || (m_minedUnits > 0 && !DeliverMiningOre(ship)))" in mining
assert "movement->Follow(" not in mining
assert "!m_AI->HasMiningFlight()" in source("src/eve-server/npc/Drone.cpp")
drone_snapshot = source("src/eve-server/npc/Drone.cpp").split("void DroneSE::EncodeDestiny", 1)[1]
assert "follow.followRange = m_destiny->GetFollowRange();" in drone_snapshot
assert "data.speedfraction = m_destiny->GetCommandedSpeedFraction();" in drone_snapshot
entity = source("src/eve-server/npc/EntityService.cpp")
assert "pTarget != nullptr && pTarget->IsAsteroidSE()" in entity
assert "return OrderMiningDrones(call, droneIDs, targetID, false);" in entity
assert "return OrderMiningDrones(call, droneIDs, targetID, true);" in entity
assert "ship->TargetMgr()->GetTarget(asteroidID, true) != asteroid" in entity
assert "drone->GetControllerID() == ship->GetID() && drone->GetHomeShip() == ship" in entity
# A startup cleanup must never strip attributes from probe/wormhole items
# that it intentionally preserves outside the live solar-system location range.
cleaner = source("src/eve-server/DBCleaner.cpp")
probe_cleanup = cleaner.split("void DBCleaner::CleanGroupFromSpace(", 1)[1].split(
    "void DBCleaner::CleanOrphanedWormholes(", 1)[0]
wormhole_cleanup = cleaner.split("void DBCleaner::CleanOrphanedWormholes(", 1)[1].split(
    "void DBCleaner::CleanDungeonEntities(", 1)[0]
space_filter = "locationID >= 30000000 AND locationID <= 32000000"
assert probe_cleanup.count(space_filter) == 2, (
    "Probe attribute and entity deletions must use the same space filter")
assert wormhole_cleanup.count(space_filter) == 2, (
    "Wormhole attribute and entity deletions must use the same space filter")
assert wormhole_cleanup.count(
    "NOT EXISTS (SELECT 1 FROM sysSignatures WHERE sigItemID = entity.itemID)") == 2, (
    "Both wormhole deletions must use NULL-safe orphan detection")
assert "NOT IN (SELECT sigItemID FROM sysSignatures)" not in wormhole_cleanup

# Never hang a loaded solar system when enumerating nonempty rat-spawn
# bubbles. Iteration must progress for both small and large collections.
system = source("src/eve-server/system/SystemManager.cpp")
spawn_bubble_copy = system.split("void SystemManager::GetSpawnBubbles(", 1)[1].split(
    "void SystemManager::RemoveSpawnBubble(", 1)[0]
assert "for (const auto& entry : m_ratBubbles)" in spawn_bubble_copy
assert "bubbleMap->emplace(entry.first, entry.second);" in spawn_bubble_copy
assert "while (itr != m_ratBubbles.end())" not in spawn_bubble_copy

# An unloaded system must reclaim the bubble allocations as well as all
# lookup entries. This is a source guard, not a runtime leak-sanitizer result.
bubbles = source("src/eve-server/system/BubbleManager.cpp")
system_bubble = source("src/eve-server/system/SystemBubble.cpp")
clear_system = bubbles.split("void BubbleManager::ClearSystemBubbles(", 1)[1].split(
    "void BubbleManager::RemoveBubble(", 1)[0]
assert "itr = m_bubbles.erase(itr);" in clear_system
assert "m_bubbleIDMap.erase(bubble->GetID());" in clear_system
assert "m_spawnIDs.erase(bubble->GetID());" in clear_system
assert "bubble->ForgetMarkersAfterSystemUnload();" in clear_system
assert "SafeDelete(bubble);" in clear_system
assert "m_sysBubbleMap.erase(systemID);" in clear_system
assert "m_bubbles.remove(" not in clear_system
assert "m_retiredBubbles.begin()" in clear_system
assert "retiredItr = m_retiredBubbles.erase(retiredItr);" in clear_system
remove_empty = bubbles.split("void BubbleManager::RemoveEmpty()", 1)[1].split(
    "void BubbleManager::Add(", 1)[0]
assert "m_retiredBubbles.push_back(retired);" in remove_empty
assert "m_markers.clear();" in system_bubble.split(
    "void SystemBubble::ForgetMarkersAfterSystemUnload()", 1)[1].split(
    "void SystemBubble::clear()", 1)[0]
clear_all = bubbles.split("void BubbleManager::clear()", 1)[1].split(
    "void BubbleManager::Process()", 1)[0]
for lookup in ("m_bubbles", "m_retiredBubbles", "m_wanderers", "m_bubbleIDMap", "m_sysBubbleMap", "m_spawnIDs"):
    assert lookup + ".clear();" in clear_all

# Disconnecting before character initialization must release allocations
# created by Client::Client, without running full gameplay logout.
client = source("src/eve-server/Client.cpp")
partial_logout = client.split("Client::~Client()", 1)[1].split("m_loaded = false;", 1)[0]
assert "if (!m_loaded)" in partial_logout
for owned in ("m_TS", "m_scan", "pShipSE", "pSession"):
    assert "SafeDelete(" + owned + ");" in partial_logout
for queued in ("m_destinyEventQueue", "m_destinyUpdateQueue"):
    assert "PyDecRef(" + queued + ");" in partial_logout
assert "m_services.ClearBoundObject(*cur, this);" in partial_logout

# DispatchPacket only borrows inbound PyPacket pointers. Keep processing
# bounded so one busy client cannot starve other clients or movement ticks,
# and reclaim each popped packet even when dispatch throws.
packet_loop = client.split("bool Client::ProcessNet()", 1)[1].split(
    "bool Client::SelectCharacter(", 1)[0]
assert "kMaxPacketsPerTick = 64;" in packet_loop
assert "handled < kMaxPacketsPerTick" in packet_loop
assert "std::unique_ptr<PyPacket> packet(PopPacket());" in packet_loop
assert "PyPacket* p = packet.get();" in packet_loop
assert "p = nullptr;" not in packet_loop
assert 'm_char.get() != nullptr' in packet_loop
assert "_SendQueuedUpdates();" in packet_loop

# ElementParser is owned as a base-class pointer in XMLParser and holds
# derived MemberElementParser/ValueParser instances. Deletion through the base
# requires a virtual destructor; ASan caught a new-delete-type-mismatch when
# eve-xmlpktgen generated packet headers under sanitizer instrumentation.
xml_parser_header = source("src/eve-core/utils/XMLParser.h")
element_interface = xml_parser_header.split("class ElementParser", 1)[1].split(
    "    };", 1)[0]
assert "virtual bool Parse(" in element_interface
assert "virtual ~ElementParser() = default;" in element_interface
xml_parser_impl = source("src/eve-core/utils/XMLParser.cpp")
assert "SafeDelete(cur.second);" in xml_parser_impl

# Marshal byte streams deliberately pack multi-byte values immediately after
# one-byte markers. Never regress to typed, potentially unaligned reads/writes.
buffer = source("src/eve-core/utils/Buffer.h")
assign = buffer.split("void AssignAt(", 1)[1].split("void AssignSeqAt(", 1)[0]
assert "std::memcpy( &mBuffer[ _index ], &value, sizeof( T ) );" in assign
assert "*(T*)&mBuffer" not in assign
unmarshal_h = source("src/eve-common/marshal/EVEUnmarshal.h")
assert "std::memcpy( &value, &*bytes, sizeof(T) );" in unmarshal_h
unmarshal_cpp = source("src/eve-common/marshal/EVEUnmarshal.cpp")
assert "ReadPackedScalar<" in unmarshal_cpp
assert "std::vector<uint16> characters(len);" in unmarshal_cpp
marshal_test = source("src/eve-test/marshal/EVEMarshalTest.cpp")
assert "unaligned.Append<uint8>(marker);" in marshal_test
assert "unaligned.Append<uint32>(word);" in marshal_test
assert "std::memcmp(&unaligned[1], &word, sizeof(word))" in marshal_test

# The EVE client rebuilds application rows from the notification's
# old/new dictionary. Omitting even an unchanged field (notably "roles")
# causes KeyError in bco_applications.py when joining a player corporation.
import re
import xml.etree.ElementTree as ET

application_columns = (
    "applicationID", "corporationID", "characterID", "applicationText",
    "roles", "grantableRoles", "status", "applicationDateTime",
    "deleted", "lastCorpUpdaterID",
)
corporation_packets = ET.fromstring(source("src/eve-common/packets/CorporationPkts.xmlp"))
application_packet = corporation_packets.find(
    "./elementDef[@name='OnCorporationApplicationChanged']/tupleInline/dictInline"
)
assert application_packet is not None
application_entries = application_packet.findall("dictInlineEntry")
assert len(application_entries) == len(application_columns)
assert {entry.attrib["key"] for entry in application_entries} == set(application_columns)
for entry in application_entries:
    key = entry.attrib["key"]
    raw_fields = [field.attrib["name"] for field in entry.findall("./tupleInline/raw")]
    assert raw_fields == [key + "Old", key + "New"], (key, raw_fields)

registry = source("src/eve-server/corporation/CorpRegistryBound.cpp")
application_change = registry.split(
    "void CorpRegistryBound::FillOCApplicationChange(", 1
)[1].split("PyResult CorpRegistryBound::GetStations(", 1)[0]
assert "if (Old.valid)" in application_change
assert "if (New.valid)" in application_change
for key in application_columns:
    for direction in ("Old", "New"):
        # Both an actual value and PyNone must be assigned for insert/delete.
        assert application_change.count("OCAC." + key + direction + " =") == 2, (
            "Missing application change value for", key, direction)

corp_database = source("src/eve-server/corporation/CorporationDB.cpp")
mine = corp_database.split("PyRep *CorporationDB::GetMyApplications(", 1)[1].split(
    "PyRep *CorporationDB::GetApplications(", 1)[0]
corp = corp_database.split("PyRep *CorporationDB::GetApplications(", 1)[1].split(
    "bool CorporationDB::GetCurrentApplicationInfo(", 1)[0]
def sql_from_cpp(body):
    return " ".join("".join(re.findall(r'"([^"]*)"', body)).split())

columns = ", ".join(application_columns)
expected_query = "SELECT " + columns + " FROM crpApplications"
assert expected_query in sql_from_cpp(mine)
assert expected_query in sql_from_cpp(corp)
insert_app = corp_database.split("bool CorporationDB::InsertApplication(", 1)[1].split(
    "bool CorporationDB::UpdateApplication(", 1)[0]
assert "(corporationID, characterID, applicationText, roles, grantableRoles," in insert_app
assert "status, applicationDateTime, deleted, lastCorpUpdaterID)" in insert_app
lookup_app = corp_database.split("bool CorporationDB::GetCurrentApplicationInfo(", 1)[1].split(
    "bool CorporationDB::InsertApplication(", 1)[0]
assert "WHERE characterID = %u AND corporationID = %u AND deleted = 0 ORDER BY applicationID DESC LIMIT 1" in sql_from_cpp(lookup_app)
assert "AND applicationText =" not in lookup_app
update_offer = registry.split("PyResult CorpRegistryBound::UpdateApplicationOffer(", 1)[1].split(
    "PyResult CorpRegistryBound::DeleteApplication(", 1)[0]
assert update_offer.index("CreateMemberAttributeUpdate(") < update_offer.index("m_db.UpdateApplication(")
withdraw_app = registry.split("PyResult CorpRegistryBound::DeleteApplication(", 1)[1].split(
    "void CorpRegistryBound::FillOCApplicationChange(", 1)[0]
assert "CorpNotify(targetCorpID," in withdraw_app
home_stations = source("src/eve-server/corporation/CorpStationMgr.cpp").split(
    "PyResult CorpStationMgrIMBound::GetPotentialHomeStations(", 1
)[1].split("PyResult CorpStationMgrIMBound::", 1)[0]
assert home_stations.index("for (auto cur : stVec)") < home_stations.index("PyDict* dict = new PyDict()")
assert "if (!stDataMgr.GetStationData(cur, data))\n            continue;" in home_stations

# Unmarshaling a packed row must NOT reinterpret the generic PyObjectEx
# returned by LoadRep as a C++ DBRowDescriptor. This previously passed normal
# builds but tripped UBSan's invalid-downcast check in EVEMarshalTest.
unmarshal_impl = source("src/eve-common/marshal/EVEUnmarshal.cpp")
packed_row = unmarshal_impl.split("PyRep* UnmarshalStream::LoadPackedRow()", 1)[1].split(
    "PyRep* UnmarshalStream::LoadError()", 1)[0]
assert "new DBRowDescriptor()" in packed_row
assert 'content() != "blue.DBRowDescriptor"' in packed_row
assert "descriptor->AddColumn(" in packed_row
assert "PyDecRef(header_element);" in packed_row
assert "new PyPackedRow(descriptor)" in packed_row
assert "(DBRowDescriptor*)header_element" not in packed_row
marshal_test = source("src/eve-test/marshal/EVEMarshalTest.cpp")
assert 'decoded->header()->ColumnCount() != 6' in marshal_test
assert 'decoded->GetField(5)->AsInt()->value() != 254' in marshal_test

# The Crucible cargo-space localization can expose raw {[numeric]...}
# tokens instead of resolving volume/available, even when capacity checks
# correctly reject the move. Keep one-unit and stack rejection messages
# fully numeric without relaxing the actual cargo-space validation.
inventory_cpp = source("src/eve-server/inventory/Inventory.cpp")
validate_add = inventory_cpp.split("bool Inventory::ValidateAddItem(", 1)[1].split(
    "// multimerge", 1
)[0]
assert "float capacity = GetRemainingCapacity(flag);" in validate_add
assert "float totalVolume = iRef->quantity() * volume;" in validate_add
assert "if (capacity < volume)" in validate_add
assert "if (totalVolume > capacity)" in validate_add
assert validate_add.count('throw CustomError("Not enough cargo space:') == 2
assert "NotEnoughCargoSpace\")" not in validate_add
assert "std::map<std::string, PyRep *> args;" not in validate_add
assert "%.4f m3 required for one unit; %.4f m3 available." in validate_add
assert "%.4f m3 required for %d unit(s); %.4f m3 available." in validate_add

# Belt rat patrols must be security limited, small, and spawned once.
# NPC encounter missions and anomaly waves deliberately keep separate logic.
belt_spawns = source("src/eve-server/system/cosmicMgrs/SpawnMgr.cpp")
belt_bubbles = source("src/eve-server/system/SystemBubble.cpp")
wander = belt_bubbles.split("void SystemBubble::ProcessWander(", 1)[1].split(
    "void SystemBubble::Add(", 1
)[0]
assert "ResetBubbleRatSpawn();" not in wander
assert "EvEBeltRats::SecurityTenths(secRating)" in belt_spawns
assert "EvEBeltRats::ForSecurity(secRating).maxShips" in belt_spawns
assert "EvEBeltRats::AllowsShipClass(secRating, shipClass, sClass)" in belt_spawns
assert "EvEBeltRats::MatchesGroupHull(shipClass, group.name)" in belt_spawns
assert "std::reverse(m_toSpawn.begin(), m_toSpawn.end())" in belt_spawns
assert "remaining -= group.quantity;" in belt_spawns
assert "m_toSpawn.erase(std::remove_if(" in belt_spawns
assert "m_spawns.count(pBubble->GetID()) != 0 || pBubble->CountNPCs() != 0" in belt_spawns
assert "return MakeSpawn(pBubble, factionID, sClass, level, anomaly);" in belt_spawns
assert "return spawnedCount != 0;" in belt_spawns
assert "rareRoll < 0.0005" in belt_spawns
assert "pBubble->IsBelt() || MakeRandomFloat() > 0.05" in belt_spawns
assert "uint32 time = sConfig.npc.RoamingTimer * 1000" in belt_spawns
assert "itr = m_spawns.erase(itr);" in belt_spawns
respawn_function = belt_spawns.split("bool SpawnMgr::ReSpawn(", 1)[1].split(
    "bool SpawnMgr::FindSpawnForBubble(", 1
)[0]
assert "spawnEntry.itemID = iRef->itemID();" in respawn_function
assert "return true;" in respawn_function
assert "itr->second.stamp = sEntityList.GetStamp() + sConfig.npc.RespawnTimer;" in belt_spawns
assert "if (!ReSpawn(spawnBubble, itr->second)) {" in belt_spawns
assert "itr->second.enabled && itr->second.spawnClass <= Spawn::Class::Insane" in belt_spawns
assert "iRef->Delete();" in respawn_function
roaming = source("src/eve-server/npc/NPCAI.cpp").split("void NPCAIMgr::WarpOut()", 1)[1].split(
    "void NPCAIMgr::SetWander()", 1)[0]
assert roaming.index("!spawns->HasRoomForRoamingRat(*destination)") < roaming.index("m_destiny->WarpTo(")
assert roaming.index("m_destiny->HasWarpOrder()") < roaming.index("spawns->MoveSpawn(")
assert "MakeRandomFloat() < 0.08" not in belt_spawns

belt_policy = source("src/eve-common/EVE_BeltRatSpawnHelpers.h")
for rule in ("if (sec >= 8) return {Frigate, 2}",
             "if (sec >= 5) return {Cruiser, 3}",
             "if (sec == 4) return {Cruiser, 3}",
             "if (sec >= 1) return {Battlecruiser, 4}",
             "return {Battleship, 5}"):
    assert rule in belt_policy
belt_cmake = source("src/eve-test/CMakeLists.txt")
assert "utils/BeltRatSpawnTest.cpp" in belt_cmake
assert 'ADD_TEST( NAME "BeltRatSpawnTest"' in belt_cmake

# The virtual buyer must resolve even when the client requests only that
# owner and there is no SQL result from which to infer the response schema.
owner_lookup = source("src/eve-server/config/ConfigDB.cpp").split(
    "PyRep *ConfigDB::GetMultiAllianceShortNamesEx", 1
)[0]
assert "if (IsTraderJoe(cur))" in owner_lookup
assert 'row->SetItemString(1, "Trader Joe");' in owner_lookup
assert "row->SetItem(0, new PyInt(npcTraderJoe));" in owner_lookup
assert 'cols->SetItemString(4, "ownerNameID");' in owner_lookup
assert "res.ColumnName(" not in owner_lookup
assert owner_lookup.count("NULL AS ownerNameID") == 5

location_lookup = source("src/eve-server/config/ConfigDB.cpp").split(
    "PyRep *ConfigDB::GetMultiLocationsEx", 1
)[1].split("PyRep* ConfigDB::GetMultiStationEx", 1)[0]
assert " UNION ALL " in location_lookup
assert "append(staticItems," in location_lookup
assert "append(dynamicItems," in location_lookup
assert "append(asteroidItems," in location_lookup
assert "LOCATE('{asteroidType}', a.itemName)" in location_lookup
assert "LEFT JOIN invTypes AS t ON t.typeID = a.typeID" in location_lookup

# A newly selected character boots its solar system immediately after the
# AnomalyMgr initialization log. Invalid persisted NPC/wreck/containter state
# must never be registered as a valid entity or crash the entire server.
system_impl = source("src/eve-server/system/SystemManager.cpp")
boot_system = system_impl.split("bool SystemManager::BootSystem()", 1)[1].split(
    "bool SystemManager::LoadCosmicMgrs()", 1
)[0]
assert '"SystemBoot", "System %s(%u): cosmic managers ready; loading system dynamics."' in boot_system
assert '"SystemBoot", "System %s(%u): loading player dynamics."' in boot_system
assert '"SystemBoot", "System %s(%u): dynamics ready; initializing structures."' in boot_system
assert "if (cur.second == nullptr)" in boot_system
assert "if (pos != nullptr)" in boot_system
assert system_impl.count("pSE = DynamicEntityFactory::BuildEntity(*this, cur);") == 2
assert system_impl.count("dynamic item %u (type %u, group %u) threw during restoration") == 2
assert system_impl.count("catch (const std::exception& e)") >= 2

npc_factory = system_impl.split("SystemEntity* DynamicEntityFactory::BuildEntity(", 1)[1].split(
    "void SystemManager::AddClient(", 1
)[0]
assert "if (npcSE == nullptr || !npcSE->Load())" in npc_factory
assert "SafeDelete(npcSE);" in npc_factory
assert "inventory unavailable." in npc_factory
assert "if (wreckInventory == nullptr)" in npc_factory
assert "if (containerInventory == nullptr)" in npc_factory
assert "if (spawnInventory == nullptr)" in npc_factory

client_boot = source("src/eve-server/Client.cpp")
character_selection = client_boot.split("bool Client::SelectCharacter(", 1)[1].split(
    "void Client::ProcessClient()", 1
)[0]
assert "if (m_system == nullptr || pShipSE == nullptr || pShipSE->DestinyMgr() == nullptr)" in character_selection
assert 'CloseClientConnection();' in character_selection
destiny_setup = client_boot.split("void Client::SetDestiny(", 1)[1].split(
    "void Client::SetBallPark()", 1
)[0]
assert "if (m_system == nullptr || m_ship.get() == nullptr)" in destiny_setup
assert "if (pShipSE == nullptr || pShipSE->DestinyMgr() == nullptr" in destiny_setup
entity_boot = source("src/eve-server/EntityList.cpp").split(
    "SystemManager* EntityList::FindOrBootSystem(", 1
)[1].split("void EntityList::AddStation(", 1)[0]
assert "catch (const std::exception& e)" in entity_boot
assert "catch (...)" in entity_boot
assert "if (!booted)" in entity_boot
assert "SafeDelete(pSM);" in entity_boot

# Actual login reference crash: assignment must retain its next reference
# before releasing the previous one, including self and cross-type aliases.
refptr = source("src/eve-core/memory/RefPtr.h")
copy_assign = refptr.split("RefPtr& AssignRef(X* next)", 1)[1].split(
    "/// The pointer to the reference-counted object.", 1
)[0]
assert "if (next == mPtr)" in copy_assign
assert copy_assign.index("next->IncRef();") < copy_assign.index("previous->DecRef();")
assert "RefPtr& operator=(RefPtr&& oth) noexcept" in refptr
assert "oth.mPtr = nullptr;" in refptr
assert 'FATAL: DecRef() on deleted object' in refptr
assert 'FATAL: IncRef() on deleted object' in refptr
assert 'std::cerr << "FATAL: IncRef() called on deleted object!' not in refptr

callable = source("src/eve-server/services/Callable.cpp")
result_copy = callable.split("PyResult& PyResult::operator=(const PyResult& oth)", 1)[1]
assert "if (this == &oth)" in result_copy
assert result_copy.index("PyIncRef(nextResult);") < result_copy.index("PySafeDecRef(ssResult);")
assert result_copy.index("PySafeIncRef(nextNamed);") < result_copy.index("PySafeDecRef(ssNamedResult);")
assert "RefPtrLifetimeTest.cpp" in source("src/eve-test/CMakeLists.txt")
assert 'ADD_TEST( NAME "RefPtrLifetimeTest"' in source("src/eve-test/CMakeLists.txt")

system_boot = source("src/eve-server/system/SystemManager.cpp")
assert "LSC channels ready." in system_boot
assert "startup complete." in system_boot
login = source("src/eve-server/Client.cpp")
assert login.count('"LoginStage"') >= 7

print("Source-level gameplay safety guards passed")
