
#include "jgldefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Log.h"
#include "JackalUtil.h"

Log g_log("logfile.txt");

void CopyLog()
{
    Log copy(g_log);
    copy.m_path = 0;
}
static int s_spareWord = 0;
int g_logDisabled = 0;

Log::Log(const char *path)
{
    m_path = 0;
    init(path);
}

Log::~Log()
{
    if (m_path) {
        FreeIfNotNull(m_path);
        m_path = 0;
    }
}

int Log::init(const char *path)
{
    FILE *f;
    if (!path)
        return 0x10;
    if (m_path) {
        FreeIfNotNull(m_path);
        m_path = 0;
    }
    m_path = (char *)AllocateBytes(strlen(path) + 1);
    if (!m_path)
        return 4;
    ClearString(m_path);
    AppendString(m_path, path);
    f = env_open(path, "wt");
    if (!f)
        return 6;
    fclose(f);
    return 0;
}

void Log::say(const char *tag, const char *detail, int value1, int value2, int value3)
{
    if (m_path == 0)
        return;
    if (m_suspended != 0)
        return;
    if (g_logDisabled != 0)
        return;
    FILE *f = env_open(m_path, "at");
    if (f == 0)
        return;
    if (detail)
        fprintf(f, "%s %s %d %d %d\n", tag, detail, value1, value2, value3);
    else
        fprintf(f, "%s %d %d %d\n", tag, value1, value2, value3);
    fclose(f);
}

void log_say(const char *tag, const char *detail, int value1, int value2, int value3)
{
    g_log.say(tag, detail, value1, value2, value3);
}

void log_set_state(int enabled)
{
    g_log.m_suspended = enabled == 0;
}

int g_logSpare = 0;
