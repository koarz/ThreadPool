#pragma once
#include <atomic>
#include <condition_variable>
#include <format>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <sys/types.h>
#include <thread>
#include <vector>

namespace koarz {
class NoneCopy {
public:
  ~NoneCopy() {}

protected:
  NoneCopy() {}

private:
  NoneCopy(const NoneCopy &) = delete;
  NoneCopy &operator=(const NoneCopy &) = delete;
};

class ThreadPool : public NoneCopy {
  using Task = std::packaged_task<void()>;

public:
  static ThreadPool &instance() {
    static ThreadPool ins;
    return ins;
  }

  ThreadPool(unsigned int num = std::thread::hardware_concurrency())
      : stop_(false) {
    if (num <= 1)
      thread_num_ = 2;
    else
      thread_num_ = num;
    start();
  }

  void start() {
    for (int i = 0; i < thread_num_; ++i) {
      pool_.emplace_back([this]() {
        while (!this->stop_.load()) {
          Task task;
          {
            std::unique_lock<std::mutex> cv_mt(cv_mt_);
            this->cv_lock_.wait(cv_mt, [this] {
              return this->stop_.load() || !this->tasks_.empty();
            });
            if (this->tasks_.empty())
              return;
            while (this->thread_num_ == 0)
              ;
            task = std::move(this->tasks_.front());
            this->tasks_.pop();
          }
          // std::cerr << std::format(
          //     "Thread {} Get a task free thread num is {}\n",
          //     __gthread_self(), thread_num_.load());
          this->thread_num_--;
          task();
          this->thread_num_++;
          // std::cerr << std::format("Task Done and thread num is {}\n",
          //                          this->thread_num_.load());
        }
      });
    }
  }
  ~ThreadPool() { stop(); }

  void stop() {
    stop_.store(true);
    cv_lock_.notify_all();
    for (auto &td : pool_) {
      if (td.joinable()) {
        td.join();
      }
    }
  }
  template <class F, class... Args>
  auto commit(F &&f, Args &&...args) -> std::future<decltype(std::forward<F>(f)(
                                         std::forward<Args>(args)...))> {
    using RetType = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
    if (stop_.load())
      return std::future<RetType>{};
    auto task = std::make_shared<std::packaged_task<RetType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<RetType> ret = task->get_future();
    {
      std::lock_guard<std::mutex> cv_mt(cv_mt_);
      tasks_.emplace([task] { (*task)(); });
    }
    // std::cerr << std::format(" Commit a task\n");
    cv_lock_.notify_one();
    return ret;
  }

private:
  std::atomic_int thread_num_;
  std::queue<Task> tasks_;
  std::vector<std::thread> pool_;
  std::atomic_bool stop_;
  std::mutex cv_mt_;
  std::condition_variable cv_lock_;
};
} // namespace koarz