// Batch c3an fixtures (sound.dll leaves). Every block is allocated here (never a live sound object, device or
// list): the hooked functions only read and write the records these fixtures build. Layouts come from the
// disassembly cited in shim/src/re/c3an.cpp. All values are synthetic.
Object.assign(globalThis.DIFF_FIXTURES, {
  // --- 0x1000b030 getSpanLength: 14 objects with a (start +8, end +0xc) pair each, including equal, reversed,
  // negative and extreme pairs so the subtraction and the +1 are exercised across the whole int range.
  c3an_spans() {
    const pairs = [[0, 0], [0, 9], [5, 5], [10, 3], [-1, -1], [-8, 7], [1, 1000], [1000, 1],
                   [0, 0x7fffffff], [-0x80000000, 0], [0x7fffffff, -0x80000000], [-0x80000000, 0x7fffffff],
                   [0x1234, 0x5678], [-5, -9]];
    const out = {};
    pairs.forEach((p, i) => {
      const o = Memory.alloc(0x20);
      o.add(8).writeS32(p[0]);
      o.add(0xc).writeS32(p[1]);
      out['s' + i] = o;
    });
    return out;
  },

  // --- 0x100088c0 CommandQueue::at: one array of 0x1000 dwords starting at the object itself, each slot holding a
  // distinct value so a wrong index is visible in the return value.
  c3an_cmdq() {
    const q = Memory.alloc(0x4000);
    for (let i = 0; i < 0x1000; i++) q.add(i * 4).writeU32((0x3b000000 + i * 0x101) >>> 0);
    return { q };
  },

  // --- 0x10008860 CommandQueue::init: 12 queues of 0x1000 dwords + the two fields at 0x4000/0x4004, each with a
  // different non-zero pre-fill in the three snapshotted windows (head, middle, tail) and a guard at 0x4008 that
  // the function must leave alone.
  c3an_qinit() {
    const out = {};
    for (let b = 0; b < 12; b++) {
      const o = Memory.alloc(0x4020);
      const seed = 0x1000 + b * 0x37;
      for (let i = 0; i < 0x10; i++) o.add(i * 4).writeU32((seed + i) >>> 0);                   // 0x0000..0x003f
      for (let i = 0; i < 0x10; i++) o.add(0x2000 + i * 4).writeU32((seed ^ (i * 0x711)) >>> 0); // 0x2000..0x203f
      for (let i = 0; i < 0x18; i++) o.add(0x3fc0 + i * 4).writeU32((seed + 0x5a5a + i) >>> 0);  // 0x3fc0..0x401f
      out['q' + b] = o;
    }
    return out;
  },

  // --- 0x1000c3d0 WaveCmdQueue::pushValue: four rings. Two can accept a value (write index 3, and 0xfff so the
  // next index wraps to 0) and two are full (read index == write index + 1, once in the middle and once at the
  // wrap). Ring slots 0..15 and 0xff8..0xfff are pre-filled, and 0x41e0 is a guard.
  c3an_waveq() {
    const mk = (head, tail, seed) => {
      const o = Memory.alloc(0x4200);
      for (let i = 0; i < 16; i++) o.add(0x1d8 + i * 4).writeU32((seed + i) >>> 0);
      for (let i = 0xff8; i < 0x1000; i++) o.add(0x1d8 + i * 4).writeU32((seed + i) >>> 0);
      o.add(0x41d8).writeU32(tail);
      o.add(0x41dc).writeU32(head);
      o.add(0x41e0).writeU32(0xfeedface);
      o.add(0x41e4).writeU32(0x5ca1ab1e);
      return o;
    };
    return { mid: mk(3, 0x800, 0x40000000), wrap: mk(0xfff, 0x800, 0x41000000),
             full: mk(5, 6, 0x42000000), fullwrap: mk(0xfff, 0, 0x43000000) };
  },

  // --- 0x10028560 randRange: three LCG states (the dword at offset 0 of the object).
  c3an_rng() {
    const mk = (s) => { const o = Memory.alloc(0x10); o.writeU32(s >>> 0); return o; };
    return { r0: mk(0), r1: mk(1), r2: mk(0x12345678) };
  },

  // --- 0x1001bb80 Seq_firstTrackData: 12 objects, 8 with a head node at +0xc (distinct payloads at node+8) and 4
  // with a null head; every cursor at +0x14 starts non-zero so the store is visible in both paths.
  c3an_seqhead() {
    const out = {};
    for (let i = 0; i < 12; i++) {
      const o = Memory.alloc(0x20);
      o.add(0x14).writeU32((0xc0000000 + i) >>> 0);
      if (i < 8) {
        const n = Memory.alloc(0x20);
        n.add(4).writePointer(ptr(0));
        n.add(8).writeU32((0x11110000 + i * 0x2223) >>> 0);
        o.add(0xc).writePointer(n);
      } else {
        o.add(0xc).writePointer(ptr(0));
      }
      out['q' + i] = o;
    }
    return out;
  },

  // --- 0x1000f0c0 DsBuffer::setBufferSize: three objects, one with the guard dword at +0x60 clear (the store at
  // +0x78 runs) and two with it set (0x1 and 0x80000000, both return 0xc).
  c3an_dsbuf() {
    const mk = (guard) => {
      const o = Memory.alloc(0x90);
      o.add(0x60).writeU32(guard >>> 0);
      o.add(0x78).writeU32(0xa5a50000);
      o.add(0x7c).writeU32(0xdeadbeef);
      return o;
    };
    return { open: mk(0), busy: mk(1), busy2: mk(0x80000000) };
  },

  // --- 0x10024c50 Voice_setPlayModeBits: two objects with different values at +0x214 (all bits set, and only the
  // three mode bits clear) so the and/or masks are visible.
  c3an_voice() {
    const mk = (v) => {
      const o = Memory.alloc(0x230);
      o.add(0x210).writeU32(0x11223344);
      o.add(0x214).writeU32(v >>> 0);
      o.add(0x218).writeU32(0x55667788);
      return o;
    };
    return { full: mk(0xffffffff), clear: mk(0x000000ff) };
  },

  // --- 0x1002ceb0 VoiceSlot_init: 12 slots of 0x110 bytes, pre-filled byte by byte with a per-slot pattern, plus
  // a guard at 0x110 that the 0x41-dword store must not reach.
  c3an_slots() {
    const out = {};
    for (let b = 0; b < 12; b++) {
      const o = Memory.alloc(0x120);
      for (let i = 0; i < 0x48; i++) o.add(i * 4).writeU32((0x70000000 + b * 0x1000 + i) >>> 0);
      out['v' + b] = o;
    }
    return out;
  },

  // --- 0x10037720 Channel_setStreamSource: two channels with different values at +0x58 (bit 2 set and clear) and
  // at the +0x13a0 / +0x13a4 pair, with a guard at +0x13a8.
  c3an_chansrc() {
    const mk = (flags) => {
      const o = Memory.alloc(0x13e0);
      o.add(0x54).writeU32(0x01020304);
      o.add(0x58).writeU32(flags >>> 0);
      o.add(0x5c).writeU32(0x09080706);
      o.add(0x13a0).writeU32(0xaaaa0001);
      o.add(0x13a4).writeU32(0xaaaa0002);
      o.add(0x13a8).writeU32(0xbbbb0003);
      o.add(0x13ac).writeU32(0xbbbb0004);
      return o;
    };
    return { set: mk(0xffffffff), clear: mk(0x12345671) };
  },

  // --- 0x10038400 Channel_clearBackref: 12 channels, 7 pointing at a slot of a 16-dword target block (each slot
  // non-zero, so the store is visible) and 5 with a null back pointer.
  c3an_chanback() {
    const targets = Memory.alloc(0x40);
    for (let i = 0; i < 16; i++) targets.add(i * 4).writeU32((0x33330000 + i) >>> 0);
    const out = { targets };
    for (let i = 0; i < 12; i++) {
      const o = Memory.alloc(0x13e0);
      o.add(0x13a0).writeU32((0x44440000 + i) >>> 0);
      o.add(0x13a4).writePointer(i < 7 ? targets.add(i * 4) : ptr(0));
      o.add(0x13a8).writeU32((0x55550000 + i) >>> 0);
      out['c' + i] = o;
    }
    return out;
  },

  // --- 0x1001f150 Seq_clearTrackFlag8All: 12 sequences over one arena of 40 track records (0x40 bytes each, flag
  // byte at +0x38). A sequence is a chain of nodes (successor at +4, track record at +8) hung at +0x19c, with the
  // cursor at +0x1a4 pre-set to a non-zero value. The shapes cover a null head, a null track on the first node, a
  // null track in the middle of the chain, and chains of 1, 2, 3, 4, 5 and 8 nodes with different flag bytes.
  c3an_tracklist() {
    const tracks = Memory.alloc(40 * 0x40);
    let next = 0;
    const track = (flag) => { const t = tracks.add(next++ * 0x40); t.add(0x38).writeU8(flag); return t; };
    // one entry per node: a flag byte for a track record, or null for a node without one
    const shapes = [null,                       // q0: no head at all
                    [null],                     // q1: first node has no track record
                    [0xff],                     // q2
                    [0x08, 0x00],               // q3
                    [0xf7, 0xff, 0x55],         // q4
                    [0x81, null, 0x7e],         // q5: the walk must stop before the third node
                    [0xff, 0xf8, 0x0f, 0x08, 0x88],                          // q6
                    [0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff],        // q7
                    [0x00],                     // q8: flag byte already clear
                    [0x18, null],               // q9
                    [0x08, 0xf7, 0x08, 0xf7],   // q10
                    [0x3c, null, 0x3c]];        // q11
    const out = { tracks };
    shapes.forEach((shape, i) => {
      const seq = Memory.alloc(0x1c0);
      seq.add(0x1a4).writeU32((0x99990000 + i) >>> 0);
      seq.add(0x1a8).writeU32((0x88880000 + i) >>> 0);
      if (shape === null) {
        seq.add(0x19c).writePointer(ptr(0));
      } else {
        const nodes = shape.map(() => Memory.alloc(0x20));
        nodes.forEach((n, k) => {
          n.add(4).writePointer(k + 1 < nodes.length ? nodes[k + 1] : ptr(0));
          n.add(8).writePointer(shape[k] === null ? ptr(0) : track(shape[k]));
        });
        seq.add(0x19c).writePointer(nodes[0]);
      }
      out['q' + i] = seq;
    });
    return out;
  },
});
