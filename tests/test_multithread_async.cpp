#include <atomic>
#include "async.h"
#include "ContextManager.h"

#define BOOST_TEST_MODULE test_multithread_async

#include <boost/test/unit_test.hpp>
#include <boost/test/included/unit_test.hpp>

using namespace async;
using namespace std::chrono_literals;


BOOST_AUTO_TEST_SUITE(test_suite_main)

auto close_handle = [] (handle_t handle) {
  disconnect(handle);
};

using smart_handle = std::unique_ptr<void, decltype(close_handle)>;

void Process(std::ostream& out,
            const std::string& input,
            std::atomic_bool& is_thread_ready,
            std::atomic_bool& start_thread) {
  ContextManager::Instance().SetDefaultOstream(out);
  smart_handle handle(connect(3), close_handle);

  is_thread_ready = true;

  //spin lock
  while(!start_thread) {}

  for(const auto& symbol : input) {
    receive(handle.get(), &symbol, 1);
  }
}

struct TestSet {
  std::ostringstream output;
  std::string input;
  std::string result;
  std::thread thread;
};

BOOST_AUTO_TEST_CASE(process_multiple_handles)
{
  std::array<TestSet, 3> test_sets;
  test_sets[0].input = {"cmd11\n"
                        "cmd12\n"
                        "cmd13\n"
                        "cmd14\n"
                        "cmd15\n"};

  test_sets[0].result = {"bulk: cmd11, cmd12, cmd13\n"
                         "bulk: cmd14, cmd15\n"};

  test_sets[1].input = {"cmd21\n"
                        "{\n"
                        "cmd22\n"
                        "cmd23\n"
                        "}\n"
                        "cmd24\n"
                        "cmd25\n"};

  test_sets[1].result = {"bulk: cmd21\n"
                         "bulk: cmd22, cmd23\n"
                         "bulk: cmd24, cmd25\n"};

  test_sets[2].input = {"cmd31\n"
                        "cmd32\n"
                        "{\n"
                        "cmd33\n"
                        "cmd34\n"
                        "cmd35\n"};

  test_sets[2].result = {"bulk: cmd31, cmd32\n"};

  std::atomic_bool start_threads{false};
  std::atomic_bool is_thread_ready{false};

  for(auto& test_set : test_sets) {
    is_thread_ready = false;
    test_set.thread = std::thread(Process,
                std::ref(test_set.output),
                std::ref(test_set.input),
                std::ref(is_thread_ready),
                std::ref(start_threads));
    //spin lock
    while(!is_thread_ready) {}
  }

  start_threads = true;

  for(auto& test_set : test_sets) {
    if(test_set.thread.joinable()) {
      test_set.thread.join();
    }
  }
  std::this_thread::sleep_for(200ms);

  for(const auto& test_set : test_sets) {
    BOOST_REQUIRE_EQUAL(test_set.output.str(), test_set.result);
  }

}

BOOST_AUTO_TEST_SUITE_END()
