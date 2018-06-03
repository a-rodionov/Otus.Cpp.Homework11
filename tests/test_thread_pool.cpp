#include "ThreadPool.h"

#define BOOST_TEST_MODULE test_thread_pool

#include <boost/test/unit_test.hpp>
#include <boost/test/included/unit_test.hpp>

using namespace std::chrono_literals;


class string_concat_thread_worker
{
public:

  string_concat_thread_worker()
    : calls_count{0} {}

  explicit string_concat_thread_worker(const std::string& initial_data)
    : concatenated_str{initial_data}, calls_count{0} {}

  void operator()(const std::string& str) {
    if(0 == calls_count) {
      thread_id = std::this_thread::get_id();
    }
    concatenated_str += str;
    ++calls_count;
    std::this_thread::sleep_for(200ms);
  }

  auto GetConcatenatedString() const {
    return concatenated_str;
  }

  auto GetCallsCount() const {
    return calls_count;
  }

  auto GetThreadId() const {
    if(0 == calls_count)
      throw std::runtime_error("Worker hasn't been called yet.");
    return thread_id;
  }

private:

  std::string concatenated_str;
  std::size_t calls_count;
  std::thread::id thread_id;
};

class exception_thrower_thread_worker
{
public:

  void operator()(const std::string&) {
    throw std::runtime_error("Exception is thrown by exception_thrower_thread_worker::operator()");
  }
};

BOOST_AUTO_TEST_SUITE(test_suite_main)

BOOST_AUTO_TEST_CASE(adding_worker_threads)
{
  ThreadPool<std::string, string_concat_thread_worker> thread_pool;
  thread_pool.AddWorker();
  thread_pool.AddWorker();
  thread_pool.AddWorker();

  BOOST_REQUIRE_EQUAL(3, thread_pool.WorkersCount());

  auto thread_handlers = thread_pool.StopWorkers();
  BOOST_REQUIRE_EQUAL(3, thread_handlers.size());
}

BOOST_AUTO_TEST_CASE(verify_multithreaded_execution)
{
  ThreadPool<std::string, string_concat_thread_worker> thread_pool;
  thread_pool.PushMessage("1st part.");
  thread_pool.PushMessage("2nd part.");
  thread_pool.PushMessage("3rd part.");
  thread_pool.AddWorker();
  thread_pool.AddWorker();
  auto thread_handlers = thread_pool.StopWorkers();
  auto first_thread_handler = std::cbegin(thread_handlers);
  auto second_thread_handler = std::next(first_thread_handler);

  BOOST_REQUIRE((*first_thread_handler)->GetThreadId() != (*second_thread_handler)->GetThreadId());
  BOOST_REQUIRE(std::this_thread::get_id() != (*first_thread_handler)->GetThreadId());
  BOOST_REQUIRE(std::this_thread::get_id() != (*second_thread_handler)->GetThreadId());
}

BOOST_AUTO_TEST_CASE(pass_worker_initial_data)
{
  ThreadPool<std::string, string_concat_thread_worker> thread_pool;
  thread_pool.AddWorker("Worker's initial data.");
  thread_pool.PushMessage("1st part.");
  thread_pool.PushMessage("2nd part.");
  auto thread_handlers = thread_pool.StopWorkers();

  BOOST_REQUIRE_EQUAL(2, thread_handlers.front()->GetCallsCount());
  BOOST_REQUIRE_EQUAL("Worker's initial data.1st part.2nd part.", thread_handlers.front()->GetConcatenatedString());
}

BOOST_AUTO_TEST_CASE(pushing_messages_before_start)
{
  ThreadPool<std::string, string_concat_thread_worker> thread_pool;

  thread_pool.PushMessage("1st part.");

  thread_pool.AddWorker();  
  thread_pool.PushMessage("2nd part.");
  auto thread_handlers = thread_pool.StopWorkers();

  BOOST_REQUIRE_EQUAL(2, thread_handlers.front()->GetCallsCount());
  BOOST_REQUIRE_EQUAL("1st part.2nd part.", thread_handlers.front()->GetConcatenatedString());
}

BOOST_AUTO_TEST_CASE(pushing_messages_after_stop)
{
  ThreadPool<std::string, string_concat_thread_worker> thread_pool;
  thread_pool.AddWorker();
  thread_pool.PushMessage("1st part.");
  thread_pool.PushMessage("2nd part.");

  auto thread_handlers = thread_pool.StopWorkers();

  thread_pool.PushMessage("Data won't be processed.");

  BOOST_REQUIRE_EQUAL(2, thread_handlers.front()->GetCallsCount());
  BOOST_REQUIRE_EQUAL("1st part.2nd part.", thread_handlers.front()->GetConcatenatedString());
}

BOOST_AUTO_TEST_CASE(handle_exception_from_thread)
{
  ThreadPool<std::string, exception_thrower_thread_worker> thread_pool;
  thread_pool.PushMessage("1st part.");
  thread_pool.AddWorker();
  auto thread_handlers = thread_pool.StopWorkers();
}


BOOST_AUTO_TEST_SUITE_END()
