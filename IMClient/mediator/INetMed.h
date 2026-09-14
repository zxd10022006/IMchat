#pragma once
#include <QObject>

class INet;

// 网络中介层：网络层(收包线程)与界面层之间通过信号-槽传递数据
class INetMed : public QObject {
    Q_OBJECT
protected:
    INet* m_pINet;
public:
    INetMed() = default;
    virtual ~INetMed() = default;

    // 初始化网络
    virtual bool openNet() = 0;

    // 关闭网络
    virtual void closeNet() = 0;

    // 发送数据 data:数据 len:长度 to:目标socket
    virtual bool sendData(char* data, int len, unsigned long to) = 0;

    // 网络层收到数据后透传（由收包线程调用）
    virtual void transmitData(char* data, int len, unsigned long to) = 0;

signals:
    void sighals_recieveServer(char* pbuf, int len, unsigned long ul);
};
