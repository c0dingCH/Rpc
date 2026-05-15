#include "RpcProxy.h"
#include "RpcZkClient.h"
#include "TcpServer.h"
#include "EventLoop.h"
#include "TcpConnection.h"
#include "Logging.h"
#include "Buffer.h"
#include "Header.pb.h"
#include "RpcLoadState.h"

#include <arpa/inet.h>

static void ProxyGlobalWatcher(zhandle_t * zh, int type, int state,
                                const char * path, void * ctx) {
  if (type == ZOO_CHILD_EVENT) {
    ((RpcProxy *)ctx)->UpdateService(path);
  }
}

RpcProxy::RpcProxy()
  : zk_client_(std::make_unique<RpcZkClient>(ProxyGlobalWatcher, this))
{}

RpcProxy::~RpcProxy()
{}

void RpcProxy::Run() {
  zk_client_->Start();

  std::string root = "/rpc";
  auto services = zk_client_->GetChildrens(root.c_str(), false);
  for (auto & service : services) {
    std::string serv_path = root + "/" + service;
    auto methods = zk_client_->GetChildrens(serv_path.c_str(), false);
    for (auto & method : methods) {
      std::string method_path = serv_path + "/" + method;
      UpdateService(method_path);
    }
  }

  TcpServer server("127.0.0.10", 8888, &main_reactor_);
  server.OnConnect(std::bind(&RpcProxy::OnConnect, this, std::placeholders::_1));
  server.OnMessage(std::bind(&RpcProxy::OnMessage, this, std::placeholders::_1));
  server.Start();
  main_reactor_.Loop();
}

void RpcProxy::UpdateService(const std::string & path) {
  auto nodes = zk_client_->GetChildrens(path.c_str(), true);

  {
    std::unique_lock lock(mtx_path_);
    path_node_[path] = nodes;
  }
}

TcpConnection * RpcProxy::FindProvider(const std::string & path) {
  std::shared_lock lock_path(mtx_path_);
  auto it = path_node_.find(path);
  if (it == path_node_.end() || it->second.empty()) return nullptr;
  auto nodes = it->second;
  lock_path.unlock();

  std::shared_lock lock_addr(mtx_addr_);
  TcpConnection * best = nullptr;
  double best_rt = 0;
  for (auto & node_name : nodes) {
    auto conn_it = addr_conn_.find(node_name);
    if (conn_it != addr_conn_.end()) {
      TcpConnection * cur = conn_it->second.get();
      double rt = cur->GetLoadState()->rt();
      if (!best || rt < best_rt) {
        best = cur;
        best_rt = rt;
      }
    }
  }
  return best;
}

void RpcProxy::OnConnect(const std::shared_ptr<TcpConnection> & conn) {
  if (conn->GetState() == TcpConnection::State::kConnected) {
    conn->SetRole(TcpConnection::Role::kClient);
    LOG_INFO << "proxy client: " << conn->GetAddr();
  } 
  else {
    if (conn->IsClient()) {
      auto res = conn->GetRes();
      if (auto * cr = std::get_if<TcpConnection::ClientRes>(&res)) {
        cr->loop->RunOneFunc([id = cr->id, loop = cr->loop]() {
          loop->RemoveContext(id);
        });
      }
    } 
    else if (conn->IsProvider()) {
      auto res = conn->GetRes();
      if (auto * pr = std::get_if<TcpConnection::ProviderRes>(&res)) {
        EventLoop * loop = conn->GetLoop();
        for (auto id : pr->ids) {
          loop->RemoveContext(id);
        }
      }
      std::unique_lock lock(mtx_addr_);
      addr_conn_.erase(conn->GetAddr());
    }
  }
}

void RpcProxy::OnMessage(const std::shared_ptr<TcpConnection> & conn)
{}
