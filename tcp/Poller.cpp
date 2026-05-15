#include "Poller.h"
#include "Channel.h"
#include<sys/epoll.h>
#include<vector>
#include<string.h>
#include<unistd.h>
#include<vector>
#include<iostream>
#define MAXEVENTS 1024


Poller::Poller():fd_(-1){
  fd_ = epoll_create1(0);
  if(fd_ == -1){
    perror("create epoll error!");
    return;
  }
  events_.assign(MAXEVENTS,epoll_event{});
}

Poller::~Poller(){
  if(fd_ != -1){
    close(fd_);
    fd_ = -1;
  }
}

std::vector<Channel*> Poller::Poll(int timeout){
  std::vector<Channel*>active_events;
  int nfds = epoll_wait(fd_,events_.data(),MAXEVENTS,timeout);
  if(nfds == -1){
    perror("epoll wait error");
  }
  for(int i = 0;i < nfds;i++){
    Channel * ch = (Channel *)events_[i].data.ptr;
    ch->SetReadyEvents(events_[i].events); 
    active_events.push_back(ch);
  
  }
  return active_events;
}

void Poller::UpdateChannel(Channel * ch){
  epoll_event ev{};
  ev.events = ch->GetListenEvents();
  ev.data.ptr = ch;

  int sockfd = ch->GetFd();
  if(!ch->GetExist()){
    if(epoll_ctl(fd_, EPOLL_CTL_ADD, sockfd, &ev) == -1){
      std::cout << "Epoller::UpdateChannel epoll_ctl_add failed" << std::endl;
    }
    ch->SetExist(true);
  }
  else{
    if(epoll_ctl(fd_, EPOLL_CTL_MOD, sockfd, &ev) == -1){
      std::cout << "Epoller::UpdateChannel epoll_ctl_mod failed" << std::endl;    
    }
  } 
}

void Poller::DeleteChannel(Channel * ch){
  int sockfd = ch->GetFd();
  if(epoll_ctl(fd_,EPOLL_CTL_DEL,sockfd,nullptr) == -1){
    std::cout << "Epoller::UpdateChannel epoll_ctl_del failed" << std::endl;
  } 
  ch->SetExist(false);
}



