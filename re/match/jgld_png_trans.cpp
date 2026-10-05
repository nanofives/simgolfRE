// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links libpng 1.0.5 (version strings at 0x1011de89/0x1011df05), compiled from the unmodified
// release source with the default configuration; vendored in vendor/libpng-1.0.5 (libpng license).
#include "vendor/libpng-1.0.5/pngtrans.c"
// MATCH: jgld.dll 0x1006d630 _png_set_swap
// MATCH: jgld.dll 0x1006d700 _png_set_shift
// MATCH: jgld.dll 0x1006d750 _png_set_interlace_handling
// MATCH: jgld.dll 0x1006d7a0 _png_set_filler
// MATCH: jgld.dll 0x1006d900 _png_do_invert
// MATCH: jgld.dll 0x1006d980 _png_do_swap
// MATCH: jgld.dll 0x1006da10 _png_do_packswap
// MATCH: jgld.dll 0x1006dac0 _png_do_strip_filler
// MATCH: jgld.dll 0x1006e010 _png_do_bgr
// MATCH: jgld.dll 0x1006d600 _png_set_bgr
// MATCH: jgld.dll 0x1006d670 _png_set_packing
// MATCH: jgld.dll 0x1006d6c0 _png_set_packswap
// MATCH: jgld.dll 0x1006d850 _png_set_swap_alpha
// MATCH: jgld.dll 0x1006d890 _png_set_invert_alpha
// MATCH: jgld.dll 0x1006d8d0 _png_set_invert_mono
// MATCH: jgld.dll 0x1006e210 _png_set_user_transform_info
// MATCH: jgld.dll 0x1006e250 _png_get_user_transform_ptr
