# S6.2 notes

Classification built from model_defects.md sections 3, 3b, 5 (as written, with the cited addresses) and every non-PASS row of re/port/divergences.tsv (76 lines, grouped to 14 V rows). Instructions were NOT re-disassembled: classes and flags rest on the cited descriptions and the port notes (log/p3/p31a, p33, p34, p35). Open points are in 2_uncertain.tsv (U-1560..U-1564).

Counts: 33 D rows (defects 30-35, 37-52; #40, 47, 48, 49, 52 split into arms), 14 V rows. Recommendation fix: D40a, D41, D42, D43, D50, D51, D52b (hang or fault only). ask: D32, D37, D38, D39, D44, D46, D47b, D48a, D48b, D49a, D52a. keep: dead state, port gaps, harmless, D33 (port requirement), D45 (RNG advance).

Port today: hangs are NoReturn (D40a, D41, D42 loop, D50 on a repeated grid, D51, D52b), faults are DivideError (D42, D43), D52a is MissingResource, D46 has no limit, D47a reads an empty string, overruns reproduce through the address space.
No port code edited. Proposed switch names are `<function>.<what>`; all but five are to-implement.
