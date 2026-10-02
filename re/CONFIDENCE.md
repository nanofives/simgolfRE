# Function Confidence Rubric (C0..C4)

Adapted from Mashed RE (`..\Mashed\re\CONFIDENCE.md`). Every row in `hooks.csv` carries a level
`C0..C4`. Promotion is one-way, one level at a time, and gated by **evidence on disk**, never by
feeling. The only thing that mutates a level is `scripts/re_classify.py` (wrapped by the
`re-classify` skill). It checks the gates below mechanically and refuses when evidence is missing.

```
py -3.12 scripts/re_classify.py check   0x004490d0 --to C3    # dry run: which gates pass/fail
py -3.12 scripts/re_classify.py promote 0x004490d0 --to C3    # writes hooks.csv + CHANGELOG
py -3.12 scripts/re_classify.py demote  0x004490d0 --to C2 --reason "..."
py -3.12 scripts/re_classify.py status
```

Addresses are VAs for `golf_clean.exe` (image base `0x00400000`, no relocations) and RVAs for DLLs
(`module` column says which). Every address is only valid against the SHA-256 anchors in
`re/anchors.json` (checked by `tests/test_static.py`).

## Levels

### C0 Unknown
- The function exists in Ghidra (listed in `re/functions_ghidra.tsv` for `golf_clean.exe`).
- **To C1:** a non-default name, a subsystem, and an evidence pointer: one of (a) an export / import /
  RTTI / library-ID name, (b) a string xref, (c) a caller chain anchoring it to a known entry point.

### C1 Located
- Tentative purpose from name/strings/xrefs. Body not yet read. Subsystem assigned.
- **To C2:** the decompilation has been read end to end and an analysis note exists at
  `re/analysis/<subsystem>/<addr>_<name>.md` with these sections: `## Signature`, `## Reads`,
  `## Writes`, `## Callees`, `## Constants`. NO-GUESSING applies (see below).

### C2 Disassembled
- Mechanical transcription: every memory access, branch and callee enumerated; operand sign and width
  correct; every field cited as `+0xNN`; every constant cited with the address it appears at.
- **To C3:** all of
  1. `## Purpose` in the note, plain prose, citing at least one address.
  2. A reimplementation under `shim/src/re/` with a `// 0x<addr>` comment and an `SG_HOOK(...)`
     registration at that address (runtime-toggleable via `[hooks]` / `SIMGOLF_HOOKS_OFF`).
  3. The shim build is newer than the source file (it compiles and is deployed).
  4. **Verified:** a GREEN path-1 A/B CSV at `log/diff/<addr>_<name>.path1.csv` from
     `re/frida/diff_hook.py`, with the install witness (`0xE9` at the address) present.
  5. **Anti-island:** at least one caller and one callee at C2+, *or* identified (named / documented
     role, including named library APIs), *or* exempt:
     - *Leaf exemption* (callee half): no callees, and item 4 exercises a non-trivial input domain.
     - *Indirect-dispatch* (callee half): the only callee is an indirect call through caller/runtime
       data, and the A/B vectors exercise the dispatch itself with a recording stub.
     There is **no** exemption for the caller half: a function nothing calls (dead code, such as the
     exported Terrain accessors in `golf_clean.exe`, 0 runtime calls) stays at C2 however good its
     A/B is. Precedent: `0x004490d0` `Terrain::tileAt` (2026-10-02).
  6. No unresolved `semantic` or `structural` uncertainty filed against the address.

### C3 Understood
- Purpose stated with citations, faithful reimplementation hooked and toggleable, path-1 GREEN.
- **To C4:** a **canonical-scenario** run with the hook live (`log/diff/<addr>_<name>.scenario.csv`,
  `VERDICT,GREEN`, install witness row) comparing hook ON vs OFF on observable game state, and no
  `STUB` marker in the function body.

### C4 Verified
- Canonical reference; lower-confidence callers may lean on it.

## Three axes (scoreboard) - added 2026-10-02

The C-level answers "how much evidence do we have about this function". It does not say whether the
function matters or how strong the verification is. `scripts/scoreboard.py` derives three axes from
evidence on disk (nothing hand-entered) and writes `re/scoreboard.tsv`:

| Axis | 0 | 1 | 2 | 3 | Source |
|---|---|---|---|---|---|
| **Reach (R)** | never ran | 1 scenario-phase | 2 | 3+ | `re/coverage/*.tsv` (INT3 census) |
| **Understanding (U)** | C0 | C1 | C2 | C3/C4 | `hooks.csv` |
| **Verification (V)** | none | path-1 A/B (hand vectors) | replay >= 100 real calls, or proof | 100% VC6 match, or scenario ON/OFF | `log/diff/*.csv` |

`priority = R * ((3-U) + (3-V))`: work on reachable code that is neither understood nor verified.
Dead code (R=0) never outranks live code.

C3 "verified" accepts either a GREEN path-1 A/B or a GREEN **replay** of >= 100 recorded real calls
(`re/frida/record.py` + `re/frida/replay.py`): offline Unicorn runs of the original bytes and the
reimplementation on the exact memory the game had, checked three ways (live result == emulated
original == reimplementation) plus write sets and stack cleanup. Replay covers what the game
actually does; path-1 covers the extremes the game never produces. Use both when both are cheap.
Precedent: Terrain.dll `0x00001d50` `Terrain::tileAt`, C3 on 2000 replayed calls (2026-10-02).

## NO-GUESSING (deterministic RE)
- Report only what the decompilation literally shows. Cite the address for every constant/offset.
- Unclear value: report raw hex/decimal and mark `[UNCERTAIN U-NNNN]` (filed in `UNCERTAINTIES.md`).
- Sign-sensitive values: give raw hex and signed decimal.
- Banned in notes outside an `[UNCERTAIN]` line: *probably, likely, seems to, appears to, I think,
  presumably, might be, maybe*. `re_classify.py` rejects a C2+ note that contains them.

## Demotion
Automatic and loud (a CHANGELOG row) when: a diff regresses (C4->C3), a struct the function touches
changes shape (C3->C2), an uncertainty it depends on gets contradicting evidence, or an anchor
changes (everything that cited the old bytes drops to C1 until re-verified).

## Anti-patterns (refused)
- Promoting on "this is probably what it does".
- Skipping levels (C0 -> C3).
- Promoting an island (anonymous callees, unknown globals).
- Promoting because the build compiles. Compile-passing alone earns nothing.
- Using path-1 A/B as C4 evidence. Path-1 proves the body, not the game with the hook live.
