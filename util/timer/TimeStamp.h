#pragma once

#include<sys/time.h>
#include<iostream>
#include<string>
#include<cstring>
const int kSecond2MicroSecond = 1e6;

class TimeStamp{
public:
  TimeStamp():micro_seconds_(0){}
  explicit TimeStamp(int64_t micro_seconds):micro_seconds_(micro_seconds){}

  bool operator== (const TimeStamp & ts) const {
    return micro_seconds_ == ts.micro_seconds_;
  }

  bool operator< (const TimeStamp & ts) const {
    return micro_seconds_ < ts.micro_seconds_;
  }

  int64_t GetMicroSeconds(){
    return micro_seconds_;
  }

  std::string ToFormattedString(){
      time_t time = micro_seconds_ / kSecond2MicroSecond;
      tm t;
      localtime_r(&time, &t);
      int64_t micro_seconds = micro_seconds_ % kSecond2MicroSecond;

      char buf[64];
      memset(buf,0,sizeof buf);
      
      snprintf(buf,sizeof (buf) - 1, "%4d-%02d-%02d %02d:%02d:%02d:%06ld", 
                t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                t.tm_hour, t.tm_min, t.tm_sec, micro_seconds
              );

      return buf;
  }

  static TimeStamp Now();
  static TimeStamp AddTime(TimeStamp timestamp, double add_seconds);


private:
  int64_t micro_seconds_;

};

inline TimeStamp TimeStamp::Now(){
  timeval time;
  gettimeofday(&time, NULL);
  return TimeStamp(time.tv_sec * kSecond2MicroSecond + time.tv_usec);
}

inline TimeStamp TimeStamp::AddTime(TimeStamp timestamp, double add_seconds){
  return TimeStamp(timestamp.micro_seconds_ + static_cast<int64_t>(add_seconds * kSecond2MicroSecond) );
}
