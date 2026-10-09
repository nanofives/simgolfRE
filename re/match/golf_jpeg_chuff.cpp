// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jchuff.c"
// MATCH: golf_clean.exe 0x004b0f60 _jpeg_make_c_derived_tbl
// MATCH: golf_clean.exe 0x004b1080 _jpeg_gen_optimal_table
// MATCH: golf_clean.exe 0x004b12b0 _jinit_huff_encoder
// MATCH: golf_clean.exe 0x004b15c0 _encode_one_block
// MATCH: golf_clean.exe 0x004b19e0 _dump_buffer
// MATCH: golf_clean.exe 0x004b1a10 _emit_bits
// MATCH: golf_clean.exe 0x004b1ae0 _emit_restart
// MATCH: golf_clean.exe 0x004b1b70 _flush_bits
// MATCH: golf_clean.exe 0x004b1d80 _htest_one_block
// MATCH: golf_clean.exe 0x004b12f0 _start_pass_huff
// MATCH: golf_clean.exe 0x004b1480 _encode_mcu_huff
// MATCH: golf_clean.exe 0x004b1ca0 _encode_mcu_gather
// MATCH: golf_clean.exe 0x004b1e20 _finish_pass_gather
