// SPDX-FileCopyrightText: The halogenOS Project
// SPDX-License-Identifier: Apache-2.0

// +build ignore

#include "thread_pool.h"

#include <system_error>

#include "log.h"

ThreadPool::ThreadPool(int count, std::function<void()> on_start) {
  for (int i = 0; i < count; i++) {
    try {
      threads_.emplace_back([this, on_start] {
        on_start();
        Work();
      });
    } catch (const std::system_error& e) {
      if (threads_.empty()) {
        ERROR("cannot start worker threads: %s", e.what());
      } else {
        WARN("started %zu of %d worker threads: %s", threads_.size(), count,
             e.what());
      }
      break;
    }
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    stopping_ = true;
  }
  task_cv_.notify_all();
  for (std::thread& thread : threads_)
    thread.join();
}

void ThreadPool::Queue(std::packaged_task<void()> task) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    tasks_.push_back(std::move(task));
  }
  task_cv_.notify_one();
}

bool ThreadPool::HasQueuedTasks() {
  std::lock_guard<std::mutex> lock(mu_);
  return !tasks_.empty();
}

void ThreadPool::Work() {
  std::unique_lock<std::mutex> lock(mu_);
  while (true) {
    task_cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
    if (stopping_)
      return;
    std::packaged_task<void()> task = std::move(tasks_.front());
    tasks_.pop_front();
    lock.unlock();
    task();
    lock.lock();
  }
}
