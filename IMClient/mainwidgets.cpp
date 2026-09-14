#include "mainwidgets.h"
#include "ui_mainwidgets.h"
#include <QIcon>
#include <QListWidgetItem>
#include <QSize>
#include <QCursor>
#include <QInputDialog>
#include <QMessageBox>
#include <QDebug>

mainWidgets::mainWidgets(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::mainWidgets), userid(0)
{
    ui->setupUi(this);

    connect(ui->pb_menu, &QPushButton::clicked, this, &mainWidgets::slot_setMenu);

    m_addFri = m_menu.addAction("添加好友");
    m_sysSet = m_menu.addAction("系统设置");

    connect(&m_menu, &QMenu::triggered, this, &mainWidgets::slots_dealMenu);
}

mainWidgets::~mainWidgets()
{
    delete ui;
    for (auto &pr : m_friendMAp) {
        delete pr.second;
    }
}

// 显示当前登录用户自己的信息（头像、昵称、签名）
void mainWidgets::setmyInfo(int imgid, QString nick, QString feeling) {
    QIcon icon(QString(":/tx/%1.png").arg(imgid));
    ui->pb_img->setIcon(icon);
    ui->pb_img->setIconSize(QSize(60, 60));

    ui->l_nick->setText(nick);
    ui->le_feeling->setText(feeling);

    this->imgid = imgid;
    this->m_nick = nick;
    this->feeling = feeling;
}

// 将好友置为离线显示
void mainWidgets::setFriOff(int userid) {
    if (m_friendMAp.count(userid)) {
        m_friendMAp[userid]->setOFFline();
    }
}

void mainWidgets::closeEvent(QCloseEvent* event) {
    qDebug() << "closeEvent";
    emit signals_close();
}

// 添加好友列表项：头像图标 + 昵称 + 签名
void mainWidgets::addFriendItem(int userid, int imgid, int status, QString nick, QString feeling) {
    friendItem *pfriItem = new friendItem;

    pfriItem->setFriId(userid);
    pfriItem->showFriInfo(imgid, status, nick, feeling);

    QListWidgetItem* pItem = new QListWidgetItem;
    ui->listWidget->addItem(pItem);
    ui->listWidget->setItemWidget(pItem, pfriItem);
    pItem->setSizeHint(pfriItem->size());

    m_friendMAp[userid] = pfriItem;
}

// 给好友追加一条聊天消息
void mainWidgets::setFriendMsg(int friid, QString msg) {
    if (m_friendMAp.count(friid)) {
        m_friendMAp[friid]->setChatMas(msg);
    }
}

// 在鼠标位置上方弹出菜单
void mainWidgets::slot_setMenu() {
    QPoint po = QCursor::pos();

    QSize size = m_menu.sizeHint();
    po.setY(po.y() - size.height());

    m_menu.exec(po);
}

// 处理菜单选择
void mainWidgets::slots_dealMenu(QAction* act) {
    bool ok = false;
    QString friNick = QInputDialog::getText(this, "添加好友", "输入要添加的好友名字", QLineEdit::Normal, "...", &ok).trimmed();

    if (ok) {
        if (act == m_addFri) {
            // 检查是否已经是好友
            for (const auto &pr : m_friendMAp) {
                if (pr.second->getNick() == friNick) {
                    QMessageBox::information(this, "提示", "已经有该好友");
                    return;
                }
            }

            if (friNick == m_nick) {
                QMessageBox::information(this, "提示", "这是你自己啊 傻瓜");
                return;
            }

            emit signal_sentaddFriToKernel(friNick);
        }
    } else if (act == m_sysSet) {
        QMessageBox::information(this, "提示", "功能还没实现");
    }
}
