// Terrain::loadLighting(const char* file) (0x10006dd0; called by Terrain::rebuild with
// "<Parkland|Desert|Tropical|Links>Lighting.txt"): Terrain.dll, debug build, old iostream (<fstream.h>,
// ifstream opened with ios::nocreate = 0x20, filebuf::openprot = 420 at 0x100612c4).
// Reads the line after "#AMBIENT", "#DIFFUSE" and "#SPECULAR" as three integers / 255.0 (0x1005f200), aims
// GL_LIGHT0 from (-0.5, 0.1, -1, 0) rotated 40 deg about x then 45 about y (0x10003a50) and normalised,
// sets white specular material with shininess 13. The colour arrays are float[3] but glLightfv reads 4.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
#include <windows.h>
#include <GL/gl.h>
#include <string.h>
#include <stdlib.h>
#include <fstream.h>

void rotate(float angle, float x, float y, float z, float* v);  // 0x10003a50
void normalize(float* v);                                         // 0x10037c80

class Terrain {
public:
    void loadLighting(const char* file);
};

// MATCH: Terrain.dll 0x10006dd0 ?loadLighting@Terrain@@QAEXPBD@Z
void Terrain::loadLighting(const char* file)
{
    float ambient[3];
    float diffuse[3];
    float specular[3];
    char buf[256];
    int unused[7];                       // 0x1c bytes at [ebp-0x150, ebp-0x134) never referenced: type and name NOT determined, only the size
    ifstream in(file, ios::nocreate);
    if (in.fail())
        return;
    in.getline(buf, 256);
    while (strcmp(strtok(buf, " "), "#AMBIENT") != 0)
        in.getline(buf, 256);
    in.getline(buf, 256);
    ambient[0] = atoi(strtok(buf, " ")) / 255.0f;
    ambient[1] = atoi(strtok(NULL, " ")) / 255.0f;
    ambient[2] = atoi(strtok(NULL, " ")) / 255.0f;
    in.getline(buf, 256);
    while (strcmp(strtok(buf, " "), "#DIFFUSE") != 0)
        in.getline(buf, 256);
    in.getline(buf, 256);
    diffuse[0] = atoi(strtok(buf, " ")) / 255.0f;
    diffuse[1] = atoi(strtok(NULL, " ")) / 255.0f;
    diffuse[2] = atoi(strtok(NULL, " ")) / 255.0f;
    in.getline(buf, 256);
    while (strcmp(strtok(buf, " "), "#SPECULAR") != 0)
        in.getline(buf, 256);
    in.getline(buf, 256);
    specular[0] = atoi(strtok(buf, " ")) / 255.0f;
    specular[1] = atoi(strtok(NULL, " ")) / 255.0f;
    specular[2] = atoi(strtok(NULL, " ")) / 255.0f;
    float pos[4] = { -0.5f, 0.1f, -1.0f, 0.0f };
    rotate(40.0f, 1.0f, 0.0f, 0.0f, pos);
    rotate(45.0f, 0.0f, 1.0f, 0.0f, pos);
    normalize(pos);
    float matSpecular[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glEnable(GL_LIGHT0);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matSpecular);
    glMateriali(GL_FRONT, GL_SHININESS, 13);
    in.close();
}
