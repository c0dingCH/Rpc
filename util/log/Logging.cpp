#include"Logging.h"
#include"LogStream.h"
#include"TimeStamp.h"
#include"CurrentThread.h"

#include<iostream>
#include<cstring>
#include<string>

__thread char t_time[64];
__thread time_t t_lastseconds;


Logger::SourceFile::SourceFile(const char * file)
: file_(file),
  length_(strlen(file))
{
  const char * it = strrchr(file_, '/');
  if(it){
    file_ = it + 1;
    length_ = strlen(file_);
  }

}

class Template{
public:
  Template(const char * str,int len)
  : str_(str),
    len_(len)
  {}

  const char * str_;
  int len_;
};


inline LogStream & operator<<(LogStream &stream, Template temp){
  stream.append(temp.str_ , temp.len_);
  return stream;
}

inline LogStream & operator<<(LogStream &stream, const Logger::SourceFile & source_file){
  stream.append(source_file.file_, source_file.length_);
  return stream;
}



Logger::Impl::Impl(const char * file, int line, LogLevel level)
: logLevel_(level),
  source_file_(file),
  line_(line)
{
  FormattedTime();
  CurrentThread::tid();
  
  stream_ << Template(CurrentThread::tidString(), CurrentThread::tidStringLength()) << " ";
  stream_ << Template(logLevel(), 6);
}


void Logger::Impl::FormattedTime(){
  TimeStamp now = TimeStamp::Now();
  time_t seconds = now.GetMicroSeconds() / kSecond2MicroSecond;
  uint64_t microseconds = now.GetMicroSeconds() % kSecond2MicroSecond;
  
  if(seconds != t_lastseconds){
    tm t;
    localtime_r(&seconds, &t);
    snprintf(t_time,sizeof t_time, "%4d%02d%02d %02d:%02d:%02d.",
      t.tm_year + 1900, t.tm_mon + 1 , t.tm_mday ,
      t.tm_hour , t.tm_min, t.tm_sec
    );
    t_lastseconds = seconds;
  }
  stream_ << Template(t_time, 17);
  Fmt fmt_mic("%05dZ ", microseconds);
  stream_ << Template(fmt_mic.data(), 7);
}

void Logger::Impl::Finish(){
  stream_ << " - " << source_file_ <<" "<< line_ << "\n";
}

LogStream & Logger::Impl::stream(){
  return stream_;
}

const char * Logger::Impl::logLevel() const{
  switch(logLevel_){
    case LogLevel::DEBUG:
      return "DEBUG ";
    case LogLevel::INFO :
      return "INFO  ";
    case LogLevel::WARN :
      return "WARN  ";
    case LogLevel::ERROR:
      return "ERROR ";
    case LogLevel::FATAL:
      return "FATAL ";
  }
  return nullptr;
}


//----


void DefaultOutput(const char * data, int len){
  fwrite(data, 1 , len , stdout);
}

void DefaultFlush(){
  fflush(stdout);
}

Logger::OutputFunc g_output = DefaultOutput;
Logger::FlushFunc g_flush = DefaultFlush;
Logger::LogLevel g_logLevel = Logger::LogLevel::INFO;


Logger::Logger(const char * file, int line, LogLevel level)
: impl_(file, line, level)
{}


Logger::~Logger(){
  impl_.Finish();

  const auto & buf = impl_.stream().buffer();
  g_output(buf.data() , buf.len());

  if(impl_.logLevel_ == LogLevel::FATAL){
    g_flush();
    abort();
  }
}


LogStream & Logger::stream(){
  return impl_.stream();
}


void Logger::SetOutputFunc(Logger::OutputFunc func){
  g_output = func; 
}

void Logger::SetFlushFunc(Logger::FlushFunc func){
  g_flush = func;
}

void Logger::SetLogLevel(LogLevel level){
  g_logLevel = level;
}
