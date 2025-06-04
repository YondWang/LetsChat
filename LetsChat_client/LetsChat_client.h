#pragma once

#include <QtWidgets/QWidget>
#include "ui_LetsChat_client.h"

class LetsChat_client : public QWidget
{
    Q_OBJECT

public:
    LetsChat_client(QWidget *parent = nullptr);
    ~LetsChat_client();

private:
    Ui::LetsChat_clientClass ui;
};

