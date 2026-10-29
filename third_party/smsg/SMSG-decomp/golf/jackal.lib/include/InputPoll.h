#ifndef INPUTPOLL_H
#define INPUTPOLL_H

void do_all_non_input();
int do_key();
int do_mouse();
void do_all_mouse();

int WaitWithTimeout(int ticks, int pump);

void ProcessMouseMessages();
int WaitForInputWithTimeout(int timeout);
void WaitForInputRelease();
extern int g_waitChar;
extern int g_waitKey;
extern int quit;

extern int g_inputLatched;

int TakePendingKey();
int TakePendingChar();
int PollInputState();
void DrainPendingInput();
int ProcessInputAndCheckIdle();

#endif
