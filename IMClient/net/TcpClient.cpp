#include "TcpClient.h"
#include <iostream>
#include "../mediator/INetMed.h"

bool TcpClient::initNet() {
    WORD ver = MAKEWORD(2, 2);
    WSADATA wsadata;
    int ret = WSAStartup(ver, &wsadata);
    if (ret != 0) {
        std::cout << "WSAStartup error" << WSAGetLastError() << std::endl;
        return false;
    }

    m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_sock == INVALID_SOCKET) {
        std::cout << "TcpClient:: socket error" << std::endl;
        return false;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(duan_TCP);
    addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    if (connect(m_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cout << "TcpClient:: connect error " << WSAGetLastError() << std::endl;
        return false;
    }

    m_handle = (HANDLE)_beginthreadex(nullptr, 0, &recvThread_TCP_CLI, this, 0, nullptr);

    return true;
}

unsigned __stdcall TcpClient::recvThread_TCP_CLI(void* p) {
    ((TcpClient*)p)->recvData();
    return 1;
}

void TcpClient::closeNet() {
    m_bRunning = false;
    if (m_handle) {
        if (WAIT_TIMEOUT == WaitForSingleObject(m_handle, 5000)) {
            CloseHandle(m_handle);
        }
    }

    closesocket(m_sock);
}

bool TcpClient::sendData(char* data, int len, u_long to) {
    if (len <= 0 || !data) {
        std::cout << "paramater error" << std::endl;
        return false;
    }

    send(m_sock, (char*)&len, sizeof(int), 0);
    int num = send(m_sock, data, len, 0);
    if (num == SOCKET_ERROR) {
        std::cout << "TcpClient:: sendData error" << std::endl;
    }

    return true;
}

void TcpClient::recvData() {
    int nRecv = 0;
    int packLen = 0;

    while (m_bRunning) {
        // 先收 4 字节长度头，再按长度收完整包
        nRecv = recv(m_sock, (char*)&packLen, sizeof(int), 0);
        if (nRecv <= 0) {
            std::cout << "Tcpclient:: recv error1" << WSAGetLastError() << std::endl;
            break;
        }

        char* pack = new char[packLen];
        int len = packLen;
        int offset = 0;
        while (len > 0) {
            nRecv = recv(m_sock, pack + offset, len, 0);
            if (nRecv <= 0) {
                std::cout << "Tcpclient:: recv error2" << WSAGetLastError() << std::endl;
                break;
            }
            offset += nRecv;
            len -= nRecv;
        }
        if (nRecv <= 0) {
            delete[] pack;
            break;
        }
        m_pMed->transmitData(pack, packLen, m_sock);
    }
}
