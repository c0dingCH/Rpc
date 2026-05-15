#include"Buffer.h"
#include<cstring>
#include<iostream>
#include<string>
#include<assert.h>

Buffer::Buffer()
: buffer_(kInitialSize),
  read_index_(kPrePendIndex),
  write_index_(kPrePendIndex)
{}

Buffer::~Buffer(){}

char *Buffer::begin() { 
  return &*buffer_.begin(); 
}
const char *Buffer::begin() const { 
  return &*buffer_.begin(); 
}

char* Buffer::beginread() { 
  return begin() + read_index_; 
} 
const char* Buffer::beginread() const { 
  return begin() + read_index_; 
}

char* Buffer::beginwrite() { 
  return begin() + write_index_; 
}
const char* Buffer::beginwrite() const { 
  return begin() + write_index_; 
}

void Buffer::Append(const char* msg) {
    Append(msg, strlen(msg));
}

void Buffer::Append(const char* msg, size_t len) {
    EnsureWritableBytes(len);
    memcpy(beginwrite(), msg, len);
    write_index_ += len;
}

void Buffer::Append(const std::string& msg) {
    Append(msg.data(), msg.size()); 
}


size_t Buffer::readablebytes() const { 
  return write_index_ - read_index_; 
}
size_t Buffer::writablebytes() const { 
  return buffer_.size() - write_index_; 
} 
size_t Buffer::prependablebytes() const { 
  return read_index_; 
}

char *Buffer::Peek() { 
  return beginread(); 
}
const char *Buffer::Peek() const { 
  return beginread(); 
}

std::string Buffer::PeekAsString(size_t len){
    return std::string(beginread(), beginread() + len);
}

std::string Buffer::PeekAllAsString(){
  return std::string(beginread(), beginwrite());
}

//返回值move赋值 在编译器中已经有优化了，不用手动move

void Buffer::Retrieve(size_t len){
    assert(readablebytes() >= len);
    read_index_ += len;
}
std::string Buffer::RetrieveAsString(size_t len){
    assert(read_index_ + len <= write_index_);
    std::string ret = PeekAsString(len);
    Retrieve(len);
    return ret;
}


void Buffer::RetrieveUntil(const char *end){
  size_t len = end - beginread();  
  assert(len >= 0);
  read_index_ += len;
}
std::string Buffer::RetrieveUntilAsString(const char *end){
    assert(beginwrite() >= end);
    std::string res = PeekAsString(end - beginread());
    RetrieveUntil(end);
    return res;
}


void Buffer::RetrieveAll(){
    write_index_ = kPrePendIndex;
    read_index_ = write_index_;
}
std::string Buffer::RetrieveAllAsString(){
    assert(readablebytes() >= 0);
    std::string ret = PeekAllAsString();
    RetrieveAll();
    return ret;
}



void Buffer::EnsureWritableBytes(size_t len){
    if(writablebytes() >= len)
        return;
    if(writablebytes() + prependablebytes() >= kPrePendIndex + len){
        memcpy(begin() + kPrePendIndex, beginread(), readablebytes());  
        write_index_ = kPrePendIndex + readablebytes();
        read_index_ = kPrePendIndex;
    }else{
        buffer_.resize(write_index_ + len);
    }
}
