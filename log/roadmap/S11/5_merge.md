# S11.5 merge notes: endstate.py PE entry point / TLS callback / export-containment dead check (U-2020)

The patch is `log/roadmap/S11/5_endstate.diff` (diff of re/tools/endstate.py against log/roadmap/S11/5_endstate.py).
Apply by replacing re/tools/endstate.py with the copy and keeping its `ROOT` line as in the original. Exact anchored edits:

1. `ROOT = pathlib.Path(__file__).resolve().parents[3]   # S11.5 copy ...` -> keep the original `parents[2]` (the copy lives deeper).
2. Rename `def _exports(m):` to `def _exports_uncached(m):` and put before it (`_EXP, _LDR = {}, {}`, cached `_exports`).
3. Insert before `def dead_checks(module, addr):` the helpers `_loader_entries` (+ cached wrapper and `_loader_entries_uncached`),
   `_container`, `has_tls` (text in 5_endstate.py).
4. In dead_checks, after `exp = sorted(t for t in targets if t in _exports(m))` add:
   `exps = _exports(m)`; `exp += sorted(e for e in exps if _container(m, e) == va and e not in exp)`;
   `ldr = [f"0x{e:08x} {lab}" for e, lab in sorted(_loader_entries(m).items()) if _container(m, e) == va or e in targets]`.
5. Append to the returned list: `("not the PE entry point or a TLS callback", not ldr, ", ".join(ldr)),`.
6. Update the module docstring's dead list with: "not the PE entry point, a TLS callback, or a function containing an export".
7. tests/test_endstate_entry.py: after the merge run `SG_ENDSTATE_PATH=re/tools/endstate.py ...` or change PATCHED's default to re/tools/endstate.py.
8. log/roadmap/S11/3_triage.py's `is_entry` workaround becomes redundant (harmless).
