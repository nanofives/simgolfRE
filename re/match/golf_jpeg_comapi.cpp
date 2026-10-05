// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jcomapi.c"
// MATCH: golf_clean.exe 0x004afa60 _jpeg_abort
// MATCH: golf_clean.exe 0x004afa90 _jpeg_destroy
// MATCH: golf_clean.exe 0x004afab0 _jpeg_alloc_quant_table
// MATCH: golf_clean.exe 0x004afad0 _jpeg_alloc_huff_table
