#include "RpcLoadScore.h"

#include <cstdlib>
#include <iostream>

void RpcLoadScore::EnLoad() {
  active_cnt_++;
  AddCnt();
}

void RpcLoadScore::DeLoad() {
  active_cnt_--;
  AddCnt();
}

void RpcLoadScore::AddCnt(){
  if (ewma_cnt_ < 1e-6) ewma_cnt_ = static_cast<double>(active_cnt_);
  else ewma_cnt_ = active_cnt_ * kAlpha + (1 - kAlpha) * ewma_cnt_;
}

void RpcLoadScore::AddLoad(long long rt) {
  if (ewma_rt_ < 1e-6) ewma_rt_ = static_cast<double>(rt);
  else ewma_rt_ = rt * kAlpha + (1 - kAlpha) * ewma_rt_;
  double v = 0.25 + static_cast<double>(rand()) / RAND_MAX * 0.5;
  weigth_ = std::min(weigth_ + v , kMaxWeight);
}

void RpcLoadScore::Timeout(){
  double v = 0.5 + static_cast<double>(rand()) / RAND_MAX * 0.5;
  weigth_ = std::max(0.0, weigth_ - v);
}


