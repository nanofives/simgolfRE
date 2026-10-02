// Terrain::tileHit: screen point -> tile under it (Terrain.dll, debug build; not compiled into the exe).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
//
// Determined by the match (alternatives tried and rejected): `-a + 24` (not `24 - a`: neg/add at 0x1000ac88),
// the doubling done in the FPU (fadd st,st at 0x1000ac04: not an int `h + h`, which adds a 4-byte temp).
// NOT determined (these spellings compile to identical code): 2.0f vs 2.0 vs 2.0f*h; `tmp -= div` vs
// `tmp = tmp - div`. Local names are ours; their declaration order is fixed by the frame offsets.
// v38 = -1200 and v3c = 300 are stored and never read (0x1000abcd / 0x1000abd4). 400/300 = centre of 800x600.
#include <windows.h>
#include <stdlib.h>

class Tile;

class Terrain {
public:
    Tile* tileAt(int x, int y);
    Tile* tileHit(int x, int y);
};

// MATCH: Terrain.dll 0x1000ab30 ?tileHit@Terrain@@QAEPAVTile@@HH@Z
Tile* Terrain::tileHit(int x, int y)
{
    char buf[8];
    int a;
    int b;
    int dy;
    int dx;
    int tmp;
    int div;
    int h;
    float scale;
    int tileY;
    int tileX;
    int v38;
    int v3c;

    OutputDebugString("\nX: ");
    OutputDebugString(itoa(x, buf, 10));
    OutputDebugString("   Y: ");
    OutputDebugString(itoa(y, buf, 10));
    OutputDebugString("\n");

    v38 = -1200;
    v3c = 300;
    h = 32;
    scale = 20.0f;
    dx = x - 400;
    dy = y - 300;
    div = (int)(h * 2.0f * scale);

    tmp = dy * h - (int)(dx * scale) + div / 2;
    if (tmp < 0)
        tmp -= div;
    a = tmp / div;

    tmp = dy * h + (int)(dx * scale) + div / 2;
    if (tmp < 0)
        tmp -= div;
    b = tmp / div;

    tileY = -a + 24;
    tileX = b + 24;

    OutputDebugString("\nTile X: ");
    OutputDebugString(itoa(tileX, buf, 10));
    OutputDebugString("   Tile Y: ");
    OutputDebugString(itoa(tileY, buf, 10));
    OutputDebugString("\n");

    return tileAt(tileY, tileX);
}
