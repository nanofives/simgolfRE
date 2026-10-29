#ifndef TILE_H
#define TILE_H

class Tile;


struct Vec3 {
    float x, y, z;
};

#ifndef TILE_REVISION
#define TILE_REVISION 1
#endif
#if TILE_REVISION >= 2 || !defined(TERRAIN_EXPORTS)
struct TileNode {
    TileNode *_Next;
    TileNode *_Prev;
    Tile *_Value;
};

struct TileAcc {
    static TileNode *&_Next(TileNode *_P) { return _P->_Next; }
    static TileNode *&_Prev(TileNode *_P) { return _P->_Prev; }
    static Tile *&_Value(TileNode *_P) { return _P->_Value; }
};

inline void *operator new(unsigned int, void *_P)
{
    return _P;
}

template <class T> inline T *_Allocate(int _N, T *)
{
    if (_N < 0)
        _N = 0;
    return ((T *)operator new((unsigned int)_N * sizeof(T)));
}
template <class T1, class T2> inline void _Construct(T1 *_P, const T2 &_V)
{
    new ((void *)_P) T1(_V);
}
template <class T> inline void _Destroy(T *)
{
}

struct TileAlloc {
public:
    char *_Charalloc(unsigned int _N) { return _Allocate((int)_N, (char *)0); }
    void construct(Tile **_P, Tile *const &_V) { _Construct(_P, _V); }
    void destroy(Tile **_P) { _Destroy(_P); }
    void deallocate(void *_P, unsigned int) { operator delete(_P); }
};

class TileIterBase {
public:
    TileNode *m_pNode;

    TileIterBase() {}
    TileIterBase(TileNode *_P) : m_pNode(_P) {}
};

class TileIter : public TileIterBase {
public:
    TileIter() {}
    TileIter(TileNode *_P) : TileIterBase(_P) {}

    TileNode *_Mynode() const { return m_pNode; }
    TileIter &operator++()
    {
        m_pNode = TileAcc::_Next(m_pNode);
        return *this;
    }
    TileIter operator++(int)
    {
        TileIter _T = *this;
        ++*this;
        return _T;
    }
    bool operator==(const TileIter &_X) const { return m_pNode == _X.m_pNode; }
    bool operator!=(const TileIter &_X) const { return !(*this == _X); }
    Tile *&operator*() const { return TileAcc::_Value(m_pNode); }
};
class TileList {
public:
    TileAlloc allocator;
    TileNode *_Head;
    unsigned int _Size;

    TileList(const TileAlloc &_Al = TileAlloc()) : allocator(_Al), _Head(_Buynode(0, 0)), _Size(0) {}

    TileIter begin() { return TileIter(TileAcc::_Next(_Head)); }
    TileIter end() { return TileIter(_Head); }
    unsigned int size() const { return _Size; }
    bool empty() const { return size() == 0; }
#if TILE_REVISION >= 2
    void clear() { erase(begin(), end()); }
#else
    void clear();
#endif

    TileNode *_Buynode(TileNode *_Narg, TileNode *_Parg)
    {
        TileNode *_S = (TileNode *)allocator._Charalloc(sizeof(TileNode));
        TileAcc::_Next(_S) = _Narg != 0 ? _Narg : _S;
        TileAcc::_Prev(_S) = _Parg != 0 ? _Parg : _S;
        return (_S);
    }
    TileIter insert(TileIter _P, Tile *const &_X)
    {
        TileNode *_S = _P._Mynode();
        TileAcc::_Prev(_S) = _Buynode(_S, TileAcc::_Prev(_S));
        _S = TileAcc::_Prev(_S);
        TileAcc::_Next(TileAcc::_Prev(_S)) = _S;
        allocator.construct(&TileAcc::_Value(_S), _X);
        ++_Size;
        return TileIter(_S);
    }
    void push_back(Tile *const &_X) { insert(end(), _X); }
    void push_front(Tile *const &_X) { insert(begin(), _X); }
    void _Freenode(TileNode *_S) { allocator.deallocate(_S, 1); }
    TileIter erase(TileIter _P)
    {
        TileNode *_S = (_P++)._Mynode();
        TileAcc::_Next(TileAcc::_Prev(_S)) = TileAcc::_Next(_S);
        TileAcc::_Prev(TileAcc::_Next(_S)) = TileAcc::_Prev(_S);
        allocator.destroy(&TileAcc::_Value(_S));
        _Freenode(_S);
        --_Size;
        return (_P);
    }
    ~TileList()
    {
        erase(begin(), end());
        _Freenode(_Head);
        _Head = 0, _Size = 0;
    }

#if TILE_REVISION >= 2
    TileIter erase(TileIter _F, TileIter _L)
    {
        while (_F != _L) {
            TileNode *_S = (_F++)._Mynode();
            TileAcc::_Next(TileAcc::_Prev(_S)) = TileAcc::_Next(_S);
            TileAcc::_Prev(TileAcc::_Next(_S)) = TileAcc::_Prev(_S);
            allocator.destroy(&TileAcc::_Value(_S));
            _Freenode(_S);
            --_Size;
        }
        return (_F);
    }
#else
    TileIter erase(TileIter _F, TileIter _L)
    {
        while (_F != _L)
            erase(_F++);
        return (_F);
    }
#endif
};
#else
#include <list>
typedef std::list<Tile *> TileList;
typedef TileList::iterator TileIter;
#endif


struct Face {
    int v[3];
    int uvSet0, uvRow0;
    int uvSet1, uvRow1;
    int uvSet2, uvRow2;
    unsigned int tex;
    unsigned char texIdx;
    unsigned char pad29[3];
    float nrm[3];
};

struct FaceBlock {
    int numFaces;
    Face face[8];

    FaceBlock();

    void InitTriangleMesh(int tileCol, int tileRow, int numCols);

    void SetNineVertexHeights(int height);
    void setCornerVertexNormals(float *normal);
    float getMaxCornerVertexHeight();
    ~FaceBlock();
};

class TilePathFlags {
public:
    bool m_hasPath, m_connected, m_visible, f3, f4, f5, f6;

#if TILE_REVISION >= 2 || !defined(TERRAIN_EXPORTS)
    void clear()
    {
        m_connected = false;
        m_hasPath = false;
        m_visible = false;
        f3 = false;
        f4 = false;
        f5 = false;
        f6 = false;
    }

    TilePathFlags() { clear(); }
#else
    void clear();
    TilePathFlags();
#endif
};

class TileWallState {
public:
    int m_wall[9];
    bool m_enabled[8];

#if TILE_REVISION >= 2 || !defined(TERRAIN_EXPORTS)
    void resetEnabledFlags()
    {
        m_enabled[0] = false;
        m_enabled[2] = false;
        m_enabled[6] = false;
        m_enabled[4] = false;
    }

    TileWallState() { resetEnabledFlags(); }
#else
    void resetEnabledFlags();
    TileWallState();
#endif
};


class Tile {
public:
    int m_cornerElevation[8];
    int m_field20;
    int m_type;
    int m_variation;
    int m_gridX;
    int m_gridY;
    Tile *m_neighbours[4];
    FaceBlock m_faceBlock;
    TilePathFlags m_pathFlags;
    TileWallState m_wallState;
    int m_field23c;
    int m_texSet;
    unsigned char m_normalsDirty;
    unsigned char pad245[3];

#if TILE_REVISION >= 2 || !defined(TERRAIN_EXPORTS)
    int getGridX() { return m_gridX; }
    int getGridY() { return m_gridY; }
#else
    int getGridX();
    int getGridY();
#endif

    bool isTypeSkipStrataFace();


#if TILE_REVISION >= 2 || !defined(TERRAIN_EXPORTS)
    FaceBlock *GetFaceBlock() { return (&m_faceBlock); }
#else
    FaceBlock *GetFaceBlock();
#endif

    void CalcFaceNormal(Face *pFace);

    void CalcCornerVertexNormals(int nRowDir, int nColDir);

    void calcVertexNormals();

    void reset(int iGridX, int iGridY);


    Tile();

    ~Tile();

    void ElevateCorner(int iCorner);

    void LowerCornerElevation(int iCorner);

    void RenderTile();

    int getType() { return m_type; }

    void RenderTilePath();
    void RenderTileQuadrantsByTransitionMask(int mask, int texBase);

    bool getWallEnabled(int nDir) { return m_wallState.m_enabled[nDir]; }

#if TILE_REVISION >= 2 || !defined(TERRAIN_EXPORTS)
    int maxCornerHeight()
    {
        int maxEdge0 = m_cornerElevation[1] > m_cornerElevation[7] ? m_cornerElevation[1] : m_cornerElevation[7];
        int maxEdge4 = m_cornerElevation[3] > m_cornerElevation[5] ? m_cornerElevation[3] : m_cornerElevation[5];
        return maxEdge0 > maxEdge4 ? maxEdge0 : maxEdge4;
    }
#else
    int maxCornerHeight();
#endif

    int getCornerElevation(int nCorner) { return m_cornerElevation[nCorner]; }
#if TILE_REVISION >= 2 || !defined(TERRAIN_EXPORTS)
    char getVariation() { return ((char)m_variation); }
    bool hasPath() { return (m_pathFlags.m_hasPath); }
    bool hasConnectedPath() { return (m_pathFlags.m_connected); }
#else
    char getVariation();
    bool hasPath();
    bool hasConnectedPath();
#endif

#if TILE_REVISION >= 2 || !defined(TERRAIN_EXPORTS)
    void setRandomVariation(int n) { m_texSet = n; }
    void setVariation(int n) { m_variation = n; }
#else
    void setRandomVariation(int n);
    void setVariation(int n);
#endif

#if TILE_REVISION >= 2 || !defined(TERRAIN_EXPORTS)
    void setPath(bool on, int also)
    {
        if (on) {
            m_pathFlags.m_hasPath = 1;
            if (also)
                m_pathFlags.m_connected = 1;
            if (m_type == 0x16 || m_type == 0 || m_type == 2 || m_type == 1 || m_type == 3 || m_type == 7 ||
                m_type == 9)
                m_pathFlags.m_visible = 0;
            else
                m_pathFlags.m_visible = 1;
        } else {
            m_pathFlags.m_hasPath = 0;
            m_pathFlags.m_connected = 0;
            m_pathFlags.m_visible = 0;
        }
    }
#else
    void setPath(bool on, int also);
#endif

#if TILE_REVISION >= 2 || !defined(TERRAIN_EXPORTS)
    void setWall(int side, int value, bool enabled)
    {
        m_wallState.m_enabled[side] = enabled;
        m_wallState.m_wall[side] = value;
    }
#else
    void setWall(int side, int value, bool enabled);
#endif

#if TILE_REVISION >= 2 || !defined(TERRAIN_EXPORTS)
    void setNeighbour(int nDir, Tile *pTile) { m_neighbours[nDir] = pTile; }
#else
    void setNeighbour(int nDir, Tile *pTile);
#endif

    void UndoType6ElevationRaise();
    void UndoType7ElevationDrop();
    void setType(int iType);

    void RaiseTileType6Heights();
    void LowerTileType7Heights();

    void drawEdgeTransitionsWithNeighbours();
    void assignFaceTextures();
    void updateHasPathForType(int type);

    void DrawTileEdgeTransitionsForType(int iType);

    void SetFaceTexture(int iTileType, int iFace, int iSub);

    int getNeighborTransitionCode(Tile *n);

    unsigned char typeDrawsEdgeTransitions(int type);
    unsigned char typeAllowsEdgeTransition();
    int PackEdgeTransitionCode(int nEdge, int nSide, int nCorner);
    int MapTypeCodeToIndex(int code);

#if TILE_REVISION >= 2 || !defined(TERRAIN_EXPORTS)
    void setConnectedPath(int bOn)
    {
        if (bOn)
            m_pathFlags.m_connected = true;
        else
            m_pathFlags.m_connected = false;
    }
#else
    void setConnectedPath(int bOn);
#endif
    void RenderTileWalls();
    void RenderStrataFace(int depth);
};

typedef char TileSizeCheck[sizeof(Tile) == 0x248 ? 1 : -1];

#define NUM_TEXTURED_TILES 31

struct TileTypeDef {
    union {
        unsigned int glTexture;
        unsigned int tex[25][9];
    };
};

int FloatToTexCoordCode(float t);

void InitVertexGrid(int nCols, int nRows);

float (*GetVertexArrayBase())[3];
float (*GetNormalArrayBase())[3];

#define NUM_TEX_SLOTS 37
extern TileTypeDef g_TileTypes[NUM_TEX_SLOTS];

#endif
