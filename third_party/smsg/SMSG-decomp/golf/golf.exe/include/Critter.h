#ifndef CRITTER_H
#define CRITTER_H

struct Critter {
    int x;
    int y;
    int state;
    short leader;
    short sprite;
    signed char animFrame;
    signed char dir;
    signed char kind;
    unsigned char pad13;
};
extern Critter g_gameAnimals[128];

struct AnimalKind {
    char name[16];
    char habitat;
    char themes;
};
extern AnimalKind g_animalKinds[9];

struct BackgroundActor {
    int x;
    int y;
    int height;
    signed char dir;
    signed char type;
    char pad0e[2];
};

#endif
