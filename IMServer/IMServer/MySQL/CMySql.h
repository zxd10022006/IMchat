#pragma once

#include <mysql.h>
#include <string>
#include <iostream>
#include <list>

#pragma comment(lib, "libmysql.lib")

// MySQL 数据库操作类：连接、查询、更新、断开连接
class CMySql
{
public:
	CMySql(void);
	~CMySql(void);

	// 连接数据库（ip, 用户名, 密码, 数据库名, 端口）
	bool ConnectMySql(const char* host, const char* user, const char* pass, const char* db, short nport = 3306);
	// 断开连接
	void DisConnect();
	// 查询：返回值表示 sql 语句是否执行成功；查询结果（按行按列展开）写入 lstStr
	bool SelectMySql(const char* szSql, int nColumn, std::list<std::string>& lstStr);
	// 更新（删除、插入、修改）：返回值表示 sql 语句是否执行成功
	bool UpdateMySql(const char* szSql);

private:
	MYSQL* m_sock;
};
