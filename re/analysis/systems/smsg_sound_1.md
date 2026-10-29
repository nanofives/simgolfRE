# smsg_sound_1: sound.dll names from SMSG-decomp's PDB, checked against our disassembly (S10.9, 2026-10-29)

Source of the names: third_party/smsg/release/sound.pdb, read by log/roadmap/S10/pdb/pdbread.py into
log/roadmap/S10/pdb/sound.procs.tsv. They are another project's names. A row is here only when OUR binary
(original/sound.dll, capstone) supports the name. Each row's evidence column says which rule held, and the purpose column
states only what the instructions do. The noun in a name (`get_next`, `set_listener_ypos`) comes from SMSG. Our check
covers the verb and the class.

## Candidates
Every sound.dll function SMSG names where the Ghidra master still has `FUN_` and no names file has a row: 561
(log/roadmap/S10/9_cand_sound.tsv, `log/roadmap/S10/9_tools/measure.py`). 143 of them are not functions in our master
(dead code behind unreferenced incremental-link thunks, b1): deferred, not named.

## Rules (log/roadmap/S10/9_tools/verdict.py, shape.py)
- Class pin: the function sits in a vtable slot that some code installs (pointer_callers.references, installer cited),
  or a named function calls it.
- ACCEPT_GET: a `get_` / `is_` / `has_` name and a body that only reads (no store, no call).
- ACCEPT_SET: a `set_` / `reset_` / `clear_` / `enable` ... name and a body that stores its arguments or constants into
  this, with no call.
- ACCEPT_FWD: the body passes the call to slot s of an object it loads from this, and slot s of another vtable holds a
  body (not a forwarder) that SMSG gives the same method name (vtslots.py).
- ACCEPT_VT: a constructor / destructor / deleting destructor that stores the vtable SMSG's public names for that class,
  or the deleting-destructor shape (bit 0 of the flag argument around the destructor call).
- Hand verdicts (log/roadmap/S10/9_manual_sound*.tsv): imports, our named callees, the command ids a body queues and
  the handlers we already named, allocation sizes, sibling slots. `$E` initializers and exit destructors take our
  `dynInit_` / `atexitDtor_` convention instead of the compiler's `$E` numbers.
- Rejected: constant or empty stubs (no instruction supports the verb), forwarders whose target slot has no
  same-named body, bodies over about 60 bytes with no import or named callee supporting the verb (not reviewed in
  depth), and functions with no caller and no stored pointer.

A name SMSG gives to two slots of one class carries its address (`Midi::load_1002df70`), and so does a name already
taken in our files or the master.

## Counts
Ledger: log/roadmap/S10/9_ledger_sound.tsv (one row per candidate, with the verdict and the reason for each rejection).
