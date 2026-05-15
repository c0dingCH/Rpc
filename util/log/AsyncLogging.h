#pragma once
#include"LogStream.h"
#include"Latch.h"

#include<mutex>
#include<condition_variable>
#include<thread>
#include<memory>
#include<vector>

static const double BufferWriteTimeout = 3.0;
static const int64_t FileMaximumSize = 1024 * 1024 * 1024; 

class Latch;

class AsyncLogging{
public:
  typedef FixedBuffer<FixedBufferLargerSize> Buffer;

  AsyncLogging(const char *);
  ~AsyncLogging();

  void Start();
  void Stop();
  
  void Append(const char * msg, int len);
  void Thread();


private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::thread thread_;
  Latch latch_;
  bool running_;
  const char * file_path_;

  std::unique_ptr<Buffer>cur_;
  std::unique_ptr<Buffer>nxt_;
  std::vector<std::unique_ptr<Buffer>>buffers_;
  
};
