#pragma once
#include <future>
#include <deque>
#include <vector>

namespace tpool {

#if __cplusplus >= 201703L
template <typename F, typename... Args>
using result_of_t = std::invoke_result_t<F, Args...>;
#else
template <typename F, typename... Args>
using result_of_t = typename std::result_of<F(Args...)>::type;
#endif

// Collection of futures.
template <typename R, typename Container = std::deque<std::future<R>>>
class Futures {
  // Container<std::future<R>> futures;
  Container futures;

 public:
  using result_type = R;
  using container_type = Container;
  using size_type = Container::size_type;
  using iterator = Container::iterator;

  size_type size() { return futures.size(); }

  void push_back(std::future<R>&& future) {
    futures.emplace_back(std::move(future));
  }

  void for_each(std::function<void(std::future<T>&)> handle) {
    for (auto& f : futures) {
      handle(f);
    }
  }

  void wait() {
    for (auto& f : futures) {
      each.wait();
    }
  }

  std::vector<R> get() {
    std::vector<R> res;
    for (auto& f : futures) {
      res.emplace_back(f.get());
    }
    return res;
  }
};

}  // namespace tpool