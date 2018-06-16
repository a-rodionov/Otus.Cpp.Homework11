#include "async.h"
#include "ContextManager.h"

namespace async {

handle_t connect(std::size_t bulk) {
  try {
    return ContextManager::Instance().MakeContext(bulk);
  }
  catch(...) {
    std::cerr << __PRETTY_FUNCTION__ << ". Failed to create new context.\n";
    return nullptr;
  }
}

void receive(handle_t handle, const char *data, std::size_t size) {
  try {
    auto context = ContextManager::Instance().Find(handle);
    if(context) {
      context->Process(data, size);
    }
  }
  catch(...) {
    std::cerr << __PRETTY_FUNCTION__ << ". Failed to process data.\n";
  }
}

void disconnect(handle_t handle) {
  try {
    ContextManager::Instance().Erase(handle);
  }
  catch(...) {
    std::cerr << __PRETTY_FUNCTION__ << ". Failed to erase context.\n";
  }
}

} 
