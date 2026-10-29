#ifndef SPRITEASSETS_H
#define SPRITEASSETS_H

#include "Sprite.h"
#include "AssetSurface.h"

extern SpriteObject g_themePaintImgs[80];
extern SpriteObject g_notReadPic;
extern SpriteObject g_gameDummyFrameArray[16];
extern SpriteObject g_simUnusedSpritePair[2];
extern SpriteObject g_gameGolferBodyGfx[16];
extern SpriteObject g_juniorsBarrelArt;
extern SpriteObject g_simUnreferencedBitmaps[20];
extern SpriteObject g_tinyBallPicture;
extern SpriteObject g_groundShadowPixel;
extern SpriteObject g_bigGolfBallDot;
extern SpriteObject g_speechStripTile;
extern SpriteObject g_talkBubblePointSpr;
extern SpriteObject g_selectHaloSprite;

extern SpriteObject g_themeFrame[][8];
extern SpriteObject g_themeAlphaFrames[][8];
extern SpriteObject g_bulldozeDimCursor;
extern SpriteObject g_bulldozeLitCursor;
extern SpriteObject g_bmpReserved;
extern SpriteObject g_simReservedGfx[4];
extern SpriteObject g_gameReservedSpriteObjs[16];
extern SpriteObject g_headBodyBgPic;

extern SpriteObject g_simUnreferencedImages[32];
extern SpriteObject g_courseBigSprs[12];
extern SpriteObject g_bubblePieces[40];
extern SpriteObject g_playerHeadBmpArray[432];
extern SpriteObject g_allExpressionHeads[120];
extern SpriteObject g_simHoleMarkArt[72];

extern SpriteObject g_simFlowerNeighborGfx[20];
extern SpriteObject g_courseCliffArt[224];
extern SpriteObject g_transPopUpButtonBmps[8];
extern SpriteObject g_transPopUpMaskSprites[8];
extern SpriteObject g_amenityPanelParts[12];
extern SpriteObject g_amenitiesMaskSprites[4];

extern SpriteObject g_toolPanelBgImages[16];
extern SpriteObject g_toolbarPanelBackings_A[16];
extern SpriteObject g_themeTerrainButtonBmps[96];
extern SpriteObject g_lowerLeftBarParts[42];
extern SpriteObject g_lowerLeftBarAlphaSprites[28];
extern SpriteObject g_buildingPnlFrames[24];
extern SpriteObject g_amenitiesIcons[30];
extern SpriteObject g_peopleButtonBmps[44];
extern SpriteObject g_joeCoolToolbarBmpsAlpha[2];

extern SpriteObject g_staffPanelSpr[40];
extern SpriteObject g_gameGlowingPanelDiscs[10];
extern SpriteObject g_simElevationPanelButtonFrames[15];
extern SpriteObject g_memberWindowSprite[17];
extern SpriteObject g_trophyPartCells[70];
extern SpriteObject g_trophiesMasksBmps[70];

extern SpriteObject g_mapSiteMarkerPics[32];
extern SpriteObject g_mapMarkerButtonAlphas[32];
extern SpriteObject g_simTopBoxes[4];
extern SpriteObject g_courseInfoBoxPicsAlphaMask[4];
extern SpriteObject g_s_courseInfo[4];
extern SpriteObject g_cgScreenSprites[50];

extern SpriteObject g_gameGolferStatsBtnSprs[30];
extern SpriteObject g_golferStatsBarCellsMask[30];
extern SpriteObject g_popupEdgeBmps[36];
extern SpriteObject g_transPopupCellsAlpha[36];
extern SpriteObject g_infoBtnImages[39];
extern SpriteObject g_infoButtonBitmapMask[39];

extern SpriteObject g_checkBoxFrames[3];
extern SpriteObject g_siteMapSprs[16];
extern SpriteObject g_regionAlphaFrames[16];
extern SpriteObject g_bldgSprites[10][4];
extern SpriteObject g_s_transPopUpSprites[9];
extern SpriteObject g_messageIconPics[5];
extern SpriteObject g_speakerIconsAlpha[5];
extern SpriteObject g_simTitlePageGraphics[14];

extern AssetSurface g_golferHeadPage;

#define PALETTE_COLORS               0x100
#define TITLE_LOGO_LIT_SPRITE        13
#define GLYPH_HOLE_AWARD             13
#define GLYPH_HOME_SOLD              17
#define GLYPH_STORY                  30
#define PANEL_PORTRAIT_FRAME         29

#endif
