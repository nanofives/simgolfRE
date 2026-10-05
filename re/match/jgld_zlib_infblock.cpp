// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links zlib 1.0.2 ("inflate 1.0.2 Copyright 1995-1996 Mark Adler" at file offset 0x12728c), compiled
// from the unmodified release source; vendored in vendor/zlib-1.0.2 (zlib license).
#include "vendor/zlib-1.0.2/infblock.c"
// MATCH: jgld.dll 0x1009cc70 _inflate_blocks_reset
// MATCH: jgld.dll 0x1009cd90 _inflate_blocks_new
// MATCH: jgld.dll 0x1009e290 _inflate_blocks_free
// MATCH: jgld.dll 0x1009e310 _inflate_set_dictionary
// MATCH: jgld.dll 0x1009ce80 _inflate_blocks
