#pragma once
#include"Common.h"
#include<mutex>
#include<condition_variable>

class Latch{
public:
  DISALLOW_COPY_AND_MOVE(Latch);

  explicit Latch(int count) 
  : count_(count)
  {}

  void wait(){
    std::unique_lock<std::mutex>lock(mutex_);
    while(count_ > 0){
      cv_.wait(lock);
    }
  } 

  void notify(){
    std::unique_lock<std::mutex>lock(mutex_);
    count_--;
    if(count_ == 0){
      cv_.notify_all();
    }
  }

private:
  int count_;
  std::mutex mutex_;
  std::condition_variable cv_;

};
