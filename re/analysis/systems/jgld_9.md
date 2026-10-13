# jgld_9: round 33 CRT callee names (2026-10-13)

The debug static CRT functions jgld.dll's Surface::writePcx8, Surface::saveImage, loadPng and createDisplay call, named
by batch c3ff (log/c3/c3ff_names.tsv, evidence per row) from their FID names in re/functions_ghidra_jgld.dll.tsv:
`__chkesp` 0x1007e780, `operator_new` 0x1007f0f0, `memcpy` 0x1007f3a0, `strlen` 0x1007f6e0, `strcat` 0x1007f770,
`fclose` 0x1007f850, `fopen` 0x1007faa0, `_setjmp3` 0x1007fab8. They had no hooks.csv row, which the C3 gate needs
for every callee.
