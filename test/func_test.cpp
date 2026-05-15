#include<iostream>
#include"User.pb.h"
#include<string>
#include "RpcApplication.h"

using std::cout;
using std::endl;

int main(int argc , char ** argv){
  RpcApplication::Init(argc, argv);
  auto t = RpcApplication::config().Load("proxy");
  cout<<t<<endl;
}
