#pragma once
#include<iostream>
#include"Common.h"
#include<string>
#include<cstring>
#include<algorithm>
#include<iostream>

static const int FixedBufferSize = 4096;
static const int FixedBufferLargerSize = 4096 * 1000;
static const int kMaxNumericSize = 48;

template<int SIZE>
class FixedBuffer{
public:
  FixedBuffer();
  ~FixedBuffer();

  void append(const char *buf, int len);
  const char * data() const;
  int len() const;

  char * current();
  int avail() const;
  void add(int len);

  void reset();
  const char * end() const;
  void bzero();

private:
  char data_[SIZE];
  char *cur_;
};



class LogStream{
public:
  typedef FixedBuffer<FixedBufferSize> Buffer;
  typedef LogStream self;

  DISALLOW_COPY_AND_MOVE(LogStream);
  LogStream();
  ~LogStream();

  void append(const char *buf, int len);
  Buffer & buffer() ;
  void resetBuffer();


  self & operator<<(short num);
  self & operator<<(unsigned short num);
  self & operator<<(int num);
  self & operator<<(unsigned int num);
  self & operator<<(long num);
  self & operator<<(unsigned long num);
  self & operator<<(long long num);
  self & operator<<(unsigned long long num);

  self & operator<<(const float & num);
  self & operator<<(const double & num);
  self & operator<<(char c);
  self & operator<<(const char * str);
  self & operator<<(const std::string & str);


private:
  template <typename T>
  void formattedInteger(T val);
  Buffer buffer_;

};


template <typename T>
void LogStream::formattedInteger(T val){
  if(buffer_.avail() >= kMaxNumericSize){
    char * buf = buffer_.current();
    char * cur = buf;
    bool negative_num = val < 0;
    
    do{
      int tmp = val % 10;
      *(cur++) = tmp + '0';
      val /= 10;
    }while(val != 0);
    if(negative_num) *(++cur) = '-';

    std::reverse(buf,cur);
    buffer_.add(cur - buf);
  }
}


class Fmt{
public:
  template<typename T>
  Fmt(const char * fmt ,T val);
  ~Fmt() = default;

  const char * data() const { return buf_; }
  int length() const { return length_; }

private:
  char buf_[32];
  int length_;

};


template<typename T>
Fmt::Fmt(const char * fmt, T val){
  static_assert(std::is_arithmetic<T>::value == true, "Must be arithmetic type");

  length_ = snprintf(buf_, sizeof buf_, fmt, val);

  assert(static_cast<size_t>(length_) < sizeof buf_);
}



inline LogStream & operator<< (LogStream & logstream, const Fmt & fmt){
  logstream.append(fmt.data(), fmt.length());
  return logstream;
}




template Fmt::Fmt(const char *, short);
template Fmt::Fmt(const char *, unsigned short);
template Fmt::Fmt(const char *, int);
template Fmt::Fmt(const char *, unsigned int);
template Fmt::Fmt(const char *, long);
template Fmt::Fmt(const char *, unsigned long);
template Fmt::Fmt(const char *, long long);
template Fmt::Fmt(const char *, unsigned long long);

template Fmt::Fmt(const char *, double);
template Fmt::Fmt(const char *, float);
template Fmt::Fmt(const char *, char);





template<int SIZE>
FixedBuffer<SIZE>::FixedBuffer()
  : cur_(data_)
{}


template<int SIZE>
FixedBuffer<SIZE>::~FixedBuffer(){}


template<int SIZE>
void FixedBuffer<SIZE>::append(const char * buf, int len){
  if(avail() >= len){
    memcpy(cur_, buf, len);
    add(len);
  }
}

template<int SIZE>
int FixedBuffer<SIZE>::len() const{
  return cur_ - data_;
}

template<int SIZE>
const char * FixedBuffer<SIZE>::data() const{
  return data_;
}

template<int SIZE>
char * FixedBuffer<SIZE>::current(){
  return cur_;
}

template<int SIZE>
int FixedBuffer<SIZE>::avail() const{
  return static_cast<int>(end() - cur_);
}

template<int SIZE>
void FixedBuffer<SIZE>::add(int len){
  cur_ += len;
}

template<int SIZE>
void FixedBuffer<SIZE>::reset(){
  memset(data_,0 , cur_ - data_);
  cur_ = data_;
}

template<int SIZE>
void FixedBuffer<SIZE>::bzero(){
  memset(data_, 0, sizeof data_);
  cur_ = data_;
}

template<int SIZE>
const char * FixedBuffer<SIZE>::end() const{
    return data_ + sizeof data_ ;
}
