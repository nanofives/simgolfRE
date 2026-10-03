// golf_clean.exe 0x004669f0 (release). WIP 56.6%: same instructions, different register roles. The original
// keeps &g_rec[golfer].partner in ebx (pushed at entry), the loop index in edi pushed inside the branch, s in
// esi, and ends the matched branch with `test eax,eax; jne L; jmp L`. Tried: maxChapter >= 2, 16-bit flag
// test, pointer loop, a single chapter variable, empty if after the stores. Not in the 100% suite.
// FLAGS golf_clean.exe: /O2
// Full golfer record view for the chapter picker (offsets from the instructions of 0x4669f0).
struct GolferRec {                       // 0x100 bytes at 0x5794b8
    int   x, y;                          // +0x00, +0x04
    char  pad08[0x21 - 0x08];
    char  hole;                          // +0x21
    char  pad22[0x37 - 0x22];
    unsigned char holeState[0x70 - 0x37];// +0x37: per hole, chapter * 8 + 1 once shown
    unsigned char thoughts[0x88 - 0x70]; // +0x70: recent thought ids (0x13 = hole finished)
    unsigned short tflags[(0xa2 - 0x88) / 2]; // +0x88: per recent thought, top 2 bits used
    short partner;                       // +0xa2
    short mood;                          // +0xa4
    char  padA6[0xb2 - 0xa6];
    short stage;                         // +0xb2
    short counter;                       // +0xb4
    char  padB6[0x100 - 0xb6];
};
extern GolferRec g_rec[];                // 0x5794b8 (same storage as g_golfers)
extern int g_lastChapter;                // 0x838a94
int  clamp(int v, int lo, int hi);       // 0x467130
int  nearObjects(int x, int y, unsigned int want);  // 0x407000
void f_466b70(int a, int maxChapter, int chapter, int z);  // 0x466b70

// Picks the chapter (0..maxChapter) the partner of `golfer` shows on `hole` and stores it as the partner's
// stage and hole state. If the partner is on that hole and has not shown it yet: (clamp(mood, counter, 99)
// - 1) / 2, capped to maxChapter - 1, +1 / -1 / -2 by the top bits of the first non-0x13 recent thought,
// and maxChapter when nearObjects(partner position, 8). Otherwise: this golfer's own state for the hole / 8.
// MATCH: golf_clean.exe 0x004669f0 ?pickChapter@@YAXHHHI@Z
void pickChapter(int a, int maxChapter, int hole, unsigned int golfer)
{
    int chapter = clamp(g_rec[golfer ^ 1].holeState[hole] >> 3, 0, maxChapter);
    short* partner = &g_rec[golfer].partner;
    if (g_rec[*partner].hole == hole && g_rec[*partner].holeState[hole] == 0) {
        GolferRec* p = &g_rec[*partner];
        int i;
        for (i = 0; p->thoughts[i] == 0x13 && i < 4; i++)
            ;
        int s = (clamp(p->mood, p->counter, 99) - 1) / 2;
        if (maxChapter > 1 && s > maxChapter - 1)
            s = maxChapter - 1;
        unsigned short t = g_rec[*partner].tflags[i] & 0xc000;
        if (t == 0x4000)
            s++;
        else if (s != 0) {
            if (t != 0)
                s--;
            if (s != 0 && t == 0xc000)
                s--;
        }
        if (nearObjects(g_rec[*partner].x, g_rec[*partner].y, 8))
            s = maxChapter;
        chapter = clamp(s, 0, maxChapter);
        g_rec[*partner].stage = chapter;
        g_rec[*partner].holeState[hole] = chapter * 8 + 1;
    } else {
        g_rec[*partner].stage = chapter;
        g_rec[*partner].holeState[hole] = chapter * 8 + 1;
    }
    g_lastChapter = chapter;
    f_466b70(a, maxChapter, chapter, -1);
}
