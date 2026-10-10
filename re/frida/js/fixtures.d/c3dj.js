// Fixtures for C3 batch c3dj (sound.dll, round 26): C2 rows earlier batches dropped before the heap stand-in (realloc, scheduled
// failures), the import stand-in and the freeze helper existed. Layouts come from the disassembly cited in
// shim/src/re/c3dj.cpp. Every object, vtable, COM-shaped interface and list node here is allocated by the fixture; no live
// sound object, device, list, window or file is handed to an arm. Stand-ins used (one call per fixture): __heapStandIn
// (re/frida/js/heapstandin.js) over sound.dll's operator new 0x424d7 / operator delete 0x4249a. All values are synthetic.
// NativeCallbacks are kept alive through __diffKeepAlive.
Object.assign(globalThis.DIFF_FIXTURES, {

  c3dj_fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; },
  c3dj_keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; },
  c3dj_snd(rva) { return Process.getModuleByName('sound.dll').base.add(rva); },

  // n objects of `size` bytes named o0..o(n-1); every dword is a distinct marker (0x5a000000 + row << 16 + offset) so a store
  // into a wrong field or a missed store reads RED. Rows listed in `fills` are filled with that byte instead (0x00 and 0xff
  // exercise stores whose old value equals the new one).
  c3dj_objs(n, size, fills) {
    const out = {};
    for (let i = 0; i < n; i++) {
      const b = __keepAlloc(size);
      if (fills && fills[i] !== undefined) globalThis.DIFF_FIXTURES.c3dj_fill(b, size, fills[i]);
      else for (let o = 0; o < size; o += 4) b.add(o).writeU32((0x5a000000 + (i << 16) + o) >>> 0);
      out['o' + i] = b;
    }
    return out;
  },
  // twelve 0x100-byte objects: markers everywhere except row 3 (all zero) and row 7 (all 0xff)
  c3dj_objs12() { return globalThis.DIFF_FIXTURES.c3dj_objs(12, 0x100, { 3: 0x00, 7: 0xff }); },

  // CommandQueue::clear: ten 0x4010-byte queues (ring of 0x1000 dwords, indices at +0x4000 / +0x4004); row 2 all zero, row 5 all
  // 0xff, the rest markers
  c3dj_cmdclr() { return globalThis.DIFF_FIXTURES.c3dj_objs(10, 0x4010, { 2: 0x00, 5: 0xff }); },

  // Ctrl5b5c0_transposeTable: objects of 0x100 bytes whose 0x80-byte table at +0x5c holds
  //   t0 bytes 0..127, t1 (5 i) & 0xff (values above 0x7f), t2 all 0xff, t3 all 0, t4 a linear-congruential sequence,
  //   t5 the marker pattern of row 5 (bytes above 0x7f too)
  c3dj_tbl() {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3dj_objs(6, 0x100);
    const put = (o, f) => { for (let i = 0; i < 0x80; i++) o.add(0x5c + i).writeU8(f(i) & 0xff); };
    put(out.o0, (i) => i);
    put(out.o1, (i) => 5 * i);
    put(out.o2, () => 0xff);
    put(out.o3, () => 0);
    let s = 0x2545f491;
    put(out.o4, () => { s = (Math.imul(s, 1103515245) + 12345) >>> 0; return s >>> 16; });
    put(out.o5, (i) => 0x80 + ((i * 37) & 0x7f));
    return out;
  },

  // --- device objects whose destructor releases the sub-object at +0x14 through its vtable slot +0x14 -------------------
  // Ten 0x40-byte device objects d0..d9 (markers everywhere); rows 1..7 and 9 hold a sub-object at +0x14 (a distinct 0x40-byte
  // interface i1..i7 each, row 9 shares i1), rows 0 and 8 hold NULL. An interface's vtable slot +0x14 is a recording thiscall stub:
  // +8 counts the calls, +0xc keeps the last `this`, +0x10 gets 0x600d; it returns 0x1357.
  c3dj_dev() {
    const F = globalThis.DIFF_FIXTURES;
    const o = F.c3dj_objs(10, 0x40), out = {};
    for (let r = 0; r < 10; r++) out['d' + r] = o['o' + r];
    const vt = F.c3dj_fill(__keepAlloc(0x40), 0x40, 0);
    vt.add(0x14).writePointer(F.c3dj_keep(new NativeCallback(function (self) {
      self.add(8).writeU32((self.add(8).readU32() + 1) >>> 0);
      self.add(0xc).writeU32(self.toUInt32());
      self.add(0x10).writeU32(0x600d);
      return 0x1357;
    }, 'uint32', ['pointer'], 'thiscall')));
    for (let k = 1; k <= 7; k++) {
      const i = F.c3dj_fill(__keepAlloc(0x40), 0x40, 0x30 + k);
      i.writePointer(vt);
      i.add(8).writeU32(0);
      out['i' + k] = i;
    }
    for (let r = 0; r < 10; r++) {
      const k = r === 9 ? 1 : (r === 0 || r === 8 ? 0 : r);
      out['d' + r].add(0x14).writePointer(k ? out['i' + k] : ptr(0));
    }
    return out;
  },
  // the same with operator delete 0x1004249a recorded by the heap stand-in (the count at freelog+0, the pointers from +4)
  c3dj_devfree() {
    const out = globalThis.DIFF_FIXTURES.c3dj_dev();
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    out.freelog = h.log;
    return out;
  },
  // MemBlock_deleteDtor: eight 0x10-byte objects whose pointer at +0 is NULL (rows 0, 4) or a synthetic address (never
  // dereferenced: the stand-in only records the pointers handed to operator delete)
  c3dj_memblk() {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3dj_objs(8, 0x10);
    for (let r = 0; r < 8; r++) out['o' + r].writePointer((r === 0 || r === 4) ? ptr(0) : ptr(0x7e570000 + 0x1000 * r));
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    out.freelog = h.log;
    return out;
  },

  // --- the channel/voice destructor family (SubChannel, Ch5bb90, Mixer, Ch5be88, Ch5c178, Ch5c234, Ch5c2ec ...) ---------------
  // Twelve 0x120-byte objects c0..c11. Fields: +0x40 a sound object (NULL, or a synthetic address that delete_sound only records),
  // +0x44 the flag dword (bit 1 = linked into the active list), +0x48 / +0x4c the neighbours, +0x50 a buffer pointer (NULL or a
  // synthetic address that operator delete only records) and, from +0x58 up to +0x10c, an array of buffer pointers (Mixer_dtor
  // walks 0x28 from +0x58, destroyMixerVoices from +0x6c) of which a row-dependent subset is non-NULL. Rows:
  //   0 nothing set;      1 sound+buffer, unlinked;                2 sound, mid-list (prev n0, next n1);
  //   3 buffer, head (no prev, next n1);                          4 sound+buffer, tail (prev n0, no next);
  //   5 sole element (no prev, no next);                          6 sound+buffer, sole element;
  //   7 buffer only, unlinked;  8 sound only, unlinked;           9 mid-list, flag dword all ones (prev n2, next n3);
  //  10 sound+buffer, tail with flag dword 0xfffffff2 (prev n2);  11 buffer, head (next n3)
  // virt: the sound object at +0x40 is a fixture object whose vtable slot 0 is a recording thiscall(1 arg) stub (the deleting
  // destructor Ch5c178_dtor / Ch5c234_dtor call): +8 counts the calls, +0xc keeps this, +0x10 the argument.
  // The module globals 0x100b5070 (list head) and 0x100b506c (list tail) are state: the menu never reads or writes them (a 20 s
  // probe with every hook off entered none of the functions that do: log/c3/c3dj_notes.txt section D).
  c3dj_chan(virt) {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3dj_objs(12, 0x120);
    const nb = F.c3dj_objs(4, 0x60);
    for (let i = 0; i < 4; i++) out['n' + i] = nb['o' + i];
    const o = {}; for (let r = 0; r < 12; r++) { o[r] = out['o' + r]; delete out['o' + r]; }
    const rows = [
      /* q  p  bit1 prev next */
      [0, 0, 0, -1, -1], [1, 1, 0, -1, -1], [1, 0, 1, 0, 1], [0, 1, 1, -1, 1], [1, 1, 1, 0, -1], [0, 0, 1, -1, -1],
      [1, 1, 1, -1, -1], [0, 1, 0, -1, -1], [1, 0, 0, -1, -1], [0, 0, 1, 2, 3], [1, 1, 1, 2, -1], [0, 1, 1, -1, 3]];
    let vt = null;
    if (virt) {
      vt = F.c3dj_fill(__keepAlloc(0x20), 0x20, 0);
      vt.writePointer(F.c3dj_keep(new NativeCallback(function (self, a) {
        self.add(8).writeU32((self.add(8).readU32() + 1) >>> 0);
        self.add(0xc).writeU32(self.toUInt32());
        self.add(0x10).writeU32(a >>> 0);
        return 0x5150;
      }, 'uint32', ['pointer', 'uint32'], 'thiscall')));
    }
    rows.forEach(function (r, i) {
      const c = o[i];
      let q = ptr(0);
      if (r[0]) {
        if (virt) {
          q = F.c3dj_fill(__keepAlloc(0x20), 0x20, 0x20 + i);
          q.writePointer(vt);
          q.add(8).writeU32(0);
          out['s' + i] = q;
        } else q = ptr(0x7e400000 + 0x100 * i);
      }
      c.add(0x40).writePointer(q);
      let fl = 0x00010000 + i;                          // bit 1 clear
      if (i === 9) fl = 0xffffffff; else if (i === 10) fl = 0xfffffff2; else if (r[2]) fl = fl | 2;
      c.add(0x44).writeU32(fl >>> 0);
      c.add(0x48).writePointer(r[3] >= 0 ? out['n' + r[3]] : ptr(0));
      c.add(0x4c).writePointer(r[4] >= 0 ? out['n' + r[4]] : ptr(0));
      c.add(0x50).writePointer(r[1] ? ptr(0x7e500000 + 0x100 * i) : ptr(0));
      for (let off = 0x58; off < 0x110; off += 4)
        c.add(off).writePointer((((off >> 2) + i) % 3 !== 0) ? ptr(0x7e600000 + 0x1000 * i + off) : ptr(0));
      out['c' + i] = c;
    });
    const snd = Process.getModuleByName('sound.dll').base;
    out.gHead = snd.add(0xb5070);
    out.gTail = snd.add(0xb506c);
    const h1 = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    const h2 = __heapStandIn({ module: 'sound.dll', free: [0x97d0] });
    out.freelog = h1.log;
    out.dslog = h2.log;
    return out;
  },
  c3dj_chan_plain() { return globalThis.DIFF_FIXTURES.c3dj_chan(false); },
  c3dj_chan_virt() { return globalThis.DIFF_FIXTURES.c3dj_chan(true); },

  // --- DsBuffer / Ds3DBuffer: COM-shaped interfaces answered by recording stdcall stubs --------------------------------------
  // A COM-shaped object is 0x300 bytes: +0 vtable (shared); +4 the number of calls received; +8..+0x17 the call order (slot
  // index + 1 per call, up to 16); +0x20 + 4 s the result code of the slot at offset 4 s (s = 0 QueryInterface, 2 Release, 4 GetCurrentPosition,
  // 9 GetStatus, 13 SetCurrentPosition, 18 Stop, 19 Unlock / SetPosition); +0x78 the status dword GetStatus writes; +0x7c
  // the value QueryInterface writes to its output pointer when its code is not negative; +0x80 + 0x20 s one record per slot: the call
  // count and the arguments after the interface (an output pointer into the caller's stack is recorded as 1 when non-NULL, an
  // interface identifier as its offset from the module base, every other argument raw).
  c3dj_comvt() {
    const F = globalThis.DIFF_FIXTURES;
    const snd = Process.getModuleByName('sound.dll').base;
    const vt = F.c3dj_fill(__keepAlloc(0x80), 0x80, 0);
    const rec = function (self, s, args) {
      const r = self.add(0x80 + 0x20 * s);
      r.writeU32((r.readU32() + 1) >>> 0);
      args.forEach(function (v, i) { r.add(4 + 4 * i).writeU32(v >>> 0); });
      const n = self.add(4).readU32();
      if (n < 16) self.add(8 + n).writeU8(s + 1);
      self.add(4).writeU32(n + 1);
      return self.add(0x20 + 4 * s).readU32();
    };
    const T = (n) => ['pointer'].concat(Array(n).fill('uint32'));
    const put = (off, n, fn) => vt.add(off).writePointer(F.c3dj_keep(new NativeCallback(fn, 'uint32', T(n), 'stdcall')));
    put(0x00, 2, function (self, iid, out) {
      const code = rec(self, 0, [ptr(iid).sub(snd).toUInt32(), out]);
      if ((code | 0) >= 0) ptr(out).writeU32(self.add(0x7c).readU32());
      return code;
    });
    put(0x08, 0, function (self) { return rec(self, 2, []); });
    put(0x10, 2, function (self, o1, o2) {
      const code = rec(self, 4, [ptr(o1).isNull() ? 0 : 1, ptr(o2).isNull() ? 0 : 1]);
      if (!ptr(o1).isNull()) ptr(o1).writeU32(0x11110001);
      if (!ptr(o2).isNull()) ptr(o2).writeU32(0x22220002);
      return code;
    });
    put(0x24, 1, function (self, out) {
      const code = rec(self, 9, [ptr(out).isNull() ? 0 : 1]);
      if (!ptr(out).isNull()) ptr(out).writeU32(self.add(0x78).readU32());
      return code;
    });
    put(0x34, 1, function (self, a) { return rec(self, 13, [a]); });
    put(0x48, 0, function (self) { return rec(self, 18, []); });
    put(0x4c, 4, function (self, a, b, c, d) { return rec(self, 19, [a, b, c, d]); });
    return vt;
  },
  // cfg: codes {qi, rel, cur (slot 4), status (slot 9), setcur (13), stop, unl (19)}, statusVal, qiOut
  c3dj_mkcom(vt, cfg) {
    const F = globalThis.DIFF_FIXTURES;
    cfg = cfg || {};
    const com = F.c3dj_fill(__keepAlloc(0x300), 0x300, 0);
    com.writePointer(vt);
    const code = (s, v) => com.add(0x20 + 4 * s).writeU32((v || 0) >>> 0);
    code(0, cfg.qi); code(2, cfg.rel); code(4, cfg.cur); code(9, cfg.status); code(13, cfg.setcur);
    code(18, cfg.stop); code(19, cfg.unl);
    com.add(0x78).writeU32((cfg.statusVal || 0) >>> 0);
    com.add(0x7c).writeU32((cfg.qiOut === undefined ? 0xa0000001 : cfg.qiOut) >>> 0);
    return com;
  },
  // DsBuffer rows. Each row r: l60 / l64 / l68 / l6c link the interface at +0x60 / +0x64 / +0x68 / +0x6c (a0 / b0 / e0 / c0 in the
  // fixture; unlinked ones stay allocated and in the state), f5c = the dword at +0x5c, f44 = the dword at +0x44, b0 / b4 = the
  // bytes at +0 and +4, c60 / c64 / c68 / c6c = the interface configurations.
  c3dj_dsrows(rows) {
    const F = globalThis.DIFF_FIXTURES;
    const vt = F.c3dj_comvt();
    const out = {};
    rows.forEach(function (r, i) {
      out['a' + i] = F.c3dj_mkcom(vt, r.c60);
      out['b' + i] = F.c3dj_mkcom(vt, r.c64);
      out['e' + i] = F.c3dj_mkcom(vt, r.c68);
      out['c' + i] = F.c3dj_mkcom(vt, r.c6c);
      const d = __keepAlloc(0x100);
      for (let o = 0; o < 0x100; o += 4) d.add(o).writeU32((0x4d000000 + (i << 12) + o) >>> 0);
      d.writeU8(r.b0 === undefined ? 0x11 : r.b0);
      d.add(4).writeU8(r.b4 === undefined ? 0x22 : r.b4);
      d.add(0x44).writeU32((r.f44 === undefined ? 0 : r.f44) >>> 0);
      d.add(0x5c).writeU32((r.f5c === undefined ? 1 : r.f5c) >>> 0);
      d.add(0x60).writePointer(r.l60 ? out['a' + i] : ptr(0));
      d.add(0x64).writePointer(r.l64 ? out['b' + i] : ptr(0));
      d.add(0x68).writePointer(r.l68 ? out['e' + i] : ptr(0));
      d.add(0x6c).writePointer(r.l6c ? out['c' + i] : (r.m6c ? ptr(0x6c6c0000 + i) : ptr(0)));
      d.add(0xc8).writeU32((0x3f800000 + i) >>> 0);
      d.add(0xcc).writeU32((0x40000000 + i) >>> 0);
      d.add(0xd0).writeU32((0x40400000 + i) >>> 0);
      out['d' + i] = d;
    });
    return out;
  },
  c3dj_ds_stop() {
    const rows = [{ l60: 0 }];
    [0, 0x80004005, 1, 0xffffffff, 0x7fffffff, 2, 3, 0x88780096, 0x80000000, 0x12345678, 5].forEach(function (c) {
      rows.push({ l60: 1, c60: { stop: c } });
    });
    return globalThis.DIFF_FIXTURES.c3dj_dsrows(rows);
  },
  c3dj_ds_unlock() {
    const rows = [{ l60: 0 }];
    [0, 1, 0x80004005, 0xffffffff, 0x7fffffff, 2, 0x88780096, 0x80000000, 3, 0x12345678, 0xfffffffe].forEach(function (c) {
      rows.push({ l60: 1, c60: { unl: c } });
    });
    return globalThis.DIFF_FIXTURES.c3dj_dsrows(rows);
  },
  c3dj_ds_busy() {
    const rows = [{ l60: 0, f44: 0 }, { l60: 0, f44: 7 }];
    // [f44, status]
    [[0, 0], [0, 1], [0, 2], [0, 3], [0, 0x80000001], [1, 0], [1, 1], [5, 0], [0xffffffff, 1], [0, 0xfffffffe],
     [0, 0x1ff1], [0x80000000, 4]].forEach(function (t) {
      rows.push({ l60: 1, f44: t[0], c60: { statusVal: t[1] } });
    });
    return globalThis.DIFF_FIXTURES.c3dj_dsrows(rows);
  },
  c3dj_ds_qnotify() {
    const FAIL = 0x80004002;
    // [l68, l64, qi68, qi64]
    const rows = [];
    [[0, 0, 0, 0], [1, 0, 0, 0], [1, 0, FAIL, 0], [0, 1, 0, 0], [0, 1, 0, FAIL], [1, 1, 0, 0], [1, 1, FAIL, 0], [1, 1, 0, FAIL],
     [1, 0, 1, 0], [1, 0, 0x80000000, 0], [0, 1, 0, 0x7fffffff], [0, 1, 0, 0xffffffff]].forEach(function (t, i) {
      rows.push({ l68: t[0], l64: t[1], c68: { qi: t[2], qiOut: 0xa8000000 + i }, c64: { qi: t[3], qiOut: 0xa6000000 + i } });
    });
    return globalThis.DIFF_FIXTURES.c3dj_dsrows(rows);
  },
  c3dj_ds_q3d() {
    const FAIL = 0x80004002;
    // [l60, b0, qi]
    const rows = [];
    [[0, 0, 0], [0, 2, 0], [1, 0, 0], [1, 2, 0], [1, 0, FAIL], [1, 2, FAIL], [1, 3, 0], [1, 1, FAIL], [1, 0xfe, 0], [1, 0xfd, 0],
     [1, 0, 1], [1, 2, 0x80000000], [1, 0xff, 0]].forEach(function (t, i) {
      rows.push({ l60: t[0], b0: t[1], c60: { qi: t[2], qiOut: 0xb3000000 + i } });
    });
    return globalThis.DIFF_FIXTURES.c3dj_dsrows(rows);
  },
  c3dj_ds_3d() {
    const rows = [{ l64: 0 }];
    [0, 1, 0x80004005, 0xffffffff, 2, 0x7fffffff, 0x80000000].forEach(function (c) { rows.push({ l64: 1, c64: { unl: c } }); });
    return globalThis.DIFF_FIXTURES.c3dj_dsrows(rows);
  },
  c3dj_ds_setpos() {
    // [l60, b4, setcur, cur]
    const rows = [];
    [[0, 0], [0, 1], [1, 0], [1, 1], [1, 0xfe], [1, 0xff], [0, 0xfe], [1, 0x80], [0, 0x7f], [1, 0x7e], [1, 0x55], [0, 0xaa]]
      .forEach(function (t, i) {
        rows.push({ l60: t[0], b4: t[1], c60: { setcur: 0x100 + i, cur: 0x200 + i } });
      });
    return globalThis.DIFF_FIXTURES.c3dj_dsrows(rows);
  },
  // release rows: [l60, f5c, l64, l68, l6c]
  //   0 no buffer interface; 1 buffer but +0x5c clear; 2 buffer only; 3 + 3D interface, no notify interface;
  //   4 + 3D and notify interfaces; 5 notify source set but no 3D interface; 6 all four linked; 7 buffer + 3D, +0x5c clear;
  //   8 all linked, Stop and Release codes non-zero; 9 3D only linked (no buffer); 10 all linked, +0x5c = 0xffffffff
  c3dj_ds_rel() {
    const T = [[0, 1, 0, 0, 0], [1, 0, 0, 0, 0], [1, 1, 0, 0, 0], [1, 1, 1, 0, 0], [1, 1, 1, 0, 1], [1, 1, 0, 1, 1], [1, 1, 1, 1, 1],
               [1, 0, 1, 0, 0], [1, 1, 1, 1, 1], [0, 1, 1, 0, 0], [1, 0xffffffff, 1, 1, 1], [1, 2, 1, 0, 1]];
    const rows = T.map(function (t, i) {
      const nz = (i === 8);
      return { l60: t[0], f5c: t[1], l64: t[2], l68: t[3], l6c: t[4], f44: 0x31 + i, m6c: !t[2],
               c60: nz ? { stop: 0x80004005, rel: 7 } : {}, c64: nz ? { rel: 9 } : {}, c6c: nz ? { rel: 0xffffffff } : {} };
    });
    return globalThis.DIFF_FIXTURES.c3dj_dsrows(rows);
  },
  c3dj_ds_rel_free() {
    const out = globalThis.DIFF_FIXTURES.c3dj_ds_rel();
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    out.freelog = h.log;
    return out;
  },

  // --- small forwarders: closeDecoder, Ctrl_appendIfAccepted, DsDevice_registerBuffer, addToSoundList ------------------------
  // A list header {+0 front, +4 back, +8 cursor, +12 count} at `at` inside `blk`, with n linked nodes {prev, next, value} taken
  // from the spare space at blk+spare (0x10 apart).
  c3dj_hdr(blk, at, n, spare, count) {
    const node = (i) => blk.add(spare + 0x10 * i);
    for (let i = 0; i < n; i++) {
      node(i).writePointer(i > 0 ? node(i - 1) : ptr(0));
      node(i).add(4).writePointer(i < n - 1 ? node(i + 1) : ptr(0));
      node(i).add(8).writeU32((0xc1000000 + i) >>> 0);
    }
    const h = blk.add(at);
    h.writePointer(n ? node(0) : ptr(0));
    h.add(4).writePointer(n ? node(n - 1) : ptr(0));
    h.add(8).writeU32(0);
    h.add(0xc).writeU32((count === undefined ? n : count) >>> 0);
  },
  // closeDecoder: decoder objects x0..x11 (0x80 bytes) whose +0x54 holds a DsBuffer (rows of c3dj_ds_stop; x0 holds the DsBuffer
  // without an interface, x1 holds NULL)
  c3dj_closedec() {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3dj_ds_stop();
    for (let i = 0; i < 12; i++) {
      const x = __keepAlloc(0x80);
      for (let o = 0; o < 0x80; o += 4) x.add(o).writeU32((0x5d000000 + (i << 12) + o) >>> 0);
      x.add(0x54).writePointer(i === 1 ? ptr(0) : out['d' + i]);
      out['x' + i] = x;
    }
    return out;
  },
  // Ctrl_appendIfAccepted: ten control objects o0..o9 (0x80 bytes). The vtable slot 0 is a recording thiscall(1 arg) stub that returns
  // the dword at +0x70 (the low byte decides) and keeps its argument at +0x74 and the call count at +0x78. The list header at +0xc
  // holds 0, 1 or 2 nodes. The heap stand-in serves the list node.
  c3dj_ctrlapp() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const vt = F.c3dj_fill(__keepAlloc(0x20), 0x20, 0);
    vt.writePointer(F.c3dj_keep(new NativeCallback(function (self, a) {
      self.add(0x74).writeU32(a >>> 0);
      self.add(0x78).writeU32((self.add(0x78).readU32() + 1) >>> 0);
      return self.add(0x70).readU32();
    }, 'uint32', ['pointer', 'uint32'], 'thiscall')));
    const acc = [1, 0, 0x100, 0x101, 0xffffff00, 0xff, 0x80, 0xdeadbe01, 0, 1];
    const shape = [0, 0, 1, 1, 2, 2, 0, 1, 2, 2];
    for (let i = 0; i < 10; i++) {
      const o = __keepAlloc(0x100);
      for (let k = 0; k < 0x100; k += 4) o.add(k).writeU32((0x5e000000 + (i << 12) + k) >>> 0);
      o.writePointer(vt);
      o.add(0x70).writeU32(acc[i] >>> 0);
      o.add(0x78).writeU32(0);
      F.c3dj_hdr(o, 0xc, shape[i], 0x80);
      out['o' + i] = o;
    }
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100 });
    out.arena = h.arena;
    return out;
  },
  // DsDevice_registerBuffer: devices o0..o5 (0x1400 bytes, list header at +0x1338 holding 0, 1, 2, 0 (count 7), 2 nodes) and the
  // buffers b0..b3 (0x40 bytes) handed to them
  c3dj_dsreg() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const shape = [0, 1, 2, 0, 2, 1];
    for (let i = 0; i < 6; i++) {
      const o = __keepAlloc(0x1400);
      for (let k = 0; k < 0x1400; k += 4) o.add(k).writeU32((0x5f000000 + (i << 12) + k) >>> 0);
      F.c3dj_hdr(o, 0x1338, shape[i], 0x1380, i === 3 ? 7 : undefined);
      out['o' + i] = o;
    }
    for (let i = 0; i < 4; i++) {
      const b = __keepAlloc(0x40);
      for (let k = 0; k < 0x40; k += 4) b.add(k).writeU32((0x6f000000 + (i << 8) + k) >>> 0);
      out['b' + i] = b;
    }
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100 });
    out.arena = h.arena;
    return out;
  },
  // addToSoundList: owners o0..o5 (0x200 bytes, list header at +0x1c8 holding 0, 1, 2, 0 (count 9), 2, 1 nodes) and the sound objects
  // s0..s5 (0x100 bytes) whose dword at +0xc8 holds 0, 0x40, 0xffffffff, 0x12345678, 0xffffff3f, 0x80
  c3dj_addsl() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const shape = [0, 1, 2, 0, 2, 1];
    for (let i = 0; i < 6; i++) {
      const o = __keepAlloc(0x200);
      for (let k = 0; k < 0x200; k += 4) o.add(k).writeU32((0x5f100000 + (i << 12) + k) >>> 0);
      F.c3dj_hdr(o, 0x1c8, shape[i], 0x1e0, i === 3 ? 9 : undefined);
      out['o' + i] = o;
    }
    const fl = [0, 0x40, 0xffffffff, 0x12345678, 0xffffff3f, 0x80];
    for (let i = 0; i < 6; i++) {
      const s = __keepAlloc(0x100);
      for (let k = 0; k < 0x100; k += 4) s.add(k).writeU32((0x6f100000 + (i << 8) + k) >>> 0);
      s.add(0xc8).writeU32(fl[i] >>> 0);
      out['s' + i] = s;
    }
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100 });
    out.arena = h.arena;
    return out;
  },

  // --- Voice_requestStop / Voice_requestStopDrain ----------------------------------------------------------------------------
  // Voice objects v0..v11 (0x100 bytes): the flag dword at +0xac (rows listed below), vtable slot +0x5c a recording thiscall stub that
  // returns the dword at +0x30 and counts its calls at +0x34. g_midiDevice 0x100b4a1c is pointed (first thread frozen, restored by the
  // freeze flush) at a fixture command queue (CommandQueue layout: ring at +0x50, read index +0x4050, write index +0x4054; mode
  // 'q' = empty, 'full' = write index right below the read index, 'null' = no device). The heap stand-in serves the queue node.
  // Rows [flag dword, slot result]: 0 {0,0} 1 {1,0} 2 {4,0} 3 {5,1} 4 {0,1} 5 {8,0} 6 {0xc,5} 7 {0xffffffff,1} 8 {0xfffffffa,0}
  // 9 {2,0x80000000} 10 {0x14,1} 11 {1,0xffffffff}
  c3dj_vreq(mode) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const vt = F.c3dj_fill(__keepAlloc(0x100), 0x100, 0);
    vt.add(0x5c).writePointer(F.c3dj_keep(new NativeCallback(function (self) {
      self.add(0x34).writeU32((self.add(0x34).readU32() + 1) >>> 0);
      return self.add(0x30).readU32();
    }, 'uint32', ['pointer'], 'thiscall')));
    const rows = [[0, 0], [1, 0], [4, 0], [5, 1], [0, 1], [8, 0], [0xc, 5], [0xffffffff, 1], [0xfffffffa, 0], [2, 0x80000000],
                  [0x14, 1], [1, 0xffffffff]];
    rows.forEach(function (r, i) {
      const v = __keepAlloc(0x100);
      for (let k = 0; k < 0x100; k += 4) v.add(k).writeU32((0x4b000000 + 0x1000 * i + k) >>> 0);
      v.writePointer(vt);
      v.add(0x30).writeU32(r[1] >>> 0);
      v.add(0x34).writeU32(0);
      v.add(0xac).writeU32(r[0] >>> 0);
      out['v' + i] = v;
    });
    const q = F.c3dj_fill(__keepAlloc(0x4060), 0x4060, 0x71);
    for (let s = 0; s < 0x1000; s++) q.add(0x50 + 4 * s).writeU32((0x5a000000 + s) >>> 0);
    q.add(0x4050).writeU32(mode === 'full' ? 6 : 0);
    q.add(0x4054).writeU32(mode === 'full' ? 5 : 0);
    out.q = q;
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100 });
    out.arena = h.arena;
    const g = F.c3dj_snd(0xb4a1c);
    out.gdev = g;
    __freeze.thread();
    __freeze.save(g, 4);
    g.writePointer(mode === 'null' ? ptr(0) : q);
    return out;
  },
  c3dj_vreq_q() { return globalThis.DIFF_FIXTURES.c3dj_vreq('q'); },
  c3dj_vreq_full() { return globalThis.DIFF_FIXTURES.c3dj_vreq('full'); },
  c3dj_vreq_null() { return globalThis.DIFF_FIXTURES.c3dj_vreq('null'); },

  // --- node lists cleared from the front: PtrList_freeNodesB, nodeListClear2, nodeListClear (header at +0) and VoiceBase_clearList
  // (header at +8). Rows: the node values (0 = NULL value, otherwise a synthetic pointer) and an optional count override. operator
  // delete 0x1004249a is recorded by the heap stand-in (nothing is freed).
  c3dj_lists(at) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const rows = [
      { v: [] }, { v: [1] }, { v: [0] }, { v: [1, 1] }, { v: [0, 1] }, { v: [1, 0] }, { v: [1, 1, 1] }, { v: [1, 1, 0] },
      { v: [0, 0, 0] }, { v: [1, 1, 1, 1, 1] }, { v: [1, 0, 1], count: 0 }, { v: [1, 1], count: 0xffffffff }];
    rows.forEach(function (r, i) {
      const blk = __keepAlloc(0x100);
      for (let k = 0; k < 0x100; k += 4) blk.add(k).writeU32((0x5c000000 + (i << 12) + k) >>> 0);
      const n = r.v.length;
      const node = (j) => blk.add(0x80 + 0x10 * j);
      r.v.forEach(function (v, j) {
        node(j).writePointer(j > 0 ? node(j - 1) : ptr(0));
        node(j).add(4).writePointer(j < n - 1 ? node(j + 1) : ptr(0));
        node(j).add(8).writeU32(v ? (0xc7000000 + 0x100 * i + j) >>> 0 : 0);
      });
      const h = blk.add(at);
      h.writePointer(n ? node(0) : ptr(0));
      h.add(4).writePointer(n ? node(n - 1) : ptr(0));
      h.add(8).writeU32(0x0c0c0c0c);
      h.add(0xc).writeU32((r.count === undefined ? n : r.count) >>> 0);
      out['l' + i] = blk;
    });
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    out.freelog = h.log;
    return out;
  },
  c3dj_lists0() { return globalThis.DIFF_FIXTURES.c3dj_lists(0); },
  c3dj_lists8() { return globalThis.DIFF_FIXTURES.c3dj_lists(8); },

  // --- Channel_set3DPosition: channels ch0..ch9 (0x80 bytes): flag byte +0x5c (bit 5 = 3D) and the DsBuffer at +0x54 (rows of
  // c3dj_ds_3d: d0 has no 3D interface, d1..d7 carry answer codes). Rows: 0 not 3D, 1 3D without DsBuffer, 2.. 3D with d0..d7
  c3dj_ch3d() {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3dj_ds_3d();
    const rows = [[0x00, -1], [0x20, -1], [0x20, 0], [0x20, 1], [0x20, 2], [0x20, 3], [0x20, 4], [0x20, 5], [0x00, 3], [0xff, 6]];
    rows.forEach(function (r, i) {
      const c = __keepAlloc(0x80);
      for (let k = 0; k < 0x80; k += 4) c.add(k).writeU32((0x5d100000 + (i << 12) + k) >>> 0);
      c.add(0x5c).writeU8(r[0]);
      c.add(0x54).writePointer(r[1] >= 0 ? out['d' + r[1]] : ptr(0));
      out['ch' + i] = c;
    });
    return out;
  },

  // --- J. object-held lists, name copies and virtual forwarders ---------------------------------------------------------------
  // list header {+0 front, +4 back, +8 cursor, +12 count} at `at` inside `blk`; vals = the node values (0 = NULL value); nodes
  // {previous, next, value} from blk+spare (0x10 apart)
  c3dj_vhdr(blk, at, vals, spare, count, cursor) {
    const node = (i) => blk.add(spare + 0x10 * i);
    const n = vals.length;
    vals.forEach(function (v, i) {
      node(i).writePointer(i > 0 ? node(i - 1) : ptr(0));
      node(i).add(4).writePointer(i < n - 1 ? node(i + 1) : ptr(0));
      node(i).add(8).writeU32(v >>> 0);
    });
    const h = blk.add(at);
    h.writePointer(n ? node(0) : ptr(0));
    h.add(4).writePointer(n ? node(n - 1) : ptr(0));
    h.add(8).writePointer(cursor === undefined ? ptr(0x0c0c0c0c) : node(cursor));
    h.add(0xc).writeU32((count === undefined ? n : count) >>> 0);
  },
  // thiscall stub makers (kept alive); every stub counts its calls at self+off and records its first argument
  c3dj_stub(nargs, fn) {
    return globalThis.DIFF_FIXTURES.c3dj_keep(new NativeCallback(fn, 'uint32', ['pointer'].concat(Array(nargs).fill('uint32')), 'thiscall'));
  },

  // VoiceBase_openInner: voices v0..v9 (0x40 bytes): vtable slot +4 (1 argument) returns the dword at +0x30, slot +8 (no argument) counts at
  // +0x3c; the sub-object at +0x14 (0x40 bytes) has slot +0x10 (2 arguments) returning the dword at its +0x30. Counters: voice +0x34 / +0x38
  // (slot +4: calls, last argument), sub +0x34 / +0x38 / +0x3c (slot +0x10: calls, a1, a2).
  c3dj_vopen() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const vt = F.c3dj_fill(__keepAlloc(0x40), 0x40, 0);
    vt.add(4).writePointer(F.c3dj_stub(1, function (self, a) {
      self.add(0x34).writeU32((self.add(0x34).readU32() + 1) >>> 0);
      self.add(0x38).writeU32(a >>> 0);
      return self.add(0x30).readU32();
    }));
    vt.add(8).writePointer(F.c3dj_stub(0, function (self) {
      self.add(0x3c).writeU32((self.add(0x3c).readU32() + 1) >>> 0);
      return 0x7777;
    }));
    const sv = F.c3dj_fill(__keepAlloc(0x40), 0x40, 0);
    sv.add(0x10).writePointer(F.c3dj_stub(2, function (self, a, b) {
      self.add(0x34).writeU32((self.add(0x34).readU32() + 1) >>> 0);
      self.add(0x38).writeU32(a >>> 0);
      self.add(0x3c).writeU32(b >>> 0);
      return self.add(0x30).readU32();
    }));
    const rows = [[0, 0], [0, 1], [0, 0xffffffff], [5, 1], [0xffffffff, 0], [0, 0x80000000], [1, 7], [0, 2], [0x80000000, 0], [0, 0x1234]];
    rows.forEach(function (r, i) {
      const v = __keepAlloc(0x40), s = __keepAlloc(0x40);
      for (let k = 0; k < 0x40; k += 4) { v.add(k).writeU32((0x5a100000 + (i << 8) + k) >>> 0); s.add(k).writeU32((0x5a200000 + (i << 8) + k) >>> 0); }
      v.writePointer(vt);
      s.writePointer(sv);
      v.add(0x14).writePointer(s);
      v.add(0x30).writeU32(r[0] >>> 0);
      s.add(0x30).writeU32(r[1] >>> 0);
      v.add(0x34).writeU32(0); v.add(0x3c).writeU32(0); s.add(0x34).writeU32(0);
      out['v' + i] = v;
      out['s' + i] = s;
    });
    return out;
  },

  // Seq_freeChannelObjects: ten sequences q0..q9 (0x210 bytes): 0x80 object pointers at +4 (the dword at +0x204 follows the array),
  // the buffer pointer at +0. Each row owns four deleting-destructor objects a, b, c, d (0x20 bytes; vtable slot 0 is a thiscall stub with
  // 1 argument: it counts at +8, keeps the argument at +0xc and appends the object's row letter to the order log at `ord`).
  c3dj_seqfree() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const ord = F.c3dj_fill(__keepAlloc(0x40), 0x40, 0);
    out.ord = ord;
    const vt = F.c3dj_fill(__keepAlloc(0x20), 0x20, 0);
    vt.writePointer(F.c3dj_keep(new NativeCallback(function (self, a) {
      self.add(8).writeU32((self.add(8).readU32() + 1) >>> 0);
      self.add(0xc).writeU32(a >>> 0);
      const n = ord.readU32();
      if (n < 0x30) { ord.add(4 + n).writeU8(self.add(0x10).readU8()); ord.writeU32(n + 1); }
      return 0;
    }, 'uint32', ['pointer', 'uint32'], 'thiscall')));
    // slots: [index, which object (0..3)]; extra: the dword at +0x204 as an object index, buf: the pointer at +0
    const rows = [
      { s: [] },
      { s: [[0, 0]] },
      { s: [[3, 0], [4, 0], [5, 0], [7, 1]] },
      { s: [[0, 0], [1, 1], [2, 0]] },
      { s: [[0x7f, 0]], extra: 0 },
      { s: [[0x7f, 0]], extra: 1 },
      { s: [[0x7e, 0], [0x7f, 0]], extra: 0 },
      { s: [[10, 2]], buf: true },
      { s: [[0, 0], [1, 0], [2, 0], [3, 0], [4, 0]] },
      { s: [[0, 0], [1, 0], [2, 1], [3, 1], [4, 2], [5, 3], [6, 0], [7, 0], [0x40, 1], [0x41, 1]], buf: true }];
    rows.forEach(function (r, i) {
      const q = __keepAlloc(0x210);
      for (let k = 0; k < 0x210; k += 4) q.add(k).writeU32((0x5a300000 + (i << 12) + k) >>> 0);
      for (let k = 4; k < 0x204; k += 4) q.add(k).writeU32(0);
      const objs = [];
      for (let j = 0; j < 4; j++) {
        const o = F.c3dj_fill(__keepAlloc(0x20), 0x20, 0);
        o.writePointer(vt);
        o.add(0x10).writeU8(0x41 + i * 4 + j);        // identity byte for the order log
        objs.push(o);
        out['o' + i + j] = o;
      }
      r.s.forEach(function (e) { q.add(4 + 4 * e[0]).writePointer(objs[e[1]]); });
      q.add(0x204).writePointer(r.extra === undefined ? ptr(0x5eeeeeee) : objs[r.extra]);
      q.writePointer(r.buf ? ptr(0x7e700000 + 0x100 * i) : ptr(0));
      out['q' + i] = q;
    });
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    out.freelog = h.log;
    return out;
  },

  // Sound_anyChannelActive: sets z0..z8 (0x210 bytes: 0x80 channel pointers at +4). A channel (0x1500 bytes) has its element count at
  // +0x1438, element pointers from +0x1440 and a vtable slot +0x5c stub that returns the dword at +0x14f0; an element (0x20 bytes) has the
  // same slot returning the dword at +0x10. cnt = the element count, el = per element the result (-1 = NULL element), own = the
  // channel's own result.
  c3dj_anyactive() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const cvt = F.c3dj_fill(__keepAlloc(0x80), 0x80, 0);
    cvt.add(0x5c).writePointer(F.c3dj_stub(0, function (self) {
      self.add(0x14f4).writeU32((self.add(0x14f4).readU32() + 1) >>> 0);
      return self.add(0x14f0).readU32();
    }));
    const evt = F.c3dj_fill(__keepAlloc(0x80), 0x80, 0);
    evt.add(0x5c).writePointer(F.c3dj_stub(0, function (self) {
      self.add(0x14).writeU32((self.add(0x14).readU32() + 1) >>> 0);
      return self.add(0x10).readU32();
    }));
    const sets = [
      [],
      [[0, 0, [], 0]],
      [[0, 0, [], 1]],
      [[0, 1, [], 0], [1, 0, [], 7]],
      [[0, 3, [0, 0], 0]],
      [[0, 4, [-1, 5, 0], 0]],
      [[0, 2, [0], 9]],
      [[5, 1, [], 0], [127, 0, [], 0]],
      [[0, 0xffffffff, [], 0], [1, 2, [0], 0]],
      [[0, 2, [0], 0], [3, 1, [], 0], [4, 3, [0, -1], 4]],
      [[2, 5, [0, 0, 0, 0], 0], [9, 0, [], 0], [20, 1, [], 0xffffffff]]];
    sets.forEach(function (chs, i) {
      const z = __keepAlloc(0x210);
      for (let k = 0; k < 0x210; k += 4) z.add(k).writeU32(0);
      z.add(0x204).writeU32(0x5a400000 + i);
      chs.forEach(function (c, j) {
        const ch = __keepAlloc(0x1500);
        for (let k = 0; k < 0x1500; k += 4) ch.add(k).writeU32((0x5a500000 + (i << 12) + (j << 8) + (k & 0xfc)) >>> 0);
        ch.writePointer(cvt);
        ch.add(0x1438).writeU32(c[1] >>> 0);
        ch.add(0x14f0).writeU32(c[3] >>> 0);
        ch.add(0x14f4).writeU32(0);
        c[2].forEach(function (res, k) {
          if (res < 0) { ch.add(0x1440 + 4 * k).writePointer(ptr(0)); return; }
          const e = F.c3dj_fill(__keepAlloc(0x20), 0x20, 0);
          e.writePointer(evt);
          e.add(0x10).writeU32(res >>> 0);
          ch.add(0x1440 + 4 * k).writePointer(e);
          out['e' + i + '_' + j + '_' + k] = e;
        });
        z.add(4 + 4 * c[0]).writePointer(ch);
        out['c' + i + '_' + j] = ch;
      });
      out['z' + i] = z;
    });
    return out;
  },

  // findAndUnlinkNode / findAndUnlinkNodeB: lists l0..l9 (header at +0, nodes from +0x80). operator delete is recorded.
  c3dj_findlists() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const rows = [{ v: [] }, { v: [11] }, { v: [11, 22] }, { v: [11, 22, 33] }, { v: [11, 22, 33, 44] }, { v: [11, 11, 22] },
                  { v: [0, 5] }, { v: [5, 0] }, { v: [1, 2, 3, 4, 5, 6] }, { v: [7], count: 0 }];
    rows.forEach(function (r, i) {
      const blk = __keepAlloc(0x100);
      for (let k = 0; k < 0x100; k += 4) blk.add(k).writeU32((0x5a600000 + (i << 12) + k) >>> 0);
      F.c3dj_vhdr(blk, 0, r.v, 0x80, r.count, r.v.length > 2 ? 1 : undefined);
      out['l' + i] = blk;
    });
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    out.freelog = h.log;
    return out;
  },

  // name setters: objects n0..n3 (0x220 bytes) whose three name fields (+0x24, +0x50, +0x204) are NULL (n0, n2) or hold a synthetic old
  // block (n1, n3; the stand-in only records the release); strings t0..t8 (kept alive). operator new / delete are stood in.
  c3dj_names() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 4; i++) {
      const o = __keepAlloc(0x220);
      for (let k = 0; k < 0x220; k += 4) o.add(k).writeU32((0x5a700000 + (i << 12) + k) >>> 0);
      const old = (i & 1) ? 1 : 0;
      [0x24, 0x50, 0x204].forEach(function (off) { o.add(off).writePointer(old ? ptr(0x7e800000 + 0x1000 * i + off) : ptr(0)); });
      out['n' + i] = o;
    }
    const strs = ['', 'a', 'ab', 'abc', 'abcd', 'hello world', 'abcdefghijklmnopqrstuvwxyz01234', 'x'.repeat(63)];
    strs.forEach(function (s, i) { out['t' + i] = Memory.allocUtf8String(s); globalThis.__diffKeepAlive.push(out['t' + i]); });
    const hi = __keepAlloc(8); hi.writeByteArray([0xff, 0x80, 0x41, 0x00, 0x7f]);
    out.t8 = hi;
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], free: [0x4249a], arenaSize: 0x400 });
    out.arena = h.arena;
    out.freelog = h.log;
    return out;
  },

  // Channel_releaseSound: channels h0..h7 (0x60 bytes): vtable slot +0x5c (no argument) returns the dword at +0x30, slot +0x80 counts at +0x38;
  // the sound object at +0x40 (rows with one) is a 0x20-byte object whose vtable slot +0x14 returns its dword at +0x10 and counts at +0x14.
  // delete_sound 0x97d0 is recorded (dslog).
  c3dj_chrel() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const vt = F.c3dj_fill(__keepAlloc(0x100), 0x100, 0);
    vt.add(0x5c).writePointer(F.c3dj_stub(0, function (self) {
      self.add(0x34).writeU32((self.add(0x34).readU32() + 1) >>> 0);
      return self.add(0x30).readU32();
    }));
    vt.add(0x80).writePointer(F.c3dj_stub(0, function (self) {
      self.add(0x38).writeU32((self.add(0x38).readU32() + 1) >>> 0);
      return 0;
    }));
    const sv = F.c3dj_fill(__keepAlloc(0x40), 0x40, 0);
    sv.add(0x14).writePointer(F.c3dj_stub(0, function (self) {
      self.add(0x14).writeU32((self.add(0x14).readU32() + 1) >>> 0);
      return self.add(0x10).readU32();
    }));
    // [has object, slot +0x5c result, slot +0x14 result, flag dword]
    const rows = [[0, 0, 0, 1], [1, 0, 7, 1], [1, 2, 0, 3], [1, 1, 0x1234, 0x10001], [1, 0xffffffff, 0xffffffff, 0xffffffff], [0, 5, 0, 0xffffffff],
                  [1, 0, 0, 0xfffffffe], [1, 0x80000000, 0x80000000, 0]];
    rows.forEach(function (r, i) {
      const c = __keepAlloc(0x60);
      for (let k = 0; k < 0x60; k += 4) c.add(k).writeU32((0x5a800000 + (i << 8) + k) >>> 0);
      c.writePointer(vt);
      c.add(0x30).writeU32(r[1] >>> 0);
      c.add(0x34).writeU32(0); c.add(0x38).writeU32(0);
      c.add(0x44).writeU32(r[3] >>> 0);
      if (r[0]) {
        const q = F.c3dj_fill(__keepAlloc(0x20), 0x20, 0x55);
        q.writePointer(sv);
        q.add(0x10).writeU32(r[2] >>> 0);
        q.add(0x14).writeU32(0);
        c.add(0x40).writePointer(q);
        out['q' + i] = q;
      } else c.add(0x40).writePointer(ptr(0));
      out['h' + i] = c;
    });
    const h2 = __heapStandIn({ module: 'sound.dll', free: [0x97d0] });
    out.dslog = h2.log;
    return out;
  },

  // unlinkDsBuffer: buffers u0..u5 (0x100 bytes; previous +0xbc, next +0xc0). The module's DirectSound buffer list head / tail (0x100b4a34 /
  // 0x100b4a38) are state; the game's first thread is frozen and both words are restored at the flush.
  c3dj_unlds() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 10; i++) {
      const b = __keepAlloc(0x100);
      for (let k = 0; k < 0x100; k += 4) b.add(k).writeU32((0x5a900000 + (i << 12) + k) >>> 0);
      out['u' + i] = b;
    }
    const link = (i, p, n) => {
      out['u' + i].add(0xbc).writePointer(p < 0 ? ptr(0) : out['u' + p]);
      out['u' + i].add(0xc0).writePointer(n < 0 ? ptr(0) : out['u' + n]);
    };
    link(0, -1, 1); link(1, 0, 2); link(2, 1, -1); link(3, -1, -1); link(4, 1, 2); link(5, 4, -1);
    link(6, 0, 3); link(7, 3, 1); link(8, 2, 0); link(9, -1, 5);
    const snd = Process.getModuleByName('sound.dll').base;
    out.gHead = snd.add(0xb4a34);
    out.gTail = snd.add(0xb4a38);
    __freeze.thread();
    __freeze.save(out.gHead, 4);
    __freeze.save(out.gTail, 4);
    out.gHead.writePointer(out.u0);
    out.gTail.writePointer(out.u2);
    return out;
  },

  // --- K. VoiceBase_dtor and Midi_Device::dtor ----------------------------------------------------------------------------------
  // a sub-object with a recording thiscall slot +0x14 (as in c3dj_dev): +8 counts the calls, +0xc keeps this
  c3dj_subobj(i) {
    const F = globalThis.DIFF_FIXTURES;
    if (!F.__subvt) {
      const vt = F.c3dj_fill(__keepAlloc(0x40), 0x40, 0);
      vt.add(0x14).writePointer(F.c3dj_keep(new NativeCallback(function (self) {
        self.add(8).writeU32((self.add(8).readU32() + 1) >>> 0);
        self.add(0xc).writeU32(self.toUInt32());
        return 0x1357;
      }, 'uint32', ['pointer'], 'thiscall')));
      F.__subvt = vt;
    }
    const s = F.c3dj_fill(__keepAlloc(0x40), 0x40, 0x30 + i);
    s.writePointer(F.__subvt);
    s.add(8).writeU32(0);
    return s;
  },
  // VoiceBase objects w0..w7 (0x400 bytes): 16 member lists of 0x18 bytes from +0x24 (header at element+8: front, back, cursor, count),
  // their nodes from +0x200 (two per list); +0x14 the sub-object (rows 1, 2, 4, 6) or NULL. Rows: the node values of each non-empty list.
  c3dj_vbase() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const rows = [
      { sub: 0, l: {} },
      { sub: 1, l: { 0: [1, 1] } },
      { sub: 1, l: { 3: [0], 15: [0] } },
      { sub: 0, l: { 0: [1], 1: [1], 2: [1], 3: [1], 4: [1], 5: [1], 6: [1], 7: [1], 8: [1], 9: [1], 10: [1], 11: [1], 12: [1], 13: [1], 14: [1], 15: [1] } },
      { sub: 1, l: { 0: [0, 1], 1: [0, 1] } },
      { sub: 0, l: { 7: [1, 1], 8: [1, 0] } },
      { sub: 1, l: { 15: [1, 1], 14: [1], 5: [0] } },
      { sub: 0, l: { 2: [1, 1], 9: [1, 1] } }];
    rows.forEach(function (r, i) {
      const w = __keepAlloc(0x400);
      for (let k = 0; k < 0x400; k += 4) w.add(k).writeU32((0x5ab00000 + (i << 12) + k) >>> 0);
      for (let e = 0; e < 16; e++) {
        const vals = (r.l[e] || []).map((v, j) => v ? (0xc9000000 + 0x1000 * i + 0x10 * e + j) >>> 0 : 0);
        const el = w.add(0x24 + 0x18 * e);
        const node = (j) => w.add(0x200 + 0x10 * (2 * e + j));
        vals.forEach(function (v, j) {
          node(j).writePointer(j > 0 ? node(j - 1) : ptr(0));
          node(j).add(4).writePointer(j < vals.length - 1 ? node(j + 1) : ptr(0));
          node(j).add(8).writeU32(v);
        });
        el.add(8).writePointer(vals.length ? node(0) : ptr(0));
        el.add(0xc).writePointer(vals.length ? node(vals.length - 1) : ptr(0));
        el.add(0x10).writeU32(0x0c0c0c0c);
        el.add(0x14).writeU32(vals.length);
      }
      const s = r.sub ? F.c3dj_subobj(i) : null;
      if (s) out['s' + i] = s;
      w.add(0x14).writePointer(s ? s : ptr(0));
      out['w' + i] = w;
    });
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    out.freelog = h.log;
    return out;
  },
  // Midi_Device objects m0..m7 (0x4060 bytes): the ring from +0x50 is filled with markers (CommandQueue::clear zeroes it and the two
  // indices at +0x4050 / +0x4054), the node list header at +0x40 (front, back, cursor +0x48, count +0x4c) with its nodes from +0x3000,
  // the sub-object at +0x14 (rows 1, 3, 5, 7) or NULL.
  c3dj_midi() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const rows = [
      { sub: 0, v: [] }, { sub: 1, v: [1] }, { sub: 0, v: [0] }, { sub: 1, v: [1, 1] }, { sub: 0, v: [0, 1] }, { sub: 1, v: [1, 0, 1] },
      { sub: 0, v: [1, 1, 1] }, { sub: 1, v: [1, 1, 0], count: 0 }];
    rows.forEach(function (r, i) {
      const m = __keepAlloc(0x4060);
      for (let k = 0; k < 0x4060; k += 4) m.add(k).writeU32((0x5ac00000 + (i << 16) + k) >>> 0);
      F.c3dj_vhdr(m, 0x40, r.v.map((v, j) => v ? (0xca000000 + 0x100 * i + j) >>> 0 : 0), 0x3000, r.count, undefined);
      const s = r.sub ? F.c3dj_subobj(i) : null;
      if (s) out['s' + i] = s;
      m.add(0x14).writePointer(s ? s : ptr(0));
      out['m' + i] = m;
    });
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    out.freelog = h.log;
    return out;
  },

  // --- L. constructors: twelve marker objects (row 3 all zero, row 7 all 0xff) of the size the constructor writes over ----------
  c3dj_objs12_120() { return globalThis.DIFF_FIXTURES.c3dj_objs(12, 0x120, { 3: 0x00, 7: 0xff }); },
  c3dj_objs12_1b0() { return globalThis.DIFF_FIXTURES.c3dj_objs(12, 0x1b0, { 3: 0x00, 7: 0xff }); },
  c3dj_objs12_4060() { return globalThis.DIFF_FIXTURES.c3dj_objs(12, 0x4060, { 3: 0x00, 7: 0xff }); },
});
