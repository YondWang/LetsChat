#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>

namespace Ui {
class Dialog;
}

class Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = nullptr);
    QString NameGetter();
    ~Dialog();

private slots:
    void on_login_bt_clicked();

    void on_exit_bt_clicked();

    void on_usrName_le_returnPressed();

private:
    Ui::Dialog *ui;
    QString usr_name;
};

#endif // DIALOG_H
