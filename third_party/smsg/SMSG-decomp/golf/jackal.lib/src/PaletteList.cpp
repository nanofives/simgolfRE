#include "jgldefs.h"
#include "PaletteList.h"

int PaletteList::fill_entry(PalStruct *elem)
{
    FLCPalette *p = new FLCPalette;
    elem->palette = p;
    if (!p)
        return 4;
    m_created = p;
    return 0;
}

void PaletteList::kill_entry(PalStruct *elem)
{
    FLCPalette *p = elem->palette;
    if (p) {
        delete p;
        elem->palette = 0;
    }
}
