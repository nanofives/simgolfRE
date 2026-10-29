#ifndef FIXEDMATH_H
#define FIXEDMATH_H

#define SIN_TABLE_STEPS     256
#define SIN_TABLE_GUARD     4

extern int sine_table[SIN_TABLE_STEPS + SIN_TABLE_GUARD];

void trig_init();
int sin_table(int angle, int amplitude);
int cos_table(int angle, int amplitude);

#endif
