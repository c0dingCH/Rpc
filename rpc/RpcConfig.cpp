#include "RpcConfig.h"
#include "Logging.h"

#include <iostream>

bool RpcConfig::LoadConfigFile(const char * path_config_file){
  FILE * file = fopen(path_config_file, "r");
  if(!file){
    LOG_ERROR << "No such config file: " << path_config_file;
    return false;
  }

  while(!feof(file)){
    char buf[512];
    fgets(buf, 512, file);
    
    if(*buf == '#' || *buf == '\0') continue;

    int it = 0;
    while(*(buf + it) == ' ')it++;
    std::string read_buf(buf + it);
    while(read_buf.back() == ' ' || read_buf.back() == '\n') read_buf.pop_back();
    
    int it_equal = read_buf.find('=');
    if(it_equal == -1){
      LOG_ERROR << "No '=' in a row";
      return false;
    }
    
    std::string key = read_buf.substr(0,it_equal);
    std::string val = read_buf.substr(it_equal + 1);
    
    configs_[key] = val;
  }

  return true;
}
    
std::string RpcConfig::Load(const std::string & key){
  auto it = configs_.find(key);
  if(it == configs_.end()){ 
    LOG_ERROR << "No such config";
    return "";
  }
  return it->second;
} 
