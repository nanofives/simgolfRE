#include "fx.h"

int g_total;
const char g_name[] = "fixture";
int g_table[4] = { 1, 2, 3, 4 };
static int s_hits;
static const char *s_words[] = { "zero", "one", "two" };

static int bump(int v)
{
    s_hits++;
    return v + s_hits;
}

int classify(int v)
{
    switch (v) {
    case 0:
        return 10;
    case 1:
        return 11;
    case 2:
        return 13;
    case 3:
        return 17;
    case 4:
        return 19;
    case 5:
        return 23;
    default:
        return -1;
    }
}

int sum(int n)
{
    int i;
    int t = 0;
    for (i = 0; i < n; i++) {
        t += classify(i) + g_table[i & 3];
        t += s_words[i % 3][0];
    }
    g_total += t;
    return bump(t);
}
