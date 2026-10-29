#ifndef SYSMESSAGEHANDLER_H
#define SYSMESSAGEHANDLER_H

#include "WidgetBase.h"
#include "Surface.h"
#include "Scroll.h"
#include "Timer.h"
#include "Network.h"

class SysMessageHandler : public SysMessage {
public:
    virtual void OnSize(int w, int h);
    virtual void OnPaint(RECT *r);
    virtual long OnNCHitTest(int x, int y);
    virtual void OnKey(unsigned int vk, int down);
    virtual void OnChar(char ch);
    virtual void OnMouseMove(int x, int y);
    virtual void OnMouseWheel(int delta, int x, int y);
    virtual void OnLButtonDown(int dbl, int x, int y);
    virtual void OnLButtonUp(int x, int y);
    virtual void OnRButtonDown(int dbl, int x, int y);
    virtual void OnRButtonUp(int x, int y);
    virtual void OnActivate(int active, int minimized);
    virtual void OnJackalTimer(unsigned int wParam);
    virtual void OnNetMessage();
};

extern SysMessageHandler g_windowCallbacks;

extern WidgetBase *g_hoverWidget;
extern int g_resizeHit;
extern WidgetBase *g_resizingWidget;
extern int g_resizeGrabX;
extern int g_resizeGrabY;
extern void(__stdcall *g_afterInputHook)();
extern void(__cdecl *g_keyDownHook)(int key);
extern void(__cdecl *g_rButtonDownHook)(int, int);
extern int task_flags;

#endif
