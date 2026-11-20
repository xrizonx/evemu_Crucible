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

# DispatchPacket only borrows inbound PyPacket pointers. Every iteration
# of the network receive loop must reclaim the packet, including failures.
packet_loop = client.split("bool Client::ProcessNet()", 1)[1].split(
    "void Client::", 1)[0]
assert "while (PyPacket* p = PopPacket())" in packet_loop
assert "std::unique_ptr<PyPacket> packet(p);" in packet_loop
assert "p = nullptr;" not in packet_loop
assert 'm_char.get() != nullptr' in packet_loop

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

print("Source-level gameplay safety guards passed")
