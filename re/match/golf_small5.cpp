// Small golf_clean.exe functions, batch 5 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2

extern signed char g_tileType[50][50];   // 0x5722e8 ([x][y]; 0x14 = hidden, as in Terrain.dll's Tile::isHidden)

// 1 when (x, y) is outside the 50x50 grid or the tile type is 0x14.
// MATCH: golf_clean.exe 0x0040bf60 ?tileBlocked@@YAHHH@Z
int tileBlocked(int x, int y)
{
    if (x < 0 || x >= 50 || y < 0 || y >= 50 || g_tileType[x][y] == 0x14)
        return 1;
    return 0;
}

// Tile type at a fixed-point position (0x400 per tile); 0x14 when blocked.
// MATCH: golf_clean.exe 0x0040bfa0 ?typeAtPos@@YAHHH@Z
int typeAtPos(int px, int py)
{
    int x = px >> 10, y = py >> 10;
    if (tileBlocked(x, y))
        return 0x14;
    return g_tileType[x][y];
}

struct Rect4 { int x0, y0, x1, y1; };

// MATCH: golf_clean.exe 0x00492610 ?inRect@@YAHHHPAURect4@@@Z
int inRect(int x, int y, Rect4* r)
{
    if (x < r->x0)
        return 0;
    if (x >= r->x1)
        return 0;
    if (y < r->y0)
        return 0;
    return y < r->y1;
}

struct GolferT { char pad[0x88]; unsigned short tflags[5]; char pad2[0x100 - 0x92]; };
extern GolferT g_gt[];                   // 0x5794b8

// +1 per recent thought flagged 0x4000, -1 per 0xc000 (top two bits of the 5 shorts at golfer +0x88).
// MATCH: golf_clean.exe 0x0045c420 ?thoughtBalance@@YAHH@Z
int thoughtBalance(int golfer)
{
    int n = 0;
    unsigned short* p = g_gt[golfer].tflags;
    for (int i = 5; i; i--, p++) {
        if ((*p & 0xc000) == 0x4000)
            n++;
        if ((*p & 0xc000) == 0xc000)
            n--;
    }
    return n;
}

struct Rec24 { int x, y, id; char pad[0x24 - 12]; };
extern Rec24 g_rec24[];                  // 0x5736b0, stride 0x24, until 0x575ab0

// Frees (id = -1) every record whose position is in tile (tx, ty).
// MATCH: golf_clean.exe 0x00402930 ?freeAtTile@@YAXHH@Z
void freeAtTile(int tx, int ty)
{
    for (int i = 0; i < 0x100; i++)
        if (g_rec24[i].id != -1 && g_rec24[i].x >> 10 == tx && g_rec24[i].y >> 10 == ty)
            g_rec24[i].id = -1;
}

class View47b {
public:
    void toLocal(int* x, int* y);        // 0x47b290
    void toGlobal(int* x, int* y);       // 0x47b2d0
    void f_47b170(int* x, int* y);       // 0x47b170
    void f_47b200(int* x, int* y);       // 0x47b200
    char pad[0x1ac];
    int  m_1ac, m_1b0;                   // +0x1ac, +0x1b0 origin
};

// MATCH: golf_clean.exe 0x0047b290 ?toLocal@View47b@@QAEXPAH0@Z
void View47b::toLocal(int* x, int* y)
{
    f_47b170(x, y);
    *x -= m_1ac;
    *y -= m_1b0;
}

// MATCH: golf_clean.exe 0x0047b2d0 ?toGlobal@View47b@@QAEXPAH0@Z
void View47b::toGlobal(int* x, int* y)
{
    f_47b200(x, y);
    *x += m_1ac;
    *y += m_1b0;
}

extern int g_83ad4c;                     // 0x83ad4c
int  f_483bb0();                         // 0x483bb0
void f_497b00();                         // 0x497b00

// MATCH: golf_clean.exe 0x00483bd0 ?drain483bd0@@YAXXZ
void drain483bd0()
{
    g_83ad4c = 0x3f;
    if (f_483bb0()) {
        do
            g_83ad4c = 0x3f;
        while (f_483bb0());
    }
    g_83ad4c = 0;
    f_497b00();
}
