// Fixtures for C3 batch c3cl (sound.dll: the sequencer track finder 0x10023080 and its callee 0x1002e430,
// the open/close inner-device slots of the device base vtables 0x1005b914 / 0x1005b9e4 / 0x1005ba5c, and
// slots 23 and 27 of the channel vtable 0x1005bb90).
// Every block, vtable, list node, track element, inner device and channel device here is allocated by the
// fixture: no live sound object, device, DirectSound buffer, MIDI stream, sound list or sound.dll module
// global is ever handed to an arm, and nothing is freed. Layouts come from the disassembly cited in
// shim/src/re/c3cl.cpp. All values are synthetic.
// NativeCallbacks are pushed onto __diffKeepAlive by hand (only Memory.alloc is rewritten to __keepAlloc).
Object.assign(globalThis.DIFF_FIXTURES, {

  c3cl_zero(p, n) { for (let o = 0; o < n; o += 4) p.add(o).writeU32(0); return p; },
  c3cl_keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; },

  // --- 0x1002e430 Track_setField8RaisingMax: six track elements of 0x40 bytes, no vtable (the body makes no
  // call). Only +8 and +0xc matter; +0 and +4 carry markers so that a body writing the wrong field is seen.
  // The six values of +0xc span both sides of the unsigned compare at 0x1002e437 against the six argument
  // values of the registry entry, including the pair (+0xc = 0x80000000, value = 0x7fffffff) which a SIGNED
  // compare would resolve the other way.
  c3cl_track() {
    const F = globalThis.DIFF_FIXTURES;
    const mk = (max) => {
      const p = F.c3cl_zero(__keepAlloc(0x40), 0x40);
      p.add(0x00).writeU32(0xa0a0a0a0);
      p.add(0x04).writeU32(0x0b0b0b0b);
      p.add(0x08).writeU32(0x08080808);
      p.add(0x0c).writeU32(max >>> 0);
      p.add(0x10).writeU32(0x10101010);
      return p;
    };
    return {
      w_zero: mk(0x00000000), w_one: mk(0x00000001), w_small: mk(0x00000005),
      w_posmax: mk(0x7fffffff), w_sign: mk(0x80000000), w_all: mk(0xffffffff),
    };
  },

  // --- 0x10023080 Seq_findTrackById: nine sequencer blocks of 0x600 bytes. The sequencer's own fields are
  // +0x218 (the list head), +0x220 (the walk cursor the body rewrites) and +0x224 (the gate: zero means the
  // body returns 0x24 at once). Nodes live at block+0x300 (0x20 each, {+4 next, +8 element}) and elements at
  // block+0x400 (0x40 each), inside the SAME block, so one state region per sequencer covers the cursor,
  // every node and every element's +8 / +0xc that the callee 0x1002e430 writes.
  // Each element carries its id at +4 and a pre-set maximum at +0xc that is above some of the registry's
  // values and below others, so the callee's unsigned compare is exercised from both sides here too.
  // The nine shapes: a gated sequencer with a full list, an empty list, a list whose first element is null,
  // one, three and five elements, three elements with the same id, a null element in the middle, and a list
  // of two whose ids are both missed (the walk runs off the end).
  c3cl_seqfind() {
    const F = globalThis.DIFF_FIXTURES;
    // ids: a number is an element with that id, null is a node with no element.
    const mk = (gate, ids) => {
      const blk = F.c3cl_zero(__keepAlloc(0x600), 0x600);
      blk.add(0x220).writeU32(0x22002200);          // the cursor, overwritten by every non-gated call
      blk.add(0x224).writeU32(gate);
      const n = ids.length;
      for (let i = 0; i < n; ++i) {
        const node = blk.add(0x300 + i * 0x20);
        node.add(4).writePointer(i + 1 < n ? blk.add(0x300 + (i + 1) * 0x20) : ptr(0));
        if (ids[i] === null) {
          node.add(8).writePointer(ptr(0));
        } else {
          const el = blk.add(0x400 + i * 0x40);
          el.add(0x00).writeU32((0xe0e00000 + i) >>> 0);
          el.add(0x04).writeS32(ids[i]);
          el.add(0x08).writeU32((0x88880000 + i) >>> 0);
          el.add(0x0c).writeU32((i * 0x1000) >>> 0);   // the maximum the callee may raise
          node.add(8).writePointer(el);
        }
      }
      blk.add(0x218).writePointer(n ? blk.add(0x300) : ptr(0));
      return blk;
    };
    return {
      f_gated: mk(0, [11, 33, 7]),
      f_empty: mk(1, []),
      f_nullfirst: mk(1, [null, 33]),
      f_one: mk(1, [11]),
      f_three: mk(1, [11, 33, 7]),
      f_five: mk(1, [11, 22, 33, 44, 7]),
      f_dup: mk(1, [33, 33, 33]),
      f_nullmid: mk(1, [11, null, 33]),
      f_miss2: mk(1, [1, 2]),
    };
  },

  // --- the five device-base slots (0x10029320, 0x1002a000, 0x1002a470, 0x10029f90, 0x1002a420): fourteen
  // blocks of 0x200 bytes. The outer object is block+0 and its inner device - the pointer the bodies read at
  // this+0x14 - is block+0x100 when the shape has one, so a single state region per block covers both
  // objects and every call record.
  // The outer vtable's slot +4 is the open pre-step the open bodies call with the second argument: it bumps
  // the object's counter at +0x84, records the argument at +0x88 and RETURNS the object's own code at +0x80,
  // which is what selects the early-return arm. Slot +8 is the close step both failure paths call: it bumps
  // +0x8c. The inner vtable's slot +0x10 is the real open: it bumps the inner's counter at +0x84, records
  // both arguments at +0x8c and +0x90 and returns the inner's own code at +0x80. Slot +0x14 is the inner
  // release the close bodies call: it bumps the inner's counter at +0x88.
  // An arm that calls the wrong slot, the wrong object or the wrong number of times therefore reads RED on
  // the records, not only on the return value.
  // The fourteen shapes cover: no inner at all (the close bodies' null arm), no inner with a non-zero outer
  // code (the open bodies' early-return arm, which never reads the inner), an inner with a zero inner code
  // (the open success arm) and an inner with four different non-zero inner codes (the open failure arm,
  // which calls slot +8 and, in MidiDeviceBase_openInner only, clears this+0x14).
  c3cl_dev() {
    const F = globalThis.DIFF_FIXTURES;
    const ovt = F.c3cl_zero(__keepAlloc(0x100), 0x100);
    ovt.add(0x04).writePointer(F.c3cl_keep(new NativeCallback(function (self, b) {
      self.add(0x84).writeU32((self.add(0x84).readU32() + 1) >>> 0);
      self.add(0x88).writeS32(b);
      return self.add(0x80).readS32();
    }, 'int', ['pointer', 'int'], 'thiscall')));
    ovt.add(0x08).writePointer(F.c3cl_keep(new NativeCallback(function (self) {
      self.add(0x8c).writeU32((self.add(0x8c).readU32() + 1) >>> 0);
    }, 'void', ['pointer'], 'thiscall')));
    const ivt = F.c3cl_zero(__keepAlloc(0x100), 0x100);
    ivt.add(0x10).writePointer(F.c3cl_keep(new NativeCallback(function (self, a, b) {
      self.add(0x84).writeU32((self.add(0x84).readU32() + 1) >>> 0);
      self.add(0x8c).writeS32(a);
      self.add(0x90).writeS32(b);
      return self.add(0x80).readS32();
    }, 'int', ['pointer', 'int', 'int'], 'thiscall')));
    ivt.add(0x14).writePointer(F.c3cl_keep(new NativeCallback(function (self) {
      self.add(0x88).writeU32((self.add(0x88).readU32() + 1) >>> 0);
    }, 'void', ['pointer'], 'thiscall')));
    // hasInner, the outer code at +0x80, the inner code at +0x80.
    const mk = (hasInner, outerCode, innerCode) => {
      const blk = F.c3cl_zero(__keepAlloc(0x200), 0x200);
      blk.writePointer(ovt);
      blk.add(0x80).writeS32(outerCode);
      blk.add(0x18).writeU32(0x18181818);          // a neighbour of this+0x14, must not be touched
      if (hasInner) {
        const inner = blk.add(0x100);
        inner.writePointer(ivt);
        inner.add(0x80).writeS32(innerCode);
        blk.add(0x14).writePointer(inner);
      }
      return blk;
    };
    return {
      a_null0: mk(0, 0, 0), a_null1: mk(0, 0, 0),
      a_nullE: mk(0, 0xb, 0), a_nullE2: mk(0, 0x24, 0),
      a_ok0: mk(1, 0, 0), a_ok1: mk(1, 0, 0), a_ok2: mk(1, 0, 0), a_ok3: mk(1, 0, 0),
      a_fail1: mk(1, 0, 0xb), a_fail2: mk(1, 0, 0x24), a_fail3: mk(1, 0, -1),
      a_fail4: mk(1, 0, 0x7fffffff),
      a_early: mk(1, 0x13, 0), a_early2: mk(1, -1, 0x24),
    };
  },

  // --- 0x1002baa0 getPlaybackTime and 0x1002c040 Channel_applyFlags: channel blocks of 0x200 bytes. The
  // channel is block+0 and the device it reads at this+0x40 is block+0x100 when the shape has one, so one
  // state region per block covers the channel fields and the device's call records.
  // The device vtable's slot +0x5c is what getPlaybackTime forwards to with nothing pushed: it bumps the
  // device's counter at +0x84 and returns the device's own value at +0x80. Slot +0x6c is what
  // Channel_applyFlags forwards the flag word to: it bumps +0x88 and records the word at +0x8c, so a wrong
  // or missing forward reads RED even though the function itself always returns 0.
  // getPlaybackTime shapes: four with a device (four different forwarded values), two with no device and
  // bit 4 of +0x58 clear, two with the bit set and +0x68 zero, two whose +0x64 limit is at or below every
  // attainable elapsed count (limit 0 takes `jbe` for every clock value at all, limit 1 for every clock
  // value above 1) and two whose limit is at the unsigned ceiling (0xffffffff with anchor 1 and 0xfffffffe
  // with anchor 2, which miss `jbe` for every clock value except 0 and 1). Every threshold is decided by
  // the fields alone, so timeGetTime cannot move a result between the two arms.
  c3cl_chan() {
    const F = globalThis.DIFF_FIXTURES;
    const dvt = F.c3cl_zero(__keepAlloc(0x100), 0x100);
    dvt.add(0x5c).writePointer(F.c3cl_keep(new NativeCallback(function (self) {
      self.add(0x84).writeU32((self.add(0x84).readU32() + 1) >>> 0);
      return self.add(0x80).readS32();
    }, 'int', ['pointer'], 'thiscall')));
    dvt.add(0x6c).writePointer(F.c3cl_keep(new NativeCallback(function (self, f) {
      self.add(0x88).writeU32((self.add(0x88).readU32() + 1) >>> 0);
      self.add(0x8c).writeU32(f >>> 0);
    }, 'void', ['pointer', 'uint32'], 'thiscall')));
    // hasDevice, the device's forwarded value, +0x58, +0x64, +0x68.
    const mk = (hasDevice, devValue, state, limit, anchor) => {
      const blk = F.c3cl_zero(__keepAlloc(0x200), 0x200);
      blk.add(0x30).writeU32(0x30303030);
      blk.add(0x58).writeU32(state >>> 0);
      blk.add(0x5c).writeU32(0x5c5c5c5c);          // a neighbour of +0x58, must not be touched
      blk.add(0x64).writeU32(limit >>> 0);
      blk.add(0x68).writeU32(anchor >>> 0);
      if (hasDevice) {
        const dev = blk.add(0x100);
        dev.writePointer(dvt);
        dev.add(0x80).writeS32(devValue);
        blk.add(0x40).writePointer(dev);
      }
      return blk;
    };
    return {
      // getPlaybackTime
      t_dev0: mk(1, 0, 0, 0, 0), t_dev1: mk(1, 1, 0x10, 0, 0),
      t_dev2: mk(1, 0x1234, 0, 0xffffffff, 1), t_dev3: mk(1, -1, 0xffffffff, 0, 0),
      t_off0: mk(0, 0, 0x00000000, 0xfffffff0, 1),
      t_off1: mk(0, 0, 0xffffffef, 0xfffffff0, 1),
      t_zero0: mk(0, 0, 0x00000010, 0xfffffff0, 0),
      t_zero1: mk(0, 0, 0xffffffff, 0xfffffff0, 0),
      t_past0: mk(0, 0, 0x00000010, 0x00000000, 0x80000000),
      t_past1: mk(0, 0, 0x000000f0, 0x00000001, 0x00000001),
      t_live0: mk(0, 0, 0x00000010, 0xffffffff, 0x00000001),
      t_live1: mk(0, 0, 0x00000030, 0xfffffffe, 0x00000002),
      // Channel_applyFlags
      c_dev0: mk(1, 0, 0x00000000, 0, 0),
      c_dev1: mk(1, 0, 0x0000aaaa, 0, 0),
      c_nodev0: mk(0, 0, 0x00000000, 0, 0),
      c_nodev1: mk(0, 0, 0x80000000, 0, 0),
      c_nodev2: mk(0, 0, 0xffffffff, 0, 0),
      c_dev2: mk(1, 0, 0x55555555, 0, 0),
    };
  },

});
