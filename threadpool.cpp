#include "threadpool.hpp"

ThreadPool::ThreadPool(size_t num_threads) : should_terminate(false) {
  Start(num_threads);
}

ThreadPool::~ThreadPool() { Stop(); }

void ThreadPool::Start(size_t num_threads) {
  for (size_t i = 0; i < num_threads; ++i) {
    threads.emplace_back(&ThreadPool::ThreadLoop, this);
  }
}

void ThreadPool::Stop() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    should_terminate = true;
  }

  mutex_condition.notify_all();

  for (auto &thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

void ThreadPool::Wait() {
  while (Busy()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

void ThreadPool::ThreadLoop() {
  while (true) {
    std::unique_lock<std::mutex> lock(queue_mutex);

    mutex_condition.wait(lock,
                         [this] { return should_terminate || !jobs.empty(); });

    // Thread exits when there are no more jobs and termination is
    // requested
    if (should_terminate && jobs.empty()) {
      return;
    }

    auto job = jobs.front();
    jobs.pop();

    lock.unlock();

    // Execute the job outside the lock
    job();
  }
}

bool ThreadPool::Busy() {
  bool poolbusy;
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    poolbusy = !jobs.empty();
  }
  return poolbusy;
}

class WebServer {
public:
  WebServer(ThreadPool &threadPool) : thread_pool(threadPool) {}

  void Start() {
    // In a real web server, you would have a loop here to accept and handle
    // incoming requests For simplicity, let's simulate processing a request
    // every second
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));

      // Enqueue a job for processing the request with arguments
      thread_pool.QueueJob([this] { HandleRequest(42, "example_argument"); });
    }
  }

private:
  void HandleRequest(int value, const std::string &argument) {
    // Simulate processing a request with arguments
    std::cout << "Processing request with value " << value << " and argument '"
              << argument << "' in thread " << std::this_thread::get_id()
              << std::endl;
  }

  ThreadPool &thread_pool;
};
