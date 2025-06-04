#include "LetsChat_client.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    LetsChat_client window;
    window.show();
    return app.exec();
}
