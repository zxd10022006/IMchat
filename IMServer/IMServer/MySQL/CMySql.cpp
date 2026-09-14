#include "CMySql.h"

CMySql::CMySql(void)
{
	/* 构造函数中初始化一个 MYSQL 对象，用来连接 mysql 服务器。
	传入 NULL 指针会自动分配一个 MYSQL 对象，
	自动分配的对象在调用 mysql_close() 时会释放。*/
	m_sock = new MYSQL;
	mysql_init(m_sock);
	mysql_set_character_set(m_sock, "utf8mb4"); // 与源码 UTF-8 编码保持一致
}

CMySql::~CMySql(void)
{
	if (m_sock) {
		delete m_sock;
		m_sock = nullptr;
	}
}

void CMySql::DisConnect()
{
	mysql_close(m_sock);
}

bool CMySql::ConnectMySql(const char* host, const char* user, const char* pass, const char* db, short nport)
{
	if (!mysql_real_connect(m_sock, host, user, pass, db, nport, nullptr, CLIENT_MULTI_STATEMENTS)) {
		std::cout << "连接数据库失败，失败原因：" << mysql_error(m_sock);
		return false;
	}
	return true;
}

bool CMySql::SelectMySql(const char* szSql, int nColumn, std::list<std::string>& lstStr)
{
	// mysql_query() 向 MySQL 发送并执行 SQL 语句
	if (mysql_query(m_sock, szSql)) {
		std::cout << "查询数据库失败，失败原因：" << mysql_error(m_sock);
		return false;
	}

	/* mysql_store_result() 用于成功返回数据的每个查询（SELECT、SHOW、DESCRIBE、EXPLAIN、CHECK TABLE 等）*/
	MYSQL_RES* results = mysql_store_result(m_sock);
	if (results == nullptr) {
		std::cout << "查询数据库失败，查询结果为空";
		return false;
	}

	// 逐行读取数据，每行按列展开后放入 lstStr
	MYSQL_ROW record = nullptr;
	while ((record = mysql_fetch_row(results))) {
		for (int i = 0; i < nColumn; i++) {
			lstStr.push_back(record[i] ? record[i] : "");
		}
	}
	mysql_free_result(results);
	return true;
}

bool CMySql::UpdateMySql(const char* szSql)
{
	if (!szSql) {
		return false;
	}
	if (mysql_query(m_sock, szSql)) {
		std::cout << "更新数据库失败，失败原因：" << mysql_error(m_sock);
		return false;
	}
	return true;
}
