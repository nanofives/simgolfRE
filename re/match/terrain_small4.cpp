// Terrain.dll functions, batch 4 (debug build): vertex grid initialisation and the lowest corner of a tile.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#include <stdlib.h>

extern float g_vertices[][3];                    // 0x100b28c8
extern float g_normals[][3];                      // 0x10070a18
extern float g_tileW;                            // 0x10063e4c
extern float g_tileH;                            // 0x10063e50

// Lays out a w x h grid of tiles, 3x3 vertices each, centred on the origin, height 0, normal (0, 1, 0).
// MATCH: Terrain.dll 0x1000c560 ?initGrid@@YAXHH@Z
void initGrid(int w, int h)
{
    int n = w * h;
    int row = w * 3;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int base = x * 3 + y * row * 3;
            float ox = x * g_tileW - w * g_tileW / 2.0f;
            float oz = y * g_tileH - h * g_tileH / 2.0f;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    g_vertices[base + i + j * row][0] = i * (g_tileW / 2.0f) + ox;
                    g_vertices[base + i + j * row][2] = j * (g_tileH / 2.0f) + oz;
                    g_vertices[base + i + j * row][1] = 0.0f;
                    g_normals[base + i + j * row][0] = 0.0f;
                    g_normals[base + i + j * row][1] = 1.0f;
                    g_normals[base + i + j * row][2] = 0.0f;
                }
            }
        }
    }
}

class Tile {
public:
    float minHeight();
    int m_v[0x65];
};
// Lowest height among the tile's vertices at +0x04, +0x44, +0x7c, +0xb0, +0x190, +0x158, +0xe8, +0x08. Written as
// one ternary per step: the same steps through stdlib's __min macro compile to FPU copies (61.6%), the plain
// ternary to the original's integer copies.
// MATCH: Terrain.dll 0x10002900 ?minHeight@Tile@@QAEMXZ
float Tile::minHeight()
{
    float m = g_vertices[m_v[1]][1];
    m = (g_vertices[m_v[0x11]][1] < m) ? g_vertices[m_v[0x11]][1] : m;
    m = (g_vertices[m_v[0x1f]][1] < m) ? g_vertices[m_v[0x1f]][1] : m;
    m = (g_vertices[m_v[0x2c]][1] < m) ? g_vertices[m_v[0x2c]][1] : m;
    m = (g_vertices[m_v[0x64]][1] < m) ? g_vertices[m_v[0x64]][1] : m;
    m = (g_vertices[m_v[0x56]][1] < m) ? g_vertices[m_v[0x56]][1] : m;
    m = (g_vertices[m_v[0x3a]][1] < m) ? g_vertices[m_v[0x3a]][1] : m;
    m = (g_vertices[m_v[2]][1] < m) ? g_vertices[m_v[2]][1] : m;
    return m;
}
