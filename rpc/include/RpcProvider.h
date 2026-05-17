#pragma once
#include<google/protobuf/service.h>
#include<google/protobuf/descriptor.h>
#include<unordered_map>
#include<memory>

#include "EventLoop.h"
#include "Header.pb.h"

class TcpConnection;

class RpcProvider{
public:
  void NotifyService(google::protobuf::Service * serv);

  void Run();

private:
  EventLoop main_reactor_;    
  
  struct ServiceInfo{
    google::protobuf::Service *serv;
    std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> methods;
  };

  std::unordered_map<std::string, ServiceInfo> servs_;
  

  void OnConnect(const std::shared_ptr<TcpConnection> & conn);
  void OnMessage(const std::shared_ptr<TcpConnection> & conn);

  void SendResponse(const std::shared_ptr<TcpConnection> &conn,
                          google::protobuf::RpcController * controller,
                          google::protobuf::Message * response);
};
