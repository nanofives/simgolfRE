// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links libpng 1.0.5 (version strings at 0x1011de89/0x1011df05), compiled from the unmodified
// release source with the default configuration; vendored in vendor/libpng-1.0.5 (libpng license).
#include "vendor/libpng-1.0.5/pngget.c"
// MATCH: jgld.dll 0x1006e4c0 _png_get_x_pixels_per_meter
// MATCH: jgld.dll 0x1006e520 _png_get_y_pixels_per_meter
// MATCH: jgld.dll 0x1006e580 _png_get_pixels_per_meter
// MATCH: jgld.dll 0x1006e5f0 _png_get_pixel_aspect_ratio
// MATCH: jgld.dll 0x1006e670 _png_get_x_offset_microns
// MATCH: jgld.dll 0x1006e6d0 _png_get_y_offset_microns
// MATCH: jgld.dll 0x1006e730 _png_get_x_offset_pixels
// MATCH: jgld.dll 0x1006e790 _png_get_y_offset_pixels
// MATCH: jgld.dll 0x1006eae0 _png_get_IHDR
// MATCH: jgld.dll 0x1006edb0 _png_get_pHYs
// MATCH: jgld.dll 0x1006ef10 _png_get_text
// MATCH: jgld.dll 0x1006efd0 _png_get_tRNS
// MATCH: jgld.dll 0x1006e280 _png_get_valid
// MATCH: jgld.dll 0x1006e2c0 _png_get_rowbytes
// MATCH: jgld.dll 0x1006e300 _png_get_image_width
// MATCH: jgld.dll 0x1006e340 _png_get_image_height
// MATCH: jgld.dll 0x1006e380 _png_get_bit_depth
// MATCH: jgld.dll 0x1006e3c0 _png_get_color_type
// MATCH: jgld.dll 0x1006e400 _png_get_filter_type
// MATCH: jgld.dll 0x1006e440 _png_get_interlace_type
// MATCH: jgld.dll 0x1006e480 _png_get_compression_type
// MATCH: jgld.dll 0x1006e7f0 _png_get_channels
// MATCH: jgld.dll 0x1006e830 _png_get_signature
// MATCH: jgld.dll 0x1006e870 _png_get_bKGD
// MATCH: jgld.dll 0x1006e8d0 _png_get_cHRM
// MATCH: jgld.dll 0x1006e9c0 _png_get_gAMA
// MATCH: jgld.dll 0x1006ea20 _png_get_sRGB
// MATCH: jgld.dll 0x1006ea80 _png_get_hIST
// MATCH: jgld.dll 0x1006ec40 _png_get_oFFs
// MATCH: jgld.dll 0x1006ecc0 _png_get_pCAL
// MATCH: jgld.dll 0x1006ee50 _png_get_PLTE
// MATCH: jgld.dll 0x1006eeb0 _png_get_sBIT
// MATCH: jgld.dll 0x1006ef70 _png_get_tIME
// MATCH: jgld.dll 0x1006f0a0 _png_get_rgb_to_gray_status
