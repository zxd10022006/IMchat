#include "kernel.h"
#include <QMessageBox>
#include <QDebug>
#include <string>
#include <cstring>

Kernel::Kernel(QObject *par)
    : QObject(par), m_plogin(new Login), m_mainwin(new mainWidgets)
{
    // 启动时显示登录界面
    m_plogin->show();

    // ========== 信号-槽连接 ==========
    connect(m_plogin, &Login::signal_sendRegister, this, &Kernel::slots_Register);
    connect(m_plogin, &Login::signal_delete, this, &Kernel::slots_delete);
    connect(m_plogin, &Login::signal_sendLogin, this, &Kernel::slots_Login);
    connect(m_mainwin, &mainWidgets::signal_sentaddFriToKernel, this, &Kernel::slot_addFri);
    connect(m_mainwin, &mainWidgets::signals_close, this, &Kernel::slots_close);

    // ========== 注册协议处理函数到映射表 ==========
    m_dealFuncArr[DEF_PROT_REGISTER_RS - DEF_PROT_BASE] = &Kernel::deal_registerInfo;
    m_dealFuncArr[DEF_PROT_LOGIN_RS - DEF_PROT_BASE] = &Kernel::deal_loginInfo;
    m_dealFuncArr[DEF_PROT_FRIEND_INFO - DEF_PROT_BASE] = &Kernel::deal_friendInfo;
    m_dealFuncArr[DEF_PROT_CHAT_INFO_RS - DEF_PROT_BASE] = &Kernel::deal_chatInfo;
    m_dealFuncArr[DEF_PROT_CHAT_INFO_RQ - DEF_PROT_BASE] = &Kernel::deal_chatInfoRq;
    m_dealFuncArr[DEF_PROT_ADD_FRIEND_RS - DEF_PROT_BASE] = &Kernel::deal_addFriendRs;
    m_dealFuncArr[DEF_PROT_FRIEND_OFFLINE - DEF_PROT_BASE] = &Kernel::deal_setFriOff;
    m_dealFuncArr[DEF_PROT_ADD_FRIEND_RQ - DEF_PROT_BASE] = &Kernel::deal_addFRi;

    // ========== 模拟计时器：定时将好友 7 置为离线 ==========
    m_frOFFlineTimer.setInterval(8000);
    connect(&m_frOFFlineTimer, &QTimer::timeout, this, &Kernel::slots_frOFFlineTimer);
    m_frOFFlineTimer.start();

    m_TcpClientMed = new TcpClientMed;
    connect(m_TcpClientMed, &TcpClientMed::sighals_recieveServer, this, &Kernel::slot_recieveServer);
}

Kernel* Kernel::s_instance = nullptr;

Kernel* Kernel::instance()
{
    if (!s_instance) {
        s_instance = new Kernel;  // 懒汉式，首次调用时创建
    }
    return s_instance;
}

void Kernel::openTcpNet()
{
    if (m_TcpClientMed) {
        m_TcpClientMed->openNet();
    }
}

// 服务器响应统一入口：解析协议号 -> 查表 -> 调用处理函数 -> 释放 pbuf
void Kernel::slot_recieveServer(char *pbuf, int len, unsigned long ul) {
    qDebug() << "Kernel::slot_recieveServer";

    protType pt = *(protType*)pbuf;

    if (pt >= DEF_PROT_BASE && pt <= (DEF_PROT_BASE + 100)) {
        DealFun dealfun = m_dealFuncArr[pt - DEF_PROT_BASE];
        if (dealfun) {
            (this->*dealfun)(pbuf, len, ul);
        } else {
            QMessageBox::information(m_plogin, "提示", "处理函数为空！！！" + QString::number(pt));
        }
    } else {
        QMessageBox::information(m_plogin, "提示", "协议类型解析错误！！！" + QString::number(pt));
    }

    if (pbuf) {
        delete[] pbuf;
        pbuf = nullptr;
    }
}

// 登录请求：封装协议包发给服务器
void Kernel::slots_Login(QString tel, QString passwd) {
    PROT_LOGIN_RQ loginRQ;

    std::string telstr = tel.toStdString();
    std::string passstr = passwd.toStdString();
    strcpy_s(loginRQ.tel, 15, telstr.c_str());
    strcpy_s(loginRQ.passwd, 20, passstr.c_str());

    m_TcpClientMed->sendData((char*)&loginRQ, sizeof(loginRQ), 6);
}

// 加好友请求：封装协议包发给服务器
void Kernel::slot_addFri(QString FriNick) {
    qDebug() << "Kernel::slot_addFri 请求添加好友:" << FriNick;

    PROT_ADD_FRIEND_RQ addFriRq;
    addFriRq.userid = m_mainwin->getUserid();

    u_to_g(m_mainwin->getNick(), addFriRq.usernick, sizeof(addFriRq.usernick));
    u_to_g(FriNick, addFriRq.frinick, sizeof(addFriRq.frinick));

    m_TcpClientMed->sendData((char*)&addFriRq, sizeof(addFriRq), 6);
}

// 注册请求：封装协议包发给服务器
void Kernel::slots_Register(QString nick, QString tel, QString passwd) {
    PROT_REGISTER_RQ Register;

    std::string telstr = tel.toStdString();
    std::string passwdstr = passwd.toStdString();

    u_to_g(nick, Register.nick, 30);
    strcpy_s(Register.tel, 15, telstr.c_str());
    strcpy_s(Register.passwd, 20, passwdstr.c_str());

    m_TcpClientMed->sendData((char*)&Register, sizeof(Register), 6);
}

// 登录响应
void Kernel::deal_loginInfo(char *pbuf, int len, unsigned long ul)
{
    qDebug() << "Kernel::deal_loginInfo called";

    PROT_LOGIN_RS* ploginRs = (PROT_LOGIN_RS*)pbuf;

    if (ploginRs->result == LOGIN_SUCC) {
        // 登录成功：切到主界面，保存当前用户 ID
        m_plogin->hide();
        m_mainwin->show();

        m_loginUserId = ploginRs->userid;
        m_mainwin->setUserid(ploginRs->userid);

        // 处理登录响应到达前暂存的好友信息
        for (const auto& info : m_pendingFriendInfo) {
            char* buf = new char[sizeof(PROT_FRIEND_INFO)];
            memcpy(buf, &info, sizeof(PROT_FRIEND_INFO));
            deal_friendInfo(buf, sizeof(PROT_FRIEND_INFO), 0);
        }
        m_pendingFriendInfo.clear();

    } else if (ploginRs->result == LOGIN_NOEX) {
        QMessageBox::information(m_plogin, "提示", "用户不存在 请先注册！！！");
    } else if (ploginRs->result == LOGIN_PASSERR) {
        QMessageBox::information(m_plogin, "提示", "密码错误！！！");
    }
}

// 注册响应
void Kernel::deal_registerInfo(char *pbuf, int len, unsigned long ul)
{
    qDebug() << "Kernel::deal_registerInfo called";

    PROT_REGISTER_RS* pRegisterRs = (PROT_REGISTER_RS*)pbuf;

    if (pRegisterRs->result == REGIS_SUCC) {
        QMessageBox::information(m_plogin, "提示", "注册成功，欢迎使用！");
    } else if (pRegisterRs->result == REGIS_NICK_EXISTS) {
        QMessageBox::information(m_plogin, "提示", "注册失败,昵称被使用！！！");
    } else if (pRegisterRs->result == REGIS_TEL_EXISTS) {
        QMessageBox::information(m_plogin, "提示", "注册失败,电话号码被使用！！！");
    }
}

// 好友信息：自己则更新个人信息，好友则更新/添加列表项
void Kernel::deal_friendInfo(char *pbuf, int len, unsigned long ul) {
    qDebug() << "Kernel::deal_friendInfo";

    PROT_FRIEND_INFO *pfri = (PROT_FRIEND_INFO*) pbuf;

    int imgid = pfri->imgid;
    QString nick = g_to_u(pfri->nick);
    QString feeling = g_to_u(pfri->feeling);

    // 登录响应还没到，暂存起来等 deal_loginInfo 处理
    if (m_loginUserId == 0) {
        m_pendingFriendInfo.append(*pfri);
        return;
    }

    if (pfri->userid == m_loginUserId) {
        // 自己的信息 -> 更新主界面左上角个人信息
        m_mainwin->setmyInfo(imgid, nick, feeling);
    } else if (m_mainwin->m_friendMAp.count(pfri->userid)) {
        // 已有好友 -> 更新信息
        m_mainwin->m_friendMAp[pfri->userid]->showFriInfo(pfri->imgid, pfri->status, nick, feeling);
    } else {
        // 新好友 -> 添加列表项
        m_mainwin->addFriendItem(pfri->userid, pfri->imgid, pfri->status, nick, feeling);
    }
}

// 聊天响应
void Kernel::deal_chatInfo(char *pbuf, int len, unsigned long ul) {
    qDebug() << "deal_chatInfo!!!";

    PROT_CHAT_INFO_RS *pchatINfoRs = (PROT_CHAT_INFO_RS*) pbuf;
    if (pchatINfoRs->result == CHAT_RES_SUCC) {
        m_mainwin->setFriendMsg(pchatINfoRs->userid, "收到了");
    } else if (pchatINfoRs->result == CHAT_RES_FALT) {
        m_mainwin->setFriendMsg(pchatINfoRs->userid, "离线了");
    }
}

// 发送聊天消息给好友
void Kernel::slot_sendMasToKer2(int friid, QString msg) {
    qDebug() << "slot_sendMasToKer" << friid << "  " << msg;

    PROT_CHAT_INFO_RQ chatInfoRQ;
    chatInfoRQ.userid = m_mainwin->getUserid();
    chatInfoRQ.friid = friid;

    u_to_g(msg, chatInfoRQ.msg, MAX_MSG);

    m_TcpClientMed->sendData((char*)&chatInfoRQ, sizeof(chatInfoRQ), 6);
}

// 收到好友发来的聊天请求
void Kernel::deal_chatInfoRq(char *pbuf, int len, unsigned long ul) {
    qDebug() << "void Kernel::deal_chatInfoRq";
    PROT_CHAT_INFO_RQ* pchatInfoRQ = (PROT_CHAT_INFO_RQ*) pbuf;

    QString msg = g_to_u(pchatInfoRQ->msg);
    m_mainwin->setFriendMsg(pchatInfoRQ->userid, QString("<p><font size='5' color ='black'>") + msg + "</font></p>");
}

// 添加好友响应
void Kernel::deal_addFriendRs(char *pbuf, int len, unsigned long ul) {
    qDebug() << "Kernel::deal_addFriendRs";

    PROT_ADD_FRIEND_RS *pAddRs = (PROT_ADD_FRIEND_RS*)pbuf;
    QString friNick = g_to_u(pAddRs->userNick);
    QString msg;
    switch (pAddRs->result) {
    case ADD_FRI_RESULT_ACCP:
        msg = QString("添加 %1 成功！").arg(friNick);
        break;
    case ADD_FRI_RESULT_REFU:
        msg = QString("%1 拒绝了你的好友请求").arg(friNick);
        break;
    case ADD_FRI_RESULT_OFF:
        msg = QString("%1 不在线").arg(friNick);
        break;
    case ADD_FRI_RESULT_NOEXIT:
        msg = QString("用户 %1 不存在").arg(friNick);
        break;
    }

    QMessageBox::information(m_mainwin, "添加好友结果", msg);
}

// 好友离线通知
void Kernel::deal_setFriOff(char *pbuf, int len, unsigned long ul) {
    PROT_FRIEND_OFFLINE *pfrioff = (PROT_FRIEND_OFFLINE *)pbuf;
    m_mainwin->setFriOff(pfrioff->userid);
}

// 收到别人的加好友请求，弹窗询问
void Kernel::deal_addFRi(char *pbuf, int len, unsigned long ul) {
    PROT_ADD_FRIEND_RQ* paddFri = (PROT_ADD_FRIEND_RQ*)pbuf;
    QMessageBox::StandardButton but = QMessageBox::information(m_mainwin, "提示",
                                 g_to_u(paddFri->usernick) + "请求添加好友",
                                 QMessageBox::Yes | QMessageBox::No);
    PROT_ADD_FRIEND_RS addFriRs;
    addFriRs.userid = m_mainwin->getUserid();
    addFriRs.friid = paddFri->userid;
    u_to_g(m_mainwin->getNick(), addFriRs.userNick, sizeof(addFriRs.userNick));
    strcpy_s(addFriRs.friNick, 30, paddFri->usernick);

    if (but == QMessageBox::Yes) {
        addFriRs.result = ADD_FRI_RESULT_ACCP;
    } else {
        addFriRs.result = ADD_FRI_RESULT_REFU;
    }

    m_TcpClientMed->sendData((char*)&addFriRs, sizeof(addFriRs), 6);
}

// gb2312/utf-8 --> QString
QString Kernel::g_to_u(char* src) {
    if (!src) return {};

    QString utf8Result = QString::fromUtf8(src);
    // UTF-8 解码有效（不含 U+FFFD 替换字符）则直接返回
    if (!utf8Result.contains(QChar::ReplacementCharacter) && !utf8Result.isEmpty())
        return utf8Result;

    // 否则按本地编码（GBK）解码
    return QString::fromLocal8Bit(src);
}

// QString --> gb2312
void Kernel::u_to_g(QString src, char* dst, int len) {
    if (!dst || len <= 0) return;
    QByteArray ba = src.toLocal8Bit();
    int copyLen = qMin(ba.size(), len - 1);
    memcpy(dst, ba.constData(), copyLen);
    dst[copyLen] = '\0';
}

// 定时将好友 7 置为离线（模拟好友下线）
void Kernel::slots_frOFFlineTimer() {
    PROT_FRIEND_OFFLINE friOFF;
    friOFF.userid = 7;

    deal_setFriOff((char*)&friOFF, sizeof(friOFF), 0);
}

// 关闭窗口：通知服务器下线并回收资源
void Kernel::slots_close() {
    qDebug() << "slots_close()";
    PROT_FRIEND_OFFLINE myOff;
    myOff.userid = m_mainwin->getUserid();

    m_TcpClientMed->sendData((char*)&myOff, sizeof(myOff), 6);

    slots_delete();
}

// 回收空间
void Kernel::slots_delete() {
    qDebug() << "Kernel::slots_delete()";

    if (m_plogin) {
        delete m_plogin;
        m_plogin = nullptr;
    }
    if (m_mainwin) {
        delete m_mainwin;
        m_mainwin = nullptr;
    }
    if (m_TcpClientMed) {
        m_TcpClientMed->closeNet();
        delete m_TcpClientMed;
        m_TcpClientMed = nullptr;
    }
}
