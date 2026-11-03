# S11.5 notes: endstate.py loader-entry check

Done: `log/roadmap/S11/5_endstate.py` (copy of re/tools/endstate.py, shared tool untouched) adds a fourth dead check, "not the PE entry point or a TLS callback":
AddressOfEntryPoint (golf_clean.exe 0x004a682f, jgld.dll 0x10082770, sound.dll 0x100430fe, Terrain.dll 0x10019b80) plus every TLS callback
(AddressOfCallBacks array, null terminated), judged against the function CONTAINING the address (module ranges), and the export check now also
refuses a function containing an export address, not only an export at its exact entry/thunk (existing check: PE export table vs body/thunk).
Also caches _exports and the loader entries (the test calls dead_checks ~250 times). ROOT uses parents[3] in the copy.

Numbers (reproduce: `~/bin/py -3.12 -m pytest tests/test_endstate_entry.py -q -o addopts=""`, 5 passed):
- the four entry points FAIL dead naming the new check; S11.3's 188 dead + 32 trivial addresses (parsed from 3_merge.md) all still PASS.
- No module has a TLS directory (U-2040).
Controls (2 of 2 RED, 0 blind):
1. SG_ENDSTATE_PATH=re/tools/endstate.py (unpatched): test_entry_points_refused_and_marks_pass FAILS (1 failed, 3 passed, 1 skipped); test_control_unpatched_reads_red asserts the unpatched tool does not refuse.
2. patched copy with the check forced True (test_control_check_disabled_reads_red): the refusal assert is false, so the main assertion would read RED.
Re-run of S11.3's endstate step with the patched copy (`~/bin/py -3.12 log/roadmap/S11/5_rerun.py`, log 5_rerun.log, cache 5_cache/endstate.json), 1,881 open rows:
only the four entry points change verdict (dead/dead_ok True -> none/False); no other open C2 row changes. S11.3's triage already excluded them by hand, so the marks stand.
Files: see 5_files.txt. Merge: 5_merge.md. U-rows: 5_uncertain.tsv (U-2040..U-2042).

## Revision after lead review
- Test target: SG_ENDSTATE_PATH, else re/tools/endstate.py when its source holds the new check, else log/roadmap/S11/5_endstate.py. It no longer reads log/ files.
- End-state rows come from hooks.csv: all 283 dead/trivial rows pass the patched check (no finding). After I23's marks the same test covers 503.
- The S11.3 one-off (188 dead + 32 trivial from 3_merge.md, all pass; the four entries fail) was run with the first version of the test (5 passed) and is recorded here only.
- Unit tests on stubbed pefile/module (FakeMod, FakePE): TLS array with two callbacks and a NULL terminator -> both collected; a callback at a non-entry address of function A refuses dead; an export at a non-entry address of A refuses "not exported" while B passes.
  Controls (each RED): TLS label line disabled, export-containment line disabled, check line forced True.
- Unpatched-tool control (test_control_unpatched_reads_red): unpatched re/tools/endstate.py does not refuse the entries (the main test failed on it before: 1 failed, 3 passed, 1 skipped); it skips itself once the tool is patched.
- Runs: patched copy as target 10 passed; scratch copy of re/tools/endstate.py with 5_endstate.diff applied and ROOT line set to parents[2] (SG_ENDSTATE_PATH): 10 passed (11 total before removing one weak test). Note the diff changes the ROOT line to parents[3]: the merge must keep parents[2] (merge step 1).
