#pragma once

#include <string>
#include <zookeeper/zookeeper.h>


class RpcZkClient{
public:
  RpcZkClient();
  ~RpcZkClient();
  
  void Start();

  void Create(const char * path, const char * data, int len, int state = 0);

  std::string GetData(const char * path);

private:
  zhandle_t * zhandle_{nullptr};

};
