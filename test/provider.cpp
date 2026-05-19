#include "RpcProvider.h"
#include "User.pb.h"
#include "RpcApplication.h"

#include <iostream>

class UserService : public test::UserService{
public:
  void Login(google::protobuf::RpcController* controller,
             const test::LoginRequest* request,
             test::LoginResponse* response,
             google::protobuf::Closure* done) {
    if (request->name() == "cheng" && request->pwd() == "123") {
      response->set_success(true);
    }

    done->Run();
  }

};


int main(int argc, char **argv){
  RpcApplication::Init(argc,argv);

  RpcProvider pr;
  pr.NotifyService(new UserService());
  
  pr.Run();
}





