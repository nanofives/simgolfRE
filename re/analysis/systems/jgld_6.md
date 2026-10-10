# jgld_6: Display's constructor callees and the CRT strcpy entry (2026-10-10)

Named for round 26 (batch c3dk). Display::Display 0x100654b0 first calls `DisplayBase::ctor` 0x10065630 (stores the
DisplayBase vtable 0x1011d7dc, then Display::Display overwrites it with the Display vtable 0x1011d640 at 0x100654ff), then
`DisplayMember::ctor` 0x10069270 on this+4, which writes nothing inside the object and only clears g_commandHandler
0x10128728, the callback onCommand 0x1006a0e0 tests. Both match 100% (re/match/jgld_raw_03.cpp, jgld_raw_04.cpp).

`strcpy` 0x1007f760 is the CRT strcat.asm entry pair: it loads the destination into edi and jumps into _strcat's copy loop
(0x1007f7d1) past the end-of-destination scan. Its callers are Font::create 0x10068850, Font::loadFile 0x10068ae0 and
__CrtDbgReport 0x10081f90.
