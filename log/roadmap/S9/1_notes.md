# S9.1 notes: network survey (static, no game run)

Worker: claude2 session, 2026-10-29. Step row: re/roadmap/S9_steps.tsv S9.1.

## 1. Result
golf_clean.exe v1.03 links the Jackal network layer (0x00497b00..0x0049bf70, 20 functions with the gates) but the
layer can never run. g_netSession 0x008400b8 is .bss and has no store; every entry tests it; g_netInterface
0x008400b0 is only ever stored 0; no DirectPlay creation, connect, receive thread or import exists. v1.00
(golf_v100_clean.exe) has the same shape: its gate pair 0x004948a0 / 0x004948c0 tests session 0x0083ac48 (4 refs, no
store) and interface 0x0083ac40 (stores only from ebx at 0x00494ae4 / 0x00494b18; ebx not traced there), and the
same five unreferenced GUIDs at 0x004bb3e8... Full write-up: re/analysis/net/NET_MODEL.md. Early heads-up was sent
to the lead during the step (send_to_session).

## 2. Deliverables and numbers
- re/analysis/net/NET_MODEL.md: sections 0 verdict, 1 transport, 2 the 13 interface calls, 3 modes / screens, 4 network
  model, 5 message formats, 6 state ownership, 7 what a recording would need + firewall, 8 corrections,
  9 what a working network mode needs that the image lacks (9.1, with evidence) and what the present code fixes (9.2).
- re/port/net_functions.tsv: 20 rows (derived: addr name size level path1 port_module reach callers dp_calls
  net_globals; hand: role). re/port/net_state.tsv: 26 rows (14 const strings, 12 mutable; none sim class).
  re/port/net_messages.tsv: 10 rows (5 app types, 5 DPSYS system messages).
- re/tools/net_survey.py: `report` / `write` / `check`. `write` regenerates the derived columns and keeps the hand
  columns; the hand columns are filled by log/roadmap/S9/1_fill_functions.py and 1_fill_state.py (re-run after
  `write`). One `check` takes about 17 s (capstone over every function).
- tests/test_net_survey.py (static marker): 9 tests, 9 passed in 31 s (log/roadmap/S9/1_test.log).
  Controls 5 / 5 RED: a store to g_netSession added to the state table, a Release dropped from netSendShutdown's
  dp_calls, the drop notice's type changed to 0x40, a parse site moved outside netPoll, the keep-alive row removed.
  Blind: none measured; the hand columns (role, meaning) are not checked by the tool (they cite addresses).

Reproduce:
```
~/bin/py -3.12 re/tools/net_survey.py check
PROYECTOS_TEST_SLOT=off ~/bin/py -3.12 -m pytest tests/test_net_survey.py -v -p no:cacheprovider
```

## 3. Firewall (read only)
`netsh advfirewall firewall show rule name=all [verbose]` and `netsh advfirewall show allprofiles`, 2026-10-29:
domain / private / public all ON, BlockInbound,AllowOutbound, InboundUserNotification enabled; 604 rules, none
naming golf, simgolf, dplay, dplaysvr, and no 47624 in the verbose listing. Nothing was changed, no socket bound.
The original never binds (section 1); anything that hosts a DirectPlay or Winsock session on this PC (system
dplayx.dll under the port, or our own Winsock host) will raise the inbound prompt: Mariano's decision.

## 4. Decision for the lead (BLOCKED_DECISION material for S9.2 part 2 / S9.5 / S9.6)
The plan's gates (a) "every recorded session replays" and (b) "interop with an ORIGINAL instance" need an original
that forms a session; v1.03 does not. Options the facts leave:
1. Close S9 as "the original has no network play": port the dormant layer for completeness only (S9.3, judged by its
   C3 path-1 rows as planned) and drop S9.2 part 2 / S9.5 / S9.6.
2. Keep the network as a port-only feature: define the protocol from the dormant code (net_messages.tsv) plus our own
   choices for what is absent (session creation, receive thread, acks U-1753, stream source U-1752), host it in the
   app, and judge it port-to-port; there is no original to match, so "interop" has no meaning.
3. Construct a session around the original (a Frida harness that builds a session object and a recording
   IDirectPlay2A, as the C3 fixtures did) to record what the dormant code sends; this is a constructed scenario,
   not the game's behaviour, and every field of the absent constructor is a choice of ours.
S9.2 part 1 (the DirectPlay call recorder) will log zero calls in any organic run.

## 5. Tool notes
- The interface-call finder follows defs back through jump sources (Body.defs): a linear walk missed
  EnumGroupPlayers 0x0049844b, whose interface load sits before a branch.
- Refused tool calls: none. Ghidra headless decomp (re/tools/decomp.py) of 7 net functions ran once on a pool slot,
  read only (log/roadmap/S9/1_decomp.log).

## 6. Open
U-1750 (why `DPLAYX.dll` is in .idata), U-1751 (unwritten header word / tail bytes), U-1752 (what the +0x64 stream
source is), U-1753 (ack format absent), U-1754 (session object layout only from its accessors).
