#pragma once
#include "INet.h"

class TcpClient : public INet {
public:
    TcpClient(INetMed* p) { m_pMed = p; }
    ~TcpClient() = default;

    bool initNet() override;
    void closeNet() override;
    bool sendData(char* data, int len, u_long to) override;
    void recvData() override;

    // 接收线程入口
    static unsigned __stdcall recvThread_TCP_CLI(void* p);
};
