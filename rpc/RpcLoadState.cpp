#include "RpcLoadState.h"

void RpcLoadState::AddLoad(long long rt){
  if(rt_ < 1e-6) rt_ = static_cast<double>(rt);
  else rt_ = rt * alpha_ + (1 - alpha_) * rt_;
}
