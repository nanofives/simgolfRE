#ifndef LOG_H
#define LOG_H

#include "DataPath.h"

struct _iobuf;
_iobuf *env_open(const char *name, const char *mode);

class Log {
public:
    Log(const char *path);
    virtual ~Log();
    int init(const char *path);
    void say(const char *tag, const char *detail, int value1, int value2, int value3);

    char *m_path;
    int m_suspended;
};

extern Log g_log;
extern int g_logDisabled;

void log_say(const char *tag, const char *detail, int value1, int value2, int value3);

#endif
