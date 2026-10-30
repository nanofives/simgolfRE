"""Fills the hand column `meaning` of re/port/net_state.tsv (S9.1). Re-run after `net_survey.py write`."""
from pathlib import Path
P = Path(__file__).resolve().parents[3] / 're' / 'port' / 'net_state.tsv'
M = {
 '004c1434': 'string read by S3List_addVoiceRx out-of-heap abort (binary text; not net state)',
 '004c1458': 'string read by S3List_addVoiceRx out-of-heap abort (binary text; not net state)',
 '004c3f70': 'string pool entry addressed by the senders error dialogs (binary text)',
 '004c4944': 'string pool entry addressed by the senders error dialogs (binary text)',
 '004c52b8': 'string pool entry addressed by the senders error dialogs (binary text)',
 '004e4680': 'jgld text-file section prefix the net dialogs load through TextView::loadScript',
 '004e47b0': 'send-failure dialog text (senders)',
 '004e47c8': 'net dialog caption (senders)',
 '004e47e8': 'dialog section id NET_SENDTIMEDOUT (senders)',
 '004e47fc': 'send-failure-after-retries dialog text (netSendMessages)',
 '004e4824': 'dialog section id NET_OKTODROPHOST (netHandleControl)',
 '004e4838': 'dialog section id NET_OKTODROPCLIENT (netHandleControl)',
 '004e484c': 'dialog section id NET_ADJUSTTOLERANCE (netHandleControl)',
 '004e4860': 'dialog section id NET_OKTODROPHOSTS (netHandleControl)',
 '00839650': 'heap word copied into every heap object; the in-frame dialogs and VoiceRx nodes save and restore it (not net-owned)',
 '0083ab88': 'command id stored by Window::onCommand on its non-net branch (0x0047eed0); not net state',
 '0083afcc': 'set to 1 by the net dialogs (netService 0x00498cc4, netSendMessages, netHandleControl); render-class flag shared with the UI',
 '0083c340': 'byte set to 0 when a guaranteed send passes 20000 ms (cmp 0x0049876a, store 0x00498775); its address is also an immediate operand in both senders',
 '0083ff6c': 'peer table pointer: netService stores session+0x170 (0x0049832b / 0x00498331) before EnumGroupPlayers; netMarkRecordCb reads it',
 '0083ff9c': 'DPNAME out-buffer of GetPlayerName (netPoll 0x0049b8bb, size word 0x100 at 0x0049b8ad); ends at 0x0084009c',
 '0083ffa8': 'DPNAME.lpszLongNameA (+0xc) of that buffer, passed to netQueueMessage as the new peer name (0x0049b8d6)',
 '008400ac': 'previous host id: when the removed peer is the host (cmp 0x0049b5c3) netHandleControl stores +0x780 here (0x0049b5c7) and makes slot 0 the host (+0x780 = +0x170, 0x0049b5d7); no reader in the image',
 '008400b0': 'IDirectPlay2A interface (dplay.h slots); only store is 0 (netSendShutdown 0x00497d78, ebx = 0 from 0x00497d20); .bss, so NULL for the whole process',
 '008400b8': 'Jackal net session object; NO store in the image; .bss, so NULL for the whole process; every entry into the net layer tests it',
 '008400bc': 'netServiceControl re-entry guard for the peer timeout scan (set 0x0049abb7, cleared 0x0049ac35 / 0x0049ac85 / 0x0049ace0)',
 '008400c0': 'netServiceControl re-entry guard around the keep-alive send (set 0x0049ab8d, cleared 0x0049aba7)',
}
lines = P.read_text(encoding='utf-8').splitlines()
head = lines[0].split('\t')
out = [lines[0]]
for line in lines[1:]:
    r = line.split('\t')
    r[head.index('meaning')] = M.get(r[0], r[head.index('meaning')])
    out.append('\t'.join(r))
P.write_text('\n'.join(out) + '\n', encoding='utf-8')
print('filled', sum(1 for l in out[1:] if l.split('\t')[-1]), 'of', len(out) - 1)
