#pragma once
#include <google/protobuf/service.h>
#include <string>
class ProviderController : public google::protobuf::RpcController {
public:
  ProviderController() = default;
  
  void Reset() override {
    code_ = 0;
    msg_.clear();
    canceled_ = false;
  }
  
  bool Failed() const override {
    return code_ != 2;
  }
  
  std::string ErrorText() const override {
    return msg_;
  }
  
  void SetFailed(const std::string& reason) override {
    code_ = -1;
    msg_ = reason;
  }
  
  void StartCancel() override {
    canceled_ = true;
    if (cancel_cb_) cancel_cb_->Run();
  }
  
  bool IsCanceled() const override {
    return canceled_;
  }
  
  void NotifyOnCancel(google::protobuf::Closure* cb) override {
    cancel_cb_ = cb;
  }
  
  void set_request_id(uint64_t request_id) { request_id_ = request_id; }
  void set_context_id(uint64_t context_id) { context_id_ = context_id; }
  uint64_t request_id() { return request_id_ ; }
  uint64_t context_id() { return context_id_ ; }
  int code(){ return code_; }
  std::string msg(){ return msg_; }

private:
  int code_{0}; // 0 invalid  1 info  2 = ok , 3 transport , 4 clienterror , 5 servererror
  std::string msg_;
  bool canceled_{false};
  google::protobuf::Closure* cancel_cb_{nullptr};
  
  uint64_t request_id_{ 0 };
  uint64_t context_id_{ 0 };
};
  
