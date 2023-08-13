#pragma once
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <functional>

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
    std::lock_guard lock(m);
    jobs.emplace_back(std::forward<T>(job));
  }

  bool try_pop(J &tmp) {
    std::lock_guard lock(m);
    if (jobs.empty()) {
      return false
    };
    tmp = std::move(jobs.front());
    jobs.pop_front();
    return true;
  }

  size_type size() {
    std::lock_guard lock(m);
    return jobs.size();
  }

  bool empty() {
    std::lock_guard lock(m);
    return jobs.empty();
  }
};

class Worker {
 private:
  std::thread t;
  JobQueue<std::function<void()>> jobs;

  void worker_loop() {
    std::function<void()> current_job;
    while (true) {
      if (jobs.try_pop(current_job)) {
        current_job();
      } else {
        std::this_thread::yield();
      }
    }
  }

 public:
  Worker(const Worker &) = delete;
  Worker(Worker &&) = delete;

  explicit Worker() : t(&Worker::worker_loop, this) {}
  template <typename F>
  void add_job(F &&job) {
    jobs.push([job] {
      try {
        job();
      } catch (std::exception &e) {
      }
    });
  }
};

}  // namespace tpool
