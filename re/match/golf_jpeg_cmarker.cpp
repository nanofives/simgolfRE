// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jcmarker.c"
// MATCH: golf_clean.exe 0x004afaf0 _jinit_marker_writer
// MATCH: golf_clean.exe 0x004afb60 _write_any_marker
// MATCH: golf_clean.exe 0x004afbb0 _emit_byte
// MATCH: golf_clean.exe 0x004afbf0 _emit_marker
// MATCH: golf_clean.exe 0x004afc10 _emit_2bytes
// MATCH: golf_clean.exe 0x004afc40 _write_file_header
// MATCH: golf_clean.exe 0x004afc80 _emit_jfif_app0
// MATCH: golf_clean.exe 0x004afd20 _emit_adobe_app14
// MATCH: golf_clean.exe 0x004afdc0 _write_frame_header
// MATCH: golf_clean.exe 0x004afec0 _emit_dqt
// MATCH: golf_clean.exe 0x004affa0 _emit_sof
// MATCH: golf_clean.exe 0x004b0120 _emit_dht
// MATCH: golf_clean.exe 0x004b01f0 _emit_dri
// MATCH: golf_clean.exe 0x004b0220 _emit_sos
// MATCH: golf_clean.exe 0x004b0320 _write_tables_only
