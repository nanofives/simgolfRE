// Batch c3au fixtures (sound.dll: DirectSound buffer geometry, sequencer list helpers, the bit (de)serialiser,
// two x87 leaves). Every block is allocated here: no live sound object, device, list or DirectSound buffer is
// ever handed to an arm. Layouts come from the disassembly cited in shim/src/re/c3au.cpp. All values synthetic.
Object.assign(globalThis.DIFF_FIXTURES, {
  // --- 0x10035770 Channel_findStreamNode: 12 channels. A channel carries the list head at +0x1348, the walk
  // cursor at +0x1350 (pre-set non-zero so the first store is visible) and the position at +0x16c. A node is
  // {next +4, record +8}; a record is {value +0, key +4}. The walk accepts a record while key <= position
  // (unsigned) and returns the last accepted record's value, -1 if none.
  // c0 no head; c1 head with a null record; c2 one accepted record; c3 three accepted; c4 three where the
  // second key is above the position; c5 first key above the position; c6 key exactly equal to the position;
  // c7 a second node whose record pointer is null; c8 position 0 and key 0; c9 position 0x7fffffff against the
  // key 0xffffffff (the compare is unsigned, so it is rejected); c10 position 0xffffffff accepts four records;
  // c11 one accepted record followed by a null successor.
  c3au_streamchan() {
    const arena = __keepAlloc(64 * 8);        // 64 records of 8 bytes
    let nrec = 0;
    const rec = (value, key) => {
      const r = arena.add(nrec++ * 8);
      r.writeU32(value >>> 0); r.add(4).writeU32(key >>> 0);
      return r;
    };
    const chain = (specs) => {                // specs: [key or null] -> node list, records with value 0x100+i
      const nodes = specs.map(() => __keepAlloc(0x10));
      nodes.forEach((n, i) => {
        n.add(4).writePointer(i + 1 < nodes.length ? nodes[i + 1] : ptr(0));
        n.add(8).writePointer(specs[i] === null ? ptr(0) : rec(0x1000 + i, specs[i]));
      });
      return nodes[0];
    };
    const mk = (pos, specs) => {
      const o = __keepAlloc(0x1400);
      for (let a = 0x160; a < 0x180; a += 4) o.add(a).writeU32((0x30000000 + a) >>> 0);
      o.add(0x16c).writeU32(pos >>> 0);
      o.add(0x1340).writeU32(0x31313131);
      o.add(0x1344).writeU32(0x32323232);
      o.add(0x1348).writePointer(specs === null ? ptr(0) : chain(specs));
      o.add(0x134c).writeU32(0x33333333);
      o.add(0x1350).writeU32(0x34343434);
      o.add(0x1354).writeU32(0x35353535);
      return o;
    };
    return {
      c0: mk(100, null),
      c1: mk(100, [null]),
      c2: mk(100, [50]),
      c3: mk(100, [10, 50, 90]),
      c4: mk(100, [10, 500, 90]),
      c5: mk(100, [500, 10]),
      c6: mk(100, [100, 101]),
      c7: mk(100, [10, null, 20]),
      c8: mk(0, [0, 1]),
      c9: mk(0x7fffffff, [0xffffffff, 1]),
      c10: mk(0xffffffff, [1, 2, 3, 4]),
      c11: mk(100, [7]),
    };
  },

  // --- 0x10038330 Channel_findVoiceByTag: four channels, each with 0x10 voice records (flag byte at
  // 0x2d4 + i*0x110, tag dword at 0x31c + i*0x110) and a search mask in bits 12..27 of +0x58.
  // v_all has every mask bit set and 16 different tags, with bit 3 of the flag byte set on voices 2 and 5 (so
  // those are skipped even when their tag matches); v_even has only the even mask bits set; v_none has the
  // mask clear (nothing is ever accepted); v_dup has the same tag on voices 1, 4 and 9 so the first accepted
  // one wins. Bits 28..31 of +0x58 are set in every channel, so the `and eax, 0xffff` must drop them.
  c3au_voicetags() {
    const mk = (mask, tags, blocked) => {
      const o = __keepAlloc(0x1400);
      for (let i = 0x2d0; i < 0x12e8; i++) o.add(i).writeU8((i * 13) & 0xff);
      o.add(0x58).writeU32((((mask & 0xffff) << 12) | 0xf00000ab) >>> 0);
      tags.forEach((t, i) => {
        o.add(0x2d4 + i * 0x110).writeU8(blocked.indexOf(i) >= 0 ? 0x0f : 0x07);   // bit 3 set = skipped
        o.add(0x31c + i * 0x110).writeU32(t >>> 0);
      });
      return o;
    };
    const tags = [0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f];
    const dup = [0x40, 0x41, 0x42, 0x43, 0x41, 0x45, 0x46, 0x47,
                 0x48, 0x41, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f];
    return {
      v_all: mk(0xffff, tags, [2, 5]),
      v_even: mk(0x5555, tags, []),
      v_none: mk(0x0000, tags, []),
      v_dup: mk(0xfffe, dup, [1]),
    };
  },

  // --- 0x1001e230 Track_resetReadState: 12 tracks. +0x4c carries the flag word whose bit 1 survives the reset,
  // +0x17c the value copied to +0x178, +0x180 the signed value clamped into [-0x40, 0x3f]. The clamp cases are
  // t0/t6 below -0x40, t1/t7 exactly -0x40, t2/t8 in range, t3/t9 exactly 0x3f, t4/t10/t11 above 0x3f; bit 1 of
  // +0x4c is set in t1, t3, t5, t7, t9 and t11 and clear in the others.
  c3au_tracks() {
    const cfg = [[0xfffffffd, -0x41], [0x00000002, -0x40], [0xaaaaaaaa, 0], [0x00000003, 0x3f],
                 [0x00000000, 0x40], [0x00000002, 1], [0x12345678, -0x7fffffff], [0xffffffff, -100],
                 [0x55555551, -1], [0x00000002, 0x3f], [0x00000000, 0x7fffffff], [0x00000002, 0x1000]];
    const out = {};
    cfg.forEach((c, i) => {
      const o = __keepAlloc(0x200);
      for (let a = 0x40; a < 0x60; a += 4) o.add(a).writeU32((0x40000000 + a + i) >>> 0);
      for (let a = 0x170; a < 0x190; a += 4) o.add(a).writeU32((0x41000000 + a + i) >>> 0);
      o.add(0x4c).writeU32(c[0] >>> 0);
      o.add(0x17c).writeU32((0x42000000 + i) >>> 0);
      o.add(0x180).writeS32(c[1]);
      out['t' + i] = o;
    });
    return out;
  },

  // --- 0x100116a0 seekListToIndex: four lists. A list is {head +4, cursor +8, count +0x148}; a node's
  // successor is at +0x10. l_empty has count 0; l_busy has a non-null cursor (the third guard); l5 is a chain
  // of five nodes with a null cursor; l1 a single node. The walk leaves the cursor on the node at the index.
  c3au_seeklists() {
    const chain = (n) => {
      const nodes = [];
      for (let i = 0; i < n; i++) {
        const p = __keepAlloc(0x20);
        p.writeU32((0x50000000 + i) >>> 0);
        nodes.push(p);
      }
      nodes.forEach((p, i) => p.add(0x10).writePointer(i + 1 < nodes.length ? nodes[i + 1] : ptr(0)));
      return nodes[0];
    };
    const mk = (count, cursor, n) => {
      const o = __keepAlloc(0x200);
      o.writeU32(0x51515151);
      o.add(4).writePointer(n ? chain(n) : ptr(0));
      o.add(8).writePointer(cursor ? ptr(0x52525252) : ptr(0));
      o.add(0xc).writeU32(0x53535353);
      o.add(0x140).writeU32(0x54545454);
      o.add(0x144).writeU32(0x55555555);
      o.add(0x148).writeU32(count >>> 0);
      o.add(0x14c).writeU32(0x56565656);
      return o;
    };
    return { l_empty: mk(0, false, 0), l_busy: mk(5, true, 5), l5: mk(5, false, 5), l1: mk(1, false, 1) };
  },

  // --- 0x1002ad10 Sound_setSourceType: two objects with different pre-fills of +0x44 (the flag word is only
  // ever ored into, so a start value with the bits already set and one with them clear are both exercised) and
  // +0x54.
  c3au_sources() {
    const mk = (f44, f54) => {
      const o = __keepAlloc(0x80);
      for (let a = 0x40; a < 0x60; a += 4) o.add(a).writeU32((0x60000000 + a) >>> 0);
      o.add(0x44).writeU32(f44 >>> 0);
      o.add(0x54).writeU32(f54 >>> 0);
      return o;
    };
    return { s_clear: mk(0x00000000, 0x61616161), s_set: mk(0xffffffff, 0x62626262) };
  },

  // --- 0x1001d780 Seq_listGetAt: six sequences plus one out slot. A sequence holds the list head at +0x19c and
  // the cursor at +0x1a4 (pre-set non-zero); a node is {next +4, payload +8}. q_none has no head; q_nopay a
  // head whose payload is null; q1/q3/q5 are chains of one, three and five payload-carrying nodes; q_hole is a
  // five-node chain whose third node has a null payload, which ends the walk early.
  c3au_getat() {
    const pay = __keepAlloc(32 * 0x10);
    let np = 0;
    const payload = () => { const p = pay.add(np * 0x10); p.writeU32((0x70000000 + np) >>> 0); np++; return p; };
    const chain = (flags) => {                 // flags[i] = true -> that node carries a payload
      const nodes = flags.map(() => __keepAlloc(0x10));
      nodes.forEach((n, i) => {
        n.add(4).writePointer(i + 1 < nodes.length ? nodes[i + 1] : ptr(0));
        n.add(8).writePointer(flags[i] ? payload() : ptr(0));
      });
      return nodes[0];
    };
    const mk = (flags) => {
      const o = __keepAlloc(0x240);
      for (let a = 0x190; a < 0x1b0; a += 4) o.add(a).writeU32((0x71000000 + a) >>> 0);
      o.add(0x19c).writePointer(flags === null ? ptr(0) : chain(flags));
      o.add(0x1a4).writeU32(0x72727272);
      return o;
    };
    const out = __keepAlloc(0x10);
    out.writeU32(0x73737373);
    out.add(4).writeU32(0x74747474);
    return {
      out,
      q_none: mk(null),
      q_nopay: mk([false]),
      q1: mk([true]),
      q3: mk([true, true, true]),
      q5: mk([true, true, true, true, true]),
      q_hole: mk([true, true, false, true, true]),
    };
  },

  // --- 0x10022280 Seq_copyChannelParams: one destination object whose 0x4c-byte slots start at +0x264, and
  // eight source records. A source's index at +0x28 picks the slot, so p0/p1/p2/p7 land on slots 0, 1, 2 and 7
  // and p_same writes slot 1 a second time with different values. Each source is filled with its own pattern
  // over 0 .. 0x3f so the copied dwords, the word at +0x18 (the byte pair at +0x1a must stay) and the
  // untouched offset 0 of the slot are all compared.
  c3au_chanparams() {
    const dst = __keepAlloc(0x600);
    for (let a = 0x260; a < 0x5a0; a += 4) dst.add(a).writeU32((0x80000000 + a) >>> 0);
    const mk = (index, seed) => {
      const o = __keepAlloc(0x40);
      for (let a = 0; a < 0x38; a += 4) o.add(a).writeU32((seed + a) >>> 0);
      o.add(0x28).writeU32(index >>> 0);
      return o;
    };
    return {
      dst,
      p0: mk(0, 0x81000000), p1: mk(1, 0x82000000), p2: mk(2, 0x83000000),
      p3: mk(3, 0x84000000), p7: mk(7, 0x85000000), p_same: mk(1, 0x86000000),
      p_hi: mk(0xa, 0x87000000), p_zero: mk(0, 0x00000000),
      p4: mk(4, 0x88000000), p5: mk(5, 0x89000000),
    };
  },

  // --- 0x10023c70 Seq_cmdHandler11: six sequences. A sequence holds the guard at +0x224, the list head at
  // +0x218 and the cursor at +0x220; a node is {next +4, record +8}; a record is {key +4, value +0x30}.
  // k_guard has a zero guard; k_none no head; k_norec a head with a null record; k3 three records with keys
  // 10, 20 and 30; k_hole a three-node chain whose middle record is null; k_dup two records with key 20 (the
  // first one wins).
  c3au_cmd11() {
    const arena = __keepAlloc(32 * 0x40);
    let nr = 0;
    const record = (key) => {
      const r = arena.add(nr * 0x40);
      for (let a = 0; a < 0x40; a += 4) r.add(a).writeU32((0x90000000 + nr * 0x100 + a) >>> 0);
      r.add(4).writeU32(key >>> 0);
      nr++;
      return r;
    };
    const chain = (keys) => {
      const nodes = keys.map(() => __keepAlloc(0x10));
      nodes.forEach((n, i) => {
        n.add(4).writePointer(i + 1 < nodes.length ? nodes[i + 1] : ptr(0));
        n.add(8).writePointer(keys[i] === null ? ptr(0) : record(keys[i]));
      });
      return nodes[0];
    };
    const mk = (guard, keys) => {
      const o = __keepAlloc(0x240);
      for (let a = 0x210; a < 0x230; a += 4) o.add(a).writeU32((0x91000000 + a) >>> 0);
      o.add(0x224).writeU32(guard >>> 0);
      o.add(0x218).writePointer(keys === null ? ptr(0) : chain(keys));
      o.add(0x220).writeU32(0x92929292);
      return o;
    };
    return {
      arena,
      k_guard: mk(0, [10]),
      k_none: mk(1, null),
      k_norec: mk(1, [null]),
      k3: mk(1, [10, 20, 30]),
      k_hole: mk(1, [10, null, 30]),
      k_dup: mk(1, [20, 20]),
    };
  },

  // --- 0x10037e10 MmioBuffer_init: six objects pre-filled over 0 .. 0xbf, three 0x48-byte blocks and three
  // 0x14-byte format records (only their first 0x12 bytes are read). Passing 0 as the block must leave bytes
  // 0 .. 0x47 of the object alone.
  c3au_mmio() {
    const out = {};
    for (let i = 0; i < 6; i++) {
      const o = __keepAlloc(0x100);
      for (let a = 0; a < 0xc0; a++) o.add(a).writeU8(((a + i * 29) & 0xff) | 1);
      out['m' + i] = o;
    }
    for (let i = 0; i < 3; i++) {
      const b = __keepAlloc(0x48);
      for (let a = 0; a < 0x48; a += 4) b.add(a).writeU32((0xa0000000 + i * 0x10000 + a) >>> 0);
      out['blk' + i] = b;
      const f = __keepAlloc(0x20);
      for (let a = 0; a < 0x14; a += 4) f.add(a).writeU32((0xb0000000 + i * 0x10000 + a) >>> 0);
      out['fmt' + i] = f;
    }
    return out;
  },

  // --- 0x10010eb0 DsBuffer::setRegionMs: four buffers. Fields: buffer size +0x78, byte rate +0xac, 16-bit
  // block alignment +0xb4. b_cd is 44100*4 bytes/s with alignment 4 and a 0x20000-byte buffer; b_small has a
  // tiny buffer so most durations are rejected; b_align1 has alignment 1; b_big has a byte rate whose product
  // with a large duration wraps the 32-bit multiply.
  c3au_dsregions() {
    const mk = (size, rate, align) => {
      const o = __keepAlloc(0x100);
      for (let a = 0x30; a < 0x50; a += 4) o.add(a).writeU32((0xc0000000 + a) >>> 0);
      o.add(0x78).writeU32(size >>> 0);
      o.add(0xac).writeU32(rate >>> 0);
      o.add(0xb4).writeU16(align & 0xffff);
      o.add(0xb6).writeU16(0xc1c1);
      return o;
    };
    return { b_cd: mk(0x20000, 176400, 4), b_small: mk(0x400, 176400, 4),
             b_align1: mk(0x20000, 8000, 1), b_big: mk(0xffffffff, 0x40000000, 2) };
  },

  // --- 0x1003e380 floatCopySign: one arena of floats, written as raw IEEE-754 bit patterns so every case is
  // exact: both zeros, +-1, +-0.1, +-1/3, +-FLT_MAX, +-the smallest denormal, +-infinity and a quiet NaN of
  // each sign (an SNaN is deliberately left out: `fld dword` of one would quiet it inside the original).
  c3au_floats() {
    const bits = [0x00000000, 0x80000000,     // f0 +0.0   f1 -0.0
                  0x3f800000, 0xbf800000,     // f2 +1.0   f3 -1.0
                  0x3dcccccd, 0xbdcccccd,     // f4 +0.1   f5 -0.1
                  0x3eaaaaab, 0xbeaaaaab,     // f6 +1/3   f7 -1/3
                  0x7f7fffff, 0xff7fffff,     // f8 +FLT_MAX  f9 -FLT_MAX
                  0x00000001, 0x80000001,     // f10 smallest denormal, both signs
                  0x7f800000, 0xff800000,     // f12 +inf  f13 -inf
                  0x7fc00000, 0xffc00000];    // f14 +QNaN f15 -QNaN
    const arena = __keepAlloc(0x100);
    const out = { arena };
    bits.forEach((v, i) => { arena.add(i * 4).writeU32(v >>> 0); out['f' + i] = arena.add(i * 4); });
    return out;
  },

  // --- 0x1003c180 bitPack / 0x1003c1d0 bitUnpack: six independent records. Each record owns a count slot
  // (1..9 values, which keeps bitPermute's local array inside its frame and the mask table index inside the
  // 10-entry table at 0x10064454), two scalar slots, a value array, a 54-dword bit array and a 0x2550-byte
  // context block whose dword at +0x2540 is the toggled parity bit. The pack records (n0..n5) start with a bit
  // array full of a filler pattern that the serialiser must overwrite; the unpack records (u0..u5) start with
  // a bit array of 0/1 values the deserialiser reads, and their scalar and value slots are what it writes.
  c3au_bits() {
    const out = {};
    const cfg = [[1, [0x1234]],
                 [2, [0x7fff, 0x0001]],
                 [3, [0, 0x4000, 0x7fff]],
                 [5, [1, 2, 3, 4, 5]],
                 [9, [0x7fff, 0x5555, 0x2aaa, 0x1111, 0x0f0f, 0x00ff, 0x0001, 0x7ffe, 0x3333]],
                 [9, [0, 0, 0, 0, 0, 0, 0, 0, 0]]];
    cfg.forEach((c, i) => {
      const n = c[0], values = c[1];
      const cnt = __keepAlloc(0x10); cnt.writeU32(n);
      const a = __keepAlloc(0x10); a.writeU32((0x1000 + i) >>> 0);
      const b = __keepAlloc(0x10); b.writeU32((0x2000 + i * 3) >>> 0);
      const val = __keepAlloc(0x40);
      values.forEach((v, k) => val.add(k * 4).writeU32(v >>> 0));
      const bits = __keepAlloc(0x100);
      for (let k = 0; k < 54; k++) bits.add(k * 4).writeU32(0xdddd0000 + k);
      const ctx = __keepAlloc(0x2560);
      ctx.add(0x2540).writeU32(i & 1);
      out['cnt' + i] = cnt; out['a' + i] = a; out['b' + i] = b;
      out['val' + i] = val; out['bits' + i] = bits; out['ctx' + i] = ctx;
      // the unpack side: the same shapes with a readable bit pattern and scratch outputs
      const ucnt = __keepAlloc(0x10); ucnt.writeU32(n);
      const ua = __keepAlloc(0x10); ua.writeU32(0xeeee0000);
      const ub = __keepAlloc(0x10); ub.writeU32(0xeeee1111);
      const uval = __keepAlloc(0x40);
      for (let k = 0; k < 10; k++) uval.add(k * 4).writeU32((0xeeee2000 + k) >>> 0);
      const ubits = __keepAlloc(0x100);
      for (let k = 0; k < 54; k++) ubits.add(k * 4).writeU32((k * 7 + i) % 3 === 0 ? 1 : 0);
      out['ucnt' + i] = ucnt; out['ua' + i] = ua; out['ub' + i] = ub;
      out['uval' + i] = uval; out['ubits' + i] = ubits;
    });
    return out;
  },

  // --- 0x100383b0 Voice_setLevel: four voice records with different pan values at +0x48, which is what
  // Voice_setPanGains (0x10037f30) is called with: w_zero has pan 0 (the callee's early path that copies the
  // level double to both gain pairs), w_pos 0x20 and w_neg -0x20 (its two signed arms), w_clamp 0x7fffffff
  // (clamped to 0x3f). The gain fields at +0x68 .. +0x87 and +0x50, +0x58 start with their own patterns.
  c3au_voicelevels() {
    const mk = (pan) => {
      const o = __keepAlloc(0x100);
      for (let a = 0x40; a < 0x90; a += 4) o.add(a).writeU32((0xd0000000 + a) >>> 0);
      o.add(0x48).writeS32(pan);
      return o;
    };
    return { w_zero: mk(0), w_pos: mk(0x20), w_neg: mk(-0x20), w_clamp: mk(0x7fffffff) };
  },

  // --- 0x10006bd0 computeCaptureDuration: 12 objects. Fields: total +0xb4, result +0xc0, quotient +0xc8,
  // span +0xd4, scale +0xdc, 16-bit unit +0xe4. d0 has a zero total and d1 a zero span (the two guards); the
  // other ten divide exactly or leave a remainder whose ratio to the scale is not exact in binary
  // (1/3, 1/7, 1/1000), including a negative span so the signed division truncates toward zero.
  c3au_capture() {
    const cfg = [[0, 1000, 48000, 4],            // d0: zero total -> returns at once
                 [192000, 0, 48000, 4],          // d1: zero span
                 [192000, 1000, 48000, 4],
                 [192000, 1001, 48000, 4],
                 [192000, 100000, 3, 4],
                 [192000, -1001, 48000, 4],
                 [44100, 7, 44100, 1],
                 [44100, 22051, 44100, 2],
                 [96000, 1, 1000, 4],
                 [96000, 0x7fffffff, 48000, 4],
                 [176400, 123456, 7, 4],
                 [8000, 3, 3, 1]];
    const out = {};
    cfg.forEach((c, i) => {
      const o = __keepAlloc(0x120);
      for (let a = 0xb0; a < 0xf0; a += 4) o.add(a).writeU32((0xe0000000 + a + i) >>> 0);
      o.add(0xb4).writeU32(c[0] >>> 0);
      o.add(0xd4).writeS32(c[1]);
      o.add(0xdc).writeU32(c[2] >>> 0);
      o.add(0xe4).writeU16(c[3] & 0xffff);
      o.add(0xe6).writeU16(0xe1e1);
      out['d' + i] = o;
    });
    return out;
  },

  // --- 0x1003d040 codecApplyFixedCoeffs: six sample buffers of 16 floats with their own count slot and
  // four-float filter state. s0 has a count of 0 and is the one passed with a NULL sample pointer (the
  // filter's loop starts at 1, so nothing is read or written through the 4 that the wrapper passes instead);
  // the others filter 1, 4, 8 and 16 samples. The sample values span small, large and negative magnitudes so
  // the filter's output differs per buffer.
  c3au_codec() {
    const out = {};
    const counts = [0, 1, 4, 8, 16, 16];
    counts.forEach((n, i) => {
      const buf = __keepAlloc(0x80);
      for (let k = 0; k < 16; k++) buf.add(k * 4).writeFloat((k + 1) * (i % 2 ? -1 : 1) * (i === 5 ? 1e5 : 0.1));
      const cnt = __keepAlloc(0x10); cnt.writeU32(n);
      const st = __keepAlloc(0x20);
      st.writeFloat(0.25 * (i + 1));
      st.add(4).writeFloat(-0.5 * (i + 1));
      st.add(8).writeFloat(1.0 / (i + 3));
      st.add(0xc).writeFloat(0.0);
      out['buf' + i] = buf; out['cnt' + i] = cnt; out['st' + i] = st;
    });
    return out;
  },
});
