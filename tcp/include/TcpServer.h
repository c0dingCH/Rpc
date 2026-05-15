#pragma once
#include<map>
#include<vector>
#include<functional>
#include"Common.h"
#include<memory>
class EventLoop;
class Socket;
class Acceptor;
class TcpConnection;
class EventLoopThreadPool;

class TcpServer{
public:
  TcpServer(const char *ip, const short port, EventLoop * main_reactor);
  ~TcpServer();
  DISALLOW_COPY_AND_MOVE(TcpServer);


  void Start();
  void HandleNewConnection(int sockfd);
  void HandleCloseConnection(const std::shared_ptr<TcpConnection> &conn);
  void HandleCloseConnectionInPoller(const std::shared_ptr<TcpConnection> &conn);

  void OnConnect (const std:: function<void(const std::shared_ptr<TcpConnection>)> &cb);
  void OnMessage (const std:: function<void(const std::shared_ptr<TcpConnection>)> &cb);

  void SetThreadNums(const int & thread_nums);

private:
  int next_connid_{1};
  EventLoop * main_reactor_;
  std::unique_ptr<Acceptor> acceptor_;
  std::map<int,std::shared_ptr<TcpConnection>>connections_;
  std::unique_ptr<EventLoopThreadPool> event_loop_thread_pool_;

  std::function<void(const std::shared_ptr<TcpConnection>)> on_connect_ ;
  std::function<void(const std::shared_ptr<TcpConnection>)> on_message_ ;

  void Initial();
};
