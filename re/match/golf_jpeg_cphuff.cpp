// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jcphuff.c"
// MATCH: golf_clean.exe 0x004b1f30 _jinit_phuff_encoder
// MATCH: golf_clean.exe 0x004b2420 _dump_buffer
// MATCH: golf_clean.exe 0x004b2460 _emit_restart
// MATCH: golf_clean.exe 0x004b2510 _flush_bits
// MATCH: golf_clean.exe 0x004b25d0 _emit_eobrun
// MATCH: golf_clean.exe 0x004b27d0 _emit_buffered_bits
