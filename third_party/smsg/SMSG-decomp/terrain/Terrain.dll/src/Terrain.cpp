#include <assert.h>
#include <fstream.h>
#include <stdio.h>
#include <stdlib.h>

#include "Terrain.h"
#include "Tile.h"
#include "GL/gl.h"
#include "GL/glu.h"
#include "Bmp.h"

float powf(float _X, float _Y);
int factorial(int n);

const float kScreenToWorldScale = 2.2097087f;

#define VIEWPORT_HALF_WIDTH     432
#define VIEWPORT_HALF_HEIGHT    300

#define WORLD_UNITS_PER_CELL    100.0f
#define ISO_YAW_DEGREES         45.0f
#define TILE_HALF_EXTENT        50.0f
#define GRID_CENTRE_CELL        25
#define SKIRT_OFFSET            10.0f

void SetCourseTextureId(int nType, int nIndex, int nSub, int uTex);

void rotateVectorAboutAxis(float fAngle, float fX, float fY, float fZ, float *pv);
void Vec3_Normalize(float *v);
unsigned int LoadTgaTexture(const char *pszFile);

#include <string.h>

static PIXELFORMATDESCRIPTOR g_pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL,
                                       PFD_TYPE_RGBA, 16 };

float degreesToRadians(float fDegrees)
{
    return fDegrees * g_fDegToRad;
}

Terrain::Terrain(long nWidth, long nHeight)
{
    int i, j, k, l, m;

    g_pCollar = 0;
    m_width = nWidth;
    m_height = nHeight;
    m_zoomLevel = 0;
    m_ambient[0] = 0.15f;
    m_ambient[1] = 0.15f;
    m_ambient[2] = 0.2f;
    m_ambient[3] = 1.0f;
    m_hRC = 0;
    m_bLoaded = 0;

    for (i = 0; i < m_width; i++)
        for (j = 0; j < m_height; j++)
            m_tiles[i + j * m_width].reset(i, j);

    m_flipVertical = 0;
    g_nCourseType = 0;

    for (k = 0; k < 0x25; k++)
        m_courseTypes[k].nVariants = 0;

    for (k = 0; k < 0x25; k++)
        for (l = 0; l < 0x19; l++)
            for (m = 0; m < 0x9; m++)
                g_TileTypes[k].tex[l][m] = 0;

    strcpy(m_courseTypes[0].szName, "Tee");
    strcpy(m_courseTypes[1].szName, "PuttingGreen");
    strcpy(m_courseTypes[2].szName, "Fairway");
    strcpy(m_courseTypes[3].szName, "FirmFairway");
    strcpy(m_courseTypes[4].szName, "Rough");
    strcpy(m_courseTypes[5].szName, "DeepRough");
    strcpy(m_courseTypes[8].szName, "GrassySand");
    strcpy(m_courseTypes[9].szName, "PotSandBunker");
    strcpy(m_courseTypes[10].szName, "Overgrowth");
    strcpy(m_courseTypes[11].szName, "Brush");
    strcpy(m_courseTypes[12].szName, "Rock");
    strcpy(m_courseTypes[13].szName, "Woods");
    strcpy(m_courseTypes[17].szName, "WaterShallow");
    strcpy(m_courseTypes[18].szName, "Marsh");
    strcpy(m_courseTypes[19].szName, "Overgrowth");
    strcpy(m_courseTypes[22].szName, "Building");
    strcpy(m_courseTypes[24].szName, "WaterDeep");
    strcpy(m_courseTypes[23].szName, "WaterMiddle");
    strcpy(m_courseTypes[25].szName, "WaterShallowDesert");
    strcpy(m_courseTypes[26].szName, "TrickyGreen");
    strcpy(m_courseTypes[27].szName, "SandBunker1");
    strcpy(m_courseTypes[28].szName, "SandBunker2");
    strcpy(m_courseTypes[29].szName, "SandBunker3");
    strcpy(m_courseTypes[30].szName, "SandBunker4");

    LoadCourseTextures();
}

void Tile::setRandomVariation(int n)
{
    m_texSet = n;
}

Terrain::~Terrain()
{
    m_width = -1;
    m_height = -1;

    if (g_pCollar) {
        delete g_pCollar;
        g_pCollar = 0;
    }
    if (g_pInstance) {
        g_pInstance = 0;
    }

    freeCourseTextures();
}

Terrain *Terrain::getInstance()
{
    if (g_pInstance == 0)
        g_pInstance = new Terrain(0x32, 0x32);
    return g_pInstance;
}
Vec3 *Terrain::getNormalArray()
{
    return (Vec3 *)GetNormalArrayBase();
}

Vec3 *Terrain::getVertexArray()
{
    return (Vec3 *)GetVertexArrayBase();
}

void Terrain::setType(Tile *pTile, int iType, int iVariation)
{
    if (m_courseTypes[iType].nVariants > 0)
        pTile->setRandomVariation(rand() % m_courseTypes[iType].nVariants);
    else
        pTile->setRandomVariation(0);

    pTile->setVariation(iVariation);
    pTile->setType(iType);
}
int Terrain::getVariation(Tile *pTile)
{
    return pTile->getVariation();
}

bool Terrain::initGLRenderState()
{
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);

    if (m_flipVertical)
        glFrontFace(GL_CW);
    else
        glFrontFace(GL_CCW);

    glEnable(GL_CULL_FACE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    return true;
}

void Terrain::changeLighting(int brighter)
{
    if (brighter) {
        if (m_ambient[0] < 1.0f) {
            m_ambient[0] += 0.1f;
            m_ambient[1] += 0.1f;
            m_ambient[2] += 0.1f;
        }
    } else {
        if (m_ambient[0] > 0.1f) {
            m_ambient[0] -= 0.1f;
            m_ambient[1] -= 0.1f;
            m_ambient[2] -= 0.1f;
        }
    }

    float lightDir[4] = { -0.5f, 0.1f, -1.0f, 0.0f };
    rotateVectorAboutAxis(40.0f, 1.0f, 0.0f, 0.0f, lightDir);
    rotateVectorAboutAxis(45.0f, 0.0f, 1.0f, 0.0f, lightDir);
    Vec3_Normalize(lightDir);

    float diffuse[4] = { 0.9f, 0.9f, 0.9f, 1.0f };
    float specular[4] = { 1.0f, 1.0f, 0.9f, 1.0f };
    float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    glLightfv(0x4000, 0x1203, lightDir);
    glLightfv(0x4000, 0x1200, m_ambient);
    glLightfv(0x4000, 0x1201, diffuse);
    glLightfv(0x4000, 0x1202, specular);
    glEnable(0x4000);
}

bool Terrain::initLighting()
{
    LoadLightingForCourseType();
    return true;
}

void Terrain::EnableBlackFillLight()
{
    glDisable(0x4000);

    float ambient[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float diffuse[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float specular[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    glLightfv(0x4001, 0x1200, ambient);
    glLightfv(0x4001, 0x1201, diffuse);
    glLightfv(0x4001, 0x1202, specular);
    glEnable(0x4001);
}

void Terrain::LoadLightingForCourseType()
{
    switch (g_nCourseType) {
    case 0: this->LoadLightingFromFile("ParklandLighting.txt"); break;
    case 2: this->LoadLightingFromFile("TropicalLighting.txt"); break;
    case 1: this->LoadLightingFromFile("DesertLighting.txt"); break;
    case 3: this->LoadLightingFromFile("LinksLighting.txt"); break;
    default: this->LoadLightingFromFile("lighting.txt"); break;
    }
}

void Terrain::LoadLightingFromFile(const char *pszFile)
{
    float amb[3];
    float dif[3];
    float spec[3];
    char szLine[256];

    ifstream ifs(pszFile, ios::nocreate);
    if (ifs.fail())
        return;

    ifs.getline(szLine, 256);
    while (strcmp(strtok(szLine, " "), "#AMBIENT") != 0)
        ifs.getline(szLine, 256);
    ifs.getline(szLine, 256);
    amb[0] = atoi(strtok(szLine, " ")) / 255.0f;
    amb[1] = atoi(strtok(0, " ")) / 255.0f;
    amb[2] = atoi(strtok(0, " ")) / 255.0f;

    ifs.getline(szLine, 256);
    while (strcmp(strtok(szLine, " "), "#DIFFUSE") != 0)
        ifs.getline(szLine, 256);
    ifs.getline(szLine, 256);
    dif[0] = atoi(strtok(szLine, " ")) / 255.0f;
    dif[1] = atoi(strtok(0, " ")) / 255.0f;
    dif[2] = atoi(strtok(0, " ")) / 255.0f;

    ifs.getline(szLine, 256);
    while (strcmp(strtok(szLine, " "), "#SPECULAR") != 0)
        ifs.getline(szLine, 256);
    ifs.getline(szLine, 256);
    spec[0] = atoi(strtok(szLine, " ")) / 255.0f;
    spec[1] = atoi(strtok(0, " ")) / 255.0f;
    spec[2] = atoi(strtok(0, " ")) / 255.0f;

    float lightPos[4] = { -0.5f, 0.1f, -1.0f, 0.0f };
    rotateVectorAboutAxis(40.0f, 1.0f, 0.0f, 0.0f, lightPos);
    rotateVectorAboutAxis(45.0f, 0.0f, 1.0f, 0.0f, lightPos);
    Vec3_Normalize(lightPos);

    float matSpec[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    glLightfv(0x4000, 0x1203, lightPos);
    glLightfv(0x4000, 0x1200, amb);
    glLightfv(0x4000, 0x1201, dif);
    glLightfv(0x4000, 0x1202, spec);
    glEnable(0x4000);
    glMaterialfv(0x404, 0x1202, matSpec);
    glMateriali(0x404, 0x1601, 13);

    ifs.close();
}

void rotateVectorAboutAxis(float fDegrees, float ax, float ay, float az, float *v)
{
    float rad = degreesToRadians(fDegrees);
    float c = (float)cos(rad);
    float s = (float)sin(rad);

    float m00 = 1.0f, m01 = 0.0f, m02 = 0.0f;
    float m10 = 0.0f, m11 = 1.0f, m12 = 0.0f;
    float m20 = 0.0f, m21 = 0.0f, m22 = 1.0f;

    if (ax != 0.0f && ay == 0.0f && az == 0.0f) {
        m11 = c;
        m12 = -s;
        m21 = s;
        m22 = c;
        v[1] = m11 * v[1] + m12 * v[2];
        v[2] = m21 * v[1] + m22 * v[2];
    } else if (ax == 0.0f && ay != 0.0f && az == 0.0f) {
        m00 = c;
        m02 = s;
        m20 = -s;
        m22 = c;
        v[0] = m00 * v[0] + m02 * v[2];
        v[2] = m20 * v[0] + m22 * v[2];
    } else if (ax == 0.0f && ay == 0.0f && az != 0.0f) {
        m00 = c;
        m01 = -s;
        m10 = s;
        m11 = c;
        v[0] = m00 * v[0] + m01 * v[1];
        v[1] = m10 * v[0] + m11 * v[1];
    }
}

void Vec3_Normalize(float *v)
{
    float len;

    len = (float)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] = v[0] / len;
    v[1] = v[1] / len;
    v[2] = v[2] / len;
}

void Terrain::freeCourseTextures()
{
    int i;
    int j;
    int k;

    if (!m_bLoaded)
        return;

    for (i = 0; i < 37; i++) {
        for (j = 0; j < 25; j++) {
            glDeleteTextures(9, g_TileTypes[i].tex[j]);
            for (k = 0; k < 9; k++)
                g_TileTypes[i].tex[j][k] = 0;
        }
        m_courseTypes[i].nVariants = 0;
    }

    m_bLoaded = false;
}
void Terrain::drawCircle(Tile *t, float r)
{
}

void Terrain::drawLine(int iX1, int iY1, int iX2, int iY2, int iColour, int iWidth, int iAlpha)
{
    float fRed;
    float fGreen;
    float fBlue;

    fRed = ((iColour >> 7) & 0xf8) / 255.0f;
    fGreen = ((iColour >> 2) & 0xf8) / 255.0f;
    fBlue = ((iColour << 3) & 0xf8) / 255.0f;

    glMatrixMode(0x1701);
    glPushMatrix();
    glLoadIdentity();

    if (m_flipVertical)
        glOrtho(0.0, m_screenWidth, 0.0, m_screenHeight, -1.0, 1.0);
    else
        glOrtho(0.0, m_screenWidth, m_screenHeight, 0.0, -1.0, 1.0);

    glMatrixMode(0x1700);
    glPushMatrix();
    glLoadIdentity();

    glDisable(0x0b50);
    glDisable(0x0de1);
    glEnable(0x0be2);
    glBlendFunc(0x0302, 0x0303);
    glLineWidth(iWidth);
    glColor4f(fRed, fGreen, fBlue, iAlpha / 10.0f);

    glBegin(1);
    glVertex2i(iX1, iY1);
    glVertex2i(iX2, iY2);
    glEnd();

    glEnable(0x0b50);
    glEnable(0x0de1);
    glDisable(0x0be2);

    glPopMatrix();
    glMatrixMode(0x1701);
    glPopMatrix();
    glMatrixMode(0x1700);
    glFlush();
}

void Terrain::drawCardinalSpline(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, int colour, int iWidth,
                                 int iAlpha)
{
    float r, g, b;
    Point2D pt, p0, p1, m0, m1;
    float t;

    r = ((colour >> 7) & 0xf8) / 255.0f;
    g = ((colour >> 2) & 0xf8) / 255.0f;
    b = ((colour << 3) & 0xf8) / 255.0f;

    m0.x = (float)(x0 - x2);
    m0.y = (float)(y0 - y2);
    m1.x = (float)(x3 - x1);
    m1.y = (float)(y3 - y1);
    p0.x = (float)x0;
    p0.y = (float)y0;
    p1.x = (float)x1;
    p1.y = (float)y1;

    glMatrixMode(0x1701);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, m_screenWidth, m_screenHeight, 0, -1, 1);
    glMatrixMode(0x1700);
    glPushMatrix();
    glLoadIdentity();

    glDisable(0xb50);
    glDisable(0xde1);
    glEnable(0xbe2);
    glBlendFunc(0x302, 0x303);
    glLineWidth((float)iWidth);
    glColor4f(r, g, b, iAlpha / 10.0f);

    glBegin(3);
    glVertex2fv(&p0.x);
    for (t = 0.01f; t < 1.0f; t += 0.01f) {
        evalCardinalSplinePoint2D(&pt, t, &p0, &p1, &m0, &m1);
        glVertex2fv(&pt.x);
    }
    glVertex2fv(&p1.x);
    glEnd();

    glEnable(0xb50);
    glEnable(0xde1);
    glDisable(0xbe2);

    glPopMatrix();
    glMatrixMode(0x1701);
    glPopMatrix();
    glMatrixMode(0x1700);
    glFlush();
}

void Terrain::evalCardinalSplinePoint2D(Point2D *pOut, float t, Point2D *p0, Point2D *p1, Point2D *m0, Point2D *m1)
{
    float t3 = (float)pow((double)t, 3.0);
    float t2 = (float)pow((double)t, 2.0);

    pOut->x = 0.0f;
    pOut->x = (2.0f * t3 - 3.0f * t2 + 1.0f) * p0->x + (-2.0f * t3 + 3.0f * t2) * p1->x + (t3 - 2.0f * t2 + t) * m0->x +
              (t3 - t2) * m1->x;

    pOut->y = 0.0f;
    pOut->y = (2.0f * t3 - 3.0f * t2 + 1.0f) * p0->y + (-2.0f * t3 + 3.0f * t2) * p1->y + (t3 - 2.0f * t2 + t) * m0->y +
              (t3 - t2) * m1->y;
}

void Terrain::drawBezierSpline(int x0, int y0, int x1, int y1, int x2, int y2, int colour, int iWidth, int iAlpha)
{
    float r, g, b;
    float pt[2];
    float cp[3][2];
    float w;
    float fStep;
    float t;
    int i, j;

    r = ((colour >> 7) & 0xf8) / 255.0f;
    g = ((colour >> 2) & 0xf8) / 255.0f;
    b = ((colour << 3) & 0xf8) / 255.0f;

    fStep = 1.0f / ((abs(x0 - x1) + abs(y0 - y1) + abs(x1 - x2) + abs(y1 - y2)) / 10.0f);

    cp[0][0] = (float)x0;
    cp[0][1] = (float)y0;
    cp[1][0] = (float)x1;
    cp[1][1] = (float)y1;
    cp[2][0] = (float)x2;
    cp[2][1] = (float)y2;

    glMatrixMode(0x1701);
    glPushMatrix();
    glLoadIdentity();

    if (m_flipVertical)
        glOrtho(0, m_screenWidth, 0, m_screenHeight, -1, 1);
    else
        glOrtho(0, m_screenWidth, m_screenHeight, 0, -1, 1);

    glMatrixMode(0x1700);
    glPushMatrix();
    glLoadIdentity();

    glDisable(0xb50);
    glDisable(0xde1);
    glEnable(0xbe2);
    glBlendFunc(0x302, 0x303);
    glLineWidth((float)iWidth);
    glColor4f(r, g, b, iAlpha / 10.0f);

    glBegin(3);
    glVertex2fv(cp[0]);
    for (t = 0.0f + fStep; t < 1.0f; t += fStep) {
        pt[0] = 0.0f;
        pt[1] = 0.0f;
        for (i = 0; i < 3; i++) {
            w = bernsteinBasisTerm(t, i);
            for (j = 0; j < 2; j++)
                pt[j] = w * cp[i][j] + pt[j];
        }
        glVertex2fv(pt);
    }
    glVertex2fv(cp[2]);
    glEnd();

    glEnable(0xb50);
    glEnable(0xde1);
    glDisable(0xbe2);

    glPopMatrix();
    glMatrixMode(0x1701);
    glPopMatrix();
    glMatrixMode(0x1700);
    glFlush();
}
float Terrain::bernsteinBasisTerm(float t, int i)
{
    float c = factorial(2) / (float)(factorial(i) * factorial(2 - i));
    return powf(t, (float)i) * c * powf(1.0f - t, (float)(2 - i));
}
float powf(float _X, float _Y)
{
    return ((float)pow((double)_X, (double)_Y));
}
int factorial(int n)
{
    int f;
    int i;

    if (n == 0)
        return (1);

    f = n;
    for (i = n; i > 2; i--)
        f *= i - 1;

    return (f);
}

int Tile::getGridX()
{
    return m_gridX;
}

bool Terrain::render(Tile *pCentre, float fAngle)
{
    int nUp, nDown, nLeft, nRight;
    Tile *pTile = 0;

    nUp = nDown = nLeft = nRight = 0x10 << m_zoomLevel;

    glLoadIdentity();
    glPushMatrix();
    glRotated(g_fCameraPitchDegrees, 1.0, 0.0, 0.0);
    glRotatef(ISO_YAW_DEGREES + fAngle, 0.0f, 1.0f, 0.0f);

    if (pCentre != 0)
        glTranslatef((float)(GRID_CENTRE_CELL - pCentre->getGridX()) * WORLD_UNITS_PER_CELL, 0.0f,
                     (float)(GRID_CENTRE_CELL - pCentre->getGridY()) * WORLD_UNITS_PER_CELL);

    SetRotationQuadrantFromDegrees(fAngle);

    if (pCentre == 0) {
        for (int i = 0xa; i < 0x28; i++)
            for (int j = 0x28; j > 0xa; j--)
                tileAt(j, i)->RenderTile();
    } else {
        if (pCentre->getGridY() < nUp)
            nUp = pCentre->getGridY();
        if (0x32 - pCentre->getGridY() < nDown)
            nDown = 0x32 - pCentre->getGridY();
        if (pCentre->getGridX() < nLeft)
            nLeft = pCentre->getGridX();
        if (0x32 - pCentre->getGridX() < nRight)
            nRight = 0x32 - pCentre->getGridX();

        if (fAngle == 0.0f) {
            for (int i = pCentre->getGridY() - nUp; i < pCentre->getGridY() + nDown; i++) {
                for (int j = pCentre->getGridX() + nRight; j >= pCentre->getGridX() - nLeft; j--) {
                    pTile = tileAt(j, i);
                    if (pTile != 0 && !pTile->isTypeSkipStrataFace() && !IsTileCulled(pTile, pCentre, fAngle))
                        pTile->RenderTile();
                }
            }
        } else if (fAngle == 90.0f) {
            for (int j = pCentre->getGridX() + nRight; j >= pCentre->getGridX() - nLeft; j--) {
                for (int i = pCentre->getGridY() + nDown; i >= pCentre->getGridY() - nUp; i--) {
                    pTile = tileAt(j, i);
                    if (pTile != 0 && !pTile->isTypeSkipStrataFace() && !IsTileCulled(pTile, pCentre, fAngle))
                        pTile->RenderTile();
                }
            }
        } else if (fAngle == 180.0f) {
            for (int i = pCentre->getGridY() + nDown; i >= pCentre->getGridY() - nUp; i--) {
                for (int j = pCentre->getGridX() - nLeft; j < pCentre->getGridX() + nRight; j++) {
                    pTile = tileAt(j, i);
                    if (pTile != 0 && !pTile->isTypeSkipStrataFace() && !IsTileCulled(pTile, pCentre, fAngle))
                        pTile->RenderTile();
                }
            }
        } else {
            for (int i = pCentre->getGridY() - nUp; i < pCentre->getGridY() + nDown; i++) {
                for (int j = pCentre->getGridX() - nLeft; j < pCentre->getGridX() + nRight; j++) {
                    pTile = tileAt(j, i);
                    if (pTile != 0 && !pTile->isTypeSkipStrataFace() && !IsTileCulled(pTile, pCentre, fAngle))
                        pTile->RenderTile();
                }
            }
        }
    }

    glPopMatrix();
    glFlush();
    return true;
}

void Terrain::pathUpdateRender(Tile *pCentre, float fAngle)
{
    int nUp, nDown, nLeft, nRight;
    Tile *pTile;
    int i, j;

    nUp = nDown = nLeft = nRight = 0xf << m_zoomLevel;

    glLoadIdentity();
    glPushMatrix();
    glRotated(g_fCameraPitchDegrees, 1.0, 0.0, 0.0);
    glRotatef(ISO_YAW_DEGREES + fAngle, 0.0f, 1.0f, 0.0f);

    if (pCentre != 0)
        glTranslatef((float)(GRID_CENTRE_CELL - pCentre->getGridX()) * WORLD_UNITS_PER_CELL, 0.0f,
                     (float)(GRID_CENTRE_CELL - pCentre->getGridY()) * WORLD_UNITS_PER_CELL);

    if (pCentre->getGridY() < nUp)
        nUp = pCentre->getGridY();
    if (0x32 - pCentre->getGridY() < nDown)
        nDown = 0x32 - pCentre->getGridY();
    if (pCentre->getGridX() < nLeft)
        nLeft = pCentre->getGridX() + 1;
    if (0x31 - pCentre->getGridX() < nRight)
        nRight = 0x31 - pCentre->getGridX();

    for (i = pCentre->getGridY() - nUp; i < pCentre->getGridY() + nDown; i++) {
        for (j = pCentre->getGridX() + nRight; j > pCentre->getGridX() - nLeft; j--) {
            pTile = tileAt(j, i);
            if (pTile->hasPath() && !pTile->isTypeSkipStrataFace() && !IsTileCulled(pTile, pCentre, fAngle)) {
                InsertTileInDrawOrder(pTile, pCentre, fAngle);
                InsertWallAdjacentTilesInDrawOrder(pTile, pCentre, fAngle);
            }
        }
    }

    for (TileIter it = m_drawList.begin(); it != m_drawList.end(); it++)
        (*it)->RenderTile();

    m_drawList.clear();

    glPopMatrix();
    glFlush();
}

int Tile::getGridY()
{
    return m_gridY;
}

bool Terrain::IsTileCulled(Tile *pTile, Tile *pCentre, float fAngle)
{
    int sx, sy;
    int marginX, marginY;
    int dy, dx;
    int tileW, tileHalfW, tileH, tileHalfH;

    if (m_zoomLevel == 2)
        return false;

    marginX = (m_screenWidth >> 1) * m_zoomLevel;
    marginY = (m_screenHeight >> 1) * m_zoomLevel;

    dy = pTile->getGridY() - pCentre->getGridY();
    dx = pTile->getGridX() - pCentre->getGridX();

    if (m_screenWidth == 0x320 && m_screenHeight == 0x258) {
        tileW = 0x40;
        tileH = 0x28;
        tileHalfW = 0x20;
        tileHalfH = 0x14;
    } else if (m_screenWidth == 0x400 && m_screenHeight == 0x300) {
        tileW = 0x50;
        tileH = 0x34;
        tileHalfW = 0x28;
        tileHalfH = 0x1a;
    } else if (m_screenWidth == 0x500 && m_screenHeight == 0x400) {
        tileW = 0x68;
        tileH = 0x44;
        tileHalfW = 0x34;
        tileHalfH = 0x22;
    }

    if (fAngle == 0.0) {
        sx = (m_screenWidth >> 1) + dy * tileHalfW + dx * tileHalfW;
        sy = (m_screenHeight >> 1) + dy * tileHalfH - dx * tileHalfH;
        sy -= pTile->maxCornerHeight() * 5;
        return sx > m_screenWidth + marginX || sx < -tileW - marginX || sy > m_screenHeight + tileH + marginY ||
               sy < -tileH - marginY;
    } else if (fAngle == 90.0) {
        sx = (m_screenWidth >> 1) - dy * tileHalfW + dx * tileHalfW;
        sy = (m_screenHeight >> 1) - dy * tileHalfH - dx * tileHalfH;
        sy -= pTile->maxCornerHeight() * 5;
        return sx > m_screenWidth + tileHalfW + marginX || sx < -tileW - marginX ||
               sy > m_screenHeight + tileH + marginY || sy < -tileH - marginY;
    } else if (fAngle == 180.0) {
        sx = (m_screenWidth >> 1) - dy * tileHalfW - dx * tileHalfW;
        sy = (m_screenHeight >> 1) - dy * tileHalfH + dx * tileHalfH;
        sy -= pTile->maxCornerHeight() * 5;
        return sx > m_screenWidth + tileW + marginX || sx < -tileHalfW - marginX ||
               sy > m_screenHeight + tileH + marginY || sy < -tileH - marginY;
    } else {
        sx = (m_screenWidth >> 1) + dy * tileHalfW - dx * tileHalfW;
        sy = (m_screenHeight >> 1) + dy * tileHalfH + dx * tileHalfH;
        sy -= pTile->maxCornerHeight() * 5;
        return sx > m_screenWidth + tileHalfW + marginX || sx < -tileHalfW - marginX ||
               sy > m_screenHeight + tileHalfH + marginY || sy < -tileH - tileHalfH - marginY;
    }
}

void Terrain::InsertWallAdjacentTilesInDrawOrder(Tile *pTile, Tile *pCentre, float fAngle)
{
    Tile *pAdj[3];
    int gridX;
    int gridY;
    int i;

    gridX = pTile->getGridX();
    gridY = pTile->getGridY();

    pAdj[0] = 0;
    pAdj[1] = 0;
    pAdj[2] = 0;

    if (fAngle == 0.0f) {
        if (getWall(pTile, 6)) {
            pAdj[0] = tileAt(gridX - 1, gridY);
            pAdj[2] = tileAt(gridX - 1, gridY + 1);
        }
        if (getWall(pTile, 4)) {
            pAdj[1] = tileAt(gridX, gridY + 1);
            pAdj[2] = tileAt(gridX - 1, gridY + 1);
        }
    } else if (fAngle == 90.0f) {
        if (getWall(pTile, 6)) {
            pAdj[0] = tileAt(gridX - 1, gridY);
            pAdj[2] = tileAt(gridX - 1, gridY - 1);
        }
        if (getWall(pTile, 0)) {
            pAdj[1] = tileAt(gridX, gridY - 1);
            pAdj[2] = tileAt(gridX - 1, gridY - 1);
        }
    } else if (fAngle == 180.0f) {
        if (getWall(pTile, 2)) {
            pAdj[0] = tileAt(gridX + 1, gridY);
            pAdj[2] = tileAt(gridX + 1, gridY - 1);
        }
        if (getWall(pTile, 0)) {
            pAdj[1] = tileAt(gridX, gridY - 1);
            pAdj[2] = tileAt(gridX + 1, gridY - 1);
        }
    } else {
        if (getWall(pTile, 2)) {
            pAdj[0] = tileAt(gridX + 1, gridY);
            pAdj[2] = tileAt(gridX + 1, gridY + 1);
        }
        if (getWall(pTile, 4)) {
            pAdj[1] = tileAt(gridX, gridY + 1);
            pAdj[2] = tileAt(gridX + 1, gridY + 1);
        }
    }

    for (i = 0; i < 3; i++) {
        if (pAdj[i]) {
            if (!pAdj[i]->isTypeSkipStrataFace()) {
                if (!IsTileCulled(pAdj[i], pCentre, fAngle)) {
                    InsertTileInDrawOrder(pAdj[i], pCentre, fAngle);
                    InsertWallAdjacentTilesInDrawOrder(pAdj[i], pCentre, fAngle);
                }
            }
        }
    }
}

void Terrain::LoadCourseTextures()
{
    char szPath[260] = "./Data/Textures/";
    char szFile[260];
    GLuint texId = 0;
    int len;
    int count = 0;

    if (m_bLoaded)
        freeCourseTextures();

    switch (g_nCourseType) {
    case 0: strcat(szPath, "Parkland/"); break;
    case 2: strcat(szPath, "Tropical/"); break;
    case 1: strcat(szPath, "Desert/"); break;
    case 3: strcat(szPath, "Links/"); break;
    default: strcat(szPath, "Parkland/"); break;
    }

    for (int i = 0; i < 31; i++) {
        if (i == 0) {
            strcpy(szFile, szPath);
            strcat(szFile, m_courseTypes[i].szName);
            strcat(szFile, "A0001.bmp");
            for (int c = '1'; c < 0x3a; c++) {
                szFile[strlen(szFile) - 5] = c;
                texId = UploadBmpTexture(szFile, 0, GL_LINEAR, GL_LINEAR, GL_REPEAT);
                SetCourseTextureId(0, count++, 0, texId);
            }
            for (int d = '1'; d < '3'; d++) {
                szFile[strlen(szFile) - 6] = d;
                for (int e = '0'; e < 0x3a; e++) {
                    if (count > 0x18)
                        continue;
                    szFile[strlen(szFile) - 5] = e;
                    texId = UploadBmpTexture(szFile, 0, GL_LINEAR, GL_LINEAR, GL_REPEAT);
                    SetCourseTextureId(0, count++, 0, texId);
                }
            }
        } else if (m_courseTypes[i].szName != 0) {
            for (int a = 'A'; a < 'F'; a++) {
                strcpy(szFile, szPath);
                strcat(szFile, m_courseTypes[i].szName);
                len = strlen(szFile);
                szFile[len] = a;
                szFile[len + 1] = 0;
                strcat(szFile, "0001.bmp");
                texId = UploadBmpTexture(szFile, 0, GL_LINEAR, GL_LINEAR, GL_REPEAT);
                if (texId != 0) {
                    SetCourseTextureId(i, m_courseTypes[i].nVariants, 0, texId);
                    for (int b = '2'; b < 0x3a; b++) {
                        szFile[strlen(szFile) - 5] = b;
                        texId = UploadBmpTexture(szFile, 0, GL_LINEAR, GL_LINEAR, GL_REPEAT);
                        SetCourseTextureId(i, m_courseTypes[i].nVariants, b - '1', texId);
                    }
                    m_courseTypes[i].nVariants++;
                } else {
                    break;
                }
            }
        }
    }

    strcpy(szFile, szPath);
    strcat(szFile, "/Path.tga");
    SetCourseTextureId(0x20, 0, 0, LoadTgaTexture(szFile));
    strcpy(szFile, szPath);
    strcat(szFile, "/PathCap.tga");
    SetCourseTextureId(0x20, 0, 1, LoadTgaTexture(szFile));
    strcpy(szFile, szPath);
    strcat(szFile, "/PathInside.tga");
    SetCourseTextureId(0x20, 0, 2, LoadTgaTexture(szFile));
    strcpy(szFile, szPath);
    strcat(szFile, "/PathCurve.tga");
    SetCourseTextureId(0x20, 0, 3, LoadTgaTexture(szFile));
    m_courseTypes[32].nVariants++;

    strcpy(szFile, szPath);
    strcat(szFile, "/PathX.tga");
    SetCourseTextureId(0x21, 0, 0, LoadTgaTexture(szFile));
    strcpy(szFile, szPath);
    strcat(szFile, "/PathCapX.tga");
    SetCourseTextureId(0x21, 0, 1, LoadTgaTexture(szFile));
    strcpy(szFile, szPath);
    strcat(szFile, "/PathInsideX.tga");
    SetCourseTextureId(0x21, 0, 2, LoadTgaTexture(szFile));
    strcpy(szFile, szPath);
    strcat(szFile, "/PathCurveX.tga");
    SetCourseTextureId(0x21, 0, 3, LoadTgaTexture(szFile));
    m_courseTypes[33].nVariants++;

    strcpy(szFile, szPath);
    strcat(szFile, "/CliffTest.bmp");
    SetCourseTextureId(0x23, 0, 0, UploadBmpTexture(szFile, 0, GL_LINEAR, GL_LINEAR, GL_REPEAT));
    m_courseTypes[35].nVariants++;

    strcpy(szFile, szPath);
    strcat(szFile, "/RetainWallA.bmp");
    SetCourseTextureId(0x22, 0, 0, UploadBmpTexture(szFile, 0, GL_LINEAR, GL_LINEAR, GL_REPEAT));
    m_courseTypes[34].nVariants++;

    strcpy(szFile, szPath);
    strcat(szFile, "/strata.bmp");
    SetCourseTextureId(0x24, 0, 0, UploadBmpTexture(szFile, 0, GL_LINEAR, GL_LINEAR, GL_REPEAT));
    m_courseTypes[36].nVariants++;

    m_bLoaded = true;
}

#line 1458 "C:\\Projects\\3DTerrainLowPoly\\Terrain.cpp"
void Terrain::renderTile(int iTileType, int screenX, int screenY, int num, int den)
{
    float scale;
    float x;
    float y;

    scale = (float)num / den;
    x = (float)(screenX - VIEWPORT_HALF_WIDTH);
    y = (float)(screenY - VIEWPORT_HALF_HEIGHT);

    x = x * kScreenToWorldScale;
    y = y * kScreenToWorldScale;

    glLoadIdentity();
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glRotated(g_fCameraPitchDegrees, 1.0, 0.0, 0.0);
    glRotatef(ISO_YAW_DEGREES, 0.0f, 1.0f, 0.0f);
    glScalef(scale, scale, scale);

    assert(iTileType < NUM_TEXTURED_TILES);

    glDisable(GL_LIGHTING);
    glBindTexture(GL_TEXTURE_2D, g_TileTypes[iTileType].glTexture);
    glBegin(GL_TRIANGLES);

    glNormal3f(0.0f, 1.0f, 0.0f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-TILE_HALF_EXTENT, 0.0f, -TILE_HALF_EXTENT);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-TILE_HALF_EXTENT, 0.0f, TILE_HALF_EXTENT);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(TILE_HALF_EXTENT, 0.0f, TILE_HALF_EXTENT);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-TILE_HALF_EXTENT, 0.0f, -TILE_HALF_EXTENT);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(TILE_HALF_EXTENT, 0.0f, TILE_HALF_EXTENT);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(TILE_HALF_EXTENT, 0.0f, -TILE_HALF_EXTENT);

    glEnd();
    glEnable(GL_LIGHTING);
    glPopMatrix();
    glFlush();
}














void Terrain::RenderTileEdgeSkirt(Tile *pTile, Tile *pGridTile, float fAngle)
{
    assert(pTile != NULL);

    bool bDrawSkirt = false;

    glLoadIdentity();
    glPushMatrix();
    glRotated(g_fCameraPitchDegrees, 1.0, 0.0, 0.0);
    glRotatef(ISO_YAW_DEGREES + fAngle, 0.0f, 1.0f, 0.0f);



    if (pTile != NULL) {
        glTranslatef((GRID_CENTRE_CELL - pGridTile->getGridX()) * WORLD_UNITS_PER_CELL, 0.0f,
                     (GRID_CENTRE_CELL - pGridTile->getGridY()) * WORLD_UNITS_PER_CELL);
    }

    this->SetRotationQuadrantFromDegrees(fAngle);

    if (fAngle == 0.0f) {
        if (this->tileAt(pTile->getGridX() + 1, pTile->getGridY()) == NULL ||
            this->tileAt(pTile->getGridX() + 1, pTile->getGridY())->isTypeSkipStrataFace() ||
            this->tileAt(pTile->getGridX(), pTile->getGridY() - 1) == NULL ||
            this->tileAt(pTile->getGridX(), pTile->getGridY() - 1)->isTypeSkipStrataFace() ||
            this->tileAt(pTile->getGridX() + 1, pTile->getGridY() - 1) == NULL ||
            this->tileAt(pTile->getGridX() + 1, pTile->getGridY() - 1)->isTypeSkipStrataFace())
            bDrawSkirt = true;
    } else if (fAngle == 90.0f) {
        if (this->tileAt(pTile->getGridX() + 1, pTile->getGridY()) == NULL ||
            this->tileAt(pTile->getGridX() + 1, pTile->getGridY())->isTypeSkipStrataFace() ||
            this->tileAt(pTile->getGridX(), pTile->getGridY() + 1) == NULL ||
            this->tileAt(pTile->getGridX(), pTile->getGridY() + 1)->isTypeSkipStrataFace() ||
            this->tileAt(pTile->getGridX() + 1, pTile->getGridY() + 1) == NULL ||
            this->tileAt(pTile->getGridX() + 1, pTile->getGridY() + 1)->isTypeSkipStrataFace())
            bDrawSkirt = true;
    } else if (fAngle == 180.0f) {
        if (this->tileAt(pTile->getGridX() - 1, pTile->getGridY()) == NULL ||
            this->tileAt(pTile->getGridX() - 1, pTile->getGridY())->isTypeSkipStrataFace() ||
            this->tileAt(pTile->getGridX(), pTile->getGridY() + 1) == NULL ||
            this->tileAt(pTile->getGridX(), pTile->getGridY() + 1)->isTypeSkipStrataFace() ||
            this->tileAt(pTile->getGridX() - 1, pTile->getGridY() + 1) == NULL ||
            this->tileAt(pTile->getGridX() - 1, pTile->getGridY() + 1)->isTypeSkipStrataFace())
            bDrawSkirt = true;
    } else {
        if (this->tileAt(pTile->getGridX() - 1, pTile->getGridY()) == NULL ||
            this->tileAt(pTile->getGridX() - 1, pTile->getGridY())->isTypeSkipStrataFace() ||
            this->tileAt(pTile->getGridX(), pTile->getGridY() - 1) == NULL ||
            this->tileAt(pTile->getGridX(), pTile->getGridY() - 1)->isTypeSkipStrataFace() ||
            this->tileAt(pTile->getGridX() - 1, pTile->getGridY() - 1) == NULL ||
            this->tileAt(pTile->getGridX() - 1, pTile->getGridY() - 1)->isTypeSkipStrataFace())
            bDrawSkirt = true;
    }

    if (bDrawSkirt) {
        this->EnableBlackFillLight();
        glTranslatef(SKIRT_OFFSET, 0.0f, -SKIRT_OFFSET);
        pTile->RenderTile();
        glTranslatef(-2.0f * SKIRT_OFFSET, 0.0f, 0.0f);
        pTile->RenderTile();
        glTranslatef(0.0f, 0.0f, 2.0f * SKIRT_OFFSET);
        pTile->RenderTile();
        glTranslatef(2.0f * SKIRT_OFFSET, 0.0f, 0.0f);
        pTile->RenderTile();
        glTranslatef(-SKIRT_OFFSET, 0.0f, -SKIRT_OFFSET);
        pTile->RenderTile();
        glDisable(GL_LIGHT1);
        glEnable(GL_LIGHT0);
    }

    glPopMatrix();
    glFlush();
}












void Terrain::localRender(Tile *pTile, Tile *pCameraTile, float rotationDegrees)
{
    Tile *pNeighbour;
    int row0, col0, col90, row90, row180, col180, rowE, colE;
    assert(pTile != NULL);

    glLoadIdentity();
    glPushMatrix();
    glRotated(g_fCameraPitchDegrees, 1.0, 0.0, 0.0);
    glRotatef(ISO_YAW_DEGREES + rotationDegrees, 0.0f, 1.0f, 0.0f);

    if (pTile != NULL)
        glTranslatef((float)(GRID_CENTRE_CELL - pCameraTile->getGridX()) * WORLD_UNITS_PER_CELL, 0.0f,
                     (float)(GRID_CENTRE_CELL - pCameraTile->getGridY()) * WORLD_UNITS_PER_CELL);

    SetRotationQuadrantFromDegrees(rotationDegrees);

    if (rotationDegrees == 0.0f) {
        for (row0 = pTile->getGridY() - 2; row0 <= pTile->getGridY() + 2; row0++) {
            for (col0 = pTile->getGridX() + 2; col0 >= pTile->getGridX() - 2; col0--) {
                pNeighbour = tileAt(col0, row0);
                if (pNeighbour != NULL) {
                    if (pNeighbour->isTypeSkipStrataFace())
                        continue;
                    InsertTileInDrawOrder(pNeighbour, pTile, rotationDegrees);
                    InsertWallAdjacentTilesInDrawOrder(pNeighbour, pTile, rotationDegrees);
                }
            }
        }
    } else if (rotationDegrees == 90.0f) {
        for (col90 = pTile->getGridX() + 2; col90 >= pTile->getGridX() - 2; col90--) {
            for (row90 = pTile->getGridY() + 2; row90 >= pTile->getGridY() - 2; row90--) {
                pNeighbour = tileAt(col90, row90);
                if (pNeighbour != NULL) {
                    if (pNeighbour->isTypeSkipStrataFace())
                        continue;
                    InsertTileInDrawOrder(pNeighbour, pTile, rotationDegrees);
                    InsertWallAdjacentTilesInDrawOrder(pNeighbour, pTile, rotationDegrees);
                }
            }
        }
    } else if (rotationDegrees == 180.0f) {
        for (row180 = pTile->getGridY() + 2; row180 >= pTile->getGridY() - 2; row180--) {
            for (col180 = pTile->getGridX() - 2; col180 <= pTile->getGridX() + 2; col180++) {
                pNeighbour = tileAt(col180, row180);
                if (pNeighbour != NULL) {
                    if (pNeighbour->isTypeSkipStrataFace())
                        continue;
                    InsertTileInDrawOrder(pNeighbour, pTile, rotationDegrees);
                    InsertWallAdjacentTilesInDrawOrder(pNeighbour, pTile, rotationDegrees);
                }
            }
        }
    } else {
        for (rowE = pTile->getGridY() - 2; rowE <= pTile->getGridY() + 2; rowE++) {
            for (colE = pTile->getGridX() - 2; colE <= pTile->getGridX() + 2; colE++) {
                pNeighbour = tileAt(colE, rowE);
                if (pNeighbour != NULL) {
                    if (pNeighbour->isTypeSkipStrataFace())
                        continue;
                    InsertTileInDrawOrder(pNeighbour, pTile, rotationDegrees);
                    InsertWallAdjacentTilesInDrawOrder(pNeighbour, pTile, rotationDegrees);
                }
            }
        }
    }

    for (TileIter it = m_drawList.begin(); it != m_drawList.end(); it++)
        (*it)->RenderTile();

    m_drawList.clear();

    glPopMatrix();
    glFlush();
}
void Terrain::setZoomLevel(int iZoom)
{
    float fW;
    float fH;

    glMatrixMode(0x1701);
    glLoadIdentity();

    if (m_screenWidth == 0x320 && m_screenHeight == 0x258) {
        fW = 1767.767f;
        fH = 1325.8252f;
    } else if (m_screenWidth == 0x400 && m_screenHeight == 0x300) {
        fW = 1810.1934f;
        fH = 1357.645f;
    } else if (m_screenWidth == 0x500 && m_screenHeight == 0x400) {
        fW = 1740.5706f;
        fH = 1392.4564f;
    }

    switch (iZoom) {
    case 4:
        resize(m_screenWidth, m_screenHeight);
        m_zoomLevel = 0;
        break;
    case 2:
        fW *= 2.0f;
        fH *= 2.0f;
        m_zoomLevel = 1;
        break;
    case 1:
        fW = fW * 4.0f;
        fH = fH * 4.0f;
        m_zoomLevel = 2;
        break;
    }

    if (m_flipVertical)
        glOrtho(-fW * 0.5f, fW * 0.5f, fH * 0.5f, -fH * 0.5f, 5000.0, -5000.0);
    else
        glOrtho(-fW * 0.5f, fW * 0.5f, -fH * 0.5f, fH * 0.5f, 5000.0, -5000.0);

    glMatrixMode(0x1700);
}

void Terrain::stripRender(Tile *pCentre, int nDir, float fAngle)
{
    int i;
    int j;
    int x;
    int y;
    int nA;
    int nB;
    int nC;
    int nD;
    Tile *pTile = 0;

    glLoadIdentity();
    glPushMatrix();
    glRotated(g_fCameraPitchDegrees, 1.0, 0.0, 0.0);
    glRotatef(ISO_YAW_DEGREES + fAngle, 0.0f, 1.0f, 0.0f);

    if (pCentre != 0)
        glTranslatef((float)(GRID_CENTRE_CELL - pCentre->getGridX()) * WORLD_UNITS_PER_CELL, 0.0f,
                     (float)(GRID_CENTRE_CELL - pCentre->getGridY()) * WORLD_UNITS_PER_CELL);

    SetRotationQuadrantFromDegrees(fAngle);

    switch (nDir) {
    case 1:
        stripRender(pCentre, 8, fAngle);
        stripRender(pCentre, 2, fAngle);
        break;
    case 2:
        nA = 0xf << m_zoomLevel;
        nB = 3 << m_zoomLevel;
        nC = (m_zoomLevel << m_zoomLevel) + 5;
        nD = 4;
        for (i = pCentre->getGridX() - nC; i < pCentre->getGridX() + nD; i++) {
            for (y = pCentre->getGridY() - nA, x = i; y < pCentre->getGridY() + nB; x++, y++) {
                pTile = tileAt(x, y);
                if (y > 0x31 || x > 0x31)
                    break;
                if (x >= 0 && y >= 0 && !pTile->isTypeSkipStrataFace() && !IsTileCulled(pTile, pCentre, fAngle)) {
                    InsertTileInDrawOrder(pTile, pCentre, fAngle);
                    InsertWallAdjacentTilesInDrawOrder(pTile, pCentre, fAngle);
                }
            }
        }
        break;
    case 3:
        stripRender(pCentre, 2, fAngle);
        stripRender(pCentre, 4, fAngle);
        break;
    case 4:
        nC = 4 << m_zoomLevel;
        nD = 0x10 << m_zoomLevel;
        nA = 2;
        nB = (m_zoomLevel << m_zoomLevel) + 9;
        for (j = pCentre->getGridY() - nB; j < pCentre->getGridY() + nA; j++) {
            for (x = pCentre->getGridX() + nD, y = j; x > pCentre->getGridX() - nC; x--, y++) {
                pTile = tileAt(x, y);
                if (x < 0 || y > 0x31)
                    break;
                if (y >= 0 && x <= 0x31 && !pTile->isTypeSkipStrataFace() && !IsTileCulled(pTile, pCentre, fAngle)) {
                    InsertTileInDrawOrder(pTile, pCentre, fAngle);
                    InsertWallAdjacentTilesInDrawOrder(pTile, pCentre, fAngle);
                }
            }
        }
        break;
    case 5:
        stripRender(pCentre, 4, fAngle);
        stripRender(pCentre, 6, fAngle);
        break;
    case 6:
        nC = 3 << m_zoomLevel;
        nD = 0x10 << m_zoomLevel;
        nA = 4;
        nB = (m_zoomLevel << m_zoomLevel) + 8;
        for (j = pCentre->getGridY() - nB; j < pCentre->getGridY() + nA; j++) {
            for (x = pCentre->getGridX() - nD, y = j; x < pCentre->getGridX() + nC; x++, y++) {
                pTile = tileAt(x, y);
                if (y > 0x31 || x > 0x31)
                    break;
                if (x >= 0 && y >= 0 && !pTile->isTypeSkipStrataFace() && !IsTileCulled(pTile, pCentre, fAngle)) {
                    InsertTileInDrawOrder(pTile, pCentre, fAngle);
                    InsertWallAdjacentTilesInDrawOrder(pTile, pCentre, fAngle);
                }
            }
        }
        break;
    case 7:
        stripRender(pCentre, 8, fAngle);
        stripRender(pCentre, 6, fAngle);
        break;
    case 8:
        nC = 4 << m_zoomLevel;
        nD = 0x11 << m_zoomLevel;
        nA = (m_zoomLevel << m_zoomLevel) + 0xc;
        nB = -2;
        for (j = pCentre->getGridY() + nB; j < pCentre->getGridY() + nA; j++) {
            for (x = pCentre->getGridX() - nD, y = j; x < pCentre->getGridX() + nC; x++, y--) {
                pTile = tileAt(x, y);
                if (y < 0 || x > 0x31)
                    break;
                if (x >= 0 && y <= 0x31 && !pTile->isTypeSkipStrataFace() && !IsTileCulled(pTile, pCentre, fAngle)) {
                    InsertTileInDrawOrder(pTile, pCentre, fAngle);
                    InsertWallAdjacentTilesInDrawOrder(pTile, pCentre, fAngle);
                }
            }
        }
        break;
    }

    for (TileIter it = m_drawList.begin(); it != m_drawList.end(); it++)
        (*it)->RenderTile();

    m_drawList.clear();

    glPopMatrix();
    glFlush();
}

void Terrain::closeSystem()
{
    if (m_hRC == 0)
        return;

    if (!wglMakeCurrent(0, 0))
        MessageBoxA(0, "Release of RC failed!", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);

    if (!wglDeleteContext(m_hRC))
        MessageBoxA(0, "Deletion of RC failed!", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);

    m_hRC = 0;
    freeCourseTextures();
}

void Terrain::initSystem(int nWidth, int nHeight, HDC hDC, bool bFlip)
{
    int nPixelFormat;

    if (m_hRC != 0)
        return;

    if (hDC != 0) {
        nPixelFormat = ChoosePixelFormat(hDC, &g_pfd);
        if (nPixelFormat == 0)
            MessageBoxA(0, "Can't Find A Suitable PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION);

        if (!SetPixelFormat(hDC, nPixelFormat, &g_pfd))
            MessageBoxA(0, "Can't Set The PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION);

        m_hRC = wglCreateContext(hDC);
        if (m_hRC == 0)
            MessageBoxA(0, "Can't Create A GL Rendering Context.", "ERROR", MB_OK | MB_ICONEXCLAMATION);

        if (!wglMakeCurrent(hDC, m_hRC))
            MessageBoxA(0, "Can't Activate The GL Rendering Context.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
    }

    m_flipVertical = bFlip;
    initGLRenderState();
    initLighting();
    LoadCourseTextures();
    resize(nWidth, nHeight);

    m_screenWidth = nWidth;
    m_screenHeight = nHeight;

    if (m_screenWidth == 800 && m_screenHeight == 600)
        g_fCameraPitchDegrees = 38.682186f;
    else if (m_screenWidth == 1024 && m_screenHeight == 768)
        g_fCameraPitchDegrees = 40.541603f;
    else if (m_screenWidth == 1280 && m_screenHeight == 1024)
        g_fCameraPitchDegrees = 40.83222f;
}

void Terrain::resize(int w, int h)
{
    if (h == 0)
        h = 1;
    if (w == 0)
        w = 1;

    glViewport(0, 0, w, h);
    glMatrixMode(0x1701);
    glLoadIdentity();

    if (w == 800 && h == 600) {
        w = 1767;
        h = 1325;
    } else if (w == 1024 && h == 768) {
        w = 1810;
        h = 1303;
    } else if (w == 1280 && h == 1024) {
        w = 1740;
        h = 1392;
    }

    if (m_flipVertical)
        glOrtho(-w >> 1, w >> 1, h >> 1, -h >> 1, 5000.0, -5000.0);
    else
        glOrtho(-w >> 1, w >> 1, -h >> 1, h >> 1, 5000.0, -5000.0);

    glMatrixMode(0x1700);
    glLoadIdentity();
}

bool Terrain::initTileGrid()
{
    int i;
    int j;
    int k;

    InitVertexGrid(m_width, m_height);

    for (i = 0; i < m_width; i++) {
        for (j = 0; j < m_height; j++) {
            m_tiles[i + j * m_width].reset(i, j);
            m_tiles[i + j * m_width].GetFaceBlock()->InitTriangleMesh(i, j, m_width);
        }
    }

    for (i = 0; i < m_width; i++) {
        for (k = 0; k < m_height; k++) {
            m_tiles[i + k * m_width].setNeighbour(2, tileAt(i - 1, k));
            m_tiles[i + k * m_width].setNeighbour(0, tileAt(i, k - 1));
            m_tiles[i + k * m_width].setNeighbour(3, tileAt(i + 1, k));
            m_tiles[i + k * m_width].setNeighbour(1, tileAt(i, k + 1));
        }
    }

    return true;
}

void Terrain::layPath(Tile *t, int on, int type)
{
    if (on)
        t->setPath(true, type);
    else
        t->setPath(false, type);
}
bool Terrain::hasConnectedPath(int x, int y)
{
    return tileAt(x, y)->hasConnectedPath();
}
void Terrain::updatePath(int x, int y, int on)
{
    tileAt(x, y)->setConnectedPath(on);
}
bool Terrain::hasPath(Tile *t)
{
    return t->hasPath();
}

void Terrain::setWall(Tile *t, int side, int value, bool enabled)
{
    t->setWall(side, value, enabled);
}

void Terrain::elevateCorner(Tile *t, int corner)
{
    if (t)
        t->ElevateCorner(corner);
}

void Terrain::lowerCorner(Tile *t, int corner)
{
    if (t)
        t->LowerCornerElevation(corner);
}
void Terrain::lowerEdgeCorner(Tile *t, int corner, Tile *gridTile, float angle)
{
    if (t) {
        RenderTileEdgeSkirt(t, gridTile, angle);
        t->LowerCornerElevation(corner);
    }
}

void Terrain::calcNormals(Tile *t)
{
    if (t)
        t->calcVertexNormals();
}
void Terrain::calcAllNormals(Tile *centre)
{
    Tile *t;
    int yLo, yHi, xLo, xHi;

    yLo = yHi = xLo = xHi = 13 << m_zoomLevel;

    for (int i = centre->getGridY() - yLo; i < centre->getGridY() + yHi; i++) {
        for (int j = centre->getGridX() + xHi; j > centre->getGridX() - xLo; j--) {
            t = tileAt(j, i);
            if (t)
                t->calcVertexNormals();
        }
    }
}
void Terrain::setSplineHeight(float fHeight)
{
    g_fSplineHeight = fHeight;
}

void Terrain::passCollarInfo(int p[], int n)
{
    int i;

    if (g_pCollar == 0)
        g_pCollar = new int[n];
    else {
        delete g_pCollar;
        g_pCollar = new int[n];
    }

    for (i = 0; i < n; i++)
        g_pCollar[i] = p[i];
}

void Terrain::initTerrain()
{
    initTileGrid();
    glVertexPointer(3, GL_FLOAT, 0, getVertexArray());
    glNormalPointer(GL_FLOAT, 0, getNormalArray());
}

void Terrain::resetTerrain()
{
    int x;
    int y;

    for (x = 0; x < m_width; x++)
        for (y = 0; y < m_height; y++)
            m_tiles[x + y * m_width].reset(x, y);

    initTileGrid();
    glVertexPointer(3, GL_FLOAT, 0, getVertexArray());
    glNormalPointer(GL_FLOAT, 0, getNormalArray());
}

Tile *Terrain::tileHit(int mouseX, int mouseY)
{
    char buf[8];
    int tileRow;
    int tileCol;
    int dy;
    int dx;
    int num;
    int den;
    int halfTile;
    float scale;
    int tileY;
    int tileX;
    int originX;
    int originY;

    OutputDebugStringA("\nX: ");
    OutputDebugStringA(itoa(mouseX, buf, 10));
    OutputDebugStringA("   Y: ");
    OutputDebugStringA(itoa(mouseY, buf, 10));
    OutputDebugStringA("\n");

    originX = -1200;
    originY = 300;
    halfTile = 32;
    scale = 20.0f;

    dx = mouseX - 400;
    dy = mouseY - 300;

    den = (int)(halfTile * 2.0f * scale);
    num = dy * halfTile - (int)(dx * scale) + den / 2;
    if (num < 0)
        num -= den;
    tileRow = num / den;

    num = dy * halfTile + (int)(dx * scale) + den / 2;
    if (num < 0)
        num -= den;
    tileCol = num / den;

    tileY = -tileRow + 24;
    tileX = tileCol + 24;

    OutputDebugStringA("\nTile X: ");
    OutputDebugStringA(itoa(tileX, buf, 10));
    OutputDebugStringA("   Tile Y: ");
    OutputDebugStringA(itoa(tileY, buf, 10));
    OutputDebugStringA("\n");

    return tileAt(tileY, tileX);
}

void Terrain::SetRotationQuadrantFromDegrees(float fDegrees)
{
    if (fDegrees == 0.0f)
        g_QuadrantIndex = 0;
    else if (fDegrees == 90.0f)
        g_QuadrantIndex = 1;
    else if (fDegrees == 180.0f)
        g_QuadrantIndex = 2;
    else
        g_QuadrantIndex = 3;
}

void Terrain::InsertTileInDrawOrder(Tile *pTile, Tile *pCentre, float fAngle)
{
    int gridX = pTile->getGridX();
    int gridY = pTile->getGridY();
    TileIter it;


    if (fAngle == 0.0f) {
        if (m_drawList.empty()) {
            m_drawList.push_back(pTile);
        } else {
            it = m_drawList.begin();
            while (it != m_drawList.end() && gridX < (*it)->getGridX())
                it++;
            if (it == m_drawList.end() || (*it)->getGridX() < gridX) {
                m_drawList.insert(it, pTile);
            } else {
                while (it != m_drawList.end() && gridY > (*it)->getGridY() && (*it)->getGridX() == gridX)
                    it++;
                if (it != m_drawList.end() && (*it)->getGridY() == gridY && (*it)->getGridX() == gridX)
                    return;
                m_drawList.insert(it, pTile);
            }
        }
    } else if (fAngle == 90.0f) {
        if (m_drawList.empty()) {
            m_drawList.push_back(pTile);
        } else {
            it = m_drawList.begin();
            while (it != m_drawList.end() && gridY < (*it)->getGridY())
                it++;
            if (it == m_drawList.end() || (*it)->getGridY() < gridY) {
                m_drawList.insert(it, pTile);
            } else {
                while (it != m_drawList.end() && gridX < (*it)->getGridX() && (*it)->getGridY() == gridY)
                    it++;
                if (it != m_drawList.end() && (*it)->getGridY() == gridY && (*it)->getGridX() == gridX)
                    return;
                m_drawList.insert(it, pTile);
            }
        }
    } else if (fAngle == 180.0f) {
        if (m_drawList.empty()) {
            m_drawList.push_back(pTile);
        } else {
            it = m_drawList.begin();
            while (it != m_drawList.end() && gridX > (*it)->getGridX())
                it++;
            if (it == m_drawList.end() || (*it)->getGridX() > gridX) {
                m_drawList.insert(it, pTile);
            } else {
                while (it != m_drawList.end() && gridY < (*it)->getGridY() && (*it)->getGridX() == gridX)
                    it++;
                if (it != m_drawList.end() && (*it)->getGridY() == gridY && (*it)->getGridX() == gridX)
                    return;
                m_drawList.insert(it, pTile);
            }
        }
    } else {
        if (m_drawList.empty()) {
            m_drawList.push_back(pTile);
        } else {
            it = m_drawList.begin();
            while (it != m_drawList.end() && gridY > (*it)->getGridY())
                it++;
            if (it == m_drawList.end() || (*it)->getGridY() < gridY) {
                m_drawList.insert(it, pTile);
            } else {
                while (it != m_drawList.end() && gridX > (*it)->getGridX() && (*it)->getGridY() == gridY)
                    it++;
                if (it != m_drawList.end() && (*it)->getGridY() == gridY && (*it)->getGridX() == gridX)
                    return;
                m_drawList.insert(it, pTile);
            }
        }
    }
}
