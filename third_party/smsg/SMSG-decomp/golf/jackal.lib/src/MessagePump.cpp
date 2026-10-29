#include "jgldefs.h"
#include "Degrees.h"
#include "MessagePump.h"
#include "Surface.h"
#include "SysMessageHandler.h"
#include "WidgetBase.h"
#include "Video.h"
#include "Network.h"

void wait_task()
{
    graphsys->wait_task();
}

int do_task()
{
    do_video();
    do_net();
    check_net();
    return graphsys->do_task();
}

void do_all_tasks()
{
    task_flags = 0x3f;
    while (do_task())
        task_flags = 0x3f;
    task_flags = 0;
    do_net();
}

int do_non_input()
{
    do_video();
    check_net();
    do_net();
    return graphsys->do_non_input();
}

void do_all_non_input()
{
    task_flags = 0x20;
    while (do_non_input())
        task_flags = 0x20;
    task_flags = 0;
    do_net();
    check_net();
}

int do_key()
{
    do_video();
    check_net();
    return graphsys->do_key();
}

void do_all_keys()
{
    task_flags = 2;
    while (do_key())
        task_flags = 2;
    task_flags = 0;
    do_net();
}

int do_mouse()
{
    do_video();
    check_net();
    return graphsys->do_mouse();
}

void do_all_mouse()
{
    task_flags = 8;
    while (do_mouse())
        task_flags = 8;
    task_flags = 0;
    do_net();
}

void flush_input()
{
    flush_keyboard();
    flush_mouse();
    check_net();
}

void flush_keyboard()
{
    graphsys->flush_keyboard();
    check_net();
}

void flush_mouse()
{
    graphsys->flush_mouse();
    g_mouseDownWidget = 0;
    g_mouseDownWidgetAlt = 0;
    check_net();
}

int PumpUserTick()
{
    check_net();
    return graphsys->do_timer();
}

void flush_timer()
{
    task_flags |= 0x3f;
    if (graphsys)
        graphsys->flush_timer();
    task_flags = 0;
}
