// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links libpng 1.0.5 (version strings at 0x1011de89/0x1011df05), compiled from the unmodified
// release source with the default configuration; vendored in vendor/libpng-1.0.5 (libpng license).
#include "vendor/libpng-1.0.5/png.c"
// MATCH: jgld.dll 0x10078550 _png_get_header_version
// MATCH: jgld.dll 0x10078590 _png_set_sig_bytes
// MATCH: jgld.dll 0x100785f0 _png_sig_cmp
// MATCH: jgld.dll 0x10078670 _png_check_sig
// MATCH: jgld.dll 0x100786b0 _png_zalloc
// MATCH: jgld.dll 0x100787d0 _png_calculate_crc
// MATCH: jgld.dll 0x10078870 _png_create_info_struct
// MATCH: jgld.dll 0x10078980 _png_info_destroy
// MATCH: jgld.dll 0x10078b00 _png_convert_to_rfc1123
// MATCH: jgld.dll 0x10078be0 _png_get_copyright
// MATCH: jgld.dll 0x10078750 _png_zfree
// MATCH: jgld.dll 0x10078790 _png_reset_crc
// MATCH: jgld.dll 0x100788d0 _png_destroy_info_struct
// MATCH: jgld.dll 0x10078940 _png_info_init
// MATCH: jgld.dll 0x10078aa0 _png_get_io_ptr
// MATCH: jgld.dll 0x10078ad0 _png_init_io
// MATCH: jgld.dll 0x10078c20 _png_check_version
