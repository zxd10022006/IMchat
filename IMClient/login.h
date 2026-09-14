#ifndef LOGIN_H
#define LOGIN_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class Login;
}
QT_END_NAMESPACE

class Login : public QWidget
{
    Q_OBJECT

public:
    explicit Login(QWidget *parent = nullptr);
    ~Login() override;

    void closeEvent(QCloseEvent* event) override;

private:
    // 校验手机号：11 位纯数字
    bool isValidPhone(const QString& tel);

    Ui::Login *ui;

public slots:
    void slots_register();
    void slots_reg_clear();
    void slots_login();
    void slots_log_clear();

signals:
    void signal_sendRegister(QString, QString, QString);
    void signal_sendLogin(QString, QString);
    void signal_delete();
};
#endif // LOGIN_H
