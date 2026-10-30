"""Fills the hand column `role` of re/port/net_functions.tsv (S9.1). Re-run after `net_survey.py write`."""
from pathlib import Path
P = Path(__file__).resolve().parents[3] / 're' / 'port' / 'net_functions.tsv'
R = {
 '0047ee70': 'GATE (vtable slot, no direct caller): command ids session+0x54 / +0x58 (0x0047ee80 / 0x0047ee99) start netBeginOp0 / 1, a release flushes with netFlush; skipped when g_netSession is NULL (0x0047ee7e)',
 '00495d20': 'GATE (slot 13 of the UI event table 0x004ba87c, installer storeUiEventTable): netPoll when g_netSession is set (0x00495d28)',
 '00497b00': 'GATE: called by the six input pumps; tail-jumps to netPump only when g_netSession and g_netInterface are both set (0x00497b08 / 0x00497b11)',
 '00497b20': 'GATE: called by nine UI pump / modal functions; tail-jumps to netServiceControl only when both pointers are set (0x00497b28 / 0x00497b31)',
 '00497b40': 'host only (+0x77c == +0x780, 0x00497b5a): sends the peer roster, message type 0x10 guaranteed (0x00497bfc..0x00497c06)',
 '00497c20': 'starts a stream to destination a in mode 0 (+0x60 = 0 0x00497c4b, +0xe4 = a 0x00497c52, source slot +0x90 0x00497c58, +0xe8 bit 29 0x00497c5e)',
 '00497c70': 'the same as netBeginOp0 in mode 1 (+0x60 = 1)',
 '00497cc0': 'ends the stream: source slot +0x20, then message type 0x200 to +0xe4 (0x00497cfe)',
 '00497d10': 'session teardown: source slot +0x14, resets timers / ids / counts, Close + Release of g_netInterface (0x00497d6a / 0x00497d75), clears it, frees buffers and the queues +0x120 / +0x148',
 '00497f60': 'EnumGroupPlayers callback (pushed at 0x00498442): marks the slot of a player whose flag byte +0x14 has bit 1 (ack expected)',
 '00497fc0': 'sender (data, size, dest, type, flags), ret 0x14; used by netPump in mode 1; Jackal header + own ack / resend loop or DPSEND_GUARANTEED; Send x3',
 '00499140': 'sender with the same arguments; used by every other sender call and by netPump in mode 0; Send x6, retry dialogs',
 '0049aa30': 'peer name by id over the 16 slots session+0x170 (stride 0x58)',
 '0049aa70': 'per-pump stream service: source slot +0x94 pending count (100 / 0x32 hysteresis on +0xe8 bit 30), slot +0x88 next buffer sent as type 0x100 to +0xe4, slot +0x8c releases it',
 '0049ab40': 'every 2000 ms (timeGetTime test at 0x0049ab69): keep-alive type 0x20 broadcast (0x0049ab93); with +0xe8 bit 18, host scans peers for silence past +0xf0 ms and calls netHandleControl',
 '0049acf0': 'peer drop: drop-confirm dialogs (NET_OKTODROP*), compacts the slot table, host migration (0x0049b5c3..0x0049b5d7), type 0x80 guaranteed notice (0x0049b635); keeps the slot-15 duplicate',
 '0049b690': 'adds a peer (name, id, flag byte) to the 16 slots +0x170 or returns if the id is there; flag bit 0 makes it the host (+0x780); count +0x6f8 + 1; the host re-sends the roster (netSendList); session virtual slot +8 (name, id); appends a VoiceRx to the list +0xc0 and starts it (slot +0x84 with 0x1000, 0, 0)',
 '0049b7b0': 'drains the receive queue +0x148: app types 0x80 (drop) and 0x10 (roster); DirectPlay system messages 3 / 5 / 0x31 / 0x102 / 0x103; nothing in the image fills the queue',
 '0049b970': 'appends a VoiceRx sound object (VoiceRx::ctor 0x004852e0, sound kind 6) for a new peer: the receive side of voice',
 '0049bec0': 'removes a node by key from the session list used by netHandleControl',
}
lines = P.read_text(encoding='utf-8').splitlines()
head = lines[0].split('\t')
out = [lines[0]]
for line in lines[1:]:
    r = line.split('\t')
    r[head.index('role')] = R.get(r[0], r[head.index('role')])
    out.append('\t'.join(r))
P.write_text('\n'.join(out) + '\n', encoding='utf-8')
print('filled', sum(1 for l in out[1:] if l.split('\t')[-1]), 'of', len(out) - 1)
