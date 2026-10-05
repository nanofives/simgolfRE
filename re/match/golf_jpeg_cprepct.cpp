// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jcprepct.c"
// MATCH: golf_clean.exe 0x004b3a80 _jinit_c_prep_controller
// MATCH: golf_clean.exe 0x004b3d00 _expand_bottom_edge
// MATCH: golf_clean.exe 0x004b3f10 _create_context_buffer
