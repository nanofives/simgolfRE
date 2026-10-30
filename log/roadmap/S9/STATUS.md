S9 network play  (lead session cmvtz4j3u0ye9p715pbgfr408, opened 2026-10-29 G12)
steps: 6 total, 2 accepted (S9.1, S9.3), 1 held (S9.2), 3 dropped (G14); DONE sent
workers alive: 0
game slots held: 0 (none needed: the original never forms a session)
last event: 2026-10-29 S9.3 accepted (29 tests, 13/13 RED); MERGE.md + files.txt final; DONE sent
facts so far: golf_clean.exe imports no socket DLL; DPLAYX.dll is only a name string: no import
  descriptor and no LoadLibraryA site refers to it. Transport by design: DirectPlay (SMSG Network.cpp: IDirectPlay2A via DirectPlayCreate + DirectPlayLobbyCreateA). dplayx.dll / dpwsockx.dll /
  dpnet.dll / dplaysvr.exe are present in SysWOW64 on LIVING-PC.
decision: G14 option 1. Remaining: S9.3, then DONE with MERGE.md / files.txt
-------------------------------------------------------------------------------------------------------------------------

## Log
- 2026-10-29 17:15 plan: S9.1 survey, S9.2 recorder + two-instance driver, S9.3 C3/C4 net functions, S9.4 netService /
  netSendMessages, S9.5 replay gate (a), S9.6 app + interop gate (b).
- 2026-10-29 17:50 S9.1 interim: no writer of 0x8400b8 / 0x8400b0, no Open / EnumSessions / Receive call site, no DPLAYX
  import descriptor in golf.exe, golf_clean.exe or v1.00 (name string only); 13 interface call sites (Close, Release,
  EnumGroupPlayers, 9 Send, GetPlayerName). S9.2 sent two hold notices (it reads them at its turn end).
- 2026-10-29 18:05 S9.2 PARTIAL held_by_lead: recorder + reader + 24 static tests (9/9 RED), Frida part never run; worker closed.

- 2026-10-29 18:30 S9.1 DONE and accepted (lead re-ran tests 9 + 24 passed, 0 banned words); worker closed; MERGE.md draft written.

- 2026-10-29 G14 option 1 received; steps S9.2 p2 / S9.4-6 dropped; S9.3 rescoped to the 13 path-1 functions and spawned.

- 2026-10-29 S9.3 accepted; lead stripped image-range addresses from net_*.inc comments; DONE.
