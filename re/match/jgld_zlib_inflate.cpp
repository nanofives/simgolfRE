// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links zlib 1.0.2 ("inflate 1.0.2 Copyright 1995-1996 Mark Adler" at file offset 0x12728c), compiled
// from the unmodified release source; vendored in vendor/zlib-1.0.2 (zlib license).
#include "vendor/zlib-1.0.2/inflate.c"
// MATCH: jgld.dll 0x1009bdd0 _inflateReset
// MATCH: jgld.dll 0x1009be70 _inflateEnd
// MATCH: jgld.dll 0x1009bf10 _inflateInit2_
// MATCH: jgld.dll 0x1009c0b0 _inflateInit_
// MATCH: jgld.dll 0x1009c7e0 _inflateSetDictionary
// MATCH: jgld.dll 0x1009c8c0 _inflateSync
// MATCH: jgld.dll 0x1009c0f0 _inflate
