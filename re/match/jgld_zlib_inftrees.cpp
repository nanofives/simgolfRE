// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links zlib 1.0.2 ("inflate 1.0.2 Copyright 1995-1996 Mark Adler" at file offset 0x12728c), compiled
// from the unmodified release source; vendored in vendor/zlib-1.0.2 (zlib license).
#include "vendor/zlib-1.0.2/inftrees.c"
// MATCH: jgld.dll 0x1009e710 _inflate_trees_bits
// MATCH: jgld.dll 0x1009e7a0 _huft_build
// MATCH: jgld.dll 0x1009ef00 _inflate_trees_dynamic
// MATCH: jgld.dll 0x1009f030 _inflate_trees_fixed
// MATCH: jgld.dll 0x1009f210 _falloc
// MATCH: jgld.dll 0x1009f250 _inflate_trees_free
