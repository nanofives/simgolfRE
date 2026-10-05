// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jcparam.c"
// MATCH: golf_clean.exe 0x004ae600 _jpeg_add_quant_table
// MATCH: golf_clean.exe 0x004ae6c0 _jpeg_set_linear_quality
// MATCH: golf_clean.exe 0x004ae700 _jpeg_quality_scaling
// MATCH: golf_clean.exe 0x004ae740 _jpeg_set_quality
// MATCH: golf_clean.exe 0x004ae760 _jpeg_set_defaults
// MATCH: golf_clean.exe 0x004ae850 _std_huff_tables
// MATCH: golf_clean.exe 0x004ae8b0 _add_huff_table
// MATCH: golf_clean.exe 0x004ae910 _jpeg_default_colorspace
// MATCH: golf_clean.exe 0x004ae990 _jpeg_set_colorspace
