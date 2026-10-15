// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// libpng 1.0.5 pngerror.c (see jgld_png_png.cpp): the static png_default_error, round 36 batch m36e.
#include "vendor/libpng-1.0.5/pngerror.c"
// MATCH: jgld.dll 0x100792b0 _png_default_error
