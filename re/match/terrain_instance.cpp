// Terrain::getInstance (Terrain.dll, debug): singleton allocated with new + constructor (SEH frame => /GX).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX

class Terrain {
public:
    Terrain(int width, int height);      // 0x10002ae0 (called with 0x32, 0x32 at 0x10003208)
    static Terrain* getInstance();
    char m_storage[0x164ad0];            // sizeof(Terrain): operator new(0x164ad0) at 0x100031f1
};

Terrain* g_terrain;                      // 0x10106b4c

// MATCH: Terrain.dll 0x100031a0 ?getInstance@Terrain@@SAPAV1@XZ
Terrain* Terrain::getInstance()
{
    if (g_terrain == 0)
        g_terrain = new Terrain(50, 50);
    return g_terrain;
}
