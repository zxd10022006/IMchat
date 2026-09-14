#pragma once
#include "INetMed.h"

// TCP 服务器中介者：内部持有 TcpServer，收到数据转发给 Kernel 处理
class TcpServerMed : public INetMed {
public:
	TcpServerMed();
	~TcpServerMed();

	// 初始化网络
	bool openNet();

	// 关闭网络
	void closeNet();

	// 发送数据
	bool sendData(char* data, int len, unsigned long to);

	// 接收数据（转发给 Kernel 处理）
	void transmitData(char* data, int len, unsigned long to);
};
