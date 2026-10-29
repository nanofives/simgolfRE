#include "jgldefs.h"
#include <windows.h>
#include "Screen.h"
#include "InputPoll.h"

void ProcessMouseMessages()
{
    MSG msg;
    while (PeekMessageA(&msg, 0, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (g_mouseEvent == 0)
        g_mouseButton = 0;
    do_mouse();
    g_mouseEvent = 0;
}

int WaitForInputWithTimeout(int timeout)
{
    int now;
    int start = timeGetTime();
    g_waitChar = 0;
    g_mouseButton = 0;
    do {
        ProcessMouseMessages();
        now = timeGetTime();
    } while (!g_mouseButton && !PollInputState() && !quit && (timeout == 0 || now - start < timeout));
    g_waitChar = g_pendingChar;
    g_waitKey = g_pendingKey;
    if (PollInputState()) {
        g_waitChar = TakePendingChar();
        g_waitKey = TakePendingKey();
    }
    flush_input();
    return 1;
}

void WaitForInputRelease()
{
    g_mouseButton = 0;
    do {
        do_all_tasks();
        ProcessMouseMessages();
    } while (g_mouseButton != 0 || PollInputState() != 0);
}

void WaitForAnyInput()
{
    g_mouseButton = 0;
    do {
        WaitMessage();
        ProcessMouseMessages();
    } while (g_mouseButton == 0 && PollInputState() == 0 && quit == 0);
}

int g_waitChar;
int g_waitSpare = 0;
