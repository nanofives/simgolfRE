#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include "crt.h"

#include "Golfer.h"
#include "BuildingSearch.h"
#include "StringPool.h"
#include "ToolbarSelect.h"
#include "CourseStats.h"

extern unsigned char *g_jpegOutRowPtr;
extern unsigned short *g_sampleSrcPtr;
extern int g_jpegBlockSize;
extern int g_jpegSurfacePitch;

extern char g_gameCoursePath[];
extern char *g_courseTypeNames[4];

extern "C" int CreateJPG(FILE *outfile, int width, int height, unsigned char *(*getRow)(int));

unsigned char *DownsampleRGB555ToRGB888(int count);
void SaveSurfaceToJPEG(const char *filename, AssetSurface *surface, int x, int y, int width, int height, int blockSize);
void SaveScreenshotToJPEG(char *path);
int SaveCourseInfoToText(char *path);
void ProcessPendingScreenshot();
void SetCourseFileName(const char *name);

#endif
