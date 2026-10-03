// Live exported Terrain methods of Terrain.dll (debug build). Most are thin wrappers over Tile methods.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
//
// Tile method names are TENTATIVE: taken from the exported Terrain wrapper that calls them (the callee
// bodies are not exported). Call targets are masked by the matcher, so they do not affect the match.
#include <stdlib.h>

class Tile {
public:
    bool hasPath();                                  // callee of Terrain::hasPath      (thunk -> 0x10013320)
    char getVariation();                             // callee of Terrain::getVariation (thunk -> 0x10015340); char: movsx eax,al after the call
    void calcNormals();                              // callee of Terrain::calcNormals  (thunk -> 0x10012cf0)
    void elevateCorner(int corner);                  // (thunk -> 0x1000c7b0)
    void lowerCorner(int corner);                    // (thunk -> 0x1000ccc0)
    void setWall(int a, int b, bool on);             // (thunk -> 0x10015400)
    void layPath(bool on, int dir);                  // (thunk -> 0x10013400)
};

class Terrain {
public:
    bool hasPath(Tile* t);
    int  getVariation(Tile* t);
    void calcNormals(Tile* t);
    void elevateCorner(Tile* t, int corner);
    void lowerCorner(Tile* t, int corner);
    void setWall(Tile* t, int a, int b, bool on);
    void layPath(Tile* t, int on, int dir);
    void lowerEdgeCorner(Tile* t, int corner, Tile* other, float amount);
    void lowerEdge(Tile* t, Tile* other, float amount);  // 0x10038900, matched in terrain_edge.cpp (draws, does not lower)
};

// MATCH: Terrain.dll 0x1000a510 ?hasPath@Terrain@@QAE_NPAVTile@@@Z
bool Terrain::hasPath(Tile* t)
{
    return t->hasPath();
}

// MATCH: Terrain.dll 0x10003390 ?getVariation@Terrain@@QAEHPAVTile@@@Z
int Terrain::getVariation(Tile* t)
{
    return t->getVariation();
}

// MATCH: Terrain.dll 0x1000a6f0 ?calcNormals@Terrain@@QAEXPAVTile@@@Z
void Terrain::calcNormals(Tile* t)
{
    if (t != 0)
        t->calcNormals();
}

// MATCH: Terrain.dll 0x1000a5c0 ?elevateCorner@Terrain@@QAEXPAVTile@@H@Z
void Terrain::elevateCorner(Tile* t, int corner)
{
    if (t != 0)
        t->elevateCorner(corner);
}

// MATCH: Terrain.dll 0x1000a620 ?lowerCorner@Terrain@@QAEXPAVTile@@H@Z
void Terrain::lowerCorner(Tile* t, int corner)
{
    if (t != 0)
        t->lowerCorner(corner);
}

// MATCH: Terrain.dll 0x1000a560 ?setWall@Terrain@@QAEXPAVTile@@HH_N@Z
void Terrain::setWall(Tile* t, int a, int b, bool on)
{
    t->setWall(a, b, on);
}

// MATCH: Terrain.dll 0x1000a3e0 ?layPath@Terrain@@QAEXPAVTile@@HH@Z
void Terrain::layPath(Tile* t, int on, int dir)
{
    if (on)
        t->layPath(true, dir);
    else
        t->layPath(false, dir);
}

// MATCH: Terrain.dll 0x1000a680 ?lowerEdgeCorner@Terrain@@QAEXPAVTile@@H0M@Z
void Terrain::lowerEdgeCorner(Tile* t, int corner, Tile* other, float amount)
{
    if (t != 0) {
        lowerEdge(t, other, amount);   // ecx = this (0x1000a6af), t/other/amount pushed
        t->lowerCorner(corner);        // ecx = t (0x1000a6bb)
    }
}
