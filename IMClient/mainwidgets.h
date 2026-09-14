#ifndef MAINWIDGETS_H
#define MAINWIDGETS_H

#include <map>
#include <QWidget>
#include <QMenu>
#include "frienditem.h"

namespace Ui {
class mainWidgets;
}

// 登录成功后的主界面：左上角个人信息 + 空间/朋友/消息标签页 + 好友列表 + 菜单
class mainWidgets : public QWidget
{
    Q_OBJECT

public:
    explicit mainWidgets(QWidget *parent = nullptr);
    ~mainWidgets();

    void setUserid(int id) { userid = id; }
    int getUserid() { return userid; }
    QString getNick() { return m_nick; }

    // 设置当前登录用户自己的信息（头像、昵称、签名）
    void setmyInfo(int imgid, QString nick, QString feeling);

    // 将好友置为离线显示
    void setFriOff(int userid);

    // 添加好友到列表（已存在则更新）
    void addFriendItem(int userid, int imgid, int status, QString nick, QString feeling);

    // 给好友追加一条聊天消息
    void setFriendMsg(int friid, QString msg);

    // 好友列表：userid -> 列表项控件
    std::map<int, friendItem*> m_friendMAp;

    void closeEvent(QCloseEvent* event) override;

signals:
    void signal_sentaddFriToKernel(QString);
    void signals_close();

public slots:
    // 在鼠标位置上方弹出菜单
    void slot_setMenu();
    // 处理菜单选择
    void slots_dealMenu(QAction*);

private:
    Ui::mainWidgets *ui;
    int userid;        ///< 当前登录用户的 ID
    int imgid;         ///< 当前用户头像编号
    QString m_nick;    ///< 当前用户昵称
    QString feeling;   ///< 当前用户心情签名

    QMenu m_menu;
    QAction *m_addFri;
    QAction *m_sysSet;
};

#endif // MAINWIDGETS_H
