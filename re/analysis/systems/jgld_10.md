# jgld_10: round 34 callee names (2026-10-14)

jgld.dll debug CRT functions named by batches c3fh and c3fm (log/c3/<batch>_names.tsv, evidence per row).

- `__ftol` 0x1007f12c, `fabs` 0x10082960, `pow` 0x10082a79: the CRT callees of png_init_read_transformations
  0x100702a0 (c3fh), at the call sites libpng 1.0.5's pngrtran.c writes as fabs / pow.
- `abs` 0x1007f370: hooked by c3fm (72 callers, among them the C3 blitters and Surface::dashedLine16), so it needs a
  row before promotion.
- `__fsopen`, `__nh_malloc`, `_CrtDbgReport`, `__fclose_lk`, `__lock_file`, `__unlock_file`: callees of the jgld CRT
  entries c3fm hooks (fopen, fclose, operator_new). None is hooked. `_CrtDbgReport` is the debug report that
  `__chkesp` 0x1007e780 and a NULL-stream fclose reach.
