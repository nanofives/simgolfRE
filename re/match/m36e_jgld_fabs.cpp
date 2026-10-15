// FLAGS jgld.dll: /Od
// LANG c
// jgld.dll static CRT fabs (VC6 12.00.8168 libcmtd, re/fid/jgld.dll.fid.tsv), round 36 batch m36e, written from the
// instructions in the shape of the VC6 CRT's transcendental sources: the control word is saved with
// _ctrlfp(0x133f, 0xffff) (push 0xffff / push 0x133f at 0x10082966) and restored with _ctrlfp(saved, 0xffff) on every
// return; an exponent of 0x7ff0 (0x10082983) dispatches on _sptype 0x1008c120 (1 = +inf, 2 = -inf, 3 = quiet NaN,
// else signalling NaN); operation code 0x15 (`push 0x15` at 0x100829f4) and flag 8 (`push 8` at 0x10082a1d) go to
// _handle_qnan1 0x1008b3e0 and _except1 0x1008b4b0; the signalling NaN is quietened by adding 1.0 (the double at
// 0x1011d030 reads 1.0); the finite result clears bit 31 of the high dword (0x10082a2c).
unsigned int __cdecl _ctrlfp(unsigned int newctrl, unsigned int mask);                  /* 0x1008c320 */
int __cdecl _sptype(double x);                                                         /* 0x1008c120 */
double __cdecl _handle_qnan1(unsigned int op, double arg, unsigned int cw);            /* 0x1008b3e0 */
double __cdecl _except1(int flags, int opcode, double arg, double res, unsigned int cw); /* 0x1008b4b0 */

#define D_LO(x) ((unsigned int *)&(x))
#define D_HI(x) ((unsigned int *)&(x) + 1)
#define D_EXP(x) ((unsigned short *)&(x) + 3)
#define IS_D_SPECIAL(x) ((*D_EXP(x) & 0x7ff0) == 0x7ff0)
#define _maskfp() _ctrlfp(0x133f, 0xffff)
#define _rstorfp(cw) _ctrlfp(cw, 0xffff)
#define RETURN(fpcw, result) return _rstorfp(fpcw), (result)

// MATCH: jgld.dll 0x10082960 _fabs
double __cdecl fabs(double x)
{
    unsigned int savedcw;
    double result;

    savedcw = _maskfp();

    if (IS_D_SPECIAL(x)) {
        switch (_sptype(x)) {
        case 1:
            RETURN(savedcw, x);
        case 2:
            RETURN(savedcw, -x);
        case 3:
            return _handle_qnan1(0x15, x, savedcw);
        default:
            return _except1(8, 0x15, x, x + 1.0, savedcw);
        }
    }

    *D_HI(result) = *D_HI(x) & ~(1 << 31);
    *D_LO(result) = *D_LO(x);
    RETURN(savedcw, result);
}
