#pragma once
#include <atomic>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include "EventLoop.h"

class TcpConnection;
class RpcZkClient;

class RpcProxy{
public:
  RpcProxy();
  ~RpcProxy();

  void Run();

  void UpdateService(const std::string& path);

  TcpConnection * FindProvider(const std::string & path);  
  void OnConnect(const std::shared_ptr<TcpConnection>& conn); 
  void OnMessage(const std::shared_ptr<TcpConnection>& conn);

private:
  EventLoop main_reactor_;
  std::unique_ptr<RpcZkClient> zk_client_;

  std::unordered_map<std::string, std::vector<std::string> > path_node_;
  std::unordered_map<std::string, std::shared_ptr<TcpConnection> > addr_conn_;

  std::shared_mutex mtx_path_;
  std::shared_mutex mtx_addr_;
};
