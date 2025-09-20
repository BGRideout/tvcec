#ifndef CECLOG_H
#define CECLOG_H

#include <stdint.h>
#include <stdio.h>

class CECLog
{
private:
    uint16_t            log_mask_;              // Log detail mask
    char                *log_file_;             // Log file name

    void print(const char *format, va_list ap);

public:
    CECLog();
    ~CECLog();

    void setMask(const uint16_t &mask) { log_mask_ = mask; }
    void setLogFile(const char *filename);
    void print(const char *format, ...);
    void print(const uint16_t &mask, const char *format, ...);
};

#endif // CECLOG_H
