// Terrain::relight (0x100076e0; the name comes from earlier notes, the body loads textures): Terrain.dll,
// debug build. Builds "./Data/Textures/<Parkland|Desert|Tropical|Links>/" (course type 0x10070a0c,
// default Parkland), loads the Tee set (type 0: A0001..A0009 then A0010..A0029 up to 0x19 variations) and,
// for every other named type up to 0x1e, variations A..E of "<name><letter>0001.bmp".."0009.bmp" until a
// load fails; then the path (0x20, 0x21: TGA), retaining wall (0x22), cliff (0x23) and strata (0x24) sets.
// `else if (m_types[i].name)` tests an array address against 0 (always true): unnamed types reach the loop
// and stop at the first failed load.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
#include <string.h>

typedef unsigned int GLuint;
GLuint loadBMP(const char* file, int a, int minFilter, int magFilter, int wrap);  // 0x1000bbd0
GLuint loadTGA(const char* file);                                                // 0x1000be30
void   setTexture(int type, int variation, int slot, GLuint tex);              // 0x10012e70

struct TypeInfo {                        // Terrain +0x2c, stride 0x18
    char name[0x14];
    int  count;
};

class Terrain {
public:
    void relight();
    void reloadTextures();               // 0x100380a0

    char     m_pad0[0x29];
    bool     m_texturesLoaded;           // +0x29
    char     m_pad2a[2];
    TypeInfo m_types[0x25];              // +0x2c
};

extern int g_courseType;                 // 0x10070a0c

// MATCH: Terrain.dll 0x100076e0 ?relight@Terrain@@QAEXXZ
void Terrain::relight()
{
    char base[0x104] = "./Data/Textures/";
    char path[0x104];
    GLuint tex = 0;
    size_t len;
    int count = 0;
    int i, c1, c2, c3, letter, d;
    if (m_texturesLoaded)
        reloadTextures();
    switch (g_courseType) {
    case 0:
        strcat(base, "Parkland/");
        break;
    case 2:
        strcat(base, "Tropical/");
        break;
    case 1:
        strcat(base, "Desert/");
        break;
    case 3:
        strcat(base, "Links/");
        break;
    default:
        strcat(base, "Parkland/");
    }
    for (i = 0; i < 0x1f; i++) {
        if (i == 0) {
            strcpy(path, base);
            strcat(path, m_types[i].name);
            strcat(path, "A0001.bmp");
            for (c1 = '1'; c1 < ':'; c1++) {
                path[strlen(path) - 5] = c1;
                tex = loadBMP(path, 0, 0x2601, 0x2601, 0x2901);
                setTexture(0, count++, 0, tex);
            }
            for (c2 = '1'; c2 < '3'; c2++) {
                path[strlen(path) - 6] = c2;
                for (c3 = '0'; c3 < ':'; c3++) {
                    if (count > 0x18)
                        continue;
                    path[strlen(path) - 5] = c3;
                    tex = loadBMP(path, 0, 0x2601, 0x2601, 0x2901);
                    setTexture(0, count++, 0, tex);
                }
            }
        } else if (m_types[i].name) {
            for (letter = 'A'; letter < 'F'; letter++) {
                strcpy(path, base);
                strcat(path, m_types[i].name);
                len = strlen(path);
                path[len] = letter;
                path[len + 1] = 0;
                strcat(path, "0001.bmp");
                tex = loadBMP(path, 0, 0x2601, 0x2601, 0x2901);
                if (tex) {
                    setTexture(i, m_types[i].count, 0, tex);
                    for (d = '2'; d < ':'; d++) {
                        path[strlen(path) - 5] = d;
                        tex = loadBMP(path, 0, 0x2601, 0x2601, 0x2901);
                        setTexture(i, m_types[i].count, d - '1', tex);
                    }
                    m_types[i].count++;
                } else
                    break;
            }
        }
    }
    strcpy(path, base);
    strcat(path, "/Path.tga");
    setTexture(0x20, 0, 0, loadTGA(path));
    strcpy(path, base);
    strcat(path, "/PathCap.tga");
    setTexture(0x20, 0, 1, loadTGA(path));
    strcpy(path, base);
    strcat(path, "/PathInside.tga");
    setTexture(0x20, 0, 2, loadTGA(path));
    strcpy(path, base);
    strcat(path, "/PathCurve.tga");
    setTexture(0x20, 0, 3, loadTGA(path));
    m_types[0x20].count++;
    strcpy(path, base);
    strcat(path, "/PathX.tga");
    setTexture(0x21, 0, 0, loadTGA(path));
    strcpy(path, base);
    strcat(path, "/PathCapX.tga");
    setTexture(0x21, 0, 1, loadTGA(path));
    strcpy(path, base);
    strcat(path, "/PathInsideX.tga");
    setTexture(0x21, 0, 2, loadTGA(path));
    strcpy(path, base);
    strcat(path, "/PathCurveX.tga");
    setTexture(0x21, 0, 3, loadTGA(path));
    m_types[0x21].count++;
    strcpy(path, base);
    strcat(path, "/CliffTest.bmp");
    setTexture(0x23, 0, 0, loadBMP(path, 0, 0x2601, 0x2601, 0x2901));
    m_types[0x23].count++;
    strcpy(path, base);
    strcat(path, "/RetainWallA.bmp");
    setTexture(0x22, 0, 0, loadBMP(path, 0, 0x2601, 0x2601, 0x2901));
    m_types[0x22].count++;
    strcpy(path, base);
    strcat(path, "/strata.bmp");
    setTexture(0x24, 0, 0, loadBMP(path, 0, 0x2601, 0x2601, 0x2901));
    m_types[0x24].count++;
    m_texturesLoaded = true;
}
