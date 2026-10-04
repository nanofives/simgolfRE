// golf_clean.exe functions, batch 23 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <stdlib.h>
#include <string.h>

int f_467270(int a, int b);              // 0x467270 (b * cos(a))
int f_4672b0(int a, int b);              // 0x4672b0 (b * sin(a))
int worldToScreen(int wx, int wy, int* sx, int* sy, int flags);   // 0x42fb90
void penMove(int sx, int sy);            // 0x40bf00
void penLine(int sx, int sy, int color, int width, int style);   // 0x40bf20
// Draws a circle of radius r tiles around world point (x, y) as 24 segments (angle step 0x0aaaaaaa = 1/24 turn),
// lifting the pen where a point is off screen. The screen x reuses r.
// MATCH: golf_clean.exe 0x00407c60 ?drawCircle@@YAXHHHH@Z
void drawCircle(int x, int y, int r, int color)
{
    int len = (r << 10) / 25;
    int prev = -1;
    int a = 0;
    int sy;
    int n = 25;
    do {
        int px = f_467270(a, len) + x;
        int py = f_4672b0(a, len) + y;
        if (worldToScreen(px, py, &r, &sy, 0)) {
            if (prev == -1)
                penMove(r, sy);
            else
                penLine(r, sy, color, 2, 7);
            prev = r;
        } else {
            prev = -1;
        }
        a += 0xaaaaaaa;
    } while (--n);
}

extern char g_text[];                    // 0x51a068
extern char g_numBuf[];                  // 0x58a528
extern short g_year;                     // 0x5a6d3c
extern char* g_monthNames[];             // 0x4c2908 ("March", ...)
// Appends "<month> <year>" for a game date (8 months a year of 1024 units; year 0 = 2001) and stores the year.
// MATCH: golf_clean.exe 0x0040d7b0 ?appendDate@@YAXH@Z
void appendDate(int date)
{
    int year = date / 8192;
    int month = date / 1024 % 8;
    g_year = (short)year;
    strcat(g_text, g_monthNames[month]);
    strcat(g_text, " ");
    strcat(g_text, itoa(year + 2001, g_numBuf, 10));
}
