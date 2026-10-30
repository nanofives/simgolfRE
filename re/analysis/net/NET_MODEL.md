# The network layer of golf_clean.exe v1.03 (roadmap S9.1)

Static survey from the anchored binary (`original/golf_clean.exe`, SHA-256 E74EA9F9...). No game run.
Re-derived by `re/tools/net_survey.py` (`report` prints the facts, `check` compares the three tables
`re/port/net_functions.tsv`, `net_state.tsv`, `net_messages.tsv` with the binary); `tests/test_net_survey.py`
checks them, with negative controls. Interface slot names come from the DX8 SDK's `tools/dxsdk8/include/dplay.h`.

## 0. Verdict

**In v1.03 the network layer is linked but never activated. No input, file or mode reaches a DirectPlay call.**

- The Jackal session object pointer `g_netSession` 0x008400b8 lies in .bss. .data's initialised part ends at
  0x004e7000 (SizeOfRawData 0x26000), so the loader gives it 0. No instruction in the image stores to it:
  its four dword references are all reads.
- Every way into the net layer tests `g_netSession` first and returns when it is NULL:
  - gfxFlushA 0x00497b00 (`je` 0x00497b08), called by the six input pumps;
  - gfxFlushB 0x00497b20 (`je` 0x00497b28), called by nine UI pump / modal functions;
  - uiIdleNetPoll 0x00495d20 (`je` 0x00495d28), slot 13 of the UI event table 0x004ba87c;
  - Window::onCommand 0x0047ee70 (`je` 0x0047ee7e).
  No other function in the layer has a caller outside the layer: net_functions.tsv `callers`.
- The IDirectPlay2A pointer `g_netInterface` 0x008400b0 (.bss) has exactly one store,
  `mov [0x8400b0], ebx` at 0x00497d78 in netSendShutdown. ebx is 0 there (`xor ebx, ebx` 0x00497d20).
- The code that would create the session is absent:
  - no DirectPlayCreate or DirectPlayLobbyCreate call;
  - no import of DPLAYX.dll, no CreateThread and no CreateEventA (section 1);
  - no Receive call (section 2).
- The runtime record agrees:
  - all 205 segment start images (port/corpus/segments) hold 0 at 0x008400b0 / b8 / bc / c0 and at 0x0083ff6c;
  - re/scoreboard.tsv gives reach 0 to every net function except gfxFlushA / gfxFlushB, which every scenario
    enters and which return at once.

What follows is therefore a model of a dormant layer: what the code would send and parse if a session object
existed. The session object's constructor, connect screens and receive thread are not in the image. Every
field meaning below comes from the code that is there.

Consequence for S9: two original instances cannot host or join. The original cannot produce any recording of
packets or per-frame network state, and gates (a) and (b) of re/roadmap/S9_PLAN.md have no original side.
S9.1 notes section 4 lists the choices this leaves the lead.

## 1. Transport: DirectPlay, never loaded

- **Imports** (import directory 0x004bf008, 0x78 bytes = 5 descriptors + the null one): binkw32.dll,
  Terrain.dll, KERNEL32.dll, USER32.dll, WINMM.dll. golf.exe (SafeDisc) holds the same five
  descriptors at 0x004bf008 (its header points at SafeDisc's own table, RVA 0x44b740). There is no socket DLL (wsock32 / ws2_32), no ole32 and no DPLAYX.dll.
  KERNEL32 gives no CreateThread, CreateEventA or _beginthread. The layer still calls SetEvent / ResetEvent /
  WaitForMultipleObjects on handles in the session object (netService 0x0049865c / 0x00498679), and nothing
  in the image creates those handles.
- **`DPLAYX.dll`** is a name string at 0x004bf82e, in the import-name area right after `Terrain.dll`
  (0x004bf822). No import descriptor names it (the five Name RVAs are 0xbf430, 0xbf822, 0xbf9f2, 0xbfb7e,
  0xbfbea), and no dword in the image equals 0x004bf82e. The three LoadLibraryA sites load other things:
  - soundVersionCheck 0x00483fd1: `.\sound.dll`, string 0x004e43bc;
  - loadGraphsyObject 0x004a00aa: engineInit's argument (0x004855c2), with GetProcAddress
    `get_graphsy_object_ptr`, jgld.dll's only export;
  - ___crtMessageBoxA 0x004ac8ea: `user32.dll`, string 0x004bc9a4.
  So nothing loads DirectPlay at run time either. [U-1750: why the linker kept the name string]
- **GUIDs in .rdata**, all with zero dword references in the image:
  IID_IDirectPlay2A 0x004bc3e8, DPAID_INet 0x004bc3f8, DPSPGUID_TCPIP 0x004bc408, DPSPGUID_MODEM 0x004bc418,
  DPSPGUID_SERIAL 0x004bc428. The same five sit at 0x004bb3e8.. in golf_v100_clean.exe. sound.dll holds all
  34 GUIDs of dplay.h / dplobby.h (0x1005d75c..0x1005d96c), also with zero references.
- **Interface version**: IDirectPlay2A is the only DirectPlay IID in the exe. The slot offsets the exe calls
  (section 2) are IDirectPlay2's, and IDirectPlay3 / 4 only append methods after slot 31, so the offsets are
  the same in every later interface.
- **Service providers, session description, ports**: none are asked for. No code builds a DPSESSIONDESC2,
  calls EnumSessions, Open or InitializeConnection, or holds a port number. The provider GUIDs above are the
  only traces, and nothing references them.
- **Firewall**: the original never binds a socket, because no DirectPlay object is ever created. Running it
  needs no firewall rule. Rules read on LIVING-PC: S9.1 notes section 3.

## 2. Every DirectPlay call (IDirectPlay2A through `g_netInterface`)

`net_survey.py` finds `call [V + d]` where V was loaded from `[I]` and I from `[0x008400b0]`. It follows the
defs back through jump sources. Arguments are listed first argument first (the interface).

| site | function | slot | method | arguments |
|---|---|---|---|---|
| 0x00497d6a | netSendShutdown | +0x10 | Close | - |
| 0x00497d75 | netSendShutdown | +0x08 | Release | - |
| 0x0049844b | netService | +0x28 | EnumGroupPlayers | (dest, NULL, netMarkRecordCb 0x00497f60, NULL, 0) |
| 0x0049862f | netService | +0x68 | Send | (local id +0x77c, dest, 0, packet, size + 0xc) |
| 0x00498734 | netService | +0x68 | Send | (local id, slot id, 0, packet, size + 0xc), per marked peer |
| 0x00498f1a | netService | +0x68 | Send | (local id, dest, flags & 1 ? DPSEND_GUARANTEED : 0, packet, size + 0xc); edi = [esp+0x1c] loaded at 0x004982f0 |
| 0x00499528, 0x0049960e, 0x0049970f, 0x00499e58, 0x00499f5f, 0x0049a809 | netSendMessages | +0x68 | Send | the same three shapes |
| 0x0049b8c2 | netPoll | +0x54 | GetPlayerName | (dpId, 0x0083ff9c, &size = 0x100) |

That is 13 calls and five methods. There is no Open, EnumSessions, CreatePlayer, Receive, GetSessionDesc or
SetSessionDesc, and no lobby interface. The other [0x008400b0] reads are NULL tests (netService 0x00497ffb /
0x00498336 / 0x004985de / 0x004988bd / 0x00498e06 and the matching ones in netSendMessages, netBeginOp0 / 1,
netPoll, netServiceControl, gfxFlushA / B).

Arguments marked `size + 0xc` are the Jackal packet length (section 5). DPSEND_GUARANTEED is the value of
`setne` on the caller's flags bit 0: netService 0x00498dff..0x00498e04, netSendMessages 0x0049a6ed..0x0049a6f2.

## 3. Modes and screens that reach the layer

- **No screen.** The connect, host, join, service-select and lobby screens exist only as text sections in the
  install's jackal.txt (section ids NETCONNECT_*, SELECTSERVICE, DPLAYERROR, NET_LOBBY_*). None of these ids
  occurs in golf_clean.exe, golf_v100_clean.exe, jgld.dll or sound.dll (byte search). The exe holds only the
  in-session ids its dialogs load:
  - NET_SENDTIMEDOUT 0x004e47e8;
  - NET_OKTODROPHOST 0x004e4824, NET_OKTODROPCLIENT 0x004e4838, NET_ADJUSTTOLERANCE 0x004e484c,
    NET_OKTODROPHOSTS 0x004e4860.
  The dialogs load them through TextView::loadScript with the prefix at 0x004e4680.
- **mainLoop, the championship and the title menu** make no call into the layer. The layer is entered only from
  the four gates in section 0. The input pumps call those gates on every pass, and they return when
  `g_netSession` is NULL. re/port/scope.tsv lists mainloop_phase `SETUP .. H2+boot` for these rows; that is the
  reach of the pumps that call the gates, not a net code path.
- **Chat**: no chat message type and no DPSYS_CHAT (0x0109) handling. netPoll's system switch takes 3, 5, 0x31,
  0x102 and 0x103 only (0x0049b84b..0x0049b904).
- **Voice**: the session object's stream source (+0x64, section 4) is started by Window::onCommand's two
  commands, which call netBeginOp0 / netBeginOp1. Each buffer is sent as type 0x100 to session+0xe4, and the
  release sends type 0x200.
  - For every peer added, netQueueMessage appends a VoiceRx sound object. VoiceRx::ctor 0x004852e0 (sound kind
    6, name `Voice Rx` 0x004e43c8) is called through S3List_addVoiceRx 0x0049b970 on the list session+0xc0 and
    started with slot +0x84 (0x1000, 0, 0).
  - That the source records audio is not shown by the code that is present [U-1752]. No exe code feeds type
    0x100 into a VoiceRx, because the receive dispatch is absent.
  - sound.dll's LPC-10 codec is not called from the exe. Its only reach is through VoiceRx's sound object
    (S4 notes, U-1421).

## 4. The network model the code implies

**Topology.** Peer to peer over DirectPlay, with one host. The session object holds 16 peer slots of 0x58 bytes
at +0x170 (netNameOf 0x0049aa37 / 0x0049aa42; netQueueMessage).
- **Host.** +0x77c is the local DPID (`Local ID`, string 0x004e48c0) and +0x780 is the host's DPID (`Host ID`,
  0x004e48cc). This side is the host when they are equal (netSendList 0x00497b5a, netPoll 0x0049b892).
- **Slot fields.** +0 DPID, +8 ack-expected flag, +0x10 last-heard time (netServiceControl reads slot dword 4), +0x14 flag byte (bit 0 = host, bit 1 =
  wants acks), +0x15 name. Writers: netQueueMessage, netMarkRecordCb, netPoll 0x0049b944.

**Authority.**
- Only the host acts on DPSYS_CREATEPLAYERORGROUP (netPoll 0x0049b886..0x0049b894). It adds the peer and
  re-sends the roster (type 0x10) to everyone (netQueueMessage -> netSendList).
- Clients learn the peer list only from the roster.
- When the host leaves, netHandleControl stores the old host id in 0x008400ac (0x0049b5c7) and makes slot 0's
  id the new host (+0x780 = +0x170, 0x0049b5d7). This is host migration by slot order.
- With +0xe8 bit 18 set, the host times out silent peers: netServiceControl compares now - slot +0x10 with +0xf0
  ms, and netSendShutdown sets +0xf0 to 20000 at 0x00497d22. The host then runs the drop dialogs and
  broadcasts type 0x80 (netHandleControl 0x0049b635). A client times out the host in the same way (slot 0
  +0x180).

**What is sent.**
- Every 2000 ms (0x0049ab69): a keep-alive, type 0x20, to all players.
- On membership change: the roster, from the host only.
- On a drop: the drop notice, type 0x80.
- While a stream is open, each buffer the source yields: type 0x100. With more than 100 pending
  (0x0049aab4..0x0049aabd) the source is flushed and bit 30 of +0xe8 is set until the count falls below 0x32.
  Closing the stream sends type 0x200.
- **No simulation state is sent.** No net function reads or writes a GameState sim object (net_state.tsv
  `gs_class`: engine, pointer, render, ui and const only). Nothing in mainLoop calls the layer. The layer is
  neither lockstep nor state sync: it carries membership, liveness and the stream. Each instance keeps its own
  course.

**Reliability.** netService / netSendMessages take (data, size, dest, type, flags).
- flags bit 0 asks for delivery confirmation.
- With session +0xe8 bit 28 set, DirectPlay's own DPSEND_GUARANTEED does it (section 2).
- Otherwise the Jackal layer does it:
  - it sets bit 2 of the type word (0x0049830c);
  - it clears every slot's ack flag (+0x178, 0x0049830f..0x00498329);
  - EnumGroupPlayers marks the destination's members whose flag bit 1 is set (netMarkRecordCb);
  - it sends to each marked peer and waits on the session's event handles (WaitForMultipleObjects
    0x00498679);
  - it resends until 20000 ms (0x0049876a), then shows NET_SENDTIMEDOUT.
- The acknowledgement packet and the code that would signal those events are not in the image [U-1753].

**Loopback.** A type below 0x10 (`test ebx, 0xfff0` 0x00498129), with session flag bit 0 or caller flag bit 2,
is appended to the local queue +0x120 (listAppend4a4b60 0x00498169 / 0x0049818a). It is sent too when the
destination is 0 and two or more peers exist (0x0049818f).

**Receive.** netPoll pops (buffer, from) pairs from the queue +0x148 (queuePop 0x004a4c70, 0x0049b7df).
- `from` 0 means a DirectPlay system message; anything else is a Jackal packet.
- No instruction in the image pushes to +0x148: the only references are netPoll's pops and netSendShutdown's
  clear (0x00497f20). The receive thread that would call Receive and fill the queue is absent.

## 5. Message formats (net_messages.tsv)

**Jackal packet** (netService 0x0049819c..0x004982ea; netSendMessages has the same code at 0x004992d8..):
- malloc(size + 0xc);
- +0 word: the caller's type, with bit 2 added for a Jackal-acked send;
- +2 word: not written [U-1751];
- +4 dword: sequence number (session +0xf4 incremented, 0x004982b3..0x004982c0) when flags bit 0 is set, else
  0 (0x004982c5);
- +8: the payload (rep movs 0x004982dd..0x004982ea);
- the length passed to Send is size + 0xc, so the 4 bytes after the payload are malloc bytes never written
  [U-1751].

The five application types are 0x10 roster, 0x20 keep-alive, 0x80 peer drop, 0x100 stream data and 0x200
stream end. The five system messages are DPSYS 3 / 5 / 0x31 / 0x102 / 0x103. Layouts, senders, parse sites:
net_messages.tsv. netPoll parses only 0x80 and 0x10 of the application types (0x0049b7f9 / 0x0049b80f).

## 6. State ownership (net_state.tsv)

**The session object owns the layer's state.** It is the heap object `g_netSession` would point to; layout from
the code that is present:

| fields | contents |
|---|---|
| +0x14 / +0x18 | read at entry (netService) |
| +0x20 | event handle (netSendShutdown 0x00497d58) |
| +0x38 | critical section (0x00497d52) |
| +0x54 / +0x58 / +0x5c / +0x60 | onCommand ids and stream arguments; +0x60 = mode |
| +0x64 | stream source object (slots +0x14 +0x20 +0x88 +0x8c +0x90 +0x94) |
| +0xc0 | VoiceRx list |
| +0xe4 | stream destination |
| +0xe8 | flag word: bits 0, 18, 28, 29, 30 used |
| +0xec | last keep-alive |
| +0xf0 | timeout ms |
| +0xf4 | sequence |
| +0x120 | loopback queue |
| +0x148 | receive queue |
| +0x170 | 16 peer slots |
| +0x6f8 | peer count |
| +0x700 | max payload, checked at 0x0049810f |
| +0x77c / +0x780 | local / host DPID |

The object's constructor is not in the image, so its class, size and initial values are not determined
[U-1754].

**Exe globals the layer touches.** 26 addresses. 14 are const strings; the other 12 are below. None is in the
sim class.
- engine: g_netInterface, g_netSession, g_netControl8400bc (both guards), g_net8400ac, g_netBuf83c340,
  g_newObjectHeap. g_newObjectHeap is shared with every heap object, not net-owned.
- pointer: g_netPeerTable 0x0083ff6c.
- ui: the DPNAME buffer 0x0083ff9c (+0xc read). The model types it int32_t[4] (g_ui_83ff9c); the code passes a
  0x100-byte buffer (0x0049b8ad). Proposed correction: S9.1 merge notes.
- ui: g_ui_83ab88, written on onCommand's non-net branch.
- render: g_flag_83afcc, set by the dialogs.

The segment and boot gates compare none of these, and all hold 0 in every recorded image.

## 7. What a two-instance recording would capture, and why it cannot be made with the original

A live session would need g_netSession set, and nothing in the image sets it. Two original instances would show
the title menu and no network option: the menus in mainLoop have no net branch. The only way to exercise the
layer is to construct a session object and an IDirectPlay2A from outside, the way the C3 fixtures do:
  - c3ez / c3fj / c3fi: fixture session at [0x008400b8], recorded interface;
  - CLAUDE.md, round 33.
The calls such a harness would record are the 13 sites of section 2, with:
  - every Send's (from, to, flags, bytes);
  - GetPlayerName's DPNAME;
  - EnumGroupPlayers' callback results;
  - the queue +0x148 contents fed to netPoll;
  - timeGetTime at netService 0x004986c2 / 0x0049870c / 0x00498760 and netServiceControl 0x0049aca4 /
    0x0049acce;
  - WaitForMultipleObjects' answers.
That is a constructed scenario, not a recording of the game: the original never forms a session by itself.

**Firewall** (read only, 2026-10-29, LIVING-PC): all three profiles are BlockInbound with
InboundUserNotification enabled. None of the 604 rules names golf, simgolf, dplay or dplaysvr, and the verbose
listing has no 47624 (the DPAID_INetPort example value in dplobby.h line 792). A program that binds a DirectPlay
TCP/IP host on this PC would get the inbound prompt, and approving it is Mariano's decision. The original never reaches a bind.

## 8. Corrections this survey makes to earlier notes

- re/c3_hold.tsv's reasons for netService / netSendMessages name `wsock32`: the exe imports no socket DLL, and
  the layer's transport calls are the IDirectPlay2A slots of section 2.
- re/roadmap/S9_PLAN.md assumes the original binds sockets and can host / join. It cannot (section 0).
- g_ui_83ff9c is GetPlayerName's DPNAME buffer of 0x100 bytes ending at 0x0084009c, not int32_t[4] (section 6).
- re/names/exe_5.tsv names 0x0049b690 netQueueMessage with the purpose `appends an outgoing message`. Its C4
  Purpose (re/analysis/net/0049b690_netQueueMessage.md) and this survey find that it adds a peer. A rename
  (netAddPeer) is a lead for a names round.

## 9. What a working network mode would need (for the S9 decision)

### 9.1 Missing from the v1.03 image, each with the evidence that it is absent

| piece | evidence it is absent (golf_clean.exe; v1.00 the same where checked) |
|---|---|
| Loading DirectPlay | No DPLAYX.dll import descriptor (section 1). The `DPLAYX.dll` string 0x004bf82e has no reference. The three LoadLibraryA sites load sound.dll, jgld.dll and user32.dll. No CoCreateInstance and no ole32 import. |
| Creating the interface (DirectPlayCreate, QueryInterface for IDirectPlay2A) | IID_IDirectPlay2A 0x004bc3e8 has 0 references. The only store to g_netInterface is 0 (0x00497d78). No call reaches slot 0 (QueryInterface) of the interface (section 2). |
| Choosing a service provider / connection (TCP/IP, modem, serial; InitializeConnection, DPAID_INet address) | The provider GUIDs 0x004bc408 / 0x004bc418 / 0x004bc428 and DPAID_INet 0x004bc3f8 have 0 references. No code builds a DirectPlay address. |
| Session enumerate / create / open (EnumSessions slot +0x34, Open +0x60, a DPSESSIONDESC2), player creation (CreatePlayer +0x18) | None of these slots is called through g_netInterface. The 13 calls are Close, Release, EnumGroupPlayers, Send and GetPlayerName only (net_survey.py interface_calls). |
| The session object (constructor, vtable, events, critical section init) | g_netSession 0x008400b8 has no store (4 reads). No CreateEventA import, so the event handles the senders wait on (0x00498679) cannot be made. No InitializeCriticalSection reference in the net range: all its references (0x004a798a..0x004a9c9d) are in the CRT. [U-1754] |
| Receive thread (Receive slot +0x64, filling the queue +0x148, signalling ack events) | No CreateThread / _beginthread import. No call through slot +0x64. No instruction pushes to +0x148: its only references are netPoll's pops and netSendShutdown's clear (0x00497f20). [U-1753] |
| Acknowledgement packet of the Jackal resend protocol | Not built anywhere. netPoll parses app types 0x80 and 0x10 only (0x0049b7f9 / 0x0049b80f). [U-1753] |
| Parsers for keep-alive 0x20, stream data 0x100, stream end 0x200 | netPoll ignores them (0x0049b811 `je` 0x0049b94b). Nothing feeds stream data into the per-peer VoiceRx objects. |
| Stream source at session +0x64 (voice capture) | Only its virtual slots are called (+0x14 +0x20 +0x88 +0x8c +0x90 +0x94). No constructor and no vtable in the image. [U-1752] |
| Connect / host / join / service / lobby / password dialogs | jackal.txt defines NETCONNECT_*, SELECTSERVICE, DPLAYERROR, NET_LOBBY_*, HOSTOLDERVERSION / CLIENTOLDERVERSION / CLIENTNEWERVERSION and GUARANTEEDFAILURE. None of these ids occurs in the exe, v1.00, jgld.dll or sound.dll (byte search). |
| A menu entry | No `Multiplayer` / `Network` text in the exe (byte search 0 hits). Among the install's text files only jackal.txt has `Multiplayer`, under the sections above. No function outside the layer calls into it except the four gates, which return on NULL (net_functions.tsv `callers`). mainLoop has no call to a net function. |
| Lobby launch (IDirectPlayLobby, GetConnectionSettings) | No lobby IID in the exe (the five DirectPlay GUIDs of section 1 are all it holds). No lobby interface pointer. No DirectPlayLobbyCreate. |
| Game data exchange (any simulation state, turn or command) | No net function reads or writes a sim-class object (net_state.tsv). No net message carries game state (section 4). |

### 9.2 Fixed by the code that IS present (a port or a protocol must keep these to match the original's layer)

- **Packet framing:** type word +0, sequence dword +4 (session +0xf4 + 1, only when flags bit 0 is set), payload
  at +8, Send length = payload + 0xc. The Jackal-acked broadcast sets bit 2 of the type word (0x0049830c).
  Word +2 and the 4 tail bytes are unwritten [U-1751].
- **Send call sites and arguments:** the 9 Send sites with (local DPID +0x77c, destination, flags 0 or
  DPSEND_GUARANTEED, packet, length) and the choice between DirectPlay's guarantee (+0xe8 bit 28) and the Jackal
  ack / resend loop: EnumGroupPlayers with netMarkRecordCb, per-peer resend, 20000 ms limit (0x0049876a),
  NET_SENDTIMEDOUT dialog. Loopback of types below 0x10 into the queue +0x120.
- **Control messages:**
  - roster 0x10: count byte, then 0x28-byte records of id, name[32] and flag dword. The host sends it.
  - keep-alive 0x20: every 2000 ms.
  - peer drop 0x80: 4-byte id, guaranteed.
  - stream 0x100 / 0x200: to session +0xe4, with the 100 / 0x32 pending hysteresis.
  - DirectPlay system messages handled: 3 create (host only, GetPlayerName -> add peer), 5 destroy (drop),
    0x31 session lost (shutdown), 0x102 data (peer flag byte), 0x103 name. Layouts: net_messages.tsv.
- **Session rules:**
  - 16 peer slots of 0x58 bytes. Host = slot flag bit 0. Local / host DPIDs at +0x77c / +0x780.
  - Host migration to slot 0 (0x0049b5c3..0x0049b5d7, old host id to 0x008400ac).
  - Silence timeouts: +0xf0, 20000 by default (0x00497d22). The NET_ADJUSTTOLERANCE dialog sets it to
    5000 / 10000 / 20000 / 30000 (netHandleControl jump tables 0x0049b66c / 0x0049b67c).
  - Drop-confirm dialogs NET_OKTODROPHOST / CLIENT / HOSTS.
- **Slot-15 defect:** netHandleControl removes a peer by moving slots i+1..15 down one, using 0x16 dwords per
  slot (`rep movsd` loop 0x0049b5a1..0x0049b5b0, count 0xf - index at 0x0049b594 / 0x0049b599). Slot 15 is never
  cleared, so it stays a copy of slot 14's new contents: the old slot 15. When slot 15 was occupied, every scan over 16 slots
  (netNameOf, netQueueMessage, netPoll 0x0049b923) can then still find that id. The peer count +0x6f8 is decremented
  (0x0049b5f2). S9's plan keeps this behind a defect switch, default 1.
- **Also present:** the per-peer VoiceRx sound objects (receive side of the stream) and the onCommand
  push-to-talk style start (ids +0x54 / +0x58) and release (netFlush).
