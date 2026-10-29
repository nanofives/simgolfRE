#include "jgldefs.h"
#include "crt.h"
#include "GameFlags.h"
#include "Golfer.h"
#include "Geometry.h"
#include "Plot.h"
#include "IsoView.h"
#include "TerrainCircle.h"
#include "ScreenText.h"
#include "RenderQueue.h"
#include "Toolbar.h"
#include "Screen.h"
#include "AssetSurface.h"
#include "FloatingText.h"
#include "Scorecard.h"
#include "ShotTrail.h"
#include "SaveGame.h"
#include "Employee.h"
#include "EmployeeRender.h"

char g_staffNumBuffer[12] = {0};
int g_carryAnimTick = 0;
int g_spareStaffWord = 0;
SpriteObject *g_lastStaffSprite = 0;

void RenderEmployees(void)
{
    int i;
    int sprId;

    if (g_selectedItemIndex >= CARRIED_GOLFER) {
        int gi = g_selectedItemIndex - 0x80;
        int dirIdx;
        SpriteObject *spr;
        int cx, cy;
        int tileRow, tileCol;
        int outfit;

        g_golferTypeArray[g_golferPool[gi].typeIdx].appearance &= APPEARANCE_LOW_BITS;
        if (IsGolferMale(gi))
            outfit = g_golferTypeArray[g_golferPool[gi].typeIdx].appearance >> 4;
        outfit = Clamp(outfit, 0, 9);
        dirIdx = (g_camDirIdx - g_golferPool[gi].dir - 2) & 7;
        g_simScreen.get_mouse_pos(&cx, &cy);
        ScreenToTileCoords(cx, cy, &tileCol, &tileRow, 0);
        if ((g_clock & 2) || (g_runModeFlags & GAME_PAUSED))
            DrawCircleOnTerrain(g_golferPool[gi].x, g_golferPool[gi].y, 0xc, COLOR_WHITE);
        strcpy(g_printBuffer, "I'm flying!");
        RenderToolbarTooltip(cx, cy - g_cameraZoom * 8);
        SetupGolferAppearance(gi);
        spr = GetFlicBankSprite(outfit + 30, g_carryAnimTick++ % g_flicBankFrameLens[outfit + 30], dirIdx);
        g_lastStaffSprite = spr;
        AddSpriteToRenderQueue(cx, cy, 0x3e7, 0x1e, 0x28, spr, SPRITE_FULL_SCALE, gi | 0x100);
    }

    for (i = 0; i < 64; i++) {
        int scrX, scrY;
        int frame;
        int curX, curY;
        int tileRow, tileCol;
        int dirIdx;
        int extra;
        int grp;
        int gf;
        SpriteObject *spr;

        g_staffSlots[i].screenX = -1;
        if (g_staffSlots[i].flags == 0)
            continue;
        if (WorldToScreen(g_staffSlots[i].posX, g_staffSlots[i].posY, &scrX, &scrY, 0) == 0 && g_selectedItemIndex != i)
            continue;
        g_staffSlots[i].screenX = scrX;
        g_staffSlots[i].screenY = scrY;
        dirIdx = (g_camDirIdx - g_staffSlots[i].dir - 2) & 7;
        extra = -2 - g_staffSlots[i].job;
        if (g_staffSlots[i].flags & STAFF_UPGRADED)
            extra += 4;
        gf = g_runModeFlags;
        sprId = extra + 0x82;
        if (g_staffSlots[i].job == JOB_PLAYER_AVATAR) {
            if (g_watchedPlayerIdx != -1)
                continue;
            if ((gf & GAME_TOURNAMENT) && g_golferPool[1].holeIdx)
                continue;
            sprId = 0x63;
            g_golferPool[PREVIEW_GOLFER].ageFlags = g_golferTypeArray[0].ageBits;
            g_golferPool[PREVIEW_GOLFER].typeIdx = 0;
            g_golferPool[PREVIEW_GOLFER].talents = TALENT_LENGTH | TALENT_ACCURACY | TALENT_IMAGINATION;
            g_golferPool[PREVIEW_GOLFER].talentLevel = STAND_IN_POWER_TIER;
            g_golferPool[PREVIEW_GOLFER].persona = PERSONA_COMMISSIONER;
            extra = (g_golferTypeArray[0].appearance >> 4) & 3;
            if (g_golferTypeArray[0].ageBits & GOLFER_TYPE_FEMALE)
                extra += 5;
        }
        if (g_staffSlots[i].job >= 0) {
            extra = 0;
            sprId = 0xc;
        }
        if (g_staffSlots[i].anim < 11) {
            grp = 0x20e;
            if (g_staffSlots[i].job == JOB_PLAYER_AVATAR)
                grp = 0;
        } else {
            grp = 0x216;
            if (g_staffSlots[i].job == JOB_PLAYER_AVATAR)
                grp = 0xa;
            if (g_staffSlots[i].anim == 12) {
                grp = 0x21e;
                if (g_staffSlots[i].job == JOB_PLAYER_AVATAR)
                    grp = 0x1e;
            }
        }
        if ((gf & GAME_PAUSED) == 0) {
            g_staffSlots[i].animTick++;
            if (grp == 0x20e || grp == 0)
                g_staffSlots[i].animTick++;
        }
        grp += extra;
        frame = g_staffSlots[i].animTick % g_flicBankFrameLens[grp];
        if (frame == 0 && g_staffSlots[i].anim == 12)
            g_staffSlots[i].anim = 11;
        spr = GetFlicBankSprite(grp, frame, dirIdx);
        g_lastStaffSprite = spr;
        if (g_staffSlots[i].job != JOB_PLAYER_AVATAR)
            AddSpriteToRenderQueue(scrX, scrY, scrY, 0x1e, 0x28, spr, SPRITE_FULL_SCALE, sprId);
        else
            AddSpriteToRenderQueue(scrX, scrY, scrY, 0x1e, 0x28, spr, SPRITE_FULL_SCALE, 0x199);
        g_printBuffer[0] = 0;
        if (g_staffSlots[i].talkTimer == 0 && g_staffSlots[i].job < 0) {
            if (g_staffSlots[i].job == JOB_PLAYER_AVATAR)
                strcpy(g_printBuffer, g_golferTypeArray[0].name);
            else
                BuildEmployeeName(i);
            g_pMenu->SetFontsForStyles(g_cameraZoom > DEFAULT_ZOOM ? &g_smallManualFont : &g_boldArialFont, 0, 0, 0);
            if (g_runModeFlags & GAME_DEBUG_OVERLAY)
                strcat(g_printBuffer, itoa(i, g_staffNumBuffer, 10));
            if ((g_runModeFlags & 0x80) == 0 && g_cameraZoom >= DEFAULT_ZOOM && g_toolbarView != TOOLBAR_SHOT)
                DrawTextCenteredAtY(g_printBuffer, scrX, scrY,
                    (g_staffSlots[i].flags & STAFF_QUITTING) ? 0x80007d08 : 0x80007fff);
        }
        if (g_selectedItemIndex == i) {
            int size;
            unsigned int color;

            if ((g_clock & 2) || (g_runModeFlags & GAME_PAUSED))
                DrawCircleOnTerrain(g_staffSlots[i].posX, g_staffSlots[i].posY, 0xc, COLOR_WHITE);
            g_simScreen.get_mouse_pos(&curX, &curY);
            ScreenToTileCoords(curX, curY, &tileRow, &tileCol, 0);
            size = (g_staffSlots[i].job == -4) ? 6 : 4;
            if (g_staffSlots[i].job == -4 && (g_staffSlots[i].flags & STAFF_UPGRADED))
                size += 2;
            switch (g_staffSlots[i].job) {
            case -4: color = 0x80007ff0; break;
            case -3: color = 0x80000018; break;
            case -5: color = 0x800003ff; break;
            case -2: color = 0x80006318; break;
            default: color = 0x80006318; break;
            }
            DrawCircleOnTerrain(tileRow * 1024 + 512, tileCol * 1024 + 512, size * 25, color);
            if (g_staffSlots[i].job == JOB_PLAYER_AVATAR) {
                g_golferPool[PREVIEW_GOLFER].typeIdx = 0;
                g_golferPool[PREVIEW_GOLFER].ageFlags = g_golferTypeArray[0].ageBits;
                g_golferPool[PREVIEW_GOLFER].talents = TALENT_LENGTH | TALENT_ACCURACY | TALENT_IMAGINATION;
                g_golferPool[PREVIEW_GOLFER].talentLevel = STAND_IN_POWER_TIER;
                g_golferPool[PREVIEW_GOLFER].persona = PERSONA_PLAYER;
                g_golferPool[PREVIEW_GOLFER].activity = ACTIVITY_IDLE;
                SetupGolferAppearance(PREVIEW_GOLFER);
                sprId = 0;
            }
            DrawSpriteImmediate(curX, curY, 0, 0x1e, 0x28, g_lastStaffSprite, SPRITE_FULL_SCALE, sprId | 0x200);
        }
        g_staffSlots[i].animTick = frame;
    }
}

void PresentEmployeeScreen()
{
    g_simScreen.RenderFullSurface(0);
}
