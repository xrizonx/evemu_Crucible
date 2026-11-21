#include "eve-test.h"
#include "utils/EvEMath.h"

int utils_SkillTrainingTest(int argc, char* argv[])
{
    const int64 minute = EvE::Time::Minute;
    const int64 start = 10 * minute;
    const double rate = EvEMath::Skill::PointsPerMinute(20, 20);
    const double boostedRate = EvEMath::Skill::PointsPerMinute(20, 21);

    // A +1 secondary implant adds half a skill point per minute.
    if (rate != 30.0 || boostedRate != 30.5)
        return EXIT_FAILURE;

    const int64 original = EvEMath::Skill::EndTime(100, 710, rate, start);
    const int64 boosted = EvEMath::Skill::EndTime(100, 710, boostedRate, start);
    if (boosted != start + 20 * minute || boosted >= original)
        return EXIT_FAILURE;

    // The remaining interval is recalculated from the earned SP checkpoint.
    const int64 checkpoint = start + 2 * minute;
    const int64 resumed = EvEMath::Skill::EndTime(161, 710, rate, checkpoint);
    if (resumed <= boosted ||
        EvEMath::Skill::StartTime(161, 710, rate, resumed) != checkpoint)
        return EXIT_FAILURE;

    if (EvEMath::Skill::EndTime(709, 710, rate, start) <= start ||
        EvEMath::Skill::EndTime(710, 710, rate, start) != 0)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
