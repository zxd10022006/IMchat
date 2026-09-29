#pragma once
#include "mediator/INetMed.h"
#include "MySQL/CMySql.h"
#include "net/def.h"
#include <iostream>
#include <map>
#include <list>
#include <string>
#include <functional>

class Kernel {
private:
	INetMed* m_INetMed;
	using DealFun = void (Kernel::*)(char*, int, unsigned long);
	CMySql m_sql;

	// 保存在线用户 id → socket（同时表示用户的登录状态）
	std::map<int, SOCKET> m_map_id_socket;

	// 协议类型 → 处理函数的映射表
	DealFun m_funArr[PROTO_COUNT];

	// ===== 辅助函数 =====
	// 执行查询并把结果按行按列展开写入 out；失败时打印错误并返回 false
	bool sqlSelect(const char* sql, int cols, std::list<std::string>& out);
	// 校验某字段的值在 t_user 表中是否已存在；已存在则把失败结果发回客户端
	bool fieldExists(const char* field, const char* value, int result, unsigned long to);
	// 给某个在线用户发数据，不在线返回 false
	bool sendToUser(int uid, char* data, int len);
	// 遍历某用户的好友 id 列表（查 t_friend 表的 idA），对每个好友调用 fn
	void forEachFriend(int id, std::function<void(int)> fn);

public:
	Kernel();
	~Kernel();

	// 注册协议处理函数表
	void setFunArr();

	// 打开服务器
	bool startServer();
	// 关闭服务器
	void endServer();
	// 数据分发函数（把收到的数据按协议类型分发给对应处理函数）
	void deal_Data(char* data, int len, unsigned long to);

	static Kernel* m_kernel;

	// 注册请求
	void deal_register_RQ(char* data, int len, unsigned long to);
	// 登录请求
	void deal_login_RQ(char* data, int len, unsigned long to);
	// 查询自己和好友的信息（登录后显示好友列表用）
	void getUandFInfor(int id);
	// 根据用户 id 查询用户信息
	void getInfor_id(int id, PROT_FRIEND_INFO* infor);
	// 下线请求
	void deal_OfflineRq(char* data, int len, unsigned long from);
	// 聊天请求
	void deal_ChatRq(char* data, int len, unsigned long from);
	// 加好友请求
	void deal_AddFriRq(char* data, int len, unsigned long from);
	// 加好友应答
	void deal_AddFriRs(char* data, int len, unsigned long from);
};
