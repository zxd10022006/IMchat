# IM 即时通讯项目优化计划书

> 基于当前 IMServer（VS 工程）与 IMClient（Qt 工程）源码的现状分析，对照 15 项核心目标功能，
> 给出分阶段的详细优化实施方案，包括每一步做什么、用什么技术栈、如何验收。

---

## 一、项目现状分析

### 1.1 现有架构

**服务端 IMServer**（Windows / Visual Studio / C++ / WinSock2 / MySQL C API）

```
main.cpp
  └── Kernel（业务中枢 + 协议分发）
        ├── TcpServerMed（中介者）
        │     └── TcpServer（网络层）
        │           ├── acceptThread：accept 循环，每来一个连接 _beginthreadex 开一条 recvThread
        │           └── recvThread：阻塞 recv，4 字节长度头拆包后回调中介者
        └── CMySql（libmysql 同步查询/更新）
```

- 协议分发：`Kernel::m_funArr[PROTO_COUNT]` 函数指针表，`deal_Data` 按协议号查表分发（[Kernel.cpp](../../IMServer/IMServer/Kernel.cpp)）
- 在线用户：`std::map<int, SOCKET> m_map_id_socket`（uid → socket）
- 粘包处理：4 字节长度头 + `recvAll` 循环收满（[INet.cpp](../../IMServer/IMServer/net/INet.cpp)）—— 已解决基本粘包，但每连接一线程

**客户端 IMClient**（Windows / Qt Widgets / CMake / 原生 WinSock）

```
main.cpp → Kernel::instance()（单例中枢）
             ├── Login / mainWidgets / Chat / friendItem（UI 层）
             ├── TcpClientMed → TcpClient（_beginthreadex 阻塞收包线程）
             │     收包后 emit sighals_recieveServer → Kernel::slot_recieveServer 查表分发
             └── m_dealFuncArr[50]（协议处理函数表）
```

### 1.2 目标功能与现状对照

| # | 目标功能 | 现状 | 差距 |
|---|---------|------|------|
| 1 | 用户注册、登录（账号密码校验） | ✅ 已有 | 明文密码存库、SQL 拼接有注入风险 |
| 2 | TCP 长连接，Protobuf 编解码，解决粘包 | 🟡 部分 | 长连接+长度头已有；**协议是裸结构体，非 Protobuf** |
| 3 | 一对一私聊，在线实时转发 | ✅ 已有 | 无 ack、无持久化，离线即失败 |
| 4 | 心跳机制，超时踢下线 | ❌ 无 | 异常断连后服务端 `m_map_id_socket` 残留脏数据 |
| 5 | 离线消息：存 DB，登录自动拉取 | ❌ 无 | 现在对方离线直接回"发送失败" |
| 6 | 基础群聊：创建群，群内广播 | ❌ 无 | |
| 7 | 日志系统（连接、消息、异常） | ❌ 无 | 目前全靠 `std::cout` |
| 8 | CMake 一键编译；单元测试 Demo | 🟡 部分 | 客户端有 CMake；**服务端只有 .sln/.vcxproj**；无任何测试 |
| 9 | 多 Reactor 主从模型 | ❌ 无 | 当前是"每连接一线程"，1000 连接 = 1000 线程 |
| 10 | Redis 缓存在线用户会话 | ❌ 无 | 用 `std::map` 内存存储，无法横向扩展 |
| 11 | 消息 ack 机制，可靠投递 | ❌ 无 | 发出即忘，无重传、无去重 |
| 12 | 消息持久化分页查询 | ❌ 无 | 消息完全不落库 |
| 13 | 黑白名单 | ❌ 无 | |
| 14 | 文件传输（小文件） | ❌ 无 | |
| 15 | 压测脚本：多客户端并发 | ❌ 无 | |

### 1.3 现存关键问题清单（改造时顺带修复）

1. **每连接一线程**：`TcpServer::acceptThread` 每 accept 一个连接就 `_beginthreadex` 开线程（[TcpServer.cpp:61](../../IMServer/IMServer/net/TcpServer.cpp#L61)），并发量上不去
2. **裸结构体协议**：`char tel[15]` 等定长数组直接 `(char*)&struct` 发送，存在字节序/对齐/版本兼容问题；`PROT_CHAT_INFO_RQ` 固定 8KB，每条消息都发 8KB，浪费带宽
3. **SQL 注入**：`sprintf_s(sql, "... where tel = '%s'", rq->tel)` 直接拼接用户输入（[Kernel.cpp:148](../../IMServer/IMServer/Kernel.cpp#L148)），且密码明文存储
4. **IO 线程里同步查 MySQL**：收包线程直接 `mysql_query` 阻塞，一条慢 SQL 卡住该连接所有消息
5. **无心跳**：客户端拔网线/杀进程，服务端 `m_map_id_socket` 永久残留，好友也不会收到下线通知
6. **无消息持久化**：消息不落库，无历史记录
7. **双份 def.h**：`IMServer/IMServer/net/def.h` 与 `IMClient/def/def.h` 手动同步，改一处漏一处就会协议不兼容
8. **登录时序 hack**：服务端先发好友信息再发登录应答，客户端用 `m_pendingFriendInfo` 暂存兜底（[kernel.cpp:134](../../IMClient/kernel.cpp#L134)），协议设计应保证有序
9. **硬编码**：服务端 MySQL 账号/密码、客户端 `127.0.0.1:4321` 全部写死在代码里
10. **调试残留**：客户端每 8 秒模拟好友 7 下线的定时器（[kernel.cpp:301](../../IMClient/kernel.cpp#L301)）需删除
11. **服务端绑定 Visual Studio**：依赖硬编码的 `C:\Program Files\MySQL\MySQL Server 5.7\include` 路径，无法一键构建、无法上 Linux

---

## 二、目标架构与技术栈选型

### 2.1 技术栈总表

| 组件 | 选型 | 理由 |
|------|------|------|
| 语言 | C++17（服务端/客户端） | 现有代码基础，性能好 |
| 构建系统 | CMake ≥ 3.16（全项目统一） | 目标功能第 8 条要求一键编译；替换 .sln |
| 序列化 | Protobuf 3.x（protoc 代码生成） | 目标功能第 2 条；跨语言、可版本演进 |
| 网络模型 | 自研多 Reactor（主 Reactor accept + 子 Reactor 处理 IO） | 目标功能第 9 条；参考 muduo 设计 |
| IO 复用 | Linux：epoll；Windows：select（Poller 抽象层） | 服务端建议部署 Linux/WSL2；Windows 保留可编译可调试 |
| 数据库 | MySQL 5.7/8.0（libmysqlclient，预处理语句 + 线程级连接） | 存量兼容，修复注入问题 |
| 缓存 | Redis（hiredis C 客户端） | 目标功能第 10 条 |
| 日志 | spdlog（异步、按天滚动） | 目标功能第 7 条；头文件库接入成本低 |
| 配置 | nlohmann/json（服务端）、ini/JSON（客户端） | 去掉硬编码 |
| 密码安全 | SHA-256 + 随机盐（OpenSSL），进阶可换 bcrypt | 目标功能第 1 条的安全化改造 |
| 单元测试 | GoogleTest + CMake CTest | 目标功能第 8 条 |
| 压测 | Python asyncio 快速脚本 + C++ asio 高并发工具 | 目标功能第 15 条 |
| 依赖管理 | vcpkg（manifest 模式，vcpkg.json） | Windows 下 protobuf/spdlog/gtest/hiredis/mysql 一键装 |
| 服务端 OS | Linux（推荐）/ WSL2 开发，Windows select 兜底 | epoll 是主 Reactor 模型的标配 |

### 2.2 目标架构图

```
                          ┌─────────────────────────── IMClient (Qt, Windows) ───────────────────────────┐
                          │ Login / mainWidgets / Chat / GroupUI                                          │
                          │        │ signals/slots                                                       │
                          │      Kernel（协议分发 + 业务槽函数）                                           │
                          │        │                                                                      │
                          │  QtTcpClient（QTcpSocket 异步收发 + 长度头拆包 + Protobuf 编解码 + 心跳定时器）  │
                          └────────┴──────────────────────────────────────────────────────────────────────┘
                                   │ TCP 长连接（4 字节长度头 + Envelope{cmd,seq,body}）
                          ┌────────▼──────────────────────────────────────────────────────────┐
                          │                 IMServer（Linux / C++17 / 多 Reactor）              │
                          │                                                                    │
                          │  主 Reactor 线程            子 Reactor 线程 × N（N≈CPU 核数）         │
                          │  ┌────────────┐            ┌───────────────────────────┐           │
                          │  │ Acceptor   │──连接分发──▶│ EventLoop: epoll_wait      │           │
                          │  │ (accept)   │ round-robin│  ├─ TcpConnection(输入/输出缓冲)          │
                          │  └────────────┘            │  ├─ ProtobufCodec（拆包/解码）            │
                          │                            │  └─ TimerQueue（心跳/重传定时器）          │
                          │                            └───────────┬───────────────┘           │
                          │                                        │ 解码后的业务请求              │
                          │                            ┌───────────▼───────────────┐           │
                          │                            │ 业务线程池（8~16 线程）      │           │
                          │                            │  ├─ UserService 登录/注册   │           │
                          │                            │  ├─ MessageService 单聊/群聊/离线/ack/分页 │
                          │                            │  ├─ GroupService / BlockService / FileService │
                          │                            │  ├─ DAO 层（每线程一个 MySQL 连接，预处理语句）│
                          │                            │  └─ SessionManager（uid ↔ connectionId 双向映射）│
                          │                            └───────────┬───────────────┘           │
                          └────────────────────────────────────────┼───────────────────────────┘
                                        ┌──────────────────────────┼──────────────────────┐
                                        │                          │                      │
                                  ┌─────▼─────┐              ┌─────▼─────┐          ┌─────▼─────┐
                                  │ MySQL     │              │ Redis     │          │ 磁盘/日志  │
                                  │ t_user    │              │ im:session│          │ spdlog    │
                                  │ t_friend  │              │ :{uid}    │          │ 按天滚动   │
                                  │ t_message │              │ 在线会话   │          │           │
                                  │ t_group   │              │ TTL 心跳   │          │           │
                                  │ t_block   │              └───────────┘          └───────────┘
                                  └───────────┘
```

### 2.3 目录结构规划

```
IMchat/
├── CMakeLists.txt              # 顶层：一键构建全部目标
├── README.md/                  # 本文档所在目录（计划书）
├── proto/
│   └── im.proto                # 唯一协议源文件（两端共用）
├── common/                     # 公共静态库 im_common
│   ├── CMakeLists.txt
│   └── src/
│       ├── proto/              # protoc 生成的 im.pb.h/.cc
│       └── codec/ProtobufCodec.h/.cpp   # 长度头 + Envelope 编解码（两端共用）
├── IMServer/
│   ├── CMakeLists.txt
│   ├── config/server.json      # ip/端口/线程数/MySQL/Redis/日志级别
│   └── src/
│       ├── main.cpp
│       ├── net/                # 多 Reactor 网络库（见 Phase 2）
│       ├── codec/              # 引用 common
│       ├── service/            # 业务层 UserService/MessageService/GroupService/BlockService/FileService
│       ├── db/                 # MysqlConn（每线程连接）、UserDao/MsgDao/GroupDao/BlockDao
│       ├── cache/SessionRedis.h/.cpp
│       ├── thread/ThreadPool.h/.cpp
│       └── log/LogInit.h/.cpp
├── IMClient/
│   ├── CMakeLists.txt          # 改造：引入 proto/common，去掉原生 WinSock
│   ├── net/QtTcpClient.h/.cpp  # 替换 net/TcpClient.cpp
│   └── config/client.ini
├── tests/                      # GoogleTest 单元测试（见 Phase 11）
├── IMBench/                    # C++ asio 压测客户端（见 Phase 12）
└── scripts/
    ├── build_server.sh         # Linux 一键构建服务端
    ├── build_client.bat        # Windows 一键构建客户端
    ├── bench.py                # Python 快速压测脚本
    └── sql/schema_v2.sql       # 新表结构 + 旧表迁移脚本
```

---

## 三、分阶段实施计划

> 原则：**每个阶段结束都保持"可编译 + 可运行 + 现有功能不回退"**，采用"模块化替换"（旧代码与新代码并存 → 新代码验证通过 → 删除旧代码），避免一次性大爆炸式重写。
> 单元测试建议随每个阶段同步编写，Phase 11 做集中补全。

---

### Phase 0：工程基建 —— 统一 CMake、配置化、日志系统

**目标**：把 VS 工程迁移到 CMake 全项目统一构建，依赖外置、配置外置、日志上 spdlog。这是所有后续阶段的地基。

**要解决的问题**：目标功能 7、8；现状问题 9、11。

**具体步骤**：

1. **顶层 CMake 骨架**
   - 新建根目录 [CMakeLists.txt](../../CMakeLists.txt)：`add_subdirectory(common IMServer IMClient tests IMBench)`
   - 统一 C++17、`CMAKE_EXPORT_COMPILE_COMMANDS`、Debug/Release 区分
   - 服务端新建 [IMServer/CMakeLists.txt](../../IMServer/CMakeLists.txt)，把 .vcxproj 里的 8 个源文件迁入，同时保留 .sln 直至新构建验证通过后删除
   - 依赖查找统一走 `find_package`：`Protobuf`、`spdlog`、`MySQL`（`libmysql`）、`hiredis`、`GTest`、`nlohmann_json`
2. **vcpkg manifest 管理依赖**
   - 根目录新建 [vcpkg.json](../../vcpkg.json)，声明 `protobuf spdlog nlohmann-json gtest hiredis libmysql`
   - CMake 里 `CMAKE_TOOLCHAIN_FILE` 指向 vcpkg；Linux 用 `apt install libmysqlclient-dev libhiredis-dev protobuf-compiler libprotobuf-dev libspdlog-dev libgtest-dev`，README 写明两条安装路径
3. **配置中心**
   - 服务端：[config/server.json](../../IMServer/config/server.json)：`listen_ip/port`、`sub_reactor_num`、`business_thread_num`、`mysql{host,user,pass,db}`、`redis{host,port}`、`heartbeat_timeout`、`msg_retry_times`、`log_level`
   - 用 nlohmann/json 实现 `ServerConfig::load(path)`，启动时打印生效配置
   - 客户端：`config/client.ini` 存服务器 ip/端口，登录页增加"设置服务器地址"入口
4. **日志系统**
   - 服务端集成 spdlog：`initLogger(cfg)` 创建三个异步 logger——`server_log`（info 级，记录连接建立/断开、登录/登出、消息转发摘要 `msg_id/from/to/长度`，**不记消息内容**）、`error_log`（error 级，异常/SQL 错误）、控制台输出
   - 按天滚动 + 单文件 50MB 上限 + 保留 7 天（`spdlog::sinks::daily_file_sink`）
   - 替换所有 `std::cout`（`TcpServer.cpp`、`Kernel.cpp`、`CMySql.cpp`）
   - 客户端：`qInstallMessageHandler` 把 qDebug/qWarning 重定向到本地日志文件
5. **一键构建脚本**
   - [scripts/build_server.sh](../../scripts/build_server.sh)：`cmake -S . -B build -DBUILD_SERVER=ON ... && cmake --build build -j`
   - [scripts/build_client.bat](../../scripts/build_client.bat)：同上（Windows + Qt）
   - README 补充"两条命令跑起来"说明

**技术栈**：CMake 3.16+、vcpkg（或 Linux apt）、nlohmann/json、spdlog 1.x

#### Cmake学习内容：

基础概念：project、set、add_subdirectory、add_executable、target_link_libraries、target_include_directories

C++ 标准设置，`CMAKE_EXPORT_COMPILE_COMMANDS`作用（给 clangd/IDE 代码补全）

find_package 查找第三方库，区分 PRIVATE/PUBLIC/INTERFACE

Debug/Release 编译配置、MSVC 编译选项

看懂构建报错：像你之前`add_subdirectory`语法错误这种基础问题，自己能定位，不用每次等 AI

区分：顶层 CMake 和子目录 CMake 的职责（你的 IM 项目就是典型多子目录工程）

#### vcpkg 学习内容

vcpkg 是什么：C++ 包管理器，解决 C++ 没有自带包管理的痛点

manifest 模式（vcpkg.json）和全局安装的区别（你现在正在用的）

toolchain 文件作用（`vcpkg.cmake`给 CMake 传递库路径）

看懂报错：比如你这次的 cmake 版本依赖问题、网络下载失败怎么处理

知道：Linux 下不用 vcpkg，可以用系统包管理器 apt 安装库，两种方案取舍

#### nlohmann_json学习内容

读取 json 文件，解析 json 对象，读取字段（你的 server.json 配置中心）

序列化 / 反序列化，简单的类型转换

理解：配置文件外置，硬编码写死 ip 端口是坏习惯，工程化项目用 json 配置

#### spdlog学习内容

基础 API：创建 logger、sink 概念（控制台 sink、文件 sink、daily 按天滚动文件）

同步 logger vs 异步 logger（**面试高频！**）

日志级别、日志文件滚动策略（大小切割、保留天数，就是你需求里的 50MB、保留 7 天）

项目实战：替换 cout，全局初始化日志，不同模块用不同 logger（你的 server_log /error_log）

**验收标准**：

- 删除 `.sln/.vcxproj` 后，一条命令可编译出 `IMServer` 可执行文件；客户端 CMake 构建流程不受影响
- 修改 `server.json` 中端口重启生效；日志按天生成文件，包含连接/断开/转发摘要记录
- 现有注册/登录/单聊/加好友全功能在 Windows（select 兜底）下回归通过

---

### Phase 1：协议层 Protobuf 重构

**目标**：用 Protobuf 替换双份裸结构体协议，消除字节序/对齐/版本问题，缩小消息体积。这是所有业务功能扩展的前提（后续所有新功能都直接定义在 proto 里）。

**要解决的问题**：目标功能 2；现状问题 2、7、8。

**具体步骤**：

1. **设计并编写 [proto/im.proto](../../proto/im.proto)**
   - 统一信封 `Envelope { uint32 cmd; uint64 seq; bytes body; }`（body 里再放具体消息的序列化字节），或直接用 `oneof` 内嵌所有消息。**建议 oneof 方案**：类型安全、免二次反序列化，示例见 [五、协议设计草案](#五协议设计草案)
   - cmd 枚举覆盖现有 9 个协议 + 新增：心跳 `PING/PONG`、离线消息拉取、消息 ACK、群聊、黑名单、文件传输、历史分页（新协议在对应 Phase 定义）
2. **protoc 集成到 CMake**
   - `find_package(Protobuf REQUIRED)` + `protobuf_generate_cpp`，生成文件放 `common/src/proto/`
   - 删除 [IMServer/IMServer/net/def.h](../../IMServer/IMServer/net/def.h) 和 [IMClient/def/def.h](../../IMClient/def/def.h)，两端统一 `#include "im.pb.h"`
3. **公共编解码库 im_common**
   - `ProtobufCodec`：`encode(msg) -> [4字节长度][protobuf字节流]`、`decode(输入缓冲) -> 完整包`（处理半包：缓冲数据不足一个完整包时等待下次数据到达，这就是粘包/半包的完整解法）
   - 帧格式保持"4 字节长度头"以兼容现有网络层，Phase 2 再做网络层替换
4. **服务端改造 Kernel 分发**
   - `deal_Data` 改为：解析 `Envelope.cmd` → `switch/查表` → 把 `body` 反序列化为具体消息 → 传给处理函数（处理函数签名改为接收 protobuf 消息对象）
   - 现有 6 个 `deal_*` 函数逐个迁移：`PROT_REGISTER_RQ → RegisterReq` 等；`PROT_CHAT_INFO_RQ` 的 8KB 定长 `msg[8192]` 改为 `string content`，只传实际长度
5. **客户端同步改造**
   - [IMClient/kernel.cpp](../../IMClient/kernel.cpp) 各 `slots_*` 改为填充 protobuf 消息 → `Envelope` → `ProtobufCodec::encode` → 发送
   - `slot_recieveServer` 改为按 cmd 分发
6. **协议版本兼容**：`Envelope` 中加 `uint32 version = 1;`，服务端校验客户端版本，不匹配返回错误码（为未来协议升级留口子）

**技术栈**：Protobuf 3.x、protoc

**验收标准**：
- 两端 def.h 删除，全项目只有一个 im.proto
- 现有功能（注册/登录/好友列表/单聊/加好友/离线通知）全部回归通过
- 一条 20 字节消息实际传输 ≤ 100 字节（对比现在固定 8KB+）
- 客户端与服务端协议不一致时（改 proto 只编译一端）能报出明确错误而非崩溃

---

### Phase 2：网络层多 Reactor 主从重构

**目标**：把"每连接一线程"替换为"1 个主 Reactor accept + N 个子 Reactor 处理 IO"（N = CPU 核数），每连接一个 `TcpConnection` 对象 + 输入/输出缓冲区。这是本项目**最核心、工作量最大**的阶段。

**要解决的问题**：目标功能 9；现状问题 1。

**具体步骤**：

1. **实现 Reactor 核心组件**（新建 [IMServer/src/net/](../../IMServer/src/net/)）
   - `EventLoop`：每个子 Reactor 线程一个；核心循环 `epoll_wait → 分发就绪事件 → 执行 pending 任务队列`；提供 `runInLoop()/queueInLoop()` 跨线程投递任务（保证连接对象只在所属 loop 线程被访问，**无锁设计**）
   - `Poller` 抽象基类 + `EpollPoller`（Linux）+ `SelectPoller`（Windows 调试用）
   - `Channel`：封装 fd + 关心的事件 + 读写回调
   - `TcpConnection`：持有 socket、`inputBuffer/outputBuffer`、所属 loop；`onMessage` 回调；发送数据先写缓冲，`EAGAIN` 时注册可写事件；连接断开回调
   - `Acceptor`：在主 Reactor 中 accept，`SO_REUSEADDR` + 非阻塞 socket
   - `TcpServer`：管理主 loop + 子 loop 列表，新连接 round-robin 分发给子 loop
   - `TimerQueue`（最小堆实现）：`runAt/runAfter/runEvery`，为 Phase 4 心跳、Phase 6 重传提供定时器
2. **接入拆包与分发**
   - 子 loop 收到数据 → `ProtobufCodec::decode`（Phase 1 的缓冲逻辑挪进 `inputBuffer`）→ 完整包投递业务线程池（**禁止在 IO 线程做 MySQL/Redis 阻塞调用**）
   - 业务处理完成后通过 `conn->send(encoded)` 回包（线程安全由 `queueInLoop` 保证）
3. **业务线程池**
   - `ThreadPool`：固定 8~16 线程 + 有界任务队列；`Kernel` 的 `deal_*` 逻辑整体迁移为线程池任务
4. **连接生命周期管理**
   - 双向映射 `SessionManager`：`uid ↔ connId`（替代现有 `m_map_id_socket`），登录成功登记、下线/断开注销
   - 对端断开（read 返回 0/错误）时：清输入缓冲、注销会话、通知好友（复用离线通知协议）、回收连接对象——**修复现状问题 5 的脏数据**
5. **客户端网络层改造**
   - 用 `QTcpSocket` 重写 [IMClient/net/TcpClient.cpp](../../IMClient/net/TcpClient.cpp)：异步 `readyRead` 信号驱动，`QByteArray` 输入缓冲 + 同一套 `ProtobufCodec` 拆包，删除 `_beginthreadex` 收包线程与跨线程信号
   - 好处：天然融入 Qt 事件循环，无线程同步问题，自动处理 `EAGAIN`
6. **压力自测**：先用 Phase 12 的雏形脚本跑 500~1000 连接冒烟，确认无崩溃、无泄漏（对比改造前线程数：1000 连接从 1000 线程降到 ~8 线程）

**技术栈**：epoll/select、POSIX thread、eventfd（Linux）/ socketpair（Windows 唤醒）、std::unique_ptr 管理连接生命周期；设计参考 muduo

**验收标准**：
- 500 连接下：注册/登录/单聊/离线通知全功能正常；服务端线程数 ≈ 1 + N 子 Reactor + 业务线程数（不随连接数增长）
- 客户端拔网线 → 服务端 5 秒内感知断连、清理会话、通知其好友（临时逻辑，Phase 4 由心跳正式接管）
- 连接/断开日志完整记录

---

### Phase 3：账号安全与数据库层加固

**目标**：密码加盐哈希、SQL 预处理语句、数据库连接线程化，堵住注入与明文存储漏洞。

**要解决的问题**：目标功能 1 的安全化；现状问题 3、4。

**具体步骤**：

1. **密码加盐哈希**
   - 注册：服务端生成 16 字节随机盐（`RAND_bytes`，OpenSSL），存 `pass_hash = SHA256(salt + passwd)` 十六进制 + `pass_salt`
   - 登录：按 tel 取盐 → 计算哈希 → 与库中比对（恒定时间比较）
   - 表结构迁移（[scripts/sql/schema_v2.sql](../../scripts/sql/schema_v2.sql)）：`ALTER TABLE t_user ADD COLUMN pass_hash CHAR(64), ADD COLUMN pass_salt CHAR(16);` 用脚本把旧明文批量算哈希回填 → 验证 → `DROP COLUMN pass`
2. **SQL 预处理语句**
   - `CMySql` 增加 `StmtSelect/StmtUpdate`：`mysql_stmt_init → mysql_stmt_prepare → mysql_stmt_bind_param`，所有含用户输入的 SQL 改为 `?` 占位符
   - 涉及点：`Kernel::deal_register_RQ`、`deal_login_RQ`、`fieldExists`、`forEachFriend`、`deal_AddFriRq`、`deal_AddFriRs`（全部迁移到 DAO 层，见下）
3. **DAO 层 + 每线程连接**
   - 新建 [IMServer/src/db/](../../IMServer/src/db/)：`UserDao`（注册/登录/查用户/查好友）、`MsgDao`（Phase 5 用）、`FriendDao`
   - libmysql 单连接非线程安全 → 业务线程池中**每个线程一个 `thread_local` MySQL 连接**（简单可靠），连接失败自动重连
   - 删除 Kernel 中散落的 `sprintf_s(sql, ...)` 拼接
4. **输入长度校验**：协议层拒绝超长字段（tel ≤ 15、nick ≤ 30、passwd ≤ 64、content ≤ 8192），非法即断连并记 error 日志（防恶意包）

**技术栈**：OpenSSL（SHA-256 + RAND_bytes）、libmysqlclient 预处理语句、thread_local

**验收标准**：
- 数据库中不再存在明文密码；老账号迁移后仍能正常登录
- 用"昵称输入 `' OR '1'='1"之类 payload 注册/加好友，无注入效果（有单测用例固化）
- 登录/注册在 100 并发下平均响应 < 50ms（预处理语句 + 每线程连接，无排队）

---

### Phase 4：心跳机制与会话管理

**目标**：应用层心跳保活 + 超时踢下线 + 异常断连清理 + 客户端自动重连。

**要解决的问题**：目标功能 4；现状问题 5。

**具体步骤**：

1. **协议**：`Ping { uint64 client_time; }` / `Pong { uint64 server_time; }`（Pong 带服务器时间，客户端可计算 RTT）
2. **服务端**（利用 Phase 2 的 TimerQueue）
   - 每连接维护 `last_recv_time`，任何包到达即刷新
   - 子 loop 定时器每 1s 扫描：`now - last_recv_time > 3 × 心跳间隔(10s)` → 判定死亡 → 关连接 → `SessionManager` 注销 → 通知在线好友下线（复用 `FRIEND_OFFLINE`）
   - 连接建立后 15s 内未登录 → 主动断开（防空连接占资源）
3. **客户端**
   - `QTimer` 每 10s 发 `Ping`；连续 3 次（30s）未收到任何包 → 判定断线 → 自动重连（指数退避 1s/2s/4s…上限 30s）+ 重新登录 + 重新拉取会话状态
   - UI 顶部显示连接状态（"连接中/已连接/重连中"）
4. **登录互踢（可选增强）**：同账号第二次登录时踢掉旧连接（SessionManager 查重，给旧连接发 `KickOut` 通知后断开）
5. **清理调试残留**：删除客户端 [kernel.cpp:301](../../IMClient/kernel.cpp#L301) 模拟好友 7 下线的定时器

**技术栈**：TimerQueue 定时器、QTimer、指数退避重连

**验收标准**：
- 客户端强杀进程（不发送下线包）→ 30s 内服务端移除会话、好友列表变灰
- 断网 30s → 客户端进入重连态；恢复网络 → 自动重连成功且好友列表状态正确
- 同账号双开互踢正常；心跳包流量占比 < 1%（10s 一次，包体 < 50B）

---

### Phase 5：离线消息 + 消息持久化分页查询

**目标**：消息全部落库；对方离线时消息入库、上线自动拉取；支持历史消息分页查询。之后所有消息功能（ack/群聊/黑名单）都建立在持久化之上。

**要解决的问题**：目标功能 5、12；现状问题 6。

**具体步骤**：

1. **建表**（[scripts/sql/schema_v2.sql](../../scripts/sql/schema_v2.sql)）：`t_message` 见 [四、数据库设计](#四数据库设计)，建 `(to_uid, to_type, status, create_time)` 组合索引
2. **发消息流程改造**（`MessageService::sendSingle`）：
   - 收到 `ChatReq` → **先落库**（生成自增 `msg_id`，status=0 未读）→ 查 SessionManager：
     - 在线：转发给对方，对方 `ChatAck`（Phase 6）后把该条 status 置 1
     - 离线：消息留在库中，不回复"发送失败"
   - 发送方始终收到 `SendResult { msg_id, client_seq, 成功 }`（"已送达服务器"）
3. **离线拉取协议**：
   - `PullOfflineReq { last_msg_id, limit }` / `PullOfflineRsp { repeated MessageItem items; bool has_more; }`
   - 登录成功后客户端自动请求：服务端按 `to_uid=我 AND status=0` 分批返回（每批 ≤ 50 条，按 msg_id 升序），客户端逐批拉取直到 `has_more=false`，全部收到后发 `PullOfflineDone` 标记已读
4. **历史分页协议**：
   - `HistoryReq { peer_id, peer_type, before_msg_id, limit }` / `HistoryRsp { items, has_more }`（向前翻页：取 msg_id < before 的最近 N 条）
5. **客户端**
   - [IMClient/chat.cpp](../../IMClient/chat.cpp) 聊天窗口：消息区顶部"加载更多历史"按钮、未读消息置顶显示（昵称+条数）、离线期间消息登录后自动出现在对应会话
   - 消息展示统一走 `msg_id` 去重
6. **会话列表（可选增强）**：主界面增加"最近会话"页签（t_message 按 `to_uid` 聚合最后一条）

**技术栈**：MySQL InnoDB、组合索引、LIMIT 分页

**验收标准**：
- A 给离线 B 发消息 → B 登录后自动收到，且消息按时间序完整
- 500 条历史消息分页拉取，每页响应 < 100ms
- 断网重连后拉取不重复、不丢失（按 msg_id 断点续拉）

---

### Phase 6：消息 ACK 机制，可靠投递

**目标**：端到端 ack + 超时重传 + 去重，保证"发出去的消息一定能到、只到一次"。

**要解决的问题**：目标功能 11。

**具体步骤**：

1. **seq 与消息身份**
   - 客户端每个连接维护自增 `client_seq`；服务端落库后生成全局唯一 `msg_id`
   - 单聊链路：`A → ChatReq(client_seq=1) → 服务端落库(msg_id=100) → 转发 B → B 回 ChatAck(msg_id=100, client_seq=1) → 服务端标记已读 → 回 A SendResult(msg_id=100, status=已送达)`——客户端 A 显示"已送达"，B 收到且只处理一次
2. **客户端重传队列**
   - 发送后消息进入 `pending` 队列，启动定时器（3s）：未收到 `SendResult` → 原包重发（同一 `client_seq`），最多 3 次 → 提示"发送失败"并标记红色感叹号（点击可重发）
   - 收到 `ChatAck`/`SendResult` 后按 `client_seq` 移除
3. **服务端去重**
   - 用 `(from_uid, client_seq)` 查重：已存在 → 不重复落库，仅重发 `SendResult`（幂等）
4. **群聊 ack 简化策略**（Phase 7 落地）：服务端对每个在线成员独立投递并跟踪 ack，离线成员走离线消息通道；`SendResult` 只在"服务端确认入库"后返回，不等待所有成员 ack
5. **Redis 辅助（可选）**：`im:dedup:{from_uid}:{client_seq}` 短 TTL（1 分钟）做快速去重，减少 DB 压力

**技术栈**：定时器重传队列、幂等去重

**验收标准**：
- 压测中随机丢弃 1% 数据包：最终消息不丢不重，接收端全部按序
- 断网 10s 内恢复：pending 消息自动重传成功，无重复
- 单测覆盖：重传 3 次放弃、乱序 ack、重复 ack（见 Phase 11）

---

### Phase 7：基础群聊

**目标**：创建群、加群/退群、群内广播消息、群离线消息。复用 Phase 5/6 的持久化与 ack 基础设施。

**要解决的问题**：目标功能 6。

**具体步骤**：

1. **建表**：`t_group`、`t_group_member`（见 [四、数据库设计](#四数据库设计)）
2. **协议**：`CreateGroupReq/Rsp`、`JoinGroupReq/Rsp`、`QuitGroup`、`GroupChatReq`（复用 `ChatReq` 加 `peer_type=GROUP` + `group_id`）、`GroupInfoRsp { group_id, name, members[] }`
3. **服务端 GroupService**
   - 建群：群主默认入群，群名唯一校验
   - 入群：群主/管理员审批或直接入群（基础版：直接入群，向全体成员广播"XXX 加入群聊"系统消息）
   - 广播：`GroupChatReq` → 落库（`to_type=1`）→ 查成员列表 → 逐成员投递（在线转发 / 离线走离线消息通道）
   - 退群：广播系统消息；群内无成员时解散
4. **客户端**
   - 主界面增加"群聊"页签：我的群列表 + "创建群/搜索加群"入口（新建 [groupchat.ui](../../IMClient/groupchat.ui) 或复用 [chat.ui](../../IMClient/chat.ui) 改造）
   - 群消息展示"昵称: 内容"；成员列表展示在线状态
5. **扩展点**：`t_group_member.role` 预留管理员体系，本期只实现 owner/member 两级

**技术栈**：MySQL 多表事务（入群 = 插入成员 + 发系统消息，用事务保证一致）

**验收标准**：
- 3 人建群 → 群广播消息 3 人实时收到；1 人离线时其余人正常收发，离线者上线后补收
- 100 人群广播延迟 P99 < 200ms
- 退群后不再收到该群消息

---

### Phase 8：黑白名单

**目标**：黑名单屏蔽（拉黑后拒收对方消息、对方看到"被拒收"）；白名单模式（仅好友/名单内可发消息）。基础版做黑名单，白名单做成可配置开关。

**要解决的问题**：目标功能 13。

**具体步骤**：

1. **建表**：`t_block`（uid 拉黑 block_uid）
2. **协议**：`BlockReq { uid, block_uid, action }`、`BlockRsp { result }`、`BlockListRsp { blocked_uid[] }`；`SendResult` 增加错误码 `ERR_BLOCKED`
3. **服务端 BlockService**
   - 加/删黑名单：落库 + 内存缓存（登录时加载到本地 `unordered_set`，变更时增量更新）
   - 消息发送前校验：from 在黑名单 of to → 拒收并回 `ERR_BLOCKED`；白名单模式开启时：from 不在 to 的好友列表 → 拒收
4. **客户端**
   - 好友右键菜单"加入/移出黑名单"
   - 被拒收提示、被拉黑方无感知（不回包即可，可选回 `ERR_BLOCKED` 提示）

**技术栈**：MySQL + 内存缓存一致性（写库成功后更新缓存）

**验收标准**：
- A 拉黑 B 后：B 发消息收到"被拒收"，A 无任何提示；解除后恢复
- 白名单模式下陌生人消息被拦截；单测覆盖过滤逻辑

---

### Phase 9：Redis 在线会话缓存

**目标**：在线用户会话（uid → 连接信息）从本地 `std::map` 迁到 Redis，为多节点/服务重启无感做准备。

**要解决的问题**：目标功能 10。

**具体步骤**：

1. **hiredis 接入**：`RedisClient` 封装（连接、ping 检测、断线重连）
   - key 设计：
     - `im:session:{uid}` → hash：`{ server_id, conn_id, login_time, last_hb }`，登录写入、下线删除
     - `im:server:{server_id}:conns` → set：该节点全部在线 uid（节点启动清空、优雅关闭删除）
   - 心跳每 10s 刷新 `last_hb`；`EXPIRE 60s` 做兜底（服务崩溃后 60s 自动过期）
2. **SessionManager 改造**
   - 写路径：登录/登出同步写 Redis；本地仍保留 `uid→TcpConnection*` 内存表（转发路径零延迟）
   - 读路径：转发优先查本地表；本地未命中查 Redis（未来多节点时有用）
3. **降级策略**：Redis 不可用时回退纯本地表（记 error 日志），保证单机功能不挂
4. **多节点准备（说明性设计，不实现）**：文档说明如何按 uid 哈希路由到不同服务节点（本期单节点，架构预留）

**技术栈**：Redis 5+、hiredis、TTL 过期兜底

**验收标准**：
- 在线会话实时同步到 Redis：登录后 `im:session:{uid}` 存在，下线 1s 内删除
- kill -9 服务端 → 60s 后 Redis 会话自动过期；重启服务端后用户可重新登录无残留
- 停掉 Redis → 单机收发消息不受影响（降级生效）

---

### Phase 10：文件传输（小文件）

**目标**：支持 ≤ 10MB 小文件点对点传输（服务端中转模式），带进度显示与完整性校验。

**要解决的问题**：目标功能 14。

**具体步骤**：

1. **协议**：
   - `FileSendReq { file_name, file_size, md5 }` → 对方 `FileSendResp { accept }`
   - `FileChunk { transfer_id, seq, offset, data }`（分片 64KB）
   - `FileDone { transfer_id, ok, md5 }`
2. **服务端中转**（基础版）：发送方分片 → 服务端按 `transfer_id` 缓存（内存）或直接流式转发给接收方 → 接收方每片回 `FileChunkAck`（复用 Phase 6 思路）→ 完成后校验 md5
   - 限制：单文件 ≤ 10MB、并发传输数每用户 ≤ 3、速率不限（小文件场景够用）
3. **客户端**
   - 聊天窗口"发送文件"按钮 → 文件选择对话框 → 对方弹窗"接受/拒绝" → 进度条（已收/总大小）→ 完成保存到指定目录
   - 接收方离线：文件请求暂存（存库文件元数据 + 服务端临时文件，上线后拉取），本期可选实现
4. **扩展说明**：大文件/多线程断点续传、点对点打洞（NAT）列为后续方向，不在本期范围

**技术栈**：分片传输 + 序号重传（复用 Phase 6）、MD5（OpenSSL）

**验收标准**：
- 1MB 文件端到端传输成功，md5 一致；传输中拔网线 → 恢复后重传未确认分片
- 10MB 文件传输期间双方消息交互不受影响（限速/分片不阻塞 IO 线程）
- 100 并发文件传输（各 1MB）无内存暴涨（服务端流式中转，不整体缓存）

---

### Phase 11：单元测试补全

**目标**：为关键模块建立 GoogleTest 测试体系，纳入 CMake CTest 一键运行。

**要解决的问题**：目标功能 8 的测试部分。

**具体步骤**：

1. **测试工程**：[tests/CMakeLists.txt](../../tests/CMakeLists.txt)，`enable_testing()` + `add_test`，`ctest` 一键跑全部
2. **用例清单**（每个模块一个 test 文件）：
   - `test_codec`：Protobuf 编解码往返；**粘包**（两个包一次到达拆成两个）、**半包**（一个包分 3 次到达拼回完整包）、超大长度头拒绝（防恶意包）
   - `test_heartbeat`：虚拟时钟（TimerQueue 注入 fake clock）验证 3 次超时踢下线、心跳刷新
   - `test_ack`：丢包重传、重传 3 次放弃、重复 seq 去重、乱序 ack
   - `test_message`：离线消息拉取分批、断点续拉不重不漏、分页查询
   - `test_group`：广播成员集合正确性、退群后不再投递
   - `test_block`：黑/白名单过滤矩阵
   - `test_dao`：UserDao/MsgDao 增删查改（连本地测试库，用例自建自清）
   - `test_session`：登录/登出/互踢后 SessionManager 状态机
3. **可测性改造**：DAO/Redis 接口抽象出虚基类，业务层依赖接口，测试用内存实现（如 `FakeSessionStore`）替代
4. **回归基线**：每次 Phase 完成后全量 `ctest` 通过才算完成

**技术栈**：GoogleTest、CMake CTest、fake clock / mock 接口

**验收标准**：
- `ctest` 一条命令跑通全部用例（目标 ≥ 40 个用例，覆盖上述 8 个模块）
- 每个 Phase 的"验收标准"均有对应测试用例固化

---

### Phase 12：压测与调优

**目标**：自研压测工具模拟多客户端并发，量化并发能力，定位瓶颈并调优。

**要解决的问题**：目标功能 15。

**具体步骤**：

1. **两级压测工具**
   - [scripts/bench.py](../../scripts/bench.py)（Python asyncio，快速版）：参数 `--clients 200 --msg-per-conn 100 --msg-size 100 --rate 0`；支持注册/登录/心跳/单聊随机互发；输出 TPS、P50/P99 延迟、错误率。适合日常回归（千级连接）
   - [IMBench/](../../IMBench/)（C++17 + asio 独立库，高并发版）：`-c 10000` 万级长连接、多线程发压（每线程一个 asio io_context）、支持消息 ack 等待与超时统计。适合极限压测
2. **压测维度**
   - 连接数：5k / 10k 长连接建立与保持（内存/句柄观察）
   - 吞吐：单聊转发 TPS（如 1k 连接 × 每秒各 10 条 = 1 万 msg/s）
   - 延迟：P50/P99（统计 SendResult 往返）
   - 混合：30% 登录登出 + 70% 消息 + 心跳，模拟真实流量
3. **调优清单（按观察结果逐项实施）**
   - 子 Reactor 数量与业务线程数扫描（2/4/8 对比）
   - `t_message` 落库批量化（高吞吐下合并写入）、索引命中检查（EXPLAIN）
   - 发送侧合并写（`writev`/攒批）、`TCP_NODELAY`
   - TIME_WAIT 优化（服务端 `SO_REUSEADDR`，压测客户端端口复用）
   - 内存分配优化（protobuf 消息对象池，可选）
4. **输出压测报告**：[docs/bench_report.md](../../docs/bench_report.md) 记录环境、参数、优化前后对比图表

**技术栈**：Python asyncio、C++ asio（standalone）、性能指标统计

**验收标准（参考值，随机器调整）**：
- 单机（4C8G）≥ 5000 长连接稳定在线，消息转发 ≥ 2 万 msg/s，P99 < 200ms
- 压测过程中服务端内存稳定无泄漏（RSS 波动 < 10%）
- 压测报告归档，可复现（脚本 + 参数 + 结果）

---

## 四、数据库设计

```sql
-- ===== 用户表（Phase 3 改造）=====
CREATE TABLE t_user (
  id          INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  tel         VARCHAR(15)  NOT NULL UNIQUE,
  nick        VARCHAR(30)  NOT NULL UNIQUE,
  pass_hash   CHAR(64)     NOT NULL,            -- SHA256(salt+password) 十六进制
  pass_salt   CHAR(16)     NOT NULL,            -- 随机盐
  feeling     VARCHAR(100) NOT NULL DEFAULT '',
  iconid      INT          NOT NULL DEFAULT 35,
  create_time DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ===== 好友表（沿用现有 t_friend 双向存储）=====
CREATE TABLE t_friend (
  idA INT UNSIGNED NOT NULL,
  idB INT UNSIGNED NOT NULL,
  PRIMARY KEY (idA, idB)
) ENGINE=InnoDB;

-- ===== 消息表（Phase 5 新增，核心表）=====
CREATE TABLE t_message (
  msg_id      BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  from_uid    INT UNSIGNED NOT NULL,
  to_uid      INT UNSIGNED NOT NULL,   -- 单聊：对方 uid；群聊：group_id
  to_type     TINYINT      NOT NULL DEFAULT 0,  -- 0 单聊 1 群聊
  msg_type    TINYINT      NOT NULL DEFAULT 0,  -- 0 文本 1 系统消息 2 文件
  content     VARCHAR(8192) NOT NULL,
  status      TINYINT      NOT NULL DEFAULT 0,  -- 0 未读 1 已读
  create_time DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  KEY idx_offline (to_uid, to_type, status, create_time),   -- 离线拉取
  KEY idx_history (to_uid, to_type, msg_id),                -- 分页翻历史
  KEY idx_dedup  (from_uid, create_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ===== 群组（Phase 7 新增）=====
CREATE TABLE t_group (
  group_id    INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  name        VARCHAR(50) NOT NULL UNIQUE,
  owner_uid   INT UNSIGNED NOT NULL,
  create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

CREATE TABLE t_group_member (
  group_id INT UNSIGNED NOT NULL,
  uid      INT UNSIGNED NOT NULL,
  role     TINYINT NOT NULL DEFAULT 2,  -- 0 群主 1 管理员 2 成员
  join_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (group_id, uid)
) ENGINE=InnoDB;

-- ===== 黑名单（Phase 8 新增）=====
CREATE TABLE t_block (
  uid        INT UNSIGNED NOT NULL,   -- 拉黑人
  block_uid  INT UNSIGNED NOT NULL,   -- 被拉黑人
  create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (uid, block_uid)
) ENGINE=InnoDB;
```

**Redis key 设计**（Phase 9）：

| Key | 类型 | 内容 | 生命周期 |
|-----|------|------|---------|
| `im:session:{uid}` | hash | `server_id / conn_id / login_time / last_hb` | 登录写入，登出删除，`EXPIRE 60s` 心跳续期 |
| `im:server:{server_id}:conns` | set | 该节点在线 uid 列表 | 节点优雅关闭时删除 |

---

## 五、协议设计草案

```proto
syntax = "proto3";
package im;

// ===== 信封：所有包统一由 Envelope 包裹，cmd 区分类型 =====
message Envelope {
  uint32 version = 1;   // 协议版本，当前 = 1
  Cmd cmd = 2;          // 命令类型
  uint64 seq = 3;       // 客户端请求序号（ack/重传/去重用）
  oneof payload {
    RegisterReq   register_req   = 10;
    RegisterRsp   register_rsp   = 11;
    LoginReq      login_req      = 12;
    LoginRsp      login_rsp      = 13;
    FriendInfo    friend_info    = 14;
    ChatReq       chat_req       = 15;
    ChatAck       chat_ack       = 16;
    SendResult    send_result    = 17;
    FriendOffline friend_offline = 18;
    Ping          ping           = 19;
    Pong          pong           = 20;
    PullOfflineReq  pull_offline_req  = 21;
    PullOfflineRsp  pull_offline_rsp  = 22;
    HistoryReq    history_req    = 23;
    HistoryRsp    history_rsp    = 24;
    // ... Phase 7~10 的群聊/黑名单/文件协议按编号继续追加
  }
}

enum Cmd {
  CMD_UNKNOWN = 0;
  REGISTER_REQ = 1;  REGISTER_RSP = 2;
  LOGIN_REQ = 3;     LOGIN_RSP = 4;
  FRIEND_INFO = 5;
  CHAT_REQ = 6;      CHAT_ACK = 7;   SEND_RESULT = 8;
  FRIEND_OFFLINE = 9;
  PING = 10;         PONG = 11;
  PULL_OFFLINE_REQ = 12; PULL_OFFLINE_RSP = 13;
  HISTORY_REQ = 14;  HISTORY_RSP = 15;
  // ...
}

// ===== 核心消息示例 =====
message LoginReq { string tel = 1; string passwd = 2; }

message ChatReq {
  uint32 from_uid = 1;
  uint32 to_id = 2;      // 单聊：对方 uid；群聊：group_id
  uint32 peer_type = 3;  // 0 单聊 1 群聊
  uint32 msg_type = 4;   // 0 文本 1 系统 2 文件
  string content = 5;
}

message ChatAck { uint64 msg_id = 1; uint64 client_seq = 2; }  // 接收方回执

message SendResult {
  uint64 msg_id = 1;
  uint64 client_seq = 2;
  int32 code = 3;        // 0 成功 1 被拉黑 2 超时失败 ...
}

message MessageItem {           // 离线拉取/历史分页共用
  uint64 msg_id = 1;
  uint32 from_uid = 2;
  uint32 to_id = 3;
  uint32 peer_type = 4;
  uint32 msg_type = 5;
  string content = 6;
  uint64 create_time = 7;
}

message HistoryReq  { uint32 peer_id = 1; uint32 peer_type = 2; uint64 before_msg_id = 3; uint32 limit = 4; }
message HistoryRsp  { repeated MessageItem items = 1; bool has_more = 2; }
```

**帧格式**（沿用现有长度头方案，粘包/半包由 ProtobufCodec 完整处理）：

```
+----------------+----------------------+
| 4 字节 长度(LE) | Envelope 序列化字节流  |
+----------------+----------------------+
```

---

## 六、里程碑与迭代节奏

> 工作量按单人全职估算，可按实际情况伸缩；每个里程碑结束都有**可演示的成果**。

| 里程碑 | 内容 | 预计周期 | 里程碑验收 |
|--------|------|---------|-----------|
| M1 | Phase 0 工程基建 + Phase 1 Protobuf 改造 | 第 1~2 周 | 一键构建；新旧协议全功能等价 |
| M2 | Phase 2 多 Reactor + Phase 3 安全加固 | 第 3~5 周 | 500 连接稳定；密码哈希+预处理语句上线 |
| M3 | Phase 4 心跳 + Phase 5 离线消息/持久化分页 | 第 6~7 周 | 杀进程 30s 内好友感知下线；离线消息不丢 |
| M4 | Phase 6 ACK + Phase 7 群聊 + Phase 8 黑白名单 | 第 8~9 周 | 可靠投递（丢包 1% 不重不漏）；3 人群聊演示 |
| M5 | Phase 9 Redis + Phase 10 文件传输 | 第 10~11 周 | 会话入 Redis；10MB 文件断点续传 |
| M6 | Phase 11 测试补全 + Phase 12 压测调优 | 第 12~13 周 | ctest 全绿；压测报告达标 |

**依赖关系**（决定先后顺序）：
- Phase 0 → 所有阶段（构建/日志/配置地基）
- Phase 1 → Phase 2（网络层拆出的是 Protobuf 包）
- Phase 3 → Phase 5（落库要 DAO/预处理语句）
- Phase 4/5 → Phase 6（ack 需要心跳会话 + msg_id）
- Phase 5 → Phase 7/8/10（群聊/黑名单/文件都建立在消息持久化上）
- Phase 2/4 → Phase 9（SessionManager 与连接生命周期就绪后才能迁 Redis）
- Phase 1~10 → Phase 11（测试覆盖已完成的模块），Phase 2~10 → Phase 12

---

## 七、风险与注意事项

1. **大重构的兼容风险**：协议（Phase 1）与网络层（Phase 2）是两座大山，务必分阶段替换、每阶段保持可运行；旧 .sln、旧 def.h、旧 TcpServer 在对应阶段验收通过前不要删除
2. **跨平台**：服务端主战场建议 Linux（epoll 是主从 Reactor 的标准组合），Windows 下用 SelectPoller 兜底保证日常调试；客户端保持 Windows/Qt。开发环境可用 WSL2 + CLion/VS Code
3. **数据迁移**：`t_user` 加盐改造必须写迁移脚本 + 回滚方案，先备份再执行；`t_friend` 沿用不动
4. **压测注意**：压测客户端会打爆本机端口（TIME_WAIT），压测脚本需设置 `SO_REUSEADDR` 与端口范围限制；压测与服务器不要同机跑满核
5. **日志容量**：消息转发摘要高频写入，spdlog 必须按天滚动 + 容量上限，避免磁盘写满
6. **安全红线**：密码哈希、SQL 预处理、协议长度校验是安全底线，禁止"先上线后补"
7. **性能陷阱**：任何在 IO 线程上的阻塞调用（MySQL/Redis/文件 IO）都会拖垮整个 Reactor，代码评审时作为一票否决项
8. **客户端旧代码清理**：模拟下线的调试定时器、`sendData` 里无意义的 `to=6` 参数、双份 def.h 等，随对应 Phase 顺手删除，避免越积越多

---

## 附：技术栈速查

| 用途 | 技术 | 引入 Phase |
|------|------|-----------|
| 构建 | CMake 3.16+ / vcpkg | 0 |
| 配置 | nlohmann/json（服务端）、ini（客户端） | 0 |
| 日志 | spdlog（异步 + 按天滚动） | 0 |
| 序列化 | Protobuf 3 + protoc | 1 |
| 网络 | epoll / select + 自研 Reactor（muduo 思路）；客户端 QTcpSocket | 2 |
| 加密 | OpenSSL（SHA-256、MD5、RAND_bytes） | 3 / 10 |
| 存储 | MySQL（InnoDB、预处理语句、组合索引） | 3 / 5 |
| 缓存 | Redis + hiredis | 9 |
| 测试 | GoogleTest + CTest | 11 |
| 压测 | Python asyncio / C++ asio | 12 |
