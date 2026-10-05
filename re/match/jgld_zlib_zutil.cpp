// FLAGS jgld.dll: /Od /ZI /GZ
// LANG c
// jgld.dll links zlib 1.0.2 ("inflate 1.0.2 Copyright 1995-1996 Mark Adler" at file offset 0x12728c), compiled
// from the unmodified release source; vendored in vendor/zlib-1.0.2 (zlib license).
#include "vendor/zlib-1.0.2/zutil.c"
// MATCH: jgld.dll 0x1009e630 _z_error
// MATCH: jgld.dll 0x1009e680 _zcalloc
// MATCH: jgld.dll 0x1009e6d0 _zcfree
