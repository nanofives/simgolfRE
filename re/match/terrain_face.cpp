// Tile::setFaceTexture (Terrain.dll, debug build): picks the texture of one of the eight faces from the
// texture table (g_textures[type][variation][piece], terrain_ctor.cpp), with per-terrain overrides of `type`.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd

extern unsigned int g_textures[0x25][0x19][9];  // 0x100687f8

struct Face {                                    // stride 0x38, Tile +0x48
    char         pad0[0x24];
    unsigned int texture;                        // +0x24
    char         piece;                          // +0x28
    char         pad29[0x38 - 0x29];
};

class Tile {
public:
    void setFaceTexture(int type, int face, int piece);
    char  m_pad0[0x24];
    int   m_type;                                // +0x24
    int   m_28;                                  // +0x28
    char  m_pad2c[0x48 - 0x2c];
    Face  m_faces[8];                            // +0x48
    char  m_pad208[0x240 - 0x208];
    int   m_variation;                           // +0x240
};

// Face argument -> face slot: 0->2, 1->3, 2->7, 3->6, 4->5, 5->4, 6->0, 7->1.
// MATCH: Terrain.dll 0x10012ec0 ?setFaceTexture@Tile@@QAEXHHH@Z
void Tile::setFaceTexture(int type, int face, int piece)
{
    switch (m_type) {
    case 0xd:
    case 0xe:
    case 0xf:
    case 0x10:
        type = 0xd;
        break;
    case 0x11:
        if (m_28 == 1)
            type = 0x17;
        else if (m_28 == 2)
            type = 0x18;
        break;
    case 6:
    case 0x15:
        type = 4;
        break;
    case 0x16:
        if (m_28 >= 0x40 && m_28 < 0x48)
            type = 4;
        break;
    case 1:
        if (m_28 & 0x80)
            type = 0x1a;
        break;
    case 7:
        switch (m_28 % 4) {
        case 0: type = 0x1b; break;
        case 1: type = 0x1c; break;
        case 2: type = 0x1d; break;
        case 3: type = 0x1e; break;
        }
        break;
    }
    if ((type == 4 || m_type == 7 || type == 2) && piece > 4)
        piece = 0;
    switch (face) {
    case 7:
        m_faces[1].texture = g_textures[type][m_variation][piece];
        m_faces[1].piece = piece;
        break;
    case 0:
        m_faces[2].texture = g_textures[type][m_variation][piece];
        m_faces[2].piece = piece;
        break;
    case 1:
        m_faces[3].texture = g_textures[type][m_variation][piece];
        m_faces[3].piece = piece;
        break;
    case 2:
        m_faces[7].texture = g_textures[type][m_variation][piece];
        m_faces[7].piece = piece;
        break;
    case 3:
        m_faces[6].texture = g_textures[type][m_variation][piece];
        m_faces[6].piece = piece;
        break;
    case 4:
        m_faces[5].texture = g_textures[type][m_variation][piece];
        m_faces[5].piece = piece;
        break;
    case 5:
        m_faces[4].texture = g_textures[type][m_variation][piece];
        m_faces[4].piece = piece;
        break;
    case 6:
        m_faces[0].texture = g_textures[type][m_variation][piece];
        m_faces[0].piece = piece;
        break;
    }
}
