#pragma once

class RpcLoadScore {
public:
  RpcLoadScore() = default;

  void EnLoad();
  void DeLoad();
  void AddCnt();
  void AddLoad(long long rt);
  void Timeout();

  void Reset(){ ewma_cnt_ = ewma_rt_ = 0; }
  double Load() const { 
    if(ewma_cnt_ < 1e-6 || ewma_rt_ < 1e-6) return kNoneRequest;
    return weigth_ / (ewma_cnt_ * ewma_rt_); 
  }


  static constexpr double kAlpha = 0.2;
  static constexpr double kMaxWeight = 50.0;
  static constexpr double kNoneRequest = 1e14;

private:
  int active_cnt_{0};
  double ewma_cnt_{0};
  double ewma_rt_{0};
  double weigth_{20};
};
