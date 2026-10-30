# S9.3 notes (worker, 2026-10-29)

Module `net` (decision G14: the original has no network play; the layer is specification only, NET_MODEL.md section 0).
Nothing in tick calls it; no kDrops change.

## What exists
- `port/include/portsim/sim/net.h`: `NetHost` (virtual `call(Call, args, payload)` and `queue_pop`), `RecordingNetHost`
  (log, scripted answers, an inbox for the receive queue +0x148). `Call` codes name each outward call at the slot level:
  the stream source slots at session+0x64 (0x14 0x20 0x88 0x8c 0x90 0x94), the session's own slots (+8 +0xc +0x10 +0x14
  +0x18), IDirectPlay2A (Close 0x10, Release 8, GetPlayerName 0x54), the unported peers (netService, netSendMessages),
  list/payload slots, the imports (timeGetTime, critical section, SetEvent, Sleep, CloseHandle, free), HUD text and dialogs.
- `port/src/sim/net.cpp` + `net_*.inc`: 12 functions with bodies, registered under their hooks.csv names (all 13 rows
  of the step), plus test-only entries `net.host*` (reset, answer, push, log readers). Defect switch
  `netHandleControl.slot15Stays` (default 1 = the original keeps slot 15's old contents).
- `port/test/test_net.py`: 29 tests (see Evidence). Mirror build: log/roadmap/S9/3_mirror.py -> port/build/S9_3_root,
  build dir port/build/S9_3 (x64), built with log/roadmap/S2/build.py (log/roadmap/S9/3_build_x64.log).

## Evidence, and why it is not a path-1 hash replay
The C3 path-1 CSVs hash the whole fixture block (`$objs`, `$clog`), which holds the fixture process's own addresses:
vtable pointers at +0, +0x64, list links, node pointers, and call-log arguments such as `this`. test_bridge.py could replay
its rows because it masks the stand-in pointer arguments; here the hashed bytes contain unmasked heap addresses that no
CSV records. So only rows whose RESULT is address-independent can be compared with the CSV:
- netNameOf: result = obj + offset, replayed for every row (test_name_of_replays_c3).
- netPump: the return value of the rows that do not release a message (the release row returned `msg ^ 0x8c8c`).
Every other function is judged by assertions written from the hook sources' instruction-level notes on a session object
rebuilt in a zero GameState, with the NetHost log checked call by call (slot, arguments, payload bytes). This is weaker
than the step's gate asks for; it is stated here and is the reason for PARTIAL.

## Rows the replay cannot rebuild (reason)
netBeginOp0/1, netFlush, netSendList, netSendShutdown, netQueueMessage, netServiceControl, netHandleControl, netPoll,
S3List_addVoiceRx, netListRemove: state hash covers fixture addresses (above). netServiceControl also reads a scripted
clock array indexed by a counter inside the hashed block.

## Not ported (no path-1 rows)
uiIdleNetPoll 0x00495d20, netMarkRecordCb 0x00497f60, netService 0x00497fc0, netSendMessages 0x00499140: they stay host calls
(`Call::NetService`, `Call::NetSendMessages`).

## Simplifications (each a U-row, log/roadmap/S9/3_uncertain.tsv)
- U-1771 netSendList: bytes 1..3 and the record bytes the original never writes are malloc garbage there, 0 here.
- U-1772 S3List_addVoiceRx 0x0049b970 is only forwarded to the host: its body is the ring add with a 0x64-byte VoiceRx
  payload built through the exe's operator new and VoiceRx::ctor, which have no port peer.
- U-1773 netHandleControl's dialogs (TextView ctor/loadScript/build, SEH teardown) are one `DialogRun` host call; the
  script ids 0x004e4680 / 0x004e4824 / 0x004e4838 / 0x004e484c / 0x004e4860 are not passed (kind 1 = host drop, 2 = client).
  The render flag word set on the first dialog's "drop" answer is not modelled. netSendShutdown's critical-section,
  CloseHandle and free calls are host calls, as is listClear on the three queues.
- U-1774 netPoll's GetPlayerName passes the size 0x100 by value; the original passes the address of a stack word holding 0x100.

## Controls
One mode-1 control per function (g_nc_*), each asserted RED in test_net.py. See the test log for the final count.

## Result (2026-10-29)
- x64 mirror build port/build/S9_3: clean. `PROYECTOS_TEST_SLOT=off SG_PORTSIM_DLL=port/build/S9_3/Release/portsim_c.dll
  ~/bin/py -3.12 -m pytest port/test/test_net.py -q` = 29 passed (log/roadmap/S9/3_test_x64.log). The test slots were all busy
  (6 holders), so the quick run skipped the slot with the documented one-command switch; run 1 had 8 failures, all test
  mistakes (CSV meta rows read as vectors, bytes-vs-list compares, ring passed at the wrong base, a fixture write that
  zeroed peer 1's id) plus one port fix (netSendMessages calls lacked the data argument slot).
- Controls: 13 modes (one per registered function), each asserted RED in its own test: 13/13 RED, 0 blind. Check arithmetic:
  begin_op control flips the mode word (RED on both), flush control leaves bit 29, pump control moves the threshold to 0x33
  (RED on the row with pending 0x32), service control leaves the guard set after the third arm, list-remove control keeps the count.
- Win32: portsim_c compiled for Win32 in port/build/S9_3_w32 (see log/roadmap/S9/3_build_w32.out for the final status); the
  Win32 runner (runner32) was not built, so the tests ran on x64 only. The module has no x87 code and no pointer-size
  dependence (32-bit virtual addresses throughout).
- Segment gate and boot gate: NOT run. Six test slots were busy for the whole session and this account is near its limit;
  the module is not called from tick and adds only a module line, so segment digests cannot change, but that is argument,
  not measurement. Run them on the integrator's build after the modules.inc line lands.
- Gate shortfall: the step asks for path-1 FNV replays of every function; only netNameOf (all 20 rows) and netPump (the
  rows with address-free results) are CSV-exact. See "Evidence". Verdict PARTIAL.
