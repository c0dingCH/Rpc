#include"Channel.h"
#include"EventLoop.h"
#include"TcpConnection.h"
#include"Logging.h"

Channel::Channel(EventLoop *loop, int fd):loop_(loop), fd_(fd), listen_events_(0), ready_events_(0),exist_(false){}


Channel::~Channel(){
}

void Channel::EnableRead(){
  listen_events_ |= EPOLLIN;
  loop_->UpdateChannel(this);
}

void Channel::EnableWrite(){
  listen_events_ |= EPOLLOUT;
  loop_->UpdateChannel(this);
}

void Channel::DisableWrite(){
  if(listen_events_ & EPOLLOUT){
    listen_events_ ^= EPOLLOUT;
    loop_->UpdateChannel(this);
  }
  
}

void Channel::DisableAll(){
  listen_events_ = 0;
  loop_ -> UpdateChannel(this);
}


void Channel::EnableET(){
  listen_events_ |= EPOLLET;
  loop_->UpdateChannel(this);
}

void Channel::HandleEvent(){
  if(tied_){
    std::shared_ptr<void> gaurd = tie_.lock();
    HandleEventWithGaurd();
  }
  else{
    HandleEventWithGaurd();
  }
}

void Channel::HandleEventWithGaurd(){
  if(ready_events_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)){
    read_();
  }
  if(ready_events_ & EPOLLOUT){
    write_();
  }
}

int Channel::GetFd() const {
  return fd_;
}

short Channel::GetListenEvents() const{
  return listen_events_;
}

short Channel::GetReadyEvents() const{
  return ready_events_;
}

bool Channel::GetExist() const{
  return exist_;
}

void Channel::SetExist(bool exist){
  exist_ = exist;
}

void Channel::SetReadyEvents(short ev){
  ready_events_ = ev;
}

void Channel::SetReadCallback(const std::function<void()>&cb){
  read_ = std::move(cb);
}

void Channel::SetWriteCallback(const std::function<void()>&cb){
  write_ = std::move(cb); 
}

void Channel::Tie(const std::shared_ptr<void> &ptr){
  tied_ = true;
  tie_ = ptr;
}

bool Channel::IsWriting(){
  return (listen_events_ & EPOLLOUT) != 0;
}

