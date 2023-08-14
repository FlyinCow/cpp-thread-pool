#pragma once
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <fmt/core.h>
#include <functional>

#if __cplusplus >= 201703L
template <typename F, typename... Args>
using result_of_t = std::invoke_result_t<F, Args...>;
#else
template <typename F, typename... Args>
using result_of_t = typename std::result_of<F(Args...)>::type;
#endif

namespace tpool {

template <typename J>
class JobQueue {
 private:
  std::deque<J> jobs;
  std::mutex m;

 public:
  JobQueue(const JobQueue &) = delete;
  JobQueue(JobQueue &&) = default;
  JobQueue() = default;

 public:
  using size_type = typename std::deque<J>::size_type;

  template <typename T>
  void push(T &&job) {
    std::lock_guard<std::mutex> lock(m);
    jobs.emplace_back(std::forward<T>(job));
  }

  bool try_pop(J &tmp) {
    std::lock_guard<std::mutex> lock(m);
    if (jobs.empty()) {
      return false;
    }
    tmp = std::move(jobs.front());
    jobs.pop_front();
    return true;
  }

  size_type size() {
    std::lock_guard<std::mutex> lock(m);
    return jobs.size();
  }

  bool empty() {
    std::lock_guard<std::mutex> lock(m);
    return jobs.empty();
  }
};

class Worker {
 private:
  std::thread t;
  std::mutex m;
  std::condition_variable job_done_cv;
  bool destructing = false;
  JobQueue<std::function<void()>> jobs;

  void worker_loop() {
    std::function<void()> current_job;
    while (true) {  // todo: 避免忙等
      if (jobs.try_pop(current_job)) {
        current_job();
      } else {
        {
          std::lock_guard<std::mutex> lock(m);
          if (destructing) {
            job_done_cv.notify_one();
            return;
          }
        }
        std::this_thread::yield();
      }
    }
  }

 public:
  Worker(const Worker &) = delete;
  Worker(Worker &&) = delete;

  explicit Worker() : t(&Worker::worker_loop, this) {}

  // todo: use wait_for to wait for all task done
  // void sync(int timeout = -1) {}

  ~Worker() {
    {
      std::unique_lock<std::mutex> lock(m);
      destructing = true;
      job_done_cv.wait(lock);
    }
    if (t.joinable()) {
      t.join();
    }
  }

  template <typename F, typename R = result_of_t<F>,
            typename DR = typename std::enable_if<std::is_void<R>::value>::type>
  void add_job(F &&job) {
    jobs.push([job] {
      try {
        job();
      } catch (std::exception &e) {
        // todo: log
      }
    });
  }

  template <
      typename F, typename R = result_of_t<F>,
      typename DR = typename std::enable_if<!std::is_void<R>::value, R>::type>
  auto add_job(F &&job) -> std::future<R> {
    auto promise = std::make_shared<std::promise<R>>();
    auto ret_future = promise->get_future();

    jobs.push([job, promise] {
      try {
        promise->set_value(job());
      } catch (std::exception &e) {
        // todo: exception
      }
    });
    return ret_future;
  }
};

// class SyncBlock {
//  private:
//   Worker &worker;
//   int timeout;

//  public:
//   SyncBlock(Worker &Worker, int timeout = -1)
//       : worker(worker), timeout(timeout) {}
//   SyncBlock(const SyncBlock &) = delete;
//   SyncBlock(SyncBlock &&) = delete;
//   ~SyncBlock() { worker.sync(timeout); }
// };

}  // namespace tpool
