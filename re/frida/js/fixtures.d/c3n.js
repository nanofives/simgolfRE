// C3 batch c3n fixtures (2026-10-07): util constructors, virtual-dispatch helpers, rebuildHeightfield. Synthetic
// values only (no game text). Main-menu state.
Object.assign(globalThis.DIFF_FIXTURES, {
  // 12 objects of 0x24 bytes (b0..b11, obj = b0) for Buf_init / Buf_ctor / NodeListB::ctor / NodeListC::ctor. Every
  // byte starts non-zero and distinct per object ((i*0x24 + k) * 7 + 0x11, never 0 mod 256 for these k), so the
  // bytes each constructor leaves alone (Buf_init: +0..+3 and +5..+7) keep a per-object pattern and the zeroed or
  // vtable fields are visible. State region: the whole block ($obj, 0, 12*0x24).
  c3n_bufs() {
    const n = 12, stride = 0x24;
    const obj = Memory.alloc(n * stride);
    const fx = { obj };
    for (let i = 0; i < n * stride; i++) {
      let v = (i * 7 + 0x11) & 0xff;
      if (v === 0) v = 0x5a;
      obj.add(i).writeU8(v);
    }
    for (let i = 0; i < n; i++) fx['b' + i] = obj.add(i * stride);
    return fx;
  },

  // Virtual-dispatch fixture for resourceQuery / resourceLoad / resourceLoadB (two fake vtables):
  //   * thisVt: slot +4 = vt4(self, b) and slot +8 = vt8(self) (thiscall NativeCallbacks);
  //   * innerVt: slot +0x10 = vt10(inner, a, b) and slot +0x74 = q(inner);
  //   * inner objects i0..i3 (+0 innerVt, +4 = value 100, 200, -7, -21);
  //   * 12 outer objects t0..t11 of 0x20 bytes ($obj = t0): +0 thisVt, +4 = id 0x40 + i, +0x14 = inner i % 4 for
  //     t0..t7 and NULL for t8..t11 (resourceQuery's NULL branch; never passed to the loads);
  //   * rec (0x40 bytes): what the callbacks record. Both arms make the same calls with the same inputs, so rec
  //     matches across arms and changes only on the vectors that reach a slot.
  // Callback results: vt4 returns (b ^ 0x55) | 1 for an odd b (non-zero: the early return) and 0 for an even b;
  // vt10 returns 0 when a == 0, else a*3 + inner value (0 for t3/i3 with a = 7: -21 + 21); vt8 returns 0x77
  // (ignored by both loads); q returns inner value ^ 0x1234.
  c3n_res() {
    const rec = Memory.alloc(0x40);
    const thisVt = Memory.alloc(0x20), innerVt = Memory.alloc(0x80);
    const vt4 = new NativeCallback(function (self, b) {
      rec.writeS32(b);
      rec.add(4).writeS32(self.add(4).readS32());
      return (b & 1) ? (((b ^ 0x55) | 1) | 0) : 0;
    }, 'int', ['pointer', 'int'], 'thiscall');
    const vt8 = new NativeCallback(function (self) {
      rec.add(0x14).writeS32(rec.add(0x14).readS32() + 1);
      rec.add(0x18).writeS32(self.add(4).readS32());
      return 0x77;
    }, 'int', ['pointer'], 'thiscall');
    const vt10 = new NativeCallback(function (inner, a, b) {
      rec.add(8).writeS32(a);
      rec.add(0xc).writeS32(b);
      const v = inner.add(4).readS32();
      rec.add(0x10).writeS32(v);
      return a === 0 ? 0 : ((a * 3 + v) | 0);
    }, 'int', ['pointer', 'int', 'int'], 'thiscall');
    const q = new NativeCallback(function (inner) {
      const v = inner.add(4).readS32();
      rec.add(0x1c).writeS32(v);
      rec.add(0x20).writeS32(rec.add(0x20).readS32() + 1);
      return (v ^ 0x1234) | 0;
    }, 'int', ['pointer'], 'thiscall');
    globalThis.__diffKeepAlive.push(vt4, vt8, vt10, q);   // NativeCallbacks are not rewritten to __keepAlloc
    thisVt.add(4).writePointer(vt4);
    thisVt.add(8).writePointer(vt8);
    innerVt.add(0x10).writePointer(vt10);
    innerVt.add(0x74).writePointer(q);
    const inners = [100, 200, -7, -21].map((v) => {
      const o = Memory.alloc(0x10);
      o.writePointer(innerVt);
      o.add(4).writeS32(v);
      return o;
    });
    const outer = Memory.alloc(12 * 0x20);
    const fx = { obj: outer, rec };
    for (let i = 0; i < 12; i++) {
      const t = outer.add(i * 0x20);
      t.writePointer(thisVt);
      t.add(4).writeS32(0x40 + i);
      t.add(0x14).writePointer(i < 8 ? inners[i % 4] : ptr(0));
      fx['t' + i] = t;
    }
    return fx;
  },

  // rebuildHeightfield 0x0042f7a0: the heightBlend world from the base _blend(tick, m2, m4) fixture (golf_tables
  // tile/wall tables, course record bytes, table 0x005a4998, height grid 0x00838c1c, bit 0 of 0x0059e7b8 cleared),
  // then:
  //   * tile types 0x005722e8 in square blocks of side `blk` (types repeat inside a block, so relaxTile and
  //     raiseFromNeighbours find same-type neighbours): type = kinds[(x/blk + 2*(y/blk) + shift) % kinds.length];
  //   * type flags dwords 0x0057837c + t*0x30 and bytes +7 (0x00578377) per type, see FLAGS below (no type has
  //     1|2|4 together, so the lower/raise sweep cannot alternate on one cell);
  //   * tile bytes 0x0056988c = (x/2 + y/3) % 3 (raiseFromNeighbours' sameByte test);
  //   * flat = true sets bit 0 of 0x0059e7b8 (heightBlend then returns 3 everywhere: one sweep, nothing changes).
  // FLAGS: type 1 = 1|2 (relaxTile), 2 = 1|4 with byte +7 = 0x10 (raise, sameByte 1), 3 = 1|4 with byte +7 = 0
  // (raise, sameByte 0), 4 = 2 (level = 4th cornerRange output), 5 = 4 (3rd output), 6 = 8, 7 = 0, 8 = 1,
  // 9 = 2|4 (flag 4's store wins), 0x14 = 0 (blocked), 0x11 = 0 (heightBlend's 3), others 0.
  _c3n_hf(tick, m2, m4, blk, shift, flat) {
    const F = globalThis.DIFF_FIXTURES;
    F._blend(tick, m2, m4);
    const FLAGS = { 1: 3, 2: 5, 3: 5, 4: 2, 5: 4, 6: 8, 7: 0, 8: 1, 9: 6 };
    for (let t = 0; t < 32; t++) {
      ptr('0x0057837c').add(t * 0x30).writeU32(FLAGS[t] || 0);
      ptr('0x00578377').add(t * 0x30).writeU8(t === 2 ? 0x10 : 0);
    }
    const kinds = [1, 2, 3, 4, 5, 1, 6, 2, 7, 3, 8, 9, 0x14, 1, 0x11, 3];
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++) {
        const k = kinds[(Math.floor(x / blk) + 2 * Math.floor(y / blk) + shift) % kinds.length];
        ptr('0x005722e8').add(x * 50 + y).writeU8(k);
        ptr('0x0056988c').add(x * 50 + y).writeU8((Math.floor(x / 2) + Math.floor(y / 3)) % 3);
      }
    const fl = ptr('0x0059e7b8');
    fl.writeU8(flat ? (fl.readU8() | 1) : (fl.readU8() & 0xfe));
    return { obj: ptr('0x005722e8') };
  },
  c3n_hf_c2() { return globalThis.DIFF_FIXTURES._c3n_hf(0, 2, 1, 5, 0, false); },
  c3n_hf_c0() { return globalThis.DIFF_FIXTURES._c3n_hf(0, 0, 0, 4, 3, false); },
  c3n_hf_tick() { return globalThis.DIFF_FIXTURES._c3n_hf(5, 2, 1, 6, 7, false); },
  c3n_hf_flat() { return globalThis.DIFF_FIXTURES._c3n_hf(0, 2, 1, 5, 1, true); },

  // scanTileLine 0x00421fa0: golf_tables tile types ((x*7 + y*3) & 0x1f, so types 0x11 and 0x14 occur), then per type
  // t < 32 the signed byte +2 (0x00578372 + t*0x30) and byte +6 (0x00578376 + t*0x30):
  //   mixed: +2 = (t*5) % 9 - 3 (-3..5), +6 = 0xd when t % 4 == 1;
  //   neg:   +2 = -((t*3) % 7) - 1 (-1..-7, so the score can end below 0 and be clamped), +6 = 0xd only for t == 9.
  // Reads only (0x005783a2 is the one byte it writes; the registry lists it as state).
  _c3n_scan(neg) {
    globalThis.DIFF_FIXTURES.golf_tables();
    for (let t = 0; t < 32; t++) {
      ptr('0x00578372').add(t * 0x30).writeS8(neg ? -((t * 3) % 7) - 1 : (t * 5) % 9 - 3);
      ptr('0x00578376').add(t * 0x30).writeU8(neg ? (t === 9 ? 0xd : 0) : (t % 4 === 1 ? 0xd : 0));
    }
    return { obj: ptr('0x005722e8') };
  },
  c3n_scan_mixed() { return globalThis.DIFF_FIXTURES._c3n_scan(false); },
  c3n_scan_neg() { return globalThis.DIFF_FIXTURES._c3n_scan(true); },
});
