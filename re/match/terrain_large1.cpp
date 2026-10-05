// Terrain.dll functions, batch 5 (debug build): texture file loader, light direction rotation, lighting level,
// zoom projection.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <stdlib.h>
#include <math.h>

extern "C" GLubyte* LoadDIBitmap(const char* filename, BITMAPINFO** info);   // 0x100016a0 (terrain_bitmap.cpp)
struct ImageInfo;
void swapRedBlue(unsigned char* p, ImageInfo* info);                          // 0x10001b60
float degToRad(float d);                                                      // 0x10001880
void normalize(float* v);                                                     // 0x10037c80

// Loads a .bmp as a texture (a reworked SuperBible TextureLoad: BGR->RGB swap, GL_NEAREST magnification,
// GL_LINEAR minification parameter, mipmaps unless min_filter is GL_NEAREST/GL_LINEAR). 0 on failure.
// MATCH: Terrain.dll 0x1000bbd0 ?textureLoad@@YAIPADEIII@Z
GLuint textureLoad(char* filename, GLboolean alpha, GLenum minfilter, GLenum magfilter, GLenum wrap)
{
    BITMAPINFO* info;
    GLubyte* bits;
    GLenum type;
    GLuint texture;
    if ((bits = LoadDIBitmap(filename, &info)) == NULL)
        return 0;
    swapRedBlue(bits, (ImageInfo*)info);
    if (info->bmiHeader.biHeight == 1)
        type = GL_TEXTURE_1D;
    else
        type = GL_TEXTURE_2D;
    glGenTextures(1, &texture);
    glBindTexture(type, texture);
    glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(type, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(type, GL_TEXTURE_WRAP_T, wrap);
    glTexEnvi(type, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    if (minfilter == GL_LINEAR || minfilter == GL_NEAREST)
        glTexImage2D(type, 0, 3, info->bmiHeader.biWidth, info->bmiHeader.biHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, bits);
    else if (type == GL_TEXTURE_1D)
        gluBuild1DMipmaps(type, 3, info->bmiHeader.biWidth, GL_RGBA, GL_UNSIGNED_BYTE, bits);
    else
        gluBuild2DMipmaps(type, 3, info->bmiHeader.biWidth, info->bmiHeader.biHeight, GL_RGB, GL_UNSIGNED_BYTE, bits);
    free(info);
    free(bits);
    return texture;
}

// Rotates v by `deg` degrees about the x, y or z axis (only an axis-aligned (ax, ay, az) does anything). The
// second component uses the already rotated first one, as in the original.
// MATCH: Terrain.dll 0x10003a50 ?rotateAxis@@YAXMMMMPAM@Z
void rotateAxis(float deg, float ax, float ay, float az, float* v)
{
    float rad;
    float c = (float)cos(rad = degToRad(deg));
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

class Terrain {
public:
    void changeLighting(int up);
    void setZoomLevel(int level);
    void resize(int w, int h);                   // 0x10007740 (terrain_methods.cpp)
    int   m_0;
    float m_ambient[4];                          // +0x04
    int   m_14, m_18;
    int   m_zoom;                                // +0x1c
    int   m_screenW, m_screenH;                  // +0x20, +0x24
    bool  m_flipY;                               // +0x28
};

// Raises (up != 0, up to 1.0) or lowers (down to 0.1) the ambient light by 0.1 and sets up light 0.
// MATCH: Terrain.dll 0x10003540 ?changeLighting@Terrain@@QAEXH@Z
void Terrain::changeLighting(int up)
{
    if (up) {
        if (m_ambient[0] < 1.0f) {
            m_ambient[0] += 0.1f;
            m_ambient[1] += 0.1f;
            m_ambient[2] += 0.1f;
        }
    } else if (m_ambient[0] > 0.1f) {
        m_ambient[0] -= 0.1f;
        m_ambient[1] -= 0.1f;
        m_ambient[2] -= 0.1f;
    }
    float dir[4] = {-0.5f, 0.1f, -1.0f, 0.0f};
    rotateAxis(40.0f, 1.0f, 0.0f, 0.0f, dir);
    rotateAxis(45.0f, 0.0f, 1.0f, 0.0f, dir);
    normalize(dir);
    float diffuse[4] = {0.9f, 0.9f, 0.9f, 1.0f};
    float specular[4] = {1.0f, 1.0f, 0.9f, 1.0f};
    float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, dir);
    glLightfv(GL_LIGHT0, GL_AMBIENT, m_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glEnable(GL_LIGHT0);
}

// Orthographic projection for the screen size and zoom level 1 (x4), 2 (x2) or 4 (resize).
// MATCH: Terrain.dll 0x10008fc0 ?setZoomLevel@Terrain@@QAEXH@Z
void Terrain::setZoomLevel(int level)
{
    float w, h;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (m_screenW == 800 && m_screenH == 600) {
        w = 1767.767f;
        h = 1325.8252f;
    } else if (m_screenW == 1024 && m_screenH == 768) {
        w = 1810.1934f;
        h = 1357.645f;
    } else if (m_screenW == 1280 && m_screenH == 1024) {
        w = 1740.5706f;
        h = 1392.4564f;
    }
    switch (level) {
    case 4:
        resize(m_screenW, m_screenH);
        m_zoom = 0;
        break;
    case 2:
        w *= 2.0f;
        h *= 2.0f;
        m_zoom = 1;
        break;
    case 1:
        w *= 4.0f;
        h *= 4.0f;
        m_zoom = 2;
        break;
    }
    if (m_flipY)
        glOrtho(-w * 0.5f, w * 0.5f, h * 0.5f, -h * 0.5f, 5000.0, -5000.0);
    else
        glOrtho(-w * 0.5f, w * 0.5f, -h * 0.5f, h * 0.5f, 5000.0, -5000.0);
    glMatrixMode(GL_MODELVIEW);
}
