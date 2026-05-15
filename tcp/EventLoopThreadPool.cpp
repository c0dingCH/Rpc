#include"EventLoopThreadPool.h"
#include"EventLoop.h"
#include"EventLoopThread.h"
#include<vector>
#include<memory>
#include<iostream>
EventLoopThreadPool::EventLoopThreadPool(EventLoop * main_reactor):main_reactor_(main_reactor){

}

EventLoopThreadPool::~EventLoopThreadPool(){

}

void EventLoopThreadPool::Start(){
  for(int i = 0;i < thread_nums_;i++){
    threads_.push_back(std::make_unique<EventLoopThread>());
    loops_.push_back(threads_.back() -> StartThread());
  }

}

void EventLoopThreadPool::SetThreadNums(const int &thread_nums){
  thread_nums_ = thread_nums;
}

EventLoop * EventLoopThreadPool::GetNextLoop(){
  EventLoop * loop = main_reactor_;
  
  if(!loops_.empty()){
    loop = loops_[next_];
    next_++;
    if(next_ == thread_nums_)next_ = 0;
  }

  return loop;
}
