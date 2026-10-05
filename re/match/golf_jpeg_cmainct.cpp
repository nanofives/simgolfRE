// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jcmainct.c"
// MATCH: golf_clean.exe 0x004b0560 _jinit_c_main_controller
// MATCH: golf_clean.exe 0x004b0640 _process_data_simple_main
