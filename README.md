# Rpc

一个从零构建的自定义 C++ 透明代理 RPC 框架。

## 架构

```
+--------+     TCP      +--------+     TCP      +-----------+
| Client | -----------> | Proxy  | -----------> | Provider  |
|        | <----------- |        | <----------- |           |
+--------+   :8888      +--------+   :8888      +-----------+
                           |                          |
                           | ZooKeeper watch          | ZooKeeper register
                           v                          v
                       +-----------+             /rpc/{Service}/{Method}/{addr}
                       | ZooKeeper |
                       |  :2181    |
                       +-----------+
```

Client → Proxy → Provider 三层模型。Proxy 作为透明代理，客户端无需知道提供者地址。

## 核心特性

- **透明代理路由** — 客户端只需连接 Proxy，无需感知后端 Provider
- **自适应负载均衡** — 基于 EWMA（α=0.2）的响应时间 + 活跃请求数综合评分，评分越高表示越空闲
- **熔断器** — 连续 10 次超时或 50% 故障率触发断开；300ms 后进入半开状态，允许单个探针自动恢复
- **超时重试** — 默认 200ms 超时，最多重试 2 次，指数退避（1.8x–2.2x，上限 5s）
- **服务发现** — 基于 ZooKeeper 的临时节点注册 + Watch 机制，自动感知 Provider 上下线
- **自定义 TCP 网络层** — 边缘触发 epoll、多 Reactor（主从 EventLoop）、非阻塞 I/O
- **自定义二进制协议** — `[total_size][header_size][protobuf_header][protobuf_body]`
- **异步日志** — 双缓冲设计，后台线程写文件

## 构建

**依赖**：C++17、g++、CMake ≥ 3.10、protobuf、zookeeper_mt、pthreads

```sh
mkdir -p build && cd build && cmake .. && make -j$(nproc)
```

**编译产物**：
- `build/libchat.so` — 共享库
- `build/test/provider`、`build/test/proxy`、`build/test/client`、`build/test/func_test`

## 运行

4 个终端按顺序启动：

| 步骤 | 终端 | 命令 |
|------|------|------|
| 1 | ZooKeeper | `zkServer.sh start` |
| 2 | Provider | `build/test/provider -i test/p1_config` |
| 3 | Proxy | `build/test/proxy` |
| 4 | Client | `build/test/client` |

配置加载测试：`build/test/func_test -i test/config`

## 性能

**硬件环境**：VMware Ubuntu，1×4 CPU

### Proxy 裸测（wrk）

| 场景 | QPS | Transfer/s |
|------|-----|------------|
| 场景一 | 12w | 123.76 MB/s |
| 场景二 | 15w | 10 MB/s |

### Proxy 裸测（手动：短连接 + 18 线程 + 4000 请求）

收到请求直接回复，QPS ≈ 8000

### 完整 RPC 链路（1 Proxy → 3 Provider，同一服务）

处理登录请求，QPS ≈ 5000

### Provider 模拟处理延迟

`rand() % 50ms` 内随机 sleep，10 组测试（负载不刷新），取 QPS 平均值：

| 配置 | QPS |
|------|-----|
| 不加熔断器 | 401 |
| 加熔断器 | 600 |

## 项目结构

```
├── rpc/          RPC 层
│   ├── include/  RpcProvider / RpcProxy / RpcChannel / RpcZkClient / RpcLoadScore / RpcCircuitBreaker / ...
│   ├── pbs/      Protobuf 定义与生成代码
│   └── *.cpp
├── tcp/          自定义 TCP 网络层（EventLoop / TcpServer / TcpConnection / Acceptor / Poller）
├── util/         工具库（Buffer / 日志 / 定时器）
├── base/         基础原语（CurrentThread / Latch）
├── test/         测试文件与示例 proto
├── CMakeLists.txt
└── AGENTS.md     项目详细开发参考
```

## 关键技术细节

### 线缆协议

```
[0-3]    uint32_t total_size     total_size = 8 + header_size + body_size
[4-7]    uint32_t header_size
[8...]   header (protobuf)       RequestHeader / ResponseHeader
[...]    body (protobuf)         业务消息体
```

### 硬编码地址

| 组件 | 地址 |
|------|------|
| RpcProxy | `127.0.0.10:8888` |
| ZooKeeper | `127.0.0.1:2181` |
| RpcProvider | 通过 `-i <config>` 中的 `hp` 配置 |

### 配置格式

`key=value`，`#` 注释，行上限 512 字符。

```
# provider 配置示例
hp=127.0.0.11:8888
```

### 代码约定

- `DISALLOW_COPY` / `DISALLOW_COPY_AND_MOVE` 宏禁止拷贝
- `LOG_INFO` / `LOG_ERROR` / `LOG_FATAL` 宏记录日志
- 边缘触发 epoll（`TcpConnection` 默认启用 ET）
- 编译标志 `-g -Wall -Werror -DTHREADED`

## 关键类关系

| 类 | 职责 |
|---|---|
| `RpcProvider` | 注册服务、发布到 ZK、接收请求并调用用户实现 |
| `RpcProxy` | 发现 Provider、负载均衡、请求转发、熔断与重试 |
| `RpcChannel` | 客户端 Stub，封装请求序列化与 TCP 通信 |
| `RpcZkClient` | ZooKeeper 会话管理与节点操作 |
| `TcpServer` | TCP 服务器（Acceptor + EventLoop 线程池） |
| `EventLoop` | Reactor 核心（epoll + TimerQueue + 跨线程任务队列） |
| `RpcLoadScore` | EWMA 负载评分（活跃数 + 响应时间） |
| `RpcCircuitBreaker` | 熔断器（连续超时 / 故障率 → 断开 → 半开探针） |

详见 `AGENTS.md` 获取完整的开发参考。
