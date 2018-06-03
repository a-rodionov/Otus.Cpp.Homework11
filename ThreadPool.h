#pragma once

#include <iostream>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <list>
#include <queue>

template<typename MessageType, typename ThreadHandler>
class ThreadPool {

public:

  ThreadPool()
    : done{false} {}

  ~ThreadPool() {
    JoinWorkers();
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  template<typename ... Args>
  void AddWorker(Args&& ... args) {
    std::lock_guard<std::mutex> lk(threads_mutex);
    thread_handlers.push_back(std::make_shared<ThreadHandler>(std::forward<Args>(args)...));
    try {
      threads.push_back(std::thread(&ThreadPool::WorkerThread, this, thread_handlers.back()));
    }
    catch(...) {
      thread_handlers.pop_back();
      throw;
    }
  }

  auto StopWorkers() {
    std::lock_guard<std::mutex> lk(threads_mutex);
    JoinWorkers();
    auto thread_handlers_copy{thread_handlers};
    thread_handlers.clear();
    threads.clear();
    done = false;
    return thread_handlers_copy;
  }

  auto WorkersCount() const {
    std::lock_guard<std::mutex> lk(threads_mutex);
    return threads.size();
  }

  void PushMessage(const MessageType& message) {
    std::lock_guard<std::mutex> lk(queue_mutex);
    messages.push(message);
    queue_event.notify_one();
  }

  void PushMessage(MessageType&& message) {
    std::lock_guard<std::mutex> lk(queue_mutex);
    messages.push(std::move(message));
    queue_event.notify_one();
  }

private:

  void JoinWorkers() {
    if(threads.empty()) {
      return;
    }
    done = true;
    queue_event.notify_all();
    for(auto& thread : threads) {
      if(thread.joinable()) {
        thread.join();
      }
    }
  }

  void WorkerThread(const std::shared_ptr<ThreadHandler>& thread_handler) {
    try {
      while(true) {
        std::unique_lock<std::mutex> lk(queue_mutex);
        queue_event.wait(lk, [&](){ return (!messages.empty() || done); });
        if(messages.empty()) {
          break;
        }
        auto message = messages.front();
        messages.pop();
        lk.unlock();
        (*thread_handler)(message);
      }
    }
    catch (const std::exception& e)
    {
      std::cerr << e.what() << std::endl;
    }
  }

  std::vector<std::thread> threads;
  std::list<std::shared_ptr<ThreadHandler>> thread_handlers;
  mutable std::mutex threads_mutex;
  std::atomic_bool done;

  std::queue<MessageType> messages;
  std::mutex queue_mutex;
  std::condition_variable queue_event;
};
