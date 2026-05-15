#pragma once
#include<functional>
#include<memory>
#include"Common.h"
class EventLoop;
class Channel;

class Acceptor{
public:
  explicit Acceptor(EventLoop * _loop, const char * ip, const short port);
  ~Acceptor();
    

  void Create();

  void Bind(const char * ip, const short port);

  void Listen();

  void AcceptConnection() const;

  void SetNewConnectionCallback(const std::function<void(int)> &cb);

private:
  EventLoop * loop_{nullptr};
  int listenfd_{-1};
  std::unique_ptr<Channel> channel_;
  std::function<void(int)> new_connection_;
};
