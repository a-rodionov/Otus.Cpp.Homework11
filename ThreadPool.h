#pragma once

#include <iostream>
#include <vector>
#include <queue>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>

class ThreadPool {

public:

  ThreadPool() = default;

  ~ThreadPool() {
    JoinWorkers();
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  auto AddWorker() {
    // Одновременно разрешается исполнение лишь одного метода AddWorker 
    std::lock_guard<std::mutex> lk(threads_mutex);
    // Если обрабатывающих потоков еще нет, то создается io_service::work,
    // чтобы обрабатывающий поток при запуске io_service.run() не завершался
    // при остутствии задач в очереди.
    if(threads.empty()) {
      work = std::make_shared<boost::asio::io_service::work>(io_service);
    }

    is_new_thread_on_pause = true;
    is_new_thread_force_terminate = false;
    is_new_thread_started = false;

    // При запуске обрабатывающий поток ожидает сброса флага is_new_thread_on_pause для
    // продолжения работы. Данное разрешение вместе с флагом is_new_thread_force_terminate
    // устанавливает основной поток. Данная процедура позволяет основному потоку сразу завершить
    // обрабатывающий при возникновении исключений. Как только обрабатывающий поток успешно
    // продолжит работу, он установит флаг is_new_thread_started. Основной поток ожидает его
    // установки, что гарантирует, что при выходе из метода AddWorker будет работать еще один
    // обрабатывающий поток. Таким образом, вызов метода AddWorker является блокирующим и
    // гарантирующем, что обрабатывающий поток будет создан и работать или никакие изменения
    // не будут произведены и сгенерируется исключение.
    auto worker_thread = std::thread([this] () {
      while(is_new_thread_on_pause)
        std::this_thread::yield();
      if(is_new_thread_force_terminate)
        return;
      is_new_thread_started = true;

      for(;;) {
        try {
          io_service.run();
          break;
        }
        catch (std::exception& exc) {
          BOOST_LOG_TRIVIAL(error) << exc.what();
          std::lock_guard<std::mutex> lk(exceptions_mutex);
          exceptions.push(std::current_exception());
        }
      }
    });

    try {
      threads.push_back(std::move(worker_thread));
      is_new_thread_on_pause = false;
      while(!is_new_thread_started)
        std::this_thread::yield();
    }
    catch(std::exception& exc) {
      if(threads.empty()) {
        work.reset();
      }
      is_new_thread_force_terminate = true;
      is_new_thread_on_pause = false;
      if(worker_thread.joinable()) {
        worker_thread.join();
      }
      throw exc;
    }
    return threads.back().get_id();
  }

  void StopWorkers() {
    std::lock_guard<std::mutex> lk(threads_mutex);
    JoinWorkers();
    threads.clear();
    io_service.reset();
  }

  auto WorkersCount() const {
    std::lock_guard<std::mutex> lk(threads_mutex);
    return threads.size();
  }

  template<typename Task>
  void AddTask(Task&& task) {
    io_service.post(task);
  }

  std::exception_ptr GetLastException() {
    std::exception_ptr exc;
    std::lock_guard<std::mutex> lk(exceptions_mutex);
    if (!exceptions.empty()) {
      exc = exceptions.front();
      exceptions.pop();
    }
    return exc;
  }

private:

  void JoinWorkers() {
    if(threads.empty()) {
      return;
    }

    work.reset();
    for(auto& thread : threads) {
      if(thread.joinable()) {
        thread.join();
      }
    }
  }

  boost::asio::io_service io_service;
  std::shared_ptr<boost::asio::io_service::work> work;
  std::vector<std::thread> threads;
  mutable std::mutex threads_mutex;

  std::queue<std::exception_ptr> exceptions;
  std::mutex exceptions_mutex;

  std::atomic_bool is_new_thread_on_pause;
  std::atomic_bool is_new_thread_force_terminate;
  std::atomic_bool is_new_thread_started;
};
