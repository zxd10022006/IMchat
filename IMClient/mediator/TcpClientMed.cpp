#include "TcpClientMed.h"
#include "../net/TcpClient.h"

TcpClientMed::TcpClientMed() { m_pINet = new TcpClient(this); }
TcpClientMed::~TcpClientMed() { delete m_pINet; }

bool TcpClientMed::openNet() {
    return m_pINet && m_pINet->initNet();
}

void TcpClientMed::closeNet() {
    if (m_pINet) {
        m_pINet->closeNet();
    }
}

bool TcpClientMed::sendData(char* data, int len, u_long to) {
    return m_pINet && m_pINet->sendData(data, len, to);
}

// 收包线程收到数据后发信号给 Kernel
void TcpClientMed::transmitData(char* data, int len, u_long to) {
    emit sighals_recieveServer(data, len, to);
}
