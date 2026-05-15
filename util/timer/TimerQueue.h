#pragma once
#include"Common.h"

#include<vector>
#include<set>
#include<functional>
#include<memory>

class Channel;
class EventLoop;
class Timer;
class TimeStamp;

class TimerQueue{
public:
  DISALLOW_COPY_AND_MOVE(TimerQueue);
  TimerQueue(EventLoop * loop);
  ~TimerQueue();

  void CreateTimerFd();
  
  void ReadTimerFd();
  void HandleRead();

  bool Insert(Timer * timer);
  void AddTimer(TimeStamp timestamp, const std::function<void()> & cb, double interval);

  void ResetTimers();
  void ResetTimerFd(Timer * timer);

private:
  typedef std::pair<TimeStamp,Timer *> Entry;

  EventLoop * loop_{nullptr};
  int timer_fd_{-1};
  std::unique_ptr<Channel>channel_;

  std::vector<Entry>active_timers_;
  std::set<Entry>timers_;
};


