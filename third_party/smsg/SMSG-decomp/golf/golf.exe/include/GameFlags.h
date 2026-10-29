#ifndef GAMEFLAGS_H
#define GAMEFLAGS_H

extern int g_runModeFlags;

#define GAME_PAUSED                  4
#define GAME_TOURNAMENT              0x200000
#define GAME_SANDBOX                 0x1000000
#define GAME_SCRIPT_PLAYING          0x2000000
#define GAME_CHAMPIONSHIP            0x4000000
#define GAME_ELEVATION_TOOL          2
#define GAME_DEBUG_OVERLAY           0x20
#define GAME_SECOND_TEES             0x40
#define GAME_NO_CLUBHOUSE            0x800
#define GAME_PRO_CHALLENGE           0x2000
#define GAME_START_MATCH             0x4000
#define GAME_TUTORIAL                0x8000
#define GAME_SLOW_PACE               0x20000
#define GAME_ROUTE_RECORDED          0x40000
#define GAME_TOURNAMENT_OFFERED      0x400000
#define GAME_DEMOLISH_TOOL           0x8000000
#define GAME_SAVE_CUSTOM_TYPES       0x40000000
#define GAME_FAST_CLOCK              8
#define GAME_ANY_TOURNAMENT          (GAME_CHAMPIONSHIP | GAME_TOURNAMENT)

#endif
