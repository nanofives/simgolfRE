// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links libpng 1.0.5 (version strings at 0x1011de89/0x1011df05), compiled from the unmodified
// release source with the default configuration; vendored in vendor/libpng-1.0.5 (libpng license).
#include "vendor/libpng-1.0.5/pngrio.c"
// MATCH: jgld.dll 0x10078c40 _png_read_data
// MATCH: jgld.dll 0x10078cb0 _png_set_read_fn
// MATCH: jgld.dll 0x10078d40 _png_default_read_data
