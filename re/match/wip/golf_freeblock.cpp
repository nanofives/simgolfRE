// golf_clean.exe 0x0043d520 (release). WIP 81%: logic as below; the original keeps size[i] in edx and the slot
// pointer in ecx during the merge loop. Tried base-left comparison. Not in the 100% suite.
// FLAGS golf_clean.exe: /O2
extern int g_blockStart[100];            // 0x820b70
extern int g_blockSize[100];             // 0x820d00

// Returns n units at `base` to the 100 (start, size) blocks: extends the block ending at `base` and merges the
// block that then follows it; otherwise takes the first free slot.
// MATCH: golf_clean.exe 0x0043d520 ?freeBlock@@YAXHH@Z
void freeBlock(int base, int n)
{
    int i;
    for (i = 0; i < 100; i++) {
        if (g_blockStart[i] >= 0 && base == g_blockStart[i] + g_blockSize[i]) {
            g_blockSize[i] += n;
            for (int j = 0; j < 100; j++) {
                if (g_blockStart[j] == g_blockStart[i] + g_blockSize[i]) {
                    g_blockStart[j] = -1;
                    g_blockSize[i] += g_blockSize[j];
                    g_blockSize[j] = 0;
                }
            }
            return;
        }
    }
    for (i = 0; i < 100; i++) {
        if (g_blockStart[i] < 0) {
            g_blockStart[i] = base;
            g_blockSize[i] = n;
            return;
        }
    }
}
