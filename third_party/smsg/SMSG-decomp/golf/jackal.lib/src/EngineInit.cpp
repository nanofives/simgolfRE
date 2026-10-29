#include "jgldefs.h"
#include "EngineInit.h"
#include "filewin.h"
#include "BasePop.h"
#include "GraphsyLoader.h"
#include "Surface.h"
#include "WidgetBase.h"
#include "SysMessageHandler.h"
#include "PaletteList.h"
#include "CheckBoxCtl.h"
#include "ListBoxCtl.h"
#include "FixedMath.h"
#include "Strings.h"
#include "Timer.h"
#include "Display.h"

int quit = 0;
int jackal_flags = 0;

int jackal_init(const char *dll, const char *title, int flags)
{
    int rc;

    jackal_flags = flags;
    graphsys = LoadGraphsyDll(dll);
    if (!graphsys)
        return 25;

    if (flags & 0x8000) {
        rc = graphsys->init_app(&g_windowCallbacks, title, &WidgetBase::s_primaryBuffer);
        if (rc) {
            jackal_close();
            return rc;
        }
    }
    if (flags & 0x0001) {
        rc = WidgetBase::init_class(title, 1);
        if (rc)
            return rc;
    }
    if (flags & 0x0002) {
        InitIdentityPalette(0);
        g_activePalettePtr = &g_startupPalette;
        g_startupPalette.init();
        g_startupPalette.set();
    }
    if (flags & 0x0004) {
        rc = InitUnusedModule();
        if (rc)
            return rc;
    }
    if (flags & 0x0008) {
        rc = CheckBoxCtl::init_class();
        if (rc)
            return rc;
    }
    if (flags & 0x0010) {
        rc = CheckButton::init_class();
        if (rc)
            return rc;
    }
    if (flags & 0x0020) {
        rc = ListBoxCtl::init_class();
        if (rc)
            return rc;
    }
    if (flags & 0x0080) {
        rc = SetDefaultFont(&default_font);
        if (rc)
            return rc;
        SetButtonDefaults(&default_font, 0, 0);
        g_widgetTitleFont = g_defaultFont;
    }
    if (flags & 0x0100) {
        rc = BasePop::init_class();
        if (rc)
            return rc;
    }
    if (flags & 0x0200) {
        rc = FileWin::init_class();
        if (rc)
            return rc;
    }
    if (flags & 0x0400) {
        rc = LoadSmallSprites();
        if (rc)
            return rc;
    }
    if (flags & 0x4000)
        trig_init();
    if (flags & 0x0800) {
        rc = init_cursor_class();
        if (rc)
            return rc;
    }
    if (flags & 0x2000) {
        if (strings.init(0x8000))
            return 1;
    }
    if (flags & 0x1000) {
        rc = InitTimers();
        if (rc)
            return rc;
    }
    return 0;
}

int InitUnusedModule()
{
    return 0;
}

void ShutdownUnusedModule()
{
}

void jackal_close()
{
    display_close();
    Time::close_class();
    ShutdownUnusedModule();
    ShutdownUnusedModule();
    CheckBoxCtl::close_class();
    ListBoxCtl::close_class();
    ReleaseDefaultFont();
    ReleaseSmallSprites();
    FileWin::close_class();
    BasePop::close_class();
    close_cursor_class();
    WidgetBase::close_class();
    ReleaseAllPalettes();
    strings.shutdown();
    FreeGraphsyDll();
}

int g_initSpare = 0;
