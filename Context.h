#pragma once

#include "Storage.h"
#include "ConsoleOutput.h"
#include "FileOutput.h"
#include "CommandProcessor.h"

class Context {

public:

  Context(std::size_t bulkSize,
          std::shared_ptr<ConsoleOutput>& consoleOutput,
          std::shared_ptr<FileOutput>& fileOutput)
    : commandProcessor{std::make_unique<CommandProcessor>()},
    storage{std::make_shared<Storage>(bulkSize)}
  {
    storage->Subscribe(consoleOutput);
    storage->Subscribe(fileOutput);
    commandProcessor->Subscribe(storage);
  }

  ~Context() {
    commandProcessor->Process(nullptr, 0, true);
  }

  void Process(const char* data, std::size_t size) {
    std::lock_guard<std::mutex> exclusive_lk(process_mutex);
    commandProcessor->Process(data, size);
  }

private:

  std::mutex process_mutex;
  std::unique_ptr<CommandProcessor> commandProcessor;
  std::shared_ptr<Storage> storage;

};
