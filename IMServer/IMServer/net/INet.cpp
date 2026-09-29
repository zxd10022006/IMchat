#include "INet.h"

INet::INet() {
	m_handle = nullptr;
	m_sock = INVALID_SOCKET;
	m_bRunning = true;
	m_pMed = nullptr;
}

// 加载套接字库
bool INet::initWinsock() {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cout << __func__ << ": WSAStartup error " << WSAGetLastError() << std::endl;
		return false;
	}
	return true;
}

// 先发 4 字节长度头，再发数据体
bool INet::sendAll(SOCKET s, const char* data, int len) {
	if (s == INVALID_SOCKET || !data || len <= 0) {
		std::cout << __func__ << ": paramater error" << std::endl;
		return false;
	}
	//发送4字节长度头
		const char* pHead = (const char*)&len;
		int headRemain = sizeof(int);
		int offset = 0;
		while (headRemain>0) {
			int n = send(s, pHead + offset, headRemain, 0);
			if (n == SOCKET_ERROR) {
				std::cout << __func__ << ":send head error" << WSAGetLastError() << std::endl;
				return false;
			}
			headRemain -= n;
			offset += n;

		}

		//发送数据
		int dataRemain = len;
		offset = 0;
		while (dataRemain>0) {
			int n = send(s, data+offset, dataRemain, 0);
			if (n == SOCKET_ERROR) {
				std::cout << __func__ << ":send data error" << WSAGetLastError() << std::endl;
				return false;
			}
			dataRemain -= n;
			offset += n;
		}
	return true;
}

// 循环接收直到收满 len 字节；返回实际收到的字节数，<=0 表示连接断开或出错
int INet::recvAll(SOCKET s, char* buf, int len) {
	int offset = 0;
	while (offset < len) {
		int n = recv(s, buf + offset, len - offset, 0);
		if (n <= 0) {
			if (n < 0) {
				std::cout << __func__ << ": recv error " << WSAGetLastError() << std::endl;
			}
			return n;
		}
		offset += n;
	}
	return offset;
}

// 等待线程退出并关闭句柄；超时则强制结束线程
void INet::closeThread(HANDLE& h) {
	if (h) {
		if (WAIT_TIMEOUT == WaitForSingleObject(h, 5000)) {
			// 等待超时，强制杀死线程，防止线程句柄泄漏
			TerminateThread(h, -1);
		}
		CloseHandle(h);
		h = nullptr;
	}
}
