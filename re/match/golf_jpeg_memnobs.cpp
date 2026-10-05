// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jmemnobs.c"
// MATCH: golf_clean.exe 0x004b0460 _jpeg_get_large   // 2 names compile identically: name not determined
// MATCH: golf_clean.exe 0x004b0470 _jpeg_free_large   // 2 names compile identically: name not determined
// MATCH: golf_clean.exe 0x004b0480 _jpeg_mem_available
// MATCH: golf_clean.exe 0x004b0490 _jpeg_open_backing_store
// MATCH: golf_clean.exe 0x004b04b0 _jpeg_mem_init   // `return 0` function: the linker folded identical functions (/OPT:ICF), name not determined
