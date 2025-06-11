#include "dialog.h"
#include "ui_dialog.h"

#include <QMessageBox>

Dialog::Dialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);
}

QString Dialog::NameGetter()
{
    return usr_name;
}

Dialog::~Dialog()
{
    delete ui;
}

void Dialog::on_login_bt_clicked()
{
    if(!ui->usrName_le->text().isEmpty()){
        usr_name = ui->usrName_le->text();
        setResult(1);
        hide();
    }
    else
        QMessageBox::information(this, "tip!!", "userName cant be NULL!");
}


void Dialog::on_exit_bt_clicked()
{
    setResult(0);
    hide();
}

void Dialog::on_usrName_le_returnPressed()
{
    on_login_bt_clicked();
}

