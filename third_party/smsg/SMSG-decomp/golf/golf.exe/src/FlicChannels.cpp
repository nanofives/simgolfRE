#include "jgldefs.h"
#include "FlicChannels.h"
#include "FlicAnim.h"

FlicAnim g_channelBanks[591];
FlicSprite g_channelPlayback[591];
SpriteObject g_channelFrameSprites[591];
FLCPalette g_channelPalettes[591];

SpriteObject *FLC_GetNextFrameSurface(int channel)
{
    if (g_channelPlayback[channel].m_frame >= g_channelBanks[channel].FrameCount() - 1) {
        if (g_channelPlayback[channel].m_seq == g_channelBanks[channel].SequenceCount() - 1)
            return 0;
        g_channelPlayback[channel].seek_anim(g_channelPlayback[channel].m_seq + 1, 0, 0);
        return g_channelPlayback[channel].current_sprite();
    }
    return g_channelPlayback[channel].NextFrame();
}

SpriteObject *FLC_LoadAndGetSurface(const char *path, int channel, int *frameCount)
{
    FlicAnim *bank = &g_channelBanks[channel];
    if (bank->load(path, 0, &g_channelPalettes[channel], 0) == 0) {
        FlicSprite *player = &g_channelPlayback[channel];
        player->init(bank, 0);
        if (frameCount)
            *frameCount = bank->FrameCount();
        return player->current_sprite();
    }
    if (frameCount)
        *frameCount = 0;
    return 0;
}

void FLC_ResetAndCleanup(int channel)
{
    g_channelPlayback[channel].close();
    g_channelBanks[channel].close();
}

int g_channelSpare[2] = {0};
