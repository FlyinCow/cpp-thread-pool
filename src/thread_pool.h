#pragma once
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <map>
#include <memory>
#include <fmt/core.h>
#include <functional>
#include "utils.h"

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
    while (true) {                      // todo: 避免忙等
      if (jobs.try_pop(current_job)) {  // still have jobs to do
        current_job();
      } else if (destructing) {  // ~Worker() is called
        {
          std::lock_guard<std::mutex> lock(m);
          if (destructing) {  // double check lock
            job_done_cv.notify_one();
            return;
          }
        }
      } else {  // no job to to
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

  using id = std::thread::id;
  id get_id() { return t.get_id(); }

  /**
   * @brief Add a job withou a return value.
   *
   * @tparam F Funcion type
   * @param job Job to add. Should be of type `void()`.
   */
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

  /**
   * @brief Add a job with a return value. The value returned will be wrapped in
   * a `std::future<R>`.
   *
   * @tparam F Function type.
   * @tparam R Return value type.
   * @param job Job to add. Should be of type `R()`.
   * @return std::future<R>
   */
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

class TPool {
 private:
  unsigned int worker_count;
  std::vector<std::shared_ptr<Worker>> workers;
  std::shared_ptr<Worker> current_worker;
  size_t current_pos = 0;

  std::shared_ptr<Worker> get_best_worker() {
    auto ret = workers[current_pos];
    current_pos = (current_pos + 1) % worker_count;
    return ret;
  }

 public:
  TPool(const TPool &) = delete;
  TPool(TPool &&) = delete;

  explicit TPool(int worker_count = 1) : worker_count(worker_count) {
    for (int i = 0; i < std::max(worker_count, 1); i++) {  // at least 1 worker
      auto wp = std::make_shared<Worker>();
      workers.emplace_back(wp);
    }
    current_worker = workers[0];  // so `current_worker != nullptr`.
  }

  template <
      typename F, typename R = result_of_t<F>,
      typename DR = typename std::enable_if<std::is_void<R>::value, R>::type>
  void submit(F &&job) {
    get_best_worker()->add_job(std::forward<F>(job));
  }

  template <
      typename F, typename R = result_of_t<F>,
      typename DR = typename std::enable_if<!std::is_void<R>::value, R>::type>
  std::future<R> submit(F &&job) {
    return get_best_worker()->add_job(std::forward<F>(job));
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
