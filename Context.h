#pragma once

#include "Storage.h"
#include "ConsoleOutput.h"
#include "FileOutput.h"
#include "CommandProcessor.h"

class Context {

public:

  Context(std::size_t bulkSize,
          std::shared_ptr<SharedOstream>& shared_ostream,
          std::size_t consoleOutputThreads = 1,
          std::size_t fileOutputThreads = 2)
    : commandProcessor{std::make_unique<CommandProcessor>()},
    storage{std::make_shared<Storage>(bulkSize)},
    consoleOutput{std::make_shared<ConsoleOutput>(shared_ostream, consoleOutputThreads)},
    fileOutput{std::make_shared<FileOutput>(fileOutputThreads)}
  {
    storage->Subscribe(consoleOutput);
    storage->Subscribe(fileOutput);
    commandProcessor->Subscribe(storage);
  }

  ~Context() {
    commandProcessor->Process(nullptr, 0, true);
    consoleOutput->StopWorkers();
    fileOutput->StopWorkers();
  }

  void Process(const char* data, std::size_t size) {
    std::lock_guard<std::mutex> exclusive_lk(process_mutex);
    commandProcessor->Process(data, size);
  }

private:

  std::mutex process_mutex;
  std::unique_ptr<CommandProcessor> commandProcessor;
  std::shared_ptr<Storage> storage;
  std::shared_ptr<ConsoleOutput> consoleOutput;
  std::shared_ptr<FileOutput> fileOutput;
};
