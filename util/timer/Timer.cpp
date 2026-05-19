#include"Timer.h"
#include"TimeStamp.h"
#include<functional>

double Timer::kPrecision = 1e-7; // 精确度的下一位, 原始单位为s， 精确度为um， 所以为1e-6 * 1e-1

Timer::Timer(TimeStamp timestamp, const std::function<void()>& cb, double interval)
  : expiration_(timestamp),
    callback_(std::move(cb)),
    interval_(interval),
    repeat_(interval > Timer::kPrecision)
  {}

Timer::~Timer() = default;



void Timer::ReStart(TimeStamp now){
  expiration_ = TimeStamp::AddTime(now,interval_);
}

void Timer::Run(){
  callback_();
}

bool Timer::GetRepeat(){
  return repeat_;
}

TimeStamp Timer::GetExpiration(){
  return expiration_;
}
