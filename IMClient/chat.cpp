#include "chat.h"
#include "ui_chat.h"
#include <QDateTime>
#include <QMessageBox>

Chat::Chat(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Chat)
{
    ui->setupUi(this);

    // 发送按钮 -> 发送消息
    connect(ui->pb_sent, &QPushButton::clicked, this, &Chat::slot_sendMessage);
    // 清空按钮 -> 清除聊天记录
    connect(ui->pushButton_5, &QPushButton::clicked, this, &Chat::slot_clearChat);
}

Chat::~Chat()
{
    delete ui;
}

void Chat::setChatInfo(QString friendNick)
{
    setWindowTitle(QString("与 %1 聊天中").arg(friendNick));
}

void Chat::slot_sendMessage()
{
    // 获取输入内容
    auto msg = ui->pte_sent->toPlainText().trimmed();

    // 空消息不发送
    if (msg.isEmpty()) {
        QMessageBox::information(this, "提示", "请输入文字");
        return;
    }

    // 时间戳 + 格式化消息
    auto timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    auto formattedMsg = QString("<font color='gray'> me [%1]:</font> <p><font size='4'>%2</font></p>").arg(timestamp, msg);
    ui->tb_chat->append(formattedMsg);

    // 发给 Kernel，由它转发服务器
    emit signal_sentMasToSer(msg);

    // 清空输入框
    ui->pte_sent->clear();
}

void Chat::slot_clearChat()
{
    ui->tb_chat->clear();
}

void Chat::setMsg(QString msg) {
    ui->tb_chat->append(msg);
}
