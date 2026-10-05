// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links zlib 1.0.2 ("inflate 1.0.2 Copyright 1995-1996 Mark Adler" at file offset 0x12728c), compiled
// from the unmodified release source; vendored in vendor/zlib-1.0.2 (zlib license).
#include "vendor/zlib-1.0.2/infcodes.c"
// MATCH: jgld.dll 0x1009f2f0 _inflate_codes_new
// MATCH: jgld.dll 0x100a00e0 _inflate_codes_free
// MATCH: jgld.dll 0x1009f370 _inflate_codes
