---
name: diff-original
description: Frida A/B diff between SimGolf's original code and our reimplementation, inside the live game. Use after writing or changing an SG_HOOK reimplementation in shim/src/re/, before any C3 promotion, or when asked to "diff", "verify", "A/B", or "compare against the original".
---

# diff-original (SimGolf)

Path-1 A/B: `re/frida/diff_hook.py` boots `golf_clean.exe` (windowed, intro skipped) under Frida, asks
the shim for the hook (`SimGolfShim_FindHook`), and calls the **original** (MinHook trampoline) and
the **reimplementation** (detour) with identical inputs. It also records the install witness (`0xE9`
at the hooked address) and writes `log/diff/<addr>_<name>.path1.csv` ending in `VERDICT,GREEN|RED`.

## Steps

1. Reimplementation in `shim/src/re/<Area>.cpp` with `// 0x<addr>` and `SG_HOOK(...)`; `shim\build.bat`.
2. Add the test vectors to `re/frida/hooks_registry.py` (never a one-off harness). Cover each branch,
   each boundary and one past it, negatives, INT_MIN/INT_MAX. Pointer arguments come from a fixture in
   `re/frida/js/diff_fixtures.js` (`"$obj"` in a vector). Pointer returns are compared relative to `$obj`.
3. `py -3.12 re/frida/diff_hook.py <name>` (or `--all`). RED: read the mismatching rows; fix the body,
   never the vectors.
4. Hand the CSV to the `re-classify` skill.

## Evidence grade

Path-1 is **C3-grade at most**: it proves the body matches on those inputs, not that the game behaves
the same with the hook live. C4 needs a canonical-scenario ON/OFF run (DEFERRED D-0002).

## Hygiene

- The game is single-instance. The tool refuses to start if a SimGolf window exists. Never kill a
  process you did not spawn.
- Frida `Interceptor.attach` on hot paths is fine for counting; keep traces short.
- `tests/test_diff_tool.py` proves the tool reads GREEN on a faithful body and RED on a wrong one.
  If you change the diff tool, run it.
