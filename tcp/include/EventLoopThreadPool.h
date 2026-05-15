#pragma once
#include"Common.h"
#include<vector>
#include<memory>

class EventLoopThread;
class EventLoop;
class EventLoopThreadPool{
public:

  DISALLOW_COPY_AND_MOVE(EventLoopThreadPool);
  EventLoopThreadPool(EventLoop * main_reactor);
  ~EventLoopThreadPool();
  
  
  void Start();
  void SetThreadNums(const int & thread_nums); 
  EventLoop * GetNextLoop();


private:
  EventLoop * main_reactor_{nullptr};
  int thread_nums_{0};
  int next_{0};
  std::vector<std::unique_ptr<EventLoopThread>>threads_;
  std::vector<EventLoop *>loops_;
};
