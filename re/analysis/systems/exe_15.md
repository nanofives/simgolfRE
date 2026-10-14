# exe_15: round 34 callee names (2026-10-14)

golf_clean.exe functions named by batches c3fh, c3fi and c3fm (log/c3/<batch>_names.tsv, evidence per row) so that
their callers can pass the C3 gate. All but one are static CRT functions identified by FID full matches against the
VC6 12.00.8168 libraries, with the call forms checked at the call sites cited.

- `__ftol` 0x004a6030 and `_malloc` 0x004a504f: the callees of allocResampleTable 0x00481870 (c3fh).
- `_free`, `ehVectorConstructorIterator` (FID `??_L@YGXPAXIHP6EX0@Z1@Z`, the same name as sound.dll's copy),
  `operator_new`, `_rewind`, `_fseek`, `__strcmpi`, `__strnicmp`: callees of netSendShutdown, Flic::createStream and
  Reader::open (c3fi).
- `emptyFunc_4a0890`: a one-byte `ret` with nine callers (c3fi); a `trivial` end-state candidate.
- `_strchr` 0x004a6170 and `_xtoa` 0x004ad452: hooked by c3fm, so they need a row before promotion. `_strstr`
  enters `_strchr` six bytes past its entry (0x004a6176).
- `__fsopen`, `__fclose_lk`, `__lock_file`, `__unlock_file`, `__filbuf`, `__getptd`: callees of the exe's file and
  string CRT that c3fm hooks; each takes locks, uses TLS, the heap or the file system and none is hooked.
