#ifndef THREADPOOL_H
#define THREADPOOL_H

/**
 * @file threadpool.hpp
 * @author Dr. Abiira Nathan (nabiira2by2@gmail.com)
 * @brief A threadpool implementation in C++.
 * @version 0.1
 * @date 2023-11-14
 *
 * @copyright Copyright (c) 2023
 * References:
 * https://matgomes.com/thread-pools-cpp-with-queues/
 * https://stackoverflow.com/questions/15752659/thread-pooling-in-c11
 */

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
 public:
  ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
  ~ThreadPool();

  // Start the thread-pool with the given number of threads.
  void Start(size_t num_threads);

  // Queue a new job to the threadpool.
  template <typename Function, typename... Args>
  void QueueJob(Function &&job, Args &&...args) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    jobs.emplace(
        std::bind(std::forward<Function>(job), std::forward<Args>(args)...));
    lock.unlock();
    mutex_condition.notify_one();
  }

  // Stop processing jobs and stop all threads.
  void Stop();

  // Returns true if the pool still has jobs to be done.
  bool Busy();

  // Wait for all the jobs to finish
  void Wait();

 private:
  void ThreadLoop();

  bool should_terminate;   // Tells threads to stop looking for jobs
  std::mutex queue_mutex;  // Prevents data races to the job queue

  // Allows threads to wait on new jobs or termination
  std::condition_variable mutex_condition;
  std::vector<std::thread> threads;
  std::queue<std::function<void()>> jobs;
};

#endif /* THREADPOOL_H */
