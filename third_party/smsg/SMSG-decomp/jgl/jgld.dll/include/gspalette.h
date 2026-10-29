#ifndef GSPALETTE_H
#define GSPALETTE_H
#include "gsobject.h"
struct tagPALETTEENTRY;
class GSPalette : public GSObject {
public:
    GSPalette();
    virtual ~GSPalette();
    virtual int              init() = 0;
    virtual void             close() = 0;
    virtual void             set() = 0;
    virtual int              get_entries(unsigned char *rgb, int first, int count) = 0;
    virtual int              set_entries(const unsigned char *rgb, int first, int count) = 0;
    virtual unsigned short  *get_555_table() = 0;
    virtual unsigned short  *get_565_table() = 0;
    virtual tagPALETTEENTRY *get_entry(int index) = 0;
};
#endif
