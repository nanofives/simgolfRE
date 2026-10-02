---
name: re-classify
description: Move a SimGolf function up or down the C0..C4 confidence ladder. Use whenever RE work on a function finishes (note written, reimplementation hooked, A/B run), when asked "what's the confidence of X", "promote", "mark as done", "C2/C3/C4", or after a regression. Wraps scripts/re_classify.py, which enforces re/CONFIDENCE.md mechanically and is the ONLY writer of hooks.csv and re/analysis/CHANGELOG.md.
---

# re-classify (SimGolf)

The rubric is `re/CONFIDENCE.md`. The enforcement is `scripts/re_classify.py`. This skill decides
*what evidence to point at*; the script decides *whether it is enough*. Never hand-edit `hooks.csv` or
the changelog.

## Flow

1. Dry-run the gate and read every FAIL line:
   ```
   py -3.12 scripts/re_classify.py check 0x<addr> --to C<n> [evidence flags]
   ```
2. Produce the missing evidence (do not argue with the gate):
   | Target | Typical missing evidence | How to produce it |
   |---|---|---|
   | C1 | real name, subsystem, evidence pointer | export/RTTI/string xref from Ghidra MCP (or `re/tools/decomp.py`) |
   | C2 | note with Signature/Reads/Writes/Callees/Constants, callers/callees recorded | read the decomp end to end; cite every address; disassemble to verify `ret N`, sizes, signedness |
   | C3 | Purpose, `shim/src/re/*.cpp` with `// 0x<addr>` + `SG_HOOK`, fresh build, path-1 GREEN, anti-island | `shim\build.bat`; add a `hooks_registry.py` entry; `py -3.12 re/frida/diff_hook.py <name>` |
   | C4 | canonical-scenario ON/OFF diff (`*.scenario.csv`), no STUB | scenario harness (DEFERRED D-0002 until the first live C3) |
3. Promote (writes `hooks.csv` + one CHANGELOG line under the marker):
   ```
   py -3.12 scripts/re_classify.py promote 0x<addr> --to C<n> [--name --subsystem --evidence --note --callers --callees --file]
   ```
4. Report to the user: old -> new level, any new U-/S- rows, what is still open.

## Flags

- `--callers none` means "searched and found nothing" (dead code). It blocks C3 by design.
- `--callees none` = leaf; C3 then needs >= 10 A/B vectors (leaf exemption).
- `--callees indirect` = only an indirect call through caller/runtime data; the A/B must exercise
  the dispatch with a recording stub.
- Callers/callees accept `;`-separated addresses or names. An address counts if it is C2+ in
  `hooks.csv` or has a non-default name; a bare `FUN_xxxxxxxx` never counts (island).

## Uncertainties

Any `[UNCERTAIN]` in a note must be filed in `UNCERTAINTIES.md` as `U-NNNN` *before* promotion, with a
path to resolution, and the marker rewritten as `[UNCERTAIN U-NNNN]`. Open `semantic`/`structural`
rows against the address block C3.

## Demotion

`py -3.12 scripts/re_classify.py demote 0x<addr> --to C<n> --reason "..."`: on a diff regression, an
anchor change, a struct reshaping, or contradicting evidence. Loud by design.

## Precedent

`0x004490d0` `Terrain::tileAt`: C2, A/B GREEN (54 vectors), **refused C3** because nothing calls the
`golf_clean.exe` copy (0 runtime calls; the live one is in `Terrain.dll`). Correct outcome.
