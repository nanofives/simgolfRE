// Fixtures for C3 batch c3dh (sound.dll, round 25): rows earlier batches dropped for imports, the CRT heap or live state.
// Layouts come from the disassembly cited in shim/src/re/c3dh.cpp. Every object, vtable, COM-shaped interface, list node and
// string here is allocated by the fixture; no live sound object, device, list, window or file is handed to an arm.
// Stand-ins used (one call per fixture): __heapStandIn (re/frida/js/heapstandin.js) over sound.dll's operator new 0x424d7
// and operator delete 0x4249a, __importStandIn (re/frida/js/importstandin.js) over sound.dll's IAT slots. Module globals a
// fixture writes are saved first and restored by re/frida/js/freeze.js before the game's first thread is resumed.
// All values are synthetic. NativeCallbacks are kept alive through __diffKeepAlive.
Object.assign(globalThis.DIFF_FIXTURES, {

  c3dh_fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; },
  c3dh_keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; },
  c3dh_snd(rva) { return Process.getModuleByName('sound.dll').base.add(rva); },
  c3dh_words(p, vals) { vals.forEach(function (v, i) { p.add(4 * i).writeU32((v >>> 0)); }); },

  // --- ring queues -----------------------------------------------------------------------------------------------
  // CommandQueue (object 0x4060 bytes: ring of 0x1000 node pointers at +0x50, read index +0x4050, write index +0x4054)
  // and WaveCmdQueue (0x41f0 bytes: ring at +0x1d8, read index +0x41d8, write index +0x41dc). Rows are [read, write]:
  //   0 empty; 1 next = 0xfff; 2 wraps to 0; 3 full (next == read); 4 full across the wrap; 5 read index above the mask
  //   (never equal); 6 write index beyond the ring (next = 0x234); 7 the same with read = 0x234 (full); 8 all-ones indices.
  // Every ring slot starts as a distinct marker so a store into the wrong slot reads RED.
  c3dh_ringq(kind) {
    const F = globalThis.DIFF_FIXTURES;
    const cfg = kind === 'cmd' ? { size: 0x4060, ring: 0x50, rd: 0x4050, wr: 0x4054 }
                               : { size: 0x41f0, ring: 0x1d8, rd: 0x41d8, wr: 0x41dc };
    const rows = [[0, 0], [3, 0xffe], [7, 0xfff], [1, 0], [0, 0xfff], [0x1000, 5], [0, 0x1233], [0x234, 0x1233],
                  [0xffffffff, 0xffffffff]];
    const out = {};
    rows.forEach(function (r, i) {
      const q = F.c3dh_fill(__keepAlloc(cfg.size), cfg.size, 0x70 + i);
      for (let s = 0; s < 0x1000; s++) q.add(cfg.ring + 4 * s).writeU32((0x5a000000 + (i << 16) + s) >>> 0);
      q.add(cfg.rd).writeU32(r[0] >>> 0);
      q.add(cfg.wr).writeU32(r[1] >>> 0);
      out['q' + i] = q;
    });
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100 });
    out.arena = h.arena;
    return out;
  },
  c3dh_cmdq() { return globalThis.DIFF_FIXTURES.c3dh_ringq('cmd'); },
  c3dh_wcq() { return globalThis.DIFF_FIXTURES.c3dh_ringq('wave'); },

  // --- list headers {+0 front, +4 back, +8 cursor, +0xc count} with 12-byte nodes {+0 prev, +4 next, +8 value} -----------
  // Block of 0x100 per row: header at `hdr`, three nodes at +0x80 / +0x90 / +0xa0. shape: number of linked nodes (0..3),
  // plus options: {front0: front pointer zero, back0: back pointer zero, cursor: index of the cursor node or -1, count}.
  c3dh_listblock(hdr, shape, opt) {
    const F = globalThis.DIFF_FIXTURES;
    const blk = F.c3dh_fill(__keepAlloc(0x100), 0x100, 0x6b);
    const node = (i) => blk.add(0x80 + 0x10 * i);
    for (let i = 0; i < 3; i++) {
      const prev = i > 0 ? node(i - 1) : ptr(0), next = (i < shape - 1) ? node(i + 1) : ptr(0);
      node(i).writePointer(prev);
      node(i).add(4).writePointer(next);
      node(i).add(8).writeU32((0xc1000000 + 0x100 * (opt.valbase || 0) + i) >>> 0);   // clients are replaced by the caller
      node(i).add(0xc).writeU32(0xdddd0000 + i);
    }
    const h = blk.add(hdr);
    h.writePointer(opt.front0 || shape === 0 ? ptr(0) : node(0));
    h.add(4).writePointer(opt.back0 || shape === 0 ? ptr(0) : node(shape - 1));
    h.add(8).writePointer(opt.cursor === undefined || opt.cursor < 0 ? ptr(0) : node(opt.cursor));
    h.add(0xc).writeU32((opt.count === undefined ? shape : opt.count) >>> 0);
    return blk;
  },

  // linkClient / NodeList_insert fixtures. Rows: [shape, options].
  c3dh_listrows(which) {
    const F = globalThis.DIFF_FIXTURES;
    const rows = which === 'link' ? [
      [0, {}], [1, {}], [2, {}], [3, {}], [1, { back0: true }], [2, { count: 0xffffffff }], [0, { count: 7 }], [3, { cursor: 1 }],
    ] : [
      [0, {}],                                   // empty: append, no back
      [0, { back0: false, count: 5 }],           // empty list header with a count
      [2, { front0: true }],                     // front 0 but a back node: append at the back (0x10024e8e taken)
      [1, {}],                                   // cursor 0: insert at the front
      [3, {}],                                   // cursor 0: insert at the front of three
      [3, { cursor: 2 }],                        // cursor = last node (next 0): append path through 0x10024e6d
      [3, { cursor: 0 }],                        // cursor = first node: insert between 0 and 1
      [3, { cursor: 1 }],                        // cursor = middle node
      [2, { cursor: 1, back0: true }],           // cursor last, back pointer 0: append path with no back (0x10024ea8)
      [2, { cursor: 0, count: 0xffffffff }],     // count wraps
    ];
    const out = {};
    rows.forEach(function (r, i) { out['l' + i] = F.c3dh_listblock(0, r[0], r[1]); });
    out.nrows = ptr(rows.length);
    return out;
  },
  c3dh_link() {
    const out = globalThis.DIFF_FIXTURES.c3dh_listrows('link');
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100 });
    out.arena = h.arena;
    return out;
  },
  // the same rows with the first allocation of every vector failing (empty list only: a NULL node is stored through
  // the old front, and written through at 0x10009073 / [eax+4] when the list is not empty)
  c3dh_link_oom() {
    const out = globalThis.DIFF_FIXTURES.c3dh_listrows('link');
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100, failMask: 1 });
    out.arena = h.arena;
    return out;
  },
  // the same rows with the first allocation of every vector failing: only the empty-header rows are used (a NULL node is linked through the old
  // back/front otherwise and the original faults there)
  c3dh_nodeins_oom() {
    const out = globalThis.DIFF_FIXTURES.c3dh_listrows('ins');
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100, failMask: 1 });
    out.arena = h.arena;
    return out;
  },
  c3dh_nodeins() {
    const out = globalThis.DIFF_FIXTURES.c3dh_listrows('ins');
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100 });
    out.arena = h.arena;
    return out;
  },

  // --- sound timer objects for registerClient / unregisterClient -------------------------------------------------------
  // The list header sits at +0x4c (front), +0x50 (back), +0x54 (cursor), +0x58 (count); nodes at +0x80 / +0x90 / +0xa0
  // are linked front -> back through their +4 words and carry the client objects cl0..cl2 as values. A client is a 0x20-byte
  // object whose vtable slot +0xc is a recording thiscall stub: +8 counts the calls, +0xc keeps the last `this`, +0x10
  // gets 0x600d (unregisterClient calls it on the client whether or not the node was found).
  c3dh_clients(out) {
    const F = globalThis.DIFF_FIXTURES;
    const vt = F.c3dh_fill(__keepAlloc(0x40), 0x40, 0);
    vt.add(0xc).writePointer(F.c3dh_keep(new NativeCallback(function (self) {
      self.add(8).writeU32((self.add(8).readU32() + 1) >>> 0);
      self.add(0xc).writeU32(self.toUInt32());
      self.add(0x10).writeU32(0x600d);
      return 0x1357;
    }, 'uint32', ['pointer'], 'thiscall')));
    for (let i = 0; i < 4; i++) {
      const c = F.c3dh_fill(__keepAlloc(0x20), 0x20, 0x30 + i);
      c.writePointer(vt);
      c.add(8).writeU32(0);
      out['cl' + i] = c;
    }
    out.clblk = out.cl0;      // (state: each client separately)
    return out;
  },
  // rows: [clients in list order, cursor index or -1, count]; the timer block is 0x100 bytes
  c3dh_timers(rows) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    F.c3dh_clients(out);
    rows.forEach(function (r, i) {
      const blk = F.c3dh_fill(__keepAlloc(0x100), 0x100, 0x6b);
      const node = (k) => blk.add(0x80 + 0x10 * k);
      const n = r[0].length;
      r[0].forEach(function (cl, k) {
        node(k).writePointer(k > 0 ? node(k - 1) : ptr(0));
        node(k).add(4).writePointer(k < n - 1 ? node(k + 1) : ptr(0));
        node(k).add(8).writePointer(out['cl' + cl]);
        node(k).add(0xc).writeU32(0xdddd0000 + k);
      });
      blk.add(0x4c).writePointer(n ? node(0) : ptr(0));
      blk.add(0x50).writePointer(n ? node(n - 1) : ptr(0));
      blk.add(0x54).writePointer(r[1] >= 0 ? node(r[1]) : ptr(0xc0ffee00));
      blk.add(0x58).writeU32((r[2] === undefined ? n : r[2]) >>> 0);
      out['t' + i] = blk;
    });
    return out;
  },
  c3dh_reg(leaveRet) {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3dh_timers([[[], -1], [[0], -1], [[0, 1], 0], [[0, 1, 2], 2], [[2], 0, 0xffffffff], [[1, 1], -1],
                               [[0, 1, 2], -1, 3], [[], 0, 9]]);
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100 });
    out.arena = h.arena;
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!EnterCriticalSection': { ret: 0 }, 'KERNEL32.dll!LeaveCriticalSection': { ret: leaveRet } } });
    out.implog = s.log;
    return out;
  },
  c3dh_reg_a() { return globalThis.DIFF_FIXTURES.c3dh_reg(0x7e57); },
  c3dh_reg_z() { return globalThis.DIFF_FIXTURES.c3dh_reg(0); },
  c3dh_unreg() {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3dh_timers([
      [[], -1],                      // empty list: not found
      [[0], -1],                     // the only node (no previous, no next)
      [[0, 1], -1],                  // front node with a next
      [[0, 1], -1],                  // (same list, the client at the back is removed by the vector)
      [[0, 1, 2], -1],               // middle node
      [[0, 1], -1],                  // client 2 is not in the list
      [[1, 1, 0], 1],                // duplicate values: the first match goes
      [[0], 0, 0],                   // count 0: the decrement wraps
    ]);
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    out.freelog = h.log;
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!EnterCriticalSection': { ret: 0 }, 'KERNEL32.dll!LeaveCriticalSection': { ret: 0 } } });
    out.implog = s.log;
    return out;
  },

  // =====================================================================================================================
  // B. COM-shaped interfaces, timers, window objects
  // =====================================================================================================================

  // A COM-shaped interface object (0x200 bytes): +0 vtable; result codes +0x10 (slot 0x14), +0x14 (slot 0x30), +0x18 (slot
  // 0x38), +0x1c (slot 0x48); per-slot records at +0x40 + 0x20 * k (k = 0 for 0x14, 1 for 0x30, 2 for 0x38, 3 for 0x48): the
  // call count and the arguments after the interface; +0x24 counts all calls and +0x28.. keeps the order (slot index + 1).
  // The stubs are stdcall with the interface as the first argument, like the DirectSound methods.
  c3dh_mkcom(codes) {
    const F = globalThis.DIFF_FIXTURES;
    const com = F.c3dh_fill(__keepAlloc(0x200), 0x200, 0);
    const vt = F.c3dh_fill(__keepAlloc(0x80), 0x80, 0);
    const rec = function (self, k, args) {
      const r = self.add(0x40 + 0x20 * k);
      r.writeU32((r.readU32() + 1) >>> 0);
      args.forEach(function (v, i) { r.add(4 + 4 * i).writeU32(v >>> 0); });
      const n = self.add(0x24).readU32();
      if (n < 16) self.add(0x28 + n).writeU8(k + 1);
      self.add(0x24).writeU32(n + 1);
    };
    const t4 = ['pointer', 'uint32', 'uint32', 'uint32'];
    vt.add(0x14).writePointer(F.c3dh_keep(new NativeCallback(function (self, a, b, c) {
      rec(self, 0, [a, b, c]); return self.add(0x10).readU32(); }, 'uint32', t4, 'stdcall')));
    vt.add(0x30).writePointer(F.c3dh_keep(new NativeCallback(function (self, a, b, c) {
      rec(self, 1, [a, b, c]); return self.add(0x14).readU32(); }, 'uint32', t4, 'stdcall')));
    vt.add(0x38).writePointer(F.c3dh_keep(new NativeCallback(function (self, a, b, c, d) {
      rec(self, 2, [a, b, c, d]); return self.add(0x18).readU32(); }, 'uint32', t4.concat(['uint32']), 'stdcall')));
    vt.add(0x48).writePointer(F.c3dh_keep(new NativeCallback(function (self) {
      rec(self, 3, []); return self.add(0x1c).readU32(); }, 'uint32', ['pointer'], 'stdcall')));
    com.writePointer(vt);
    com.add(0x10).writeU32((codes.fmt || 0) >>> 0);
    com.add(0x14).writeU32((codes.play || 0) >>> 0);
    com.add(0x18).writeU32((codes.pos || 0) >>> 0);
    com.add(0x1c).writeU32((codes.stop || 0) >>> 0);
    return com;
  },

  // DsBuffer objects (0x100 bytes): +0x48 / +0x4c / +0x50 / +0x54 / +0x58 / +0x78 position bookkeeping, +0x60 the interface,
  // +0xac the sample rate and +0xb4 the block alignment of the WAVEFORMATEX at +0xa8. Every field starts as a distinct marker.
  c3dh_dsbuf(com, f) {
    const F = globalThis.DIFF_FIXTURES;
    const b = F.c3dh_fill(__keepAlloc(0x100), 0x100, 0x44);
    for (let o = 0; o < 0x100; o += 4) b.add(o).writeU32((0x4d000000 + o) >>> 0);
    const set = (o, v) => b.add(o).writeU32((v >>> 0));
    set(0x48, f.p48 === undefined ? 0x11111111 : f.p48);
    set(0x4c, f.p4c === undefined ? 0x22222222 : f.p4c);
    set(0x50, f.p50 === undefined ? 0x00000100 : f.p50);
    set(0x54, f.p54 === undefined ? 0x33333333 : f.p54);
    set(0x58, f.p58 === undefined ? 0xffffffff : f.p58);
    set(0x78, f.p78 === undefined ? 0x7fffffff : f.p78);
    set(0xac, f.rate === undefined ? 22050 : f.rate);
    set(0xb4, f.align === undefined ? 4 : f.align);
    b.add(0x60).writePointer(com ? com : ptr(0));
    return b;
  },
  c3dh_dsbufs(rows, outs) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    rows.forEach(function (r, i) {
      const com = r.com ? F.c3dh_mkcom(r.com) : null;
      if (com) out['c' + i] = com;
      out['d' + i] = F.c3dh_dsbuf(com, r);
    });
    if (outs) { out.outs = F.c3dh_fill(__keepAlloc(0x20), 0x20, 0x77); out.o1 = out.outs; out.o2 = out.outs.add(8); }
    return out;
  },
  // timeGetTime is stood in for the key: every call returns `now` (the call count and nothing else is logged)
  c3dh_dsimports(now) {
    const s = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!timeGetTime': { ret: now } } });
    return s.log;
  },

  c3dh_ds_restart() {
    const F = globalThis.DIFF_FIXTURES;
    const rows = [
      { },                                                              // no interface
      { com: {} },                                                      // Stop, Play ok, format ok
      { com: { fmt: 0x88780096 } },                                     // the format result is the return value
      { com: { play: 0x8878000a } },                                    // Play fails: its code, no format call
      { com: { play: 1, fmt: 5 } },
      { com: { play: 0xffffffff, stop: 7 } },
      { com: { stop: 0x1234, fmt: 0xffffffff } },
      { com: { fmt: 1 }, p48: 0, p4c: 0 },
      { p48: 0xffffffff, p4c: 0xffffffff },
      { com: {}, p54: 0 },
      { com: { play: 0x80004001, fmt: 3 }, p54: 0xffffffff },
      { com: { fmt: 0x7fffffff } },
    ];
    const out = F.c3dh_dsbufs(rows, false);
    out.implog = F.c3dh_dsimports(0x7a1b3c4d);
    return out;
  },
  c3dh_ds_play() {
    const F = globalThis.DIFF_FIXTURES;
    const rows = [
      { },
      { com: {} },
      { com: { fmt: 0x88780096 } },
      { com: { play: 0x8878000a } },
      { com: { play: 1, fmt: 5 } },
      { com: { play: 0xffffffff, stop: 7 } },
      { com: { stop: 0x1234, fmt: 0xffffffff } },
      { com: { fmt: 1 }, p48: 0, p4c: 0 },
      { p54: 0 },
      { com: {}, p54: 0xffffffff },
      { com: { play: 0x80004001, fmt: 3 } },
      { com: { fmt: 0x7fffffff } },
    ];
    const out = F.c3dh_dsbufs(rows, false);
    out.implog = F.c3dh_dsimports(0x6b2c1d0e);
    return out;
  },
  // getTimedPosition: p4c == 0 takes the first branch (elapsed = now - p54), otherwise the second; p78 (the length) decides the
  // reset branch (*out1 above it): length 0x10 resets, 0x7fffffff does not.
  c3dh_ds_timed() {
    const F = globalThis.DIFF_FIXTURES;
    const now = 0x00100000;
    const rows = [
      { p4c: 0, p54: now - 1000, p50: 0, rate: 22050, align: 4, p58: 0xffffffff, p78: 0x7fffffff },        // 0
      { p4c: 0, p54: now - 1000, p50: 77, rate: 44100, align: 2, p58: 0xfffffff0, p78: 0x7fffffff },       // 1
      { p4c: 0, p54: now + 5000, p50: 5, rate: 11025, align: 0x10004, p58: 0xffffffff, p78: 0x7fffffff },  // 2 elapsed wraps; align's high half is ignored
      { p4c: 0, p54: 0, p50: 0xffffffff, rate: 0xffffffff, align: 0xffff, p58: 0x0000ffff, p78: 0xffffffff }, // 3 products wrap
      { p4c: 0, p54: now - 3000, p50: 100, rate: 8000, align: 1, p58: 0xffffffff, p78: 0x10, com: {} },     // 4 reset (out1 above length), interface
      { p4c: 0, p54: now - 3000, p50: 100, rate: 8000, align: 1, p58: 0xffffffff, p78: 0x10 },             // 5 reset, no interface
      { p4c: 5, p48: 1000, p50: 20, rate: 22050, align: 4, p58: 0xffffffff, p78: 0x7fffffff },             // 6 second branch
      { p4c: 0x1000, p48: 123456, p50: 3, rate: 44100, align: 2, p58: 0x00ffffff, p78: 0x7fffffff },       // 7
      { p4c: 1, p48: 0xffffffff, p50: 0xffffffff, rate: 22050, align: 4, p58: 0xffffffff, p78: 0xffffffff }, // 8 wraps
      { p4c: 0xffffffff, p48: 7, p50: 0, rate: 1, align: 1, p58: 0, p78: 0 },                              // 9 mask 0
      { p4c: 50, p48: 100000, p50: 9, rate: 22050, align: 4, p58: 0xffffffff, p78: 0x20, com: { play: 3 } }, // 10 reset, Play fails
      { p4c: 50, p48: 100000, p50: 9, rate: 22050, align: 4, p58: 0xffffffff, p78: 0x20, com: { fmt: 0x1111 } }, // 11 reset, format
      { p4c: 0, p54: now - 10, p50: 1, rate: 44100, align: 4, p58: 0xffffffff, p78: 0x7fffffff, com: {} },  // 12 interface present, no reset
      { p4c: 9, p48: 0, p50: 0, rate: 0, align: 0, p58: 0, p78: 0 },                                       // 13 zeros
    ];
    const out = F.c3dh_dsbufs(rows, true);
    out.nrows = ptr(rows.length);
    out.implog = F.c3dh_dsimports(now);
    return out;
  },

  c3dh_listener() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const codes = [null, 0, 0x8878000a, 0x80004001, 0x8007000e, 0x80070057, 0x88780064, 0x88780078, 0x887800aa, 0x887800ab, 0x12345678, 0xffffffff];
    codes.forEach(function (c, i) {
      const o = F.c3dh_fill(__keepAlloc(0x200), 0x200, 0x5c);
      for (let k = 0; k < 0x200; k += 4) o.add(k).writeU32((0x4c000000 + k) >>> 0);
      const com = c === null ? null : F.c3dh_mkcom({ pos: c });
      if (com) out['c' + i] = com;
      o.add(0xd8).writePointer(com ? com : ptr(0));
      out['l' + i] = o;
    });
    return out;
  },

  // SoundWindowObj objects (0x20 bytes): +4 flag byte (upper bytes marker), +8 window handle, +0xc saved procedure.
  c3dh_winobj() {
    const F = globalThis.DIFF_FIXTURES;
    const rows = [[0, 0, 0], [4, 0, 0x1111], [0, 0x2222, 0x3333], [4, 0x2222, 0x3333], [0xff, 0x4444, 0x5555], [0xfb, 0x4444, 0x5555],
                  [5, 0x6666, 0], [4, 0xffffffff, 0x7777], [0x04, 1, 0xffffffff], [0x03, 0x8888, 0x9999], [0x0c, 0xaaaa, 0xbbbb],
                  [0x84, 0xcccc, 0xdddd]];
    const out = {};
    rows.forEach(function (r, i) {
      const o = F.c3dh_fill(__keepAlloc(0x20), 0x20, 0x31);
      o.add(4).writeU32((0x5a5a5a00 | r[0]) >>> 0);
      o.add(8).writeU32(r[1] >>> 0);
      o.add(0xc).writeU32(r[2] >>> 0);
      out['w' + i] = o;
    });
    const s = __importStandIn({ module: 'sound.dll', imports: { 'USER32.dll!SetWindowLongA': { args: 3, ret: 0x77 } } });
    out.implog = s.log;
    return out;
  },

  // postSoundThreadMessage: objects (flag byte +0, thread id +0x1c); the PostThreadMessageA results are the key's schedule.
  c3dh_postthr(sched) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    [[1, 0x1111], [0, 0x2222], [0xff, 0x3333], [0xfe, 0x4444], [3, 0], [1, 0xffffffff]].forEach(function (r, i) {
      const o = F.c3dh_fill(__keepAlloc(0x40), 0x40, 0x31);
      o.writeU32((0x7a7a7a00 | r[0]) >>> 0);
      o.add(0x1c).writeU32(r[1] >>> 0);
      out['p' + i] = o;
    });
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'USER32.dll!PostThreadMessageA': { args: 4, ret: sched },
      'KERNEL32.dll!OutputDebugStringA': { ret: 0 },
      'KERNEL32.dll!Sleep': { args: 1, ret: 0 } } });
    out.implog = s.log;
    return out;
  },
  c3dh_postthr_ok() { return globalThis.DIFF_FIXTURES.c3dh_postthr([1]); },
  c3dh_postthr_r1() { return globalThis.DIFF_FIXTURES.c3dh_postthr([0, 1]); },
  c3dh_postthr_r3() { return globalThis.DIFF_FIXTURES.c3dh_postthr([0, 0, 0, 5]); },

  // Midi_Device::disableOutput: objects (flags dword +0x24, vtable slot +0x50 a recording thiscall stub: +0x30 counts the calls,
  // +0x34 keeps the flags it saw). The module byte 0x100b49bc is set to `byte` with the first thread frozen and restored at the
  // flush; the byte is part of the state.
  c3dh_midioff(byte) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const vt = F.c3dh_fill(__keepAlloc(0x80), 0x80, 0);
    vt.add(0x50).writePointer(F.c3dh_keep(new NativeCallback(function (self) {
      self.add(0x30).writeU32((self.add(0x30).readU32() + 1) >>> 0);
      self.add(0x34).writeU32(self.add(0x24).readU32());
      return 0x2468;
    }, 'uint32', ['pointer'], 'thiscall')));
    [0, 1, 2, 3, 0xff, 0x100, 0x101, 0xffffff01, 0xfffffffe, 0x80000001, 0x55, 0xaa].forEach(function (f, i) {
      const o = F.c3dh_fill(__keepAlloc(0x80), 0x80, 0x66);
      o.writePointer(vt);
      o.add(0x24).writeU32(f >>> 0);
      o.add(0x30).writeU32(0);
      out['m' + i] = o;
    });
    const g = F.c3dh_snd(0xb49bc);
    out.g = g;
    __freeze.thread();
    __freeze.fill(g, 1, byte);
    return out;
  },
  c3dh_midioff_ff() { return globalThis.DIFF_FIXTURES.c3dh_midioff(0xff); },
  c3dh_midioff_02() { return globalThis.DIFF_FIXTURES.c3dh_midioff(0x02); },
  c3dh_midioff_00() { return globalThis.DIFF_FIXTURES.c3dh_midioff(0x00); },
  c3dh_midioff_04() { return globalThis.DIFF_FIXTURES.c3dh_midioff(0x04); },

  // --- sound timer objects for start / killEvent / registerTimerClient / openOutput -----------------------------------
  // A timer block (0x100 bytes): +0 delay, +4 / +8 tick words, +0x18 resolution, +0x1c event flags, +0x3c state flags, +0x40
  // event id, the client list header at +0x4c (front, back, cursor, count) and, for rows with clients, nodes at +0x80.
  c3dh_tblk(o) {
    const F = globalThis.DIFF_FIXTURES;
    const b = F.c3dh_fill(__keepAlloc(0x100), 0x100, 0x6b);
    for (let k = 0; k < 0x100; k += 4) b.add(k).writeU32((0x7b000000 + k) >>> 0);
    const set = (k, v) => b.add(k).writeU32((v >>> 0));
    set(0, o.delay === undefined ? 10 : o.delay);
    set(4, 0x44444444); set(8, 0x88888888);
    set(0x18, o.res === undefined ? 1 : o.res);
    set(0x1c, o.fl1c === undefined ? 1 : o.fl1c);
    set(0x3c, o.fl3c === undefined ? 0 : o.fl3c);
    set(0x40, o.id === undefined ? 0x40404040 : o.id);
    set(0x4c, 0); set(0x50, 0); set(0x54, 0); set(0x58, 0);
    return b;
  },
  // the module re-entrancy word 0x100b49a8: set to 5 for the key (the game's timer thread then leaves its tick handler at once,
  // 0x10008edf), saved first and restored at the flush; it is part of the key's state
  c3dh_tickword(out) {
    const g = globalThis.DIFF_FIXTURES.c3dh_snd(0xb49a8);
    out.g49a8 = g;
    __freeze.setU32(g, 5);
  },
  c3dh_tstart(sched) {
    const F = globalThis.DIFF_FIXTURES;
    const rows = [
      { delay: 10, res: 1, fl1c: 1, fl3c: 0 }, { delay: 0, res: 0, fl1c: 0, fl3c: 0 },
      { delay: 0xffffffff, res: 0xffffffff, fl1c: 0xffffffff, fl3c: 0 }, { delay: 5, res: 2, fl1c: 0, fl3c: 4 },
      { delay: 5, res: 2, fl1c: 0, fl3c: 0xfffffffb }, { delay: 5, res: 2, fl1c: 0, fl3c: 5 }, { delay: 100, res: 10, fl1c: 1, fl3c: 7 },
      { delay: 7, res: 1, fl1c: 1, fl3c: 0x80000000 }, { delay: 7, res: 1, fl1c: 1, fl3c: 0xffffff04 }, { delay: 1000, res: 5, fl1c: 1, fl3c: 2 },
      { delay: 1, res: 1, fl1c: 1, fl3c: 0x100 }, { delay: 3, res: 3, fl1c: 3, fl3c: 0x1000 },
    ];
    const out = {};
    rows.forEach(function (r, i) { out['t' + i] = F.c3dh_tblk(r); });
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'WINMM.dll!timeBeginPeriod': { args: 1, ret: sched.bp },
      'WINMM.dll!timeSetEvent': { ret: sched.se } } });
    out.implog = s.log;
    F.c3dh_tickword(out);
    return out;
  },
  c3dh_tstart_ok() { return globalThis.DIFF_FIXTURES.c3dh_tstart({ bp: 0, se: [0x5151, 0x5252] }); },
  c3dh_tstart_bp61() { return globalThis.DIFF_FIXTURES.c3dh_tstart({ bp: [0x61], se: [0x5151] }); },
  c3dh_tstart_se61() { return globalThis.DIFF_FIXTURES.c3dh_tstart({ bp: 0, se: [0x61] }); },
  c3dh_tkill() {
    const F = globalThis.DIFF_FIXTURES;
    const rows = [0, 1, 3, 5, 7, 0xff, 0xfffffffe, 0x100, 0x101, 4, 0x80000001, 0x55].map(function (f, i) {
      return { fl3c: f, id: 0x5000 + i, res: 1 + i };
    });
    const out = {};
    rows.forEach(function (r, i) { out['t' + i] = F.c3dh_tblk(r); });
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!EnterCriticalSection': { ret: 0 }, 'KERNEL32.dll!LeaveCriticalSection': { ret: 0 },
      'WINMM.dll!timeKillEvent': { ret: 0 }, 'WINMM.dll!timeEndPeriod': { args: 1, ret: 0 } } });
    out.implog = s.log;
    F.c3dh_tickword(out);
    return out;
  },

  // registerTimerClient / Midi_Device::openOutput: six client objects (0x80 bytes, +0x2c a marker) and a fixture sound timer that
  // g_soundTimer 0x100b49ec is pointed at (or NULL) with the game's first thread frozen; both are restored at the flush.
  c3dh_tclients(out) {
    const F = globalThis.DIFF_FIXTURES;
    for (let i = 0; i < 6; i++) {
      const c = F.c3dh_fill(__keepAlloc(0x80), 0x80, 0x35);
      for (let k = 0; k < 0x80; k += 4) c.add(k).writeU32((0x4a000000 + 0x100 * i + k) >>> 0);
      out['k' + i] = c;
    }
    return out;
  },
  c3dh_regtimer(mode) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    F.c3dh_tclients(out);
    const timer = F.c3dh_tblk({ delay: 10, res: 1, fl1c: 1, fl3c: mode === 'busy' ? 4 : 0 });
    out.timer = timer;
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100 });
    out.arena = h.arena;
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!EnterCriticalSection': { ret: 0 }, 'KERNEL32.dll!LeaveCriticalSection': { ret: 0x7e57 },
      'WINMM.dll!timeBeginPeriod': { args: 1, ret: mode === 'bp61' ? [0x61] : 0 },
      'WINMM.dll!timeSetEvent': { ret: mode === 'se61' ? [0x61] : [0x5151] } } });
    out.implog = s.log;
    F.c3dh_tickword(out);
    const g = F.c3dh_snd(0xb49ec);
    out.gtimer = g;
    __freeze.thread();
    __freeze.save(g, 4);
    g.writePointer(mode === 'null' ? ptr(0) : timer);
    return out;
  },
  c3dh_regtimer_ok() { return globalThis.DIFF_FIXTURES.c3dh_regtimer('ok'); },
  c3dh_regtimer_busy() { return globalThis.DIFF_FIXTURES.c3dh_regtimer('busy'); },
  c3dh_regtimer_null() { return globalThis.DIFF_FIXTURES.c3dh_regtimer('null'); },
  c3dh_regtimer_bp61() { return globalThis.DIFF_FIXTURES.c3dh_regtimer('bp61'); },
  c3dh_regtimer_se61() { return globalThis.DIFF_FIXTURES.c3dh_regtimer('se61'); },
  c3dh_midiopen(mode) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    F.c3dh_tclients(out);
    const timer = F.c3dh_tblk({});
    out.timer = timer;
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100 });
    out.arena = h.arena;
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!EnterCriticalSection': { ret: 0 }, 'KERNEL32.dll!LeaveCriticalSection': { ret: 0x7e57 },
      'WINMM.dll!midiOutOpen': { args: 5, impl: function (a, ctx) {
        if (mode === 'fail') return 5;
        a[0].writeU32((0x4d1d0000 + ctx.call) >>> 0);
        return 0;
      }, record: [1, 2, 3, 4] } } });
    out.implog = s.log;
    const g = F.c3dh_snd(0xb49ec);
    out.gtimer = g;
    __freeze.thread();
    __freeze.save(g, 4);
    g.writePointer(mode === 'null' ? ptr(0) : timer);
    return out;
  },
  c3dh_midiopen_ok() { return globalThis.DIFF_FIXTURES.c3dh_midiopen('ok'); },
  c3dh_midiopen_fail() { return globalThis.DIFF_FIXTURES.c3dh_midiopen('fail'); },
  c3dh_midiopen_null() { return globalThis.DIFF_FIXTURES.c3dh_midiopen('null'); },

  // =====================================================================================================================
  // C. mmio handles, wave-in devices, MIDI files, handle lists
  // =====================================================================================================================

  // The module counter of open mmio handles 0x100b4a08 (0 and unmoving at the menu): set to 100 for the key with the game's first
  // thread frozen, restored at the flush; it is part of the key's state ($gcnt). Returns the standard mmio stand-in imports.
  c3dh_mmcount(out) {
    const F = globalThis.DIFF_FIXTURES;
    const g = F.c3dh_snd(0xb4a08);
    out.gcnt = g;
    __freeze.thread();
    __freeze.setU32(g, 100);
  },

  // --- openMmioFile ------------------------------------------------------------------------------------------------
  // Names: the first character '0' makes mmioOpenA fail, anything else returns a handle derived from the second character. The stand-in
  // records the name pointer, the flags and the 18 dwords of the MMIOINFO it was handed.
  c3dh_openmmio() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    ['1a', '0b', '2c', '0d', '9e', '1f', '7g', 'Zh', '0i', '3j', '4k', '5l'].forEach(function (nm, i) {
      const s = F.c3dh_fill(__keepAlloc(0x20), 0x20, 0);
      s.writeUtf8String(nm);
      out['n' + i] = s;
      const o = F.c3dh_fill(__keepAlloc(0x40), 0x40, 0x3a);
      for (let k = 0; k < 0x40; k += 4) o.add(k).writeU32((0x4f000000 + 0x100 * i + k) >>> 0);
      out['o' + i] = o;
    });
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'WINMM.dll!mmioOpenA': { args: 3, impl: function (a, ctx) {
        const nm = a[0].readU8(), c2 = a[0].add(1).readU8();
        const vals = [a[0].toUInt32(), a[2].toUInt32()];
        for (let k = 0; k < 0x12; k++) vals.push(a[1].add(4 * k).readU32());
        ctx.record(vals);
        return nm === 0x30 ? 0 : (0x4d000000 + c2);
      } } } });
    out.implog = s.log;
    F.c3dh_mmcount(out);
    return out;
  },

  // --- Wave_Device::openMemoryMmio ---------------------------------------------------------------------------------------
  c3dh_openmem() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    [[0, 77], [1, 77], [0, 0x666], [0, 0], [0xffffffff, 5], [0, 1000], [0x80000000, 9], [0, 0xffffffff]].forEach(function (r, i) {
      const o = F.c3dh_fill(__keepAlloc(0x100), 0x100, 0x3b);
      for (let k = 0; k < 0x100; k += 4) o.add(k).writeU32((0x4e000000 + 0x100 * i + k) >>> 0);
      o.add(0xb0).writeU32(r[0] >>> 0);
      o.add(0xb8).writeU32(r[1] >>> 0);
      out['o' + i] = o;
    });
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'WINMM.dll!mmioOpenA': { args: 3, impl: function (a, ctx) {
        const vals = [a[0].toUInt32(), a[2].toUInt32()];
        for (let k = 0; k < 0x12; k++) vals.push(a[1].add(4 * k).readU32());
        ctx.record(vals);
        const len = a[1].add(0x14).readU32();
        return len === 0x666 ? 0 : (0x4d000000 + (len & 0xffff));
      } },
      'WINMM.dll!mmioGetInfo': { args: 3, impl: function (a, ctx) {
        for (let k = 0; k < 0x12; k++) a[1].add(4 * k).writeU32((0x6e000000 + (a[0].toUInt32() & 0xffff) * 0x100 + k) >>> 0);
        ctx.record([a[0].toUInt32(), a[2].toUInt32()]);
        return 0;
      } } } });
    out.implog = s.log;
    F.c3dh_mmcount(out);
    return out;
  },

  // --- WaveInDevice_ctor -----------------------------------------------------------------------------------------------
  c3dh_wavein_ctor(numDevs) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 12; i++) {
      const o = F.c3dh_fill(__keepAlloc(0x80), 0x80, 0x3c);
      for (let k = 0; k < 0x80; k += 4) o.add(k).writeU32((0x4d000000 + 0x100 * i + k) >>> 0);
      out['o' + i] = o;
    }
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'WINMM.dll!waveInGetNumDevs': { args: 0, ret: numDevs },
      'WINMM.dll!waveInGetDevCapsA': { args: 3, impl: function (a, ctx) {
        for (let k = 0; k < 0x30; k += 4) a[1].add(k).writeU32((0x7c000000 + k + ctx.call) >>> 0);
        ctx.record([a[0].toUInt32(), a[2].toUInt32()]);
        return 0;
      } } } });
    out.implog = s.log;
    return out;
  },
  c3dh_wavein_ctor_n3() { return globalThis.DIFF_FIXTURES.c3dh_wavein_ctor([3]); },
  c3dh_wavein_ctor_n0() { return globalThis.DIFF_FIXTURES.c3dh_wavein_ctor([0]); },
  c3dh_wavein_ctor_nbig() { return globalThis.DIFF_FIXTURES.c3dh_wavein_ctor([0xffffffff]); },

  // --- stopSoundThreadFull ---------------------------------------------------------------------------------------------
  c3dh_stopthr() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    [[1, 0x1111], [0, 0x2222], [3, 0x3333], [0xff, 0x4444], [0xfe, 0x5555], [0x100, 0x6666], [0x101, 0], [0x81, 0xffffffff],
     [2, 0x7777], [0x55, 0x8888], [0xa5, 0x9999], [0x11, 1]].forEach(function (r, i) {
      const o = F.c3dh_fill(__keepAlloc(0x40), 0x40, 0x3d);
      for (let k = 0; k < 0x40; k += 4) o.add(k).writeU32((0x4b000000 + 0x100 * i + k) >>> 0);
      o.add(0).writeU32((0x5a5a5a00 | (r[0] & 0xff)) >>> 0);
      o.add(0x24).writeU32(r[1] >>> 0);
      out['s' + i] = o;
    });
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!EnterCriticalSection': { ret: 0 }, 'KERNEL32.dll!LeaveCriticalSection': { ret: 0 },
      'KERNEL32.dll!SetEvent': { args: 1, ret: 1 }, 'KERNEL32.dll!Sleep': { args: 1, ret: 0 } } });
    out.implog = s.log;
    return out;
  },

  // --- WaveInDevice_close ----------------------------------------------------------------------------------------------
  // Rows: the handle encodes the waveInClose result (0x70000000 | code; 0 = no handle); headers are 0x20-byte blocks whose first
  // dword 0xbad makes waveInUnprepareHeader fail; the buffer list at +0x5c holds one node per header.
  c3dh_waveinclose() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const rows = [
      { h: 0, hd: [] },                                        // no handle: 0x13
      { code: 0, hd: [] }, { code: 0, hd: [1] }, { code: 0, hd: [1, 1, 1] }, { code: 0, hd: [1, 0xbad, 1] },
      { code: 0, hd: [0xbad] }, { code: 0, hd: [0] }, { code: 0, hd: [1, 0] },
      { code: 1, hd: [1] }, { code: 2, hd: [1] }, { code: 3, hd: [] }, { code: 4, hd: [1] }, { code: 5, hd: [1] }, { code: 6, hd: [1] },
      { code: 7, hd: [1] }, { code: 8, hd: [1] }, { code: 0x1f, hd: [1] }, { code: 0x20, hd: [1] }, { code: 0x21, hd: [1] },
      { code: 0x22, hd: [1] }, { code: 0xffff, hd: [1] },
    ];
    rows.forEach(function (r, i) {
      const b = F.c3dh_fill(__keepAlloc(0x300), 0x300, 0x3e);
      for (let k = 0; k < 0x300; k += 4) b.add(k).writeU32((0x4c000000 + 0x100 * i + k) >>> 0);
      const hdr = (j) => b.add(0x180 + 0x20 * j);
      const node = (j) => b.add(0x100 + 0x10 * j);
      r.hd.forEach(function (v, j) {
        hdr(j).writeU32(v === 0 ? 0x77 : v);
        node(j).writePointer(j > 0 ? node(j - 1) : ptr(0));
        node(j).add(4).writePointer(j < r.hd.length - 1 ? node(j + 1) : ptr(0));
        node(j).add(8).writePointer(v === 0 && j === 0 && false ? ptr(0) : hdr(j));
      });
      if (r.hd.length === 1 && r.hd[0] === 0) node(0).add(8).writePointer(ptr(0));   // a node without a header pointer
      if (r.hd.length === 2 && r.hd[1] === 0) node(1).add(8).writePointer(ptr(0));
      b.add(4).writeU32(0xa5a5a5ff);
      b.add(0x44).writeU32(r.h === undefined ? (0x70000000 | r.code) >>> 0 : r.h);
      b.add(0x5c).writePointer(r.hd.length ? node(0) : ptr(0));
      b.add(0x64).writeU32(0x64646464);
      out['w' + i] = b;
    });
    out.nrows = ptr(rows.length);
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!EnterCriticalSection': { ret: 0 }, 'KERNEL32.dll!LeaveCriticalSection': { ret: 0 },
      'WINMM.dll!waveInStop': { args: 1, ret: 0 }, 'WINMM.dll!waveInReset': { args: 1, ret: 0 },
      'WINMM.dll!waveInUnprepareHeader': { args: 3, impl: function (a, ctx) { return a[1].readU32() === 0xbad ? 33 : 0; } },
      'WINMM.dll!waveInClose': { args: 1, impl: function (a, ctx) { return a[0].toUInt32() & 0xffff; } } } });
    out.implog = s.log;
    return out;
  },

  // --- WaveInDevice_allocCaptureBuffers ------------------------------------------------------------------------------
  c3dh_alloccap() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    // rows: front node's header pointer (null = no front node, 0 = a front node with a NULL header, else a non-zero header word)
    [null, null, null, 0, 0, 0x1234, null, 0xffffffff].forEach(function (front, i) {
      const b = F.c3dh_fill(__keepAlloc(0x100), 0x100, 0x3f);
      for (let k = 0; k < 0x100; k += 4) b.add(k).writeU32((0x4a000000 + 0x100 * i + k) >>> 0);
      if (front !== null) {
        const n = b.add(0x80);
        n.writePointer(ptr(0)); n.add(4).writePointer(ptr(0)); n.add(8).writeU32(front >>> 0);
        b.add(0x5c).writePointer(n); b.add(0x60).writePointer(n);
      } else {
        b.add(0x5c).writePointer(ptr(0)); b.add(0x60).writePointer(ptr(0));
      }
      b.add(0x68).writeU32(i === 6 ? 0xfffffffe : (front !== null ? 1 : 0));
      out['a' + i] = b;
    });
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x800 });
    out.arena = h.arena;
    return out;
  },

  // --- Midi_resetFileState --------------------------------------------------------------------------------------------
  // objects 0x1a0 bytes: +0 mmio handle; g_soundTimer is pointed at a fixture timer of the key's period (or NULL)
  c3dh_midireset(period) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    [0, 5, 0x1234, 0, 0xffffffff, 7].forEach(function (h, i) {
      const o = F.c3dh_fill(__keepAlloc(0x1a0), 0x1a0, 0x2b);
      for (let k = 0; k < 0x1a0; k += 4) o.add(k).writeU32((0x4d000000 + 0x1000 * i + k) >>> 0);
      o.add(0).writeU32(h >>> 0);
      out['r' + i] = o;
    });
    const timer = F.c3dh_fill(__keepAlloc(0x40), 0x40, 0);
    timer.writeU32((period === null ? 0x7e7e7e7e : period) >>> 0);
    out.timer = timer;
    const s = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!mmioClose': { args: 2, ret: 0 } } });
    out.implog = s.log;
    F.c3dh_mmcount(out);
    const g = F.c3dh_snd(0xb49ec);
    out.gtimer = g;
    __freeze.save(g, 4);
    g.writePointer(period === null ? ptr(0) : timer);
    return out;
  },
  c3dh_midireset_null() { return globalThis.DIFF_FIXTURES.c3dh_midireset(null); },
  c3dh_midireset_p1() { return globalThis.DIFF_FIXTURES.c3dh_midireset(1); },
  c3dh_midireset_p7() { return globalThis.DIFF_FIXTURES.c3dh_midireset(7); },
  c3dh_midireset_p999() { return globalThis.DIFF_FIXTURES.c3dh_midireset(999); },
  c3dh_midireset_p1000() { return globalThis.DIFF_FIXTURES.c3dh_midireset(1000); },
  c3dh_midireset_p5000() { return globalThis.DIFF_FIXTURES.c3dh_midireset(5000); },

  // --- handle lists --------------------------------------------------------------------------------------------------
  // Rows build one block each: the object at +0 (flag byte +0x44, flag dword +0x58, list front +0x5c / back +0x60 / count +0x68, +0x74),
  // up to eight 0x10-byte list nodes {prev, next, element} from nodeBase and up to four 0x60-byte elements from elemBase. An element
  // is a handle number (stored at hoff, 0 = none), 'null' (a node whose element pointer is NULL) or an object {h, f34, p3c}.
  // opt: blk (block size), hoff (offset of the handle in an element), nodeBase, elemBase, vtable (slot +0x20 is a recording stub:
  // +0x30 counts the calls and +0x34 keeps the flag byte it saw).
  c3dh_hlists(rows, opt) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const blk = opt.blk || 0x400, nodeBase = opt.nodeBase || 0x100, elemBase = opt.elemBase || 0x200;
    let vt = null;
    if (opt.vtable) {
      vt = F.c3dh_fill(__keepAlloc(0x80), 0x80, 0);
      vt.add(0x20).writePointer(F.c3dh_keep(new NativeCallback(function (self) {
        self.add(0x30).writeU32((self.add(0x30).readU32() + 1) >>> 0);
        self.add(0x34).writeU32(self.add(0x44).readU32());
        return 0x2468;
      }, 'uint32', ['pointer'], 'thiscall')));
    }
    rows.forEach(function (r, i) {
      const b = F.c3dh_fill(__keepAlloc(blk), blk, 0x6b);
      for (let k = 0; k < blk; k += 4) b.add(k).writeU32((0x4d000000 + 0x10000 * i + k) >>> 0);
      const node = (j) => b.add(nodeBase + 0x10 * j);
      const elem = (j) => b.add(elemBase + 0x60 * j);
      const n = r.elems.length;
      r.elems.forEach(function (e, j) {
        const ep = elem(j);
        let ptrv = ptr(0);
        if (e !== 'null') {
          F.c3dh_fill(ep, 0x60, 0x2c);
          for (let k = 0; k < 0x60; k += 4) ep.add(k).writeU32((0x4e000000 + 0x10000 * i + 0x100 * j + k) >>> 0);
          const o = typeof e === 'object' ? e : { h: e };
          ep.add(opt.hoff).writeU32((o.h || 0) >>> 0);
          if (opt.elemInit) opt.elemInit(ep, o);
          ptrv = ep;
        }
        node(j).writePointer(j > 0 ? node(j - 1) : ptr(0));
        node(j).add(4).writePointer(j < n - 1 ? node(j + 1) : ptr(0));
        node(j).add(8).writePointer(ptrv);
      });
      b.add(0x44).writeU32((r.f44 === undefined ? 0 : r.f44) >>> 0);
      b.add(0x58).writeU32((r.f58 === undefined ? 0 : r.f58) >>> 0);
      b.add(0x5c).writePointer(n ? node(0) : ptr(0));
      b.add(0x60).writePointer(n ? node(n - 1) : ptr(0));
      b.add(0x68).writeU32((r.count === undefined ? n : r.count) >>> 0);
      b.add(0x74).writeU32(0x74747474);
      if (opt.vtable) { b.writePointer(vt); b.add(0x30).writeU32(0); }
      if (opt.objInit) opt.objInit(b, r);
      out['h' + i] = b;
    });
    return out;
  },
  c3dh_listrows_common() {
    // [flags44, elems]
    return [
      { f44: 0, elems: [] }, { f44: 2, elems: [0x5001] }, { f44: 0, elems: [0x5002] }, { f44: 0, elems: [0] },
      { f44: 0, elems: [0x5003, 0x5004, 0x5005] }, { f44: 0, elems: [0x5006, 0, 0x5007, 0x5008] },
      { f44: 0, elems: [0x5009, 'null', 0x500a] }, { f44: 0, elems: ['null', 0x500b] }, { f44: 3, elems: [0x500c, 0x500d] },
      { f44: 0, elems: [0x500e, 0x500f], count: 0 }, { f44: 0xfd, elems: [0, 0x5010] }, { f44: 0x02, elems: [] },
      { f44: 0, elems: [0x5011, 'null', 0x5012, 'null', 0x5013] },
    ];
  },
  c3dh_mmioheader() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const s = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!mmioClose': { args: 2, ret: 0 } } });
    out.implog = s.log;
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    out.freelog = h.log;
    F.c3dh_mmcount(out);
    return out;
  },
  c3dh_hl_closenode() {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3dh_hlists(F.c3dh_listrows_common(), { hoff: 0x44, vtable: true });
    Object.assign(out, F.c3dh_mmioheader());
    return out;
  },
  c3dh_hl_freehandle() {
    const F = globalThis.DIFF_FIXTURES;
    // the handle is the FIRST dword of an element; element handles are never zero here (mmioClose has no test)
    const rows = F.c3dh_listrows_common();
    const out = F.c3dh_hlists(rows, { hoff: 0 });
    Object.assign(out, F.c3dh_mmioheader());
    return out;
  },
  c3dh_hl_wsdestroy() {
    const F = globalThis.DIFF_FIXTURES;
    // Ch5c178_dtor (the base destructor the function ends with) needs +0x40 = 0, +0x50 = 0 and bit 1 of +0x44 clear
    const rows = F.c3dh_listrows_common().map(function (r) { return Object.assign({}, r, { f44: r.f44 & ~2 }); });
    const out = F.c3dh_hlists(rows, { hoff: 0x44, objInit: function (b) { b.add(0x40).writeU32(0); b.add(0x50).writeU32(0); } });
    Object.assign(out, F.c3dh_mmioheader());
    return out;
  },
  // Sound_stop / Sound_freeBufferList: the module's live handle-record list (front 0x100b49c8, back +4, cursor +8, count +0xc) is
  // replaced for the key by a fixture list of records pointing at the elements (`live`) or left empty as at the menu.
  c3dh_hl_sound(live) {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3dh_hlists(F.c3dh_listrows_common(), { hoff: 0x44, vtable: true });
    Object.assign(out, F.c3dh_mmioheader());
    const g = F.c3dh_snd(0xb49c8);
    out.glist = g;
    if (live) {
      // one record per element with a non-zero handle, in row order, plus one record for a foreign pointer
      const recs = [];
      for (let i = 0; i < 13; i++) {
        for (let j = 0; j < 4; j++) {
          const b = out['h' + i];
          const e = b.add(0x200 + 0x60 * j);
          if (j < F.c3dh_listrows_common()[i].elems.length && F.c3dh_listrows_common()[i].elems[j] !== 'null' && e.add(0x44).readU32() !== 0) recs.push(e);
        }
      }
      const blk = F.c3dh_fill(__keepAlloc(0x200), 0x200, 0x7d);
      const rec = (k) => blk.add(0x10 * k);
      const lastRec = out.h4.add(0x200 + 0x60 * 2);   // row 4's third element is released by every list function: its record goes last (no next)
      const rest = recs.filter(function (r) { return r.toString() !== lastRec.toString(); });
      const all = [rest[0], ptr(0x1234)].concat(rest.slice(1)).concat([lastRec]);   // front record, a foreign pointer, the rest, the last record
      all.forEach(function (e, k) {
        rec(k).writePointer(k > 0 ? rec(k - 1) : ptr(0));
        rec(k).add(4).writePointer(k < all.length - 1 ? rec(k + 1) : ptr(0));
        rec(k).add(8).writePointer(e);
        rec(k).add(0xc).writeU32(0x7d000000 + k);
      });
      out.liveblk = blk;
      
      __freeze.save(g, 0x10);
      g.writePointer(rec(0));
      g.add(4).writePointer(rec(all.length - 1));
      g.add(8).writePointer(rec(1));
      g.add(0xc).writeU32(all.length);
    }
    return out;
  },
  c3dh_hl_sound_empty() { return globalThis.DIFF_FIXTURES.c3dh_hl_sound(false); },
  c3dh_hl_sound_live() { return globalThis.DIFF_FIXTURES.c3dh_hl_sound(true); },

  // WaveStream::scalarDeletingDtor: the same objects as WaveStream::destroy (the flags decide the delete of the object itself)
  c3dh_hl_wsdel() { return globalThis.DIFF_FIXTURES.c3dh_hl_wsdestroy(); },

  // Midi_Device::closeFile: objects of 0x600 bytes (handle at +0, flags +0x4c, reader list +0x19c / +0x1a0 / +0x1a8), nodes from
  // +0x200, readers (0x60 bytes: +0xc empty sub-list, +0x34 flags, +0x3c a block pointer) from +0x300.
  c3dh_midiclose() {
    const F = globalThis.DIFF_FIXTURES;
    const rows = [
      { f4c: 1, h: 0x6001, elems: [] }, { f4c: 0, h: 0x6002, elems: [] }, { f4c: 1, h: 0, elems: [] },
      { f4c: 1, h: 0x6003, elems: [{ f34: 0, p3c: 0 }] }, { f4c: 0, h: 0x6004, elems: [{ f34: 0, p3c: 0x7000 }] },
      { f4c: 0xff, h: 0x6005, elems: [{ f34: 4, p3c: 0x7010 }, { f34: 0, p3c: 0x7020 }, { f34: 0xfb, p3c: 0 }] },
      { f4c: 0x100, h: 0x6006, elems: [{ f34: 4, p3c: 0 }, 'null', { f34: 0, p3c: 0x7030 }] },
      { f4c: 3, h: 0x6007, elems: ['null', { f34: 0, p3c: 0x7040 }] }, { f4c: 1, h: 0x6008, elems: [{ f34: 0xff, p3c: 0x7050 }], count: 0 },
      { f4c: 0x101, h: 0xffffffff, elems: [{ f34: 0x04, p3c: 0x7060 }, { f34: 0x00, p3c: 0x7070 }, { f34: 0x04, p3c: 0 }, { f34: 1, p3c: 1 }] },
    ];
    const out = F.c3dh_hlists(rows, {
      blk: 0x600, nodeBase: 0x200, elemBase: 0x300, hoff: 0x20,
      elemInit: function (ep, o) {
        ep.add(0xc).writeU32(0); ep.add(0x10).writeU32(0); ep.add(0x18).writeU32(0);
        ep.add(0x34).writeU32(0x5a5a5a00 | ((o.f34 || 0) & 0xff));
        ep.add(0x3c).writeU32((o.p3c || 0) >>> 0);
      },
      objInit: function (b, r) {
        b.writeU32(r.h >>> 0);
        b.add(0x4c).writeU32((0x5a5a5a00 | (r.f4c & 0xff)) >>> 0);
        b.add(0x164).writeU32(0x16416416); b.add(0x16c).writeU32(0x16c16c16);
        // the list header of a Midi_Device sits at +0x19c / +0x1a0 / +0x1a8
        b.add(0x19c).writePointer(b.add(0x5c).readPointer());
        b.add(0x1a0).writePointer(b.add(0x60).readPointer());
        b.add(0x1a8).writeU32(b.add(0x68).readU32());
        // (+0x5c / +0x60 / +0x68 hold the same values here; they are untouched by closeFile)
      },
    });
    Object.assign(out, F.c3dh_mmioheader());
    return out;
  },

  // =====================================================================================================================
  // E. Swapped module globals, codec allocators, the CRT's per-thread data
  // =====================================================================================================================

  // --- Sound_findByName ------------------------------------------------------------------------------------------------
  // The module list head 0x100b4fe8 and the walk cursor 0x100b4ff0 (0 and unmoving at the menu) are pointed at a fixture list for the key
  // with the game's first thread frozen and restored at the flush. A sound is a 0x10-byte record whose first dword points at its name.
  // shapes: names in list order, 'null' = a node whose sound pointer is NULL, [] = empty list.
  c3dh_findname(shape) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const blk = F.c3dh_fill(__keepAlloc(0x200), 0x200, 0x5b);
    const node = (j) => blk.add(0x100 + 0x10 * j);
    const snd = (j) => blk.add(0x180 + 0x10 * j);
    shape.forEach(function (nm, j) {
      node(j).writePointer(j > 0 ? node(j - 1) : ptr(0));
      node(j).add(4).writePointer(j < shape.length - 1 ? node(j + 1) : ptr(0));
      if (nm === 'null') { node(j).add(8).writePointer(ptr(0)); return; }
      const s = F.c3dh_fill(__keepAlloc(0x20), 0x20, 0);
      s.writeUtf8String(nm);
      out['nm' + j] = s;
      snd(j).writePointer(s);
      node(j).add(8).writePointer(snd(j));
    });
    out.blk = blk;
    ['alpha', 'beta', 'gamma', 'delta', '', 'alph', 'alphabet', 'a', 'ga', 'BETA'].forEach(function (nm, i) {
      const s = F.c3dh_fill(__keepAlloc(0x20), 0x20, 0);
      s.writeUtf8String(nm);
      out['s' + i] = s;
    });
    const gh = F.c3dh_snd(0xb4fe8), gc = F.c3dh_snd(0xb4ff0);
    out.gh = gh; out.gc = gc;
    __freeze.thread();
    __freeze.save(gh, 4);
    __freeze.save(gc, 4);
    gh.writePointer(shape.length ? node(0) : ptr(0));
    gc.writePointer(ptr(0x77770000));
    return out;
  },
  c3dh_findname_l0() { return globalThis.DIFF_FIXTURES.c3dh_findname([]); },
  c3dh_findname_l1() { return globalThis.DIFF_FIXTURES.c3dh_findname(['alpha']); },
  c3dh_findname_l3() { return globalThis.DIFF_FIXTURES.c3dh_findname(['alpha', 'beta', 'gamma']); },
  c3dh_findname_lnull() { return globalThis.DIFF_FIXTURES.c3dh_findname(['alpha', 'null', 'gamma']); },
  c3dh_findname_lhead() { return globalThis.DIFF_FIXTURES.c3dh_findname(['null', 'beta']); },

  // --- Midi_Device::flushMessages ---------------------------------------------------------------------------------------
  // g_midiDevice 0x100b4a1c is pointed at a fixture device (handle at +0x3c) with the first thread frozen. Ten Midi_Device objects of
  // 0x480 bytes hold the 0x80 pending-message dwords from +0x258.
  c3dh_flush(handle) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const pats = [
      [], [[0, 0x00112233]], [[0x7f, 0x00ff0001]], [[3, 0x80ff02], [5, 0x05000090]], [[0, 0x11000000], [1, 0x00ff00ff]],
      [[10, 0x00aa0001], [11, 0x00bb0002], [12, 0x00cc0003]], [[127, 0x00010090]], [[126, 0xff000001], [127, 0x7f0000ff]],
      [[2, 0x00300000], [64, 0x00ff00ff], [65, 0x00ff0000]], [[1, 0xffffffff], [2, 0x01010101], [3, 0x02020202], [4, 0x10000000]],
    ];
    pats.forEach(function (pt, i) {
      const o = F.c3dh_fill(__keepAlloc(0x480), 0x480, 0x6c);
      for (let k = 0; k < 0x480; k += 4) o.add(k).writeU32((0x4d000000 + 0x1000 * i + k) >>> 0);
      for (let k = 0; k < 0x80; k++) o.add(0x258 + 4 * k).writeU32(0);
      pt.forEach(function (e) { o.add(0x258 + 4 * e[0]).writeU32(e[1] >>> 0); });
      out['o' + i] = o;
    });
    const dev = F.c3dh_fill(__keepAlloc(0x80), 0x80, 0x33);
    dev.add(0x3c).writeU32(handle >>> 0);
    out.dev = dev;
    const s = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!midiOutShortMsg': { args: 2, ret: 0 } } });
    out.implog = s.log;
    const g = F.c3dh_snd(0xb4a1c);
    out.gdev = g;
    __freeze.thread();
    __freeze.save(g, 4);
    g.writePointer(dev);
    return out;
  },
  c3dh_flush_h() { return globalThis.DIFF_FIXTURES.c3dh_flush(0x4d1d0001); },
  c3dh_flush_h0() { return globalThis.DIFF_FIXTURES.c3dh_flush(0); },

  // --- Voice_requestCmd12 -----------------------------------------------------------------------------------------------
  // Voice objects (0x100 bytes): vtable slot +0xb4 is a recording thiscall stub returning the dword at +0x30 (the limit) and counting its
  // calls at +0x34. g_midiDevice 0x100b4a1c is pointed (first thread frozen) at a fixture command queue (CommandQueue layout: ring at +0x50,
  // read index +0x4050, write index +0x4054; `mode` 'q' = empty, 'full' = write index right below the read index, 'null' = no device).
  c3dh_req12(mode) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const vt = F.c3dh_fill(__keepAlloc(0x100), 0x100, 0);
    vt.add(0xb4).writePointer(F.c3dh_keep(new NativeCallback(function (self) {
      self.add(0x34).writeU32((self.add(0x34).readU32() + 1) >>> 0);
      return self.add(0x30).readU32();
    }, 'uint32', ['pointer'], 'thiscall')));
    [0, 1, 5, 0x10, 0xff, 0x100, 0xffffffff, 0x80000000].forEach(function (lim, i) {
      const v = F.c3dh_fill(__keepAlloc(0x100), 0x100, 0x4c);
      for (let k = 0; k < 0x100; k += 4) v.add(k).writeU32((0x4a000000 + 0x1000 * i + k) >>> 0);
      v.writePointer(vt);
      v.add(0x30).writeU32(lim >>> 0);
      v.add(0x34).writeU32(0);
      out['v' + i] = v;
    });
    const q = F.c3dh_fill(__keepAlloc(0x4060), 0x4060, 0x71);
    for (let s = 0; s < 0x1000; s++) q.add(0x50 + 4 * s).writeU32((0x5a000000 + s) >>> 0);
    q.add(0x4050).writeU32(mode === 'full' ? 6 : 0);
    q.add(0x4054).writeU32(mode === 'full' ? 5 : 0);
    out.q = q;
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100 });
    out.arena = h.arena;
    const g = F.c3dh_snd(0xb4a1c);
    out.gdev = g;
    __freeze.thread();
    __freeze.save(g, 4);
    g.writePointer(mode === 'null' ? ptr(0) : q);
    return out;
  },
  c3dh_req12_q() { return globalThis.DIFF_FIXTURES.c3dh_req12('q'); },
  c3dh_req12_full() { return globalThis.DIFF_FIXTURES.c3dh_req12('full'); },
  c3dh_req12_null() { return globalThis.DIFF_FIXTURES.c3dh_req12('null'); },

  // --- Seq_cmdHandler12 ---------------------------------------------------------------------------------------------------
  // Seq blocks of 0x300 bytes: the element list header is at +0x1fc (front) / +0x204 (cursor) (Seq_listGetAt reads +0x19c / +0x1a4 of
  // this+0x60); nodes {prev, next, element} from +0x100, elements (0x40 bytes, flag byte +0x38) from +0x180.
  c3dh_seqcmd12() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    [[], ['e'], ['e', 'e'], ['e', 'e', 'e'], ['e', 'null', 'e'], ['null']].forEach(function (shape, i) {
      const b = F.c3dh_fill(__keepAlloc(0x300), 0x300, 0x6d);
      for (let k = 0; k < 0x300; k += 4) b.add(k).writeU32((0x4a000000 + 0x1000 * i + k) >>> 0);
      const node = (j) => b.add(0x100 + 0x10 * j);
      const elem = (j) => b.add(0x180 + 0x40 * j);
      shape.forEach(function (e, j) {
        node(j).writePointer(j > 0 ? node(j - 1) : ptr(0));
        node(j).add(4).writePointer(j < shape.length - 1 ? node(j + 1) : ptr(0));
        if (e === 'null') { node(j).add(8).writePointer(ptr(0)); return; }
        elem(j).add(0x38).writeU32((0x5a5a5a00 | (0x25 + j * 0x10)) >>> 0);
        node(j).add(8).writePointer(elem(j));
      });
      b.add(0x1fc).writePointer(shape.length ? node(0) : ptr(0));
      b.add(0x204).writeU32(0x20420420);
      out['q' + i] = b;
    });
    return out;
  },

  // --- codecSetParams ---------------------------------------------------------------------------------------------------
  // The three codec words 0x100b58a0 / a4 / a8 (all 0 at the menu, written only by this function) are preset to `vals` and restored.
  c3dh_codecset(vals) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const g = F.c3dh_snd(0xb58a0);
    out.g = g;
    __freeze.save(g, 12);
    vals.forEach(function (v, i) { g.add(4 * i).writeU32(v >>> 0); });
    return out;
  },
  c3dh_codecset_v0() { return globalThis.DIFF_FIXTURES.c3dh_codecset([0, 0, 0]); },
  c3dh_codecset_v1() { return globalThis.DIFF_FIXTURES.c3dh_codecset([0xa, 0xb4, 1]); },
  c3dh_codecset_v2() { return globalThis.DIFF_FIXTURES.c3dh_codecset([1, 2, 3]); },
  c3dh_codecset_v3() { return globalThis.DIFF_FIXTURES.c3dh_codecset([0xffffffff, 0xffffffff, 0xffffffff]); },
  c3dh_codecset_v4() { return globalThis.DIFF_FIXTURES.c3dh_codecset([0xa, 0, 0]); },
  c3dh_codecset_v5() { return globalThis.DIFF_FIXTURES.c3dh_codecset([0, 0xb4, 0]); },
  c3dh_codecset_v6() { return globalThis.DIFF_FIXTURES.c3dh_codecset([0, 0, 1]); },
  c3dh_codecset_v7() { return globalThis.DIFF_FIXTURES.c3dh_codecset([5, 0xb4, 1]); },
  c3dh_codecset_v8() { return globalThis.DIFF_FIXTURES.c3dh_codecset([0xa, 5, 1]); },
  c3dh_codecset_v9() { return globalThis.DIFF_FIXTURES.c3dh_codecset([0xa, 0xb4, 5]); },

  // --- codecAllocContextA / B, Channel_setFormat -----------------------------------------------------------------------------
  // malloc 0x10042acd is replaced for the key by the heap stand-in (arena of `size` bytes; fill byte; failMask 1 = the first call fails).
  // The codec words are saved and restored (codecInitContextA / B reach codecSetParams).
  c3dh_allocctx(size, fill, failMask) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x42acd], arenaSize: size, fill: fill, failMask: failMask });
    out.arena = h.arena;
    const g = F.c3dh_snd(0xb58a0);
    out.gcodec = g;
    __freeze.save(g, 12);
    return out;
  },
  c3dh_allocctx_a() { return globalThis.DIFF_FIXTURES.c3dh_allocctx(0x2600, 0xcd, 0); },
  c3dh_allocctx_a5a() { return globalThis.DIFF_FIXTURES.c3dh_allocctx(0x2600, 0x5a, 0); },
  c3dh_allocctx_aoom() { return globalThis.DIFF_FIXTURES.c3dh_allocctx(0x2600, 0xcd, 1); },
  c3dh_allocctx_b() { return globalThis.DIFF_FIXTURES.c3dh_allocctx(0xc40, 0xcd, 0); },
  c3dh_allocctx_b5a() { return globalThis.DIFF_FIXTURES.c3dh_allocctx(0xc40, 0x5a, 0); },
  c3dh_allocctx_boom() { return globalThis.DIFF_FIXTURES.c3dh_allocctx(0xc40, 0xcd, 1); },
  c3dh_setfmt(failMask) {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3dh_allocctx(0xc40, 0xcd, failMask);
    for (let i = 0; i < 6; i++) {
      const o = F.c3dh_fill(__keepAlloc(0x100), 0x100, 0x3a);
      for (let k = 0; k < 0x100; k += 4) o.add(k).writeU32((0x4d000000 + 0x1000 * i + k) >>> 0);
      out['o' + i] = o;
    }
    for (let i = 0; i < 2; i++) {
      const f = F.c3dh_fill(__keepAlloc(0x20), 0x20, 0);
      F.c3dh_words(f, [0x11110000 + i, 0x22220000 + i, 0x33330000 + i, 0x44440000 + i, 0x5555 + i]);
      out['f' + i] = f;
    }
    return out;
  },
  c3dh_setfmt_ok() { return globalThis.DIFF_FIXTURES.c3dh_setfmt(0); },
  c3dh_setfmt_oom() { return globalThis.DIFF_FIXTURES.c3dh_setfmt(1); },

  // --- _errno / __doserrno ---------------------------------------------------------------------------------------------
  // The CRT's per-thread data lookup (_getptd 0x10044171) reads GetLastError, TlsGetValue and restores the last error with SetLastError.
  // mode 'a' / 'b': TlsGetValue returns a fixture block; 'new': it returns 0 and the function builds the block with calloc 0x100458a1 (heap
  // stand-in, zero-filled), TlsSetValue and GetCurrentThreadId.
  c3dh_errno(mode) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const ptd = F.c3dh_fill(__keepAlloc(0x100), 0x100, 0x5d);
    for (let k = 0; k < 0x100; k += 4) ptd.add(k).writeU32((0x4d000000 + k + (mode === 'b' ? 0x1000 : 0)) >>> 0);
    out.ptd = ptd;
    const imports = {
      'KERNEL32.dll!GetLastError': { args: 0, ret: mode === 'b' ? 0xffffffff : 5 },
      'KERNEL32.dll!TlsGetValue': { args: 1, ret: mode === 'new' ? 0 : ptd },
      'KERNEL32.dll!SetLastError': { args: 1, ret: 0 },
      'KERNEL32.dll!TlsSetValue': { args: 2, ret: 1 },
      'KERNEL32.dll!GetCurrentThreadId': { args: 0, ret: 0x7777 },
    };
    const s = __importStandIn({ module: 'sound.dll', imports: imports });
    out.implog = s.log;
    if (mode === 'new') {
      const h = __heapStandIn({ module: 'sound.dll', alloc: [{ rva: 0x458a1, args: 2, sizeArgs: 2, fill: 0 }], arenaSize: 0x100 });
      out.arena = h.arena;
    }
    return out;
  },
  c3dh_errno_a() { return globalThis.DIFF_FIXTURES.c3dh_errno('a'); },
  c3dh_errno_b() { return globalThis.DIFF_FIXTURES.c3dh_errno('b'); },
  c3dh_errno_new() { return globalThis.DIFF_FIXTURES.c3dh_errno('new'); },

  // --- sbhRegionLocate -------------------------------------------------------------------------------------------------
  // The old small-block heap's static region list (head node 0x100653d8; unused at run time, the system heap is selected) is relinked for the
  // key: head -> region A (0x00100000, 0x00500000) -> region B (0x80000000, 0x80400000) -> head. Restored at the flush.
  c3dh_sbhloc() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const regs = F.c3dh_fill(__keepAlloc(0x80), 0x80, 0x33);
    const A = regs, B = regs.add(0x40);
    const head = F.c3dh_snd(0x653d8);
    out.head = head;
    A.writePointer(B); A.add(0x10).writeU32(0x00100000); A.add(0x14).writeU32(0x00500000);
    B.writePointer(head); B.add(0x10).writeU32(0x80000000); B.add(0x14).writeU32(0x80400000);
    out.regs = regs;
    out.A = A; out.B = B;
    const outs = F.c3dh_fill(__keepAlloc(0x20), 0x20, 0x77);
    out.outs = outs; out.o1 = outs; out.o2 = outs.add(8);
    __freeze.thread();
    __freeze.save(head, 0x18);
    head.writePointer(A);
    head.add(0x10).writeU32(0xffffffff);
    head.add(0x14).writeU32(0);
    return out;
  },

  // --- WaveInDevice_dtor -------------------------------------------------------------------------------------------------
  // Blocks of 0x300 bytes: the device at +0 (flag dword +4, handle +0x44 = 0x70000000 | the waveInClose result, buffer list +0x5c / +0x60 /
  // +0x68), list nodes from +0x100 and 0x20-byte buffer headers from +0x180 whose first dword is the buffer pointer (never dereferenced:
  // operator delete is the recording stand-in; 0xbad also makes waveInUnprepareHeader fail inside WaveInDevice_close).
  c3dh_wavedtor() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const rows = [
      { f4: 0, h: 0, hd: [] }, { f4: 1, code: 0, hd: [] }, { f4: 2, code: 0, hd: [0x6a000001] },
      { f4: 3, code: 0, hd: [0x6a000002, 0x6a000003, 0x6a000004] }, { f4: 2, h: 0, hd: [0x6a000005] },
      { f4: 2, code: 4, hd: [0x6a000006, 0x6a000007] }, { f4: 1, code: 0, hd: [0xbad, 0x6a000008] },
      { f4: 0xff, code: 0, hd: ['null', 0x6a000009] }, { f4: 0, code: 0, hd: [0x6a00000a] },
      { f4: 0, code: 0, hd: [0x6a00000b, 'null', 0x6a00000c] }, { f4: 0xfd, code: 3, hd: [0x6a00000d, 0x6a00000e] },
      { f4: 2, code: 0x21, hd: [0x6a00000f, 0xbad, 0x6a000010] },
      { f4: 0, code: 0, hd: [0x6a000011, 'null', 0x6a000012, 'null', 0x6a000013] },
    ];
    rows.forEach(function (r, i) {
      const b = F.c3dh_fill(__keepAlloc(0x300), 0x300, 0x3e);
      for (let k = 0; k < 0x300; k += 4) b.add(k).writeU32((0x4c000000 + 0x100 * i + k) >>> 0);
      const hdr = (j) => b.add(0x180 + 0x20 * j);
      const node = (j) => b.add(0x100 + 0x10 * j);
      const n = r.hd.length;
      r.hd.forEach(function (v, j) {
        node(j).writePointer(j > 0 ? node(j - 1) : ptr(0));
        node(j).add(4).writePointer(j < n - 1 ? node(j + 1) : ptr(0));
        if (v === 'null') { node(j).add(8).writePointer(ptr(0)); return; }
        hdr(j).writeU32(v >>> 0);
        node(j).add(8).writePointer(hdr(j));
      });
      b.add(4).writeU32((0xa5a5a500 | r.f4) >>> 0);
      b.add(0x44).writeU32(r.h === undefined ? (0x70000000 | r.code) >>> 0 : r.h);
      b.add(0x5c).writePointer(n ? node(0) : ptr(0));
      b.add(0x60).writePointer(n ? node(n - 1) : ptr(0));
      b.add(0x64).writeU32(0x64646464);
      b.add(0x68).writeU32(n);
      out['w' + i] = b;
    });
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!EnterCriticalSection': { ret: 0 }, 'KERNEL32.dll!LeaveCriticalSection': { ret: 0 },
      'WINMM.dll!waveInStop': { args: 1, ret: 0 }, 'WINMM.dll!waveInReset': { args: 1, ret: 0 },
      'WINMM.dll!waveInUnprepareHeader': { args: 3, impl: function (a, ctx) { return a[1].readU32() === 0xbad ? 33 : 0; } },
      'WINMM.dll!waveInClose': { args: 1, impl: function (a, ctx) { return a[0].toUInt32() & 0xffff; } } } });
    out.implog = s.log;
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    out.freelog = h.log;
    return out;
  },

  // =====================================================================================================================
  // G. Heap-only functions
  // =====================================================================================================================

  c3dh_cstr(s) {
    const p = globalThis.DIFF_FIXTURES.c3dh_fill(__keepAlloc(0x80), 0x80, 0);
    p.writeUtf8String(s);
    return p;
  },

  // --- Sound_setName ----------------------------------------------------------------------------------------------
  // Sound objects (0x40 bytes): +0x1c the current name pointer (0, or an address that is only recorded by the stand-in's operator delete), +0x20
  // the path prefix pointer (0 = none). Names: strings with and without backslashes, empty, a lone backslash, a trailing backslash.
  c3dh_setname() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const rows = [[0, null], [0x6a001000, null], [0, 'C:\\snd\\'], [0x6a001004, 'base\\'], [0, ''], [0x6a001008, 'x'],
                  [0, 'path\\to\\'], [0x6a00100c, null], [0, 'a'], [0x6a001010, 'longer_prefix_dir\\sub\\'], [0, 'p'], [0x6a001014, '']];
    rows.forEach(function (r, i) {
      const o = F.c3dh_fill(__keepAlloc(0x40), 0x40, 0x3a);
      for (let k = 0; k < 0x40; k += 4) o.add(k).writeU32((0x4d000000 + 0x100 * i + k) >>> 0);
      o.add(0x1c).writeU32(r[0]);
      o.add(0x20).writePointer(r[1] === null ? ptr(0) : F.c3dh_cstr(r[1]));
      out['o' + i] = o;
    });
    ['wave.wav', 'dir\\file.wav', 'a\\b\\c.mid', '\\', 'end\\', '', 'x', 'nine_char', 'C:\\game\\sounds\\boom.wav', 'two\\\\slashes.wav'].forEach(function (s, i) {
      out['n' + i] = F.c3dh_cstr(s);
    });
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], free: [0x4249a], arenaSize: 0x400 });
    out.arena = h.arena;
    out.freelog = h.log;
    return out;
  },

  // --- allocVoiceSlot ---------------------------------------------------------------------------------------------
  // Blocks of 0x400 bytes: the slot table at +0 (entry i at +4+4i), old slots A at +0x100 and B at +0x140 (+0 last index, +4 first index), and
  // the descriptor at +0x180 ({+0 last index, +4 first index, +8 value, +0xc name}).
  c3dh_allocslot() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const rows = [
      { desc: [4, 2, 0x11, 'alpha'] },
      { ent: { 3: 'A', 4: 'A', 5: 'A' }, A: [5, 3], desc: [4, 2, 0x12, 'beta'] },
      { ent: { 1: 'A', 2: 'A', 4: 'B' }, A: [2, 1], B: [4, 4], desc: [5, 0, 0x13, 'gamma'] },
      { desc: [1, 5, 0x14, 'x'] },
      { ent: { 3: 'A' }, A: [3, 3], desc: [3, 3, 0x15, 'delta'] },
      { desc: [0, 0, 0x16, 'eps'] },
      { ent: { 2: 'A', 5: 0x6b00 }, A: [2, 5], desc: [3, 1, 0x17, 'zeta'] },
      { desc: [7, 6, 0x18, 'nine_char'] },
      { ent: { 0: 'A', 1: 'A', 2: 'A', 3: 'A' }, A: [3, 0], desc: [2, 1, 0x19, 'a_name_of_thirty_chars_exactly'] },
      { ent: { 6: 'A' }, A: [6, 6], desc: [6, 6, 0x1a, ''] },
      { ent: { 2: 'A', 3: 'B' }, A: [2, 2], B: [3, 3], desc: [3, 1, 0x1b, 'quirk'] },
      { desc: [7, 0xffffffff, 0x1c, 'huge_lo'] },
    ];
    rows.forEach(function (r, i) {
      const b = F.c3dh_fill(__keepAlloc(0x400), 0x400, 0x6e);
      for (let k = 0; k < 0x400; k += 4) b.add(k).writeU32((0x4d000000 + 0x1000 * i + k) >>> 0);
      for (let k = 0; k < 0x100; k += 4) b.add(k).writeU32(0);
      const A = b.add(0x100), B = b.add(0x140), d = b.add(0x180);
      if (r.A) { A.writeU32(r.A[0]); A.add(4).writeU32(r.A[1]); }
      if (r.B) { B.writeU32(r.B[0]); B.add(4).writeU32(r.B[1]); }
      Object.keys(r.ent || {}).forEach(function (k) {
        const v = r.ent[k];
        b.add(4 + 4 * parseInt(k)).writePointer(v === 'A' ? A : (v === 'B' ? B : ptr(v)));
      });
      d.writeU32(r.desc[0] >>> 0); d.add(4).writeU32(r.desc[1] >>> 0); d.add(8).writeU32(r.desc[2] >>> 0);
      F.c3dh_fill(d.add(0xc), 0x30, 0);
      d.add(0xc).writeUtf8String(r.desc[3]);
      out['b' + i] = b;
      out['t' + i] = b;
      out['d' + i] = d;
    });
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], free: [0x4249a], arenaSize: 0x130 });
    out.arena = h.arena;
    out.freelog = h.log;
    return out;
  },

  // --- buildVoiceRxName ----------------------------------------------------------------------------------------------
  c3dh_buildrx() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 10; i++) {
      const o = F.c3dh_fill(__keepAlloc(0x60), 0x60, 0x3b);
      for (let k = 0; k < 0x60; k += 4) o.add(k).writeU32((0x4a000000 + 0x100 * i + k) >>> 0);
      out['r' + i] = o;
    }
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x40 });
    out.arena = h.arena;
    return out;
  },

  // =====================================================================================================================
  // H. Notification posts
  // =====================================================================================================================

  // PostMessageA / PostThreadMessageA are stood in (results 1). The sound-thread post goes to the module object 0x100b5038 (flag byte 0 at the menu: the post
  // returns 0x15 before any import), which is only read.
  // The sound engine g_soundEngine 0x100b4a20 is created a moment after the window; its flag word +0x1a4 (bit 5 = 0x20) selects the PostMessageA arm of
  // postMidiNotification / onWaveInCallbackMessage. Wait (up to 10 s) until the pointer is set and the bit is up, as it is for the rest of the run.
  c3dh_waitengine() {
    const g = globalThis.DIFF_FIXTURES.c3dh_snd(0xb4a20);
    for (let i = 0; i < 200; i++) {
      const e = g.readPointer();
      if (!e.isNull() && (e.add(0x1a4).readU32() & 0x20) !== 0) return;
      Thread.sleep(0.05);
    }
    throw new Error('c3dh: the sound engine never reached flag bit 5');
  },
  c3dh_postimports() {
    return __importStandIn({ module: 'sound.dll', imports: {
      'USER32.dll!PostMessageA': { args: 4, ret: 1 }, 'USER32.dll!PostThreadMessageA': { args: 4, ret: 1 },
      'KERNEL32.dll!OutputDebugStringA': { ret: 0 }, 'KERNEL32.dll!Sleep': { args: 1, ret: 0 } } }).log;
  },
  // postNotification: four devices (flag dword +0x1a4: bit 5 set or clear) and three records (0x1440 bytes: window +0x1420, flag byte +0x1428)
  c3dh_postnote() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    [0x20, 0, 0xffffffdf, 0xffffffff].forEach(function (fl, i) {
      const d = F.c3dh_fill(__keepAlloc(0x1c0), 0x1c0, 0x3a);
      for (let k = 0; k < 0x1c0; k += 4) d.add(k).writeU32((0x4d000000 + 0x1000 * i + k) >>> 0);
      d.add(0x1a4).writeU32(fl >>> 0);
      out['d' + i] = d;
    });
    [[0x5001, 0], [0x5002, 0xfe], [0, 0xff]].forEach(function (r, i) {
      const b = F.c3dh_fill(__keepAlloc(0x1440), 0x1440, 0x4b);
      for (let k = 0; k < 0x1440; k += 4) b.add(k).writeU32((0x4a000000 + 0x10000 * i + k) >>> 0);
      b.add(0x1420).writeU32(r[0]);
      b.add(0x1428).writeU32((0x5a5a5a00 | r[1]) >>> 0);
      out['r' + i] = b;
    });
    out.implog = F.c3dh_postimports();
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100 });
    out.arena = h.arena;
    return out;
  },
  // postMidiNotification: objects (word +0xac with bit 8, window +0x210, flags +0x214)
  c3dh_postmidi() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    [[0, 0], [0x100, 0], [0xff, 0x10], [0x1ff, 0], [0xfffffeff, 0xffff0000], [0x100, 0xffffffff], [0, 0xffffffef], [0x80, 0x5]].forEach(function (r, i) {
      const o = F.c3dh_fill(__keepAlloc(0x220), 0x220, 0x4c);
      for (let k = 0; k < 0x220; k += 4) o.add(k).writeU32((0x4e000000 + 0x1000 * i + k) >>> 0);
      o.add(0xac).writeU32(r[0] >>> 0);
      o.add(0x210).writeU32((0x6000 + i) >>> 0);
      o.add(0x214).writeU32(r[1] >>> 0);
      out['m' + i] = o;
    });
    F.c3dh_waitengine();
    out.implog = F.c3dh_postimports();
    return out;
  },
  c3dh_wavecb() {
    const out = {};
    globalThis.DIFF_FIXTURES.c3dh_waitengine();
    out.implog = globalThis.DIFF_FIXTURES.c3dh_postimports();
    return out;
  },

  // =====================================================================================================================
  // I. The CRT's heap selection and the host image's linker version
  // =====================================================================================================================

  // A fake host image (0x100 bytes): "MZ" at +0, e_lfanew at +0x3c, the linker version bytes at e_lfanew + 0x1a / +0x1b.
  c3dh_image(o) {
    const F = globalThis.DIFF_FIXTURES;
    const im = F.c3dh_fill(__keepAlloc(0x100), 0x100, 0x22);
    im.writeU16(o.mz === undefined ? 0x5a4d : o.mz);
    im.add(0x3c).writeU32(o.lfanew === undefined ? 0x80 : o.lfanew);
    if (o.lfanew === undefined || o.lfanew === 0x80) { im.add(0x80 + 0x1a).writeU8(o.major); im.add(0x80 + 0x1b).writeU8(o.minor); }
    return im;
  },
  // getModuleLinkerVersion: GetModuleHandleA returns the fake image; two output cells per key
  c3dh_linkerver(img) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const image = F.c3dh_image(img);
    out.image = image;
    const cells = F.c3dh_fill(__keepAlloc(0x10), 0x10, 0);
    cells.writeU32(0xaaaaaaaa); cells.add(8).writeU32(0x55555555);
    out.outs = cells; out.o0 = cells; out.o1 = cells.add(8);
    out.implog = __importStandIn({ module: 'sound.dll', imports: { 'KERNEL32.dll!GetModuleHandleA': { args: 1, ret: image, record: [0] } } }).log;
    return out;
  },
  c3dh_linkerver_6() { return globalThis.DIFF_FIXTURES.c3dh_linkerver({ major: 6, minor: 0 }); },
  c3dh_linkerver_5() { return globalThis.DIFF_FIXTURES.c3dh_linkerver({ major: 5, minor: 12 }); },
  c3dh_linkerver_badmz() { return globalThis.DIFF_FIXTURES.c3dh_linkerver({ mz: 0x4d5a, major: 7, minor: 9 }); },
  c3dh_linkerver_nolfanew() { return globalThis.DIFF_FIXTURES.c3dh_linkerver({ lfanew: 0, major: 7, minor: 9 }); },
  c3dh_linkerver_7() { return globalThis.DIFF_FIXTURES.c3dh_linkerver({ major: 7, minor: 9 }); },

  // crtHeapSelect: o = { ver: [ok, platform, major], env: string or null, mod: string, img: {major, minor} }
  c3dh_heapsel(o) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const image = F.c3dh_image(o.img || { major: 6, minor: 0 });
    out.image = image;
    out.implog = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!GetVersionExA': { args: 1, record: false, impl: function (a) {
        a[0].add(4).writeU32(o.ver[2]); a[0].add(0x10).writeU32(o.ver[1]); return o.ver[0]; } },
      'KERNEL32.dll!GetEnvironmentVariableA': { args: 3, record: [0, 2], impl: function (a) {
        if (o.env === null || o.env === undefined) return 0;
        a[1].writeUtf8String(o.env); return o.env.length; } },
      'KERNEL32.dll!GetModuleFileNameA': { args: 3, record: [0, 2], impl: function (a) {
        const s = o.mod || ''; a[1].writeUtf8String(s); return s.length; } },
      'KERNEL32.dll!GetModuleHandleA': { args: 1, ret: image, record: [0] } } }).log;
    return out;
  },
  c3dh_heapsel_nt5() { return globalThis.DIFF_FIXTURES.c3dh_heapsel({ ver: [1, 2, 5] }); },
  c3dh_heapsel_nt4() { return globalThis.DIFF_FIXTURES.c3dh_heapsel({ ver: [1, 2, 4], img: { major: 6, minor: 0 } }); },
  c3dh_heapsel_w9x() { return globalThis.DIFF_FIXTURES.c3dh_heapsel({ ver: [1, 1, 5], img: { major: 5, minor: 0 } }); },
  c3dh_heapsel_verfail() { return globalThis.DIFF_FIXTURES.c3dh_heapsel({ ver: [0, 2, 9], img: { major: 7, minor: 0 } }); },
  c3dh_heapsel_glob1() { return globalThis.DIFF_FIXTURES.c3dh_heapsel({ ver: [1, 1, 4], env: '__global_heap_selected,1' }); },
  c3dh_heapsel_glob2() { return globalThis.DIFF_FIXTURES.c3dh_heapsel({ ver: [1, 1, 4], env: '__GLOBAL_HEAP_SELECTED,2;junk' }); },
  c3dh_heapsel_glob3() { return globalThis.DIFF_FIXTURES.c3dh_heapsel({ ver: [1, 1, 4], env: '__GLOBAL_HEAP_SELECTED,3' }); },
  c3dh_heapsel_globbad() { return globalThis.DIFF_FIXTURES.c3dh_heapsel({ ver: [1, 1, 4], env: '__GLOBAL_HEAP_SELECTED,9', img: { major: 4, minor: 0 } }); },
  c3dh_heapsel_globnocomma() { return globalThis.DIFF_FIXTURES.c3dh_heapsel({ ver: [1, 1, 4], env: '__GLOBAL_HEAP_SELECTED', img: { major: 6, minor: 0 } }); },
  c3dh_heapsel_exe2() { return globalThis.DIFF_FIXTURES.c3dh_heapsel({ ver: [1, 1, 4], env: 'c:\\game\\golf.exe,2', mod: 'C:\\Game\\Golf.exe' }); },
  c3dh_heapsel_exenomatch() { return globalThis.DIFF_FIXTURES.c3dh_heapsel({ ver: [1, 1, 4], env: 'D:\\OTHER.EXE,2', mod: 'C:\\Game\\Golf.exe', img: { major: 5, minor: 0 } }); },

  // --- SoundTimer::stopKeepLock ----------------------------------------------------------------------------------------
  // Timer blocks with client lists (c3dh_timers): +0x3c state flags (bit 0 = a timer event is running), +0x40 the event id, +0x18 the resolution. The
  // re-entrancy word 0x100b49a8 is preset to 5 (c3dh_tickword) and compared.
  c3dh_stopkeep() {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3dh_timers([
      [[], -1], [[0], -1], [[0, 1], -1], [[0, 1, 2], 1], [[2], 0], [[1, 1], -1], [[0, 1, 2], -1, 0], [[], 0, 9],
      [[0, 1], -1, 1], [[0], -1], [[1, 2], -1], [[2, 0, 1], 2],
    ]);
    // flags (+0x3c) per row: bit 0 clear / set, bit 2 set, upper bits
    [0, 1, 5, 7, 0x100, 0x101, 0xfffffffe, 0xffffffff, 4, 0x55, 0x80000001, 2].forEach(function (f, i) {
      out['t' + i].add(0x3c).writeU32(f >>> 0);
      out['t' + i].add(0x40).writeU32((0x5000 + i) >>> 0);
      out['t' + i].add(0x18).writeU32((1 + i) >>> 0);
      out['t' + i].add(4).writeU32(0x44444444);
      out['t' + i].add(8).writeU32(0x88888888);
    });
    // row 8: the second node of the list carries a NULL client (the drain loop ends there without a virtual call)
    out.t8.add(0x80 + 0x10 + 8).writePointer(ptr(0));
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    out.freelog = h.log;
    out.implog = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!EnterCriticalSection': { ret: 0 }, 'KERNEL32.dll!LeaveCriticalSection': { ret: 0 },
      'WINMM.dll!timeKillEvent': { ret: 0 }, 'WINMM.dll!timeEndPeriod': { args: 1, ret: 0 } } }).log;
    F.c3dh_tickword(out);
    return out;
  },
});
