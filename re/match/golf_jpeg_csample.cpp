// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jcsample.c"
// MATCH: golf_clean.exe 0x004b4040 _jinit_downsampler
// MATCH: golf_clean.exe 0x004b4390 _expand_right_edge
// MATCH: golf_clean.exe 0x004b43f0 _fullsize_downsample
