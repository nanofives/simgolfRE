// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jcapimin.c"
// MATCH: golf_clean.exe 0x004ae150 _jpeg_CreateCompress
// MATCH: golf_clean.exe 0x004ae210 _jpeg_abort_compress   // 2 names compile identically: name not determined
// MATCH: golf_clean.exe 0x004ae220 _jpeg_suppress_tables
// MATCH: golf_clean.exe 0x004ae270 _jpeg_finish_compress
