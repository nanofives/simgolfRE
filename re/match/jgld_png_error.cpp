// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// libpng 1.0.5 pngerror.c (see jgld_png_png.cpp).
#include "vendor/libpng-1.0.5/pngerror.c"
// MATCH: jgld.dll 0x10079340 _png_set_error_fn
// MATCH: jgld.dll 0x10079380 _png_get_error_ptr
// MATCH: jgld.dll 0x10078fe0 _png_error
// MATCH: jgld.dll 0x10079040 _png_warning
// MATCH: jgld.dll 0x100790a0 _png_chunk_error   // 2 names compile identically: name not determined
// MATCH: jgld.dll 0x10079100 _png_format_buffer
// MATCH: jgld.dll 0x10079250 _png_chunk_warning
// MATCH: jgld.dll 0x10079300 _png_default_warning
