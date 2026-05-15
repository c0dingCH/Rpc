#pragma once
#include "RpcConfig.h"

class RpcApplication{
public:
  static void Init(int argc, char **argv);
  static RpcApplication& instance() { static RpcApplication rpc_application; return rpc_application; };
  static RpcConfig& config() { return config_; };

private:
  static RpcConfig config_;

  RpcApplication(){};
  RpcApplication(const RpcApplication&) = delete;
  RpcApplication(RpcApplication&&) = delete;
};

