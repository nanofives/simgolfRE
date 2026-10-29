extern const float g_fDegToRad = 0.0174532924f;

#include "Tile.h"
#include "Terrain.h"
#include "Bmp.h"
#include "GL/glu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_QuadrantIndexStorage;

int *g_pCollar;
class Terrain;

Terrain *g_pInstance;

class Terrain;


class UnknownBB70 {
public:
    UnknownBB70();
    ~UnknownBB70();
};
UnknownBB70::UnknownBB70()
{
}

UnknownBB70::~UnknownBB70()
{
}

void Vec3_Normalize(float *v);

extern "C" {
float g_fTileSizeX = 100.0f;
float g_fTileSizeZ = 100.0f;
}
float g_fSplineHeight = 15.0f;

GLuint UploadBmpTexture(const char *pszFile, int, GLenum eMinFilter, GLenum eMagFilter, GLenum eWrap)
{
    BITMAPINFOHEADER *pbi;
    unsigned char *pBits;
    GLenum eTarget;
    GLuint uTex;

    pBits = LoadBmpFile(pszFile, &pbi);
    if (pBits == 0)
        return 0;

    SwapBgrToRgb(pBits, pbi);

    if (pbi->biHeight == 1)
        eTarget = GL_TEXTURE_1D;
    else
        eTarget = GL_TEXTURE_2D;

    glGenTextures(1, &uTex);
    glBindTexture(eTarget, uTex);
    glTexParameteri(eTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(eTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(eTarget, GL_TEXTURE_WRAP_S, eWrap);
    glTexParameteri(eTarget, GL_TEXTURE_WRAP_T, eWrap);
    glTexEnvi(eTarget, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    if (eMinFilter == GL_LINEAR || eMinFilter == GL_NEAREST)
        glTexImage2D(eTarget, 0, 3, pbi->biWidth, pbi->biHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pBits);
    else if (eTarget == GL_TEXTURE_1D)
        gluBuild1DMipmaps(eTarget, 3, pbi->biWidth, GL_RGBA, GL_UNSIGNED_BYTE, pBits);
    else
        gluBuild2DMipmaps(eTarget, 3, pbi->biWidth, pbi->biHeight, GL_RGB, GL_UNSIGNED_BYTE, pBits);

    free(pbi);
    free(pBits);

    return uTex;
}

unsigned int LoadTgaTexture(const char *pszFile)
{
    unsigned char cTgaCompare[12] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    unsigned char TgaHeader[12];
    unsigned char header[6];
    int bytesPerPixel;
    int imageSize;
    int t;
    unsigned int texture;
    unsigned int width;
    unsigned int height;
    unsigned char *imageData;
    FILE *fp;

    fp = fopen(pszFile, "rb");
    if (fp == 0)
        return 0;

    if (fread(TgaHeader, 1, 12, fp) != 12 || memcmp(cTgaCompare, TgaHeader, 12) != 0 || fread(header, 1, 6, fp) != 6) {
        fclose(fp);
        return 0;
    }

    width = header[1] * 256 + header[0];
    height = header[3] * 256 + header[2];

    if (width <= 0 || height <= 0 || (header[4] != 24 && header[4] != 32)) {
        fclose(fp);
        return 0;
    }

    bytesPerPixel = header[4] / 8;
    imageSize = width * height * bytesPerPixel;
    imageData = new unsigned char[imageSize];

    if (imageData == 0 || fread(imageData, 1, imageSize, fp) != imageSize) {
        fclose(fp);
        if (imageData != 0)
            delete[] imageData;
        return 0;
    }

    for (int i = 0; i < imageSize; i += bytesPerPixel) {
        t = imageData[i];
        imageData[i] = imageData[i + 2];
        imageData[i + 2] = t;
    }

    fclose(fp);

    glGenTextures(1, &texture);
    glBindTexture(0x0de1, texture);
    glTexParameterf(0x0de1, 0x2801, 0x2601);
    glTexParameterf(0x0de1, 0x2800, 0x2601);

    if (bytesPerPixel == 3)
        glTexImage2D(0x0de1, 0, 0x1907, width, height, 0, 0x1907, 0x1401, imageData);
    else
        glTexImage2D(0x0de1, 0, 0x1908, width, height, 0, 0x1908, 0x1401, imageData);

    if (imageData != 0)
        delete[] imageData;

    return texture;
}

Tile::Tile()
{
}

void Tile::reset(int iGridX, int iGridY)
{
    bool bUnusedFlag;
    int i;

    m_gridX = iGridX;
    m_gridY = iGridY;

    bUnusedFlag = false;

    m_texSet = 0;

    m_cornerElevation[1] = 0;
    m_cornerElevation[7] = 0;
    m_cornerElevation[3] = 0;
    m_cornerElevation[5] = 0;

    for (i = 0; i < 4; i++)
        m_neighbours[i] = 0;

    m_type = 4;
    m_variation = 0;

    m_pathFlags.clear();
    m_wallState.resetEnabledFlags();
}
Tile::~Tile()
{
    m_gridX = -1;
    m_gridY = -1;

    m_cornerElevation[0] = 0;
    m_cornerElevation[1] = 0;
    m_cornerElevation[2] = 0;
    m_cornerElevation[3] = 0;

    m_normalsDirty = 0;

    if (m_neighbours[0] != 0)
        m_neighbours[0]->setNeighbour(1, 0);
    if (m_neighbours[1] != 0)
        m_neighbours[1]->setNeighbour(0, 0);
    if (m_neighbours[2] != 0)
        m_neighbours[2]->setNeighbour(3, 0);
    if (m_neighbours[3] != 0)
        m_neighbours[3]->setNeighbour(2, 0);
}

void Tile::setNeighbour(int nDir, Tile *pTile)
{
    m_neighbours[nDir] = pTile;
}

void InitVertexGrid(int nCols, int nRows)
{
    int nTotal = nCols * nRows;
    int nStride = nCols * 3;
    int row;
    int col;

    for (row = 0; row < nRows; row++) {
        for (col = 0; col < nCols; col++) {
            int iBase = col * 3 + row * nStride * 3;
            float fX0 = (float)col * g_fTileSizeX - (float)nCols * g_fTileSizeX / 2.0f;
            float fZ0 = (float)row * g_fTileSizeZ - (float)nRows * g_fTileSizeZ / 2.0f;
            int i;
            int j;

            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                    g_Verts[iBase + i + j * nStride].x = (float)i * (g_fTileSizeX / 2.0f) + fX0;
                    g_Verts[iBase + i + j * nStride].z = (float)j * (g_fTileSizeZ / 2.0f) + fZ0;
                    g_Verts[iBase + i + j * nStride].y = 0.0f;
                    g_VertNormals[iBase + i + j * nStride][0] = 0.0f;
                    g_VertNormals[iBase + i + j * nStride][1] = 1.0f;
                    g_VertNormals[iBase + i + j * nStride][2] = 0.0f;
                }
            }
        }
    }
}

void Tile::ElevateCorner(int iCorner)
{
    int iSrcCentreFace;
    int iSrcCentreVert;
    int iDstEdgeAFace;
    int iDstEdgeAVert;
    int iDstEdgeBFace;
    int iDstEdgeBVert;
    int iSrcEdgeAFace;
    int iSrcEdgeAVert;
    int iSrcEdgeBFace;
    int iSrcEdgeBVert;
    int iCornerFace;
    int iCornerVert;
    float fCornerHeight;
    float fSrcCentre;
    float fSrcEdgeA;
    float fSrcEdgeB;
    float fBase;
    float fCentre;

    switch (iCorner) {
    case 1:
        iCornerFace = 3;
        iCornerVert = 2;
        iSrcCentreFace = 4;
        iSrcCentreVert = 1;
        iDstEdgeAFace = 3;
        iDstEdgeAVert = 1;
        iDstEdgeBFace = 1;
        iDstEdgeBVert = 2;
        iSrcEdgeAFace = 7;
        iSrcEdgeAVert = 1;
        iSrcEdgeBFace = 0;
        iSrcEdgeBVert = 0;
        break;
    case 7:
        iCornerFace = 0;
        iCornerVert = 0;
        iSrcCentreFace = 7;
        iSrcCentreVert = 1;
        iDstEdgeAFace = 1;
        iDstEdgeAVert = 2;
        iDstEdgeBFace = 0;
        iDstEdgeBVert = 1;
        iSrcEdgeAFace = 3;
        iSrcEdgeAVert = 2;
        iSrcEdgeBFace = 4;
        iSrcEdgeBVert = 1;
        break;
    case 3:
        iCornerFace = 7;
        iCornerVert = 1;
        iSrcCentreFace = 0;
        iSrcCentreVert = 0;
        iDstEdgeAFace = 5;
        iDstEdgeAVert = 1;
        iDstEdgeBFace = 3;
        iDstEdgeBVert = 1;
        iSrcEdgeAFace = 4;
        iSrcEdgeAVert = 1;
        iSrcEdgeBFace = 3;
        iSrcEdgeBVert = 2;
        break;
    case 5:
        iCornerFace = 4;
        iCornerVert = 1;
        iSrcCentreFace = 3;
        iSrcCentreVert = 2;
        iDstEdgeAFace = 0;
        iDstEdgeAVert = 1;
        iDstEdgeBFace = 5;
        iDstEdgeBVert = 1;
        iSrcEdgeAFace = 0;
        iSrcEdgeAVert = 0;
        iSrcEdgeBFace = 7;
        iSrcEdgeBVert = 1;
        break;
    }

    fCornerHeight = (float)(m_cornerElevation[iCorner] + 1) * g_fSplineHeight;
    fSrcCentre = g_Verts[m_faceBlock.face[iSrcCentreFace].v[iSrcCentreVert]].y;
    fSrcEdgeA = g_Verts[m_faceBlock.face[iSrcEdgeAFace].v[iSrcEdgeAVert]].y;
    fSrcEdgeB = g_Verts[m_faceBlock.face[iSrcEdgeBFace].v[iSrcEdgeBVert]].y;

    fBase = fCornerHeight;
    fCentre = (fBase + fSrcCentre) * 0.5f;

    g_Verts[m_faceBlock.face[0].v[2]].y =
        (fCentre > g_Verts[m_faceBlock.face[0].v[2]].y) ? fCentre : g_Verts[m_faceBlock.face[0].v[2]].y;

    g_Verts[m_faceBlock.face[iDstEdgeAFace].v[iDstEdgeAVert]].y = (fBase + fSrcEdgeA) * 0.5f;
    g_Verts[m_faceBlock.face[iDstEdgeBFace].v[iDstEdgeBVert]].y = (fBase + fSrcEdgeB) * 0.5f;

    m_cornerElevation[iCorner] = m_cornerElevation[iCorner] + 1;
    g_Verts[m_faceBlock.face[iCornerFace].v[iCornerVert]].y = fCornerHeight;

    m_normalsDirty = 1;

    if (m_neighbours[0] != 0)
        m_neighbours[0]->m_normalsDirty = 1;
    if (m_neighbours[0] != 0 && m_neighbours[2] != 0)
        m_neighbours[0]->m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[0] != 0 && m_neighbours[3] != 0)
        m_neighbours[0]->m_neighbours[3]->m_normalsDirty = 1;
    if (m_neighbours[2] != 0)
        m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[3] != 0)
        m_neighbours[3]->m_normalsDirty = 1;
    if (m_neighbours[1] != 0)
        m_neighbours[1]->m_normalsDirty = 1;
    if (m_neighbours[1] != 0 && m_neighbours[2] != 0)
        m_neighbours[1]->m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[1] != 0 && m_neighbours[3] != 0)
        m_neighbours[1]->m_neighbours[3]->m_normalsDirty = 1;
}

void Tile::LowerCornerElevation(int iCorner)
{
    int iSrcCentreFace;
    int iSrcCentreVert;
    int iDstEdgeAFace;
    int iDstEdgeAVert;
    int iDstEdgeBFace;
    int iDstEdgeBVert;
    int iSrcEdgeAFace;
    int iSrcEdgeAVert;
    int iSrcEdgeBFace;
    int iSrcEdgeBVert;
    int iCornerFace;
    int iCornerVert;
    float fCornerHeight;
    float fSrcCentre;
    float fSrcEdgeA;
    float fSrcEdgeB;
    float fBase;
    float fCentre;

    switch (iCorner) {
    case 1:
        iCornerFace = 3;
        iCornerVert = 2;
        iSrcCentreFace = 4;
        iSrcCentreVert = 1;
        iDstEdgeAFace = 3;
        iDstEdgeAVert = 1;
        iDstEdgeBFace = 1;
        iDstEdgeBVert = 2;
        iSrcEdgeAFace = 7;
        iSrcEdgeAVert = 1;
        iSrcEdgeBFace = 0;
        iSrcEdgeBVert = 0;
        break;
    case 7:
        iCornerFace = 0;
        iCornerVert = 0;
        iSrcCentreFace = 7;
        iSrcCentreVert = 1;
        iDstEdgeAFace = 1;
        iDstEdgeAVert = 2;
        iDstEdgeBFace = 0;
        iDstEdgeBVert = 1;
        iSrcEdgeAFace = 3;
        iSrcEdgeAVert = 2;
        iSrcEdgeBFace = 4;
        iSrcEdgeBVert = 1;
        break;
    case 3:
        iCornerFace = 7;
        iCornerVert = 1;
        iSrcCentreFace = 0;
        iSrcCentreVert = 0;
        iDstEdgeAFace = 5;
        iDstEdgeAVert = 1;
        iDstEdgeBFace = 3;
        iDstEdgeBVert = 1;
        iSrcEdgeAFace = 4;
        iSrcEdgeAVert = 1;
        iSrcEdgeBFace = 3;
        iSrcEdgeBVert = 2;
        break;
    case 5:
        iCornerFace = 4;
        iCornerVert = 1;
        iSrcCentreFace = 3;
        iSrcCentreVert = 2;
        iDstEdgeAFace = 0;
        iDstEdgeAVert = 1;
        iDstEdgeBFace = 5;
        iDstEdgeBVert = 1;
        iSrcEdgeAFace = 0;
        iSrcEdgeAVert = 0;
        iSrcEdgeBFace = 7;
        iSrcEdgeBVert = 1;
        break;
    }

    fCornerHeight = (float)(m_cornerElevation[iCorner] - 1) * g_fSplineHeight;
    fSrcCentre = g_Verts[m_faceBlock.face[iSrcCentreFace].v[iSrcCentreVert]].y;
    fSrcEdgeA = g_Verts[m_faceBlock.face[iSrcEdgeAFace].v[iSrcEdgeAVert]].y;
    fSrcEdgeB = g_Verts[m_faceBlock.face[iSrcEdgeBFace].v[iSrcEdgeBVert]].y;

    fBase = fCornerHeight;
    fCentre = (fBase + fSrcCentre) * 0.5f;

    g_Verts[m_faceBlock.face[0].v[2]].y =
        (fCentre < g_Verts[m_faceBlock.face[0].v[2]].y) ? fCentre : g_Verts[m_faceBlock.face[0].v[2]].y;

    g_Verts[m_faceBlock.face[iDstEdgeAFace].v[iDstEdgeAVert]].y = (fBase + fSrcEdgeA) * 0.5f;
    g_Verts[m_faceBlock.face[iDstEdgeBFace].v[iDstEdgeBVert]].y = (fBase + fSrcEdgeB) * 0.5f;

    m_cornerElevation[iCorner] = m_cornerElevation[iCorner] - 1;
    g_Verts[m_faceBlock.face[iCornerFace].v[iCornerVert]].y = fCornerHeight;

    if (m_type == 7) {
        g_Verts[m_faceBlock.face[0].v[2]].y = fCornerHeight - 13.0f;
        if (m_neighbours[0] != 0 && m_neighbours[0]->getType() == 7) {
            g_Verts[m_faceBlock.face[1].v[2]].y = fCornerHeight - 13.0f;
            if (m_neighbours[2] != 0 && m_neighbours[2]->getType() == 7 &&
                m_neighbours[0]->m_neighbours[2]->getType() == 7)
                g_Verts[m_faceBlock.face[0].v[0]].y = fCornerHeight - 13.0f;
            if (m_neighbours[3] != 0 && m_neighbours[3]->getType() == 7 &&
                m_neighbours[0]->m_neighbours[3]->getType() == 7)
                g_Verts[m_faceBlock.face[3].v[2]].y = fCornerHeight - 13.0f;
        }
        if (m_neighbours[1] != 0 && m_neighbours[1]->getType() == 7) {
            g_Verts[m_faceBlock.face[5].v[1]].y = fCornerHeight - 13.0f;
            if (m_neighbours[2] != 0 && m_neighbours[2]->getType() == 7 &&
                m_neighbours[1]->m_neighbours[2]->getType() == 7)
                g_Verts[m_faceBlock.face[5].v[0]].y = fCornerHeight - 13.0f;
            if (m_neighbours[3] != 0 && m_neighbours[3]->getType() == 7 &&
                m_neighbours[1]->m_neighbours[3]->getType() == 7)
                g_Verts[m_faceBlock.face[7].v[1]].y = fCornerHeight - 13.0f;
        }
        if (m_neighbours[2] != 0 && m_neighbours[2]->getType() == 7)
            g_Verts[m_faceBlock.face[0].v[1]].y = fCornerHeight - 13.0f;
        if (m_neighbours[3] != 0 && m_neighbours[3]->getType() == 7)
            g_Verts[m_faceBlock.face[3].v[1]].y = fCornerHeight - 13.0f;
    }

    m_normalsDirty = 1;

    if (m_neighbours[0] != 0)
        m_neighbours[0]->m_normalsDirty = 1;
    if (m_neighbours[0] != 0 && m_neighbours[2] != 0)
        m_neighbours[0]->m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[0] != 0 && m_neighbours[3] != 0)
        m_neighbours[0]->m_neighbours[3]->m_normalsDirty = 1;
    if (m_neighbours[2] != 0)
        m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[3] != 0)
        m_neighbours[3]->m_normalsDirty = 1;
    if (m_neighbours[1] != 0)
        m_neighbours[1]->m_normalsDirty = 1;
    if (m_neighbours[1] != 0 && m_neighbours[2] != 0)
        m_neighbours[1]->m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[1] != 0 && m_neighbours[3] != 0)
        m_neighbours[1]->m_neighbours[3]->m_normalsDirty = 1;
}

void Tile::updateHasPathForType(int type)
{
    if (m_pathFlags.m_hasPath) {
        if (type == 20 || type == 17 || type == 22 || type == 0 || type == 2 || type == 1 || type == 3 || type == 7 ||
            type == 9)
            m_pathFlags.m_visible = false;
        else
            m_pathFlags.m_visible = true;
    }

    drawEdgeTransitionsWithNeighbours();
}

void Tile::LowerTileType7Heights()
{
    m_pathFlags.m_visible = 0;

    g_Verts[m_faceBlock.face[0].v[2]].y -= 13.0f;

    if (m_neighbours[0] && m_neighbours[0]->getType() == 7) {
        g_Verts[m_faceBlock.face[1].v[2]].y -= 13.0f;
        g_Verts[m_neighbours[0]->GetFaceBlock()->face[5].v[1]].y -= 13.0f;
        if (m_neighbours[2] && m_neighbours[2]->getType() == 7 && m_neighbours[0]->m_neighbours[2]->getType() == 7) {
            g_Verts[m_faceBlock.face[0].v[0]].y -= 13.0f;
            g_Verts[m_neighbours[0]->GetFaceBlock()->face[4].v[1]].y -= 13.0f;
            g_Verts[m_neighbours[2]->GetFaceBlock()->face[3].v[2]].y -= 13.0f;
            g_Verts[m_neighbours[0]->m_neighbours[2]->GetFaceBlock()->face[7].v[1]].y -= 13.0f;
        }
        if (m_neighbours[3] && m_neighbours[3]->getType() == 7 && m_neighbours[0]->m_neighbours[3]->getType() == 7) {
            g_Verts[m_faceBlock.face[3].v[2]].y -= 13.0f;
            g_Verts[m_neighbours[0]->GetFaceBlock()->face[7].v[1]].y -= 13.0f;
            g_Verts[m_neighbours[3]->GetFaceBlock()->face[0].v[0]].y -= 13.0f;
            g_Verts[m_neighbours[0]->m_neighbours[3]->GetFaceBlock()->face[4].v[1]].y -= 13.0f;
        }
    }
    if (m_neighbours[1] && m_neighbours[1]->getType() == 7) {
        g_Verts[m_faceBlock.face[5].v[1]].y -= 13.0f;
        g_Verts[m_neighbours[1]->GetFaceBlock()->face[1].v[2]].y -= 13.0f;
        if (m_neighbours[2] && m_neighbours[2]->getType() == 7 && m_neighbours[1]->m_neighbours[2]->getType() == 7) {
            g_Verts[m_faceBlock.face[5].v[0]].y -= 13.0f;
            g_Verts[m_neighbours[1]->GetFaceBlock()->face[0].v[0]].y -= 13.0f;
            g_Verts[m_neighbours[2]->GetFaceBlock()->face[7].v[1]].y -= 13.0f;
            g_Verts[m_neighbours[1]->m_neighbours[2]->GetFaceBlock()->face[3].v[2]].y -= 13.0f;
        }
        if (m_neighbours[3] && m_neighbours[3]->getType() == 7 && m_neighbours[1]->m_neighbours[3]->getType() == 7) {
            g_Verts[m_faceBlock.face[7].v[1]].y -= 13.0f;
            g_Verts[m_neighbours[1]->GetFaceBlock()->face[3].v[2]].y -= 13.0f;
            g_Verts[m_neighbours[3]->GetFaceBlock()->face[4].v[1]].y -= 13.0f;
            g_Verts[m_neighbours[1]->m_neighbours[3]->GetFaceBlock()->face[0].v[0]].y -= 13.0f;
        }
    }
    if (m_neighbours[2] && m_neighbours[2]->getType() == 7) {
        g_Verts[m_faceBlock.face[0].v[1]].y -= 13.0f;
        g_Verts[m_neighbours[2]->GetFaceBlock()->face[3].v[1]].y -= 13.0f;
    }
    if (m_neighbours[3] && m_neighbours[3]->getType() == 7) {
        g_Verts[m_faceBlock.face[3].v[1]].y -= 13.0f;
        g_Verts[m_neighbours[3]->GetFaceBlock()->face[0].v[1]].y -= 13.0f;
    }

    m_normalsDirty = 1;
    if (m_neighbours[0] && m_neighbours[0]->getType() == 7)
        m_neighbours[0]->m_normalsDirty = 1;
    if (m_neighbours[0] && m_neighbours[2] && m_neighbours[0]->m_neighbours[2]->getType() == 7)
        m_neighbours[0]->m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[0] && m_neighbours[3] && m_neighbours[0]->m_neighbours[3]->getType() == 7)
        m_neighbours[0]->m_neighbours[3]->m_normalsDirty = 1;
    if (m_neighbours[2] && m_neighbours[2]->getType() == 7)
        m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[3] && m_neighbours[3]->getType() == 7)
        m_neighbours[3]->m_normalsDirty = 1;
    if (m_neighbours[1] && m_neighbours[1]->getType() == 7)
        m_neighbours[1]->m_normalsDirty = 1;
    if (m_neighbours[1] && m_neighbours[2] && m_neighbours[1]->m_neighbours[2]->getType() == 7)
        m_neighbours[1]->m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[1] && m_neighbours[3] && m_neighbours[1]->m_neighbours[3]->getType() == 7)
        m_neighbours[1]->m_neighbours[3]->m_normalsDirty = 1;

    drawEdgeTransitionsWithNeighbours();
}

void Tile::RaiseTileType6Heights()
{
    g_Verts[m_faceBlock.face[0].v[2]].y += 20.0f;

    if (m_neighbours[0] && m_neighbours[0]->getType() == 6) {
        g_Verts[m_faceBlock.face[1].v[2]].y += 20.0f;
        g_Verts[m_neighbours[0]->GetFaceBlock()->face[5].v[1]].y += 20.0f;
        if (m_neighbours[2] && m_neighbours[2]->getType() == 6 && m_neighbours[0]->m_neighbours[2]->getType() == 6) {
            g_Verts[m_faceBlock.face[0].v[0]].y += 20.0f;
            g_Verts[m_neighbours[0]->GetFaceBlock()->face[4].v[1]].y += 20.0f;
            g_Verts[m_neighbours[2]->GetFaceBlock()->face[3].v[2]].y += 20.0f;
            g_Verts[m_neighbours[0]->m_neighbours[2]->GetFaceBlock()->face[7].v[1]].y += 20.0f;
        }
        if (m_neighbours[3] && m_neighbours[3]->getType() == 6 && m_neighbours[0]->m_neighbours[3]->getType() == 6) {
            g_Verts[m_faceBlock.face[3].v[2]].y += 20.0f;
            g_Verts[m_neighbours[0]->GetFaceBlock()->face[7].v[1]].y += 20.0f;
            g_Verts[m_neighbours[3]->GetFaceBlock()->face[0].v[0]].y += 20.0f;
            g_Verts[m_neighbours[0]->m_neighbours[3]->GetFaceBlock()->face[4].v[1]].y += 20.0f;
        }
    }
    if (m_neighbours[1] && m_neighbours[1]->getType() == 6) {
        g_Verts[m_faceBlock.face[5].v[1]].y += 20.0f;
        g_Verts[m_neighbours[1]->GetFaceBlock()->face[1].v[2]].y += 20.0f;
        if (m_neighbours[2] && m_neighbours[2]->getType() == 6 && m_neighbours[1]->m_neighbours[2]->getType() == 6) {
            g_Verts[m_faceBlock.face[5].v[0]].y += 20.0f;
            g_Verts[m_neighbours[1]->GetFaceBlock()->face[0].v[0]].y += 20.0f;
            g_Verts[m_neighbours[2]->GetFaceBlock()->face[7].v[1]].y += 20.0f;
            g_Verts[m_neighbours[1]->m_neighbours[2]->GetFaceBlock()->face[3].v[2]].y += 20.0f;
        }
        if (m_neighbours[3] && m_neighbours[3]->getType() == 6 && m_neighbours[1]->m_neighbours[3]->getType() == 6) {
            g_Verts[m_faceBlock.face[7].v[1]].y += 20.0f;
            g_Verts[m_neighbours[1]->GetFaceBlock()->face[3].v[2]].y += 20.0f;
            g_Verts[m_neighbours[3]->GetFaceBlock()->face[4].v[1]].y += 20.0f;
            g_Verts[m_neighbours[1]->m_neighbours[3]->GetFaceBlock()->face[0].v[0]].y += 20.0f;
        }
    }
    if (m_neighbours[2] && m_neighbours[2]->getType() == 6) {
        g_Verts[m_faceBlock.face[0].v[1]].y += 20.0f;
        g_Verts[m_neighbours[2]->GetFaceBlock()->face[3].v[1]].y += 20.0f;
    }
    if (m_neighbours[3] && m_neighbours[3]->getType() == 6) {
        g_Verts[m_faceBlock.face[3].v[1]].y += 20.0f;
        g_Verts[m_neighbours[3]->GetFaceBlock()->face[0].v[1]].y += 20.0f;
    }

    m_normalsDirty = 1;
    if (m_neighbours[0])
        m_neighbours[0]->m_normalsDirty = 1;
    if (m_neighbours[0] && m_neighbours[2])
        m_neighbours[0]->m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[0] && m_neighbours[3])
        m_neighbours[0]->m_neighbours[3]->m_normalsDirty = 1;
    if (m_neighbours[2])
        m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[3])
        m_neighbours[3]->m_normalsDirty = 1;
    if (m_neighbours[1])
        m_neighbours[1]->m_normalsDirty = 1;
    if (m_neighbours[1] && m_neighbours[2])
        m_neighbours[1]->m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[1] && m_neighbours[3])
        m_neighbours[1]->m_neighbours[3]->m_normalsDirty = 1;

    drawEdgeTransitionsWithNeighbours();
}

void Tile::RenderTile()
{
    int lastTex;
    int i;
    int rot;
    int tex;

    for (i = 0; i < m_faceBlock.numFaces; i++) {
        if (g_QuadrantIndex != 0 && getType() == 7) {
            rot = m_variation % 4;
            rot -= g_QuadrantIndex;
            rot = rot < 0 ? rot + 4 : rot;
            tex = g_TileTypes[rot + 0x1b].tex[m_texSet][m_faceBlock.face[i].texIdx];
            if (tex != lastTex) {
                glBindTexture(0x0de1, tex);
                lastTex = tex;
            }
        } else {
            if (m_faceBlock.face[i].tex != (unsigned int)lastTex) {
                glBindTexture(0x0de1, m_faceBlock.face[i].tex);
                lastTex = m_faceBlock.face[i].tex;
            }
        }

        glBegin(4);
        glTexCoord2fv(g_FaceUV[m_faceBlock.face[i].uvSet0][m_faceBlock.face[i].uvRow0][g_QuadrantIndex]);
        glArrayElement(m_faceBlock.face[i].v[0]);
        glTexCoord2fv(g_FaceUV[m_faceBlock.face[i].uvSet1][m_faceBlock.face[i].uvRow1][g_QuadrantIndex]);
        glArrayElement(m_faceBlock.face[i].v[1]);
        glTexCoord2fv(g_FaceUV[m_faceBlock.face[i].uvSet2][m_faceBlock.face[i].uvRow2][g_QuadrantIndex]);
        glArrayElement(m_faceBlock.face[i].v[2]);
        glEnd();
    }

    if (m_pathFlags.m_hasPath && m_pathFlags.m_visible)
        RenderTilePath();

    RenderTileWalls();
    RenderStrataFace(3);
}

void Tile::RenderStrataFace(int depth)
{
    int flip;

    if (m_type == 0x14)
        return;

    if (g_QuadrantIndex == 0 || g_QuadrantIndex == 2)
        flip = 0;
    else
        flip = 1;

    glBindTexture(0x0de1, g_TileTypes[36].glTexture);

    if (m_neighbours[1] && m_neighbours[1]->getType() == 0x14 && (g_QuadrantIndex == 0 || g_QuadrantIndex == 3)) {
        glBegin(0x0004);
        glNormal3fv(&g_NormalTowardNb1[flip].x);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[5].v[0]].x);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(g_Verts[m_faceBlock.face[5].v[0]].x, -75.0f, g_Verts[m_faceBlock.face[5].v[0]].z);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_Verts[m_faceBlock.face[6].v[2]].x, -75.0f, g_Verts[m_faceBlock.face[6].v[2]].z);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[5].v[0]].x);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_Verts[m_faceBlock.face[6].v[2]].x, -75.0f, g_Verts[m_faceBlock.face[6].v[2]].z);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[6].v[2]].x);
        glEnd();

        if (g_QuadrantIndex == 0) {
            if (m_neighbours[3] && depth > 0)
                m_neighbours[3]->RenderStrataFace(depth - 1);
        } else if (g_QuadrantIndex == 3) {
            if (m_neighbours[2] && depth > 0)
                m_neighbours[2]->RenderStrataFace(depth - 1);
        }
    }

    if (m_neighbours[0] && m_neighbours[0]->getType() == 0x14 && (g_QuadrantIndex == 1 || g_QuadrantIndex == 2)) {
        glBegin(0x0004);
        glNormal3fv(&g_NormalTowardNb0[flip].x);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[2].v[2]].x);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(g_Verts[m_faceBlock.face[2].v[2]].x, -75.0f, g_Verts[m_faceBlock.face[2].v[2]].z);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_Verts[m_faceBlock.face[0].v[0]].x, -75.0f, g_Verts[m_faceBlock.face[0].v[0]].z);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[2].v[2]].x);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_Verts[m_faceBlock.face[0].v[0]].x, -75.0f, g_Verts[m_faceBlock.face[0].v[0]].z);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[0].v[0]].x);
        glEnd();

        if (g_QuadrantIndex == 1) {
            if (m_neighbours[3] && depth > 0)
                m_neighbours[3]->RenderStrataFace(depth - 1);
        } else if (g_QuadrantIndex == 2) {
            if (m_neighbours[2] && depth > 0)
                m_neighbours[2]->RenderStrataFace(depth - 1);
        }
    }

    if (m_neighbours[2] && m_neighbours[2]->getType() == 0x14 && (g_QuadrantIndex == 0 || g_QuadrantIndex == 1)) {
        glBegin(0x0004);
        glNormal3fv(&g_NormalTowardNb2[flip].x);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[0].v[0]].x);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(g_Verts[m_faceBlock.face[0].v[0]].x, -75.0f, g_Verts[m_faceBlock.face[0].v[0]].z);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_Verts[m_faceBlock.face[4].v[1]].x, -75.0f, g_Verts[m_faceBlock.face[4].v[1]].z);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[0].v[0]].x);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_Verts[m_faceBlock.face[4].v[1]].x, -75.0f, g_Verts[m_faceBlock.face[4].v[1]].z);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[4].v[1]].x);
        glEnd();

        if (g_QuadrantIndex == 0) {
            if (m_neighbours[0] && depth > 0)
                m_neighbours[0]->RenderStrataFace(depth - 1);
        } else if (g_QuadrantIndex == 1) {
            if (m_neighbours[1] && depth > 0)
                m_neighbours[1]->RenderStrataFace(depth - 1);
        }
    }

    if (m_neighbours[3] && m_neighbours[3]->getType() == 0x14 && (g_QuadrantIndex == 2 || g_QuadrantIndex == 3)) {
        glBegin(0x0004);
        glNormal3fv(&g_NormalTowardNb3[flip].x);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[7].v[1]].x);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(g_Verts[m_faceBlock.face[7].v[1]].x, -75.0f, g_Verts[m_faceBlock.face[7].v[1]].z);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_Verts[m_faceBlock.face[3].v[2]].x, -75.0f, g_Verts[m_faceBlock.face[3].v[2]].z);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[7].v[1]].x);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_Verts[m_faceBlock.face[3].v[2]].x, -75.0f, g_Verts[m_faceBlock.face[3].v[2]].z);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[3].v[2]].x);
        glEnd();

        if (g_QuadrantIndex == 2) {
            if (m_neighbours[1] && depth > 0)
                m_neighbours[1]->RenderStrataFace(depth - 1);
        } else if (g_QuadrantIndex == 3) {
            if (m_neighbours[0] && depth > 0)
                m_neighbours[0]->RenderStrataFace(depth - 1);
        }
    }
}

TilePathFlags::TilePathFlags()
{
    clear();
}

void TilePathFlags::clear()
{
    m_connected = false;
    m_hasPath = false;
    m_visible = false;
    f3 = false;
    f4 = false;
    f5 = false;
    f6 = false;
}

TileWallState::TileWallState()
{
    resetEnabledFlags();
}

void TileWallState::resetEnabledFlags()
{
    m_enabled[0] = false;
    m_enabled[2] = false;
    m_enabled[6] = false;
    m_enabled[4] = false;
}

void Tile::RenderTileWalls()
{
    int side;

    if (g_QuadrantIndex == 0 || g_QuadrantIndex == 2)
        side = 0;
    else
        side = 1;

    if (m_wallState.m_enabled[0] && m_neighbours[0] != 0) {
        if (m_wallState.m_wall[0] == 1)
            glBindTexture(0x0de1, g_TileTypes[35].glTexture);
        else
            glBindTexture(0x0de1, g_TileTypes[34].glTexture);
        glBegin(0x0004);
        glNormal3fv(&g_NormalTowardNb1[side].x);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[0]->m_faceBlock.face[5].v[0]].x);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[0].v[0]].x);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[1].v[2]].x);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[0]->m_faceBlock.face[5].v[0]].x);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[1].v[2]].x);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[0]->m_faceBlock.face[5].v[1]].x);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[0]->m_faceBlock.face[5].v[1]].x);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[1].v[2]].x);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[2].v[2]].x);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[0]->m_faceBlock.face[5].v[1]].x);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[2].v[2]].x);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[0]->m_faceBlock.face[6].v[2]].x);
        glEnd();
    }

    if (m_wallState.m_enabled[6] && m_neighbours[2] != 0) {
        if (m_wallState.m_wall[6] == 1)
            glBindTexture(0x0de1, g_TileTypes[35].glTexture);
        else
            glBindTexture(0x0de1, g_TileTypes[34].glTexture);
        glBegin(0x0004);
        glNormal3fv(&g_NormalTowardNb3[side].x);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[2]->m_faceBlock.face[7].v[1]].x);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[4].v[1]].x);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[4].v[0]].x);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[2]->m_faceBlock.face[7].v[1]].x);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[4].v[0]].x);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[2]->m_faceBlock.face[3].v[1]].x);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[2]->m_faceBlock.face[3].v[1]].x);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[4].v[0]].x);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[0].v[0]].x);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[2]->m_faceBlock.face[3].v[1]].x);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[0].v[0]].x);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[2]->m_faceBlock.face[3].v[2]].x);
        glEnd();
    }

    if (m_wallState.m_enabled[2] && m_neighbours[3] != 0) {
        if (m_wallState.m_wall[2] == 1)
            glBindTexture(0x0de1, g_TileTypes[35].glTexture);
        else
            glBindTexture(0x0de1, g_TileTypes[34].glTexture);
        glBegin(0x0004);
        glNormal3fv(&g_NormalTowardNb2[side].x);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[3]->m_faceBlock.face[0].v[0]].x);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[3].v[2]].x);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[3].v[1]].x);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[3]->m_faceBlock.face[0].v[0]].x);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[3].v[1]].x);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[3]->m_faceBlock.face[4].v[0]].x);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[3]->m_faceBlock.face[4].v[0]].x);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[3].v[1]].x);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[7].v[1]].x);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[3]->m_faceBlock.face[4].v[0]].x);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[7].v[1]].x);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[3]->m_faceBlock.face[4].v[1]].x);
        glEnd();
    }

    if (m_wallState.m_enabled[4] && m_neighbours[1] != 0) {
        if (m_wallState.m_wall[4] == 1)
            glBindTexture(0x0de1, g_TileTypes[35].glTexture);
        else
            glBindTexture(0x0de1, g_TileTypes[34].glTexture);
        glBegin(0x0004);
        glNormal3fv(&g_NormalTowardNb0[side].x);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[1]->m_faceBlock.face[2].v[2]].x);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[6].v[2]].x);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[5].v[1]].x);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[1]->m_faceBlock.face[2].v[2]].x);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[5].v[1]].x);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[1]->m_faceBlock.face[1].v[2]].x);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[1]->m_faceBlock.face[1].v[2]].x);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[5].v[1]].x);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[5].v[0]].x);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[1]->m_faceBlock.face[1].v[2]].x);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(&g_Verts[m_faceBlock.face[5].v[0]].x);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(&g_Verts[m_neighbours[1]->m_faceBlock.face[0].v[0]].x);
        glEnd();
    }
}

void Tile::RenderTilePath()
{
    int texBase;
    int mask;
    float inset;
    bool hasNb0, hasNb2, hasNb3, hasNb1;

    if (!m_pathFlags.m_visible)
        return;

    mask = 0;
    inset = 16.67f;

    hasNb0 = m_neighbours[0] != 0 && m_neighbours[0]->m_pathFlags.m_hasPath != 0;
    hasNb2 = m_neighbours[2] != 0 && m_neighbours[2]->m_pathFlags.m_hasPath != 0;
    hasNb3 = m_neighbours[3] != 0 && m_neighbours[3]->m_pathFlags.m_hasPath != 0;
    hasNb1 = m_neighbours[1] != 0 && m_neighbours[1]->m_pathFlags.m_hasPath != 0;

    if (m_pathFlags.m_connected)
        texBase = 0x21;
    else
        texBase = 0x20;

    glEnable(0x0be2);
    glBlendFunc(0x0302, 0x0303);
    glBindTexture(0x0de1, g_TileTypes[texBase].tex[0][0]);

    if (hasNb0) {
        mask += 1;
        glBegin(6);
        glNormal3fv(g_VertNormals[m_faceBlock.face[0].v[2]]);
        glTexCoord2f(0.333f, 0.667f);
        glVertex3f(g_Verts[m_faceBlock.face[0].v[2]].x - inset, g_Verts[m_faceBlock.face[0].v[2]].y,
                   g_Verts[m_faceBlock.face[0].v[2]].z);
        glNormal3fv(g_VertNormals[m_faceBlock.face[0].v[2]]);
        glTexCoord2f(0.667f, 0.667f);
        glVertex3f(inset + g_Verts[m_faceBlock.face[0].v[2]].x, g_Verts[m_faceBlock.face[0].v[2]].y,
                   g_Verts[m_faceBlock.face[0].v[2]].z);
        glNormal3fv(g_VertNormals[m_faceBlock.face[1].v[2]]);
        glTexCoord2f(0.667f, 1.0f);
        glVertex3f(inset + g_Verts[m_faceBlock.face[0].v[2]].x, g_Verts[m_faceBlock.face[1].v[2]].y,
                   g_Verts[m_faceBlock.face[1].v[2]].z);
        glNormal3fv(g_VertNormals[m_faceBlock.face[1].v[2]]);
        glTexCoord2f(0.333f, 1.0f);
        glVertex3f(g_Verts[m_faceBlock.face[0].v[2]].x - inset, g_Verts[m_faceBlock.face[1].v[2]].y,
                   g_Verts[m_faceBlock.face[1].v[2]].z);
        glEnd();
    }

    if (hasNb1) {
        mask += 2;
        glBindTexture(0x0de1, g_TileTypes[texBase].tex[0][0]);
        glBegin(6);
        glNormal3fv(g_VertNormals[m_faceBlock.face[0].v[2]]);
        glTexCoord2f(0.333f, 0.333f);
        glVertex3f(g_Verts[m_faceBlock.face[0].v[2]].x - inset, g_Verts[m_faceBlock.face[0].v[2]].y,
                   g_Verts[m_faceBlock.face[0].v[2]].z);
        glNormal3fv(g_VertNormals[m_faceBlock.face[6].v[1]]);
        glTexCoord2f(0.333f, 0.0f);
        glVertex3f(g_Verts[m_faceBlock.face[6].v[1]].x - inset, g_Verts[m_faceBlock.face[6].v[1]].y,
                   g_Verts[m_faceBlock.face[6].v[1]].z);
        glNormal3fv(g_VertNormals[m_faceBlock.face[6].v[2]]);
        glTexCoord2f(0.667f, 0.0f);
        glVertex3f(inset + g_Verts[m_faceBlock.face[6].v[1]].x, g_Verts[m_faceBlock.face[6].v[1]].y,
                   g_Verts[m_faceBlock.face[6].v[1]].z);
        glNormal3fv(g_VertNormals[m_faceBlock.face[0].v[2]]);
        glTexCoord2f(0.667f, 0.333f);
        glVertex3f(inset + g_Verts[m_faceBlock.face[0].v[2]].x, g_Verts[m_faceBlock.face[0].v[2]].y,
                   g_Verts[m_faceBlock.face[0].v[2]].z);
        glEnd();
    }

    if (hasNb3) {
        mask += 4;
        glBindTexture(0x0de1, g_TileTypes[texBase].tex[0][0]);
        glBegin(6);
        glNormal3fv(g_VertNormals[m_faceBlock.face[0].v[2]]);
        glTexCoord2f(0.667f, 0.667f);
        glVertex3f(g_Verts[m_faceBlock.face[0].v[2]].x, g_Verts[m_faceBlock.face[0].v[2]].y,
                   g_Verts[m_faceBlock.face[0].v[2]].z - inset);
        glNormal3fv(g_VertNormals[m_faceBlock.face[0].v[2]]);
        glTexCoord2f(0.667f, 0.333f);
        glVertex3f(g_Verts[m_faceBlock.face[0].v[2]].x, g_Verts[m_faceBlock.face[0].v[2]].y,
                   inset + g_Verts[m_faceBlock.face[0].v[2]].z);
        glNormal3fv(g_VertNormals[m_faceBlock.face[7].v[2]]);
        glTexCoord2f(1.0f, 0.333f);
        glVertex3f(g_Verts[m_faceBlock.face[7].v[2]].x, g_Verts[m_faceBlock.face[7].v[2]].y,
                   inset + g_Verts[m_faceBlock.face[7].v[2]].z);
        glNormal3fv(g_VertNormals[m_faceBlock.face[7].v[2]]);
        glTexCoord2f(1.0f, 0.667f);
        glVertex3f(g_Verts[m_faceBlock.face[7].v[2]].x, g_Verts[m_faceBlock.face[7].v[2]].y,
                   g_Verts[m_faceBlock.face[7].v[2]].z - inset);
        glEnd();
    }

    if (hasNb2) {
        mask += 8;
        glBindTexture(0x0de1, g_TileTypes[texBase].tex[0][0]);
        glBegin(6);
        glNormal3fv(g_VertNormals[m_faceBlock.face[0].v[2]]);
        glTexCoord2f(0.333f, 0.667f);
        glVertex3f(g_Verts[m_faceBlock.face[0].v[2]].x, g_Verts[m_faceBlock.face[0].v[2]].y,
                   g_Verts[m_faceBlock.face[0].v[2]].z - inset);
        glNormal3fv(g_VertNormals[m_faceBlock.face[4].v[0]]);
        glTexCoord2f(0.0f, 0.667f);
        glVertex3f(g_Verts[m_faceBlock.face[4].v[0]].x, g_Verts[m_faceBlock.face[4].v[0]].y,
                   g_Verts[m_faceBlock.face[4].v[0]].z - inset);
        glNormal3fv(g_VertNormals[m_faceBlock.face[4].v[0]]);
        glTexCoord2f(0.0f, 0.333f);
        glVertex3f(g_Verts[m_faceBlock.face[4].v[0]].x, g_Verts[m_faceBlock.face[4].v[0]].y,
                   inset + g_Verts[m_faceBlock.face[4].v[0]].z);
        glNormal3fv(g_VertNormals[m_faceBlock.face[0].v[2]]);
        glTexCoord2f(0.333f, 0.333f);
        glVertex3f(g_Verts[m_faceBlock.face[0].v[2]].x, g_Verts[m_faceBlock.face[0].v[2]].y,
                   inset + g_Verts[m_faceBlock.face[0].v[2]].z);
        glEnd();
    }

    RenderTileQuadrantsByTransitionMask(mask, texBase);
    glDisable(0x0be2);
}

void Tile::RenderTileQuadrantsByTransitionMask(int mask, int texBase)
{
    int texUL = 0, texUR = 0, texLL = 0, texLR = 0;

    switch (mask) {
    case 0: texLL = texLR = texUL = texUR = 3; break;
    case 1: texLR = texLL = 3; break;
    case 2: texUR = texUL = 3; break;
    case 4: texUL = texLL = 3; break;
    case 8: texUR = texLR = 3; break;
    case 5:
        texLL = 3;
        texUR = 2;
        break;
    case 9:
        texLR = 3;
        texUL = 2;
        break;
    case 7: texLR = texUR = 2; break;
    case 11: texLL = texUL = 2; break;
    case 13: texUR = texUL = 2; break;
    case 6:
        texUL = 3;
        texLR = 2;
        break;
    case 10:
        texUR = 3;
        texLL = 2;
        break;
    case 14: texLR = texLL = 2; break;
    case 15: texLL = texLR = texUL = texUR = 2; break;
    default: return;
    }

    if (texUL) {
        glBindTexture(0x0de1, g_TileTypes[texBase].tex[0][texUL]);
        glBegin(4);
        glTexCoord2f(0.0f, 0.5f);
        glArrayElement(m_faceBlock.face[0].v[1]);
        glTexCoord2f(0.5f, 0.5f);
        glArrayElement(m_faceBlock.face[0].v[2]);
        glTexCoord2f(0.5f, 1.0f);
        glArrayElement(m_faceBlock.face[1].v[2]);
        glEnd();
    }
    if (texUR) {
        glBindTexture(0x0de1, g_TileTypes[texBase].tex[0][texUR]);
        glBegin(4);
        glTexCoord2f(0.5f, 1.0f);
        glArrayElement(m_faceBlock.face[1].v[2]);
        glTexCoord2f(0.5f, 0.5f);
        glArrayElement(m_faceBlock.face[0].v[2]);
        glTexCoord2f(1.0f, 0.5f);
        glArrayElement(m_faceBlock.face[3].v[1]);
        glEnd();
    }
    if (texLL) {
        glBindTexture(0x0de1, g_TileTypes[texBase].tex[0][texLL]);
        glBegin(4);
        glTexCoord2f(0.5f, 0.0f);
        glArrayElement(m_faceBlock.face[5].v[1]);
        glTexCoord2f(0.5f, 0.5f);
        glArrayElement(m_faceBlock.face[4].v[2]);
        glTexCoord2f(0.0f, 0.5f);
        glArrayElement(m_faceBlock.face[4].v[0]);
        glEnd();
    }
    if (texLR) {
        glBindTexture(0x0de1, g_TileTypes[texBase].tex[0][texLR]);
        glBegin(4);
        glTexCoord2f(1.0f, 0.5f);
        glArrayElement(m_faceBlock.face[3].v[1]);
        glTexCoord2f(0.5f, 0.5f);
        glArrayElement(m_faceBlock.face[4].v[2]);
        glTexCoord2f(0.5f, 0.0f);
        glArrayElement(m_faceBlock.face[5].v[1]);
        glEnd();
    }
}

void Tile::CalcFaceNormal(Face *pFace)
{
    Vec3 edge1;
    Vec3 edge2;

    edge1.x = g_Verts[pFace->v[1]].x - g_Verts[pFace->v[0]].x;
    edge1.y = g_Verts[pFace->v[1]].y - g_Verts[pFace->v[0]].y;
    edge1.z = g_Verts[pFace->v[1]].z - g_Verts[pFace->v[0]].z;

    edge2.x = g_Verts[pFace->v[2]].x - g_Verts[pFace->v[1]].x;
    edge2.y = g_Verts[pFace->v[2]].y - g_Verts[pFace->v[1]].y;
    edge2.z = g_Verts[pFace->v[2]].z - g_Verts[pFace->v[1]].z;

    pFace->nrm[0] = edge1.y * edge2.z - edge1.z * edge2.y;
    pFace->nrm[1] = edge1.z * edge2.x - edge1.x * edge2.z;
    pFace->nrm[2] = edge1.x * edge2.y - edge1.y * edge2.x;

    Vec3_Normalize(pFace->nrm);
}

void Tile::CalcCornerVertexNormals(int nRowDir, int nColDir)
{
    FaceBlock *pRow = 0;
    FaceBlock *pCol = 0;
    FaceBlock *pDiag = 0;

    if (m_neighbours[nRowDir])
        pRow = m_neighbours[nRowDir]->GetFaceBlock();
    if (m_neighbours[nColDir]) {
        pCol = m_neighbours[nColDir]->GetFaceBlock();
        if (m_neighbours[nRowDir] && m_neighbours[nColDir]->m_neighbours[nRowDir])
            pDiag = m_neighbours[nColDir]->m_neighbours[nRowDir]->GetFaceBlock();
    }

    if (nRowDir == 0) {
        if (nColDir == 2) {
            CalcFaceNormal(&m_faceBlock.face[0]);
            CalcFaceNormal(&m_faceBlock.face[1]);
            CalcFaceNormal(&m_faceBlock.face[2]);
            CalcFaceNormal(&m_faceBlock.face[4]);
            if (pRow) {
                CalcFaceNormal(&pRow->face[4]);
                CalcFaceNormal(&pRow->face[5]);
                CalcFaceNormal(&pRow->face[6]);
            }
            if (pCol) {
                CalcFaceNormal(&pCol->face[2]);
                CalcFaceNormal(&pCol->face[3]);
                CalcFaceNormal(&pCol->face[7]);
            }
            if (pDiag) {
                CalcFaceNormal(&pDiag->face[6]);
                CalcFaceNormal(&pDiag->face[7]);
            }
            for (int i = 0; i < 3; i++) {
                g_VertNormals[m_faceBlock.face[1].v[2]][i] = m_faceBlock.face[1].nrm[i] + m_faceBlock.face[2].nrm[i];
                g_VertNormals[m_faceBlock.face[0].v[1]][i] = m_faceBlock.face[0].nrm[i] + m_faceBlock.face[4].nrm[i];
                g_VertNormals[m_faceBlock.face[0].v[0]][i] = m_faceBlock.face[0].nrm[i] + m_faceBlock.face[1].nrm[i];
                if (pRow) {
                    g_VertNormals[m_faceBlock.face[0].v[0]][i] += pRow->face[4].nrm[i] + pRow->face[5].nrm[i];
                    g_VertNormals[m_faceBlock.face[1].v[2]][i] += pRow->face[5].nrm[i] + pRow->face[6].nrm[i];
                }
                if (pCol) {
                    g_VertNormals[m_faceBlock.face[0].v[0]][i] += pCol->face[2].nrm[i] + pCol->face[3].nrm[i];
                    g_VertNormals[m_faceBlock.face[0].v[1]][i] += pCol->face[3].nrm[i] + pCol->face[7].nrm[i];
                }
                if (pDiag) {
                    g_VertNormals[m_faceBlock.face[0].v[0]][i] += pDiag->face[6].nrm[i] + pDiag->face[7].nrm[i];
                }
            }
            Vec3_Normalize(g_VertNormals[m_faceBlock.face[0].v[0]]);
            Vec3_Normalize(g_VertNormals[m_faceBlock.face[0].v[1]]);
            Vec3_Normalize(g_VertNormals[m_faceBlock.face[1].v[2]]);
        }
        if (nColDir == 3) {
            CalcFaceNormal(&m_faceBlock.face[2]);
            CalcFaceNormal(&m_faceBlock.face[3]);
            CalcFaceNormal(&m_faceBlock.face[7]);
            if (pRow) {
                CalcFaceNormal(&pRow->face[6]);
                CalcFaceNormal(&pRow->face[7]);
            }
            if (pCol) {
                CalcFaceNormal(&pCol->face[0]);
                CalcFaceNormal(&pCol->face[4]);
            }
            if (pDiag) {
                CalcFaceNormal(&pDiag->face[4]);
                CalcFaceNormal(&pDiag->face[5]);
            }
            for (int j = 0; j < 3; j++) {
                g_VertNormals[m_faceBlock.face[3].v[1]][j] = m_faceBlock.face[7].nrm[j] + m_faceBlock.face[3].nrm[j];
                g_VertNormals[m_faceBlock.face[3].v[2]][j] = m_faceBlock.face[2].nrm[j] + m_faceBlock.face[3].nrm[j];
                if (pRow) {
                    g_VertNormals[m_faceBlock.face[3].v[2]][j] += pRow->face[6].nrm[j] + pRow->face[7].nrm[j];
                }
                if (pCol) {
                    g_VertNormals[m_faceBlock.face[3].v[2]][j] += pCol->face[0].nrm[j] + pCol->face[1].nrm[j];
                    g_VertNormals[m_faceBlock.face[3].v[1]][j] += pCol->face[0].nrm[j] + pCol->face[4].nrm[j];
                }
                if (pDiag) {
                    g_VertNormals[m_faceBlock.face[3].v[2]][j] += pDiag->face[4].nrm[j] + pDiag->face[5].nrm[j];
                }
            }
            Vec3_Normalize(g_VertNormals[m_faceBlock.face[3].v[1]]);
            Vec3_Normalize(g_VertNormals[m_faceBlock.face[3].v[2]]);
        }
    } else if (nRowDir == 1) {
        if (nColDir == 2) {
            CalcFaceNormal(&m_faceBlock.face[4]);
            CalcFaceNormal(&m_faceBlock.face[5]);
            CalcFaceNormal(&m_faceBlock.face[6]);
            if (pRow) {
                CalcFaceNormal(&pRow->face[0]);
                CalcFaceNormal(&pRow->face[1]);
                CalcFaceNormal(&pRow->face[2]);
            }
            if (pCol) {
                CalcFaceNormal(&pCol->face[6]);
                CalcFaceNormal(&pCol->face[7]);
            }
            if (pDiag) {
                CalcFaceNormal(&pDiag->face[2]);
                CalcFaceNormal(&pDiag->face[3]);
            }
            for (int k = 0; k < 3; k++) {
                g_VertNormals[m_faceBlock.face[4].v[1]][k] = m_faceBlock.face[5].nrm[k] + m_faceBlock.face[4].nrm[k];
                g_VertNormals[m_faceBlock.face[5].v[1]][k] = m_faceBlock.face[5].nrm[k] + m_faceBlock.face[6].nrm[k];
                if (pRow) {
                    g_VertNormals[m_faceBlock.face[4].v[1]][k] += pRow->face[0].nrm[k] + pRow->face[1].nrm[k];
                    g_VertNormals[m_faceBlock.face[5].v[1]][k] += pRow->face[1].nrm[k] + pRow->face[2].nrm[k];
                }
                if (pCol) {
                    g_VertNormals[m_faceBlock.face[4].v[1]][k] += pCol->face[6].nrm[k] + pCol->face[7].nrm[k];
                }
                if (pDiag) {
                    g_VertNormals[m_faceBlock.face[4].v[1]][k] += pDiag->face[2].nrm[k] + pDiag->face[3].nrm[k];
                }
            }
            Vec3_Normalize(g_VertNormals[m_faceBlock.face[4].v[1]]);
            Vec3_Normalize(g_VertNormals[m_faceBlock.face[5].v[1]]);
        }
        if (nColDir == 3) {
            CalcFaceNormal(&m_faceBlock.face[6]);
            CalcFaceNormal(&m_faceBlock.face[7]);
            if (pRow) {
                CalcFaceNormal(&pRow->face[2]);
                CalcFaceNormal(&pRow->face[3]);
            }
            if (pCol) {
                CalcFaceNormal(&pCol->face[4]);
                CalcFaceNormal(&pCol->face[5]);
            }
            if (pDiag) {
                CalcFaceNormal(&pDiag->face[0]);
                CalcFaceNormal(&pDiag->face[1]);
            }
            for (int m = 0; m < 3; m++) {
                g_VertNormals[m_faceBlock.face[7].v[1]][m] = m_faceBlock.face[7].nrm[m] + m_faceBlock.face[6].nrm[m];
                if (pRow) {
                    g_VertNormals[m_faceBlock.face[7].v[1]][m] += pRow->face[2].nrm[m] + pRow->face[3].nrm[m];
                }
                if (pCol) {
                    g_VertNormals[m_faceBlock.face[7].v[1]][m] += pCol->face[4].nrm[m] + pCol->face[5].nrm[m];
                }
                if (pDiag) {
                    g_VertNormals[m_faceBlock.face[7].v[1]][m] += pDiag->face[0].nrm[m] + pDiag->face[1].nrm[m];
                }
            }
            Vec3_Normalize(g_VertNormals[m_faceBlock.face[7].v[1]]);
        }
    }
}

void Tile::calcVertexNormals(void)
{
    if (!m_normalsDirty)
        return;

    CalcCornerVertexNormals(0, 2);
    CalcCornerVertexNormals(1, 2);
    CalcCornerVertexNormals(0, 3);
    CalcCornerVertexNormals(1, 3);

    for (int i = 0; i <= 2; i++) {
        g_VertNormals[m_faceBlock.face[0].v[2]][i] = m_faceBlock.face[0].nrm[i] + m_faceBlock.face[1].nrm[i] +
                                                     m_faceBlock.face[2].nrm[i] + m_faceBlock.face[3].nrm[i] +
                                                     m_faceBlock.face[4].nrm[i] + m_faceBlock.face[5].nrm[i] +
                                                     m_faceBlock.face[6].nrm[i] + m_faceBlock.face[7].nrm[i];
    }

    Vec3_Normalize(g_VertNormals[m_faceBlock.face[0].v[2]]);
    m_normalsDirty = 0;
}

void SetCourseTextureId(int nType, int nRow, int nCol, int nId)
{
    g_TileTypes[nType].tex[nRow][nCol] = nId;
}

void Tile::SetFaceTexture(int iTileType, int iFace, int iSub)
{
    switch (m_type) {
    case 13:
    case 14:
    case 15:
    case 16: iTileType = 13; break;
    case 17:
        if (m_variation == 1)
            iTileType = 23;
        else if (m_variation == 2)
            iTileType = 24;
        break;
    case 6:
    case 21: iTileType = 4; break;
    case 22:
        if (m_variation >= 0x40 && m_variation < 0x48)
            iTileType = 4;
        break;
    case 1:
        if (m_variation & 0x80)
            iTileType = 26;
        break;
    case 7:
        switch (m_variation % 4) {
        case 0: iTileType = 27; break;
        case 1: iTileType = 28; break;
        case 2: iTileType = 29; break;
        case 3: iTileType = 30; break;
        }
        break;
    }

    if (iTileType == 4 || m_type == 7 || iTileType == 2) {
        if (iSub > 4)
            iSub = 0;
    }

    switch (iFace) {
    case 7:
        m_faceBlock.face[1].tex = g_TileTypes[iTileType].tex[m_texSet][iSub];
        m_faceBlock.face[1].texIdx = (char)iSub;
        break;
    case 0:
        m_faceBlock.face[2].tex = g_TileTypes[iTileType].tex[m_texSet][iSub];
        m_faceBlock.face[2].texIdx = (char)iSub;
        break;
    case 1:
        m_faceBlock.face[3].tex = g_TileTypes[iTileType].tex[m_texSet][iSub];
        m_faceBlock.face[3].texIdx = (char)iSub;
        break;
    case 2:
        m_faceBlock.face[7].tex = g_TileTypes[iTileType].tex[m_texSet][iSub];
        m_faceBlock.face[7].texIdx = (char)iSub;
        break;
    case 3:
        m_faceBlock.face[6].tex = g_TileTypes[iTileType].tex[m_texSet][iSub];
        m_faceBlock.face[6].texIdx = (char)iSub;
        break;
    case 4:
        m_faceBlock.face[5].tex = g_TileTypes[iTileType].tex[m_texSet][iSub];
        m_faceBlock.face[5].texIdx = (char)iSub;
        break;
    case 5:
        m_faceBlock.face[4].tex = g_TileTypes[iTileType].tex[m_texSet][iSub];
        m_faceBlock.face[4].texIdx = (char)iSub;
        break;
    case 6:
        m_faceBlock.face[0].tex = g_TileTypes[iTileType].tex[m_texSet][iSub];
        m_faceBlock.face[0].texIdx = (char)iSub;
        break;
    }
}

bool Tile::hasPath()
{
    return (m_pathFlags.m_hasPath);
}

bool Tile::hasConnectedPath()
{
    return (m_pathFlags.m_connected);
}

void Tile::setConnectedPath(int bOn)
{
    if (bOn)
        m_pathFlags.m_connected = true;
    else
        m_pathFlags.m_connected = false;
}

void Tile::setPath(bool on, int also)
{
    if (on) {
        m_pathFlags.m_hasPath = 1;
        if (also)
            m_pathFlags.m_connected = 1;
        if (m_type == 0x16 || m_type == 0 || m_type == 2 || m_type == 1 || m_type == 3 || m_type == 7 || m_type == 9)
            m_pathFlags.m_visible = 0;
        else
            m_pathFlags.m_visible = 1;
    } else {
        m_pathFlags.m_hasPath = 0;
        m_pathFlags.m_connected = 0;
        m_pathFlags.m_visible = 0;
    }
}

void Tile::drawEdgeTransitionsWithNeighbours()
{
    DrawTileEdgeTransitionsForType(m_type);

    if (m_neighbours[0])
        m_neighbours[0]->DrawTileEdgeTransitionsForType(-1);
    if (m_neighbours[0] && m_neighbours[2])
        m_neighbours[0]->m_neighbours[2]->DrawTileEdgeTransitionsForType(-1);
    if (m_neighbours[2])
        m_neighbours[2]->DrawTileEdgeTransitionsForType(-1);
    if (m_neighbours[1] && m_neighbours[2])
        m_neighbours[1]->m_neighbours[2]->DrawTileEdgeTransitionsForType(-1);
    if (m_neighbours[1])
        m_neighbours[1]->DrawTileEdgeTransitionsForType(-1);
    if (m_neighbours[1] && m_neighbours[3])
        m_neighbours[1]->m_neighbours[3]->DrawTileEdgeTransitionsForType(-1);
    if (m_neighbours[3])
        m_neighbours[3]->DrawTileEdgeTransitionsForType(-1);
    if (m_neighbours[0] && m_neighbours[3])
        m_neighbours[0]->m_neighbours[3]->DrawTileEdgeTransitionsForType(-1);
}

void Tile::DrawTileEdgeTransitionsForType(int iType)
{
    int code = 0;
    int idx = 0;
    unsigned char bDesertUp = 0;
    unsigned char bDesertDown = 0;
    unsigned char bDesertLeft = 0;
    unsigned char bDesertRight = 0;
    unsigned char bDesertUpLeft = 0;
    unsigned char bDesertUpRight = 0;
    unsigned char bDesertDownLeft = 0;
    unsigned char bDesertDownRight = 0;

    if (iType == -1)
        iType = m_type;
    if (iType == 20)
        return;

    int codeUp = getNeighborTransitionCode(m_neighbours[0]);
    int codeLeft = getNeighborTransitionCode(m_neighbours[2]);
    int codeRight = getNeighborTransitionCode(m_neighbours[3]);
    int codeDown = getNeighborTransitionCode(m_neighbours[1]);
    int codeUpLeft = 0;
    int codeUpRight = 0;
    int codeDownLeft = 0;
    int codeDownRight = 0;

    if (!typeDrawsEdgeTransitions(iType))
        return;

    if (m_neighbours[0]) {
        if (g_nCourseType == 1 && iType == 17 && m_neighbours[0]->typeAllowsEdgeTransition())
            bDesertUp = 1;
        if (m_neighbours[2]) {
            if (g_nCourseType == 1 && iType == 17 && m_neighbours[0]->m_neighbours[2]->typeAllowsEdgeTransition())
                bDesertUpLeft = 1;
            codeUpLeft = getNeighborTransitionCode(m_neighbours[0]->m_neighbours[2]);
        }
        if (m_neighbours[3]) {
            if (g_nCourseType == 1 && iType == 17 && m_neighbours[0]->m_neighbours[3]->typeAllowsEdgeTransition())
                bDesertUpRight = 1;
            codeUpRight = getNeighborTransitionCode(m_neighbours[0]->m_neighbours[3]);
        }
    }
    if (m_neighbours[1]) {
        if (g_nCourseType == 1 && iType == 17 && m_neighbours[1]->typeAllowsEdgeTransition())
            bDesertDown = 1;
        if (m_neighbours[2]) {
            if (g_nCourseType == 1 && iType == 17 && m_neighbours[1]->m_neighbours[2]->typeAllowsEdgeTransition())
                bDesertDownLeft = 1;
            codeDownLeft = getNeighborTransitionCode(m_neighbours[1]->m_neighbours[2]);
        }
        if (m_neighbours[3]) {
            if (g_nCourseType == 1 && iType == 17 && m_neighbours[1]->m_neighbours[3]->typeAllowsEdgeTransition())
                bDesertDownRight = 1;
            codeDownRight = getNeighborTransitionCode(m_neighbours[1]->m_neighbours[3]);
        }
    }
    if (m_neighbours[2]) {
        if (g_nCourseType == 1 && iType == 17 && m_neighbours[2]->typeAllowsEdgeTransition())
            bDesertLeft = 1;
    }
    if (m_neighbours[3]) {
        if (g_nCourseType == 1 && iType == 17 && m_neighbours[3]->typeAllowsEdgeTransition())
            bDesertRight = 1;
    }

    code = PackEdgeTransitionCode(codeUp, codeLeft, codeUpLeft);
    idx = MapTypeCodeToIndex(code);
    if (bDesertUp)
        iType = 25;
    SetFaceTexture(iType, 7, idx);
    if (bDesertUp)
        iType = 17;

    code = PackEdgeTransitionCode(codeUp, codeRight, codeUpRight);
    idx = MapTypeCodeToIndex(code);
    if (bDesertUp)
        iType = 25;
    SetFaceTexture(iType, 0, idx);
    if (bDesertUp)
        iType = 17;

    code = PackEdgeTransitionCode(codeRight, codeUp, codeUpRight);
    idx = MapTypeCodeToIndex(code);
    if (bDesertRight)
        iType = 25;
    SetFaceTexture(iType, 1, idx);
    if (bDesertRight)
        iType = 17;

    code = PackEdgeTransitionCode(codeRight, codeDown, codeDownRight);
    idx = MapTypeCodeToIndex(code);
    if (bDesertRight)
        iType = 25;
    SetFaceTexture(iType, 2, idx);
    if (bDesertRight)
        iType = 17;

    code = PackEdgeTransitionCode(codeDown, codeRight, codeDownRight);
    idx = MapTypeCodeToIndex(code);
    if (bDesertDown)
        iType = 25;
    SetFaceTexture(iType, 3, idx);
    if (bDesertDown)
        iType = 17;

    code = PackEdgeTransitionCode(codeDown, codeLeft, codeDownLeft);
    idx = MapTypeCodeToIndex(code);
    if (bDesertDown)
        iType = 25;
    SetFaceTexture(iType, 4, idx);
    if (bDesertDown)
        iType = 17;

    code = PackEdgeTransitionCode(codeLeft, codeUp, codeUpLeft);
    idx = MapTypeCodeToIndex(code);
    if (bDesertLeft)
        iType = 25;
    SetFaceTexture(iType, 6, idx);
    if (bDesertLeft)
        iType = 17;

    code = PackEdgeTransitionCode(codeLeft, codeDown, codeDownLeft);
    idx = MapTypeCodeToIndex(code);
    if (bDesertLeft)
        iType = 25;
    SetFaceTexture(iType, 5, idx);
    if (bDesertLeft)
        iType = 17;
}

int Tile::MapTypeCodeToIndex(int code)
{
    switch (code) {
    case 0:
    case 1:
    case 2:
    case 10:
    case 12:
    case 20:
    case 21: return 0;
    case 3:
    case 43: return 2;
    case 4:
    case 14:
    case 24:
    case 34: return 4;
    case 5:
    case 7:
    case 25: return 3;
    case 6:
    case 16: return 1;
    case 40:
    case 41:
    case 42:
    case 61: return 8;
    case 50:
    case 52:
    case 70: return 7;
    case 60: return 5;
    case 30: return 6;
    }
    return 0;
}

int Tile::PackEdgeTransitionCode(int nEdge, int nSide, int nCorner)
{
    int v = 0;

    if (nEdge == 1)
        v += 4;
    else if (nEdge == 2)
        v += 40;

    if (nSide == 1)
        v += 1;
    else if (nSide == 2)
        v += 10;

    if (nCorner == 1)
        v += 2;
    else if (nCorner == 2)
        v += 20;

    return v;
}

float (*GetVertexArrayBase())[3]
{
    return (float (*)[3])g_Verts;
}

float (*GetNormalArrayBase())[3]
{
    return g_VertNormals;
}

void Tile::setType(int iType)
{
    if (m_pathFlags.m_hasPath)
        m_pathFlags.m_visible = 1;

    if (m_type == 7)
        UndoType7ElevationDrop();
    if (m_type == 6)
        UndoType6ElevationRaise();
    else if (m_type == 0x11)
        m_normalsDirty = 1;

    m_type = iType;

    switch (iType) {
    case 7: LowerTileType7Heights(); break;
    case 17: updateHasPathForType(iType); break;
    case 6: RaiseTileType6Heights(); break;
    case 13:
    case 14:
    case 15:
    case 16: updateHasPathForType(13); break;
    case 0:
    case 9: assignFaceTextures(); break;
    case 4:
    case 20: drawEdgeTransitionsWithNeighbours(); break;
    default: updateHasPathForType(iType); break;
    }
}

void Tile::UndoType6ElevationRaise()
{
    g_Verts[m_faceBlock.face[0].v[2]].y -= 20.0f;

    if (m_neighbours[0] && m_neighbours[0]->getType() == 6) {
        g_Verts[m_faceBlock.face[1].v[2]].y -= 20.0f;
        g_Verts[m_neighbours[0]->GetFaceBlock()->face[5].v[1]].y -= 20.0f;
        if (m_neighbours[2] && m_neighbours[2]->getType() == 6 && m_neighbours[0]->m_neighbours[2]->getType() == 6) {
            g_Verts[m_faceBlock.face[0].v[0]].y += 20.0f;
            g_Verts[m_neighbours[0]->GetFaceBlock()->face[4].v[1]].y -= 20.0f;
            g_Verts[m_neighbours[2]->GetFaceBlock()->face[3].v[2]].y -= 20.0f;
            g_Verts[m_neighbours[0]->m_neighbours[2]->GetFaceBlock()->face[7].v[1]].y -= 20.0f;
        }
        if (m_neighbours[3] && m_neighbours[3]->getType() == 6 && m_neighbours[0]->m_neighbours[3]->getType() == 6) {
            g_Verts[m_faceBlock.face[3].v[2]].y -= 20.0f;
            g_Verts[m_neighbours[0]->GetFaceBlock()->face[7].v[1]].y -= 20.0f;
            g_Verts[m_neighbours[3]->GetFaceBlock()->face[0].v[0]].y -= 20.0f;
            g_Verts[m_neighbours[0]->m_neighbours[3]->GetFaceBlock()->face[4].v[1]].y -= 20.0f;
        }
    }
    if (m_neighbours[1] && m_neighbours[1]->getType() == 6) {
        g_Verts[m_faceBlock.face[5].v[1]].y -= 20.0f;
        g_Verts[m_neighbours[1]->GetFaceBlock()->face[1].v[2]].y -= 20.0f;
        if (m_neighbours[2] && m_neighbours[2]->getType() == 6 && m_neighbours[1]->m_neighbours[2]->getType() == 6) {
            g_Verts[m_faceBlock.face[5].v[0]].y -= 20.0f;
            g_Verts[m_neighbours[1]->GetFaceBlock()->face[0].v[0]].y -= 20.0f;
            g_Verts[m_neighbours[2]->GetFaceBlock()->face[7].v[1]].y -= 20.0f;
            g_Verts[m_neighbours[1]->m_neighbours[2]->GetFaceBlock()->face[3].v[2]].y -= 20.0f;
        }
        if (m_neighbours[3] && m_neighbours[3]->getType() == 6 && m_neighbours[1]->m_neighbours[3]->getType() == 6) {
            g_Verts[m_faceBlock.face[7].v[1]].y -= 20.0f;
            g_Verts[m_neighbours[1]->GetFaceBlock()->face[3].v[2]].y -= 20.0f;
            g_Verts[m_neighbours[3]->GetFaceBlock()->face[4].v[1]].y -= 20.0f;
            g_Verts[m_neighbours[1]->m_neighbours[3]->GetFaceBlock()->face[0].v[0]].y -= 20.0f;
        }
    }
    if (m_neighbours[2] && m_neighbours[2]->getType() == 6) {
        g_Verts[m_faceBlock.face[0].v[1]].y -= 20.0f;
        g_Verts[m_neighbours[2]->GetFaceBlock()->face[3].v[1]].y -= 20.0f;
    }
    if (m_neighbours[3] && m_neighbours[3]->getType() == 6) {
        g_Verts[m_faceBlock.face[3].v[1]].y -= 20.0f;
        g_Verts[m_neighbours[3]->GetFaceBlock()->face[0].v[1]].y -= 20.0f;
    }

    m_normalsDirty = 1;
    if (m_neighbours[0])
        m_neighbours[0]->m_normalsDirty = 1;
    if (m_neighbours[0] && m_neighbours[2])
        m_neighbours[0]->m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[0] && m_neighbours[3])
        m_neighbours[0]->m_neighbours[3]->m_normalsDirty = 1;
    if (m_neighbours[2])
        m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[3])
        m_neighbours[3]->m_normalsDirty = 1;
    if (m_neighbours[1])
        m_neighbours[1]->m_normalsDirty = 1;
    if (m_neighbours[1] && m_neighbours[2])
        m_neighbours[1]->m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[1] && m_neighbours[3])
        m_neighbours[1]->m_neighbours[3]->m_normalsDirty = 1;
}

void Tile::UndoType7ElevationDrop()
{
    g_Verts[m_faceBlock.face[0].v[2]].y += 13.0f;

    if (m_neighbours[0] && m_neighbours[0]->getType() == 7) {
        g_Verts[m_faceBlock.face[1].v[2]].y += 13.0f;
        g_Verts[m_neighbours[0]->GetFaceBlock()->face[5].v[1]].y += 13.0f;
        if (m_neighbours[2] && m_neighbours[2]->getType() == 7 && m_neighbours[0]->m_neighbours[2]->getType() == 7) {
            g_Verts[m_faceBlock.face[0].v[0]].y += 13.0f;
            g_Verts[m_neighbours[0]->GetFaceBlock()->face[4].v[1]].y += 13.0f;
            g_Verts[m_neighbours[2]->GetFaceBlock()->face[3].v[2]].y += 13.0f;
            g_Verts[m_neighbours[0]->m_neighbours[2]->GetFaceBlock()->face[7].v[1]].y += 13.0f;
        }
        if (m_neighbours[3] && m_neighbours[3]->getType() == 7 && m_neighbours[0]->m_neighbours[3]->getType() == 7) {
            g_Verts[m_faceBlock.face[3].v[2]].y += 13.0f;
            g_Verts[m_neighbours[0]->GetFaceBlock()->face[7].v[1]].y += 13.0f;
            g_Verts[m_neighbours[3]->GetFaceBlock()->face[0].v[0]].y += 13.0f;
            g_Verts[m_neighbours[0]->m_neighbours[3]->GetFaceBlock()->face[4].v[1]].y += 13.0f;
        }
    }
    if (m_neighbours[1] && m_neighbours[1]->getType() == 7) {
        g_Verts[m_faceBlock.face[5].v[1]].y += 13.0f;
        g_Verts[m_neighbours[1]->GetFaceBlock()->face[1].v[2]].y += 13.0f;
        if (m_neighbours[2] && m_neighbours[2]->getType() == 7 && m_neighbours[1]->m_neighbours[2]->getType() == 7) {
            g_Verts[m_faceBlock.face[5].v[0]].y += 13.0f;
            g_Verts[m_neighbours[1]->GetFaceBlock()->face[0].v[0]].y += 13.0f;
            g_Verts[m_neighbours[2]->GetFaceBlock()->face[7].v[1]].y += 13.0f;
            g_Verts[m_neighbours[1]->m_neighbours[2]->GetFaceBlock()->face[3].v[2]].y += 13.0f;
        }
        if (m_neighbours[3] && m_neighbours[3]->getType() == 7 && m_neighbours[1]->m_neighbours[3]->getType() == 7) {
            g_Verts[m_faceBlock.face[7].v[1]].y += 13.0f;
            g_Verts[m_neighbours[1]->GetFaceBlock()->face[3].v[2]].y += 13.0f;
            g_Verts[m_neighbours[3]->GetFaceBlock()->face[4].v[1]].y += 13.0f;
            g_Verts[m_neighbours[1]->m_neighbours[3]->GetFaceBlock()->face[0].v[0]].y += 13.0f;
        }
    }
    if (m_neighbours[2] && m_neighbours[2]->getType() == 7) {
        g_Verts[m_faceBlock.face[0].v[1]].y += 13.0f;
        g_Verts[m_neighbours[2]->GetFaceBlock()->face[3].v[1]].y += 13.0f;
    }
    if (m_neighbours[3] && m_neighbours[3]->getType() == 7) {
        g_Verts[m_faceBlock.face[3].v[1]].y += 13.0f;
        g_Verts[m_neighbours[3]->GetFaceBlock()->face[0].v[1]].y += 13.0f;
    }

    m_normalsDirty = 1;
    if (m_neighbours[0])
        m_neighbours[0]->m_normalsDirty = 1;
    if (m_neighbours[0] && m_neighbours[2])
        m_neighbours[0]->m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[0] && m_neighbours[3])
        m_neighbours[0]->m_neighbours[3]->m_normalsDirty = 1;
    if (m_neighbours[2])
        m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[3])
        m_neighbours[3]->m_normalsDirty = 1;
    if (m_neighbours[1])
        m_neighbours[1]->m_normalsDirty = 1;
    if (m_neighbours[1] && m_neighbours[2])
        m_neighbours[1]->m_neighbours[2]->m_normalsDirty = 1;
    if (m_neighbours[1] && m_neighbours[3])
        m_neighbours[1]->m_neighbours[3]->m_normalsDirty = 1;
}

void Tile::assignFaceTextures()
{
    int i;
    int j;

    if (m_pathFlags.m_hasPath)
        m_pathFlags.m_visible = 0;

    if (m_type == 0) {
        for (i = 0; i < m_faceBlock.numFaces; i++)
            m_faceBlock.face[i].tex = g_TileTypes[0].tex[m_variation][0];
    } else {
        for (j = 0; j < m_faceBlock.numFaces; j++)
            m_faceBlock.face[j].tex = g_TileTypes[m_type].tex[m_texSet][3];
    }

    drawEdgeTransitionsWithNeighbours();
}

char Tile::getVariation()
{
    return ((char)m_variation);
}

void Tile::setVariation(int n)
{
    m_variation = n;
}

FaceBlock *Tile::GetFaceBlock()
{
    return (&m_faceBlock);
}

void Tile::setWall(int side, int value, bool enabled)
{
    m_wallState.m_enabled[side] = enabled;
    m_wallState.m_wall[side] = value;
}

bool Tile::isTypeSkipStrataFace()
{
    return m_type == 20;
}

unsigned char Tile::typeDrawsEdgeTransitions(int type)
{
    int r;

    if (type != 9 && type != 0)
        r = 1;
    else
        r = 0;

    return (unsigned char)r;
}

int Tile::maxCornerHeight()
{
    int maxEdge0 = m_cornerElevation[1] > m_cornerElevation[7] ? m_cornerElevation[1] : m_cornerElevation[7];
    int maxEdge4 = m_cornerElevation[3] > m_cornerElevation[5] ? m_cornerElevation[3] : m_cornerElevation[5];
    return maxEdge0 > maxEdge4 ? maxEdge0 : maxEdge4;
}

unsigned char Tile::typeAllowsEdgeTransition()
{
    int r;

    if (m_type != 2 && m_type != 7 && m_type != 1 && m_type != 0 && m_type != 9 && m_type != 8 && m_type != 3)
        r = 0;
    else
        r = 1;

    return (unsigned char)r;
}

int Tile::getNeighborTransitionCode(Tile *n)
{
    if (n == 0)
        return 0;

    if (m_type == 0x11) {
        if (m_variation == 0 && m_type != n->getType())
            return 1;
        if (m_variation != n->getVariation()) {
            if (m_variation == 0 || (m_variation == 1 && n->getVariation() == 2))
                return 2;
            return 1;
        }
        return 0;
    }

    if (g_pCollar[m_type] != g_pCollar[n->getType()])
        return 1;
    if (n->getType() != m_type)
        return 2;
    return 0;
}
