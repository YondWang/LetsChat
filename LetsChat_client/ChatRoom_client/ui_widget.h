/********************************************************************************
** Form generated from reading UI file 'widget.ui'
**
** Created by: Qt User Interface Compiler version 5.12.12
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WIDGET_H
#define UI_WIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Widget
{
public:
    QGridLayout *gridLayout;
    QSpacerItem *horizontalSpacer_2;
    QFrame *line;
    QWidget *widget_5;
    QGridLayout *gridLayout_4;
    QWidget *widget_2;
    QLabel *userName_tl;
    QWidget *widget;
    QGridLayout *gridLayout_3;
    QTextEdit *send_te;
    QPushButton *pushButton_2;
    QSpacerItem *horizontalSpacer_3;
    QPushButton *close_pb;
    QPushButton *send_pb;
    QPushButton *sendFile_pb;
    QPushButton *pushButton_3;
    QSpacerItem *horizontalSpacer_4;
    QLabel *connectStatus_lb;
    QWidget *widget_3;
    QGridLayout *gridLayout_5;
    QPushButton *logout_bt;
    QListWidget *chatWindow_lw;
    QSpacerItem *horizontalSpacer;

    void setupUi(QWidget *Widget)
    {
        if (Widget->objectName().isEmpty())
            Widget->setObjectName(QString::fromUtf8("Widget"));
        Widget->resize(1135, 788);
        Widget->setStyleSheet(QString::fromUtf8(""));
        gridLayout = new QGridLayout(Widget);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer_2, 0, 2, 1, 2);

        line = new QFrame(Widget);
        line->setObjectName(QString::fromUtf8("line"));
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);

        gridLayout->addWidget(line, 3, 0, 1, 4);

        widget_5 = new QWidget(Widget);
        widget_5->setObjectName(QString::fromUtf8("widget_5"));
        QSizePolicy sizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(widget_5->sizePolicy().hasHeightForWidth());
        widget_5->setSizePolicy(sizePolicy);
        widget_5->setMinimumSize(QSize(180, 71));
        widget_5->setMaximumSize(QSize(180, 71));
        gridLayout_4 = new QGridLayout(widget_5);
        gridLayout_4->setObjectName(QString::fromUtf8("gridLayout_4"));
        widget_2 = new QWidget(widget_5);
        widget_2->setObjectName(QString::fromUtf8("widget_2"));
        sizePolicy.setHeightForWidth(widget_2->sizePolicy().hasHeightForWidth());
        widget_2->setSizePolicy(sizePolicy);
        widget_2->setStyleSheet(QString::fromUtf8("image: url(:/res/avatar.png);"));

        gridLayout_4->addWidget(widget_2, 0, 0, 1, 1);

        userName_tl = new QLabel(widget_5);
        userName_tl->setObjectName(QString::fromUtf8("userName_tl"));
        QSizePolicy sizePolicy1(QSizePolicy::Minimum, QSizePolicy::Minimum);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(userName_tl->sizePolicy().hasHeightForWidth());
        userName_tl->setSizePolicy(sizePolicy1);
        userName_tl->setStyleSheet(QString::fromUtf8("font: 14pt \"\345\256\213\344\275\223\";"));

        gridLayout_4->addWidget(userName_tl, 0, 1, 1, 1);


        gridLayout->addWidget(widget_5, 0, 0, 1, 1);

        widget = new QWidget(Widget);
        widget->setObjectName(QString::fromUtf8("widget"));
        QSizePolicy sizePolicy2(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(widget->sizePolicy().hasHeightForWidth());
        widget->setSizePolicy(sizePolicy2);
        gridLayout_3 = new QGridLayout(widget);
        gridLayout_3->setObjectName(QString::fromUtf8("gridLayout_3"));
        send_te = new QTextEdit(widget);
        send_te->setObjectName(QString::fromUtf8("send_te"));
        QSizePolicy sizePolicy3(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(send_te->sizePolicy().hasHeightForWidth());
        send_te->setSizePolicy(sizePolicy3);
        QFont font;
        font.setFamily(QString::fromUtf8("\346\245\267\344\275\223"));
        font.setPointSize(12);
        send_te->setFont(font);

        gridLayout_3->addWidget(send_te, 2, 0, 1, 7);

        pushButton_2 = new QPushButton(widget);
        pushButton_2->setObjectName(QString::fromUtf8("pushButton_2"));
        pushButton_2->setFont(font);

        gridLayout_3->addWidget(pushButton_2, 0, 1, 1, 1);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_3->addItem(horizontalSpacer_3, 0, 3, 1, 4);

        close_pb = new QPushButton(widget);
        close_pb->setObjectName(QString::fromUtf8("close_pb"));
        close_pb->setStyleSheet(QString::fromUtf8("font: 12pt \"\346\245\267\344\275\223\";"));

        gridLayout_3->addWidget(close_pb, 3, 5, 1, 1);

        send_pb = new QPushButton(widget);
        send_pb->setObjectName(QString::fromUtf8("send_pb"));
        send_pb->setStyleSheet(QString::fromUtf8("font: 12pt \"\346\245\267\344\275\223\";"));

        gridLayout_3->addWidget(send_pb, 3, 6, 1, 1);

        sendFile_pb = new QPushButton(widget);
        sendFile_pb->setObjectName(QString::fromUtf8("sendFile_pb"));
        sendFile_pb->setFont(font);

        gridLayout_3->addWidget(sendFile_pb, 0, 0, 1, 1);

        pushButton_3 = new QPushButton(widget);
        pushButton_3->setObjectName(QString::fromUtf8("pushButton_3"));
        pushButton_3->setFont(font);

        gridLayout_3->addWidget(pushButton_3, 0, 2, 1, 1);

        horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_3->addItem(horizontalSpacer_4, 3, 0, 1, 5);


        gridLayout->addWidget(widget, 4, 0, 1, 4);

        connectStatus_lb = new QLabel(Widget);
        connectStatus_lb->setObjectName(QString::fromUtf8("connectStatus_lb"));
        sizePolicy1.setHeightForWidth(connectStatus_lb->sizePolicy().hasHeightForWidth());
        connectStatus_lb->setSizePolicy(sizePolicy1);
        QFont font1;
        font1.setPointSize(10);
        connectStatus_lb->setFont(font1);
        connectStatus_lb->setAlignment(Qt::AlignCenter);

        gridLayout->addWidget(connectStatus_lb, 0, 1, 1, 1);

        widget_3 = new QWidget(Widget);
        widget_3->setObjectName(QString::fromUtf8("widget_3"));
        sizePolicy1.setHeightForWidth(widget_3->sizePolicy().hasHeightForWidth());
        widget_3->setSizePolicy(sizePolicy1);
        gridLayout_5 = new QGridLayout(widget_3);
        gridLayout_5->setObjectName(QString::fromUtf8("gridLayout_5"));
        logout_bt = new QPushButton(widget_3);
        logout_bt->setObjectName(QString::fromUtf8("logout_bt"));
        QFont font2;
        font2.setFamily(QString::fromUtf8("\346\245\267\344\275\223"));
        font2.setPointSize(12);
        font2.setUnderline(true);
        logout_bt->setFont(font2);

        gridLayout_5->addWidget(logout_bt, 1, 0, 1, 1);

        chatWindow_lw = new QListWidget(widget_3);
        chatWindow_lw->setObjectName(QString::fromUtf8("chatWindow_lw"));
        QFont font3;
        font3.setFamily(QString::fromUtf8("\345\256\213\344\275\223"));
        font3.setPointSize(13);
        chatWindow_lw->setFont(font3);

        gridLayout_5->addWidget(chatWindow_lw, 2, 0, 1, 2);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_5->addItem(horizontalSpacer, 1, 1, 1, 1);


        gridLayout->addWidget(widget_3, 1, 0, 2, 4);


        retranslateUi(Widget);

        QMetaObject::connectSlotsByName(Widget);
    } // setupUi

    void retranslateUi(QWidget *Widget)
    {
        Widget->setWindowTitle(QApplication::translate("Widget", "YUAN`sChatRoom", nullptr));
        userName_tl->setText(QApplication::translate("Widget", "USERNAME", nullptr));
        pushButton_2->setText(QApplication::translate("Widget", "PushButton", nullptr));
        close_pb->setText(QApplication::translate("Widget", "\345\205\263\351\227\255", nullptr));
        send_pb->setText(QApplication::translate("Widget", "\345\217\221\351\200\201", nullptr));
        sendFile_pb->setText(QApplication::translate("Widget", "\345\217\221\351\200\201\346\226\207\344\273\266", nullptr));
        pushButton_3->setText(QApplication::translate("Widget", "PushButton", nullptr));
        connectStatus_lb->setText(QApplication::translate("Widget", "ConnectStatus", nullptr));
        logout_bt->setText(QApplication::translate("Widget", "pushButton", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Widget: public Ui_Widget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WIDGET_H
