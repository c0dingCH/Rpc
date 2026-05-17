#pragma once
#include <google/protobuf/service.h>
#include <functional>

class RpcClosure: public google::protobuf::Closure {
public:
  explicit RpcClosure(const std::function<void()> cb) : cb_(cb) {}

  void Run() override{
    cb_();
    delete this;
  }

private:
  std::function<void()> cb_;

};


