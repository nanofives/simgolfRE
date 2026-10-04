// golf_clean.exe functions, batch 28 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2

extern signed char g_tileType[50][50];   // 0x5722e8
extern unsigned char g_tileByte[50][50]; // 0x56988c
extern signed char g_level543018[50][50];// 0x543018
extern int g_dirX[8], g_dirY[8];         // 0x4c2878 {0,1,1,1,0,-1,-1,-1}, 0x4c2898 {-1,-1,0,1,1,1,0,-1}
int offMap(int x, int y);                // 0x40bf60
// Raises tile (x, y)'s level to the highest level among its 4 edge neighbours (directions 0, 2, 4, 6) of the
// same tile type (and, when sameByte, the same tile byte); returns 1 when it changed. nx before ny is required;
// `x + dir` and `dir + x` compile the same (source not determined there).
// MATCH: golf_clean.exe 0x0042f6e0 ?raiseFromNeighbours@@YAHHHH@Z
int raiseFromNeighbours(int x, int y, int sameByte)
{
    int changed = 0;
    int best = g_level543018[x][y];
    for (int i = 0; i < 8; i += 2) {
        int nx = x + g_dirX[i];
        int ny = y + g_dirY[i];
        if (!offMap(nx, ny) && g_tileType[nx][ny] == g_tileType[x][y] &&
            (!sameByte || g_tileByte[nx][ny] == g_tileByte[x][y]) && g_level543018[nx][ny] > best) {
            best = g_level543018[nx][ny];
            g_level543018[x][y] = (signed char)best;
            changed = 1;
        }
    }
    return changed;
}
