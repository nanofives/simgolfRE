// FLAGS jgld.dll: /Od
// LANG c
// jgld.dll static CRT (VC6 12.00.8168 libcmtd, identified by re/fid/jgld.dll.fid.tsv), round 36 batch m36e: the C-source
// stdio entries fclose and fopen. The CRT objects carry no /GZ stack fill (re/match/skip.tsv's reason for the CRT
// fragments), so the flags are /Od only. Written from the instructions in the shape of the VC6 CRT sources:
// fclose's _ASSERTE(stream != NULL) reports file/line 0x3a (`push 0x3a` at 0x1007f86b; #line puts it there),
// and its `int 3` is crtdbg.h's inline _CrtDbgBreak; the `xor eax, eax; test eax, eax; jne` at 0x1007f882 is the
// macro's do { } while (0). _IOSTRG = 0x40 (`and edx, 0x40` at 0x1007f88e); _SH_DENYNO = 0x40 (`push 0x40` at 0x1007faa3).
#define _MT
#define _DEBUG
#include <stdio.h>
#include <crtdbg.h>
int __cdecl _fclose_lk(FILE *stream);                                   /* 0x1007f8e0 */
void __cdecl _lock_file(FILE *stream);                                  /* 0x10083230 */
void __cdecl _unlock_file(FILE *stream);                                /* 0x100832a0 */
FILE * __cdecl _fsopen(const char *file, const char *mode, int shflag); /* 0x1007f9a0 */

// MATCH: jgld.dll 0x1007f850 _fclose
int __cdecl fclose(FILE *stream)
{
        int result = EOF;

#line 58
        _ASSERTE(stream != NULL);

        if (stream->_flag & _IOSTRG) {
                stream->_flag = 0;
        }
        else {
                _lock_file(stream);
                result = _fclose_lk(stream);
                _unlock_file(stream);
        }

        return(result);
}

// MATCH: jgld.dll 0x1007faa0 _fopen
FILE * __cdecl fopen(const char *file, const char *mode)
{
        return( _fsopen(file, mode, 0x40) );
}
