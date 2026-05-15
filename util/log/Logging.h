#pragma once
#include"LogStream.h"
#include"sys/time.h"


class Logger{
public:
  enum LogLevel{
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
  };

  class SourceFile{
  public:
    SourceFile(const char * file);
    const char * file_;
    int length_;
  };

  Logger(const char * file, int line, LogLevel level);
  ~Logger();

  LogStream &stream();

  static LogLevel logLevel();
  static void SetLogLevel(LogLevel);
  

  typedef void (*OutputFunc)(const char * data, int len);
  typedef void (*FlushFunc)();
  static void SetOutputFunc(OutputFunc);
  static void SetFlushFunc(FlushFunc);

private:
  class Impl{
  public:
    DISALLOW_COPY_AND_MOVE(Impl);
    typedef Logger::LogLevel LogLevel;

    Impl(const char *file, int line, LogLevel level);

    void FormattedTime();
    void Finish();

    LogStream & stream();
    const char * logLevel() const;
    LogLevel logLevel_;

  private:
    Logger::SourceFile source_file_;
    LogStream stream_;
    int line_;//代码行数
  };
  
  Impl impl_;
};


extern Logger::LogLevel g_logLevel; //全局的level变量
inline Logger::LogLevel Logger::logLevel(){
  return g_logLevel;
}




//日志宏

#define LOG_DEBUG if(Logger::logLevel() <= Logger::LogLevel::DEBUG) \
  Logger(__FILE__,__LINE__,Logger::LogLevel::DEBUG/**,__func__**/).stream()

#define LOG_INFO  if(Logger::logLevel() <= Logger::LogLevel::INFO) \
  Logger(__FILE__,__LINE__,Logger::LogLevel::INFO).stream()

#define LOG_WARN  Logger(__FILE__,__LINE__,Logger::LogLevel::WARN).stream()

#define LOG_ERROR Logger(__FILE__,__LINE__,Logger::LogLevel::ERROR).stream()

#define LOG_FATAL Logger(__FILE__,__LINE__,Logger::LogLevel::FATAL).stream()





