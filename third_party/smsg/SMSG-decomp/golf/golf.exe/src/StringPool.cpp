#include "jgldefs.h"
#include "crt.h"
#include "Golfer.h"
#include "StringPool.h"
#include "TextWrap.h"
#include <string.h>

int ParseFileForSection(const char *path, const char *section)
{
    char line[256];
    int i;
    FILE *fp;
    int count;

    count = 0;
    fp = fopen(path, "rt");
    while (1) {
        fgets(line, 250, fp);
        line[strlen(line) - 1] = 0;
        if (stricmp(line, section) == 0)
            break;
        count++;
        if (count >= 100)
            break;
    }
    if (count >= 100) {
        fclose(fp);
        return -1;
    }
    count = 0;
    i = 0;
    fgets(line, 250, fp);
    line[strlen(line) - 1] = 0;
    for (; i < 100; i++) {
        strcat(g_printBuffer, line);
        strcat(g_printBuffer, " ");
        fgets(line, 250, fp);
        line[strlen(line) - 1] = 0;
        count++;
        if (line[0] == '*')
            break;
    }
    fclose(fp);
    return count;
}

void StringReplace(const char *find, const char *repl)
{
    char rest[0x200];
    char *p;

    p = strstr(g_printBuffer, find);
    if (p == 0) {
        return;
    }
    strcpy(rest, p);
    *p = 0;
    strcat(g_printBuffer, repl);
    strcat(g_printBuffer, rest + strlen(find));
}

void ClearStringPool()
{
    memset(g_txtHeapBuffer, 0, sizeof g_txtHeapBuffer);
    memset(g_strOffsetTbl, -1, sizeof g_strOffsetTbl);
}

int StoreStringInPool(int slot)
{
    int highWater;
    int freeSlot;
    int i;
    int len;

    if (slot != -1 && g_strOffsetTbl[slot] != -1) {
        int at = g_strOffsetTbl[slot];
        int gone = strlen(g_txtHeapBuffer + at) + 1;
        memcpy(g_txtHeapBuffer + at, g_txtHeapBuffer + at + gone, 0x1002 - gone - at);
        g_strOffsetTbl[slot] = -1;
        for (i = 0; i < 128; i++)
            if (g_strOffsetTbl[i] > at)
                g_strOffsetTbl[i] -= gone;
    }
    highWater = 0;
    freeSlot = -1;
    for (i = 0; i < 128; i++) {
        if (g_strOffsetTbl[i] != -1) {
            if (g_stringPoolLens[i] + g_strOffsetTbl[i] > highWater)
                highWater = g_stringPoolLens[i] + g_strOffsetTbl[i];
        }
        if (g_strOffsetTbl[i] == -1 && freeSlot == -1 && i > 32)
            freeSlot = i;
    }
    if (slot != -1)
        freeSlot = slot;
    len = strlen(g_printBuffer) + 1;
    if (highWater + len < 0x1002) {
        strcpy(g_txtHeapBuffer + highWater, g_printBuffer);
        g_strOffsetTbl[freeSlot] = (short)highWater;
        g_stringPoolLens[freeSlot] = (short)len;
        return freeSlot;
    }
    return -1;
}

int AppendPooledString(int slot)
{
    if (slot != -1 && g_strOffsetTbl[slot] != -1) {
        strcat(g_printBuffer, &g_txtHeapBuffer[g_strOffsetTbl[slot]]);
        return 1;
    }
    return 0;
}

static int s_poolSpare[52] = {0};
unsigned int g_freeLandmarks = 0;
unsigned short *g_colorLut = 0;
int g_courseId = 0;
static int s_poolSpare1[3] = {0};
int g_level = 0;
int g_screenWidth = 0;
int g_screenHeight = 0;
static int s_poolSpare2 = 0;
