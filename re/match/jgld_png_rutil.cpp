// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links libpng 1.0.5 (version strings at 0x1011de89/0x1011df05), compiled from the unmodified
// release source with the default configuration; vendored in vendor/libpng-1.0.5 (libpng license).
#include "vendor/libpng-1.0.5/pngrutil.c"
// MATCH: jgld.dll 0x100793b0 _png_get_int_32   // 2 names compile identically: name not determined
// MATCH: jgld.dll 0x10079410 _png_get_uint_32
// MATCH: jgld.dll 0x10079470 _png_get_uint_16
// MATCH: jgld.dll 0x10079510 _png_crc_finish
// MATCH: jgld.dll 0x10079620 _png_crc_error
// MATCH: jgld.dll 0x100796e0 _png_handle_IHDR
// MATCH: jgld.dll 0x100799e0 _png_handle_PLTE
// MATCH: jgld.dll 0x10079c20 _png_handle_IEND
// MATCH: jgld.dll 0x10079cc0 _png_handle_gAMA
// MATCH: jgld.dll 0x10079ec0 _png_handle_sBIT
// MATCH: jgld.dll 0x1007a0d0 _png_handle_cHRM
// MATCH: jgld.dll 0x1007a7d0 _png_handle_sRGB
// MATCH: jgld.dll 0x1007ab40 _png_handle_tRNS
// MATCH: jgld.dll 0x1007ae70 _png_handle_bKGD
// MATCH: jgld.dll 0x1007b1a0 _png_handle_hIST
// MATCH: jgld.dll 0x1007b380 _png_handle_pHYs
// MATCH: jgld.dll 0x1007b4d0 _png_handle_oFFs
// MATCH: jgld.dll 0x1007b620 _png_handle_pCAL
// MATCH: jgld.dll 0x1007b9b0 _png_handle_tIME
// MATCH: jgld.dll 0x1007baf0 _png_handle_tEXt
// MATCH: jgld.dll 0x1007bc40 _png_handle_zTXt
// MATCH: jgld.dll 0x1007c150 _png_handle_unknown
// MATCH: jgld.dll 0x1007c310 _png_combine_row
// MATCH: jgld.dll 0x1007c810 _png_do_read_interlace
// MATCH: jgld.dll 0x1007ce80 _png_read_filter_row
// MATCH: jgld.dll 0x1007d280 _png_read_finish_row
// MATCH: jgld.dll 0x1007d6b0 _png_read_start_row
// MATCH: jgld.dll 0x100794b0 _png_crc_read
// MATCH: jgld.dll 0x1007c1f0 _png_check_chunk_name
