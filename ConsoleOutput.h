#pragma once

#include <iostream>
#include "IOutput.h"
#include "ThreadPool.h"
#include "Statistics.h"

struct ConsoleOutputSharedData {
  ConsoleOutputSharedData(std::ostream& out)
    : out{out} {}

  std::ostream& out;
  std::mutex ostream_mutex;
};



class ConsoleOutputThreadHandler : public BaseStatistics
{

public:

  explicit ConsoleOutputThreadHandler(const std::shared_ptr<ConsoleOutputSharedData>& consoleOutputSharedData)
    : consoleOutputSharedData{consoleOutputSharedData} {}

  void operator()(const std::list<std::string>& data) {
    std::unique_lock<std::mutex> lk(consoleOutputSharedData->ostream_mutex);
    OutputFormattedBulk(consoleOutputSharedData->out, data);
    lk.unlock();
    ++statistics.blocks;
    statistics.commands += data.size();
  }

private:

  std::shared_ptr<ConsoleOutputSharedData> consoleOutputSharedData;
};



class ConsoleOutput : public IOutput, public ThreadPool<std::list<std::string>, ConsoleOutputThreadHandler>
{

public:

  explicit ConsoleOutput(std::ostream& out, size_t threads_count = 1)
    : consoleOutputSharedData{std::make_shared<ConsoleOutputSharedData>(out)}
  {
    for(decltype(threads_count) i{0}; i < threads_count; ++i) {
      AddWorker();
    }
  }

  void AddWorker() {
    ThreadPool::AddWorker(consoleOutputSharedData);
  }

  void Output(const std::size_t, const std::list<std::string>& data) override {
    PushMessage(data);
  }

private:

  std::shared_ptr<ConsoleOutputSharedData> consoleOutputSharedData;
};
