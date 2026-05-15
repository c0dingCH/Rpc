#pragma once
#include <google/protobuf/service.h>
#include <string>

class RpcController: {
public:
  void Reset(){ failed_ = false; error_text_.clear(); }
  
  bool failed(){ return failed_; }
  std::string error_msg(){ return error_msg_; }
  
  void SetFailed(const std::string &msg){ failed_ = false, error_msg_ = msg; }

  /**
  void StartCancel();
  bool IsCanceled() const;
  void NotifyOnCancel(google::protobuf::Closure * cb);
  **/

private:
  bool failed_{false};
  std::string error_msg_;

};

