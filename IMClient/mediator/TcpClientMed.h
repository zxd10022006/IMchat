#pragma once
#include "INetMed.h"

class TcpClientMed : public INetMed {
public:
    TcpClientMed();
    ~TcpClientMed();

    bool openNet() override;
    void closeNet() override;
    bool sendData(char* data, int len, unsigned long to) override;
    void transmitData(char* data, int len, unsigned long to) override;
};
