#include <vector>
#include <thread>
#include <future>
#include <queue>

class ThreadPool {
  // 任务类型，把外界投递的任务都包装成此类型
  using Task = std::packaged_task<void()>;

private:
  explicit ThreadPool(unsigned int size = std::thread::hardware_concurrency()) {
    if (size < 1) {
      _poolSize = 2;
    } else {
      _poolSize = size;
    }
    _threads.reserve(_poolSize);

    for (unsigned int i = 0; i < _poolSize; i++) {
      _threads.emplace_back([this]() -> void {
        // 每个线程循环处理任务
        while (true) {
          Task task;
          {
            std::unique_lock<std::mutex> lock{_mtx};
            _cv.wait(lock, [this]() -> bool {
              return _stop.load(std::memory_order_acquire) || !_tasks.empty();
            });// 等待条件变量被唤醒，若任务为空且线程池未被关闭则继续等待

            if (_stop.load(std::memory_order_acquire) && _tasks.empty()) {
              break;
            }// 唤醒后，若线程池被关闭且任务为空则退出

            task = std::move(_tasks.front());
            _tasks.pop();
          }// lock的作用域结束并释放
          task();
        }
      });
    }
  }

  ~ThreadPool() {
    _stop.store(true, std::memory_order_release);
    _cv.notify_all();
  }

public:
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  static ThreadPool& getInstance() {
    static ThreadPool instance;
    return instance;
  }

  template <typename Func, typename... Args>
  auto commit(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>> {
    using ReturnType = std::invoke_result_t<Func, Args...>;

    if (_stop.load(std::memory_order_relaxed)) {
      throw std::runtime_error("ThreadPool is stopped");
    }

    auto task = std::packaged_task<ReturnType()>(
      [func = std::forward<Func>(func), args...]() mutable -> ReturnType {
        return func(args...);
      });

    auto ret = task.get_future(); // 先把返回的future拿出去
    {
      std::lock_guard<std::mutex> lock{_mtx};
      _tasks.emplace([task = std::move(task)]() mutable -> void {
        task();
      }); // 再包装成返回void的callable放进线程池执行
    }
    _cv.notify_one();
    return ret;
  }

private:
  std::mutex _mtx;
  std::condition_variable _cv;

  std::queue<Task> _tasks;
  std::vector<std::jthread> _threads;

  std::atomic_bool _stop{false};
  size_t _poolSize{0};
};