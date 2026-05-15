#include "RpcZkClient.h"
#include "Logging.h"

#include <semaphore.h>
#include <string>

void global_watcher(zhandle_t * zh, int type, int state, const char * path, void * watcherCtx){
  if(type == ZOO_SESSION_EVENT && state == ZOO_CONNECTED_STATE){
    sem_t * sem = (sem_t *)zoo_get_context(zh);
    sem_post(sem);
  }
}


RpcZkClient::RpcZkClient(){}

RpcZkClient::~RpcZkClient(){
  if(zhandle_){
    zookeeper_close(zhandle_);
  }
};

void RpcZkClient::Start(){
  //这里先用静态的，后续用config
  std::string host = "127.0.0.1";
  std::string port = "2181";
  std::string hp = host + ":" + port;
  
  zhandle_ = zookeeper_init(hp.c_str(), global_watcher, 30000, nullptr, nullptr, 0);
  if (!zhandle_){
    LOG_FATAL << "zookeeper_init error!";
  }
  
  sem_t sem;
  sem_init(&sem, 0, 0);
  zoo_set_context(zhandle_, &sem);

  sem_wait(&sem);
  LOG_INFO<< "zookeeper_init success!";
}


void RpcZkClient::Create(const char * path, const char * data, int len, int state){
  char path_buffer[128];
  int buffer_len = sizeof(path_buffer);
  int flag = zoo_exists(zhandle_, path, 0 , nullptr);
  if(ZNONODE == flag){
    flag = zoo_create(zhandle_, path , data, len, &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, buffer_len);
    
    if(flag == ZOK){
      LOG_INFO << "znode create success... path:" << path;
    }
    else{
      LOG_FATAL << "flag:" << flag<< "\n" << "znode create error... path:" << path;
    }
  }

}


std::string RpcZkClient::GetData(const char * path){
  char buffer[64];
  int len = sizeof(buffer);

  int flag = zoo_get(zhandle_, path, 0 , buffer, &len, nullptr);
  if(flag != ZOK){
    LOG_ERROR << "get znode error ... paht:"<<path;  
    return "";
  }
  else{
    return buffer;
  }

}

