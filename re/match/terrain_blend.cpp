// Tile::f_10013670(int type) (Terrain.dll, debug build): recomputes the 8 face textures of a tile from the
// types of its 4 neighbours and 4 diagonals (type -1 = the tile's own type; 0x14 = hidden, skipped).
// On course type 1 (0x10070a0c) with tile type 0x11, a face next to a neighbour for which 0x100155b0 is true
// is set with type 0x19 instead and the type restored to 0x11 afterwards. Four flags (b4..b7, the diagonals)
// are initialised and set but never read. Names of the helpers are unknown (by address).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX

class Tile {
public:
    void f_10013670(int type);
    int  f_10015650(Tile* n);            // 0x10015650 (neighbour value used for blending)
    bool f_100155b0();                   // 0x100155b0
    void f_10012ec0(int type, int face, int value);  // 0x10012ec0
    bool f_100154a0(int type);           // 0x100154a0
    int  f_10013f00(int a, int b, int c);            // 0x10013f00
    int  f_10013dd0(int code);           // 0x10013dd0

    char  m_pad0[0x24];
    int   m_type;                        // +0x24
    char  m_pad28[0x34 - 0x28];
    Tile* m_n[4];                        // +0x34
};

extern int g_courseType;                 // 0x10070a0c

// MATCH: Terrain.dll 0x10013670 ?f_10013670@Tile@@QAEXH@Z
void Tile::f_10013670(int type)
{
    int code = 0, value = 0;
    bool b0 = false, b1 = false, b2 = false, b3 = false, b4 = false, b5 = false, b6 = false, b7 = false;
    int t0, t1, t2, t3, d0, d1, d2, d3;
    if (type == -1)
        type = m_type;
    if (type == 0x14)
        return;
    t0 = f_10015650(m_n[0]);
    t1 = f_10015650(m_n[2]);
    t2 = f_10015650(m_n[3]);
    t3 = f_10015650(m_n[1]);
    d0 = 0;
    d1 = 0;
    d2 = 0;
    d3 = 0;
    if (!f_100154a0(type))
        return;
    if (m_n[0] != 0) {
        if (g_courseType == 1 && type == 0x11 && m_n[0]->f_100155b0())
            b0 = true;
        if (m_n[2] != 0) {
            if (g_courseType == 1 && type == 0x11 && m_n[0]->m_n[2]->f_100155b0())
                b4 = true;
            d0 = f_10015650(m_n[0]->m_n[2]);
        }
        if (m_n[3] != 0) {
            if (g_courseType == 1 && type == 0x11 && m_n[0]->m_n[3]->f_100155b0())
                b5 = true;
            d1 = f_10015650(m_n[0]->m_n[3]);
        }
    }
    if (m_n[1] != 0) {
        if (g_courseType == 1 && type == 0x11 && m_n[1]->f_100155b0())
            b1 = true;
        if (m_n[2] != 0) {
            if (g_courseType == 1 && type == 0x11 && m_n[1]->m_n[2]->f_100155b0())
                b6 = true;
            d2 = f_10015650(m_n[1]->m_n[2]);
        }
        if (m_n[3] != 0) {
            if (g_courseType == 1 && type == 0x11 && m_n[1]->m_n[3]->f_100155b0())
                b7 = true;
            d3 = f_10015650(m_n[1]->m_n[3]);
        }
    }
    if (m_n[2] != 0 && g_courseType == 1 && type == 0x11 && m_n[2]->f_100155b0())
        b2 = true;
    if (m_n[3] != 0 && g_courseType == 1 && type == 0x11 && m_n[3]->f_100155b0())
        b3 = true;

    code = f_10013f00(t0, t1, d0);
    value = f_10013dd0(code);
    if (b0) type = 0x19;
    f_10012ec0(type, 7, value);
    if (b0) type = 0x11;
    code = f_10013f00(t0, t2, d1);
    value = f_10013dd0(code);
    if (b0) type = 0x19;
    f_10012ec0(type, 0, value);
    if (b0) type = 0x11;
    code = f_10013f00(t2, t0, d1);
    value = f_10013dd0(code);
    if (b3) type = 0x19;
    f_10012ec0(type, 1, value);
    if (b3) type = 0x11;
    code = f_10013f00(t2, t3, d3);
    value = f_10013dd0(code);
    if (b3) type = 0x19;
    f_10012ec0(type, 2, value);
    if (b3) type = 0x11;
    code = f_10013f00(t3, t2, d3);
    value = f_10013dd0(code);
    if (b1) type = 0x19;
    f_10012ec0(type, 3, value);
    if (b1) type = 0x11;
    code = f_10013f00(t3, t1, d2);
    value = f_10013dd0(code);
    if (b1) type = 0x19;
    f_10012ec0(type, 4, value);
    if (b1) type = 0x11;
    code = f_10013f00(t1, t0, d0);
    value = f_10013dd0(code);
    if (b2) type = 0x19;
    f_10012ec0(type, 6, value);
    if (b2) type = 0x11;
    code = f_10013f00(t1, t3, d2);
    value = f_10013dd0(code);
    if (b2) type = 0x19;
    f_10012ec0(type, 5, value);
    if (b2) type = 0x11;                 // dead store kept by /Od (0x10013c36); Ghidra drops it
}
