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
    int ret =  1;
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
    char *logfile = nullptr;
    uint16_t logmask = 0xff;
    for (int ii = 1; ii < argc; ii++)
    {
        if (strcmp(argv[ii], "-dw") == 0) log |= CEC::CEC_LOG_WARNING;
        if (strcmp(argv[ii], "-dn") == 0) log |= CEC::CEC_LOG_NOTICE;
        if (strcmp(argv[ii], "-dt") == 0) log |= CEC::CEC_LOG_TRAFFIC;
        if (strcmp(argv[ii], "-da") == 0) log |= CEC::CEC_LOG_ALL;

        if (strcmp(argv[ii], "-11") == 0) {if (logmask == 0xff) logmask = 0; logmask |= 1;}
        if (strcmp(argv[ii], "-l2") == 0) {if (logmask == 0xff) logmask = 0; logmask |= 2;}
        if (strcmp(argv[ii], "-l4") == 0) {if (logmask == 0xff) logmask = 0; logmask |= 4;}

        if (strcmp(argv[ii], "-log") == 0 && ii + 1 < argc)
        {
            logfile = argv[++ii];
        }
        else if (argv[ii][0] != '-') remote = argv[ii];
    }
    tvcec->setLogLevel(static_cast<CEC::cec_log_level>(log));
    tvcec->setRemote(remote);
    tvcec->setLogFile(logfile);
    tvcec->setLogMask(logmask);
    if (tvcec->init())
    {
        ret = a.exec();
    }

    delete tvcec;

    return ret;
}
