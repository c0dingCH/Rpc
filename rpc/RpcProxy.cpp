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
#include <iomanip>

void ProxyGlobalWatcher(zhandle_t * zh, int type, int state,
                        const char * path, void * ctx) {

  if(type == ZOO_SESSION_EVENT && state == ZOO_CONNECTED_STATE){
    sem_t * sem = (sem_t *)ctx;
    sem_post(sem);
  }
  else if (type == ZOO_CHILD_EVENT) {
    int depth = 0, len = strlen(path);
    for(int i = 0;i < len; i++)depth += path[i] == '/';
    ((RpcProxy *)ctx)->OnZkChildEvent(path, depth);
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
  auto services = zk_client_->GetChildrens(root.c_str(), true);
  for (auto & service : services) {
    std::string serv_path = root + "/" + service;
    auto methods = zk_client_->GetChildrens(serv_path.c_str(), true);
    for (auto & method : methods) {
      std::string method_path = serv_path + "/" + method;
      UpdateService(method_path, true);
    }
  }

  main_reactor_.Loop();
}


void RpcProxy::UpdateService(const std::string & path, bool initing) { // Run的逻辑就是，对于UpdateService的path就是method
  std::cout << path<< " "<<"update" << std::endl;
  auto addrs = zk_client_->GetChildrens(path.c_str(), true); // 监听第 3 层的children
  if(!addrs.size())return;

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


void RpcProxy::OnZkChildEvent(const std::string & path, int depth) {
  if (depth == 3) {
    UpdateService(path, false);
  } 
  else {
    auto children = zk_client_->GetChildrens(path.c_str(), true);
    for (auto & child : children){
      OnZkChildEvent(path + "/" + child, depth + 1);
    }
  }
}


std::shared_ptr<TcpConnection> RpcProxy::FindProvider(const std::string & path) { // 这里读load信息不需要要锁，大致就好了
  std::shared_lock lock_path(mtx_path_);
  auto it = path_node_.find(path);
  if (it == path_node_.end() || it->second.empty()) return nullptr;
  auto nodes = it->second;
  lock_path.unlock();

  std::shared_lock lock_addr(mtx_addr_);
  std::shared_ptr<TcpConnection> best = nullptr;
  double best_rt = 0;
  for (auto & node_name : nodes) {
    auto conn_it = addr_conn_.find(node_name);
    if (conn_it != addr_conn_.end()) {
      double rt = conn_it->second->GetLoadState()->rt();
      

      //std::cout<<std::fixed<<std::setprecision(10) << rt<<std::endl;
      if (!best || rt < best_rt) {
        best = conn_it->second;
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
      uint64_t context_id = conn->GetContextId();
      if (context_id != 0) {
        EventLoop * client_loop = conn->GetLoop();
        auto ctx = client_loop->GetContext(context_id); 
        if (ctx && ctx->provider_conn) {
          auto * provider_conn = ctx->provider_conn.get();
          provider_conn->GetLoop()->RunOneFunc([provider_conn, context_id]() {
            provider_conn->RemoveCtxLoop(context_id);
          });
        }
        client_loop->RemoveContext(context_id);// context的维护和clent的reactor是同一个，是安全的
        conn->SetContextId(0);
      }
    }
    else if (conn->IsProvider()) {
      // provider 异常关闭，不做额外处理, client请求超时重试就行了 
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
        SendErrorResponse(4, "parse RequestHeader error", conn);
        return;
      }
      HandleRequest(std::move(header), std::move(all_buf), header_size, conn);
    }
    else{
      protoheader::ResponseHeader header;
      if(!header.ParseFromString(all_buf.substr(8, header_size))){
        LOG_ERROR << "parse ResponseHeader error";
        SendErrorResponse(5, "parse ResponseHeader error", conn);
        return;
      }
      HandleResponse(header, std::move(all_buf), conn);
    }
  }
}

void RpcProxy::SendErrorResponse(int code, const std::string& msg,
                                  const std::shared_ptr<TcpConnection>& conn) {
  protoheader::ResponseHeader resp_header;
  resp_header.set_code(code);
  resp_header.set_msg(msg);

  std::string header_str;
  resp_header.SerializeToString(&header_str);

  uint32_t header_size = header_str.size();
  uint32_t total_size = 8 + header_size;

  std::string packet;
  packet.append((char*)&total_size, 4);
  packet.append((char*)&header_size, 4);
  packet.append(header_str);

  conn->Send(packet);
}

void RpcProxy::HandleRequest(protoheader::RequestHeader header,
                             std::string all_buf,
                             uint32_t old_header_size,
                             const std::shared_ptr<TcpConnection> & client_conn) {
  // 始终在 client 的 loop 上执行（由 OnMessage 或 timer 回调调用）
  EventLoop * client_loop = client_conn->GetLoop();

  uint64_t context_id = client_conn->GetContextId();
  auto ctx = context_id ? client_loop->GetContext(context_id) : nullptr;

  // --- 重试处理（原地更新 context，不删重建） ---
  if (ctx && ctx->provider_conn) {
    auto * old_provider = ctx->provider_conn.get();

    if (ctx->cnt >= 2) {
      old_provider->GetLoop()->RunOneFunc([old_provider, context_id]() {
        old_provider->RemoveCtxLoop(context_id);
      });
      client_loop->RemoveContext(context_id);
      client_conn->SetContextId(0);
      SendErrorResponse(6, "over retry times", client_conn);
      return;
    }

    // 更新旧 provider 负载 + 清除映射
    old_provider->GetLoop()->RunOneFunc([old_provider, context_id]() {
      old_provider->GetLoadState()->EnLoad();
      old_provider->GetLoadState()->AddLoad(kRpcTimeoutMs);
      old_provider->GetLoadState()->DeLoad();
      old_provider->RemoveCtxLoop(context_id);
    });
  }

  // --- 找 provider ---
  std::string path = "/rpc/" + header.service() + "/" + header.method();
  auto provider_conn = FindProvider(path);
  if (!provider_conn) {
    if (context_id != 0) {
      client_loop->RemoveContext(context_id);
      client_conn->SetContextId(0);
    }
    SendErrorResponse(5, "no provider", client_conn);
    return;
  }

  // --- 创建 timer ---
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long now = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;

  Timer * timer = client_loop->RunAfter(kRpcTimeoutMs / 1000.0,
      [this, header, all_buf, old_header_size, client_conn]() mutable {
        HandleRequest(std::move(header), std::move(all_buf), old_header_size, client_conn);
      });

  // --- 创建/更新 context（重试时原地更新，避免删重建） ---
  if (ctx && ctx->provider_conn) {
    ctx->provider_conn = provider_conn;
    ctx->when = now;
    ctx->timer = timer;
    ctx->cnt++;
  }
  else {
    context_id = client_loop->AddContext(client_conn, provider_conn, now, timer);
    client_conn->SetContextId(context_id);
  }

  // --- 准备报文 ---
  header.set_context_id(context_id);
  std::string new_header_str;
  header.SerializeToString(&new_header_str);
  all_buf.replace(8, old_header_size, new_header_str);

  // --- 在 provider 的 loop 上执行：EnLoad + 注册映射 + 发送 ---
  provider_conn->GetLoop()->RunOneFunc([provider_conn, context_id, client_loop,
                                        all_buf = std::move(all_buf)]() {
    provider_conn->GetLoadState()->EnLoad();
    provider_conn->SetCtxLoop(context_id, client_loop);
    provider_conn->Send(std::move(all_buf));
  });
}

void RpcProxy::HandleResponse(protoheader::ResponseHeader & header,
                              std::string && all_buf,
                              const std::shared_ptr<TcpConnection> & provider_conn) {
  uint64_t context_id = header.context_id();
  EventLoop * client_loop = provider_conn->GetCtxLoop(context_id);
  if (!client_loop) {
    LOG_ERROR << "the provider's response overtime " << context_id; // 如果是超时， deload等操作在超时回调的时候有做处理
    return;
  }

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long now = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;

  // 切到 client 的 loop 检查 context 有效性
  client_loop->RunOneFunc([this, context_id, client_loop, provider_conn, all_buf = std::move(all_buf), now]() {
    auto ctx = client_loop->GetContext(context_id);
    if (!ctx || ctx->provider_conn.get() != provider_conn.get()) {
      LOG_ERROR << "the provider's response overtime " << context_id; // 如果是超时， deload等操作在超时回调的时候有做处理
      return;
    }

    long long rt = now - ctx->when;
    auto client_conn = ctx->client_conn;

    // 清理 client 侧的 context
    client_loop->RemoveContext(context_id);
    client_conn->SetContextId(0);

    // 更新 provider 负载 + 清除映射（在 provider 的 loop 上）， 如果超时了，就由client端来更新负载了
    provider_conn->GetLoop()->RunOneFunc([provider_conn, rt, context_id]() {
      provider_conn->GetLoadState()->AddLoad(rt);
      provider_conn->GetLoadState()->DeLoad();
      provider_conn->RemoveCtxLoop(context_id);
    });

    // 转发响应给 client
    client_conn->Send(all_buf);
  });
}
