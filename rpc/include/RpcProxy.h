#pragma once
#include <atomic>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include "EventLoop.h"
#include "Header.pb.h"

class TcpConnection;
class RpcZkClient;
class TcpServer;

class RpcProxy{
public:
  RpcProxy();
  ~RpcProxy();

  void Run();

  void UpdateService(const std::string& path, bool initing);

  TcpConnection * FindProvider(const std::string & path);  
  void OnConnect(const std::shared_ptr<TcpConnection>& conn); 
  void OnMessage(const std::shared_ptr<TcpConnection>& conn);

private:
  void HandleRequest(protoheader::RequestHeader & header,
                     std::string && all_buf,
                     uint32_t old_header_size,
                     const std::shared_ptr<TcpConnection> & client_conn);

  void HandleResponse(protoheader::ResponseHeader & header,
                      std::string && all_buf,
                      const std::shared_ptr<TcpConnection> & provider_conn);

  EventLoop main_reactor_;
  std::unique_ptr<RpcZkClient> zk_client_;
  std::unique_ptr<TcpServer> server_;

  std::unordered_map<std::string, std::vector<std::string> > path_node_; // key=path_method, val=all the ip:port
  std::unordered_map<std::string, std::shared_ptr<TcpConnection> > addr_conn_;// key=ip:prt val=conn, 不仅用于路由，还用于维护conn生命周期

  std::shared_mutex mtx_path_;
  std::shared_mutex mtx_addr_;
};
