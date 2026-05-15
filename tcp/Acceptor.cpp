#include"Acceptor.h"
#include"EventLoop.h"
#include"Channel.h"
#include<functional>
#include<iostream>
#include<cstring>
#include<fcntl.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<assert.h>
#include<unistd.h>
Acceptor::Acceptor(EventLoop * loop, const char * ip, const short port):loop_(loop){
  Create(); 
  Bind(ip,port);
  Listen();

  channel_ = std::make_unique<Channel>(loop_,listenfd_);
  channel_->EnableRead();
  auto cb = std::bind(&Acceptor::AcceptConnection, this);
  channel_->SetReadCallback(cb);

}

Acceptor::~Acceptor(){
  loop_->DeleteChannel(channel_.get());
  if(listenfd_ != -1){
    close(listenfd_);
    listenfd_ = -1;
  }
}


void Acceptor::Create(){
  assert(listenfd_ == -1);
  listenfd_ = socket(AF_INET,SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,0);
  //CLOEXEC close fd when use exec()
  if(listenfd_ == -1)
    std::cout<<"Failed to create socket"<<std::endl;

}

void Acceptor::Bind(const char * ip, const short port){
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(ip);
  addr.sin_port = htons(port);
  if(::bind(listenfd_, (sockaddr *)&addr, sizeof addr) == -1){
    std::cout<<"Failed to Bind: "<<ip<<" : "<< port<< std::endl;
  }
}

void Acceptor::Listen(){
  assert(listenfd_ != -1);
  if(::listen(listenfd_, SOMAXCONN) == -1)
      std::cout<<"Failed to Listen "<<std::endl;
}


void Acceptor::AcceptConnection() const{
  sockaddr_in addr{};
  socklen_t len = sizeof addr;
  int clnt_fd = accept4(listenfd_, (sockaddr *)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if(clnt_fd == -1){
    std::cout<<"acceptor error"<<std::endl;
  }
  new_connection_(clnt_fd); 
}

void Acceptor::SetNewConnectionCallback(const std::function<void(int)> &cb){
  new_connection_ = std::move(cb);
}


