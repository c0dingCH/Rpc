#include "RpcLoadState.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "Logging.h"

#include <cstring>

RpcLoadState::~RpcLoadState() {
  CancelProbeTimer();
}

void RpcLoadState::SetDown() {
  if (state_ == kDown) return;
  state_ = kDown;
  timeout_cnt_ = 0;
  ScheduleProbe();
}

void RpcLoadState::SetUp() {
  state_ = kUp;
  timeout_cnt_ = 0;
  CancelProbeTimer();
}

void RpcLoadState::ScheduleProbe() {
  CancelProbeTimer();
  probe_timer_ = conn_->GetLoop()->RunAfter(kProbeInterval, [this]() {
    probe_timer_ = nullptr; // 防止悬空
    if (state_ != kDown) return;
    SendProbe();
  });
}

void RpcLoadState::CancelProbeTimer() {
  if (probe_timer_) {
    conn_->GetLoop()->CancelTimer(probe_timer_);
    probe_timer_ = nullptr; // 防止悬空
  }
}

void RpcLoadState::AddLoad(long long rt, bool timeout) {
  if (ewma_rt_ < 1e-6) ewma_rt_ = static_cast<double>(rt);
  else ewma_rt_ = rt * kAlpha + (1 - kAlpha) * ewma_rt_;

  completed_cnt_++;
  avg_rt_ = (avg_rt_ * (completed_cnt_ - 1) + rt) / completed_cnt_;

  if (timeout) {
    timeout_cnt_++;
    total_timeouts_++;
  } else {
    timeout_cnt_ = 0;
  }

  CheckThreshold();
}

void RpcLoadState::CheckThreshold() {
  if (state_ != kUp) return;
  if (timeout_cnt_ >= kTimeoutThreshold
    || ewma_cnt_ >= kEwmaCntThreshold
    || (completed_cnt_ > 0 && ewma_rt_ >= 2 * avg_rt_)) {
    SetDown();
  }
}

void RpcLoadState::SendProbe() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long now = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
  SetHalfOpen(now);

  uint32_t total_size = 8;
  uint32_t header_size = 0;
  std::string packet;
  packet.append((char*)&total_size, 4);
  packet.append((char*)&header_size, 4);
  conn_->Send(std::move(packet));
}

void RpcLoadState::HandleProbeResponse() {
  if (state_ != kHalfOpen) return;

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long now = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
  long long probe_rt = now - when_probe_;

  double threshold = (completed_cnt_ > 0) ? avg_rt_ * 2 : 1000.0;
  if (probe_rt < threshold) {
    ewma_cnt_ = active_cnt_;
    ewma_rt_ = probe_rt;
    SetUp();
  } 
  else {
    SetDown();
  }
}
