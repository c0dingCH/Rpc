#pragma once
#include"Common.h"
#include"TimeStamp.h"
#include<functional>

class Timer{
public:
  DISALLOW_COPY_AND_MOVE(Timer);

  Timer(TimeStamp timestamp, const std::function<void()>& cb, double interval);
  ~Timer();

  void ReStart(TimeStamp now);
  void Run();
 
  TimeStamp GetExpiration();
  bool GetRepeat();

private:
  TimeStamp expiration_; // 指针的话前向声名就够了，变量得包含头文件
  std::function<void()> callback_;
  double interval_;
  bool repeat_;
};
