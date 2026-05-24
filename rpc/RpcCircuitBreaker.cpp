#include "RpcCircuitBreaker.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "Logging.h"

#include <iostream>

RpcCircuitBreaker::RpcCircuitBreaker(TcpConnection* conn)
  : conn_(conn) {}

RpcCircuitBreaker::~RpcCircuitBreaker() {
  CancelHalfOpenTimer();
}

void RpcCircuitBreaker::Record(bool success){
  if (success) RecordSuccess();
  else RecordTimeout();
}

void RpcCircuitBreaker::RecordTimeout() {
  if (state_ != kUp) return;
  consecutive_timeouts_++;
  PushResult(false);
  CheckThreshold();  // 这里注不注释表示是否开启熔断
}

void RpcCircuitBreaker::RecordSuccess() {
  if (state_ != kUp) return;
  consecutive_timeouts_ = 0;
  PushResult(true);
}

void RpcCircuitBreaker::ProbeSucceeded() {

  SetUp();
}

void RpcCircuitBreaker::ProbeFailed() {
  SetDown();
}

void RpcCircuitBreaker::SetDown() {
  if (state_ == kDown) return;
  state_ = kDown;
  consecutive_timeouts_ = 0;
  probe_sent_.store(false, std::memory_order_release);
  std::queue<bool>().swap(window_);
  window_failures_ = 0;
  ScheduleHalfOpen();
}

void RpcCircuitBreaker::SetUp() {
  state_ = kUp;
  consecutive_timeouts_ = 0;
  probe_sent_.store(false, std::memory_order_release);
  
  conn_->GetLoadScore()->Reset(); // Load 重新加载  
}

void RpcCircuitBreaker::SetHalfOpen() {
  if (state_ != kDown) return;
  state_ = kHalfOpen;
  probe_sent_.store(false, std::memory_order_release);
}

bool RpcCircuitBreaker::TryAcquire(bool & probe) {
  if (state_ == kDown) return false;
  if (state_ == kUp) return true;

  bool expected = false;
  if(probe_sent_.compare_exchange_strong(expected, true,std::memory_order_acq_rel, std::memory_order_acquire)){
    probe = true;
    return true;
  }
  return false;
}

void RpcCircuitBreaker::CheckThreshold() {
  if (state_ != kUp) return;

  if (consecutive_timeouts_ >= kConsecutiveTimeoutThreshold) {
    std::cout << "circuit breaker: consecutive timeouts " << consecutive_timeouts_ << std::endl;
    SetDown();
    return;
  }

  int total = window_.size();
  if (total > kMinWindowForRatio &&
      static_cast<double>(window_failures_) / total > kFailureRatio) {
    std::cout << "circuit breaker: failure ratio " << window_failures_ << "/" << total << std::endl;
    SetDown();
  }
}

void RpcCircuitBreaker::PushResult(bool success) {
  window_.push(success);
  if (!success) window_failures_++;

  if (window_.size() > kWindowSize) {
    if (!window_.front()) window_failures_--;
    window_.pop();
  }
}

void RpcCircuitBreaker::ScheduleHalfOpen() {
  CancelHalfOpenTimer();
  half_open_timer_ = conn_->GetLoop()->RunAfter(kHalfOpenDelay, [this]() {
    half_open_timer_ = nullptr;
    if (state_ != kDown) return;
    SetHalfOpen();
  });
}

void RpcCircuitBreaker::CancelHalfOpenTimer() {
  if (half_open_timer_) {
    conn_->GetLoop()->CancelTimer(half_open_timer_);
    half_open_timer_ = nullptr;
  }
}
