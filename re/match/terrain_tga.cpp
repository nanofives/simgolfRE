// Terrain.dll (debug build): LoadTGA, the uncompressed-TGA texture loader adapted from the NeHe OpenGL tutorials
// (12-byte header compare, BGR->RGB swap, new[]/delete[]), and Terrain::renderTile (one textured quad with the
// assert at Terrain.cpp line base+0x13).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Returns the GL texture name, 0 on failure.
// MATCH: Terrain.dll 0x1000be30 ?LoadTGA@@YAIPAD@Z
GLuint LoadTGA(char* filename)
{
    GLubyte TGAheader[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    GLubyte TGAcompare[12];
    GLubyte header[6];
    GLuint  bytesPerPixel;
    GLuint  imageSize;
    GLuint  temp;
    GLuint  texture;
    GLuint  width;
    GLuint  height;
    GLubyte* imageData;
    FILE*   file;
    int     i;

    file = fopen(filename, "rb");
    if (file == NULL)
        return 0;
    if (fread(TGAcompare, 1, sizeof(TGAcompare), file) != sizeof(TGAcompare) ||
        memcmp(TGAheader, TGAcompare, sizeof(TGAheader)) != 0 ||
        fread(header, 1, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return 0;
    }
    width = header[1] * 256 + header[0];
    height = header[3] * 256 + header[2];
    if (width <= 0 || height <= 0 || (header[4] != 24 && header[4] != 32)) {
        fclose(file);
        return 0;
    }
    bytesPerPixel = header[4] / 8;
    imageSize = width * height * bytesPerPixel;
    imageData = new GLubyte[imageSize];
    if (imageData == NULL || fread(imageData, 1, imageSize, file) != imageSize) {
        fclose(file);
        if (imageData != NULL)
            delete [] imageData;
        return 0;
    }
    for (i = 0; i < (int)imageSize; i += bytesPerPixel) {
        temp = imageData[i];
        imageData[i] = imageData[i + 2];
        imageData[i + 2] = temp;
    }
    fclose(file);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (bytesPerPixel == 3)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);
    if (imageData != NULL)
        delete [] imageData;
    return texture;
}

#define NUM_TEXTURED_TILES 31
extern unsigned int g_textures[0x25][0x19][9];  // 0x100687f8
extern float g_viewAngle;                        // 0x10070a10
class Terrain {
public:
    void renderTile(int iTileType, int x, int y, int size, int div);
};

// MATCH: Terrain.dll 0x100080e0 ?renderTile@Terrain@@QAEXHHHHH@Z
void Terrain::renderTile(int iTileType, int x, int y, int size, int div)
{
    float scale = (float)size / div;
    float tx = (float)(x - 432);
    float ty = (float)(y - 300);
    tx *= 2.2097087f;
    ty *= 2.2097087f;
    glLoadIdentity();
    glPushMatrix();
    glTranslatef(tx, ty, 0.0f);
    glRotated(g_viewAngle, 1.0, 0.0, 0.0);
    glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
    glScalef(scale, scale, scale);
    // (seven source lines in the original between here and the assert: blank or comment lines, their
    // content is not recoverable; with /ZI __LINE__ is relative to the line of `{`, and the original assert
    // passes base+0x13 at 0x100081f6)
    //
    //
    //
    //
    assert(iTileType < NUM_TEXTURED_TILES);
    glDisable(GL_LIGHTING);
    glBindTexture(GL_TEXTURE_2D, g_textures[iTileType][0][0]);
    glBegin(GL_TRIANGLES);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-50.0f, 0.0f, -50.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-50.0f, 0.0f, 50.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(50.0f, 0.0f, 50.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-50.0f, 0.0f, -50.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(50.0f, 0.0f, 50.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(50.0f, 0.0f, -50.0f);
    glEnd();
    glEnable(GL_LIGHTING);
    glPopMatrix();
    glFlush();
}
