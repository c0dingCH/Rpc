#include "RpcApplication.h"
#include "RpcConfig.h"
#include "Logging.h"


#include <unistd.h>
#include <iostream>
using std::cout;
using std::endl;

RpcConfig RpcApplication::config_;

static void ShowConfigHelp(){
  cout<< "config help:"<<endl<<endl;
  cout<<"one line limit char 512 (include '\\n')" << endl;
  cout<<"formate : key=value"<<endl;
  cout<<"row notes begin with '#' " << endl;
}

static void ShowArgsHelp(){
  cout<<"formage : command -i <configfile>" << endl;
}

void RpcApplication::Init(int argc, char **argv){
  if(argc < 2){
    ShowArgsHelp();
    exit(EXIT_FAILURE);
  }

  int c = 0;
  std::string path_config_file;
  while((c = getopt(argc, argv, "i:")) != -1){
    switch(c){
      case 'i':
        path_config_file = optarg;
        break;
      case '?':
        ShowArgsHelp();
        exit(EXIT_FAILURE);
      case ':':
        ShowArgsHelp();
        exit(EXIT_FAILURE);
      default:
        break;
    }
  }
  
  if(!config_.LoadConfigFile(path_config_file.c_str())){
    ShowConfigHelp(); 
    exit(EXIT_FAILURE);
  }
  LOG_INFO << "config load ok";
}
