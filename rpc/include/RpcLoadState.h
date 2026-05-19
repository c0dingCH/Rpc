#pragma once

class TcpConnection;
class Timer;

class RpcLoadState{
public:
  enum State { kUp, kDown, kHalfOpen };

  explicit RpcLoadState(TcpConnection * conn) : conn_(conn) {}
  ~RpcLoadState();

  void EnLoad() {
    active_cnt_++;
    ewma_cnt_ = active_cnt_ * kAlpha + (1 - kAlpha) * ewma_cnt_;
    total_reqs_++;
    CheckThreshold();
  }

  void DeLoad() {
    active_cnt_--;
    ewma_cnt_ = active_cnt_ * kAlpha + (1 - kAlpha) * ewma_cnt_;
    CheckThreshold();
  }

  void AddLoad(long long rt, bool timeout);

  double Load() const { return ewma_cnt_ * ewma_rt_; }
  bool operator<(const RpcLoadState & rhs) const { return Load() < rhs.Load(); }

  bool IsUp() const { return state_ == kUp; }
  bool IsDown() const { return state_ == kDown; }
  bool IsHalfOpen() const { return state_ == kHalfOpen; }

  void SetDown();
  void SetHalfOpen(long long when) { state_ = kHalfOpen; when_probe_ = when; }
  void SetUp();

  void SendProbe();
  void HandleProbeResponse();

  static constexpr double kAlpha = 0.2;
  static constexpr double kProbeInterval = 1.250; // 0.5 ~ 2  ,  2 ~ 5   si

  static constexpr int kTimeoutThreshold = 5; // 3 ~ 5 times
  static constexpr double kEwmaCntThreshold = 8.0; // cpu.corn * 2

private:
  void CheckThreshold();
  void ScheduleProbe();
  void CancelProbeTimer();

  State state_{kUp};
  int timeout_cnt_{0};
  int active_cnt_{0};
  double ewma_rt_{0};
  double ewma_cnt_{0};
  long long when_probe_{0};

  long long total_reqs_{0};
  long long total_timeouts_{0};
  long long completed_cnt_{0};
  double avg_rt_{0};

  TcpConnection * conn_{nullptr};
  Timer * probe_timer_{nullptr}; 
  // 这里为了避免conn_ 关闭导致指针悬空，析构的时候会cancel timer
  // 所以timer执行的时候conn一定是活着的，否则timer就被cancel了                    


  // 差一个动态权重变化， 在负载均衡中作为分子， 权重参与的负载均衡是socre = weigth(权重) / load(负载)， score越大越好， 当前负载判断是load越小越好

};
