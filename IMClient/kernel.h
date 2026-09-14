#ifndef KERNEL_H
#define KERNEL_H

#include <QObject>
#include <QTimer>
#include <QList>
#include "login.h"
#include "def/def.h"
#include "mainwidgets.h"
#include "mediator/TcpClientMed.h"

// Kernel 是整个客户端的中枢（Controller）：
// Login/主界面操作 -> Kernel 槽函数 -> 封装协议包 -> TcpClientMed 发给服务器
// 服务器响应 -> sighals_recieveServer -> slot_recieveServer -> 按协议号查函数指针表分发 -> 对应 deal_* 函数
class Kernel : public QObject
{
    Q_OBJECT
public:
    // 单例入口
    static Kernel* instance();

    INetMed* m_TcpClientMed;

    /// 登录/注册界面
    Login* m_plogin;
    /// 主聊天界面
    mainWidgets* m_mainwin;

    void openTcpNet();

    // 协议处理函数指针类型，参数统一为 (pbuf, len, ul)
    using DealFun = void (Kernel::*)(char*, int, unsigned long);

    // 协议处理函数映射表，下标 = 协议号 - DEF_PROT_BASE，实现 O(1) 分发
    DealFun m_dealFuncArr[50];

public slots:
    // 服务器响应统一入口：按协议号查表分发，最后释放 pbuf
    void slot_recieveServer(char *pbuf, int len, unsigned long ul);

    // 登录/注册/加好友请求：封装协议包发给服务器
    void slots_Login(QString, QString);
    void slots_Register(QString, QString, QString);
    void slot_addFri(QString);

    // 发送聊天消息给好友
    void slot_sendMasToKer2(int friid, QString msg);

    // 定时模拟好友 7 离线
    void slots_frOFFlineTimer();

    // 关闭窗口：通知服务器下线并回收资源
    void slots_close();
    void slots_delete();

public:
    // ========== 协议处理函数（由 slot_recieveServer 分发调用） ==========
    /// 登录响应：成功则切主界面，否则弹窗提示
    void deal_loginInfo(char *pbuf, int len, unsigned long ul);
    /// 注册响应：弹窗提示结果
    void deal_registerInfo(char *pbuf, int len, unsigned long ul);
    /// 好友信息：自己则更新个人信息，好友则更新/添加列表项
    void deal_friendInfo(char *pbuf, int len, unsigned long ul);
    /// 聊天响应
    void deal_chatInfo(char *pbuf, int len, unsigned long ul);
    /// 收到好友发来的聊天请求
    void deal_chatInfoRq(char *pbuf, int len, unsigned long ul);
    /// 添加好友响应
    void deal_addFriendRs(char *pbuf, int len, unsigned long ul);
    /// 好友离线通知
    void deal_setFriOff(char *pbuf, int len, unsigned long ul);
    /// 收到别人的加好友请求，弹窗询问
    void deal_addFRi(char *pbuf, int len, unsigned long ul);

    // gb2312/utf-8 --> QString
    QString g_to_u(char* src);
    // QString --> gb2312
    void u_to_g(QString src, char* dst, int len);

private:
    explicit Kernel(QObject *parent = nullptr);
    Kernel(const Kernel&) = delete;
    Kernel& operator=(const Kernel&) = delete;

    static Kernel* s_instance;

    QTimer m_frOFFlineTimer;

    int m_loginUserId = 0;
    // 好友信息比登录响应先到时暂存在这里
    QList<PROT_FRIEND_INFO> m_pendingFriendInfo;
};

#endif // KERNEL_H
