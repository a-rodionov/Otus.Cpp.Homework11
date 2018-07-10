#include <atomic>
#include <algorithm>
#include "async.h"
#include "ContextManager.h"
#include <unistd.h>

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

struct SingleContextTestSet {
  std::string input;
  std::thread thread;
};

void CallAllFunctionsInSingleThread(
        SingleContextTestSet& test_set,
        std::atomic_bool& is_thread_ready,
        std::atomic_bool& start_thread) {

  smart_handle handle(connect(3), close_handle);

  is_thread_ready = true;

  //spin lock
  while(!start_thread) {}

  for(const auto& symbol : test_set.input) {
    receive(handle.get(), &symbol, 1);
  }
}

BOOST_AUTO_TEST_CASE(single_context_in_thread)
{
  std::array<SingleContextTestSet, 3> test_sets;
  test_sets[0].input = {"cmd11\n"
                        "cmd12\n"
                        "cmd13\n"
                        "cmd14\n"
                        "cmd15\n"};

  test_sets[1].input = {"cmd21\n"
                        "{\n"
                        "cmd22\n"
                        "cmd23\n"
                        "}\n"
                        "cmd24\n"
                        "cmd25\n"};

  test_sets[2].input = {"cmd31\n"
                        "cmd32\n"
                        "{\n"
                        "cmd33\n"
                        "cmd34\n"
                        "cmd35\n"};

  std::vector<std::string> ethalon {
    {"bulk: cmd11, cmd12, cmd13"},
    {"bulk: cmd14, cmd15"},
    {"bulk: cmd21"},
    {"bulk: cmd22, cmd23"},
    {"bulk: cmd24, cmd25"},
    {"bulk: cmd31, cmd32"},
  };

  std::vector<std::string> result;
  result.reserve(ethalon.size());

  auto oss = std::make_shared<std::ostringstream>();
  ContextManager::Instance().SetDefaultOstream(oss);

  std::atomic_bool start_threads{false};
  std::atomic_bool is_thread_ready{false};

  for(auto& test_set : test_sets) {
    is_thread_ready = false;
    test_set.thread = std::thread(CallAllFunctionsInSingleThread,
                                  std::ref(test_set),
                                  std::ref(is_thread_ready),
                                  std::ref(start_threads));
    //spin lock
    while(!is_thread_ready) {}
  }

  // Запуск одновременной работы потоков
  start_threads = true;

  for(auto& test_set : test_sets) {
    if(test_set.thread.joinable()) {
      test_set.thread.join();
    }
  }
  std::this_thread::sleep_for(200ms);

  size_t prev = 0, pos = 0;
  auto result_str = oss->str();
  do
  {
    pos = result_str.find('\n', prev);
    if (std::string::npos == pos)
      break;
    result.push_back(result_str.substr(prev, pos-prev));
    prev = pos + 1;
  }
  while (pos < result_str.length() && prev < result_str.length());

  std::sort(std::begin(ethalon), std::end(ethalon));
  std::sort(std::begin(result), std::end(result));
  BOOST_REQUIRE_EQUAL_COLLECTIONS(std::cbegin(ethalon),
                                  std::cend(ethalon),
                                  std::cbegin(result),
                                  std::cend(result));
}


struct MultipleContextsTestSet {
  //std::ostringstream output;
  std::string input;
  //std::string result;
  std::thread thread;
  handle_t handle;
  std::atomic_size_t position;
  size_t max_position;
};

template<size_t N>
void CallAllFunctionsInMultipleThreads(
        std::array<MultipleContextsTestSet, N>& test_sets,
        size_t index,
        std::atomic_bool& start_thread) {

  //spin lock
  while(!start_thread) {}

  // Запись по 1 байту в каждый контекст
  // Каждый байт тестовой строки будет записан в соответствующий
  // контекст лишь один раз. Чтобы по какой-то причине
  // более поздние байты не были записаны другим потоком раньше
  // предыдущих, введен контроль позиции с помощью atomic
  for(size_t position{0}; position <= test_sets[index].max_position; ++position) {
    decltype(index) real_index = (index + position) % N;
    if(position < test_sets[real_index].input.size()) {
      while((position) != test_sets[real_index].position) {
        std::this_thread::yield();
      }
      receive(test_sets[real_index].handle, &test_sets[real_index].input.at(position), 1);
      ++test_sets[real_index].position;
    }
    else if (position == test_sets[real_index].input.size()) {
      while(test_sets[real_index].input.size() != test_sets[real_index].position) {
        std::this_thread::yield();
      }
      close_handle(test_sets[real_index].handle);
    }
  }
}

BOOST_AUTO_TEST_CASE(multiple_contexts_in_thread)
{
  std::array<MultipleContextsTestSet, 3> test_sets;
  test_sets[0].input = {"cmd11\n"
                        "cmd12\n"
                        "cmd13\n"
                        "cmd14\n"
                        "cmd15\n"};
  test_sets[0].position = 0;

  test_sets[1].input = {"cmd21\n"
                        "{\n"
                        "cmd22\n"
                        "cmd23\n"
                        "}\n"
                        "cmd24\n"
                        "cmd25\n"};
  test_sets[1].position = 0;

  test_sets[2].input = {"cmd31\n"
                        "cmd32\n"
                        "{\n"
                        "cmd33\n"
                        "cmd34\n"
                        "cmd35\n"};
  test_sets[2].position = 0;

  std::vector<std::string> ethalon {
    {"bulk: cmd11, cmd12, cmd13"},
    {"bulk: cmd14, cmd15"},
    {"bulk: cmd21"},
    {"bulk: cmd22, cmd23"},
    {"bulk: cmd24, cmd25"},
    {"bulk: cmd31, cmd32"}
  };

  std::vector<std::string> result;
  result.reserve(ethalon.size());

  auto oss = std::make_shared<std::ostringstream>();
  ContextManager::Instance().SetDefaultOstream(oss);

  test_sets[0].max_position = test_sets[1].max_position = test_sets[2].max_position = 
    std::max_element(std::cbegin(test_sets),
                      std::cend(test_sets),
                      [] (const auto& lhs, const auto& rhs) {
                        return lhs.input.size() < rhs.input.size();
                      })->input.size();

  std::atomic_bool start_threads{false};

  // Все контексты будут открыты в главном потоке, а закрыты в отдельных потоках
  for(size_t i{0}; i < test_sets.size(); ++i) {
    test_sets[i].handle = connect(3);
    test_sets[i].thread = std::thread(CallAllFunctionsInMultipleThreads<test_sets.size()>,
                                      std::ref(test_sets),
                                      i,
                                      std::ref(start_threads));
  }

  // Запуск одновременной работы потоков
  start_threads = true;

  for(auto& test_set : test_sets) {
    if(test_set.thread.joinable()) {
      test_set.thread.join();
    }
  }
  std::this_thread::sleep_for(200ms);

  size_t prev = 0, pos = 0;
  auto result_str = oss->str();
  do
  {
    pos = result_str.find('\n', prev);
    if (std::string::npos == pos)
      break;
    result.push_back(result_str.substr(prev, pos-prev));
    prev = pos + 1;
  }
  while (pos < result_str.length() && prev < result_str.length());

  std::sort(std::begin(ethalon), std::end(ethalon));
  std::sort(std::begin(result), std::end(result));
  BOOST_REQUIRE_EQUAL_COLLECTIONS(std::cbegin(ethalon),
                                  std::cend(ethalon),
                                  std::cbegin(result),
                                  std::cend(result));
}



std::string GetCurrentWorkingDir( void ) {
  char buff[FILENAME_MAX];
  ssize_t len = sizeof(buff);
  int bytes = std::min(readlink("/proc/self/exe", buff, len), len - 1);
  if(bytes >= 0)
      buff[bytes] = '\0';
  std::string current_working_dir(buff);
  return current_working_dir.substr(0, current_working_dir.find_last_of('/'));
}

BOOST_AUTO_TEST_CASE(multithread_async_app)
{
  std::vector<std::string> ethalon {
    {"bulk: 0 0, , 1 1"},
    {"bulk: 11"},
    {"bulk: , 12, 13"},
    {"bulk: 12, 13, 14"},
    {"bulk: 14"},
    {"bulk: 15"},
    {"bulk: 16, 17, 18, 19, 20"},
    {"bulk: 21, 22, 23, , 12, 13, 14"},
    {"bulk: 2, 3, 4"},
    {"bulk: 5, 6, 7"},
    {"bulk: 8, 9, 10"}
  };
  std::vector<std::string> result;
  result.reserve(ethalon.size());

  auto path = GetCurrentWorkingDir();
  path += "/multithread_async_app";
  std::string cmd{"seq 1 200 | "};
  cmd += path;
  cmd += " > console.output";
  std::system(cmd.c_str());
  std::system("sort console.output | uniq > unique.output");

  std::ifstream ifs{"unique.output", std::ifstream::in};
  BOOST_REQUIRE_EQUAL(true, ifs.is_open());
  for(std::string line; getline(ifs, line);) {
    result.push_back(line);
  }
  ifs.close();

  BOOST_CHECK_EQUAL_COLLECTIONS(std::cbegin(ethalon),
                                std::cend(ethalon),
                                std::cbegin(result),
                                std::cend(result));
  std::system("rm -f *.log");
  std::system("rm -f console.output");
  std::system("rm -f unique.output");
}

BOOST_AUTO_TEST_CASE(remove_test_files)
{
  std::system("rm -f *.log");
}

BOOST_AUTO_TEST_SUITE_END()
