// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links libpng 1.0.5 (version strings at 0x1011de89/0x1011df05), compiled from the unmodified
// release source with the default configuration; vendored in vendor/libpng-1.0.5 (libpng license).
#include "vendor/libpng-1.0.5/pngread.c"
// MATCH: jgld.dll 0x1006bac0 _png_create_read_struct
// MATCH: jgld.dll 0x1006bca0 _png_read_init
// MATCH: jgld.dll 0x1006be20 _png_read_info
// MATCH: jgld.dll 0x1006c3f0 _png_read_row
// MATCH: jgld.dll 0x1006cb20 _png_read_rows
// MATCH: jgld.dll 0x1006cd00 _png_read_end
// MATCH: jgld.dll 0x1006d250 _png_read_destroy
// MATCH: jgld.dll 0x1006c340 _png_read_update_info
// MATCH: jgld.dll 0x1006c3a0 _png_start_read_image
// MATCH: jgld.dll 0x1006cc50 _png_read_image
// MATCH: jgld.dll 0x1006d150 _png_destroy_read_struct
// MATCH: jgld.dll 0x1006d5d0 _png_set_read_status_fn
