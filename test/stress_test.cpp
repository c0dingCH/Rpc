#include "RpcChannel.h"
#include "User.pb.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <getopt.h>

struct Result {
  std::vector<long long> latencies_us;
  long long errors{0};
};

static void worker(int requests, Result * res) {
  for (int i = 0; i < requests; ++i) {
    test::LoginRequest req;
    req.set_name("cheng");
    req.set_pwd("123");

    test::LoginResponse rsp;
    auto * channel = new RpcChannel();
    test::UserService_Stub stub(channel);

    auto start = std::chrono::steady_clock::now();
    stub.Login(nullptr, &req, &rsp, nullptr);
    auto end = std::chrono::steady_clock::now();

    if (rsp.success()) {
      long long us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
      res->latencies_us.push_back(us);
    } else {
      res->errors++;
    }

    delete channel;
  }
}

int main(int argc, char ** argv) {
  int threads = 16;
  int requests = 10000;

  int opt;
  while ((opt = getopt(argc, argv, "t:r:")) != -1) {
    switch (opt) {
      case 't': threads = atoi(optarg); break;
      case 'r': requests = atoi(optarg); break;
      default:
        std::cerr << "Usage: " << argv[0] << " [-t threads] [-r requests]\n";
        return 1;
    }
  }

  int per_thread = requests / threads;
  int remainder = requests % threads;

  std::vector<std::thread> ths;
  std::vector<Result> results(threads);

  auto t_start = std::chrono::steady_clock::now();

  for (int i = 0; i < threads; ++i) {
    int n = per_thread + (i < remainder ? 1 : 0);
    ths.emplace_back(worker, n, &results[i]);
  }

  for (auto & t : ths) t.join();

  auto t_end = std::chrono::steady_clock::now();
  long long total_us = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();

  std::vector<long long> all_lat;
  long long total_errors = 0;
  for (auto & r : results) {
    all_lat.insert(all_lat.end(), r.latencies_us.begin(), r.latencies_us.end());
    total_errors += r.errors;
  }

  std::sort(all_lat.begin(), all_lat.end());
  size_t n = all_lat.size();

  double total_sec = total_us / 1000000.0;
  double avg_us = n ? std::accumulate(all_lat.begin(), all_lat.end(), 0LL) / (double)n : 0;

  auto p = [&](double pt) -> long long {
    if (n == 0) return 0;
    return all_lat[size_t(pt * (n - 1) / 100.0)];
  };

  std::cout << "\n===== Stress Test Results =====\n";
  std::cout << "Requests:      " << requests << "\n";
  std::cout << "Threads:       " << threads << "\n";
  std::cout << "Errors:        " << total_errors << "\n";
  std::cout << "Total time:    " << total_sec << "s\n";
  std::cout << "QPS:           " << (total_sec > 0 ? (int)(requests / total_sec) : 0) << "\n";
  std::cout << "Avg latency:   " << avg_us / 1000.0 << "ms\n";
  std::cout << "P50 latency:   " << p(50) / 1000.0 << "ms\n";
  std::cout << "P90 latency:   " << p(90) / 1000.0 << "ms\n";
  std::cout << "P99 latency:   " << p(99) / 1000.0 << "ms\n";
  std::cout << "Max latency:   " << (n ? all_lat.back() : 0) / 1000.0 << "ms\n";

  return total_errors ? 1 : 0;
}
