// Tile::elevateCorner (Terrain.dll, debug build): raises one corner (1/3/5/7) by one step of 15.0
// (0x10063e54), sets the corner vertex's height, the two edge midpoints to the mean with their far ends
// (* 0.5, 0x1005f3e0), and the tile centre to max(itself, mean with one neighbour vertex); then marks this
// tile and its neighbours' normals dirty (+0x244).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
// Switch cases are laid out 1, 7, 3, 5 (source order).

struct Face {                            // stride 0x38, Tile +0x48
    int  v[3];
    char pad[0x38 - 12];
};

class Tile {
public:
    void elevateCorner(int corner);      // 0x1000c7b0
    void lowerCorner(int corner);        // 0x1000ccc0 (type-7 tiles: centre and same-type neighbour vertices set 13.0 below, 0x1005f518)
    int  f_10001f60();                   // 0x10001f60 (type getter; compared with 7)

    int   m_corner[8];                   // +0x00 (odd indices are the corner heights in steps)
    char  m_pad20[0x24 - 0x20];
    int   m_type;                        // +0x24
    char  m_pad28[0x34 - 0x28];
    Tile* m_n[4];                        // +0x34
    int   m_faceCount;                   // +0x44
    Face  m_faces[8];                    // +0x48 (m_faces[0].v[2] at +0x50 = centre vertex)
    char  m_pad208[0x244 - 0x208];
    bool  m_normalsDirty;                // +0x244
};

extern float g_vertices[][3];            // 0x100b28c8 (Tile::vertexArray); [v][1] = height

// MATCH: Terrain.dll 0x1000c7b0 ?elevateCorner@Tile@@QAEXH@Z
void Tile::elevateCorner(int corner)
{
    int f0, v0, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5;
    switch (corner) {
    case 1:
        f5 = 3; v5 = 2; f0 = 4; v0 = 1; f1 = 3; v1 = 1; f2 = 1; v2 = 2; f3 = 7; v3 = 1; f4 = 0; v4 = 0;
        break;
    case 7:
        f5 = 0; v5 = 0; f0 = 7; v0 = 1; f1 = 1; v1 = 2; f2 = 0; v2 = 1; f3 = 3; v3 = 2; f4 = 4; v4 = 1;
        break;
    case 3:
        f5 = 7; v5 = 1; f0 = 0; v0 = 0; f1 = 5; v1 = 1; f2 = 3; v2 = 1; f3 = 4; v3 = 1; f4 = 3; v4 = 2;
        break;
    case 5:
        f5 = 4; v5 = 1; f0 = 3; v0 = 2; f1 = 0; v1 = 1; f2 = 5; v2 = 1; f3 = 0; v3 = 0; f4 = 7; v4 = 1;
        break;
    }
    float newH = (m_corner[corner] + 1) * 15.0f;
    float a = g_vertices[m_faces[f0].v[v0]][1];
    float b = g_vertices[m_faces[f3].v[v3]][1];
    float c = g_vertices[m_faces[f4].v[v4]][1];
    float h = newH;
    float m = (h + a) * 0.5f;
    g_vertices[m_faces[0].v[2]][1] = m > g_vertices[m_faces[0].v[2]][1] ? m : g_vertices[m_faces[0].v[2]][1];
    g_vertices[m_faces[f1].v[v1]][1] = (h + b) * 0.5f;
    g_vertices[m_faces[f2].v[v2]][1] = (h + c) * 0.5f;
    m_corner[corner]++;
    g_vertices[m_faces[f5].v[v5]][1] = newH;
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

// MATCH: Terrain.dll 0x1000ccc0 ?lowerCorner@Tile@@QAEXH@Z
void Tile::lowerCorner(int corner)
{
    int f0, v0, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5;
    switch (corner) {
    case 1:
        f5 = 3; v5 = 2; f0 = 4; v0 = 1; f1 = 3; v1 = 1; f2 = 1; v2 = 2; f3 = 7; v3 = 1; f4 = 0; v4 = 0;
        break;
    case 7:
        f5 = 0; v5 = 0; f0 = 7; v0 = 1; f1 = 1; v1 = 2; f2 = 0; v2 = 1; f3 = 3; v3 = 2; f4 = 4; v4 = 1;
        break;
    case 3:
        f5 = 7; v5 = 1; f0 = 0; v0 = 0; f1 = 5; v1 = 1; f2 = 3; v2 = 1; f3 = 4; v3 = 1; f4 = 3; v4 = 2;
        break;
    case 5:
        f5 = 4; v5 = 1; f0 = 3; v0 = 2; f1 = 0; v1 = 1; f2 = 5; v2 = 1; f3 = 0; v3 = 0; f4 = 7; v4 = 1;
        break;
    }
    float newH = (m_corner[corner] - 1) * 15.0f;
    float a = g_vertices[m_faces[f0].v[v0]][1];
    float b = g_vertices[m_faces[f3].v[v3]][1];
    float c = g_vertices[m_faces[f4].v[v4]][1];
    float h = newH;
    float m = (h + a) * 0.5f;
    g_vertices[m_faces[0].v[2]][1] = m < g_vertices[m_faces[0].v[2]][1] ? m : g_vertices[m_faces[0].v[2]][1];
    g_vertices[m_faces[f1].v[v1]][1] = (h + b) * 0.5f;
    g_vertices[m_faces[f2].v[v2]][1] = (h + c) * 0.5f;
    m_corner[corner]--;
    g_vertices[m_faces[f5].v[v5]][1] = newH;
    if (m_type == 7) {
        g_vertices[m_faces[0].v[2]][1] = newH - 13.0f;
        if (m_n[0] != 0 && m_n[0]->f_10001f60() == 7) {
            g_vertices[m_faces[1].v[2]][1] = newH - 13.0f;
            if (m_n[2] != 0 && m_n[2]->f_10001f60() == 7 && m_n[0]->m_n[2]->f_10001f60() == 7)
                g_vertices[m_faces[0].v[0]][1] = newH - 13.0f;
            if (m_n[3] != 0 && m_n[3]->f_10001f60() == 7 && m_n[0]->m_n[3]->f_10001f60() == 7)
                g_vertices[m_faces[3].v[2]][1] = newH - 13.0f;
        }
        if (m_n[1] != 0 && m_n[1]->f_10001f60() == 7) {
            g_vertices[m_faces[5].v[1]][1] = newH - 13.0f;
            if (m_n[2] != 0 && m_n[2]->f_10001f60() == 7 && m_n[1]->m_n[2]->f_10001f60() == 7)
                g_vertices[m_faces[5].v[0]][1] = newH - 13.0f;
            if (m_n[3] != 0 && m_n[3]->f_10001f60() == 7 && m_n[1]->m_n[3]->f_10001f60() == 7)
                g_vertices[m_faces[7].v[1]][1] = newH - 13.0f;
        }
        if (m_n[2] != 0 && m_n[2]->f_10001f60() == 7)
            g_vertices[m_faces[0].v[1]][1] = newH - 13.0f;
        if (m_n[3] != 0 && m_n[3]->f_10001f60() == 7)
            g_vertices[m_faces[3].v[1]][1] = newH - 13.0f;
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
