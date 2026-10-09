// Fixtures for C3 batch c3cf (sound.dll: the controller-base list walkers, the byte stream of the 0x1005b5f0
// table, the Wave_Device stream starter and eight sequencer slots of the Sound vtable 0x1005b000).
// Every block, vtable, list node, track record and sub-object here is allocated by the fixture: no live sound
// object, device, DirectSound buffer, MIDI stream, sound list or module global is ever handed to an arm, and
// nothing is freed. Layouts come from the disassembly cited in shim/src/re/c3cf.cpp. All values are synthetic.
// NativeCallbacks are pushed onto __diffKeepAlive by hand (only Memory.alloc is rewritten to __keepAlloc).
Object.assign(globalThis.DIFF_FIXTURES, {

  c3cf_zero(p, n) { for (let o = 0; o < n; o += 4) p.add(o).writeU32(0); return p; },
  c3cf_keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; },

  // --- 0x10006b00 startStream: nine stream objects of 0x100 bytes on one synthetic vtable whose only filled
  // slot is +0x7c, the start method the body calls with nothing pushed. That callback records how often it
  // ran at +0xe0 and the state word it SEES at +0xe4, so the A/B also measures the order of the writes (the
  // original stores +0x58 before the call). Three objects have bit 0 of +0x58 already set (the 0xd arm) and
  // six have it clear, with bit 2 set and clear and with upper bits that the `and al, 0xfb` / `or al, 3`
  // pair must leave alone.
  c3cf_stream() {
    const F = globalThis.DIFF_FIXTURES;
    const vt = F.c3cf_zero(__keepAlloc(0x100), 0x100);
    vt.add(0x7c).writePointer(F.c3cf_keep(new NativeCallback(function (self) {
      self.add(0xe0).writeU32((self.add(0xe0).readU32() + 1) >>> 0);
      self.add(0xe4).writeU32(self.add(0x58).readU32());
    }, 'void', ['pointer'], 'thiscall')));
    const mk = (state) => {
      const p = F.c3cf_zero(__keepAlloc(0x100), 0x100);
      p.writePointer(vt);
      p.add(0x58).writeU32(state >>> 0);
      p.add(0xc8).writeU32(0xc8c8c8c8);
      p.add(0xcc).writeU32(0xcccc0001);
      p.add(0xe4).writeU32(0xe4e4e4e4);
      return p;
    };
    return {
      s_busy0: mk(0x00000001), s_busy1: mk(0xffffffff), s_busy2: mk(0x00000007),
      s_free0: mk(0x00000000), s_free1: mk(0x00000004), s_free2: mk(0x00000006),
      s_free3: mk(0xfffffffe), s_free4: mk(0x12340000), s_free5: mk(0x000000fa),
    };
  },

  // --- 0x1001c2a0 CtrlBase_resetElements and 0x1001c2f0 CtrlBase_flushElements: twelve controllers. Both are
  // entered with ecx pointing 0x30 into the block, so the result cell this-0x28 is block+0x08, the list head
  // this-0x20 is block+0x10 and the walk cursor this-0x18 is block+0x18. Nodes live at block+0x80 (0x10
  // each, {+4 next, +8 element}) and elements at block+0x100 (0x40 each) inside the SAME block, so one state
  // region per controller covers the cursor, the result cell and every element's record.
  // The elements share one vtable whose slots +0x14 and +0x18 are recording callbacks: each bumps the
  // element's own counter at +0x20 and writes the slot number at +0x24, so an arm that calls the wrong slot,
  // the wrong element or the wrong number of times reads RED. An element marked 2 also carries the ADDRESS
  // of the controller's walk cursor at +0x30 and the callback clears that cell: those two lists are what
  // takes the side of the `je` at 0x1001c2be / 0x1001c30e, the re-read of the cursor the original makes
  // after the call, which no list of passive elements can reach.
  // The fourteen lists are: empty, one node with a null element, one to six elements, four lists whose null
  // element sits first, second, third and in a six-node list, and two whose first or second element clears
  // the cursor.
  c3cf_ctrl() {
    const F = globalThis.DIFF_FIXTURES;
    const evt = F.c3cf_zero(__keepAlloc(0x100), 0x100);
    const rec = (slot) => F.c3cf_keep(new NativeCallback(function (self) {
      self.add(0x20).writeU32((self.add(0x20).readU32() + 1) >>> 0);
      self.add(0x24).writeU32(slot);
      const kill = self.add(0x30).readPointer();
      if (!kill.isNull()) kill.writePointer(ptr(0));
    }, 'void', ['pointer'], 'thiscall'));
    evt.add(0x14).writePointer(rec(0x14));
    evt.add(0x18).writePointer(rec(0x18));
    const mk = (elems) => {
      const blk = F.c3cf_zero(__keepAlloc(0x400), 0x400);
      blk.add(0x08).writeU32(0x08080808);          // this-0x28, overwritten with 0x18 by resetElements only
      blk.add(0x18).writeU32(0x18181818);          // this-0x18, the walk cursor
      const n = elems.length;
      for (let i = 0; i < n; ++i) {
        const node = blk.add(0x80 + i * 0x10);
        node.add(4).writePointer(i + 1 < n ? blk.add(0x80 + (i + 1) * 0x10) : ptr(0));
        if (elems[i]) {
          const el = blk.add(0x100 + i * 0x40);
          el.writePointer(evt);
          el.add(0x24).writeU32((0x99990000 + i) >>> 0);
          if (elems[i] === 2) el.add(0x30).writePointer(blk.add(0x18));   // clears the walk cursor
          node.add(8).writePointer(el);
        } else {
          node.add(8).writePointer(ptr(0));
        }
      }
      blk.add(0x10).writePointer(n ? blk.add(0x80) : ptr(0));
      return blk;
    };
    const shapes = {
      k_empty: [], k_null1: [0], k_one: [1], k_two: [1, 1], k_three: [1, 1, 1], k_four: [1, 1, 1, 1],
      k_five: [1, 1, 1, 1, 1], k_six: [1, 1, 1, 1, 1, 1], k_stop1: [0, 1, 1], k_stop2: [1, 0, 1],
      k_stop3: [1, 1, 0, 1], k_stop5: [1, 1, 1, 1, 0, 1], k_clear1: [2, 1, 1], k_clear2: [1, 2, 1],
    };
    const out = {};
    for (const name in shapes) {
      const blk = mk(shapes[name]);
      out[name] = blk;                      // the block, for the state region
      out['p' + name.slice(1)] = blk.add(0x30);   // the pointer the body is called with
    }
    return out;
  },

  // --- 0x1001c370 Ctrl5b5c0_mapValue: eleven controllers. Here the list is at this+0xc (head this+0xc, walk
  // cursor this+0x14) and the 128-byte table the body finally indexes is this+0x5c, filled with the
  // synthetic pattern (i * 7 + 3) & 0xff. Nodes live at block+0x100 and elements at block+0x140 inside the
  // same block, so one state region per controller covers the cursor and every element's record.
  // The elements share one vtable whose slot +0x10 is the mapping callback: it records the call at +0x24 and
  // the value it was handed at +0x28, and returns (value + the element's own constant at +0x20) & 0x3f, so a
  // running value that is mapped by the wrong elements, in the wrong order or the wrong number of times
  // reads RED on both the return value and the records.
  // Element types: 3 and 4 map, 5 stops the walk and returns the running value RAW when the element's byte
  // at +0x19 is zero and is skipped when it is not, and anything else is skipped.
  // An element whose spec carries a fourth field also gets the ADDRESS of the controller's walk cursor at
  // +0x2c and the mapping callback clears that cell, which is what takes the side of the `je` at
  // 0x1001c3b1 - the re-read of the cursor the original makes after the call.
  c3cf_map() {
    const F = globalThis.DIFF_FIXTURES;
    const evt = F.c3cf_zero(__keepAlloc(0x100), 0x100);
    evt.add(0x10).writePointer(F.c3cf_keep(new NativeCallback(function (self, v) {
      self.add(0x24).writeU32((self.add(0x24).readU32() + 1) >>> 0);
      self.add(0x28).writeS32(v);
      const kill = self.add(0x2c).readPointer();
      if (!kill.isNull()) kill.writePointer(ptr(0));
      return (v + self.add(0x20).readS32()) & 0x3f;
    }, 'int', ['pointer', 'int'], 'thiscall')));
    const mk = (specs) => {
      const blk = F.c3cf_zero(__keepAlloc(0x200), 0x200);
      for (let i = 0; i < 0x80; ++i) blk.add(0x5c + i).writeU8((i * 7 + 3) & 0xff);
      blk.add(0x14).writeU32(0x14141414);
      const n = specs.length;
      for (let i = 0; i < n; ++i) {
        const node = blk.add(0x100 + i * 0x10);
        node.add(4).writePointer(i + 1 < n ? blk.add(0x100 + (i + 1) * 0x10) : ptr(0));
        const s = specs[i];
        if (s) {
          const el = blk.add(0x140 + i * 0x30);
          el.writePointer(evt);
          el.add(0x19).writeU8(s[1]);
          el.add(0x1c).writeS32(s[0]);
          el.add(0x20).writeS32(s[2]);
          if (s.length > 3) el.add(0x2c).writePointer(blk.add(0x14));   // clears the walk cursor
          node.add(8).writePointer(el);
        } else {
          node.add(8).writePointer(ptr(0));
        }
      }
      blk.add(0x0c).writePointer(n ? blk.add(0x100) : ptr(0));
      return blk;
    };
    return {
      m_empty: mk([]),
      m_nullelem: mk([null]),
      m_t3: mk([[3, 1, 5]]),
      m_t4: mk([[4, 1, -3]]),
      m_t5on: mk([[5, 1, 9]]),
      m_t5off: mk([[5, 0, 9]]),
      m_t6: mk([[6, 1, 9]]),
      m_t0: mk([[0, 1, 9]]),
      m_chain: mk([[3, 1, 5], [4, 1, 7], [6, 1, 0], [3, 1, -2]]),
      m_chain5: mk([[3, 1, 5], [5, 0, 0], [4, 1, 7]]),
      m_mixed: mk([[5, 1, 0], [3, 1, 11], [5, 0, 0]]),
      m_null2: mk([[3, 1, 5], null, [4, 1, 7]]),
      m_clear: mk([[3, 1, 5, 'kill'], [4, 1, 7], [3, 1, -2]]),
    };
  },

  // --- 0x1001c7a0 streamReadByte and 0x1001c7e0 streamConsumeByte: twelve streams of 0x200 bytes. The
  // buffer at +0x2c points at block+0x80, 0x80 synthetic bytes (i * 13 + 5) & 0xff; the position is +0x30,
  // the limit +0x3c and the cached byte +0x19. The refill is the object's own vtable slot +0x14: the
  // callback records the call at +0x40 and installs the position and limit parked at +0x44 and +0x48, so an
  // arm that refills when it should not (or does not when it should) reads RED on the byte, the position and
  // the counter. Four streams have position < limit (no refill), four have position == limit and four have
  // position > limit; every position the body can reach stays inside the 0x80-byte buffer.
  c3cf_bytes() {
    const F = globalThis.DIFF_FIXTURES;
    const svt = F.c3cf_zero(__keepAlloc(0x100), 0x100);
    svt.add(0x14).writePointer(F.c3cf_keep(new NativeCallback(function (self) {
      self.add(0x40).writeU32((self.add(0x40).readU32() + 1) >>> 0);
      self.add(0x30).writeU32(self.add(0x44).readU32());
      self.add(0x3c).writeU32(self.add(0x48).readU32());
    }, 'void', ['pointer'], 'thiscall')));
    const mk = (pos, limit, rpos, rlimit) => {
      const p = F.c3cf_zero(__keepAlloc(0x200), 0x200);
      p.writePointer(svt);
      for (let i = 0; i < 0x80; ++i) p.add(0x80 + i).writeU8((i * 13 + 5) & 0xff);
      p.add(0x19).writeU8(0xee);
      p.add(0x2c).writePointer(p.add(0x80));
      p.add(0x30).writeU32(pos >>> 0);
      p.add(0x3c).writeU32(limit >>> 0);
      p.add(0x44).writeU32(rpos >>> 0);
      p.add(0x48).writeU32(rlimit >>> 0);
      return p;
    };
    return {
      b_mid0: mk(3, 0x40, 0, 0x40), b_first: mk(0, 0x40, 0, 0x40), b_last: mk(0x3f, 0x40, 0, 0x40),
      b_mid1: mk(0x20, 0x7f, 0, 0x7f), b_mid2: mk(0x7e, 0x7f, 0, 1), b_mid3: mk(1, 2, 0, 1),
      b_eq0: mk(0x40, 0x40, 0x10, 0x40), b_eq1: mk(0, 0, 5, 0x30), b_eq2: mk(0, 0, 0, 0),
      b_eq3: mk(0x10, 0x10, 0x7f, 0x80),
      b_over0: mk(0x50, 0x40, 0, 0x20), b_over1: mk(0x7f, 1, 0x20, 0x40),
    };
  },

  // --- 0x10020f50 Seq_setField730Clamped: six sequencers of 0x800 bytes. Three have a null inner device at
  // +0x40, so the original Seq_applyChannelLevel 0x1002b260 this body calls only writes +4; three carry a
  // synthetic inner object whose vtable slot +0x40 records the value at +0x80 and the call count at +0x84.
  // The compared windows are 0..0x5f (the +4 the callee writes and the +0x40 pointer) and 0x700..0x75f (the
  // +0x730 this body writes).
  c3cf_seqlevel() {
    const F = globalThis.DIFF_FIXTURES;
    const dvt = F.c3cf_zero(__keepAlloc(0x100), 0x100);
    dvt.add(0x40).writePointer(F.c3cf_keep(new NativeCallback(function (self, v) {
      self.add(0x80).writeS32(v);
      self.add(0x84).writeU32((self.add(0x84).readU32() + 1) >>> 0);
    }, 'void', ['pointer', 'int'], 'thiscall')));
    const out = {};
    for (let i = 0; i < 3; ++i) {
      const seq = F.c3cf_zero(__keepAlloc(0x800), 0x800);
      seq.add(0x04).writeS32(-9 - i);
      seq.add(0x730).writeS32(-1234 - i);
      out['l_null' + i] = seq;
    }
    for (let i = 0; i < 3; ++i) {
      const inner = F.c3cf_zero(__keepAlloc(0x100), 0x100);
      inner.writePointer(dvt);
      inner.add(0x80).writeS32(-55 - i);
      inner.add(0x84).writeU32(700 + i);
      const seq = F.c3cf_zero(__keepAlloc(0x800), 0x800);
      seq.add(0x04).writeS32(-19 - i);
      seq.add(0x40).writePointer(inner);
      seq.add(0x730).writeS32(4321 + i);
      out['l_dev' + i] = seq;
      out['l_inner' + i] = inner;
    }
    return out;
  },

  // --- 0x10022240 Seq_reset: twelve sequencers of 0x800 bytes. The body calls the original Seq_rewind
  // 0x1001e1c0 on the reader at this+0x60, whose list head is this+0x1fc, whose walk cursor is this+0x204
  // and whose flag byte is this+0xac; the reader list is either empty or a single node with a NULL element,
  // which is what keeps Seq_rewind from calling Seq_rewindTracks 0x1001e8e0 (a path that would need two more
  // synthetic vtable slots) while still giving the cursor two outcomes. The node sits at block+0x600, away
  // from every compared window.
  // The compared windows are 0xa0..0xbf (the reader flag byte), 0x1f0..0x20f (the list head), 0x210..0x21f
  // (the +0x214 flag word) and 0x730..0x74f (the +0x740 cell).
  c3cf_seqreset() {
    const F = globalThis.DIFF_FIXTURES;
    const mk = (flagsAc, f214, f740, withNode) => {
      const seq = F.c3cf_zero(__keepAlloc(0x800), 0x800);
      seq.add(0xac).writeU32(flagsAc >>> 0);
      seq.add(0x214).writeU32(f214 >>> 0);
      seq.add(0x740).writeS32(f740);
      seq.add(0x204).writeU32(0x20420420);
      if (withNode) {
        const n = seq.add(0x600);
        n.add(4).writePointer(ptr(0));
        n.add(8).writePointer(ptr(0));
        seq.add(0x1fc).writePointer(n);
      }
      return seq;
    };
    return {
      r0: mk(0x00000000, 0x00000000, 0, 0), r1: mk(0xffffffff, 0xffffffff, -1, 0),
      r2: mk(0x0000009c, 0x00000018, 7, 0), r3: mk(0x12345678, 0x87654321, -99, 0),
      r4: mk(0x00000063, 0x000000e7, 0, 0), r5: mk(0x000000ff, 0x00000008, 1, 0),
      r6: mk(0x00000000, 0x00000010, 0, 1), r7: mk(0xffffffff, 0xffffff00, -1, 1),
      r8: mk(0x0000009c, 0x0000001f, 5, 1), r9: mk(0xaabbcc00, 0x0000ff18, -5, 1),
      r10: mk(0x00000080, 0x00000007, 2, 1), r11: mk(0x0000001c, 0xfedcba98, 3, 1),
    };
  },

  // --- 0x100227a0 Seq_setField240: four sequencers of 0x300 bytes whose +0x240 starts at four different
  // values. The compared window is 0x230..0x24f.
  c3cf_seqfield240() {
    const F = globalThis.DIFF_FIXTURES;
    const mk = (v) => {
      const p = F.c3cf_zero(__keepAlloc(0x300), 0x300);
      p.add(0x240).writeS32(v);
      p.add(0x23c).writeU32(0x23c23c00);
      p.add(0x244).writeU32(0x24424400);
      return p;
    };
    return { q0: mk(0), q1: mk(-1), q2: mk(0x7fffffff), q3: mk(1234) };
  },

  // --- 0x10023000 Seq_findTrackOrZero: nine sequencers of 0x600 bytes. The track list is the one at
  // this+0x218 (head) with the walk cursor at this+0x220 and the active dword at this+0x224; nodes live at
  // block+0x300 and elements at block+0x400 inside the same block, and each element carries its id at +4.
  // The compared window is 0x210..0x22f (the cursor and the active dword). The list shapes are: inactive,
  // empty, a single null element, one / three / five ids, a duplicated id, a null element in the middle and
  // a set of extreme ids.
  c3cf_seqfind() {
    const F = globalThis.DIFF_FIXTURES;
    const mk = (active, ids) => {
      const blk = F.c3cf_zero(__keepAlloc(0x600), 0x600);
      blk.add(0x220).writeU32(0x22022022);
      blk.add(0x224).writeU32(active >>> 0);
      const n = ids.length;
      for (let i = 0; i < n; ++i) {
        const node = blk.add(0x300 + i * 0x10);
        node.add(4).writePointer(i + 1 < n ? blk.add(0x300 + (i + 1) * 0x10) : ptr(0));
        if (ids[i] === null) {
          node.add(8).writePointer(ptr(0));
        } else {
          const el = blk.add(0x400 + i * 0x40);
          el.add(4).writeS32(ids[i]);
          el.add(8).writeU32((0xe1e10000 + i) >>> 0);
          node.add(8).writePointer(el);
        }
      }
      blk.add(0x218).writePointer(n ? blk.add(0x300) : ptr(0));
      return blk;
    };
    return {
      f_inactive: mk(0, [11, 22, 33]),
      f_empty: mk(1, []),
      f_nullfirst: mk(1, [null]),
      f_one: mk(1, [11]),
      f_three: mk(7, [11, 22, 33]),
      f_dup: mk(1, [7, 7, 9]),
      f_nullmid: mk(1, [11, null, 33]),
      f_five: mk(0xffffffff, [1, 2, 3, 4, 5]),
      f_extreme: mk(1, [-1, 0, 0x7fffffff]),
    };
  },

  // --- 0x10023f10 Seq_getTrackFlagHigh: five sequencers of 0x800 bytes on one synthetic vtable whose slot
  // +0xb4 is a callback returning the count parked at the sequencer's own +0x7f0 (a cell no body of this
  // batch touches), so one vtable serves every count. The list Seq_listGetAt 0x1001d780 walks is at
  // this+0x60, so its head is this+0x1fc and its cursor this+0x204; nodes live at block+0x280 and the track
  // records at block+0x300 (0x80 each), with the flag byte at record+0x38.
  // Seq_listGetAt writes 0 through the out pointer when the index is past the end of the list and the body
  // then dereferences it, so every ACCEPTED index below also has a node: the five shapes pair a count with a
  // node list that is at least count + 1 long, except g_c9_n3 whose vectors stay at indices 0..2.
  // The compared windows are 0x1f0..0x20f (the cursor) and 0x300..0x5ff (the records).
  c3cf_seqflag() {
    const F = globalThis.DIFF_FIXTURES;
    const vt = F.c3cf_zero(__keepAlloc(0x100), 0x100);
    vt.add(0xb4).writePointer(F.c3cf_keep(new NativeCallback(function (self) {
      return self.add(0x7f0).readU32();
    }, 'uint32', ['pointer'], 'thiscall')));
    const mk = (count, flags) => {
      const seq = F.c3cf_zero(__keepAlloc(0x800), 0x800);
      seq.writePointer(vt);
      seq.add(0x7f0).writeU32(count >>> 0);
      seq.add(0x204).writeU32(0x20420420);
      const n = flags.length;
      for (let i = 0; i < n; ++i) {
        const node = seq.add(0x280 + i * 0x10);
        node.add(4).writePointer(i + 1 < n ? seq.add(0x280 + (i + 1) * 0x10) : ptr(0));
        const rec = seq.add(0x300 + i * 0x80);
        rec.add(0x38).writeU8(flags[i]);
        rec.add(0x3c).writeU32((0x3c3c0000 + i) >>> 0);
        node.add(8).writePointer(rec);
      }
      seq.add(0x1fc).writePointer(n ? seq.add(0x280) : ptr(0));
      return seq;
    };
    return {
      g_c0_n2: mk(0, [0x00, 0x10]),
      g_c1_n3: mk(1, [0x10, 0x00, 0xff]),
      g_c3_n5: mk(3, [0x00, 0x10, 0xef, 0xff, 0x55]),
      g_c5_n6: mk(0x105, [0x10, 0x20, 0x30, 0x10, 0x00, 0xaa]),
      g_chi_n3: mk(0x00ff0002, [0x1f, 0xe0, 0x11]),
    };
  },

  // --- 0x100241f0 Seq_getListElementAt: five sequencers of 0x800 bytes with the same this+0x60 list (head
  // this+0x1fc, cursor this+0x204), nodes at block+0x280 and elements at block+0x300 (0x40 each). This body
  // never dereferences what Seq_listGetAt wrote, so an index past the end is a vector like any other.
  // The compared window is 0x1f0..0x20f (the cursor).
  c3cf_seqelem() {
    const F = globalThis.DIFF_FIXTURES;
    const mk = (shape) => {
      const seq = F.c3cf_zero(__keepAlloc(0x800), 0x800);
      seq.add(0x204).writeU32(0x20420420);
      const n = shape.length;
      for (let i = 0; i < n; ++i) {
        const node = seq.add(0x280 + i * 0x10);
        node.add(4).writePointer(i + 1 < n ? seq.add(0x280 + (i + 1) * 0x10) : ptr(0));
        if (shape[i]) {
          const el = seq.add(0x300 + i * 0x40);
          el.writeU32((0xe1e10000 + i) >>> 0);
          node.add(8).writePointer(el);
        } else {
          node.add(8).writePointer(ptr(0));
        }
      }
      seq.add(0x1fc).writePointer(n ? seq.add(0x280) : ptr(0));
      return seq;
    };
    return {
      e_empty: mk([]), e_one: mk([1]), e_three: mk([1, 1, 1]), e_five: mk([1, 1, 1, 1, 1]),
      e_nullmid: mk([1, 0, 1]),
    };
  },

  // --- 0x10024630 Seq_resetTrackControllers: three sequencers of 0x400 bytes with the this+0x60 list (head
  // this+0x1fc, cursor this+0x204) and nodes at block+0x280. The track records are 0x300 bytes each in their
  // own arena because the body walks record+0x254 down to record+0x58; the sub-objects it finds there are
  // 0x40 bytes each in a second arena, all on one synthetic vtable whose slot +0x34 records the call count
  // at +0x10 and the three arguments at +0x14, +0x18 and +0x1c. That is what proves the walk direction, the
  // step, the bounds and the argument order: the body pushes 0x19, 0x7f and 0, i.e. it calls the slot with
  // (0, 0x7f, 0x19).
  // The compared windows are the sequencer's 0x1f0..0x20f, the whole record arena and the whole sub-object
  // arena.
  c3cf_seqctrl() {
    const F = globalThis.DIFF_FIXTURES;
    const cvt = F.c3cf_zero(__keepAlloc(0x100), 0x100);
    cvt.add(0x34).writePointer(F.c3cf_keep(new NativeCallback(function (self, a, b, c) {
      self.add(0x10).writeU32((self.add(0x10).readU32() + 1) >>> 0);
      self.add(0x14).writeS32(a);
      self.add(0x18).writeS32(b);
      self.add(0x1c).writeS32(c);
    }, 'void', ['pointer', 'int', 'int', 'int'], 'thiscall')));
    const NCTRL = 12;
    const ctrls = F.c3cf_zero(__keepAlloc(NCTRL * 0x40), NCTRL * 0x40);
    for (let i = 0; i < NCTRL; ++i) {
      ctrls.add(i * 0x40).writePointer(cvt);
      ctrls.add(i * 0x40 + 0x14).writeS32(-1 - i);
    }
    const out = { ctrls: ctrls };
    const mk = (name, recs) => {
      const seq = F.c3cf_zero(__keepAlloc(0x400), 0x400);
      seq.add(0x204).writeU32(0x20420420);
      const n = recs.length;
      const arena = n ? F.c3cf_zero(__keepAlloc(n * 0x300), n * 0x300) : F.c3cf_zero(__keepAlloc(0x40), 0x40);
      for (let i = 0; i < n; ++i) {
        const node = seq.add(0x280 + i * 0x10);
        node.add(4).writePointer(i + 1 < n ? seq.add(0x280 + (i + 1) * 0x10) : ptr(0));
        const rec = arena.add(i * 0x300);
        rec.add(0x38).writeU8(recs[i][0]);
        for (const s of recs[i][1]) rec.add(s[0]).writePointer(ctrls.add(s[1] * 0x40));
        node.add(8).writePointer(rec);
      }
      seq.add(0x1fc).writePointer(n ? seq.add(0x280) : ptr(0));
      out[name] = seq;
      out[name + '_arena'] = arena;
      return seq;
    };
    // record = [initial +0x38 byte, [[slot offset, sub-object index], ...]]
    mk('c_r5', [
      [0x00, [[0x58, 0], [0x254, 1]]],
      [0xff, [[0x100, 2], [0x104, 3], [0x250, 4]]],
      [0x10, []],
      [0x01, [[0x5c, 5]]],
      [0xfe, [[0x58, 6], [0x5c, 7], [0x100, 8], [0x250, 9], [0x254, 10]]],
    ]);
    mk('c_r1', [[0x01, [[0x58, 11], [0x5c, 0], [0x254, 1]]]]);
    mk('c_r0', []);
    return out;
  },
});
