#include "market/MarketOrderReader.h"

#include <atomic>
#include <cstdio>

namespace {
struct FixtureConnection {
    struct Rows {
        OrderKey key;
        std::thread::id worker;
    };
    struct Settings {
        std::atomic<unsigned>& started;
        std::shared_future<void> release;
    };

    explicit FixtureConnection(const Settings& settings) : settings(settings) {}

    std::unique_ptr<Rows> Fetch(const OrderKey& key)
    {
        ++settings.started;
        if (settings.release.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
            return nullptr;
        return std::make_unique<Rows>(Rows{key, std::this_thread::get_id()});
    }

    const Settings& settings;
};
}

int utils_MarketOrderReaderTest(int, char**)
{
    using Reader = MarketOrderReader<FixtureConnection>;
    std::atomic<unsigned> started{0};
    std::promise<void> release;
    Reader reader(2, {started, release.get_future().share()});

    const OrderKey firstKey{10000001, 70001};
    const OrderKey otherRegion{10000002, firstKey.second};
    auto first = reader.Submit(firstKey);
    auto duplicate = reader.Submit(firstKey);
    auto second = reader.Submit(otherRegion);
    auto deadline = OrderClock::now() + std::chrono::seconds(3);
    while (started.load() < 2 && OrderClock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (started.load() != 2 || !first || first != duplicate || !second || first == second) {
        release.set_value();
        std::fputs("Market reads failed to run concurrently or coalesce by key.\n", stderr);
        return 1;
    }

    reader.Invalidate(firstKey);
    auto refresh = reader.Submit(firstKey);
    std::vector<std::shared_ptr<Reader::Read>> queued;
    for (std::uint32_t i = 0; i < 127; ++i)
        queued.push_back(reader.Submit({firstKey.first, 80000 + i}));
    const bool bounded = !reader.Submit({firstKey.first, 90000});
    release.set_value();

    if (!first->invalidated || !refresh || refresh == first || !bounded)
        return 1;
    for (const auto& read : queued)
        if (!read)
            return 1;

    auto firstRows = first->rows.get();
    auto secondRows = second->rows.get();
    auto refreshedRows = refresh->rows.get();
    if (!firstRows || !secondRows || !refreshedRows ||
        firstRows->worker == secondRows->worker ||
        firstRows->key != firstKey || secondRows->key != otherRegion || refreshedRows->key != firstKey)
        return 1;

    return 0;
}
