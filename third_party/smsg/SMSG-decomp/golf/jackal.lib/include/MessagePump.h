#ifndef MESSAGEPUMP_H
#define MESSAGEPUMP_H

void wait_task();
int do_task();
void do_all_tasks();
int do_non_input();
void do_all_non_input();
int do_key();
void do_all_keys();
int do_mouse();
void do_all_mouse();
void flush_input();
void flush_keyboard();
void flush_mouse();
int PumpUserTick();
void flush_timer();

#endif
