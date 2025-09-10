#pragma once

#include <vector>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

class ThreadPool {
public:
    static ThreadPool& getInstance();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename Func, typename... Args>
    auto commit(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>> {
        using ReturnType = std::invoke_result_t<Func, Args...>;

        if (_stop.load(std::memory_order_relaxed)) {
            throw std::runtime_error("ThreadPool is stopped");
        }

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
        );

        auto ret = task->get_future();
        {
            std::lock_guard<std::mutex> lock{_mtx};
            _tasks.emplace([task]() { (*task)(); });
        }
        _cv.notify_one();
        return ret;
    }

private:
    using Task = std::packaged_task<void()>;

    explicit ThreadPool(unsigned int size = std::thread::hardware_concurrency());
    ~ThreadPool();

    std::mutex _mtx;
    std::condition_variable _cv;

    std::queue<Task> _tasks;
    std::vector<std::jthread> _threads;

    std::atomic_bool _stop{false};
    size_t _poolSize{0};
};