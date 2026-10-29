#include "jgldefs.h"
#include "CheckBoxCtl.h"
#include "EngineInit.h"

SpriteObject CheckBoxCtl::s_checkBoxEmpty;
SpriteObject CheckBoxCtl::s_checkBoxTicked;

SpriteObject *g_checkBoxSpritePair[2] = {&CheckBoxCtl::s_checkBoxEmpty, &CheckBoxCtl::s_checkBoxTicked};

int CheckButton::init_class()
{
    return 0;
}

int g_checkBoxSpare[2] = {0};
