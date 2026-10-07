// Batch c3ar fixtures (sound.dll MIDI sequencer / voice-table helpers). Every block is allocated here (never a
// live sound object, device, list or surface): the hooked functions only read and write the records these fixtures
// build. Layouts come from the disassembly cited in shim/src/re/c3ar.cpp. All values are synthetic.
Object.assign(globalThis.DIFF_FIXTURES, {
  // --- 0x1001e710 Midi_readVarLen: one byte arena holding 15 variable-length encodings 8 bytes apart (the longest
  // is 5 bytes), plus one out-counter slot pre-filled with a non-zero value so the zeroing at 0x1001e71b is
  // visible. e0..e3 are single bytes, e4..e7 two, e8..e10 three, e11/e12 four and e13 the five-byte case whose
  // fifth group shifts the first one out of the 32-bit accumulator. e14 is a two-byte encoding of 0x53c.
  c3ar_varlen() {
    const enc = [[0x00], [0x01], [0x40], [0x7f],
                 [0x80, 0x00], [0x81, 0x00], [0xc0, 0x01], [0xff, 0x7f],
                 [0x81, 0x80, 0x00], [0xaa, 0xd5, 0x2a], [0xff, 0xff, 0x7f],
                 [0x81, 0x80, 0x80, 0x00], [0xff, 0xff, 0xff, 0x7f],
                 [0xff, 0xff, 0xff, 0xff, 0x7f], [0x8a, 0x3c]];
    const buf = Memory.alloc(0x100);
    for (let i = 0; i < 0x100; i++) buf.add(i).writeU8(0x5a);   // filler: a byte with bit 7 clear ends any walk
    const out = { buf };
    enc.forEach((bytes, i) => {
      const p = buf.add(i * 8);
      bytes.forEach((b, k) => p.add(k).writeU8(b));
      out['e' + i] = p;
    });
    const cnt = Memory.alloc(0x10);
    cnt.writeU32(0xdeadbeef);
    cnt.add(4).writeU32(0xfeedface);
    out.cnt = cnt;
    return out;
  },

  // --- 0x1001e9f0 Track_advancePos: three tracks. t0 accepts, lands exactly on the limit and overshoots depending
  // on the delta; t1 has position == limit == 0 (every non-zero delta is rejected); t2 is near 2^32 so the add
  // wraps. +0x34 starts with bit 1 clear in t0/t2 and set in t1.
  c3ar_tracks() {
    const mk = (pos, limit, acc, flags) => {
      const o = Memory.alloc(0x60);
      o.add(0x28).writeU32(0x01020304);
      o.add(0x2c).writeU32(acc >>> 0);
      o.add(0x30).writeU32(0x05060708);
      o.add(0x34).writeU32(flags >>> 0);
      o.add(0x38).writeU32(0x090a0b0c);
      o.add(0x3c).writeU32(0x0d0e0f10);
      o.add(0x40).writeU32(pos >>> 0);
      o.add(0x44).writeU32(limit >>> 0);
      return o;
    };
    return { t0: mk(0x100, 0x200, 0x1000, 0x11110000),
             t1: mk(0, 0, 0xffffffff, 0x00000002),
             t2: mk(0xfffffff0, 0xffffffff, 0x7fffffff, 0x00000000) };
  },

  // --- 0x10037760 Voice_setLoopFlag: one channel. The 16 voice flag bytes at 0x2d4 + i*0x110 get different start
  // values (bit 1 and bit 2 set and clear), and the whole snapshotted window is filled with a byte pattern so a
  // write at a wrong stride is visible.
  c3ar_voicechan() {
    const flags = [0x00, 0x02, 0x04, 0x06, 0xff, 0xfd, 0xfb, 0xf9,
                   0x55, 0xaa, 0x01, 0x03, 0x80, 0x7f, 0x06, 0xf8];
    const ch = Memory.alloc(0x1400);
    for (let i = 0x2d0; i < 0x12e8; i++) ch.add(i).writeU8((i * 7) & 0xff);
    flags.forEach((f, i) => ch.add(0x2d4 + i * 0x110).writeU8(f));
    return { ch };
  },

  // --- 0x100377f0 Channel_broadcastVoiceBit1: four channels. on1/on2 have bit 0 of +0x58 set (the 16-voice loop
  // runs and +0x58 is left alone), off1/off2 have it clear (only bit 7 of +0x58 changes); off1 starts with bit 7
  // of +0x58 SET, so an argument with bit 0 clear shows the `and al, 0x7f` actually clearing it, and off2 starts
  // with bit 7 clear. Voice flag bytes and the dwords around +0x58 are pre-filled.
  c3ar_bcast() {
    const mk = (f58) => {
      const o = Memory.alloc(0x1400);
      for (let i = 0x2d0; i < 0x12e8; i++) o.add(i).writeU8((i * 11) & 0xff);
      [0x00, 0x02, 0x04, 0x06, 0xff, 0xfd, 0xfb, 0xf9, 0x55, 0xaa, 0x01, 0x03, 0x80, 0x7f, 0x06, 0xf8]
        .forEach((v, i) => o.add(0x2d4 + i * 0x110).writeU8(v));
      o.add(0x50).writeU32(0x11223344);
      o.add(0x54).writeU32(0x55667788);
      o.add(0x58).writeU32(f58 >>> 0);
      o.add(0x5c).writeU32(0x99aabbcc);
      o.add(0x60).writeU32(0xddeeff00);
      return o;
    };
    return { on1: mk(0xabcdef01), on2: mk(0x00000001), off1: mk(0xffffff80), off2: mk(0x12345670) };
  },

  // --- 0x100382c0 Channel_allocVoiceSlot: 14 channels. +0x58 is (mask << 12) | 0xabc | 0xf0000000, so bits 12..27
  // hold the mask and bits 28..31 are set in every one (they must be dropped by the `and eax, 0xffff`). The masks
  // give trailing-one counts 0, 1, 2, 3, 4, 5, 8, 12, 13, 15, 16, 0, 1 and 0. The out counter starts non-zero.
  c3ar_allocslot() {
    const masks = [0x0000, 0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x00ff, 0x0fff,
                   0x1fff, 0x7fff, 0xffff, 0xaaaa, 0x5555, 0xfffe];
    const out = { out: Memory.alloc(0x10) };
    out.out.writeU32(0xdeadbeef);
    out.out.add(4).writeU32(0x5ca1ab1e);
    masks.forEach((m, i) => {
      const o = Memory.alloc(0x1400);
      o.add(0x50).writeU32(0x13131313);
      o.add(0x54).writeU32(0x14141414);
      o.add(0x58).writeU32(((m << 12) | 0xabc | 0xf0000000) >>> 0);
      o.add(0x5c).writeU32(0x15151515);
      out['c' + i] = o;
    });
    return out;
  },

  // --- 0x1001ae00 Seq_setPlayRange: two sequences, one with every bit of +0x214 set except bit 0 and one with it
  // all clear, so the `or esi, 1` is observable both ways. The whole 0x200..0x21f window is pre-filled.
  c3ar_seqrange() {
    const mk = (f214) => {
      const o = Memory.alloc(0x240);
      for (let i = 0x200; i < 0x220; i += 4) o.add(i).writeU32((0x77770000 + i) >>> 0);
      o.add(0x214).writeU32(f214 >>> 0);
      return o;
    };
    return { s0: mk(0xfffffffe), s1: mk(0x00000000) };
  },

  // --- 0x1001adc0 Midi_advanceMsgRing: 12 rings, (index +0x20c, end +0x204, start +0x208). r2/r4/r5/r9/r10 pass
  // the end and reload the start; r3 and r8 hold 0xffffffff, whose increment wraps to 0 and stays below the end.
  c3ar_msgring() {
    const cfg = [[0, 10, 0], [9, 10, 0], [10, 10, 3], [0xffffffff, 10, 7],
                 [5, 0, 2], [0, 0, 0], [0xfffffffe, 0xffffffff, 1], [0, 0xffffffff, 9],
                 [0xffffffff, 0xffffffff, 4], [3, 3, 0xdead], [200, 200, 50], [100, 200, 50]];
    const out = {};
    cfg.forEach((c, i) => {
      const o = Memory.alloc(0x240);
      o.add(0x200).writeU32((0x60000000 + i) >>> 0);
      o.add(0x204).writeU32(c[1] >>> 0);
      o.add(0x208).writeU32(c[2] >>> 0);
      o.add(0x20c).writeU32(c[0] >>> 0);
      o.add(0x210).writeU32((0x61000000 + i) >>> 0);
      o.add(0x214).writeU32((0x62000000 + i) >>> 0);
      out['r' + i] = o;
    });
    return out;
  },

  // --- 0x1001ab60 Sound_initEntry: 12 entries, each pre-filled byte by byte with its own non-zero pattern over
  // 0 .. 0x22f, which covers the zeroed range (0 .. 0x217), the dword set to 0xfa at 0x218 and a guard at 0x21c.
  c3ar_entries() {
    const out = {};
    for (let b = 0; b < 12; b++) {
      const o = Memory.alloc(0x240);
      for (let i = 0; i < 0x230; i++) o.add(i).writeU8(((i + b * 37) & 0xff) | 1);   // never 0
      out['e' + b] = o;
    }
    return out;
  },

  // --- 0x10025010 NodeList_initNode: two nodes with different pre-fills (the two link dwords must end zero and
  // the payload must hold the argument).
  c3ar_nodes() {
    const mk = (seed) => {
      const o = Memory.alloc(0x20);
      for (let i = 0; i < 4; i++) o.add(i * 4).writeU32((seed + i) >>> 0);
      return o;
    };
    return { n0: mk(0x31310001), n1: mk(0xfffffff0) };
  },

  // --- 0x10008810 pairStore: two objects with different pre-fills; only the first two dwords may change.
  c3ar_pairs() {
    const mk = (seed) => {
      const o = Memory.alloc(0x20);
      for (let i = 0; i < 4; i++) o.add(i * 4).writeU32((seed + i * 0x111) >>> 0);
      return o;
    };
    return { p0: mk(0x41410000), p1: mk(0xcafe0000) };
  },

  // --- 0x1000fec0 DsBuffer::hasAuxInterfaces: 12 objects. b0/b8/b10 have both pointer fields zero (result 0); the
  // rest have one or both non-zero, including 0x80000000 so the test cannot be a sign test. The values are never
  // dereferenced by the function.
  c3ar_dsbuf() {
    const pairs = [[0, 0], [1, 0], [0, 1], [1, 1], [0xffffffff, 0], [0, 0xffffffff],
                   [0x80000000, 0], [0, 0x80000000], [0, 0], [0x12345678, 0x9abcdef0],
                   [0, 0], [0xdeadbeef, 0]];
    const out = {};
    pairs.forEach((p, i) => {
      const o = Memory.alloc(0x80);
      o.add(0x60).writeU32((0x70000000 + i) >>> 0);
      o.add(0x64).writeU32(p[0] >>> 0);
      o.add(0x68).writeU32(p[1] >>> 0);
      o.add(0x6c).writeU32((0x71000000 + i) >>> 0);
      out['b' + i] = o;
    });
    return out;
  },

  // --- 0x100381f0 Voice_noteOff: 12 voice records, each with a (double at +0x58, double at +0xf0) pair written as
  // raw dwords so the exact bit patterns are controlled: +0.0, -0.0, a quiet NaN (0x7ff8000000000000), the smallest
  // denormal (0x0000000000000001), 1.0, -1.0, 1.5, 2.5, 3.0 and 0x7fe0000000000000. v3/v8/v9/v10 are the only ones
  // with both doubles non-zero and ordered. +0x94, +0x9c and +0xe4 start non-zero and differ per record.
  c3ar_voices() {
    //          [hi58, lo58, hif0, lof0]
    const vals = [[0x00000000, 0, 0x00000000, 0],   // v0  +0.0, +0.0
                  [0x00000000, 0, 0x3ff80000, 0],   // v1  +0.0, 1.5
                  [0x3ff80000, 0, 0x00000000, 0],   // v2  1.5, +0.0
                  [0x3ff80000, 0, 0x40040000, 0],   // v3  1.5, 2.5      -> bit 3 of +0x9c
                  [0x80000000, 0, 0x40080000, 0],   // v4  -0.0, 3.0
                  [0x40080000, 0, 0x80000000, 0],   // v5  3.0, -0.0
                  [0x7ff80000, 0, 0x3ff00000, 0],   // v6  NaN, 1.0      -> unordered, C3 set
                  [0x3ff00000, 0, 0x7ff80000, 0],   // v7  1.0, NaN
                  [0x00000000, 1, 0x7fe00000, 0],   // v8  denormal, huge -> bit 3
                  [0xbff00000, 0, 0xbff00000, 0],   // v9  -1.0, -1.0     -> bit 3
                  [0x00000000, 1, 0x3ff00000, 0],   // v10 denormal, 1.0  -> bit 3
                  [0x3ff00000, 0, 0x00000000, 0]];  // v11 1.0, +0.0
    const b94 = [0xff, 0x01, 0xfe, 0x00, 0x55, 0xaa, 0x7f, 0x80, 0x11, 0x22, 0x33, 0x44];
    const b9c = [0x00, 0x01, 0x10, 0xf3, 0xff, 0x08, 0x04, 0x0c, 0x55, 0xaa, 0x20, 0x40];
    const out = {};
    vals.forEach((v, i) => {
      const o = Memory.alloc(0x120);
      o.add(0x50).writeU32((0x80000000 + i) >>> 0);
      o.add(0x54).writeU32((0x81000000 + i) >>> 0);
      o.add(0x58).writeU32(v[1] >>> 0);
      o.add(0x5c).writeU32(v[0] >>> 0);
      o.add(0x90).writeU32((0x82000000 + i) >>> 0);
      o.add(0x94).writeU8(b94[i]);
      o.add(0x98).writeU32((0x83000000 + i) >>> 0);
      o.add(0x9c).writeU8(b9c[i]);
      o.add(0xe0).writeU32((0x84000000 + i) >>> 0);
      o.add(0xe4).writeU32((0x85000000 + i) >>> 0);
      o.add(0xe8).writeU32((0x86000000 + i) >>> 0);
      o.add(0xf0).writeU32(v[3] >>> 0);
      o.add(0xf4).writeU32(v[2] >>> 0);
      out['v' + i] = o;
    });
    return out;
  },

  // --- 0x100285a0 nextRandomFloat: 12 LCG states (the dword at offset 0 of the object), including 0, 1, the
  // multiplier and the increment themselves, and both signed extremes.
  c3ar_rngf() {
    const seeds = [0, 1, 2, 0x12345678, 0xdeadbeef, 0xffffffff, 0x7fffffff, 0x80000000,
                   0x0019660d, 0x3c6ef35f, 0xa5a5a5a5, 0x5a5a5a5a];
    const out = {};
    seeds.forEach((s, i) => {
      const o = Memory.alloc(0x10);
      o.writeU32(s >>> 0);
      o.add(4).writeU32((0x90000000 + i) >>> 0);
      out['r' + i] = o;
    });
    return out;
  },

  // --- 0x1001e1c0 Seq_rewind: 12 sequences over one arena of 16 track records (0x60 bytes each). A sequence hangs
  // a chain of nodes (successor at +4, track record at +8) at +0x19c, with the cursor at +0x1a4 pre-set non-zero
  // and +0x4c pre-filled so the `and al, 0x63` is observable. EVERY track record has a null list head at +0xc,
  // which keeps Seq_rewindTracks (0x1001e8e0) and its callee FUN_1001bc00 on their null-list paths, so no virtual
  // call is ever made through a fixture object; +0x1c is 7 (not 5) for the same reason.
  // Shapes: q0 no head; q1/q10 a first node whose track pointer is null; q4/q6 a null track inside the chain;
  // q2/q8 one record, q3/q9 two, q5 three, q7 five. q11 is the aliasing case: its single node points at
  // seq+0x154 as the track record, so Seq_rewindTracks' store at track+0x50 lands on the sequence's own cursor
  // at +0x1a4 and zeroes it, and Seq_rewind's re-read (je at 0x1001e1e9) ends the walk.
  c3ar_seqs() {
    const tracks = Memory.alloc(20 * 0x60);      // the chains below use 17 records
    let next = 0;
    const fill = (t, k) => {
      t.add(0x0c).writePointer(ptr(0));                       // null list head: no virtual call below
      t.add(0x14).writeU32((0xaaaa0000 + k) >>> 0);
      t.add(0x1c).writeU32(7);
      t.add(0x2c).writeU32((0xbbbb0000 + k) >>> 0);
      t.add(0x30).writeU32((0xcccc0000 + k) >>> 0);
      t.add(0x34).writeU32(((k & 1) ? 0xdddd0001 : 0xdddd0003) >>> 0);
      t.add(0x38).writeU32(((k & 1) ? 0xeeee00f0 : 0xeeee00ff) >>> 0);
      t.add(0x3c).writeU32((0x11110000 + k) >>> 0);
      t.add(0x40).writeU32((0x22220000 + k) >>> 0);
      t.add(0x48).writeU32((0x33330000 + k) >>> 0);
      t.add(0x4c).writeU32((0x44440000 + k) >>> 0);
      t.add(0x50).writeU32((0x55550000 + k) >>> 0);
      t.add(0x54).writeU32((0x66660000 + k) >>> 0);
      t.add(0x58).writeU32((0x77770000 + k) >>> 0);
      t.add(0x5c).writeU32((0x88880000 + k) >>> 0);
      return t;
    };
    const track = () => { const k = next++; return fill(tracks.add(k * 0x60), k); };
    // one entry per node: true = the node carries a track record, false = its track pointer is null
    const shapes = [null, [false], [true], [true, true], [true, false], [true, true, true],
                    [true, true, false], [true, true, true, true, true], [true], [true, true],
                    [false], 'alias'];
    const f4c = [0xffffffff, 0x000000ff, 0xabcdef9c, 0x00000063, 0x0000009c, 0xffffff00,
                 0x12345678, 0x80000080, 0x0000ffff, 0x55555555, 0xaaaaaaaa, 0x0f0f0f0f];
    const out = { tracks };
    shapes.forEach((shape, i) => {
      const seq = Memory.alloc(0x240);
      for (let a = 0x40; a < 0x60; a += 4) seq.add(a).writeU32((0x50000000 + a + i) >>> 0);
      for (let a = 0x140; a < 0x1c0; a += 4) seq.add(a).writeU32((0x51000000 + a + i) >>> 0);
      seq.add(0x4c).writeU32(f4c[i] >>> 0);
      seq.add(0x1a4).writeU32((0x99990000 + i) >>> 0);
      if (shape === null) {
        seq.add(0x19c).writePointer(ptr(0));
      } else if (shape === 'alias') {
        // the track record IS the sequence at +0x154: track+0x0c = seq+0x160 (the null list head the callee
        // needs), track+0x1c = seq+0x170, track+0x50 = seq+0x1a4 (the cursor the callee zeroes)
        seq.add(0x160).writePointer(ptr(0));
        seq.add(0x170).writeU32(7);
        const n = Memory.alloc(0x20);
        n.add(4).writePointer(ptr(0));
        n.add(8).writePointer(seq.add(0x154));
        seq.add(0x19c).writePointer(n);
      } else {
        const nodes = shape.map(() => Memory.alloc(0x20));
        nodes.forEach((n, k) => {
          n.add(4).writePointer(k + 1 < nodes.length ? nodes[k + 1] : ptr(0));
          n.add(8).writePointer(shape[k] ? track() : ptr(0));
        });
        seq.add(0x19c).writePointer(nodes[0]);
      }
      out['q' + i] = seq;
    });
    return out;
  },
});
