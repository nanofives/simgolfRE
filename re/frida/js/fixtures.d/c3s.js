// Fixtures for C3 batch c3s (skipToken, scanToken, Widget::setFlagBit0/1, Window::setField26c5a0/2705a0,
// TextView::setSize, findMarkupTokenIndex, EditBox::lineIndexAt, ListModel::setClickHandler, bitTest, bitSet,
// flagToggle49eec0, getFieldValueByKind). Only our own synthetic values (no game text). Names are prefixed c3s_.
// Allocations go through __keepAlloc (diff_hook rewrites Memory.alloc) so blocks referenced only from a field are not
// freed mid-run. Which vector takes which side of every jcc is listed in log/c3/c3s_purpose.md.
Object.assign(globalThis.DIFF_FIXTURES, {
  // skipToken 0x00476d80 / scanToken 0x00476dd0 (text, &len). Sixteen strings s0..s15 in one block (obj) and sixteen
  // length cells l0..l15 in `lens` (the state region), each preset to the count the vector scans.
  c3s_text() {
    const S = [
      ['abc{def', 7], ['ab}c', 4], ['x[y', 3], [']zz', 3], ['ab$', 3], ['k=v', 3], ['a^b{c', 5], ['plain', 5],
      ['plain{', 3], ['{', 0], ['^^x', 3], ['\xfb{', 2], ['ab=c', 0], ['ab=c{', 2], ['q', 1], ['a}b', 2],
    ];
    const b = Memory.alloc(0x200);
    for (let i = 0; i < 0x200; i++) b.add(i).writeU8(0);
    const lens = Memory.alloc(4 * S.length);
    const fx = { obj: b, lens: lens };
    S.forEach(([s, n], i) => {
      const p = b.add(i * 0x20);
      for (let j = 0; j < s.length; j++) p.add(j).writeU8(s.charCodeAt(j) & 0xff);
      lens.add(4 * i).writeS32(n);
      fx['s' + i] = p;
      fx['l' + i] = lens.add(4 * i);
    });
    return fx;
  },

  // Widget::setFlagBit0 0x00491490 / setFlagBit1 0x004914b0 (this, on). Twelve 0x30-byte objects o0..o11 in one
  // block (obj, the state region); the flag dword at +0x24 takes values with bits 0 and 1 in every combination, the
  // sign bit and bits above the low byte.
  c3s_flags() {
    const F = [0, 1, 2, 3, 0xffffffff, 0xfffffffe, 0xfffffffd, 0x80000000, 0x12345678, 0x55, 0xaa, 0x100];
    const b = Memory.alloc(0x30 * F.length);
    for (let i = 0; i < 0x30 * F.length; i++) b.add(i).writeU8((i * 13 + 7) & 0xff);
    const fx = { obj: b };
    F.forEach((v, i) => {
      const o = b.add(i * 0x30);
      o.add(0x24).writeU32(v >>> 0);
      fx['o' + i] = o;
    });
    return fx;
  },

  // Window::setField26c5a0 0x0047ba70 / setField2705a0 0x0047ba90 (this, v). Three children c0..c2 (0x5b0 bytes each,
  // in `obj`, the state region; +0x5a0 preset to 0xc0c0c0c0 + i) and five windows (0x280 bytes each):
  // w0 (+0x26c = c0, +0x270 = c1), w1 (c2, NULL), w2 (NULL, c2), w3 (NULL, NULL), w4 (c1, c0).
  c3s_win() {
    const kids = Memory.alloc(3 * 0x5b0);
    for (let i = 0; i < 3 * 0x5b0; i += 4) kids.add(i).writeU32(0);
    const c = [0, 1, 2].map((i) => {
      const p = kids.add(i * 0x5b0);
      p.add(0x5a0).writeU32(0xc0c0c0c0 + i);
      return p;
    });
    const W = [[c[0], c[1]], [c[2], null], [null, c[2]], [null, null], [c[1], c[0]]];
    const wins = Memory.alloc(W.length * 0x280);
    for (let i = 0; i < W.length * 0x280; i += 4) wins.add(i).writeU32(0);
    const fx = { obj: kids };
    W.forEach(([a, z], i) => {
      const w = wins.add(i * 0x280);
      w.add(0x26c).writePointer(a || ptr(0));
      w.add(0x270).writePointer(z || ptr(0));
      fx['w' + i] = w;
    });
    return fx;
  },

  // TextView::setSize 0x0048e190 (this, w, h). One 0x1fc0-byte object; +0x1fb8 / +0x1fbc (the state region) preset to
  // 0x11111111 / 0x22222222 so a skipped store is visible.
  c3s_textview() {
    const b = Memory.alloc(0x1fc0);
    for (let i = 0; i < 0x1fc0; i += 4) b.add(i).writeU32(0);
    b.add(0x1fb8).writeU32(0x11111111);
    b.add(0x1fbc).writeU32(0x22222222);
    return { obj: b, tv: b };
  },

  // findMarkupTokenIndex 0x004a4ad0 (this, key). Three 0x1d80-byte objects, table of 0x18-byte entries at +0x580;
  // every entry's dwords 1..5 hold key + 0x1000 (never searched). t1: keys 100 + 3i for i < 40, entry 40 = -1, the
  // entries after it 500. t2: all 256 entries 1000 + i, no terminator. t3: entry 0 = -1, the rest 100.
  c3s_tokens() {
    const mk = (keyOf) => {
      const o = Memory.alloc(0x1d80);
      for (let i = 0; i < 0x580; i += 4) o.add(i).writeU32(0);
      for (let i = 0; i < 0x100; i++) {
        const e = o.add(0x580 + i * 0x18);
        const k = keyOf(i);
        e.writeS32(k);
        for (let j = 1; j < 6; j++) e.add(4 * j).writeS32(k + 0x1000);
      }
      return o;
    };
    const t1 = mk((i) => (i < 40 ? 100 + 3 * i : i === 40 ? -1 : 500));
    const t2 = mk((i) => 1000 + i);
    const t3 = mk((i) => (i === 0 ? -1 : 100));
    return { obj: t1, t1: t1, t2: t2, t3: t3 };
  },

  // EditBox::lineIndexAt 0x00486330 (this, pos, starts, count). Edit boxes e0 (+0x574 = 10), e1 (0) and e2
  // (0xfffffff0, so base + pos wraps); start arrays A = [0, 5, 20, 40, 41, 100, 0x80000000, 0xffffffff] (unsigned
  // values above 0x7fffffff) and B = [50, 60]. Read-only.
  c3s_lines() {
    const mk = (base) => {
      const o = Memory.alloc(0x580);
      for (let i = 0; i < 0x580; i += 4) o.add(i).writeU32(0x5a5a5a5a);
      o.add(0x574).writeU32(base >>> 0);
      return o;
    };
    const arr = (vals) => {
      const p = Memory.alloc(4 * vals.length + 4);
      vals.forEach((v, i) => p.add(4 * i).writeU32(v >>> 0));
      p.add(4 * vals.length).writeU32(0);
      return p;
    };
    const e0 = mk(10);
    return {
      obj: e0, e0: e0, e1: mk(0), e2: mk(0xfffffff0),
      A: arr([0, 5, 20, 40, 41, 100, 0x80000000, 0xffffffff]), B: arr([50, 60]),
    };
  },

  // ListModel::setClickHandler 0x00489ab0 (this, target, a, b). Twelve 0x80-byte models m0..m11 in one block (obj,
  // the state region; +0x74/+0x78/+0x7c preset to distinct values); targets hOn (+4 = 1), hNeg (+4 = -1) and hOff
  // (+4 = 0).
  c3s_click() {
    const b = Memory.alloc(12 * 0x80);
    for (let i = 0; i < 12 * 0x80; i += 4) b.add(i).writeU32(0);
    const fx = { obj: b };
    for (let i = 0; i < 12; i++) {
      const m = b.add(i * 0x80);
      m.add(0x74).writeU32(0xaaaa0000 + i);
      m.add(0x78).writeU32(0xbbbb0000 + i);
      m.add(0x7c).writeU32(0xcccc0000 + i);
      fx['m' + i] = m;
    }
    const tgt = (v) => {
      const t = Memory.alloc(0x10);
      t.writeU32(0x7a7a7a7a); t.add(4).writeS32(v); t.add(8).writeU32(0); t.add(12).writeU32(0);
      return t;
    };
    fx.hOn = tgt(1); fx.hNeg = tgt(-1); fx.hOff = tgt(0);
    return fx;
  },

  // bitTest 0x0049f030, bitSet 0x0049eff0, flagToggle49eec0 0x0049eec0 (this, n[, on]). Four 0x200-byte objects
  // o0..o3 in one block (obj, the state region). Each object's first dword points to its own table whose dword at +8
  // is the virtual-base offset: 0x10, 0, 0x40, 0x20; the flag dword at object + offset + 0xf0 is 0x0000a5a5,
  // 0x80000001, 0xffffffff, 0 respectively. The rest of each object is filled with 0x33 bytes.
  c3s_bits() {
    const OFF = [0x10, 0, 0x40, 0x20];
    const FL = [0x0000a5a5, 0x80000001, 0xffffffff, 0];
    const b = Memory.alloc(4 * 0x200);
    for (let i = 0; i < 4 * 0x200; i++) b.add(i).writeU8(0x33);
    const fx = { obj: b };
    OFF.forEach((off, i) => {
      const o = b.add(i * 0x200);
      const t = Memory.alloc(0x10);
      t.writeU32(0); t.add(4).writeU32(0); t.add(8).writeU32(off); t.add(12).writeU32(0);
      o.writePointer(t);
      o.add(off + 0xf0).writeU32(FL[i] >>> 0);
      fx['o' + i] = o;
    });
    return fx;
  },

  // getFieldValueByKind 0x004a1370 (this). Twenty-one 0x220-byte objects k0..k20 in one block (obj); +0x1f4 = kind
  // (0..17, then -1, 0x7fffffff, 0x80000000); +4 points to a table whose dword at +8 is the virtual-base offset (0 for
  // even i, 0x20 for odd i); the dword at object + offset + 0xd4 = 0x1000 + i and at +0x118 = 0x2000 + i. Read-only.
  c3s_kind() {
    const KIND = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, -1, 0x7fffffff, 0x80000000 | 0];
    const tabs = [0, 0x20].map((off) => {
      const t = Memory.alloc(0x10);
      t.writeU32(0); t.add(4).writeU32(0); t.add(8).writeU32(off); t.add(12).writeU32(0);
      return t;
    });
    const b = Memory.alloc(KIND.length * 0x220);
    for (let i = 0; i < KIND.length * 0x220; i += 4) b.add(i).writeU32(0x44444444);
    const fx = { obj: b };
    KIND.forEach((k, i) => {
      const o = b.add(i * 0x220);
      o.writeU32(0);
      o.add(4).writePointer(tabs[i & 1]);
      o.add(0x1f4).writeS32(k);
      o.add((i & 1 ? 0x20 : 0) + 0xd4).writeU32(0x1000 + i);
      o.add(0x118).writeU32(0x2000 + i);
      fx['k' + i] = o;
    });
    return fx;
  },
});
