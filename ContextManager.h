#pragma once

#include <map>
#include <shared_mutex>
#include "Context.h"
#include "async.h"

using namespace async;

class ContextManager {

public:

  static ContextManager& Instance()
  {
    static ContextManager contextManager;
    return contextManager;
  }

  ContextManager(const ContextManager&) = delete;
  ContextManager& operator=(const ContextManager&) = delete;
  ContextManager(const ContextManager&&) = delete;
  ContextManager& operator=(const ContextManager&&) = delete;

  handle_t MakeContext(std::size_t bulkSize) {
    auto context = std::make_shared<Context>(bulkSize);
    std::lock_guard<std::shared_timed_mutex> exclusive_lk(contexts_mutex);
    auto result = contexts.insert(std::make_pair(context.get(), std::move(context)));
    if(false == result.second)
      return nullptr;
    return reinterpret_cast<handle_t>(result.first->first);
  }

  void Erase(handle_t handle) {
    std::shared_lock<std::shared_timed_mutex> shared_lk(contexts_mutex);
    auto context = contexts.find(reinterpret_cast<Context*>(handle));
    shared_lk.unlock();
    if(std::cend(contexts) != context) {
      std::lock_guard<std::shared_timed_mutex> exclusive_lk(contexts_mutex);
      contexts.erase(context);
    }
  }

  auto Find(handle_t handle) {
    std::shared_lock<std::shared_timed_mutex> shared_lk(contexts_mutex);
    auto context = contexts.find(reinterpret_cast<Context*>(handle));
    if(std::cend(contexts) == context)
      return std::shared_ptr<Context>();
    return context->second;
  }

private:

  ContextManager();

  std::shared_timed_mutex contexts_mutex;
  std::map<Context*, std::shared_ptr<Context>> contexts;
};