#ifndef TERRAININFLUENCE_H
#define TERRAININFLUENCE_H

extern unsigned char g_badMoodMap[][50];
extern unsigned char g_goodMoodMap[][50];
extern unsigned char g_ballStopCountMap[];
extern unsigned char g_posReactionTileCount[];
extern unsigned char g_gameDispleasedTally[];

void AdjustMoodInRadius(int row, int col, int amount);

void RecalculateMoodMaps();

#endif
