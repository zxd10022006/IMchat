#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <process.h>
#include "../def/def.h"

class INetMed;

// 网络层抽象接口：TcpClient 等具体网络类实现收发逻辑
class INet {
protected:
    SOCKET m_sock;
    HANDLE m_handle;
    bool m_bRunning;
    INetMed* m_pMed;
public:
    INet() : m_sock(INVALID_SOCKET), m_handle(nullptr), m_bRunning(true), m_pMed(nullptr) {}
    virtual ~INet() = default;

    // 初始化网络
    virtual bool initNet() = 0;

    // 关闭网络
    virtual void closeNet() = 0;

    // 发送数据 data:数据 len:长度 to:目标socket
    virtual bool sendData(char* data, int len, u_long to) = 0;

    // 接收数据（内部线程调用）
    virtual void recvData() = 0;
};
