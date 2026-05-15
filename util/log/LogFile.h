#pragma once

#include<sys/time.h>
#include<iostream>

static const time_t kFlushInterval = 3; 

class LogFile{
public:
  LogFile(const char * file_path);
  LogFile();
  ~LogFile();

  void Write(const char * msg, int len);
  void Flush();
  int64_t write_bytes();
  
private:
  FILE * fp_{nullptr};
  time_t lastwrite_;
  time_t lastflush_;
  int64_t write_bytes_;
};
