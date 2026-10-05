// FLAGS golf_clean.exe: /O2
// LANG c
// golf_clean.exe links IJG libjpeg 6a ("6a  7-Feb-96" in original\JPEG.lib), compiled from the unmodified
// release source (vendor/jpeg-6a, IJG license) with the jconfig.h written there.
#include "vendor/jpeg-6a/jcmaster.c"
// MATCH: golf_clean.exe 0x004b51e0 _jinit_c_master_control
// MATCH: golf_clean.exe 0x004b52a0 _initial_setup
// MATCH: golf_clean.exe 0x004b5470 _validate_script
// MATCH: golf_clean.exe 0x004b5810 _prepare_for_pass
// MATCH: golf_clean.exe 0x004b59d0 _select_scan_parameters
// MATCH: golf_clean.exe 0x004b5ad0 _per_scan_setup
// MATCH: golf_clean.exe 0x004b5ca0 _pass_startup
