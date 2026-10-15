// Negative-control build of batch c3fp (never in shim/re_batches.txt; built only on instances\c3fp through
// log/c3/c3fp_negctl_batches.txt). The body is shim/src/re/c3fp.cpp compiled with C3FP_NEGCTL: wrong-body variant k
// (environment C3FP_NEGVAR, default 1, echoed in word 2 of C3fpNegCtl) is dormant until a fixture arms word 0 through
// __freeze.negOn() after the freeze. Variants (each one detail, see the NEG(k) sites in c3fp.cpp):
//   1  the normal copy step does not advance the read cursor (0x100369ed)
//   2  after a hand-over copy the read cursor advances by the bytes copied instead of the segment bytes left (0x100368f5)
//   3  the fade's step capture sets bit 0 of ch+0x5c instead of bit 1 (0x10036c2b)
//   4  the unchained notice compares against obj+0x14 / 2 instead of / 4 (0x10036f37)
//   5  the hand-over leaves bit 1 of the sixteen record bytes (0x10036ed0)
//   6  the fade's last subtraction is a 53-bit double operation instead of the x87 one (0x10036c48 / 0x10036c4e)
#define C3FP_NEGCTL 1
#include "c3fp.cpp"
