#pragma once
#include"Common.h"
#include"TimeStamp.h"
#include<functional>

class Timer{
public:
  DISALLOW_COPY_AND_MOVE(Timer);

  Timer(TimeStamp timestamp, const std::function<void()>& cb, double interval);
  ~Timer();
  
  static double kPrecision;

  void ReStart(TimeStamp now);
  void Run();
  void Cancel() { cancelled_ = true; }
 
  TimeStamp GetExpiration();
  bool GetRepeat();
  bool IsCancelled() const { return cancelled_; }

private:
  TimeStamp expiration_; // 指针的话前向声名就够了，变量得包含头文件
  std::function<void()> callback_;
  double interval_;
  bool repeat_;
  bool cancelled_{false};
};
