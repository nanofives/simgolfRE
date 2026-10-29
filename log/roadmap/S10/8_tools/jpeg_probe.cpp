// FLAGS golf_clean.exe: /O2
// LANG c
// S10.8 scratch probe (never a canonical source): shows the differences of the two libjpeg statics below 100%.
#include "C:/Users/maria/Desktop/Proyectos/SimGolf/re/match/vendor/jpeg-6a/jccolor.c"
#include "C:/Users/maria/Desktop/Proyectos/SimGolf/re/match/vendor/jpeg-6a/jfdctint.c"
// MATCH: golf_clean.exe 0x004b4ca0 _rgb_ycc_start
// MATCH: golf_clean.exe 0x004b5d50 _jpeg_fdct_islow
