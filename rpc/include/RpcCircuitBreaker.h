#pragma once
#include <atomic>
#include <queue>

class TcpConnection;
class EventLoop;
class Timer;

class RpcCircuitBreaker {
public:
  enum State { kUp, kDown, kHalfOpen };

  explicit RpcCircuitBreaker(TcpConnection* conn);
  ~RpcCircuitBreaker();


  void Record(bool success);
  void RecordTimeout();
  void RecordSuccess();

  bool IsUp() const { return state_ == kUp; }
  bool IsHalfOpen() const { return state_ == kHalfOpen; }
  bool TryAcquire(bool & half_open);
  void ResetProbeSent() { probe_sent_.store(false, std::memory_order_release); }
  void ProbeSucceeded();
  void ProbeFailed();


  static constexpr int kConsecutiveTimeoutThreshold = 10; // 连续失败的次数阈值
  static constexpr int kWindowSize = 100; // 窗口大小
  static constexpr int kMinWindowForRatio = 30; // 失败率计算前提是窗口 > 10

  static constexpr double kFailureRatio = 0.5; // 失败率
  static constexpr double kHalfOpenDelay = 0.3; // 熔断窗口时间

private:
  void SetDown();
  void SetUp();
  void SetHalfOpen();

  void CheckThreshold();
  void PushResult(bool success);
  void ScheduleHalfOpen();
  void CancelHalfOpenTimer();

  State state_{kUp};
  int consecutive_timeouts_{0}; // 连续的timeout
  std::atomic<bool> probe_sent_{false};

  std::queue<bool> window_; // 窗口
  int window_failures_{0};

  TcpConnection* conn_{nullptr};
  Timer* half_open_timer_{nullptr};// 熔断窗口期定时器
};
