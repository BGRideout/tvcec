#include <QCoreApplication>
#include "tvcec.h"
#include <iostream>
#include <signal.h>
#include <string.h>

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

    QString remote("tvremote.local");
    TVCEC *tvcec = new TVCEC();
    uint32_t log = CEC::CEC_LOG_ERROR;
    for (int ii = 1; ii < argc; ii++)
    {
        if (strcmp(argv[ii], "-w") == 0) log |= CEC::CEC_LOG_WARNING;
        if (strcmp(argv[ii], "-n") == 0) log |= CEC::CEC_LOG_NOTICE;
        if (strcmp(argv[ii], "-t") == 0) log |= CEC::CEC_LOG_TRAFFIC;
        if (strcmp(argv[ii], "-a") == 0) log |= CEC::CEC_LOG_ALL;
        if (argv[ii][0] != '-') remote = argv[ii];
    }
    tvcec->setLogLevel(static_cast<CEC::cec_log_level>(log));
    tvcec->setRemote(remote);

    int ret = a.exec();

    delete tvcec;

    return ret;
}
