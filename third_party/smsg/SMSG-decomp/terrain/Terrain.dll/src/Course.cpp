#include "Terrain.h"
#include "Bmp.h"
#include "GL/gl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


void SetCourseTextureId(int nType, int nIndex, int nSub, int uTex);
unsigned int LoadTgaTexture(const char *pszFile);

void Terrain::loadNewCourseType(int nType)
{
    if (g_nCourseType != nType) {
        g_nCourseType = nType;
        LoadLightingForCourseType();
        freeCourseTextures();
        LoadCourseTextures();
    }
}
