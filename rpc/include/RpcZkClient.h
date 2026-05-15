#pragma once

#include <string>
#include <zookeeper/zookeeper.h>
#include <vector>

class RpcZkClient{
public:
  typedef void (*watcher)(zhandle_t * zh, int type, int state, const char * path, void * ctx);

  RpcZkClient();
  RpcZkClient(watcher global_watcher, void * ctx);

  ~RpcZkClient();
  
  void Start();

  bool Exists(const char * path, bool watch = false);

  void Create(const char * path, const char * data, int len, int state = 0);

  std::vector<std::string> GetChildrens(const char * path, bool watch = false);
  std::string GetData(const char * path, bool watch = false);

private:
  zhandle_t * zhandle_{nullptr};
  watcher global_watcher_;
  void * ctx_{nullptr};

};
