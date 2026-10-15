// Fixtures for C3 batch m36d (flagIsSet 0x0049ef80, flagSet 0x0049eef0). Only our own synthetic values. Names are
// prefixed m36d_. Memory.alloc is rewritten by diff_hook to __keepAlloc (field-only references are not freed).
//
// Object layout: obj+0 holds the vbptr; vbtable+8 = 0x40, so the ListModel base is obj+0x40 (c3x's fixtures used 0,
// which cannot tell a body that ignores the vbtable from the original). At base+0xc0: +0xc8 head, +0xcc cursor,
// +0xd0 count, +0xd4 index; base+0xf0 the mask; base+0xf4 a guard word (0x5a5a5a5a) right after it. Nodes are 0x10
// bytes: +4 id, +8 a decoy pointer to the LAST node (so a body that follows +8 instead of +0xc walks wrongly),
// +0xc next. The state region is obj+0x100..obj+0x138 (cursor, count, index, mask, guard).
//
// Every fixture freezes the first thread (re/frida/js/freeze.js) and then arms the negative-control switch, which a
// normal build does not have (negOn does nothing there).
(function () {
  function zero(p, size) { for (let i = 0; i < size; i += 4) p.add(i).writeU32(0); }
  let frozenNow = false;
  function freezeNow() {
    if (frozenNow) return;
    __freeze.thread();
    frozenNow = true;
    __freeze.atFlush(function () { frozenNow = false; });
    __freeze.negOn();
  }

  const BASE = 0x40;
  function flagObj(ids, count, mask, idx, cursorJunk) {
    freezeNow();
    const obj = Memory.alloc(0x180);
    zero(obj, 0x180);
    const vbt = Memory.alloc(0x10);
    vbt.writeS32(0);
    vbt.add(4).writeS32(0x10);              // vbtable+4: a different offset (a body reading +4 lands on obj+0x10)
    vbt.add(8).writeS32(BASE);              // vbtable+8 = ListModel offset
    vbt.add(0xc).writeS32(0);
    obj.writePointer(vbt);
    const m = obj.add(BASE);
    m.add(0xd4).writeS32(idx);              // index preset (kept when the list has no head)
    m.add(0xcc).writeU32(cursorJunk >>> 0); // cursor preset
    m.add(0xf0).writeU32(mask >>> 0);
    m.add(0xf4).writeU32(0x5a5a5a5a);       // guard after the mask
    m.add(0xd0).writeS32(count);
    if (ids && ids.length) {
      const nodes = ids.map(function (id) { const n = Memory.alloc(0x10); zero(n, 0x10); n.add(4).writeS32(id); return n; });
      for (let k = 0; k < nodes.length; k++) {
        nodes[k].add(8).writePointer(nodes[nodes.length - 1]);
        if (k + 1 < nodes.length) nodes[k].add(0xc).writePointer(nodes[k + 1]);
      }
      m.add(0xc8).writePointer(nodes[0]);
    }
    // the same layout at obj+0x10 (where vbtable+4 points) holds different values, so a wrong base reads RED
    const w = obj.add(0x10);
    w.add(0xd0).writeS32(1);
    w.add(0xf0).writeU32(0xffffffff);
    return { obj: obj };
  }
  function range(a, n) { const r = []; for (let i = 0; i < n; i++) r.push(a + i); return r; }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ids 10, 20, 30, 20 (a duplicate after the first 20) at indices 0..3, count 4; mask 0b1101.
    m36d_flags_pop() { return flagObj([10, 20, 30, 20], 4, 0xd, 7, 0x11111111); },
    // 40 nodes, ids 100..139, count 40; mask 0xa5a5a5a5: indices 32..39 use bits 0..7 (shl counts mod 32).
    m36d_flags_long() { return flagObj(range(100, 40), 40, 0xa5a5a5a5, 3, 0x22222222); },
    // 6 nodes (ids 1..6) but count 3: ids 4..6 are past the count, so the seek stops at index 3.
    m36d_flags_short() { return flagObj([1, 2, 3, 4, 5, 6], 3, 0x0000000a, 0, 0x33333333); },
    // no head: nothing is reset, the preset index 5 (bit 5 of 0x20) and cursor stay.
    m36d_flags_empty() { return flagObj(null, 0, 0x20, 5, 0x44444444); },
    // head present but count 0: index reset to 0, cursor to the head, no node compared.
    m36d_flags_zcount() { return flagObj([10, 20], 0, 0x3, 9, 0x55555555); },
    // head present, count -2: the loop condition 0 < count fails at once (signed jle at 0x0049ef1c / 0x0049efa8).
    m36d_flags_negcount() { return flagObj([10, 20], -2, 0x2, 1, 0x66666666); },
  });
})();
