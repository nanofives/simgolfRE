// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jdatadst.c"
// MATCH: golf_clean.exe 0x004ae4d0 _jpeg_stdio_dest
// MATCH: golf_clean.exe 0x004ae590 _term_destination
