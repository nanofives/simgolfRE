// golf_clean.exe 0x0040ca10 / 0x0042f1c0 (release). WIP, not in the 100% suite.
// fillTiles 46%: the original keeps y + h in the h stack slot, sets col/row with `= 1; if (==) = 0` (mov/jne/xor,
//   not setne) and keeps two separate draw calls. spreadByte 57%: register roles and the recursion loop differ
//   (tried the literal shape below only).
// FLAGS golf_clean.exe: /O2

struct Sprite2c { void draw(void* surface, int x, int y, void* pal, int flags); char pad[0x2c]; };   // 0x4740f0
extern void* g_surface4c1570;            // 0x4c1570
extern void* g_pal824148;                // 0x824148
extern Sprite2c g_tileSprite;            // 0x5a5554
extern Sprite2c g_frame9[9];             // 0x541580, stride 0x2c: 3x3 frame pieces (row * 3 + col)
// Fills the rectangle (x, y, w, h) with 16x16 tiles: the plain tile, or (framed) the 3x3 frame pieces with
// corners/edges on the first and last row and column.
// MATCH: golf_clean.exe 0x0040ca10 ?fillTiles@@YAXHHHHH@Z
void fillTiles(int x, int y, int w, int h, int framed)
{
    int xe = x + w;
    for (int cx = x; cx < xe; cx += 16) {
        int ye = y + h;
        for (int cy = y; cy < ye; cy += 16) {
            if (!framed) {
                g_tileSprite.draw(g_surface4c1570, cx, cy, g_pal824148, 0);
            } else {
                int col = cx == x ? 0 : 1;
                if (cx + 16 >= xe)
                    col = 2;
                int row = cy == y ? 0 : 1;
                if (cy + 16 >= ye)
                    row = 2;
                g_frame9[row * 3 + col].draw(g_surface4c1570, cx, cy, g_pal824148, 0);
            }
        }
    }
}

extern signed char g_tileType[50][50];   // 0x5722e8
extern signed char g_tileByte[50][50];   // 0x56988c
struct TypeDef30 { char pad[6]; char kind; char pad7[0x30 - 7]; };   // 0x578370, stride 0x30
extern TypeDef30 g_typeDefs[];
extern int g_dirX[8], g_dirY[8];         // 0x4c2878, 0x4c2898
int offMap(int x, int y);                // 0x40bf60
// Recomputes tile (x, y)'s byte from its 8 neighbours: 0 when any neighbour's type kind is not 0x11, 1 when any
// neighbour's byte is 0; on a change, recurses into the neighbours of kind 0x11.
// MATCH: golf_clean.exe 0x0042f1c0 ?spreadByte@@YAXHH@Z
void spreadByte(int x, int y)
{
    signed char v = g_tileByte[x][y];
    int i;
    for (i = 0; i < 8; i++) {
        int ny = g_dirY[i] + y;
        int nx = g_dirX[i] + x;
        if (!offMap(nx, ny)) {
            if (g_typeDefs[g_tileType[nx][ny]].kind != 0x11)
                v = 0;
            if (g_tileByte[nx][ny] == 0 && v)
                v = 1;
        }
    }
    if (v != g_tileByte[x][y]) {
        g_tileByte[x][y] = v;
        for (i = 0; i < 8; i++) {
            int ny = g_dirY[i] + y;
            int nx = g_dirX[i] + x;
            if (!offMap(nx, ny) && g_typeDefs[g_tileType[nx][ny]].kind == 0x11)
                spreadByte(nx, ny);
        }
    }
}
