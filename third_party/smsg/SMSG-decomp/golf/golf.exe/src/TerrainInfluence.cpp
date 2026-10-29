#include "jgldefs.h"
#include "crt.h"
#include "Geometry.h"
#include "TerrainInfluence.h"
#include "TerrainGrid.h"

void AdjustMoodInRadius(int row, int col, int amount)
{
    int amt = amount * 2 / 3;
    int dy, dx;

    for (dy = -7; dy <= 7; dy++) {
        int r = row + dy;
        for (dx = -7; dx <= 7; dx++) {
            int c = col + dx;
            int d;
            if (IsPositionInvalid(r, c))
                continue;
            d = ApproxDistance(dy * 3, dx * 3);
            if (amt > 0) {
                int t = g_goodMoodMap[r][c];
                t += amt / (d + 2);
                g_goodMoodMap[r][c] = Clamp(t, 0, 255);
            } else {
                int t = g_badMoodMap[r][c];
                t -= amt / (d + 2);
                g_badMoodMap[r][c] = Clamp(t, 0, 255);
            }
        }
    }
}

void RecalculateMoodMaps()
{
    int i, j;
    int total = 0;

    for (i = 0; i < 50; i++)
        for (j = 0; j < 50; j++)
            total += g_ballStopCountMap[i * 50 + j];
    memset(g_goodMoodMap, 0, 2500);
    memset(g_badMoodMap, 0, 2500);
    if (total < 10)
        return;
    if (total > 1750)
        total = 1750;
    for (i = 0; i < 50; i++)
        for (j = 0; j < 50; j++) {
            if (g_posReactionTileCount[i * 50 + j])
                AdjustMoodInRadius(i, j, g_posReactionTileCount[i * 50 + j] * 20000 / total);
            if (g_gameDispleasedTally[i * 50 + j])
                AdjustMoodInRadius(i, j, g_gameDispleasedTally[i * 50 + j] * -40000 / total);
        }
}

unsigned char g_badMoodMap[50][50];
static int s_unused0;
int g_infoPanelHover;
unsigned char g_goodMoodMap[50][50];
int g_hot;
char g_numBuf[16] = {0};
int g_bgEnabled = 0;
