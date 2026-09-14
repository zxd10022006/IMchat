#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma once
#include <WinSock2.h>
#include <iostream>
#include <process.h>
#include "def.h"
#pragma comment(lib, "Ws2_32.lib")

class INetMed;

// 网络抽象基类：
// 加载/卸载套接字库、收发公共逻辑（先发长度头、循环收满）沉淀在这里，供派生类复用
class INet {
protected:
	SOCKET m_sock;
	HANDLE m_handle;      // 监听/接收线程句柄
	bool m_bRunning;
	INetMed* m_pMed;

	// 加载套接字库（WSAStartup 样板）
	bool initWinsock();
	// 先发 4 字节长度头，再发数据体
	bool sendAll(SOCKET s, const char* data, int len);
	// 循环接收直到收满 len 字节；返回实际收到的字节数，<=0 表示连接断开或出错
	int recvAll(SOCKET s, char* buf, int len);
	// 等待线程退出并关闭句柄；超时则强制结束线程
	void closeThread(HANDLE& h);
public:
	// 构造函数
	INet();
	virtual ~INet() {}

	// 初始化网络
	virtual bool initNet() = 0;

	// 关闭网络
	virtual void closeNet() = 0;

	// 发送数据
	// data: 要发送的数据
	// len:  数据长度
	// to:   TCP 协议传目标 socket，UDP 协议传对方 IP
	virtual bool sendData(char* data, int len, u_long to) = 0;
};
