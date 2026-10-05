// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links zlib 1.0.2 ("inflate 1.0.2 Copyright 1995-1996 Mark Adler" at file offset 0x12728c), compiled
// from the unmodified release source; vendored in vendor/zlib-1.0.2 (zlib license).
#include "vendor/zlib-1.0.2/crc32.c"
// MATCH: jgld.dll 0x1009ca40 _get_crc_table
// MATCH: jgld.dll 0x1009ca70 _crc32
