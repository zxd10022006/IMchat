#pragma once

class INet;

// 网络中介者抽象基类：对外统一收发接口，内部持有具体网络对象
class INetMed {
protected:
	INet* m_pINet;
public:
	// 构造函数
	INetMed() {}
	virtual ~INetMed() {}

	// 初始化网络
	virtual bool openNet() = 0;

	// 关闭网络
	virtual void closeNet() = 0;

	// 发送数据
	// data: 要发送的数据
	// len:  数据长度
	// to:   TCP 协议传目标 socket，UDP 协议传对方 IP
	virtual bool sendData(char* data, int len, unsigned long to) = 0;

	// 接收数据（回调函数，收到数据时被调用）
	virtual void transmitData(char* data, int len, unsigned long to) = 0;
};
