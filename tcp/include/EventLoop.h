#pragma once
#include<functional>
#include<mutex>
#include<unordered_map>
#include"Common.h"
#include<memory>

class Poller;
class Channel;
class ThreadPool;
class TimeStamp;
class Timer;
class TimerQueue;
class TcpConnection;

class EventLoop{
public:
  struct Context {
    std::shared_ptr<TcpConnection> client_conn;
    std::shared_ptr<TcpConnection> provider_conn;
    Timer * timer{nullptr};
    int cnt{0};
    long long when{0};
    long long timeout_ms{200};
    bool is_probe{false};
  };

  EventLoop();
  ~EventLoop();
  
  DISALLOW_COPY_AND_MOVE(EventLoop);

  void Loop();
  void UpdateChannel(Channel * ch);
  void DeleteChannel(Channel * ch);


  void RunOneFunc(const std::function<void()> &cb);
  void QueueOneFunc(const std::function<void()> &cb);
  void DoToDoList();
  void Read();
  bool IsInThreadLoop();
  

  Timer * RunAt(TimeStamp timestamp, const std::function<void()> & cb);
  Timer * RunAfter(double wait_time, const std::function<void()> & cb);
  Timer * RunEvery(double interval , const std::function<void()> & cb);
  void CancelTimer(Timer * timer);

  uint64_t AddContext(uint64_t id,
                      std::shared_ptr<TcpConnection> client_conn,
                      std::shared_ptr<TcpConnection> provider_conn,
                      long long when, Timer * timer = nullptr);
  void RemoveContext(uint64_t id);
  Context* GetContext(uint64_t id);

private:
  std::unique_ptr<Poller> poller_;
  std::unique_ptr<Channel> wakeup_channel_;
  std::vector<std::function<void()>>to_do_list_;

  std::mutex mtx_;
  bool calling_funcs_{false}; 
  pid_t tid_{-1};
  int wakeup_fd_;
  
  std::unique_ptr<TimerQueue> timer_queue_;

  std::unordered_map<uint64_t, std::unique_ptr<Context>> contexts_;
  uint64_t glob_id_{0};
};
