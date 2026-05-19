# AGENTS.md

## Project: Custom C++ RPC Framework

A transparent-proxy RPC framework built from scratch. Client → Proxy → Provider.

## Architecture

- **`rpc/`** — RPC layer: `RpcProvider` (service server), `RpcProxy` (transparent proxy), `RpcChannel` (client stub), `RpcZkClient` (ZooKeeper wrapper)
- **`tcp/`** — Custom TCP networking: epoll-based `EventLoop`, `TcpServer`, `TcpConnection`, `Acceptor`, `Poller`
- **`util/`** — `Buffer`, async `Logging`, `Timer`/`TimerQueue`
- **`base/`** — `CurrentThread`, `Common.h` (DISALLOW_COPY macros), `Latch`
- **`rpc/pbs/`** — Protobuf definitions and generated code

## Hardcoded addresses (not configurable)

| Component   | Address            |
|-------------|--------------------|
| RpcProvider | `127.0.0.11:8888`  |
| RpcProxy    | `127.0.0.10:8888`  |
| ZooKeeper   | `127.0.0.1:2181`   |

## Build & dependencies

- C++17, g++, CMake ≥3.10, `-Wall -Werror -DTHREADED`
- Requires: `protobuf`, `zookeeper_mt`, pthreads
- Output: `libchat.so` + test binaries in `build/test/`

```sh
mkdir -p build && cd build && cmake .. && make -j$(nproc)
```

## Wire protocol (custom binary over TCP)

Packet: `[4B total_size][4B header_size][header (protobuf)][body (protobuf)]`
- `total_size` = 8 + `header_size` + `body_size`

## Running tests

Each test file in `test/` compiles to a standalone binary. Start in order:

```sh
# Terminal 1: start ZooKeeper
zkServer.sh start

# Terminal 2: start provider
build/test/provider

# Terminal 3: start proxy
build/test/proxy

# Terminal 4: run client
build/test/client

# Or test config loading:
build/test/func_test -i test/config
```

## Config format

- CLI: `-i <configfile>`, key=value, `#` comments, 512-char line limit
- Example config line: `proxy=127.0.0.10:8888`

## Key code conventions

- DISALLOW_COPY / DISALLOW_COPY_AND_MOVE macros from `base/Common.h`
- Logging via `LOG_INFO`, `LOG_ERROR`, `LOG_FATAL` macros
- Edge-triggered epoll (ET set on TcpConnection channels)
- `TcpConnection` has a `Role` (kClient/kProvider) and `variant<ClientRes,ProviderRes>` for per-role state
- `RpcProxy` does load balancing by response time (via `RpcLoadState`), retries on timeout (200ms), max 2 retries
