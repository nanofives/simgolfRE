#ifndef TERRAIN_H
#define TERRAIN_H

#include <windows.h>

#include "GL/gl.h"
#include "Tile.h"

#ifdef TERRAIN_EXPORTS
#define TERRAIN_API __declspec(dllexport)
#else
#define TERRAIN_API __declspec(dllimport)
#endif

#define TERRAIN_GRID_DIM   50
#define TERRAIN_GRID_TILES (TERRAIN_GRID_DIM * TERRAIN_GRID_DIM)

struct CourseType {
    char szName[0x14];
    int nVariants;
};

struct Point2D {
    float x, y;
};

class Terrain {
public:
    Terrain(long nWidth, long nHeight);
    TERRAIN_API ~Terrain();

    TERRAIN_API void calcAllNormals(Tile *);
    TERRAIN_API void calcNormals(Tile *);
    TERRAIN_API void changeLighting(int);
    TERRAIN_API void closeSystem();

    void freeCourseTextures();
    TERRAIN_API void drawBezierSpline(int x0, int y0, int x1, int y1, int x2, int y2, int colour, int iWidth,
                                      int iAlpha);

    float bernsteinBasisTerm(float t, int i);
    TERRAIN_API void drawCardinalSpline(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, int colour,
                                        int iWidth, int iAlpha);
    TERRAIN_API void drawCircle(Tile *, float);
    TERRAIN_API void drawLine(int, int, int, int, int, int, int);
    TERRAIN_API void elevateCorner(Tile *, int);
    TERRAIN_API int getElevation(Tile *, int);
    static TERRAIN_API Terrain *getInstance();
    TERRAIN_API int getType(Tile *);
    TERRAIN_API int getVariation(Tile *);
    TERRAIN_API bool getWall(Tile *, int);
    TERRAIN_API bool hasConnectedPath(int, int);
    TERRAIN_API bool hasPath(Tile *);
    TERRAIN_API void initSystem(int, int, HDC, bool);
    TERRAIN_API void initTerrain();

    bool initTileGrid();
    TERRAIN_API void layPath(Tile *, int, int);
    TERRAIN_API void loadNewCourseType(int);
    TERRAIN_API void localRender(Tile *pTile, Tile *pCameraTile, float rotationDegrees);
    TERRAIN_API void lowerCorner(Tile *, int);
    TERRAIN_API void lowerEdgeCorner(Tile *, int, Tile *, float);
    TERRAIN_API void passCollarInfo(int *const, int);
    TERRAIN_API void pathUpdateRender(Tile *, float);
    TERRAIN_API bool render(Tile *, float);
    TERRAIN_API void renderTile(int iTileType, int screenX, int screenY, int num, int den);
    TERRAIN_API void resetTerrain();
    TERRAIN_API void resize(int, int);
    TERRAIN_API void setSplineHeight(float);
    TERRAIN_API void setType(Tile *, int, int);
    TERRAIN_API void setWall(Tile *, int, int, bool);
    TERRAIN_API void setZoomLevel(int);
    TERRAIN_API void stripRender(Tile *, int, float);
    TERRAIN_API Tile *tileAt(int, int);
    TERRAIN_API Tile *tileHit(int, int);
    TERRAIN_API void updatePath(int, int, int);

    void RenderTileEdgeSkirt(Tile *pTile, Tile *pGridTile, float fAngle);

    void SetRotationQuadrantFromDegrees(float);

    void InsertTileInDrawOrder(Tile *, Tile *, float);
    void InsertWallAdjacentTilesInDrawOrder(Tile *, Tile *, float);
    void EnableBlackFillLight();

    bool IsTileCulled(Tile *pTile, Tile *pCentre, float fAngle);

    void LoadLightingFromFile(const char *pszFile);

    void LoadLightingForCourseType();

    void LoadCourseTextures();

    bool initGLRenderState();
    bool initLighting();

    Vec3 *getVertexArray();
    Vec3 *getNormalArray();

    void evalCardinalSplinePoint2D(Point2D *pOut, float t, Point2D *p0, Point2D *p1, Point2D *m0, Point2D *m1);

private:
    HGLRC m_hRC;
    float m_ambient[4];
    int m_width;
    int m_height;
    int m_zoomLevel;
    int m_screenWidth;
    int m_screenHeight;
    bool m_flipVertical;
    bool m_bLoaded;
    char m_pad2a[2];
    CourseType m_courseTypes[0x25];
    Tile m_tiles[TERRAIN_GRID_TILES];
    TileList m_drawList;
};

typedef char TerrainSizeCheck[sizeof(Terrain) == 0x164ad0 ? 1 : -1];


extern "C" GLfloat g_VertNormals[22500][3];
extern "C" int g_nCourseType;
extern "C" int *g_pCollar;
extern "C" int g_QuadrantIndex;
extern const float g_fDegToRad;
extern "C" Terrain *g_pInstance;
extern "C" TileTypeDef g_TileTypes[NUM_TEX_SLOTS];
extern "C" Vec3 g_NormalTowardNb1[2];
extern "C" Vec3 g_NormalTowardNb0[2];
extern "C" Vec3 g_NormalTowardNb3[2];
extern "C" Vec3 g_NormalTowardNb2[2];
extern float g_fCameraPitchDegrees;
extern float g_fSplineHeight;
extern "C" double cos(double);
extern "C" double sin(double);
extern "C" double __cdecl sqrt(double _X);
extern "C" double __cdecl pow(double _X, double _Y);
extern "C" GLfloat g_FaceUV[3][3][4][2];
extern "C" Vec3 g_Verts[22500];

#endif
