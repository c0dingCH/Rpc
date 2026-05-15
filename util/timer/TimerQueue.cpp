#include"TimerQueue.h"
#include"EventLoop.h"
#include"Channel.h"
#include"TimeStamp.h"
#include"Timer.h"

#include<vector>
#include<set>
#include<assert.h>
#include<sys/timerfd.h>
#include<iostream>
#include<memory>
#include<functional>
#include<unistd.h>

TimerQueue::TimerQueue(EventLoop * loop)
  : loop_(loop)
{
  CreateTimerFd();
  channel_ = std::make_unique<Channel>(loop,timer_fd_);
  channel_ -> EnableRead();
  channel_ -> SetReadCallback(std::bind(&TimerQueue::HandleRead, this));
}


TimerQueue::~TimerQueue(){
  if(timer_fd_ != -1){
    close(timer_fd_);
    timer_fd_ = -1;
  }
  for(auto & entry : timers_)delete entry.second;
}

void TimerQueue::CreateTimerFd(){
  timer_fd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  assert(timer_fd_ != -1);
}

void TimerQueue::ReadTimerFd(){
  uint64_t read_one_byte = 1;
  if(read(timer_fd_ , &read_one_byte, sizeof read_one_byte) == -1 ){
    std::cout<<"read timerfd error"<<std::endl;
  }
}

void TimerQueue::HandleRead(){
  ReadTimerFd();
  
  active_timers_.clear();
  auto end = timers_.lower_bound(Entry(TimeStamp::Now(), reinterpret_cast<Timer *>(UINTPTR_MAX)));
  active_timers_.insert(active_timers_.end(),timers_.begin(), end);
  timers_.erase(timers_.begin(),end );
  
  for(auto &entry : active_timers_){
    entry.second->Run();
  }

  ResetTimers();
}

void TimerQueue::AddTimer(TimeStamp timestamp, const std::function<void()> & cb, double interval){
  Timer * timer = new Timer(timestamp,cb,interval);
  if(Insert(timer)){
    ResetTimerFd(timer);
  }
}


bool TimerQueue::Insert(Timer * timer){
  bool instantly = false;
  if(timers_.empty() || timer->GetExpiration() < timers_.begin() -> first){
    instantly = true;
  }
  timers_.insert(Entry(timer->GetExpiration(), timer));
  return instantly;
}

void TimerQueue::ResetTimers(){
  for(auto &entry : active_timers_){
    auto timer = entry.second;
    if(timer -> GetRepeat()){
      timer -> ReStart(TimeStamp::Now());
      Insert(timer);
    } 
    else{
      delete timer;
    }
  }

  if(!timers_.empty()){
    ResetTimerFd(timers_.begin()->second);
  }

}

void TimerQueue::ResetTimerFd(Timer * timer){
  itimerspec new_{};
  itimerspec old_{};


  int64_t micro_second_dif = timer->GetExpiration().GetMicroSeconds() - TimeStamp::Now().GetMicroSeconds();
  if(micro_second_dif < 100){
    micro_second_dif = 100;
  }

  new_.it_value.tv_sec = static_cast<time_t>(
    micro_second_dif / kSecond2MicroSecond
  );

  new_.it_value.tv_nsec = static_cast<long long>(
    micro_second_dif % kSecond2MicroSecond * 1000    
  );
  
  assert(::timerfd_settime(timer_fd_, 0, &new_, &old_) != -1);
}




