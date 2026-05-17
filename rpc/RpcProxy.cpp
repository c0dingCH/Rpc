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
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <fcntl.h>
#include <future>
#include <time.h>
#include <semaphore.h>

void ProxyGlobalWatcher(zhandle_t * zh, int type, int state,
                        const char * path, void * ctx) {

  if(type == ZOO_SESSION_EVENT && state == ZOO_CONNECTED_STATE){
    sem_t * sem = (sem_t *)ctx;
    sem_post(sem);
  }
  else if (type == ZOO_CHILD_EVENT) {
    ((RpcProxy *)ctx)->UpdateService(path, false);
  }
}


RpcProxy::RpcProxy(){
  zk_client_ = std::make_unique<RpcZkClient>(ProxyGlobalWatcher, this);
  server_ = std::make_unique<TcpServer>("127.0.0.10", 8888, &main_reactor_);
  server_ -> OnConnect(std::bind(&RpcProxy::OnConnect, this, std::placeholders::_1));
  server_ -> OnMessage(std::bind(&RpcProxy::OnMessage, this, std::placeholders::_1));

}

RpcProxy::~RpcProxy()
{}

void RpcProxy::Run() {
  zk_client_->Start();
  server_ -> Start();

  std::string root = "/rpc";
  auto services = zk_client_->GetChildrens(root.c_str(), false);
  for (auto & service : services) {
    std::string serv_path = root + "/" + service;
    auto methods = zk_client_->GetChildrens(serv_path.c_str(), false);
    for (auto & method : methods) {
      std::string method_path = serv_path + "/" + method;
      UpdateService(method_path, true);
    }
  }

  main_reactor_.Loop();
}


void RpcProxy::UpdateService(const std::string & path, bool initing) { // Run的逻辑就是，对于UpdateService的path就是method
  puts("update");
  auto addrs = zk_client_->GetChildrens(path.c_str(), true); // 监听第 3 层的children
  //node_name == ip:port
  {
    std::unique_lock lock(mtx_path_);
    path_node_[path] = addrs;
  }

  std::vector<std::string> pending_addrs;
  {
    std::shared_lock lock(mtx_addr_);
    for (auto & addr : addrs) {
      if (!addr_conn_.count(addr)) pending_addrs.push_back(addr);
    }
  }

  size_t n = pending_addrs.size();
  std::vector<std::promise<std::shared_ptr<TcpConnection>>> promises(n);
  std::vector<std::future<std::shared_ptr<TcpConnection>>> futures;
  futures.reserve(n);
  for(auto & p : promises)futures.push_back(p.get_future());

  for (size_t i = 0; i < n; i++) {
    auto &addr = pending_addrs[i];
    auto colon = addr.find(':');


    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    sockaddr_in saddr{};
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr(addr.substr(0, colon).c_str());
    saddr.sin_port = htons(std::stoi(addr.substr(colon + 1)));
    if(connect(fd, (sockaddr*)&saddr, sizeof(saddr)) == -1){
      LOG_ERROR << " the addr "<< addr << " provider isn't start"; 
    };
    fcntl(fd, F_SETFL, O_NONBLOCK); // 不要忘记非阻塞了

    auto create_provider_conn = [this, fd, p = &promises[i]]() {
      auto loop = server_->GetNextLoop();
      auto conn = std::make_shared<TcpConnection>(loop , fd, -1);
      conn->SetRole(TcpConnection::Role::kProvider);
      conn->SetOnCloseCallback(std::bind(&TcpServer::HandleCloseConnection, server_.get(), std::placeholders::_1));
      conn->SetOnConnectCallback(std::bind(&RpcProxy::OnConnect, this, std::placeholders::_1));
      conn->SetOnMessageCallback(std::bind(&RpcProxy::OnMessage, this, std::placeholders::_1));
      loop -> RunOneFunc(std::bind(&TcpConnection::ConnectionEstablished, conn));
      p -> set_value(std::move(conn));
    };

    if(initing) create_provider_conn();
    else main_reactor_.RunOneFunc(create_provider_conn);
  }

  if(n <= 0)return;

  {
    std::unique_lock lock(mtx_addr_);
    for (size_t i = 0; i < n; i++) {
      auto conn = futures[i].get();
      addr_conn_[pending_addrs[i]] = conn;
    }
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
    if (!conn->IsProvider()) conn->SetRole(TcpConnection::Role::kClient);
    LOG_INFO << "proxy client: " << conn->GetAddr();
  }
  if (conn->GetState() == TcpConnection::State::kClosed) {
    if (conn->IsClient()) {
      auto res = conn->GetRes();
      if (auto * cr = std::get_if<TcpConnection::ClientRes>(&res)) {
          if(cr->loop) cr->loop->RemoveContext(cr->context_id);
      }
    }
    else if (conn->IsProvider()) {
      auto res = conn->GetRes();
      if (auto * pr = std::get_if<TcpConnection::ProviderRes>(&res)) {
        EventLoop * loop = conn->GetLoop();
        for (auto context_id : pr->context_ids) {
          loop->RemoveContext(context_id);
        }
      }
  
      std::unique_lock lock(mtx_addr_);
      auto it = addr_conn_.find(conn->GetAddr());
      if(it == addr_conn_.end()){
        LOG_ERROR << "provider_conn remove error";
      }
      else addr_conn_.erase(it);
      
    }
  }
}


void RpcProxy::OnMessage(const std::shared_ptr<TcpConnection> & conn) {
  puts("on message");
  if(conn->GetState() != TcpConnection::State::kConnected) return;

  Buffer * buf = conn->GetReadBuffer();

  while(1){
    if(buf->readablebytes() < 4) return;
    std::string head_buffer = buf->PeekAsString(4);

    uint32_t total_size;
    head_buffer.copy((char *)&total_size, 4, 0);
    if(buf->readablebytes() < total_size) return;

    std::string all_buf = buf->RetrieveAsString(total_size);
    uint32_t header_size;
    all_buf.copy((char *)&header_size, 4, 4);

    if(conn->IsClient()){
      protoheader::RequestHeader header;
      if(!header.ParseFromString(all_buf.substr(8, header_size))){
        LOG_ERROR << "parse RequestHeader error";
        return;
      }
      HandleRequest(header, std::move(all_buf), header_size, conn);
    }
    else{
      protoheader::ResponseHeader header;
      if(!header.ParseFromString(all_buf.substr(8, header_size))){
        LOG_ERROR << "parse ResponseHeader error";
        return;
      }
      HandleResponse(header, std::move(all_buf), conn);
    }
  }
}

void RpcProxy::HandleRequest(protoheader::RequestHeader & header,
                             std::string && all_buf,
                             uint32_t old_header_size,
                             const std::shared_ptr<TcpConnection> & client_conn) {
  std::string path = "/rpc/" + header.service() + "/" + header.method();
  TcpConnection * provider_conn = FindProvider(path);
  if(!provider_conn){
    LOG_ERROR << "no provider for " << path;
    return;
  }
  // load count + 1
  provider_conn->GetLoadState()->EnLoad();

  // ctx 初始化
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long now = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;

  EventLoop * loop = provider_conn->GetLoop();
  auto ctx = std::make_unique<EventLoop::Context>(client_conn.get(), provider_conn, now);
  uint64_t context_id = loop->SetContext(std::move(ctx));

  //添加引用时资源
  provider_conn->AddContextId(context_id);
  client_conn->SetClientRes(loop, context_id);
  //注入ctx_id
  header.set_context_id(context_id);
  //序列化后替换
  std::string new_header_str;
  header.SerializeToString(&new_header_str);
  all_buf.replace(8, old_header_size, new_header_str);
  std::cout<< old_header_size <<" "<<new_header_str.size()<<std::endl;
  provider_conn->Send(std::move(all_buf));
}

void RpcProxy::HandleResponse(protoheader::ResponseHeader & header,
                              std::string && all_buf,
                              const std::shared_ptr<TcpConnection> & provider_conn) {
  EventLoop * loop = provider_conn->GetLoop();
  auto * ctx = loop->GetContext(header.context_id());
  if(!ctx){ // 说明client异常关闭，此ctx被释放了，不用管
    LOG_ERROR << "no context for context_id: " << header.context_id();
    return;
  }
  // 更新load
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long now = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
  long long rt = now - ctx->when;

  provider_conn->GetLoadState()->AddLoad(rt);
  provider_conn->GetLoadState()->DeLoad();
  //释放ctx 和 资源记录 
  provider_conn->RemoveContextId(header.context_id());

  
  auto caller = ctx->caller;
  caller->Send(std::move(all_buf));  
  
  caller->SetClientRes(nullptr, 0);
  loop -> RemoveContext(header.context_id());

}
