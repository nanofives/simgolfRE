// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links libpng 1.0.5 (version strings at 0x1011de89/0x1011df05), compiled from the unmodified
// release source with the default configuration; vendored in vendor/libpng-1.0.5 (libpng license).
#include "vendor/libpng-1.0.5/pngmem.c"
// MATCH: jgld.dll 0x10078da0 _png_create_struct
// MATCH: jgld.dll 0x10078e60 _png_malloc
// MATCH: jgld.dll 0x10078ed0 _png_free
// MATCH: jgld.dll 0x10078f20 _png_memcpy_check   // 2 names compile identically: name not determined
// MATCH: jgld.dll 0x10078f80 _png_memset_check
// MATCH: jgld.dll 0x10078e20 _png_destroy_struct
