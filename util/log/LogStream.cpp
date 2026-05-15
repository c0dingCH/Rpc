#include"LogStream.h"

#include<string>
#include<iostream>
#include<cstring>




//------


LogStream::LogStream(){}

LogStream::~LogStream(){}

void LogStream::append(const char * str,int len){
  buffer_.append(str,len);
}

FixedBuffer<FixedBufferSize> & LogStream::buffer() {
  return buffer_;
}

void LogStream::resetBuffer(){
  buffer_.reset();
}



LogStream & LogStream::operator<<(short num){
  formattedInteger(num);  
  return *this; 
}

LogStream & LogStream::operator<<(unsigned short num){
  formattedInteger(num);  
  return *this; 
  
}

LogStream & LogStream::operator<<(int num){
  formattedInteger(num);  
  return *this; 
}

LogStream & LogStream::operator<<(unsigned int num){
  formattedInteger(num);  
  return *this; 
}
LogStream & LogStream::operator<<(long num){
  formattedInteger(num);  
  return *this; 
}

LogStream & LogStream::operator<<(unsigned long num){
  formattedInteger(num);  
  return *this; 
}

LogStream & LogStream::operator<<(long long num){
  formattedInteger(num);  
  return *this; 
}

LogStream & LogStream::operator<<(unsigned long long num){
  formattedInteger(num);  
  return *this; 
}



LogStream & LogStream::operator<<(const float & num){
  return *this<<static_cast<double>(num);  
}

LogStream & LogStream::operator<<(const double & num){
  char str[32] = {0};
  int len = snprintf(str,sizeof str,"%g", num);
  buffer_.append(str,len);
  return *this;
}

LogStream & LogStream::operator<<(char c){
  buffer_.append(&c, 1);
  return * this;
}

LogStream & LogStream::operator<<(const char * str){
  buffer_.append(str, strlen(str));
  return *this;
}

LogStream & LogStream::operator<<(const std::string & str){
  buffer_.append(str.c_str(), str.size());
  return *this;
}
