// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jutils.c"
// MATCH: golf_clean.exe 0x004b04c0 _jdiv_round_up
// MATCH: golf_clean.exe 0x004b04d0 _jround_up
// MATCH: golf_clean.exe 0x004b04f0 _jcopy_sample_rows
// MATCH: golf_clean.exe 0x004b0540 _jzero_far
