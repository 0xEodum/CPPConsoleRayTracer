#include "core/ThreadPool.hpp"

#include <algorithm>

namespace crt {

ThreadPool::ThreadPool(std::size_t threadCount) {
    if (threadCount == 0) threadCount = std::max(1u, std::thread::hardware_concurrency());
    workers_.reserve(threadCount - 1);
    for (std::size_t i = 1; i < threadCount; ++i) workers_.emplace_back([this, i] { workerLoop(i); });
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    for (auto& t : workers_) t.join();
}

void ThreadPool::parallelFor(std::size_t count, const Job& job) {
    if (count == 0) return;
    if (workers_.empty() || count == 1) {
        for (std::size_t i = 0; i < count; ++i) job(i, 0);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        job_ = &job;
        jobCount_ = count;
        nextJob_.store(0, std::memory_order_relaxed);
        busyWorkers_ = workers_.size();
        ++generation_;
    }
    wake_.notify_all();

    drain(0);

    std::unique_lock<std::mutex> lock(mutex_);
    done_.wait(lock, [this] { return busyWorkers_ == 0; });
    job_ = nullptr;
}

void ThreadPool::drain(std::size_t workerIndex) {
    const Job& job = *job_;
    for (;;) {
        const std::size_t i = nextJob_.fetch_add(1, std::memory_order_relaxed);
        if (i >= jobCount_) break;
        job(i, workerIndex);
    }
}

void ThreadPool::workerLoop(std::size_t workerIndex) {
    std::uint64_t seenGeneration = 0;
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [&] { return stopping_ || generation_ != seenGeneration; });
            if (stopping_) return;
            seenGeneration = generation_;
        }
        drain(workerIndex);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (--busyWorkers_ == 0) done_.notify_one();
        }
    }
}

}  // namespace crt
