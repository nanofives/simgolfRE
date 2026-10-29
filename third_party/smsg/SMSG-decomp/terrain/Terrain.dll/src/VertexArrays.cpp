#include <windows.h>
#include "GL/gl.h"
#include "Terrain.h"

TileTypeDef g_TileTypes[NUM_TEX_SLOTS];

extern "C" int g_nCourseType = 0;
float g_fCameraPitchDegrees = 0;

extern "C" {
int g_QuadrantIndex = 0;
GLfloat g_VertNormals[22500][3] = { 0 };
Vec3 g_Verts[22500] = { 0 };
}
