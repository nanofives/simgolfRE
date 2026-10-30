# S9.1 merge notes (lines for shared files; the integrator applies them)

## 1. UNCERTAINTIES.md: append the five rows of log/roadmap/S9/1_uncertain.tsv
U-1750 .. U-1754, as table rows in the file's format (`| U-17xx | addr | type | description | path | open |`).

## 2. re/c3_hold.tsv: correct two reasons (the transport is DirectPlay, not wsock32)
Row `golf_clean.exe 00497fc0`: replace `the COM object [0x008400b0], wsock32,` with
`the IDirectPlay2A object [0x008400b0] (Send / EnumGroupPlayers slots; the exe imports no socket DLL),`.
Row `golf_clean.exe 00499140`: replace `same frame, dialog, COM and wsock32 layer` with
`same frame, dialog and IDirectPlay2A Send layer`.
Evidence: re/analysis/net/NET_MODEL.md sections 1-2, `re/tools/net_survey.py report`.

## 3. Model correction (re/model/DECISIONS.md list for the next model phase)
`g_ui_83ff9c` 0x0083ff9c (re/model/addresses.tsv, int32_t[4], class ui) is GetPlayerName's DPNAME out-buffer:
netPoll passes it with a size word of 0x100 (0x0049b8ad / 0x0049b8bb) and reads +0xc = lpszLongNameA
(0x0049b8d6). Proposed: `g_netPlayerName` uint8_t[0x100], ending at 0x0084009c (below g_net8400ac).

## 4. Name lead (next names round)
0x0049b690 `netQueueMessage` adds a peer to the session's slot table (its C4 Purpose and NET_MODEL.md section 4);
proposed `netAddPeer`. Not applied here (names files and the Ghidra master are not this step's).

## 5. CLAUDE.md: one paragraph after the S10 / S9 rows
Roadmap S9.1, the network survey (2026-10-29, re/analysis/net/NET_MODEL.md, tables re/port/net_functions.tsv /
net_messages.tsv / net_state.tsv, tool re/tools/net_survey.py, tests tests/test_net_survey.py, U-1750..U-1754):
the Jackal network layer is linked but never active in v1.03 (and v1.00). g_netSession 0x008400b8 is .bss with no
store in the image, and the four entries into the layer (gfxFlushA / gfxFlushB from the input pumps, uiIdleNetPoll,
Window::onCommand) test it first; g_netInterface 0x008400b0 (IDirectPlay2A) is only ever stored 0 (0x00497d78). There
is no DPLAYX.dll import descriptor (only a name string 0x004bf82e with no reference), no LoadLibraryA of it, no
CreateThread / CreateEventA / socket import, no Receive / Open / EnumSessions call, and the DirectPlay GUIDs at
0x004bc3e8..0x004bc428 have no reference; the connect / host / join screens exist only as jackal.txt sections the exe
never names. The 13 interface calls are Close, Release, EnumGroupPlayers (callback netMarkRecordCb), 9 Send and
GetPlayerName. Messages: Jackal packet = type word, sequence dword, payload at +8, length size + 0xc; app types 0x10
roster (host), 0x20 keep-alive (2 s), 0x80 peer drop, 0x100 stream data / 0x200 stream end (the +0x64 source,
started by onCommand), DPSYS 3 / 5 / 0x31 / 0x102 / 0x103 parsed by netPoll; no simulation state is sent. All 205
segment images hold 0 in every net global. So the original cannot host or join and S9's recording / interop gates
have no original side.

## 6. Decision for the lead (S9.2 part 2, S9.5, S9.6)
See log/roadmap/S9/1_notes.md section 4.
