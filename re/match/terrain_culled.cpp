// Terrain::isCulled (Terrain.dll, debug build): screen-space visibility test of tile `t` relative to the
// view centre tile for one of four camera angles. Never culls when the zoom shift m_1c is 2 (0x10006870).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
//
// Tile footprint constants per window size (800x600 / 1024x768 / 1280x1024): A 0x40/0x50/0x68,
// B 0x28/0x34/0x44, C 0x20/0x28/0x34, D 0x14/0x1a/0x22. Angle literals are doubles (0x1005f368 = 0.0,
// 0x1005f358 = 90.0, 0x1005f348 = 180.0). Each branch returns its own || chain (4 bool temps).
// Any other window size leaves A..D uninitialised (no else branch in the original).

class Tile {
public:
    int getY();                          // 0x10006810
    int getX();                          // 0x10005960
    int maxHeight();                     // 0x10015500
};

class Terrain {
public:
    bool isCulled(Tile* t, Tile* center, float angle);

    char  m_pad0[0x1c];
    int   m_1c;                          // +0x1c zoom shift
    int   m_width;                       // +0x20 window width (pixels)
    int   m_height;                      // +0x24 window height
};

// MATCH: Terrain.dll 0x10006850 ?isCulled@Terrain@@QAE_NPAVTile@@0M@Z
bool Terrain::isCulled(Tile* t, Tile* center, float angle)
{
    int sx, sy, halfW, halfH, dy, dx, A, C, B, D;
    if (m_1c == 2)
        return false;
    halfW = (m_width >> 1) * m_1c;
    halfH = (m_height >> 1) * m_1c;
    dy = t->getY() - center->getY();
    dx = t->getX() - center->getX();
    if (m_width == 800 && m_height == 600) {
        A = 0x40;
        B = 0x28;
        C = 0x20;
        D = 0x14;
    } else if (m_width == 1024 && m_height == 768) {
        A = 0x50;
        B = 0x34;
        C = 0x28;
        D = 0x1a;
    } else if (m_width == 1280 && m_height == 1024) {
        A = 0x68;
        B = 0x44;
        C = 0x34;
        D = 0x22;
    }
    if (angle == 0.0) {
        sx = (m_width >> 1) + dy * C + dx * C;
        sy = (m_height >> 1) + dy * D - dx * D;
        sy -= t->maxHeight() * 5;
        return sx > m_width + halfW || sx < -A - halfW || sy > m_height + B + halfH || sy < -B - halfH;
    } else if (angle == 90.0) {
        sx = (m_width >> 1) - dy * C + dx * C;
        sy = (m_height >> 1) - dy * D - dx * D;
        sy -= t->maxHeight() * 5;
        return sx > m_width + C + halfW || sx < -A - halfW || sy > m_height + B + halfH || sy < -B - halfH;
    } else if (angle == 180.0) {
        sx = (m_width >> 1) - dy * C - dx * C;
        sy = (m_height >> 1) - dy * D + dx * D;
        sy -= t->maxHeight() * 5;
        return sx > m_width + A + halfW || sx < -C - halfW || sy > m_height + B + halfH || sy < -B - halfH;
    } else {
        sx = (m_width >> 1) + dy * C - dx * C;
        sy = (m_height >> 1) + dy * D + dx * D;
        sy -= t->maxHeight() * 5;
        return sx > m_width + C + halfW || sx < -C - halfW || sy > m_height + D + halfH || sy < -B - D - halfH;
    }
}
