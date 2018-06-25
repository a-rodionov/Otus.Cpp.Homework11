#include "async.h"
#include "ContextManager.h"

#define BOOST_TEST_MODULE test_async

#include <boost/test/unit_test.hpp>
#include <boost/test/included/unit_test.hpp>

using namespace async;
using namespace std::chrono_literals;


BOOST_AUTO_TEST_SUITE(test_suite_main)

auto close_handle = [] (handle_t handle) {
  disconnect(handle);
};

using smart_handle = std::unique_ptr<void, decltype(close_handle)>;

BOOST_AUTO_TEST_CASE(unique_handles)
{
  smart_handle handle_1(connect(3), close_handle);
  smart_handle handle_2{connect(3), close_handle};
  BOOST_REQUIRE(handle_1 != handle_2);
}

BOOST_AUTO_TEST_CASE(multiple_disconnects)
{
  smart_handle handle_1(connect(3), close_handle);
  disconnect(handle_1.get());
  disconnect(handle_1.get());
}

BOOST_AUTO_TEST_CASE(no_processing_after_disconnect)
{
  std::string testData{"cmd1\n"
                      "cmd2\n"
                      "cmd3\n"
                      "cmd4\n"
                      "cmd5\n"};
  std::ostringstream oss;

  ContextManager::Instance().SetDefaultOstream(std::make_shared<SharedOstream>(oss));

  smart_handle handle_1(connect(3), close_handle);
  disconnect(handle_1.get());
  receive(handle_1.get(), testData.c_str(), testData.size());

  BOOST_REQUIRE_EQUAL(oss.str().size(), 0);
}

BOOST_AUTO_TEST_CASE(concat_multiple_receive)
{
  std::string testData{"cmd1\n"
                      "cmd2\n"
                      "cmd3\n"
                      "cmd4\n"
                      "cmd5\n"};
  std::string result{
    "bulk: cmd1, cmd2, cmd3\n"
    "bulk: cmd4, cmd5\n"
  };
  std::ostringstream oss;

  ContextManager::Instance().SetDefaultOstream(std::make_shared<SharedOstream>(oss));

  {
    smart_handle handle_1(connect(3), close_handle);
    for(const auto& symbol : testData) {
      receive(handle_1.get(), &symbol, 1);
    }
  }
  std::this_thread::sleep_for(200ms);

  BOOST_REQUIRE_EQUAL(oss.str(), result);
}

BOOST_AUTO_TEST_CASE(output_as_soon_as_possible)
{
  std::array<std::string, 2> testData {
    "cmd1\ncmd2\n"
    , "cmd3\n"};
  std::string result{"bulk: cmd1, cmd2, cmd3\n"};
  std::ostringstream oss;

  ContextManager::Instance().SetDefaultOstream(std::make_shared<SharedOstream>(oss));

  smart_handle handle_1(connect(3), close_handle);
  receive(handle_1.get(), testData[0].c_str(), testData[0].size());
  std::this_thread::sleep_for(200ms);

  BOOST_REQUIRE_EQUAL(oss.str().size(), 0);

  receive(handle_1.get(), testData[1].c_str(), testData[1].size());
  std::this_thread::sleep_for(200ms);

  BOOST_REQUIRE_EQUAL(oss.str(), result);
}

BOOST_AUTO_TEST_CASE(flush_output_by_disconnect)
{
  std::string testData{"cmd1\n"
                      "cmd2\n"
                      "cmd3\n"
                      "cmd4\n"
                      "cmd5\n"};
  std::string first_part{"bulk: cmd1, cmd2, cmd3\n"};
  std::string result{
    "bulk: cmd1, cmd2, cmd3\n"
    "bulk: cmd4, cmd5\n"
  };
  std::ostringstream oss;

  ContextManager::Instance().SetDefaultOstream(std::make_shared<SharedOstream>(oss));

  {
    smart_handle handle_1(connect(3), close_handle);
    receive(handle_1.get(), testData.c_str(), testData.size());
    std::this_thread::sleep_for(200ms);

    BOOST_REQUIRE_EQUAL(oss.str(), first_part);
  }
  std::this_thread::sleep_for(200ms);

  BOOST_REQUIRE_EQUAL(oss.str(), result);
}

BOOST_AUTO_TEST_CASE(process_multiple_handles)
{
  std::string testData_1{"cmd1\n"
                        "cmd2\n"
                        "cmd3\n"
                        "cmd4\n"
                        "cmd5\n"};
  std::string result_1{
    "bulk: cmd1, cmd2, cmd3\n"
    "bulk: cmd4, cmd5\n"
  };

  std::string testData_2{"some\n"
                        "abra\n"
                        "{\n"
                        "cadabra\n"
                        "}\n"};
  std::string result_2{
    "bulk: some, abra\n"
    "bulk: cadabra\n"
  };

  std::ostringstream oss_1, oss_2;

  {
    ContextManager::Instance().SetDefaultOstream(std::make_shared<SharedOstream>(oss_1));
    smart_handle handle_1(connect(3), close_handle);

    ContextManager::Instance().SetDefaultOstream(std::make_shared<SharedOstream>(oss_2));
    smart_handle handle_2(connect(3), close_handle);

    auto itr_1 = std::cbegin(testData_1);
    auto itr_2 = std::cbegin(testData_2);
    while((itr_1 != std::cend(testData_1)) || (itr_2 != std::cend(testData_2))) {
      if(itr_1 != std::cend(testData_1)) {
        receive(handle_1.get(), &*itr_1, 1);
        ++itr_1;
      }
      if(itr_2 != std::cend(testData_2)) {
        receive(handle_2.get(), &*itr_2, 1);
        ++itr_2;
      }
    }
  }
  std::this_thread::sleep_for(200ms);

  BOOST_REQUIRE_EQUAL(oss_1.str(), result_1);
  BOOST_REQUIRE_EQUAL(oss_2.str(), result_2);
}

BOOST_AUTO_TEST_CASE(remove_test_files)
{
  std::system("rm -f *.log");
}

BOOST_AUTO_TEST_SUITE_END()
