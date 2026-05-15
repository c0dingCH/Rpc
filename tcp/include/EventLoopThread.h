#pragma once
#include"Common.h"
#include<mutex>
#include<condition_variable>
#include<thread>

class EventLoop;

class EventLoopThread{
public:
  DISALLOW_COPY_AND_MOVE(EventLoopThread);
  EventLoopThread();
  ~EventLoopThread();
  
  EventLoop * StartThread();


private:
  void ThreadFunc();

  std::mutex mtx_;
  std::condition_variable cv_;
  
  std::thread thread_;
  EventLoop * loop_{nullptr};


};
