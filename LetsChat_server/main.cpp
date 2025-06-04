#include <cstdio>
#include "CChatServer.h"

int main()
{
    CChatServer chatServer;
    int err = chatServer.StartService();


    getchar();
    return 0;
}