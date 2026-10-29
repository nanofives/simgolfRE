#ifndef SCORECARD_H
#define SCORECARD_H

#include "crt.h"
#include "Golfer.h"
#include "FloatingText.h"
#include "ScreenText.h"

#define EXHIBITION_STAKE    2000

void RenderScorecard(int player);

enum ScorecardLabelsUnrecovered {
    SCORECARD_LABELS_UNRECOVERED_1
};
extern Font g_boldArialFont;
extern char g_numBuf[];
struct ScorecardSignUnrecovered;
extern int g_proMatchStakeLvl;

#endif
