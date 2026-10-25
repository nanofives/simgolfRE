# S1.2 merge notes (apply by the integrator; no shared file was edited)

1. UNCERTAINTIES.md: append the three rows of log/roadmap/S1/2_uncertain.tsv (U-1160, U-1161, U-1162, columns as the table).
2. UNCERTAINTIES.md, row U-1012: set the status to `resolved (S1.2, 2026-10-24)` and append to the description:
   "Resolved by port/test/test_shots_strict.py: 8 constructed states (seeds of `_case`; 42 of 700 swept differ) where the original,
   run in Unicorn, ends with the strict flag sticky; the port ON equals it in eax and the whole image, OFF is RED (8 / 8);
   code hooks on 0x00423b0e / 0x00423b20 show a scan reading 1 with no store at its own landing. Residual: U-1160..U-1162."
3. re/analysis/data/model_defects.md row 37 / re/analysis/data/divergences (`re/port/divergences.tsv`, `integrator ledger`): the
   chooseShotTarget.strictFlagSticky row now has evidence: test_shots_strict.py (cause was `no segment or test reaches it`).
4. port/test/test_shots.py::test_defects_the_vectors_cannot_see keeps its meaning (the 12 vectors still do not see the switch);
   its docstring can point at test_shots_strict.py. No change to port/src/sim/shots.cpp or the defect switch.
5. CLAUDE.md (one line, optional): "U-1012 resolved (2026-10-24, S1.2): constructed states in port/test/test_shots_strict.py judge
   chooseShotTarget.strictFlagSticky against the original in Unicorn (SG_EMU_ORACLE=1)."
