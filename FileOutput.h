#pragma once

#include <fstream>
#include "IOutput.h"
#include "ThreadPool.h"
#include "Statistics.h"

std::string MakeFilename(std::size_t timestamp, const std::thread::id& id, unsigned short counter) {
  std::stringstream filename;
  filename << "bulk" << std::to_string(timestamp) << "_" << id << "_" << counter << ".log";
  return filename.str();
}

class FileOutputThreadHandler : public BaseStatistics
{

public:

  void operator()(const std::pair<std::size_t, std::list<std::string>>& data) {
    auto filename = MakeFilename(data.first, std::this_thread::get_id(), counter++);
    std::ofstream ofs{filename.c_str(), std::ofstream::out | std::ofstream::trunc};
    if(ofs.fail())
      throw std::runtime_error("FileOutput::Output. Can't open file for output.");
    OutputFormattedBulk(ofs, data.second);
    auto is_failed = ofs.fail();
    ofs.close();
    if(is_failed)
      throw std::runtime_error("FileOutput::Output. Failed to write to file.");
    ++statistics.blocks;
    statistics.commands += data.second.size();
    processed_filenames.push_back(std::move(filename));
  }

  auto GetProcessedFilenames() const {
    return processed_filenames;
  }

private:

  unsigned short counter{0};
  std::vector<std::string> processed_filenames;
};

class FileOutput : public IOutput, public ThreadPool<std::pair<std::size_t, std::list<std::string>>, FileOutputThreadHandler>
{
public:

  explicit FileOutput(size_t threads_count = 1)
  {
    for(decltype(threads_count) i{0}; i < threads_count; ++i)
      AddWorker();
  }

  void Output(const std::size_t timestamp, const std::list<std::string>& data) override {
    PushMessage(std::make_pair(timestamp, data));
  }

};
