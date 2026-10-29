"""S10.11 ledger: one row per unnamed retail body reached through a paired slot (11_slotpool.tsv), with its verdict.
Usage: ledger.py <slotpool tsv> <slotfeat tsv> <names tsv> <out tsv>"""
import sys, csv, collections

HAND = {
    0x10005c70: 'stores 1 into byte this+0x19 (m_flags.state in the demo layout) at 0x10005c70: nothing ties the value to a refresh',
    0x1000ece0: 'stores 0 into this+0x38 (m_time_past) at 0x1000ece0: the verb update_sound has no support',
    0x10031a30: 'tail-jumps its own slot +0x14 (0x10031a32), which the demo record names unload: verb stop not supported',
    0x1002e060: 'forwards to slot +0xc8 of this+0x40 (0x1002e06e); every paired table holds this same forwarder in that slot',
    0x1002c1c0: 'forwards to slot +0xc0 of this+0x40; every paired table holds this same forwarder in that slot',
    0x1002c1f0: 'forwards to slot +0xc4 of this+0x40; every paired table holds this same forwarder in that slot',
    0x1002b870: 'forwards to slot +0xd0 of this+0x40; every paired table holds this same forwarder in that slot',
    0x1002b8a0: 'forwards to slot +0xd4 of this+0x40; every paired table holds this same forwarder in that slot',
    0x1002a4b0: 'forwards to slot +0x64 of this+0x14; the Dll_Wave_In_Device table holds this same forwarder there',
    0x1002a4e0: 'forwards to slot +0x68 of this+0x14; the target 0x1000ebe0 is an unnamed stub',
    0x1001c340: 'this-adjusting (-0x2c) forwarder to slot +4 of the primary table (0x1001c34b), i.e. to 0x1001bcf0, itself unnamed',
    0x10029420: '183-byte body not reviewed in depth (walks the global list at 0x100b5070)',
    0x1001bcf0: 'calls slot +0x10 (do_control) of every list control of type 3..5 (0x1001bd1e): the verb start is not tied',
    0x1001e860: 'calls slot +0x10 (do_control) of every list control of type 3..5 (0x1001e88f): the verb start is not tied',
    0x1003a440: '213-byte body not reviewed in depth',
}


def main(sp, sft, names, out):
    acc = {int(r['addr'], 16): r['name'] for r in csv.DictReader(open(names, encoding='utf-8'), delimiter='\t')}
    agree = {}
    for r in csv.DictReader(open(sft), delimiter='\t'):
        agree.setdefault(int(r['retail_body'], 16), r['agree'])
    tally = collections.Counter()
    with open(out, 'w', newline='') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(['retail_va', 'size', 'kind', 'agree', 'demo_names', 'verdict', 'name', 'reason'])
        for r in csv.DictReader(open(sp), delimiter='\t'):
            va = int(r['retail_va'], 16)
            if va in acc:
                v, why = 'ACCEPT', 'see re/names/demo_sound_1.tsv'
            elif va in HAND:
                v, why = 'REJECT_HAND', HAND[va]
            elif r['kind'] in ('CONST', 'EMPTY') or agree.get(va) == 'EMPTY_BOTH':
                v, why = 'REJECT_STUB', '%s body (%s): no instruction supports the verb (S10.9 stub rule)' % (r['kind'], r['detail'] or 'no field, no import')
            else:
                v, why = 'REJECT_HAND', 'not reviewed'
            tally[v] += 1
            w.writerow([r['retail_va'], r['size'], r['kind'], agree.get(va, '-'), r['demo_names'], v, acc.get(va, '-'), why])
    print(dict(tally))


if __name__ == '__main__':
    main(*sys.argv[1:5])
