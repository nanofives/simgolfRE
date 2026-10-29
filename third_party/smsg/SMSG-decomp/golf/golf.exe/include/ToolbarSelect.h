#ifndef TOOLBARSELECT_H
#define TOOLBARSELECT_H

void RenderDialogueBubble();
unsigned int GetColorForSkillSet(int skills);
void FormatDateString(int clock);
void SelectScreenItem(int index, int useExplicitId);
void AppendCourseName(int form);

struct TourSite {
    char city[0x18];
    signed char id;
    char courseName[0x21];
    short x;
    short y;
    signed char landscape;
    signed char coast;
    signed char relief;
    char feature[0x41];
};
extern TourSite g_tourSites[];

extern int g_shotSelectedKind;

extern int g_simListItemRefs[];
extern int g_toolbarItemIDCnt;

extern int g_brushPicks[];

extern int g_pickState;
extern int g_selectedVariant;

#endif
