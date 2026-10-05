// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links libpng 1.0.5 (version strings at 0x1011de89/0x1011df05), compiled from the unmodified
// release source with the default configuration; vendored in vendor/libpng-1.0.5 (libpng license).
#include "vendor/libpng-1.0.5/pngrtran.c"
// MATCH: jgld.dll 0x1006f0d0 _png_set_crc_action
// MATCH: jgld.dll 0x1006f230 _png_set_background
// MATCH: jgld.dll 0x1006f3a0 _png_set_dither
// MATCH: jgld.dll 0x1006ff80 _png_set_gamma
// MATCH: jgld.dll 0x100700f0 _png_set_rgb_to_gray
// MATCH: jgld.dll 0x100702a0 _png_init_read_transformations
// MATCH: jgld.dll 0x100714c0 _png_read_transform_info
// MATCH: jgld.dll 0x100717d0 _png_do_read_transformations
// MATCH: jgld.dll 0x10071e80 _png_do_unpack
// MATCH: jgld.dll 0x100720c0 _png_do_unshift
// MATCH: jgld.dll 0x10072430 _png_do_chop
// MATCH: jgld.dll 0x100724f0 _png_do_read_swap_alpha
// MATCH: jgld.dll 0x10072860 _png_do_read_invert_alpha
// MATCH: jgld.dll 0x10072bd0 _png_do_read_filler
// MATCH: jgld.dll 0x10073310 _png_do_gray_to_rgb
// MATCH: jgld.dll 0x100736c0 _png_do_rgb_to_gray
// MATCH: jgld.dll 0x10074630 _png_build_grayscale_palette
// MATCH: jgld.dll 0x10074750 _png_do_background
// MATCH: jgld.dll 0x10076760 _png_do_gamma
// MATCH: jgld.dll 0x10076ec0 _png_do_expand_palette
// MATCH: jgld.dll 0x10077a90 _png_do_dither
// MATCH: jgld.dll 0x10077d30 _png_build_gamma_table
// MATCH: jgld.dll 0x1006f330 _png_set_strip_16
// MATCH: jgld.dll 0x1006f360 _png_set_strip_alpha
// MATCH: jgld.dll 0x10070000 _png_set_expand   // 4 names compile identically: name not determined
// MATCH: jgld.dll 0x10070030 _png_set_gray_1_2_4_to_8   // 3 names compile identically: name not determined
// MATCH: jgld.dll 0x10070060 _png_set_palette_to_rgb   // 2 names compile identically: name not determined
// MATCH: jgld.dll 0x10070090 _png_set_tRNS_to_alpha
// MATCH: jgld.dll 0x100700c0 _png_set_gray_to_rgb
// MATCH: jgld.dll 0x10070260 _png_set_read_user_transform_fn
// MATCH: jgld.dll 0x10077300 _png_do_expand
