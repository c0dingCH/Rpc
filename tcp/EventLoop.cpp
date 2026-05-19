#include"EventLoop.h"
#include"Channel.h"
#include"Poller.h"
#include"CurrentThread.h"
#include"TimerQueue.h"
#include"Timer.h"
#include"TimeStamp.h"
#include"Logging.h"
#include"TcpConnection.h"

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


void EventLoop::CancelTimer(Timer * timer){
  timer_queue_ -> DeleteTimer(timer);
}

Timer * EventLoop::RunAt(TimeStamp timestamp, const std::function<void()> & cb){
  return timer_queue_ -> AddTimer(timestamp, std::move(cb), 0.0);      
}

Timer * EventLoop::RunAfter(double wait_time, const std::function<void()> & cb){
  return timer_queue_ -> AddTimer(TimeStamp::AddTime(TimeStamp::Now(), wait_time), std::move(cb), 0.0);
}

Timer * EventLoop::RunEvery(double interval , const std::function<void()> & cb){
  return timer_queue_ -> AddTimer(TimeStamp::Now(), std::move(cb), interval);
}

uint64_t EventLoop::AddContext(std::shared_ptr<TcpConnection> client_conn,
                                std::shared_ptr<TcpConnection> provider_conn,
                                long long when, Timer * timer) {
  while(contexts_.count(++glob_id_) || glob_id_ == 0); // 获取一个没有被维护的上下文
  
  auto ctx = std::make_unique<Context>();
  ctx->client_conn = std::move(client_conn);
  ctx->provider_conn = std::move(provider_conn);
  ctx->when = when;
  ctx->timer = timer;
  contexts_[glob_id_] = std::move(ctx);
  return glob_id_;
}

void EventLoop::RemoveContext(uint64_t id) {
  auto it = contexts_.find(id);
  if(it != contexts_.end()){
    if(it->second->timer) CancelTimer(it->second->timer);
    contexts_.erase(it);
  }
}

EventLoop::Context* EventLoop::GetContext(uint64_t id) {
  auto it = contexts_.find(id);
  if(it != contexts_.end()) return it->second.get();
  return nullptr;
}


