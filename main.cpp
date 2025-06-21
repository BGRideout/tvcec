#include <QCoreApplication>
#include "tvcec.h"
#include <iostream>
#include <signal.h>

void handle_signal(int signal)
{
    qApp->quit();
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // Install the ctrl-C signal handler
    if( SIG_ERR == signal(SIGINT, handle_signal) )
    {
        std::cerr << "Failed to install the SIGINT signal handler\n";
        return 1;
    }

    TVCEC *tvcec = new TVCEC();

    int ret = a.exec();

    delete tvcec;

    return ret;
}
