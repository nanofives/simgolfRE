#ifndef GOLFERSTATS_H
#define GOLFERSTATS_H

#include "crt.h"
#include "Golfer.h"
#include "FloatingText.h"
#include "Screen.h"
#include "InfoScreenAssets.h"
#include "InputPoll.h"
#include "Scorecard.h"

enum GolferStatsSuffixesUnrecovered {
    GOLFER_STATS_SUFFIX_UNRECOVERED_1,
    GOLFER_STATS_SUFFIX_UNRECOVERED_2,
    GOLFER_STATS_SUFFIX_UNRECOVERED_3,
    GOLFER_STATS_SUFFIX_UNRECOVERED_4
};

int GolferAge(int golfer);

void AppendSkillWord(int v);

void RenderHoleStatsScreen(int hole);

void RenderPlayerCommentsReport();

enum GolferStatsLabelsUnrecovered {
    GOLFER_STATS_LABEL_UNRECOVERED_1,
    GOLFER_STATS_LABEL_UNRECOVERED_2,
    GOLFER_STATS_LABEL_UNRECOVERED_3,
    GOLFER_STATS_LABEL_UNRECOVERED_4,
    GOLFER_STATS_LABEL_UNRECOVERED_5,
    GOLFER_STATS_LABEL_UNRECOVERED_6,
    GOLFER_STATS_LABEL_UNRECOVERED_7,
    GOLFER_STATS_LABEL_UNRECOVERED_8,
    GOLFER_STATS_LABEL_UNRECOVERED_9,
    GOLFER_STATS_LABEL_UNRECOVERED_10,
    GOLFER_STATS_LABEL_UNRECOVERED_11,
    GOLFER_STATS_LABEL_UNRECOVERED_12,
    GOLFER_STATS_LABEL_UNRECOVERED_13,
    GOLFER_STATS_LABEL_UNRECOVERED_14,
    GOLFER_STATS_LABEL_UNRECOVERED_15
};

extern char g_numString[];
extern int g_lastHundredthsShown;
extern int g_remarkColourValue;
extern int g_displayFocus;
extern char g_reportScreenId;

void AppendMoney(int hundredths);
void BuildGolferDialogue(int msgId, int actionArg, int strokeCode, int golfer);

#endif
