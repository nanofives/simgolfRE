// golf_clean.exe 0x0042f120 / 0x004340a0 / 0x00449f00 (release). WIP 79-97%: flood42f120 differs in the 16-bit
// mask test (and bp, 0x420 / test bp, bp), buttonAt in where the -0x17f offset is applied, syncTile in caching
// t->m_type in ebp before the first call. Not in the 100% suite.
// Small golf_clean.exe functions, batch 14 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2

struct TypeDef30 { char pad[2]; signed char f2; char pad3[0x30 - 3]; };   // 0x578370, stride 0x30
extern TypeDef30 g_typeDefs[];
extern signed char g_tileType[50][50];   // 0x5722e8
extern unsigned short g_tileFlags[50][50];   // 0x53caf0
extern int g_dirDx[], g_dirDy[];         // 0x4c2878, 0x4c2898 (8 directions)

// Marks the tile with flag 0x40 and, when its type has +2 > 0, recurses into the 4 even-direction neighbours
// that carry flag 0x420 and are not marked yet.
// MATCH: golf_clean.exe 0x0042f120 ?flood42f120@@YAXHH@Z
void flood42f120(int x, int y)
{
    if (g_tileFlags[x][y] & 0x40)
        return;
    g_tileFlags[x][y] |= 0x40;
    if (g_typeDefs[g_tileType[x][y]].f2 <= 0)
        return;
    for (int d = 0; d < 8; d += 2) {
        int nx = x + g_dirDx[d], ny = y + g_dirDy[d];
        unsigned short f = g_tileFlags[nx][ny];
        unsigned short m = f & 0x420;
        if (m && !(f & 0x40))
            flood42f120(nx, ny);
    }
}

int approxDistance(int dx, int dy);      // 0x467170

// Round button hit test: -2 / -3 for the two buttons at (0xec, 0x20e) r16 and (0x115, 0x237) r12, else the
// index 0..8 of a 5 + 4 grid (y scaled by 2, radius 40), -1 for none.
// MATCH: golf_clean.exe 0x004340a0 ?buttonAt@@YAHHH@Z
int buttonAt(int x, int y)
{
    if (approxDistance(x - 0xec, y - 0x20e) < 16)
        return -2;
    if (approxDistance(x - 0x115, y - 0x237) < 12)
        return -3;
    x += -0x17f;
    y = y * 2 - 0x426;
    for (int i = 0; i < 9; i++) {
        if (approxDistance(x, y) < 40)
            return i;
        if (i != 4)
            x -= 0x50;
        else {
            y -= 0x50;
            x += 0x118;
        }
    }
    return -1;
}

class Tile;
class Terrain {
public:
    int  getVariation(Tile* t);          // import thunk 0x4a4f2e
    void setType(Tile* t, int type, int rotation);   // import thunk 0x4a4f28
};
class Tile { public: char pad[0x24]; int m_type; };
extern Terrain* g_terrain;               // 0x820ed0
extern char g_820f2c;                    // 0x820f2c (set when type 6 or 7 is involved)
int tileType(int x, int y);              // 0x4492d0
int tileByte(int x, int y);              // 0x4492f0

// Pushes the exe's grid type/byte for (x, y) into Terrain.dll's tile when they differ; true if it did.
// MATCH: golf_clean.exe 0x00449f00 ?syncTile@@YA_NPAVTile@@HH@Z
bool syncTile(Tile* t, int x, int y)
{
    if (t->m_type == tileType(x, y) && g_terrain->getVariation(t) == tileByte(x, y))
        return false;
    if (tileType(x, y) == 7 || t->m_type == 7 || tileType(x, y) == 6 || t->m_type == 6)
        g_820f2c = 1;
    g_terrain->setType(t, tileType(x, y), tileByte(x, y));
    return true;
}
