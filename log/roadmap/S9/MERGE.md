# S9 merge request (decision G14 = option 1: S9 closes as "the original has no network play")

## 1. Files (log/roadmap/S9/files.txt)
S9.1: re/analysis/net/NET_MODEL.md, re/port/net_functions.tsv, net_messages.tsv, net_state.tsv, re/tools/net_survey.py,
tests/test_net_survey.py. S9.2 part 1: re/frida/js/net_rec.js, re/tools/net_rec.py, re/tools/net_trace.py,
re/analysis/net/net_recordings.md, tests/test_net_trace.py. Section: re/roadmap/S9_steps.tsv, log/roadmap/S9/*.
S9.2's recorder and tests stay as held notes (G14). S9.3: module net as specification (port/include/portsim/sim/net.h, port/src/sim/net.cpp + net_*.inc, port/test/test_net.py).
Dropped by G14: S9.2 part 2, S9.4, S9.5, S9.6.

## 2. Shared-file edits, in apply order
1. UNCERTAINTIES.md: append the rows of log/roadmap/S9/1_uncertain.tsv (U-1750..U-1754) and
   log/roadmap/S9/2_uncertain.tsv (U-1760..U-1763) in the file's table format. U-1760 / U-1761 are closed by S9.1's
   evidence (status `resolved: NET_MODEL.md section 0`).
2. re/c3_hold.tsv: the two reason edits of log/roadmap/S9/1_merge.md section 2 (netService 0x00497fc0, netSendMessages
   0x00499140: IDirectPlay2A, not wsock32).
3. re/model/DECISIONS.md: the g_ui_83ff9c -> g_netPlayerName uint8_t[0x100] correction (1_merge.md section 3).
4. Rename proposal, P (decision D1 class: S9.3 registers port functions under hooks.csv names, so the name is a port key):
   netQueueMessage 0x0049b690 -> netAddPeer (1_merge.md section 4; it adds a peer to the slot table). Deferred by the orchestrator.
5. re/roadmap/S9_PLAN.md: append `## Outcome (G14, 2026-10-29)`: the original has no network play (NET_MODEL.md section 0);
   the survey is merged, the dormant layer is ported as specification (S9.3), recordings / replay / interop gates dropped.
6. re/roadmap/sections.tsv row S9: status `done`, gate column replaced by: survey merged, net module as specification judged
   by its C3 path-1 rows, segments 205/205 and boot rows unchanged; notes `G14: the original has no network play`.
7. port/src/sim/modules.inc: append `SG_MODULE(net)`, then touch port/src/sim/registry.cpp (MSBuild does not rebuild it).
   Until this line lands, port test_static's test_every_module_file_is_listed fails on the shared tree (net.cpp unlisted).
8. CLAUDE.md: the paragraph below, after the S10 paragraph.

## 3. CLAUDE.md paragraph
Roadmap S9.1, the network survey (2026-10-29, re/analysis/net/NET_MODEL.md, tables re/port/net_functions.tsv /
net_messages.tsv / net_state.tsv, tool re/tools/net_survey.py, tests tests/test_net_survey.py, U-1750..U-1754, U-1760..U-1763):
the Jackal network layer is linked but never active in v1.03 or v1.00. g_netSession 0x008400b8 is .bss with no store in the
image; the four entries into the layer (gfxFlushA / gfxFlushB from the input pumps, uiIdleNetPoll, Window::onCommand) test it
and return; g_netInterface 0x008400b0 (IDirectPlay2A) is only ever stored 0 (0x00497d78). No DPLAYX.dll import descriptor
(golf.exe, golf_clean.exe, v1.00: only the name string 0x004bf82e, unreferenced), no LoadLibraryA of it, no CreateThread /
CreateEventA / socket import, no Receive / Open / EnumSessions call; the DirectPlay GUIDs 0x004bc3e8..0x004bc428 are
unreferenced and the connect / host / join dialogs exist only in jackal.txt. The 13 interface calls: Close, Release,
EnumGroupPlayers (callback netMarkRecordCb), 9 Send, GetPlayerName. Packet = type word, sequence dword, payload at +8;
types 0x10 roster (host only), 0x20 keep-alive every 2 s, 0x80 peer drop, 0x100 / 0x200 stream data / end; DPSYS 3 / 5 /
0x31 / 0x102 / 0x103 parsed by netPoll; no simulation state is sent (membership, liveness and a stream only; host migration
by slot order). All 205 segment images hold 0 in every net global. The firewall here is BlockInbound with notification and
has no rule for the game. S9.2 part 1 built a generic DirectPlay recorder (re/frida/js/net_rec.js, reader
re/tools/net_trace.py, 24 static tests); its Frida part never ran, since no organic run can reach DirectPlay.
Decision G14 (Mariano, 2026-10-29): S9 closes as `the original has no network play`; recordings, replay and interop gates
dropped; the dormant layer is ported as specification only (S9.3: module net, 12 bodies + 13 registered
names over a recording NetHost at the IDirectPlay2A slot level, never called from tick; port/test/test_net.py 29 passed, 13 / 13
controls RED; only netNameOf (20 rows) and netPump's address-free rows are CSV-exact path-1 replays, because the other
net path-1 CSVs hash fixture blocks that hold the run's own heap addresses, so the rest are judged by instruction-level
assertions on rebuilt session objects with the host call log checked call by call; uiIdleNetPoll, netMarkRecordCb,
netService, netSendMessages have no path-1 rows and stay host calls; netHandleControl.slot15Stays default 1; U-1770..U-1774).

## 4. Gate command lines (static, no game)
```
~/bin/py -3.12 re/tools/net_survey.py check
PROYECTOS_TEST_SLOT=off ~/bin/py -3.12 -m pytest tests/test_net_survey.py tests/test_net_trace.py -q -p no:cacheprovider
```
Expected: 33 passed (9 + 24); controls RED 5 / 5 (survey) and 9 / 9 (trace). Lead's run 2026-10-29: 33 passed in 28.5 s.

## 5. S9.3 gate (needs the integrator's build with SG_MODULE(net))
```
SG_PORTSIM_DLL=<build>/Release/portsim_c.dll ~/bin/py -3.12 -m pytest port/test/test_net.py -q   # 29 passed
~/bin/py -3.12 -m pytest port/test/test_static.py -q                                             # 6 passed
```
Measured: 29 passed on the worker's mirror (port/build/S9_3) and again by the lead; Win32 compiled, runner32 not built, so
test_net.py ran on x64 only. NOT measured (six test slots busy all session, account near its limit): segments 205/205 and the
boot rows with the module linked; the module adds one registry line and no tick call. Run both on the integrator's build.
Lead edit after the worker: image-range addresses removed from net_*.inc comments (port test_static rule); code unchanged.
