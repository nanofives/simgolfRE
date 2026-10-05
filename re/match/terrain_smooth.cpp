// Tile::smoothNormals (Terrain.dll, debug build): recomputes the face normals around the tile corner shared
// with neighbours a (0/1) and b (2/3) and the diagonal one (b's neighbour a), sums the adjacent face normals into
// the shared vertices' entries of g_normals and normalizes them. Called by calcNormals (0x10012cf0) with
// (0,2) (1,2) (0,3) (1,3).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#include <windows.h>

extern float g_normals[][3];                     // 0x10070a18
void normalize(float* v);                        // 0x10037c80

struct Face {                                    // stride 0x38
    int   v[3];                                  // +0x00
    float tc[6];                                 // +0x0c
    int   texture;                               // +0x24
    int   piece;                                 // +0x28
    float normal[3];                             // +0x2c
};
struct Faces { int count; Face f[8]; };          // Tile +0x44

class Tile {
public:
    Faces* getFaces();                           // 0x100153c0
    void faceNormal(Face* f);                    // 0x10011d60 (called with the Tile in ecx here)
    void smoothNormals(int a, int b);
    char  m_pad0[0x34];
    Tile* m_n[4];                                // +0x34
    Faces m_faces;                               // +0x44
};

// MATCH: Terrain.dll 0x10011ef0 ?smoothNormals@Tile@@QAEXHH@Z
void Tile::smoothNormals(int a, int b)
{
    Faces* fa = 0;
    Faces* fb = 0;
    Faces* fc = 0;
    if (m_n[a] != 0)
        fa = m_n[a]->getFaces();
    if (m_n[b] != 0) {
        fb = m_n[b]->getFaces();
        if (m_n[a] != 0 && m_n[b]->m_n[a] != 0)
            fc = m_n[b]->m_n[a]->getFaces();
    }
    if (a == 0) {
        if (b == 2) {
            faceNormal(&m_faces.f[0]);
            faceNormal(&m_faces.f[1]);
            faceNormal(&m_faces.f[2]);
            faceNormal(&m_faces.f[4]);
            if (fa != 0) {
                faceNormal(&fa->f[4]);
                faceNormal(&fa->f[5]);
                faceNormal(&fa->f[6]);
            }
            if (fb != 0) {
                faceNormal(&fb->f[2]);
                faceNormal(&fb->f[3]);
                faceNormal(&fb->f[7]);
            }
            if (fc != 0) {
                faceNormal(&fc->f[6]);
                faceNormal(&fc->f[7]);
            }
            for (int i = 0;
            i < 3;
            i++) {
                g_normals[m_faces.f[1].v[2]][i] = m_faces.f[1].normal[i] + m_faces.f[2].normal[i];
                g_normals[m_faces.f[0].v[1]][i] = m_faces.f[0].normal[i] + m_faces.f[4].normal[i];
                g_normals[m_faces.f[0].v[0]][i] = m_faces.f[0].normal[i] + m_faces.f[1].normal[i];
                if (fa != 0) {
                    g_normals[m_faces.f[0].v[0]][i] += fa->f[4].normal[i] + fa->f[5].normal[i];
                    g_normals[m_faces.f[1].v[2]][i] += fa->f[5].normal[i] + fa->f[6].normal[i];
                }
                if (fb != 0) {
                    g_normals[m_faces.f[0].v[0]][i] += fb->f[2].normal[i] + fb->f[3].normal[i];
                    g_normals[m_faces.f[0].v[1]][i] += fb->f[3].normal[i] + fb->f[7].normal[i];
                }
                if (fc != 0) {
                    g_normals[m_faces.f[0].v[0]][i] += fc->f[6].normal[i] + fc->f[7].normal[i];
                }
            }
            normalize(g_normals[m_faces.f[0].v[0]]);
            normalize(g_normals[m_faces.f[0].v[1]]);
            normalize(g_normals[m_faces.f[1].v[2]]);
        }
        if (b == 3) {
            faceNormal(&m_faces.f[2]);
            faceNormal(&m_faces.f[3]);
            faceNormal(&m_faces.f[7]);
            if (fa != 0) {
                faceNormal(&fa->f[6]);
                faceNormal(&fa->f[7]);
            }
            if (fb != 0) {
                faceNormal(&fb->f[0]);
                faceNormal(&fb->f[4]);
            }
            if (fc != 0) {
                faceNormal(&fc->f[4]);
                faceNormal(&fc->f[5]);
            }
            for (int i = 0;
            i < 3;
            i++) {
                g_normals[m_faces.f[3].v[1]][i] = m_faces.f[7].normal[i] + m_faces.f[3].normal[i];
                g_normals[m_faces.f[3].v[2]][i] = m_faces.f[2].normal[i] + m_faces.f[3].normal[i];
                if (fa != 0) {
                    g_normals[m_faces.f[3].v[2]][i] += fa->f[6].normal[i] + fa->f[7].normal[i];
                }
                if (fb != 0) {
                    g_normals[m_faces.f[3].v[2]][i] += fb->f[0].normal[i] + fb->f[1].normal[i];
                    g_normals[m_faces.f[3].v[1]][i] += fb->f[0].normal[i] + fb->f[4].normal[i];
                }
                if (fc != 0) {
                    g_normals[m_faces.f[3].v[2]][i] += fc->f[4].normal[i] + fc->f[5].normal[i];
                }
            }
            normalize(g_normals[m_faces.f[3].v[1]]);
            normalize(g_normals[m_faces.f[3].v[2]]);
        }
    }
    else if (a == 1) {
        if (b == 2) {
            faceNormal(&m_faces.f[4]);
            faceNormal(&m_faces.f[5]);
            faceNormal(&m_faces.f[6]);
            if (fa != 0) {
                faceNormal(&fa->f[0]);
                faceNormal(&fa->f[1]);
                faceNormal(&fa->f[2]);
            }
            if (fb != 0) {
                faceNormal(&fb->f[6]);
                faceNormal(&fb->f[7]);
            }
            if (fc != 0) {
                faceNormal(&fc->f[2]);
                faceNormal(&fc->f[3]);
            }
            for (int i = 0;
            i < 3;
            i++) {
                g_normals[m_faces.f[4].v[1]][i] = m_faces.f[5].normal[i] + m_faces.f[4].normal[i];
                g_normals[m_faces.f[5].v[1]][i] = m_faces.f[5].normal[i] + m_faces.f[6].normal[i];
                if (fa != 0) {
                    g_normals[m_faces.f[4].v[1]][i] += fa->f[0].normal[i] + fa->f[1].normal[i];
                    g_normals[m_faces.f[5].v[1]][i] += fa->f[1].normal[i] + fa->f[2].normal[i];
                }
                if (fb != 0) {
                    g_normals[m_faces.f[4].v[1]][i] += fb->f[6].normal[i] + fb->f[7].normal[i];
                }
                if (fc != 0) {
                    g_normals[m_faces.f[4].v[1]][i] += fc->f[2].normal[i] + fc->f[3].normal[i];
                }
            }
            normalize(g_normals[m_faces.f[4].v[1]]);
            normalize(g_normals[m_faces.f[5].v[1]]);
        }
        if (b == 3) {
            faceNormal(&m_faces.f[6]);
            faceNormal(&m_faces.f[7]);
            if (fa != 0) {
                faceNormal(&fa->f[2]);
                faceNormal(&fa->f[3]);
            }
            if (fb != 0) {
                faceNormal(&fb->f[4]);
                faceNormal(&fb->f[5]);
            }
            if (fc != 0) {
                faceNormal(&fc->f[0]);
                faceNormal(&fc->f[1]);
            }
            for (int i = 0;
            i < 3;
            i++) {
                g_normals[m_faces.f[7].v[1]][i] = m_faces.f[7].normal[i] + m_faces.f[6].normal[i];
                if (fa != 0) {
                    g_normals[m_faces.f[7].v[1]][i] += fa->f[2].normal[i] + fa->f[3].normal[i];
                }
                if (fb != 0) {
                    g_normals[m_faces.f[7].v[1]][i] += fb->f[4].normal[i] + fb->f[5].normal[i];
                }
                if (fc != 0) {
                    g_normals[m_faces.f[7].v[1]][i] += fc->f[0].normal[i] + fc->f[1].normal[i];
                }
            }
            normalize(g_normals[m_faces.f[7].v[1]]);
        }
    }
}
