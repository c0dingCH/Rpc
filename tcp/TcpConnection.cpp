#include"TcpConnection.h"
#include"Channel.h"
#include"EventLoop.h"
#include"Buffer.h"
#include"Common.h"
#include"TimeStamp.h"
#include"Logging.h"
#include"RpcLoadState.h"

#include<functional>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<iostream>
#include<fcntl.h>
#include<sys/socket.h>
#include<sys/ioctl.h>
#include<sys/sendfile.h>
#include<arpa/inet.h>
#include<netinet/in.h>

#define READ_BUFFER 1024

TcpConnection::TcpConnection(EventLoop * loop, int connfd ,int connid):loop_(loop),connfd_(connfd),connid_(connid){
  if(loop_ != nullptr){
    channel_ = std::make_unique<Channel>(loop_,connfd_);
    channel_ -> EnableET();  
    channel_ -> SetReadCallback(std::bind(&TcpConnection::HandleMessage, this)); // 这里用裸指针，避免循环引用
    channel_ -> SetWriteCallback(std::bind(&TcpConnection::HandleWrite, this));
  } 
  read_buffer_ = std::make_unique<Buffer>();
  send_buffer_ = std::make_unique<Buffer>();
  state_ = State::kConnected;
}

TcpConnection::~TcpConnection(){
  puts("xi gou le");
  if(connfd_ != -1){
    close(connfd_);
    connfd_ = -1;
  }
}

void TcpConnection::ConnectionEstablished(){
  channel_->EnableRead();
  channel_->Tie(shared_from_this());
  if(on_connect_){
    on_connect_(shared_from_this());
  }
}

void TcpConnection::ConnectionDestructor(){
  loop_->DeleteChannel(channel_.get());
  if(on_connect_){
    on_connect_(shared_from_this());
  }
}


void TcpConnection::HandleMessage(){
  Read();
  if(state_ == State::kConnected && on_message_){
    on_message_(shared_from_this());
    
  }
}

void TcpConnection::HandleWrite(){
  if(channel_ -> IsWriting()){
    if(state_ == State::kConnected){
      ssize_t n = write(connfd_, send_buffer_ -> beginread(), send_buffer_ -> readablebytes());
      send_buffer_ -> Retrieve(n);

      if(n >= 0){
        if(send_buffer_ -> readablebytes() == 0){
          channel_ -> DisableWrite();
          LOG_INFO << "write once ok !";
        }
      }
      else{
        if(errno != EAGAIN && errno != EWOULDBLOCK){
          LOG_ERROR << "syswrite error";
          if(errno == EPIPE || errno == ECONNRESET){
            fault_error_ = true;
            send_buffer_ -> RetrieveAll();
            channel_ -> DisableWrite();
          }
        }
      }

    }
    else{
      //不用手动diablewrite了，既然close，handleclose会dsiableall
      LOG_INFO << "give up writing for disconnection";
    }
  }

}


void TcpConnection::HandleClose(){
  if(state_ != State::kClosed){
    state_ = State::kClosed;
    
    LOG_INFO <<"client fd: "<<connfd_<<" kClosed ";
    
    channel_ -> DisableAll();

    if(on_close_){ 
      on_close_(shared_from_this());
    }
    
  }
}       


void TcpConnection::Read(){
  read_buffer_->RetrieveAll();
  ReadNonBlocking();
}

void TcpConnection::Send(const char * msg){
  Send(msg,(int)strlen(msg));
}

void TcpConnection::Send(const char * msg, int len){
  Send(std::string(msg,len));
}

void TcpConnection::Send(const std::string & msg){ // Send 安全问题，在RunOneFunc里面有控制到，自己线程直接跑，别的线程串行跑,用回调排队还能防止其他线程串行等待 
  if(state_ == State::kConnected){
    loop_ -> RunOneFunc(std::bind(&TcpConnection::SendInLoop, this, msg));    
  }
  else{
    LOG_ERROR << "can't send for disconnected conn";
  }
}

void TcpConnection::SendInLoop(const std::string &msg){
  fault_error_ = false;
  if(state_ == State::kConnected){
    ssize_t n = 0;
    if(!channel_ -> IsWriting() && send_buffer_ -> readablebytes() == 0){
      n = write(connfd_, msg.c_str(), msg.size());
    }
    if(n < 0){
      n = 0;
      if(errno != EAGAIN && errno != EWOULDBLOCK){
        LOG_ERROR << "system error";
        if(errno == EPIPE || errno == ECONNRESET){ //写入已经关闭了的连接，可能还有其他errno值
          fault_error_ = true;
        }
      }
     
    }
    if(n < static_cast<int>(msg.size()) && !fault_error_){
      send_buffer_ -> Append(msg.c_str() + n, msg.size() - n);
      if(!channel_ -> IsWriting()){
        channel_ -> EnableWrite();
      }
    }
  }
  else{
    LOG_ERROR << "give up writing for disconnection";
  }
}


void TcpConnection::ReadNonBlocking(){
  int sockfd = connfd_;
  char buf[1024];
  while(true){
    memset(buf,0,sizeof buf);
    ssize_t read_bytes = ::read(sockfd,buf,sizeof buf);
    if(read_bytes > 0){
      read_buffer_->Append(buf,read_bytes);
    }
    else if(read_bytes == 0){
      //printf("client %d disconnected \n", sockfd);
      HandleClose();
      break;
    }
    else if(read_bytes == -1 && errno == EINTR){
      printf("Continue reading\n");
      continue;
    }
    else if(read_bytes == -1 && (errno == EAGAIN  || errno ==  EWOULDBLOCK)){
      break;
    }
    else{
      printf("Other errno on client %d read \n", sockfd);
      HandleClose();
      break;
    }
  }
}

//void TcpConnection::WriteNonBlocking(){
//  int sockfd = connfd_;
//  char buf[send_buffer_->readablebytes()];
//  memcpy(buf,send_buffer_->beginread(), send_buffer_->readablebytes());
//  int data_size = send_buffer_->readablebytes();
//  int data_left = data_size;
//  while(data_left > 0){
//    ssize_t write_bytes = ::write(sockfd, buf + data_size - data_left, data_left);
//    if(write_bytes == -1){
//      if(errno == EINTR){
//        printf("continue writing\n");
//        continue;
//      }
//      else if(errno == EAGAIN || errno == EWOULDBLOCK) break;
//      else{
//        printf("Other errno on client %d write",sockfd);
//        HandleClose();
//        break;
//      }
//    }
//   data_left -= write_bytes;
//  }
//}


TcpConnection::State TcpConnection::GetState() const{
  return state_;
}

EventLoop * TcpConnection::GetLoop() const{
  return loop_;
}

int TcpConnection::GetFd() const{
  return connfd_;
}

int TcpConnection::GetId() const{
  return connid_;
}

std::string TcpConnection::GetAddr(){
  sockaddr_in addr{};
  socklen_t len = sizeof addr;
  getpeername(connfd_, (sockaddr *)&addr, &len);
  return std::string(inet_ntoa(addr.sin_addr)) + ":" + std::to_string(ntohs(addr.sin_port));
}

TimeStamp TcpConnection::GetTimeStamp(){
  return timestamp_;
}


Buffer * TcpConnection::GetReadBuffer() const{
  return read_buffer_.get();
}


Buffer * TcpConnection::GetSendBuffer() const{
  return send_buffer_.get();
}



void TcpConnection::SetOnCloseCallback(const std::function<void(const std::shared_ptr<TcpConnection>)> &cb){
  on_close_ = std::move(cb);
}

void TcpConnection::SetOnMessageCallback(const std::function<void(const std::shared_ptr<TcpConnection>)> &cb){
  on_message_ = std::move(cb);
}

void TcpConnection::SetOnConnectCallback(const std::function<void(const std::shared_ptr<TcpConnection>)> &cb){
  on_connect_ = std::move(cb);
}


void TcpConnection::SetTimeStamp(TimeStamp timestamp){
  timestamp_ = timestamp;
}


void TcpConnection::SetRole(Role role){
  role_ = role;
  if(IsProvider()){
    res_.emplace<ProviderRes>();
    if(!load_state_)load_state_ = std::make_unique<RpcLoadState>();
    else LOG_ERROR << "LoadState set before SetRole";
  }
}

void TcpConnection::SetClientRes(EventLoop * loop, uint64_t context_id){
  res_ = ClientRes{loop, context_id};
}

void TcpConnection::AddContextId(uint64_t context_id){
  if(auto * pr = std::get_if<ProviderRes>(&res_)){
    if(pr->context_ids.count(context_id)){
      LOG_ERROR << " re_add reactor id : "<< context_id << " in conn";
    }
    else{
      pr->context_ids.insert(context_id);
    }
  }
}

void TcpConnection::RemoveContextId(uint64_t context_id){
  if(auto * pr = std::get_if<ProviderRes>(&res_)){
    auto it = pr->context_ids.find(context_id);
    if(it == pr->context_ids.end()){
      LOG_ERROR << " re_remove reactor id : "<< context_id << " in conn";
    }
    else{
      pr->context_ids.erase(it);
    }
  }
}
