#include <iostream>
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>//
#include <thread>//

#include "RpcProvider.h"
#include "EventLoop.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include "Logging.h"
#include "CurrentThread.h"
#include "RpcZkClient.h"
#include "Buffer.h"
#include "Header.pb.h"
#include "ProviderController.h"
#include "RpcClosure.h"
#include "RpcApplication.h"

int ccc = 0;

void RpcProvider::NotifyService(google::protobuf::Service *serv) {
  ServiceInfo serv_info;
  serv_info.serv = serv;

  const google::protobuf::ServiceDescriptor * serv_desc = serv->GetDescriptor();
  std::string serv_name = serv_desc->name();
  int method_cnt = serv_desc->method_count();

  LOG_INFO << "Notify server : "<<serv_name;

  for(int i = 0;i < method_cnt;i++){
    const google::protobuf::MethodDescriptor * method_desc = serv_desc->method(i);
    std::string method_name = method_desc->name();
    serv_info.methods[method_name] = method_desc;
  
    LOG_INFO << "method"<<i<<" "<<method_name;
  }
  
  servs_[serv_name] = serv_info;
}

void RpcProvider::Run() {

  //静态ip端口
  std::string hp = RpcApplication::instance().config().Load("hp");
  puts(hp.c_str());
  int it = hp.find(':'); 
  if(it == -1){
    LOG_FATAL << "illegal addr";
  }
  std::string ip = hp.substr(0,it);
  short port = atoi(hp.substr(it + 1).c_str());

  TcpServer server(ip.c_str(), port, &main_reactor_);
  server.OnConnect(std::bind(&RpcProvider::OnConnect, this, std::placeholders::_1));
  server.OnMessage(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1));
  
  //server.SetThreadNums();

  RpcZkClient zk_client;
  zk_client.Start();

  std::string root = "/rpc";
  zk_client.Create(root.c_str(), nullptr, 0);

  for(auto &[serv_name, serv_info] : servs_){
    std::string serv_path = root + "/" + serv_name;
    zk_client.Create(serv_path.c_str(), nullptr, 0);

    for(auto &[method_name, method] : serv_info.methods){
      std::string method_path = serv_path + "/" + method_name;
      zk_client.Create(method_path.c_str(), nullptr, 0 , 0);

      method_path.push_back('/');
      method_path.append(hp);
      zk_client.Create(method_path.c_str(), nullptr, 0, ZOO_EPHEMERAL);
    }
  } 
  
  LOG_INFO << "RpcProvide start service at " << hp; 

  server.Start();
  main_reactor_.Loop();
}



void RpcProvider::OnConnect(const std::shared_ptr<TcpConnection> &conn) {
  if(conn->GetState() != TcpConnection::State::kConnected) return;  
  sockaddr_in addr{};
  socklen_t len = sizeof addr;
  getpeername(conn->GetFd(), (sockaddr *)&addr, &len);

  LOG_INFO  <<  CurrentThread::tid()
            <<  " EchoServer::OnNewConnection : new connection"
            <<  " fd[#"<< conn->GetFd()<<"]"
            <<  " from "<< inet_ntoa(addr.sin_addr)<<":"<<ntohs(addr.sin_port);

}


// 包格式 :   4B(tatol_size except the 4B) + 4B(header_size) +  header_size + header.args_size(data) 
void RpcProvider::OnMessage(const std::shared_ptr<TcpConnection> &conn) {
  if(conn->GetState() != TcpConnection::State::kConnected) return;

  Buffer * buf = conn->GetReadBuffer();

  while(1){
    if(buf->readablebytes() < 4) return;
    std::string head_buffer = buf -> PeekAsString(4);

    // 包的总长度
    uint32_t total_size;
    head_buffer.copy((char *)&total_size, 4, 0);
    if(buf->readablebytes() < total_size)return;

    //header的长度
    std::string all_buf = buf -> RetrieveAsString(total_size);
    uint32_t header_size;
    all_buf.copy((char *)&header_size, 4, 4);

    //header解析
    protoheader::RequestHeader rpc_header;
    std::string serv_name;
    std::string method_name;
    uint64_t request_id = 0, context_id = 0;

    if(rpc_header.ParseFromString(all_buf.substr(8,header_size))){
      serv_name = rpc_header.service();
      method_name = rpc_header.method();
      request_id = rpc_header.request_id();
      context_id = rpc_header.context_id();
    }
    else{
      LOG_ERROR << "rpc_header_str: " << all_buf.substr(8,header_size) << " parse error! ";
      return;
    }

    auto it_servs = servs_.find(serv_name);
    if(it_servs == servs_.end()){
      LOG_ERROR << "No such service: " << serv_name;
      return;
    }

    auto it_method = it_servs -> second.methods.find(method_name);
    if(it_method == it_servs -> second.methods.end()){
      LOG_ERROR << "No such method: " << method_name <<" from service: " << serv_name;
      return;
    }

    google::protobuf::Service * serv = it_servs->second.serv;
    const google::protobuf::MethodDescriptor * method = it_method -> second;

    auto request = std::shared_ptr<google::protobuf::Message>(serv->GetRequestPrototype(method).New());
    if(!request -> ParseFromString(all_buf.substr(8 + header_size))){
      LOG_ERROR << "request parse error, content: " << all_buf.substr(8 + header_size);
      return;
    }

    auto response = std::shared_ptr<google::protobuf::Message>(serv->GetResponsePrototype(method).New());
    
    auto controller = std::make_shared<ProviderController>();
    controller->set_request_id(request_id);
    controller->set_context_id(context_id);

    google::protobuf::Closure * done = new RpcClosure([this, conn, controller, request, response]() {
      SendResponse(conn, controller.get(), response.get());
    });

    serv->CallMethod(method, (google::protobuf::RpcController *)controller.get(), request.get(), response.get(), done);
  }
}




void RpcProvider::SendResponse(const std::shared_ptr<TcpConnection> &conn,
                               google::protobuf::RpcController * controller,
                               google::protobuf::Message * response) {
  protoheader::ResponseHeader resp_header;
  auto * pc = dynamic_cast<ProviderController *>(controller);
  resp_header.set_request_id(pc->request_id());
  resp_header.set_context_id(pc->context_id());
  resp_header.set_code(pc->code());
  resp_header.set_msg(pc->msg());

  std::string header_str, body_str;
  if (!resp_header.SerializeToString(&header_str)) {
    LOG_ERROR << "serialize ResponseHeader error";
    return;
  }
  if (!response->SerializeToString(&body_str)) {
    LOG_ERROR << "serialize response body error";
    return;
  }

  uint32_t header_size = header_str.size();
  uint32_t total_size = 8 + header_size + body_str.size();

  std::string msg;
  msg.append((char *)&total_size, 4);
  msg.append((char *)&header_size, 4);
  msg.append(header_str);
  msg.append(body_str);

  //std::cout<<total_size <<" "<<header_size<<" "<<header_str.size() << " "<<body_str.size()<<std::endl;
  if(++ccc >= 4) std::this_thread::sleep_for(std::chrono::milliseconds(300));
  else std::this_thread::sleep_for(std::chrono::milliseconds(100));
  conn->Send(msg);
  puts("react");
}


