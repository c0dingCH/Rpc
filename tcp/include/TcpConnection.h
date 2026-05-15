#pragma once
#include<functional>
#include"Common.h"
#include"TimeStamp.h"
#include<memory>

class EventLoop;
class Socket;
class Channel;
class Buffer;
class User;

class TcpConnection : public std::enable_shared_from_this<TcpConnection>{
public:
  enum State{
    kInvalid = 1,
    kConnected,
    kClosed
  };

  TcpConnection(EventLoop * loop, int connfd, int connid);
  ~TcpConnection();
  DISALLOW_COPY_AND_MOVE(TcpConnection);

  void ConnectionEstablished();
  void ConnectionDestructor();

  void SetOnCloseCallback(const std::function<void(const std::shared_ptr<TcpConnection>)> &cb);
  void SetOnMessageCallback(const std::function<void(const std::shared_ptr<TcpConnection>)> & cb);
  void SetOnConnectCallback(const std::function<void(const std::shared_ptr<TcpConnection>)> & cb);
  void SetTimeStamp(TimeStamp timestamp);

  void Read();
  void Send(const char * msg);
  void Send(const char * msg, int len);
  void Send(const std::string & msg);
  void SendInLoop(const std::string &msg);

  Buffer * GetReadBuffer() const;
  Buffer * GetSendBuffer() const;
  
  State GetState() const;
  EventLoop * GetLoop() const;
  int GetFd() const;
  int GetId() const;
  TimeStamp GetTimeStamp();
  
  void SetUser(User* user);
  User* GetUser() const;

  void HandleClose();
  void HandleMessage();
  void HandleWrite();

private:
  EventLoop * loop_{nullptr};
  int connfd_{-1};
  int connid_{-1};
  State state_{State::kInvalid};
  

  std::unique_ptr<Channel> channel_;
  std::unique_ptr<Buffer> read_buffer_;
  std::unique_ptr<Buffer> send_buffer_;

  std::function<void(const std::shared_ptr<TcpConnection>)> on_close_;
  std::function<void(const std::shared_ptr<TcpConnection>)> on_message_;
  std::function<void(const std::shared_ptr<TcpConnection>)> on_connect_;

  void ReadNonBlocking();
   
  TimeStamp timestamp_;
  bool fault_error_{false};
  User* user_{nullptr};


};
