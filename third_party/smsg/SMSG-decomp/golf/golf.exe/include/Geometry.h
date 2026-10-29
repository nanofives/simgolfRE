#ifndef GEOMETRY_H
#define GEOMETRY_H

int Clamp(int v, int lo, int hi);
int Sign(int v);

int ApproxDistance(int dx, int dy);

int GetDirection8(int dx, int dy);
extern int g_dirRowStep[8];
extern int g_dirColStep[8];

int CalculateAngle(int dx, int dy);
int FixedPointSin(int angle, int amp);
int FixedPointCos(int angle, int amp);

extern unsigned long g_startTime;
void InitTimer();

extern char g_noiseGrid[20][19];
void GenerateRandomTerrainNoise();

extern signed char g_bilerpGrid[20][19];
int BilinearSample(int x, int y);

#endif
