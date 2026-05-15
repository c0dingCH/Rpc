#include "RpcChannel.h"
#include "Logging.h"
#include "Header.pb.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>


void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                            google::protobuf::RpcController* controller,
                            const google::protobuf::Message* request,
                            google::protobuf::Message* response,
                            google::protobuf::Closure* done) {
  const google::protobuf::ServiceDescriptor* serv_desc = method->service();
  std::string serv_name = serv_desc->name();
  std::string method_name = method->name();

  std::string args_str;
  if (!request->SerializeToString(&args_str)) {
    controller->SetFailed("serialize request error!");
    LOG_ERROR << "serialize request error!";
    return;
  }

  protoheader::RpcHeader rpc_header;
  rpc_header.set_serv_name(serv_name);
  rpc_header.set_method_name(method_name);

  std::string header_str;
  uint32_t header_size = 0;
  if (!rpc_header.SerializeToString(&header_str)) {
    controller->SetFailed("serialize header error!");
    LOG_ERROR << "serialize header error!";
    return;
  }
  header_size = header_str.size();

  uint32_t total_size = 8 + header_size + args_str.size();

  std::string msg = std::string((char*)&total_size, 4) + std::string((char*)&header_size, 4) + header_str + args_str;

  int serv_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (serv_fd == -1) {
    LOG_ERROR << "create socket fd error";
    return;
  }


  //固定的proxy地址，这里先表示为provider的地址


  std::string ip = "127.0.0.11";
  short port = 8888;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(ip.c_str());
  addr.sin_port = htons(port);

  if (connect(serv_fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
    LOG_ERROR << "connect to proxy error";
    return;
  }

  if (send(serv_fd, msg.c_str(), msg.size(), 0) == -1) {
    LOG_ERROR << "request send error";
    return;
  }

  char recv_buf[1024] = {0};
  ssize_t recv_bytes = 0;
  if ((recv_bytes = recv(serv_fd, recv_buf, 1024, 0)) == -1) {
    LOG_ERROR << "recv response error";
    return;
  }

  if (!response->ParseFromArray(recv_buf, recv_bytes)) {
    close(serv_fd);
    LOG_ERROR << "parse response error";
    return;
  }

  close(serv_fd);
}
