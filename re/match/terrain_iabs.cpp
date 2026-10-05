// Terrain.dll 0x100158b0: the debug CRT's abs() (called as abs() by LoadDIBitmap/SaveDIBitmap, terrain_bitmap.cpp).
// Compiled /Od WITHOUT /GZ like the CRT, unlike Terrain.cpp; it is the first function of the CRT block
// (0x100158b0..). The FidDb did not identify it (its _abs;_labs entry at 0x100158e0 is not unique).
// FLAGS Terrain.dll: /Od
// MATCH: Terrain.dll 0x100158b0 ?iabs@@YAHH@Z
int iabs(int v)
{
    int r;
    if (v >= 0)
        r = v;
    else
        r = -v;
    return r;
}
