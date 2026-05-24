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
  void OnZkChildEvent(const std::string & path,  int depth);

  std::shared_ptr<TcpConnection> FindProvider(const std::string & path, bool & is_probe);
  void OnConnect(const std::shared_ptr<TcpConnection>& conn); 
  void OnMessage(const std::shared_ptr<TcpConnection>& conn);

private:
  uint64_t NextContextId() {
    uint64_t id;
    do {
      id = ctx_id_gen_.fetch_add(1, std::memory_order_relaxed);
    } while (id == 0);
    return id;
  }

  void SendErrorResponse(int code, const std::string& msg,
                         const std::shared_ptr<TcpConnection>& conn);

  static constexpr long long kRpcTimeoutMs = 200; // 200ms
  std::atomic<uint64_t> ctx_id_gen_{1};

  void HandleRequest(protoheader::RequestHeader header,
                     std::string args,
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
