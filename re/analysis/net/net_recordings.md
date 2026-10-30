# NET.trace: DirectPlay calls beside a schema v1 trace (roadmap S9.2, 2026-10-29)

A NET.trace holds every call golf_clean.exe (or any non-DirectPlay module) makes into DirectPlay during one run of the
original, with arguments, results, payload bytes, the callbacks DirectPlay makes back, and the objects it hands out. It is
a SIDE FILE of the run's schema v1 trace (re/model/trace_schema.md): both come from the same agent and share its position
key. trace_format.py and trace_rec.js are not changed by it.

Tools: writer `re/frida/js/net_rec.js` (appended after scenario_det.js + trace_rec.js by `re/tools/net_rec.py record`),
reader `re/tools/net_trace.py` (`validate`, `summary`, `dump`, `diff`, `coverage`; `tables [--write]` / `check-tables`
generate and check the agent's interface table from the DirectX 8 SDK headers tools/dxsdk8/include/dplay.h and dplobby.h).
Files: `<out stem>.NET.trace` next to the schema v1 trace `<out>` (default port/corpus/net/<scenario>/<label>.trace), a
sidecar `<out stem>.NET.json` (sha256, validate, summary, agent errors, pointer watch).

## 1. What is hooked and why (golf_clean.exe v1.03)

- The exe imports no DirectPlay function: its import directory (0x004bf008, 0x78 bytes) lists binkw32, Terrain, KERNEL32,
  USER32 and WINMM only. The string `DPLAYX.dll` at 0x004bf828 has no code reference, and no module of the game holds a
  DirectPlay export name.
- The net layer keeps its IDirectPlay2 pointer at 0x008400b0. It reads it (0x00497b0a ... 0x0049b8a4), calls Close
  through slot +0x10 (0x00497d6a) and Release through slot +0x8 (0x00497d75) and clears it (0x00497d78). No instruction
  of the four modules encodes a store of another value there.
- So the agent does not assume one creation path:
  1. dplayx.dll's exports listed in the table are hooked with Interceptor.attach when the module maps (any caller).
  2. The exe's GetProcAddress / LoadLibraryA IAT slots (0x004ba0a4 / 0x004ba0a8) are wrapped; a call is recorded
     when it names or returns a DirectPlay module (GETPROC / LOADLIB).
  3. Every interface pointer handed out through an out parameter gets its vtable slots 0..n-1 (n from the table) replaced
     by thunks `8 x nop; jmp [cell]; 2 x nop` with an Interceptor.attach each. A call is named by (vtable, slot), never by
     its implementing function, which dplayx may share between interfaces.
  4. Callback arguments are replaced by thunks that record each invocation.
- Calls whose return address lies in a DirectPlay module (dplayx.dll, dpwsockx.dll, dpmodemx.dll, dpnet.dll, ...) are
  dplayx's own and are not recorded.
- A real-time poll (50 ms) of 0x008400b0 reports to the sidecar (`watch`) when that pointer changes. It is not a trace
  record, because its timing is the host's.

## 2. File layout

```
offset 0  char[8] "SGNETTR\0"   8 u16 version (1)   10 u16 header size (20)   12 u32 meta_len   16 u32 0
       20 meta: UTF-8 JSON, keys sorted, no spaces: net_schema 1, scenario, tables (sha256 of the agent's generated
          block), ifaces / slots / methods (names and slot counts; interface id = index + 1, 0 = the exports), exports,
          nexports, cbs, ncbs, spec, trace_sha256 (sha256 of the schema v1 trace of the same run)
       .. records: u8 type, u8 sub, u32 payload length, payload (little-endian, no padding); the last one is END
```

| type | name | payload |
|---|---|---|
| 1 | MODULE (sub 1 mapped) | u32 pos, u8 thr, str8 module name (lower case) |
| 2 | LOADLIB | u32 pos, u8 thr, u32 ra, u8 dp (result is a DirectPlay module), str8 base name |
| 3 | GETPROC | u32 pos, u8 thr, u32 ra, u8 dp, u8 by_ordinal, u32 ordinal, str8 name, u8 export id (0xff not in the table, 0xfe NULL) |
| 4 | CALL | u32 pos, u32 seq, u8 thr, u32 ra, u8 iface, u8 slot, u32 obj (0xffffffff for an export), items (entry values) |
| 5 | RET | u32 pos, u32 seq (its CALL), u8 thr, u32 eax (HRESULT, or the count of AddRef / Release), indexed items (out values) |
| 6 | CB | u32 pos, u32 seq, u8 thr, u32 fn (the callback's call-site form), u8 callback id, items |
| 7 | CBRET | as RET, for a CB (its BOOL and out values such as EnumSessions' *lpdwTimeOut) |
| 8 | NEWOBJ | u32 pos, u32 obj (ordinals from 0), u8 iface, u32 seq (the call that produced it; 0xffffffff none), u8 how (1 out parameter, 2 first seen as `this`) |
| 0x7f | END (sub 1 = at ExitProcess) | u32 records before it, u32 CALLs, u32 CBs, u32 objects |

`pos` is the schema v1 position (trace_rec.js trPos()); `ra` / `fn` use trace_rec's call-site form (exe VAs, DLLs as
0x80000000 | id << 24 | rva, other modules 0xff000000 | low 24 bits); `thr` 0 = the game's main thread, 1, 2, ... other
threads by first record. items = u8 count + items; indexed items = u8 count + (u8 parameter index, item).

## 3. Items and parameter kinds

Items carry their own tag, so a reader needs no table to parse them:

| tag | item | body |
|---|---|---|
| 0 | U32 | u32 |
| 1 | NULL | (a NULL pointer argument) |
| 2 | BYTES | u32 n, n bytes (an in buffer: Send's data, CreatePlayer's data, an address) |
| 3 / 4 | STR / WSTR | u32 n, n bytes without the NUL (ANSI / UTF-16LE) |
| 5 | OBJ | u32 object ordinal |
| 6 | CB | u32 the callback's call-site form |
| 7 | PTR | (a non-NULL pointer whose target is not recorded: contexts, handles, out pointers at entry) |
| 8 | GUID | 16 bytes |
| 9 | STRUCT | u8 kind (1 DPSESSIONDESC2, 2 DPNAME, 3 DPLCONNECTION, 4 DPCAPS, 5 sized), u32 n, n bytes with its pointer members zeroed, u8 count, one item per pointer member |
| 10 | RELBYTES | u32 n, n bytes, u16 k, k x u32 offset: an out buffer (Receive, GetPlayerName, GetSessionDesc, GetConnectionSettings) whose aligned dwords that pointed into the buffer itself hold their offset instead; the offsets are listed |
| 11 | BADPTR | (reading the argument faulted) |
| 12 | OUTNULL | (an out object pointer came back NULL) |

The kind of each parameter comes from its header type (net_trace.arg_kinds): DWORD / DPID / BOOL -> U32; REFIID / LPGUID
-> GUID; LPDPID / LPDWORD -> U32 at entry and at return; LPVOID + DWORD -> BYTES (except a context after a callback);
LPVOID + LPDWORD -> RELBYTES at return when eax is 0; LPDPSESSIONDESC2 / LPDPNAME / LPDPLCONNECTION -> STRUCT (strings by
the interface's charset: the A interfaces and IDirectPlay v1 ANSI); LPDPCAPS -> STRUCT at return; callback typedefs -> CB
(and the argument is replaced by the thunk); LPDIRECTPLAY* * and LPVOID * after a REFIID -> an OBJ at return (NEWOBJ first;
an ANSI interface hands out the A form); IUnknown * -> OBJ when known, else PTR. Struct offsets (dplay.h): DPSESSIONDESC2
0x50 bytes, lpszSessionName +0x30, lpszPassword +0x34; DPNAME 0x10, +0x8, +0xc; DPLCONNECTION 0x28, lpSessionDesc +0x8,
lpPlayerName +0xc, lpAddress +0x20, dwAddressSize +0x24. Copies stop at 1 MB (NET_CFG.maxBytes).

## 4. Validity rules (`net_trace.validate`)

1. Magic, version 1, header size 20, metadata parses with net_schema 1.
2. pos never decreases over the file (all threads).
3. CALL and CB share one seq counter, from 0, +1 per record; every RET / CBRET closes an open CALL / CB of that seq.
4. A CALL's iface / slot lies inside the tables (iface 0 = an export with obj 0xffffffff); its object exists already.
5. NEWOBJ ordinals count from 0; a NEWOBJ's iface lies inside the tables.
6. A new thread ordinal is the next unused one.
7. END is last and its totals equal the counts.
Calls still open at END are allowed: a thread can be inside a blocking DirectPlay call when the run ends.

## 5. Two instances, determinism and replay

- Each side runs its own scenario_det.js virtual clock. The clocks are independent. Packets travel in real time between
  two processes through dplayx's own threads and sockets.
- So a side's received payloads are inputs of that side, the same as its harness deliveries. They are not functions of its
  own trace. The record order of a Receive against the main-thread clock (its `pos`) is set by real-time arrival, not by
  the virtual clock.
- A replay of one side therefore needs the recorded Receive results (and EnumSessions / EnumPlayers callbacks, DPIDs,
  session GUIDs) fed back at their recorded positions, in place of the real dplayx. The NET.trace holds them.
- Other things the record holds that two runs of one schedule need not share:
  - DPIDs and session instance GUIDs, which DirectPlay generates;
  - the number of polling calls that return DPERR_NOMESSAGES before a message arrives;
  - which non-main thread a record comes from;
  - the pos of other-thread records, which is the main-thread clock count when they were written.
- Byte-identical NET.traces across three runs are therefore a measured result (part 2), not an assumption. The records
  that vary must be listed with their cause.
- The schema v1 trace of each side stays valid on its own. Every input that reaches the game's own memory through a
  DirectPlay call is in the NET.trace, not in the v1 trace.
