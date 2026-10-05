// golf_clean.exe 0x0040de70 (release). WIP 61%, not in the 100% suite: the original loads x, y and r up front,
// spills x1/y0/y1 to the stack and reads the tile flags with `mov cx, word ptr` before testing 0x200.
// Tried: hoisted y bounds, a ushort flags local (best, below), centre-tile locals.
// FLAGS golf_clean.exe: /O2

int offMap(int x, int y);                // 0x40bf60
int distance(int dx, int dy);            // 0x40acd0
extern unsigned short g_tileFlags[50][50];   // 0x53caf0
extern int g_nearestDist;                // 0x568d0c
extern int g_nearestTx, g_nearestTy;     // 0x56a91c, 0x56a920
// Nearest tile with flag 0x200 within 4 tiles of world point (x, y) and closer than r tiles: result in
// g_nearestTx/Ty (-1: none) and g_nearestDist.
// MATCH: golf_clean.exe 0x0040de70 ?nearestFlag200@@YAXHHH@Z
void nearestFlag200(int x, int y, int r)
{
    int cx = x >> 10;
    int cy = y >> 10;
    int x0 = cx - 4;
    g_nearestDist = r << 10;
    int x1 = cx + 4;
    g_nearestTx = -1;
    for (int tx = x0; tx <= x1; tx++) {
        for (int ty = cy - 4; ty <= cy + 4; ty++) {
            if (!offMap(tx, ty)) {
                unsigned short f = g_tileFlags[tx][ty];
                if (f & 0x200) {
                    int d = distance(tx * 0x400 - x + 0x200, ty * 0x400 - y + 0x200);
                    if (d < g_nearestDist) {
                        g_nearestDist = d;
                        g_nearestTx = tx;
                        g_nearestTy = ty;
                    }
                }
            }
        }
    }
}
