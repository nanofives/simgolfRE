// Terrain::initSystem / Terrain::resize (Terrain.dll, debug): OpenGL context setup and the projection.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
//
// resize() special-cases three screen sizes with fixed orthographic extents (800x600 -> 1767x1325,
// 1024x768 -> 1810x1303, 1280x1024 -> 1740x1392, 0x10009f57..0x10009fa2); initSystem() sets a matching
// global per size (0x10070a10). The game always runs 800x600 (the shim keeps it so in a window).
#include <windows.h>
#include <GL/gl.h>

class Terrain {
public:
    void initSystem(int width, int height, HDC dc, bool flipY);
    void resize(int width, int height);

    void initTextures();                 // 0x100033e0 (unexported; name tentative)
    void initLists();                    // 0x100037e0 (unexported; name tentative)
    void relight();                      // 0x100076e0 (unexported; also called by loadNewCourseType)

    HGLRC m_glrc;                        // +0x00
    char  m_pad0[0x20 - 4];
    int   m_screenW;                     // +0x20
    int   m_screenH;                     // +0x24
    bool  m_flipY;                       // +0x28
};

extern PIXELFORMATDESCRIPTOR g_pfd;      // 0x10063e10
float g_screenScale;                     // 0x10070a10

// MATCH: Terrain.dll 0x10009c80 ?initSystem@Terrain@@QAEXHHPAUHDC__@@_N@Z
void Terrain::initSystem(int width, int height, HDC dc, bool flipY)
{
    int pf;
    if (m_glrc != 0)
        return;                          // early return: je body / jmp epilogue at 0x10009c9c
    if (dc != 0) {
        pf = ChoosePixelFormat(dc, &g_pfd);
        if (pf == 0)
            MessageBox(0, "Can't Find A Suitable PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
        if (!SetPixelFormat(dc, pf, &g_pfd))
            MessageBox(0, "Can't Set The PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
        m_glrc = wglCreateContext(dc);
        if (m_glrc == 0)
            MessageBox(0, "Can't Create A GL Rendering Context.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
        if (!wglMakeCurrent(dc, m_glrc))
            MessageBox(0, "Can't Activate The GL Rendering Context.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
    }
    m_flipY = flipY;
    initTextures();
    initLists();
    relight();
    resize(width, height);
    m_screenW = width;
    m_screenH = height;
    if (m_screenW == 800 && m_screenH == 600)
        g_screenScale = 38.682186f;
    else if (m_screenW == 1024 && m_screenH == 768)
        g_screenScale = 40.541603f;
    else if (m_screenW == 1280 && m_screenH == 1024)
        g_screenScale = 40.83222f;
}

// MATCH: Terrain.dll 0x10009ed0 ?resize@Terrain@@QAEXHH@Z
void Terrain::resize(int width, int height)
{
    if (height == 0)
        height = 1;
    if (width == 0)
        width = 1;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (width == 800 && height == 600) {
        width = 1767;
        height = 1325;
    } else if (width == 1024 && height == 768) {
        width = 1810;
        height = 1303;
    } else if (width == 1280 && height == 1024) {
        width = 1740;
        height = 1392;
    }
    if (m_flipY)
        glOrtho(-width >> 1, width >> 1, height >> 1, -height >> 1, 5000.0, -5000.0);
    else
        glOrtho(-width >> 1, width >> 1, -height >> 1, height >> 1, 5000.0, -5000.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}
