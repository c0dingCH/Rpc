#include <iostream>
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "RpcProvider.h"
#include "EventLoop.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include "Logging.h"
#include "CurrentThread.h"
#include "RpcZkClient.h"
#include "Buffer.h"
#include "Header.pb.h"

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
  std::string ip = "127.0.0.11";
  short port = 8888;
  std::string hp = ip + ":" + std::to_string(port);

  TcpServer tcp_serv(ip.c_str(), port, &main_reactor_);
  tcp_serv.OnConnect(std::bind(&RpcProvider::OnConnect, this, std::placeholders::_1));
  tcp_serv.OnMessage(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1));
  
  //tcp_serv.SetThreadNums();

  RpcZkClient zk_client;
  zk_client.Start();

  for(auto &[serv_name, serv_info] : servs_){
    std::string serv_path = "/" + serv_name;
    zk_client.Create(serv_path.c_str(), nullptr, 0);

    for(auto &[method_name, method] : serv_info.methods){
      std::string method_path = serv_path + "/" + method_name;
      zk_client.Create(method_path.c_str(), hp.c_str(), hp.size(), ZOO_EPHEMERAL);
    }
  }
  
  LOG_INFO << "RpcProvide start service at " << hp; 

  tcp_serv.Start();
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
    protoheader::RpcHeader rpc_header;
    std::string serv_name;
    std::string method_name;

    if(rpc_header.ParseFromString(all_buf.substr(8,header_size))){
      serv_name = rpc_header.serv_name();
      method_name = rpc_header.method_name();
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

    google::protobuf::Message * request = serv->GetRequestPrototype(method).New();
    if(!request -> ParseFromString(all_buf.substr(8 + header_size))){
      LOG_ERROR << "request parse error, content: " << all_buf.substr(8 + header_size);
      return;
    }
    google::protobuf::Message * response = serv->GetResponsePrototype(method).New();
    
    google::protobuf::Closure * cb = google::protobuf::NewCallback<
      RpcProvider, const std::shared_ptr<TcpConnection> &,google::protobuf::Message *>(
      this, &RpcProvider::SendResponse, conn, response);


    serv->CallMethod(method, nullptr, request, response, cb);

  }

  


}

void RpcProvider::SendResponse(const std::shared_ptr<TcpConnection> &conn, google::protobuf::Message * response) {
  std::string msg;
  if (response->SerializeToString(&msg)){
    conn->Send(msg);
  }
  else{
    LOG_ERROR << "serialize response_str error!";
  }
  conn->HandleClose();

}
