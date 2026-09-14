#ifndef CHAT_H
#define CHAT_H

#include <QWidget>

namespace Ui {
class Chat;
}

// 一对一聊天窗口：消息显示区 + 输入区 + 发送/清空按钮
class Chat : public QWidget
{
    Q_OBJECT

public:
    explicit Chat(QWidget *parent = nullptr);
    ~Chat();

    // 设置聊天对象（标题显示"与 XXX 聊天中"）
    void setChatInfo(QString friendNick);

    // 追加一条消息到显示区
    void setMsg(QString msg);

signals:
    // 发送消息给服务器
    void signal_sentMasToSer(QString);

private slots:
    // 发送按钮：把输入框内容追加到消息区并清空
    void slot_sendMessage();
    // 清空按钮：清除消息区
    void slot_clearChat();

private:
    Ui::Chat *ui;
};

#endif // CHAT_H
