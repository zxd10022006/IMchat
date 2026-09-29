# IM 即时通讯项目简化优化计划(V2)

> 原则:复用现有技术栈(WinSock + 定长结构体协议 + libmysql + Qt),**零新下载依赖**;
> 以复习为主、学习新东西为辅;每一步小步增量,做完可编译可运行、现有功能不回退。
>
> 构建系统:服务端回到 VS .sln,客户端保持 Qt Creator/CMake(放弃 vcpkg 路线,CMake 实验文件保留不动)。

## 与原 15 项目标的关系

| 处理 | 目标 |
|------|------|
| 保持/微调 | 注册登录、TCP 长连接+粘包、一对一私聊、好友系统 |
| 简化实现 | 心跳(Step 4)、离线消息(Step 5)、群聊(Step 7)、ack(Step 6)、持久化分页(Step 5)、黑白名单(Step 8)、文件传输(Step 9)、日志(Step 3,自写)、单测(Step 10,自写框架)、压测(Step 11,自写) |
| 取消 | Protobuf(保留结构体协议,两端 def.h 手动同步并加注释提醒)、Redis(单机内存会话够用)、vcpkg |
| 降级为可选 | 多 Reactor → Step 12 的 select/IOCP 进阶(学有余力再做) |

## 技术栈(全部零下载)

| 组件 | 技术 | 说明 |
|------|------|------|
| 网络 | WinSock2 + 4 字节长度头 + 每连接一线程 | 保持现状 |
| 协议 | 定长结构体 + 协议号查表分发 | 保持现状 |
| 数据库 | 本机 MySQL 5.7(libmysql) | 保持现状,Step 2 加转义 |
| 密码哈希 | Windows BCrypt(SHA-256 + 盐) | 新知识,系统自带 API,零下载 |
| 日志 | 自写 Log 类(单例+互斥+按天滚动) | 纯复习 |
| 测试 | 自写 MiniTest 迷你框架 | 纯复习 |
| 压测 | 自写 C++ 控制台客户端 | 纯复习 |
| 客户端 | Qt Widgets + QTcpSocket 同款模型 | 保持现状 |

---

## Step 0:基线清理(半天)

**做什么**:确认双端能正常编译运行,清掉之前 CMake/vcpkg 实验的垃圾。

1. 服务端:用 VS 打开 .sln,确认编译通过、能跑
2. 客户端:用 Qt Creator 打开,确认编译通过
3. 删除根目录 `build/` 目录(vcpkg 失败的缓存垃圾);根目录 CMakeLists.txt、IMServer/CmakeLists.txt 保留不动(不影响 .sln),想删也行
4. 确认 MySQL 5.7 服务在跑、IM 库和 t_user/t_friend 表存在
5. 跑一遍现有全功能:注册、登录、好友列表、单聊、加好友、离线通知

**验收**:双端正常,现有功能全部 OK。这步不做任何代码修改。

---

## Step 1:修已知 bug(1~2 个晚上,纯复习)

**做什么**:修 4 个现存问题,全部在现有架构内。

1. **sendAll 短写 bug**:[INet.cpp](../../IMServer/IMServer/net/INet.cpp) 的 `sendAll` 只 `send` 一次——TCP 发送不一定一次发完(短写),必须循环发送直到发完或出错。同样修 [TcpClient.cpp](../../IMClient/net/TcpClient.cpp) 的 sendData。
   - 复习:TCP 流语义、send 返回值含义
2. **异常断连残留**:客户端强杀进程后,服务端 `m_map_id_socket` 里 uid 永久残留,好友也不知道他下线了。改法:
   - INetMed 增加虚函数 `onDisconnect(SOCKET)`;[TcpServer.cpp](../../IMServer/IMServer/net/TcpServer.cpp) 的 recvData 循环退出时回调它
   - Kernel 实现:反向遍历 `m_map_id_socket` 找到该 socket 对应的 uid → 移除 → 给所有在线好友发 FRIEND_OFFLINE(把 deal_OfflineRq 里的"通知好友"逻辑抽成公共函数 `notifyFriendsOffline(uid)` 复用)
   - 复习:虚函数/回调、反向映射、资源清理
3. **删客户端调试残留**:[kernel.cpp](../../IMClient/kernel.cpp) 里每 8 秒模拟好友 7 下线的 `m_frOFFlineTimer`(构造、connect、槽函数)整段删除
4. **登录时序**:[Kernel.cpp](../../IMServer/IMServer/Kernel.cpp) 的 deal_login_RQ 改为**先发 LOGIN_RS 再调 getUandFInfor**;客户端 [kernel.cpp](../../IMClient/kernel.cpp) 里 `m_pendingFriendInfo` 暂存 hack 连带删除
   - 复习:协议时序设计、为何应答要最先发

**验收**:强杀客户端进程 → 5 秒内服务端移除该用户、其好友收到下线通知;登录后好友列表正常;双端无调试残留输出。

---

## Step 2:密码哈希 + SQL 转义(1~2 个晚上,少量新知识)

**做什么**:堵住明文密码和 SQL 注入,全部用系统自带 API。

1. **BCrypt 封装** `SHA256` 工具函数:
   - `BCryptOpenAlgorithmProvider(BCRYPT_SHA256_ALGORITHM)` → `BCryptHash` 算哈希,输出 64 位十六进制字符串
   - 盐:用 `BCryptGenRandom` 生成 16 字节随机盐(十六进制存储)
   - 封装成 `PasswordUtil::hash(salt, passwd)` 和 `PasswordUtil::genSalt()`
   - 新知识:BCrypt API 调用流程(provider → hash → close)
2. **t_user 表改造**:加列 `pass_hash CHAR(64)`、`pass_salt CHAR(16)`;老数据用**惰性迁移**——登录时若 `pass_hash` 为空,拿老明文比对,通过则顺手算哈希回填(不用写迁移工具,几行代码)
3. **注册/登录改哈希**:
   - 注册:生成盐 → 存 hash+salt,不再存明文
   - 登录:按 tel 取盐 → 算哈希 → 比对
4. **SQL 转义**:CMySql 增加 `static std::string escape(const char*)`(调 `mysql_real_escape_string`);[Kernel.cpp](../../IMServer/IMServer/Kernel.cpp) 里所有把用户输入(tel/nick/passwd/frinick)拼进 SQL 的地方先转义
   - 复习:SQL 注入原理、为何转义能防
   - 新知识:mysql_real_escape_string 用法

**验收**:数据库里没有任何明文密码;老账号能正常登录;注册昵称带 `'` 或中文不出错;用 `' OR '1'='1` 当昵称注册/加好友无注入效果。

---

## Step 3:自写日志系统(1 个晚上,纯复习)

**做什么**:不引 spdlog,自己写一个够用的 Log 类,替换全部 `std::cout`。

1. 新建 [Log.h/.cpp](../../IMServer/IMServer/Log.h):单例模式,成员 `std::ofstream` + `std::mutex`
   - `Log::info/Log::error(const std::string&)`:拼 `[2026-09-28 20:15:03][INFO] 内容` 写文件 + 控制台
   - 按天滚动:打开文件时记录当天日期,写入前发现日期变了就关旧开新(`logs/server_YYYYMMDD.log`)
   - 时间格式化:`localtime_s` + `strftime`
   - 复习:单例模式、互斥锁、文件流、时间格式化
2. **替换清单**:
   - [TcpServer.cpp](../../IMServer/IMServer/net/TcpServer.cpp):连接建立/断开(记录 socket 号)
   - [Kernel.cpp](../../IMServer/IMServer/Kernel.cpp):登录/注册成功失败(记录 uid/昵称)、消息转发摘要(uid→uid、字节数,**不记消息内容**)、异常
   - [CMySql.cpp](../../IMServer/IMServer/MySQL/CMySql.cpp):SQL 失败时记录完整 SQL 语句
   - [main.cpp](../../IMServer/IMServer/main.cpp):删掉无限循环里的 "server is running" 打印,改成启动完成打一条
3. `logs/` 目录加进 .gitignore

**验收**:运行全流程后 `logs/server_当前日期.log` 里能看到连接、登录、转发摘要、错误四类记录;第二天重启后日志自动开新文件。

---

## Step 4:心跳机制 + 超时踢下线(1~2 个晚上,复习+新协议)

**做什么**:解决"客户端死了服务端不知道"的问题。

1. **协议**:两端 def.h 同步加(文件头加注释"修改时两端必须同步"):`PROT_PING`/`PROT_PONG`(protType + userid),`PROTO_COUNT` 相应扩大
2. **服务端**(利用每连接一线程的现状,改动极小):
   - recv 循环前 `setsockopt(SO_RCVTIMEO, 10s)` 设接收超时
   - 线程局部记 `lastRecvTime`,收到任何包(含 PING)刷新;recv 超时后检查:超过 30s 无任何数据 → 主动断连,走 Step 1 的 onDisconnect 清理(踢下线 + 通知好友)
   - 收到 PING 回 PONG(带服务器时间,客户端可算 RTT)
   - 复习:setsockopt、时间差计算
3. **客户端**:
   - `QTimer` 每 10s 发 PING
   - 30s 没收到任何包 → 弹窗提示"与服务器断开连接"
   - (可选)自动重连:指数退避 1s/2s/4s 重试,连上后自动重新登录
   - 复习:QTimer、状态机

**验收**:客户端强杀 → 服务端 30s 内踢人、好友列表变灰;正常挂机 1 小时不断连;双方同时在线时 PING/PONG 日志可见。

---

## Step 5:离线消息 + 历史分页(2~3 个晚上,纯复习)

**做什么**:消息全部落库,离线不丢,历史可翻页。这是后续 ack/群聊的地基。

1. **建表** `t_message`:
   ```sql
   CREATE TABLE t_message (
     msg_id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
     from_uid INT UNSIGNED NOT NULL,
     to_uid INT UNSIGNED NOT NULL,        -- 群聊时存 group_id
     to_type TINYINT NOT NULL DEFAULT 0,  -- 0 单聊 1 群聊(Step 7 用)
     msg_type TINYINT NOT NULL DEFAULT 0, -- 0 文本 1 系统 2 文件(Step 9 用)
     content VARCHAR(8192) NOT NULL,
     status TINYINT NOT NULL DEFAULT 0,   -- 0 未读 1 已读
     create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
     KEY idx_offline (to_uid, to_type, status),
     KEY idx_history (to_uid, to_type, msg_id)
   ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
   ```
2. **发消息流程改造**(deal_ChatRq):
   - 先 insert 落库(拿到 msg_id)→ 对方在线:转发 + 把这条 status 置 1;离线:留存,不回"发送失败"
   - 发送方收到的 RS 含义改为"服务器已收妥"(带 msg_id)
   - 复习:INSERT 后取自增 id(`mysql_insert_id`)
3. **离线拉取(简单版,无需新协议)**:
   - 登录成功后,服务端 `select ... where to_uid=我 and status=0 order by msg_id` 把未读消息**逐条转发**给用户,转发前 update status=1(先标已读再发,发丢了也不重复)
   - 客户端不用改任何逻辑——离线消息就是普通聊天消息,只是晚点到
4. **历史分页(新协议)**:
   - def.h 加 `PROT_HISTORY_RQ{prottype,userid,friid,last_msg_id}` / `PROT_HISTORY_RS{prottype,msg_id,from_uid,content,create_time}`(RS 逐条发,发完发一条 msg_id=0 表示结束,简单可靠)
   - SQL:`where ((from_uid=u and to_uid=f) or (from_uid=f and to_uid=u)) and msg_id < last order by msg_id desc limit 20`
   - 复习:LIMIT 分页、组合条件索引
5. **客户端**:聊天窗口顶部"加载更多"按钮,点击发 HISTORY_RQ,把历史消息倒序插到消息区顶部

**验收**:A 给离线 B 发消息,B 登录后自动收到且按时间序完整;500 条历史分页翻到底,每页响应 < 100ms;重连后拉取不重复。

---

## Step 6:消息回执 ack + 重传(1~2 个晚上,复习+一点新概念)

**做什么**:"发出去的消息一定到、且只到一次"。

1. **协议**:PROT_CHAT_INFO_RQ 加 `int client_seq`(客户端自增序号);PROT_CHAT_INFO_RS 加 `int client_seq` 和 `int msg_id`
2. **客户端发送队列**:
   - 发送后把消息放入 `std::map<int seq, 待确认消息>`(含 QTimer 3s 定时)
   - 收到 RS(按 client_seq 匹配)→ 移除,聊天框显示"已送达"
   - 3s 没收到 → 重发原包(同 seq);3 次 → 标红"发送失败",点击可重发
   - 复习:map、QTimer、状态机
3. **服务端去重(幂等)**:
   - 内存 map `(from_uid, client_seq) → 处理时间`,收到消息先查:5s 内重复 → 不重复落库,只重发 RS
   - 定期清理过期条目
   - 新概念:幂等性、去重窗口
4. **显示升级**:客户端聊天消息显示"发送中 → 已送达 → 失败"三态

**验收**:断网 10s 恢复后 pending 消息自动重传成功、接收方只收到一次;服务端日志可见"重复消息被丢弃"记录;聊天记录里每条消息有送达状态。

---

## Step 7:基础群聊(2 个晚上,复习)

**做什么**:建群、加群、群内广播,复用 Step 5 的落库和离线通道。

1. **建表**:
   ```sql
   CREATE TABLE t_group (group_id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
     name VARCHAR(50) NOT NULL UNIQUE, owner_uid INT UNSIGNED NOT NULL,
     create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP);
   CREATE TABLE t_group_member (group_id INT UNSIGNED NOT NULL, uid INT UNSIGNED NOT NULL,
     join_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY(group_id, uid));
   ```
2. **协议**(两端 def.h 同步):`PROT_CREATE_GROUP_RQ/RS`、`PROT_JOIN_GROUP_RQ/RS`、`PROT_GROUP_CHAT_RQ{group_id,from_uid,content}`、`PROT_GROUP_INFO_RQ/RS`(RS 逐条发成员信息,与好友列表同套路)
3. **服务端**:
   - 建群:insert t_group + 群主入 t_group_member,给群主发 GROUP_INFO_RS
   - 加群:insert 成员表,给全体成员广播系统消息"XXX 加入群聊"(t_message 落库,msg_type=1,to_type=1,离线成员靠 Step 5 补收)
   - 群消息:落库(to_type=1)→ `select uid from t_group_member where group_id=?` → 遍历成员 sendToUser(在线转发,离线留库)
   - 复习:多表插入、遍历广播
4. **客户端**:
   - 主界面加"群聊"页签:我的群列表 + "创建群""加群"按钮
   - 新建 GroupChat 类(参考现有 Chat 类改造):消息区 + 成员列表 + 输入区;群消息显示"昵称: 内容"

**验收**:3 人建群后群广播 3 人实时收到;1 人离线时其余正常,离线者上线补收;退群后不再收到(基础版可不做退群,记录为待办)。

---

## Step 8:黑白名单(1 个晚上,复习)

**做什么**:拉黑后拒收对方消息。

1. **建表**:`t_block(uid INT, block_uid INT, create_time DATETIME, PRIMARY KEY(uid, block_uid))`
2. **协议**:`PROT_BLOCK_RQ{userid,block_uid,op}`(op:0 拉黑 1 解除)/ `PROT_BLOCK_RS{result}`;CHAT_INFO_RS 的 result 加一个 `CHAT_RES_BLOCKED` 常量
3. **服务端**:
   - 登录时把自己的黑名单加载进 `std::unordered_set<int> m_blacklist`(Kernel 成员),变更时落库后同步更新内存
   - deal_ChatRq 转发前检查:对方黑名单里有我 → 回 RS"被拒收"(不落库不转发);我的黑名单里有对方 → 直接拒绝自己发送
   - 复习:内存缓存与 DB 一致性(先写库成功再改内存)
4. **客户端**:好友列表项右键菜单"拉黑/取消拉黑";收到"被拒收"提示

**验收**:A 拉黑 B 后,B 发消息收到"被拒收"提示,A 毫无感知;解除拉黑后恢复;重启服务器(重新登录)后黑名单仍然生效(落库验证)。

---

## Step 9:小文件传输(2 个晚上,复习)

**做什么**:≤10MB 文件点对点传输,服务端中转,带进度条。

1. **协议**(两端 def.h 同步):
   - `PROT_FILE_RQ{prottype,userid,friid,file_id,file_name[100],file_size,md5[33]}`
   - `PROT_FILE_RS{prottype,friid,userid,file_id,result}`(0 接受 1 拒绝)
   - `PROT_FILE_CHUNK{prottype,userid,friid,file_id,seq,total,data[4096],len}`
   - `PROT_FILE_DONE{prottype,userid,friid,file_id,ok}`
2. **服务端**:纯转发——收到 CHUNK 转发给接收方(不落盘、不缓存),转发 DONE;限单文件 10MB
   - 复习:大流量转发路径与普通消息的区别
3. **客户端**:
   - 聊天窗口"发送文件"按钮 → QFileDialog → 发 FILE_RQ;对方弹窗"接受/拒绝"
   - 发送方 QFile 按 4096 字节分片循环发;接收方 QFile 顺序写
   - 进度条(已收字节/总大小);完成后计算 md5 校验(Windows BCrypt 里顺手加 MD5 或简单比较大小即可,md5 可选)
   - 复习:QFile、分片循环、进度条(QProgressBar)
4. **注意**:文件传输期间聊天消息走同一条连接,chunk 别把消息通道塞死——chunk 发送循环里适当 `Sleep(1)` 让消息插队(简单实用)

**验收**:1MB 文件双方收发成功、内容一致;传输中对方消息正常收到;10MB 文件传完进度条准确。

---

## Step 10:单元测试 Demo(1~2 个晚上,复习)

**做什么**:不引 gtest,自写迷你测试框架,给关键模块补测试。

1. **自写 MiniTest.h**(纯头文件):
   - 宏 `TEST_CASE(name)` 注册测试函数(静态注册表,`std::vector<std::pair<const char*, void(*)()>>`)
   - 宏 `EXPECT_EQ(a,b)`/`EXPECT_TRUE(x)`:不通过打印文件名行号、失败计数累加
   - `main()` 跑全部注册用例,输出 `X passed, Y failed`,失败返回非零
   - 复习:宏、函数指针、静态初始化注册
2. **新工程** `tests/`(VS console 工程,include IMServer 源码路径即可):用例清单:
   - SHA256 已知向量("abc" 的哈希与标准值一致)
   - 转义函数:注入串转义后查不到"裸引号"
   - **拆包纯函数**:把拆包逻辑从 recvData 抽成 `PacketParser` 类(append 数据、tryParse 出完整包),测试粘包(两个包拼一起一次到达)、半包(一个包分 3 次到达)
   - Log 写入一行后文件存在且含关键字
   - 分页 SQL(连测试库):插入 30 条,分页 20+10 拉全
3. 每个后续 Step 的新逻辑都顺手补一个用例(Step 6 去重、Step 8 黑名单判断等纯函数)

**验收**:`tests.exe` 运行输出全绿;故意改错一个断言能看到清晰的失败信息。

---

## Step 11:压测脚本 + 报告(2 个晚上,复习)

**做什么**:自写控制台压测客户端,量化并发能力。

1. **新工程** `IMBench/`(VS console):把 IMServer 的 INet/TcpClient 代码拷一份简化版(连接 + 长度头收发)
   - 参数:`-c 连接数 -m 每连接消息数`;每线程一个连接(复用每连接一线程模式,简单)
   - 流程:自动注册随机账号(昵称 bench_i)→ 登录 → 随机配对互发消息 → 统计:总消息数、总耗时 → 输出 **TPS、平均延迟、失败数**
   - 复习:多线程、std::chrono 计时、命令行参数
2. **跑压测**:500 连接 × 各 50 条消息,记录结果;Step 1~11 完成后重跑对比
3. **输出报告**:README.md/bench_report.md 记录环境、参数、结果

**验收**:能跑出可信数字(≥ 几千 msg/s 量级);压测过程中服务端不崩、内存不涨;报告归档可复现。

---

## Step 12(可选进阶):网络层 IO 复用

学有余力再做,三选一:

- **A. select 单线程事件循环**:用 select 管理所有连接,替换每连接一线程。经典复习内容,Windows 下直接可用,服务端线程数从 N 连接降到 1 收 + 1 发
- **B. IOCP**:Windows 高性能完成端口,新知识较多,面试价值高
- **C. 小优化**:不动架构,只做 closeNet 优雅关闭、线程句柄及时回收、发送缓冲

---

## 节奏建议

| 里程碑 | 步骤 | 预计 |
|--------|------|------|
| M1 修 bug + 安全 | 0~2 | 第 1 周 |
| M2 日志 + 心跳 | 3~4 | 第 2 周 |
| M3 离线消息 + ack | 5~6 | 第 3~4 周 |
| M4 群聊 + 黑名单 + 文件 | 7~9 | 第 5~6 周 |
| M5 测试 + 压测 | 10~11 | 第 7 周 |
| M6 可选进阶 | 12 | 学有余力 |

每步完成后跑一遍现有功能回归(注册/登录/单聊/加好友)+ 该步验收标准,再进入下一步。
