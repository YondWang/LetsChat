#include "widget.h"
#include "dialog.h"
#include <QApplication>
#include <QMessageBox>

#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Dialog dlg;
    int ret = dlg.exec();
    if(ret == 1){
        Widget w(dlg.NameGetter(), nullptr);
        //qDebug() << w.usrNameGetter();
        w.show();
        return a.exec();
    }
    else if(ret == 0){
        //QMessageBox::warning(nullptr, "error!!!", "program failed to start!!!");
        exit(0);
        return 0;
    }
    return 0;
}
