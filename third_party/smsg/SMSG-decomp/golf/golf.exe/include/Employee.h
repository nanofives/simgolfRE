
#ifndef EMPLOYEE_H
#define EMPLOYEE_H

struct Employee {
    int posX;
    int posY;
    int screenX;
    int screenY;
    signed char postRow;
    signed char postCol;
    signed char flags;
    signed char job;
    signed char talkKind;
    signed char talkTimer;
    signed char dir;
    unsigned char pad17;
    short stepsLeft;
    short waitTimer;
    short animTick;
    short anim;
    short talkGolfer;
    short hiredAt;
    short paid;
    short tally;
    unsigned char pad28[2];
    char customName[0x22];
};

extern Employee g_staffSlots[64];

extern char *g_jobTitles[10];
extern char *g_staffFirstNames[10];

extern signed char g_jobWages[12];

extern int g_selectedItemIndex;

int SpawnEmployee(int job);

void RenderEmployees(void);

void UpdateAllEmployees(void);



#define CARRIED_GOLFER               0x80
#define STAFF_UPGRADED               8
#define STAFF_QUITTING               0x10
#define JOB_PLAYER_AVATAR            (-6)

#endif
