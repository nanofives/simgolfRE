# Phase 1 stream H brief: name the exe game-code FUN_ functions (written 2026-10-18)

Stream H turns the 281 neutral-named game-code functions of golf_clean.exe (0x401000-0x4a4ecf; 275 reachable, 104 of 16 bytes or
less) into evidence-backed names, using the finished data model. Four children, one chunk each: `h1` .. `h4`, rows in
re/model/h_candidates.tsv (column `chunk`). You own only your chunk's addresses.

## Read first
CLAUDE.md (phase 1, b1, names paragraphs), re/model/STREAM_BRIEF.md (rules and decisions apply here too: NO-GUESSING, no original
text copied, `~/bin/py -3.12`, you cannot write the git index), re/model/DECISIONS.md, re/analysis/data/model_integration.md,
re/model/addresses.tsv (symbol, va, size, type, evidence for every data object), re/model/include/simgolf_model.h (struct layouts),
re/analysis/data/golfer_record.md and record_tables.md, re/model/e1_vtables.tsv and e2_vtables.tsv (vtable slot maps: a function
that sits in a slot of a named class is `Class::method`), re/model/ownership.tsv, re/names/*.tsv (the vocabulary and conventions of
existing names: camelCase functions, `Class::method`, `_vtordisp` and `_adj274` suffixes for this-adjustor thunks, subsystem tags
sim golfer economy course terrain ui frontend render video audio net util crt boot save ai), re/analysis/data/model_defects.md.

## Method per function
1. Disassemble / decompile (`~/bin/py -3.12 re/tools/decomp.py 0x<addr>`; `re/tools/xref.py golf_clean.exe 0x<addr>` for callers,
   callees and strings).
2. Find what it touches in the MODEL: every data address it reads or writes -> its symbol in addresses.tsv and the field in the
   struct; its callers' names and subsystem; its callees. A function that sets golfer field X, calls an economy function and is
   called from the hole-end screen is named by that, not by its size.
3. Name only with evidence, citing instruction addresses. If the evidence stops, KEEP the FUN_ name and list the function as
   DEFERRED with the reason (this is a normal outcome for thunks and fragments). Never invent a purpose.
4. Tiny functions (<= 16 bytes): name the clear patterns (return constant, accessor of a model field, tail jump to a named
   callee: `X_fwd`, this-adjustor thunks by their existing suffix convention) and defer the rest.
5. Names must be unique across ALL re/names files for the module (names_validate checks). Purposes are one sentence of what the
   function does, in plain words, no quoted binary text (names_sanitize rewrites quotes).

## Deliverables (replace K with your chunk number)
- `re/names/model_hK.tsv`: `module addr name subsystem evidence purpose` (module is golf_clean.exe, addr 0x........),
  one row per NAMED function. Run `~/bin/py -3.12 re/tools/names_sanitize.py`, `names_validate.py model_hK`,
  `names_check_evidence.py`.
- `re/analysis/systems/model_hK.md`: the stub the validator wants (one paragraph pointing at the notes file).
- `re/model/hK_notes.md`: a table of every function of the chunk: address, old name, new name or DEFERRED, subsystem, reach,
  evidence in one line, and the group it belongs to (the roles you found). Counts at the end: named, deferred (with reasons
  tallied), defects found (instruction addresses; do not fix), model corrections you noticed (a wrong field or symbol).
- Do NOT run ApplyNames, touch the Ghidra master, hooks.csv or CHANGELOG (the parent applies names and runs the C1 -> C2 batch
  recipe), and do not edit another chunk's addresses. Scratch under log/p1/hK/.
- Final report under 80 lines: counts, the largest groups, deferred reasons, model corrections, files written.
