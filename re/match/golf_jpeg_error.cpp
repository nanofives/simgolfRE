// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jerror.c"
// MATCH: golf_clean.exe 0x004aec80 _jpeg_std_error
// MATCH: golf_clean.exe 0x004aecd0 _error_exit
// MATCH: golf_clean.exe 0x004aecf0 _output_message
