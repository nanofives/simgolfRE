// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links libpng 1.0.5 (version strings at 0x1011de89/0x1011df05), compiled from the unmodified
// release source with the default configuration; vendored in vendor/libpng-1.0.5 (libpng license).
#include "vendor/libpng-1.0.5/pngset.c"
// MATCH: jgld.dll 0x1007dad0 _png_set_bKGD
// MATCH: jgld.dll 0x1007db30 _png_set_cHRM
// MATCH: jgld.dll 0x1007dbd0 _png_set_gAMA
// MATCH: jgld.dll 0x1007dc20 _png_set_hIST
// MATCH: jgld.dll 0x1007dc70 _png_set_IHDR
// MATCH: jgld.dll 0x1007ddb0 _png_set_oFFs
// MATCH: jgld.dll 0x1007de10 _png_set_pCAL
// MATCH: jgld.dll 0x1007dfc0 _png_set_pHYs
// MATCH: jgld.dll 0x1007e020 _png_set_PLTE
// MATCH: jgld.dll 0x1007e070 _png_set_sBIT
// MATCH: jgld.dll 0x1007e0d0 _png_set_sRGB
// MATCH: jgld.dll 0x1007e120 _png_set_sRGB_gAMA_and_cHRM
// MATCH: jgld.dll 0x1007e220 _png_set_text
// MATCH: jgld.dll 0x1007e400 _png_set_tIME
// MATCH: jgld.dll 0x1007e470 _png_set_tRNS
// MATCH: jgld.dll 0x1007e500 _png_permit_empty_plte
