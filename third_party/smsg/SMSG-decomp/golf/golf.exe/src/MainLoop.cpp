#include <fcntl.h>
#include <sys/stat.h>
#include "crt.h"
#include "MainLoop.h"

#define LAST_TILE                    0x31
#define GRID_CENTER                  0x19
#define TILE_WORLD_HALF              0x200
#define TILE_WORLD_SHIFT             10
#define TILE_ORIGIN_MASK             0xfffffc00
#define SUBTILE_SHIFT                4
#define YARDS_PER_TILE               0x19
#define TILE_YARDS_SQUARED           625
#define DIRECTION_COUNT              8
#define DIRECTION_MASK               7
#define DIRECTION_OPPOSITE           4
#define AXIS_COUNT                   4
#define FACING_MASK                  3
#define FACING_COUNT                 4
#define ISO_Y_SCALE_NUM              0x14
#define ISO_Y_SCALE_DEN              0x20
#define TILE_PIXELS_PER_ZOOM         0xb
#define TILE_SPAN_PER_ZOOM           8
#define TILE_HALF_HEIGHT             0x14
#define TEXT_MIN_ZOOM                2
#define LABEL_LIFT_PER_ZOOM          10
#define CLIFF_MAX_HEIGHT             10
#define MAX_VERTEX_HEIGHT_FLAT       10
#define MAX_VERTEX_HEIGHT            13
#define ELEV_TOOL_AREA               2
#define ELEV_TOOL_COUNT              3
#define GRID_MODE_VERTEX             0
#define GRID_MODE_SQUARE             1
#define GRID_MODE_AREA               2
#define GRID_MODE_ANALYZE            3
#define SANDBOX_CLEAR_HEIGHT         4

#define SCREEN_CENTER_X              400
#define SCREEN_LAST_X                0x31f
#define SCREEN_LAST_Y                0x257
#define VIEW_CENTER_X                0x15e
#define VIEW_CENTER_Y                300
#define TOOLBAR_TOP                  0x1e0
#define MAIN_BAR_TOP                 0x1b8
#define MAIN_BAR_WIDTH               200
#define RIGHT_PANEL_X                0x294
#define CONFIRM_DIALOG_TOP           0x8c
#define OFFER_DIALOG_TOP             0x168
#define DIALOG_TOP                   100
#define EXPAND_DIALOG_X              0x1c2
#define EXPAND_DIALOG_TOP            0xa0
#define CAREER_OVER_DIALOG_X         0xfa
#define CAREER_OVER_DIALOG_TOP       0x96
#define REPORT_PANEL_HALF_WIDTH      200
#define REPORT_PANEL_MARGIN          8

#define COLOR_RED                    0x80007d08
#define COLOR_GREEN                  0x80002308
#define COLOR_LIGHT_GREEN            0x800043f0
#define COLOR_DEEP_GREEN             0x80000100
#define COLOR_BLUE                   0x80000018
#define COLOR_BRIGHT_BLUE            0x8000211f
#define COLOR_PALE_BLUE              0x8000631f
#define COLOR_CYAN                   0x800003ff
#define COLOR_DARK_PURPLE            0x80002008
#define COLOR_MAGENTA                0x80006090
#define COLOR_PINK                   0x80007e9f
#define COLOR_PALE_PINK              0x80007f9c
#define PIXEL_WHITE                  0x7fff

#define GAME_HIDE_TREES              0x400
#define GAME_HOLD_FIRST_TEE          0x100000
#define GAME_CHEATED                 0x20000000

#define TRIGGER_PRO_CHALLENGE        0x40
#define TRIGGER_CELEBRITY_HOME       0x80
#define TRIGGER_UPSET_GOLFER         8
#define TRIGGER_ADVANCE_STORY        4
#define TRIGGER_DELAY_TICKS          0x10

#define TILE_STYLE_MASK              0x1f
#define TILE_CLEAR_SCENIC            0xfeff
#define TILE_CLEAR_WEEDS             0xf7ff
#define TILE_CLEAR_ANIMATING         0xbfff
#define TILE_ANIM_DONE               0xff
#define TILE_ANIM_LAST_FRAME         0x18
#define UNDO_PATH                    0x80
#define UNDO_BENCH                   0x81
#define UNDO_FLOWER_BED              0x82
#define UNDO_SCENIC_TREE             0x90
#define UNDO_SCENIC_BRIDGE           0x93
#define TILE_KEEP_ON_WIPE            0x480

#define RISE_EAST                    0x04
#define RISE_SOUTH                   0x10
#define RISE_WEST                    0x40


#define CATEGORY_WASTE_BUNKER        8

#define LANDMARK_STANDING_STONES     3
#define LANDMARK_BASE_PRICE          0x19
#define LANDMARK_PRICE_STEP          5
#define PRICE_MARKUP_PERCENT         200
#define BUILDING_LEVEL_MAX           99
#define EVENT_NEW_BUILDING_TYPE      0x40
#define SCRIPT_PLACE_BUILDING        0x100
#define BENCH_STYLES                 5
#define BENCH_DESIGNS                7
#define BRIDGE_STYLES                8
#define HOME_SITE_LAYOUTS            6

#define HOLE_SLOT_COUNT              19
#define HOLE_AWARDS                  (HOLE_TOP_100 | HOLE_TOP_18)
#define HOLE_UPHILL                  0x1000
#define HOLE_DOWNHILL                0x2000
#define TEE_VARIANTS                 3
#define MIN_PAR                      3
#define SHORT_HOLE_YARDS             250
#define DOGLEG_MIN_ANGLE             0x71c71c6
#define LONG_HOLE_YARDS              200
#define RANGE_DISCOUNT_YARDS         300
#define PAR2_MAX_YARDS               0x32
#define PAR4_MIN_YARDS               0xfa
#define PAR5_MIN_YARDS               0x1db
#define PAR6_OVER_YARDS              0x271
#define FLAG_DIFFICULTY_STEP         0x14

#define ROUTE_GOLFER                 0x9a
#define ROUTE_COUNT                  4
#define FIRST_TRACED_ROUTE           2
#define ROUTE_MAX_SHOTS              5
#define LANDING_SWEEP_START          0xaaaaa9a
#define ANGLE_15_DEGREES             0xaaaaaaa
#define ANGLE_45_DEGREES             0x20000000
#define LANDING_SWEEP_SPOKES         25
#define LANDING_FIRST_YARDS          150
#define LANDING_STEP_YARDS           50
#define LANDING_END_YARDS            300
#define LANDING_FIRST_RADIUS_SCALED  0x19000
#define LANDING_STEP_RADIUS_SCALED   0xc800
#define LANDING_SPOTS_PER_HOLE       3
#define LANDING_EDGE_LOW             0x18
#define LANDING_EDGE_HIGH            0x30

#define GOLFER_COUNT                 0x98
#define STAND_IN_GOLFER              0x99
#define SELECTED_SPOTLIGHT           0x100
#define PRO_ROSTER_SIZE              100
#define SKILL_COUNT                  0xc
#define PLAYER_SKILL_SLOTS           12
#define SHOT_SKILL_COUNT             10
#define SKILLS_NO_ACCURACY           5
#define SKILLS_NO_IMAGINATION        3
#define TALENT_BITS                  3
#define GOLFER_REPLAYING             0x8000000
#define STORY_RESULT_RANGE           0x20
#define ACTION_CALMED_BY_SCENERY     0x2c
#define MSG_CODE_MASK                0x3fff
#define SEEN_STORY_START             0x40000
#define TALK_DIM_BELOW               5
#define ATTITUDE_LIMIT               4
#define FORCED_SKILL_RATING          500

#define SWING_DOWNSWING              4
#define SWING_IMPACT                 5
#define SWING_FOLLOW_THROUGH         6
#define GOLFER_WALKING               7
#define GOLFER_STANDING              11
#define GOLFER_HAPPY_REACTION        12
#define GOLFER_SAD_REACTION          13
#define GOLFER_POINTING              14
#define GOLFER_ADDRESSING            16
#define GOLFER_SITTING_DOWN          17
#define GOLFER_SEATED                18
#define GOLFER_WASHING_BALL          19
#define ANIM_STAND                   10
#define ANIM_FIDGET                  0x14
#define ANIM_HAPPY                   0x1e
#define ANIM_SWING                   0x28
#define ANIM_SWING_NO_ACCURACY       0x32
#define ANIM_SWING_NO_IMAGINATION    0x37
#define ANIM_SITTING_DOWN            0x3c
#define ANIM_SEATED                  0x46
#define ANIM_PITCH                   0x50
#define ANIM_PUTT                    0x5a
#define ANIM_LINE_UP_PUTT            100
#define ANIM_POINT_AT                0x6e
#define ANIM_ADDRESS                 0x78
#define ANIM_PITCH_ADDRESS           0x82
#define ANIM_PUTT_ADDRESS            0x8c
#define ANIM_LEAN_RIGHT              0x96
#define ANIM_LEAN_LEFT               0xa0
#define ANIM_LOOK_UP                 0xaa
#define ANIM_SUCCESS                 0xb4
#define ANIM_SAD                     0xbe
#define ANIM_FAILURE                 0xc8
#define ANIM_TIRED_WALK              0xd2
#define ANIM_HANDSHAKE               0xdc
#define ANIM_CART                    0xe6
#define FEMALE_VARIANT_BASE          5
#define LEAN_LOOP_LAST               0x14
#define LEAN_LOOP_FIRST              6
#define FIDGET_STAGGER               0x25
#define FIDGET_CYCLE_MASK            0x7f
#define WATCH_CURVING_BALL_TILES     3
#define WATCH_BALL_TILES             5
#define MAX_ANIM_FRAME               99
#define WEED_NO_FRAME_CAP            999

#define LAST_FULL_SWING_CLUB         6
#define LAST_FULL_SWING_CLUB_HIGH    8
#define LAST_AIMED_CLUB              0xb
#define BACKSPIN_REACH_NUM           0xd
#define BACKSPIN_REACH_DEN           0x12
#define PATH_DIVISIONS               0x14
#define BACKSPIN_PATH_DIVISIONS      0x10
#define LOFT_NORMAL                  2
#define SKILL_LONG_DRIVER            2
#define SKILL_ACCURATE_DRIVER        4
#define SKILL_ACCURATE_IRONS         8
#define SKILL_DRAW_SHOT              0x20
#define SKILL_FADE_SHOT              0x40
#define SKILL_HIGH_BACKSPIN          0x80
#define SKILL_RECOVERY               0x100

#define STAFF_COUNT                  64
#define BANNER_TILE_WIDTH            0x20
#define COMPLAINT_TICKS              100
#define COMPLAINT_STAGGER            0x23
#define WAGE_ODDS_BASE               4

#define SOUND_HALF_VOLUME            0x32
#define AMBIENT_SOUND_ODDS           100
#define AMBIENT_SOUND_KINDS          6
#define AMBIENT_PAN_RANGE            0x80
#define PAN_CENTER                   0x40
#define AMBIENT_VOLUME_RANGE         0x28
#define AMBIENT_BASE_VOLUME          0x3c
#define AMBIENT_PITCH_SPREAD         200


#define BANK_SPLASH                  0xf8
#define BANK_WILLOW_TREE             0xf9
#define BANK_CACTUS_GROUP            0xfa
#define BANK_RHODY                   0xfb
#define BANK_SCENIC_FLOWERS          0xfc
#define BANK_SCENIC_TREE             0xfd
#define BANK_ORNAMENTAL_GRASS        0xfe
#define BANK_VINEYARD                0xff
#define BANK_POT_OF_GOLD             0x100
#define BANK_ROSE_BUSH               0x101
#define BANK_ROCK_FORM               0x102
#define BANK_COW_SKULL               0x127
#define BANK_DEAD_GOLFER             0x128
#define BANK_DESERT_WATER_ROCK       0x129
#define BANK_GROUSE                  0x12a
#define BANK_NATURAL_BRIDGE          299
#define BANK_DEAD_SWAMP_TREES        300
#define BANK_TROPICAL_WILDFLOWERS    0x12d
#define BANK_LINKS_ROCKS             0x12e
#define BANK_SPECIMEN_TREE_BASE      0x12e
#define BANK_PLUM                    0x12f
#define BANK_SCENIC_TREES            0x12f
#define BANK_SCENIC_ELM              0x131
#define BANK_LANDMARKS               0x168
#define BANK_STANDING_STONES         0x16b
#define BANK_PARKLAND_ROCK           0x16d
#define BANK_EASTER_HEAD             0x173
#define BANK_PUTTING_GREEN_ICON      0x17c
#define BANK_DRIVING_RANGE_ICON      0x17e
#define BANK_CRABGRASS               0x184
#define BANK_FLAG_POP                0x185
#define BANK_FLAG_OPEN               0x189
#define BANK_TEE_MARKER              0x18d
#define BANK_PINE_SMALL              0x192
#define BANK_PINE_MEDIUM             0x193
#define BANK_PINE_LARGE              0x194
#define PINE_SPECIES_OFFSET          6
#define BANK_PALM_SMALL              0x195
#define BANK_PALM_MEDIUM             0x196
#define BANK_PALM_LARGE              0x197
#define BANK_FIR_SMALL               0x198
#define BANK_TREE_SMALL              0x19b
#define BANK_TREE_MEDIUM             0x19c
#define BANK_TREE_LARGE              0x19d
#define TREE_SIZES                   3
#define BANK_DOLPHIN                 0x19f
#define BANK_WATERSPRAY              0x1a0
#define BANK_FLOWERS_ONE_SIDE        0x1a3
#define FLOWER_SIN_BANKS             6
#define FLOWER_WALLED_BANKS          0xc
#define BANK_GAZEBO                  0x1b4
#define BANK_CART_GARAGE             0x1c2
#define BANK_MARINA                  0x1ce
#define BANK_SWIM_CLUB               0x1d4
#define BANK_AIRSTRIP                0x1da
#define BANK_PRO_SHOP                0x1e0
#define BANK_RESORT_HOTEL            0x1e6
#define BANK_BRIDGE_TILE             0x1fe
#define BANK_BRIDGE_CAP              0x1ff
#define BANK_BRIDGE_L                0x200
#define BANK_BRIDGE_T                0x201
#define BANK_BRIDGE_X                0x202
#define BANK_BRIDGE_REFLECTION       0x204
#define BANK_BRIDGE_RAMP             0x205
#define BANK_BENCH                   0x208
#define BANK_SCENIC_BRIDGE           0x226
#define BANK_ROCK_ABOVE_WATER        0x22e
#define BANK_ROCK_UNDERWATER         0x230
#define BANK_WAVE                    0x233
#define BANK_WATERFALL_SHORT         0x237
#define BANK_WATERFALL_TALL          0x238
#define BANK_SPRAY_SHORT             0x23b
#define BANK_SPRAY_TALL              0x23c

#define PAL_PRO_SHOP                 0xe
#define PAL_SNACK_BAR                0x10
#define PAL_RESORT_HOTEL             0x11
#define PAL_AIRSTRIP                 0x12
#define PAL_CART_GARAGE              0x17
#define PAL_CLUBHOUSE                0x19
#define PAL_MARINA                   0x1b
#define PAL_SWIM_CLUB                0x1c
#define PAL_WATERSPRAY               0x1d
#define PAL_TREE                     0x25
#define PAL_PINE                     0x29
#define PAL_PINE_ALT                 0x2a
#define PAL_FLOWER_BED               0x2d
#define PAL_FLOWER_BED_BUILDING      0x2e
#define PAL_FLOWER_BED_RED           0x31
#define PAL_PALM                     0x32
#define PAL_WILLOW                   0x36
#define PAL_CRABGRASS                0x4c
#define PAL_WEED_FLOWERS             0x4d
#define PAL_BRIDGE                   0x4e
#define PAL_ORNAMENTAL_GRASS         0x55
#define PAL_VINEYARD                 0x56
#define PAL_CACTUS_GROUP             0x57
#define PAL_RHODY                    0x58
#define PAL_SCENIC_FLOWERS           0x59
#define PAL_DOLPHIN                  0x5a
#define PAL_ROSE_BUSH                0x5b
#define PAL_ROCK_FORM                0x5c
#define PAL_SCENIC_TREE              0x5f
#define PAL_FLAG                     99
#define PAL_LANDMARK_BASE            100
#define PAL_PARKLAND_ROCK            0x69
#define PAL_LIGHTHOUSE               0x6b
#define PAL_GOLD                     0x81
#define PAL_BENCH_TEST_BASE          0x8c
#define PAL_BUILDING_TYPE_BASE       0x90
#define PAL_GAZEBO                   0x96
#define PAL_COW_SKULL                0xa0
#define PAL_DEAD_GOLFER              0xa1
#define PAL_DESERT_WATER_ROCK        0xa2
#define PAL_GROUSE                   0xa3
#define PAL_NATURAL_BRIDGE           0xa4
#define PAL_DEAD_SWAMP_TREES         0xa5
#define PAL_SCENIC_ELM               0xa6
#define PAL_TROPICAL_WILDFLOWERS     0xa7
#define PAL_LINKS_ROCKS              0xa8
#define PAL_SCENIC_BRIDGE            0xa9
#define PAL_BENCH                    0xaa
#define PAL_PRACTICE_ICON            0xb1
#define PAL_WATER                    0xb2
#define PAL_SPECIMEN_TREE_BASE       0xb3
#define PAL_PLUM                     0xb4
#define PAL_SCENIC_TREES             0xb4
#define PAL_FLOWERS_WILTED           0xbb
#define PAL_TOPIARY                  0xbc

#define SPRITE_TRANSLUCENT           0x200
#define SPRITE_RED_SILHOUETTE        0x800
#define SPRITE_SMALL_SCALE           3
#define SPRITE_ORIGIN_SMALL          0x28
#define SPRITE_ORIGIN_MEDIUM         0x3c
#define SPRITE_ORIGIN_BUILDING       0x8c
#define SPRITE_ORIGIN_LARGE          0xf0
#define SPRITE_ORIGIN_SCENIC         100
#define GOLFER_ORIGIN_X              0x1e
#define GOLFER_ORIGIN_Y              0x28
#define GROUSE_ORIGIN_X              0x28
#define GROUSE_ORIGIN_Y              0x26
#define CLIFF_ORIGIN_Y               0x28
#define CLIFF_RIGHT_END_ORIGIN_X     0x13
#define CLIFF_TOP_END_ORIGIN_X       0xd
#define CLIFF_LEFT_CORNER_ORIGIN_X   6
#define CLIFF_HEIGHTS                8
#define CLIFF_PIECES                 7
#define SHORT_FALL_MAX_DROP          3
#define LINKS_BACK_LEFT_AXIS         5
#define SPARKLE_CYCLE_MASK           0x1f
#define SPARKLE_FRAMES               5
#define FLOWER_COLORS                5
#define FLOWER_WALLED_VARIATION      10
#define ORNAMENT_SPACING             5
#define DOLPHIN_CYCLE                64
#define PIN_RING_DX                  0x14
#define PIN_RING_DY                  10
#define HEIGHT_LABEL_DX              0x10
#define HEIGHT_LABEL_DY              3
#define SLOPE_LABEL_DY               4
#define SURNAME_COUNT                16
#define ADJECTIVE_COUNT              8

#define TITLE_ART_COUNT              7
#define MENU_BUTTON_COUNT            6
#define MENU_CAPTION_COUNT           5
#define TITLE_LOGO                   6
#define MENU_HOVER_RADIUS            0xa0
#define MENU_HOVER_Y_SCALE           4
#define MENU_HOVER_Y_OFFSET          0x18
#define MENU_CAPTION_DY              8
#define MENU_CAPTION_HOVER_DY        6
#define MENU_THEME_DY                0x18
#define MENU_THEME_REST_DY           0x1a
#define MENU_SELECT_THEME            2
#define MENU_CHAMPIONSHIP            3
#define MENU_SANDBOX                 4
#define MENU_EXIT                    5
#define START_CHAMPIONSHIP           2
#define START_SANDBOX                3
#define START_SELECT_THEME           4
#define START_QUIT                   (-2)
#define COURSE_ID_RANGE              0xffff
#define PAINT_SLOT_COUNT             0x10
#define LEVEL_SAVED_GAME             6

#define SANDBOX_START_MONEY          100000
#define SANDBOX_MONEY                10000
#define DOLLARS_PER_MONEY_UNIT       100
#define CHEAT_MONEY                  100
#define FACILITIES_START             6
#define LANDMARKS_ALL                0x3fff
#define PATH_TOOL_COST               2
#define PATH_TOOL_COST_WATER         0xf
#define BENCH_TOOL_COST              2
#define HOME_SITE_MIN_SCORE          0x32
#define HOME_APPEAL_DECAY            12
#define FORCED_HOME_APPEAL           2500
#define CELEBRITY_COUNT              100
#define CELEB_APPEAL_BASE            400
#define CELEB_APPEAL_STEP            300
#define CHARGES_PER_MONTH_BASE       2
#define STAT_DECAY_DIVISOR           8
#define HOLE_STAT_LONG_DRIVE         5
#define HOLE_STAT_COUNT              8
#define HOLE_COMMENT_KINDS           0x40
#define DEBT_INTEREST_DIVISOR        0x32
#define TALLY_DECAY_SHIFT            3
#define GOLFER_TYPE_COUNT            76
#define RIVAL_RATING_CATEGORIES      4
#define RIVAL_COURSE_COUNT           9
#define RATING_WEALTH_UNIT           1000
#define MAX_WEEDY_NEIGHBOURS         3
#define FLOWER_WEED_ODDS             3
#define NEIGHBOUR_WEEDY              1
#define NEIGHBOUR_TURF               2
#define WEED_SPREAD_MASK             (NEIGHBOUR_WEEDY | NEIGHBOUR_TURF)
#define WEED_GROWTH_ODDS             8

#define PRO_CHALLENGE_FIRST_HOLE     3
#define PRO_RATING_BIAS              0x14
#define RATING_PER_STAKE_LEVEL       5
#define DRAWS_PER_RATING_STEP        4
#define WAGER_PER_HOLE_UNIT          400
#define WAGER_MATCH_UNIT             800
#define ARRIVAL_TICK_MASK            0x7f
#define MIN_WAITING_GOLFERS          2
#define MAX_WAITING_GOLFERS          8

#define TICKS_PER_MONTH              0x400
#define CUSTOMIZE_PROMPT_CLOCK       0x2200
#define FIRST_MONTH                  3
#define DAYS_PER_MONTH               0x1e
#define FIRST_GAME_YEAR              0x7d1
#define HISTORY_MONTHS               500
#define LEDGER_YEARS                 100
#define LEDGER_FACILITIES            4
#define LEDGER_MAINTENANCE           6

#define REPORT_COURSE_TABLE          2
#define REPORT_PLAYER_COMMENTS       3
#define REPORT_HISTOGRAPH            4
#define REPORT_KEYBOARD_HELP         5
#define REPORT_FINANCES              6
#define REPORT_GOLFER_PAIRING        7
#define REPORT_TOP10_COURSES         8
#define REPORT_SGA_EVALUATION        9
#define REPORT_MEMBERSHIP            10
#define SGA_EVALUATION_QUIET         2
#define PICK_RADIUS                  0x28
#define FACILITY_CLICK_PAUSE         10
#define EXPAND_FANFARE_TICKS         0x5a
#define KEY_SCROLL_TILES             4

#define KEY_FAIRWAY                  'f'
#define KEY_ROUGH                    'r'
#define KEY_GREEN                    'g'
#define KEY_SAND                     's'
#define KEY_LOW_PUNCH                'l'
#define KEY_TREES                    't'
#define KEY_WATER                    'w'
#define KEY_RAVINE                   'v'
#define KEY_HOLE                     'h'
#define KEY_DRAW_SHOT                'd'
#define KEY_CANCEL_MATCH             'n'
#define KEY_UNZOOM                   'x'
#define KEY_ZOOM                     'z'
#define KEY_RECENTER                 '0'
#define KEY_PATHWAY                  'p'
#define KEY_BENCHES                  'b'
#define KEY_CLEAR_COURSE             'X'
#define KEY_SAVE_CHAMPIONSHIP_COURSE 'C'
#define KEY_SAVE_GAME                'S'
#define KEY_LOAD_GAME                'L'
#define KEY_TOGGLE_NAMES             'N'
#define KEY_RELOAD_BALL_PIC          'B'
#define KEY_TOURNAMENT_OFFER         'J'
#define KEY_ELEVATION_BAR            'e'
#define KEY_ROUTING_MAP              'R'
#define KEY_PAUSE                    'P'
#define KEY_TOGGLE_TREES             'T'
#define KEY_MONEY_CHEAT              '+'
#define KEY_ANALYZE_NEAREST_HOLE     '/'
#define KEY_ANALYZE_SHOT             '.'
#define KEY_GOLFER_INFO              '?'
#define KEY_LOWER                    '-'
#define KEY_RAISE                    '='
#define KEY_RESTART_COURSE           'W'

#define ROSTER_LEFT_X                0xcc
#define ROSTER_BUTTON_Y              0x23f
#define ROSTER_LABEL_Y               0x24f
#define ROSTER_SWATCH_WIDTH          0xe
#define ROSTER_SWATCH_HEIGHT         9
#define ROSTER_BUTTON_WIDTH          0x10
#define ROSTER_RIGHT_X               0x310
#define COURSE_PANEL_X               0x30
#define COURSE_PANEL_Y               5
#define HUD_TITLE_X                  0x90
#define COURSE_NAME_Y                0xd
#define COURSE_DATE_Y                0x1b
#define TROPHY_ROW_MAX               0x14
#define TROPHY_ROW_WIDTH             0xa0
#define ICON_STEP_MIN                2
#define ICON_STEP_MAX                0xc
#define HUD_ROW_CENTER_X             0x94
#define TROPHY_ROW_Y                 0x26
#define TALLY_X                      0x50
#define TALLY_STEP                   0x24
#define TALLY_TEXT_DX                0xe
#define GLYPH_TOUR_WIN               33
#define GLYPH_WAITING                20
#define WAITING_ROW_WIDTH            0x96
#define WAITING_ROW_MAX              10
#define WAITING_ROW_Y                0x32
#define WAITING_TALLY_X              0x88
#define MONEY_PANEL_X                0x289
#define MONEY_PANEL_Y                0xd
#define MONEY_TEXT_X                 0x2c4
#define MONEY_TEXT_Y                 0x23
#define FUN_PANEL_X                  0x2a5
#define FUN_PANEL_Y                  0x37
#define FUN_TEXT_X                   0x2d2
#define FUN_TEXT_Y                   0x4e
#define SKILL_PANEL_X                0x2bb
#define SKILL_PANEL_Y                99
#define SKILL_TEXT_X                 0x2dc
#define SKILL_TEXT_Y                 0x79
#define STATUS_TEXT_Y                10

#define CLOSEUP_SRC_W                0x50
#define CLOSEUP_SRC_H                0x46
#define CLOSEUP_W                    0xa0
#define CLOSEUP_H                    0x8c
#define CLOSEUP_MARGIN               0x50
#define CLOSEUP_MAX_X                0x2d0
#define CLOSEUP_MAX_Y                0x208
#define CLOSEUP_SRC_MARGIN           0x28
#define CLOSEUP_SRC_MAX_X            0x2f8
#define CLOSEUP_SRC_MAX_Y            0x230
#define HEADS_PER_SEX                72
#define HEAD_VARIANTS                3
#define DIVOT_FLIGHT_TICKS           0x40
#define DIVOT_COUNT                  256

#define SPOTLIGHT_MIN_WIDTH          0xa0
#define SPOTLIGHT_MIN_HEIGHT         0x78
#define SIMFOTO_MIN_WIDTH            0xf0
#define SIMFOTO_MIN_HEIGHT           200
#define SIMFOTO_MAX_LEFT             700
#define SIMFOTO_MAX_TOP              500
#define SIMFOTO_MIN_RIGHT            100
#define SIMFOTO_MAX_RIGHT            0x310
#define SIMFOTO_MAX_BOTTOM           0x248

#define LOT_PANEL_X                  0x2a4
#define LOT_PANEL_Y                  0x17c
#define LOT_PANEL_WIDTH              0x78
#define LOT_PANEL_HEIGHT             0x70
#define LOT_PANEL_CENTER_X           0x2e0
#define LOT_VALUE_Y                  0x195
#define LOT_CLEARING_Y               0x1a4
#define LOT_PREP_Y                   0x1b3
#define LOT_PROFIT_Y                 0x1c7
#define LOT_RULE_LEFT                0x2b0
#define LOT_RULE_RIGHT               0x310
#define LOT_RULE_Y                   0x1c2
#define PAINT_SPECIAL_BASE           0x80
#define PAINT_PATH                   0xa0
#define PAINT_PATH_BENCH             0x280
#define TREE_ICON_SLOT               7
#define ICONS_PER_THEME              20
#define SAND_TRAP_ICON_SLOT          2
#define TURNED_TRAP_ICONS            16
#define NEW_VARIATION_BASE           12

#define SHOT_PANEL_X                 0x196
#define SHOT_PANEL_ATTITUDE_Y        0x21e
#define SHOT_PANEL_CLUB_Y            0x22c
#define SHOT_PANEL_DISTANCE_Y        0x23a
#define SHOT_PANEL_LIE_Y             0x248
#define SKILL_LIST_X                 0x1ee
#define SKILL_LIST_X2                0x281
#define SKILL_LIST_TOP               0x21c
#define SKILL_LINE_HEIGHT            0xc
#define SKILL_LIST_BOTTOM            600

#define ANALYSIS_SHOT_COUNT          0xc
#define ANALYSIS_GROUP_COUNT         4
#define ALL_SKILLS_GROUP             3
#define SHOTS_PER_GROUP              3
#define LANDING_MARK_RADIUS          5
#define LIE_PENALTY_YARDS            10
#define CURSOR_MARK_RADIUS           10
#define ANALYSIS_PANEL_X             0x11a
#define ANALYSIS_PANEL_Y             0x1a
#define ANALYSIS_PANEL_W             0x100
#define ANALYSIS_PANEL_H             0x94
#define ANALYSIS_RULE_Y              0x40
#define ANALYSIS_TITLE_X             0x19a
#define ANALYSIS_TITLE_Y             0x22
#define ANALYSIS_SUBTITLE_Y          0x32
#define ANALYSIS_LABEL_X             0x128
#define ANALYSIS_LABEL_BOTTOM_Y      0x90
#define ANALYSIS_ROW_HEIGHT          0x14
#define ANALYSIS_YARDS_X             0x1ea
#define ANALYSIS_YARDS_BOTTOM_Y      0x93
#define BALL_TRAIL_COUNT             16

#define ACCOMP_FIRST_TOURNAMENT      3
#define ACCOMP_FIRST_500K_TOURNAMENT 8
#define ACCOMP_FIRST_1M_TOURNAMENT   0xd
#define PRIZE_500K                   500
#define PRIZE_1M                     1000

#define FRAME_PACE_SLOW              250
#define FRAME_PACE_NORMAL            175
#define CAREER_OVER_WARNINGS         3
#define LONG_CAREER_YEARS            10
#define CLUBHOUSE_DOOR_OFFSET        0x600

#define VIEW_CLEAR_ELEVATION_REBUILT 0xf7
#define TILE_CLEAR_HOLE              0xffe0
#define TILE_CLEAR_FLOWERS           0xefff
#define TILE_CLEAR_REPAINT           0xd6ff
#define GREEN_VARIATION_ALTERNATE    0xff
#define TILE_CLEAR_GROWTH            0xe7ff
#define TILE_CLEAR_BRIDGE            0xe7f8
#define TILE_CLEAR_FLOWER_SITE       0xf4ff
#define FLOWER_BED_COUNT_BASE        0x40
#define CONSTRUCTION_PITCH_JITTER    300
#define APPEAL_PER_MONEY_UNIT        4
#define HOME_VALUE_BUYBACK_DIVISOR   50
#define LANDMARK_REFUND_BASE         5
#define LANDMARK_REFUND_STEP         10
#define COSTLY_CLEARING_COST         50
#define COVER_BIT_GREEN              2
#define COVER_BIT_TREE               0x2000
#define COVER_CLEAR_TEE              0xfffffffe
#define NAME_PROMPT_SIZE             100
#define TOURNAMENT_COURSE_PENDING    (-2)
#define KEY_CLOSE_PANEL              ' '
#define ANALYSIS_RULE_RIGHT_X        (ANALYSIS_PANEL_X + ANALYSIS_PANEL_W)
#define LAST_ANALYSIS_SHOT           (ANALYSIS_SHOT_COUNT - 1)
#define ANGLE_TO_SIXTEENTHS_SHIFT    28
#define LOFT_HIGH_BACKSPIN           3
#define CLUB_PICK_NUM                0x3c
#define CLUB_PICK_DEN                3
#define SKILL_PERCENT_PER_POINT      10

#define LEVEL_PICK_THEME_AGAIN       5
#define EXIT_FADE_TICKS              0x3c

#define ARRIVAL_WAIT_MIN             6
#define ARRIVAL_WAIT_RANGE           6
#define SPEAKER_POPUP_ICON0          (-0x14)
#define SPEAKER_POPUP_ICON1          (-3)
#define SPEAKER_POPUP_ICON2          (-2)

#define CLIFF_WALL_CONTINUED         2
#define CLIFF_RIGHT_END              3
#define CLIFF_INNER_CORNER           4
#define CLIFF_BACK_LEFT_WALL         5
#define CLIFF_TOP_END                6
#define CLIFF_LEFT_CORNER            7
#define WATER_ANIM_ROW_STAGGER       7
#define TALL_FALL_OFFSET             0xd
#define GROUSE_SIZE_ODDS             3
#define PIN_BLINK_TICK_BIT           4U
#define PLAQUE_ORIGIN_X              9
#define PLAQUE_ORIGIN_Y              0x10
#define PLAQUE_ROW_STEP              3
#define PLAQUE_DY_PER_ZOOM           3

#define DEBUG_BAR_DIVISOR            4
#define SPRITE_ORIGIN_FLOWER_BED     0x2a
#define ORNAMENT_ORIGIN_X            0x50
#define ORNAMENT_ORIGIN_Y            0x4c
#define DOLPHIN_CYCLE_SHIFT          6
#define SPECIMEN_TREE_KINDS          8
#define ANIM_FRAME_USED              (-2)
#define FLOWER_BED_COLORS            4
#define HOLE_BED_PALETTE_SHIFT       0
#define BRIDGE_SIDE_UNRECOVERED(flags, value) ((flags) == ~(value))
#define ORNAMENT_Y_UNRECOVERED(y) (isTopiary = tileHalfHeight + 1, ornamentBank = isTopiary + 1, bedRow = ornamentBank + 1, bedRow + (y) - 3)

#define WAVE_VARIANTS                3
#define ROCK_VARIANTS                3
#define FALL_FROM_BACK_RIGHT         0
#define FALL_FROM_BACK_LEFT          1
#define FALL_INTO_BACK_RIGHT         2
#define FALL_INTO_BACK_LEFT          3
#define BRIDGE_LINK_MASKS            16
#define BENCH_DX_PER_ZOOM            4
#define BENCH_DY_HALVES_PER_ZOOM     5
#define BENCH_DEPTH_HALVES_PER_ZOOM  6
#define TEE_MARKER_DIAG_DX           4
#define TEE_MARKER_DIAG_DY           2
#define TEE_MARKER_DX                5
#define TEE_MARKER_DY                3
#define NAME_LABEL_PERIOD_SHIFT      5
#define NAME_LABEL_PHASE_MASK        7
#define CAPTION_BLINK_BIT            2
#define CAPTION_LINE_HEIGHT          0xd
#define HOLE_PROMPT_BLINK_MASK       0xc
#define LABEL_STAGGER                13
#define HOME_LABEL_BLINK_MASK        0x30
#define UNREACHED_BLINK_BIT          8
#define HOME_NAME_DY                 0xc
#define HOME_LINE_DY_PER_ZOOM        3
#define BUILDING_LABEL_DY            0x10
#define BED_SIDE_NEWEST              8
#define BED_SIDE_TOP_SHIFT           3
#define BED_BACK_SIDES               0x9
#define BED_SIDE_MASK                0xf
#define BED_ICON_ORIGIN_X            0x20
#define BED_ICON_ORIGIN_Y            0xe
#define TREE_TINT_SAMPLE_SHIFT       8
#define PALM_TINT_SAMPLE_SHIFT       7
#define TINT_BAND_HEIGHT             32
#define TINT_MASK                    3
#define PINE_SPECIES_BAND            13
#define PINE_SPECIES_BIT             4
#define ROUTE_PAR_AFTER_GREEN        3
#define ROUTE_LINE_SPACING           3
#define ROUTE_LINE_ALPHA             9
#define LINE_WIDTH_THIN              2
#define GRID_EDGE_WIDTH              3
#define BUBBLE_PADDING               8
#define BUBBLE_HEIGHT                10
#define BUBBLE_ALPHA                 5
#define BUBBLE_TAIL_DX               4
#define STORY_BUBBLE_STAGGER         7
#define PORTRAIT_SHIFT_X             0xc
#define PORTRAIT_OFFSET_X            0x38
#define PORTRAIT_OFFSET_Y            0x2a
#define PORTRAIT_MIRROR_X            0x18
#define NAME_BAR_HEIGHT              0xb
#define HINT_SWAP_BIT                0x20
#define HINT_GAP_MASK                0x18
#define HINT_TOOLBAR_BIT             0x40
#define HINT_TOOLBAR_SWAP_BIT        0x80
#define OUTFIT_MAN                   1
#define OUTFIT_MAN_IMAGINATIVE_ACCURATE 2
#define OUTFIT_MAN_IMAGINATIVE       3
#define OUTFIT_WOMAN_ELDER           5
#define OUTFIT_WOMAN_OLDER           6
#define OUTFIT_WOMAN                 7
#define OUTFIT_WOMAN_PLAYFUL         8
#define OUTFIT_SHIFT                 4
#define WAGGLE_FRAME                 3
#define IMPACT_EIGHTHS               5
#define SEAT_WAIT_TICKS              12
#define BALL_REST_SPEED              0x400
#define GOLFER_ID_FLAG               0x100
#define SPRITE_HALF_SCALE            2
#define RING_OFFSET_X                0x14
#define RING_OFFSET_Y                10
#define SIMFOTO_CAPTION_DY           4
#define SIMFOTO_DATE_DX              0x50
#define SIMFOTO_DATE_DY              0x10
#define EVEN_PIXEL_MASK              0xffe
#define SIMFOTO_JPEG_MARGIN          2
#define SIMFOTO_JPEG_GROW            6
#define ZOOM_VIEW_LEFT               (SCREEN_WIDTH / 4)
#define ZOOM_VIEW_TOP                (SCREEN_HEIGHT / 4)
#define ZOOM_VIEW_WIDTH              (SCREEN_WIDTH / 2)
#define ZOOM_VIEW_HEIGHT             (SCREEN_HEIGHT / 2)
#define ZOOM_VIEW_FACTOR             2
#define MOOD_BUTTON_OFFSET           2
#define MOOD_BUTTON_LAST             10
#define REGION_BADGE_X               3
#define REGION_BADGE_Y               3
#define DEBT_FLASH_BIT               4
#define MEASURE_LINE_MAX_Y           500
#define DEMOLISH_ICON_DX             0x10
#define DEMOLISH_ICON_DY             0x17
#define DEMOLISH_LABEL_DY            0xc
#define LANDMARK_REACH_STEP          5
#define LANDMARK_REACH_BASE          0x28
#define LANDMARK_REACH_SCALE         5
#define LANDMARK_REACH_DIVISOR       4
#define PAINT_DEMOLISH               (-2)
#define BRUSH_LABEL_DX               4
#define BRUSH_LABEL_DY               0xc
#define BRUSH_TOOL_LABEL_DY          0x10
#define PATH_LABEL_DY                8
#define PATH_BENCH_STYLES            8
#define TUTORIAL_CAMERA_OFFSET       4
#define VERTEX_ROW_STRIDE            (GRID_SIZE + 1)
#define ANALYSIS_GROUP_MASK          (ANALYSIS_GROUP_COUNT - 1)
#define POWER_TIER_MASK              3
#define HEIGHT_ABOVE_ANY_VERTEX      99
#define HOLE_RESULT_SHIFT            3
#define STORY_LUCK_RANGE             4
#define STORY_MOOD_MARGIN            4
#define TREE_SCALE_NORMAL            (-8)
#define TREE_SCALE_SMALL             (-7)
#define TREE_SCALE_LARGE             (-9)
#define INFO_RING_RADIUS             0xf

int g_holeAwardCount = 0;
int g_homesSold = 0;
int g_ratingWealth = 0;
int g_ratingUnawarded = 0;
int g_zoomView = 0;
int g_zoomDouble = 0;
int g_shotPlanMode = 0;
SpriteObject *g_curSprite = 0;
int g_bubbleStep = 0;
char g_statusPriority = 0;
int g_scriptCount = 0;
int g_useEffectPitches = 0;
int g_effectPitchNext = 0;

inline int BorderFacing(int bank, int facing, int parity)
{
    facing = bank == 0x1a5 ? facing + 1 & 3 : facing;
    return facing == -1 ? (facing = parity & 3) : facing;
}

inline int BridgeCapFacing(short side)
{
    return (-1 - side) & 3;
}

void OnFrameIdle()
{
}

void MainLoop()
{
    int tileHalfHeight = 0;
    int command;
    ConfigureAndPlaySound(SOUND_TITLE_MUSIC, SOUND_FULL_VOLUME, 0, 0, 0);
    memcpy(g_squareTypeTable, g_terrainTypeDefaults, sizeof g_terrainTypeDefaults);
    g_introBackdrop.Create(SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0, 0);
    g_saveGameSnapArt.Create(SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0, 0);
    g_golferHeadPage.Create(HEAD_PORTRAIT_SIZE, HEAD_VARIANTS * HEAD_PORTRAIT_SIZE, 8, 1, 0, 0);
    FLCPalette titlePalette;
    short titleArtPos[TITLE_ART_COUNT * 2];
    titleArtPos[0] = 0x2a;
    titleArtPos[1] = 0x15;
    titleArtPos[2] = 0x1a0;
    titleArtPos[3] = 0x1f;
    titleArtPos[4] = 0x1fe;
    titleArtPos[5] = 0x167;
    titleArtPos[6] = 0x11f;
    titleArtPos[7] = 0x1da;
    titleArtPos[8] = 0xd;
    titleArtPos[9] = 0x17e;
    titleArtPos[10] = 0x2d1;
    titleArtPos[11] = 0x210;
    titleArtPos[12] = 0xa8;
    titleArtPos[13] = 0xc6;
    short menuAnchor[MENU_BUTTON_COUNT * 2];
    menuAnchor[0] = 0xb6;
    menuAnchor[1] = 0x42;
    menuAnchor[2] = 0x23d;
    menuAnchor[3] = 0x58;
    menuAnchor[4] = 0x277;
    menuAnchor[5] = 0x199;
    menuAnchor[6] = 0x19f;
    menuAnchor[7] = 0x20d;
    menuAnchor[8] = 0x8d;
    menuAnchor[9] = 0x1af;
    menuAnchor[10] = 0x2d1;
    menuAnchor[11] = 0x210;
    char *menuCaptions[MENU_CAPTION_COUNT];
    menuCaptions[0] = "Continue Saved Game";
    menuCaptions[1] = "Start New Game";
    menuCaptions[2] = "Select A Theme";
    menuCaptions[3] = "Play a Championship";
    menuCaptions[4] = "Sandbox Mode";
    g_pMenu->SetFontsForStyles(&g_smallKleptoFont, 0, 0, 0);
    g_introBackdrop.load_pcx("interface\\TitleBase.pcx", &g_palette, 0, PALETTE_COLORS, 0);
    g_introBackdrop.BlitSurfaceRegion(g_pMenu, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, g_screenWidth, g_screenHeight);
    for (int button = 0; button < TITLE_ART_COUNT; button++) {
        g_simTitlePageGraphics[button * 2 + (button == TITLE_LOGO)].BlitScaled(g_pMenu, titleArtPos[button * 2],
            titleArtPos[button * 2 + 1], 0);
        if (button < MENU_CAPTION_COUNT) {
            DrawTextCenteredAtY(menuCaptions[button], menuAnchor[button * 2],
                menuAnchor[button * 2 + 1] + MENU_CAPTION_DY, COLOR_LIGHT_GREY);
        }
    }
    g_pMenu->SetFontsForStyles(&g_smallKleptoFont, 0, 0, 0);
    g_configBitMask = 0xffffffff;
    g_level = -1;
    g_courseId = g_rng.NextInt(COURSE_ID_RANGE);
    LoadOrGenerateHighScores();
    if (FileExists("Themes\\Default.pro") != 0) {
        LoadGolferFromTheme("Default.pro", 0, 0);
    }
    g_currentThemeId = 0;
    strcpy(g_activeThemeDir, "Standard");
    LoadInfoScreenAssets();
    ShowLoadingScreen(0);
    g_simScreen.RenderFullSurface(0);
    ResetAllGameState(1);
    memset(g_buttonSlotOfType, -1, sizeof g_buttonSlotOfType);
    for (int paintSlot = 0; paintSlot < PAINT_SLOT_COUNT; paintSlot++) {
        g_buttonSlotOfType[g_keyOrder[paintSlot]] = paintSlot;
    }
    strcpy(g_tickerMessageBuf_1, "Ticker Tape #1");
    strcpy(g_tickerMessageBuf_2, "Ticker Tape #2");
    strcpy(g_simGolferStatusTxt, g_empty);
    WIN32_FIND_DATAA autoSaveFind;
    FindFirstFileA("saved games\\&AutoSave1.sve", &autoSaveFind);
    int gameChosen = 0;
    int teeShotY;
    int teeShotX;
    unsigned saplingPalette;
    int saplingBank;
    int nearestDistance;
    int cursorTileScreenY;
    int cursorTileScreenX;
    int avatarScreenY;
    int avatarScreenX;
    int speechX;
    int animFrame;
    unsigned animBank;
    for (;;) {
        int titleChoice;
        int i;
        int j;
        int k;
        int mouseTileX;
        int scanDir;
        if (gameChosen == 0) {
titleMenu:
            g_pMenu->SetFontsForStyles(&g_smallKleptoFont, 0, 0, 0);
            g_clock = 0;
            g_runModeFlags = GAME_SAVE_CUSTOM_TYPES;
            InitNewGameData();
            InitPlots();
            memset(g_golferSkillGrades, 0, sizeof g_golferSkillGrades);
            memset(g_simMoneyPopupTime, 0, sizeof g_simMoneyPopupTime);
            g_pMenu->SetFontsForStyles(&g_smallKleptoFont, 0, 0, 0);
            g_introBackdrop.load_pcx("interface\\TitleBase.pcx", &g_palette, 0, PALETTE_COLORS, 0);
            g_introBackdrop.BlitSurfaceRegion(g_pMenu, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, g_screenWidth,
                g_screenHeight);
            for (i = 0; i < TITLE_ART_COUNT; i++) {
                g_simTitlePageGraphics[i * 2 + (i == TITLE_LOGO)].BlitScaled(g_pMenu, titleArtPos[i * 2],
                    titleArtPos[i * 2 + 1], 0);
                if (i < MENU_CAPTION_COUNT) {
                    DrawTextCenteredAtY(menuCaptions[i], menuAnchor[i * 2], menuAnchor[i * 2 + 1] + MENU_CAPTION_DY,
                        COLOR_LIGHT_GREY);
                }
                if (i == 1) {
                    strcpy(g_printBuffer, "(");
                    strcat(g_printBuffer, g_activeThemeDir);
                    strcat(g_printBuffer, ")");
                    DrawTextCenteredAtY(g_printBuffer, menuAnchor[2], menuAnchor[3] + MENU_THEME_DY, COLOR_LIGHT_GREY);
                }
            }
            WaitForInputRelease();
            int hasHovered = 0;
            strcpy(g_printBuffer, "What would you like to do?\n");
            do {
                WaitWithTimeout(1, 0);
                do_all_tasks();
                ProcessMouseMessages();
                int cursorY;
                int cursorX;
                g_simScreen.get_mouse_pos(&cursorX, &cursorY);
                int closestDistance = MENU_HOVER_RADIUS;
                if (hasHovered) {
                    titleChoice = -1;
                } else {
                    titleChoice = 1;
                }
                for (int button = 0; button < MENU_BUTTON_COUNT; button++) {
                    int buttonDistance = Math_Distance2D(cursorX - menuAnchor[button * 2],
                        (cursorY - menuAnchor[button * 2 + 1]) * MENU_HOVER_Y_SCALE - MENU_HOVER_Y_OFFSET);
                    if (buttonDistance < closestDistance) {
                        closestDistance = buttonDistance;
                        titleChoice = button;
                        hasHovered = 1;
                    }
                }
                g_introBackdrop.BlitSurfaceRegion(g_pMenu, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, g_screenWidth,
                    g_screenHeight);
                g_simTitlePageGraphics[TITLE_LOGO_LIT_SPRITE].BlitScaled(g_pMenu, titleArtPos[TITLE_LOGO * 2],
                    titleArtPos[TITLE_LOGO * 2 + 1], 0);
                for (button = 0; button < MENU_BUTTON_COUNT; button++) {
                    g_simTitlePageGraphics[button * 2 + (button == TITLE_LOGO ? 1 : titleChoice == button)].BlitScaled(
                        g_pMenu, titleArtPos[button * 2], titleArtPos[button * 2 + 1], 0);
                    if (button < MENU_CAPTION_COUNT) {
                        if (button == titleChoice) {
                            DrawTextCenteredAtY(menuCaptions[button], menuAnchor[button * 2],
                                menuAnchor[button * 2 + 1] + MENU_CAPTION_HOVER_DY, COLOR_DARK_GREY);
                        } else {
                            DrawTextCenteredAtY(menuCaptions[button], menuAnchor[button * 2],
                                menuAnchor[button * 2 + 1] + MENU_CAPTION_DY, COLOR_GREY);
                        }
                    }
                    if (button == 1) {
                        strcpy(g_printBuffer, "(");
                        strcat(g_printBuffer, g_activeThemeDir);
                        strcat(g_printBuffer, ")");
                        DrawTextCenteredAtY(g_printBuffer, menuAnchor[2],
                            (titleChoice != 1 ? MENU_THEME_REST_DY : MENU_THEME_DY) + menuAnchor[3],
                            titleChoice != 1 ? COLOR_GREY : COLOR_DARK_GREY);
                    }
                }
                g_simScreen.RenderFullSurface(0);
            } while ((g_mouseButton == 0) || (titleChoice == -1));
            g_introBackdrop.BlitSurfaceRegion(g_pMenu, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, g_screenWidth,
                g_screenHeight);
            for (j = 0; j < MENU_CAPTION_COUNT; j++) {
                g_simTitlePageGraphics[j * 2].BlitScaled(g_pMenu, titleArtPos[j * 2], titleArtPos[j * 2 + 1], 0);
                DrawTextCenteredAtY(menuCaptions[j], menuAnchor[j * 2], menuAnchor[j * 2 + 1] + MENU_CAPTION_DY,
                    COLOR_LIGHT_GREY);
            }
            if (titleChoice >= 0 && titleChoice != MENU_CHAMPIONSHIP && titleChoice != 1) {
                g_simTitlePageGraphics[TITLE_LOGO_LIT_SPRITE].BlitScaled(g_pMenu, titleArtPos[TITLE_LOGO * 2],
                    titleArtPos[TITLE_LOGO * 2 + 1], 0);
            }
            switch (titleChoice) {
            case -2:
            case 1: titleChoice = 0; break;
            case 0: titleChoice = 1; break;
            case MENU_SELECT_THEME: titleChoice = START_SELECT_THEME; break;
            case MENU_CHAMPIONSHIP: titleChoice = START_CHAMPIONSHIP; break;
            case MENU_SANDBOX: titleChoice = START_SANDBOX; break;
            case MENU_EXIT: titleChoice = START_QUIT;
            }
            WaitForInputRelease();
            if (titleChoice == START_SELECT_THEME) {
                ShowThemeSelect();
                {
                    AssetSurface logo;
                    logo.load_pcx("logo.pcx", &g_palette, 0, PALETTE_COLORS, 0);
                    logo.BlitSurfaceRegion(g_pMenu, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, g_screenWidth,
                        g_screenHeight);
                }
                gameChosen = 0;
                continue;
            }
            if (titleChoice == START_SANDBOX) {
                g_gameCashHundreds = SANDBOX_START_MONEY;
                g_runModeFlags |= GAME_SANDBOX;
                g_unlockedBldgTypeLimit = FACILITIES_ALL;
                g_shownLandmarkBitmask = LANDMARKS_ALL;
                g_level = 1;
            }
            gameChosen = 1;
            for (;;) {
                if (((titleChoice == 0) || (titleChoice == -1)) || (titleChoice == START_CHAMPIONSHIP)) {
                    strcpy(g_printBuffer, "Select difficulty...\n Easy.\n");
                    strcat(g_printBuffer, " Moderate.\n");
                    strcat(g_printBuffer, " Difficult.\n");
                    strcat(g_printBuffer, " Impossible.\n");
                    g_level = ShowDifficultySelectScreen();
                    if (g_level == -1) {
                        gameChosen = 0;
                        break;
                    }
                    g_courseId = g_rng.NextInt(COURSE_ID_RANGE);
                }
                g_introBackdrop.load_pcx("interface//Pictbckgrnd1.pcx", 0, 0, PALETTE_COLORS, 2);
                if (titleChoice == START_CHAMPIONSHIP) {
                    g_currentThemeId = -1;
                    g_runModeFlags |= GAME_CHAMPIONSHIP;
                } else if (titleChoice == 1) {
                    g_level = LEVEL_SAVED_GAME;
                }
                if (g_level != LEVEL_PICK_THEME_AGAIN)
                    break;
                ShowThemeSelect();
                {
                    AssetSurface logo;
                    logo.load_pcx("logo.pcx", &g_palette, 0, PALETTE_COLORS, 0);
                    logo.BlitSurfaceRegion(g_pMenu, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, g_screenWidth,
                        g_screenHeight);
                }
            }
            if (gameChosen == 0)
                continue;
            ShowLoadingScreen(-1);
            InitWorldMapCourseList();
        }
        LoadThemeFiles(g_level != LEVEL_SAVED_GAME);
        FadeOutSound(SOUND_TITLE_MUSIC);
        if (titleChoice == START_QUIT) {
            FadeOutSound(SOUND_COURSE_AMBIENCE);
            WaitWithTimeout(EXIT_FADE_TICKS, 0);
            strcpy(g_printBuffer, "top10.sve");
            g_gameCurrentHandle = _open(g_printBuffer, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IWRITE);
            _write(g_gameCurrentHandle, g_topTenScoresArray, sizeof g_topTenScoresArray);
            _close(g_gameCurrentHandle);
            SaveGolferToTheme(0, titleChoice, 0);
            PlayBinkVideoWithTimeout("./flics/SMSG_closingfinal.bik", 1);
            WaitForInputRelease();
            ConfigureAndPlaySound(SOUND_CREDITS_MUSIC, SOUND_FULL_VOLUME, 0, 0, 0);
            ShowCreditsScreen();
            FadeOutSound(SOUND_CREDITS_MUSIC);
            return;
        }
        g_toolbarView = TOOLBAR_CLOSED;
        g_mouseDownX = -1;
        if ((g_runModeFlags & GAME_CHAMPIONSHIP) != 0) {
            if (StartTournament() == 0) {
                g_runModeFlags &= ~GAME_CHAMPIONSHIP;
                gameChosen = 0;
                continue;
            }
            LoadThemeAssets();
            g_gameCourseRefreshReq = 1;
        } else if (g_level == LEVEL_SAVED_GAME) {
            if (ShowLoadGameScreen(1, 0) == 0) {
                gameChosen = 0;
                continue;
            }
            LoadThemeFiles(1);
            LoadThemeAssets();
            g_gameCourseRefreshReq = 1;
        } else {
            g_currentFeesOption = 0;
            g_gameCashHundreds = (g_runModeFlags & GAME_SANDBOX) != 0 ? SANDBOX_MONEY : START_MONEY;
            g_myWorldCourseIdx = 0;
            g_myWorldCourseIdx = ChooseWorldMapCourse(1);
            if (g_myWorldCourseIdx == -1) {
                gameChosen = 0;
                continue;
            }
            g_currentThemeId = g_mapCourseList[g_myWorldCourseIdx].kind;
            LoadThemeAssets();
            GenerateCourseMap(g_myWorldCourseIdx);
            RebuildTerrainElevation();
            g_unlockedBldgTypeLimit = FACILITIES_START;
            if ((g_runModeFlags & GAME_SANDBOX) != 0) {
                g_gameCashHundreds = SANDBOX_MONEY;
                g_unlockedBldgTypeLimit = FACILITIES_ALL;
                g_shownLandmarkBitmask = LANDMARKS_ALL;
            }
            g_annualStatsRecords[0].v[LEDGER_OTHER] = g_gameCashHundreds;
        }
        for (;;) {
            UpdateBuildingAccessibility();
            for (;;) {
                int frameStartTime = timeGetTime();
                memset(g_buildingLevelPerKind, 0, sizeof g_buildingLevelPerKind);
                memset(g_simOpenFacilityLvl, 0, sizeof g_simOpenFacilityLvl);
                g_reachableAmenityFlags = 0;
                if ((g_runModeFlags & GAME_SANDBOX) != 0) {
                    g_gameCashHundreds = SANDBOX_MONEY;
                }
                for (int buildingSlot = 0; buildingSlot < BUILDING_SLOT_COUNT; buildingSlot++) {
                    short kind = g_buildingTable[buildingSlot].kind;
                    if (kind >= SCENERY_PUTTING_GREEN && kind != SCENERY_HOME_SITE) {
                        g_buildingLevelPerKind[g_buildingTable[buildingSlot].kind] =
                            Clamp(g_buildingLevelPerKind[g_buildingTable[buildingSlot].kind],
                                g_buildingTable[buildingSlot].count + 1, BUILDING_LEVEL_MAX);
                        if ((g_buildingTable[buildingSlot].flags & SLOT_REACHABLE) != 0) {
                            g_simOpenFacilityLvl[g_buildingTable[buildingSlot].kind] =
                                Clamp(g_simOpenFacilityLvl[g_buildingTable[buildingSlot].kind],
                                    g_buildingTable[buildingSlot].count + 1, BUILDING_LEVEL_MAX);
                            g_reachableAmenityFlags |= 1 << g_buildingTable[buildingSlot].kind;
                        }
                    }
                }
                if (g_buildingLevelPerKind[SCENERY_SNACK_BAR] != 0) {
                    g_simOpenFacilityLvl[SCENERY_SNACK_BAR] = 0;
                    g_buildingLevelPerKind[SCENERY_SNACK_BAR] = 0;
                }
                g_buildingLevelPerKind[SCENERY_WILLOW_TREE] = 0;
                if ((g_runModeFlags & GAME_START_MATCH) != 0) {
                    StartProMatch((g_runModeFlags & GAME_PRO_CHALLENGE) != 0);
                }
                int waitingGolfers = 0;
                unsigned firstTeeOpen = g_allHolesArray[1].par != 0;
                int waitingPair = -1;
                int golfersOnFirstHole = 0;
                if (((g_toolbarView == TOOLBAR_BUILDINGS) && (g_elevToolbarShown != 0))
                    && ((g_runModeFlags & GAME_DEMOLISH_TOOL) == 0)) {
                    g_runModeFlags |= GAME_ELEVATION_TOOL;
                    g_selectedBuildingKind = -1;
                    g_paintTerrain = -1;
                } else {
                    g_runModeFlags &= ~GAME_ELEVATION_TOOL;
                }
                int loopCount = GOLFER_COUNT;
                int golferScan = g_simLatestGolferNum + GOLFER_COUNT;
                do {
                    if (g_golferPool[golferScan % GOLFER_COUNT].holeIdx == -1) {
                        waitingGolfers++;
                        if (g_golferPool[golferScan % GOLFER_COUNT ^ 1].holeIdx == -1) {
                            waitingPair = golferScan % GOLFER_COUNT;
                        }
                    }
                    if (g_golferPool[golferScan % GOLFER_COUNT].holeIdx == 1) {
                        golfersOnFirstHole++;
                    }
                    if ((g_golferPool[golferScan % GOLFER_COUNT].holeIdx == 1)
                        && (g_golferPool[golferScan % GOLFER_COUNT].holeStrokes == 0)) {
                        firstTeeOpen = 0;
                        waitingGolfers++;
                    }
                    golferScan--;
                    loopCount--;
                } while (loopCount != 0);
                if (g_golferPool[waitingPair ^ 1].holeIdx != -1) {
                    firstTeeOpen = 0;
                }
                if ((g_runModeFlags & GAME_HOLD_FIRST_TEE) == 0) {
                    golfersOnFirstHole = 0;
                }
                if (((((g_runModeFlags & GAME_TOURNAMENT) == 0) && (golfersOnFirstHole == 0)) && (firstTeeOpen != 0))
                    && ((waitingPair != -1 && (g_golferPool[waitingPair ^ 1].holeIdx <= 0)))) {
                    g_golferPool[waitingPair].holeIdx = 1;
                    g_golferPool[waitingPair ^ 1].holeIdx = 1;
                    PairGolfers(waitingPair, waitingPair ^ 1);
                }
                int golferArrives;
                if (((g_clock & ARRIVAL_TICK_MASK) == 0) || (waitingGolfers < MIN_WAITING_GOLFERS)) {
                    golferArrives = true;
                } else {
                    golferArrives = false;
                }
                if (waitingGolfers >= MAX_WAITING_GOLFERS) {
                    golferArrives = false;
                }
                int newGolfer;
                int golferIndex;
                if ((((g_runModeFlags & GAME_TOURNAMENT) == 0) && (golferArrives))
                    && ((newGolfer = SpawnGolfer(1)) != -1)) {
                    g_simLatestGolferNum = newGolfer;
                    g_golferPool[newGolfer].partner = newGolfer;
                    g_golferPool[newGolfer].waitTimer = -ARRIVAL_WAIT_MIN - g_rng.NextInt(ARRIVAL_WAIT_RANGE);
                    int proChallengeAllowed = g_pendingProIndex == -1;
                    for (golferIndex = 0; golferIndex < GOLFER_COUNT; golferIndex++) {
                        if ((g_golferPool[golferIndex].holeIdx != 0)
                            && ((g_golferPool[golferIndex].persona & PERSONA_KIND) == PERSONA_ROSTER)) {
                            proChallengeAllowed = false;
                        }
                    }
                    if ((g_forceBits & TRIGGER_PRO_CHALLENGE) != 0) {
                        g_golferPool[newGolfer].talents = SKILLS_ALL;
                        g_clubSkillsScore = FORCED_SKILL_RATING;
                        if (g_clock <= TICKS_PER_YEAR) {
                            g_clock = TICKS_PER_YEAR + 1;
                        }
                        proChallengeAllowed = true;
                    }
                    if (proChallengeAllowed
                        && ((g_golferPool[newGolfer].talents == SKILLS_ALL) && (TICKS_PER_YEAR < g_clock))
                        && (g_allHolesArray[g_proMatchStakeLvl + PRO_CHALLENGE_FIRST_HOLE].par != 0)) {
                        loopCount = 0;
                        int ratingSum;
                        int proIndex;
                        do {
                            proIndex = g_rng.NextInt(PRO_ROSTER_SIZE);
                            ratingSum = -PRO_RATING_BIAS;
                            for (int skill = 0; skill < SKILL_COUNT; skill++) {
                                ratingSum += g_golfProList[proIndex].ratings[skill];
                            }
                            loopCount++;
                        } while ((g_golfProList[proIndex].figure == -1)
                                 || (ratingSum
                                     > loopCount / DRAWS_PER_RATING_STEP + g_proMatchStakeLvl * RATING_PER_STAKE_LEVEL)
                                 || (ratingSum < g_proMatchStakeLvl * RATING_PER_STAKE_LEVEL
                                                     - loopCount / DRAWS_PER_RATING_STEP));
                        if ((TICKS_PER_YEAR < g_clock)
                            && ((g_golfProList[proIndex].figure != -1 && (g_pendingProIndex == -1)))) {
                            strcpy(g_printBuffer, "Famous golfer ");
                            strcat(g_printBuffer, g_golfProList[proIndex].name);
                            strcat(g_printBuffer, " challenges you to a match at your course with a wager of \xa7");
                            strcat(g_printBuffer,
                                itoa((g_proMatchStakeLvl * RATING_PER_STAKE_LEVEL + RATING_PER_STAKE_LEVEL)
                                         * WAGER_PER_HOLE_UNIT,
                                    g_numberBuf, 10));
                            strcat(g_printBuffer, " per hole and \xa7");
                            strcat(g_printBuffer,
                                itoa((g_proMatchStakeLvl * RATING_PER_STAKE_LEVEL + RATING_PER_STAKE_LEVEL)
                                         * WAGER_MATCH_UNIT,
                                    g_numberBuf, 10));
                            strcat(g_printBuffer, " for the match! ");
                            int bestRating = 0;
                            k = 0;
                            int ratingsLeft = SKILL_COUNT;
                            unsigned char *pRating = g_golfProList[proIndex].ratings;
                            do {
                                if (*pRating > bestRating) {
                                    k = *pRating;
                                    bestRating = *pRating;
                                }
                                pRating++;
                                ratingsLeft--;
                            } while (ratingsLeft != 0);
                            if (bestRating != 0) {
                                char *proudPhrase = "She is proud of her ";
                                if ((g_golfProList[proIndex].figure & ROSTER_FEMALE) == 0) {
                                    proudPhrase = "He is proud of his ";
                                }
                                strcat(g_printBuffer, proudPhrase);
                                strcat(g_printBuffer, itoa(bestRating, g_numberBuf, 10));
                                strcat(g_printBuffer, "0 percent skill rating in");
                                int firstSkill = true;
                                for (int skill = 0; skill < SKILL_COUNT; skill++) {
                                    if (g_golfProList[proIndex].ratings[skill] == k) {
                                        char *skillSeparator = " ";
                                        if (!firstSkill) {
                                            skillSeparator = " and ";
                                        }
                                        strcat(g_printBuffer, skillSeparator);
                                        strcat(g_printBuffer, g_skillNames[skill]);
                                        firstSkill = false;
                                    }
                                }
                            }
                            strcat(g_printBuffer, ". Click on the match icon on the ");
                            strcat(g_printBuffer, g_golferTypeArray[0].name);
                            strcat(g_printBuffer, " panel when you are ready to start the match.");
                            int messageShown = ShowStoryMessage(COLOR_DARK_GREY, 0, SPEAKER_POPUP_ICON0);
                            if (messageShown != 0) {
                                g_runModeFlags |= GAME_PRO_CHALLENGE;
                                ConfigureAndPlaySound(SOUND_TIME_TUNNEL, SOUND_FULL_VOLUME, 0, 0, 0);
                                g_pendingProIndex = proIndex;
                                g_forceBits &= ~TRIGGER_PRO_CHALLENGE;
                                g_proMatchStakeLvl++;
                            }
                        }
                    }
                }
                char *surnames[SURNAME_COUNT];
                surnames[0] = "Pickett";
                surnames[1] = "Jackson";
                surnames[2] = "Beauregard";
                surnames[3] = "Grant";
                surnames[4] = "Cody";
                surnames[5] = "Meier";
                surnames[6] = "Gibson";
                surnames[7] = "Lindsay";
                surnames[8] = "Chao";
                surnames[9] = "Briggs";
                surnames[10] = "Gordon";
                surnames[11] = "Williams";
                surnames[12] = "Cromer";
                surnames[13] = "Gilmore";
                surnames[14] = "Sandler";
                surnames[15] = "Merrin";
                char *courseAdjectives[ADJECTIVE_COUNT];
                courseAdjectives[0] = "Sleepy";
                courseAdjectives[1] = "Shady";
                courseAdjectives[2] = "Windy";
                courseAdjectives[3] = "Sunny";
                courseAdjectives[4] = "Quiet";
                courseAdjectives[5] = "Rainy";
                courseAdjectives[6] = "Hidden";
                courseAdjectives[7] = "Happy";
                if ((g_tileUphillMap[0][0] & VIEW_TILES_SYNCED) == 0) {
                    memset(g_cellScreenPosX, 0, sizeof g_cellScreenPosX);
                }
                UpdateTerrainView(g_cameraTileX, g_cameraTileY, g_cameraZoom, g_camDirIdx);
                g_editedTileRowIdx = -1;
                if ((g_clock == 0) || (g_gameCourseRefreshReq != 0)) {
                    g_sounds[SOUND_COURSE_AMBIENCE].stop();
                    LoadCourseType(g_currentThemeId);
                    ResetAndRebuildTerrain();
                    UpdateTerrainView(g_cameraTileX, g_cameraTileY, g_cameraZoom, g_camDirIdx);
                    g_gameCourseRefreshReq = 0;
                    ConfigureAndPlaySound(SOUND_COURSE_AMBIENCE, SOUND_FULL_VOLUME, 0, 0, 0);
                }
                if ((((g_cameraTileX != g_paintedCameraTileX) || (g_cameraTileY != g_paintedCameraTileY))
                        || (g_curPreviousCamScale != g_cameraZoom))
                    || (g_previousCamOrientation != g_camDirIdx)) {
                    memset(g_cellScreenPosX, 0, sizeof(g_cellScreenPosX));
                    memset(g_trailSavedPlotPosX, 0xff, sizeof(g_trailSavedPlotPosX));
                }
                g_paintedCameraTileX = g_cameraTileX;
                g_paintedCameraTileY = g_cameraTileY;
                g_curPreviousCamScale = g_cameraZoom;
                g_previousCamOrientation = g_camDirIdx;
                ResetRenderState();
                g_simFrameZoomSnapshot = g_cameraZoom;
                g_tileSpan = g_cameraZoom * TILE_SPAN_PER_ZOOM;
                tileHalfHeight = (g_cameraZoom * TILE_HALF_HEIGHT) / DEFAULT_ZOOM;
                int tileCol;
                for (tileCol = 0; tileCol < GRID_SIZE; tileCol++) {
                    for (i = 0; i < GRID_SIZE; i = i + 1) {
                        int tile = tileCol + i * GRID_SIZE;
                        int tileScreenY;
                        int tileScreenX;
                        int tileOnScreen = TileToScreenCached(i, tileCol, &tileScreenX, &tileScreenY);
                        if (tileOnScreen == 0)
                            continue;
                        int tileLeftX = tileScreenX - g_tileSpan;
                        int tileTopY = tileScreenY - tileHalfHeight;
                        if (((g_runModeFlags & GAME_ELEVATION_TOOL) != 0)
                            && (g_gameElevPanelChoice != GRID_MODE_ANALYZE)) {
                            int gridCornerY;
                            int gridCornerX;
                            int gridOnScreen = 0;
                            if (WorldToScreen(i << TILE_WORLD_SHIFT, tileCol << TILE_WORLD_SHIFT, &gridCornerX,
                                    &gridCornerY, 0)
                                != 0) {
                                int gridEdgeEndY;
                                int gridEdgeEndX;
                                WorldToScreen(i * TILE_WORLD_SIZE + (TILE_WORLD_SIZE - 1), tileCol << TILE_WORLD_SHIFT,
                                    &gridEdgeEndX, &gridEdgeEndY, 0);
                                g_simScreen.line(gridCornerX, gridCornerY, gridEdgeEndX, gridEdgeEndY, COLOR_BLACK);
                                WorldToScreen(i << TILE_WORLD_SHIFT, tileCol * TILE_WORLD_SIZE + (TILE_WORLD_SIZE - 1),
                                    &gridEdgeEndX, &gridEdgeEndY, 0);
                                g_simScreen.line(gridCornerX, gridCornerY, gridEdgeEndX, gridEdgeEndY, COLOR_BLACK);
                                if (((g_runModeFlags & GAME_DEBUG_OVERLAY) != 0) && (g_flatSquareAltMap[tile] != 0)) {
                                    g_printBuffer[0] = '\0';
                                    strcat(g_printBuffer, itoa(g_flatSquareAltMap[tile], g_numberBuf, 10));
                                    DrawTextClampedToScreen(g_printBuffer, gridCornerX + HEIGHT_LABEL_DX,
                                        gridCornerY - HEIGHT_LABEL_DY, COLOR_RED);
                                }
                            }
                        }
                        if (g_gameTileTerrainKind[tile] == TERRAIN_OUT_OF_BOUNDS) {
                            continue;
                        }
                        int tileCategory = g_squareTypeTable[g_gameTileTerrainKind[tile]].category;
                        int capOffsetX = 0, capOffsetY = 0;
                        int cliffPiece;
                        g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
                        if (((g_runModeFlags & GAME_DEBUG_OVERLAY) != 0) && (g_tileUphillMap[0][tile] != 0)) {
                            g_printBuffer[0] = '\0';
                            strcat(g_printBuffer, itoa(g_tileUphillMap[0][tile], g_numberBuf, 10));
                            DrawTextCenteredAtY(g_printBuffer, tileScreenX, tileScreenY - SLOPE_LABEL_DY, COLOR_WHITE);
                        }
                        if (g_gameTileTerrainKind[tile] == TERRAIN_WATER) {
                            unsigned backLeftRise;
                            unsigned backRightRise;
                            unsigned frontRightRise;
                            unsigned frontLeftRise;
                            switch (g_camDirIdx) {
                            case 0:
                                backLeftRise = 1;
                                backRightRise = RISE_EAST;
                                frontRightRise = RISE_SOUTH;
                                frontLeftRise = RISE_WEST;
                                break;
                            case QUARTER_TURN:
                                backLeftRise = RISE_EAST;
                                backRightRise = RISE_SOUTH;
                                frontRightRise = RISE_WEST;
                                frontLeftRise = 1;
                                break;
                            case HALF_TURN:
                                backLeftRise = RISE_SOUTH;
                                backRightRise = RISE_WEST;
                                frontRightRise = 1;
                                frontLeftRise = RISE_EAST;
                                break;
                            case THREE_QUARTER_TURN:
                                backLeftRise = RISE_WEST;
                                backRightRise = 1;
                                frontRightRise = RISE_EAST;
                                frontLeftRise = RISE_SOUTH;
                            }
                            short cliffPieceLeft[CLIFF_PIECES + 1];
                            cliffPieceLeft[0] = 1;
                            cliffPieceLeft[1] = 0x13;
                            cliffPieceLeft[2] = 0x2c;
                            cliffPieceLeft[3] = 0x52;
                            cliffPieceLeft[4] = 0x71;
                            cliffPieceLeft[5] = 0x8e;
                            cliffPieceLeft[6] = 0xa1;
                            cliffPieceLeft[7] = 0xae;
                            short cliffPieceAnchorX[CLIFF_PIECES];
                            cliffPieceAnchorX[0] = 6;
                            cliffPieceAnchorX[1] = 0x20;
                            cliffPieceAnchorX[2] = 0x3f;
                            cliffPieceAnchorX[3] = 0x61;
                            cliffPieceAnchorX[4] = 0x81;
                            cliffPieceAnchorX[5] = 0x9b;
                            cliffPieceAnchorX[6] = 0xa7;
                            int waterHeight;
                            int neighborHeight;
                            int backRightDir;
                            int backLeftDir;
                            if ((backRightRise & g_tileUphillMap[0][tile]) != 0) {
                                cliffPiece = 1;
                                int topCorner = 0;
                                waterHeight = GetCornerElevation(i, tileCol, (g_camDirIdx + 1) & DIRECTION_MASK, 0);
                                backRightDir = (g_camDirIdx + 2) & DIRECTION_MASK;
                                neighborHeight = Clamp(GetCornerElevation(g_dirRowStep[backRightDir] + i,
                                                           g_dirColStep[backRightDir] + tileCol,
                                                           (g_camDirIdx - 1) & DIRECTION_MASK, 0),
                                    0, CLIFF_MAX_HEIGHT);
                                backLeftDir = g_camDirIdx & DIRECTION_MASK;
                                if ((backRightRise
                                        & g_tileUphillMap[g_dirRowStep[backLeftDir] + i]
                                                         [tileCol + g_dirColStep[backLeftDir]])
                                    != 0) {
                                    cliffPiece = CLIFF_WALL_CONTINUED;
                                }
                                if ((backLeftRise & g_tileUphillMap[0][tile]) != 0) {
                                    cliffPiece = CLIFF_INNER_CORNER;
                                }
                                if (neighborHeight > waterHeight) {
                                    DrawSpriteImmediate(tileScreenX, tileScreenY - tileHalfHeight,
                                        tileScreenY - tileHalfHeight,
                                        cliffPieceAnchorX[cliffPiece - 1] - cliffPieceLeft[cliffPiece - 1],
                                        CLIFF_ORIGIN_Y,
                                        &g_courseCliffArt[((g_currentThemeId * CLIFF_HEIGHTS - waterHeight)
                                                              + neighborHeight)
                                                              * CLIFF_PIECES
                                                          + cliffPiece - 1],
                                        SPRITE_FULL_SCALE, -1);
                                }
                                int rightDir = (g_camDirIdx + 3) & DIRECTION_MASK;
                                if (((frontRightRise & g_tileUphillMap[0][tile]) == 0)
                                    && ((backLeftRise
                                            & g_tileUphillMap[g_dirRowStep[rightDir] + i]
                                                             [tileCol + g_dirColStep[rightDir]])
                                        != 0)) {
                                    backRightDir = (g_camDirIdx + 2) & DIRECTION_MASK;
                                    neighborHeight = Clamp(GetCornerElevation(g_dirRowStep[backRightDir] + i,
                                                               g_dirColStep[backRightDir] + tileCol,
                                                               (g_camDirIdx - 3) & DIRECTION_MASK, 0),
                                        0, CLIFF_MAX_HEIGHT);
                                    if (neighborHeight > waterHeight) {
                                        DrawSpriteImmediate(g_tileSpan + tileScreenX, tileScreenY, tileScreenY,
                                            CLIFF_RIGHT_END_ORIGIN_X, CLIFF_ORIGIN_Y,
                                            &g_courseCliffArt[((g_currentThemeId * CLIFF_HEIGHTS - waterHeight)
                                                                  + neighborHeight)
                                                                  * CLIFF_PIECES
                                                              + (CLIFF_RIGHT_END - 1)],
                                            SPRITE_FULL_SCALE, -1);
                                    }
                                }
                            }
                            backRightDir = g_tileUphillMap[0][tile];
                            if (((backLeftRise & backRightDir) != 0) && ((backRightRise & backRightDir) == 0)) {
                                cliffPiece = 0;
                                waterHeight = GetCornerElevation(i, tileCol, (g_camDirIdx + 1) & DIRECTION_MASK, 0);
                                backLeftDir = g_camDirIdx & DIRECTION_MASK;
                                neighborHeight = Clamp(GetCornerElevation(g_dirRowStep[backLeftDir] + i,
                                                           g_dirColStep[backLeftDir] + tileCol,
                                                           (g_camDirIdx - 3) & DIRECTION_MASK, 0),
                                    0, CLIFF_MAX_HEIGHT);
                                int frontLeftDir = (g_camDirIdx - 2) & DIRECTION_MASK;
                                if ((backLeftRise
                                        & g_tileUphillMap[g_dirRowStep[frontLeftDir] + i]
                                                         [tileCol + g_dirColStep[frontLeftDir]])
                                    != 0) {
                                    cliffPiece = CLIFF_BACK_LEFT_WALL;
                                }
                                if ((backRightRise & g_tileUphillMap[0][tile]) != 0) {
                                    neighborHeight = 0;
                                }
                                if ((cliffPiece != 0) && (neighborHeight > waterHeight)) {
                                    DrawSpriteImmediate(tileScreenX - g_tileSpan, tileScreenY, tileScreenY,
                                        cliffPieceAnchorX[cliffPiece - 1] - cliffPieceLeft[cliffPiece - 1],
                                        CLIFF_ORIGIN_Y,
                                        &g_courseCliffArt[((g_currentThemeId * CLIFF_HEIGHTS - waterHeight)
                                                              + neighborHeight)
                                                              * CLIFF_PIECES
                                                          + cliffPiece - 1],
                                        SPRITE_FULL_SCALE, -1);
                                }
                                int continueDir = (2 - g_camDirIdx) & DIRECTION_MASK;
                                if ((backLeftRise
                                        & g_tileUphillMap[g_dirRowStep[continueDir] + i]
                                                         [tileCol + g_dirColStep[continueDir]])
                                    == 0) {
                                    backLeftDir = g_camDirIdx & DIRECTION_MASK;
                                    neighborHeight = Clamp(GetCornerElevation(g_dirRowStep[backLeftDir] + i,
                                                               g_dirColStep[backLeftDir] + tileCol,
                                                               (g_camDirIdx + 3) & DIRECTION_MASK, 0),
                                        0, CLIFF_MAX_HEIGHT);
                                    if (neighborHeight > waterHeight) {
                                        DrawSpriteImmediate(tileScreenX, tileScreenY - tileHalfHeight, tileScreenY,
                                            CLIFF_TOP_END_ORIGIN_X, CLIFF_ORIGIN_Y,
                                            &g_courseCliffArt[((g_currentThemeId * CLIFF_HEIGHTS - waterHeight)
                                                                  + neighborHeight)
                                                                  * CLIFF_PIECES
                                                              + (CLIFF_TOP_END - 1)],
                                            SPRITE_FULL_SCALE, -1);
                                    }
                                }
                            }
                            int leftDir = (g_camDirIdx - 1) & DIRECTION_MASK;
                            if (((g_tileUphillMap[0][tile] & frontLeftRise) != 0)
                                && ((g_tileUphillMap[g_dirRowStep[leftDir] + i][tileCol + g_dirColStep[leftDir]]
                                        & frontRightRise)
                                    != 0)) {
                                int frontLeftDir = (g_camDirIdx - 2) & DIRECTION_MASK;
                                int cornerNeighborHeight = Clamp(GetCornerElevation(g_dirRowStep[frontLeftDir] + i,
                                                                     g_dirColStep[frontLeftDir] + tileCol,
                                                                     (g_camDirIdx + 1) & DIRECTION_MASK, 0),
                                    0, CLIFF_MAX_HEIGHT);
                                if (ELEV_SEA_LEVEL < cornerNeighborHeight) {
                                    DrawSpriteImmediate(tileScreenX - g_tileSpan, tileScreenY, tileScreenY,
                                        CLIFF_LEFT_CORNER_ORIGIN_X, CLIFF_ORIGIN_Y,
                                        &g_courseCliffArt[(g_currentThemeId * CLIFF_HEIGHTS + cornerNeighborHeight
                                                              - ELEV_SEA_LEVEL)
                                                              * CLIFF_PIECES
                                                          + (CLIFF_LEFT_CORNER - 1)],
                                        SPRITE_FULL_SCALE, -1);
                                }
                            }
                            backLeftDir = g_camDirIdx & DIRECTION_MASK;
                            if (((frontRightRise
                                     & g_tileUphillMap[g_dirRowStep[backLeftDir] + i]
                                                      [tileCol + g_dirColStep[backLeftDir]])
                                    != 0)
                                && (g_gameTileTerrainKind[(g_dirRowStep[backLeftDir] + i) * GRID_SIZE
                                                          + (tileCol + g_dirColStep[backLeftDir])]
                                    == TERRAIN_WATER)) {
                                g_curSprite = GetFlicBankSprite(BANK_WATERFALL_SHORT,
                                    (i * WATER_ANIM_ROW_STAGGER + g_clock) % g_flicBankFrameLens[BANK_WATERFALL_SHORT],
                                    FALL_INTO_BACK_LEFT);
                                DrawSpriteImmediate(tileScreenX - g_tileSpan / 2, tileHalfHeight / 3 + tileScreenY,
                                    tileScreenY, SPRITE_ORIGIN_MEDIUM, SPRITE_ORIGIN_MEDIUM, g_curSprite,
                                    SPRITE_FULL_SCALE, PAL_WATER);
                            }
                            backRightDir = (g_camDirIdx + 2) & DIRECTION_MASK;
                            if (((frontLeftRise
                                     & g_tileUphillMap[g_dirRowStep[backRightDir] + i]
                                                      [tileCol + g_dirColStep[backRightDir]])
                                    != 0)
                                && (g_gameTileTerrainKind[(g_dirRowStep[backRightDir] + i) * GRID_SIZE
                                                          + (tileCol + g_dirColStep[backRightDir])]
                                    == TERRAIN_WATER)) {
                                g_curSprite = GetFlicBankSprite(BANK_WATERFALL_SHORT,
                                    (i * WATER_ANIM_ROW_STAGGER + g_clock) % g_flicBankFrameLens[BANK_WATERFALL_SHORT],
                                    FALL_INTO_BACK_RIGHT);
                                DrawSpriteImmediate(g_tileSpan / 2 + tileScreenX, tileHalfHeight / 3 + tileScreenY,
                                    tileScreenY, SPRITE_ORIGIN_MEDIUM, SPRITE_ORIGIN_MEDIUM, g_curSprite,
                                    SPRITE_FULL_SCALE, PAL_WATER);
                            }
                            if ((backRightRise & g_tileUphillMap[0][tile]) != 0) {
                                int fallBaseHeight =
                                    GetCornerElevation(i, tileCol, (g_camDirIdx + 1) & DIRECTION_MASK, 0);
                                Clamp(GetCornerElevation(g_dirRowStep[backRightDir] + i,
                                          tileCol + g_dirColStep[backRightDir], (g_camDirIdx - 1) & DIRECTION_MASK, 0),
                                    0, CLIFF_MAX_HEIGHT);
                                backRightDir = titleChoice = (g_camDirIdx + 2) & DIRECTION_MASK;
                                if (g_gameTileTerrainKind[(g_dirRowStep[backRightDir] + i) * GRID_SIZE + tileCol
                                                          + g_dirColStep[backRightDir]]
                                    == TERRAIN_WATER) {
                                    int upperHeight = GetCornerElevation(g_dirRowStep[backRightDir] + i,
                                        tileCol + g_dirColStep[backRightDir], (g_camDirIdx - 3) & DIRECTION_MASK, 0);
                                    int fallDrop = upperHeight - fallBaseHeight;
                                    int fallPixels;
                                    if (fallDrop <= SHORT_FALL_MAX_DROP) {
                                        fallPixels = ((fallDrop - 1) * g_elevStep * g_cameraZoom) / 8;
                                        g_curSprite = GetFlicBankSprite(BANK_WATERFALL_SHORT,
                                            (g_clock + i * WATER_ANIM_ROW_STAGGER)
                                                % g_flicBankFrameLens[BANK_WATERFALL_SHORT],
                                            FALL_FROM_BACK_RIGHT);
                                        if (0 < fallPixels) {
                                            DrawSpriteImmediate(g_tileSpan / 2 + tileScreenX,
                                                (tileScreenY - tileHalfHeight / 2) - fallPixels, tileScreenY,
                                                SPRITE_ORIGIN_MEDIUM, SPRITE_ORIGIN_MEDIUM, g_curSprite,
                                                SPRITE_FULL_SCALE, PAL_WATER);
                                        }
                                        g_curSprite = GetFlicBankSprite(BANK_SPRAY_SHORT,
                                            (i * WATER_ANIM_ROW_STAGGER + g_clock)
                                                % g_flicBankFrameLens[BANK_SPRAY_SHORT],
                                            FALL_FROM_BACK_RIGHT);
                                        DrawSpriteImmediate(g_tileSpan / 2 + tileScreenX,
                                            tileScreenY - tileHalfHeight / ((fallPixels != 0) + 2), tileScreenY,
                                            SPRITE_ORIGIN_MEDIUM, SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE,
                                            PAL_WATER);
                                    } else {
                                        fallPixels =
                                            ((fallDrop * 2 - TALL_FALL_OFFSET) * g_elevStep * g_cameraZoom) / 8;
                                        g_curSprite = GetFlicBankSprite(BANK_WATERFALL_TALL,
                                            (i * WATER_ANIM_ROW_STAGGER + g_clock)
                                                % g_flicBankFrameLens[BANK_WATERFALL_TALL],
                                            FALL_FROM_BACK_RIGHT);
                                        DrawSpriteImmediate(g_tileSpan / 2 + tileScreenX,
                                            (tileScreenY - tileHalfHeight / 2) - fallPixels, tileScreenY,
                                            SPRITE_ORIGIN_MEDIUM, SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE,
                                            PAL_WATER);
                                        g_curSprite = GetFlicBankSprite(BANK_SPRAY_TALL,
                                            (g_clock + i * WATER_ANIM_ROW_STAGGER)
                                                % g_flicBankFrameLens[BANK_SPRAY_TALL],
                                            FALL_FROM_BACK_RIGHT);
                                        DrawSpriteImmediate(g_tileSpan / 2 + tileScreenX,
                                            tileScreenY - tileHalfHeight / 3, tileScreenY, SPRITE_ORIGIN_MEDIUM,
                                            SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE, PAL_WATER);
                                    }
                                }
                            }
                            if ((backLeftRise & g_tileUphillMap[0][tile]) != 0) {
                                backLeftDir = g_camDirIdx & DIRECTION_MASK;
                                if (g_gameTileTerrainKind[(g_dirRowStep[backLeftDir] + i) * GRID_SIZE
                                                          + g_dirColStep[backLeftDir] + tileCol]
                                    == TERRAIN_WATER) {
                                    int fallBaseHeight =
                                        GetCornerElevation(i, tileCol, (g_camDirIdx + 1) & DIRECTION_MASK, 0);
                                    int upperHeight = GetCornerElevation(g_dirRowStep[backLeftDir] + i,
                                        tileCol + g_dirColStep[backLeftDir], (g_camDirIdx + 3) & DIRECTION_MASK, 0);
                                    int fallDrop = upperHeight - fallBaseHeight;
                                    int fallPixels;
                                    if (fallDrop <= SHORT_FALL_MAX_DROP) {
                                        fallPixels = ((fallDrop - 1) * g_elevStep * g_cameraZoom) / 8;
                                        g_curSprite = GetFlicBankSprite(BANK_WATERFALL_SHORT,
                                            (i * WATER_ANIM_ROW_STAGGER + g_clock)
                                                % g_flicBankFrameLens[BANK_WATERFALL_SHORT],
                                            FALL_FROM_BACK_LEFT);
                                        if (0 < fallPixels) {
                                            DrawSpriteImmediate(tileScreenX - g_tileSpan / 2,
                                                (tileScreenY - tileHalfHeight / 2) - fallPixels, tileScreenY,
                                                SPRITE_ORIGIN_MEDIUM, SPRITE_ORIGIN_MEDIUM, g_curSprite,
                                                SPRITE_FULL_SCALE, PAL_WATER);
                                        }
                                        g_curSprite = GetFlicBankSprite(BANK_SPRAY_SHORT,
                                            (i * WATER_ANIM_ROW_STAGGER + g_clock)
                                                % g_flicBankFrameLens[BANK_SPRAY_SHORT],
                                            FALL_FROM_BACK_LEFT);
                                        DrawSpriteImmediate(tileScreenX - g_tileSpan / 2,
                                            tileScreenY - tileHalfHeight / ((fallPixels != 0) + 2), tileScreenY,
                                            SPRITE_ORIGIN_MEDIUM, SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE,
                                            PAL_WATER);
                                    } else {
                                        fallPixels =
                                            ((fallDrop * 2 - TALL_FALL_OFFSET) * g_elevStep * g_cameraZoom) / 8;
                                        g_curSprite = GetFlicBankSprite(BANK_WATERFALL_TALL,
                                            (g_clock + i * WATER_ANIM_ROW_STAGGER)
                                                % g_flicBankFrameLens[BANK_WATERFALL_TALL],
                                            FALL_FROM_BACK_LEFT);
                                        DrawSpriteImmediate(tileScreenX - g_tileSpan / 2,
                                            (tileScreenY - tileHalfHeight / 2) - fallPixels, tileScreenY,
                                            SPRITE_ORIGIN_MEDIUM, SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE,
                                            PAL_WATER);
                                        g_curSprite = GetFlicBankSprite(BANK_SPRAY_TALL,
                                            (i * WATER_ANIM_ROW_STAGGER + g_clock)
                                                % g_flicBankFrameLens[BANK_SPRAY_TALL],
                                            FALL_FROM_BACK_LEFT);
                                        DrawSpriteImmediate(tileScreenX - g_tileSpan / 2,
                                            tileScreenY - tileHalfHeight / 3, tileScreenY, SPRITE_ORIGIN_MEDIUM,
                                            SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE, PAL_WATER);
                                    }
                                }
                            }
                        }
                        if (((g_gameTileTerrainKind[tile] == TERRAIN_BRUSH) && (g_currentThemeId == THEME_LINKS))
                            && ((g_tileExtra[tile] & TILE_SCENIC) == 0)) {
                            g_curSprite = GetFlicBankSprite(BANK_GROUSE, 0,
                                (g_camDirIdx / QUARTER_TURN + tileCol * -2 + i) & FACING_MASK);
                            AddSpriteToRenderQueue(tileScreenX, tileScreenY, tileScreenY, GROUSE_ORIGIN_X,
                                GROUSE_ORIGIN_Y, g_curSprite,
                                ((tileCol + i) % GROUSE_SIZE_ODDS != 0) ? SPRITE_SMALL_SCALE : SPRITE_FULL_SCALE,
                                PAL_GROUSE);
                        }
                        if (g_gameTileTerrainKind[tile] == TERRAIN_WATER) {
                            if ((g_rng.NextInt(3) != 0) && (g_cameraZoom >= DEFAULT_ZOOM)) {
                                int sparklePhase = g_clock / 2 - tileCol * 9 - i * 11;
                                int sparkleCycle = sparklePhase & SPARKLE_CYCLE_MASK;
                                int sparkleX = (g_clock + tileCol * 0x11) % g_tileSpan + tileLeftX;
                                if ((sparklePhase & SPARKLE_CYCLE_MASK) < SPARKLE_FRAMES) {
                                    switch (sparkleCycle) {
                                    case 0:
                                    case 4: g_simScreen.put_pixel(sparkleX, tileScreenY, COLOR_GREY); break;
                                    case 1:
                                    case 3: g_simScreen.put_pixel(sparkleX, tileScreenY, COLOR_LIGHT_GREY); break;
                                    case 2: g_simScreen.put_pixel(sparkleX, tileScreenY, COLOR_WHITE); break;
                                    }
                                }
                            }
                            if (g_gameTileTerrainKind[tile] == TERRAIN_WATER) {
                                if (((g_gameTileTerrainKind[tile - 1] == TERRAIN_WATER)
                                        && (g_tileVariationIdx[tile - 1] > 0))
                                    && ((g_gameTileTerrainKind[tile + 1] != TERRAIN_WATER
                                         && (((g_gameTileTerrainKind[tile + GRID_SIZE] == TERRAIN_WATER)
                                                 && (g_gameTileTerrainKind[tile - GRID_SIZE] == TERRAIN_WATER))
                                             && ((tileCol + i) % 3 != 0))))) {
                                    int waveBank = (i + tileCol * 2) % WAVE_VARIANTS + BANK_WAVE;
                                    if ((g_currentThemeId == THEME_TROPICAL) || (g_currentThemeId == THEME_LINKS)) {
                                        g_curSprite = GetFlicBankSprite(waveBank,
                                            (i * WATER_ANIM_ROW_STAGGER + g_clock) % g_flicBankFrameLens[waveBank],
                                            (g_camDirIdx / QUARTER_TURN + 1) & FACING_MASK);
                                    } else {
                                        g_curSprite = GetFlicBankSprite(waveBank,
                                            (i * WATER_ANIM_ROW_STAGGER + g_clock) % g_flicBankFrameLens[waveBank],
                                            (g_camDirIdx / QUARTER_TURN - 1) & FACING_MASK);
                                    }
                                    DrawSpriteImmediate(tileScreenX, tileScreenY, tileScreenY, SPRITE_ORIGIN_MEDIUM,
                                        SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE, PAL_WATER);
                                }
                            }
                        }
                        if (((g_gameTileTerrainKind[tile] == TERRAIN_WATER)
                                && (g_gameTileTerrainKind[tile + GRID_SIZE] == TERRAIN_WATER))
                            && ((g_gameTileTerrainKind[tile + 1] == TERRAIN_WATER
                                 && (g_gameTileTerrainKind[tile + (GRID_SIZE + 1)] == TERRAIN_WATER)))) {
                            int nextCol = tileCol + 1;
                            int nextRow = k = i + 1;
                            int seCornerHeight = 0;
                            if (GetCornerElevation(i, tileCol, CORNER_SE, 0)
                                == GetCornerElevation(nextRow, tileCol + 1, CORNER_NW, 0)) {
                                int rockBank = -1;
                                if ((g_tileVariationIdx[tile] <= 1) && (((i - tileCol) * 3 & 7) == 0)) {
                                    rockBank = (tileCol + i) % ROCK_VARIANTS + BANK_ROCK_ABOVE_WATER;
                                }
                                if ((g_tileVariationIdx[tile] <= 1) && ((tileCol * tileCol + i * 3) % 0xd == 0)) {
                                    rockBank = (tileCol + i) % ROCK_VARIANTS + BANK_ROCK_UNDERWATER;
                                }
                                int rockScreenY;
                                int rockScreenX;
                                if (rockBank != -1
                                    && WorldToScreen(nextRow * TILE_WORLD_SIZE, nextCol * TILE_WORLD_SIZE, &rockScreenX,
                                           &rockScreenY, 0)
                                           != 0) {
                                    g_curSprite = GetFlicBankSprite(rockBank, g_clock % g_flicBankFrameLens[rockBank],
                                        ((g_camDirIdx / QUARTER_TURN - tileCol) + 1 + i) & FACING_MASK);
                                    DrawSpriteImmediate(rockScreenX, rockScreenY, rockScreenY, SPRITE_ORIGIN_MEDIUM,
                                        SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE, PAL_WATER);
                                }
                            }
                        }
                        int sameTypeMask = 0;
                        unsigned neighborTypes = 0;
                        int corner = 1;
                        do {
                            int diffCatMask = 0;
                            int neighborsLeft = 3;
                            scanDir = g_camDirIdx + corner - 1;
                            do {
                                sameTypeMask = sameTypeMask >> 1;
                                int neighborDir = scanDir & DIRECTION_MASK;
                                int neighborTerrain = g_gameTileTerrainKind[(g_dirRowStep[neighborDir] + i) * GRID_SIZE
                                                                            + g_dirColStep[neighborDir] + tileCol];
                                neighborTypes = neighborTypes | 1 << neighborTerrain;
                                diffCatMask = diffCatMask >> 1;
                                if (g_squareTypeTable[g_gameTileTerrainKind[(g_dirRowStep[neighborDir] + i) * GRID_SIZE
                                                                            + g_dirColStep[neighborDir] + tileCol]]
                                        .category
                                    != tileCategory) {
                                    diffCatMask = diffCatMask | 4;
                                    scanDir = scanDir + 1;
                                } else {
                                    scanDir = scanDir + 1;
                                }
                                neighborsLeft = neighborsLeft - 1;
                            } while (neighborsLeft != 0);
                            corner = corner + 2;
                        } while (corner < DIRECTION_COUNT);
                        char tileTerrain = g_gameTileTerrainKind[tile];
                        int edgeDir = 0;
                        do {
                            sameTypeMask = sameTypeMask >> 1;
                            if (g_gameTileTerrainKind[(g_dirRowStep[edgeDir] + i) * GRID_SIZE + g_dirColStep[edgeDir]
                                                      + tileCol]
                                == tileTerrain) {
                                sameTypeMask = sameTypeMask | 8;
                            }
                            edgeDir = edgeDir + 2;
                        } while (edgeDir < DIRECTION_COUNT);
                        if (tileTerrain != TERRAIN_GREEN) {
                            g_tileExtra[tile] &= ~TILE_PIN;
                        }
                        short tileFlags = g_tileExtra[tile];
                        if (((((tileFlags & TILE_PATH) != 0)
                                 && (0 < ((TerrainTypeDef *)((char *)g_squareTypeTable
                                                             + (command = tileTerrain * sizeof(TerrainTypeDef))))
                                         ->lieGrade))
                                && (tileTerrain != TERRAIN_BUILDING))
                            && (tileTerrain != TERRAIN_HOME_SITE)) {
                            int pathNeighbors = 0;
                            cliffPiece = 6;
                            int neighborDir;
                            do {
                                neighborDir = (cliffPiece + g_camDirIdx) & DIRECTION_MASK;
                                pathNeighbors = pathNeighbors << 1;
                                if ((g_tileExtra[g_dirColStep[neighborDir] + (g_dirRowStep[neighborDir] + i) * GRID_SIZE
                                                 + tileCol]
                                        & TILE_PATH)
                                    != 0) {
                                    pathNeighbors = pathNeighbors | 1;
                                }
                                cliffPiece = cliffPiece - 2;
                            } while (cliffPiece >= 0);
                            short bridgeBankForLinks[BRIDGE_LINK_MASKS];
                            bridgeBankForLinks[0] = 0;
                            bridgeBankForLinks[1] = BANK_BRIDGE_TILE;
                            bridgeBankForLinks[2] = BANK_BRIDGE_TILE;
                            bridgeBankForLinks[3] = BANK_BRIDGE_L;
                            bridgeBankForLinks[4] = BANK_BRIDGE_TILE;
                            bridgeBankForLinks[5] = BANK_BRIDGE_TILE;
                            bridgeBankForLinks[6] = BANK_BRIDGE_L;
                            bridgeBankForLinks[7] = BANK_BRIDGE_T;
                            bridgeBankForLinks[8] = BANK_BRIDGE_TILE;
                            bridgeBankForLinks[9] = BANK_BRIDGE_L;
                            bridgeBankForLinks[10] = BANK_BRIDGE_TILE;
                            bridgeBankForLinks[11] = BANK_BRIDGE_T;
                            bridgeBankForLinks[12] = BANK_BRIDGE_L;
                            bridgeBankForLinks[13] = BANK_BRIDGE_T;
                            bridgeBankForLinks[14] = BANK_BRIDGE_T;
                            bridgeBankForLinks[15] = BANK_BRIDGE_X;
                            char bridgeFacingForLinks[BRIDGE_LINK_MASKS];
                            bridgeFacingForLinks[0] = 0;
                            bridgeFacingForLinks[1] = 1;
                            bridgeFacingForLinks[2] = 0;
                            bridgeFacingForLinks[3] = 2;
                            bridgeFacingForLinks[4] = 1;
                            bridgeFacingForLinks[5] = 1;
                            bridgeFacingForLinks[6] = 1;
                            bridgeFacingForLinks[7] = 2;
                            bridgeFacingForLinks[8] = 0;
                            bridgeFacingForLinks[9] = 3;
                            bridgeFacingForLinks[10] = 0;
                            bridgeFacingForLinks[11] = 3;
                            bridgeFacingForLinks[12] = 0;
                            bridgeFacingForLinks[13] = 0;
                            bridgeFacingForLinks[14] = 1;
                            bridgeFacingForLinks[15] = 0;
                            if (((TerrainTypeDef *)((char *)g_squareTypeTable + command))->category == CATEGORY_WATER) {
                                int bridgeLinks = 0;
                                int landLinks = 0;
                                int allLinksOnLand = true;
                                int sidesLeft = 4;
                                int sideDir = g_camDirIdx;
                                do {
                                    bridgeLinks = bridgeLinks >> 1;
                                    landLinks = landLinks >> 1;
                                    neighborDir = sideDir & DIRECTION_MASK;
                                    int neighborRow = g_dirRowStep[neighborDir] + i;
                                    int neighborCol = g_dirColStep[neighborDir] + tileCol;
                                    if ((g_tileExtra[neighborCol + neighborRow * GRID_SIZE] & TILE_PATH) == 0) {
                                        if (BRIDGE_SIDE_UNRECOVERED(tileFlags, tile)) {
                                            goto sideCounted;
                                        } else {
                                            goto sideCounted;
                                        }
                                    } else {
                                        bridgeLinks = bridgeLinks | 8;
                                        if (g_squareTypeTable[g_gameTileTerrainKind[neighborCol
                                                                                    + neighborRow * GRID_SIZE]]
                                                .category
                                            == CATEGORY_WATER) {
                                            allLinksOnLand = false;
                                            if (BRIDGE_SIDE_UNRECOVERED(tileFlags, bridgeLinks)) {
                                                goto linkCounted;
                                            } else {
                                                goto linkCounted;
                                            }
                                        } else {
                                            landLinks = landLinks | 8;
                                        }
linkCounted:;
                                    }
sideCounted:;
                                    sideDir = sideDir + 2;
                                    sidesLeft = sidesLeft - 1;
                                } while (sidesLeft != 0);
                                if (allLinksOnLand) {
                                    if ((tileFlags & TILE_SCENIC) != 0) {
                                        int scenicStyle = tileFlags & TILE_STYLE_MASK;
                                        g_curSprite = GetFlicBankSprite(scenicStyle + BANK_SCENIC_BRIDGE, 0,
                                            (pathNeighbors & LINKS_BACK_LEFT_AXIS) != 0);
                                        AddSpriteToRenderQueue(tileScreenX, tileScreenY, tileScreenY - tileHalfHeight,
                                            SPRITE_ORIGIN_SMALL, SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE,
                                            PAL_SCENIC_BRIDGE);
                                    } else {
                                        int pathAxisBits = pathNeighbors & LINKS_BACK_LEFT_AXIS;
                                        g_curSprite = GetFlicBankSprite(BANK_BRIDGE_CAP, 0, pathAxisBits != 0);
                                        if (pathAxisBits != 0) {
                                            capOffsetX = -(g_tileSpan / 2);
                                        } else {
                                            capOffsetX = g_tileSpan / 2;
                                        }
                                        capOffsetY = tileHalfHeight / 2;
                                        int firstCapY = tileScreenY - capOffsetY;
                                        AddSpriteToRenderQueue(capOffsetX + tileScreenX, firstCapY, firstCapY,
                                            SPRITE_ORIGIN_MEDIUM, SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE,
                                            PAL_BRIDGE);
                                        g_curSprite = GetFlicBankSprite(BANK_BRIDGE_CAP, 0, pathAxisBits != 0 ? 3 : 2);
                                        if (pathAxisBits != 0) {
                                            capOffsetX = -(g_tileSpan / 2);
                                        } else {
                                            capOffsetX = g_tileSpan / 2;
                                        }
                                        AddSpriteToRenderQueue(tileScreenX - capOffsetX, capOffsetY + tileScreenY,
                                            tileScreenY - tileHalfHeight, SPRITE_ORIGIN_MEDIUM, SPRITE_ORIGIN_MEDIUM,
                                            g_curSprite, SPRITE_FULL_SCALE, PAL_BRIDGE);
                                    }
                                } else {
                                    short bridgeBank = bridgeBankForLinks[bridgeLinks];
                                    int bridgeKind = bridgeBank;
                                    int endFacing = -1;
                                    if (bridgeKind == BANK_BRIDGE_TILE) {
                                        switch (landLinks) {
                                        case 1: endFacing = 3; break;
                                        case 2: endFacing = 2; break;
                                        case 4: endFacing = 1; break;
                                        case 8: endFacing = 0; break;
                                        }
                                    }
                                    if (endFacing != -1) {
                                        g_curSprite = GetFlicBankSprite(BANK_BRIDGE_RAMP, 0, endFacing);
                                        AddSpriteToRenderQueue(tileScreenX, tileScreenY, tileScreenY - tileHalfHeight,
                                            SPRITE_ORIGIN_MEDIUM, SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE,
                                            PAL_BRIDGE);
                                    } else if (bridgeKind != -1) {
                                        for (int side = 0; side < 4; side++) {
                                            neighborDir = (g_camDirIdx + side * 2) & DIRECTION_MASK;
                                            int neighborRow = g_dirRowStep[neighborDir] + i;
                                            int neighborCol = g_dirColStep[neighborDir] + tileCol;
                                            if (((g_tileExtra[neighborCol + neighborRow * GRID_SIZE] & TILE_PATH) != 0)
                                                && (g_gameTileTerrainKind[neighborCol + neighborRow * GRID_SIZE]
                                                    != TERRAIN_WATER)) {
                                                g_curSprite =
                                                    GetFlicBankSprite(BANK_BRIDGE_CAP, 0, BridgeCapFacing(side));
                                                AddSpriteToRenderQueue(tileScreenX, tileScreenY,
                                                    (g_dirColStep[side * 2] - 1) * tileHalfHeight + tileScreenY,
                                                    SPRITE_ORIGIN_MEDIUM, SPRITE_ORIGIN_MEDIUM, g_curSprite,
                                                    SPRITE_FULL_SCALE, PAL_BRIDGE);
                                            }
                                        }
                                        if (bridgeLinks != 0) {
                                            g_curSprite =
                                                GetFlicBankSprite(bridgeBank, 0, bridgeFacingForLinks[bridgeLinks]);
                                            AddSpriteToRenderQueue(tileScreenX, tileScreenY,
                                                tileScreenY - tileHalfHeight, SPRITE_ORIGIN_MEDIUM,
                                                SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE, PAL_BRIDGE);
                                            if (bridgeBank == BANK_BRIDGE_TILE) {
                                                g_curSprite = GetFlicBankSprite(BANK_BRIDGE_REFLECTION,
                                                    g_clock % g_flicBankFrameLens[BANK_BRIDGE_REFLECTION],
                                                    bridgeFacingForLinks[bridgeLinks]);
                                                DrawSpriteImmediate(tileScreenX, tileScreenY, tileScreenY,
                                                    SPRITE_ORIGIN_MEDIUM, SPRITE_ORIGIN_MEDIUM, g_curSprite,
                                                    SPRITE_FULL_SCALE, PAL_BRIDGE);
                                            }
                                        }
                                    }
                                }
                            }
                            if ((g_tileExtra[tile] & TILE_BENCH) != 0) {
                                int benchDrawn = false;
                                int walkDir = 0;
                                do {
                                    int walkable = IsNeighborWalkable(i, tileCol, walkDir);
                                    if (walkable != 0) {
                                        int benchDir = ((walkDir - g_camDirIdx) + 3) & DIRECTION_MASK;
                                        int benchStyle = g_tileVariationIdx[tile] % BENCH_DESIGNS;
                                        g_curSprite = GetFlicBankSprite(benchStyle + BANK_BENCH, 0,
                                            (-(benchDir / 2) - 2) & FACING_MASK);
                                        AddSpriteToRenderQueue(
                                            tileScreenX + g_dirRowStep[benchDir] * g_cameraZoom * -BENCH_DX_PER_ZOOM,
                                            tileScreenY
                                                - (g_dirColStep[benchDir] * g_cameraZoom * BENCH_DY_HALVES_PER_ZOOM)
                                                      / 2,
                                            tileScreenY
                                                - (g_dirColStep[benchDir] * g_cameraZoom * BENCH_DEPTH_HALVES_PER_ZOOM)
                                                      / 2,
                                            SPRITE_ORIGIN_SMALL, SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE,
                                            benchStyle + PAL_BENCH);
                                        benchDrawn = true;
                                    }
                                    walkDir = walkDir + 2;
                                } while (walkDir < DIRECTION_COUNT);
                                if (!benchDrawn) {
                                    g_tileExtra[tile] &= ~TILE_BENCH;
                                }
                            }
                        }
                        tileFlags = g_tileExtra[tile];
                        int tileHole = tileFlags & TILE_HOLE_MASK;
                        if ((tileFlags & TILE_PIN) != 0) {
                            if (((g_toolbarView == TOOLBAR_SHOT) && (g_watchedPlayerIdx != -1))
                                && ((g_golferPool[g_watchedPlayerIdx].holeIdx == tileHole
                                     && (((g_clock & PIN_BLINK_TICK_BIT) != 0
                                          || ((g_runModeFlags & GAME_PAUSED) != 0)))))) {
                                g_selectHaloSprite.BlitWithScaleOffset(g_pMenu, g_tileSpan - PIN_RING_DX + tileLeftX,
                                    tileTopY + tileHalfHeight - PIN_RING_DY, g_shadeTable, 0);
                            }
                            tileFlags = g_tileExtra[tile];
                            int flagColor = 0;
                            int flagBank = g_currentThemeId + BANK_FLAG_OPEN;
                            int flagFrame;
                            if ((tileFlags & (TILE_ANIMATING | TILE_WEEDS)) == TILE_ANIMATING) {
                                flagFrame = g_tileAnimFrame[tile];
                                flagBank = g_currentThemeId + BANK_FLAG_POP;
                                if (flagFrame >= g_flicBankFrameLens[flagBank]) {
                                    tileFlags &= TILE_CLEAR_ANIMATING;
                                    flagFrame = g_flicBankFrameLens[flagBank] - 1;
                                    g_tileExtra[tile] = tileFlags;
                                }
                            } else {
                                int difficulty =
                                    (g_allHolesArray[tileHole].moodSum * 100)
                                    / (g_allHolesArray[tileHole].shots / 2 + 4 + g_allHolesArray[tileHole].rounds);
                                flagFrame = g_clock % g_flicBankFrameLens[flagBank];
                                flagColor = Clamp(difficulty / FLAG_DIFFICULTY_STEP, 0, 3);
                                if (g_allHolesArray[tileHole].par == 0) {
                                    flagColor = 1;
                                }
                            }
                            g_curSprite = GetFlicBankSprite(flagBank, flagFrame, flagColor);
                            AddSpriteToRenderQueue(g_tileSpan + tileLeftX, tileTopY + tileHalfHeight,
                                tileTopY + tileHalfHeight, SPRITE_ORIGIN_SMALL, SPRITE_ORIGIN_SMALL, g_curSprite,
                                SPRITE_FULL_SCALE, PAL_FLAG);
                        }
                        if ((g_gameTileTerrainKind[tile] == 0) && (tileHole != 0)) {
                            if (g_allHolesArray[tileHole].par != 0) {
                                int teeDir = g_allHolesArray[tileHole].teeDir;
                                int oldVariation = g_tileVariationIdx[tile];
                                g_tileVariationIdx[tile] =
                                    ((g_camDirIdx - teeDir - 2) & DIRECTION_MASK) * TEE_VARIANTS + 1;
                                if ((g_allHolesArray[tileHole].flags & HOLE_DOGLEG_LEFT) != 0) {
                                    g_tileVariationIdx[tile]++;
                                }
                                if ((g_allHolesArray[tileHole].flags & HOLE_DOGLEG_RIGHT) != 0) {
                                    g_tileVariationIdx[tile] += 2;
                                }
                                if (g_tileVariationIdx[tile] != oldVariation) {
                                    g_tileUphillMap[0][0] &= ~VIEW_TILES_SYNCED;
                                }
                                int markerDir = ((teeDir - g_camDirIdx) + 1) & DIRECTION_MASK;
                                int markerBank = Clamp(g_allHolesArray[tileHole].par - MIN_PAR, 0, 2) + BANK_TEE_MARKER;
                                int markerFrames = g_flicBankFrameLens[markerBank];
                                int markerFrame = firstTeeOpen = markerFrames - 1;
                                tileFlags = g_tileExtra[tile];
                                if ((tileFlags & (TILE_ANIMATING | TILE_WEEDS)) == TILE_ANIMATING) {
                                    markerFrame = g_tileAnimFrame[tile];
                                    if (markerFrame >= g_flicBankFrameLens[markerBank]) {
                                        markerFrame = firstTeeOpen;
                                        g_tileExtra[tile] = tileFlags & TILE_CLEAR_ANIMATING;
                                    }
                                }
                                g_curSprite = GetFlicBankSprite(markerBank, markerFrame, 0);
                                int *pColStep;
                                if ((markerDir & 1) != 0) {
                                    pColStep = &g_dirColStep[markerDir];
                                    int *pRowStep = &g_dirRowStep[markerDir];
                                    DrawSpriteImmediate(tileScreenX
                                                            + g_simFrameZoomSnapshot * *pRowStep * TEE_MARKER_DIAG_DX,
                                        tileScreenY + *pColStep * g_simFrameZoomSnapshot * TEE_MARKER_DIAG_DY,
                                        tileScreenY, SPRITE_ORIGIN_SMALL, SPRITE_ORIGIN_SMALL, g_curSprite,
                                        SPRITE_FULL_SCALE, PAL_TEE_MARKER);
                                    DrawSpriteImmediate(tileScreenX
                                                            - g_simFrameZoomSnapshot * *pRowStep * TEE_MARKER_DIAG_DX,
                                        tileScreenY - *pColStep * g_simFrameZoomSnapshot * TEE_MARKER_DIAG_DY,
                                        tileScreenY, SPRITE_ORIGIN_SMALL, SPRITE_ORIGIN_SMALL, g_curSprite,
                                        SPRITE_FULL_SCALE, PAL_TEE_MARKER);
                                } else {
                                    pColStep = &g_dirColStep[markerDir];
                                    int *pRowStep = &g_dirRowStep[markerDir];
                                    DrawSpriteImmediate(tileScreenX
                                                            + g_simFrameZoomSnapshot * *pRowStep * TEE_MARKER_DX,
                                        tileScreenY + *pColStep * g_simFrameZoomSnapshot * TEE_MARKER_DY, tileScreenY,
                                        SPRITE_ORIGIN_SMALL, SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE,
                                        PAL_TEE_MARKER);
                                    DrawSpriteImmediate(tileScreenX
                                                            - g_simFrameZoomSnapshot * *pRowStep * TEE_MARKER_DX,
                                        tileScreenY - *pColStep * g_simFrameZoomSnapshot * TEE_MARKER_DY, tileScreenY,
                                        SPRITE_ORIGIN_SMALL, SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE,
                                        PAL_TEE_MARKER);
                                }
                                int holeFlags = g_allHolesArray[tileHole].flags;
                                int plaquePiece = tileHole - 1;
                                if ((holeFlags & HOLE_TOP_100) != 0) {
                                    plaquePiece = tileHole + (MAX_HOLES - 1);
                                }
                                if ((holeFlags & HOLE_TOP_18) != 0) {
                                    plaquePiece = tileHole + (2 * MAX_HOLES - 1);
                                }
                                if ((holeFlags & HOLE_AWARDS) != 0) {
                                    if (((g_allHolesArray[tileHole].teeDir - g_camDirIdx) & DIRECTION_OPPOSITE) != 0) {
                                        AddSpriteToRenderQueue(tileScreenX,
                                            tileScreenY - g_simFrameZoomSnapshot * PLAQUE_DY_PER_ZOOM,
                                            tileScreenY - g_simFrameZoomSnapshot * PLAQUE_DY_PER_ZOOM, PLAQUE_ORIGIN_X,
                                            (plaquePiece / MAX_HOLES) * PLAQUE_ROW_STEP + PLAQUE_ORIGIN_Y,
                                            &g_simHoleMarkArt[plaquePiece], SPRITE_FULL_SCALE, -1);
                                    } else {
                                        AddSpriteToRenderQueue(tileScreenX,
                                            tileScreenY + g_simFrameZoomSnapshot * PLAQUE_DY_PER_ZOOM,
                                            tileScreenY + g_simFrameZoomSnapshot * PLAQUE_DY_PER_ZOOM, PLAQUE_ORIGIN_X,
                                            (plaquePiece / MAX_HOLES) * PLAQUE_ROW_STEP + PLAQUE_ORIGIN_Y,
                                            &g_simHoleMarkArt[plaquePiece], SPRITE_FULL_SCALE, -1);
                                    }
                                }
                            } else {
                                g_tileVariationIdx[tile] = 0;
                            }
                        }
                    }
                }
                int passRow;
                tileCol = 0;
                do {
                    passRow = 0;
                    do {
                        if (IsPositionInvalid(passRow, tileCol) == 0) {
                            int tileAnimFrame = -1;
                            int tile = tileCol + passRow * GRID_SIZE;
                            if ((g_tileExtra[tile] & TILE_ANIMATING) != 0) {
                                tileAnimFrame = g_tileAnimFrame[tile];
                            }
                            int screenY;
                            int screenX;
                            if (TileToScreenCached(passRow, tileCol, &screenX, &screenY) == 0) {
                                passRow = passRow + 1;
                                continue;
                            }
                            if ((g_runModeFlags & GAME_DEBUG_OVERLAY) != 0) {
                                int barWidth = g_tileSpan / DEBUG_BAR_DIVISOR;
                                g_simScreen.fill(barWidth + screenX, screenY - g_ballStopCountMap[tile], barWidth,
                                    g_ballStopCountMap[tile], COLOR_TEAL);
                                barWidth = g_tileSpan / DEBUG_BAR_DIVISOR;
                                g_simScreen.fill(screenX - barWidth, screenY - g_posReactionTileCount[tile] * 2,
                                    barWidth, g_posReactionTileCount[tile] * 2, COLOR_BRIGHT_GREEN);
                                g_simScreen.fill(screenX, screenY - g_gameDispleasedTally[tile] * 2,
                                    g_tileSpan / DEBUG_BAR_DIVISOR, g_gameDispleasedTally[tile] * 2, COLOR_RED);
                            }
                            g_printBuffer[0] = '\0';
                            if ((g_tileExtra[tileCol + passRow * GRID_SIZE] & TILE_NAMED) != 0) {
                                Font *labelFont;
                                if (g_cameraZoom >= DEFAULT_ZOOM) {
                                    labelFont = &g_smallManualFont;
                                } else {
                                    labelFont = &g_boldArialFont;
                                }
                                AssetSurface *menu = g_pMenu;
                                menu->SetFontsForStyles(labelFont, 0, 0, 0);
                                switch (g_gameTileTerrainKind[tile]) {
                                case TERRAIN_WATER:
                                    strcpy(g_printBuffer, "Lake ");
                                    strcat(g_printBuffer, surnames[(tileCol + passRow) % SURNAME_COUNT]);
                                    break;
                                case TERRAIN_WETLANDS:
                                    strcpy(g_printBuffer, surnames[(tileCol + passRow) % SURNAME_COUNT]);
                                    strcat(g_printBuffer, "'s Marsh");
                                    break;
                                case TERRAIN_ROCKS:
                                    strcpy(g_printBuffer, courseAdjectives[(tileCol + passRow) % ADJECTIVE_COUNT]);
                                    strcat(g_printBuffer, " Hollow");
                                    break;
                                case TERRAIN_TREE:
                                case TERRAIN_PINE_TREE:
                                case TERRAIN_PALM_TREE:
                                    strcpy(g_printBuffer, courseAdjectives[(tileCol + passRow) % ADJECTIVE_COUNT]);
                                    strcat(g_printBuffer, " Grove");
                                    break;
                                }
                                if (g_printBuffer[0] != '\0'
                                    && (((g_clock >> NAME_LABEL_PERIOD_SHIFT) + tileCol * 3 + passRow)
                                           & NAME_LABEL_PHASE_MASK)
                                           == 0
                                    && (g_configBitMask & PREF_SHOW_NAMES) == 0 && TEXT_MIN_ZOOM < g_cameraZoom
                                    && g_toolbarView != TOOLBAR_SHOT) {
                                    DrawTextCenteredWithShadow(g_printBuffer, screenX, screenY - 3, COLOR_WHITE);
                                }
                            }
                            short tileFlags = g_tileExtra[tile];
                            if ((g_tileExtra[tile] & TILE_FLOWERS) != 0) {
                                int bedTile = tile;
                                int variation = g_tileVariationIdx[bedTile];
                                int bedNeighbors = 0;
                                int flowerPalette = variation % FLOWER_COLORS + PAL_FLOWER_BED;
                                if ((tileFlags & TILE_WEEDS) != 0) {
                                    flowerPalette = PAL_FLOWERS_WILTED;
                                }
                                int dir = 0;
                                do {
                                    bedNeighbors = bedNeighbors >> 1;
                                    int neighborTile = 0;
                                    if ((g_tileExtra[g_dirColStep[dir] + (g_dirRowStep[dir] + passRow) * GRID_SIZE
                                                     + tileCol]
                                            & TILE_FLOWERS)
                                            == TILE_FLOWERS
                                        && variation / FLOWER_COLORS
                                               == g_tileVariationIdx[(passRow + g_dirRowStep[dir]) * GRID_SIZE + tileCol
                                                                     + g_dirColStep[dir]]
                                                      / FLOWER_COLORS
                                        && g_flatSquareAltMap[tile]
                                               == g_flatSquareAltMap[(passRow + g_dirRowStep[dir]) * GRID_SIZE + tileCol
                                                                     + g_dirColStep[dir]]) {
                                        bedNeighbors = bedNeighbors | BED_SIDE_NEWEST;
                                    }
                                    dir = dir + 2;
                                } while (dir < DIRECTION_COUNT);
                                int quarterTurns = g_camDirIdx / QUARTER_TURN;
                                if (0 < quarterTurns) {
                                    do {
                                        bedNeighbors = (bedNeighbors >> 1) | ((bedNeighbors & 1) << BED_SIDE_TOP_SHIFT);
                                        quarterTurns = quarterTurns - 1;
                                    } while (quarterTurns != 0);
                                }
                                int flowerBank = g_flowerBedSprites[bedNeighbors].bank;
                                int flowerFacing = g_flowerBedSprites[bedNeighbors].facing;
                                if (flowerBank == BANK_FLOWERS_ONE_SIDE) {
                                    flowerFacing = flowerFacing ^ 2;
                                }
                                flowerFacing = BorderFacing(flowerBank, flowerFacing, passRow - tileCol * 2);
                                if (variation >= FLOWER_WALLED_VARIATION) {
                                    flowerBank = flowerBank + FLOWER_WALLED_BANKS;
                                } else if (variation >= FLOWER_COLORS) {
                                    flowerBank = flowerBank + FLOWER_SIN_BANKS;
                                }
                                int lastFrame = g_flicBankFrameLens[flowerBank] - 1;
                                int flowerFrame = lastFrame;
                                if (tileAnimFrame != -1 && (tileFlags & TILE_WEEDS) == 0
                                    && (flowerFrame = tileAnimFrame,
                                        g_tileAnimFrame[tile] >= g_flicBankFrameLens[flowerBank] - 1)) {
                                    g_tileExtra[tile] = tileFlags & TILE_CLEAR_ANIMATING;
                                    g_tileAnimFrame[tile] = TILE_ANIM_DONE;
                                }
                                g_curSprite = GetFlicBankSprite(flowerBank, flowerFrame, flowerFacing);
                                DrawSpriteImmediate(screenX, screenY, screenY, SPRITE_ORIGIN_FLOWER_BED,
                                    SPRITE_ORIGIN_FLOWER_BED, g_curSprite, SPRITE_FULL_SCALE, flowerPalette);
                                int bedRow = passRow;
                                int bedCol = tileCol;
                                if ((bedCol + bedRow * 2) % ORNAMENT_SPACING == 0
                                    && (bedNeighbors & BED_BACK_SIDES) == BED_BACK_SIDES
                                    && (g_tileExtra[tile - (GRID_SIZE + 1)] & TILE_FLOWERS) == TILE_FLOWERS) {
                                    if (GetTileElevation(bedRow, bedCol, 0)
                                        == GetTileElevation(bedRow - 1, bedCol - 1, 0)) {
                                        int isTopiary = bedRow & 1;
                                        int ornamentBank = isTopiary + BANK_GAZEBO;
                                        int ornamentPalette = isTopiary != 0 ? PAL_TOPIARY : PAL_GAZEBO;
                                        if (tileAnimFrame != -1 && tileAnimFrame < g_flicBankFrameLens[ornamentBank]
                                            && (g_tileExtra[tile] & TILE_WEEDS) == 0) {
                                            g_curSprite = GetFlicBankSprite(ornamentBank,
                                                tileAnimFrame % g_flicBankFrameLens[ornamentBank],
                                                (g_camDirIdx / QUARTER_TURN + bedCol) & FACING_MASK);
                                        } else {
                                            g_curSprite =
                                                GetFlicBankSprite(ornamentBank, g_flicBankFrameLens[ornamentBank] - 1,
                                                    (g_camDirIdx / QUARTER_TURN + bedCol) & FACING_MASK);
                                        }
                                        switch (g_camDirIdx) {
                                        case 0:
                                            AddSpriteToRenderQueue(screenX - g_tileSpan, screenY, screenY,
                                                ORNAMENT_ORIGIN_X, ORNAMENT_ORIGIN_Y, g_curSprite, SPRITE_FULL_SCALE,
                                                ornamentPalette);
                                            break;
                                        case QUARTER_TURN:
                                            AddSpriteToRenderQueue(screenX, ORNAMENT_Y_UNRECOVERED(screenY), screenY,
                                                ORNAMENT_ORIGIN_X, ORNAMENT_ORIGIN_Y, g_curSprite, SPRITE_FULL_SCALE,
                                                ornamentPalette);
                                            break;
                                        case HALF_TURN:
                                            AddSpriteToRenderQueue(g_tileSpan + screenX, screenY, screenY,
                                                ORNAMENT_ORIGIN_X, ORNAMENT_ORIGIN_Y, g_curSprite, SPRITE_FULL_SCALE,
                                                ornamentPalette);
                                            break;
                                        case THREE_QUARTER_TURN:
                                            AddSpriteToRenderQueue(screenX, screenY - tileHalfHeight, screenY,
                                                ORNAMENT_ORIGIN_X, ORNAMENT_ORIGIN_Y, g_curSprite, SPRITE_FULL_SCALE,
                                                ornamentPalette);
                                            break;
                                        }
                                        g_tileExtra[tile] &= TILE_CLEAR_SCENIC;
                                    } else {
                                        g_tileExtra[tile] &= TILE_CLEAR_SCENIC;
                                    }
                                } else {
                                    g_tileExtra[tile] &= TILE_CLEAR_SCENIC;
                                }
                            }
                            if ((g_tileExtra[tile] & TILE_SCENIC) != 0) {
                                g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
                                if ((g_configBitMask & PREF_SHOW_NAMES) == 0 && TEXT_MIN_ZOOM < g_cameraZoom
                                    && g_toolbarView != TOOLBAR_SHOT) {
                                    g_printBuffer[0] = '\0';
                                    int hasName = GetMemberNameAtPosition(passRow, tileCol, 1);
                                    if (hasName != 0) {
                                        DrawTextCenteredWithShadow(g_printBuffer, screenX, screenY + 5, COLOR_WHITE);
                                    }
                                }
                                int sceneryFacing = (g_camDirIdx / QUARTER_TURN - tileCol * 2 + passRow) & FACING_MASK;
                                char tileTerrain = g_gameTileTerrainKind[tile];
                                switch (g_squareTypeTable[tileTerrain].category) {
                                case 0x15:
                                case 0x16: break;
                                case CATEGORY_OUT_OF_BOUNDS:
                                case CATEGORY_LIKE_OUT_OF_BOUNDS:
                                    if (g_currentThemeId != THEME_DESERT) {
                                        g_curSprite = GetFlicBankSprite(BANK_DEAD_SWAMP_TREES, 0, sceneryFacing);
                                        AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_SMALL,
                                            SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE, PAL_DEAD_SWAMP_TREES);
                                    }
                                    break;
                                case CATEGORY_WATER:
                                    if ((g_tileExtra[tile] & TILE_PATH) == 0 && tileTerrain != TERRAIN_WETLANDS) {
                                        if (g_tileVariationIdx[tile] == 0 || tileTerrain != TERRAIN_WATER) {
                                            if (g_currentThemeId == THEME_DESERT) {
                                                g_curSprite =
                                                    GetFlicBankSprite(BANK_DESERT_WATER_ROCK, 0, sceneryFacing);
                                                AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_SMALL,
                                                    SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE,
                                                    PAL_DESERT_WATER_ROCK);
                                            } else {
                                                g_curSprite = GetFlicBankSprite(BANK_WATERSPRAY,
                                                    g_clock % g_flicBankFrameLens[BANK_WATERSPRAY], 0);
                                                AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_SMALL,
                                                    SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE,
                                                    PAL_WATERSPRAY);
                                            }
                                        } else {
                                            int dolphinTime = passRow * 3 + g_clock + tileCol * 7;
                                            int dolphinFrame = dolphinTime % DOLPHIN_CYCLE;
                                            if (dolphinFrame < g_flicBankFrameLens[BANK_DOLPHIN]) {
                                                g_curSprite = GetFlicBankSprite(BANK_DOLPHIN,
                                                    Clamp(dolphinFrame, 0, g_flicBankFrameLens[BANK_DOLPHIN] - 1),
                                                    ((dolphinTime >> DOLPHIN_CYCLE_SHIFT) + tileCol + passRow)
                                                        & FACING_MASK);
                                                AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_MEDIUM,
                                                    SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE, PAL_DOLPHIN);
                                            }
                                        }
                                    }
                                    break;
                                case CATEGORY_TREES:
                                    g_curSprite = GetFlicBankSprite(BANK_SCENIC_FLOWERS, 0, sceneryFacing);
                                    DrawSpriteImmediate(screenX, screenY, screenY, SPRITE_ORIGIN_SMALL,
                                        SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE, PAL_SCENIC_FLOWERS);
                                    if (g_gameTileTerrainKind[tile] == TERRAIN_ELM_TREE) {
                                        g_tileExtra[tile] &= TILE_CLEAR_WEEDS;
                                        int treeKind = g_tileVariationIdx[tile] % SPECIMEN_TREE_KINDS;
                                        int treeBank = treeKind + BANK_SPECIMEN_TREE_BASE;
                                        int treePalette = treeKind + PAL_SPECIMEN_TREE_BASE;
                                        if (treeKind == 0) {
                                            treeBank = BANK_WILLOW_TREE;
                                            int terrainHeight = CalculateTerrainHeight(
                                                passRow << TREE_TINT_SAMPLE_SHIFT, tileCol << TREE_TINT_SAMPLE_SHIFT);
                                            treePalette = ((terrainHeight / TINT_BAND_HEIGHT) & TINT_MASK) + PAL_WILLOW;
                                        }
                                        if (tileAnimFrame != -1) {
                                            g_curSprite = GetFlicBankSprite(treeBank, tileAnimFrame,
                                                (passRow - tileCol * 2) & FACING_MASK);
                                            if (tileAnimFrame == g_flicBankFrameLens[treeBank] - 1) {
                                                g_tileExtra[tile] &= TILE_CLEAR_ANIMATING;
                                            }
                                            if (tileAnimFrame == 0) {
                                                ConfigureAndPlaySound(SOUND_MAPLE, SOUND_FULL_VOLUME, 0, 0, 0);
                                            }
                                            tileAnimFrame = ANIM_FRAME_USED;
                                        } else {
                                            g_curSprite = GetFlicBankSprite(treeBank, g_flicBankFrameLens[treeBank] - 1,
                                                (passRow - tileCol * 2) & FACING_MASK);
                                        }
                                        if (treeBank == BANK_WILLOW_TREE) {
                                            AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_BUILDING,
                                                SPRITE_ORIGIN_BUILDING, g_curSprite, SPRITE_FULL_SCALE, treePalette);
                                        } else {
                                            AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_SCENIC,
                                                SPRITE_ORIGIN_SCENIC, g_curSprite, SPRITE_FULL_SCALE, treePalette);
                                        }
                                    } else if (g_currentThemeId == THEME_DESERT) {
                                        g_curSprite = GetFlicBankSprite(BANK_CACTUS_GROUP,
                                            g_flicBankFrameLens[BANK_CACTUS_GROUP] - 1, sceneryFacing);
                                        AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_MEDIUM,
                                            SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE, PAL_CACTUS_GROUP);
                                    } else {
                                        if (g_gameTileTerrainKind[tile] == TERRAIN_TREE) {
                                            g_curSprite = GetFlicBankSprite(BANK_SCENIC_TREE,
                                                g_flicBankFrameLens[BANK_SCENIC_TREE] - 1, sceneryFacing);
                                            AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_SCENIC,
                                                SPRITE_ORIGIN_SCENIC, g_curSprite, SPRITE_FULL_SCALE, PAL_SCENIC_TREE);
                                        }
                                        if (g_gameTileTerrainKind[tile] == TERRAIN_PALM_TREE) {
                                            g_curSprite = GetFlicBankSprite(BANK_EASTER_HEAD, 0, sceneryFacing);
                                            AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_BUILDING,
                                                SPRITE_ORIGIN_BUILDING, g_curSprite, SPRITE_FULL_SCALE, PAL_LIGHTHOUSE);
                                        }
                                        if (g_gameTileTerrainKind[tile] == TERRAIN_PINE_TREE) {
                                            g_curSprite = GetFlicBankSprite(BANK_PLUM,
                                                g_flicBankFrameLens[BANK_PLUM] - 1, sceneryFacing);
                                            AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_SCENIC,
                                                SPRITE_ORIGIN_SCENIC, g_curSprite, SPRITE_FULL_SCALE, PAL_PLUM);
                                        }
                                    }
                                    break;
                                case CATEGORY_ROUGH:
                                    if (tileTerrain != TERRAIN_ROUGH) {
                                        if (tileTerrain == TERRAIN_RAVINE) {
                                            if (g_currentThemeId == THEME_DESERT) {
                                                g_curSprite = GetFlicBankSprite(BANK_ROCK_FORM, 0, sceneryFacing);
                                                AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_MEDIUM,
                                                    SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE,
                                                    PAL_ROCK_FORM);
                                            } else {
                                                g_curSprite = GetFlicBankSprite(BANK_SCENIC_FLOWERS, 0, sceneryFacing);
                                                DrawSpriteImmediate(screenX, screenY, screenY, SPRITE_ORIGIN_SMALL,
                                                    SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE,
                                                    PAL_SCENIC_FLOWERS);
                                                g_curSprite = GetFlicBankSprite(BANK_SCENIC_TREE, 0, sceneryFacing);
                                                AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_SCENIC,
                                                    SPRITE_ORIGIN_SCENIC, g_curSprite, SPRITE_FULL_SCALE,
                                                    PAL_SCENIC_TREE);
                                            }
                                            break;
                                        }
                                        if (tileTerrain == TERRAIN_ROCKS) {
                                            if (g_currentThemeId == THEME_DESERT) {
                                                g_curSprite = GetFlicBankSprite(BANK_NATURAL_BRIDGE, 0, sceneryFacing);
                                                AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_SMALL,
                                                    SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE,
                                                    PAL_NATURAL_BRIDGE);
                                            } else {
                                                if (g_currentThemeId == THEME_LINKS) {
                                                    g_curSprite = GetFlicBankSprite(BANK_LINKS_ROCKS, 0, sceneryFacing);
                                                    AddSpriteToRenderQueue(screenX, screenY, screenY,
                                                        SPRITE_ORIGIN_SMALL, SPRITE_ORIGIN_SMALL, g_curSprite,
                                                        SPRITE_FULL_SCALE, PAL_LINKS_ROCKS);
                                                } else {
                                                    g_curSprite =
                                                        GetFlicBankSprite(BANK_PARKLAND_ROCK, 0, sceneryFacing);
                                                    AddSpriteToRenderQueue(screenX, screenY, screenY,
                                                        SPRITE_ORIGIN_BUILDING, SPRITE_ORIGIN_BUILDING, g_curSprite,
                                                        SPRITE_FULL_SCALE, PAL_PARKLAND_ROCK);
                                                }
                                            }
                                        } else if (g_mapCourseList[g_myWorldCourseIdx].site != SITE_SPAIN) {
                                            g_curSprite = GetFlicBankSprite(BANK_ROSE_BUSH, 0, sceneryFacing);
                                            AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_MEDIUM,
                                                SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE, PAL_ROSE_BUSH);
                                        } else {
                                            g_curSprite = GetFlicBankSprite(BANK_VINEYARD, 0, sceneryFacing);
                                            AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_MEDIUM,
                                                SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE, PAL_VINEYARD);
                                        }
                                        break;
                                    }
                                    if (g_mapCourseList[g_myWorldCourseIdx].site == SITE_IRELAND) {
                                        g_curSprite = GetFlicBankSprite(BANK_POT_OF_GOLD, 0, sceneryFacing);
                                        DrawSpriteImmediate(screenX, screenY, screenY, SPRITE_ORIGIN_MEDIUM,
                                            SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE, PAL_GOLD);
                                    } else if (g_currentThemeId == THEME_DESERT) {
                                        int cowSkull = tileCol + passRow & 3;
                                        g_curSprite = GetFlicBankSprite(
                                            cowSkull != 0 ? BANK_COW_SKULL : BANK_DEAD_GOLFER, 0, sceneryFacing);
                                        DrawSpriteImmediate(screenX, screenY, screenY, SPRITE_ORIGIN_SMALL,
                                            SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE,
                                            cowSkull != 0 ? PAL_COW_SKULL : PAL_DEAD_GOLFER);
                                    } else {
                                        g_curSprite = GetFlicBankSprite(BANK_SCENIC_FLOWERS, 0, sceneryFacing);
                                        DrawSpriteImmediate(screenX, screenY, screenY, SPRITE_ORIGIN_SMALL,
                                            SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE, PAL_SCENIC_FLOWERS);
                                        g_curSprite = GetFlicBankSprite(BANK_ORNAMENTAL_GRASS, 0, sceneryFacing);
                                        AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_MEDIUM,
                                            SPRITE_ORIGIN_MEDIUM, g_curSprite, SPRITE_FULL_SCALE, PAL_ORNAMENTAL_GRASS);
                                    }
                                    break;
                                default:
                                    g_curSprite = GetFlicBankSprite(BANK_SCENIC_FLOWERS, 0, sceneryFacing);
                                    DrawSpriteImmediate(screenX, screenY, screenY, SPRITE_ORIGIN_SMALL,
                                        SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE, PAL_SCENIC_FLOWERS);
                                    g_curSprite = GetFlicBankSprite(BANK_TROPICAL_WILDFLOWERS, 0, sceneryFacing);
                                    AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_SMALL,
                                        SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE, PAL_TROPICAL_WILDFLOWERS);
                                    break;
                                case CATEGORY_SAND:
                                    g_curSprite = GetFlicBankSprite(BANK_RHODY, 0, sceneryFacing);
                                    AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_SMALL,
                                        SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE, PAL_RHODY);
                                    break;
                                case CATEGORY_ROCKS:
                                    g_curSprite = GetFlicBankSprite(BANK_PARKLAND_ROCK, 0, sceneryFacing);
                                    AddSpriteToRenderQueue(screenX, screenY, screenY, SPRITE_ORIGIN_SMALL,
                                        SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE, PAL_PARKLAND_ROCK);
                                    break;
                                }
                            }
                            int buildingType = g_tileExtra[tile] & TILE_FACILITY_MASK;
                            if ((g_gameTileTerrainKind[tile] == TERRAIN_HOME_SITE
                                    || g_gameTileTerrainKind[tile] == TERRAIN_BUILDING)
                                && buildingType != 0) {
                                buildingType--;
                                int buildingLevel = g_buildingLevelPerKind[buildingType];
                                if (buildingType == SCENERY_SNACK_BAR) {
                                    buildingLevel = 1;
                                }
                                int buildingScreenY;
                                int buildingScreenX;
                                if (buildingType == SCENERY_SNACK_BAR || buildingType < SCENERY_PUTTING_GREEN
                                    || buildingType == SCENERY_HOME_SITE) {
                                    int centerOffset =
                                        (g_facilityTypes[buildingType].side * TILE_WORLD_SIZE - TILE_WORLD_SIZE) / 2;
                                    WorldToScreen(passRow * TILE_WORLD_SIZE + TILE_WORLD_HALF + centerOffset,
                                        tileCol * TILE_WORLD_SIZE + TILE_WORLD_HALF + centerOffset, &buildingScreenX,
                                        &buildingScreenY, 0);
                                } else {
                                    WorldToScreen(
                                        ((g_facilityTypes[buildingType].side - 2 + g_buildingLevelPerKind[buildingType])
                                            * TILE_WORLD_SIZE)
                                                / 2
                                            + TILE_WORLD_HALF + passRow * TILE_WORLD_SIZE,
                                        ((buildingLevel - 2 + g_facilityTypes[buildingType].side) * TILE_WORLD_SIZE) / 2
                                            + TILE_WORLD_HALF + tileCol * TILE_WORLD_SIZE,
                                        &buildingScreenX, &buildingScreenY, 0);
                                }
                                buildingLevel--;
                                g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                                if (buildingType != SCENERY_FLOWER_BED) {
                                    RenderSpriteByType(buildingType, buildingLevel, buildingScreenX, buildingScreenY,
                                        screenX, screenY, g_tileVariationIdx[tile]);
                                } else {
                                    int bedNeighbors = 0;
                                    int flowerPalette;
                                    if (FindNearestHole(passRow, tileCol) % 4 == 0) {
                                        flowerPalette = g_tileVariationIdx[tile] + HOLE_BED_PALETTE_SHIFT;
                                    } else {
                                        flowerPalette = g_tileVariationIdx[tile];
                                    }
                                    flowerPalette = flowerPalette % FLOWER_BED_COLORS + PAL_FLOWER_BED_BUILDING;
                                    int dir = 0;
                                    do {
                                        bedNeighbors = bedNeighbors >> 1;
                                        int neighborTile =
                                            tileCol + g_dirColStep[dir] + (g_dirRowStep[dir] + passRow) * GRID_SIZE;
                                        if ((g_tileExtra[neighborTile] & TILE_FLOWERS) == TILE_FLOWERS
                                            && g_flatSquareAltMap[tile]
                                                   == g_flatSquareAltMap[(passRow + g_dirRowStep[dir]) * GRID_SIZE
                                                                         + tileCol + g_dirColStep[dir]]) {
                                            bedNeighbors = bedNeighbors | BED_SIDE_NEWEST;
                                        }
                                        dir = dir + 2;
                                    } while (dir < DIRECTION_COUNT);
                                    g_tileExtra[tile] &= TILE_CLEAR_SCENIC;
                                    tileFlags = g_tileExtra[tile];
                                    int quarterTurns = g_camDirIdx / QUARTER_TURN;
                                    if (0 < quarterTurns) {
                                        do {
                                            bedNeighbors =
                                                (bedNeighbors >> 1) | ((bedNeighbors & 1) << BED_SIDE_TOP_SHIFT);
                                            quarterTurns = quarterTurns - 1;
                                        } while (quarterTurns != 0);
                                    }
                                    if ((g_runModeFlags & GAME_PAUSED) != 0) {
                                        int flowerBank = g_flowerBedSprites[bedNeighbors].bank;
                                        int flowerFacing = g_flowerBedSprites[bedNeighbors].facing;
                                        flowerFacing = BorderFacing(flowerBank, flowerFacing, passRow - tileCol * 2);
                                        int flowerFrame = g_flicBankFrameLens[flowerBank] - 1;
                                        if (tileAnimFrame != -1
                                            && (flowerFrame = tileAnimFrame,
                                                g_tileAnimFrame[tile] >= g_flicBankFrameLens[flowerBank] - 1)) {
                                            g_tileExtra[tile] = tileFlags & TILE_CLEAR_ANIMATING;
                                            g_tileAnimFrame[tile] = TILE_ANIM_DONE;
                                        }
                                        g_curSprite = GetFlicBankSprite(flowerBank, flowerFrame, flowerFacing);
                                        DrawSpriteImmediate(buildingScreenX, buildingScreenY, buildingScreenY,
                                            SPRITE_ORIGIN_FLOWER_BED, SPRITE_ORIGIN_FLOWER_BED, g_curSprite,
                                            SPRITE_FULL_SCALE, flowerPalette);
                                    } else {
                                        DrawSpriteImmediate(buildingScreenX, buildingScreenY, 0, BED_ICON_ORIGIN_X,
                                            BED_ICON_ORIGIN_Y,
                                            &g_simFlowerNeighborGfx[(bedNeighbors - 1) & BED_SIDE_MASK],
                                            SPRITE_FULL_SCALE, PAL_FLOWER_BED_RED);
                                    }
                                }
                                if ((g_buildingTable[FindBuildingAtTile(passRow, tileCol)].flags & SLOT_REACHABLE) == 0
                                    && (g_clock & CAPTION_BLINK_BIT) != 0 && buildingType >= SCENERY_PUTTING_GREEN
                                    && buildingType != SCENERY_HOME_SITE) {
                                    g_printBuffer[0] = '\0';
                                    if (g_buildingLevelPerKind[buildingType] == LEVEL_UPGRADED) {
                                        strcpy(g_printBuffer, "Upgraded ");
                                    }
                                    if (g_buildingLevelPerKind[buildingType] == LEVEL_DELUXE) {
                                        strcpy(g_printBuffer, "Deluxe ");
                                    }
                                    strcat(g_printBuffer, g_facilityTypes[buildingType].name);
                                }
                            }
                            screenX = screenX - g_tileSpan;
                            screenY = screenY - tileHalfHeight;
                        }
                        passRow = passRow + 1;
                    } while (passRow < GRID_SIZE);
                    tileCol = tileCol + 1;
                } while (tileCol < GRID_SIZE);
                if (g_nextOpenHoleIndex <= MAX_HOLES && g_allHolesArray[g_nextOpenHoleIndex].teeX != 0
                    && g_allHolesArray[g_nextOpenHoleIndex].aimX != 0) {
                    int route = 0;
                    int pathIndex = 0;
                    int pathYards;
                    int routeEndX;
                    int routeEndY;
                    for (route = FIRST_TRACED_ROUTE; route < ROUTE_COUNT; route++) {
                        unsigned routeColor;
                        pathYards = 0;
                        g_shotsForTrace = 0;
                        memset(&g_golferPool[ROUTE_GOLFER], 0, sizeof(Golfer));
                        switch (route) {
                        case 0:
                            g_golferPool[ROUTE_GOLFER].talents = SKILLS_NO_LENGTH;
                            routeColor = COLOR_PALE_YELLOW;
                            break;
                        case 1:
                            g_golferPool[ROUTE_GOLFER].talents = SKILLS_NO_ACCURACY;
                            routeColor = COLOR_BRIGHT_GREEN;
                            break;
                        case 2:
                            g_golferPool[ROUTE_GOLFER].talents = SKILLS_NO_IMAGINATION;
                            routeColor = COLOR_BRIGHT_BLUE;
                            break;
                        case 3: g_golferPool[ROUTE_GOLFER].talents = SKILLS_ALL; routeColor = COLOR_WHITE;
                        }
                        g_golferPool[ROUTE_GOLFER].talentLevel = 1;
                        int teeTileX;
                        if (route != 0) {
                            teeTileX = g_allHolesArray[g_nextOpenHoleIndex].teeX;
                        } else {
                            teeTileX = g_allHolesArray[g_nextOpenHoleIndex].secondTeeX;
                        }
                        g_golferPool[ROUTE_GOLFER].ballX = teeTileX * TILE_WORLD_SIZE + TILE_WORLD_HALF;
                        g_golferPool[ROUTE_GOLFER].x = g_golferPool[ROUTE_GOLFER].ballX;
                        g_golferPool[ROUTE_GOLFER].shotFromX = g_golferPool[ROUTE_GOLFER].ballX;
                        int teeTileY;
                        if (route != 0) {
                            teeTileY = g_allHolesArray[g_nextOpenHoleIndex].teeY;
                        } else {
                            teeTileY = g_allHolesArray[g_nextOpenHoleIndex].secondTeeY;
                        }
                        g_golferPool[ROUTE_GOLFER].holeIdx = g_nextOpenHoleIndex;
                        g_golferPool[ROUTE_GOLFER].ballY = teeTileY * TILE_WORLD_SIZE + TILE_WORLD_HALF;
                        g_golferPool[ROUTE_GOLFER].y = g_golferPool[ROUTE_GOLFER].ballY;
                        g_golferPool[ROUTE_GOLFER].shotFromY = g_golferPool[ROUTE_GOLFER].ballY;
                        do {
                            if ((g_runModeFlags & GAME_ROUTE_RECORDED) == 0) {
                                g_shotPlanMode = SHOT_PLAN_ROUTE;
                                PlanGolferShot(ROUTE_GOLFER, 0, -1, 0, 0);
                                g_golferPool[ROUTE_GOLFER].targetX = g_plannedEndX >> TILE_WORLD_SHIFT;
                                g_golferPool[ROUTE_GOLFER].targetY = g_plannedEndY >> TILE_WORLD_SHIFT;
                                g_routeTargetX[pathIndex] = g_golferPool[ROUTE_GOLFER].targetX;
                                g_routeTargetY[pathIndex] = g_golferPool[ROUTE_GOLFER].targetY;
                                pathIndex = pathIndex + 1;
                                g_shotPlanMode = 0;
                            } else {
                                g_golferPool[ROUTE_GOLFER].targetX = g_routeTargetX[pathIndex];
                                g_golferPool[ROUTE_GOLFER].targetY = g_routeTargetY[pathIndex];
                                pathIndex = pathIndex + 1;
                            }
                            if (g_golferPool[ROUTE_GOLFER].holeStrokes == 0 && route == FIRST_TRACED_ROUTE) {
                                teeShotX = g_golferPool[ROUTE_GOLFER].targetX;
                                teeShotY = g_golferPool[ROUTE_GOLFER].targetY;
                                break;
                            }
                            if (1
                                < g_squareTypeTable[g_gameTileTerrainKind[g_golferPool[ROUTE_GOLFER].targetX * GRID_SIZE
                                                                          + g_golferPool[ROUTE_GOLFER].targetY]]
                                    .lieGrade)
                                break;
                            int leg = CalculateDistanceInYards(g_golferPool[ROUTE_GOLFER].shotFromX,
                                g_golferPool[ROUTE_GOLFER].shotFromY, g_golferPool[ROUTE_GOLFER].targetX,
                                g_golferPool[ROUTE_GOLFER].targetY);
                            pathYards = pathYards + leg;
                            int toScreenY;
                            int toScreenX;
                            int fromScreenY;
                            int fromScreenX;
                            if (WorldToScreen(g_golferPool[ROUTE_GOLFER].shotFromX,
                                    g_golferPool[ROUTE_GOLFER].shotFromY, &fromScreenX, &fromScreenY, 0)
                                    != 0
                                && TileToScreenCached(g_golferPool[ROUTE_GOLFER].targetX,
                                       g_golferPool[ROUTE_GOLFER].targetY, &toScreenX, &toScreenY)
                                       != 0) {
                                int nudge = route * ROUTE_LINE_SPACING - ROUTE_LINE_SPACING;
                                fromScreenX = fromScreenX + nudge;
                                toScreenX = toScreenX + nudge;
                                DrawTerrainLine(fromScreenX, fromScreenY + 1, toScreenX, toScreenY + 1, COLOR_BLACK,
                                    LINE_WIDTH_THIN, LINE_ALPHA);
                                DrawTerrainLine(fromScreenX, fromScreenY, toScreenX, toScreenY, routeColor,
                                    LINE_WIDTH_THIN, ROUTE_LINE_ALPHA);
                            }
                            g_golferPool[ROUTE_GOLFER].ballX =
                                g_golferPool[ROUTE_GOLFER].targetX * TILE_WORLD_SIZE + TILE_WORLD_HALF;
                            g_golferPool[ROUTE_GOLFER].x = g_golferPool[ROUTE_GOLFER].ballX;
                            g_golferPool[ROUTE_GOLFER].shotFromX = g_golferPool[ROUTE_GOLFER].ballX;
                            g_golferPool[ROUTE_GOLFER].ballY =
                                g_golferPool[ROUTE_GOLFER].targetY * TILE_WORLD_SIZE + TILE_WORLD_HALF;
                            g_golferPool[ROUTE_GOLFER].y = g_golferPool[ROUTE_GOLFER].ballY;
                            g_golferPool[ROUTE_GOLFER].shotFromY = g_golferPool[ROUTE_GOLFER].ballY;
                            if (g_shotsForTrace == 0
                                && g_gameTileTerrainKind[g_golferPool[ROUTE_GOLFER].targetX * GRID_SIZE
                                                         + g_golferPool[ROUTE_GOLFER].targetY]
                                       == TERRAIN_GREEN) {
                                g_shotsForTrace = g_golferPool[ROUTE_GOLFER].holeStrokes + ROUTE_PAR_AFTER_GREEN;
                            }
                            g_golferPool[ROUTE_GOLFER].holeStrokes += 1;
                        } while (
                            g_golferPool[ROUTE_GOLFER].holeStrokes < ROUTE_MAX_SHOTS
                            && (g_golferPool[ROUTE_GOLFER].targetX != g_allHolesArray[g_nextOpenHoleIndex].aimX
                                || g_golferPool[ROUTE_GOLFER].targetY != g_allHolesArray[g_nextOpenHoleIndex].aimY));
                        routeEndY = g_golferPool[ROUTE_GOLFER].targetY;
                        routeEndX = g_golferPool[ROUTE_GOLFER].targetX;
                    }
                    g_runModeFlags |= GAME_ROUTE_RECORDED;
                    g_allHolesArray[g_nextOpenHoleIndex].yards = 0;
                    if (routeEndX == g_allHolesArray[g_nextOpenHoleIndex].aimX
                        && routeEndY == g_allHolesArray[g_nextOpenHoleIndex].aimY) {
                        g_allHolesArray[g_nextOpenHoleIndex].yards = pathYards * YARDS_PER_TILE / YARDS_PER_TILE;
                    }
                    g_allHolesArray[g_nextOpenHoleIndex].teeDir =
                        ((CalculateAngle(teeShotX - g_allHolesArray[g_nextOpenHoleIndex].teeX,
                              teeShotY - g_allHolesArray[g_nextOpenHoleIndex].teeY)
                                 >> ANGLE_TO_SIXTEENTHS_SHIFT
                             & 0xf)
                            + 1)
                            >> 1
                        & DIRECTION_MASK;
                    g_allHolesArray[g_nextOpenHoleIndex].flags &= ~(HOLE_UPHILL | HOLE_DOWNHILL);
                    if (GetTileElevation(g_allHolesArray[g_nextOpenHoleIndex].aimX,
                            g_allHolesArray[g_nextOpenHoleIndex].aimY, 0)
                        > GetTileElevation(g_allHolesArray[g_nextOpenHoleIndex].teeX,
                              g_allHolesArray[g_nextOpenHoleIndex].teeY, 0)
                              + 1) {
                        g_allHolesArray[g_nextOpenHoleIndex].flags |= HOLE_UPHILL;
                    }
                    if (GetTileElevation(g_allHolesArray[g_nextOpenHoleIndex].aimX,
                            g_allHolesArray[g_nextOpenHoleIndex].aimY, 0)
                        < GetTileElevation(g_allHolesArray[g_nextOpenHoleIndex].teeX,
                              g_allHolesArray[g_nextOpenHoleIndex].teeY, 0)
                              - 1) {
                        g_allHolesArray[g_nextOpenHoleIndex].flags |= HOLE_DOWNHILL;
                    }
                    if (pathYards < SHORT_HOLE_YARDS) {
                        teeShotX = g_allHolesArray[g_nextOpenHoleIndex].teeX;
                        teeShotY = g_allHolesArray[g_nextOpenHoleIndex].teeY;
                    }
                    int doglegAngle =
                        CalculateAngle(g_allHolesArray[g_nextOpenHoleIndex].aimX
                                           - g_allHolesArray[g_nextOpenHoleIndex].teeX,
                            g_allHolesArray[g_nextOpenHoleIndex].aimY - g_allHolesArray[g_nextOpenHoleIndex].teeY)
                        - CalculateAngle(g_allHolesArray[g_nextOpenHoleIndex].aimX - teeShotX,
                            g_allHolesArray[g_nextOpenHoleIndex].aimY - teeShotY);
                    if (g_allHolesArray[g_nextOpenHoleIndex].aimX == teeShotX
                        && g_allHolesArray[g_nextOpenHoleIndex].aimY == teeShotY) {
                        doglegAngle = 0;
                    }
                    g_allHolesArray[g_nextOpenHoleIndex].flags &= ~(HOLE_DOGLEG_LEFT | HOLE_DOGLEG_RIGHT);
                    if (DOGLEG_MIN_ANGLE < doglegAngle) {
                        g_allHolesArray[g_nextOpenHoleIndex].flags |= HOLE_DOGLEG_LEFT;
                    }
                    if (doglegAngle < -DOGLEG_MIN_ANGLE) {
                        g_allHolesArray[g_nextOpenHoleIndex].flags |= HOLE_DOGLEG_RIGHT;
                    }
                    int bearingToCorner = CalculateAngle(teeShotX - g_allHolesArray[g_nextOpenHoleIndex].aimX,
                        teeShotY - g_allHolesArray[g_nextOpenHoleIndex].aimY);
                    int sweepAngle = LANDING_SWEEP_START;
                    HoleLandingPoint *landing = g_gameFairwayTargetPoints[g_nextOpenHoleIndex];
                    memset(landing, 0xff, sizeof(g_gameFairwayTargetPoints[0]));
                    int spoke = LANDING_SWEEP_SPOKES;
                    do {
                        int landingSlot = 0;
                        int pinYardsScaled = LANDING_FIRST_RADIUS_SCALED;
                        int sampleAngle = ((spoke & 1) != 0 ? 1 : -1) * (sweepAngle / 8) + bearingToCorner;
                        int minHoleYards = LANDING_FIRST_YARDS;
                        do {
                            if (minHoleYards < pathYards) {
                                int radius = pinYardsScaled / YARDS_PER_TILE;
                                int spotX = FixedPointSin(sampleAngle, radius) + TILE_WORLD_HALF
                                            + g_allHolesArray[g_nextOpenHoleIndex].aimX * TILE_WORLD_SIZE;
                                int spotY = (g_allHolesArray[g_nextOpenHoleIndex].aimY * TILE_WORLD_SIZE
                                                - FixedPointCos(sampleAngle, radius))
                                            + TILE_WORLD_HALF;
                                if (g_gameTileTerrainKind[(spotX >> TILE_WORLD_SHIFT) * GRID_SIZE
                                                          + (spotY >> TILE_WORLD_SHIFT)]
                                    == TERRAIN_FAIRWAY) {
                                    int spotTileX = spotX >> TILE_WORLD_SHIFT;
                                    int spotTileY = spotY >> TILE_WORLD_SHIFT;
                                    int subX = (spotX % TILE_WORLD_SIZE) >> SUBTILE_SHIFT;
                                    int subY = (spotY % TILE_WORLD_SIZE) >> SUBTILE_SHIFT;
                                    if ((subX >= LANDING_EDGE_LOW
                                            || g_gameTileTerrainKind[(spotTileX - 1) * GRID_SIZE + spotTileY]
                                                   == TERRAIN_FAIRWAY)
                                        && (subX < LANDING_EDGE_HIGH
                                            || g_gameTileTerrainKind[(spotTileX + 1) * GRID_SIZE + spotTileY]
                                                   == TERRAIN_FAIRWAY)
                                        && (subY >= LANDING_EDGE_LOW
                                            || g_gameTileTerrainKind[spotTileX * GRID_SIZE + spotTileY - 1]
                                                   == TERRAIN_FAIRWAY)
                                        && (subY < LANDING_EDGE_HIGH
                                            || g_gameTileTerrainKind[spotTileX * GRID_SIZE + spotTileY + 1]
                                                   == TERRAIN_FAIRWAY)) {
                                        g_gameFairwayTargetPoints[g_nextOpenHoleIndex][landingSlot].x = spotX;
                                        g_gameFairwayTargetPoints[g_nextOpenHoleIndex][landingSlot].y = spotY;
                                    }
                                }
                            }
                            landingSlot = landingSlot + 1;
                            minHoleYards = minHoleYards + LANDING_STEP_YARDS;
                            pinYardsScaled = pinYardsScaled + LANDING_STEP_RADIUS_SCALED;
                        } while (minHoleYards < LANDING_END_YARDS);
                        spoke = spoke - 1;
                        sweepAngle = sweepAngle - ANGLE_15_DEGREES;
                    } while (0 < spoke);
                }
                SaveRenderQueue();
                UpdateAndRenderWildlife();
                for (int hole = 1; hole < HOLE_SLOT_COUNT; hole++) {
                    for (int landingSlot = 0; landingSlot < LANDING_SPOTS_PER_HOLE; landingSlot++) {
                        if (0 < g_gameFairwayTargetPoints[hole][landingSlot].x) {
                            int spotScreenY;
                            int spotScreenX;
                            WorldToScreen(g_gameFairwayTargetPoints[hole][landingSlot].x,
                                g_gameFairwayTargetPoints[hole][landingSlot].y, &spotScreenX, &spotScreenY, 0);
                        }
                    }
                }
                int drawRow = 0;
                (void)avatarScreenX;
                (void)speechX;
                int drawCol = 0;
                do {
                    drawRow = 0;
                    do {
                        int tileAnimFrame = -1;
                        int tile = drawCol + drawRow * GRID_SIZE;
                        short tileFlags = g_tileExtra[tile];
                        if ((tileFlags & TILE_ANIMATING) != 0) {
                            tileAnimFrame = g_tileAnimFrame[tile];
                            if ((tileFlags & TILE_WEEDS) == 0 || g_rng.NextInt(WEED_GROWTH_ODDS) == 0) {
                                g_tileAnimFrame[tile] += 1;
                            }
                            if (TILE_ANIM_LAST_FRAME < tileAnimFrame) {
                                g_tileExtra[tile] &= TILE_CLEAR_ANIMATING;
                            }
                        }
                        int tileScreenY;
                        int tileScreenX;
                        if (TileToScreenCached(drawRow, drawCol, &tileScreenX, &tileScreenY) == 0) {
                            if (0 < tileAnimFrame && (tileFlags = g_tileExtra[tile], (tileFlags & TILE_WEEDS) == 0)) {
                                g_tileExtra[tile] = tileFlags & TILE_CLEAR_ANIMATING;
                            }
                        } else {
                            int tileTopY = tileScreenY - tileHalfHeight;
                            char tileTerrain = g_gameTileTerrainKind[drawRow * GRID_SIZE + drawCol];
                            if (tileTerrain != TERRAIN_OUT_OF_BOUNDS) {
                                int buildingType = g_tileExtra[tile] & TILE_FACILITY_MASK;
                                if ((tileTerrain == TERRAIN_HOME_SITE || tileTerrain == TERRAIN_BUILDING)
                                    && buildingType != 0) {
                                    buildingType = buildingType - 1;
                                    int buildingScreenY;
                                    int buildingScreenX;
                                    if (buildingType < SCENERY_PUTTING_GREEN || buildingType == SCENERY_SNACK_BAR
                                        || buildingType == SCENERY_HOME_SITE) {
                                        int footprintSpan =
                                            g_facilityTypes[buildingType].side * TILE_WORLD_SIZE - TILE_WORLD_SIZE;
                                        WorldToScreen(drawRow * TILE_WORLD_SIZE + TILE_WORLD_HALF + footprintSpan / 2,
                                            drawCol * TILE_WORLD_SIZE + TILE_WORLD_HALF + footprintSpan / 2,
                                            &buildingScreenX, &buildingScreenY, 0);
                                    } else {
                                        WorldToScreen(drawRow * TILE_WORLD_SIZE + TILE_WORLD_HALF
                                                          + (g_facilityTypes[buildingType].side - 2
                                                                + g_buildingLevelPerKind[buildingType])
                                                                * TILE_WORLD_SIZE / 2,
                                            drawCol * TILE_WORLD_SIZE + TILE_WORLD_HALF
                                                + (g_facilityTypes[buildingType].side - 2
                                                      + g_buildingLevelPerKind[buildingType])
                                                      * TILE_WORLD_SIZE / 2,
                                            &buildingScreenX, &buildingScreenY, 0);
                                    }
                                    int plotSlot;
                                    if ((g_tileExtra[tile] & TILE_ANIMATING) == 0 && buildingType == SCENERY_HOME_SITE
                                        && (plotSlot = FindBuildingAtTile(drawRow, drawCol),
                                            g_buildingTable[plotSlot].count != 0)) {
                                        RenderPlot(plotSlot & 1, drawRow, drawCol);
                                    }
                                }
                                if ((g_tileExtra[tile] & TILE_WEEDS) != 0) {
                                    if ((g_runModeFlags & GAME_CHAMPIONSHIP) != 0) {
                                        g_tileExtra[tile] &= TILE_CLEAR_WEEDS;
                                    }
                                    int maxFrame = WEED_NO_FRAME_CAP;
                                    if ((g_tileExtra[tile] & TILE_ANIMATING) != 0) {
                                        maxFrame = Clamp(tileAnimFrame, 0, MAX_ANIM_FRAME);
                                    }
                                    int variant = (drawRow + drawCol * 2) % 4;
                                    if (g_squareTypeTable[g_gameTileTerrainKind[drawRow * GRID_SIZE + drawCol]].lieGrade
                                        <= 0) {
                                        g_curSprite = GetFlicBankSprite(BANK_CRABGRASS,
                                            Clamp(g_flicBankFrameLens[BANK_CRABGRASS] - (drawCol & 2) - (drawRow & 1)
                                                      - 1,
                                                variant, maxFrame),
                                            0);
                                        DrawSpriteImmediate(tileScreenX, tileScreenY, tileTopY, GOLFER_ORIGIN_X,
                                            GOLFER_ORIGIN_Y, g_curSprite, SPRITE_FULL_SCALE, PAL_CRABGRASS);
                                    } else {
                                        g_curSprite = GetFlicBankSprite(BANK_FLOWERS,
                                            Clamp(g_flicBankFrameLens[BANK_FLOWERS] - (drawCol & 2) - (drawRow & 1) - 1,
                                                variant, maxFrame),
                                            0);
                                        DrawSpriteImmediate(tileScreenX, tileScreenY, tileTopY, GOLFER_ORIGIN_X,
                                            GOLFER_ORIGIN_Y, g_curSprite, SPRITE_FULL_SCALE, PAL_WEED_FLOWERS);
                                    }
                                    tileAnimFrame = -1;
                                }
                                int terrainType = g_gameTileTerrainKind[drawRow * GRID_SIZE + drawCol];
                                if (g_squareTypeTable[terrainType].category == CATEGORY_TREES && 1 < g_cameraZoom
                                    && (g_runModeFlags & GAME_HIDE_TREES) == 0
                                    && ((g_tileExtra[tile] & TILE_SCENIC) == 0
                                        || g_gameTileTerrainKind[drawRow * GRID_SIZE + drawCol] == TERRAIN_ELM_TREE)) {
                                    int neighborCoverMask = 0;
                                    int sameCoverCount = 0;
                                    int symmetricAxes = 0;
                                    int diagonalDir = 1;
                                    passRow = diagonalDir;
                                    do {
                                        for (int dirOffset = -1; dirOffset <= 1; dirOffset++) {
                                            int neighborDir = (g_camDirIdx + passRow + dirOffset) & DIRECTION_MASK;
                                            neighborCoverMask =
                                                neighborCoverMask
                                                | 1 << g_gameTileTerrainKind[(drawRow + g_dirRowStep[neighborDir])
                                                                                 * GRID_SIZE
                                                                             + drawCol + g_dirColStep[neighborDir]];
                                            if (0 <= dirOffset
                                                && g_gameTileTerrainKind[(drawRow + g_dirRowStep[neighborDir])
                                                                             * GRID_SIZE
                                                                         + drawCol + g_dirColStep[neighborDir]]
                                                       == terrainType) {
                                                sameCoverCount = sameCoverCount + 1;
                                            }
                                        }
                                        passRow = passRow + 2;
                                    } while (passRow < DIRECTION_COUNT);
                                    int axis = 0;
                                    int kind = g_squareTypeTable[terrainType].kind;
                                    do {
                                        int sideCover = g_gameTileTerrainKind[(drawRow + g_dirRowStep[axis]) * GRID_SIZE
                                                                              + drawCol + g_dirColStep[axis]];
                                        int oppositeCover =
                                            g_gameTileTerrainKind[(drawRow + g_dirRowStep[axis | DIRECTION_OPPOSITE])
                                                                      * GRID_SIZE
                                                                  + drawCol + g_dirColStep[axis | DIRECTION_OPPOSITE]];
                                        if (g_squareTypeTable[sideCover].kind == kind
                                            && g_squareTypeTable[oppositeCover].kind == kind) {
                                            symmetricAxes = symmetricAxes + 1;
                                        }
                                        if (g_squareTypeTable[sideCover].kind != kind
                                            && g_squareTypeTable[oppositeCover].kind != kind) {
                                            symmetricAxes = symmetricAxes + 1;
                                        }
                                        axis = axis + 1;
                                    } while (axis < AXIS_COUNT);
                                    if (g_gameTileTerrainKind[drawRow * GRID_SIZE + drawCol] != TERRAIN_ELM_TREE) {
                                        int scale;
                                        if (g_gameTileTerrainKind[drawRow * GRID_SIZE + drawCol] == TERRAIN_TREE) {
                                            scale = TREE_SCALE_NORMAL;
                                            int treeBank = BANK_TREE_LARGE;
                                            switch (symmetricAxes) {
                                            case 0: treeBank = BANK_TREE_SMALL; break;
                                            case 1: scale = TREE_SCALE_SMALL;
                                            case 2: treeBank = BANK_TREE_MEDIUM; break;
                                            case 3: scale = TREE_SCALE_SMALL;
                                            case 4: treeBank = BANK_TREE_LARGE;
                                            }
                                            if (g_tileVariationIdx[drawRow * GRID_SIZE + drawCol] != 0) {
                                                treeBank =
                                                    g_tileVariationIdx[drawRow * GRID_SIZE + drawCol] % TREE_SIZES
                                                    + BANK_TREE_SMALL;
                                                scale = TREE_SCALE_NORMAL;
                                            }
                                            int palette = ((CalculateTerrainHeight(drawRow << TREE_TINT_SAMPLE_SHIFT,
                                                                drawCol << TREE_TINT_SAMPLE_SHIFT)
                                                               / TINT_BAND_HEIGHT)
                                                              & TINT_MASK)
                                                          + PAL_TREE;
                                            int treeFacing = (drawRow - drawCol * 2) & FACING_MASK;
                                            if (tileAnimFrame != -1) {
                                                g_curSprite = GetFlicBankSprite(treeBank, tileAnimFrame, treeFacing);
                                                if (tileAnimFrame == g_flicBankFrameLens[treeBank] - 1) {
                                                    g_tileExtra[tile] &= TILE_CLEAR_ANIMATING;
                                                }
                                                if (tileAnimFrame == 0) {
                                                    ConfigureAndPlaySound(SOUND_MAPLE, SOUND_FULL_VOLUME, 0, 0, 0);
                                                }
                                                tileAnimFrame = ANIM_FRAME_USED;
                                                scale = TREE_SCALE_NORMAL;
                                            } else {
                                                g_curSprite = GetFlicBankSprite(treeBank,
                                                    g_flicBankFrameLens[treeBank] - 1, treeFacing);
                                                if ((neighborCoverMask & COVER_BIT_TREE) == 0
                                                    && (g_currentThemeId == THEME_PARK
                                                        || g_currentThemeId == THEME_LINKS)) {
                                                    g_curSprite = GetFlicBankSprite(BANK_SCENIC_ELM,
                                                        g_flicBankFrameLens[BANK_SCENIC_ELM] - 1, treeFacing);
                                                    AddSpriteToRenderQueue(tileScreenX, tileScreenY,
                                                        g_tileSpan / 6 + tileScreenY, SPRITE_ORIGIN_SCENIC,
                                                        SPRITE_ORIGIN_SCENIC, g_curSprite, SPRITE_FULL_SCALE,
                                                        PAL_SCENIC_ELM);
                                                    palette = -1;
                                                }
                                            }
                                            if (palette != -1) {
                                                AddSpriteToRenderQueue(((drawRow + drawCol * 3) % 5 - 2) * g_cameraZoom
                                                                           + tileScreenX,
                                                    tileScreenY, tileTopY + tileHalfHeight, SPRITE_ORIGIN_BUILDING,
                                                    SPRITE_ORIGIN_BUILDING, g_curSprite, scale, palette);
                                            }
                                        }
                                        if (g_gameTileTerrainKind[drawRow * GRID_SIZE + drawCol] == TERRAIN_PINE_TREE) {
                                            scale = TREE_SCALE_NORMAL;
                                            int pineBank = BANK_PINE_LARGE;
                                            switch (symmetricAxes) {
                                            case 0: pineBank = BANK_PINE_SMALL; break;
                                            case 2: scale = TREE_SCALE_SMALL;
                                            case 1: pineBank = BANK_PINE_MEDIUM; break;
                                            case 3: scale = TREE_SCALE_SMALL;
                                            case 4: pineBank = BANK_PINE_LARGE;
                                            }
                                            int palette = PAL_PINE;
                                            if (((CalculateTerrainHeight(drawRow << TREE_TINT_SAMPLE_SHIFT,
                                                      drawCol << TREE_TINT_SAMPLE_SHIFT)
                                                     / PINE_SPECIES_BAND)
                                                    & PINE_SPECIES_BIT)
                                                != 0) {
                                                pineBank = pineBank + PINE_SPECIES_OFFSET;
                                                if (g_currentThemeId == THEME_TROPICAL) {
                                                    palette = PAL_PINE_ALT;
                                                } else if (g_currentThemeId == THEME_DESERT) {
                                                    palette = PAL_PINE_ALT;
                                                }
                                            }
                                            if (g_currentThemeId == THEME_PARK || g_currentThemeId == THEME_LINKS) {
                                                palette = ((CalculateTerrainHeight(drawRow << TREE_TINT_SAMPLE_SHIFT,
                                                                drawCol << TREE_TINT_SAMPLE_SHIFT)
                                                               / TINT_BAND_HEIGHT)
                                                              & TINT_MASK)
                                                          + PAL_PINE;
                                            }
                                            int growFrame = tileAnimFrame;
                                            if (g_tileVariationIdx[drawRow * GRID_SIZE + drawCol] != 0) {
                                                pineBank =
                                                    g_tileVariationIdx[drawRow * GRID_SIZE + drawCol] % TREE_SIZES
                                                    + BANK_PINE_SMALL;
                                                if (g_currentThemeId == THEME_TROPICAL
                                                    || g_currentThemeId == THEME_DESERT) {
                                                    palette = PAL_PINE;
                                                }
                                                if ((g_tileVariationIdx[drawRow * GRID_SIZE + drawCol] / TREE_SIZES & 1)
                                                    != 0) {
                                                    pineBank = pineBank + PINE_SPECIES_OFFSET;
                                                    if (g_currentThemeId == THEME_TROPICAL
                                                        || g_currentThemeId == THEME_DESERT) {
                                                        palette = palette + 1;
                                                    }
                                                }
                                                scale = TREE_SCALE_NORMAL;
                                            }
                                            int treeFacing = (drawRow - drawCol * 2) & FACING_MASK;
                                            if (tileAnimFrame != -1) {
                                                g_curSprite = GetFlicBankSprite(pineBank, tileAnimFrame, treeFacing);
                                                if (growFrame == g_flicBankFrameLens[pineBank] - 1) {
                                                    g_tileExtra[tile] &= TILE_CLEAR_ANIMATING;
                                                }
                                                if (growFrame == 0) {
                                                    ConfigureAndPlaySound(SOUND_TREE_SPROUT, SOUND_HALF_VOLUME, 0, 0,
                                                        0);
                                                }
                                                tileAnimFrame = ANIM_FRAME_USED;
                                                scale = TREE_SCALE_NORMAL;
                                            } else {
                                                g_curSprite = GetFlicBankSprite(pineBank,
                                                    g_flicBankFrameLens[pineBank] - 1, treeFacing);
                                            }
                                            if (pineBank == BANK_PINE_SMALL) {
                                                int sortKey = tileTopY + tileHalfHeight;
                                                AddSpriteToRenderQueue(g_tileSpan / ((drawRow & 1) != 0 ? 3 : 5)
                                                                           + tileScreenX,
                                                    tileScreenY, sortKey, SPRITE_ORIGIN_BUILDING,
                                                    SPRITE_ORIGIN_BUILDING, g_curSprite, ((drawCol & 2U) >> 1) + scale,
                                                    palette);
                                                AddSpriteToRenderQueue(tileScreenX
                                                                           - g_tileSpan / ((drawCol & 1) != 0 ? 3 : 5),
                                                    tileScreenY - g_tileSpan / ((drawRow & 2) != 0 ? 4 : -6), sortKey,
                                                    SPRITE_ORIGIN_BUILDING, SPRITE_ORIGIN_BUILDING, g_curSprite,
                                                    ((~drawCol & 2U) >> 1) + scale, palette);
                                            } else {
                                                AddSpriteToRenderQueue(((drawRow + drawCol * 3) % 5 - 2) * g_cameraZoom
                                                                           + tileScreenX,
                                                    tileScreenY, tileTopY + tileHalfHeight, SPRITE_ORIGIN_BUILDING,
                                                    SPRITE_ORIGIN_BUILDING, g_curSprite, scale, palette);
                                            }
                                        }
                                        if (g_gameTileTerrainKind[drawRow * GRID_SIZE + drawCol] == TERRAIN_PALM_TREE) {
                                            int growFrame = tileAnimFrame;
                                            int palmScale = TREE_SCALE_NORMAL;
                                            int palmBank = BANK_PALM_LARGE;
                                            switch (symmetricAxes) {
                                            case 0: palmBank = BANK_PALM_SMALL; break;
                                            case 2: palmScale = TREE_SCALE_LARGE;
                                            case 1: palmBank = BANK_PALM_MEDIUM; break;
                                            case 3: palmScale = TREE_SCALE_SMALL;
                                            case 4: palmBank = BANK_PALM_LARGE;
                                            }
                                            if (g_tileVariationIdx[drawRow * GRID_SIZE + drawCol] != 0) {
                                                palmScale = TREE_SCALE_NORMAL;
                                                palmBank =
                                                    g_tileVariationIdx[drawRow * GRID_SIZE + drawCol] % TREE_SIZES
                                                    + BANK_PALM_SMALL;
                                            }
                                            int treeFacing = (drawRow - drawCol * 2) & FACING_MASK;
                                            if (tileAnimFrame != -1) {
                                                g_curSprite = GetFlicBankSprite(palmBank, tileAnimFrame, treeFacing);
                                                if (growFrame == g_flicBankFrameLens[palmBank] - 1) {
                                                    g_tileExtra[tile] &= TILE_CLEAR_ANIMATING;
                                                }
                                                if (growFrame == 0) {
                                                    ConfigureAndPlaySound(SOUND_PALM, SOUND_HALF_VOLUME, 0, 0, 0);
                                                }
                                                tileAnimFrame = ANIM_FRAME_USED;
                                                palmScale = TREE_SCALE_NORMAL;
                                            } else {
                                                g_curSprite = GetFlicBankSprite(palmBank,
                                                    g_flicBankFrameLens[palmBank] - 1, treeFacing);
                                            }
                                            int palette = ((CalculateTerrainHeight(drawRow << PALM_TINT_SAMPLE_SHIFT,
                                                                drawCol << PALM_TINT_SAMPLE_SHIFT)
                                                               / TINT_BAND_HEIGHT)
                                                              & TINT_MASK)
                                                          + PAL_PALM;
                                            if (palmBank == BANK_PALM_SMALL) {
                                                int sortKey = tileTopY + tileHalfHeight;
                                                AddSpriteToRenderQueue(g_tileSpan / ((drawRow & 1) != 0 ? 3 : 5)
                                                                           + tileScreenX,
                                                    tileScreenY, sortKey, SPRITE_ORIGIN_BUILDING,
                                                    SPRITE_ORIGIN_BUILDING, g_curSprite,
                                                    ((drawCol & 2U) >> 1) + palmScale, palette);
                                                AddSpriteToRenderQueue(tileScreenX
                                                                           - g_tileSpan / ((drawCol & 1) != 0 ? 3 : 5),
                                                    tileScreenY - g_tileSpan / ((drawRow & 2) != 0 ? 4 : -6), sortKey,
                                                    SPRITE_ORIGIN_BUILDING, SPRITE_ORIGIN_BUILDING, g_curSprite,
                                                    ((~drawCol & 2U) >> 1) + palmScale, palette);
                                            } else {
                                                AddSpriteToRenderQueue(((drawRow + drawCol * 3) % 5 - 2) * g_cameraZoom
                                                                           + tileScreenX,
                                                    tileScreenY, tileTopY + tileHalfHeight, SPRITE_ORIGIN_BUILDING,
                                                    SPRITE_ORIGIN_BUILDING, g_curSprite, palmScale, palette);
                                            }
                                        }
                                        if (0 <= tileAnimFrame) {
                                            g_tileExtra[tile] &= TILE_CLEAR_ANIMATING;
                                        }
                                    }
                                }
                            }
                        }
                        drawRow++;
                    } while (drawRow < GRID_SIZE);
                    drawCol++;
                } while (drawCol < GRID_SIZE);
                g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                g_renderedBallCount = 0;
                golferIndex = 0;
                do {
                    g_golferPool[golferIndex].screenX = -1;
                    if ((g_golferPool[golferIndex].holeIdx != 0) && (g_golferPool[golferIndex].holeIdx != -1)) {
                        int animVariant;
                        int type;
                        if (IsGolferMale(golferIndex) == 0) {
                            animVariant = OUTFIT_WOMAN;
                            type = g_golferPool[golferIndex].typeIdx;
                            if ((g_golferTypeArray[type].ageBits & AGE_PLUS_20) != 0) {
                                animVariant = OUTFIT_WOMAN_OLDER;
                            }
                            if ((g_golferTypeArray[type].caps & TRAIT_PLAYFUL) != 0) {
                                animVariant = OUTFIT_WOMAN_PLAYFUL;
                            }
                            if ((g_golferTypeArray[type].ageBits & AGE_PLUS_45) != 0) {
                                animVariant = OUTFIT_WOMAN_ELDER;
                            }
                            if (g_golferTypeArray[type].fixedKind != 0) {
                                animVariant = ((g_golferTypeArray[type].appearance >> OUTFIT_SHIFT) & OUTFIT_MASK)
                                              + FEMALE_VARIANT_BASE;
                            }
                        } else {
                            if ((g_golferPool[golferIndex].talents & TALENT_IMAGINATION) != 0) {
                                if ((g_golferPool[golferIndex].talents & TALENT_ACCURACY) != 0) {
                                    animVariant = OUTFIT_MAN_IMAGINATIVE_ACCURATE;
                                } else {
                                    animVariant = OUTFIT_MAN_IMAGINATIVE;
                                }
                            } else {
                                animVariant = OUTFIT_MAN;
                            }
                            type = g_golferPool[golferIndex].typeIdx;
                            if (g_golferTypeArray[g_golferPool[golferIndex].typeIdx].fixedKind != 0) {
                                animVariant = (g_golferTypeArray[type].appearance >> OUTFIT_SHIFT) & OUTFIT_MASK;
                            }
                            if (g_golferPool[golferIndex].persona == PERSONA_ROSTER) {
                                animVariant = g_golfProList[g_golferPool[golferIndex].rosterIndex].figure & OUTFIT_MASK;
                            }
                        }
                        g_golferTypeArray[type].appearance &= APPEARANCE_LOW_BITS;
                        if (IsGolferMale(golferIndex) == 0) {
                            g_golferTypeArray[g_golferPool[golferIndex].typeIdx].appearance |=
                                (animVariant - FEMALE_VARIANT_BASE) << OUTFIT_SHIFT;
                        } else {
                            g_golferTypeArray[g_golferPool[golferIndex].typeIdx].appearance |= animVariant
                                                                                               << OUTFIT_SHIFT;
                        }
                        int spriteFacing =
                            ((g_camDirIdx - g_golferPool[golferIndex].dir) - QUARTER_TURN) & DIRECTION_MASK;
                        unsigned frameAtStart = g_golferPool[golferIndex].animTick;
                        if (g_golferPool[golferIndex].activity < GOLFER_WALKING) {
                            animBank = ANIM_SWING;
                            animFrame = g_golferPool[golferIndex].animTick;
                            if (IsGolferMale(golferIndex) != 0) {
                                if ((g_golferPool[golferIndex].talents & TALENT_ACCURACY) == 0) {
                                    animBank = ANIM_SWING_NO_ACCURACY;
                                } else if ((g_golferPool[golferIndex].talents & TALENT_IMAGINATION) == 0) {
                                    animBank = ANIM_SWING_NO_IMAGINATION;
                                }
                            }
                            if (g_golferPool[golferIndex].club
                                > (g_golferPool[golferIndex].shotType != SHOT_HIGH_BACKSPIN
                                        ? LAST_FULL_SWING_CLUB
                                        : LAST_FULL_SWING_CLUB_HIGH)) {
                                animBank = ANIM_PITCH;
                            }
                            if (g_golferPool[golferIndex].shotStep <= SHOT_STEP_ADDRESSED) {
                                g_golferPool[golferIndex].animTick = 0;
                            }
                            if (g_golferPool[golferIndex].club == CLUB_PUTTER) {
                                animBank = ANIM_PUTT;
                            }
                            int frameCount;
                            if ((g_runModeFlags & GAME_PAUSED) == 0
                                && g_golferPool[golferIndex].activity != SWING_FOLLOW_THROUGH) {
                                if ((g_golferPool[golferIndex].flags & GOLFER_REPLAYING) != 0) {
                                    if (g_golferPool[golferIndex].animTick > 0) {
                                        g_golferPool[golferIndex].animTick -= 1;
                                    } else {
                                        g_golferPool[golferIndex].flags =
                                            g_golferPool[golferIndex].flags & ~GOLFER_REPLAYING;
                                    }
                                    animFrame = Clamp(g_golferPool[golferIndex].animTick, 0, MAX_ANIM_FRAME);
                                } else {
                                    if (((g_golferPool[golferIndex].animTick == WAGGLE_FRAME) && g_rng.NextInt(2) != 0)
                                        && ((g_golferPool[golferIndex].club <= LAST_LONG_CLUB
                                             && ((g_golferPool[golferIndex].flags & GOLFER_BALL_TRAIL) == 0)))) {
                                        g_golferPool[golferIndex].flags =
                                            g_golferPool[golferIndex].flags | GOLFER_REPLAYING;
                                        animFrame = g_golferPool[golferIndex].animTick;
                                    } else {
                                        animFrame = ++g_golferPool[golferIndex].animTick;
                                    }
                                }
                                frameCount = g_flicBankFrameLens[animBank + animVariant];
                                if (animFrame >= frameCount) {
                                    animFrame = frameCount - 1;
                                    g_golferPool[golferIndex].animTick = animFrame;
                                }
                                int impactLimit = ((animBank != ANIM_PUTT) + IMPACT_EIGHTHS) * frameCount;
                                if ((animFrame < impactLimit / 8)
                                    && (g_golferPool[golferIndex].activity == SWING_DOWNSWING)) {
                                    g_golferPool[golferIndex].shotStep -= 1;
                                }
                                if ((animFrame < frameCount - 1)
                                    && (g_golferPool[golferIndex].activity == SWING_IMPACT)) {
                                    g_golferPool[golferIndex].shotStep -= 1;
                                }
                            }
                            if (g_golferPool[golferIndex].activity == SWING_FOLLOW_THROUGH) {
                                int ballDistance =
                                    Math_Distance2D(g_golferPool[golferIndex].ballX - g_golferPool[golferIndex].x,
                                        g_golferPool[golferIndex].ballY - g_golferPool[golferIndex].y);
                                if (ballDistance
                                    > (((g_golferPool[golferIndex].flags & (GOLFER_LEANS_LEFT | GOLFER_LEANS_RIGHT))
                                                   != 0
                                               ? WATCH_CURVING_BALL_TILES
                                               : WATCH_BALL_TILES)
                                        * TILE_WORLD_SIZE)) {
                                    animBank = ANIM_LOOK_UP;
                                    animFrame = ++g_golferPool[golferIndex].animTick;
                                    g_golferPool[golferIndex].x =
                                        g_golferPool[golferIndex].shotFromX
                                        - g_dirRowStep[g_golferPool[golferIndex].dir & DIRECTION_MASK]
                                              * (((g_golferPool[golferIndex].dir & 1) != 0 ? 3 : 5) << 6);
                                    g_golferPool[golferIndex].y =
                                        g_golferPool[golferIndex].shotFromY
                                        - g_dirColStep[g_golferPool[golferIndex].dir & DIRECTION_MASK]
                                              * (((g_golferPool[golferIndex].dir & 1) != 0 ? 3 : 5) << 6);
                                    if ((g_golferPool[golferIndex].flags & GOLFER_LEANS_LEFT) != 0) {
                                        animBank = ANIM_LEAN_LEFT;
                                        if (animFrame == LEAN_LOOP_LAST) {
                                            animFrame = LEAN_LOOP_FIRST;
                                            g_golferPool[golferIndex].animTick = LEAN_LOOP_FIRST;
                                        }
                                    }
                                    if ((g_golferPool[golferIndex].flags & GOLFER_LEANS_RIGHT) != 0) {
                                        animBank = ANIM_LEAN_RIGHT;
                                        if (animFrame == LEAN_LOOP_LAST) {
                                            animFrame = LEAN_LOOP_FIRST;
                                            g_golferPool[golferIndex].animTick = LEAN_LOOP_FIRST;
                                        }
                                    }
                                    frameCount = g_flicBankFrameLens[animBank + animVariant];
                                    if (animFrame >= frameCount) {
                                        animFrame = frameCount - 1;
                                    }
                                } else {
                                    frameCount = g_flicBankFrameLens[animBank + animVariant];
                                    animFrame = frameCount - 1;
                                }
                            }
                            if (animBank != ANIM_PUTT) {
                                spriteFacing = (spriteFacing + QUARTER_TURN) & DIRECTION_MASK;
                            }
                        } else if (g_golferPool[golferIndex].activity < GOLFER_STANDING) {
                            animBank = 0;
                            if (GOLFER_TIRED_LEVEL < g_golferPool[golferIndex].fatigue) {
                                animBank = ANIM_TIRED_WALK;
                            }
                            if ((g_golferPool[golferIndex].flags & GOLFER_IN_CART) != 0) {
                                animBank = ANIM_CART;
                            }
                            animFrame =
                                ++g_golferPool[golferIndex].animTick % g_flicBankFrameLens[animBank + animVariant];
                            if ((g_golferPool[golferIndex].flags & GOLFER_SLOW_WALK) == 0) {
                                animFrame =
                                    ++g_golferPool[golferIndex].animTick % g_flicBankFrameLens[animBank + animVariant];
                            }
                        } else {
                            if (g_golferPool[golferIndex].activity == GOLFER_STANDING) {
                                animBank = ANIM_STAND;
                                if (g_golferPool[golferIndex].club == CLUB_PUTTER) {
                                    animBank = ANIM_LINE_UP_PUTT;
                                }
                                animFrame =
                                    ++g_golferPool[golferIndex].animTick % g_flicBankFrameLens[animBank + animVariant];
                                int fidgetFrame;
                                if ((((animBank == ANIM_STAND) && (g_golferPool[golferIndex].holeStrokes == 0))
                                        && (fidgetFrame = g_clock + golferIndex * FIDGET_STAGGER & FIDGET_CYCLE_MASK,
                                            fidgetFrame < g_flicBankFrameLens[ANIM_FIDGET + animVariant]))) {
                                    animFrame = fidgetFrame;
                                    animBank = ANIM_FIDGET;
                                    if (g_golferPool[golferIndex].waitTimer == 0) {
                                        g_golferPool[golferIndex].waitTimer = -2;
                                    }
                                }
                                if ((g_golferPool[golferIndex].club == CLUB_PUTTER) && (animFrame == 0)) {
                                    g_golferPool[golferIndex].waitTimer = 0;
                                }
                            }
                            if (g_golferPool[golferIndex].activity == GOLFER_HAPPY_REACTION) {
                                animBank = (g_golferPool[golferIndex].flags & GOLFER_ALT_REACTION) != 0 ? ANIM_SUCCESS
                                                                                                        : ANIM_HAPPY;
                                if ((golferIndex & 1) == 0 || (g_clock & 1U) != 0
                                    || g_golferPool[golferIndex].animTick == 0
                                    || (g_runModeFlags & GAME_FAST_CLOCK) != 0
                                    || g_golferPool[golferIndex].animTick
                                           > g_flicBankFrameLens[animBank + animVariant] / 2) {
                                    animFrame = ++g_golferPool[golferIndex].animTick
                                                % g_flicBankFrameLens[animBank + animVariant];
                                } else {
                                    animFrame = g_golferPool[golferIndex].animTick
                                                % g_flicBankFrameLens[animBank + animVariant];
                                }
                                if (animFrame == 0) {
                                    if (((g_golferPool[golferIndex].flags & GOLFER_REPLAYING) == 0)
                                        && ((golferIndex & 1) == 0)) {
                                        g_golferPool[golferIndex].flags =
                                            g_golferPool[golferIndex].flags | GOLFER_REPLAYING;
                                        g_golferPool[golferIndex].animTick = 0;
                                        g_golferPool[golferIndex].flags ^= GOLFER_ALT_REACTION;
                                    } else {
                                        g_golferPool[golferIndex].waitTimer = 0;
                                    }
                                }
                            }
                            if (g_golferPool[golferIndex].activity == GOLFER_SAD_REACTION) {
                                animBank = (g_golferPool[golferIndex].flags & GOLFER_ALT_REACTION) != 0 ? ANIM_FAILURE
                                                                                                        : ANIM_SAD;
                                if ((golferIndex & 1) == 0 || (g_clock & 1U) != 0
                                    || g_golferPool[golferIndex].animTick == 0
                                    || (g_runModeFlags & GAME_FAST_CLOCK) != 0
                                    || g_golferPool[golferIndex].animTick
                                           > g_flicBankFrameLens[animBank + animVariant] / 2) {
                                    animFrame = ++g_golferPool[golferIndex].animTick
                                                % g_flicBankFrameLens[animBank + animVariant];
                                } else {
                                    animFrame = g_golferPool[golferIndex].animTick
                                                % g_flicBankFrameLens[animBank + animVariant];
                                }
                                if (animFrame == 0) {
                                    if (((g_golferPool[golferIndex].flags & GOLFER_REPLAYING) == 0)
                                        && ((golferIndex & 1) == 0)) {
                                        g_golferPool[golferIndex].flags =
                                            g_golferPool[golferIndex].flags | GOLFER_REPLAYING;
                                        g_golferPool[golferIndex].animTick = 0;
                                        g_golferPool[golferIndex].flags ^= GOLFER_ALT_REACTION;
                                    } else {
                                        g_golferPool[golferIndex].waitTimer = 0;
                                    }
                                }
                            }
                            if (g_golferPool[golferIndex].activity == GOLFER_POINTING) {
                                int frameCount = g_flicBankFrameLens[ANIM_POINT_AT + animVariant];
                                animBank = ANIM_POINT_AT;
                                int pointFrame = ++g_golferPool[golferIndex].animTick % frameCount;
                                animFrame = pointFrame;
                                if (pointFrame == frameCount / 2) {
                                    short rewindSteps = g_rng.NextInt(4);
                                    g_golferPool[golferIndex].animTick += rewindSteps * -2;
                                }
                                if (pointFrame == 0) {
                                    g_golferPool[golferIndex].activity = GOLFER_STANDING;
                                    g_golferPool[golferIndex].waitTimer = pointFrame;
                                }
                            }
                            if (g_golferPool[golferIndex].activity == GOLFER_SEATED) {
                                animBank = ANIM_SEATED;
                                spriteFacing = spriteFacing / 2;
                                animFrame =
                                    ++g_golferPool[golferIndex].animTick % g_flicBankFrameLens[animBank + animVariant];
                                int waitTimer = g_golferPool[golferIndex].waitTimer;
                                if (waitTimer
                                    > SEAT_WAIT_TICKS - g_flicBankFrameLens[ANIM_SITTING_DOWN + animVariant]) {
                                    g_golferPool[golferIndex].animBank = ANIM_SITTING_DOWN;
                                    animFrame = (SEAT_WAIT_TICKS - 1) - g_golferPool[golferIndex].waitTimer;
                                    animBank = ANIM_SITTING_DOWN;
                                    if (animFrame < SEAT_WAIT_TICKS) {
                                        g_golferPool[golferIndex].activity = GOLFER_STANDING;
                                        g_golferPool[golferIndex].x =
                                            (g_golferPool[golferIndex].x & TILE_ORIGIN_MASK) + TILE_WORLD_HALF;
                                        g_golferPool[golferIndex].y =
                                            (g_golferPool[golferIndex].y & TILE_ORIGIN_MASK) + TILE_WORLD_HALF;
                                    }
                                }
                            }
                            if (g_golferPool[golferIndex].activity == GOLFER_SITTING_DOWN) {
                                animBank = ANIM_SITTING_DOWN;
                                animFrame = ++g_golferPool[golferIndex].animTick
                                            % g_flicBankFrameLens[ANIM_SITTING_DOWN + animVariant];
                                spriteFacing = spriteFacing / 2;
                                if (animFrame == 0) {
                                    animFrame = g_flicBankFrameLens[ANIM_SITTING_DOWN + animVariant] - 1;
                                    g_golferPool[golferIndex].activity = GOLFER_SEATED;
                                    g_golferPool[golferIndex].animTick = 0;
                                }
                            }
                            if (g_golferPool[golferIndex].activity == GOLFER_WASHING_BALL) {
                                animBank = ANIM_HANDSHAKE;
                                animFrame = ++g_golferPool[golferIndex].animTick
                                            % g_flicBankFrameLens[ANIM_HANDSHAKE + animVariant];
                            }
                            if (g_golferPool[golferIndex].activity == GOLFER_ADDRESSING) {
                                animBank = g_golferPool[golferIndex].club > LAST_FULL_SWING_CLUB ? ANIM_PITCH_ADDRESS
                                                                                                 : ANIM_ADDRESS;
                                if (g_golferPool[golferIndex].club == CLUB_PUTTER) {
                                    animBank = ANIM_PUTT_ADDRESS;
                                }
                                int addressFrames = g_flicBankFrameLens[animBank + animVariant];
                                if (addressFrames != 0) {
                                    animFrame = ++g_golferPool[golferIndex].animTick % addressFrames;
                                    if (animFrame == addressFrames - 1) {
                                        g_golferPool[golferIndex].shotStep = SHOT_STEP_ADDRESSED;
                                    }
                                } else {
                                    animBank = ANIM_STAND;
                                    g_golferPool[golferIndex].shotStep = SHOT_STEP_ADDRESSED;
                                }
                            }
                        }
                        int savedFrame = frameAtStart;
                        if ((g_runModeFlags & GAME_PAUSED) != 0) {
                            g_golferPool[golferIndex].animTick = savedFrame;
                        }
                        if (animBank != g_golferPool[golferIndex].animBank) {
                            g_golferPool[golferIndex].animTick = 0;
                            g_golferPool[golferIndex].animBank = animBank;
                            animFrame = 0;
                            if ((g_golferPool[golferIndex].activity != GOLFER_HAPPY_REACTION)
                                && (g_golferPool[golferIndex].activity != GOLFER_SAD_REACTION)) {
                                g_golferPool[golferIndex].flags &= ~GOLFER_REPLAYING;
                            }
                        }
                        int golferScreenY;
                        int golferScreenX;
                        int golferOnScreen = WorldToScreen(g_golferPool[golferIndex].x, g_golferPool[golferIndex].y,
                            &golferScreenX, &golferScreenY, 0);
                        if (golferOnScreen != 0) {
                            g_golferPool[golferIndex].screenX = golferScreenX;
                            g_golferPool[golferIndex].screenY = golferScreenY;
                            if ((((g_golferPool[golferIndex].talkTimer != 0)
                                     && (g_golferPool[golferIndex].talkTimer <= TALK_SHOW_TICKS))
                                    && (g_golferPool[golferIndex].msgLine == '2'))
                                && (((g_golferPool[golferIndex].storyChapter == 1
                                         && ((g_golferPool[golferIndex].flags & GOLFER_STORY_LEAD) == 0))
                                     && ((g_knownEventsHighMask & SEEN_STORY_START) == 0)))) {
                                g_knownEventsHighMask |= SEEN_STORY_START;
                                g_printBuffer[0] = '\0';
                                AppendGolferName(golferIndex, 0);
                                strcat(g_printBuffer, " and ");
                                AppendGolferName(golferIndex ^ 1, 0);
                                strcat(g_printBuffer, " are beginning a story called '");
                                AppendThemeText(g_golferPool[golferIndex].storyId, -1, 0, 0);
                                strcat(g_printBuffer,
                                    ".'  Depending on how they like your course this story may come to a happy ending - or it may not. ");
                                strcat(g_printBuffer, "Either way you'll get to know your golfers a little better.");
                                ShowStoryMessage(COLOR_BLUE, 1, -2);
                            }
                            if (animFrame < 0) {
                                animFrame = 0;
                            }
                            animBank += animVariant;
                            int golferScale = SPRITE_FULL_SCALE;
                            g_curSprite =
                                GetFlicBankSprite(animBank, animFrame % g_flicBankFrameLens[animBank], spriteFacing);
                            if ((g_golferTypeArray[g_golferPool[golferIndex].typeIdx].fixedKind & TYPE_JUNIOR) != 0) {
                                golferScale = SPRITE_SMALL_SCALE;
                            }
                            AddSpriteToRenderQueue(golferScreenX, golferScreenY, golferScreenY, GOLFER_ORIGIN_X,
                                GOLFER_ORIGIN_Y, g_curSprite, golferScale, golferIndex | GOLFER_ID_FLAG);
                        }
                        if (((g_golferPool[golferIndex].ballX == 0)
                                || ((g_golferPool[golferIndex].flags & GOLFER_AFTER_PENALTY) != 0))
                            || (WorldToScreen(g_golferPool[golferIndex].ballX, g_golferPool[golferIndex].ballY,
                                    &golferScreenX, &golferScreenY, 0)
                                == 0)) {
                            g_trailSavedPlotPosX[golferIndex] = -1;
                        } else {
                            if ((g_runModeFlags & GAME_DEBUG_OVERLAY) != 0) {
                                g_tinyBallPicture.BlitScaled(g_pMenu, 100, 100, 0);
                            }
                            if ((((g_golferPool[golferIndex].flags & GOLFER_BALL_MOVING) == 0)
                                    || (g_golferPool[golferIndex].ballSpeed <= BALL_REST_SPEED))
                                && (g_golferPool[golferIndex].ballZ == 0)
                                && (0 < g_squareTypeTable[GetTileAtPosition(g_golferPool[golferIndex].ballX,
                                                              g_golferPool[golferIndex].ballY)]
                                        .lieGrade)) {
                                g_simScreen.line(golferScreenX, golferScreenY, golferScreenX + 1, golferScreenY,
                                    COLOR_WHITE);
                            } else {
                                AddSpriteToRenderQueue(golferScreenX, golferScreenY + 1, golferScreenY - 1, 1, 1,
                                    &g_groundShadowPixel,
                                    g_cameraZoom > DEFAULT_ZOOM ? SPRITE_HALF_SCALE : SPRITE_FULL_SCALE, -1);
                                if (g_golferPool[golferIndex].ballZ != 0) {
                                    WorldToScreen(g_golferPool[golferIndex].ballX, g_golferPool[golferIndex].ballY,
                                        &golferScreenX, &golferScreenY, -1);
                                }
                                AddSpriteToRenderQueue(golferScreenX,
                                    golferScreenY
                                        - (g_elevStep * g_cameraZoom * g_golferPool[golferIndex].ballZ) / 5 / 16,
                                    golferScreenY, 1, 1, &g_tinyBallPicture,
                                    g_zoomView != 0 ? SPRITE_HALF_SCALE : SPRITE_FULL_SCALE, -1);
                                g_visibleBallX[g_renderedBallCount] = golferScreenX;
                                int ballScreenY =
                                    golferScreenY
                                    - (g_elevStep * g_cameraZoom * g_golferPool[golferIndex].ballZ) / 5 / 16;
                                g_visibleBallY[g_renderedBallCount] = ballScreenY;
                                g_renderedBallCount++;
                                if (((g_golferPool[golferIndex].flags & GOLFER_BALL_TRAIL) != 0)
                                    && (g_trailSavedPlotPosX[golferIndex] != -1)) {
                                    DrawTerrainLineWrapper(golferScreenX, ballScreenY,
                                        g_trailSavedPlotPosX[golferIndex], g_trailSavedPlotPosY[golferIndex],
                                        COLOR_PALE_PINK, LINE_WIDTH_THIN, LINE_ALPHA);
                                }
                            }
                            g_trailSavedPlotPosX[golferIndex] = golferScreenX;
                            g_trailSavedPlotPosY[golferIndex] =
                                golferScreenY - (g_elevStep * g_cameraZoom * g_golferPool[golferIndex].ballZ) / 5 / 16;
                        }
                    }
                    golferIndex++;
                } while (golferIndex < GOLFER_COUNT);
                int divotIdx = 0;
                do {
                    Divot *divot = &g_tornDivots[divotIdx];
                    if (divot->x != -1) {
                        int divotZ = 0;
                        int divotScreenY;
                        int divotScreenX;
                        if ((g_clock - divot->spawnTick < DIVOT_FLIGHT_TICKS)
                            && WorldToScreen(divot->x, divot->y, &divotScreenX, &divotScreenY, 0) != 0) {
                            g_simScreen.fill(divotScreenX, divotScreenY, 2, 1, -COLOR_BLACK);
                            g_simScreen.fill(divotScreenX, divotScreenY - (divotZ = divot->z) * g_cameraZoom / 16,
                                g_tornDivots[divotIdx].z != 0 ? g_rng.NextInt(3) + 1 : 2, (divot->z != 0) + 1,
                                COLOR_DEEP_GREEN);
                        }
                        if (WorldToScreen(divot->originX, divot->originY, &divotScreenX, &divotScreenY, 0) != 0) {
                            g_simScreen.fill(divotScreenX - 1, divotScreenY, 3, 1, COLOR_DARK_GREY);
                        }
                    }
                    divotIdx++;
                } while (divotIdx < DIVOT_COUNT);
                RenderEmployees();
                int splashFrame = g_waterSprayFxFrame;
                if (g_waterSprayFxFrame != -1) {
                    if (g_waterSprayFxFrame < g_flicBankFrameLens[BANK_SPLASH]) {
                        int splashScreenY;
                        int splashScreenX;
                        int onScreen = WorldToScreen(g_waterSprayFxPositionX, g_waterSprayFxPositionY, &splashScreenX,
                            &splashScreenY, 0);
                        if (onScreen != 0) {
                            g_curSprite = GetFlicBankSprite(BANK_SPLASH, splashFrame, 0);
                            AddSpriteToRenderQueue(splashScreenX, splashScreenY, splashScreenY, SPRITE_ORIGIN_SMALL,
                                SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE, PAL_WATERSPRAY);
                        }
                        g_waterSprayFxFrame++;
                    } else {
                        g_waterSprayFxFrame = -1;
                    }
                }
                if (((g_reportScreenId == 1) && (g_messageSpeaker != -1))
                    && (g_golferPool[g_messageSpeaker].holeIdx != 0)) {
                    g_selectHaloSprite.BlitWithScaleOffset(g_pMenu,
                        g_golferPool[g_messageSpeaker].screenX - RING_OFFSET_X,
                        g_golferPool[g_messageSpeaker].screenY - RING_OFFSET_Y, g_shadeTable, 0);
                }
                if ((0 < g_displayFocus) && (g_golferPool[g_displayFocus - 1].holeIdx != 0)) {
                    DrawCircleOnTerrain(g_golferPool[g_displayFocus - 1].x, g_golferPool[g_displayFocus - 1].y,
                        INFO_RING_RADIUS, COLOR_BRIGHT_GREEN);
                }
                UpdateAllHomeVIPs();
                if ((g_watchedPlayerIdx == -1) && ((g_runModeFlags & GAME_TOURNAMENT) == 0)) {
                    g_golferPool[STAND_IN_GOLFER].typeIdx = 0;
                    g_golferPool[STAND_IN_GOLFER].ageFlags = g_golferTypeArray[0].ageBits;
                    g_golferPool[STAND_IN_GOLFER].talents = SKILLS_ALL;
                    g_golferPool[STAND_IN_GOLFER].talentLevel = STAND_IN_POWER_TIER;
                    g_golferPool[STAND_IN_GOLFER].persona = PERSONA_PLAYER;
                    g_golferPool[STAND_IN_GOLFER].activity = GOLFER_WALKING;
                }
                if ((g_toolbarView == TOOLBAR_SHOT) && (0 < g_golferPool[g_watchedPlayerIdx].screenX)) {
                    int followed = g_watchedPlayerIdx;
                    int moodBubble = Clamp(g_golferPool[followed].attitude + 2, 0, 4);
                    signed char attitude = g_golferPool[followed].attitude;
                    if (abs(attitude < 3) || (((abs(attitude) != 3 ? 2 : 4) & g_clock) != 0)) {
                        g_bubblePieces[moodBubble].BlitScaled(g_pMenu, g_golferPool[followed].screenX + 2,
                            g_golferPool[followed].screenY + g_cameraZoom * -LABEL_LIFT_PER_ZOOM, 0);
                    }
                }
                ProcessRenderQueue();
                ProcessPendingScreenshot();
                if ((g_toolbarView != TOOLBAR_BUILD_COURSE) || (g_buildToolbarShown != 0)) {
                    g_paintTerrain = -1;
                }
                if (((g_toolbarView != TOOLBAR_BUILD_COURSE) || (g_buildToolbarShown == 0))
                    && ((g_toolbarView != TOOLBAR_BUILDINGS) || (g_elevToolbarShown != 0))) {
                    g_selectedBuildingKind = -1;
                }
                if ((g_selectedBuildingKind != -1) || (g_paintTerrain != -1)) {
                    g_runModeFlags &= ~GAME_DEMOLISH_TOOL;
                    if ((g_selectedBuildingKind != -1) || (g_paintTerrain != -1)) {
                        g_cameraZoom = DEFAULT_ZOOM;
                    }
                }
                if ((g_nextOpenHoleIndex >= CLUBHOUSE_HOLE) && (g_paintTerrain == 0)) {
                    g_paintTerrain = -1;
                }
                g_pMenu->SetFontsForStyles(g_cameraZoom >= DEFAULT_ZOOM ? &g_smallManualFont : &g_boldArialFont, 0, 0,
                    0);
                for (int labelSlot = 0; labelSlot < BUILDING_SLOT_COUNT; labelSlot++) {
                    if (g_buildingTable[labelSlot].kind >= SCENERY_HOME_SITE) {
                        int kind = g_buildingTable[labelSlot].kind;
                        int halfFootprint = (g_facilityTypes[kind].side * TILE_WORLD_SIZE - TILE_WORLD_SIZE) / 2;
                        int buildingScreenY;
                        int buildingScreenX;
                        if (WorldToScreen(g_buildingTable[labelSlot].row * TILE_WORLD_SIZE + TILE_WORLD_HALF
                                              + halfFootprint,
                                g_buildingTable[labelSlot].col * TILE_WORLD_SIZE + TILE_WORLD_HALF + halfFootprint,
                                &buildingScreenX, &buildingScreenY, 0)
                            != 0) {
                            if ((kind == SCENERY_HOME_SITE) && (0 < g_buildingTable[labelSlot].resident)) {
                                strcpy(g_printBuffer,
                                    g_themeCelebsEntries[g_buildingTable[labelSlot].resident - 1].name);
                                strcat(g_printBuffer, "'s");
                                if ((g_cameraZoom == DEFAULT_ZOOM)
                                    && ((((labelSlot * LABEL_STAGGER) + g_clock) & HOME_LABEL_BLINK_MASK) == 0)) {
                                    DrawTextCenteredWithShadow(g_printBuffer, buildingScreenX,
                                        buildingScreenY - HOME_NAME_DY, COLOR_PINK);
                                    DrawTextCenteredWithShadow("Home", buildingScreenX,
                                        buildingScreenY + (DEFAULT_ZOOM - g_cameraZoom) * HOME_LINE_DY_PER_ZOOM,
                                        COLOR_PINK);
                                }
                            } else {
                                g_printBuffer[0] = '\0';
                                if ((kind >= SCENERY_PUTTING_GREEN)
                                    && ((kind != SCENERY_SNACK_BAR && (kind != SCENERY_HOME_SITE)))) {
                                    if (g_buildingLevelPerKind[kind] == LEVEL_UPGRADED) {
                                        strcpy(g_printBuffer, "Upgraded ");
                                    }
                                    if (g_buildingLevelPerKind[kind] == LEVEL_DELUXE) {
                                        strcpy(g_printBuffer, "Deluxe ");
                                    }
                                }
                                strcat(g_printBuffer, g_facilityTypes[kind].name);
                                if ((kind >= SCENERY_PUTTING_GREEN)
                                    && ((g_buildingTable[labelSlot].flags & SLOT_REACHABLE) == 0)) {
                                    DrawTextCenteredWithShadow(g_printBuffer, buildingScreenX,
                                        buildingScreenY - BUILDING_LABEL_DY,
                                        ((labelSlot + g_clock) & UNREACHED_BLINK_BIT) != 0 ? COLOR_RED
                                                                                           : COLOR_DARK_RED);
                                }
                            }
                        }
                    }
                }
                if (((g_runModeFlags & GAME_ELEVATION_TOOL) != 0) && (g_gameElevPanelChoice != GRID_MODE_ANALYZE)) {
                    int tileWorldY = 0;
                    for (int tileX = g_pointerPosX - 1;
                        tileX <= (g_gameElevPanelChoice == GRID_MODE_SQUARE ? 2 : 1) + g_pointerPosX; tileX++) {
                        for (int tileY = g_pointerPosY - (g_gameElevPanelChoice == GRID_MODE_SQUARE);
                            tileY <= g_pointerPosY + 2; tileY++) {
                            tileWorldY = tileY << TILE_WORLD_SHIFT;
                            int gridScreenY;
                            int gridScreenX;
                            if (WorldToScreen(tileX << TILE_WORLD_SHIFT, tileWorldY, &gridScreenX, &gridScreenY, 0)
                                != 0) {
                                Font *labelFont;
                                if ((tileX == g_pointerPosX) && (tileY == g_pointerPosY + 1)) {
                                    labelFont = &g_smallManualFont;
                                } else {
                                    labelFont = &g_boldArialFont;
                                }
                                g_pMenu->SetFontsForStyles(labelFont, 0, 0, 0);
                                if (((g_gameElevPanelChoice == GRID_MODE_SQUARE)
                                        && ((tileX == g_pointerPosX || (tileX == g_pointerPosX + 1))))
                                    && ((tileY == g_pointerPosY || (tileY == g_pointerPosY + 1)))) {
                                    g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
                                }
                                int heightAboveSea = GetTileElevation(tileX, tileY - 1, 0) - ELEV_SEA_LEVEL;
                                strcpy(g_printBuffer, heightAboveSea != 0 ? "+" : g_empty);
                                strcat(g_printBuffer, itoa(heightAboveSea, g_numberBuf, 10));
                                DrawTextCenteredWithShadow(g_printBuffer, gridScreenX, gridScreenY - 0xf, COLOR_WHITE);
                            }
                            if ((g_gameElevPanelChoice == GRID_MODE_VERTEX)
                                || (g_gameElevPanelChoice == GRID_MODE_AREA)) {
                                if ((tileX == g_pointerPosX) && (tileY == g_pointerPosY + 1)) {
                                    g_simScreen.fill(gridScreenX - 2, gridScreenY - 1, 5, 3, COLOR_MAGENTA);
                                } else if (g_gameElevPanelChoice == GRID_MODE_AREA) {
                                    g_simScreen.fill(gridScreenX - 1, gridScreenY - 1, 3, 2, COLOR_MAGENTA);
                                }
                            }
                            if (g_gameElevPanelChoice == GRID_MODE_SQUARE) {
                                int edgeStartY;
                                int edgeStartX;
                                if ((tileX == g_pointerPosX) && (tileY == g_pointerPosY + 1)) {
                                    WorldToScreen(tileX << TILE_WORLD_SHIFT, tileY * TILE_WORLD_SIZE - 1, &gridScreenX,
                                        &gridScreenY, 0);
                                    if (WorldToScreen(tileX << TILE_WORLD_SHIFT,
                                            tileY * TILE_WORLD_SIZE - TILE_WORLD_SIZE, &edgeStartX, &edgeStartY, 0)
                                        != 0) {
                                        DrawTerrainLine(edgeStartX, edgeStartY, gridScreenX, gridScreenY, COLOR_MAGENTA,
                                            GRID_EDGE_WIDTH, LINE_ALPHA);
                                    }
                                    if (WorldToScreen(tileX * TILE_WORLD_SIZE + (TILE_WORLD_SIZE - 1),
                                            tileY * TILE_WORLD_SIZE - 1, &edgeStartX, &edgeStartY, 0)
                                        != 0) {
                                        DrawTerrainLine(edgeStartX, edgeStartY, gridScreenX, gridScreenY, COLOR_MAGENTA,
                                            GRID_EDGE_WIDTH, LINE_ALPHA);
                                    }
                                }
                                if ((tileX == g_pointerPosX + 1) && (tileY == g_pointerPosY)) {
                                    if (WorldToScreen(tileX * TILE_WORLD_SIZE - 1,
                                            tileY * TILE_WORLD_SIZE + (TILE_WORLD_SIZE - 1), &edgeStartX, &edgeStartY,
                                            0)
                                        != 0) {
                                        DrawTerrainLine(edgeStartX, edgeStartY, gridScreenX, gridScreenY, COLOR_MAGENTA,
                                            GRID_EDGE_WIDTH, LINE_ALPHA);
                                    }
                                    if (WorldToScreen(tileX * TILE_WORLD_SIZE - TILE_WORLD_SIZE, tileWorldY,
                                            &edgeStartX, &edgeStartY, 0)
                                        != 0) {
                                        DrawTerrainLine(edgeStartX, edgeStartY, gridScreenX, gridScreenY, COLOR_MAGENTA,
                                            GRID_EDGE_WIDTH, LINE_ALPHA);
                                    }
                                }
                            }
                        }
                    }
                }
                if (((((g_level == 0) && (g_clock < TICKS_PER_YEAR)) && (g_staffSlots[0].flags != 0))
                        && ((g_staffSlots[0].job == JOB_PLAYER_AVATAR && (g_staffSlots[0].talkTimer == 0))))
                    && (WorldToScreen(g_staffSlots[0].posX, g_staffSlots[0].posY, &avatarScreenX, &avatarScreenY, 0)
                        != 0)) {
                    g_printBuffer[0] = '\0';
                    if (g_allHolesArray[1].par == 0) {
                        strcpy(g_printBuffer, "Press 'h' to open the hole.");
                        int greenX = g_allHolesArray[1].aimX;
                        if (greenX == 0) {
                            strcpy(g_printBuffer, "Now we need to build a green.");
                        }
                        if (g_allHolesArray[1].teeX == 0) {
                            strcpy(g_printBuffer, "First we need to build a tee.");
                        }
                        int hintClock = g_clock;
                        if ((g_clock & HINT_SWAP_BIT) == 0) {
                            strcpy(g_printBuffer, "Let's build our first golf hole.");
                            if ((greenX != 0) && (g_allHolesArray[1].teeX != 0)) {
                                strcpy(g_printBuffer, "We might add some fairway or sand traps.");
                            }
                        } else if (g_toolbarView != TOOLBAR_BUILD_COURSE) {
                            strcpy(g_printBuffer, "Click on the big 'Build Course' button.");
                        }
                        if ((hintClock & HINT_GAP_MASK) == 0) {
                            g_printBuffer[0] = '\0';
                        }
                    } else {
                        if (g_allHolesArray[2].par == 0) {
                            strcpy(g_printBuffer, "We've built one hole, let's build more!");
                            if ((g_golferPool[1].holeIdx == 1) && (g_golferPool[1].holeStrokes == 0)) {
                                strcpy(g_printBuffer, "Here come some golfers!");
                            }
                        }
                        if (g_allHolesArray[3].par != 0) {
                            int nearestHole = FindNearestHole(g_staffSlots[0].posX >> TILE_WORLD_SHIFT,
                                g_staffSlots[0].posY >> TILE_WORLD_SHIFT);
                            int fun =
                                (g_allHolesArray[nearestHole].moodSum * 100)
                                / (g_allHolesArray[nearestHole].shots / 2 + 4 + g_allHolesArray[nearestHole].rounds);
                            strcpy(g_printBuffer, "This hole has a ");
                            strcat(g_printBuffer, itoa(fun, g_numberBuf, 10));
                            strcat(g_printBuffer, "% fun rating.");
                            if (g_skillMasks[0] == 0) {
                                strcpy(g_printBuffer, "I'm ready for a practice round.");
                            } else {
                                if ((g_runModeFlags & GAME_TOURNAMENT_OFFERED) != 0) {
                                    strcpy(g_printBuffer, "I'm ready to play a tournament.");
                                }
                            }
                        }
                        int hintClock = g_clock;
                        if ((hintClock & HINT_TOOLBAR_BIT) != 0) {
                            switch (g_toolbarView) {
                            case TOOLBAR_BUILD_COURSE:
                                if (g_buildToolbarShown != 0) {
                                    if ((hintClock & HINT_TOOLBAR_SWAP_BIT) != 0) {
                                        strcpy(g_printBuffer, "Some flowers and benches will cheer our golfers.");
                                    } else {
                                        strcpy(g_printBuffer, "Adding paths will help speed up play.");
                                    }
                                } else {
                                    strcpy(g_printBuffer, "I like to make each hole different.");
                                }
                                break;
                            case TOOLBAR_BUILDINGS:
                                if (g_elevToolbarShown != 0) {
                                    strcpy(g_printBuffer, "Slopes affect the bounce and roll of the ball.");
                                } else if (g_level != 0) {
                                    strcpy(g_printBuffer, "Buildings need a path to the clubhouse.");
                                } else {
                                    strcpy(g_printBuffer, "Buildings are one way to improve our course.");
                                }
                                break;
                            case TOOLBAR_GOLFERS:
                                if (g_staffSlots[1].flags == 0) {
                                    strcpy(g_printBuffer,
                                        "If you hire employees I won't have to do everything myself!");
                                    break;
                                }
                                g_printBuffer[0] = '\0';
                                BuildEmployeeName(1);
                                strcat(g_printBuffer, " is doing a great job.");
                                hintClock = g_clock;
                                break;
                            }
                        }
                        if (((((hintClock & HINT_SWAP_BIT) != 0) || (TICKS_PER_YEAR < hintClock)) || (g_level != 0))
                            || (g_watchedPlayerIdx != -1)) {
                            g_printBuffer[0] = '\0';
                        }
                    }
                    if (g_printBuffer[0] != '\0') {
                        int lift = g_cameraZoom * LABEL_LIFT_PER_ZOOM;
                        g_pMenu->SetFontsForStyles(g_cameraZoom > DEFAULT_ZOOM ? &g_smallManualFont : &g_boldArialFont,
                            0, 0, 0);
                        int width = g_pMenu->text_width(g_printBuffer) + BUBBLE_PADDING;
                        DrawTerrainLine(avatarScreenX - width / 2, avatarScreenY - lift + BUBBLE_HEIGHT / 2,
                            width / 2 + avatarScreenX, avatarScreenY - lift + BUBBLE_HEIGHT / 2, COLOR_BLACK,
                            BUBBLE_HEIGHT, BUBBLE_ALPHA);
                        g_talkBubblePointSpr.BlitScaled(g_pMenu, avatarScreenX + BUBBLE_TAIL_DX,
                            avatarScreenY - lift + BUBBLE_HEIGHT, g_colorLut, 1, 1, 0);
                        DrawTextCenteredAtY(g_printBuffer, avatarScreenX, avatarScreenY - lift, COLOR_WHITE);
                    }
                }
                for (golferIndex = 0; golferIndex < GOLFER_COUNT; golferIndex++) {
                    int speechY;
                    if (g_golferPool[golferIndex].holeIdx != 0
                        && WorldToScreen(g_golferPool[golferIndex].x, g_golferPool[golferIndex].y, &speechX, &speechY,
                               0)
                               != 0) {
                        int lift = g_cameraZoom * LABEL_LIFT_PER_ZOOM;
                        if (g_zoomView == 0 && g_messageSpeaker != golferIndex && g_reportScreenId == 0
                            && g_paintTerrain == -1 && g_selectedBuildingKind == -1 && g_cameraZoom == DEFAULT_ZOOM
                            && ((g_golferPool[golferIndex].activity == GOLFER_HAPPY_REACTION
                                    && (g_golferPool[golferIndex].msgLine != 1 || g_clock < TICKS_PER_YEAR))
                                || g_golferPool[golferIndex].activity == GOLFER_SAD_REACTION)
                            && g_toolbarView != TOOLBAR_SHOT && CLOSEUP_MARGIN < g_golferPool[golferIndex].screenX
                            && CLOSEUP_MARGIN < g_golferPool[golferIndex].screenY
                            && g_golferPool[golferIndex].screenX < CLOSEUP_MAX_X
                            && g_golferPool[golferIndex].screenY < CLOSEUP_MAX_Y
                            && (g_golferPool[golferIndex].persona != 0 || golferIndex == 0)) {
                            int closeupSrcX =
                                Clamp(g_golferPool[golferIndex].screenX, CLOSEUP_SRC_MARGIN, CLOSEUP_SRC_MAX_X);
                            int closeupSrcY =
                                Clamp(g_golferPool[golferIndex].screenY - 10, CLOSEUP_SRC_MARGIN, CLOSEUP_SRC_MAX_Y);
                            int closeupX = Clamp(g_golferPool[golferIndex].screenX, CLOSEUP_MARGIN, CLOSEUP_MAX_X);
                            int closeupY = Clamp(g_golferPool[golferIndex].screenY - 10, CLOSEUP_MARGIN, CLOSEUP_MAX_Y);
                            g_simScreen.BlitSurfaceRegion(&g_simScreen, closeupSrcX - CLOSEUP_SRC_MARGIN,
                                closeupSrcY - CLOSEUP_SRC_MARGIN, CLOSEUP_SRC_W, CLOSEUP_SRC_H,
                                closeupX - CLOSEUP_MARGIN, closeupY - CLOSEUP_MARGIN, CLOSEUP_W, CLOSEUP_H);
                            DrawTerrainLineWrapper(closeupX - (CLOSEUP_MARGIN - 1),
                                closeupY + (CLOSEUP_H - CLOSEUP_MARGIN), closeupX + (CLOSEUP_MARGIN + 1),
                                closeupY + (CLOSEUP_H - CLOSEUP_MARGIN), COLOR_BLACK, LINE_WIDTH_THIN, LINE_ALPHA);
                            DrawTerrainLineWrapper(closeupX + CLOSEUP_MARGIN, closeupY - (CLOSEUP_MARGIN - 1),
                                closeupX + CLOSEUP_MARGIN, closeupY + (CLOSEUP_H - CLOSEUP_MARGIN), COLOR_BLACK,
                                LINE_WIDTH_THIN, LINE_ALPHA);
                            lift += 0x10;
                        }
                        if (g_golferPool[golferIndex].talkTimer != 0
                            && g_golferPool[golferIndex].talkTimer <= TALK_SHOW_TICKS && TEXT_MIN_ZOOM < g_cameraZoom) {
                            g_printBuffer[0] = '\0';
                            BuildGolferDialogue(g_golferPool[golferIndex].msgLine & MSG_LINE_MASK,
                                g_golferPool[golferIndex].msgCode & MSG_CODE_MASK, g_golferPool[golferIndex].msgStamp,
                                (g_golferPool[golferIndex].msgLine & MSG_LINE_PARTNER) != 0
                                    ? g_golferPool[golferIndex].partner
                                    : golferIndex);
                            int speechColor =
                                g_golferPool[golferIndex].talkTimer < TALK_DIM_BELOW ? COLOR_LIGHT_GREY : COLOR_WHITE;
                            if ((g_golferPool[golferIndex].msgLine & MSG_LINE_PARTNER) == 0
                                && g_golferPool[golferIndex].msgLine
                                       == (g_golferPool[golferIndex].slotKind[0] & MSG_LINE_MASK)) {
                                if ((g_golferPool[golferIndex].slotParams[0] & MOOD_MASK) == MOOD_PLEASED) {
                                    speechColor = g_golferPool[golferIndex].talkTimer < TALK_DIM_BELOW
                                                      ? COLOR_GREEN
                                                      : COLOR_LIGHT_GREEN;
                                }
                                if ((g_golferPool[golferIndex].slotParams[0] & MOOD_MASK) == MOOD_MASK) {
                                    speechColor = g_golferPool[golferIndex].talkTimer < TALK_DIM_BELOW ? COLOR_DARK_RED
                                                                                                       : COLOR_RED;
                                }
                            }
                            g_pMenu->SetFontsForStyles(
                                g_cameraZoom > DEFAULT_ZOOM ? &g_smallManualFont : &g_boldArialFont, 0, 0, 0);
                            int bubbleWidth = g_pMenu->text_width(g_printBuffer) + BUBBLE_PADDING;
                            if (g_golferPool[g_golferPool[golferIndex].partner].talkTimer != 0
                                && abs(g_golferPool[golferIndex].screenY
                                       - g_golferPool[g_golferPool[golferIndex].partner].screenY)
                                       <= 0xc) {
                                if (g_golferPool[golferIndex].screenY
                                        > g_golferPool[g_golferPool[golferIndex].partner].screenY
                                    || (g_golferPool[golferIndex].screenY
                                            == g_golferPool[g_golferPool[golferIndex].partner].screenY
                                        && (golferIndex & 1) != 0)) {
                                    lift -= 4;
                                } else {
                                    lift += 4;
                                }
                            }
                            if ((g_golferPool[golferIndex].msgLine & MSG_LINE_MASK) == LINE_STORY) {
                                lift = (((g_golferPool[golferIndex].flags & GOLFER_STORY_LEAD) != 0
                                                ? STORY_BUBBLE_STAGGER
                                                : -STORY_BUBBLE_STAGGER)
                                           - (g_golferPool[g_golferPool[golferIndex].partner].screenY
                                                 - g_golferPool[golferIndex].screenY)
                                                 / 2)
                                       + g_cameraZoom * LABEL_LIFT_PER_ZOOM;
                                int headIndex = g_golferTypeArray[g_golferPool[golferIndex].typeIdx].head;
                                if ((g_golferPool[golferIndex].flags & GOLFER_STORY_LEAD) != 0) {
                                    speechX -= PORTRAIT_SHIFT_X;
                                    g_popupEdgeBmps[PANEL_PORTRAIT_FRAME].Draw(g_pMenu,
                                        (speechX - bubbleWidth / 2) - PORTRAIT_OFFSET_X,
                                        (speechY - lift) - PORTRAIT_OFFSET_Y, 1, 2, 0);
                                    g_playerHeadBmpArray[(IsGolferMale(golferIndex) * HEADS_PER_SEX + headIndex)
                                                             * HEAD_VARIANTS
                                                         + GetGolferMoodIndex(golferIndex)]
                                        .Draw(g_pMenu, (speechX - bubbleWidth / 2) - PORTRAIT_OFFSET_X,
                                            (speechY - lift) - PORTRAIT_OFFSET_Y, 1, 2, 0);
                                } else {
                                    speechX += PORTRAIT_SHIFT_X;
                                    g_popupEdgeBmps[PANEL_PORTRAIT_FRAME].Draw(g_pMenu,
                                        bubbleWidth / 2 - PORTRAIT_SHIFT_X + speechX,
                                        (speechY - lift) - PORTRAIT_OFFSET_Y, 1, 2, 0);
                                    g_playerHeadBmpArray[(IsGolferMale(golferIndex) * HEADS_PER_SEX + headIndex)
                                                             * HEAD_VARIANTS
                                                         + GetGolferMoodIndex(golferIndex)]
                                        .Draw(g_pMenu, bubbleWidth / 2 + PORTRAIT_MIRROR_X + speechX,
                                            (speechY - lift) - PORTRAIT_OFFSET_Y, -1, 1, 2, 0);
                                }
                                g_simScreen.fill(speechX - bubbleWidth / 2, speechY - lift, bubbleWidth,
                                    NAME_BAR_HEIGHT, COLOR_DARK_PURPLE);
                            } else if ((g_configBitMask & PREF_SHOW_THOUGHTS) != 0) {
                                DrawTerrainLine(speechX - bubbleWidth / 2, (speechY - lift) + BUBBLE_HEIGHT / 2,
                                    bubbleWidth / 2 + speechX, (speechY - lift) + BUBBLE_HEIGHT / 2, COLOR_BLACK,
                                    BUBBLE_HEIGHT, BUBBLE_ALPHA);
                                g_talkBubblePointSpr.BlitScaled(g_pMenu, speechX + BUBBLE_TAIL_DX,
                                    (speechY - lift) + BUBBLE_HEIGHT, g_colorLut, 1, 1, 0);
                            } else {
                                g_printBuffer[0] = '\0';
                            }
                            DrawTextCenteredAtY(g_printBuffer, speechX, speechY - lift, speechColor);
                            speechY += 8;
                            if (g_golferPool[golferIndex].talkTimer == TALK_DIM_BELOW
                                && (g_golferPool[golferIndex].slotParams[0] & MOOD_MASK) == MOOD_MASK
                                && InLandmarkInfluence(g_golferPool[golferIndex].x, g_golferPool[golferIndex].y, 1)
                                       != 0) {
                                SetGolferAction(golferIndex, ACTION_CALMED_BY_SCENERY,
                                    g_relaxingBuildingId | GOLFER_ID_FLAG);
                            }
                        }
                    }
                }
                int complaintOffset = 0;
                int staffIndex = 0;
                do {
                    int staffSpeechY;
                    int staffSpeechX;
                    if (g_staffSlots[staffIndex].flags != 0 && g_staffSlots[staffIndex].talkTimer != 0
                        && WorldToScreen(g_staffSlots[staffIndex].posX, g_staffSlots[staffIndex].posY, &staffSpeechX,
                               &staffSpeechY, 0)
                               != 0
                        && g_staffSlots[staffIndex].talkTimer != 0
                        && g_staffSlots[staffIndex].talkTimer <= TALK_SHOW_TICKS && g_cameraZoom > TEXT_MIN_ZOOM) {
                        int complaintLine;
                        switch (g_staffSlots[staffIndex].talkKind) {
                        case TALK_COMPLAINT:
                            complaintLine = (g_clock + complaintOffset) / COMPLAINT_TICKS % 4;
                            switch (complaintLine) {
                            case 0: strcpy(g_printBuffer, "I hate my job."); break;
                            case 1: strcpy(g_printBuffer, "I don't need this aggravation."); break;
                            case 2: strcpy(g_printBuffer, "I hate my hat too."); break;
                            case 3: strcpy(g_printBuffer, "Golfers are pigs."); break;
                            }
                            break;
                        case TALK_GREETING:
                            if (IsGolferMale(g_staffSlots[staffIndex].talkGolfer) != 0)
                                strcpy(g_printBuffer, "How ya doin' buddy?");
                            else
                                strcpy(g_printBuffer, "How's it goin' missy?");
                            break;
                        case TALK_HURRY_UP:
                            if (IsGolferMale(g_staffSlots[staffIndex].talkGolfer) != 0)
                                strcpy(g_printBuffer, "Keep it moving buddy.");
                            else
                                strcpy(g_printBuffer, "Don't dawdle young lady.");
                            break;
                        case TALK_SEND_HOME:
                            if (IsGolferMale(g_staffSlots[staffIndex].talkGolfer) != 0)
                                strcpy(g_printBuffer, "OK pal, time to go home.");
                            else
                                strcpy(g_printBuffer, "Calm down missy, it's OK.");
                            break;
                        case TALK_WEEDS: strcpy(g_printBuffer, "Me hate dandelions."); break;
                        case TALK_DIVOTS: strcpy(g_printBuffer, "Me hate divots."); break;
                        case TALK_DRINK: strcpy(g_printBuffer, "How 'bout a drink?"); break;
                        default: break;
                        }

                        g_pMenu->SetFontsForStyles(g_cameraZoom > DEFAULT_ZOOM ? &g_smallManualFont : &g_boldArialFont,
                            0, 0, 0);
                        g_speechStripTile.BlitScaled(g_pMenu, staffSpeechX - BANNER_TILE_WIDTH,
                            staffSpeechY - g_cameraZoom * LABEL_LIFT_PER_ZOOM, g_colorLut, 1, 1, 0);
                        g_speechStripTile.BlitScaled(g_pMenu, staffSpeechX - 2 * BANNER_TILE_WIDTH,
                            staffSpeechY - g_cameraZoom * LABEL_LIFT_PER_ZOOM, g_colorLut, 1, 1, 0);
                        g_speechStripTile.BlitScaled(g_pMenu, staffSpeechX,
                            staffSpeechY - g_cameraZoom * LABEL_LIFT_PER_ZOOM, g_colorLut, 1, 1, 0);
                        g_speechStripTile.BlitScaled(g_pMenu, staffSpeechX - 3 * BANNER_TILE_WIDTH,
                            staffSpeechY - g_cameraZoom * LABEL_LIFT_PER_ZOOM, g_colorLut, 1, 1, 0);
                        g_speechStripTile.BlitScaled(g_pMenu, staffSpeechX + BANNER_TILE_WIDTH,
                            staffSpeechY - g_cameraZoom * LABEL_LIFT_PER_ZOOM, g_colorLut, 1, 1, 0);
                        g_speechStripTile.BlitScaled(g_pMenu, staffSpeechX + 2 * BANNER_TILE_WIDTH,
                            staffSpeechY - g_cameraZoom * LABEL_LIFT_PER_ZOOM, g_colorLut, 1, 1, 0);
                        int color =
                            g_golferPool[golferIndex].talkTimer < TALK_DIM_BELOW ? COLOR_LIGHT_GREY : COLOR_WHITE;
                        if ((g_staffSlots[staffIndex].flags & STAFF_QUITTING) != 0) {
                            color = COLOR_RED;
                        }
                        DrawTextCenteredAtY(g_printBuffer, staffSpeechX,
                            staffSpeechY - g_cameraZoom * LABEL_LIFT_PER_ZOOM, color);
                        staffSpeechY += 8;
                    }
                    staffIndex++;
                    complaintOffset += COMPLAINT_STAGGER;
                } while (staffIndex < STAFF_COUNT);
                int captionY;
                int captionX;
                if (g_nextOpenHoleIndex < CLUBHOUSE_HOLE && g_allHolesArray[g_nextOpenHoleIndex].teeX != 0
                    && g_allHolesArray[g_nextOpenHoleIndex].aimX != 0
                    && TileToScreenCached(g_allHolesArray[g_nextOpenHoleIndex].aimX,
                           g_allHolesArray[g_nextOpenHoleIndex].aimY, &captionX, &captionY)
                           != 0) {
                    if (g_allHolesArray[g_nextOpenHoleIndex].yards == 0) {
                        int dx =
                            abs(g_allHolesArray[g_nextOpenHoleIndex].teeX - g_allHolesArray[g_nextOpenHoleIndex].aimX);
                        int dy =
                            abs(g_allHolesArray[g_nextOpenHoleIndex].teeY - g_allHolesArray[g_nextOpenHoleIndex].aimY);
                        short yards = g_allHolesArray[g_nextOpenHoleIndex].yards =
                            sqrt((double)((dy * dy + dx * dx) * TILE_YARDS_SQUARED));
                        if (yards > LONG_HOLE_YARDS) {
                            g_allHolesArray[g_nextOpenHoleIndex].yards = (yards - LONG_HOLE_YARDS) / 4 + yards;
                        }
                    }
                    int length = g_allHolesArray[g_nextOpenHoleIndex].yards;
                    if ((g_allHolesArray[g_nextOpenHoleIndex].flags & (HOLE_DOGLEG_LEFT | HOLE_DOGLEG_RIGHT)) != 0) {
                        length += YARDS_PER_TILE;
                    }
                    if (length > RANGE_DISCOUNT_YARDS) {
                        length -= g_buildingLevelPerKind[SCENERY_DRIVING_RANGE] * YARDS_PER_TILE;
                    }
                    int par = 3;
                    if (length <= PAR2_MAX_YARDS) {
                        par = 2;
                    }
                    if (length >= PAR4_MIN_YARDS) {
                        par = 4;
                    }
                    if (length >= PAR5_MIN_YARDS) {
                        par = 5;
                    }
                    if (length > PAR6_OVER_YARDS) {
                        par = 6;
                    }
                    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                    strcpy(g_printBuffer, "Hole ");
                    strcat(g_printBuffer, itoa(g_nextOpenHoleIndex, g_numberBuf, 10));
                    DrawTextCenteredWithShadow(g_printBuffer, captionX,
                        (captionY - tileHalfHeight) - 2 * CAPTION_LINE_HEIGHT, COLOR_WHITE);
                    g_printBuffer[0] = '\0';
                    strcat(g_printBuffer, itoa(g_allHolesArray[g_nextOpenHoleIndex].yards, g_numberBuf, 10));
                    strcat(g_printBuffer, " yards");
                    DrawTextCenteredWithShadow(g_printBuffer, captionX,
                        (captionY - tileHalfHeight) - CAPTION_LINE_HEIGHT, COLOR_WHITE);
                    strcpy(g_printBuffer, "Par ");
                    strcat(g_printBuffer, itoa(par, g_numberBuf, 10));
                    DrawTextCenteredWithShadow(g_printBuffer, captionX, captionY - tileHalfHeight, COLOR_WHITE);
                    if (TileToScreenCached(g_allHolesArray[g_nextOpenHoleIndex].teeX,
                            g_allHolesArray[g_nextOpenHoleIndex].teeY, &captionX, &captionY)
                            != 0
                        && (g_clock & HOLE_PROMPT_BLINK_MASK) != 0) {
                        DrawTextCenteredWithShadow("Press 'h' to open hole", captionX, captionY - tileHalfHeight,
                            COLOR_WHITE);
                    }
                }
                g_shotSearchLocked = 0;
                if ((g_runModeFlags & GAME_PAUSED) == 0) {
                    UpdateAllGolfers();
                    UpdateAllEmployees();
                    UpdateDivots();
                    if (g_reportScreenId == 0 && g_displayFocus == 0) {
                        RenderAccomplishmentsScreen();
                    }
                    g_clock++;
                    if ((g_runModeFlags & GAME_FAST_CLOCK) != 0) {
                        g_clock += (g_clock & 1);
                    }
                    if ((g_clock & 3) <= g_level) {
                        int row = g_rng.NextInt(GRID_SIZE);
                        int col = g_rng.NextInt(GRID_SIZE);
                        int tile = col + row * GRID_SIZE;
                        if ((g_tileExtra[tile] & (TILE_BUILDING | TILE_WEEDS)) == 0) {
                            int weedCheck = 0;
                            int weedyNeighbours = 0;
                            int dir = 0;
                            do {
                                int neighborCol = g_dirColStep[dir] + col;
                                int neighborRow = g_dirRowStep[dir] + row;
                                if (IsPositionInvalid(neighborRow, neighborCol) == 0) {
                                    if ((g_tileExtra[neighborCol + neighborRow * GRID_SIZE] & TILE_WEEDS) != 0) {
                                        weedCheck |= NEIGHBOUR_WEEDY;
                                        weedyNeighbours++;
                                    }
                                    if (g_squareTypeTable[g_gameTileTerrainKind[neighborRow * GRID_SIZE + neighborCol]]
                                            .lieGrade
                                        <= 0) {
                                        weedCheck |= NEIGHBOUR_TURF;
                                    }
                                }
                                dir += 2;
                            } while (dir < DIRECTION_COUNT);
                            if (weedCheck != WEED_SPREAD_MASK || MAX_WEEDY_NEIGHBOURS < weedyNeighbours) {
                                weedCheck = 0;
                            }
                            if ((g_tileExtra[tile] & TILE_FLOWERS) != 0) {
                                weedCheck = g_rng.NextInt(FLOWER_WEED_ODDS) <= weedyNeighbours;
                            }
                            if (weedCheck != 0
                                && InLandmarkInfluence(g_golferPool[golferIndex].ballX, g_golferPool[golferIndex].ballY,
                                       2)
                                       == 0
                                && (g_gameTileTerrainKind[row * GRID_SIZE + col] == TERRAIN_FAIRWAY
                                    || g_squareTypeTable[g_gameTileTerrainKind[row * GRID_SIZE + col]].category
                                           == CATEGORY_ROUGH
                                    || g_squareTypeTable[g_gameTileTerrainKind[row * GRID_SIZE + col]].category
                                           == CATEGORY_TREES)) {
                                g_tileExtra[tile] |= TILE_ANIMATING | TILE_WEEDS;
                                g_tileAnimFrame[tile] = 1;
                                PlayPositionedSound(SOUND_FLIES, row * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                    col * TILE_WORLD_SIZE + TILE_WORLD_HALF, 0);
                            }
                        }
                    }
                    int chargesPerMonth = g_level + CHARGES_PER_MONTH_BASE;
                    int chargeInterval = TICKS_PER_MONTH / chargesPerMonth;
                    if (g_clock % chargeInterval == 0) {
                        short year = g_careerPlayYear;
                        for (int upkeepHole = 0; upkeepHole < HOLE_SLOT_COUNT; upkeepHole++) {
                            if (g_allHolesArray[upkeepHole].par != 0) {
                                g_gameCashHundreds--;
                                g_allHolesArray[upkeepHole].upkeep++;
                                g_annualStatsRecords[year % LEDGER_YEARS].v[LEDGER_MAINTENANCE] =
                                    g_annualStatsRecords[year % LEDGER_YEARS].v[LEDGER_MAINTENANCE] - 1;
                            }
                        }
                        for (int e = 0; e < STAFF_COUNT; e++) {
                            if (g_staffSlots[e].flags != 0 && g_staffSlots[e].job < 0
                                && g_staffSlots[e].job != JOB_PLAYER_AVATAR) {
                                int odds = WAGE_ODDS_BASE - g_level;
                                if (g_rng.NextInt(odds) <= GetHoleCountTier(g_nextOpenHoleIndex - 1)
                                    && (g_runModeFlags & GAME_CHAMPIONSHIP) == 0) {
                                    int wage = g_jobWages[-2 * g_staffSlots[e].job
                                                          - ((g_staffSlots[e].flags & STAFF_UPGRADED) ? 3 : 4)];
                                    g_gameCashHundreds -= wage;
                                    g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS].v[LEDGER_SALARIES] -= wage;
                                    int nearestHole = FindNearestHole(g_staffSlots[e].posX << TILE_WORLD_SHIFT,
                                        g_staffSlots[e].posY << TILE_WORLD_SHIFT);
                                    AddMoneyPopup(-wage, g_staffSlots[e].posX, g_staffSlots[e].posY, nearestHole);
                                    g_staffSlots[e].paid += wage;
                                }
                            }
                        }
                        g_monthlySkillScore[g_clock / TICKS_PER_MONTH % HISTORY_MONTHS] = g_clubSkillsScore;
                        g_moneyLine[g_clock / TICKS_PER_MONTH % HISTORY_MONTHS] = g_gameCashHundreds;
                        g_funRatingHistPoints[g_clock / TICKS_PER_MONTH % HISTORY_MONTHS] = g_courseTotalFunRate;
                        g_clubMemberSum = 0;
                        for (int typeIndex = 0; typeIndex < GOLFER_TYPE_COUNT; typeIndex++) {
                            if (g_golferHistoryPerCharacter[typeIndex].finishedStoryId != -1
                                && 1 < (g_golferHistoryPerCharacter[typeIndex].flags & MEMBERSHIP_MASK)) {
                                g_clubMemberSum++;
                            }
                        }
                        g_gameMembershipTrendLine[g_clock / TICKS_PER_MONTH % HISTORY_MONTHS] = g_clubMemberSum;
                        int lotStride = chargesPerMonth;
                        for (int lotSlot = g_clock / chargeInterval % lotStride; lotSlot < BUILDING_SLOT_COUNT;
                            lotSlot += lotStride) {
                            if (g_buildingTable[lotSlot].kind == SCENERY_HOME_SITE) {
                                int score = g_buildingTable[lotSlot].score;
                                g_buildingTable[lotSlot].score =
                                    CalculateLotScore(g_buildingTable[lotSlot].row, g_buildingTable[lotSlot].col)
                                    + (score - score / HOME_APPEAL_DECAY);
                                if ((g_forceBits & TRIGGER_CELEBRITY_HOME) != 0) {
                                    g_buildingTable[lotSlot].score = FORCED_HOME_APPEAL;
                                    g_forceBits &= ~TRIGGER_CELEBRITY_HOME;
                                }
                                int celeb;
                                do {
                                    celeb = g_rng.NextInt(CELEBRITY_COUNT);
                                } while (g_themeCelebsEntries[celeb].kind == -1);
                                if (g_buildingTable[lotSlot].resident == 0
                                    && g_buildingTable[lotSlot].score
                                           > (g_celebHomesBought * CELEB_APPEAL_STEP + CELEB_APPEAL_BASE)
                                                 * (g_level + CHARGES_PER_MONTH_BASE)) {
                                    int kind = g_themeCelebsEntries[celeb].kind;
                                    strcpy(g_printBuffer, "International ");
                                    strcat(g_printBuffer, g_celebKindNames[kind]);
                                    strcat(g_printBuffer, " ");
                                    strcat(g_printBuffer, g_themeCelebsEntries[celeb].name);
                                    strcat(g_printBuffer,
                                        " has purchased a vacation home at your golf course! Golfers enjoy seeing celebrities as they play.");
                                    if (ShowStoryMessage(COLOR_PURPLE, 0, SPEAKER_POPUP_ICON1) != 0) {
                                        g_buildingTable[lotSlot].resident = celeb + 1;
                                        NotifyEvent(EVENT_CELEBRITY, celeb);
                                        ConfigureAndPlaySound(SOUND_PURCHASE, SOUND_FULL_VOLUME, 0, 0, 0);
                                        g_celebHomesBought++;
                                        int homeLayout = g_rng.NextInt(HOME_SITE_LAYOUTS);
                                        SpawnHomeVIP(homeLayout, celeb, g_buildingTable[lotSlot].row,
                                            g_buildingTable[lotSlot].col);
                                    }
                                }
                            }
                        }
                    }
                    if (g_clock % TICKS_PER_MONTH == TICKS_PER_MONTH / 2) {
                        SaveGame("&AutoSave1");
                    }
                    if (g_clock % TICKS_PER_MONTH == 0) {
                        SaveGame("&AutoSave2");
                        g_monthlySkillScore[g_clock / TICKS_PER_MONTH % HISTORY_MONTHS] = g_clubSkillsScore;
                        g_moneyLine[g_clock / TICKS_PER_MONTH % HISTORY_MONTHS] = g_gameCashHundreds;
                        g_funRatingHistPoints[g_clock / TICKS_PER_MONTH % HISTORY_MONTHS] = g_courseTotalFunRate;
                        for (int slot = 1; slot < BUILDING_SLOT_COUNT; slot++) {
                            if (g_buildingTable[slot].kind >= FIRST_BUILDING_TYPE
                                && g_buildingTable[slot].kind != SCENERY_HOME_SITE
                                && (g_buildingTable[slot].flags & SLOT_REACHABLE) == 0
                                && g_buildingTable[slot].kind < SCENERY_CLUBHOUSE && g_level != 0
                                && (g_runModeFlags & GAME_CHAMPIONSHIP) == 0) {
                                strcpy(g_printBuffer, "WARNING: Your ");
                                strcat(g_printBuffer, g_facilityTypes[g_buildingTable[slot].kind].name);
                                strcat(g_printBuffer, " will not be operational until you have a path connecting it ");
                                strcat(g_printBuffer, "to your clubhouse.");
                                ShowStoryMessage(COLOR_DARK_RED, 1, -1);
                            }
                        }
                        for (int holeIndex = 1; holeIndex < HOLE_SLOT_COUNT; holeIndex++) {
                            g_allHolesArray[holeIndex].rounds -= g_allHolesArray[holeIndex].rounds / STAT_DECAY_DIVISOR;
                            g_allHolesArray[holeIndex].shots -= g_allHolesArray[holeIndex].shots / STAT_DECAY_DIVISOR;
                            g_allHolesArray[holeIndex].moodSum -=
                                g_allHolesArray[holeIndex].moodSum / STAT_DECAY_DIVISOR;
                            g_allHolesArray[holeIndex].playTime -=
                                g_allHolesArray[holeIndex].playTime / STAT_DECAY_DIVISOR;
                            g_allHolesArray[holeIndex].quitCount -=
                                g_allHolesArray[holeIndex].quitCount / STAT_DECAY_DIVISOR;
                            int commentKind = 0;
                            do {
                                g_allHolesArray[holeIndex].cmt[commentKind] -=
                                    g_allHolesArray[holeIndex].cmt[commentKind] / STAT_DECAY_DIVISOR;
                                commentKind++;
                            } while (commentKind < HOLE_COMMENT_KINDS);
                            int stat = 0;
                            do {
                                if (stat != HOLE_STAT_LONG_DRIVE) {
                                    g_allHolesArray[holeIndex].stats[stat] -=
                                        g_allHolesArray[holeIndex].stats[stat] / STAT_DECAY_DIVISOR;
                                }
                                stat++;
                            } while (stat < HOLE_STAT_COUNT);
                        }
                        if (g_gameCashHundreds < 0) {
                            g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS].v[LEDGER_MAINTENANCE] =
                                g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS].v[LEDGER_MAINTENANCE]
                                + g_gameCashHundreds / DEBT_INTEREST_DIVISOR;
                            g_gameCashHundreds += g_gameCashHundreds / DEBT_INTEREST_DIVISOR;
                        }
                        if (g_level == 0 && (g_allHolesArray[1].teeX == 0 || g_allHolesArray[1].aimX == 0)) {
                            strcpy(g_printBuffer,
                                "To create a golf hole requires just a tee and a green, the rest is up to you. ");
                            g_toolbarView = TOOLBAR_BUILD_COURSE;
                            strcat(g_printBuffer,
                                "All the pieces you need can be found on the Build Course selection panel below.");
                            ShowStoryMessage(COLOR_DARK_RED, 1, -1);
                        }
                        if (g_clock % TICKS_PER_YEAR == TICKS_PER_YEAR / 2 && g_watchedPlayerIdx == -1
                            && (g_runModeFlags & (GAME_SANDBOX | GAME_TOURNAMENT)) == 0) {
                            AssetSurface savedScreen;
                            savedScreen.Create(SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0, 0);
                            g_pMenu->copy(&savedScreen, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                            RenderHoleInfoOverlay(SGA_EVALUATION_QUIET);
                            if (g_winnersPurseK != 0) {
                                savedScreen.copy(g_pMenu, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                                strcpy(g_printBuffer,
                                    "The SGA is interested in holding a tournament at your course. 'We'd like to schedule the ");
                                strcat(g_printBuffer, g_mapCourseList[g_myWorldCourseIdx].name);
                                strcat(g_printBuffer,
                                    " Open Golf Tournament here as soon as possible.' We can offer a top prize of \xa7");
                                strcat(g_printBuffer, itoa(g_winnersPurseK, g_numberBuf, 10));
                                strcat(g_printBuffer, ",000!  Click the tournament button in the ");
                                g_reportScreenId = '\0';
                                strcat(g_printBuffer, g_golferTypeArray[0].name);
                                strcat(g_printBuffer, " panel to begin the tournament.");
                                ShowStoryMessage(COLOR_DARK_GREEN, 1, SPEAKER_POPUP_ICON0);
                                ConfigureAndPlaySound(SOUND_APPLAUSE_GOOD, SOUND_FULL_VOLUME, 0, 0, 0);
                                g_runModeFlags |= GAME_TOURNAMENT_OFFERED;
                                g_winnersPurseK = 0;
                            } else {
                                g_runModeFlags &= ~GAME_TOURNAMENT_OFFERED;
                                savedScreen.copy(g_pMenu, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                            }
                        }
                        if (g_clock % TICKS_PER_YEAR == 0) {
                            int row = 0;
                            do {
                                int col = 0;
                                do {
                                    g_posReactionTileCount[row * GRID_SIZE + col] =
                                        g_posReactionTileCount[row * GRID_SIZE + col]
                                        - (g_posReactionTileCount[row * GRID_SIZE + col] >> TALLY_DECAY_SHIFT);
                                    g_gameDispleasedTally[row * GRID_SIZE + col] =
                                        g_gameDispleasedTally[row * GRID_SIZE + col]
                                        - (g_gameDispleasedTally[row * GRID_SIZE + col] >> TALLY_DECAY_SHIFT);
                                    g_ballStopCountMap[row * GRID_SIZE + col] =
                                        g_ballStopCountMap[row * GRID_SIZE + col]
                                        - (g_ballStopCountMap[row * GRID_SIZE + col] >> TALLY_DECAY_SHIFT);
                                    col++;
                                } while (col < GRID_SIZE);
                                row++;
                            } while (row < GRID_SIZE);
                            int rival = 0;
                            do {
                                g_top10CoursePoints[rival][g_rng.NextInt(RIVAL_RATING_CATEGORIES)]++;
                                g_top10CoursePoints[rival][TOP_COURSE_TOTAL]++;
                                rival++;
                            } while (rival < RIVAL_COURSE_COUNT);
                            if ((g_runModeFlags & GAME_CHAMPIONSHIP) == 0) {
                                RenderEndOfYearReport();
                            }
                        }
                    }
                    short ambientRoll = g_rng.NextInt(AMBIENT_SOUND_ODDS);
                    if (ambientRoll == 0) {
                        int randomVolume;
                        int randomPan;
                        switch (g_rng.NextInt(AMBIENT_SOUND_KINDS)) {
                        case 0:
                            randomPan = g_rng.NextInt(AMBIENT_PAN_RANGE);
                            randomVolume = g_rng.NextInt(AMBIENT_VOLUME_RANGE);
                            ConfigureAndPlaySound(SOUND_AMBIENT_LAKE, randomVolume + AMBIENT_BASE_VOLUME,
                                PAN_CENTER - randomPan, AMBIENT_PITCH_SPREAD - g_rng.NextInt(2 * AMBIENT_PITCH_SPREAD),
                                0);
                            break;
                        case 1:
                            randomPan = g_rng.NextInt(AMBIENT_PAN_RANGE);
                            randomVolume = g_rng.NextInt(AMBIENT_VOLUME_RANGE);
                            ConfigureAndPlaySound(SOUND_AMBIENT_WIND, randomVolume + AMBIENT_BASE_VOLUME,
                                PAN_CENTER - randomPan, AMBIENT_PITCH_SPREAD - g_rng.NextInt(2 * AMBIENT_PITCH_SPREAD),
                                0);
                            break;
                        case 2:
                            randomPan = g_rng.NextInt(AMBIENT_PAN_RANGE);
                            randomVolume = g_rng.NextInt(AMBIENT_VOLUME_RANGE);
                            ConfigureAndPlaySound(SOUND_AMBIENT_DESERT, randomVolume + AMBIENT_BASE_VOLUME,
                                PAN_CENTER - randomPan, AMBIENT_PITCH_SPREAD - g_rng.NextInt(2 * AMBIENT_PITCH_SPREAD),
                                0);
                            break;
                        case 3:
                            randomPan = g_rng.NextInt(AMBIENT_PAN_RANGE);
                            randomVolume = g_rng.NextInt(AMBIENT_VOLUME_RANGE);
                            ConfigureAndPlaySound(SOUND_AMBIENT_FOREST, randomVolume + AMBIENT_BASE_VOLUME,
                                PAN_CENTER - randomPan, AMBIENT_PITCH_SPREAD - g_rng.NextInt(2 * AMBIENT_PITCH_SPREAD),
                                0);
                            break;
                        case 4:
                            randomPan = g_rng.NextInt(AMBIENT_PAN_RANGE);
                            randomVolume = g_rng.NextInt(AMBIENT_VOLUME_RANGE);
                            ConfigureAndPlaySound(SOUND_AMBIENT_GRASS, randomVolume + AMBIENT_BASE_VOLUME,
                                PAN_CENTER - randomPan, AMBIENT_PITCH_SPREAD - g_rng.NextInt(2 * AMBIENT_PITCH_SPREAD),
                                0);
                            break;
                        case 5:
                            randomPan = g_rng.NextInt(AMBIENT_PAN_RANGE);
                            randomVolume = g_rng.NextInt(AMBIENT_VOLUME_RANGE);
                            ConfigureAndPlaySound(SOUND_AMBIENT_SPRING, randomVolume + AMBIENT_BASE_VOLUME,
                                PAN_CENTER - randomPan, AMBIENT_PITCH_SPREAD - g_rng.NextInt(2 * AMBIENT_PITCH_SPREAD),
                                0);
                            break;
                        case 6:
                            randomPan = g_rng.NextInt(AMBIENT_PAN_RANGE);
                            randomVolume = g_rng.NextInt(AMBIENT_VOLUME_RANGE);
                            ConfigureAndPlaySound(SOUND_FLIES, randomVolume + AMBIENT_BASE_VOLUME,
                                PAN_CENTER - randomPan, AMBIENT_PITCH_SPREAD - g_rng.NextInt(2 * AMBIENT_PITCH_SPREAD),
                                0);
                            break;
                        }
                    }
                }
                if ((g_selectedGolferIndex != -1)
                    && (g_golferPool[g_selectedGolferIndex & SELECTED_GOLFER_MASK].screenX != -1)) {
                    int photoGolfer = g_selectedGolferIndex & SELECTED_GOLFER_MASK;
                    int partnerX = g_golferPool[photoGolfer ^ 1].screenX;
                    int golferX = g_golferPool[photoGolfer].screenX;
                    int boxLeft = (golferX > partnerX) ? partnerX : golferX;
                    int boxRight = golferX;
                    if (golferX < partnerX) {
                        boxRight = partnerX;
                    }
                    int golferY = g_golferPool[photoGolfer].screenY;
                    int boxTop = (golferY > g_golferPool[photoGolfer ^ 1].screenY)
                                     ? g_golferPool[photoGolfer ^ 1].screenY
                                     : golferY;
                    int boxBottom = (golferY < g_golferPool[photoGolfer ^ 1].screenY)
                                        ? g_golferPool[photoGolfer ^ 1].screenY
                                        : golferY;
                    int boxWidth = boxRight - boxLeft;
                    int boxHeight = boxBottom - boxTop;
                    int padY;
                    int padX;
                    if ((g_selectedGolferIndex & SELECTED_SPOTLIGHT) != 0) {
                        if (boxWidth < SPOTLIGHT_MIN_WIDTH) {
                            padX = (SPOTLIGHT_MIN_WIDTH - boxWidth) / 2;
                            boxLeft -= padX;
                            boxRight += padX;
                        }
                        if (boxHeight < SPOTLIGHT_MIN_HEIGHT) {
                            padY = (SPOTLIGHT_MIN_HEIGHT - boxHeight) / 2;
                            boxTop -= padY;
                            boxBottom += padY;
                        }
                    } else {
                        if (boxWidth < SIMFOTO_MIN_WIDTH) {
                            padX = (SIMFOTO_MIN_WIDTH - boxWidth) / 2;
                            boxLeft -= padX;
                            boxRight += padX;
                        }
                        if (boxHeight < SIMFOTO_MIN_HEIGHT) {
                            padY = (SIMFOTO_MIN_HEIGHT - boxHeight) / 2;
                            boxTop -= padY;
                            boxBottom += padY;
                        }
                    }
                    int left = Clamp(boxLeft - SIMFOTO_BORDER, SIMFOTO_BORDER, SIMFOTO_MAX_LEFT);
                    int right = Clamp(boxRight + SIMFOTO_BORDER, SIMFOTO_MIN_RIGHT, SIMFOTO_MAX_RIGHT);
                    int top = Clamp(boxTop - SIMFOTO_BORDER, SIMFOTO_BORDER, SIMFOTO_MAX_TOP);
                    int bottom = Clamp(boxBottom + SIMFOTO_BORDER, SIMFOTO_MIN_RIGHT, SIMFOTO_MAX_BOTTOM);
                    int photoWidth = right - left;
                    int outerWidth = photoWidth + 2 * SIMFOTO_BORDER;
                    g_simScreen.fill(left - SIMFOTO_BORDER, top - SIMFOTO_BORDER, outerWidth, SIMFOTO_BORDER,
                        COLOR_WHITE);
                    int photoHeight = bottom - top;
                    int outerHeight = photoHeight + 2 * SIMFOTO_BORDER;
                    g_simScreen.fill(left - SIMFOTO_BORDER, top - SIMFOTO_BORDER, SIMFOTO_BORDER, outerHeight,
                        COLOR_WHITE);
                    g_simScreen.fill(left - SIMFOTO_BORDER, bottom, outerWidth, SIMFOTO_BORDER, COLOR_WHITE);
                    g_simScreen.fill(right, top - SIMFOTO_BORDER, SIMFOTO_BORDER, outerHeight, COLOR_WHITE);
                    g_simScreen.line(left, top, right, top, COLOR_BLACK);
                    g_simScreen.line(left, top, left, bottom, COLOR_BLACK);
                    g_simScreen.line(left, bottom, right, bottom, COLOR_BLACK);
                    g_simScreen.line(right, top, right, bottom, COLOR_BLACK);
                    if ((g_golferPool[photoGolfer].storyChapter == STORY_LAST_CHAPTER)
                        && (g_golferPool[photoGolfer].msgLine == LINE_STORY)) {
                        g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
                        DrawTextCenteredWithShadow("Happy Ending!", (right + left) / 2, top + SIMFOTO_CAPTION_DY,
                            COLOR_PALE_YELLOW);
                    }
                    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                    DrawTextCenteredAtY("SimFoto", (right + left) / 2, bottom + SIMFOTO_CAPTION_DY, COLOR_LIGHT_GREY);
                    int yearTick = g_clock % TICKS_PER_YEAR;
                    g_printBuffer[0] = '\0';
                    strcat(g_printBuffer, itoa(yearTick / TICKS_PER_MONTH + FIRST_MONTH, g_numberBuf, 10));
                    strcat(g_printBuffer, "'");
                    strcat(g_printBuffer,
                        itoa(yearTick % TICKS_PER_MONTH * DAYS_PER_MONTH / TICKS_PER_MONTH + 1, g_numberBuf, 10));
                    strcat(g_printBuffer, "'");
                    strcat(g_printBuffer, itoa(g_careerPlayYear + FIRST_GAME_YEAR, g_numberBuf, 10));
                    DrawTextClampedToScreen(g_printBuffer, right - SIMFOTO_DATE_DX, bottom - SIMFOTO_DATE_DY,
                        COLOR_PALE_YELLOW);
                    if ((g_selectedGolferIndex & SELECTED_SPOTLIGHT) == 0) {
                        char dateCaption[16];
                        strcpy(dateCaption, g_printBuffer);
                        strcpy(g_printBuffer, "snapshots\\");
                        AppendGolferName(photoGolfer, 0);
                        strcat(g_printBuffer, "-");
                        strcat(g_printBuffer, dateCaption);
                        strcat(g_printBuffer, ".jpg");
                        SaveSurfaceToJPEG(g_printBuffer, g_pMenu, (left & EVEN_PIXEL_MASK) - SIMFOTO_JPEG_MARGIN,
                            (top & EVEN_PIXEL_MASK) - SIMFOTO_JPEG_MARGIN,
                            (photoWidth & EVEN_PIXEL_MASK) + SIMFOTO_JPEG_GROW,
                            (photoHeight & EVEN_PIXEL_MASK) + SIMFOTO_JPEG_GROW, 1);
                    }
                }
                if (g_zoomView != 0) {
                    g_simScreen.BlitSurfaceRegion(&g_simScreen, ZOOM_VIEW_LEFT, ZOOM_VIEW_TOP, ZOOM_VIEW_WIDTH,
                        ZOOM_VIEW_HEIGHT, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                    int ball = 0;
                    if (g_renderedBallCount > 0) {
                        do {
                            int zoomedX = g_visibleBallX[ball] * ZOOM_VIEW_FACTOR - ZOOM_VIEW_LEFT * ZOOM_VIEW_FACTOR;
                            int zoomedY = g_visibleBallY[ball] * ZOOM_VIEW_FACTOR - ZOOM_VIEW_TOP * ZOOM_VIEW_FACTOR;
                            if ((((zoomedX >= 0) && (zoomedX < SCREEN_WIDTH)) && (zoomedY >= 0))
                                && ((zoomedY < SCREEN_HEIGHT
                                     && g_simScreen.get_pixel(zoomedX, zoomedY) == PIXEL_WHITE))) {
                                g_bigGolfBallDot.Draw(g_pMenu, zoomedX - 2, zoomedY - 1, 1, 1, 0);
                            }
                            ball++;
                        } while (ball < g_renderedBallCount);
                    }
                }
                g_homesSold = (g_holeAwardCount = 0, g_ratingUnawarded = 0, 0);
                g_ratingWealth = g_gameCashHundreds / RATING_WEALTH_UNIT;
                for (int awardHole = 1; awardHole < HOLE_SLOT_COUNT; awardHole = awardHole + 1) {
                    if ((g_allHolesArray[awardHole].flags & HOLE_TOP_100) != 0) {
                        g_holeAwardCount++;
                    }
                    if ((g_allHolesArray[awardHole].flags & HOLE_TOP_18) != 0) {
                        g_holeAwardCount++;
                    }
                }
                for (int homeSlot = 0; homeSlot < BUILDING_SLOT_COUNT; homeSlot = homeSlot + 1) {
                    if ((g_buildingTable[homeSlot].kind == SCENERY_HOME_SITE)
                        && (g_buildingTable[homeSlot].resident != 0)) {
                        g_homesSold++;
                    }
                }
                g_mapCourseList[g_myWorldCourseIdx].holeAwards = g_holeAwardCount;
                g_mapCourseList[g_myWorldCourseIdx].homesSold = g_homesSold;
                g_mapCourseList[g_myWorldCourseIdx].ratingUnawarded = 0;
                g_mapCourseList[g_myWorldCourseIdx].tourPoints = g_winningPtsTotal;
                g_mapCourseList[g_myWorldCourseIdx].storyCount = g_storiesNum;
                g_mapCourseList[g_myWorldCourseIdx].moneyHi = g_gameCashHundreds / 0x100;
                g_mapCourseList[g_myWorldCourseIdx].moneyLo = g_gameCashHundreds % 0x100;
                g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                if (graphsys != 0) {
                    graphsys->set_sprite_scale(1, 1, 1);
                }
                int toolbarMouseX;
                int toolbarMouseY = 0;
                int panelPick = -1;
                g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                if ((g_runModeFlags & GAME_TOURNAMENT) == 0) {
                    int buttonX = ROSTER_LEFT_X;
                    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
                    memset(g_rosterBarButtonGolfer, 0xff, sizeof(g_rosterBarButtonGolfer));
                    int *nextRosterSlot = g_rosterBarButtonGolfer;
                    for (int stepsBack = 0; stepsBack < GOLFER_COUNT; stepsBack++) {
                        waitingPair = ((g_simLatestGolferNum - stepsBack) + GOLFER_COUNT) % GOLFER_COUNT;
                        int buttonGolfer = waitingPair;
                        if (((g_golferPool[waitingPair].holeIdx != 0) && (g_golferPool[waitingPair].holeIdx > 0))
                            && ((g_nextOpenHoleIndex <= BACK_NINE_FIRST_HOLE
                                 || ((HIBYTE(GetAsyncKeyState(VK_SHIFT)) == 0
                                      || (g_golferPool[waitingPair].holeIdx > FRONT_NINE_LAST_HOLE)))))) {
                            g_memberWindowSprite[Clamp(g_golferPool[waitingPair].enjoyment + MOOD_BUTTON_OFFSET, 1,
                                                     MOOD_BUTTON_LAST)]
                                .Draw(g_pMenu, buttonX, ROSTER_BUTTON_Y, 1, 1, 0);
                            g_simScreen.fill(buttonX + 1, ROSTER_LABEL_Y, ROSTER_SWATCH_WIDTH, ROSTER_SWATCH_HEIGHT,
                                g_golferPool[waitingPair].persona ? COLOR_PALE_BLUE : COLOR_WHITE);
                            int holeLabelColor = COLOR_BLACK;
                            if (g_golferPool[waitingPair].fatigue > GOLFER_TIRED_LEVEL) {
                                holeLabelColor = COLOR_GREY;
                            }
                            if (g_golferPool[waitingPair].thirst > GOLFER_NEED_LEVEL) {
                                holeLabelColor = COLOR_BLUE;
                            }
                            if (g_golferPool[waitingPair].hunger > GOLFER_NEED_LEVEL) {
                                holeLabelColor = COLOR_DARK_RED;
                            }
                            if ((g_golferPool[waitingPair].flags & GOLFER_WALKED_OFF) != 0) {
                                holeLabelColor = COLOR_RED;
                            }
                            g_printBuffer[0] = '\0';
                            strcat(g_printBuffer, itoa(g_golferPool[waitingPair].holeIdx, g_numberBuf, 10));
                            DrawTextCenteredAtY(g_printBuffer, buttonX + ROSTER_BUTTON_WIDTH / 2, ROSTER_LABEL_Y,
                                holeLabelColor);
                            buttonX += ROSTER_BUTTON_WIDTH;
                            *nextRosterSlot = buttonGolfer;
                            nextRosterSlot++;
                            if (buttonX > ROSTER_RIGHT_X)
                                break;
                        }
                    }
                    g_s_courseInfo[0].BlitScaled(g_pMenu, COURSE_PANEL_X, COURSE_PANEL_Y, g_colorLut, 1, 1, 0);
                    g_simTopBoxes[0].draw_antialiased(&g_courseInfoBoxPicsAlphaMask[0], g_pMenu, COURSE_PANEL_X,
                        COURSE_PANEL_Y, 0);
                    g_siteMapSprs[g_tourSites[g_mapCourseList[g_myWorldCourseIdx].site].id].draw_antialiased(
                        &g_regionAlphaFrames[g_tourSites[g_mapCourseList[g_myWorldCourseIdx].site].id], g_pMenu,
                        REGION_BADGE_X, REGION_BADGE_Y, 0);
                    g_printBuffer[0] = 0;
                    AppendCourseName(0);
                    DrawTextCenteredAtY(g_printBuffer, HUD_TITLE_X, COURSE_NAME_Y, COLOR_WHITE);
                    switch (g_level) {
                    case 0: strcpy(g_printBuffer, "Easy, "); break;
                    case 1: strcpy(g_printBuffer, "Moderate, "); break;
                    case 2: strcpy(g_printBuffer, "Difficult, "); break;
                    case 3: strcpy(g_printBuffer, "Impossible, "); break;
                    }
                    g_printBuffer[0] = '\0';
                    FormatDateString(g_clock);
                    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
                    DrawTextCenteredAtY(g_printBuffer, HUD_TITLE_X, COURSE_DATE_Y, COLOR_WHITE);
                    int trophyCount = g_storiesNum + g_winningPtsTotal + g_holeAwardCount + g_homesSold;
                    if (trophyCount < TROPHY_ROW_MAX) {
                        int step = Clamp(TROPHY_ROW_WIDTH / (trophyCount + 1), ICON_STEP_MIN, ICON_STEP_MAX);
                        int iconX = HUD_ROW_CENTER_X - (step * trophyCount) / 2;
                        int awardIcon = 0;
                        if (g_holeAwardCount > 0) {
                            do {
                                g_bubblePieces[GLYPH_HOLE_AWARD].BlitScaled(g_pMenu, iconX, TROPHY_ROW_Y, 0);
                                iconX += step;
                                awardIcon++;
                            } while (awardIcon < g_holeAwardCount);
                        }
                        int homeIcon = 0;
                        if (g_homesSold > 0) {
                            do {
                                g_bubblePieces[GLYPH_HOME_SOLD].BlitScaled(g_pMenu, iconX, TROPHY_ROW_Y, 0);
                                iconX += step;
                                homeIcon++;
                            } while (homeIcon < g_homesSold);
                        }
                        int winIcon = 0;
                        if (g_winningPtsTotal > 0) {
                            do {
                                g_bubblePieces[GLYPH_TOUR_WIN].BlitScaled(g_pMenu, iconX, TROPHY_ROW_Y, 0);
                                iconX += step;
                                winIcon++;
                            } while (winIcon < g_winningPtsTotal);
                        }
                        int storyIcon = 0;
                        if (g_storiesNum > 0) {
                            do {
                                g_bubblePieces[GLYPH_STORY].BlitScaled(g_pMenu, iconX, TROPHY_ROW_Y, 0);
                                iconX += step;
                                storyIcon++;
                            } while (storyIcon < g_storiesNum);
                        }
                    } else {
                        g_bubblePieces[GLYPH_HOLE_AWARD].BlitScaled(g_pMenu, TALLY_X, TROPHY_ROW_Y, 0);
                        strcpy(g_printBuffer, "x");
                        strcat(g_printBuffer, itoa(g_holeAwardCount, g_numberBuf, 10));
                        DrawTextWithColor(g_printBuffer, TALLY_X + TALLY_TEXT_DX, TROPHY_ROW_Y, COLOR_WHITE);
                        g_bubblePieces[GLYPH_HOME_SOLD].BlitScaled(g_pMenu, TALLY_X + TALLY_STEP, TROPHY_ROW_Y, 0);
                        strcpy(g_printBuffer, "x");
                        strcat(g_printBuffer, itoa(g_homesSold, g_numberBuf, 10));
                        DrawTextWithColor(g_printBuffer, TALLY_X + TALLY_STEP + TALLY_TEXT_DX, TROPHY_ROW_Y,
                            COLOR_WHITE);
                        g_bubblePieces[GLYPH_TOUR_WIN].BlitScaled(g_pMenu, TALLY_X + 2 * TALLY_STEP, TROPHY_ROW_Y, 0);
                        strcpy(g_printBuffer, "x");
                        strcat(g_printBuffer, itoa(g_winningPtsTotal, g_numberBuf, 10));
                        DrawTextWithColor(g_printBuffer, TALLY_X + 2 * TALLY_STEP + TALLY_TEXT_DX, TROPHY_ROW_Y,
                            COLOR_WHITE);
                        g_bubblePieces[GLYPH_STORY].BlitScaled(g_pMenu, TALLY_X + 3 * TALLY_STEP, TROPHY_ROW_Y, 0);
                        strcpy(g_printBuffer, "x");
                        strcat(g_printBuffer, itoa(g_storiesNum, g_numberBuf, 10));
                        DrawTextWithColor(g_printBuffer, TALLY_X + 3 * TALLY_STEP + TALLY_TEXT_DX, TROPHY_ROW_Y,
                            COLOR_WHITE);
                    }
                    int waitingCount = g_waitListLength;
                    if (waitingCount > 0) {
                        int waitingStep = Clamp(WAITING_ROW_WIDTH / (waitingCount + 1), ICON_STEP_MIN, ICON_STEP_MAX);
                        int iconX = HUD_ROW_CENTER_X - (waitingStep * waitingCount) / 2;
                        if (g_waitListLength < WAITING_ROW_MAX) {
                            int waitingIcon = 0;
                            if (g_waitListLength > 0) {
                                do {
                                    g_bubblePieces[GLYPH_WAITING].BlitScaled(g_pMenu, iconX, WAITING_ROW_Y, 0);
                                    iconX += waitingStep;
                                    waitingIcon++;
                                } while (waitingIcon < g_waitListLength);
                            }
                        } else {
                            g_bubblePieces[GLYPH_WAITING].BlitScaled(g_pMenu, WAITING_TALLY_X, WAITING_ROW_Y, 0);
                            strcpy(g_printBuffer, "x");
                            strcat(g_printBuffer, itoa(g_waitListLength, g_numberBuf, 10));
                            DrawTextWithColor(g_printBuffer, WAITING_TALLY_X + TALLY_TEXT_DX, WAITING_ROW_Y,
                                COLOR_WHITE);
                        }
                    }
                }
                g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                g_s_courseInfo[1].BlitScaled(g_pMenu, MONEY_PANEL_X, MONEY_PANEL_Y, g_colorLut, 1, 1, 0);
                g_simTopBoxes[1].draw_antialiased(&g_courseInfoBoxPicsAlphaMask[1], g_pMenu, MONEY_PANEL_X,
                    MONEY_PANEL_Y, 0);
                g_fundsLabelFlag = 0;
                strcpy(g_printBuffer, "§");
                int money = g_gameCashHundreds;
                FormatNumberWithCommas(money * DOLLARS_PER_MONEY_UNIT);
                if ((g_runModeFlags & GAME_SANDBOX) == 0) {
                    if ((g_runModeFlags & GAME_CHEATED) != 0) {
                        strcat(g_printBuffer, "*");
                    }
                    if (g_gameCashHundreds >= 0) {
                        DrawTextCenteredWithShadow(g_printBuffer, MONEY_TEXT_X, MONEY_TEXT_Y, COLOR_BRIGHT_GREEN);
                    } else if ((g_clock & DEBT_FLASH_BIT) != 0) {
                        DrawTextCenteredWithShadow(g_printBuffer, MONEY_TEXT_X, MONEY_TEXT_Y, COLOR_RED);
                    }
                }
                g_s_courseInfo[2].BlitScaled(g_pMenu, FUN_PANEL_X, FUN_PANEL_Y, g_colorLut, 1, 1, 0);
                g_simTopBoxes[2].draw_antialiased(&g_courseInfoBoxPicsAlphaMask[2], g_pMenu, FUN_PANEL_X, FUN_PANEL_Y,
                    0);
                strcpy(g_printBuffer, "Fun Rating: ");
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(g_courseTotalFunRate, g_numberBuf, 10));
                DrawTextCenteredWithShadow(g_printBuffer, FUN_TEXT_X, FUN_TEXT_Y, COLOR_PALE_YELLOW);
                CalculateAllHoleStatistics();
                g_s_courseInfo[3].BlitScaled(g_pMenu, SKILL_PANEL_X, SKILL_PANEL_Y, g_colorLut, 1, 1, 0);
                g_simTopBoxes[3].draw_antialiased(&g_courseInfoBoxPicsAlphaMask[3], g_pMenu, SKILL_PANEL_X,
                    SKILL_PANEL_Y, 0);
                strcpy(g_printBuffer, "Skill Rating: ");
                g_printBuffer[0] = 0;
                AppendMoney(g_clubSkillsScore);
                DrawTextCenteredWithShadow(g_printBuffer, SKILL_TEXT_X, SKILL_TEXT_Y, COLOR_CYAN);
                int waiting = g_waitListLength;
                strcpy(g_printBuffer, "Waiting list: ");
                FormatNumberWithCommas(waiting);
                g_simScreen.get_mouse_pos(&toolbarMouseX, &toolbarMouseY);
                DisableCapsLock();
                if (graphsys != 0) {
                    graphsys->set_sprite_scale(1, 1, 1);
                }
                g_simCameraZoomStored = g_cameraZoom;
                g_cameraZoom = DEFAULT_ZOOM;
                if ((g_runModeFlags & GAME_NO_CLUBHOUSE) != 0) {
                    g_toolbarView = TOOLBAR_BUILDINGS;
                    g_elevToolbarShown = 0;
                    g_selectedBuildingKind = SCENERY_CLUBHOUSE;
                }
                RenderMainBar(toolbarMouseX, toolbarMouseY);
                if (g_toolbarView == TOOLBAR_BUILD_COURSE) {
                    RenderTerrainToolbar(toolbarMouseX, toolbarMouseY);
                    g_cameraZoom = g_simCameraZoomStored;
                } else if (g_toolbarView == TOOLBAR_BUILDINGS) {
                    RenderFacilityUpgradeToolbar(toolbarMouseX, toolbarMouseY);
                    g_cameraZoom = g_simCameraZoomStored;
                } else if (g_toolbarView == TOOLBAR_GOLFERS) {
                    RenderGolferPairingPanel(toolbarMouseX, toolbarMouseY);
                    g_cameraZoom = g_simCameraZoomStored;
                } else if ((g_toolbarView == TOOLBAR_SHOT) || (g_toolbarView == TOOLBAR_PEOPLE)) {
                    RenderPeopleToolbarWithTooltip(toolbarMouseX, toolbarMouseY);
                    g_cameraZoom = g_simCameraZoomStored;
                } else {
                    g_cameraZoom = g_simCameraZoomStored;
                    if (((toolbarMouseX < RIGHT_PANEL_X) && (g_pickState == -1)) || (g_selectedGolferIndex != -1)) {
                        panelPick = -1;
                    } else {
                        panelPick = g_pickState;
                        g_pickState = -1;
                    }
                }
                if (((g_toolbarView == TOOLBAR_BUILD_COURSE) && (g_selectedBuildingKind == SCENERY_HOME_SITE))
                    && (g_waitListLength <= 0)) {
                    g_selectedBuildingKind = -1;
                }
                if (g_displayFocus != 0) {
                    if (g_displayFocus < 0) {
                        RenderHoleStatsScreen(-g_displayFocus);
                    } else {
                        int panelGolfer = g_displayFocus - 1;
                        signed char golferHole = g_golferPool[panelGolfer].holeIdx;
                        if (golferHole != -1) {
                            RenderGolferInfoPanel(panelGolfer, golferHole == 0 || golferHole == CLUBHOUSE_HOLE);
                        } else {
                            ProcessHireEmployeeSelection();
                            g_displayFocus = 0;
                        }
                    }
                }
                g_printBuffer[0] = '\0';
                if (((g_runModeFlags & GAME_PAUSED) != 0) && (g_selectedGolferIndex == -1)) {
                    strcat(g_printBuffer, "Paused");
                }
                if (g_fastRun != 0) {
                    strcat(g_printBuffer, " Fast");
                }
                g_pMenu->SetFontsForStyles(&g_hudFont, 0, 0, 0);
                DrawTextCenteredWithShadow(g_printBuffer, SCREEN_CENTER_X, STATUS_TEXT_Y, COLOR_WHITE);
                do_all_tasks();
                int cursorY;
                int cursorX;
                g_simScreen.get_mouse_pos(&cursorX, &cursorY);
                g_zoomDouble = g_zoomView;
                ScreenToTileCoords(cursorX, cursorY, &g_pointerPosX, &g_pointerPosY, 0);
                if ((cursorY < MEASURE_LINE_MAX_Y)
                    && (((((g_paintTerrain == -1 && (g_selectedBuildingKind == -1))
                              || (g_toolbarView == TOOLBAR_GOLFERS))
                             || (g_toolbarView == TOOLBAR_CLOSED))
                         && (g_mouseDownX != -1)))) {
                    int measurePixels = Math_Distance2D(cursorX - g_mouseDownX,
                        ((cursorY - g_mouseDownY) * ISO_Y_SCALE_DEN) / ISO_Y_SCALE_NUM);
                    int yards = (measurePixels * YARDS_PER_TILE) / (g_cameraZoom * TILE_PIXELS_PER_ZOOM);
                    if (yards > YARDS_PER_TILE) {
                        DrawTerrainLineWrapper(cursorX + 1, cursorY + 1, g_mouseDownX + 1, g_mouseDownY + 1,
                            COLOR_BLACK, LINE_WIDTH_THIN, LINE_ALPHA);
                        DrawTerrainLineWrapper(cursorX, cursorY + 1, g_mouseDownX, g_mouseDownY + 1, COLOR_BLACK,
                            LINE_WIDTH_THIN, LINE_ALPHA);
                        DrawTerrainLineWrapper(cursorX, cursorY, g_mouseDownX, g_mouseDownY, COLOR_PALE_YELLOW,
                            LINE_WIDTH_THIN, LINE_ALPHA);
                        DrawTerrainLineWrapper(cursorX + 1, cursorY, g_mouseDownX + 1, g_mouseDownY, COLOR_PALE_YELLOW,
                            LINE_WIDTH_THIN, LINE_ALPHA);
                        g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
                        g_printBuffer[0] = '\0';
                        strcat(g_printBuffer, itoa(yards, g_numberBuf, 10));
                        strcat(g_printBuffer, " yds.");
                        DrawTextCenteredWithShadow(g_printBuffer, (cursorX + g_mouseDownX) / 2,
                            (cursorY + g_mouseDownY) / 2 - 4, COLOR_WHITE);
                    }
                }
                if ((cursorY < TOOLBAR_TOP) && ((g_runModeFlags & GAME_DEMOLISH_TOOL) != 0)) {
                    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                    int cost = BuildDemolishTooltipText(g_pointerPosX, g_pointerPosY);
                    TileToScreenCached(g_pointerPosX, g_pointerPosY, &cursorTileScreenX, &cursorTileScreenY);
                    if (cost != 0) {
                        g_bulldozeLitCursor.Draw(g_pMenu, cursorTileScreenX - DEMOLISH_ICON_DX,
                            cursorTileScreenY - DEMOLISH_ICON_DY, 1, 1, 0);
                    } else {
                        g_bulldozeDimCursor.Draw(g_pMenu, cursorTileScreenX - DEMOLISH_ICON_DX,
                            cursorTileScreenY - DEMOLISH_ICON_DY, 1, 1, 0);
                    }
                    DrawTextCenteredWithShadow(g_printBuffer, cursorTileScreenX, cursorTileScreenY - DEMOLISH_LABEL_DY,
                        cost != 0 ? COLOR_BRIGHT_GREEN : COLOR_WHITE);
                }
                int col = -1;
                if ((g_toolbarView == TOOLBAR_BUILDINGS && g_elevToolbarShown == 0)
                    || (g_toolbarView == TOOLBAR_BUILD_COURSE && g_buildToolbarShown != 0)) {
                    if (cursorY < TOOLBAR_TOP
                        && (g_tileExtra[g_pointerPosY + g_pointerPosX * GRID_SIZE] & TILE_ANIMATING) == 0
                        && g_selectedBuildingKind != -1
                        && (g_selectedBuildingKind != SCENERY_LANDMARK || g_selectedVariant != -1)) {
                        int footprintSide = g_facilityTypes[g_selectedBuildingKind].side
                                            + g_buildingLevelPerKind[g_selectedBuildingKind];
                        int siteClearCost = ValidateBuildingPlacement(g_pointerPosX, g_pointerPosY, footprintSide,
                            g_selectedBuildingKind);
                        if ((g_selectedBuildingKind == 0) && (siteClearCost != -1)) {
                            siteClearCost = siteClearCost / 2;
                        }
                        int siteColor = siteClearCost != -1 ? COLOR_WHITE : COLOR_RED;
                        unsigned ghostFlags = siteClearCost != -1 ? SPRITE_TRANSLUCENT : SPRITE_RED_SILHOUETTE;
                        int typePalette = g_selectedBuildingKind + PAL_BUILDING_TYPE_BASE;
                        int screenY;
                        int screenX;
                        WorldToScreen(g_pointerPosX << TILE_WORLD_SHIFT, g_pointerPosY << TILE_WORLD_SHIFT, &screenX,
                            &screenY, 0);
                        int edgeToY;
                        int edgeToX;
                        WorldToScreen((footprintSide + g_pointerPosX) * TILE_WORLD_SIZE - 1,
                            g_pointerPosY << TILE_WORLD_SHIFT, &edgeToX, &edgeToY, 0);
                        DrawTerrainLineWrapper(screenX, screenY, edgeToX, edgeToY, siteColor, 1, LINE_ALPHA);
                        WorldToScreen((footprintSide + g_pointerPosX) * TILE_WORLD_SIZE - 1,
                            (footprintSide + g_pointerPosY) * TILE_WORLD_SIZE, &screenX, &screenY, 0);
                        DrawTerrainLineWrapper(screenX, screenY, edgeToX, edgeToY, siteColor, 1, LINE_ALPHA);
                        WorldToScreen(g_pointerPosX << TILE_WORLD_SHIFT,
                            (footprintSide + g_pointerPosY) * TILE_WORLD_SIZE - 1, &edgeToX, &edgeToY, 0);
                        DrawTerrainLineWrapper(screenX, screenY, edgeToX, edgeToY, siteColor, 1, LINE_ALPHA);
                        WorldToScreen(g_pointerPosX << TILE_WORLD_SHIFT, g_pointerPosY << TILE_WORLD_SHIFT, &screenX,
                            &screenY, 0);
                        DrawTerrainLineWrapper(screenX, screenY, edgeToX, edgeToY, siteColor, 1, LINE_ALPHA);
                        int halfFootprint = (footprintSide * TILE_WORLD_SIZE) / 2;
                        WorldToScreen(g_pointerPosX * TILE_WORLD_SIZE + halfFootprint,
                            g_pointerPosY * TILE_WORLD_SIZE + halfFootprint, &screenX, &screenY, 0);
                        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                        int buildingType = g_selectedBuildingKind;
                        g_printBuffer[0] = '\0';
                        strcpy(g_printBuffer, g_facilityTypes[g_selectedBuildingKind].name);
                        if (buildingType == SCENERY_LANDMARK) {
                            g_printBuffer[0] = '\0';
                            AppendLandmarkName(g_selectedVariant, 0);
                            if (g_printBuffer[0] >= 'a') {
                                g_printBuffer[0] -= 0x20;
                            }
                            if (g_selectedVariant != LANDMARK_STANDING_STONES) {
                                g_curSprite = GetFlicBankSprite(g_selectedVariant + BANK_LANDMARKS, 0,
                                    (g_camDirIdx / QUARTER_TURN + g_curBuildingTurns) & FACING_MASK);
                            } else {
                                g_curSprite = GetFlicBankSprite(BANK_STANDING_STONES, 0,
                                    (g_camDirIdx + g_curBuildingTurns) & FACING_MASK);
                            }
                            DrawSpriteImmediate(screenX, screenY, screenY, SPRITE_ORIGIN_BUILDING,
                                SPRITE_ORIGIN_BUILDING, g_curSprite, SPRITE_FULL_SCALE,
                                g_selectedVariant + PAL_LANDMARK_BASE | ghostFlags);
                            int landmarkReach =
                                (g_selectedVariant * LANDMARK_REACH_STEP + LANDMARK_REACH_BASE) * LANDMARK_REACH_SCALE;
                            DrawCircleOnTerrain(g_pointerPosX * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                g_pointerPosY * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                landmarkReach / LANDMARK_REACH_DIVISOR, COLOR_WHITE);
                        }
                        int buildingLevel = g_buildingLevelPerKind[g_selectedBuildingKind];
                        switch (g_selectedBuildingKind) {
                        case SCENERY_WILLOW_TREE:
                            g_curSprite = GetFlicBankSprite(g_selectedVariant + BANK_SCENIC_TREES,
                                g_flicBankFrameLens[g_selectedVariant + BANK_SCENIC_TREES] - 1,
                                (g_camDirIdx / QUARTER_TURN + g_curBuildingTurns) & FACING_MASK);
                            DrawSpriteImmediate(screenX, screenY, screenY, SPRITE_ORIGIN_SCENIC, SPRITE_ORIGIN_SCENIC,
                                g_curSprite, SPRITE_FULL_SCALE, g_selectedVariant + PAL_SCENIC_TREES | ghostFlags);
                            break;
                        case SCENERY_SNACK_BAR:
                            g_curSprite = GetFlicBankSprite(BANK_SNACK_BAR, 0,
                                (g_camDirIdx / QUARTER_TURN + g_curBuildingTurns) & FACING_MASK);
                            ghostFlags |= buildingLevel ? typePalette : PAL_SNACK_BAR;
                            DrawSpriteImmediate(screenX, screenY, 0, SPRITE_ORIGIN_BUILDING, SPRITE_ORIGIN_BUILDING,
                                g_curSprite, SPRITE_FULL_SCALE, ghostFlags);
                            break;
                        case SCENERY_PRO_SHOP:
                            g_curSprite = GetFlicBankSprite(buildingLevel + BANK_PRO_SHOP, 0,
                                (g_camDirIdx / QUARTER_TURN + g_curBuildingTurns) & FACING_MASK);
                            ghostFlags |= buildingLevel ? typePalette : PAL_PRO_SHOP;
                            DrawSpriteImmediate(screenX, screenY, 0, SPRITE_ORIGIN_BUILDING, SPRITE_ORIGIN_BUILDING,
                                g_curSprite, SPRITE_FULL_SCALE, ghostFlags);
                            break;
                        case SCENERY_SWIM_CLUB:
                            g_curSprite = GetFlicBankSprite(buildingLevel + BANK_SWIM_CLUB, 0,
                                (g_camDirIdx / QUARTER_TURN + g_curBuildingTurns) & FACING_MASK);
                            ghostFlags |= buildingLevel ? typePalette : PAL_SWIM_CLUB;
                            DrawSpriteImmediate(screenX, screenY, 0, SPRITE_ORIGIN_BUILDING, SPRITE_ORIGIN_BUILDING,
                                g_curSprite, SPRITE_FULL_SCALE, ghostFlags);
                            break;
                        case SCENERY_CART_GARAGE:
                            g_curSprite = GetFlicBankSprite(buildingLevel + BANK_CART_GARAGE, 0,
                                (g_camDirIdx / QUARTER_TURN + g_curBuildingTurns) & FACING_MASK);
                            ghostFlags |= buildingLevel ? typePalette : PAL_CART_GARAGE;
                            DrawSpriteImmediate(screenX, screenY, 0, SPRITE_ORIGIN_BUILDING, SPRITE_ORIGIN_BUILDING,
                                g_curSprite, SPRITE_FULL_SCALE, ghostFlags);
                            break;
                        case SCENERY_RESORT_HOTEL:
                            g_curSprite = GetFlicBankSprite(buildingLevel + BANK_RESORT_HOTEL, 0,
                                (g_camDirIdx / QUARTER_TURN + g_curBuildingTurns) & FACING_MASK);
                            ghostFlags |= buildingLevel ? typePalette : PAL_RESORT_HOTEL;
                            DrawSpriteImmediate(screenX, screenY, 0, SPRITE_ORIGIN_LARGE, SPRITE_ORIGIN_LARGE,
                                g_curSprite, SPRITE_FULL_SCALE, ghostFlags);
                            break;
                        case SCENERY_AIRSTRIP:
                            g_curSprite = GetFlicBankSprite(buildingLevel + BANK_AIRSTRIP, 0,
                                (g_camDirIdx / QUARTER_TURN + g_curBuildingTurns) & FACING_MASK);
                            ghostFlags |= buildingLevel ? typePalette : PAL_AIRSTRIP;
                            DrawSpriteImmediate(screenX, screenY, 0, SPRITE_ORIGIN_LARGE, SPRITE_ORIGIN_LARGE,
                                g_curSprite, SPRITE_FULL_SCALE, ghostFlags);
                            break;
                        case SCENERY_MARINA:
                            g_curSprite = GetFlicBankSprite(buildingLevel + BANK_MARINA, 0,
                                (g_camDirIdx / QUARTER_TURN + g_curBuildingTurns) & FACING_MASK);
                            ghostFlags |= buildingLevel ? typePalette : PAL_MARINA;
                            DrawSpriteImmediate(screenX, screenY, 0, SPRITE_ORIGIN_BUILDING, SPRITE_ORIGIN_BUILDING,
                                g_curSprite, SPRITE_FULL_SCALE, ghostFlags);
                            break;
                        case SCENERY_PUTTING_GREEN:
                            g_curSprite =
                                GetFlicBankSprite(buildingLevel + BANK_PUTTING_GREEN_ICON, g_currentThemeId, 0);
                            DrawSpriteImmediate(screenX, screenY, 0, SPRITE_ORIGIN_BUILDING, SPRITE_ORIGIN_BUILDING,
                                g_curSprite, SPRITE_FULL_SCALE, ghostFlags | PAL_PRACTICE_ICON);
                            break;
                        case SCENERY_DRIVING_RANGE:
                            g_curSprite =
                                GetFlicBankSprite(buildingLevel + BANK_DRIVING_RANGE_ICON, g_currentThemeId, 0);
                            DrawSpriteImmediate(screenX, screenY, 0, SPRITE_ORIGIN_BUILDING, SPRITE_ORIGIN_BUILDING,
                                g_curSprite, SPRITE_FULL_SCALE, ghostFlags | PAL_PRACTICE_ICON);
                            break;
                        case SCENERY_CLUBHOUSE:
                            g_curSprite = GetFlicBankSprite(BANK_CLUBHOUSE, 0,
                                (g_camDirIdx / QUARTER_TURN + g_curBuildingTurns) & FACING_MASK);
                            DrawSpriteImmediate(screenX, screenY, 0, SPRITE_ORIGIN_BUILDING, SPRITE_ORIGIN_BUILDING,
                                g_curSprite, SPRITE_FULL_SCALE, ghostFlags | PAL_CLUBHOUSE);
                            break;
                        case 1: {
                            int benchStyle = g_selectedVariant % BENCH_STYLES;
                            int benchDir = -1;
                            int walkDir = 0;
                            do {
                                if (IsNeighborWalkable(g_pointerPosX, g_pointerPosY, walkDir) != 0) {
                                    benchDir = ((walkDir - g_camDirIdx) + 3) & DIRECTION_MASK;
                                    g_curSprite = GetFlicBankSprite(benchStyle + BANK_BENCH, 0,
                                        (-(benchDir / 2) - 2) & FACING_MASK);
                                    DrawSpriteImmediate(
                                        screenX + g_dirRowStep[benchDir] * g_cameraZoom * -BENCH_DX_PER_ZOOM,
                                        screenY
                                            - (g_dirColStep[benchDir] * g_cameraZoom * BENCH_DY_HALVES_PER_ZOOM) / 2,
                                        screenY, SPRITE_ORIGIN_SMALL, SPRITE_ORIGIN_SMALL, g_curSprite,
                                        SPRITE_FULL_SCALE, benchStyle + PAL_BENCH | SPRITE_TRANSLUCENT);
                                }
                                walkDir += 2;
                            } while (walkDir < DIRECTION_COUNT);
                            if (benchDir == -1) {
                                siteColor = COLOR_RED;
                            }
                            break;
                        }
                        case SCENERY_SCENIC_BRIDGE: {
                            int bridgeStyle = g_selectedVariant % BRIDGE_STYLES;
                            g_curSprite = GetFlicBankSprite(bridgeStyle + BANK_SCENIC_BRIDGE, 0, 0);
                            DrawSpriteImmediate(screenX, screenY, screenY, SPRITE_ORIGIN_SMALL, SPRITE_ORIGIN_SMALL,
                                g_curSprite, SPRITE_FULL_SCALE, PAL_SCENIC_BRIDGE | SPRITE_TRANSLUCENT);
                            if (g_gameTileTerrainKind[g_pointerPosX * GRID_SIZE + g_pointerPosY] != TERRAIN_WATER) {
                                siteColor = COLOR_RED;
                            }
                            break;
                        }
                        }
                        if (g_selectedBuildingKind <= SCENERY_PUTTING_GREEN) {
                            DrawTextCenteredWithShadow(g_printBuffer, screenX, screenY - 8, siteColor);
                        }
                        if (g_selectedBuildingKind == SCENERY_HOME_SITE) {
                            if (siteClearCost != -1) {
                                DrawPanelFrame(LOT_PANEL_X, LOT_PANEL_Y, LOT_PANEL_WIDTH, LOT_PANEL_HEIGHT, 1);
                                strcpy(g_printBuffer, "Lot value: ");
                                strcat(g_printBuffer,
                                    itoa((CalculateLotScore(g_pointerPosX, g_pointerPosY) / APPEAL_PER_MONEY_UNIT)
                                             * DOLLARS_PER_MONEY_UNIT,
                                        g_numberBuf, 10));
                                DrawTextCenteredWithShadow(g_printBuffer, LOT_PANEL_CENTER_X, LOT_VALUE_Y,
                                    COLOR_BRIGHT_GREEN);
                                int clearingCost = siteClearCost;
                                strcpy(g_printBuffer, "Site clear: ");
                                strcat(g_printBuffer, itoa(siteClearCost * DOLLARS_PER_MONEY_UNIT, g_numberBuf, 10));
                                DrawTextCenteredWithShadow(g_printBuffer, LOT_PANEL_CENTER_X, LOT_CLEARING_Y,
                                    COLOR_RED);
                                strcpy(g_printBuffer, "Site prep.: ");
                                int sitePrepCost = g_facilityTypes[SCENERY_HOME_SITE].cost;
                                strcat(g_printBuffer, itoa(sitePrepCost * DOLLARS_PER_MONEY_UNIT, g_numberBuf, 10));
                                DrawTextCenteredWithShadow(g_printBuffer, LOT_PANEL_CENTER_X, LOT_PREP_Y, COLOR_RED);
                                g_simScreen.line(LOT_RULE_LEFT, LOT_RULE_Y, LOT_RULE_RIGHT, LOT_RULE_Y,
                                    COLOR_LIGHT_GREY);
                                int lotScore = CalculateLotScore(g_pointerPosX, g_pointerPosY);
                                int profit =
                                    (((lotScore / APPEAL_PER_MONEY_UNIT) - g_facilityTypes[SCENERY_HOME_SITE].cost)
                                        - clearingCost)
                                    * DOLLARS_PER_MONEY_UNIT;
                                strcpy(g_printBuffer, "Profit: ");
                                strcat(g_printBuffer, itoa(profit, g_numberBuf, 10));
                                DrawTextCenteredWithShadow(g_printBuffer, LOT_PANEL_CENTER_X, LOT_PROFIT_Y,
                                    COLOR_BRIGHT_GREEN);
                                strcpy(g_printBuffer, "$");
                                strcat(g_printBuffer, itoa(profit, g_numberBuf, 10));
                                DrawTextCenteredWithShadow(g_printBuffer, screenX, screenY + 8,
                                    profit > 0 ? COLOR_BRIGHT_GREEN : COLOR_RED);
                            }
                        } else if (0 < siteClearCost) {
                            strcpy(g_printBuffer, "-");
                            strcat(g_printBuffer, itoa(siteClearCost * DOLLARS_PER_MONEY_UNIT, g_numberBuf, 10));
                            DrawTextCenteredWithShadow(g_printBuffer, screenX, screenY + 8, COLOR_RED);
                        }
                    }
                }
                if (g_toolbarView == TOOLBAR_BUILD_COURSE) {
                    if ((((cursorY < TOOLBAR_TOP) && (g_buildToolbarShown == 0)) && (g_paintTerrain != -1))
                        && ((g_tileExtra[g_pointerPosY + g_pointerPosX * GRID_SIZE] & TILE_ANIMATING) == 0)) {
                        int tileCornerY;
                        int tileCornerX;
                        WorldToScreen(g_pointerPosX << TILE_WORLD_SHIFT, g_pointerPosY << TILE_WORLD_SHIFT,
                            &tileCornerX, &tileCornerY, 0);
                        TileToScreenCached(g_pointerPosX, g_pointerPosY, &cursorX, &cursorY);
                        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                        int iconLift = tileHalfHeight;
                        if (g_paintTerrain < PAINT_SPECIAL_BASE) {
                            if (g_paintTerrain == PAINT_DEMOLISH) {
                                int demolishCost = BuildDemolishTooltipText(g_pointerPosX, g_pointerPosY);
                                DrawTextCenteredWithShadow(g_printBuffer, cursorX + BRUSH_LABEL_DX,
                                    cursorY - BRUSH_LABEL_DY, demolishCost != 0 ? COLOR_BRIGHT_GREEN : COLOR_WHITE);
                            } else {
                                if (g_paintTerrain != TERRAIN_SAND_TRAP) {
                                    int paletteSlot = 0;
                                    do {
                                        if ((g_keyOrder[paletteSlot] == g_paintTerrain)
                                            || ((g_squareTypeTable[g_paintTerrain].category == CATEGORY_TREES
                                                 && (paletteSlot == TREE_ICON_SLOT)))) {
                                            g_themePaintImgs[paletteSlot + g_currentThemeId * ICONS_PER_THEME]
                                                .draw_translucent(g_pMenu, cursorX - g_tileSpan, cursorY - iconLift,
                                                    0.5, 0, 1);
                                        }
                                        paletteSlot++;
                                    } while (paletteSlot < PAINT_SLOT_COUNT);
                                    if (((g_paintTerrain == TERRAIN_GREEN)
                                            && (g_allHolesArray[g_nextOpenHoleIndex].aimX == 0))
                                        && (g_nextOpenHoleIndex < CLUBHOUSE_HOLE)) {
                                        g_curSprite = GetFlicBankSprite(g_currentThemeId + BANK_FLAG_OPEN,
                                            g_clock % g_flicBankFrameLens[g_currentThemeId + BANK_FLAG_OPEN], 1);
                                        DrawSpriteImmediate(cursorX - BRUSH_LABEL_DX, cursorY, cursorY,
                                            SPRITE_ORIGIN_SMALL, SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE,
                                            PAL_FLAG | SPRITE_TRANSLUCENT);
                                    }
                                } else {
                                    char trapIconForFacing[4];
                                    trapIconForFacing[0] = '\0';
                                    trapIconForFacing[1] = 3;
                                    trapIconForFacing[2] = 1;
                                    trapIconForFacing[3] = 2;
                                    int trapFacing =
                                        (g_curBuildingTurns - g_camDirIdx / QUARTER_TURN + FACING_COUNT) % FACING_COUNT;
                                    if (trapFacing % FACING_COUNT == 0) {
                                        g_themePaintImgs[SAND_TRAP_ICON_SLOT + g_currentThemeId * ICONS_PER_THEME]
                                            .draw_translucent(g_pMenu, cursorX - g_tileSpan, cursorY - tileHalfHeight,
                                                0.5, 0, 1);
                                    } else {
                                        g_themePaintImgs[TURNED_TRAP_ICONS
                                                         + trapIconForFacing[trapFacing] % FACING_COUNT
                                                         + g_currentThemeId * ICONS_PER_THEME]
                                            .draw_translucent(g_pMenu, cursorX - g_tileSpan, cursorY - tileHalfHeight,
                                                0.5, 0, 1);
                                    }
                                }
                                g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
                                int paintType = g_paintTerrain;
                                strcpy(g_printBuffer, g_squareTypeTable[g_paintTerrain].name);
                                g_printBuffer[0] -= 0x20;
                                if ((paintType == 1) && ((g_curBuildingTurns & 1) != 0)) {
                                    strcpy(g_printBuffer, "Tricky Green");
                                }
                                if ((g_squareTypeTable[paintType].category == CATEGORY_TREES)
                                    && (g_squareTypeTable[g_gameTileTerrainKind[g_pointerPosX * GRID_SIZE
                                                                                + g_pointerPosY]]
                                            .category
                                        != CATEGORY_TREES)) {
                                    int heightSample;
                                    if (paintType == TERRAIN_TREE) {
                                        saplingBank = BANK_TREE_SMALL;
                                        heightSample = CalculateTerrainHeight(g_pointerPosX << TREE_TINT_SAMPLE_SHIFT,
                                            g_pointerPosY << TREE_TINT_SAMPLE_SHIFT);
                                        saplingPalette = ((heightSample / TINT_BAND_HEIGHT) & TINT_MASK) + PAL_TREE;
                                        paintType = g_paintTerrain;
                                    }
                                    if (paintType == TERRAIN_PALM_TREE) {
                                        saplingBank = BANK_PALM_SMALL;
                                        heightSample = CalculateTerrainHeight(g_pointerPosX << PALM_TINT_SAMPLE_SHIFT,
                                            g_pointerPosY << PALM_TINT_SAMPLE_SHIFT);
                                        saplingPalette = ((heightSample / TINT_BAND_HEIGHT) & TINT_MASK) + PAL_PALM;
                                        paintType = g_paintTerrain;
                                    }
                                    if (paintType == TERRAIN_PINE_TREE) {
                                        saplingBank = BANK_PINE_SMALL;
                                        saplingPalette = PAL_PINE;
                                        if ((g_curBuildingTurns / TREE_SIZES & 1U) != 0) {
                                            saplingBank = BANK_FIR_SMALL;
                                            if (g_currentThemeId == THEME_TROPICAL) {
                                                saplingPalette = PAL_PINE_ALT;
                                            } else if (g_currentThemeId == THEME_DESERT) {
                                                saplingPalette = PAL_PINE_ALT;
                                            } else if (g_currentThemeId == THEME_LINKS) {
                                                saplingPalette = PAL_PINE_ALT;
                                            }
                                        }
                                    }
                                    saplingBank += g_curBuildingTurns % TREE_SIZES;
                                    if (paintType == TERRAIN_ELM_TREE) {
                                        saplingBank = BANK_WILLOW_TREE;
                                        saplingPalette = PAL_WILLOW;
                                    }
                                    g_curSprite = GetFlicBankSprite(saplingBank, g_flicBankFrameLens[saplingBank] - 1,
                                        (g_camDirIdx / QUARTER_TURN + g_pointerPosY * -2 + g_pointerPosX)
                                            & FACING_MASK);
                                    DrawSpriteImmediate(cursorX, cursorY, cursorY, SPRITE_ORIGIN_BUILDING,
                                        SPRITE_ORIGIN_BUILDING, g_curSprite, SPRITE_FULL_SCALE,
                                        saplingPalette | SPRITE_TRANSLUCENT);
                                    paintType = g_paintTerrain;
                                }
                                if ((paintType == 1) && ((g_curBuildingTurns & 1) != 0)) {
                                    DrawTextCenteredWithShadow(g_printBuffer, cursorX + BRUSH_LABEL_DX,
                                        cursorY - BRUSH_TOOL_LABEL_DY, COLOR_WHITE);
                                    paintType = g_paintTerrain;
                                }
                                int holeEndScreenX = -1;
                                int holeYards;
                                int holeEndScreenY;
                                if (paintType == 1) {
                                    if (g_allHolesArray[g_nextOpenHoleIndex].aimX == 0
                                        && g_allHolesArray[g_nextOpenHoleIndex].teeX != 0
                                        && g_nextOpenHoleIndex < CLUBHOUSE_HOLE) {
                                        if (TileToScreenCached(g_allHolesArray[g_nextOpenHoleIndex].teeX,
                                                g_allHolesArray[g_nextOpenHoleIndex].teeY, &holeEndScreenX,
                                                &holeEndScreenY)
                                            != 0) {
                                            DrawTerrainLineWrapper(holeEndScreenX, holeEndScreenY, cursorX, cursorY,
                                                COLOR_WHITE, LINE_WIDTH_THIN, LINE_ALPHA);
                                            holeYards = Math_Distance2D(
                                                (g_pointerPosX - g_allHolesArray[g_nextOpenHoleIndex].teeX)
                                                    * YARDS_PER_TILE,
                                                (g_pointerPosY - g_allHolesArray[g_nextOpenHoleIndex].teeY)
                                                    * YARDS_PER_TILE);
                                            g_printBuffer[0] = '\0';
                                            strcat(g_printBuffer, itoa(holeYards, g_numberBuf, 10));
                                            strcat(g_printBuffer, " yards");
                                            DrawTextCenteredAtY(g_printBuffer, (holeEndScreenX + cursorX) / 2,
                                                (cursorY + holeEndScreenY) / 2, COLOR_WHITE);
                                        }
                                        paintType = g_paintTerrain;
                                    }
                                }
                                if ((((paintType == 0) && (g_allHolesArray[g_nextOpenHoleIndex].teeX == 0))
                                        && (g_allHolesArray[g_nextOpenHoleIndex].aimX != 0))
                                    && ((g_nextOpenHoleIndex < CLUBHOUSE_HOLE
                                         && TileToScreenCached(g_allHolesArray[g_nextOpenHoleIndex].aimX,
                                                g_allHolesArray[g_nextOpenHoleIndex].aimY, &holeEndScreenX,
                                                &holeEndScreenY)
                                                != 0))) {
                                    DrawTerrainLineWrapper(holeEndScreenX, holeEndScreenY, cursorX, cursorY,
                                        COLOR_WHITE, LINE_WIDTH_THIN, LINE_ALPHA);
                                    holeYards = Math_Distance2D(
                                        (g_pointerPosX - g_allHolesArray[g_nextOpenHoleIndex].aimX) * YARDS_PER_TILE,
                                        (g_pointerPosY - g_allHolesArray[g_nextOpenHoleIndex].aimY) * YARDS_PER_TILE);
                                    g_printBuffer[0] = '\0';
                                    strcat(g_printBuffer, itoa(holeYards, g_numberBuf, 10));
                                    strcat(g_printBuffer, " yards");
                                    DrawTextCenteredAtY(g_printBuffer, (holeEndScreenX + cursorX) / 2,
                                        (cursorY + holeEndScreenY) / 2, COLOR_WHITE);
                                }
                            }
                        } else {
                            if (g_paintTerrain == PAINT_PATH) {
                                DrawTextCenteredWithShadow("Path", cursorX + BRUSH_LABEL_DX, cursorY - PATH_LABEL_DY,
                                    COLOR_WHITE);
                            }
                            if (g_paintTerrain == PAINT_PATH_BENCH) {
                                int benchStyle = g_curBuildingTurns % PATH_BENCH_STYLES;
                                for (;;) {
                                    if (IsNeighborWalkable(g_pointerPosX, g_pointerPosY, 0)) {
                                        int benchFacing = (3 - g_camDirIdx) & DIRECTION_MASK;
                                        g_curSprite = GetFlicBankSprite(benchStyle + BANK_BENCH, 0,
                                            (-2 - benchFacing / 2) & FACING_MASK);
                                        DrawSpriteImmediate(cursorX, cursorY, cursorY, SPRITE_ORIGIN_SMALL,
                                            SPRITE_ORIGIN_SMALL, g_curSprite, SPRITE_FULL_SCALE,
                                            benchStyle + PAL_BENCH_TEST_BASE | SPRITE_TRANSLUCENT);
                                    }
                                }
                            }
                        }
                    }
                }
                if (g_toolbarView == TOOLBAR_SHOT && cursorY < TOOLBAR_TOP) {
                    g_paintTerrain = -1;
                    g_selectedBuildingKind = -1;
                    g_selectedItemIndex = -1;
                    g_displayFocus = 0;
                    g_simScreen.get_mouse_pos(&cursorX, &cursorY);
                    int screenY;
                    int screenX;
                    WorldToScreen(g_pointerPosX * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                        g_pointerPosY * TILE_WORLD_SIZE + TILE_WORLD_HALF, &screenX, &screenY, 0);
                    int snapDistance = Math_Distance2D(cursorX - screenX, cursorY - screenY);
                    command = mouseTileX = g_pointerPosX;
                    scanDir = g_pointerPosY;
                    g_golferPool[g_watchedPlayerIdx].flags &= ~GOLFER_EXACT_AIM;
                    for (i = 0; i < 2; i++) {
                        for (j = 0; j < 2; j++) {
                            WorldToScreen((g_pointerPosX + i) * TILE_WORLD_SIZE, (g_pointerPosY + j) * TILE_WORLD_SIZE,
                                &screenX, &screenY, 0);
                            int cornerDistance = Math_Distance2D(cursorX - screenX, cursorY - screenY);
                            if (cornerDistance < snapDistance) {
                                snapDistance = cornerDistance;
                                g_pointerPosX = i + command;
                                g_pointerPosY = j + scanDir;
                                g_golferPool[g_watchedPlayerIdx].flags |= GOLFER_EXACT_AIM;
                            }
                        }
                    }
                    int aimY;
                    int aimX;
                    if ((g_golferPool[g_watchedPlayerIdx].flags & GOLFER_EXACT_AIM) != 0) {
                        aimX = g_pointerPosX << TILE_WORLD_SHIFT;
                        aimY = g_pointerPosY << TILE_WORLD_SHIFT;
                    } else {
                        aimX = g_pointerPosX * TILE_WORLD_SIZE + TILE_WORLD_HALF;
                        aimY = g_pointerPosY * TILE_WORLD_SIZE + TILE_WORLD_HALF;
                    }
                    g_golferPool[g_watchedPlayerIdx].shotFromX = g_pointerPosX;
                    g_golferPool[g_watchedPlayerIdx].shotFromY = g_pointerPosY;
                    int maxShotYards = CalculateGolferMaxDistance(g_watchedPlayerIdx);
                    if (g_shotSelectedKind == SHOT_HIGH_BACKSPIN) {
                        maxShotYards = (maxShotYards * BACKSPIN_REACH_NUM) / BACKSPIN_REACH_DEN;
                    }
                    if (g_shotSelectedKind == SHOT_LOW_PUNCH) {
                        maxShotYards = maxShotYards / 2;
                    } else if (g_shotSelectedKind != 0) {
                        if (0 < g_squareTypeTable
                                [g_gameTileTerrainKind[(g_golferPool[g_watchedPlayerIdx].ballX >> TILE_WORLD_SHIFT)
                                                           * GRID_SIZE
                                                       + (g_golferPool[g_watchedPlayerIdx].ballY >> TILE_WORLD_SHIFT)]]
                                    .lieGrade) {
                            g_shotSelectedKind = 0;
                            ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                        }
                    }
                    screenX = g_golferPool[g_watchedPlayerIdx].screenX;
                    screenY = g_golferPool[g_watchedPlayerIdx].screenY;
                    WorldToScreen(g_golferPool[g_watchedPlayerIdx].ballX, g_golferPool[g_watchedPlayerIdx].ballY,
                        &screenX, &screenY, 0);
                    int aimDistance = Math_Distance2D(g_golferPool[g_watchedPlayerIdx].ballX - aimX,
                        g_golferPool[g_watchedPlayerIdx].ballY - aimY);
                    int aimYards = (aimDistance * YARDS_PER_TILE) / TILE_WORLD_SIZE;
                    if (g_mouseClickDelay != 0) {
                        g_mouseClickDelay--;
                    }
                    if (screenX == -1) {
                        g_cameraTileX = (g_allHolesArray[g_golferPool[g_watchedPlayerIdx].holeIdx].aimX
                                            + (g_golferPool[g_watchedPlayerIdx].x >> TILE_WORLD_SHIFT) * 2)
                                        / 3;
                        g_cameraTileY = (g_allHolesArray[g_golferPool[g_watchedPlayerIdx].holeIdx].aimY
                                            + (g_golferPool[g_watchedPlayerIdx].y >> TILE_WORLD_SHIFT) * 2)
                                        / 3;
                    }
                    int shotYards = Clamp(aimYards, 0, maxShotYards);
                    int curveAngle = 0;
                    int baseAngle = CalculateAngle(aimX - g_golferPool[g_watchedPlayerIdx].ballX,
                        aimY - g_golferPool[g_watchedPlayerIdx].ballY);
                    int shotType = g_shotSelectedKind;
                    g_golferPool[g_watchedPlayerIdx].dir =
                        ((((baseAngle >> ANGLE_TO_SIXTEENTHS_SHIFT) & 0xf) + 1) >> 1) & DIRECTION_MASK;
                    int shotCurve = (abs(shotType) > 1) ? 0 : shotType * 2;
                    int pathDivisions = (shotType != SHOT_HIGH_BACKSPIN) ? PATH_DIVISIONS : BACKSPIN_PATH_DIVISIONS;
                    int loft = (shotType == SHOT_HIGH_BACKSPIN) ? LOFT_HIGH_BACKSPIN : LOFT_NORMAL;
                    int zoom;
                    if (shotType == SHOT_LOW_PUNCH) {
                        loft = 0;
                        zoom = g_cameraZoom;
                    } else {
                        zoom = g_cameraZoom;
                    }
                    int lineWidth = 2;
                    int loftScale = zoom * loft;
                    if (g_zoomView != 0) {
                        lineWidth = 3;
                    }
                    if (zoom < DEFAULT_ZOOM) {
                        lineWidth = 1;
                    }
                    int club = Clamp(((maxShotYards - shotYards) * CLUB_PICK_NUM) / (maxShotYards * CLUB_PICK_DEN),
                        g_golferPool[g_watchedPlayerIdx].holeStrokes != 0, LAST_AIMED_CLUB);
                    g_golferPool[g_watchedPlayerIdx].club = club;
                    strcpy(g_printBuffer, "Club: ");
                    AppendClubName(club);
                    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                    DrawTextCenteredAtY(g_printBuffer, SHOT_PANEL_X, SHOT_PANEL_CLUB_Y, COLOR_BLUE);
                    strcpy(g_printBuffer, "Attitude: ");
                    switch (Clamp(g_golferPool[g_watchedPlayerIdx].attitude, -ATTITUDE_LIMIT, ATTITUDE_LIMIT)) {
                    case 0: strcat(g_printBuffer, "calm"); break;
                    case 1: strcat(g_printBuffer, "determined"); break;
                    case 2: strcat(g_printBuffer, "pumped"); break;
                    case 3:
                    case 4: strcat(g_printBuffer, "invincible"); break;
                    case -1: strcat(g_printBuffer, "worried"); break;
                    case -2: strcat(g_printBuffer, "upset"); break;
                    case -3: strcat(g_printBuffer, "mad"); break;
                    case -4: strcat(g_printBuffer, "furious"); break;
                    }
                    DrawTextCenteredAtY(g_printBuffer, SHOT_PANEL_X, SHOT_PANEL_ATTITUDE_Y, COLOR_BLUE);
                    strcpy(g_printBuffer, "Lie: ");
                    strcat(g_printBuffer, g_squareTypeTable[GetTileAtPosition(g_golferPool[g_watchedPlayerIdx].ballX,
                                                                g_golferPool[g_watchedPlayerIdx].ballY)]
                                              .name);
                    DrawTextCenteredAtY(g_printBuffer, SHOT_PANEL_X, SHOT_PANEL_LIE_Y, COLOR_BLUE);
                    strcpy(g_printBuffer, "Distance: ");
                    strcat(g_printBuffer, itoa((shotYards * YARDS_PER_TILE) / YARDS_PER_TILE, g_numberBuf, 10));
                    strcat(g_printBuffer, " yds");
                    DrawTextCenteredAtY(g_printBuffer, SHOT_PANEL_X, SHOT_PANEL_DISTANCE_Y, COLOR_BLUE);
                    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
                    int skillListX = SKILL_LIST_X;
                    int skillListY = SKILL_LIST_TOP;
                    for (int skillIndex = 0; skillIndex < SHOT_SKILL_COUNT; skillIndex++) {
                        if (g_golferSkillGrades[skillIndex] != 0) {
                            g_golferPool[g_watchedPlayerIdx].skillMask |= (1 << skillIndex);
                        } else {
                            g_golferPool[g_watchedPlayerIdx].skillMask &= ~(1 << skillIndex);
                        }
                        int skillBit = 1 << skillIndex;
                        if ((skillBit & g_golferPool[g_watchedPlayerIdx].skillMask) != 0) {
                            strcpy(g_printBuffer, g_skillNames[skillIndex]);
                            strcat(g_printBuffer, " +");
                            strcat(g_printBuffer,
                                itoa(g_golferSkillGrades[skillIndex] * SKILL_PERCENT_PER_POINT, g_numberBuf, 10));
                            strcat(g_printBuffer, "%");
                            int skillApplies = false;
                            switch (skillBit) {
                            case 1: skillApplies = true; break;
                            case SKILL_LONG_DRIVER:
                            case SKILL_ACCURATE_DRIVER:
                                skillApplies = (g_golferPool[g_watchedPlayerIdx].club <= LAST_LONG_CLUB)
                                               && (g_golferPool[g_watchedPlayerIdx].holeStrokes == 0);
                                break;
                            case SKILL_ACCURATE_IRONS:
                                skillApplies = LAST_LONG_CLUB < g_golferPool[g_watchedPlayerIdx].club;
                                break;
                            case SKILL_DRAW_SHOT:
                                if (g_shotSelectedKind == 1)
                                    skillApplies = true;
                                break;
                            case SKILL_FADE_SHOT:
                                if (g_shotSelectedKind == -1)
                                    skillApplies = true;
                                break;
                            case SKILL_HIGH_BACKSPIN:
                                if (g_shotSelectedKind == SHOT_HIGH_BACKSPIN)
                                    skillApplies = true;
                                break;
                            case SKILL_RECOVERY:
                                if (0 < g_squareTypeTable[GetTileAtPosition(g_golferPool[g_watchedPlayerIdx].ballX,
                                                              g_golferPool[g_watchedPlayerIdx].ballY)]
                                        .lieGrade)
                                    skillApplies = true;
                                break;
                            }
                            DrawTextWithColor(g_printBuffer, skillListX, skillListY,
                                skillApplies ? COLOR_WHITE : COLOR_GREY);
                            skillListY += SKILL_LINE_HEIGHT;
                            if (skillListY >= SKILL_LIST_BOTTOM) {
                                skillListX = SKILL_LIST_X2;
                                skillListY = SKILL_LIST_TOP;
                            }
                        }
                    }
                    int prevGroundX = screenX;
                    int prevGroundY = screenY;
                    TerrainMoveTo(screenX, screenY);
                    curveAngle = shotCurve * ANGLE_15_DEGREES;
                    int sampleDistance = ((shotYards * 5) / pathDivisions << TILE_WORLD_SHIFT) / YARDS_PER_TILE;
                    int sampleX =
                        FixedPointSin(curveAngle + baseAngle, sampleDistance) + g_golferPool[g_watchedPlayerIdx].ballX;
                    int sampleY =
                        g_golferPool[g_watchedPlayerIdx].ballY - FixedPointCos(curveAngle + baseAngle, sampleDistance);
                    WorldToScreen(sampleX, sampleY, &screenX, &screenY, 0);
                    DrawTerrainLineWrapper(prevGroundX, prevGroundY + 1, screenX, screenY + 1, COLOR_BLACK, lineWidth,
                        LINE_ALPHA);
                    prevGroundY = screenY;
                    prevGroundX = screenX;
                    screenY -= loftScale * shotYards / 3 / 8;
                    TerrainLineTo(screenX, screenY, COLOR_WHITE, lineWidth, LINE_ALPHA);
                    int secondDistance = ((shotYards * 9) / pathDivisions << TILE_WORLD_SHIFT) / YARDS_PER_TILE;
                    sampleX = FixedPointSin((shotCurve * ANGLE_45_DEGREES) / 4 + baseAngle, secondDistance)
                              + g_golferPool[g_watchedPlayerIdx].ballX;
                    sampleY = g_golferPool[g_watchedPlayerIdx].ballY
                              - FixedPointCos((shotCurve * ANGLE_45_DEGREES) / 4 + baseAngle, secondDistance);
                    WorldToScreen(sampleX, sampleY, &screenX, &screenY, 0);
                    DrawTerrainLineWrapper(prevGroundX, prevGroundY + 1, screenX, screenY + 1, COLOR_BLACK, lineWidth,
                        LINE_ALPHA);
                    prevGroundX = screenX;
                    prevGroundY = screenY;
                    screenY -= loftScale * shotYards / 2 / 8;
                    TerrainLineTo(screenX, screenY, COLOR_WHITE, lineWidth, LINE_ALPHA);
                    int thirdDistance = ((shotYards * 12) / pathDivisions << TILE_WORLD_SHIFT) / YARDS_PER_TILE;
                    sampleX = FixedPointSin(curveAngle / 2 + baseAngle, thirdDistance)
                              + g_golferPool[g_watchedPlayerIdx].ballX;
                    sampleY = g_golferPool[g_watchedPlayerIdx].ballY
                              - FixedPointCos(curveAngle / 2 + baseAngle, thirdDistance);
                    WorldToScreen(sampleX, sampleY, &screenX, &screenY, 0);
                    DrawTerrainLineWrapper(prevGroundX, prevGroundY + 1, screenX, screenY + 1, COLOR_BLACK, lineWidth,
                        LINE_ALPHA);
                    prevGroundX = screenX;
                    prevGroundY = screenY;
                    screenY -= loftScale * shotYards / 2 / 8;
                    TerrainLineTo(screenX, screenY, COLOR_WHITE, lineWidth, LINE_ALPHA);
                    int fourthDistance = ((shotYards * 15) / pathDivisions << TILE_WORLD_SHIFT) / YARDS_PER_TILE;
                    sampleX = FixedPointSin(curveAngle / 3 + baseAngle, fourthDistance)
                              + g_golferPool[g_watchedPlayerIdx].ballX;
                    sampleY = g_golferPool[g_watchedPlayerIdx].ballY
                              - FixedPointCos(curveAngle / 3 + baseAngle, fourthDistance);
                    WorldToScreen(sampleX, sampleY, &screenX, &screenY, 0);
                    DrawTerrainLineWrapper(prevGroundX, prevGroundY + 1, screenX, screenY + 1, COLOR_BLACK, lineWidth,
                        LINE_ALPHA);
                    prevGroundX = screenX;
                    prevGroundY = screenY;
                    screenY -= loftScale * shotYards / 3 / 8;
                    TerrainLineTo(screenX, screenY, COLOR_WHITE, lineWidth, LINE_ALPHA);
                    int fifthDistance = ((shotYards * 16) / pathDivisions << TILE_WORLD_SHIFT) / YARDS_PER_TILE;
                    sampleX = FixedPointSin(curveAngle / 4 + baseAngle, fifthDistance)
                              + g_golferPool[g_watchedPlayerIdx].ballX;
                    sampleY = g_golferPool[g_watchedPlayerIdx].ballY
                              - FixedPointCos(curveAngle / 4 + baseAngle, fifthDistance);
                    WorldToScreen(sampleX, sampleY, &screenX, &screenY, 0);
                    DrawTerrainLineWrapper(prevGroundX, prevGroundY + 1, screenX, screenY + 1, COLOR_BLACK, lineWidth,
                        LINE_ALPHA);
                    prevGroundX = screenX;
                    prevGroundY = screenY;
                    TerrainLineTo(screenX, screenY, COLOR_WHITE, lineWidth, LINE_ALPHA);
                    if (g_shotSelectedKind != SHOT_HIGH_BACKSPIN) {
                        int rollDistance = ((shotYards * 20) / pathDivisions << TILE_WORLD_SHIFT) / YARDS_PER_TILE;
                        int sampleX = FixedPointSin(baseAngle, rollDistance) + g_golferPool[g_watchedPlayerIdx].ballX;
                        int sampleY = g_golferPool[g_watchedPlayerIdx].ballY - FixedPointCos(baseAngle, rollDistance);
                        WorldToScreen(sampleX, sampleY, &screenX, &screenY, 0);
                        DrawTerrainLineWrapper(prevGroundX, prevGroundY + 1, screenX, screenY + 1, COLOR_BLACK,
                            lineWidth, LINE_ALPHA);
                        TerrainLineTo(screenX, screenY, COLOR_WHITE, lineWidth, LINE_ALPHA);
                    }
                }
                g_zoomDouble = 0;
                ProcessMouseMessages();
                command = TakePendingChar();
                unsigned key = TakePendingKey();
                if ((command != 0) && ((g_runModeFlags & GAME_TUTORIAL) != 0)) {
                    if (command == VK_ESCAPE) {
                        LoadSavedGame("Tutorial Save.sve", 1, 0, 0);
                        g_cameraTileX = g_buildingTable[0].row + TUTORIAL_CAMERA_OFFSET;
                        g_nextTutStepIdx = 0;
                        g_cameraTileY = g_buildingTable[0].col - TUTORIAL_CAMERA_OFFSET;
                        g_cameraZoom = DEFAULT_ZOOM;
                        g_runModeFlags &= ~GAME_TUTORIAL;
                    } else {
                        SetupTutorialMessage();
                    }
                    command = 0;
                }
                if (g_clock == g_deferEventStamp + TRIGGER_DELAY_TICKS) {
                    g_forceBits |= g_scheduledTriggerBits;
                    g_scheduledTriggerBits = 0;
                }
                if ((g_clock == CUSTOMIZE_PROMPT_CLOCK) && (g_level == 0)) {
                    if ((strcmp("Gary Golf", g_golferTypeArray[0].name) == 0)
                        && ((g_runModeFlags & GAME_PAUSED) == 0)) {
                        strcpy(g_printBuffer, "In SimGolf you are represented by a\ncharacter currently called ");
                        strcat(g_printBuffer, g_golferTypeArray[0].name);
                        strcat(g_printBuffer,
                            ".\nYou can customize this character's name,\ngender, and personality if you wish.");
                        strcat(g_printBuffer, "\n I'd like to customize my character.\n I'll stick with ");
                        strcat(g_printBuffer, g_golferTypeArray[0].name);
                        strcat(g_printBuffer, ".\n");
                        if (ShowActionDialog(SCREEN_CENTER_X, DIALOG_TOP, 1, 1, 0) == 0) {
                            g_golferPool[SCRATCH_GOLFER].typeIdx = 0;
                            g_golferPool[SCRATCH_GOLFER].persona = PERSONA_ROSTER;
                            if (g_golferTypeArray[0].fixedKind != 0) {
                                g_golferPool[SCRATCH_GOLFER].talents = g_golferTypeArray[0].fixedKind & SKILLS_ALL;
                            } else {
                                g_golferPool[SCRATCH_GOLFER].talents = SKILLS_ALL;
                            }
                            RenderGolferCustomizationScreen(0, SCRATCH_GOLFER);
                        }
                    }
                }
                if ((g_forceBits & (TRIGGER_UPSET_GOLFER | TRIGGER_ADVANCE_STORY)) != 0) {
                    waitingPair = -1;
                    int nearestGolferDistance = 0xffff;
                    for (int golferIndex = 0; golferIndex < GOLFER_COUNT; golferIndex++) {
                        int golferDistance;
                        if (((0 < g_golferPool[golferIndex].holeIdx) && (g_golferPool[golferIndex].screenX != -1))
                            && (golferDistance = Math_Distance2D(g_golferPool[golferIndex].screenX - VIEW_CENTER_X,
                                    g_golferPool[golferIndex].screenY - VIEW_CENTER_Y),
                                golferDistance < nearestGolferDistance)) {
                            nearestGolferDistance = golferDistance;
                            waitingPair = golferIndex;
                        }
                    }
                    if ((waitingPair != -1) && ((g_forceBits & TRIGGER_UPSET_GOLFER) != 0)) {
                        g_golferPool[waitingPair].enjoyment = -1;
                        SetGolferAction(waitingPair, ACTION_UPSET, 0x14);
                    }
                    g_forceBits &= ~TRIGGER_UPSET_GOLFER;
                    if ((waitingPair != -1) && ((g_forceBits & TRIGGER_ADVANCE_STORY) != 0)) {
                        if (((g_golferPool[waitingPair].flags & GOLFER_STORY_LEAD) != 0)
                            || ((g_golferPool[g_golferPool[waitingPair].partner].flags & GOLFER_STORY_LEAD) != 0)) {
                            if ((g_golferPool[waitingPair].flags & GOLFER_STORY_LEAD) != 0) {
                                g_golferPool[g_golferPool[waitingPair].partner]
                                    .holeResult[g_golferPool[waitingPair].holeIdx] = g_rng.NextInt(
                                    g_golferPool[g_golferPool[waitingPair].partner].storyChapter << HOLE_RESULT_SHIFT);
                                if ((g_golferPool[g_golferPool[waitingPair].partner].slotParams[0] & MOOD_ANNOYED)
                                    == 0) {
                                    g_golferPool[g_golferPool[waitingPair].partner]
                                        .holeResult[g_golferPool[waitingPair].holeIdx] =
                                        g_rng.NextInt(STORY_RESULT_RANGE);
                                    if ((g_golferPool[g_golferPool[waitingPair].partner].slotParams[0] & MOOD_PLEASED)
                                        != 0) {
                                        g_golferPool[g_golferPool[waitingPair].partner]
                                            .holeResult[g_golferPool[waitingPair].holeIdx] = STORY_RESULT_SPECIAL;
                                    }
                                }
                            } else {
                                int resultRange = g_golferPool[waitingPair].storyChapter << HOLE_RESULT_SHIFT;
                                g_golferPool[waitingPair].holeResult[g_golferPool[waitingPair].holeIdx] =
                                    g_rng.NextInt(resultRange);
                                if ((g_golferPool[waitingPair].slotParams[0] & MOOD_ANNOYED) == 0) {
                                    g_golferPool[waitingPair].holeResult[g_golferPool[waitingPair].holeIdx] =
                                        g_rng.NextInt(STORY_RESULT_RANGE);
                                    if ((g_golferPool[waitingPair].slotParams[0] & MOOD_PLEASED) != 0) {
                                        g_golferPool[waitingPair].holeResult[g_golferPool[waitingPair].holeIdx] =
                                            STORY_RESULT_SPECIAL;
                                    }
                                }
                            }
                            AdvanceStoryChapter(((g_golferPool[waitingPair].flags & GOLFER_STORY_LEAD) != 0)
                                                    ? waitingPair
                                                    : (waitingPair ^ 1),
                                1);
                            g_golferPool[waitingPair].flags |= GOLFER_STORY_ADVANCED;
                            g_golferPool[g_golferPool[waitingPair].partner].flags |= GOLFER_STORY_ADVANCED;
                        }
                    }
                    g_forceBits &= ~TRIGGER_ADVANCE_STORY;
                }
                int redispatch;
                do {
                    redispatch = 0;
                    if (command != 0) {
                        g_paintedCameraTileX = -1;
                        if (g_reportScreenId != 0 && command == KEY_CLOSE_PANEL) {
                            command = 0;
                        }
                        if (command != KEY_PAUSE) {
                            g_reportScreenId = '\0';
                        }
                    }
                    if ((g_runModeFlags & GAME_SCRIPT_PLAYING) != 0) {
                        PlayScriptStep();
                    }
                    int jumpHole;
                    switch (command) {
                        int row;
                    case KEY_FAIRWAY:
                        if (g_toolbarView == TOOLBAR_SHOT) {
                            g_shotSelectedKind = -1;
                        } else {
                            g_buildToolbarShown = 0;
                            g_toolbarView = TOOLBAR_BUILD_COURSE;
                            SelectScreenItem(TERRAIN_FAIRWAY, 1);
                        }
                        break;
                    case KEY_ROUGH:
                        g_buildToolbarShown = 0;
                        g_toolbarView = TOOLBAR_BUILD_COURSE;
                        SelectScreenItem(g_currentThemeId == THEME_DESERT ? TERRAIN_DEEP_ROUGH : TERRAIN_ROUGH, 1);
                        break;
                    case KEY_GREEN:
                        g_buildToolbarShown = 0;
                        g_toolbarView = TOOLBAR_BUILD_COURSE;
                        SelectScreenItem(g_paintTerrain != TERRAIN_GREEN, 1);
                        g_curBuildingTurns = 0;
                        break;
                    case KEY_SAND:
                        if (g_toolbarView == TOOLBAR_SHOT) {
                            g_shotSelectedKind = 0;
                        } else {
                            g_buildToolbarShown = 0;
                            g_toolbarView = TOOLBAR_BUILD_COURSE;
                            SelectScreenItem(TERRAIN_SAND_TRAP, 1);
                        }
                        break;
                    case KEY_LOW_PUNCH:
                        if (g_toolbarView == TOOLBAR_SHOT) {
                            g_shotSelectedKind = SHOT_LOW_PUNCH;
                        }
                        break;
                    case KEY_TREES:
                        g_buildToolbarShown = 0;
                        g_toolbarView = TOOLBAR_BUILD_COURSE;
                        SelectScreenItem(TERRAIN_TREE, 1);
                        break;
                    case KEY_WATER:
                        g_buildToolbarShown = 0;
                        g_toolbarView = TOOLBAR_BUILD_COURSE;
                        SelectScreenItem(TERRAIN_WATER, 1);
                        break;
                    case KEY_RAVINE:
                        g_buildToolbarShown = 0;
                        g_toolbarView = TOOLBAR_BUILD_COURSE;
                        SelectScreenItem(TERRAIN_RAVINE, 1);
                        break;
                    case KEY_HOLE:
                        if (g_toolbarView == TOOLBAR_SHOT) {
                            g_shotSelectedKind = SHOT_HIGH_BACKSPIN;
                        } else {
                            OpenHoleForPlay();
                        }
                        break;
                    case KEY_DRAW_SHOT: g_shotSelectedKind = 1; break;
                    case KEY_CANCEL_MATCH:
                        if ((g_runModeFlags & GAME_TOURNAMENT) == 0) {
                            if (0 < g_watchedPlayerIdx) {
                                strcpy(g_printBuffer, "Are you sure you want to\nCANCEL this match...\n");
                                strcat(g_printBuffer, " Yes, cancel.\n No, keep playing.\n");
                                if (ShowActionDialog(SCREEN_CENTER_X, CONFIRM_DIALOG_TOP, 1, 1, 0) == 0) {
                                    for (int golferIndex = 0; golferIndex < GOLFER_COUNT; golferIndex++) {
                                        if ((g_golferPool[golferIndex].flags & GOLFER_PRO_MATCH) != 0) {
                                            g_golferPool[golferIndex].flags &= ~GOLFER_PRO_MATCH;
                                            g_golferPool[golferIndex].talents &= SKILLS_ALL;
                                            g_golferPool[golferIndex].talents -= 1 << g_rng.NextInt(TALENT_BITS);
                                            strcpy(g_printBuffer, "Match canceled.");
                                            ShowStoryMessage(COLOR_BLACK, 0, -1);
                                            if (g_toolbarView == TOOLBAR_SHOT) {
                                                g_toolbarView = TOOLBAR_BUILDINGS;
                                            }
                                            ProcessVIPGolferEvent(golferIndex);
                                            ClearGolferSlotAndNamedScenery(golferIndex);
                                            g_watchedPlayerIdx = -1;
                                        }
                                    }
                                }
                            }
                        } else {
                            strcpy(g_printBuffer, "Are you sure you want to\nCANCEL this tournament...\n");
                            strcat(g_printBuffer, " Yes, cancel.\n No, keep playing.\n");
                            if (ShowActionDialog(SCREEN_CENTER_X, CONFIRM_DIALOG_TOP, 1, 1, 0) == 0) {
                                g_reportScreenId = 0;
                                WaitForInputRelease();
                                g_runModeFlags &= ~(GAME_TOURNAMENT | GAME_TOURNAMENT_OFFERED);
                                EndTournament();
                                g_winnersPurseK = 0;
                                g_watchedPlayerIdx = -1;
                            }
                        }
                        break;
                    case KEY_UNZOOM:
                        if (g_zoomView != 0) {
                            g_zoomView = 0;
                            g_curPreviousCamScale = 1;
                        } else {
                            if (1 < g_cameraZoom) {
                                g_cameraZoom = g_cameraZoom / 2;
                            }
                            g_tileSpan = g_cameraZoom * TILE_SPAN_PER_ZOOM;
                            g_paintTerrain = -1;
                            g_selectedBuildingKind = -1;
                        }
                        break;
                    case KEY_ZOOM:
                        if (g_cameraZoom < MAX_ZOOM) {
                            g_cameraZoom *= 2;
                            g_tileSpan = g_cameraZoom * TILE_SPAN_PER_ZOOM;
                        }
                        break;
                    case KEY_RECENTER:
                        if (g_camDirIdx == 0) {
                            g_cameraTileX = g_buildingTable[0].row;
                            g_cameraTileY = g_buildingTable[0].col;
                        }
                        g_camDirIdx = 0;
                        break;
                    case KEY_PATHWAY:
                        g_toolbarView = TOOLBAR_BUILD_COURSE;
                        g_buildToolbarShown = 1;
                        g_selectedBuildingKind = 0;
                        break;
                    case KEY_BENCHES:
                        g_toolbarView = TOOLBAR_BUILD_COURSE;
                        g_buildToolbarShown = 1;
                        g_selectedBuildingKind = 1;
                        g_selectedVariant = g_rng.NextInt(BENCH_STYLES);
                        break;
                    case VK_ESCAPE: command = 0;
                    case VK_RETURN:
                        g_displayFocus = 0;
                        g_selectedBuildingKind = -1;
                        g_paintTerrain = -1;
                        g_runModeFlags &= ~(GAME_DEMOLISH_TOOL | GAME_ELEVATION_TOOL);
                        break;
                    case VK_TAB:
                        g_curBuildingTurns++;
                        if ((g_runModeFlags & GAME_ELEVATION_TOOL) != 0) {
                            g_gameElevPanelChoice = (g_gameElevPanelChoice + 1) % ELEV_TOOL_COUNT;
                        }
                        break;
                    case KEY_CLEAR_COURSE:
                        if ((g_runModeFlags & GAME_SANDBOX) != 0) {
                            short *pTileExtra = g_tileExtra;
                            int heightRow = (int)g_pointElevationMap;
                            memset(g_gameTileTerrainKind, TERRAIN_ROUGH, sizeof(g_gameTileTerrainKind));
                            for (; heightRow < (int)g_pointElevationMap[GRID_SIZE]; heightRow += VERTEX_ROW_STRIDE) {
                                memset((void *)heightRow, SANDBOX_CLEAR_HEIGHT, GRID_SIZE);
                                for (col = 0; col < GRID_SIZE; col++) {
                                    *pTileExtra++ &= TILE_KEEP_ON_WIPE;
                                }
                            }
                            InitNewGameData();
                            RebuildTerrainElevation();
                            g_gameCourseRefreshReq = 1;
                            g_runModeFlags |= GAME_NO_CLUBHOUSE;
                        }
                        break;
                    case KEY_SAVE_CHAMPIONSHIP_COURSE:
                        if ((g_runModeFlags & GAME_TOURNAMENT) == 0) {
                            g_printBuffer[0] = '\0';
                            AppendCourseName(-1);
                            StoreStringInPool(0);
                            strcpy(g_printBuffer, "Themes\\Championship\\");
                            AppendCourseName(0);
                            strcat(g_printBuffer, ".cse");
                            if (FileExists(g_printBuffer) != 0) {
                                strcat(g_printBuffer, "\nalready exists!\n Overwrite the old version.\n Cancel\n");
                                if (ShowActionDialog(SCREEN_CENTER_X, CONFIRM_DIALOG_TOP, 1, 1, 0) != 0)
                                    break;
                                strcpy(g_printBuffer, "Themes\\Championship\\");
                                AppendCourseName(0);
                                strcat(g_printBuffer, ".cse");
                            }
                            SaveCourseToFile();
                            g_printBuffer[0] = '\0';
                            AppendCourseName(1);
                            strcat(g_printBuffer, " saved for championship play.");
                            ShowStoryMessage(COLOR_DARK_RED, 0, -1);
                        } else {
                            strcpy(g_printBuffer, "Cannot save course during a tournament.");
                            ShowStoryMessage(COLOR_DARK_RED, 0, -1);
                            ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                        }
                        break;
                    case KEY_SAVE_GAME: ShowSaveGameDialog(); break;
                    case KEY_LOAD_GAME: ShowLoadGameScreen(0, 0); break;
                    case KEY_TOGGLE_NAMES: g_configBitMask ^= PREF_SHOW_NAMES; break;
                    case KEY_RELOAD_BALL_PIC: {
                        AssetSurface bldgSheet;
                        bldgSheet.Create(SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0, 0);
                        bldgSheet.load_pcx("bldg.pcx", 0, 0, PALETTE_COLORS, 2);
                        g_tinyBallPicture.close();
                        g_tinyBallPicture.extract(&bldgSheet, GOLF_BALL_PIC_X, 0, GOLF_BALL_PIC_SIZE,
                            GOLF_BALL_PIC_SIZE, 1, 1);
                    } break;
                    case KEY_TOURNAMENT_OFFER:
                        if ((g_runModeFlags & GAME_ANY_TOURNAMENT) == 0) {
                            g_winnersPurseK = 0;
                            RenderHoleInfoOverlay(SGA_EVALUATION_QUIET);
                            if (g_winnersPurseK != 0) {
                                strcpy(g_printBuffer, "The SGA offers to hold the\n");
                                strcat(g_printBuffer, itoa(g_careerPlayYear + FIRST_GAME_YEAR, g_numberBuf, 10));
                                strcat(g_printBuffer, " ");
                                strcat(g_printBuffer, g_mapCourseList[g_myWorldCourseIdx].name);
                                strcat(g_printBuffer, " Open tournament\nat your course with a\nfirst prize of \xa7");
                                strcat(g_printBuffer, itoa(g_winnersPurseK, g_numberBuf, 10));
                                strcat(g_printBuffer,
                                    ",000.\n 'Great, let the games begin.'\n 'I think I need more practice.'\n");
                                if (ShowActionDialog(SCREEN_CENTER_X, OFFER_DIALOG_TOP, 1, 1, 0) == 0) {
                                    for (int skillSlot = 0; skillSlot < PLAYER_SKILL_SLOTS; skillSlot++) {
                                        if (g_golferSkillGrades[skillSlot] != 0) {
                                            g_skillMasks[0] |= 1 << skillSlot;
                                        }
                                    }
                                    if (g_skillMasks[0] == 0) {
                                        ShowSkillPointsIntro();
                                    }
                                    InitTournamentGolfers();
                                    TriggerAccomplishment(ACCOMP_FIRST_TOURNAMENT,
                                        g_allHolesArray[1].teeX * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                        g_allHolesArray[1].teeY * TILE_WORLD_SIZE + TILE_WORLD_HALF);
                                    if (g_winnersPurseK >= PRIZE_500K) {
                                        TriggerAccomplishment(ACCOMP_FIRST_500K_TOURNAMENT,
                                            g_allHolesArray[1].teeX * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                            g_allHolesArray[1].teeY * TILE_WORLD_SIZE + TILE_WORLD_HALF);
                                    }
                                    if (g_winnersPurseK >= PRIZE_1M) {
                                        TriggerAccomplishment(ACCOMP_FIRST_1M_TOURNAMENT,
                                            g_allHolesArray[1].teeX * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                            g_allHolesArray[1].teeY * TILE_WORLD_SIZE + TILE_WORLD_HALF);
                                    }
                                } else {
                                    g_winnersPurseK = 0;
                                }
                            } else {
                                ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                                g_reportScreenId = REPORT_SGA_EVALUATION;
                            }
                        }
                        break;
                    case KEY_ELEVATION_BAR:
                        if (g_toolbarView == TOOLBAR_BUILDINGS && g_elevToolbarShown == 1) {
                            g_toolbarView = TOOLBAR_BUILDINGS;
                            g_elevToolbarShown = 0;
                        } else {
                            g_toolbarView = TOOLBAR_BUILDINGS;
                            g_elevToolbarShown = 1;
                        }
                        break;
                    case KEY_CLOSE_PANEL:
                        g_displayFocus = 0;
                        g_paintTerrain = -1;
                        g_selectedBuildingKind = -1;
                        g_selectedItemIndex = -1;
                        g_runModeFlags &= ~(GAME_DEMOLISH_TOOL | GAME_ELEVATION_TOOL);
                        g_toolbarView = TOOLBAR_CLOSED;
                        break;
                    case KEY_ROUTING_MAP: RenderRoutingMapScreen(); break;
                    case KEY_PAUSE: g_runModeFlags ^= GAME_PAUSED; break;
                    case KEY_TOGGLE_TREES: g_runModeFlags ^= GAME_HIDE_TREES; break;
                    case KEY_MONEY_CHEAT:
                        g_gameCashHundreds += CHEAT_MONEY;
                        g_runModeFlags |= GAME_CHEATED;
                        break;
                    case KEY_ANALYZE_NEAREST_HOLE:
                        g_curAnalysisHoleIndex = FindNearestHole(g_pointerPosX, g_pointerPosY);
                    case KEY_ANALYZE_SHOT: {
                        AssetSurface savedScreen;
                        savedScreen.Create(SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0, 0);
                        g_pMenu->BlitSurfaceRegion(&savedScreen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, SCREEN_WIDTH,
                            SCREEN_HEIGHT);
                        g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
                        int analysisHole = g_curAnalysisHoleIndex;
                        int groupYards[ANALYSIS_GROUP_COUNT];
                        memset(groupYards, 0, sizeof(groupYards));
                        unsigned groupColors[ANALYSIS_GROUP_COUNT];
                        int analysisShot = 0;
                        do {
                            unsigned groupColor;
                            if (analysisShot == 0) {
                                DrawCircleOnTerrain(g_pointerPosX * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                    g_pointerPosY * TILE_WORLD_SIZE + TILE_WORLD_HALF, CURSOR_MARK_RADIUS, COLOR_BLACK);
                                DrawPanelFrame(ANALYSIS_PANEL_X, ANALYSIS_PANEL_Y, ANALYSIS_PANEL_W, ANALYSIS_PANEL_H,
                                    1);
                                g_simScreen.line(ANALYSIS_PANEL_X, ANALYSIS_RULE_Y, ANALYSIS_RULE_RIGHT_X,
                                    ANALYSIS_RULE_Y, COLOR_TEAL);
                            }
                            int skillGroup = analysisShot & ANALYSIS_GROUP_MASK;
                            memset(&g_golferPool[STAND_IN_GOLFER], 0, sizeof(Golfer));
                            switch (skillGroup) {
                            case 0:
                                g_golferPool[STAND_IN_GOLFER].talents = SKILLS_NO_LENGTH;
                                groupColor = GetColorForSkillSet(SKILLS_NO_LENGTH);
                                break;
                            case 1:
                                g_golferPool[STAND_IN_GOLFER].talents = SKILLS_NO_ACCURACY;
                                groupColor = GetColorForSkillSet(SKILLS_NO_ACCURACY);
                                break;
                            case 2:
                                g_golferPool[STAND_IN_GOLFER].talents = SKILLS_NO_IMAGINATION;
                                groupColor = GetColorForSkillSet(SKILLS_NO_IMAGINATION);
                                break;
                            case 3:
                                g_golferPool[STAND_IN_GOLFER].talents = SKILLS_ALL;
                                groupColor = GetColorForSkillSet(SKILLS_ALL);
                            }
                            groupColors[skillGroup] = groupColor;
                            switch (skillGroup) {
                            case 0: strcpy(g_printBuffer, "no Length skill"); break;
                            case 1: strcpy(g_printBuffer, "no Accuracy skill"); break;
                            case 2: strcpy(g_printBuffer, "no Imagination skill"); break;
                            case 3: strcpy(g_printBuffer, "golfers with ALL skills"); break;
                            }
                            DrawTextWithColor(g_printBuffer, ANALYSIS_LABEL_X,
                                skillGroup * -ANALYSIS_ROW_HEIGHT + ANALYSIS_LABEL_BOTTOM_Y, groupColor);
                            g_golferPool[STAND_IN_GOLFER].targetX = -1;
                            g_golferPool[STAND_IN_GOLFER].shotFromX = g_golferPool[STAND_IN_GOLFER].x =
                                g_golferPool[STAND_IN_GOLFER].ballX = g_pointerPosX * TILE_WORLD_SIZE + TILE_WORLD_HALF;
                            g_golferPool[STAND_IN_GOLFER].shotFromY = g_golferPool[STAND_IN_GOLFER].y =
                                g_golferPool[STAND_IN_GOLFER].ballY = g_pointerPosY * TILE_WORLD_SIZE + TILE_WORLD_HALF;
                            int powerTier = g_level != 0 ? (analysisShot / ANALYSIS_GROUP_COUNT) & POWER_TIER_MASK : 1;
                            g_golferPool[STAND_IN_GOLFER].talentLevel = powerTier;
                            g_golferPool[STAND_IN_GOLFER].holeIdx = analysisHole;
                            g_golferPool[STAND_IN_GOLFER].flags = 0;
                            g_golferPool[STAND_IN_GOLFER].holeStrokes =
                                g_gameTileTerrainKind[g_pointerPosX * GRID_SIZE + g_pointerPosY] != 0;
                            if ((g_simOpenFacilityLvl[SCENERY_PUTTING_GREEN] != 0)
                                && ((g_golferPool[STAND_IN_GOLFER].talents & TALENT_IMAGINATION) != 0)) {
                                g_golferPool[STAND_IN_GOLFER].flags = GOLFER_PRACTISED_GREEN;
                            }
                            if ((g_simOpenFacilityLvl[SCENERY_DRIVING_RANGE] != 0)
                                && ((g_golferPool[STAND_IN_GOLFER].talents & TALENT_LENGTH) != 0)) {
                                g_golferPool[STAND_IN_GOLFER].flags |= GOLFER_PRACTISED_RANGE;
                            }
                            if ((g_simOpenFacilityLvl[SCENERY_PRO_SHOP] != 0)
                                && ((g_golferPool[STAND_IN_GOLFER].talents & TALENT_ACCURACY) != 0)) {
                                g_golferPool[STAND_IN_GOLFER].flags |= GOLFER_VISITED_PRO_SHOP;
                            }
                            int savedShots = g_allHolesArray[analysisHole].shots;
                            PlanGolferShot(STAND_IN_GOLFER, 0, -1, 0, 0);
                            g_allHolesArray[analysisHole].shots = savedShots;
                            g_ballPathSlots[analysisShot].golferId = -1;
                            StartShotTrail(STAND_IN_GOLFER);
                            int shotSpin = 0;
                            g_shotPlanMode = 0;
                            if (g_golferPool[STAND_IN_GOLFER].shotType == 1) {
                                shotSpin = 1;
                            } else if (g_golferPool[STAND_IN_GOLFER].shotType == -1) {
                                shotSpin = -1;
                            }
                            int aimWorldY;
                            int aimWorldX;
                            if ((g_golferPool[STAND_IN_GOLFER].flags & GOLFER_EXACT_AIM) != 0) {
                                aimWorldX = g_golferPool[STAND_IN_GOLFER].targetX << TILE_WORLD_SHIFT;
                                aimWorldY = g_golferPool[STAND_IN_GOLFER].targetY << TILE_WORLD_SHIFT;
                            } else {
                                aimWorldX = g_golferPool[STAND_IN_GOLFER].targetX * TILE_WORLD_SIZE + TILE_WORLD_HALF;
                                aimWorldY = g_golferPool[STAND_IN_GOLFER].targetY * TILE_WORLD_SIZE + TILE_WORLD_HALF;
                            }
                            SimulateBallPhysics(STAND_IN_GOLFER, aimWorldX, aimWorldY, shotSpin);
                            int lastPoint = g_ballPathSlots[analysisShot].count - 1;
                            int landingX = g_ballPathSlots[analysisShot].x[lastPoint];
                            int landingY = g_ballPathSlots[analysisShot].y[lastPoint];
                            DrawCircleOnTerrain(landingX, landingY, skillGroup + LANDING_MARK_RADIUS, groupColor);
                            int yardsToPin = CalculateDistanceInYards(landingX, landingY,
                                g_allHolesArray[analysisHole].aimX, g_allHolesArray[analysisHole].aimY);
                            groupYards[skillGroup] += yardsToPin;
                            int landingTerrain = GetTileAtPosition(landingX, landingY);
                            int liePenalty = g_squareTypeTable[landingTerrain].lieGrade;
                            groupYards[skillGroup] += liePenalty * LIE_PENALTY_YARDS;
                            int landingOnScreen = WorldToScreen(landingX, landingY, &landingX, &landingY, 0);
                            if (landingOnScreen != 0) {
                                if (liePenalty <= 0) {
                                    g_groundShadowPixel.BlitScaled(g_pMenu, landingX, landingY, 0);
                                    g_tinyBallPicture.BlitScaled(g_pMenu, landingX, landingY - 1, 0);
                                } else {
                                    g_simScreen.line(landingX, landingY, landingX + 1, landingY, COLOR_WHITE);
                                }
                            }
                            int trailPoint = 0;
                            g_shotPlanMode = 0;
                            if (0 < g_ballPathSlots[analysisShot].count) {
                                do {
                                    int pointScreenY;
                                    int pointScreenX;
                                    int pointOnScreen = WorldToScreen(g_ballPathSlots[analysisShot].x[trailPoint],
                                        g_ballPathSlots[analysisShot].y[trailPoint], &pointScreenX, &pointScreenY, 0);
                                    if (pointOnScreen != 0) {
                                        pointScreenY -= skillGroup;
                                        if (trailPoint == 0) {
                                            TerrainMoveTo(pointScreenX, pointScreenY + 1);
                                        } else {
                                            TerrainLineTo(pointScreenX, pointScreenY + 1, groupColor, 1, LINE_ALPHA);
                                        }
                                    }
                                    trailPoint++;
                                } while (trailPoint < g_ballPathSlots[analysisShot].count);
                            }
                            g_ballPathSlots[analysisShot].golferId = SCRATCH_GOLFER;
                            if (analysisShot == LAST_ANALYSIS_SHOT) {
                                strcpy(g_printBuffer, "Shot Analysis:");
                                DrawTextCenteredWithShadow(g_printBuffer, ANALYSIS_TITLE_X, ANALYSIS_TITLE_Y,
                                    COLOR_CYAN);
                                strcpy(g_printBuffer, "longer");
                                strcat(g_printBuffer, " hitters");
                                strcpy(g_printBuffer, "Golfers");
                                strcat(g_printBuffer, " playing hole ");
                                strcat(g_printBuffer, itoa(analysisHole, g_numberBuf, 10));
                                strcat(g_printBuffer, "...");
                                g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                                DrawTextCenteredWithShadow(g_printBuffer, ANALYSIS_TITLE_X, ANALYSIS_SUBTITLE_Y,
                                    COLOR_CYAN);
                                int allSkillsYards = groupYards[ALL_SKILLS_GROUP];
                                for (int lackingGroup = 0; lackingGroup < ALL_SKILLS_GROUP; lackingGroup++) {
                                    int lackingYards = groupYards[lackingGroup];
                                    strcpy(g_printBuffer, lackingYards < allSkillsYards ? "+" : g_empty);
                                    strcat(g_printBuffer,
                                        itoa((allSkillsYards - lackingYards) / SHOTS_PER_GROUP, g_numberBuf, 10));
                                    strcat(g_printBuffer, " yds.");
                                    DrawTextCenteredWithShadow(g_printBuffer, ANALYSIS_YARDS_X,
                                        (lackingGroup & ANALYSIS_GROUP_MASK) * -ANALYSIS_ROW_HEIGHT
                                            + ANALYSIS_YARDS_BOTTOM_Y,
                                        groupColors[lackingGroup]);
                                }
                                g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
                            }
                            g_simScreen.RenderFullSurface(0);
                            if (analysisShot == LAST_ANALYSIS_SHOT) {
                                WaitForInputWithTimeout(0);
                                savedScreen.BlitSurfaceRegion(g_pMenu, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0,
                                    SCREEN_WIDTH, SCREEN_HEIGHT);
                                flush_input();
                                g_mouseButton = 0;
                            }
                            analysisShot++;
                        } while (analysisShot < ANALYSIS_SHOT_COUNT);
                        for (int trail = 0; trail < BALL_TRAIL_COUNT; trail++) {
                            g_ballPathSlots[trail].golferId = -1;
                        }
                    } break;
                    case KEY_GOLFER_INFO:
                        if (g_displayFocus > 0) {
                            RenderGolferCustomizationScreen(g_golferPool[g_displayFocus - 1].typeIdx,
                                g_displayFocus - 1);
                            g_golferPool[g_displayFocus - 1].ageFlags =
                                g_golferTypeArray[g_golferPool[g_displayFocus - 1].typeIdx].ageBits;
                        } else {
                            g_reportScreenId = 1;
                            g_messageCountdown = REPEAT_MESSAGE_TICKS;
                        }
                        break;
                    case KEY_LOWER: {
                        ConfigureAndPlaySound(SOUND_BASS_DOWN, SOUND_FULL_VOLUME, 0, 0, 0);
                        int maxVertexHeight = (g_mapCourseList[g_myWorldCourseIdx].hilly != RELIEF_HILLY)
                                                  ? MAX_VERTEX_HEIGHT_FLAT
                                                  : MAX_VERTEX_HEIGHT;
                        if (g_gameElevPanelChoice != GRID_MODE_SQUARE) {
                            g_pointElevationMap[g_pointerPosX][g_pointerPosY] = Clamp(
                                --g_pointElevationMap[g_pointerPosX][g_pointerPosY], ELEV_SEA_LEVEL, maxVertexHeight);
                        }
                        if (g_gameElevPanelChoice == GRID_MODE_SQUARE) {
                            int highestCorner = 0;
                            for (row = g_pointerPosX; row <= g_pointerPosX + 1; row++) {
                                unsigned char *scanHeights = g_pointElevationMap[row];
                                for (col = g_pointerPosY - 1; col <= g_pointerPosY; col++) {
                                    if (scanHeights[col] > highestCorner) {
                                        highestCorner = scanHeights[col];
                                    }
                                }
                            }
                            for (row = g_pointerPosX; row <= g_pointerPosX + 1; row++) {
                                for (col = g_pointerPosY - 1; col <= g_pointerPosY; col++) {
                                    if ((g_pointElevationMap[row][col] == highestCorner)
                                        && (ELEV_SEA_LEVEL < highestCorner)) {
                                        g_pointElevationMap[row][col]--;
                                    }
                                }
                            }
                        }
                        if (g_gameElevPanelChoice == ELEV_TOOL_AREA) {
                            SmoothTerrainLower(g_pointerPosX, g_pointerPosY);
                        }
                        for (int edgeDir = 0; edgeDir < DIRECTION_COUNT; edgeDir += QUARTER_TURN) {
                            GetTileElevation(g_dirRowStep[edgeDir] + g_pointerPosX,
                                g_dirColStep[edgeDir] + g_pointerPosY, 0);
                        }
                        g_editedTileRowIdx = g_pointerPosX;
                        g_editedTileColIdx = g_pointerPosY;
                        RebuildTerrainElevation();
                    } break;
                    case KEY_RAISE: {
                        ConfigureAndPlaySound(SOUND_BASS_UP, SOUND_FULL_VOLUME, 0, 0, 0);
                        if (g_gameElevPanelChoice != GRID_MODE_SQUARE) {
                            g_pointElevationMap[g_pointerPosX][g_pointerPosY] = Clamp(
                                ++g_pointElevationMap[g_pointerPosX][g_pointerPosY], ELEV_SEA_LEVEL, MAX_VERTEX_HEIGHT);
                        }
                        if (g_gameElevPanelChoice == GRID_MODE_SQUARE) {
                            int lowestCorner = HEIGHT_ABOVE_ANY_VERTEX;
                            for (row = g_pointerPosX; row <= g_pointerPosX + 1; row++) {
                                unsigned char *scanHeights = g_pointElevationMap[row];
                                for (col = g_pointerPosY - 1; col <= g_pointerPosY; col++) {
                                    if (scanHeights[col] < lowestCorner) {
                                        lowestCorner = scanHeights[col];
                                    }
                                }
                            }
                            for (row = g_pointerPosX; row <= g_pointerPosX + 1; row++) {
                                for (col = g_pointerPosY - 1; col <= g_pointerPosY; col++) {
                                    if (g_pointElevationMap[row][col] == lowestCorner
                                        && lowestCorner < MAX_VERTEX_HEIGHT) {
                                        g_pointElevationMap[row][col]++;
                                    }
                                }
                            }
                        }
                        if (g_gameElevPanelChoice == ELEV_TOOL_AREA) {
                            SmoothTerrainRaise(g_pointerPosX, g_pointerPosY);
                        }
                        for (int edgeDir = 0; edgeDir < DIRECTION_COUNT; edgeDir += QUARTER_TURN) {
                            GetTileElevation(g_dirRowStep[edgeDir] + g_pointerPosX,
                                g_dirColStep[edgeDir] + g_pointerPosY, 0);
                        }
                        g_editedTileRowIdx = g_pointerPosX;
                        g_editedTileColIdx = g_pointerPosY;
                        RebuildTerrainElevation();
                    } break;
                    case '1': jumpHole = 1; goto jumpToHole;
                    case '2': jumpHole = 2; goto jumpToHole;
                    case '3': jumpHole = 3; goto jumpToHole;
                    case '4': jumpHole = 4; goto jumpToHole;
                    case '5': jumpHole = 5; goto jumpToHole;
                    case '6': jumpHole = 6; goto jumpToHole;
                    case '7': jumpHole = 7; goto jumpToHole;
                    case '8': jumpHole = 8; goto jumpToHole;
                    case '9': jumpHole = 9; goto jumpToHole;
                    case '!': jumpHole = 10; goto jumpToHole;
                    case '@': jumpHole = 11; goto jumpToHole;
                    case '#': jumpHole = 12; goto jumpToHole;
                    case '$': jumpHole = 13; goto jumpToHole;
                    case '%': jumpHole = 14; goto jumpToHole;
                    case '^':
                        jumpHole = 15;
jumpToHole:
                        if (g_allHolesArray[jumpHole].par != 0) {
                            g_displayFocus = 0;
                            int greenScreenY;
                            int greenScreenX;
                            if (TileToScreenCached(g_allHolesArray[jumpHole].aimX, g_allHolesArray[jumpHole].aimY,
                                    &greenScreenX, &greenScreenY)
                                != 0) {
                                g_displayFocus = -jumpHole;
                            }
                            g_cameraTileX = (g_allHolesArray[jumpHole].teeX + g_allHolesArray[jumpHole].aimX) / 2;
                            g_cameraTileY = (g_allHolesArray[jumpHole].teeY + g_allHolesArray[jumpHole].aimY) / 2;
                        }
                        break;
                    case '&': jumpHole = 16; goto jumpToHole;
                    case '*': jumpHole = 17; goto jumpToHole;
                    case '(': jumpHole = 18; goto jumpToHole;
                    case KEY_RESTART_COURSE:
                        if ((g_runModeFlags & GAME_CHAMPIONSHIP) == 0) {
                            StartNewGameOnCourse(0);
                        } else {
                            ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                        }
                        break;
                    }
                    int scrollDir;
                    switch (key) {
                    case VK_F5: RenderRoutingMapScreen(); break;
                    case VK_F6: StartNewGameOnCourse(0); break;
                    case VK_F1:
                        g_reportScreenId = g_reportScreenId != REPORT_COURSE_TABLE ? REPORT_COURSE_TABLE : 0;
                        break;
                    case VK_F2:
                        g_reportScreenId = g_reportScreenId != REPORT_PLAYER_COMMENTS ? REPORT_PLAYER_COMMENTS : 0;
                        break;
                    case VK_F3: g_reportScreenId = g_reportScreenId != REPORT_HISTOGRAPH ? REPORT_HISTOGRAPH : 0; break;
                    case VK_F4: g_reportScreenId = g_reportScreenId != REPORT_FINANCES ? REPORT_FINANCES : 0; break;
                    case VK_F7:
                        g_reportScreenId = g_reportScreenId != REPORT_SGA_EVALUATION ? REPORT_SGA_EVALUATION : 0;
                        break;
                    case VK_F8:
                        g_reportScreenId = g_reportScreenId != REPORT_KEYBOARD_HELP ? REPORT_KEYBOARD_HELP : 0;
                        break;
                    case VK_F9: g_reportScreenId = g_reportScreenId != REPORT_MEMBERSHIP ? REPORT_MEMBERSHIP : 0; break;
                    case VK_F10: ShowAccomplishmentsScreen(); break;
                    case VK_UP: scrollDir = 0; goto scrollCamera;
                    case VK_RIGHT: scrollDir = QUARTER_TURN; goto scrollCamera;
                    case VK_DOWN: scrollDir = HALF_TURN; goto scrollCamera;
                    case VK_LEFT:
                        scrollDir = THREE_QUARTER_TURN;
scrollCamera:
                        switch (scrollDir + g_camDirIdx & DIRECTION_MASK) {
                        case 0:
                            g_cameraTileX += KEY_SCROLL_TILES;
                            g_cameraTileY -= KEY_SCROLL_TILES;
                            g_cameraTileX = Clamp(g_cameraTileX, 0, LAST_TILE);
                            g_cameraTileY = Clamp(g_cameraTileY, 0, LAST_TILE);
                            break;
                        case QUARTER_TURN:
                            g_cameraTileX += KEY_SCROLL_TILES;
                            g_cameraTileY += KEY_SCROLL_TILES;
                            g_cameraTileX = Clamp(g_cameraTileX, 0, LAST_TILE);
                            g_cameraTileY = Clamp(g_cameraTileY, 0, LAST_TILE);
                            break;
                        case HALF_TURN:
                            g_cameraTileX -= KEY_SCROLL_TILES;
                            g_cameraTileY += KEY_SCROLL_TILES;
                            g_cameraTileX = Clamp(g_cameraTileX, 0, LAST_TILE);
                            g_cameraTileY = Clamp(g_cameraTileY, 0, LAST_TILE);
                            break;
                        case THREE_QUARTER_TURN:
                            g_cameraTileX -= KEY_SCROLL_TILES;
                            g_cameraTileY -= KEY_SCROLL_TILES;
                            g_cameraTileX = Clamp(g_cameraTileX, 0, LAST_TILE);
                            g_cameraTileY = Clamp(g_cameraTileY, 0, LAST_TILE);
                        }
                        break;
                    case VK_PRIOR:
                        g_camDirIdx = g_camDirIdx + QUARTER_TURN & DIRECTION_MASK;
                        g_paintedCameraTileX = -1;
                        break;
                    case VK_HOME: g_camDirIdx = g_camDirIdx - QUARTER_TURN & DIRECTION_MASK; g_paintedCameraTileX = -1;
                    }
                    ShowWelcomeMessage();
                    RenderFloatingMessages();
                    RenderMoneyPopups();
                    if ((g_reportScreenId == 1) || (g_bubbleStep != 0)) {
                        RenderDialogueBubble();
                    }
                    if (g_reportScreenId == REPORT_COURSE_TABLE) {
                        RenderHoleInfoOverlay(0);
                    }
                    if (g_reportScreenId == REPORT_PLAYER_COMMENTS) {
                        RenderPlayerCommentsReport();
                    }
                    if (g_reportScreenId == REPORT_GOLFER_PAIRING) {
                        RenderGolferPairingScreen();
                    }
                    if (g_reportScreenId == REPORT_HISTOGRAPH) {
                        RenderHistographScreen();
                    }
                    if (g_reportScreenId == REPORT_KEYBOARD_HELP) {
                        RenderKeyboardShortcutsScreen();
                    }
                    if (g_reportScreenId == REPORT_FINANCES) {
                        RenderFinancialReportScreen();
                    }
                    if (g_reportScreenId == REPORT_TOP10_COURSES) {
                        RenderTop10CoursesScreen();
                    }
                    if (g_reportScreenId == REPORT_SGA_EVALUATION) {
                        RenderHoleInfoOverlay(1);
                    }
                    if (g_reportScreenId == REPORT_MEMBERSHIP) {
                        RenderMembershipRosterScreen();
                    }
                    if (g_curTournPrepBits == TOURNAMENT_COURSE_PENDING) {
                        PrepareTournamentCourse();
                    }
                    if (g_curTournPrepBits == -1) {
                        g_curTournPrepBits = TOURNAMENT_COURSE_PENDING;
                        g_displayFocus = 0;
                    }
                    if (((g_runModeFlags & GAME_TOURNAMENT) != 0) && (g_displayFocus == 0)) {
                        RenderTournamentLeaderboard();
                    }
                    int mouseY;
                    int mouseX;
                    g_simScreen.get_mouse_pos(&mouseX, &mouseY);
                    int edgeScrollX = 0;
                    int edgeScrollY = 0;
                    if (mouseX <= 0) {
                        edgeScrollX = -1;
                        edgeScrollY = -1;
                    }
                    if (mouseY <= 0) {
                        edgeScrollX++;
                        edgeScrollY--;
                    }
                    if (mouseX >= SCREEN_LAST_X) {
                        edgeScrollX++;
                        edgeScrollY++;
                    }
                    if (mouseY >= SCREEN_LAST_Y) {
                        edgeScrollX--;
                        edgeScrollY++;
                    }
                    switch (g_camDirIdx) {
                    case 0:
                        g_cameraTileX += edgeScrollX;
                        g_cameraTileY += edgeScrollY;
                        break;
                    case QUARTER_TURN:
                        g_cameraTileX -= edgeScrollY;
                        g_cameraTileY += edgeScrollX;
                        break;
                    case HALF_TURN:
                        g_cameraTileX -= edgeScrollX;
                        g_cameraTileY -= edgeScrollY;
                        break;
                    case THREE_QUARTER_TURN: g_cameraTileX += edgeScrollY; g_cameraTileY -= edgeScrollX;
                    }
                    g_cameraTileX = Clamp(g_cameraTileX, 0, LAST_TILE);
                    g_cameraTileY = Clamp(g_cameraTileY, 0, LAST_TILE);
                    if (g_selectedGolferIndex != -1) {
                        ConfigureAndPlaySound(SOUND_CAMERA, SOUND_FULL_VOLUME, 0, 0, 0);
                        g_simScreen.RenderFullSurface(0);
                        g_selectedGolferIndex = -1;
                        WaitForInputWithTimeout(0);
                    } else {
                        OnFrameIdle();
                        g_simScreen.RenderFullSurface(0);
                        if ((g_allowedExpansionRequestFlag & 1) != 0) {
                            ConfigureAndPlaySound(SOUND_TADA, SOUND_FULL_VOLUME, 0, 0, 0);
                            WaitWithTimeout(EXPAND_FANFARE_TICKS, 0);
                            flush_input();
                            strcpy(g_printBuffer, "Do you wish to purchase additional\nland to expand your course?\n");
                            strcat(g_printBuffer, " Yup, I've got big plans.\n No, I'm fine.\n");
                            if (ShowActionDialog(EXPAND_DIALOG_X, EXPAND_DIALOG_TOP, 1, 1, 0) == 0) {
                                if (RenderTractPurchaseScreen() != 0) {
                                    g_landPurchaseNum++;
                                }
                                SyncTerrainTypes();
                                RecalculateAllWaterShoreDepths();
                                UpdateBuildingAccessibility();
                                g_editedTileRowIdx = GRID_CENTER;
                                g_editedTileColIdx = GRID_CENTER;
                                RebuildTerrainElevation();
                            }
                        }
                        g_allowedExpansionRequestFlag = 0;
                    }
                    if ((g_mouseButton != 0) && (g_selectedItemIndex != -1)) {
                        if (g_mouseButton == 1) {
                            if (IsPositionInvalid(g_pointerPosX, g_pointerPosY) != 0) {
                                ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                            } else {
                                if (g_selectedItemIndex < CARRIED_GOLFER) {
                                    g_staffSlots[g_selectedItemIndex].postRow = g_pointerPosX;
                                    g_staffSlots[g_selectedItemIndex].postCol = g_pointerPosY;
                                    g_staffSlots[g_selectedItemIndex].waitTimer = 0;
                                } else {
                                    golferIndex = g_selectedItemIndex - CARRIED_GOLFER;
                                    g_golferPool[golferIndex].x = g_pointerPosX * TILE_WORLD_SIZE + TILE_WORLD_HALF;
                                    g_golferPool[golferIndex].y = g_pointerPosY * TILE_WORLD_SIZE + TILE_WORLD_HALF;
                                    g_golferPool[golferIndex].waitTimer = 0;
                                    if (FindBuildingAtTile(g_pointerPosX, g_pointerPosY) == 0) {
                                        if (((g_runModeFlags & GAME_ANY_TOURNAMENT) == 0)
                                            && (g_golferPool[golferIndex].persona == 0)) {
                                            int golferType = g_golferPool[golferIndex].typeIdx;
                                            g_golferHistoryPerCharacter[golferType]
                                                .perHoleFlags[g_golferPool[golferIndex].holeIdx] =
                                                g_golferHistoryPerCharacter[golferType]
                                                    .perHoleFlags[g_golferPool[golferIndex].holeIdx]
                                                | HOLE_QUIT;
                                            g_golferHistoryPerCharacter[golferType].finishedStoryId = STORY_NONE;
                                        }
                                        g_golferPool[golferIndex].holeIdx = CLUBHOUSE_HOLE;
                                        g_golferPool[golferIndex].ballX = 0;
                                        g_golferPool[golferIndex].holeStrokes = 0;
                                        g_golferPool[golferIndex].enjoyment = 0;
                                    }
                                }
                                g_selectedItemIndex = -1;
                            }
                        } else {
                            g_selectedItemIndex = -1;
                        }
                        g_mouseButton = 0;
                    } else if (((g_mouseButton == 1) && (0 < g_displayFocus)) && (g_hot != -1)) {
                        waitingPair = g_displayFocus - 1;
                        switch (g_hot) {
                        case INFO_BUTTON_STORY:
                            g_mouseButton = 0;
                            g_bgEnabled ^= 1;
                            continue;
                        case INFO_BUTTON_SNAPSHOT:
                            g_selectedGolferIndex = waitingPair;
                            g_displayFocus = 0;
                            if (g_golferPool[waitingPair].screenX == -1) {
                                g_cameraTileX = g_golferPool[waitingPair].x >> TILE_WORLD_SHIFT;
                                g_cameraTileY = g_golferPool[waitingPair].y >> TILE_WORLD_SHIFT;
                            }
                            if (g_golferPool[waitingPair].talkTimer == 0) {
                                g_golferPool[waitingPair].talkTimer = 1;
                            }
                            if (g_golferPool[waitingPair ^ 1].talkTimer == 0) {
                                g_golferPool[waitingPair ^ 1].talkTimer = 1;
                                g_mouseButton = 0;
                                continue;
                            }
                            break;
                        case -1:
                            g_golferPool[waitingPair].waitTimer = -99;
                            g_golferPool[waitingPair].activity = GOLFER_HAPPY_REACTION;
                            if (g_rng.NextInt(2) != 0) {
                                g_golferPool[waitingPair].flags |= GOLFER_ALT_REACTION;
                            } else {
                                g_golferPool[waitingPair].flags &= ~GOLFER_ALT_REACTION;
                            }
                            SetGolferAction(waitingPair, ACTION_LIKES_DIFFERENT_HOLE, 0x14);
                            {
                                int golfersUpset;
                                for (golfersUpset = 0; golfersUpset < 2; golfersUpset++) {
                                    unsigned otherGolfer;
                                    do {
                                        otherGolfer = g_rng.NextInt(GOLFER_COUNT);
                                    } while ((otherGolfer == waitingPair) || (g_golferPool[otherGolfer].holeIdx <= 0));
                                    g_golferPool[otherGolfer].enjoyment -= 1;
                                }
                            }
                            g_mouseButton = 0;
                            continue;
                        case INFO_BUTTON_NEXT_CHAPTER:
                            if (((g_golferPool[waitingPair].flags & GOLFER_STORY_LEAD) != 0)
                                || ((g_golferPool[g_golferPool[waitingPair].partner].flags & GOLFER_STORY_LEAD) != 0)) {
                                int resultRange;
                                int luckBonus;
                                if ((g_golferPool[waitingPair].flags & GOLFER_STORY_LEAD) != 0) {
                                    resultRange = g_golferPool[waitingPair].storyChapter << HOLE_RESULT_SHIFT;
                                    g_golferPool[g_golferPool[waitingPair].partner]
                                        .holeResult[g_golferPool[waitingPair].holeIdx] = g_rng.NextInt(resultRange);
                                    if ((g_golferPool[g_golferPool[waitingPair].partner].slotParams[0] & MOOD_ANNOYED)
                                        == 0) {
                                        g_golferPool[g_golferPool[waitingPair].partner]
                                            .holeResult[g_golferPool[waitingPair].holeIdx] =
                                            g_rng.NextInt(STORY_RESULT_RANGE);
                                        if ((g_golferPool[g_golferPool[waitingPair].partner].slotParams[0]
                                                & MOOD_PLEASED)
                                            != 0) {
                                            luckBonus = g_rng.NextInt(STORY_LUCK_RANGE);
                                            if (GetGolferMoodBalance(waitingPair ^ 1) + STORY_MOOD_MARGIN
                                                >= g_golferPool[waitingPair].storyChapter + luckBonus + g_level) {
                                                g_golferPool[g_golferPool[waitingPair].partner]
                                                    .holeResult[g_golferPool[waitingPair].holeIdx] =
                                                    STORY_RESULT_SPECIAL;
                                            }
                                        }
                                    }
                                } else {
                                    resultRange = g_golferPool[waitingPair].storyChapter << HOLE_RESULT_SHIFT;
                                    g_golferPool[waitingPair].holeResult[g_golferPool[waitingPair].holeIdx] =
                                        g_rng.NextInt(resultRange);
                                    if ((g_golferPool[waitingPair].slotParams[0] & MOOD_ANNOYED) == 0) {
                                        g_golferPool[waitingPair].holeResult[g_golferPool[waitingPair].holeIdx] =
                                            g_rng.NextInt(STORY_RESULT_RANGE);
                                        if ((g_golferPool[waitingPair].slotParams[0] & MOOD_PLEASED) != 0) {
                                            luckBonus = g_rng.NextInt(STORY_LUCK_RANGE);
                                            if (GetGolferMoodBalance(waitingPair) + STORY_MOOD_MARGIN
                                                >= g_golferPool[waitingPair].storyChapter + luckBonus + g_level) {
                                                g_golferPool[waitingPair]
                                                    .holeResult[g_golferPool[waitingPair].holeIdx] =
                                                    STORY_RESULT_SPECIAL;
                                            }
                                        }
                                    }
                                }
                                if (AdvanceStoryChapter(((g_golferPool[waitingPair].flags & GOLFER_STORY_LEAD) != 0)
                                                            ? waitingPair
                                                            : (waitingPair ^ 1),
                                        1)
                                    == 0) {
                                    g_golferPool[waitingPair].enjoyment -= 1;
                                    g_golferPool[g_golferPool[waitingPair].partner].enjoyment =
                                        g_golferPool[g_golferPool[waitingPair].partner].enjoyment - 1;
                                }
                                g_displayFocus = 0;
                                g_mouseButton = 0;
                                g_golferPool[waitingPair].flags |= GOLFER_STORY_ADVANCED;
                                g_golferPool[g_golferPool[waitingPair].partner].flags =
                                    g_golferPool[g_golferPool[waitingPair].partner].flags | GOLFER_STORY_ADVANCED;
                                continue;
                            }
                            break;
                        case 1:
                            g_selectedItemIndex = waitingPair + CARRIED_GOLFER;
                            g_displayFocus = 0;
                            g_mouseButton = 0;
                            continue;
                        case 0:
                            RenderGolferCustomizationScreen(g_golferPool[waitingPair].typeIdx, waitingPair);
                            g_mouseButton = 0;
                            g_golferPool[waitingPair].ageFlags =
                                g_golferTypeArray[g_golferPool[waitingPair].typeIdx].ageBits;
                            continue;
                        case INFO_BUTTON_CLOSE:
                            g_displayFocus = 0;
                            g_hot = -1;
                            break;
                        }
                        g_mouseButton = 0;
                        continue;
                    } else if (g_mouseButton != 0) {
                        g_paintedCameraTileX = -1;
                        if (g_zoomView != 0) {
                            g_cameraZoom = g_cameraZoom * 2;
                        }
                        int clickRow;
                        int clickCol;
                        ScreenToTileCoords(g_clickX, g_clickY, &clickRow, &clickCol, 0);
                        if (g_zoomView != 0) {
                            g_cameraZoom = g_cameraZoom / 2;
                        }
                        if (MAIN_BAR_TOP < g_clickY) {
                            if (g_clickX < MAIN_BAR_WIDTH) {
                                command = MainBarClick(g_clickX, g_clickY);
                                if (command == 0)
                                    continue;
                                g_mouseButton = 0;
                                if (command < 0) {
                                    key = -command;
                                    command = 0;
                                }
                                redispatch = 1;
                                continue;
                            } else if (g_clickY > TOOLBAR_TOP) {
                                if (g_toolbarView == TOOLBAR_BUILD_COURSE) {
                                    g_selectedVariant = HandleCourseToolbarClick(g_clickX, g_clickY);
                                    g_curBuildingTurns = g_rng.NextInt(3);
                                    if (g_paintTerrain == TERRAIN_GREEN) {
                                        g_curBuildingTurns = 0;
                                    } else if ((g_paintTerrain == 0)
                                               && (g_allHolesArray[g_nextOpenHoleIndex].teeX != 0)) {
                                        strcpy(g_printBuffer, "You've already built a tee for hole ");
                                        strcat(g_printBuffer, itoa(g_nextOpenHoleIndex, g_numberBuf, 10));
                                        strcat(g_printBuffer,
                                            (g_allHolesArray[g_nextOpenHoleIndex].aimX != 0)
                                                ? ". You need to open this hole (press 'h') before you can build another one."
                                                : ". Next you need to add a green to this hole.");
                                        strcat(g_printBuffer, " (If you want to move the tee for hole ");
                                        strcat(g_printBuffer, itoa(g_nextOpenHoleIndex, g_numberBuf, 10));
                                        strcat(g_printBuffer, ", delete the old tee (right-click on it) first.)");
                                        if (g_nextOpenHoleIndex >= CLUBHOUSE_HOLE) {
                                            strcpy(g_printBuffer, "You cannot build more than 18 holes.");
                                        }
                                        ShowStoryMessage(COLOR_DARK_RED, 1, -1);
                                    }
                                    continue;
                                }
                                if (g_toolbarView == TOOLBAR_BUILDINGS) {
                                    g_selectedVariant = HandleFacilityUpgradeClick(g_clickX, g_clickY);
                                    WaitWithTimeout(FACILITY_CLICK_PAUSE, 0);
                                    continue;
                                }
                                if (g_toolbarView == TOOLBAR_GOLFERS) {
                                    g_selectedVariant = HandleGolferPairingClick(g_clickX, g_clickY);
                                    continue;
                                }
                                if ((g_toolbarView == TOOLBAR_SHOT) || (g_toolbarView == TOOLBAR_PEOPLE)) {
                                    command = HandlePeopleToolbarClick(g_clickX, g_clickY);
                                    g_mouseButton = 0;
                                    if (command <= 0)
                                        continue;
                                    redispatch = 1;
                                    continue;
                                }
                            }
                        }
                        if ((g_runModeFlags & GAME_ELEVATION_TOOL) != 0) {
                            if (g_mouseButton == 1) {
                                command = KEY_RAISE;
                            } else if (g_mouseButton == MOUSE_RIGHT) {
                                command = KEY_LOWER;
                            }
                            g_mouseButton = 0;
                            if (g_gameElevPanelChoice == GRID_MODE_ANALYZE) {
                                command = KEY_ANALYZE_NEAREST_HOLE;
                            }
                            redispatch = 1;
                            continue;
                        }
                        if (((g_runModeFlags & GAME_DEMOLISH_TOOL) != 0) && (g_mouseButton == 1)) {
                            DemolishTileAtPosition(g_pointerPosX, g_pointerPosY);
                        } else {
                            if (((((g_toolbarView == TOOLBAR_PEOPLE) || (g_toolbarView == TOOLBAR_CLOSED))
                                     || ((g_paintTerrain == -1 && (g_selectedBuildingKind == -1))))
                                    || (g_toolbarView == TOOLBAR_GOLFERS))
                                && (((panelPick == -1 && (g_toolbarView != TOOLBAR_SHOT)) && (g_mouseButton == 1)))) {
                                g_reportScreenId = 0;
                                g_displayFocus = 0;
                                if (!HIBYTE(GetAsyncKeyState(VK_SHIFT))) {
                                    short tileFlags;
                                    if ((IsPositionInvalid(clickRow, clickCol) == 0)
                                        && (tileFlags = g_tileExtra[clickRow * GRID_SIZE + clickCol],
                                            (tileFlags & TILE_PIN) != 0)) {
                                        g_displayFocus = -(tileFlags & TILE_HOLE_MASK);
                                        if (g_allHolesArray[-g_displayFocus].par == 0) {
                                            OpenHoleForPlay();
                                            g_displayFocus = 0;
                                        }
                                        continue;
                                    }
                                    int tileHole;
                                    if (((IsPositionInvalid(clickRow, clickCol) == 0)
                                            && (g_gameTileTerrainKind[clickRow * GRID_SIZE + clickCol] == 0))
                                        && (tileHole = g_tileExtra[clickRow * GRID_SIZE + clickCol] & TILE_HOLE_MASK,
                                            (g_allHolesArray[tileHole].flags & HOLE_AWARDS) != 0)) {
                                        g_printBuffer[0] = 0;
                                        strcpy(g_printBuffer, "Rename Hole ");
                                        strcat(g_printBuffer, itoa(tileHole, g_numberBuf, 10));
                                        strcat(g_printBuffer, "?");
                                        char promptText[NAME_PROMPT_SIZE];
                                        strcpy(promptText, g_printBuffer);
                                        g_printBuffer[0] = 0;
                                        AppendHoleName(tileHole);
                                        ShowTextInputDialog(promptText, NAME_DIALOG_X, NAME_DIALOG_Y, NAME_MAX_LENGTH);
                                        if (g_printBuffer[0] != 0) {
                                            StoreStringInPool(tileHole);
                                        }
                                        continue;
                                    }
                                }
                                if (((g_toolbarView == TOOLBAR_CLOSED) && (g_clickX >= ROSTER_LEFT_X))
                                    && (ROSTER_BUTTON_Y < g_clickY)) {
                                    int rosterGolfer =
                                        g_rosterBarButtonGolfer[(g_clickX - ROSTER_LEFT_X) / ROSTER_BUTTON_WIDTH];
                                    if ((rosterGolfer >= -1) && (rosterGolfer < GOLFER_COUNT)) {
                                        g_displayFocus = rosterGolfer + 1;
                                        g_cameraTileX = g_golferPool[rosterGolfer].x >> TILE_WORLD_SHIFT;
                                        g_cameraTileY = g_golferPool[rosterGolfer].y >> TILE_WORLD_SHIFT;
                                    }
                                    continue;
                                }
                                int clickDistance;
                                if (!HIBYTE(GetAsyncKeyState(VK_CONTROL))) {
                                    nearestDistance = PICK_RADIUS;
                                    for (int golferIndex = 0; golferIndex < GOLFER_COUNT; golferIndex++) {
                                        if ((g_golferPool[golferIndex].holeIdx != 0)
                                            && (g_golferPool[golferIndex].screenX != -1)) {
                                            clickDistance =
                                                Math_Distance2D(g_clickX - g_golferPool[golferIndex].screenX,
                                                    g_clickY - g_golferPool[golferIndex].screenY);
                                            if (clickDistance < nearestDistance) {
                                                g_displayFocus = golferIndex + 1;
                                                nearestDistance = clickDistance;
                                                g_reportScreenId = 0;
                                            }
                                        }
                                    }
                                }
                                for (int staffIndex = 0; staffIndex < STAFF_COUNT; staffIndex++) {
                                    if ((g_staffSlots[staffIndex].flags != 0) && (g_staffSlots[staffIndex].job < 0)
                                        && (g_staffSlots[staffIndex].screenX != -1)) {
                                        clickDistance = Math_Distance2D(g_clickX - g_staffSlots[staffIndex].screenX,
                                            g_clickY - g_staffSlots[staffIndex].screenY);
                                        if (clickDistance < nearestDistance) {
                                            nearestDistance = clickDistance;
                                            g_displayFocus = 0;
                                            g_selectedItemIndex = staffIndex;
                                        }
                                    }
                                }
                                if (nearestDistance < PICK_RADIUS)
                                    continue;
                                if ((g_displayFocus <= 0) || (g_golferPool[g_displayFocus - 1].holeIdx != -1)) {
                                    if (((g_tileExtra[clickRow * GRID_SIZE + clickCol] & TILE_BUILDING) != 0)
                                        && (FindBuildingAtTile(clickRow, clickCol) == 0)) {
                                        if (g_mouseButton == 1) {
                                            if (((g_runModeFlags & GAME_TOURNAMENT) != 0) || (g_nextOpenHoleIndex <= 1))
                                                continue;
                                            RenderGolferPairingScreen();
                                        }
                                        if (g_mouseButton == MOUSE_RIGHT) {
                                            g_printBuffer[0] = 0;
                                            AppendCourseName(-1);
                                            ShowTextInputDialog("Rename Course...", NAME_DIALOG_X, NAME_DIALOG_Y,
                                                NAME_MAX_LENGTH);
                                            if ((g_printBuffer[0] == 0)
                                                || (ValidateAndTrimString(g_printBuffer) == 0)) {
                                                ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                                            } else {
                                                StoreStringInPool(0);
                                            }
                                        }
                                    }
                                } else {
                                    if (g_mouseButton == 1) {
                                        RenderGolferPairingScreen();
                                    } else {
                                        RenderGolferPairingScreen();
                                    }
                                    g_displayFocus = 0;
                                }
                                continue;
                            }
                            if (g_mouseButton == MOUSE_RIGHT) {
                                if ((((g_selectedBuildingKind != -1) || (g_reportScreenId != 0))
                                        || (g_displayFocus != 0))
                                    || (g_selectedItemIndex != -1)) {
                                    g_mouseButton = 0;
                                }
                                g_selectedItemIndex = -1;
                                g_selectedBuildingKind = -1;
                                g_displayFocus = 0;
                                g_reportScreenId = 0;
                                if (g_mouseButton == MOUSE_RIGHT) {
                                    if ((g_tooltipTileRow == clickRow) && (g_tooltipTileColumn == clickCol)) {
                                        DemolishTileAtPosition(clickRow, clickCol);
                                        ClearFloatingMessageAtPosition(clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                            clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF);
                                        g_tooltipTileRow = -1;
                                        continue;
                                    }
                                    ClearFloatingMessageAtPosition(g_tooltipTileRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                        g_tooltipTileColumn * TILE_WORLD_SIZE + TILE_WORLD_HALF);
                                    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                                    BuildDemolishTooltipText(clickRow, clickCol);
                                    AddFloatingMessage(clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                        clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, COLOR_BRIGHT_GREEN);
                                    g_tooltipTileRow = clickRow;
                                    g_tooltipTileColumn = clickCol;
                                }
                            }
                            if (((g_mouseButton == 1) && (g_toolbarView != TOOLBAR_BUILD_COURSE))
                                && ((g_toolbarView == TOOLBAR_SHOT
                                     && ((g_mouseClickDelay <= 0 && (g_clickY < TOOLBAR_TOP)))))) {
                                g_toolbarView = TOOLBAR_PEOPLE;
                                g_paintTerrain = -1;
                                g_selectedBuildingKind = -1;
                                g_golferPool[g_watchedPlayerIdx].targetX = g_golferPool[g_watchedPlayerIdx].shotFromX;
                                g_golferPool[g_watchedPlayerIdx].targetY = g_golferPool[g_watchedPlayerIdx].shotFromY;
                                continue;
                            }
                            if (IsPositionInvalid(clickRow, clickCol) != 0)
                                continue;
                            if (((g_reportScreenId != 0) && (g_clickY < g_speechWindowBottom + REPORT_PANEL_MARGIN))
                                && (abs(g_clickX - SCREEN_CENTER_X) < REPORT_PANEL_HALF_WIDTH)) {
                                g_reportScreenId = 0;
                                continue;
                            }
                            int tile;
                            if (g_selectedBuildingKind != -1) {
                                if ((g_selectedBuildingKind == SCENERY_LANDMARK) && (g_selectedVariant == -1)) {
                                    g_selectedBuildingKind = -1;
                                } else if (g_mouseButton == 1) {
                                    int siteClearCost = ValidateBuildingPlacement(clickRow, clickCol,
                                        g_facilityTypes[g_selectedBuildingKind].side
                                            + g_buildingLevelPerKind[g_selectedBuildingKind],
                                        g_selectedBuildingKind);
                                    if ((g_selectedBuildingKind == 0) && (siteClearCost != -1)) {
                                        siteClearCost = siteClearCost / 2;
                                    }
                                    int buildCost = (g_facilityTypes[g_selectedBuildingKind].cost
                                                        * (g_buildingLevelPerKind[g_selectedBuildingKind] + 2))
                                                        / 2
                                                    + siteClearCost;
                                    if ((g_selectedBuildingKind == SCENERY_LANDMARK)
                                        && (buildCost = ((g_selectedVariant * LANDMARK_PRICE_STEP + LANDMARK_BASE_PRICE)
                                                            * PRICE_MARKUP_PERCENT)
                                                        / 100,
                                            ((1 << g_selectedVariant) & g_freeLandmarks) != 0)) {
                                        buildCost = 0;
                                    }
                                    if ((siteClearCost == -1)
                                        || ((g_selectedBuildingKind != SCENERY_CLUBHOUSE
                                             && (CheckCanAfford(buildCost) == 0)))) {
                                        ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                                    } else if ((g_selectedBuildingKind == SCENERY_HOME_SITE)
                                               && (CalculateLotScore(clickRow, clickCol) < HOME_SITE_MIN_SCORE)) {
                                        strcpy(g_printBuffer,
                                            "This is not a very attractive location for a building lot. ");
                                        g_reportScreenId = 0;
                                        strcat(g_printBuffer,
                                            "Home buyers like water, woods, and grass near a golf hole with a good fun factor.");
                                        ShowStoryMessage(COLOR_PURPLE, 0, -1);
                                    } else {
                                        int facilityVariant = g_selectedVariant;
                                        switch (g_selectedBuildingKind) {
                                        case SCENERY_WILLOW_TREE:
                                            tile = clickCol + clickRow * GRID_SIZE;
                                            g_gameTileTerrainKind[clickRow * GRID_SIZE + clickCol] = TERRAIN_ELM_TREE;
                                            g_tileAnimFrame[tile] = 0;
                                            g_tileExtra[tile] = (g_tileExtra[tile] & TILE_CLEAR_GROWTH)
                                                                | (TILE_ANIMATING | TILE_SCENIC);
                                            g_tileVariationIdx[clickRow * GRID_SIZE + clickCol] = facilityVariant + 1;
                                            buildCost = g_facilityTypes[g_selectedBuildingKind].cost;
                                            SetTileDataAt(clickRow, clickCol, UNDO_SCENIC_TREE, buildCost);
                                            break;
                                        case 0:
                                            tile = clickCol + clickRow * GRID_SIZE;
                                            if ((g_tileExtra[tile] & TILE_PATH) == 0) {
                                                g_tileExtra[tile] |= TILE_PATH;
                                                if (g_gameTileTerrainKind[clickRow * GRID_SIZE + clickCol]
                                                    == TERRAIN_WATER) {
                                                    g_tileExtra[tile] &= TILE_CLEAR_SCENIC;
                                                }
                                                g_editedTileRowIdx = clickRow;
                                                g_editedTileColIdx = clickCol;
                                                RebuildTerrainElevation();
                                                for (int dir = 0; dir < DIRECTION_COUNT; dir += QUARTER_TURN) {
                                                    int row = g_dirRowStep[dir] + clickRow;
                                                    int col = g_dirColStep[dir] + clickCol;
                                                    if (((g_gameTileTerrainKind[row * GRID_SIZE + col]
                                                             == TERRAIN_HOME_SITE)
                                                            || (g_gameTileTerrainKind[row * GRID_SIZE + col]
                                                                == TERRAIN_BUILDING))
                                                        && (g_buildingTable[g_tileVariationIdx[row * GRID_SIZE + col]]
                                                                .kind
                                                            >= FIRST_BUILDING_TYPE)) {
                                                        g_tileExtra[row * GRID_SIZE + col] =
                                                            g_tileExtra[row * GRID_SIZE + col] | TILE_PATH;
                                                    }
                                                }
                                                SetTileDataAt(clickRow, clickCol, UNDO_PATH, buildCost);
                                                PlayPositionedSound(SOUND_PLACE_PATH,
                                                    clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                    clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, 0);
                                            } else {
                                                buildCost = 0;
                                            }
                                            break;
                                        case SCENERY_SCENIC_BRIDGE:
                                            tile = clickCol + clickRow * GRID_SIZE;
                                            if (g_gameTileTerrainKind[clickRow * GRID_SIZE + clickCol]
                                                == TERRAIN_WATER) {
                                                g_tileExtra[tile] = g_selectedVariant % BRIDGE_STYLES
                                                                    | (g_tileExtra[tile] & TILE_CLEAR_BRIDGE)
                                                                    | (TILE_SCENIC | TILE_PATH);
                                                buildCost = g_facilityTypes[g_selectedBuildingKind].cost;
                                                SetTileDataAt(clickRow, clickCol, UNDO_SCENIC_BRIDGE, buildCost);
                                                PlayPositionedSound(SOUND_PLACE_BRIDGE,
                                                    clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                    clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, 0);
                                            } else {
                                                ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                                                buildCost = 0;
                                            }
                                            break;
                                        case 1:
                                            if ((g_tileExtra[clickRow * GRID_SIZE + clickCol] & TILE_BENCH) == 0) {
                                                int hasWalkableSide = false;
                                                for (int dir = 0; dir < DIRECTION_COUNT; dir += QUARTER_TURN) {
                                                    if (IsNeighborWalkable(clickRow, clickCol, dir) != 0) {
                                                        hasWalkableSide = true;
                                                    }
                                                }
                                                if (hasWalkableSide) {
                                                    tile = clickCol + clickRow * GRID_SIZE;
                                                    g_tileExtra[tile] = (g_tileExtra[tile] & TILE_CLEAR_FLOWERS)
                                                                        | (TILE_BENCH | TILE_PATH);
                                                    g_tileVariationIdx[clickRow * GRID_SIZE + clickCol] =
                                                        g_selectedVariant % BENCH_STYLES;
                                                    SetTileDataAt(clickRow, clickCol, UNDO_BENCH, buildCost);
                                                    PlayPositionedSound(SOUND_PLACE_BENCH,
                                                        clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                        clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, 0);
                                                } else {
                                                    strcpy(g_printBuffer, "Can't build bench there.");
                                                    ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                                                    ShowStoryMessage(COLOR_DARK_RED, 1, -1);
                                                    buildCost = 0;
                                                }
                                            } else {
                                                buildCost = 0;
                                            }
                                            break;
                                        case SCENERY_FLOWER_BED:
                                            tile = clickCol + clickRow * GRID_SIZE;
                                            if ((g_tileExtra[tile] & TILE_FLOWERS) == 0) {
                                                g_tileExtra[tile] = g_tileExtra[tile] | (TILE_ANIMATING | TILE_FLOWERS);
                                                g_tileAnimFrame[tile] = 0;
                                                ConfigureAndPlaySound(SOUND_FLOWER_BED, SOUND_FULL_VOLUME, 0, 0, 0);
                                                tile = clickCol + clickRow * GRID_SIZE;
                                                g_tileExtra[tile] = g_tileExtra[tile] & TILE_CLEAR_FLOWER_SITE;
                                                g_tileVariationIdx[clickRow * GRID_SIZE + clickCol] = g_selectedVariant;
                                                g_gameTileTerrainKind[clickRow * GRID_SIZE + clickCol] = TERRAIN_ROUGH;
                                                SetTileDataAt(clickRow, clickCol, UNDO_FLOWER_BED, buildCost);
                                            } else {
                                                g_tileVariationIdx[clickRow * GRID_SIZE + clickCol] = facilityVariant;
                                                buildCost = 0;
                                            }
                                            break;
                                        case SCENERY_CLUBHOUSE: {
                                            int clubhouseSlot = PlaceBuildingOnTerrain(clickRow, clickCol,
                                                SCENERY_CLUBHOUSE, CLUBHOUSE_TILE_FLAGS);
                                            g_buildingTable[clubhouseSlot].flags =
                                                g_buildingTable[clubhouseSlot].flags | SLOT_REACHABLE;
                                            g_runModeFlags &= ~GAME_NO_CLUBHOUSE;
                                            UpdateBuildingAccessibility();
                                            for (int golferIndex = 0; golferIndex < GOLFER_COUNT; golferIndex++) {
                                                if (g_golferPool[golferIndex].holeIdx == -1) {
                                                    g_golferPool[golferIndex].x =
                                                        clickRow * TILE_WORLD_SIZE + CLUBHOUSE_DOOR_OFFSET;
                                                    g_golferPool[golferIndex].y =
                                                        clickCol * TILE_WORLD_SIZE + CLUBHOUSE_DOOR_OFFSET;
                                                }
                                            }
                                            g_allHolesArray[CLUBHOUSE_HOLE].teeX = clickRow;
                                            g_allHolesArray[CLUBHOUSE_HOLE].teeY = clickCol;
                                        } break;
                                        default:
                                            if (((g_selectedBuildingKind >= FIRST_BUILDING_TYPE)
                                                    && (g_selectedBuildingKind != SCENERY_HOME_SITE))
                                                && (g_buildingLevelPerKind[g_selectedBuildingKind] != 0)) {
                                                for (int slot = 0; slot < BUILDING_SLOT_COUNT; slot++) {
                                                    if (g_buildingTable[slot].kind == g_selectedBuildingKind) {
                                                        DemolishBuilding(slot);
                                                    }
                                                }
                                            }
                                            int newSlot =
                                                PlaceBuildingOnTerrain(clickRow, clickCol, g_selectedBuildingKind, 0);
                                            AppendScriptCommand(clickRow, clickCol,
                                                g_selectedBuildingKind | SCRIPT_PLACE_BUILDING);
                                            if (g_selectedBuildingKind == SCENERY_FLOWER_BED) {
                                                tile = clickCol + clickRow * GRID_SIZE;
                                                g_tileExtra[tile] = g_tileExtra[tile] | TILE_ANIMATING;
                                                g_tileAnimFrame[tile] = 0;
                                                ConfigureAndPlaySound(SOUND_FLOWER_BED, SOUND_FULL_VOLUME, 0, 0, 0);
                                                g_buildingTable[newSlot].count =
                                                    g_selectedVariant + FLOWER_BED_COUNT_BASE;
                                            }
                                            if (g_selectedBuildingKind == SCENERY_LANDMARK) {
                                                g_tileExtra[clickRow * GRID_SIZE + clickCol] =
                                                    g_tileExtra[clickRow * GRID_SIZE + clickCol] & TILE_CLEAR_GROWTH;
                                                g_buildingTable[newSlot].count = g_selectedVariant;
                                                buildCost =
                                                    ((g_selectedVariant * LANDMARK_PRICE_STEP + LANDMARK_BASE_PRICE)
                                                        * PRICE_MARKUP_PERCENT)
                                                    / 100;
                                                int landmarkBit = 1 << g_selectedVariant;
                                                if ((landmarkBit & g_freeLandmarks) != 0) {
                                                    buildCost = 0;
                                                    g_freeLandmarks = g_freeLandmarks & ~landmarkBit;
                                                }
                                                break;
                                            }
                                            if ((g_selectedBuildingKind >= FIRST_BUILDING_TYPE)
                                                && (g_selectedBuildingKind != SCENERY_HOME_SITE)) {
                                                g_buildingTable[newSlot].count =
                                                    g_buildingLevelPerKind[g_selectedBuildingKind];
                                                if (g_buildingLevelPerKind[g_selectedBuildingKind] == 0) {
                                                    NotifyEvent(EVENT_NEW_BUILDING_TYPE, g_selectedBuildingKind);
                                                }
                                                ConfigureAndPlaySound(SOUND_BUILDING,
                                                    clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                    clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, 0, 0);
                                                UpdateBuildingAccessibility();
                                            }
                                            break;
                                        }
                                        if (((((((g_selectedBuildingKind == SCENERY_PRO_SHOP)
                                                    || (g_selectedBuildingKind == SCENERY_SNACK_BAR))
                                                   || (g_selectedBuildingKind == SCENERY_MARINA))
                                                  || ((g_selectedBuildingKind == SCENERY_SWIM_CLUB
                                                       || (g_selectedBuildingKind == SCENERY_CART_GARAGE))))
                                                 || (g_selectedBuildingKind == SCENERY_RESORT_HOTEL))
                                                || ((g_selectedBuildingKind == SCENERY_AIRSTRIP
                                                     || (g_selectedBuildingKind == SCENERY_LANDMARK))))
                                            || (g_selectedBuildingKind == SCENERY_CLUBHOUSE)) {
                                            int builtTile = clickCol + clickRow * GRID_SIZE;
                                            g_tileExtra[builtTile] = g_tileExtra[builtTile] | TILE_ANIMATING;
                                            g_tileAnimFrame[builtTile] = 0;
                                            ConfigureAndPlaySound(SOUND_CONSTRUCTION, SOUND_FULL_VOLUME, 0,
                                                -g_rng.NextInt(CONSTRUCTION_PITCH_JITTER), 0);
                                            tile = clickCol + clickRow * GRID_SIZE;
                                            g_tileExtra[tile] = g_tileExtra[tile] & TILE_CLEAR_GROWTH;
                                        }
                                        g_allHolesArray[CLUBHOUSE_HOLE].netCost =
                                            g_allHolesArray[CLUBHOUSE_HOLE].netCost + buildCost;
                                        g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS].v[LEDGER_FACILITIES] -=
                                            buildCost;
                                        if (g_selectedBuildingKind == SCENERY_HOME_SITE) {
                                            buildCost = buildCost
                                                        - CalculateLotScore(clickRow, clickCol) / APPEAL_PER_MONEY_UNIT;
                                            g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS].v[1] +=
                                                CalculateLotScore(clickRow, clickCol) / APPEAL_PER_MONEY_UNIT;
                                        }
                                        g_gameCashHundreds -= buildCost;
                                        if (g_selectedBuildingKind <= SCENERY_LANDMARK) {
                                            AddMoneyPopup(-buildCost, clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                FindNearestHole(clickRow, clickCol));
                                        } else {
                                            AddMoneyPopup(-buildCost, clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, -1);
                                        }
                                        if ((g_selectedBuildingKind < FIRST_BUILDING_TYPE)
                                            || (g_selectedBuildingKind == SCENERY_WILLOW_TREE)) {
                                            if (g_selectedBuildingKind == SCENERY_LANDMARK) {
                                                g_selectedBuildingKind = -1;
                                            }
                                        } else {
                                            g_selectedBuildingKind = -1;
                                        }
                                    }
                                }
                                if (g_mouseButton == MOUSE_RIGHT) {
                                    tile = clickCol + clickRow * GRID_SIZE;
                                    if (((g_gameTileTerrainKind[clickRow * GRID_SIZE + clickCol] == TERRAIN_HOME_SITE)
                                            || (g_gameTileTerrainKind[clickRow * GRID_SIZE + clickCol]
                                                == TERRAIN_BUILDING))
                                        || ((g_tileExtra[tile] & (TILE_BUILDING | TILE_BENCH)) != 0)) {
                                        int clickedSlot = FindBuildingAtTile(clickRow, clickCol);
                                        short kind = g_buildingTable[clickedSlot].kind;
                                        if (gameChosen == 0
                                            || (((kind != SCENERY_LANDMARK)
                                                    || (LAST_FIXED_LANDMARK < g_buildingTable[clickedSlot].count))
                                                && ((clickedSlot != -1 && (clickedSlot != 0))))) {
                                            strcpy(g_printBuffer, "Do you really want to\ndemolish your ");
                                            strcat(g_printBuffer, g_facilityTypes[kind].name);
                                            strcat(g_printBuffer, "?\n Yes I do.\n Ooops, no.\n");
                                            if ((ShowActionDialog(SCREEN_CENTER_X, DIALOG_TOP, 1, 1, 0) == 0)
                                                && (clickedSlot != 0)) {
                                                if (g_buildingTable[clickedSlot].kind == SCENERY_HOME_SITE) {
                                                    int buybackPrice = CalculateLotScore(clickRow, clickCol) / 2
                                                                       + g_buildingTable[clickedSlot].count
                                                                             / HOME_VALUE_BUYBACK_DIVISOR;
                                                    g_gameCashHundreds -= buybackPrice;
                                                    AddMoneyPopup(-buybackPrice,
                                                        clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                        clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, -1);
                                                    g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS].v[1] -=
                                                        buybackPrice;
                                                }
                                                kind = g_buildingTable[clickedSlot].kind;
                                                if (kind < FIRST_BUILDING_TYPE) {
                                                    int demolishRefund = g_facilityTypes[kind].cost;
                                                    if (kind == SCENERY_LANDMARK) {
                                                        demolishRefund =
                                                            (g_buildingTable[clickedSlot].count + LANDMARK_REFUND_BASE)
                                                            * LANDMARK_REFUND_STEP;
                                                    }
                                                    AddMoneyPopup(demolishRefund,
                                                        clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                        clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, -1);
                                                    g_gameCashHundreds += demolishRefund;
                                                    g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS]
                                                        .v[LEDGER_FACILITIES] -= demolishRefund;
                                                }
                                                DemolishBuilding(clickedSlot);
                                            }
                                        }
                                    }
                                }
                                UpdateBuildingAccessibility();
                                RebuildTerrainElevation();
                                continue;
                            }
                            if (g_toolbarView == TOOLBAR_GOLFERS)
                                continue;
                            if (g_paintTerrain == -1) {
                                redispatch = -1;
                                break;
                            }
                            if (g_paintTerrain < PAINT_SPECIAL_BASE) {
                                if (g_mouseButton == 1) {
                                    unsigned neighborCoverBits = 0;
                                    int sideCoverBits = 0;
                                    for (int dir = 0; dir < DIRECTION_COUNT; dir++) {
                                        if (IsPositionInvalid(clickRow + g_dirRowStep[dir],
                                                clickCol + g_dirColStep[dir])
                                            == 0) {
                                            int neighborCover =
                                                g_gameTileTerrainKind[(clickRow + g_dirRowStep[dir]) * GRID_SIZE
                                                                      + clickCol + g_dirColStep[dir]];
                                            neighborCoverBits = neighborCoverBits | 1 << neighborCover;
                                            if ((dir & 1) == 0) {
                                                sideCoverBits = sideCoverBits | 1 << neighborCover;
                                            }
                                        }
                                    }
                                    int canPaint = true;
                                    int paintCost =
                                        g_squareTypeTable[g_gameTileTerrainKind[clickRow * GRID_SIZE + clickCol]]
                                            .clearCost
                                        + g_squareTypeTable[g_paintTerrain].price;
                                    if (IsPositionInvalid(clickRow, clickCol) != 0) {
                                        ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                                        canPaint = false;
                                    } else {
                                        if (g_squareTypeTable[g_gameTileTerrainKind[clickRow * GRID_SIZE + clickCol]]
                                                .clearCost
                                            >= COSTLY_CLEARING_COST) {
                                            ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                                        }
                                    }
                                    if (((g_nextOpenHoleIndex >= CLUBHOUSE_HOLE) && (g_mouseButton == 1))
                                        && ((g_paintTerrain == 0
                                             || ((g_paintTerrain == TERRAIN_GREEN
                                                  && ((sideCoverBits & COVER_BIT_GREEN) == 0)))))) {
                                        canPaint = false;
                                    }
                                    if (CheckCanAfford(paintCost) == 0) {
                                        canPaint = false;
                                    }
                                    tile = clickCol + clickRow * GRID_SIZE;
                                    if ((g_tileExtra[tile] & TILE_PERMANENT) != 0) {
                                        canPaint = false;
                                    }
                                    int oldCover = g_gameTileTerrainKind[clickRow * GRID_SIZE + clickCol];
                                    if ((canPaint) && ((g_tileExtra[tile] & TILE_BUILDING) != 0)) {
                                        int clickedSlot = FindBuildingAtTile(clickRow, clickCol);
                                        int kind = g_buildingTable[clickedSlot].kind;
                                        if ((g_buildingTable[clickedSlot].kind == SCENERY_LANDMARK)
                                            && (g_buildingTable[clickedSlot].count <= LAST_FIXED_LANDMARK)) {
                                            canPaint = false;
                                            clickedSlot = -1;
                                        }
                                        if ((1 < g_facilityTypes[kind].side) && (clickedSlot != -1)) {
                                            strcpy(g_printBuffer, "Do you really want to\ndemolish your ");
                                            strcat(g_printBuffer, g_facilityTypes[kind].name);
                                            strcat(g_printBuffer, "?\n Yes I do.\n Ooops, no.\n");
                                            if (ShowActionDialog(SCREEN_CENTER_X, DIALOG_TOP, 1, 1, 0) != 0) {
                                                canPaint = false;
                                            }
                                        }
                                        if (canPaint) {
                                            DemolishBuilding(clickedSlot);
                                        }
                                    }
                                    short tileFlags = g_tileExtra[clickRow * GRID_SIZE + clickCol];
                                    int tileHole = tileFlags & TILE_HOLE_MASK;
                                    if (((g_runModeFlags & GAME_TOURNAMENT) != 0)
                                        && ((oldCover == 0 || ((oldCover == 1 && ((tileFlags & TILE_PIN) != 0)))))) {
                                        canPaint = false;
                                        ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                                    }
                                    int gameOptions = g_runModeFlags;
                                    if ((g_mouseButton == 1) && (canPaint)) {
                                        int nextHole;
                                        switch (g_paintTerrain) {
                                        case 1:
                                            nextHole = g_nextOpenHoleIndex;
                                            if (((g_allHolesArray[nextHole].aimX == 0) && (clickRow != 0))
                                                && (((sideCoverBits & COVER_BIT_GREEN) == 0
                                                     || (g_allHolesArray[nextHole].teeX != 0)))) {
                                                if (oldCover == 0) {
                                                    int teeHole =
                                                        g_tileExtra[clickRow * GRID_SIZE + clickCol] & TILE_HOLE_MASK;
                                                    g_allHolesArray[teeHole].par = 0;
                                                    g_allHolesArray[teeHole].teeX = 0;
                                                } else if ((oldCover == 1)
                                                           && ((g_tileExtra[clickRow * GRID_SIZE + clickCol] & TILE_PIN)
                                                               != 0)) {
                                                    canPaint = false;
                                                    break;
                                                }
                                                if (nextHole < CLUBHOUSE_HOLE) {
                                                    g_allHolesArray[nextHole].aimX = clickRow;
                                                    g_allHolesArray[nextHole].aimY = clickCol;
                                                    g_tileExtra[clickRow * GRID_SIZE + clickCol] = nextHole | TILE_PIN;
                                                }
                                            } else if (oldCover != 1) {
                                                tile = clickCol + clickRow * GRID_SIZE;
                                                g_tileExtra[tile] = g_tileExtra[tile] & TILE_CLEAR_HOLE;
                                            }
                                            break;
                                        case 0:
                                            nextHole = g_nextOpenHoleIndex;
                                            neighborCoverBits = neighborCoverBits & COVER_CLEAR_TEE;
                                            if (g_allHolesArray[nextHole].teeX != 0) {
                                                if ((g_allHolesArray[nextHole].secondTeeX == 0)
                                                    && ((g_runModeFlags & GAME_SECOND_TEES) != 0)) {
                                                    if (((neighborCoverBits & 1) == 0) && (clickRow != 0)) {
                                                        g_allHolesArray[nextHole].secondTeeX = clickRow;
                                                        g_allHolesArray[nextHole].secondTeeY = clickCol;
                                                        tile = clickCol + clickRow * GRID_SIZE;
                                                        g_tileExtra[tile] = nextHole;
                                                        g_tileVariationIdx[clickRow * GRID_SIZE + clickCol] = 0;
                                                    } else {
                                                        canPaint = false;
                                                    }
                                                } else if ((neighborCoverBits & 1) == 0) {
                                                    canPaint = false;
                                                }
                                                strcpy(g_printBuffer, "You've already built a tee for hole ");
                                                strcat(g_printBuffer, itoa(nextHole, g_numberBuf, 10));
                                                nextHole = g_nextOpenHoleIndex;
                                                char *nextStepAdvice =
                                                    ". You need to open this hole (press 'h') before you can build another one.";
                                                if (g_allHolesArray[nextHole].aimX == 0) {
                                                    nextStepAdvice = ". Next you need to add a green to this hole.";
                                                }
                                                strcat(g_printBuffer, nextStepAdvice);
                                                strcat(g_printBuffer, " (If you want to move the tee for hole ");
                                                strcat(g_printBuffer, itoa(nextHole, g_numberBuf, 10));
                                                strcat(g_printBuffer,
                                                    ", delete the old tee (right-click on it) first.)");
                                                ShowStoryMessage(COLOR_DARK_RED, 1, -1);
                                            } else {
                                                if ((neighborCoverBits & 1) == 0) {
                                                    if (oldCover == 0) {
                                                        canPaint = false;
                                                    } else if ((oldCover == 1)
                                                               && ((g_tileExtra[clickRow * GRID_SIZE + clickCol]
                                                                       & TILE_PIN)
                                                                   != 0)) {
                                                        int pinHole = g_tileExtra[clickRow * GRID_SIZE + clickCol]
                                                                      & TILE_HOLE_MASK;
                                                        g_allHolesArray[pinHole].par = 0;
                                                        g_allHolesArray[pinHole].aimX = 0;
                                                    }
                                                    g_allHolesArray[nextHole].teeX = clickRow;
                                                    g_allHolesArray[nextHole].teeY = clickCol;
                                                    g_tileExtra[clickRow * GRID_SIZE + clickCol] = nextHole;
                                                    if ((gameOptions & GAME_SECOND_TEES) == 0) {
                                                        g_allHolesArray[nextHole].secondTeeX = clickRow;
                                                        g_allHolesArray[nextHole].secondTeeY = clickCol;
                                                    }
                                                } else {
                                                    canPaint = false;
                                                }
                                            }
                                        }
                                    }
                                    if (canPaint) {
                                        tile = clickCol + clickRow * GRID_SIZE;
                                        g_tileExtra[tile] = g_tileExtra[tile] & TILE_CLEAR_FLOWERS;
                                        tileFlags = g_tileExtra[tile];
                                        if ((g_mouseButton == 1) && ((tileFlags & TILE_BENCH) == 0)) {
                                            g_tileVariationIdx[tile] =
                                                g_curBuildingTurns % NEW_VARIATION_BASE + NEW_VARIATION_BASE;
                                        }
                                        if ((g_paintTerrain == TERRAIN_GREEN) && (g_mouseButton == 1)) {
                                            g_tileVariationIdx[tile] =
                                                ((g_curBuildingTurns & 1) != 0) ? GREEN_VARIATION_ALTERNATE : 0;
                                        }
                                        if ((g_paintTerrain != oldCover) || (g_mouseButton == MOUSE_RIGHT)) {
                                            if (g_mouseButton == 1) {
                                                g_tileExtra[tile] = (tileFlags & TILE_CLEAR_REPAINT) | TILE_ANIMATING;
                                                g_tileAnimFrame[tile] = 0;
                                                TrySpawnWildlifeHabitat(clickRow, clickCol, g_paintTerrain);
                                                int paintedTile = clickCol + clickRow * GRID_SIZE;
                                                if ((g_tileExtra[paintedTile] & TILE_BENCH) == 0) {
                                                    g_tileVariationIdx[clickRow * GRID_SIZE + clickCol] =
                                                        g_curBuildingTurns % NEW_VARIATION_BASE + NEW_VARIATION_BASE;
                                                }
                                                switch (g_squareTypeTable[g_paintTerrain].category) {
                                                case CATEGORY_SAND:
                                                case CATEGORY_WASTE_BUNKER:
                                                    PlayPositionedSound(SOUND_PAINT_BUNKER,
                                                        clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                        clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, 0);
                                                    break;
                                                case CATEGORY_ROUGH:
                                                    PlayPositionedSound((g_paintTerrain != TERRAIN_ROCKS)
                                                                            ? SOUND_PAINT_ROUGH
                                                                            : SOUND_PLACE_ROCKS,
                                                        clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                        clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, 0);
                                                    break;
                                                case CATEGORY_FAIRWAY:
                                                    g_useEffectPitches = 1;
                                                    PlayPositionedSound(SOUND_PAINT_FAIRWAY,
                                                        clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                        clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, 0);
                                                    break;
                                                case CATEGORY_WATER:
                                                    PlayPositionedSound(SOUND_PLACE_WATER,
                                                        clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                        clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, 0);
                                                    break;
                                                case CATEGORY_ROCKS:
                                                    PlayPositionedSound(SOUND_PLACE_ROCKS,
                                                        clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                        clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, 0);
                                                    break;
                                                case 0:
                                                case 1:
                                                    PlayPositionedSound(SOUND_PAINT_TEE_GREEN,
                                                        clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                        clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, 0);
                                                    break;
                                                }
                                                int paintType = g_paintTerrain;
                                                if (g_paintTerrain == 0) {
                                                    g_tileVariationIdx[clickRow * GRID_SIZE + clickCol] = 0;
                                                }
                                                if (g_level == 0) {
                                                    if (paintType == TERRAIN_ELM_TREE) {
                                                        int elmTile = clickCol + clickRow * GRID_SIZE;
                                                        g_tileExtra[elmTile] = g_tileExtra[elmTile] | TILE_SCENIC;
                                                    }
                                                } else if (TREE_TRANSPLANT_LEVEL < g_level) {
                                                    if (g_squareTypeTable[paintType].category == CATEGORY_TREES) {
                                                        g_paintTerrain = TERRAIN_ROUGH;
                                                    }
                                                    if (g_squareTypeTable[oldCover].category == CATEGORY_TREES) {
                                                        g_paintTerrain = oldCover;
                                                    }
                                                }
                                                if ((oldCover != 1) && (oldCover != 0)) {
                                                    SetTileDataAt(clickRow, clickCol, oldCover, paintCost);
                                                }
                                            } else {
                                                g_selectedBuildingKind = -1;
                                                g_paintTerrain = -1;
                                            }
                                            g_gameCashHundreds -= paintCost;
                                            g_allHolesArray[g_nextOpenHoleIndex].netCost =
                                                g_allHolesArray[g_nextOpenHoleIndex].netCost + paintCost;
                                            AddMoneyPopup(-paintCost, clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                                clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, -1);
                                            g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS]
                                                .v[LEDGER_BUILD_COURSE] -= paintCost;
                                            tile = clickCol + clickRow * GRID_SIZE;
                                            g_simGolferWalkCostMap[clickRow][clickCol] =
                                                g_squareTypeTable[g_gameTileTerrainKind[clickRow * GRID_SIZE
                                                                                        + clickCol]]
                                                    .walk;
                                            if (g_gameTileTerrainKind[clickRow * GRID_SIZE + clickCol] != oldCover) {
                                                switch (oldCover) {
                                                case 0:
                                                    if (tileHole != 0) {
                                                        if (g_nextOpenHoleIndex > tileHole) {
                                                            g_nextOpenHoleIndex = tileHole;
                                                        }
                                                        if (((clickRow != g_allHolesArray[tileHole].secondTeeX)
                                                                || (clickCol != g_allHolesArray[tileHole].secondTeeY))
                                                            || (g_allHolesArray[tileHole].secondTeeX = 0,
                                                                (g_runModeFlags & GAME_SECOND_TEES) == 0)) {
                                                            g_allHolesArray[tileHole].teeX = 0;
                                                        }
                                                        g_allHolesArray[tileHole].par = 0;
                                                        if ((g_paintTerrain != TERRAIN_GREEN)
                                                            || ((g_tileExtra[tile] & TILE_PIN) == 0)) {
                                                            g_tileExtra[tile] = g_tileExtra[tile] & TILE_CLEAR_HOLE;
                                                        }
                                                    }
                                                    break;
                                                case 1:
                                                    if (tileHole != 0) {
                                                        if (g_nextOpenHoleIndex > tileHole) {
                                                            g_nextOpenHoleIndex = tileHole;
                                                        }
                                                        g_allHolesArray[tileHole].par = 0;
                                                        g_allHolesArray[tileHole].aimX = 0;
                                                        if (g_paintTerrain != 0) {
                                                            g_tileExtra[tile] = g_tileExtra[tile] & TILE_CLEAR_HOLE;
                                                        }
                                                    }
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                            } else if (g_mouseButton == 1) {
                                int toolCost = 0;
                                if (g_paintTerrain == PAINT_PATH) {
                                    int pathTile = clickCol + clickRow * GRID_SIZE;
                                    g_tileExtra[pathTile] = g_tileExtra[pathTile] | TILE_PATH;
                                    toolCost = (g_gameTileTerrainKind[clickRow * GRID_SIZE + clickCol] != TERRAIN_WATER)
                                                   ? PATH_TOOL_COST
                                                   : PATH_TOOL_COST_WATER;
                                    SetTileDataAt(clickRow, clickCol, PAINT_PATH, toolCost);
                                }
                                if (g_paintTerrain == PAINT_PATH_BENCH) {
                                    tile = clickCol + clickRow * GRID_SIZE;
                                    short *tileFlagsPtr = &g_tileExtra[tile];
                                    short tileFlags = *tileFlagsPtr;
                                    if ((tileFlags & TILE_PATH) != 0) {
                                        toolCost = BENCH_TOOL_COST;
                                        g_tileExtra[tile] = tileFlags;
                                        SetTileDataAt(clickRow, clickCol, PAINT_PATH_BENCH, BENCH_TOOL_COST);
                                    } else {
                                        ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                                    }
                                }
                                if (toolCost != 0) {
                                    g_gameCashHundreds -= toolCost;
                                    g_allHolesArray[g_nextOpenHoleIndex].netCost =
                                        g_allHolesArray[g_nextOpenHoleIndex].netCost + toolCost;
                                    AddMoneyPopup(-toolCost, clickRow * TILE_WORLD_SIZE + TILE_WORLD_HALF,
                                        clickCol * TILE_WORLD_SIZE + TILE_WORLD_HALF, -1);
                                    g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS].v[LEDGER_BUILD_COURSE] -=
                                        toolCost;
                                }
                            }
                        }
                        RecalculateAllWaterShoreDepths();
                        UpdateBuildingAccessibility();
                        g_editedTileRowIdx = clickRow;
                        g_editedTileColIdx = clickCol;
                        RebuildTerrainElevation();
                        continue;
                    }
                    if (g_fastRun == 0) {
                        g_updateTimeMs = timeGetTime() - frameStartTime;
                        if ((g_tileUphillMap[0][0] & VIEW_ELEVATION_REBUILT) != 0) {
                            g_tileUphillMap[0][0] = g_tileUphillMap[0][0] & VIEW_CLEAR_ELEVATION_REBUILT;
                        }
                        while (timeGetTime()
                               < ((g_runModeFlags & GAME_SLOW_PACE) != 0 ? FRAME_PACE_SLOW : FRAME_PACE_NORMAL) / 2
                                     + frameStartTime) {
                            WaitWithTimeout(1, 0);
                        }
                    }
                    if ((g_runModeFlags & GAME_ANY_TOURNAMENT) == GAME_CHAMPIONSHIP)
                        goto titleMenu;
                    if (((g_mouseButton != 0) || (g_quitToMenuFlag != 0)) || (g_moneyWarnCnt >= CAREER_OVER_WARNINGS)) {
                        g_quitToMenuFlag = 0;
                        if (((g_runModeFlags & GAME_SANDBOX) == 0) && (g_moneyWarnCnt >= CAREER_OVER_WARNINGS)) {
                            strcpy(g_printBuffer,
                                "Your career as a golf course designer\nhas ended.  Will you...\n Continue this game in Sandbox Mode.\n Return to Main Menu.\n");
                            if (ShowActionDialog(CAREER_OVER_DIALOG_X, CAREER_OVER_DIALOG_TOP, 1, 1, 0) == 0) {
                                g_runModeFlags |= GAME_SANDBOX;
                                g_moneyWarnCnt = 0;
                                continue;
                            }
                        } else if (g_moneyWarnCnt < CAREER_OVER_WARNINGS) {
                            char *farewellEnding;
                            if ((g_runModeFlags & GAME_CHAMPIONSHIP) != 0) {
                                strcpy(g_printBuffer, "After an exciting championship\n");
                                strcat(g_printBuffer, g_golferTypeArray[0].name);
                                farewellEnding = " returns home.\n";
                            } else {
                                strcpy(g_printBuffer, "After a ");
                                char *careerLength = "short";
                                if (g_careerPlayYear >= LONG_CAREER_YEARS) {
                                    careerLength = "lengthy";
                                }
                                strcat(g_printBuffer, careerLength);
                                strcat(g_printBuffer, " career\n");
                                strcat(g_printBuffer, g_golferTypeArray[0].name);
                                char *plansHisOrHer = " plans her";
                                if ((g_golferTypeArray[0].ageBits & GOLFER_TYPE_FEMALE) == 0) {
                                    plansHisOrHer = " plans his";
                                }
                                strcat(g_printBuffer, plansHisOrHer);
                                farewellEnding = " retirement.\n";
                            }
                            strcat(g_printBuffer, farewellEnding);
                            char *keepPlayingLine = " Wait, I don't want to quit yet!\n";
                            char *saveFirstLine = " I'd like to save this game first.\n";
                            strcat(g_printBuffer, keepPlayingLine);
                            strcat(g_printBuffer, saveFirstLine);
                            strcat(g_printBuffer, " So long for now.\n");
                            int choice = ShowActionDialog(SCREEN_CENTER_X, DIALOG_TOP, 1, 1, 0);
                            if (choice == 1) {
                                ShowSaveGameDialog();
                            } else if ((choice == 0) || (choice == -1))
                                continue;
                        }
                        g_runModeFlags &= ~GAME_CHAMPIONSHIP;
                        SaveGame("&QuitSave");
                        FadeOutSound(SOUND_COURSE_AMBIENCE);
                        goto titleMenu;
                    }
                } while (redispatch > 0);
                if (redispatch < 0)
                    break;
            }
        }
    }
}
