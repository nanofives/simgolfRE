#include "jgldefs.h"
#include "Degrees.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "StringUtil.h"
#include "HeapMessage.h"

char msg[0x1000];

void purge_leading(char *s)
{
    char buf[512];
    char *p = s;
    while (isspace((unsigned char)*p))
        p++;
    strcpy(buf, p);
    strcpy(s, buf);
}

void purge_trailing(char *s)
{
    unsigned int len = strlen(s);
    if (len == 0)
        return;
    char *p = s + len - 1;
    while (p >= s) {
        if (!isspace((unsigned char)*p))
            break;
        *p = 0;
        if (p == s)
            break;
        --p;
    }
}

void purge_spaces(char *s)
{
    purge_trailing(s);
    purge_leading(s);
}

void kill_lf(char *s)
{
    char *p = strrchr(s, '\n');
    if (p)
        *p = 0;
}

void add_lf(char *s)
{
    char *e = s + strlen(s);
    e[0] = '\n';
    e[1] = '\0';
}

void EchoString(const char *s)
{
    while (*s)
        fputchar(*s++);
}

int g_stringSpare[2] = {0};
