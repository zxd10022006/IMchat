#ifndef __DEF_H__
#define __DEF_H__

// ===== 协议类型 =====
#define DEF_PROT_BASE 1000
#define DEF_PROT_REGISTER_RQ   (DEF_PROT_BASE + 0)
#define DEF_PROT_REGISTER_RS   (DEF_PROT_BASE + 1)
#define DEF_PROT_LOGIN_RQ      (DEF_PROT_BASE + 2)
#define DEF_PROT_LOGIN_RS      (DEF_PROT_BASE + 3)
#define DEF_PROT_FRIEND_INFO   (DEF_PROT_BASE + 4)
#define DEF_PROT_CHAT_INFO_RQ  (DEF_PROT_BASE + 5)
#define DEF_PROT_CHAT_INFO_RS  (DEF_PROT_BASE + 6)
#define DEF_PROT_ADD_FRIEND_RQ (DEF_PROT_BASE + 7)
#define DEF_PROT_ADD_FRIEND_RS (DEF_PROT_BASE + 8)
#define DEF_PROT_FRIEND_OFFLINE (DEF_PROT_BASE + 9)

#define PROTO_COUNT 10

// ===== 注册结果 =====
#define REGIS_SUCC 0        // 注册成功
#define REGIS_NICK_EXISTS 1 // 昵称已存在
#define REGIS_TEL_EXISTS 2  // 手机号已存在

// ===== 登录结果 =====
#define LOGIN_SUCC 0     // 成功
#define LOGIN_NOEX 1     // 用户不存在
#define LOGIN_PASSERR 3  // 密码错误

// ===== 在线状态 =====
#define USER_ONLINE 0
#define USER_OFFLINE 1

// ===== 聊天 =====
#define MAX_MSG (8 * 1024) // 消息最大长度
#define CHAT_RES_SUCC 0    // 发送成功
#define CHAT_RES_FALT 1    // 发送失败

// ===== 加好友结果 =====
#define ADD_FRI_RESULT_ACCP 0   // 同意
#define ADD_FRI_RESULT_REFU 1   // 拒绝
#define ADD_FRI_RESULT_OFF 2    // 对方不在线
#define ADD_FRI_RESULT_NOEXIT 3 // 对方不存在

// ===== 端口 =====
#define duan_TCP 4321

using protType = unsigned int;

// 注意：所有协议结构体使用固定大小的 char 数组（如 tel[15]），
// 源字符串超长时 strcpy_s 会直接崩溃，生产环境建议改用 std::string + 序列化

// 注册请求
struct PROT_REGISTER_RQ {
	protType prottype;
	char tel[15];
	char nick[30];
	char passwd[20];
	PROT_REGISTER_RQ() : prottype(DEF_PROT_REGISTER_RQ), tel{ 0 }, nick{ 0 }, passwd{ 0 } {}
};

// 注册应答
struct PROT_REGISTER_RS {
	protType prottype;
	int result;
	PROT_REGISTER_RS() : prottype(DEF_PROT_REGISTER_RS), result(REGIS_SUCC) {}
};

// 登录请求
struct PROT_LOGIN_RQ {
	protType prottype;
	char tel[15];
	char passwd[20];
	PROT_LOGIN_RQ() : prottype(DEF_PROT_LOGIN_RQ), tel{ 0 }, passwd{ 0 } {}
};

// 登录应答
struct PROT_LOGIN_RS {
	protType prottype;
	int userid; // 当前登录用户的 id
	int result; // 成功 失败 不存在
	PROT_LOGIN_RS() : prottype(DEF_PROT_LOGIN_RS), userid(0), result(LOGIN_SUCC) {}
};

// 好友信息
struct PROT_FRIEND_INFO {
	protType prottype;
	int userid;
	int imgid;
	int status;
	char nick[30];
	char feeling[100];
	PROT_FRIEND_INFO() : prottype(DEF_PROT_FRIEND_INFO), userid(0), imgid(0), status(USER_ONLINE), nick{ 0 }, feeling{ 0 } {}
};

// 聊天请求
struct PROT_CHAT_INFO_RQ {
	protType prottype;
	int userid;
	int friid;
	char msg[MAX_MSG];
	PROT_CHAT_INFO_RQ() : prottype(DEF_PROT_CHAT_INFO_RQ), userid(0), friid(0), msg{ 0 } {}
};

// 聊天应答
struct PROT_CHAT_INFO_RS {
	protType prottype;
	int userid;
	int friid;
	int result;
	PROT_CHAT_INFO_RS() : prottype(DEF_PROT_CHAT_INFO_RS), userid(0), friid(0), result(CHAT_RES_SUCC) {}
};

// 加好友请求
struct PROT_ADD_FRIEND_RQ {
	protType prottype;
	int userid;
	char usernick[30];
	char frinick[30];
	PROT_ADD_FRIEND_RQ() : prottype(DEF_PROT_ADD_FRIEND_RQ), userid(0), usernick{ 0 }, frinick{ 0 } {}
};

// 加好友应答
struct PROT_ADD_FRIEND_RS {
	protType prottype;
	int result;
	int friid;
	char friNick[30];
	char userNick[30];
	int userid;
	PROT_ADD_FRIEND_RS() : prottype(DEF_PROT_ADD_FRIEND_RS), result(ADD_FRI_RESULT_ACCP), friid(0), friNick{ 0 }, userNick{ 0 }, userid(0) {}
};

// 好友下线通知
struct PROT_FRIEND_OFFLINE {
	protType prottype;
	int userid;
	PROT_FRIEND_OFFLINE() : prottype(DEF_PROT_FRIEND_OFFLINE), userid(0) {}
};

#endif
