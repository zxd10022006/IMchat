#include "TcpServerMed.h"
#include "../net/TcpServer.h"
#include "../Kernel.h"

TcpServerMed::TcpServerMed() { m_pINet = new TcpServer(this); }
TcpServerMed::~TcpServerMed() { if (m_pINet) delete m_pINet; m_pINet = nullptr; }

// 初始化网络
bool TcpServerMed::openNet() {
	if (m_pINet) {
		return m_pINet->initNet();
	}
	return false;
}

// 关闭网络
void TcpServerMed::closeNet() {
	if (m_pINet) {
		m_pINet->closeNet();
	}
}

// 发送数据
bool TcpServerMed::sendData(char* data, int len, u_long to) {
	if (m_pINet) {
		return m_pINet->sendData(data, len, to);
	}
	return false;
}

// 接收数据（转发给 Kernel 处理）
void TcpServerMed::transmitData(char* data, int len, u_long to) {
	Kernel::m_kernel->deal_Data(data, len, to);
}
