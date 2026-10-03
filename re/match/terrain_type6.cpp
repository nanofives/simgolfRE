// Tile type-6 transitions (Terrain.dll, debug build). 0x10014190 runs when a tile leaves type 6
// (setTypeId): it moves the centre vertex and every vertex shared with a type-6 neighbour or diagonal by
// 20.0 (0x1005f51c), then marks the neighbourhood's normals dirty. One shared corner (own face 0 vertex 0,
// the n0/n2 diagonal case) is changed with += where every other vertex uses -=; kept as compiled.
// 0x100149e0 is the same walk for type 7 with += 13.0 (0x1005f518) on every vertex (lowerCorner assigns
// height - 13.0 to the same vertices of type-7 tiles). The entering functions 0x1000de60 (+= 20.0) and
// 0x1000d540 (-= 13.0; clears +0x20a first and only dirties type-7 neighbours) end with 0x10013500.
// Mesh = the sub-object at Tile +0x44 returned by 0x100153c0 (face count then 8 faces).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX

struct Face {                            // stride 0x38
    int  v[3];
    char pad[0x38 - 12];
};

struct Mesh {                            // Tile +0x44
    int  count;
    Face faces[8];
};

class Tile {
public:
    void  f_10014190();                  // leaving type 6
    void  f_100149e0();                  // leaving type 7
    void  f_1000de60();                  // entering type 6
    void  f_1000d540();                  // entering type 7
    int   f_10001f60();                  // 0x10001f60 (type getter)
    Mesh* mesh();                        // 0x100153c0: returns this + 0x44
    void  f_10013500();                  // 0x10013500 (terrain_tile4.cpp)

    char  m_pad0[0x34];
    Tile* m_n[4];                        // +0x34
    int   m_faceCount;                   // +0x44
    Face  m_faces[8];                    // +0x48
    char  m_pad208[0x20a - 0x208];
    bool  m_20a;                         // +0x20a
    char  m_pad20b[0x244 - 0x20b];
    bool  m_normalsDirty;                // +0x244
};

extern float g_vertices[][3];            // 0x100b28c8

// MATCH: Terrain.dll 0x10014190 ?f_10014190@Tile@@QAEXXZ
void Tile::f_10014190()
{
    g_vertices[m_faces[0].v[2]][1] -= 20.0f;
    if (m_n[0] != 0 && m_n[0]->f_10001f60() == 6) {
        g_vertices[m_faces[1].v[2]][1] -= 20.0f;
        g_vertices[m_n[0]->mesh()->faces[5].v[1]][1] -= 20.0f;
        if (m_n[2] != 0 && m_n[2]->f_10001f60() == 6 && m_n[0]->m_n[2]->f_10001f60() == 6) {
            g_vertices[m_faces[0].v[0]][1] += 20.0f;
            g_vertices[m_n[0]->mesh()->faces[4].v[1]][1] -= 20.0f;
            g_vertices[m_n[2]->mesh()->faces[3].v[2]][1] -= 20.0f;
            g_vertices[m_n[0]->m_n[2]->mesh()->faces[7].v[1]][1] -= 20.0f;
        }
        if (m_n[3] != 0 && m_n[3]->f_10001f60() == 6 && m_n[0]->m_n[3]->f_10001f60() == 6) {
            g_vertices[m_faces[3].v[2]][1] -= 20.0f;
            g_vertices[m_n[0]->mesh()->faces[7].v[1]][1] -= 20.0f;
            g_vertices[m_n[3]->mesh()->faces[0].v[0]][1] -= 20.0f;
            g_vertices[m_n[0]->m_n[3]->mesh()->faces[4].v[1]][1] -= 20.0f;
        }
    }
    if (m_n[1] != 0 && m_n[1]->f_10001f60() == 6) {
        g_vertices[m_faces[5].v[1]][1] -= 20.0f;
        g_vertices[m_n[1]->mesh()->faces[1].v[2]][1] -= 20.0f;
        if (m_n[2] != 0 && m_n[2]->f_10001f60() == 6 && m_n[1]->m_n[2]->f_10001f60() == 6) {
            g_vertices[m_faces[5].v[0]][1] -= 20.0f;
            g_vertices[m_n[1]->mesh()->faces[0].v[0]][1] -= 20.0f;
            g_vertices[m_n[2]->mesh()->faces[7].v[1]][1] -= 20.0f;
            g_vertices[m_n[1]->m_n[2]->mesh()->faces[3].v[2]][1] -= 20.0f;
        }
        if (m_n[3] != 0 && m_n[3]->f_10001f60() == 6 && m_n[1]->m_n[3]->f_10001f60() == 6) {
            g_vertices[m_faces[7].v[1]][1] -= 20.0f;
            g_vertices[m_n[1]->mesh()->faces[3].v[2]][1] -= 20.0f;
            g_vertices[m_n[3]->mesh()->faces[4].v[1]][1] -= 20.0f;
            g_vertices[m_n[1]->m_n[3]->mesh()->faces[0].v[0]][1] -= 20.0f;
        }
    }
    if (m_n[2] != 0 && m_n[2]->f_10001f60() == 6) {
        g_vertices[m_faces[0].v[1]][1] -= 20.0f;
        g_vertices[m_n[2]->mesh()->faces[3].v[1]][1] -= 20.0f;
    }
    if (m_n[3] != 0 && m_n[3]->f_10001f60() == 6) {
        g_vertices[m_faces[3].v[1]][1] -= 20.0f;
        g_vertices[m_n[3]->mesh()->faces[0].v[1]][1] -= 20.0f;
    }
    m_normalsDirty = true;
    if (m_n[0] != 0)
        m_n[0]->m_normalsDirty = true;
    if (m_n[0] != 0 && m_n[2] != 0)
        m_n[0]->m_n[2]->m_normalsDirty = true;
    if (m_n[0] != 0 && m_n[3] != 0)
        m_n[0]->m_n[3]->m_normalsDirty = true;
    if (m_n[2] != 0)
        m_n[2]->m_normalsDirty = true;
    if (m_n[3] != 0)
        m_n[3]->m_normalsDirty = true;
    if (m_n[1] != 0)
        m_n[1]->m_normalsDirty = true;
    if (m_n[1] != 0 && m_n[2] != 0)
        m_n[1]->m_n[2]->m_normalsDirty = true;
    if (m_n[1] != 0 && m_n[3] != 0)
        m_n[1]->m_n[3]->m_normalsDirty = true;
}

// MATCH: Terrain.dll 0x100149e0 ?f_100149e0@Tile@@QAEXXZ
void Tile::f_100149e0()
{
    g_vertices[m_faces[0].v[2]][1] += 13.0f;
    if (m_n[0] != 0 && m_n[0]->f_10001f60() == 7) {
        g_vertices[m_faces[1].v[2]][1] += 13.0f;
        g_vertices[m_n[0]->mesh()->faces[5].v[1]][1] += 13.0f;
        if (m_n[2] != 0 && m_n[2]->f_10001f60() == 7 && m_n[0]->m_n[2]->f_10001f60() == 7) {
            g_vertices[m_faces[0].v[0]][1] += 13.0f;
            g_vertices[m_n[0]->mesh()->faces[4].v[1]][1] += 13.0f;
            g_vertices[m_n[2]->mesh()->faces[3].v[2]][1] += 13.0f;
            g_vertices[m_n[0]->m_n[2]->mesh()->faces[7].v[1]][1] += 13.0f;
        }
        if (m_n[3] != 0 && m_n[3]->f_10001f60() == 7 && m_n[0]->m_n[3]->f_10001f60() == 7) {
            g_vertices[m_faces[3].v[2]][1] += 13.0f;
            g_vertices[m_n[0]->mesh()->faces[7].v[1]][1] += 13.0f;
            g_vertices[m_n[3]->mesh()->faces[0].v[0]][1] += 13.0f;
            g_vertices[m_n[0]->m_n[3]->mesh()->faces[4].v[1]][1] += 13.0f;
        }
    }
    if (m_n[1] != 0 && m_n[1]->f_10001f60() == 7) {
        g_vertices[m_faces[5].v[1]][1] += 13.0f;
        g_vertices[m_n[1]->mesh()->faces[1].v[2]][1] += 13.0f;
        if (m_n[2] != 0 && m_n[2]->f_10001f60() == 7 && m_n[1]->m_n[2]->f_10001f60() == 7) {
            g_vertices[m_faces[5].v[0]][1] += 13.0f;
            g_vertices[m_n[1]->mesh()->faces[0].v[0]][1] += 13.0f;
            g_vertices[m_n[2]->mesh()->faces[7].v[1]][1] += 13.0f;
            g_vertices[m_n[1]->m_n[2]->mesh()->faces[3].v[2]][1] += 13.0f;
        }
        if (m_n[3] != 0 && m_n[3]->f_10001f60() == 7 && m_n[1]->m_n[3]->f_10001f60() == 7) {
            g_vertices[m_faces[7].v[1]][1] += 13.0f;
            g_vertices[m_n[1]->mesh()->faces[3].v[2]][1] += 13.0f;
            g_vertices[m_n[3]->mesh()->faces[4].v[1]][1] += 13.0f;
            g_vertices[m_n[1]->m_n[3]->mesh()->faces[0].v[0]][1] += 13.0f;
        }
    }
    if (m_n[2] != 0 && m_n[2]->f_10001f60() == 7) {
        g_vertices[m_faces[0].v[1]][1] += 13.0f;
        g_vertices[m_n[2]->mesh()->faces[3].v[1]][1] += 13.0f;
    }
    if (m_n[3] != 0 && m_n[3]->f_10001f60() == 7) {
        g_vertices[m_faces[3].v[1]][1] += 13.0f;
        g_vertices[m_n[3]->mesh()->faces[0].v[1]][1] += 13.0f;
    }
    m_normalsDirty = true;
    if (m_n[0] != 0)
        m_n[0]->m_normalsDirty = true;
    if (m_n[0] != 0 && m_n[2] != 0)
        m_n[0]->m_n[2]->m_normalsDirty = true;
    if (m_n[0] != 0 && m_n[3] != 0)
        m_n[0]->m_n[3]->m_normalsDirty = true;
    if (m_n[2] != 0)
        m_n[2]->m_normalsDirty = true;
    if (m_n[3] != 0)
        m_n[3]->m_normalsDirty = true;
    if (m_n[1] != 0)
        m_n[1]->m_normalsDirty = true;
    if (m_n[1] != 0 && m_n[2] != 0)
        m_n[1]->m_n[2]->m_normalsDirty = true;
    if (m_n[1] != 0 && m_n[3] != 0)
        m_n[1]->m_n[3]->m_normalsDirty = true;
}

// MATCH: Terrain.dll 0x1000de60 ?f_1000de60@Tile@@QAEXXZ
void Tile::f_1000de60()
{
    g_vertices[m_faces[0].v[2]][1] += 20.0f;
    if (m_n[0] != 0 && m_n[0]->f_10001f60() == 6) {
        g_vertices[m_faces[1].v[2]][1] += 20.0f;
        g_vertices[m_n[0]->mesh()->faces[5].v[1]][1] += 20.0f;
        if (m_n[2] != 0 && m_n[2]->f_10001f60() == 6 && m_n[0]->m_n[2]->f_10001f60() == 6) {
            g_vertices[m_faces[0].v[0]][1] += 20.0f;
            g_vertices[m_n[0]->mesh()->faces[4].v[1]][1] += 20.0f;
            g_vertices[m_n[2]->mesh()->faces[3].v[2]][1] += 20.0f;
            g_vertices[m_n[0]->m_n[2]->mesh()->faces[7].v[1]][1] += 20.0f;
        }
        if (m_n[3] != 0 && m_n[3]->f_10001f60() == 6 && m_n[0]->m_n[3]->f_10001f60() == 6) {
            g_vertices[m_faces[3].v[2]][1] += 20.0f;
            g_vertices[m_n[0]->mesh()->faces[7].v[1]][1] += 20.0f;
            g_vertices[m_n[3]->mesh()->faces[0].v[0]][1] += 20.0f;
            g_vertices[m_n[0]->m_n[3]->mesh()->faces[4].v[1]][1] += 20.0f;
        }
    }
    if (m_n[1] != 0 && m_n[1]->f_10001f60() == 6) {
        g_vertices[m_faces[5].v[1]][1] += 20.0f;
        g_vertices[m_n[1]->mesh()->faces[1].v[2]][1] += 20.0f;
        if (m_n[2] != 0 && m_n[2]->f_10001f60() == 6 && m_n[1]->m_n[2]->f_10001f60() == 6) {
            g_vertices[m_faces[5].v[0]][1] += 20.0f;
            g_vertices[m_n[1]->mesh()->faces[0].v[0]][1] += 20.0f;
            g_vertices[m_n[2]->mesh()->faces[7].v[1]][1] += 20.0f;
            g_vertices[m_n[1]->m_n[2]->mesh()->faces[3].v[2]][1] += 20.0f;
        }
        if (m_n[3] != 0 && m_n[3]->f_10001f60() == 6 && m_n[1]->m_n[3]->f_10001f60() == 6) {
            g_vertices[m_faces[7].v[1]][1] += 20.0f;
            g_vertices[m_n[1]->mesh()->faces[3].v[2]][1] += 20.0f;
            g_vertices[m_n[3]->mesh()->faces[4].v[1]][1] += 20.0f;
            g_vertices[m_n[1]->m_n[3]->mesh()->faces[0].v[0]][1] += 20.0f;
        }
    }
    if (m_n[2] != 0 && m_n[2]->f_10001f60() == 6) {
        g_vertices[m_faces[0].v[1]][1] += 20.0f;
        g_vertices[m_n[2]->mesh()->faces[3].v[1]][1] += 20.0f;
    }
    if (m_n[3] != 0 && m_n[3]->f_10001f60() == 6) {
        g_vertices[m_faces[3].v[1]][1] += 20.0f;
        g_vertices[m_n[3]->mesh()->faces[0].v[1]][1] += 20.0f;
    }
    m_normalsDirty = true;
    if (m_n[0] != 0)
        m_n[0]->m_normalsDirty = true;
    if (m_n[0] != 0 && m_n[2] != 0)
        m_n[0]->m_n[2]->m_normalsDirty = true;
    if (m_n[0] != 0 && m_n[3] != 0)
        m_n[0]->m_n[3]->m_normalsDirty = true;
    if (m_n[2] != 0)
        m_n[2]->m_normalsDirty = true;
    if (m_n[3] != 0)
        m_n[3]->m_normalsDirty = true;
    if (m_n[1] != 0)
        m_n[1]->m_normalsDirty = true;
    if (m_n[1] != 0 && m_n[2] != 0)
        m_n[1]->m_n[2]->m_normalsDirty = true;
    if (m_n[1] != 0 && m_n[3] != 0)
        m_n[1]->m_n[3]->m_normalsDirty = true;
    f_10013500();
}

// MATCH: Terrain.dll 0x1000d540 ?f_1000d540@Tile@@QAEXXZ
void Tile::f_1000d540()
{
    m_20a = false;
    g_vertices[m_faces[0].v[2]][1] -= 13.0f;
    if (m_n[0] != 0 && m_n[0]->f_10001f60() == 7) {
        g_vertices[m_faces[1].v[2]][1] -= 13.0f;
        g_vertices[m_n[0]->mesh()->faces[5].v[1]][1] -= 13.0f;
        if (m_n[2] != 0 && m_n[2]->f_10001f60() == 7 && m_n[0]->m_n[2]->f_10001f60() == 7) {
            g_vertices[m_faces[0].v[0]][1] -= 13.0f;
            g_vertices[m_n[0]->mesh()->faces[4].v[1]][1] -= 13.0f;
            g_vertices[m_n[2]->mesh()->faces[3].v[2]][1] -= 13.0f;
            g_vertices[m_n[0]->m_n[2]->mesh()->faces[7].v[1]][1] -= 13.0f;
        }
        if (m_n[3] != 0 && m_n[3]->f_10001f60() == 7 && m_n[0]->m_n[3]->f_10001f60() == 7) {
            g_vertices[m_faces[3].v[2]][1] -= 13.0f;
            g_vertices[m_n[0]->mesh()->faces[7].v[1]][1] -= 13.0f;
            g_vertices[m_n[3]->mesh()->faces[0].v[0]][1] -= 13.0f;
            g_vertices[m_n[0]->m_n[3]->mesh()->faces[4].v[1]][1] -= 13.0f;
        }
    }
    if (m_n[1] != 0 && m_n[1]->f_10001f60() == 7) {
        g_vertices[m_faces[5].v[1]][1] -= 13.0f;
        g_vertices[m_n[1]->mesh()->faces[1].v[2]][1] -= 13.0f;
        if (m_n[2] != 0 && m_n[2]->f_10001f60() == 7 && m_n[1]->m_n[2]->f_10001f60() == 7) {
            g_vertices[m_faces[5].v[0]][1] -= 13.0f;
            g_vertices[m_n[1]->mesh()->faces[0].v[0]][1] -= 13.0f;
            g_vertices[m_n[2]->mesh()->faces[7].v[1]][1] -= 13.0f;
            g_vertices[m_n[1]->m_n[2]->mesh()->faces[3].v[2]][1] -= 13.0f;
        }
        if (m_n[3] != 0 && m_n[3]->f_10001f60() == 7 && m_n[1]->m_n[3]->f_10001f60() == 7) {
            g_vertices[m_faces[7].v[1]][1] -= 13.0f;
            g_vertices[m_n[1]->mesh()->faces[3].v[2]][1] -= 13.0f;
            g_vertices[m_n[3]->mesh()->faces[4].v[1]][1] -= 13.0f;
            g_vertices[m_n[1]->m_n[3]->mesh()->faces[0].v[0]][1] -= 13.0f;
        }
    }
    if (m_n[2] != 0 && m_n[2]->f_10001f60() == 7) {
        g_vertices[m_faces[0].v[1]][1] -= 13.0f;
        g_vertices[m_n[2]->mesh()->faces[3].v[1]][1] -= 13.0f;
    }
    if (m_n[3] != 0 && m_n[3]->f_10001f60() == 7) {
        g_vertices[m_faces[3].v[1]][1] -= 13.0f;
        g_vertices[m_n[3]->mesh()->faces[0].v[1]][1] -= 13.0f;
    }
    m_normalsDirty = true;
    if (m_n[0] != 0 && m_n[0]->f_10001f60() == 7)
        m_n[0]->m_normalsDirty = true;
    if (m_n[0] != 0 && m_n[2] != 0 && m_n[0]->m_n[2]->f_10001f60() == 7)
        m_n[0]->m_n[2]->m_normalsDirty = true;
    if (m_n[0] != 0 && m_n[3] != 0 && m_n[0]->m_n[3]->f_10001f60() == 7)
        m_n[0]->m_n[3]->m_normalsDirty = true;
    if (m_n[2] != 0 && m_n[2]->f_10001f60() == 7)
        m_n[2]->m_normalsDirty = true;
    if (m_n[3] != 0 && m_n[3]->f_10001f60() == 7)
        m_n[3]->m_normalsDirty = true;
    if (m_n[1] != 0 && m_n[1]->f_10001f60() == 7)
        m_n[1]->m_normalsDirty = true;
    if (m_n[1] != 0 && m_n[2] != 0 && m_n[1]->m_n[2]->f_10001f60() == 7)
        m_n[1]->m_n[2]->m_normalsDirty = true;
    if (m_n[1] != 0 && m_n[3] != 0 && m_n[1]->m_n[3]->f_10001f60() == 7)
        m_n[1]->m_n[3]->m_normalsDirty = true;
    f_10013500();
}
