// golf_clean.exe 0x0040df80 (release). WIP 86%: same pointer-anchor symptom as golf_objects.cpp (the original
// walks the records with edi = &record.type; VC6 here anchors 2 bytes later). Tried index and pointer loops.
// FLAGS golf_clean.exe: /O2
struct PlacedObject { short type; short tx, ty; short pad; int level; int pad2; };
extern PlacedObject g_objects[256];      // 0x58bcb8
struct ObjSize { char size; char pad[19]; };
extern ObjSize g_objSize[];              // 0x4c26c0, stride 20
extern int g_objExtra[];                 // 0x5a8c38

// Index of the placed object whose square footprint covers tile (x, y); -1 if none.
// MATCH: golf_clean.exe 0x0040df80 ?objectAt@@YAHHH@Z
int objectAt(int x, int y)
{
    for (int i = 0; i < 256; i++) {
        short t = g_objects[i].type;
        if (t == -1)
            continue;
        int size = g_objSize[t].size;
        if (!(t < 6 || t == 5 || t == 7))
            size += g_objExtra[t] - 1;
        if (x >= g_objects[i].tx && x < g_objects[i].tx + size
            && y >= g_objects[i].ty && y < g_objects[i].ty + size)
            return i;
    }
    return -1;
}

