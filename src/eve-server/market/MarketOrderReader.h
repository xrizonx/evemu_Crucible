#ifndef EVEMU_MARKET_ORDER_READER_H
#define EVEMU_MARKET_ORDER_READER_H

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

using OrderKey = std::pair<std::uint32_t, std::uint32_t>;
using OrderClock = std::chrono::steady_clock;

// Submit and Invalidate belong to the game thread. Each worker owns one
// connection and returns results through futures without accessing clients.
template <typename Connection>
class MarketOrderReader
{
public:
    using Rows = typename Connection::Rows;
    using Settings = typename Connection::Settings;

    struct Read {
        const OrderClock::time_point expires = OrderClock::now() + std::chrono::seconds(15);
        std::promise<std::unique_ptr<Rows>> result;
        std::future<std::unique_ptr<Rows>> rows = result.get_future();
        bool invalidated = false; // accessed only by the game thread
    };

    explicit MarketOrderReader(unsigned count, const Settings& settings) : m_settings(settings)
    {
        try {
            for (unsigned i = 0; i < count; ++i)
                m_workers.emplace_back(&MarketOrderReader::Run, this);
        } catch (...) {
            Stop();
            throw;
        }
    }

    ~MarketOrderReader() { Stop(); }

    std::shared_ptr<Read> Submit(const OrderKey& key)
    {
        // Only the game thread accesses m_reads. Keep it bounded when clients
        // disconnect or rapidly browse through different items.
        for (auto it = m_reads.begin(); it != m_reads.end();) {
            if (it->second.expired())
                it = m_reads.erase(it);
            else
                ++it;
        }
        if (auto pos = m_reads.find(key); pos != m_reads.end()) {
            auto pending = pos->second.lock();
            if (pending && OrderClock::now() < pending->expires)
                return pending;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_jobs.size() >= 128)
            return {};
        auto read = std::make_shared<Read>();
        m_reads[key] = read;
        m_jobs.emplace_back(key, read);
        m_ready.notify_one();
        return read;
    }

    void Invalidate(const OrderKey& key)
    {
        auto pos = m_reads.find(key);
        if (pos == m_reads.end())
            return;
        if (auto read = pos->second.lock())
            read->invalidated = true;
        m_reads.erase(pos);
    }

private:
    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
            // Complete queued futures even if their callers still hold them.
            for (auto& job : m_jobs)
                job.second->result.set_value(nullptr);
            m_jobs.clear();
        }
        m_ready.notify_all();
        for (auto& worker : m_workers)
            worker.join();
    }

    void Run()
    {
        Connection connection(m_settings);
        while (true) {
            std::pair<OrderKey, std::shared_ptr<Read>> job;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_ready.wait(lock, [this] { return m_stopping || !m_jobs.empty(); });
                if (m_stopping)
                    break;
                job = std::move(m_jobs.front());
                m_jobs.pop_front();
            }

            std::unique_ptr<Rows> rows;
            if (job.second.use_count() > 1 && OrderClock::now() < job.second->expires)
                rows = connection.Fetch(job.first);
            job.second->result.set_value(std::move(rows));
        }
    }

    const Settings m_settings;
    std::map<OrderKey, std::weak_ptr<Read>> m_reads;
    std::mutex m_mutex;
    std::condition_variable m_ready;
    std::deque<std::pair<OrderKey, std::shared_ptr<Read>>> m_jobs;
    std::vector<std::thread> m_workers;
    bool m_stopping = false;
};

#endif
