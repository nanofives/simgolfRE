#ifndef SCREEN_H
#define SCREEN_H

#include "WidgetBase.h"

struct Screen : GraphicWin {
    virtual void on_sys_close();
    virtual void on_left_click(int x, int y);
    virtual void on_left_down(int x, int y);
    virtual void on_left_up(int x, int y);
    virtual void on_right_click(int x, int y);
    virtual void on_right_down(int x, int y);
    virtual void on_right_up(int x, int y);
    virtual int on_key_click(int ch, int vk);
    virtual int on_key_down(int vk);
    virtual int on_key_up(int vk);
    virtual void on_left_double_click(int x, int y);
    virtual void on_right_double_click(int x, int y);
};

extern Screen g_simScreen;

extern int g_mouseX;
extern int g_mouseY;
extern int g_clickX;
extern int g_clickY;
extern int g_mouseButton;
extern int g_shiftDown;
extern int g_ctrlDown;
extern int g_mouseEvent;
extern int g_mouseDoubleClick;
extern int g_mouseDownX;
extern int g_mouseDownY;
extern int g_pendingKey;
extern int g_pendingChar;
extern int g_keypadEnter;
extern int g_heldKey;
extern unsigned long g_skipWaitUntil;
extern char g_waitActive;

extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
extern "C" __declspec(dllimport) unsigned long __stdcall timeGetTime(void);
void do_all_tasks();

void ClearPendingInput();

#endif
