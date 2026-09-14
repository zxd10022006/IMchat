#ifndef FRIENDITEM_H
#define FRIENDITEM_H

#include <QWidget>
#include "def/def.h"
#include "chat.h"

namespace Ui {
class friendItem;
}

// 好友列表项：头像按钮 + 昵称 + 签名；点击头像打开聊天窗口
class friendItem : public QWidget
{
    Q_OBJECT

public:
    explicit friendItem(QWidget *parent = nullptr);
    ~friendItem();

    void setFriId(int id) { friId = id; }
    // 显示好友信息（头像、在线状态、昵称、签名）
    void showFriInfo(int imgid, int status, QString nick, QString feeling);
    // 给聊天窗口追加一条消息
    void setChatMas(QString msg);
    // 置为离线显示（灰色头像 + 灰色文字）
    void setOFFline();

    QString getNick() { return nick; }

    // 更新在线/离线状态显示
    void updateStatus(int newStatus);

public slots:
    // 点击头像打开聊天窗口
    void slot_showChat();
    // Chat 发消息 -> 转发给 Kernel
    void slot_sendMasToKer(QString mas);

signals:
    void slot_sendMasToKer1(int friid, QString msg);

private:
    Ui::friendItem *ui;
    int friId;
    QString nick;
    int imgid;

    Chat *m_chat;
};

#endif // FRIENDITEM_H
