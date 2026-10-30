# S9.2 notes: DirectPlay call recorder (part 1 written and tested before the hold; HELD BY LEAD 2026-10-29)

## Part 1: what was built (no game run)

- `re/frida/js/net_rec.js` (agent, appended after scenario_det.js + trace_rec.js). It records:
  - dplayx.dll's exports at map time;
  - the exe's GetProcAddress / LoadLibraryA slots 0x004ba0a4 / 0x004ba0a8 (only calls that name or return a DirectPlay
    module);
  - every interface handed out through an out parameter, by per-(vtable, slot) thunks;
  - every callback argument, by a thunk.
  - Calls from inside DirectPlay modules are skipped.
  - The key is trace_rec's trPos(). It ends with trace_rec's END: it wraps the trEnd binding and sends its final chunk
    first.
  - It has a 50 ms real-time watch of 0x008400b0, reported to the sidecar only.
  - The interface / slot / parameter-kind table is GENERATED from the DX8 SDK headers (`net_trace.py tables --write`),
    between markers.
  - A plain-JS codec block sits between `// <nr-codec>` markers so Node can run it in the tests.
- `re/tools/net_trace.py` (reader, no Frida):
  - SDK header parser (interfaces in slot order, IIDs, exports, callbacks, struct offsets) and the kind mapping;
  - format: Item / Record / parse / validate / summary / dump / diff / Writer;
  - CLI: tables, check-tables, validate, summary, dump, diff, coverage. coverage reads S9.1's `dp_calls` column, form
    `Send x3`.
- `re/tools/net_rec.py` (runner):
  - `record` = trace_tool.py's record with one more Run layer: it appends NET_CFG + net_rec.js and collects the
    'nrec' / 'nrec_watch' messages;
  - it writes `<out stem>.NET.trace` and `.NET.json`. The default out is port/corpus/net/<scenario>/<label>.trace, and
    `--instance` is required, so it never boots in original\;
  - `check`: tables == headers, and the combined agent parses (node --check).
- `re/analysis/net/net_recordings.md`: the format, item tags, parameter kinds, validity rules and the two-instance
  determinism section.
- `tests/test_net_trace.py` (static, 24 tests, 1.6 s). Log: log/roadmap/S9/2_test_net_trace.log.
  - Tables equal the headers. Slot counts are pinned: IDirectPlay 25, 2/2A 32, 3/3A 47, 4/4A 53, Lobby/A 14, 2/2A 15,
    3/3A 19, IClassFactory 5.
  - The parameter kinds of Send / Receive / EnumSessions / EnumPlayers / GetPlayerName / QueryInterface / Connect /
    EnumAddress / the exports.
  - The exe bytes at 0x00497d62..0x00497d78 call slots +0x10 / +0x8, which are Close / Release in the table.
  - Import directory facts. Struct offsets equal dplay.h.
  - The Node codec equals Python's encoder on CALL / CB / RET / MODULE / LOADLIB / GETPROC / NEWOBJ / END and on every
    item tag.
  - A valid fragment parses, validates and summarises.
  - Nine broken files are rejected, one per rule (NEGATIVE CONTROLS, all RED). parse rejects bad magic, truncation, bad
    meta and trailing bytes.
  - diff points at the first differing record. Relocation rewrites only self-pointers.
  - The combined agent parses. No shared file mentions net_rec.
  - An audit of every `.toUInt32()` that can reach a record (no raw pointers).
  - Every DirectPlay call S9.1's re/port/net_functions.tsv names is in the tables (5 methods: Close, EnumGroupPlayers,
    GetPlayerName, Release, Send). The test fails when the list is empty, so it cannot pass vacuously.
- Reproduce: `PROYECTOS_TEST_SLOT=off ~/bin/py -3.12 -m pytest tests/test_net_trace.py -m static -q -p no:cacheprovider`.
  On 2026-10-29 all six machine test slots were held by other sessions, and this file runs no game and takes 2 s.
  `~/bin/py -3.12 re/tools/net_rec.py check`; `~/bin/py -3.12 re/tools/net_trace.py check-tables`.
- Not changed: trace_format.py, trace_rec.js, trace_tool.py, scenario_det.js, scenario_ab.py, scenario_refs.py.

## Findings (measured in part 1; they bear on part 2)

1. golf_clean.exe imports no DirectPlay function. Its import directory 0x004bf008 lists binkw32, Terrain, KERNEL32,
   USER32 and WINMM.
2. The string `DPLAYX.dll` at 0x004bf828 has no code reference. No module holds a DirectPlay export name (`DirectPlay`
   occurs nowhere in the exe, nor in v1.00 or golf.exe).
3. The exe's three LoadLibraryA call sites load other libraries:
   - 0x004a00aa loads the display library through its argument;
   - 0x00483fd1 loads the sound library;
   - 0x004ac8ea loads USER32 for the CRT.
   The GetProcAddress call sites 0x004a00c0, 0x004a7513 and 0x004ac902.. do the same.
4. The DirectPlay pointer 0x008400b0 is only read, compared or cleared (U-1760). Even SMSG's Network.cpp assigns it only
   0 (its line 122). So the shipped exe holds no path that creates the object, unless a store goes through a computed
   pointer.
5. jackal.txt carries multiplayer dialogs that no module names (U-1761).
6. Consequence: part 2's host / join run may find no screen that starts a session. It may also record a NET.trace with
   no DirectPlay record. Run 1 below measures this before any two-instance work.

## Part 2 plan (needs 2 game slots; not started)

Order matters: run 1 decides whether runs 2-4 can exist at all.

0. **Probe the agent outside the game.** No game slot is needed. This settles U-1762.
   - Write `log/roadmap/S9/2_probe/dpprobe.c`, a 32-bit console program built with MSVC x86 through a Python build
     script (never a bare cmake). It does LoadLibraryA("dplayx.dll") and GetProcAddress("DirectPlayCreate"), then
     DirectPlayCreate(NULL, &dp1, NULL), QueryInterface(IID_IDirectPlay2A), EnumSessions with a callback, and
     Release x2.
   - No service provider is initialised, so no socket is bound. EnumSessions returns an error, which is still a CALL / RET.
   - Write `2_probe/prelude.js`, the stubs net_rec.js needs: MAIN, K, a TD page with a counter, trPos, tn, trSite, trEnd.
   - Spawn the probe with Frida. Check the records against the program's own printout: GETPROC with export id, CALL
     DirectPlayCreate, NEWOBJ IDirectPlay, RET OBJ 0, CALL QueryInterface, NEWOBJ IDirectPlay2A, CB records if any,
     the Release counts.
   - Negative control: a probe that also calls through an unwrapped copy of the vtable pointer must produce no record.
1. **One instance: is DirectPlay reached at all?**
   - Commands:
     `~/bin/py -3.12 re/tools/instances.py create s9_host`, then
     `SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/net_rec.py record --scenario ui_tour --label p2_reach --instance s9_host 2>&1 | tee log/roadmap/S9/2_run1.log`.
     Run it in the background (ui_tour is about 415 virtual s) and poll the log.
   - The run records: the v1 trace, the NET.trace, and the sidecar `watch` for 0x008400b0.
   - If S9.1 names the screen or key that should start multiplayer, use a scenario that presses it instead.
   - Expected under U-1760: no MODULE / LOADLIB / GETPROC / CALL record and no watch event. Report that to the lead as
     a decision point, because S9's premise (a network session to record) would not hold.
2. **Two instances, host and join** (only if run 1 shows a path).
   - `re/tools/scenario_s9.py` registers two scenarios through the hook at the end of re/tools/scenario.py:
     - `s9_host`: title -> the multiplayer entry S9.1 names -> #SELECTSERVICE TCP/IP -> #NETCONNECT_CREATE (session
       name, player name) -> wait in the lobby until a second player is listed -> start;
     - `s9_client`: -> #NETCONNECT_JOIN -> TCP/IP. dpwsockx's own "Locate Session" dialog is answered with 127.0.0.1
       (window of class #32770 found in the client's process; edit text set, OK posted).
     - Then they pick the session, wait, and play the session S9.1 names: the shortest one, with one chat or one turn
       each way.
   - Coordination: each runner writes `log/roadmap/S9/2_runs/<run>/<side>.state` when its agent reports a milestone
     (Open RET 0, the first Receive with data). For this, net_rec.js gets an `nrec_evt` message, sent immediately, not
     batched. The client's script polls for the host's file before it joins. Both sides run their own virtual clock.
   - FIREWALL: the TCP/IP service provider binds TCP/UDP 47624 and a port in 2300-2400 for golf_clean.exe under
     instances\. If Windows shows a firewall prompt, stop at once and report BLOCKED to the lead. Never click it.
   - Commands (host first, client after the host's `open` milestone):
     `SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/net_rec.py record --scenario s9_host --label run_a --instance s9_host`
     `SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/net_rec.py record --scenario s9_client --label run_a --instance s9_client`
3. **Repeat three times** (run_a / run_b / run_c per side).
   - Compare per side: NET.trace with `net_trace.py diff`, the v1 trace with `trace_tool.py diff`, and checkpoint states
     against the first run with scenario_refs compare.
   - Expected variance, to measure and not to assume (net_recordings.md section 5, U-1763): DPIDs, session GUIDs, the
     count of DPERR_NOMESSAGES polls, other-thread pos, and the receive position of each packet against the main clock.
   - If NET.traces differ only there, add a `diff --loose` mode that compares other-thread records by order and content.
4. **Pin.** Put the files in `port/corpus/net/<scenario>/<label>.{trace,NET.trace,NET.json,trace.json}`, plus
   `port/corpus/net/manifest.json` (sha256, sizes, validate, summaries, the variance found). Write the committed table
   through the merge file: the brief's files_owned has no committed manifest path.
- Kill only the PIDs spawned. Call purge_frida_temp after the runs. Never use original\. Run light only (the lead's rule
  for today: until 20:00).

## Status at the hold (2026-10-29, lead notices 1 and 2)

- The lead held S9.2 after S9.1's verdict: the net layer never runs in v1.03. g_netSession 0x008400b8 has no writer and
  is 0 in all 205 segment images. There is no Open / EnumSessions / Receive call site. No DPLAYX import descriptor exists
  in golf.exe, golf_clean.exe or v1.00. This agrees with part 1's own finding (U-1760).
- When the notices arrived, part 1 was already complete. Every file is left exactly as listed in 2_files.txt; nothing was
  written after the second notice.
- What exists and works offline: net_rec.js, net_trace.py, net_rec.py, net_recordings.md and test_net_trace.py
  (24/24 PASS). None is wired into any shared tool: only net_rec.py loads the agent.
- Unfinished:
  - the Frida part of net_rec.js has never run in a process (U-1762);
  - scenario_s9.py is not written;
  - the `nrec_evt` milestone message for two-instance coordination is not written;
  - there is no `diff --loose` mode;
  - no recording exists, and port/corpus/net/ was never created;
  - the whole part 2 plan above.
- If S9 is closed, the files can stay as unused tooling or be removed by their file list; nothing else depends on them.
  If S9 resumes on another target (the port's own net module, or a build that creates the object), start at run 0 of the
  part 2 plan.
- U-1760 / U-1761 can be closed with S9.1's evidence. U-1762 / U-1763 matter only if a recording is ever made.
