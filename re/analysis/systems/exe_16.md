# exe_16: round 35 callee names (2026-10-14)

golf_clean.exe functions named by batches c3fo and c3fu (log/c3/<batch>_names.tsv, evidence per row) so their
callers and callees pass the C3 gate.

- `ehVectorCtor_finally` 0x004a563c and `__ArrayUnwind` 0x004a5793: the `__finally` funclet of
  ehVectorConstructorIterator 0x004a55d2 (the only scope of its SEH frame, also called on the normal path) and the
  FID-matched routine it calls, which destroys the elements already built when a constructor raised.
- `EditBox::onKey` 0x004867b0: created in the master this round (it was code outside every function). Slot 36 of the
  EditBox vtable 0x004ba4a8; a switch over the Win32 virtual keys VK_BACK, VK_END, VK_HOME, VK_LEFT, VK_UP, VK_RIGHT,
  VK_DOWN and VK_DELETE. It is the only caller of EditBox::charPosToPixel 0x00486360.
