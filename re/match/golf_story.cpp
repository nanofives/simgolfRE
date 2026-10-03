// Golfer-pair story text (golf_clean.exe, release). Golfer records: 0x5794b8, stride 0x100.
// FLAGS golf_clean.exe: /O2
#include <string.h>

struct Golfer {                          // 0x100 bytes
    char  pad0[0x10];
    unsigned int flags;                  // +0x10 (0x100000 = playing with the pair partner)
    char  pad14[0x4a - 0x14];
    unsigned char holeStory[0xa2 - 0x4a];// +0x4a, indexed by hole
    short partner;                       // +0xa2
    char  padA4[0x100 - 0xa4];
};

extern Golfer g_golfers[];               // 0x5794b8
extern char   g_text[];                  // 0x51a068 (shared text buffer)
void f_4669f0(int a, int story, int hole, unsigned int golfer);   // 0x4669f0

// Fetches the story line for `hole` into g_text (0x4669f0). Paired: cut every '\n'. Not paired: keep the
// text after the first '\n' (empty if none). Returns the first character.
// MATCH: golf_clean.exe 0x004668f0 ?storyText@@YAHHHI@Z
int storyText(int a, int hole, unsigned int golfer)
{
    int i, len;
    int story;
    if (g_golfers[golfer].flags & 0x100000)
        story = g_golfers[golfer].holeStory[hole];
    else
        story = g_golfers[g_golfers[golfer].partner].holeStory[hole];
    if (story == 0)
        return 0;
    f_4669f0(a, story, hole, (g_golfers[golfer].flags & 0x100000) ? golfer : golfer ^ 1);
    len = strlen(g_text);
    if (g_golfers[golfer].flags & 0x100000) {
        for (i = 0; i < len; i++)
            if (g_text[i] == '\n')
                g_text[i] = 0;
    } else {
        for (i = 0; i < len; i++) {
            if (g_text[i] == '\n') {
                strcpy(g_text, g_text + i + 1);
                return g_text[0];
            }
        }
        g_text[0] = 0;
    }
    return g_text[0];
}
