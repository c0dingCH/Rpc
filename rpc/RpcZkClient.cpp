#include "RpcZkClient.h"
#include "Logging.h"

#include <semaphore.h>
#include <string>
#include <vector>

void global_watcher(zhandle_t * zh, int type, int state, const char * path, void * ctx){
  if(type == ZOO_SESSION_EVENT && state == ZOO_CONNECTED_STATE){
    sem_t * sem = (sem_t *)ctx;
    sem_post(sem);
  }
}


RpcZkClient::RpcZkClient()
: global_watcher_(global_watcher)
{}

RpcZkClient::RpcZkClient(watcher global_watcher, void * ctx)
: global_watcher_(global_watcher), ctx_(ctx)
{}

RpcZkClient::~RpcZkClient(){
  if(zhandle_){
    zookeeper_close(zhandle_);
  }
};

void RpcZkClient::Start(){
  std::string host = "127.0.0.1";
  std::string port = "2181";
  std::string hp = host + ":" + port;
  
  sem_t sem;
  sem_init(&sem, 0, 0);

  zhandle_ = zookeeper_init(hp.c_str(), global_watcher_, 30000, nullptr, &sem, 0);
  if (!zhandle_){
    LOG_FATAL << "zookeeper_init error!";
  }
  
  sem_wait(&sem);
  sem_destroy(&sem);

  zoo_set_context(zhandle_, ctx_);

  LOG_INFO<< "zookeeper_init success!";
}


bool RpcZkClient::Exists(const char * path, bool watch){
  int flag = zoo_exists(zhandle_, path, watch, nullptr);
  if(flag == ZOK) return true;
  if(flag == ZNONODE) return false;
  LOG_ERROR << "Exists error, path=" << path << " flag=" << flag;
  return false;
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

std::vector<std::string> RpcZkClient::GetChildrens(const char * path, bool watch){
  std::vector<std::string> childrens;
  String_vector chs;
  int res = zoo_get_children(zhandle_, path , watch , &chs);
  
  if(res == ZOK){
    for(int i = 0;i < chs.count;i++){
      childrens.emplace_back(chs.data[i]);
    }
  }
  else{
    LOG_ERROR << "get znode children error ... path:"<<path;  
  }
  
  return childrens;
}

std::string RpcZkClient::GetData(const char * path, bool watch){
  char buffer[64];
  int len = sizeof(buffer);

  int flag = zoo_get(zhandle_, path, watch , buffer, &len, nullptr);
  if(flag != ZOK){
    LOG_ERROR << "get znode error ... path:"<<path;  
    return "";
  }
  else{
    return buffer;
  }
}
