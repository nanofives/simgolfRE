#include "jgldefs.h"
#include "crt.h"
#include "Screen.h"
#include "ScreenText.h"
#include "SaveGame.h"
#include "RenderQueue.h"
#include "TerrainGrid.h"
#include "IsoView.h"
#include "InputPoll.h"

int g_keypadEnter = 0;
static int s_keySpare0 = 0;
static int s_keySpare1 = 0;
static int s_keySpare2 = 0;
static int s_keySpare3 = 0;
int g_heldKey = 0;

static inline void EndExpiredWait()
{
    if (g_skipWaitUntil == 0 || timeGetTime() > g_skipWaitUntil)
        g_waitActive = 0;
}

void ClearPendingInput()
{
    flush_input();
    g_mouseButton = 0;
    g_pendingChar = 0;
    g_pendingKey = 0;
}

void Screen::on_left_click(int x, int y)
{
    g_mouseX = x * 2 / ScaleXCoordinate(2);
    g_mouseY = y * 2 / ScaleXCoordinate(2);
    g_clickX = x;
    g_clickY = y;
    g_ctrlDown = (unsigned char)((unsigned short)GetAsyncKeyState(0x11) >> 8);
    g_shiftDown = (unsigned char)((unsigned short)GetAsyncKeyState(0x10) >> 8);
    g_mouseButton = 1;
    g_mouseEvent = 1;
    g_mouseDoubleClick = 0;
}

void Screen::on_left_down(int x, int y)
{
    g_mouseDownX = x;
    g_mouseDownY = y;
    EndExpiredWait();
}

void Screen::on_left_up(int, int)
{
    g_mouseDownX = -1;
    EndExpiredWait();
}

void Screen::on_right_click(int x, int y)
{
    g_mouseX = x * 2 / ScaleXCoordinate(2);
    g_mouseY = y * 2 / ScaleXCoordinate(2);
    g_clickX = x;
    g_clickY = y;
    g_mouseButton = 2;
    g_ctrlDown = (unsigned char)((unsigned short)GetAsyncKeyState(0x11) >> 8);
    g_shiftDown = (unsigned char)((unsigned short)GetAsyncKeyState(0x10) >> 8);
    g_mouseEvent = 1;
    g_mouseDoubleClick = 0;
}

void Screen::on_right_down(int x, int y)
{
    g_mouseDownX = x;
    g_mouseDownY = y;
}

void Screen::on_right_up(int, int)
{
    g_mouseDownX = -1;
}

int Screen::on_key_click(int ch, int vk)
{
    if (vk >= 0x60 && vk <= 0x69)
        g_pendingChar = 0;
    else
        g_pendingChar = ch;
    g_pendingKey = vk;
    g_keypadEnter = (g_pendingKey == 0x1000d);
    EndExpiredWait();
    return 1;
}

int Screen::on_key_down(int vk)
{
    g_heldKey = g_pendingKey = vk;
    return 0;
}

int Screen::on_key_up(int)
{
    g_heldKey = 0;
    return 0;
}

void Screen::on_left_double_click(int x, int y)
{
    g_mouseX = x * 2 / ScaleXCoordinate(2);
    g_mouseY = y * 2 / ScaleXCoordinate(2);
    g_clickX = x;
    g_clickY = y;
    g_mouseButton = 1;
    g_mouseEvent = g_mouseDoubleClick = 1;
}

void Screen::on_right_double_click(int x, int y)
{
    g_mouseX = x * 2 / ScaleXCoordinate(2);
    g_mouseY = y * 2 / ScaleXCoordinate(2);
    g_clickX = x;
    g_clickY = y;
    g_mouseButton = 2;
    g_mouseEvent = g_mouseDoubleClick = 1;
}

void Screen::on_sys_close()
{
    exit(3);
}
