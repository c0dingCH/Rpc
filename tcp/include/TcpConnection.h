#pragma once
#include<functional>
#include<unordered_map>
#include"Common.h"
#include"TimeStamp.h"
#include"Logging.h"
#include"RpcLoadScore.h"
#include"RpcCircuitBreaker.h"

#include<memory>

class EventLoop;
class Socket;
class Channel;
class Buffer;
class User;
class Timer;

class TcpConnection : public std::enable_shared_from_this<TcpConnection>{
public:
  enum State{
    kInvalid = 1,
    kConnected,
    kClosed
  };

  enum Role{
    kClient,
    kProvider
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
  std::string GetAddr();
  TimeStamp GetTimeStamp();

  void SetUser(User* user);
  User* GetUser() const;

  void HandleClose();
  void HandleMessage();
  void HandleWrite();

  void SetRole(Role role);
  bool IsProvider(){ return role_ == Role::kProvider; }
  bool IsClient() { return role_ == Role::kClient; }

  RpcLoadScore* GetLoadScore() { return load_score_.get(); }
  RpcCircuitBreaker* GetCircuitBreaker() { return circuit_breaker_.get(); }

  uint64_t GetContextId() const { return context_id_; }
  void SetContextId(uint64_t id) { context_id_ = id; }

  // provider conn 侧映射 context_id → client_loop
  void SetCtxLoop(uint64_t id, EventLoop * loop);
  EventLoop * GetCtxLoop(uint64_t id);
  void RemoveCtxLoop(uint64_t id);

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

  Role role_{kClient};
  std::unique_ptr<RpcLoadScore> load_score_;
  std::unique_ptr<RpcCircuitBreaker> circuit_breaker_;

  uint64_t context_id_{0}; // client
  std::unordered_map<uint64_t, EventLoop *> provider_context_map_; // provider
};
