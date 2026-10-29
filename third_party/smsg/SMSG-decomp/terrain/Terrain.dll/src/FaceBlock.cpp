#include "Tile.h"

typedef unsigned int GLenum;
typedef float GLfloat;

#include "Terrain.h"

FaceBlock::FaceBlock()
{
    numFaces = 0;
}

FaceBlock::~FaceBlock()
{
}

int FloatToTexCoordCode(float t)
{
    if (*(int *)&t == 0x3f000000)
        return 2;
    return (int)t;
}

void FaceBlock::InitTriangleMesh(int tileCol, int tileRow, int numCols)
{
    int n = 0;
    int rowStride = numCols * 3;
    int rowBase = tileRow * rowStride * 3;
    int colBase = tileCol * 3;

    numFaces = 8;

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            face[n].v[0] = j + i * rowStride + rowBase + colBase;
            face[n].v[1] = j + (i + 1) * rowStride + rowBase + colBase;
            face[n].v[2] = j + (rowBase + (i + 1) * rowStride + 1) + colBase;
            face[n].uvSet0 = FloatToTexCoordCode(j / 2.0f);
            face[n].uvRow0 = FloatToTexCoordCode(1.0f - i / 2.0f);
            face[n].uvSet1 = FloatToTexCoordCode(j / 2.0f);
            face[n].uvRow1 = FloatToTexCoordCode(1.0f - (i + 1) / 2.0f);
            face[n].uvSet2 = FloatToTexCoordCode((j + 1) / 2.0f);
            face[n].uvRow2 = FloatToTexCoordCode(1.0f - (i + 1) / 2.0f);
            face[n].nrm[0] = 0.0f;
            face[n].nrm[1] = 1.0f;
            face[n].nrm[2] = 0.0f;
            n++;
            face[n].v[0] = j + i * rowStride + rowBase + colBase;
            face[n].v[1] = j + (rowBase + (i + 1) * rowStride + 1) + colBase;
            face[n].v[2] = j + (rowBase + i * rowStride + 1) + colBase;
            face[n].uvSet0 = FloatToTexCoordCode(j / 2.0f);
            face[n].uvRow0 = FloatToTexCoordCode(1.0f - i / 2.0f);
            face[n].uvSet1 = FloatToTexCoordCode((j + 1) / 2.0f);
            face[n].uvRow1 = FloatToTexCoordCode(1.0f - (i + 1) / 2.0f);
            face[n].uvSet2 = FloatToTexCoordCode((j + 1) / 2.0f);
            face[n].uvRow2 = FloatToTexCoordCode(1.0f - i / 2.0f);
            face[n].nrm[0] = 0.0f;
            face[n].nrm[1] = 1.0f;
            face[n].nrm[2] = 0.0f;
            n++;
        }
    }

    face[2].v[2] = face[3].v[2];
    face[2].uvSet2 = face[3].uvSet2;
    face[2].uvRow2 = face[3].uvRow2;
    face[3].v[0] = face[2].v[1];
    face[3].uvSet0 = face[2].uvSet1;
    face[3].uvRow0 = face[2].uvRow1;
    face[4].v[2] = face[5].v[2];
    face[4].uvSet2 = face[5].uvSet2;
    face[4].uvRow2 = face[5].uvRow2;
    face[5].v[0] = face[4].v[1];
    face[5].uvSet0 = face[4].uvSet1;
    face[5].uvRow0 = face[4].uvRow1;
}

void FaceBlock::SetNineVertexHeights(int height)
{
    *(int *)&g_Verts[face[0].v[0]].y = height;
    *(int *)&g_Verts[face[1].v[2]].y = height;
    *(int *)&g_Verts[face[2].v[2]].y = height;
    *(int *)&g_Verts[face[3].v[1]].y = height;
    *(int *)&g_Verts[face[7].v[1]].y = height;
    *(int *)&g_Verts[face[6].v[1]].y = height;
    *(int *)&g_Verts[face[4].v[1]].y = height;
    *(int *)&g_Verts[face[0].v[1]].y = height;
    *(int *)&g_Verts[face[0].v[2]].y = height;
}

void FaceBlock::setCornerVertexNormals(float *normal)
{
    for (int i = 0; i < 3; i++) {
        g_VertNormals[face[0].v[0]][i] = normal[i];
        g_VertNormals[face[1].v[2]][i] = normal[i];
        g_VertNormals[face[2].v[2]][i] = normal[i];
        g_VertNormals[face[3].v[1]][i] = normal[i];
        g_VertNormals[face[7].v[1]][i] = normal[i];
        g_VertNormals[face[6].v[1]][i] = normal[i];
        g_VertNormals[face[4].v[1]][i] = normal[i];
        g_VertNormals[face[0].v[1]][i] = normal[i];
        g_VertNormals[face[0].v[2]][i] = normal[i];
    }
}

float FaceBlock::getMaxCornerVertexHeight()
{
    float m = g_Verts[face[0].v[0]].y;
    m = g_Verts[face[1].v[2]].y < m ? g_Verts[face[1].v[2]].y : m;
    m = g_Verts[face[2].v[2]].y < m ? g_Verts[face[2].v[2]].y : m;
    m = g_Verts[face[3].v[1]].y < m ? g_Verts[face[3].v[1]].y : m;
    m = g_Verts[face[7].v[1]].y < m ? g_Verts[face[7].v[1]].y : m;
    m = g_Verts[face[6].v[1]].y < m ? g_Verts[face[6].v[1]].y : m;
    m = g_Verts[face[4].v[1]].y < m ? g_Verts[face[4].v[1]].y : m;
    m = g_Verts[face[0].v[1]].y < m ? g_Verts[face[0].v[1]].y : m;
    return m;
}

extern "C" {
Vec3 g_NormalTowardNb1[2] = { { -0.707f, 0.0f, -0.707f }, { -1.0f, 1.0f, 0.0f } };
Vec3 g_NormalTowardNb0[2] = { { 0.707f, 0.0f, 0.707f }, { 1.0f, 1.0f, 0.0f } };
Vec3 g_NormalTowardNb3[2] = { { 1.0f, 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } };
Vec3 g_NormalTowardNb2[2] = { { -1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
extern "C" GLfloat g_FaceUV[3][3][4][2] = {
    {
        { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } },
        { { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f } },
        { { 0.0f, 0.5f }, { 0.5f, 0.0f }, { 1.0f, 0.5f }, { 0.5f, 1.0f } },
    },
    {
        { { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f } },
        { { 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } },
        { { 1.0f, 0.5f }, { 0.5f, 1.0f }, { 0.0f, 0.5f }, { 0.5f, 0.0f } },
    },
    {
        { { 0.5f, 0.0f }, { 1.0f, 0.5f }, { 0.5f, 1.0f }, { 0.0f, 0.5f } },
        { { 0.5f, 1.0f }, { 0.0f, 0.5f }, { 0.5f, 0.0f }, { 1.0f, 0.5f } },
        { { 0.5f, 0.5f }, { 0.5f, 0.5f }, { 0.5f, 0.5f }, { 0.5f, 0.5f } },
    },
};
}
