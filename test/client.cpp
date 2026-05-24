#include "RpcChannel.h"
#include "User.pb.h"
#include <iostream>


int main(){
  test::LoginRequest request;
  request.set_name("cheng");
  request.set_pwd("123");
  
  test::LoginResponse response;
  test::UserService_Stub stub(new RpcChannel());

  stub.Login(nullptr,&request, &response, nullptr);

  //std::cout<< response.success()<<std::endl;  

}


