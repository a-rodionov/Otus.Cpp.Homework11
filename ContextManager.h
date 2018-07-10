#pragma once

#include <set>
#include <shared_mutex>
#include "Context.h"
#include "async.h"

using namespace async;

std::shared_ptr<std::ostream> default_ostream(&std::cout, [](auto*){});

template<class T>
struct pointer_cmp {
  typedef std::true_type is_transparent;

  struct helper {
    T* ptr;
    helper() : ptr(nullptr) {}
    helper(helper const&) = default;
    helper(T* p) : ptr(p) {}
    template<class U>
    helper( std::shared_ptr<U> const& sp ) : ptr(sp.get()) {}
    template<class U>
    helper( std::unique_ptr<U> const& up ) : ptr(up.get()) {}
    bool operator<( helper const& other ) const {
      return std::less<T*>()( ptr, other.ptr );
    }
  };

  bool operator()( helper const&& lhs, helper const&& rhs ) const {
    return lhs < rhs;
  }
};

class ContextManager {

public:

  static ContextManager& Instance()
  {
    static ContextManager contextManager;
    return contextManager;
  }

  ~ContextManager() {
    consoleOutput->StopWorkers();
    fileOutput->StopWorkers();
  }

  ContextManager(const ContextManager&) = delete;
  ContextManager& operator=(const ContextManager&) = delete;
  ContextManager(const ContextManager&&) = delete;
  ContextManager& operator=(const ContextManager&&) = delete;

  handle_t MakeContext(std::size_t bulkSize) {
    auto context = std::make_shared<Context>(bulkSize, consoleOutput, fileOutput);
    std::lock_guard<std::shared_timed_mutex> exclusive_lk(member_mutex);
    auto result = contexts.insert(std::move(context));
    return reinterpret_cast<handle_t>(result.first->get());
  }

  void Erase(handle_t handle) {
    std::lock_guard<std::shared_timed_mutex> exclusive_lk(member_mutex);
    auto context = contexts.find(reinterpret_cast<Context*>(handle));
    if(std::cend(contexts) != context) {
      contexts.erase(context);
    }
  }

  auto Find(handle_t handle) const {
    std::shared_lock<std::shared_timed_mutex> shared_lk(member_mutex);
    auto context = contexts.find(reinterpret_cast<Context*>(handle));
    if(std::cend(contexts) == context)
      return std::shared_ptr<Context>();
    return *context;
  }

  void SetDefaultOstream(std::shared_ptr<std::ostream>& ostream) {
    consoleOutput->SetDefaultOstream(ostream);
  }

  void SetDefaultOstream(std::shared_ptr<std::ostream>&& ostream) {
    consoleOutput->SetDefaultOstream(std::move(ostream));
  }

private:

  ContextManager()
  : consoleOutput{std::make_shared<ConsoleOutput>(::default_ostream, 1)},
    fileOutput{std::make_shared<FileOutput>(std::thread::hardware_concurrency())} {}

  mutable std::shared_timed_mutex member_mutex;
  std::set<std::shared_ptr<Context>, pointer_cmp<Context>> contexts;

  std::shared_ptr<ConsoleOutput> consoleOutput;
  std::shared_ptr<FileOutput> fileOutput;
};