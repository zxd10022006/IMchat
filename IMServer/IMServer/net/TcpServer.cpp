#include "TcpServer.h"
#include "../mediator/INetMed.h"
#include <memory>

// 传给客户端接收线程的参数：服务器指针 + 客户端 socket
// （直接传参，避免线程启动后按线程 id 查表产生的竞态）
struct ClientParam {
	TcpServer* pThis;
	SOCKET sock;
};

// 初始化网络：加载库 → 建套接字 → 绑定 ip 和端口 → 监听 → 开启等待连接的线程
bool TcpServer::initNet() {
	if (!initWinsock()) {
		return false;
	}

	m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_sock == INVALID_SOCKET) {
		std::cout << __func__ << ": creat socket error " << WSAGetLastError() << std::endl;
		return false;
	}

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(duan_TCP);
	addr.sin_addr.S_un.S_addr = ADDR_ANY;
	if (bind(m_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		std::cout << __func__ << ": bind error " << WSAGetLastError() << std::endl;
		return false;
	}

	if (listen(m_sock, 100) == SOCKET_ERROR) {
		std::cout << __func__ << ": listen error " << WSAGetLastError() << std::endl;
		return false;
	}

	m_handle = (HANDLE)_beginthreadex(nullptr, 0, &acceptThread, this, 0, nullptr);

	return true;
}

// 等待客户端连接的线程函数：每来一个连接就开一条接收线程
unsigned __stdcall TcpServer::acceptThread(void* p) {
	TcpServer* pThis = (TcpServer*)p;
	while (pThis->m_bRunning) {
		sockaddr_in cliaddr = {};
		int sizeCli = sizeof(cliaddr);

		SOCKET clisock = accept(pThis->m_sock, (sockaddr*)&cliaddr, &sizeCli);
		if (clisock == INVALID_SOCKET) {
			if (pThis->m_bRunning) {
				std::cout << __func__ << ": accept error " << WSAGetLastError() << std::endl;
			}
			continue;
		}

		// 把服务器指针和 socket 打包传给接收线程
		ClientParam* param = new ClientParam{ pThis, clisock };
		unsigned int threadid = 0;
		HANDLE handle = (HANDLE)_beginthreadex(nullptr, 0, &recvThread, param, 0, &threadid);

		// 记录线程 id 和 socket 的对应关系（关闭服务器时用）
		pThis->m_mapThreadToSocket[threadid] = clisock;
		pThis->m_listHandle.push_back(handle);
	}
	return 1;
}

// 单个客户端连接的接收线程入口
unsigned __stdcall TcpServer::recvThread(void* p) {
	ClientParam* param = (ClientParam*)p;
	SOCKET sock = param->sock;
	TcpServer* pThis = param->pThis;
	delete param;
	pThis->recvData(sock);
	return 1;
}

// 关闭网络
void TcpServer::closeNet() {
	// 1. 让接收循环退出
	m_bRunning = false;

	// 2. 关闭监听套接字，让 accept 线程解除阻塞退出
	closesocket(m_sock);

	// 3. 关闭所有客户端 socket，让接收线程解除阻塞退出
	for (auto& kv : m_mapThreadToSocket) {
		closesocket(kv.second);
	}
	m_mapThreadToSocket.clear();

	// 4. 等待线程结束并关闭句柄
	closeThread(m_handle);
	for (HANDLE h : m_listHandle) {
		closeThread(h);
	}
	m_listHandle.clear();

	// 5. 卸载套接字库
	WSACleanup();
}

// 发送数据（to 是目标客户端的 socket）
bool TcpServer::sendData(char* data, int len, u_long to) {
	return sendAll((SOCKET)to, data, len);
}

// 接收数据（每个客户端连接对应一条线程）
void TcpServer::recvData(SOCKET sock) {
	while (m_bRunning) {
		// 先收 4 字节的包长度
		int packLen = 0;
		if (recvAll(sock, (char*)&packLen, sizeof(int)) <= 0) {
			std::cout << __func__ << ": recv head error " << WSAGetLastError() << std::endl;
			break;
		}
		// 再按包长度收数据体
		std::unique_ptr<char[]> pack(new char[packLen]);
		if (recvAll(sock, pack.get(), packLen) <= 0) {
			std::cout << __func__ << ": recv body error " << WSAGetLastError() << std::endl;
			break;
		}
		// 把数据交给中介者（Kernel）处理
		m_pMed->transmitData(pack.get(), packLen, (u_long)sock);
	}
}
