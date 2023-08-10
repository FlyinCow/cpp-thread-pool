#pragma once
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <functional>

namespace tpool {

template <typename J> class JobQueue {
private:
  std::deque<J> jobs;
  std::mutex m;

public:
  JobQueue(const JobQueue &) = delete;
  JobQueue(JobQueue &&) = default;
  JobQueue() = default;

public:
  using size_type = typename std::deque<J>::size_type;

  template <typename T> void push(T &&job) {
    std::lock_guard lock(m);
    jobs.emplace_back(std::forward<T>(job));
  }

  bool try_pop(J &tmp) {
    std::lock_guard lock(m);
    if (jobs.empty())
      return false;
    tmp = std::move(jobs.front());
    jobs.pop_front();
    return true;
  }

  size_type size() {
    std::lock_guard lock(m);
    return jobs.size();
  }
};
} // namespace tpool
