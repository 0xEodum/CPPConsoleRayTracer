#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace crt {

/// Fixed-size pool of worker threads specialised for data-parallel loops.
///
/// parallelFor() hands out job indices through an atomic counter (dynamic scheduling, so uneven
/// rows/tiles balance themselves) and the calling thread participates as worker 0. Each job also
/// receives the index of the worker running it, which lets callers keep per-worker scratch data
/// (accumulation buffers, RNGs) without any locking.
class ThreadPool {
public:
    using Job = std::function<void(std::size_t job, std::size_t worker)>;

    /// threadCount == 0 selects std::thread::hardware_concurrency().
    explicit ThreadPool(std::size_t threadCount = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /// Number of workers, including the calling thread.
    std::size_t size() const { return workers_.size() + 1; }

    /// Runs job(i, worker) for every i in [0, count) and blocks until all of them finished.
    /// Not re-entrant: must not be called from inside a job.
    void parallelFor(std::size_t count, const Job& job);

private:
    void workerLoop(std::size_t workerIndex);
    void drain(std::size_t workerIndex);

    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::condition_variable done_;

    const Job* job_ = nullptr;
    std::size_t jobCount_ = 0;
    std::atomic<std::size_t> nextJob_{0};
    std::size_t busyWorkers_ = 0;
    std::uint64_t generation_ = 0;
    bool stopping_ = false;
};

}  // namespace crt
