// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jccolor.c"
// MATCH: golf_clean.exe 0x004b01e0 _null_method   // empty function (`ret`): the linker folded identical functions (/OPT:ICF), name not determined
// MATCH: golf_clean.exe 0x004b4a60 _jinit_color_converter
// MATCH: golf_clean.exe 0x004b4eb0 _rgb_gray_convert
// MATCH: golf_clean.exe 0x004b50d0 _grayscale_convert
