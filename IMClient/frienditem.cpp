#include "frienditem.h"
#include "ui_frienditem.h"
#include <QBitmap>
#include <QDateTime>
#include "kernel.h"

friendItem::friendItem(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::friendItem), m_chat(new Chat)
{
    ui->setupUi(this);

    // 点击头像按钮 -> 打开与该好友的聊天窗口
    connect(ui->pb_img, &QPushButton::clicked, this, &friendItem::slot_showChat);
    // Chat 发送消息 -> friendItem 转发给 Kernel
    connect(m_chat, &Chat::signal_sentMasToSer, this, &friendItem::slot_sendMasToKer);
    connect(this, &friendItem::slot_sendMasToKer1,
            Kernel::instance(), &Kernel::slot_sendMasToKer2);
}

friendItem::~friendItem()
{
    delete ui;
    delete m_chat;
}

// 显示好友信息（头像、在线状态、昵称、签名）
void friendItem::showFriInfo(int imgid, int status, QString nick, QString feeling)
{
    // 设置头像
    QIcon icon(QString(":/tx/%1.png").arg(imgid));
    ui->pb_img->setIcon(icon);
    ui->pb_img->setIconSize(QSize(60, 60));

    if (status == USER_OFFLINE) {
        // 离线用户：单色位图灰色效果
        QBitmap bit(QString(":/tx/%1.png").arg(imgid));
        ui->pb_img->setIcon(bit);
        ui->pb_img->setIconSize(QSize(60, 60));
    }

    // 设置昵称和签名
    ui->l_nick->setText(nick);
    ui->label_2->setText(feeling);

    this->imgid = imgid;
    this->nick = nick;
}

// 点击头像打开聊天窗口
void friendItem::slot_showChat()
{
    m_chat->setChatInfo(nick);
    m_chat->show();
}

void friendItem::slot_sendMasToKer(QString mas)
{
    emit slot_sendMasToKer1(friId, mas);
}

// 给聊天窗口追加一条消息
void friendItem::setChatMas(QString msg)
{
    auto timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    auto showmsg = QString("<font> %1[%2]:%3</font>").arg(friId).arg(timestamp).arg(msg);
    m_chat->setMsg(showmsg);
}

void friendItem::setOFFline()
{
    updateStatus(USER_OFFLINE);
}

// 在线：彩色头像 + 正常文字；离线：灰度头像 + 灰色文字
void friendItem::updateStatus(int newStatus)
{
    // 用保存的 imgid 加载头像（不能用 friId，friId 是用户 ID 不是头像编号）
    QPixmap pix(QString(":/tx/%1.png").arg(imgid));

    if (newStatus == USER_ONLINE) {
        ui->pb_img->setIcon(QIcon(pix));
        ui->l_nick->setStyleSheet("");
        ui->label_2->setStyleSheet("");
    } else if (newStatus == USER_OFFLINE) {
        QImage img = pix.toImage().convertToFormat(QImage::Format_Grayscale8);
        ui->pb_img->setIcon(QIcon(QPixmap::fromImage(img)));
        ui->l_nick->setStyleSheet("color: gray;");
        ui->label_2->setStyleSheet("color: gray;");
    }
}
