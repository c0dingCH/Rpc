#include"Timer.h"
#include"TimeStamp.h"
#include<functional>

Timer::Timer(TimeStamp timestamp, const std::function<void()>& cb, double interval)
  : expiration_(timestamp),
    callback_(std::move(cb)),
    interval_(interval),
    repeat_(interval > 0.0)
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
