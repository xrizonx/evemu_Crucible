// Belt-only NPC spawn balancing for the Crucible-era server.
//
// Keep this separate from mission, anomaly and incursion wave composition.
// This helper is intentionally dependency-free so the precise security
// boundaries, hull classes and fleet caps can be regression-tested.
#ifndef EVE_BELT_RAT_SPAWN_HELPERS_H
#define EVE_BELT_RAT_SPAWN_HELPERS_H

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>

namespace EvEBeltRats {

enum Hull : uint8_t {
    Unknown = 0,
    Frigate = 1,
    Destroyer = 2,
    Cruiser = 3,
    Battlecruiser = 4,
    Battleship = 5
};

struct Policy {
    Hull maxHull;
    uint8_t maxShips;
};

// Security bands follow the one-decimal value visible in the EVE client.
// True security -0.01 and +0.04 both display 0.0, for example.
inline int SecurityTenths(double trueSecurity)
{
    if (!std::isfinite(trueSecurity))
        return 10; // safely avoid creating high-class rats from bad data
    return std::max(-10, std::min(10,
        static_cast<int>(std::floor(trueSecurity * 10.0 + 0.5))));
}

inline Policy ForSecurity(double trueSecurity)
{
    const int sec = SecurityTenths(trueSecurity);
    if (sec >= 8) return {Frigate, 2};        // 1.0 - 0.8
    if (sec >= 5) return {Cruiser, 3};        // 0.7 - 0.5
    if (sec == 4) return {Cruiser, 3};        // 0.4: destroyers and cruisers
    if (sec >= 1) return {Battlecruiser, 4};  // 0.3 - 0.1
    return {Battleship, 5};                   // 0.0 and negative true security
}

// npcClassGroup.shipClass: 1 normal frigate, 2 elite frigate,
// 3 destroyer, 4 cruiser, 5 elite cruiser, 6 battlecruiser,
// 7 battleship. 8/9 are special hauler/officer classes;
// 10-14 are commander variants of frigate/destroyer/cruiser/BC/BS.
inline Hull HullForShipClass(uint8_t shipClass)
{
    switch (shipClass) {
        case 1: case 2: case 10: return Frigate;
        case 3: case 11: return Destroyer;
        case 4: case 5: case 12: return Cruiser;
        case 6: case 13: return Battlecruiser;
        case 7: case 14: return Battleship;
        default: return Unknown;
    }
}

inline bool AllowsShipClass(double security, uint8_t shipClass,
                            uint8_t encounterClass)
{
    const Policy policy = ForSecurity(security);
    const Hull hull = HullForShipClass(shipClass);
    // Rare class IDs (10/11/12 in EVE_Spawn.h) are distinct from ship classes.
    if (shipClass == 8)
        return encounterClass == 10 && SecurityTenths(security) <= 0;
    if (shipClass == 9)
        return encounterClass == 12 && security <= -0.7;
    if (shipClass >= 10 && shipClass <= 14)
        return encounterClass == 11 && SecurityTenths(security) <= 3
            && hull != Unknown && hull <= policy.maxHull;
    return hull != Unknown && hull <= policy.maxHull;
}

// Defensive validation of npcClassGroup mappings. Group names can vary by
// pirate faction, but the hull label must match the ship-class column.
// Never silently spawn a Battleship from a misconfigured Frigate class.
inline bool MatchesGroupHull(uint8_t shipClass, const std::string& groupName)
{
    if (shipClass == 8 || shipClass == 9) return true; // special non-hull groups
    const Hull expected = HullForShipClass(shipClass);
    if (expected == Unknown) return false;
    std::string lower;
    lower.reserve(groupName.size());
    for (const unsigned char ch : groupName)
        lower.push_back(static_cast<char>(std::tolower(ch)));
    Hull actual = Unknown;
    if (lower.find("battleship") != std::string::npos) actual = Battleship;
    else if (lower.find("battlecruiser") != std::string::npos) actual = Battlecruiser;
    else if (lower.find("destroyer") != std::string::npos) actual = Destroyer;
    else if (lower.find("cruiser") != std::string::npos) actual = Cruiser;
    else if (lower.find("frigate") != std::string::npos) actual = Frigate;
    return actual == expected;
}

inline bool AllowOfficer(double security) { return security <= -0.7; }
inline bool AllowCommander(double security) { return SecurityTenths(security) <= 3; }
inline bool AllowHauler(double security) { return SecurityTenths(security) <= 0; }

} // namespace EvEBeltRats

#endif
