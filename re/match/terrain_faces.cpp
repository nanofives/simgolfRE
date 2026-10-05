// Faces::build (Terrain.dll, debug build): the eight triangles of a tile (Tile +0x44), two per quarter of its 3x3
// vertex patch, with texture coordinates from texCoord() (0x10002010) and an up normal; then the faces that
// share an edge copy each other's vertex and coordinates.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd

int roundHalf(float x);                          // 0x10002010 (terrain_small1.cpp): texture coordinate index

struct Face {                                    // stride 0x38
    int   v[3];                                  // +0x00 vertex indices
    int   tc[6];                                 // +0x0c
    char  pad24[0x2c - 0x24];
    float normal[3];                             // +0x2c
};

struct Faces {                                   // Tile +0x44
    int  count;
    Face f[8];
    void build(int x, int y, int w);
};

// MATCH: Terrain.dll 0x10002060 ?build@Faces@@QAEXHHH@Z
void Faces::build(int x, int y, int w)
{
    int c = 0;
    int row = w * 3;
    int yoff = y * row * 3;
    int xoff = x * 3;
    count = 8;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            f[c].v[0] = j + i * row + yoff + xoff;
            f[c].v[1] = j + (i + 1) * row + yoff + xoff;
            f[c].v[2] = j + 1 + (i + 1) * row + yoff + xoff;
            f[c].tc[0] = roundHalf(j / 2.0f);
            f[c].tc[1] = roundHalf(1.0f - i / 2.0f);
            f[c].tc[2] = roundHalf(j / 2.0f);
            f[c].tc[3] = roundHalf(1.0f - (i + 1) / 2.0f);
            f[c].tc[4] = roundHalf((j + 1) / 2.0f);
            f[c].tc[5] = roundHalf(1.0f - (i + 1) / 2.0f);
            f[c].normal[0] = 0.0f;
            f[c].normal[1] = 1.0f;
            f[c].normal[2] = 0.0f;
            c++;
            f[c].v[0] = j + i * row + yoff + xoff;
            f[c].v[1] = j + 1 + (i + 1) * row + yoff + xoff;
            f[c].v[2] = j + 1 + i * row + yoff + xoff;
            f[c].tc[0] = roundHalf(j / 2.0f);
            f[c].tc[1] = roundHalf(1.0f - i / 2.0f);
            f[c].tc[2] = roundHalf((j + 1) / 2.0f);
            f[c].tc[3] = roundHalf(1.0f - (i + 1) / 2.0f);
            f[c].tc[4] = roundHalf((j + 1) / 2.0f);
            f[c].tc[5] = roundHalf(1.0f - i / 2.0f);
            f[c].normal[0] = 0.0f;
            f[c].normal[1] = 1.0f;
            f[c].normal[2] = 0.0f;
            c++;
        }
    }
    f[2].v[2] = f[3].v[2];
    f[2].tc[4] = f[3].tc[4];
    f[2].tc[5] = f[3].tc[5];
    f[3].v[0] = f[2].v[1];
    f[3].tc[0] = f[2].tc[2];
    f[3].tc[1] = f[2].tc[3];
    f[4].v[2] = f[5].v[2];
    f[4].tc[4] = f[5].tc[4];
    f[4].tc[5] = f[5].tc[5];
    f[5].v[0] = f[4].v[1];
    f[5].tc[0] = f[4].tc[2];
    f[5].tc[1] = f[4].tc[3];
}
