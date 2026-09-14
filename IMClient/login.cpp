#include "login.h"
#include "./ui_login.h"
#include <QCloseEvent>
#include <QMessageBox>
#include <QDebug>

Login::Login(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Login)
{
    ui->setupUi(this);

    // 注册界面按钮
    connect(ui->pb_register, &QPushButton::clicked, this, &Login::slots_register);
    connect(ui->pb_reg_clear, &QPushButton::clicked, this, &Login::slots_reg_clear);
    // 登录界面按钮
    connect(ui->pb_login, &QPushButton::clicked, this, &Login::slots_login);
    connect(ui->pb_log_clear, &QPushButton::clicked, this, &Login::slots_log_clear);
}

Login::~Login()
{
    delete ui;
}

// 校验手机号：11 位纯数字
bool Login::isValidPhone(const QString& tel) {
    if (tel.size() != 11) return false;
    for (auto c : tel) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

void Login::slots_register() {
    // 判断昵称
    auto nick = ui->le_reg_nick->text();
    if (nick.size() == 0) {
        QMessageBox::information(this, "提示", "你爸没给你取名字吗！");
        return;
    }

    // 判断电话
    auto tel = ui->le_reg_tel->text();
    if (!isValidPhone(tel)) {
        QMessageBox::information(this, "提示", "你家电话号码长这样！");
        return;
    }

    // 判断密码
    auto pw = ui->le_reg_pw->text();
    auto pwa = ui->le_reg_pwagain->text();
    if (pw != pwa) {
        QMessageBox::information(this, "提示", "密码不一样啊，是不是人啊！");
        return;
    }

    emit signal_sendRegister(nick, tel, pw);
}

void Login::slots_reg_clear() {
    ui->le_reg_pw->clear();
    ui->le_reg_nick->clear();
    ui->le_reg_pwagain->clear();
    ui->le_reg_tel->clear();
}

void Login::slots_login() {
    // 电话号码
    auto tel = ui->le_log_tel->text();
    if (!isValidPhone(tel)) {
        QMessageBox::information(this, "提示", "你家电话号码长这样！");
        return;
    }
    auto pw = ui->le_log_pw->text();

    emit signal_sendLogin(tel, pw);
}

void Login::slots_log_clear() {
    ui->le_log_pw->clear();
    ui->le_log_tel->clear();
}

void Login::closeEvent(QCloseEvent* event) {
    qDebug() << "Login::closeEvent";
    emit signal_delete();
    event->accept();
}
