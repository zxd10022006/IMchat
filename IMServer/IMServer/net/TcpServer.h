#pragma once
#include "INet.h"
#include <map>
#include <vector>

// TCP 服务器：加载库 → 建套接字 → 绑定监听 → 每来一个客户端连接就开一条接收线程
class TcpServer : public INet {
	// 线程 id → 客户端 socket（关闭时遍历关掉所有客户端连接）
	std::map<unsigned int, SOCKET> m_mapThreadToSocket;
	// 所有客户端接收线程的句柄（关闭时等待线程退出）
	std::vector<HANDLE> m_listHandle;
public:
	TcpServer(INetMed* p) { m_pMed = p; }
	~TcpServer() {}

	// 初始化网络
	bool initNet();

	// 关闭网络
	void closeNet();

	// 发送数据（to 是目标客户端的 socket）
	bool sendData(char* data, int len, u_long to);

	// 接收数据（每个客户端连接对应一条线程）
	void recvData(SOCKET sock);

	// 等待客户端连接的线程函数
	static unsigned __stdcall acceptThread(void* p);

	// 单个客户端连接的接收线程函数
	static unsigned __stdcall recvThread(void* p);
};
