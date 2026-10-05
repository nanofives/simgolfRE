// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jccoefct.c"
// MATCH: golf_clean.exe 0x004b06f0 _jinit_c_coef_controller
// MATCH: golf_clean.exe 0x004b0860 _start_iMCU_row
// MATCH: golf_clean.exe 0x004b0d70 _compress_output
