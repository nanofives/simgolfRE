// Coverage build of batch c3fp (never in shim/re_batches.txt; built only on instances\c3fp through
// log/c3/c3fp_cov_batches.txt): shim/src/re/c3fp.cpp with C3FP_COV, whose J(va, c) counts, per jcc VA, how often the jump
// is taken (word 1) and not taken (word 2) in the exported C3fpCovTable {va, taken, not taken} x 256, read by Frida
// (log/c3/alt/c3fp_cov.py). Nothing is written to a file from the DLL.
#define C3FP_COV 1
#include "c3fp.cpp"
