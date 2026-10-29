# S9: network play (opened 2026-10-29)

Mariano started this section on 2026-10-29. Decision D6 had held it back as `later`; DECISIONS_PENDING.md G12 records the change.

## What we know already, all from earlier notes

- **The net layer.** golf_clean.exe's Jackal network layer is the subsystem `net`, 0x497b40-0x49bec0. re/port/scope.tsv puts 17
  functions in port module `net`.
- **At C3.** netPump, netSendList, netQueueMessage, netServiceControl and netHandleControl are C3 (c3ez, c3fj). Their dialog is
  declared through framemask.js.
- **Held at C2.** netService 0x497fc0 and netSendMessages 0x499140 are held (re/c3_hold.tsv): an SEH frame, alloca, an in-frame
  dialog, COM and wsock32. netPoll and netSendShutdown are C3 (c3fi).
- **Network voice.** It uses sound.dll's LPC-10 codec (VoiceRx, the S4.10g hold, U-1421). Today that codec is reached only on this
  path.
- **Reading references.** third_party/smsg/SMSG-decomp has the Jackal library's Network.cpp (golf/jackal.lib) and the matched
  sources S10 is importing. Their claims follow the S10 evidence rule.
- **Known defect.** netHandleControl leaves slot 15 duplicated when it removes a peer (round 34 finding).

## Goals, in order

1. **Survey.**
   - Which transport the game uses: wsock32 calls, any DirectPlay or COM objects, ports, broadcast or discovery.
   - Which game modes and screens reach it: host / join, the session lobby, the championship, chat and voice.
   - Who owns each piece of state.
   Write re/analysis/net/NET_MODEL.md from our disassembly, with citations.
2. **Recordings.**
   - Run two ORIGINAL instances on this PC, a host and a client, each on its own instance under re/tools/instances.py. This needs 2
     game slots: ask the orchestrator with NEED_GAME.
   - Record the session with the scenario lockstep tools: every packet, the socket calls, and per-frame state on both sides.
   - Pin the recordings in the corpus, like the segment corpus.
3. **Port.**
   - Module `net` in port/src/sim behind a NetHost interface: Winsock on Windows; a POSIX sockets host later, with no third-party library.
   - Model the protocol and the session state.
   - Make the app able to host and join.
4. **Gates.**
   - (a) Every recorded session replays in the port with equal per-frame sim hashes on both sides.
   - (b) INTEROP: a port instance and an ORIGINAL instance play a session together; packets are byte-equal to an original-original
     recording of the same script, or every difference is explained.
   - (c) Segments 205/205 and boot rows unchanged.
   - (d) Each ported function judged by its C3 path-1 rows, as test_bridge.py does.

## Limits

- No change to original\.
- No change to C3 / C4 hooks.
- Port defaults of defects follow D5 / G3. The slot-15 duplicate is kept unless Mariano decides otherwise.
- Firewall prompts: the original binds sockets. If Windows asks for a firewall rule, stop and report BLOCKED_DECISION, because only
  Mariano can approve it.
