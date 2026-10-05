// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jmemmgr.c"
// MATCH: golf_clean.exe 0x004aee30 _jinit_memory_mgr
// MATCH: golf_clean.exe 0x004aef70 _alloc_small
// MATCH: golf_clean.exe 0x004af0a0 _out_of_memory
// MATCH: golf_clean.exe 0x004af0c0 _alloc_large
// MATCH: golf_clean.exe 0x004af170 _alloc_sarray
// MATCH: golf_clean.exe 0x004af220 _alloc_barray
// MATCH: golf_clean.exe 0x004af2d0 _request_virt_sarray
// MATCH: golf_clean.exe 0x004af340 _request_virt_barray
// MATCH: golf_clean.exe 0x004af3b0 _realize_virt_arrays
// MATCH: golf_clean.exe 0x004af690 _do_sarray_io
// MATCH: golf_clean.exe 0x004af880 _do_barray_io
// MATCH: golf_clean.exe 0x004af920 _free_pool
