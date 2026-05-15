#include"LogFile.h"
#include"TimeStamp.h"

#include<iostream>
#include<string>

LogFile::LogFile(){
  std::string default_path = ("../LogFiles/" + TimeStamp::Now().ToFormattedString() + ".log");
  fp_ = fopen(default_path.data(), "a+");
}

LogFile::LogFile(const char * file_path)
: fp_(fopen(file_path, "a+")),
  lastwrite_(0),
  lastflush_(0),
  write_bytes_(0)
{
  if(!fp_){
    std::string default_path = ("../LogFiles/" + TimeStamp::Now().ToFormattedString() + ".log");
    fp_ = fopen(default_path.data(), "a+");
  }
}


LogFile::~LogFile(){
  Flush();
  if(fp_){
    fclose(fp_);
  }
}

void LogFile::Write(const char * msg, int len){
  int cur = 0;
  while(cur < len){
    cur += fwrite_unlocked(msg + cur , sizeof (char), len - cur, fp_);
  }

  time_t now = ::time(nullptr);
  if(len){
    write_bytes_ += len;
    lastwrite_ = now;
  }
  
  if(lastwrite_ - lastflush_ > kFlushInterval){
    Flush();
    lastflush_ = lastwrite_;
  }
}

int64_t LogFile::write_bytes(){
  return write_bytes_;
}

void LogFile::Flush(){
  fflush(fp_);
}

