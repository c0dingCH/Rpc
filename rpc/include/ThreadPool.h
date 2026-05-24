#pragma once
#include "Common.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>

class ThreadPool {
public:
  ThreadPool()
    : thread_nums_(std::thread::hardware_concurrency() * 2) {}

  ~ThreadPool() { Stop(); }
  DISALLOW_COPY_AND_MOVE(ThreadPool);

  void SetThreadNums(int n) { thread_nums_ = n; }

  void Start() {
    for (int i = 0; i < thread_nums_; ++i) {
      threads_.emplace_back(&ThreadPool::Worker, this);
    }
  }

  void Stop() {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : threads_) {
      if (t.joinable()) t.join();
    }
  }

  template<typename F>
  void Push(F&& f) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      tasks_.emplace(std::forward<F>(f));
    }
    cv_.notify_one();
  }

private:
  void Worker() {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
        if (stop_ && tasks_.empty()) return;
        task = std::move(tasks_.front());
        tasks_.pop();
      }
      task();
    }
  }

  int thread_nums_;
  std::vector<std::thread> threads_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mtx_;
  std::condition_variable cv_;
  bool stop_{false};
};
