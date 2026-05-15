#include"TcpServer.h"
#include"Channel.h"
#include"Acceptor.h"
#include"TcpConnection.h"
#include"EventLoop.h"
#include"EventLoopThreadPool.h"
#include<unistd.h>
#include<functional>
#include<string.h>
#include<errno.h>
#include<thread>
#include<iostream>
#include<signal.h>

#define READ_BUFFER 1024

TcpServer::TcpServer(const char * ip,const short port, EventLoop * main_reactor)
: main_reactor_(main_reactor)
{
  Initial();

  acceptor_ = std::make_unique<Acceptor>(main_reactor_, ip, port);
  auto cb = std::bind(&TcpServer::HandleNewConnection, this, std::placeholders::_1);
  acceptor_->SetNewConnectionCallback(cb);

  int size = std::thread::hardware_concurrency();
  event_loop_thread_pool_ = std::make_unique<EventLoopThreadPool>(main_reactor_);
  event_loop_thread_pool_ -> SetThreadNums(size);
}

TcpServer::~TcpServer(){
  
}

void TcpServer::Initial(){
  signal(SIGPIPE, SIG_IGN);
}

void TcpServer::Start(){
  event_loop_thread_pool_ -> Start();
}

void TcpServer::HandleNewConnection(int sockfd){
  assert(sockfd != -1);

  std::shared_ptr<TcpConnection> conn = std::make_shared<TcpConnection>(event_loop_thread_pool_->GetNextLoop(),sockfd, next_connid_);
  conn->SetOnCloseCallback(std::bind(&TcpServer::HandleCloseConnection, this, std::placeholders::_1));
  conn->SetOnMessageCallback(on_message_);//自己保留一份，不要用move
  conn->SetOnConnectCallback(on_connect_);
  
  connections_[sockfd] = conn;
  next_connid_++;
  if(next_connid_ == 1001){
    next_connid_ = 1;
  }
  
  conn->ConnectionEstablished();
}

void TcpServer::HandleCloseConnection(const std::shared_ptr<TcpConnection> &conn){
  main_reactor_->RunOneFunc(std::bind(&TcpServer::HandleCloseConnectionInPoller, this, conn));
} 

void TcpServer::HandleCloseConnectionInPoller(const std::shared_ptr<TcpConnection> &conn){
  int sockfd = conn->GetFd();
  assert(connections_.count(sockfd));
  connections_.erase(sockfd);
  
  auto loop = conn->GetLoop();
  loop -> QueueOneFunc(std::bind(&TcpConnection::ConnectionDestructor, conn));
}




void TcpServer::OnConnect(const std::function<void(std::shared_ptr<TcpConnection>)> & cb){
  on_connect_ = std::move(cb) ;//值传递，move就不用拷贝了
}

void TcpServer::OnMessage(const std::function<void(std::shared_ptr<TcpConnection>)> & cb){
  on_message_ = std::move(cb) ;//值传递，move就不用拷贝了
  
}


void TcpServer::SetThreadNums(const int & thread_nums){
  event_loop_thread_pool_ -> SetThreadNums(thread_nums);
}
