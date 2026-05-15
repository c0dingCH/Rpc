#include"AsyncLogging.h"
#include"LogFile.h"
#include<functional>
AsyncLogging::AsyncLogging(const char * file_path)
: latch_(1),
  running_(false),
  file_path_(file_path)
{
  cur_ = std::make_unique<Buffer>();
  nxt_ = std::make_unique<Buffer>();
}



AsyncLogging::~AsyncLogging(){}


void AsyncLogging::Start(){
  running_ = true;

  thread_ = std::thread(std::bind(&AsyncLogging::Thread,this));
  
  latch_.wait();
}


void AsyncLogging::Stop(){
  running_ = false;
  cv_.notify_one();
  if(thread_.joinable())
    thread_.join();
  
}

void AsyncLogging::Append(const char * msg, int len){
  std::unique_lock<std::mutex>lock(mutex_);

  if(len <= cur_ -> avail()){
    cur_ -> append(msg, len);
  }
  else{
    if(nxt_){
      cur_ = std::move(nxt_);
    }
    else{
      cur_.reset(new Buffer());
    }
    cur_ -> append(msg,len);
  }

  cv_.notify_one();
}


void AsyncLogging::Thread(){
  latch_.notify();

  std::unique_ptr<Buffer>new_cur = std::make_unique<Buffer>();
  std::unique_ptr<Buffer>new_nxt = std::make_unique<Buffer>();
  std::unique_ptr<LogFile>fp = std::make_unique<LogFile>();

  std::vector<std::unique_ptr<Buffer>> active_buffers;

  new_cur -> bzero();
  new_cur -> bzero();

  while(running_){
    std::unique_lock<std::mutex>lock(mutex_);
    cv_.wait_until(lock, std::chrono::system_clock::now() + BufferWriteTimeout * std::chrono::milliseconds(1000),[](){ return false; });

    buffers_.push_back(std::move(cur_));
    active_buffers.swap(buffers_);
    

    cur_ = std::move(new_cur);
    if(!nxt_) nxt_ = std::move(new_nxt);

    for(auto &buf : active_buffers){
      if(buf->len() + fp->write_bytes() > FixedBufferLargerSize){
        fp.reset(new LogFile(file_path_));
      }
      fp->Write(buf->data(), buf->len());
    }
    
    new_cur = std::move(active_buffers.back());
    active_buffers.pop_back();
    new_cur->bzero();
    
    
    if(!new_nxt){
      new_nxt = std::move(active_buffers.back());
      active_buffers.pop_back();
      new_nxt->bzero();
    }

    active_buffers.clear();
  }

}
