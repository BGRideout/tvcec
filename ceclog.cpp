#include "ceclog.h"

#include <stdarg.h>
#include <string.h>
#include <time.h>

CECLog::CECLog() : log_mask_(0xff), log_file_(nullptr)
{

}

CECLog::~CECLog()
{
    if (log_file_)
    {
        delete[] log_file_;
    }
}

void CECLog::setLogFile(const char *filename)
{
    if (log_file_)
    {
        delete [] log_file_;
        log_file_ = nullptr;
    }
    if (filename)
    {
        int n = strlen(filename);
        if (n > 0)
        {
            log_file_ = new char[n + 1];
            strncpy(log_file_, filename, n + 1);
        }
    }
}

void CECLog::print(const char *format, va_list ap)
{
    time_t  now;
    time(&now);
    char timbuf[32];
    strftime(timbuf, sizeof(timbuf), "%m/%d %H:%M:%S", localtime(&now));
    if (log_file_)
    {
        FILE *f = fopen(log_file_, "a");
        if (f)
        {
            fprintf(f, "%s ", timbuf);
            vfprintf(f, format, ap);
            fprintf(f, "\n");
            fclose(f);
        }
    }
    printf("%s ", timbuf);
    vprintf(format, ap);
    printf("\n");
    fflush(stdout);
}

void CECLog::print(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    print(format, ap);
    va_end(ap);
}

void CECLog::print(const uint16_t &mask, const char *format, ...)
{
    if ((mask & log_mask_) != 0)
    {
        va_list ap;
        va_start(ap, format);
        print(format, ap);
        va_end(ap);
    }
}
