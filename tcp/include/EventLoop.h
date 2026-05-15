#pragma once
#include<functional>
#include<mutex>
#include"Common.h"
#include<memory>
#include<map>

class Poller;
class Channel;
class ThreadPool;
class TimeStamp;
class TimerQueue;
class TcpConnection;

class EventLoop{
public:
  struct Context{
    TcpConnection * caller;
    TcpConnection * callee;
    long long when;

    Context(TcpConnection * _caller, TcpConnection * _callee, long long _when):caller(_caller), callee(_callee), when(_when){}
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
  

  void RunAt(TimeStamp timestamp, const std::function<void()> & cb);
  void RunAfter(double wait_time, const std::function<void()> & cb);
  void RunEvery(double interval , const std::function<void()> & cb);

    
  uint64_t RoundId();
  uint64_t SetContext(std::unique_ptr<Context> context);
  Context * GetContext(uint64_t id);
  void RemoveContext(uint64_t id);
  

private:
  std::unique_ptr<Poller> poller_;
  std::unique_ptr<Channel> wakeup_channel_;
  std::vector<std::function<void()>>to_do_list_;

  std::mutex mtx_;
  bool calling_funcs_{false}; 
  pid_t tid_{-1};
  int wakeup_fd_;
  
  std::unique_ptr<TimerQueue> timer_queue_;
  
  std::map<uint64_t, std::unique_ptr<Context> > contexts_;
  uint64_t id_{0};
};
