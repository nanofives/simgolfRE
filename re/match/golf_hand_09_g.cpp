// FLAGS golf_clean.exe: /O2 /GX
int dist467170(int, int);
// MATCH: golf_clean.exe 0x00434980 ?pick434980@@YAHHH@Z
int pick434980(int x, int y)
{
    int r = -1;
    if (dist467170(x - 0x10d, y - 500) < 16)
        r = -2;
    if (dist467170(x - 0x12e, y - 0x233) < 20)
        r = 0;
    if (dist467170(x - 0x14d, y - 0x20e) < 20)
        r = 1;
    if (dist467170(x - 0x16b, y - 0x233) < 20)
        r = 2;
    if (dist467170(x - 0x1d6, y - 0x23f) < 18)
        r = 3;
    if (dist467170(x - 0x24f, y - 0x23f) < 18)
        r = 4;
    if (dist467170(x - 0x28c, y - 0x23f) < 18)
        r = 5;
    if (dist467170(x - 0x2ca, y - 0x23f) < 18)
        r = 6;
    if (dist467170(x - 0xf8, y - 0x237) < 12)
        r = 7;
    if (dist467170(x - 0x213, y - 0x244) < 12)
        r = 9;
    return r;
}
