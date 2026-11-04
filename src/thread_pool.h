// SPDX-FileCopyrightText: The halogenOS Project
// SPDX-License-Identifier: Apache-2.0

#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

// Threads that run queued tasks in the order they were queued.
class ThreadPool {
 public:
  // Starts up to |count| threads, each of which calls |on_start| before it
  // runs its first task.
  ThreadPool(int count, std::function<void()> on_start);

  // Drops the queued tasks and joins the threads once the tasks already
  // running finish.
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  // Queues |task|. The future is ready with what |task| returns once a thread
  // has run it.
  template <typename Task>
  std::future<std::invoke_result_t<Task>> Run(Task task) {
    std::packaged_task<std::invoke_result_t<Task>()> run(std::move(task));
    std::future<std::invoke_result_t<Task>> done = run.get_future();
    Queue(std::packaged_task<void()>(std::move(run)));
    return done;
  }

  // The number of threads started.
  size_t ThreadCount() const { return threads_.size(); }

  // Whether a task waits for a thread to take it.
  bool HasQueuedTasks();

 private:
  void Queue(std::packaged_task<void()> task);
  void Work();

  std::mutex mu_;
  std::condition_variable task_cv_;
  std::deque<std::packaged_task<void()>> tasks_;
  bool stopping_ = false;
  std::vector<std::thread> threads_;
};

#endif  // THREAD_POOL_H_
