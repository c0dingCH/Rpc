#include"EventLoopThread.h"
#include"EventLoop.h"
#include<mutex>
#include<condition_variable>
#include<iostream>

EventLoopThread::EventLoopThread(){

}

EventLoopThread::~EventLoopThread(){

}


EventLoop * EventLoopThread::StartThread(){
  EventLoop * loop = nullptr;
  
  thread_ = std::thread(std::bind(&EventLoopThread::ThreadFunc, this));
  
  {
    std::unique_lock<std::mutex>lock(mtx_);
    cv_.wait(lock, [this](){ return loop_ != nullptr; });
    loop = loop_;
  }

  return loop;
}

void EventLoopThread::ThreadFunc(){
  EventLoop loop;
  
  {
    std::unique_lock<std::mutex>lock(mtx_);
    loop_ = &loop;
    cv_.notify_one();
  }

  loop_->Loop();


  {
    std::unique_lock<std::mutex>lock(mtx_);
    loop_ = nullptr;
  }

}

