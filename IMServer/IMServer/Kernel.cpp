#include "Kernel.h"
#include "mediator/TcpServerMed.h"
#include <iterator>

Kernel* Kernel::m_kernel = nullptr;

Kernel::Kernel() {
	m_INetMed = new TcpServerMed;
	m_kernel = this;
}

Kernel::~Kernel() {
	if (m_INetMed) delete m_INetMed;
	m_INetMed = nullptr;
}

// ===== 辅助函数 =====

// 执行查询并把结果按行按列展开写入 out；失败时打印错误并返回 false
bool Kernel::sqlSelect(const char* sql, int cols, std::list<std::string>& out) {
	if (!m_sql.SelectMySql(sql, cols, out)) {
		std::cout << "查询数据库失败 sql:" << sql << std::endl;
		return false;
	}
	return true;
}

// 校验某字段的值在 t_user 表中是否已存在
// 已存在：把失败结果发回客户端并返回 true；查询出错：按已存在处理（终止流程）
bool Kernel::fieldExists(const char* field, const char* value, int result, unsigned long to) {
	char sql[1024] = { 0 };
	sprintf_s(sql, "select %s from t_user where %s = '%s';", field, field, value);
	std::list<std::string> listRes;
	if (!sqlSelect(sql, 1, listRes)) {
		return true;
	}
	if (listRes.empty()) {
		return false;
	}
	PROT_REGISTER_RS rs;
	rs.result = result;
	m_INetMed->sendData((char*)&rs, sizeof(rs), to);
	return true;
}

// 给某个在线用户发数据，不在线返回 false
bool Kernel::sendToUser(int uid, char* data, int len) {
	auto it = m_map_id_socket.find(uid);
	if (it == m_map_id_socket.end()) {
		std::cout << "用户不在线 userid:" << uid << std::endl;
		return false;
	}
	// SOCKET 转 u_long：Windows 保证 socket 句柄值在 32 位范围内，截断是安全的
	return m_INetMed->sendData(data, len, (u_long)it->second);
}

// 遍历某用户的好友 id 列表（t_friend 表双向存储，查 idB 得到好友 idA），对每个好友调用 fn
void Kernel::forEachFriend(int id, std::function<void(int)> fn) {
	char sql[1024] = { 0 };
	sprintf_s(sql, "select idA from t_friend where idB = %d;", id);
	std::list<std::string> listRes;
	if (!sqlSelect(sql, 1, listRes)) {
		return;
	}
	for (auto& s : listRes) {
		fn(std::stoi(s));
	}
}

// ===== 服务器生命周期 =====

// 注册协议处理函数表
void Kernel::setFunArr() {
	memset(m_funArr, 0, sizeof(m_funArr));

	m_funArr[DEF_PROT_REGISTER_RQ - DEF_PROT_BASE] = &Kernel::deal_register_RQ;
	m_funArr[DEF_PROT_LOGIN_RQ - DEF_PROT_BASE] = &Kernel::deal_login_RQ;
	m_funArr[DEF_PROT_FRIEND_OFFLINE - DEF_PROT_BASE] = &Kernel::deal_OfflineRq;
	m_funArr[DEF_PROT_CHAT_INFO_RQ - DEF_PROT_BASE] = &Kernel::deal_ChatRq;
	m_funArr[DEF_PROT_ADD_FRIEND_RQ - DEF_PROT_BASE] = &Kernel::deal_AddFriRq;
	m_funArr[DEF_PROT_ADD_FRIEND_RS - DEF_PROT_BASE] = &Kernel::deal_AddFriRs;
}

// 打开服务器
bool Kernel::startServer() {
	std::cout << __func__ << std::endl;
	setFunArr();
	m_INetMed->openNet();

	// 连接数据库
	char ip[] = "127.0.0.1";
	char name[] = "root";
	char pass[] = "123456";
	char db[] = "IM";
	if (!m_sql.ConnectMySql(ip, name, pass, db)) {
		std::cout << "连接数据库失败" << std::endl;
		return false;
	}
	std::cout << "连接数据库成功" << std::endl;
	return true;
}

// 关闭服务器
void Kernel::endServer() {
	m_INetMed->closeNet();
	m_sql.DisConnect();
}

// 数据分发函数（把收到的数据按协议类型分发给对应处理函数）
void Kernel::deal_Data(char* data, int len, unsigned long to) {
	protType type = *(protType*)data;

	int funDex = type - DEF_PROT_BASE;

	if (funDex >= 0 && funDex < PROTO_COUNT) {
		DealFun pf = m_funArr[funDex];
		if (pf) {
			(this->*pf)(data, len, to);
		}
		else {
			std::cout << "未处理的协议类型:" << type << std::endl;
		}
	}
	else {
		std::cout << "非法的协议类型:" << type << std::endl;
	}
}

// ===== 协议处理函数 =====

// 注册请求
void Kernel::deal_register_RQ(char* data, int len, unsigned long to) {
	PROT_REGISTER_RQ* rq = (PROT_REGISTER_RQ*)data;
	std::cout << "nick:" << rq->nick << " passwd:" << rq->passwd << " tel:" << rq->tel << std::endl;

	PROT_REGISTER_RS rs;
	// 1. 校验昵称是否已注册
	if (fieldExists("nick", rq->nick, REGIS_NICK_EXISTS, to)) {
		return;
	}
	// 2. 校验手机号
	if (fieldExists("tel", rq->tel, REGIS_TEL_EXISTS, to)) {
		return;
	}

	// 3. 注册信息写入数据库
	char sql[1024] = { 0 };
	sprintf_s(sql, sizeof(sql),
		"insert into t_user (nick,tel,pass,feeling,iconid) values('%s','%s','%s','学习太快乐',35);",
		rq->nick, rq->tel, rq->passwd);
	if (!m_sql.UpdateMySql(sql)) {
		std::cout << "更新数据库失败 sql:" << sql << std::endl;
		return;
	}
	// 注册成功，给客户端返回成功标识
	rs.result = REGIS_SUCC;
	m_INetMed->sendData((char*)&rs, sizeof(rs), to);
}

// 根据用户 id 查询用户信息（昵称/签名/头像/在线状态）
void Kernel::getInfor_id(int id, PROT_FRIEND_INFO* infor) {
	infor->userid = id;
	infor->status = m_map_id_socket.count(id) > 0 ? USER_ONLINE : USER_OFFLINE;

	char sql[1024] = { 0 };
	sprintf_s(sql, "select nick,feeling,iconid from t_user where id = %d;", id);
	std::list<std::string> listRes;
	if (!sqlSelect(sql, 3, listRes)) {
		return;
	}

	if (listRes.size() == 3) {
		strcpy_s(infor->nick, 30, listRes.front().c_str());
		listRes.pop_front();
		strcpy_s(infor->feeling, 100, listRes.front().c_str());
		listRes.pop_front();
		infor->imgid = std::stoi(listRes.front());
	}
	else {
		std::cout << "用户信息没找到 sql:" << sql << std::endl;
	}
}

// 查询自己和好友的信息（登录后显示好友列表用）
void Kernel::getUandFInfor(int id) {
	// 1. 查询自己的信息，发给客户端
	PROT_FRIEND_INFO m_infor;
	getInfor_id(id, &m_infor);
	sendToUser(id, (char*)&m_infor, sizeof(m_infor));

	// 2. 把自己的信息发给每个好友，并把好友的信息发给自己
	PROT_FRIEND_INFO friInfor;
	forEachFriend(id, [&](int friId) {
		getInfor_id(friId, &friInfor);
		sendToUser(id, (char*)&friInfor, sizeof(friInfor));
		sendToUser(friId, (char*)&m_infor, sizeof(m_infor));
	});
}

// 登录请求
void Kernel::deal_login_RQ(char* data, int len, unsigned long to) {
	PROT_LOGIN_RQ* rq = (PROT_LOGIN_RQ*)data;
	std::cout << "账号:" << rq->tel << " 密码:" << rq->passwd << std::endl;

	PROT_LOGIN_RS rs{};
	char sql[256]{};
	// 数据库字段：密码 pass、用户 id
	sprintf_s(sql, "SELECT pass,id FROM t_user WHERE tel='%s' LIMIT 1", rq->tel);

	std::list<std::string> res;
	if (sqlSelect(sql, 2, res) && !res.empty()) {
		if (res.front() == rq->passwd) {
			rs.result = LOGIN_SUCC;
			std::cout << "登录成功" << std::endl;
			// 根据当前 id 获取自己和好友的信息，用于显示好友列表
			int userid = std::stoi(*std::next(res.begin()));
			rs.userid = userid;
			m_map_id_socket[userid] = to;
			getUandFInfor(userid);
		}
		else {
			rs.result = LOGIN_PASSERR;
			std::cout << "密码错误" << std::endl;
		}
	}
	else {
		rs.result = LOGIN_NOEX;
		std::cout << "账号未注册" << std::endl;
	}

	m_INetMed->sendData((char*)&rs, sizeof(rs), to);
}

// 下线请求
void Kernel::deal_OfflineRq(char* data, int len, unsigned long from) {
	PROT_FRIEND_OFFLINE* Rq = (PROT_FRIEND_OFFLINE*)data;

	// 1. 把下线消息转发给所有在线好友
	forEachFriend(Rq->userid, [&](int friId) {
		sendToUser(friId, data, len);
	});

	// 2. 关闭该用户的 socket，并移除登录记录
	auto it = m_map_id_socket.find(Rq->userid);
	if (it != m_map_id_socket.end()) {
		closesocket(it->second);
		m_map_id_socket.erase(it);
	}
}

// 聊天请求
void Kernel::deal_ChatRq(char* data, int len, unsigned long from) {
	PROT_CHAT_INFO_RQ* Rq = (PROT_CHAT_INFO_RQ*)data;
	if (!sendToUser(Rq->friid, data, len)) {
		// 好友不在线，给发送方返回失败
		PROT_CHAT_INFO_RS re;
		re.result = CHAT_RES_FALT;
		re.friid = Rq->userid;
		re.userid = Rq->friid;
		m_INetMed->sendData((char*)&re, sizeof(re), from);
	}
}

// 加好友请求
void Kernel::deal_AddFriRq(char* data, int len, unsigned long from) {
	PROT_ADD_FRIEND_RQ* Rq = (PROT_ADD_FRIEND_RQ*)data;

	// 1. 根据好友昵称查询 id
	char sql[1024] = { 0 };
	sprintf_s(sql, "select id from t_user where nick = '%s';", Rq->frinick);
	std::list<std::string> listRes;
	if (!sqlSelect(sql, 1, listRes)) {
		return;
	}

	if (listRes.empty()) {
		// 2. 查询结果为空，说明没有这个人
		PROT_ADD_FRIEND_RS rs;
		rs.result = ADD_FRI_RESULT_NOEXIT;
		strcpy_s(rs.userNick, sizeof(rs.userNick), Rq->frinick);
		m_INetMed->sendData((char*)&rs, sizeof(rs), from);
	}
	else if (!sendToUser(std::stoi(listRes.front()), data, len)) {
		// 3. 好友不在线
		PROT_ADD_FRIEND_RS rs;
		rs.result = ADD_FRI_RESULT_OFF;
		strcpy_s(rs.userNick, sizeof(rs.userNick), Rq->frinick);
		m_INetMed->sendData((char*)&rs, sizeof(rs), from);
	}
}

// 加好友应答
void Kernel::deal_AddFriRs(char* data, int len, unsigned long from) {
	PROT_ADD_FRIEND_RS* Rs = (PROT_ADD_FRIEND_RS*)data;

	if (Rs->result == ADD_FRI_RESULT_ACCP) {
		// 好友同意，双方各插一条好友关系
		char sql[1024] = { 0 };
		sprintf_s(sql, "insert into t_friend values (%d,%d);", Rs->friid, Rs->userid);
		if (!m_sql.UpdateMySql(sql)) {
			std::cout << "更新数据库失败 sql:" << sql << std::endl;
			return;
		}
		sprintf_s(sql, "insert into t_friend values (%d,%d);", Rs->userid, Rs->friid);
		if (!m_sql.UpdateMySql(sql)) {
			std::cout << "更新数据库失败 sql:" << sql << std::endl;
			return;
		}
		// 刷新双方的好友列表
		getUandFInfor(Rs->friid);
	}
	// 把结果转发给好友
	sendToUser(Rs->friid, data, len);
}
