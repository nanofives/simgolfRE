// C3 batch c3fi (round 34, 2026-10-13): fixtures for shim/src/re/c3fi.cpp (registry re/frida/registry.d/c3fi.py).
//
// Every key: callees not under test are recording stand-ins (re/frida/js/framemask.js FM.stand: A/B thread only, each
// call logged with `this` and its arguments in the compared block $fmlog, frame pointers as layout-independent tokens),
// the game's first thread is frozen (__freeze) and every global the key writes is saved before and restored after it.
// Per-vector inputs come from getters `$rN`: diff_hook resolves a vector's arguments before it snapshots the state, so a
// getter rebuilds that vector's objects and globals and selects that vector's stand-in answers (`S.cur`, the same for
// both arms), then returns the first argument. All objects are ours (one block per key, a state region); fixture text is
// our own synthetic ASCII. Helpers scene / code / immFn / vtable / argRows are copied from c3fa.js.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const w32 = function (a, v) { ptr(a).writeU32(v >>> 0); };
  const r32 = function (a) { return ptr(a).readU32(); };
  const S = { cur: {} };

  function blank(p, n, seed) {
    for (let k = 0; k < n; k += 4) p.add(k).writeU32((seed + k) >>> 0);
  }

  // scene({stands, build(fx), save: [[addr, n]...], init(fx)}): FM stands before the freeze, then the freeze, then the
  // saves, then the negative-control switch (dormant in a normal build).
  function scene(o) {
    const FM = __frameMask.begin({ size: o.fmSize || 0x4000, raw: !!o.raw });
    (o.stands || []).forEach(function (sp) { FM.stand(sp); });
    const fx = Object.assign({ FM: FM }, FM.fx());
    if (o.build) o.build(fx);
    __freeze.thread();
    (o.save || []).forEach(function (s) { __freeze.save(ptr(s[0]), s[1]); });
    if (o.init) o.init(fx);
    __freeze.negOn();
    return fx;
  }

  // Native method stubs for fixture vtables: `mov eax, imm32; ret N`, 16 bytes each so Interceptor can patch them;
  // FM.stand on one turns it into a recording stand-in.
  const le = function (v) { v = v >>> 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; };
  const CODE = { page: null, used: 0 };
  function code(bytes) {
    if (CODE.page === null || CODE.used + 16 > Process.pageSize) {
      CODE.page = Memory.alloc(Process.pageSize);
      Memory.protect(CODE.page, Process.pageSize, 'rwx');
      CODE.used = 0;
    }
    const p = CODE.page.add(CODE.used);
    CODE.used += 16;
    p.writeByteArray(bytes.concat(new Array(16 - bytes.length).fill(0x90)));
    return p;
  }
  const retN = function (pop) { return pop ? [0xc2, pop & 0xff, (pop >>> 8) & 0xff] : [0xc3]; };
  function immFn(imm, pop) { return code([0xb8].concat(le(imm), retN(pop))); }
  // vtable with `slots` entries; every slot a `mov eax, 0x0badc0de; ret` trap unless given in `fill` ({off: NativePointer})
  function vtable(slots, fill) {
    const vt = Memory.alloc(4 * slots);
    const dflt = immFn(0x0badc0de, 0);
    for (let k = 0; k < slots; k++) vt.add(4 * k).writePointer(dflt);
    Object.keys(fill || {}).forEach(function (off) { vt.add(+off).writePointer(fill[off]); });
    return vt;
  }
  // a vbtable: dword 0 = 0, dword k = offs[k - 1]
  function vbtable(blk, at, offs) {
    const p = blk.add(at);
    p.writeU32(0);
    offs.forEach(function (o, k) { p.add(4 * (k + 1)).writeS32(o); });
    return p;
  }

  // Row tables: fx['r' + i] is a getter (selects the row, runs setup, returns argument 0), fx['a' + i + '_' + k] a plain
  // property per further argument.
  function argRows(fx, list, setup) {
    list.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, {
        enumerable: true, configurable: true,
        get: function () { S.cur = row; const v = setup(fx, row, i); return v === undefined ? row.args[0] : v; },
      });
      for (let k = 1; k < (row.args || []).length; k++) fx['a' + i + '_' + k] = row.args[k];
    });
    return fx;
  }

  // ======================================================================================================== unlinkSound
  // Up to eight nodes of 0x50 bytes at blk + 0x80 * k (+0x44 flags, +0x48 prev, +0x4c next), chained in row.chain order;
  // [0x0083af68] = the chain's first node and [0x0083af64] its last (row.g68 / row.g64 override, as plain values).
  const US_ROWS = [
    { node: -1 }, { node: 0, chain: [0], fl: { 0: 2 } }, { node: 0, chain: [0, 1], fl: { 0: 0 } },
    { node: 0, chain: [0, 1, 2], fl: { 0: 0x202 } }, { node: 2, chain: [0, 1, 2], fl: { 2: 0xffffffff } },
    { node: 1, chain: [0, 1, 2], fl: { 1: 2 } }, { node: 2, chain: [0, 1, 2, 3], fl: { 2: 0x206 } },
    { node: 4, chain: [], fl: { 4: 2 }, g68: 0x68686868, g64: 0x64646464 }, { node: 3, chain: [3], fl: { 3: 0xfffffffd } },
    { node: 0, chain: [3, 1, 0, 2], fl: { 0: 2 } }, { node: 1, chain: [0, 1], fl: { 1: 0x200 } },
    { node: 1, chain: [0, 1], fl: { 1: 0x0202 } }, { node: 5, chain: [5, 6, 7], fl: { 5: 0x80000002 } },
  ];
  F.c3fi_us = function () {
    const blk = Memory.alloc(0x400);
    const fx = scene({ build: function (fx) { fx.obj = blk; }, save: [[0x0083af64, 8]] });
    return argRows(fx, US_ROWS.map(function (o) { return Object.assign({ args: [0] }, o); }), function (fx, o, i) {
      blank(blk, 0x400, 0x55000000 + (i << 12));
      const node = function (k) { return blk.add(0x80 * k); };
      for (let k = 0; k < 8; k++) {
        node(k).add(0x44).writeU32(0x00010000 * (k + 1));
        node(k).add(0x48).writeU32(0);
        node(k).add(0x4c).writeU32(0);
      }
      const ch = o.chain || [];
      ch.forEach(function (k, j) {
        node(k).add(0x48).writePointer(j > 0 ? node(ch[j - 1]) : ptr(0));
        node(k).add(0x4c).writePointer(j + 1 < ch.length ? node(ch[j + 1]) : ptr(0));
      });
      Object.keys(o.fl || {}).forEach(function (k) { node(+k).add(0x44).writeU32(o.fl[k]); });
      w32(0x0083af68, o.g68 !== undefined ? o.g68 : (ch.length ? node(ch[0]).toUInt32() : 0));
      w32(0x0083af64, o.g64 !== undefined ? o.g64 : (ch.length ? node(ch[ch.length - 1]).toUInt32() : 0));
      return o.node < 0 ? 0 : node(o.node);
    });
  };

  // ===================================================================================================== Window::setSize
  // The window: a 0x400 block with a fixture vtable (slots +0x11c, +0x54, +0x130, +0x58 recorders), the rects +0x1ac /
  // +0x1bc, the flag byte +0xa0, +0xb0 = the window itself or a peer whose slot +0xc records; a guarded recording display
  // at [0x0083ad50] (slot +0x30). Stood in: adjustSizeOuter (writes row.ao into &w / &h), shrinkRect (records the rect,
  // then moves its four sides by row.sh), passLocalRect, relayoutClip, Window::resized.
  const SS_ROWS = [
    { args: [0, 10, 20, 1, 0x11] }, { args: [0, 10, 20, 1, 0x12], a0: 2, ao: [33, 44] },
    { args: [0, 10, 20, 0, 0x13], sh: 2 }, { args: [0, 30, 40, 0, 0x14], s11c: 1 },
    { args: [0, 1, 2, 0, 0x15], sw: [7, 9, 1] }, { args: [0, 3, 4, 1, 0x16], sw: [100, -5, 0], sh: -3 },
    { args: [0, 50, 60, 1, 0x17], a0: 0xfd }, { args: [0, 50, 60, 1, 0x18], a0: 0x02, self: 1 },
    { args: [0, -7, -9, 0, 0x19], self: 1, sh: 1 }, { args: [0, 5, 6, 5, 0x1a], a0: 3, ao: [-1, 0x7fffffff] },
    { args: [0, 0x10000, 0x20000, 0, 0x1b], big: 1 }, { args: [0, 8, 8, 1, 0x1c], s11c: 0xffffffff },
    { args: [0, 640, 480, 0, 0x1d], s11c: 0, sw: [320, 240, 0], a0: 2 },
  ];
  F.c3fi_ss = function () {
    const win = Memory.alloc(0x400), peer = Memory.alloc(0x40), disp = Memory.alloc(0x40);
    const s11c = immFn(0, 0), s54 = immFn(0, 0xc), s130 = immFn(0, 0), s58 = immFn(0, 0), pc = immFn(0, 0x10),
          d30 = immFn(0, 4);
    const wvt = vtable(0x60, { 0x11c: s11c, 0x54: s54, 0x130: s130, 0x58: s58 });
    peer.writePointer(vtable(0x10, { 0xc: pc }));
    disp.writePointer(vtable(0x40, { 0x30: d30 }));
    const rect = function (p) { return [0, 4, 8, 12].map(function (k) { return ptr(p).add(k).readS32(); }); };
    const fx = scene({
      stands: [
        { addr: s11c, abi: 'thiscall', nargs: 0, ret: function () { return S.cur.s11c || 0; } },
        { addr: s54, abi: 'thiscall', nargs: 3, ret: 0x54,
          write: function (s, a) { const v = S.cur.sw; if (v) for (let k = 0; k < 3; k++) ptr(a[k]).writeS32(v[k]); } },
        { addr: s130, abi: 'thiscall', nargs: 0, ret: 0x130 },
        { addr: s58, abi: 'thiscall', nargs: 0, ret: 0x58 },
        { addr: pc, abi: 'thiscall', nargs: 4, ret: 0xc },
        { addr: d30, abi: 'thiscall', nargs: 1, ret: 0x30 },
        { addr: 0x0047ca10, abi: 'thiscall', nargs: 2, ret: 0xca10, deref: { 0: 1, 1: 1 },          // adjustSizeOuter
          write: function (s, a) { const v = S.cur.ao; if (v) { ptr(a[0]).writeS32(v[0]); ptr(a[1]).writeS32(v[1]); } } },
        { addr: 0x0047cce0, abi: 'thiscall', nargs: 1, ret: 0xcce0, deref: { 0: 4 },                // shrinkRect
          write: function (s, a) {
            const d = S.cur.sh || 0;
            if (d) { const r = rect(a[0]); [d, d, -d, -d].forEach(function (x, k) { ptr(a[0]).add(4 * k).writeS32(r[k] + x); }); }
          } },
        { addr: 0x0047b9a0, abi: 'thiscall', nargs: 0, ret: 0xb9a0 },                                // passLocalRect
        { addr: 0x0047e140, abi: 'mscdecl', nargs: 2, ret: 0xe140, deref: { 1: 4 } },              // relayoutClip
        { addr: 0x0047bc60, abi: 'thiscall', nargs: 2, ret: 0xbc60 },                                // Window::resized
      ],
      build: function (fx) { fx.obj = win; __recDisplay.guard(disp); },
      save: [[0x0083ad50, 4]],
      init: function () { w32(0x0083ad50, disp.toUInt32()); },
    });
    return argRows(fx, SS_ROWS, function (fx, o, i) {
      blank(win, 0x400, 0xc0000000 + (i << 16));
      win.writePointer(wvt);
      win.add(0xa0).writeU8(o.a0 || 0);
      const r1ac = o.big ? [0x7fff0000, -0x7fff0000, 5, 6] : [3 + i, 5 + 2 * i, 103 + i, 205 + 2 * i];
      const r1bc = o.big ? [0x7ffffff0, 0x7ffffff8, 1, 2] : [-11 - i, 17 + i, 89, 91 - i];
      r1ac.forEach(function (v, k) { win.add(0x1ac + 4 * k).writeS32(v); });
      r1bc.forEach(function (v, k) { win.add(0x1bc + 4 * k).writeS32(v); });
      win.add(0xb0).writePointer(o.self ? win : peer);
      return win;
    });
  };

  // ================================================================================================= Cache482f60::dtor
  // blk: the full object at +0 (`this` = blk + 0x20), its vbtable at +0x800 (second dword = row.vb), nodes of 0x40 at
  // +0x100 + 0x40 k (vbtable +0x820 -> base at +0x10 whose slot 0 records), data objects of 0x40 at +0x400 + 0x40 k
  // (vbtable +0x840 -> base at +0x20, slot 0 records). freeCacheField 0x0049c8b0 (slot 1 of 0x004babac) is stood in.
  const CD_ROWS = [
    { chain: [], count: 3 }, { chain: [0], count: 1, data: [0] }, { chain: [0, 1, 2], count: 3, data: [0, 1, 2] },
    { chain: [0, 1, 2], count: 2, data: [3, 4, 5] }, { chain: [0, 1], count: 0, data: [0, 1] },
    { chain: [0], count: -1, data: [0] }, { chain: [0, 1, 2, 3], count: 4, data: [-1, 1, -1, 3] },
    { chain: [2], count: 1, data: [2], vb: 0x3c }, { chain: [3, 0], count: 2, data: [-1, -1], vb: 0x5c },
    { chain: [1, 2, 3], count: 3, data: [5, 6, 7], vb: 0x1c, f14: 0x14141414 },
    { chain: [4, 5, 6, 7, 0], count: 5, data: [0, 1, 2, 3, 4], vb: 0x24 },
    // vb 4: both base-vtable stores (0x00483621, 0x004836bc) land on the head +8, so pass 1 sees head 0x004baba8 with a
    // count <= 0 (no walk) and pass 2 sees head 0x004ba2a8 with count 0 (its head test's nonzero side, 0x004836c9)
    { chain: [], count: 0, vb: 4 }, { chain: [0], count: -3, data: [0], vb: 4, f14: 0x7e57 },
  ];
  F.c3fi_cd = function () {
    const blk = Memory.alloc(0x1000);
    const nodeDel = immFn(0, 4), dataDel = immFn(0, 4);
    const nodeVt = vtable(4, { 0: nodeDel }), dataVt = vtable(4, { 0: dataDel });
    const fx = scene({
      stands: [
        { addr: 0x0049c8b0, abi: 'thiscall', nargs: 1, ret: 0xc8b0 },        // freeCacheField
        { addr: nodeDel, abi: 'thiscall', nargs: 1, ret: 0x0de1 },
        { addr: dataDel, abi: 'thiscall', nargs: 1, ret: 0xda7a },
      ],
      build: function (fx) { fx.obj = blk; },
    });
    return argRows(fx, CD_ROWS.map(function (o) { return Object.assign({ args: [0] }, o); }), function (fx, o, i) {
      blank(blk, 0x1000, 0xcd000000 + (i << 16));
      const full = blk;
      full.add(4).writePointer(vbtable(blk, 0x800, [o.vb || 0x1c]));
      const nvb = vbtable(blk, 0x820, [0x10]), dvb = vbtable(blk, 0x840, [0x20]);
      const node = function (k) { return blk.add(0x100 + 0x40 * k); };
      const data = function (k) { return blk.add(0x400 + 0x40 * k); };
      for (let k = 0; k < 8; k++) {
        node(k).writePointer(nvb); node(k).add(0x10).writePointer(nodeVt);
        data(k).writePointer(dvb); data(k).add(0x20).writePointer(dataVt);
      }
      o.chain.forEach(function (k, j) {
        node(k).add(0xc).writePointer(j + 1 < o.chain.length ? node(o.chain[j + 1]) : ptr(0));
        const d = (o.data || [])[j];
        node(k).add(8).writePointer(d === undefined || d < 0 ? ptr(0) : data(d));
      });
      full.add(8).writePointer(o.chain.length ? node(o.chain[0]) : ptr(0));
      full.add(0x10).writeS32(o.count);
      if (o.f14 !== undefined) full.add(0x14).writeU32(o.f14);
      return full.add(0x20);
    });
  };

  // ================================================================================================== Dialog::viewDtor
  // blk: b = blk (`this` = b + 0x1fc); [b + 4] = a vbtable at +0xe00 (dwords 1, 2 = row.o), the sub-object s = b + 0x58 with
  // [s] = a vbtable at +0xe40 (dwords 1, 2 = row.u). The five callees are stood in (ListBox::dtor answers row.lb); the
  // bare `ret` 0x004a0890 runs as it is (one byte: nothing to patch, nothing to observe).
  const VD_ROWS = [
    { o: [0x1f8, 0x770], u: [0x24, 0x59c], lb: 0x11 }, { o: [0x200, 0x300], u: [0x100, 0x180], lb: 0x22 },
    { o: [0x100, 0x7f0], u: [0x30, 0x40], lb: 0 }, { o: [0x80, 0x90], u: [0x200, 0x210], lb: 0xffffffff },
    { o: [0x300, 0x20], u: [0x500, 0x8], lb: 0x55 }, { o: [0x8, 0x10], u: [0x10, 0x14], lb: 0x66 },
    { o: [0x400, 0x404], u: [0x600, 0x604], lb: 0x77 }, { o: [0x1f8, 0x1fc], u: [0x24, 0x28], lb: 0x88 },
    { o: [0x700, 0x100], u: [0x400, 0x300], lb: 0x99 }, { o: [0x60, 0x6c0], u: [0x6a0, 0x50], lb: 0xaa },
  ];
  F.c3fi_vd = function () {
    const blk = Memory.alloc(0x1000);
    const fx = scene({
      stands: [
        { addr: 0x004a2330, abi: 'thiscall', nargs: 0, ret: 0x2330 },        // viewFieldDtor4a2330
        { addr: 0x004a3480, abi: 'thiscall', nargs: 0, ret: 0x3480 },        // panelSubDtor4a3480
        { addr: 0x0049ec80, abi: 'thiscall', nargs: 0, ret: 0xec80 },        // viewFieldDtor49ec80
        { addr: 0x0049d690, abi: 'thiscall', nargs: 0, ret: 0xd690 },        // viewFieldCleanup49d690
        { addr: 0x00489de0, abi: 'thiscall', nargs: 0, ret: function () { return S.cur.lb; } },   // ListBox::dtor
      ],
      build: function (fx) { fx.obj = blk; },
    });
    return argRows(fx, VD_ROWS.map(function (o) { return Object.assign({ args: [0] }, o); }), function (fx, o, i) {
      blank(blk, 0x1000, 0x7d000000 + (i << 16));
      blk.add(4).writePointer(vbtable(blk, 0xe00, o.o));
      blk.add(0x58).writePointer(vbtable(blk, 0xe40, o.u));
      return blk.add(0x1fc);
    });
  };

  // ======================================================================================================= Dialog::dtor
  // blk = the dialog (0x2400 bytes); the two list members' vbtables [+0x1414] / [+0x13e4] at +0x2300 / +0x2340 (dword 1 =
  // row.v); every one of the eleven callees is stood in, Base4805a0::dtor's second call answers row.ret. [0x00839650] is
  // saved and compared.
  const DD_ROWS = [
    { v: [0x10, 0x20], ret: 0x5a0 }, { v: [0x30, 0x8], ret: 0 }, { v: [0x4, 0x4], ret: 0xffffffff },
    { v: [0x100, 0x200], ret: 0x12 }, { v: [0x40, 0x44], ret: 0x34 }, { v: [-0x10, -0x20], ret: 0x56 },
    { v: [0x2c, 0x2c], ret: 0x78 }, { v: [0x80, 0x400], ret: 0x9a }, { v: [0x18, 0x50], ret: 0xbc },
    { v: [0x60, 0x70], ret: 0xde },
  ];
  F.c3fi_dd = function () {
    const blk = Memory.alloc(0x2400);
    const st = function (addr, ret) { return { addr: addr, abi: 'thiscall', nargs: 0, ret: ret }; };
    const fx = scene({
      stands: [
        st(0x0048d480, 0xd480), st(0x004928d0, 0x28d0), st(0x00491500, 0x1500), st(0x00489370, 0x9370),
        st(0x004805a0, function (s, a, n) { return n === 1 ? S.cur.ret : 0x05a0; }),
        st(0x004026a0, 0x26a0), st(0x00401c70, 0x1c70), st(0x00473ae0, 0x3ae0), st(0x004a14c0, 0x14c0),
        st(0x00488650, 0x8650), st(0x00474810, 0x4810),
      ],
      build: function (fx) { fx.obj = blk; },
      save: [[0x00839650, 4]],
    });
    return argRows(fx, DD_ROWS.map(function (o) { return Object.assign({ args: [0] }, o); }), function (fx, o, i) {
      blank(blk, 0x2400, 0xdd000000 + (i << 16));
      blk.add(0x1414).writePointer(vbtable(blk, 0x2300, [o.v[0]]));
      blk.add(0x13e4).writePointer(vbtable(blk, 0x2340, [o.v[1]]));
      w32(0x00839650, 0x96500000 + i);
      return blk;
    });
  };

  // ============================================================================================================ netPoll
  // blk: the session object (+0 vtable: slots +0x10 / +0x14 record), +4 the receive buffer blk + 0x800 (0x100 bytes),
  // peers +0x170. queuePop 0x004a4c70 is stood in by a scripted queue in compared memory: entries of 0x110 bytes at
  // blk + 0x1000 (+0 result, +4 user word, +0x10 the 0x100 message bytes), cursor at blk + 0xffc. The interface
  // [0x008400b0] (row.iface) is a fixture object whose slot +0x54 records and fills the name record 0x0083ff9c (its +0xc,
  // [0x0083ffa8], = a name pointer in blk). netHandleControl, netQueueMessage, netSendShutdown are stood in.
  const NP_NAME = 0xf00, NP_FLAG = 0xfc0;
  // message builders: [result, user, {off: dword}], bytes written as dwords at those offsets of the 0x100-byte message
  const m = function (user, fields) { return [1, user, fields]; };
  const NP_ROWS = [
    { iface: 0 },
    { iface: 1, nobuf: 1 },
    { iface: 1, q: [] },
    { iface: 1, q: [m(1, { 0: 0x80, 8: 0x4444 })] },
    { iface: 1, q: [m(1, { 0: 0x10, 8: 0 })] },
    { iface: 1, q: [m(1, { 0: 0x10, 8: 3, 0xc: 0x101, 0x10: 0x61616161, 0x30: 7, 0x34: 0x102, 0x38: 0x62626262,
                           0x58: 8, 0x5c: 0x103, 0x60: 0x63, 0x80: 9 })] },
    { iface: 1, q: [m(1, { 0: 0x90, 8: 0x55 }), m(7, { 0: 0x0f, 8: 2 }), m(1, { 0: 0x110, 8: 1, 0xc: 0x201, 0x10: 0x41, 0x30: 0x11 })] },
    { iface: 1, q: [m(0, { 0: 0x31 })] },
    { iface: 1, host: [5, 5], q: [m(0, { 0: 3, 4: 1, 8: 0x301, 0x10: NP_FLAG })], flag: 0x3c },
    { iface: 1, host: [5, 5], q: [m(0, { 0: 3, 4: 1, 8: 0x302, 0x10: 0 })] },
    { iface: 1, host: [5, 6], q: [m(0, { 0: 3, 4: 1, 8: 0x303, 0x10: NP_FLAG })], flag: 1 },
    { iface: 1, host: [5, 5], q: [m(0, { 0: 3, 4: 2, 8: 0x304, 0x10: NP_FLAG })], flag: 2 },
    { iface: 1, q: [m(0, { 0: 5, 4: 1, 8: 0x501 }), m(0, { 0: 5, 4: 0, 8: 0x502 })] },
    { iface: 1, peers: [0x11, 0x12, 0x13], q: [m(0, { 0: 0x102, 8: 0x12, 0xc: NP_FLAG }), m(0, { 0: 0x102, 8: 0x99, 0xc: NP_FLAG }),
                                                 m(0, { 0: 0x102, 8: 0x13, 0xc: 0 })], flag: 0x77 },
    { iface: 1, q: [m(0, { 0: 0x103, 8: 0x1031, 0x18: 0x1032 }), m(0, { 0: 2 }), m(0, { 0: 4 }), m(0, { 0: 0x30 })] },
    { iface: 1, q: [m(0, { 0: 0x101 }), m(0, { 0: 0x104 }), m(0, { 0: 0xffffffff }), m(0, { 0: 0 })] },
    { iface: 1, peers: Array.from({ length: 16 }, function (_, k) { return 0x40 + k; }),
      q: [m(0, { 0: 0x102, 8: 0x4f, 0xc: NP_FLAG }), m(1, { 0: 0x00, 8: 0 })], flag: 0x5e },
  ];
  F.c3fi_np = function () {
    const blk = Memory.alloc(0x1800), iface = Memory.alloc(0x40);
    const s10 = immFn(0, 8), s14 = immFn(0, 0), i54 = immFn(0, 0x10);
    const ovt = vtable(0x20, { 0x10: s10, 0x14: s14 });
    iface.writePointer(vtable(0x20, { 0x54: i54 }));
    const cur = blk.add(0xffc);
    const fx = scene({
      stands: [
        // queuePop: its outA word is the original's uninitialised frame slot until this writes it, so it is not dereferenced
        { addr: 0x004a4c70, abi: 'thiscall', nargs: 4,
          ret: function (s, a) {
            const k = cur.readU32();
            const e = blk.add(0x1000 + 0x110 * Math.min(k, 6));
            cur.writeU32(k + 1);
            const r = e.readU32();
            if (r !== 0) {
              ptr(a[0]).writeByteArray(e.add(0x10).readByteArray(0x100));
              ptr(a[1]).writeU32(e.add(4).readU32());
            }
            return r;
          } },
        { addr: 0x0049acf0, abi: 'thiscall', nargs: 2, ret: 0xacf0 },                              // netHandleControl
        { addr: 0x0049b690, abi: 'thiscall', nargs: 3, ret: 0xb690 },                              // netQueueMessage
        { addr: 0x00497d10, abi: 'thiscall', nargs: 0, ret: 0x7d10 },                              // netSendShutdown
        { addr: s10, abi: 'thiscall', nargs: 2, ret: 0x10 },
        { addr: s14, abi: 'thiscall', nargs: 0, ret: 0x14 },
        { addr: i54, abi: 'stdcall', nargs: 4, ret: 0x54, deref: { 3: 1 },
          write: function (s, a) {
            ptr(a[2]).writeU32(0x9c000000 + (a[1] & 0xffff));
            ptr(a[2]).add(0xc).writePointer(blk.add(NP_NAME + 0x10 * (a[1] & 7)));
          } },
      ],
      build: function (fx) { fx.obj = blk; },
      save: [[0x008400b0, 4], [0x0083ff9c, 0x10]],
    });
    return argRows(fx, NP_ROWS.map(function (o) { return Object.assign({ args: [0] }, o); }), function (fx, o, i) {
      blank(blk, 0x1800, 0x9e000000 + (i << 16));
      blk.writePointer(ovt);
      blk.add(4).writePointer(o.nobuf ? ptr(0) : blk.add(0x800));
      const h = o.host || [1, 2];
      blk.add(0x77c).writeU32(h[0]); blk.add(0x780).writeU32(h[1]);
      for (let k = 0; k < 16; k++) blk.add(0x170 + 0x58 * k).writeU32(0x7700 + k);
      (o.peers || []).forEach(function (id, k) { blk.add(0x170 + 0x58 * k).writeU32(id); });
      for (let k = 0; k < 8; k++) blk.add(NP_NAME + 0x10 * k).writeUtf8String('peer' + k);
      blk.add(NP_FLAG).writeU8(o.flag || 0);
      cur.writeU32(0);
      const q = o.q || [];
      for (let k = 0; k < 7; k++) {
        const e = blk.add(0x1000 + 0x110 * k);
        const x = q[k];
        e.writeU32(x ? x[0] : 0);
        e.add(4).writeU32(x ? x[1] : 0);
        for (let b = 0; b < 0x100; b += 4) e.add(0x10 + b).writeU32(0);
        if (x) Object.keys(x[2]).forEach(function (off) {
          const v = x[2][off];
          e.add(0x10 + (+off)).writeU32(v === NP_FLAG ? blk.add(NP_FLAG).toUInt32() : v >>> 0);
        });
      }
      w32(0x008400b0, o.iface ? iface.toUInt32() : 0);
      for (let k = 0; k < 0x10; k += 4) w32(0x0083ff9c + k, 0x83ff0000 + k);
      return blk;
    });
  };

  // ==================================================================================================== netSendShutdown
  // blk: the session object (0x1000): sub-object +0x64 (vtable slot +0x14 records), list +0xc0 (vtable slot +4 records;
  // +0xc8 head, +0xcc cursor, +0xd0 count), nodes of 0x40 at +0x800 + 0x40 k (vbtable at +0xe00 -> base at +0x10 whose
  // slot 0 records), payloads of 0x20 at +0xa00 + 0x20 k (slot 0 records). The interface [0x008400b0] is a fixture object
  // (slots +0x10 / +8 record). The KERNEL32 imports go through the import stand-in ($implog), free 0x004a5007 and
  // listClear4a4b 0x004a4b00 are stood in.
  const SD_ROWS = [
    { iface: 1, cs: 1, h: [1, 1, 1, 1, 1], b: [1, 1, 1, 1], chain: [0, 1], count: 2, pay: [0, 1] },
    { iface: 0, cs: 0, h: [0, 0, 0, 0, 0], b: [0, 0, 0, 0], chain: [], count: 0 },
    { iface: 1, cs: 0, h: [1, 0, 1, 0, 1], b: [0, 1, 0, 1], chain: [0], count: 1, pay: [-1] },
    { iface: 0, cs: 1, h: [0, 1, 0, 1, 0], b: [1, 0, 1, 0], chain: [2, 1, 0], count: 3, pay: [2, -1, 0] },
    { iface: 1, cs: 1, h: [0, 0, 0, 0, 1], b: [0, 0, 0, 1], chain: [0, 1, 2], count: 2, pay: [0, 1, 2] },
    { iface: 1, cs: 0, h: [1, 1, 0, 0, 0], b: [1, 1, 0, 0], chain: [3], count: 0, pay: [3] },
    { iface: 0, cs: 0, h: [1, 1, 1, 1, 1], b: [0, 0, 1, 1], chain: [1], count: -2, pay: [1] },
    { iface: 1, cs: 1, h: [0, 0, 1, 1, 0], b: [1, 1, 1, 1], chain: [0, 1, 2, 3], count: 4, pay: [-1, -1, -1, -1] },
    { iface: 0, cs: 1, h: [0, 0, 0, 0, 0], b: [0, 0, 0, 0], chain: [4, 5], count: 2, pay: [4, 5], vb: 0x14 },
    { iface: 1, cs: 0, h: [1, 0, 0, 0, 0], b: [0, 0, 0, 0], chain: [], count: 5 },
  ];
  F.c3fi_sd = function () {
    const blk = Memory.alloc(0x1000), iface = Memory.alloc(0x40);
    const sub14 = immFn(0, 0), l4 = immFn(0, 4), pay0 = immFn(0, 4), node0 = immFn(0, 4), i10 = immFn(0, 4), i8 = immFn(0, 4);
    const subVt = vtable(0x10, { 0x14: sub14 }), lvt = vtable(0x10, { 4: l4 }), payVt = vtable(4, { 0: pay0 }),
          nodeVt = vtable(4, { 0: node0 });
    iface.writePointer(vtable(0x10, { 0x10: i10, 8: i8 }));
    let implog = null;
    const fx = scene({
      stands: [
        { addr: sub14, abi: 'thiscall', nargs: 0, ret: 0x14 },
        { addr: l4, abi: 'thiscall', nargs: 1, ret: 0x4 },
        { addr: pay0, abi: 'thiscall', nargs: 1, ret: 0xa0 },
        { addr: node0, abi: 'thiscall', nargs: 1, ret: 0xd0 },
        { addr: i10, abi: 'stdcall', nargs: 1, ret: 0x10 },
        { addr: i8, abi: 'stdcall', nargs: 1, ret: 0x8 },
        { addr: 0x004a5007, abi: 'mscdecl', nargs: 1, ret: 0 },                                       // free
        { addr: 0x004a4b00, abi: 'thiscall', nargs: 0, ret: function (s, a, n) { return 0x4b000000 + 0x100 * n + (s & 0xff); } },
      ],
      build: function (fx) {
        fx.obj = blk;
        implog = __importStandIn({ module: 'golf_clean.exe', logSize: 0x1000, imports: {
          'KERNEL32.dll!EnterCriticalSection': { ret: 0 }, 'KERNEL32.dll!LeaveCriticalSection': { ret: 0 },
          'KERNEL32.dll!SetEvent': { args: 1, ret: 1 }, 'KERNEL32.dll!Sleep': { args: 1, ret: 0 },
          'KERNEL32.dll!DeleteCriticalSection': { args: 1, ret: 0 }, 'KERNEL32.dll!CloseHandle': { ret: 1 },
        } }).log;
        fx.implog = implog;
      },
      save: [[0x008400b0, 4]],
    });
    return argRows(fx, SD_ROWS.map(function (o) { return Object.assign({ args: [0] }, o); }), function (fx, o, i) {
      blank(blk, 0x1000, 0x5d000000 + (i << 16));
      blk.add(0x64).writePointer(subVt);
      blk.add(0x34).writeU32(o.cs ? 0x34 + i : 0);
      [0x14, 0x18, 0x1c, 0x20, 0x24].forEach(function (off, k) { blk.add(off).writeU32(o.h[k] ? 0x4000 + 0x10 * i + k : 0); });
      [0x720, 0x724, 0x6fc, 4].forEach(function (off, k) { blk.add(off).writeU32(o.b[k] ? 0x0b000000 + 0x100 * i + k : 0); });
      const nvb = vbtable(blk, 0xe00, [o.vb || 0x10]);
      const node = function (k) { return blk.add(0x800 + 0x40 * k); };
      const pay = function (k) { return blk.add(0xa00 + 0x20 * k); };
      for (let k = 0; k < 6; k++) {
        node(k).writePointer(nvb); node(k).add(o.vb || 0x10).writePointer(nodeVt);
        pay(k).writePointer(payVt);
      }
      blk.add(0xc0).writePointer(lvt);
      o.chain.forEach(function (k, j) {
        node(k).add(0xc).writePointer(j + 1 < o.chain.length ? node(o.chain[j + 1]) : ptr(0));
        const p = (o.pay || [])[j];
        node(k).add(8).writePointer(p === undefined || p < 0 ? ptr(0) : pay(p));
      });
      blk.add(0xc8).writePointer(o.chain.length ? node(o.chain[0]) : ptr(0));
      blk.add(0xd0).writeS32(o.count);
      w32(0x008400b0, o.iface ? iface.toUInt32() : 0);
      return blk;
    });
  };

  // ================================================================================================ Flic::createStream
  // blk: the stream at +0 (+8 its Surface, whose inner object [+0xc] is blk + 0x300 with a recording slot +0x10, or 0),
  // src at +0x100 (+0x44, +0x58 = hdr, +0x74 = palette), hdr at +0x200 (words +8, +0xa, +0x60, +0x64..+0x6a, mask +0x70),
  // palettes at +0x280 / +0x2c0 (the latter is [0x0083ad0c] when row.gpal), operator_new's block at +0x400. Stood in:
  // Stream482dd0::dtor, Surface_create (answers row.sc[call]), operator_new (row.nonew: NULL), the eh vector constructor
  // iterator, Link482b::send (answers row.send[call], default 1).
  const FCS_ROWS = [
    { src: 0 }, { f44: 0 }, { sc: [5] }, { frames: 0, pal: 1 }, { frames: 0, gpal: 1, send: [0] },
    { frames: 0, pal: 1, noinner: 1 }, { frames: 0 }, { n: 3, mask: 0, send: [1, 1, 1, 2] },
    { n: 4, mask: 0xa, gpal: 1 }, { n: 3, nonew: 1, pal: 1 }, { n: 0, mask: 0, pal: 1 },
    { n: 3, mask: 0, sc: [0, 0, 9] }, { n: 2, mask: 0, send: [0] }, { n: 35, mask: 0x80000001, send: [3, 3, 3] },
    { n: 5, mask: 0x7fffffe0, send: [1] },
  ];
  F.c3fi_fcs = function () {
    const blk = Memory.alloc(0x1000);
    const slot10 = immFn(0, 4);
    const ivt = vtable(0x10, { 0x10: slot10 });
    const fx = scene({
      stands: [
        { addr: 0x00482dd0, abi: 'thiscall', nargs: 0, ret: 0x2dd0 },                               // Stream482dd0::dtor
        { addr: 0x00474550, abi: 'thiscall', nargs: 2, ret: function (s, a, n) { return (S.cur.sc || [])[n] || 0; } },
        { addr: slot10, abi: 'thiscall', nargs: 1, ret: 0x10 },
        { addr: 0x004a57f1, abi: 'mscdecl', nargs: 1, ret: function () { return S.cur.nonew ? 0 : blk.add(0x400).toUInt32(); } },
        { addr: 0x004a55d2, abi: 'stdcall', nargs: 5, ret: 0x55d2 },                                // eh vector ctor iterator
        { addr: 0x00482e40, abi: 'thiscall', nargs: 3,                                              // Link482b::send
          ret: function (s, a, n) { const v = (S.cur.send || [])[n]; return v === undefined ? 1 : v; } },
      ],
      build: function (fx) { fx.obj = blk; },
      save: [[0x0083ad0c, 4]],
    });
    const rows = FCS_ROWS.map(function (o, i) {
      return Object.assign({}, o, { args: [blk, o.src === 0 ? 0 : blk.add(0x100), o.frames === 0 ? 0 : 1 + i] });
    });
    return argRows(fx, rows, function (fx, o, i) {
      blank(blk, 0x1000, 0xf1000000 + (i << 16));
      const src = blk.add(0x100), hdr = blk.add(0x200);
      src.add(0x44).writeU32(o.f44 === 0 ? 0 : 0x44 + i);
      src.add(0x58).writePointer(hdr);
      src.add(0x74).writePointer(o.pal ? blk.add(0x280) : ptr(0));
      blk.add(0xc).writePointer(o.noinner ? ptr(0) : blk.add(0x300));
      blk.add(0x300).writePointer(ivt);
      hdr.add(8).writeU16(0x140 + i); hdr.add(0xa).writeU16(0xc8 + 2 * i);
      hdr.add(0x60).writeU16(o.n === undefined ? 2 : o.n);
      hdr.add(0x64).writeU16(0x8064 + i); hdr.add(0x66).writeU16(0x66 + i); hdr.add(0x68).writeU16(0xff68); hdr.add(0x6a).writeU16(i);
      hdr.add(0x70).writeU32(o.mask === undefined ? 0 : o.mask);
      w32(0x0083ad0c, o.gpal ? blk.add(0x2c0).toUInt32() : 0);
      return blk;
    });
  };

  // ======================================================================================================= placeHabitat
  // Random::range is stood in by a scripted generator in compared memory (blk +0 cursor, answers from +4; past the end
  // it answers 0), tileBlocked by row.tb (a number, or [answers by call]), playSoundAt / queueMessage / playHabitatSound
  // record. addHabitatSlot runs as it is (it calls the scripted Random::range too). The tile type grid 0x005722e8, the
  // flag words 0x0053caf0, the habitat slots 0x00572cb0, the text buffer and [0x005a5a00] / [0x005a34e0] are saved,
  // seeded per row and compared.
  const PH_ROWS = [
    { args: [10, 12, 3], rng: [1, 1, 1] },
    { args: [10, 12, 4], rng: [0, 2, 0x10000, 5] },
    { args: [20, 21, 5], rng: [1, 1, 0, 12], bit20: 0 },
    { args: [20, 21, 4], rng: [1, 1, 0, 12, 1, 2, 1, 0, 2, 3], tile: 0x31 },
    { args: [30, 31, 7], rng: [2, 0, 0, 12, 1, 3, 4, 6, 7, 8, 1, 3, 4, 0x30005], tile: 0xd },
    { args: [5, 6, 2], rng: [0, 0, 0, 12, 1, 1, 1, 2, 0, 1, 1, 2], tile: 4, tb: [1, 1, 0] },
    { args: [40, 41, 9], rng: [0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], tile: 0xe, s34: 1 },
    { args: [40, 41, 9], rng: [0, 0, 0, 12, 0, 1, 0, 0, 1, 2], tile: 0xe, s34: 3 },
    { args: [15, 16, 1], rng: [1, 0, 0, 12, 3, 0, 5], tile: 4, s34: 3 },
    { args: [15, 16, 1], rng: [1, 0, 0, 12, 3, 0, 1], tile: 4, s34: 3, used11: 1 },
    { args: [25, 26, 0], rng: [0, 0, 0x20000, 12, 5, 1, 0, 2, 2, 1], tile: 0xc, s34: 2 },
    { args: [33, 34, 0xff], rng: [2, 2, 0, 12, 2, 1, 1, 1, 1, 0], tile: 0xf, s34: 2, tb: 0 },
    { args: [2, 47, 0x80], rng: [0, 1, 0, 12, 6, 0, 1, 2, 0, 3], tile: 4, s34: 1, tb: 0 },
  ];
  F.c3fi_ph = function () {
    const blk = Memory.alloc(0x200);
    const fx = scene({
      stands: [
        { addr: 0x0045c1e0, abi: 'thiscall', nargs: 1,                                               // Random::range
          ret: function () { const k = blk.readU32(); blk.writeU32(k + 1); return k < 0x70 ? blk.add(4 + 4 * k).readU32() : 0; } },
        { addr: 0x0040bf60, abi: 'mscdecl', nargs: 2,                                                // tileBlocked
          ret: function (s, a, n) { const t = S.cur.tb; return Array.isArray(t) ? (t[n] === undefined ? 0 : t[n]) : (t || 0); } },
        { addr: 0x0040c500, abi: 'mscdecl', nargs: 4, ret: 0xc500 },                                 // playSoundAt
        { addr: 0x0040c720, abi: 'mscdecl', nargs: 3, ret: 0xc720 },                                 // queueMessage
        { addr: 0x00405a00, abi: 'mscdecl', nargs: 3, ret: function (s, a) { return 0x5a000000 + a[0]; } },  // playHabitatSound
      ],
      build: function (fx) { fx.obj = blk; },
      save: [[0x005722e8, 0x9c4], [0x0053caf0, 0x1388], [0x00572ca8, 0xa10], [0x0051a068, 0x40], [0x005a5a00, 4],
             [0x005a34e0, 4]],
    });
    const g5a00 = r32(0x005a5a00);
    return argRows(fx, PH_ROWS, function (fx, o, i) {
      blk.writeByteArray(new Uint8Array(0x200).buffer);
      o.rng.forEach(function (v, k) { blk.add(4 + 4 * k).writeU32(v); });
      const x = o.args[0], y = o.args[1];
      for (let dx = -1; dx <= 1; dx++) for (let dy = -1; dy <= 1; dy++) {
        const c = (x + dx) * 50 + (y + dy);
        ptr(0x005722e8 + c).writeU8(o.tile === undefined ? 0x20 + i : o.tile);
        ptr(0x0053caf0 + 2 * c).writeU16(0xffff - i);
      }
      for (let k = 0; k < 128; k++) {
        const s = ptr(0x00572cb0 + 0x14 * k);
        for (let b = 0; b < 0x14; b += 4) s.add(b).writeU32(0x57000000 + (k << 8) + b + i);
        s.add(0x12).writeU8(k < 10 || (k === 11 && o.used11) ? k : 0xff);
      }
      w32(0x005a5a00, o.bit20 === 0 ? (g5a00 & ~0x20) : (g5a00 | 0x20));
      ptr(0x005a34e0).writeU8(o.s34 || 0);
      return x;
    });
  };

  // ======================================================================================================== Reader::open
  // blk: the reader at +0 (+4 name, +0x54 copy, +0x154 matched line, +0x158 FILE, +0x15c line buffer = blk + 0x400), a
  // fixture FILE at +0x800 (+0xc flag byte, +0x20 line cursor), the file's lines at +0x900 (0x40 bytes each, count at
  // +0x8fc). Stood in: Reader::closeFile, resolveAndOpenFile (row.open: the FILE, else NULL), KeyTable::lookup (row.off,
  // default -1), logLine, and the CRT fseek (cursor = offset, end-of-file bit cleared), rewind (cursor 0, bits 0x30
  // cleared) and fgets (copies the next line and answers the buffer; at the end it sets bit 0x10 and answers NULL;
  // row.eofLast sets bit 0x10 together with the last line, row.err answers NULL with the bit clear). __strnicmp, strchr,
  // stripNewline, trimSpaces and __strcmpi run as they are.
  const RO_ROWS = [
    { path: null, file: 0, open: 0 }, { path: null, file: 0, open: 1, sec: null },
    { path: null, file: 1, sec: null }, { path: 'SAME.ini', file: 1, sec: 'alpha', off: 2, lines: ['x', 'y', '#alpha  \n'] },
    { path: 'dir\\cfg', file: 1, open: 1, sec: 'beta', lines: ['x', '  #beta\t \n', 'z'] },
    { path: 'a.cfg', file: 1, open: 0, sec: 'beta' },
    { path: null, file: 1, sec: 'gamma', lines: ['#alpha', '#beta'] },
    { path: 'next.cfg', file: 1, open: 1, sec: 'alpha', off: 3, lines: ['x', '#ALPHA\n', 'y', 'z', 'w'] },
    { path: 'empty.cfg', open: 1, sec: 'delta', lines: [], stale: '#delta' },
    { path: 'empty.cfg', open: 1, sec: 'delta', lines: [], stale: 'other' },
    { path: null, file: 1, sec: 'eps', lines: ['a', '#eps'], eofLast: 1 },
    { path: null, file: 1, sec: 'eps', lines: ['a', '#eps  '], eofLast: 1 },
    { path: 'SAM', file: 1, sec: 'zeta', lines: ['#zeta\n'] },
    { path: 'err.cfg', open: 1, sec: 'eta', lines: ['a'], err: 1 },
  ];
  F.c3fi_ro = function () {
    const blk = Memory.alloc(0x1000);
    const file = blk.add(0x800);
    const fx = scene({
      stands: [
        { addr: 0x00487f80, abi: 'thiscall', nargs: 0, ret: 0x7f80 },                               // Reader::closeFile
        { addr: 0x004a00f0, abi: 'mscdecl', nargs: 2, str: { 0: 6, 1: 1 },                          // resolveAndOpenFile
          ret: function () { return S.cur.open ? file.toUInt32() : 0; } },
        { addr: 0x00488420, abi: 'mscdecl', nargs: 2, str: { 1: 3 },                                // KeyTable::lookup
          ret: function () { return S.cur.off === undefined ? -1 : S.cur.off; } },
        { addr: 0x004a0320, abi: 'mscdecl', nargs: 5, ret: 0x0320 },                                // logLine
        { addr: 0x004a7119, abi: 'mscdecl', nargs: 3, ret: 0,                                       // fseek
          write: function (s, a) { ptr(a[0]).add(0x20).writeU32(a[1]); ptr(a[0]).add(0xc).writeU8(ptr(a[0]).add(0xc).readU8() & ~0x10); } },
        { addr: 0x004a70b5, abi: 'mscdecl', nargs: 1, ret: 0,                                       // rewind
          write: function (s, a) { ptr(a[0]).add(0x20).writeU32(0); ptr(a[0]).add(0xc).writeU8(ptr(a[0]).add(0xc).readU8() & ~0x30); } },
        { addr: 0x004a65ee, abi: 'mscdecl', nargs: 3, str: { 0: 0 },                                // fgets
          ret: function (s, a) {
            const f = ptr(a[2]), k = f.add(0x20).readU32(), n = blk.add(0x8fc).readU32();
            if (S.cur.err) return 0;
            if (k >= n) { f.add(0xc).writeU8(f.add(0xc).readU8() | 0x10); return 0; }
            ptr(a[0]).writeByteArray(blk.add(0x900 + 0x40 * k).readByteArray(0x40));
            f.add(0x20).writeU32(k + 1);
            if (k + 1 === n && S.cur.eofLast) f.add(0xc).writeU8(f.add(0xc).readU8() | 0x10);
            return a[0];
          } },
      ],
      build: function (fx) { fx.obj = blk; },
    });
    const rows = RO_ROWS.map(function (o) {
      return Object.assign({}, o, { args: [blk, o.path === null ? 0 : blk.add(0xe00), o.sec === null ? 0 : blk.add(0xe80)] });
    });
    return argRows(fx, rows, function (fx, o, i) {
      blank(blk, 0x1000, 0x40000000 + (i << 16));
      blk.add(4).writeUtf8String('same.ini');
      blk.add(0x154).writeU32(0x154);
      blk.add(0x158).writePointer(o.file ? file : ptr(0));
      blk.add(0x15c).writePointer(blk.add(0x400));
      blk.add(0x400).writeUtf8String(o.stale || 'stale line');
      file.add(0xc).writeU32(0x01);
      file.add(0x20).writeU32(0);
      const lines = o.lines || [];
      blk.add(0x8fc).writeU32(lines.length);
      lines.forEach(function (s, k) { blk.add(0x900 + 0x40 * k).writeByteArray(new Uint8Array(0x40).buffer); blk.add(0x900 + 0x40 * k).writeUtf8String(s); });
      blk.add(0xe00).writeUtf8String(o.path || '');
      blk.add(0xe80).writeUtf8String(o.sec || '');
      return blk;
    });
  };

  // ==================================================================================================== scrollbarCreate
  // blk: the scrollbar (0x1400): +0 a fixture vtable (slot +4 records), the end buttons at +0x60c / +0xcdc with a vtable
  // whose slot +0x120 records, +0x278 = a peer whose slots +0xdc / +0xd8 answer row.dc / row.d8 (or NULL with row.nopeer),
  // +0x574 = row.f574, +0x57c = row.r57c, +0x5c0 = row.s5c0, every other field the row's fill pattern. Stood in:
  // controlInitBase, scrollbarResetThumb, Window::create (answers row.wc), Button::create (answers row.bc[call]).
  // [0x0083ff60] = row.g60, saved and compared.
  const SC_ROWS = [
    { args: [0, 5, 6, 100, 16, 0, 9, 0] }, { args: [0, 5, 6, 0, 16, 0x1234, 9, 0] }, { args: [0, 5, 6, 100, 0, 0x1234, 9, 0] },
    { args: [0, 10, 20, 100, 16, 0x1234, 7, 0], r57c: -1 },
    { args: [0, 11, 21, 16, 100, 0x1234, 8, 1], r57c: -1, g60: 1 },
    { args: [0, 12, 22, 120, 18, 0x1234, 3, 0x10], r57c: 5 },
    { args: [0, 13, 23, 18, 120, 0x1234, 4, 0], r57c: 0, nopeer: 1 },
    { args: [0, 14, 24, 90, 12, 0x1234, 5, 0x100], r57c: -1, nopeer: 1, g60: 7 },
    { args: [0, 15, 25, 90, 12, 0x1234, 6, 2], r57c: -1 },
    { args: [0, 16, 26, 90, 12, 0x1234, 6, 0], r57c: -1, wc: 7 },
    { args: [0, 17, 27, 12, 90, 0x1234, 6, 0], r57c: 3, bc: [9] },
    { args: [0, 18, 28, 12, 90, 0x1234, 6, 1], r57c: -1, bc: [0, 0x11] },
    { args: [0, 19, 29, 20, 20, 0x1234, 2, 0], r57c: 1, dc: -7, d8: 300 },
    { args: [0, -1, -2, -50, 10, 0x1234, 1, 0], r57c: -1, s5c0: -3 },
    { args: [0, 20, 30, 70, 14, 0x1234, 0, 0], r57c: -1, f574: 2, g60: 1 },
    { args: [0, 21, 31, 14, 70, 0x1234, 0, 0], r57c: -1, dc: 1000, d8: 2000 },
    { args: [0, 22, 32, 14, 70, 0x1234, 0, 0], r57c: -1, nopeer: 1, f574: 1 },
    { args: [0, 23, 33, 70, 14, 0x1234, 0, 0], r57c: 9, nopeer: 1, s5c0: 0x7fffffff },
  ];
  F.c3fi_sc = function () {
    const blk = Memory.alloc(0x1400), peer = Memory.alloc(0x20);
    const s4 = immFn(0, 4), b120 = immFn(0, 0), pdc = immFn(0, 0), pd8 = immFn(0, 0);
    const wvt = vtable(0x10, { 4: s4 }), bvt = vtable(0x50, { 0x120: b120 });
    peer.writePointer(vtable(0x40, { 0xdc: pdc, 0xd8: pd8 }));
    const fx = scene({
      stands: [
        { addr: 0x004961d0, abi: 'thiscall', nargs: 0, ret: 0x61d0 },                               // controlInitBase
        { addr: 0x004967d0, abi: 'thiscall', nargs: 1, ret: 0x67d0 },                               // scrollbarResetThumb
        { addr: 0x004806c0, abi: 'thiscall', nargs: 9, ret: function () { return S.cur.wc || 0; } },  // Window::create
        { addr: 0x004887c0, abi: 'thiscall', nargs: 8,                                              // Button::create
          ret: function (s, a, n) { return (S.cur.bc || [])[n] || 0; } },
        { addr: s4, abi: 'thiscall', nargs: 1, ret: 4 },
        { addr: b120, abi: 'thiscall', nargs: 0, ret: 0x120 },
        { addr: pdc, abi: 'thiscall', nargs: 0, ret: function () { return S.cur.dc === undefined ? 40 : S.cur.dc; } },
        { addr: pd8, abi: 'thiscall', nargs: 0, ret: function () { return S.cur.d8 === undefined ? 90 : S.cur.d8; } },
      ],
      build: function (fx) { fx.obj = blk; },
      save: [[0x0083ff60, 4]],
    });
    return argRows(fx, SC_ROWS, function (fx, o, i) {
      blank(blk, 0x1400, 0x5c000000 + (i << 16));
      blk.writePointer(wvt);
      blk.add(0x60c).writePointer(bvt);
      blk.add(0xcdc).writePointer(bvt);
      blk.add(0x278).writePointer(o.nopeer ? ptr(0) : peer);
      blk.add(0x574).writeU32(o.f574 || 0);
      blk.add(0x57c).writeS32(o.r57c === undefined ? -1 : o.r57c);
      blk.add(0x5c0).writeS32(o.s5c0 === undefined ? 16 + i : o.s5c0);
      w32(0x0083ff60, o.g60 || 0);
      return blk;
    });
  };

  // ===================================================================================================== ListBox::build
  // blk (0x1000): `this` = blk + 0x600, so [this - 0x5d0] (blk + 0x30) points at a vbtable at blk + 0xe00 whose dwords
  // 1 / 2 are row.v1 / row.v2 (V = this + v2; row.v2 = 0 puts the V fields on the direct ones). The parent is a fixture
  // block whose +0x9c is row.p9c. Every callee is stood in: ListBox::measure answers row.m, Window::create row.wc; the
  // scroll / range forwarders, setQuad family and setDrawTarget record. V fields from row.vf ({off: value}), the direct
  // fields -0x5ac / -0x5a8 / -0x5a4 from row.d.
  const LB_ROWS = [
    { args: [0, 0, 4, 5, 0] },
    { args: [0, 0x1000, 4, 5, 0], m: 5 },
    { args: [0, 0x1000, 3, 9, 0x0c], m: 7, d: [0, 2, 8], vf: { '-1440': 70, '-1436': 80, '-1280': 10, '-1248': 6 } },
    { args: [0, 0x1000, 1, 7, 0x41], d: [0, 2, 7], vf: { '-1400': 3, '-1396': 2, '-1280': 12, '-1248': 9 } },
    { args: [0, 0x1000, 2, 2, 0], d: [4, 5, 5], vf: { '-1400': 2, '-1396': 3, '-1280': 0, '-1248': 4 } },
    { args: [0, 0x1000, 2, 2, 0], d: [0, 1, 6], vf: { '-1400': 1, '-1396': 1, '-1280': 5 }, wc: 7 },
    { args: [0, 0x2000, 6, 1, 0xff], p9c: 0x1000, d: [1, 1, 0], vf: { '-1400': 2, '-1396': 4, '-1280': 9, '-1248': 3 } },
    { args: [0, 0x1000, 5, 9, 0], v2: 0, vf: { '-1400': 1, '-1396': 1, '-1280': 4, '-1248': 20 } },
    { args: [0, 0x1000, 8, 9, 0], d: [3, 50, 60], vf: { '-1400': 1, '-1396': 1, '-1280': 4, '-1248': 2 } },
    { args: [0, 0x1000, 1, 2, 0x40], d: [0, 1, 4], vf: { '-1400': 1, '-1396': 5, '-1280': 30, '-1248': 4 } },
    { args: [0, 0x1000, -3, -10, 0], d: [-2, -9, -1], vf: { '-1400': 2, '-1396': 3, '-1280': 7, '-1248': 1 } },
    { args: [0, 0x1000, 4, 4, 0x10], v1: -0x80, v2: 0x200, d: [0, 3, 9], vf: { '-1400': 4, '-1396': 2, '-1280': 25, '-1248': 11 } },
    { args: [0, 0x1000, 7, 3, 0x8], m: 7, v2: -0x40, d: [2, 1, 9], vf: { '-1440': 5, '-1436': 6, '-1280': 3, '-1248': 9 } },
  ];
  F.c3fi_lb = function () {
    const blk = Memory.alloc(0x1000), par = Memory.alloc(0x100);
    const st = function (addr, n, ret) { return { addr: addr, abi: 'thiscall', nargs: n, ret: ret }; };
    const fx = scene({
      stands: [
        st(0x0048a120, 0, function () { return S.cur.m || 0; }),                                    // ListBox::measure
        st(0x004806c0, 9, function () { return S.cur.wc || 0; }),                                   // Window::create
        st(0x0047ba30, 2, 0xba30), st(0x0047ba70, 1, 0xba70), st(0x0047ba50, 2, 0xba50), st(0x0047ba90, 1, 0xba90),
        st(0x0047ba10, 1, 0xba10), st(0x0047b9f0, 1, 0xb9f0),
        st(0x00476310, 4, 0x6310), st(0x00476340, 4, 0x6340), st(0x00476370, 4, 0x6370), st(0x004762d0, 4, 0x62d0),
      ],
      build: function (fx) { fx.obj = blk; },
    });
    const rows = LB_ROWS.map(function (o) {
      const a = o.args.slice();
      a[1] = a[1] ? par : 0;
      return Object.assign({}, o, { args: a });
    });
    return argRows(fx, rows, function (fx, o, i) {
      blank(blk, 0x1000, 0x1b000000 + (i << 16));
      const self = blk.add(0x600);
      blk.add(0x30).writePointer(vbtable(blk, 0xe00, [o.v1 === undefined ? 0x10 : o.v1, o.v2 === undefined ? 0x100 : o.v2]));
      par.add(0x9c).writeU32(o.p9c || 0);
      const d = o.d || [0, 0, 0];
      self.add(-0x5ac).writeS32(d[0]); self.add(-0x5a8).writeS32(d[1]); self.add(-0x5a4).writeS32(d[2]);
      const V = self.add(o.v2 === undefined ? 0x100 : o.v2);
      Object.keys(o.vf || {}).forEach(function (k) { V.add(+k).writeS32(o.vf[k]); });
      return self;
    });
  };

  // ================================================================================================== panelBuild4a3880
  // blk (0x1000): `this` = blk + 0x700, B = blk + 0xa8 points at a vbtable at blk + 0xf00 (dwords 1 / 2 = row.v1 / row.v2;
  // V = this + v2). Item list: row.n circular nodes of 0x20 bytes at blk + 0x400 (+4 the item 0xd0000000 + k, +0xc next,
  // +0x10 previous), head this - 0x5d8 (NULL with row.nohead), count this - 0x5d0 = row.cnt (default n), cursor index
  // this - 0x5cc = row.idx, this - 0x5b0 / -0x5ac from row.d. V fields from row.vf ({off: value}; rows -0x600, columns
  // -0x5fc, cell width -0x5ec). Every callee is stood in (Window::create answers row.wc).
  const PB_ROWS = [
    { args: [0, 0, 1, 2, 0] },
    { args: [0, 1, 1, 2, 0], n: 2, wc: 9, vf: { '-1536': 1, '-1532': 1, '-1516': 10 } },
    { args: [0, 1, 3, 4, 0x10], n: 3, idx: 8, d: [30, 4], vf: { '-1536': 2, '-1532': 3, '-1516': 20, '-1552': 2, '-1520': 3 } },
    { args: [0, 1, 5, 6, 0], n: 5, idx: 0, d: [40, 25], vf: { '-1536': 3, '-1532': 2, '-1516': 16, '-1552': 5, '-1520': 1 } },
    { args: [0, 1, 7, 8, 0], n: 0, nohead: 1, idx: 7, d: [12, 3], vf: { '-1536': 2, '-1532': 2, '-1516': 9, '-1552': 1, '-1520': 1 } },
    { args: [0, 1, 9, 1, 0], n: 0, cnt: -1, nohead: 1, idx: -2, d: [5, 5], vf: { '-1536': 4, '-1532': 4, '-1516': 7 } },
    { args: [0, 1, 2, 2, 0], n: 3, d: [8, 2], vf: { '-1536': 0, '-1532': 3, '-1516': 7 } },
    { args: [0, 1, 2, 2, 0], n: 0, nohead: 1, idx: 9, d: [8, 2], vf: { '-1536': 3, '-1532': 0, '-1516': 7, '-1552': 2, '-1520': 2 } },
    { args: [0, 2, 4, 4, 0x40], p9c: 0x1000, n: 4, idx: 1, d: [16, 6], vf: { '-1536': 2, '-1532': 2, '-1516': 11, '-1552': 4, '-1520': 4 } },
    { args: [0, 1, 6, 6, 0], v2: 0, n: 3, idx: 0, d: [9, 1] },
    { args: [0, 1, 1, 1, 0], n: 2, idx: 0, d: [0, 0], vf: { '-1536': 2, '-1532': 1, '-1516': -0x80000000, '-1552': 1, '-1520': 1 } },
    { args: [0, 1, 0, 0, 0], n: 8, idx: 3, d: [24, -5], vf: { '-1536': 4, '-1532': 3, '-1516': 13, '-1552': 6, '-1520': -2 } },
    { args: [0, 1, 8, 9, 0], v1: -0x40, v2: 0x180, n: 4, idx: 0, cnt: 6, d: [33, 7], vf: { '-1536': 2, '-1532': 4, '-1516': 5, '-1552': 2, '-1520': 2 } },
  ];
  F.c3fi_pb = function () {
    const blk = Memory.alloc(0x1000), par = Memory.alloc(0x100);
    const st = function (addr, n, ret) { return { addr: addr, abi: 'thiscall', nargs: n, ret: ret }; };
    const fx = scene({
      stands: [
        st(0x004a3be0, 0, 0x3be0),                                                                  // panelLayoutIconText
        st(0x004806c0, 9, function () { return S.cur.wc || 0; }),                                   // Window::create
        st(0x00492920, 1, 0x2920), st(0x004929b0, 7, 0x29b0),                                       // HotList::alloc / add
        st(0x00476310, 4, 0x6310), st(0x00476340, 4, 0x6340), st(0x00476370, 4, 0x6370), st(0x004762d0, 4, 0x62d0),
      ],
      build: function (fx) { fx.obj = blk; },
    });
    const rows = PB_ROWS.map(function (o) {
      const a = o.args.slice();
      a[1] = a[1] ? par : 0;
      return Object.assign({}, o, { args: a });
    });
    return argRows(fx, rows, function (fx, o, i) {
      blank(blk, 0x1000, 0x9b000000 + (i << 16));
      const self = blk.add(0x700);
      blk.add(0xa8).writePointer(vbtable(blk, 0xf00, [o.v1 === undefined ? 0x20 : o.v1, o.v2 === undefined ? 0x200 : o.v2]));
      par.add(0x9c).writeU32(o.p9c || 0);
      const n = o.n || 0;
      const node = function (k) { return blk.add(0x400 + 0x20 * k); };
      for (let k = 0; k < n; k++) {
        node(k).add(4).writeU32(0xd0000000 + k);
        node(k).add(0xc).writePointer(node((k + 1) % n));
        node(k).add(0x10).writePointer(node((k + n - 1) % n));
      }
      self.add(-0x5d8).writePointer(o.nohead || n === 0 ? ptr(0) : node(0));
      self.add(-0x5d0).writeS32(o.cnt === undefined ? n : o.cnt);
      if (o.idx !== undefined) self.add(-0x5cc).writeS32(o.idx);
      const d = o.d || [0, 0];
      self.add(-0x5b0).writeS32(d[0]); self.add(-0x5ac).writeS32(d[1]);
      const V = self.add(o.v2 === undefined ? 0x200 : o.v2);
      Object.keys(o.vf || {}).forEach(function (k) { V.add(+k).writeS32(o.vf[k]); });
      return self;
    });
  };

  // ================================================================================================ showMainRadialMenu
  // nearestMenuSpot answers row.s, Surface_stretchBlit / Surface_drawScaled / drawPanelBackdrop record (answers by call);
  // a guarded recording display at [0x0083ad50] (slot +0x9c). The globals the body reads or writes are saved and seeded
  // per row: mode [0x00567afc] = row.m, [0x004c2844] = row.z, [0x005a9cbc] = row.cb, bit 4 of [0x0059e7b8] = row.b4
  // (read-modify-write), blink counter [0x005a9f5c] = row.bl, hover counter [0x005a9f68] = row.hc, hover spot
  // [0x005aa6b8] = row.hs ('k' = the spot's image byte 0x004c7994 + s, so the hover matches after the spot branch), the
  // text buffer 0x0051a068.
  const RM_ROWS = [
    { args: [10, 20], s: -1, m: 0, z: 0, hs: 3 },
    { args: [11, 21], s: -1, m: 1, z: 4, cb: 0, hs: -1, hc: 2 },
    { args: [12, 22], s: -1, m: 2, z: 4, cb: 1, b4: 1, bl: 3, hs: -1, hc: 0x7fffffff },
    { args: [13, 23], s: 2, m: 2, z: 1, b4: 1, bl: 0, hs: 2, hc: 4 },
    { args: [14, 24], s: 0, m: 3, z: 2, b4: 0, hs: 'k', hc: 10 },
    { args: [15, 25], s: 1, m: 4, z: 1, hs: 'k', hc: 11 },
    { args: [16, 26], s: 5, m: 5, z: 4, cb: 1, b4: 1, bl: 7, hs: 5, hc: 20 },
    { args: [17, 27], s: 4, m: 0xffffffff, z: 3, b4: 1, bl: 4, hs: 'k', hc: 10 },
    { args: [18, 28], s: 4, m: 1, z: 0, b4: 0, hs: 'k', hc: 10 },
    { args: [19, 29], s: 3, m: 0, z: 1, hs: 'k', hc: 30 },
    { args: [20, 30], s: 6, m: 6, z: 0, hs: 6, hc: 10 },
    { args: [21, 31], s: 7, m: 1, z: 0, hs: 'k', hc: 10 },
    { args: [22, 32], s: 8, m: 8, z: 0, hs: 8, hc: 15 },
    { args: [23, 33], s: 9, m: 2, z: 0, hs: 'k', hc: 10 },
    { args: [24, 34], s: 10, m: 10, z: 0, hs: 10, hc: 10 },
    { args: [25, 35], s: 11, m: 11, z: 0, hs: 11, hc: 10 },
    { args: [26, 36], s: 0, m: 0, z: 4, cb: 1, hs: 0, hc: 0 },
    { args: [27, 37], s: 1, m: 3, z: 4, cb: 1, hs: 9, hc: 0, b4: 1, bl: 0x7fffffff },
    { args: [28, 38], s: 2, m: 5, z: 0, hs: 'k', hc: 10 },
    // verifier, round 34: the k branch with k != s and a hover that differs, so the store at 0x00432f39 writes k
    // (0x004c7994 + 3 = 8, + 7 = 4); before these rows every stored hover equalled s and a body storing s read GREEN
    { args: [29, 39], s: 3, m: 0, z: 0, hs: 3, hc: 10 },
    { args: [30, 40], s: 7, m: 1, z: 0, hs: -1, hc: 10 },
  ];
  F.c3fi_rm = function () {
    const disp = Memory.alloc(0x40);
    const d9c = immFn(0, 0xc);
    disp.writePointer(vtable(0x40, { 0x9c: d9c }));
    const G4 = [0x0083ad50, 0x00567afc, 0x004c2844, 0x005a9cbc, 0x0059e7b8, 0x005a9f5c, 0x005a9f68, 0x005aa6b8];
    const fx = scene({
      stands: [
        { addr: 0x004326a0, abi: 'mscdecl', nargs: 2, ret: function () { return S.cur.s; } },        // nearestMenuSpot
        { addr: 0x00473f60, abi: 'thiscall', nargs: 5, ret: function (s, a, n) { return 0x3f600 + n; } },  // stretchBlit
        { addr: 0x00473cb0, abi: 'thiscall', nargs: 7, ret: function (s, a, n) { return 0xcb000 + n; } },  // drawScaled
        { addr: 0x00432620, abi: 'mscdecl', nargs: 2, ret: 0x2620 },                                 // drawPanelBackdrop
        { addr: d9c, abi: 'thiscall', nargs: 3, ret: 0x9c },
      ],
      build: function (fx) { __recDisplay.guard(disp); },
      save: G4.map(function (a) { return [a, 4]; }).concat([[0x0051a068, 0x40]]),
      init: function () { w32(0x0083ad50, disp.toUInt32()); },
    });
    const flags0 = r32(0x0059e7b8);
    return argRows(fx, RM_ROWS, function (fx, o, i) {
      w32(0x00567afc, o.m);
      w32(0x004c2844, o.z || 0);
      w32(0x005a9cbc, o.cb || 0);
      w32(0x0059e7b8, o.b4 ? (flags0 | 4) : (flags0 & ~4));
      w32(0x005a9f5c, o.bl || 0);
      w32(0x005a9f68, o.hc || 0);
      w32(0x005aa6b8, o.hs === 'k' ? ptr(0x004c7994 + o.s).readS8() : o.hs);
      for (let k = 0; k < 0x40; k += 4) w32(0x0051a068 + k, 0x7e7e7e7e);
      return o.args[0];
    });
  };

  // ================================================================================================= showLoadingScreen
  // The two frame objects are declared by the stood-in constructors (Surface 0x2c bytes, Image 0x2b8: framemask.js
  // tokens, so both arms' calls name the same object), which also fill their first four dwords with known words; every
  // Surface_free call logs the Surface's first dword (deref), so the vtable store 0x004ba2d8 before the second one is
  // compared. Random::range answers row.rng[call], clamp answers its first argument clamped to [second, third]; the other
  // callees record. The text buffer 0x0051a068 is saved, seeded with '~' and compared.
  const LS_ROWS = [
    { args: [-1], rng: [0, 0] }, { args: [0], rng: [11, 10] }, { args: [50], rng: [12, 11] }, { args: [100], rng: [5, 3] },
    { args: [150], rng: [0x10003, 0x20004] }, { args: [-2], rng: [7, 0xffff] }, { args: [1], rng: [1, 1] },
    { args: [2], rng: [2, 2] }, { args: [99], rng: [4, 5] }, { args: [-100], rng: [6, 6] }, { args: [7], rng: [8, 7] },
    { args: [33], rng: [9, 8] }, { args: [66], rng: [10, 9] }, { args: [0x7fffffff], rng: [3, 0x80000000] },
  ];
  F.c3fi_ls = function () {
    const fx = scene({
      stands: [
        { addr: 0x00473ab0, abi: 'thiscall', nargs: 0, region: { arg: 'this', size: 0x2c },           // Surface_ctor
          fill: { arg: 'this', words: 4 }, ret: 'this' },
        { addr: 0x00474ae0, abi: 'thiscall', nargs: 0, region: { arg: 'this', size: 0x2b8 },          // Image_ctor
          fill: { arg: 'this', words: 4 }, ret: 'this' },
        { addr: 0x0045c1e0, abi: 'thiscall', nargs: 1,                                               // Random::range
          ret: function (s, a, n) { const v = (S.cur.rng || [])[n]; return v === undefined ? 0 : v; } },
        { addr: 0x00475840, abi: 'thiscall', nargs: 5, ret: 0x5840 },                                // loadImage
        { addr: 0x00473bf0, abi: 'thiscall', nargs: 7, ret: 0x3bf0 },                                // Surface_blit
        { addr: 0x00473e60, abi: 'thiscall', nargs: 4, ret: 0x3e60 },                                // Image_draw
        { addr: 0x0045b0d0, abi: 'mscdecl', nargs: 4, ret: 0xb0d0 },                                 // drawMessageBuffer
        { addr: 0x00467130, abi: 'mscdecl', nargs: 3,                                                // clamp
          ret: function (s, a) { const v = a[0] | 0, lo = a[1] | 0, hi = a[2] | 0; return v < lo ? lo : (v > hi ? hi : v); } },
        { addr: 0x00480b00, abi: 'thiscall', nargs: 5, ret: 0x0b00 },                                // fillWindowRect
        { addr: 0x00404b70, abi: 'mscdecl', nargs: 4, ret: 0x4b70 },                                 // drawCenteredText
        { addr: 0x00473ae0, abi: 'thiscall', nargs: 0, deref: { this: 1 },                           // Surface_free
          ret: function (s, a, n) { return 0x3ae00 + n; } },
        { addr: 0x00474c40, abi: 'thiscall', nargs: 0, deref: { this: 1 }, ret: 0x4c40 },            // Image_dtor
      ],
      save: [[0x0051a068, 0x40]],
    });
    return argRows(fx, LS_ROWS, function (fx, o, i) {
      for (let k = 0; k < 0x40; k += 4) w32(0x0051a068 + k, 0x7e7e7e7e);
      return o.args[0];
    });
  };
})();
