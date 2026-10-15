// FLAGS golf_clean.exe: /O1
// LANG c
// m36d: two routines of golf_clean.exe's static VC6 multithreaded CRT, written in the shape of the VC6 CRT sources
// fseek.c and rewind.c; /O1 (frame pointer kept in fseek, push-memory operands) reproduces both exactly. Callees by
// address: _lock_file 0x004a9874, _fseek_lk 0x004a7145, _unlock_file 0x004a98c6, _flush 0x004a99a0, _lseek 0x004a95bb;
// the ioinfo table __pioinfo 0x00842040 (0x24-byte entries, 32 per block) and __badioinfo 0x004e4d60.
#include <stdio.h>
void __cdecl _lock_file(FILE*);
void __cdecl _unlock_file(FILE*);
int __cdecl _fseek_lk(FILE*, long, int);
int __cdecl _flush(FILE*);
long __cdecl _lseek(int, long, int);
typedef struct { long osfhnd; char osfile; char pipech; int lockinitflag; char pad[0x18]; } ioinfo;
extern ioinfo* __pioinfo[];
extern ioinfo __badioinfo;

// MATCH: golf_clean.exe 0x004a7119 _fseek
int __cdecl fseek(FILE* stream, long offset, int whence)
{
    int retval;
    _lock_file(stream);
    retval = _fseek_lk(stream, offset, whence);
    _unlock_file(stream);
    return retval;
}

// MATCH: golf_clean.exe 0x004a70b5 _rewind
void __cdecl rewind(FILE* str)
{
    FILE* stream = str;
    int fd = stream->_file;
    _lock_file(stream);
    _flush(stream);
    stream->_flag &= ~(_IOERR | _IOEOF);
    ((fd != -1) ? (__pioinfo[fd >> 5] + (fd & 31)) : &__badioinfo)->osfile &= ~0x02;   /* ~FEOFLAG */
    if (stream->_flag & _IORW)
        stream->_flag &= ~(_IOREAD | _IOWRT);
    _lseek(fd, 0L, 0);
    _unlock_file(stream);
}
