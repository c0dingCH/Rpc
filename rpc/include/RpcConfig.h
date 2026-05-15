#pragma once

#include <unordered_map>
#include <string>

class RpcConfig{
public:
  bool LoadConfigFile(const char * path_confile);
    
  std::string Load(const std::string & key); 

private:
  std::unordered_map<std::string , std::string> configs_;
};


