#ifndef GOLF_PALETTELIST_H
#define GOLF_PALETTELIST_H

#include "ClassList.h"
#include "FLCPalette.h"

struct PalStruct : virtual HeapNew {
    FLCPalette *palette;
};

struct PaletteList : ClassList<PalStruct> {
    FLCPalette *m_created;

    virtual int fill_entry(PalStruct *elem);
    virtual void kill_entry(PalStruct *elem);
    ~PaletteList() { Clear(); }

    static PaletteList s_ourPalettes;
};

extern FLCPalette g_startupPalette;
extern FLCPalette g_identityAltPalette;

void ReleaseAllPalettes();
void InitIdentityPalette(int);

#endif
