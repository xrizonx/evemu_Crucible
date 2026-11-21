#include "eve-test.h"
#include "EVE_BeltRatSpawnHelpers.h"

#include <cstdio>
#include <cstdlib>
#include <limits>

int utils_BeltRatSpawnTest(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    using namespace EvEBeltRats;

    struct Case {
        double security;
        Hull maxHull;
        uint8_t maxShips;
    };
    // Real display-security boundaries, including non-exact trueSec values.
    const Case cases[] = {
        {1.0, Frigate, 2}, {0.90, Frigate, 2},
        {0.80, Frigate, 2}, {0.75, Frigate, 2},
        {0.74, Cruiser, 3}, {0.70, Cruiser, 3},
        {0.50, Cruiser, 3}, {0.45, Cruiser, 3},
        {0.40, Cruiser, 3}, {0.35, Cruiser, 3},
        {0.34, Battlecruiser, 4}, {0.30, Battlecruiser, 4},
        {0.10, Battlecruiser, 4}, {0.05, Battlecruiser, 4},
        {0.04, Battleship, 5}, {0.0, Battleship, 5},
        {-0.5, Battleship, 5}, {-1.0, Battleship, 5}
    };
    for (const Case& c : cases) {
        const Policy p = ForSecurity(c.security);
        if (p.maxHull != c.maxHull || p.maxShips != c.maxShips) {
            std::printf("Belt policy incorrect at security %.4f\n", c.security);
            return EXIT_FAILURE;
        }
    }

    // 1.0-0.8 allow only frigates, including advanced frigates.
    if (!AllowsShipClass(0.8, 1, 1) || !AllowsShipClass(0.8, 2, 1)
        || AllowsShipClass(0.8, 3, 1) || AllowsShipClass(0.8, 4, 1)
        || AllowsShipClass(0.8, 6, 1) || AllowsShipClass(0.8, 7, 1)
        || AllowsShipClass(0.8, 8, 10) || AllowsShipClass(0.8, 9, 12))
        return EXIT_FAILURE;

    // 0.7-0.5 stop at cruisers; 0.4 additionally permits destroyers.
    if (!AllowsShipClass(0.5, 4, 2) || !AllowsShipClass(0.5, 5, 2)
        || AllowsShipClass(0.5, 6, 2) || AllowsShipClass(0.5, 7, 2)
        || !AllowsShipClass(0.4, 3, 3) || AllowsShipClass(0.4, 6, 3))
        return EXIT_FAILURE;

    // 0.3-0.1 can spawn BCs but not BS; nullsec permits BS.
    if (!AllowsShipClass(0.3, 6, 4) || AllowsShipClass(0.3, 7, 4)
        || !AllowsShipClass(0.0, 7, 5) || !AllowsShipClass(-1.0, 7, 7))
        return EXIT_FAILURE;

    // Faction group data must not disguise a battleship as a frigate.
    if (!MatchesGroupHull(1, "Asteroid Angel Cartel Frigate")
        || !MatchesGroupHull(6, "Asteroid Angel Cartel BattleCruiser")
        || !MatchesGroupHull(7, "Asteroid Angel Cartel Battleship")
        || MatchesGroupHull(1, "Asteroid Angel Cartel Battleship")
        || MatchesGroupHull(4, "Asteroid Angel Cartel BattleCruiser")
        || MatchesGroupHull(7, "Unknown NPC Hull"))
        return EXIT_FAILURE;

    // Special/rare spawns cannot bypass high-sec and low-sec ceilings.
    if (AllowOfficer(-0.6) || !AllowOfficer(-0.8)
        || AllowsShipClass(0.0, 9, 12) || !AllowsShipClass(-0.8, 9, 12)
        || AllowsShipClass(0.4, 10, 11) || !AllowsShipClass(0.3, 10, 11)
        || AllowsShipClass(0.3, 14, 11) || !AllowsShipClass(-0.5, 14, 11)
        || AllowsShipClass(0.5, 8, 10) || !AllowsShipClass(0.0, 8, 10))
        return EXIT_FAILURE;

    if (SecurityTenths(std::numeric_limits<double>::quiet_NaN()) != 10
        || SecurityTenths(std::numeric_limits<double>::infinity()) != 10)
        return EXIT_FAILURE;

    std::printf("Belt NPC security, hull, rare-spawn and fleet limits passed.\n");
    return EXIT_SUCCESS;
}
