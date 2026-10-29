
















#include "thunk_flush.h"
int g;
void f1() { g = 1; }
P::P() { p = 0; }
Q::Q() { q = 0; }
void f2() { g = 2; g = g * 3 + 1; g = g * 5 + 2; g = g * 7 + 3; g = g * 9 + 4; g = g * 11 + 5; }
Q::~Q() { g = 3; }
void f3() { g = 4; }








