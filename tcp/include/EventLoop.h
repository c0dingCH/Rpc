#pragma once
#include<functional>
#include<mutex>
#include"Common.h"
#include<memory>
class Poller;
class Channel;
class ThreadPool;
class TimeStamp;
class TimerQueue;

class EventLoop{
public:
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

private:
  std::unique_ptr<Poller> poller_;
  std::unique_ptr<Channel> wakeup_channel_;
  std::vector<std::function<void()>>to_do_list_;

  std::mutex mtx_;
  bool calling_funcs_{false}; 
  pid_t tid_{-1};
  int wakeup_fd_;
  
  std::unique_ptr<TimerQueue> timer_queue_;
};
