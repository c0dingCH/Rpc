#include"EventLoop.h"
#include"Channel.h"
#include"Poller.h"
#include"CurrentThread.h"
#include"TimerQueue.h"
#include"Timer.h"
#include"TimeStamp.h"

#include<memory>
#include<assert.h>
#include<vector>
#include<mutex>
#include<unistd.h>
#include<sys/eventfd.h>

EventLoop::EventLoop():tid_(CurrentThread::tid()){
  poller_ = std::make_unique<Poller>();

  wakeup_fd_ = eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);
  wakeup_channel_ = std::make_unique<Channel>(this,wakeup_fd_);
  wakeup_channel_ -> EnableRead();
  wakeup_channel_ -> SetReadCallback(std::bind(&EventLoop::Read, this));

  timer_queue_ = std::make_unique<TimerQueue>(this);
}

EventLoop::~EventLoop(){
  DeleteChannel(wakeup_channel_.get());
  close(wakeup_fd_);
}


void EventLoop::UpdateChannel(Channel * ch){
  poller_->UpdateChannel(ch);
}

void EventLoop::DeleteChannel(Channel * ch){
  poller_->DeleteChannel(ch);
}

void EventLoop::Loop(){
  while(true){
    auto funcs = poller_->Poll();
    for(auto & func : funcs){
      func -> HandleEvent();
    }
    DoToDoList();
  }

}


void EventLoop::RunOneFunc(const std::function<void()>&cb){
  if(IsInThreadLoop()){
    cb();
  }
  else{
    QueueOneFunc(std::move(cb));
  }
}

void EventLoop::QueueOneFunc(const std::function<void()>&cb){
  {
    std::unique_lock<std::mutex>lock(mtx_);
    to_do_list_.push_back(std::move(cb));
  }

  if(!IsInThreadLoop() || calling_funcs_){
    uint64_t write_bytes = 1;
    assert(write(wakeup_fd_, &write_bytes, sizeof write_bytes) != -1);
  }

} 


void EventLoop::DoToDoList(){
  calling_funcs_ = true;

  std::vector<std::function<void()>>funcs;
  {
    std::unique_lock<std::mutex>lock(mtx_);
    funcs.swap(to_do_list_);
  }

  for(auto & func : funcs){
    func();
  }

  calling_funcs_ = false;
}


void EventLoop::Read(){
  uint64_t read_bytes = 1;
  assert(::read(wakeup_fd_, &read_bytes, sizeof read_bytes) != -1);
}

bool EventLoop::IsInThreadLoop(){
  return tid_ == CurrentThread::tid();
}


void EventLoop::RunAt(TimeStamp timestamp, const std::function<void()> & cb){
  timer_queue_ -> AddTimer(timestamp, std::move(cb), 0.0);      
}

void EventLoop::RunAfter(double wait_time, const std::function<void()> & cb){
  timer_queue_ -> AddTimer(TimeStamp::AddTime(TimeStamp::Now(), wait_time), std::move(cb), 0.0);
}

void EventLoop::RunEvery(double interval , const std::function<void()> & cb){
  timer_queue_ -> AddTimer(TimeStamp::Now(), std::move(cb), interval);
}
