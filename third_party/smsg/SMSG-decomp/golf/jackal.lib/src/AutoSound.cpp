#include "jgldefs.h"
#include "WidgetBase.h"

WidgetObserver *g_widgetObserverDefaults[38] = {0};

AutoSound::AutoSound()
{
    m_unusedObserver1 = g_widgetObserverDefaults[0];
    m_onShown = g_widgetObserverDefaults[1];
    m_onHidden = g_widgetObserverDefaults[2];
    m_unusedObserver2 = g_widgetObserverDefaults[3];
    m_unusedObserver3 = g_widgetObserverDefaults[4];
    m_onClose = g_widgetObserverDefaults[5];
    m_onMouseMove = g_widgetObserverDefaults[6];
    m_onLeftClick = g_widgetObserverDefaults[7];
    m_onLeftDown = g_widgetObserverDefaults[8];
    m_onLeftUp = g_widgetObserverDefaults[9];
    m_onRightClick = g_widgetObserverDefaults[10];
    m_onRightDown = g_widgetObserverDefaults[11];
    m_onRightUp = g_widgetObserverDefaults[12];
    m_onCharKey = g_widgetObserverDefaults[13];
    m_onKeyDown = g_widgetObserverDefaults[14];
    m_onKeyUp = g_widgetObserverDefaults[15];
    m_onLeftDoubleClick = g_widgetObserverDefaults[16];
    m_onRightDoubleClick = g_widgetObserverDefaults[17];
    m_onHoverChanged = g_widgetObserverDefaults[18];
    m_onMouseMoveAlt = g_widgetObserverDefaults[19];
    m_onLeftClickAlt = g_widgetObserverDefaults[20];
    m_onLeftDownAlt = g_widgetObserverDefaults[21];
    m_onLeftUpAlt = g_widgetObserverDefaults[22];
    m_onRightClickAlt = g_widgetObserverDefaults[23];
    m_onRightDownAlt = g_widgetObserverDefaults[24];
    m_onRightUpAlt = g_widgetObserverDefaults[25];
    m_onLeftDoubleClickAlt = g_widgetObserverDefaults[26];
    m_onRightDoubleClickAlt = g_widgetObserverDefaults[27];
    m_onButtonDown = g_widgetObserverDefaults[28];
    m_onButtonReleased = g_widgetObserverDefaults[29];
    m_onButtonPressed = g_widgetObserverDefaults[30];
    m_onButtonUp = g_widgetObserverDefaults[31];
    m_onScrollPosChanged = g_widgetObserverDefaults[32];
    m_onScrolling = g_widgetObserverDefaults[33];
    m_onMenuChoice = g_widgetObserverDefaults[34];
    m_onCloseButton = g_widgetObserverDefaults[35];
    m_onSelectionChanged = g_widgetObserverDefaults[36];
    m_onItemActivated = g_widgetObserverDefaults[37];
}

void AutoSound::ResetToDefaults()
{
    m_unusedObserver1 = g_widgetObserverDefaults[0];
    m_onShown = g_widgetObserverDefaults[1];
    m_onHidden = g_widgetObserverDefaults[2];
    m_unusedObserver2 = g_widgetObserverDefaults[3];
    m_unusedObserver3 = g_widgetObserverDefaults[4];
    m_onClose = g_widgetObserverDefaults[5];
    m_onMouseMove = g_widgetObserverDefaults[6];
    m_onLeftClick = g_widgetObserverDefaults[7];
    m_onLeftDown = g_widgetObserverDefaults[8];
    m_onLeftUp = g_widgetObserverDefaults[9];
    m_onRightClick = g_widgetObserverDefaults[10];
    m_onRightDown = g_widgetObserverDefaults[11];
    m_onRightUp = g_widgetObserverDefaults[12];
    m_onCharKey = g_widgetObserverDefaults[13];
    m_onKeyDown = g_widgetObserverDefaults[14];
    m_onKeyUp = g_widgetObserverDefaults[15];
    m_onLeftDoubleClick = g_widgetObserverDefaults[16];
    m_onRightDoubleClick = g_widgetObserverDefaults[17];
    m_onHoverChanged = g_widgetObserverDefaults[18];
    m_onMouseMoveAlt = g_widgetObserverDefaults[19];
    m_onLeftClickAlt = g_widgetObserverDefaults[20];
    m_onLeftDownAlt = g_widgetObserverDefaults[21];
    m_onLeftUpAlt = g_widgetObserverDefaults[22];
    m_onRightClickAlt = g_widgetObserverDefaults[23];
    m_onRightDownAlt = g_widgetObserverDefaults[24];
    m_onRightUpAlt = g_widgetObserverDefaults[25];
    m_onLeftDoubleClickAlt = g_widgetObserverDefaults[26];
    m_onRightDoubleClickAlt = g_widgetObserverDefaults[27];
    m_onButtonDown = g_widgetObserverDefaults[28];
    m_onButtonReleased = g_widgetObserverDefaults[29];
    m_onButtonPressed = g_widgetObserverDefaults[30];
    m_onButtonUp = g_widgetObserverDefaults[31];
    m_onScrollPosChanged = g_widgetObserverDefaults[32];
    m_onScrolling = g_widgetObserverDefaults[33];
    m_onMenuChoice = g_widgetObserverDefaults[34];
    m_onCloseButton = g_widgetObserverDefaults[35];
    m_onSelectionChanged = g_widgetObserverDefaults[36];
    m_onItemActivated = g_widgetObserverDefaults[37];
}

#include "AutoSoundDtor.h"
