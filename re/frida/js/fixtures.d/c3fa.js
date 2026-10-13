// C3 batch c3fa (round 33, 2026-10-13): fixtures for shim/src/re/c3fa.cpp (registry re/frida/registry.d/c3fa.py).
//
// Every key: callees not under test are recording stand-ins (re/frida/js/framemask.js FM.stand: A/B thread only,
// each call logged with `this` and its arguments in the compared block $fmlog), the game's first thread is frozen
// (__freeze) and every global the key writes is saved before and restored after it. Per-vector inputs come from getters
// `$rN`: diff_hook resolves a vector's arguments before it snapshots the state, so a getter writes that vector's globals
// and selects that vector's stand-in answers (`S.cur`, the same for both arms), then returns the first argument.
// Fixture text is our own synthetic ASCII.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const w32 = function (a, v) { ptr(a).writeU32(v >>> 0); };
  const w16 = function (a, v) { ptr(a).writeU16(v & 0xffff); };
  const w8 = function (a, v) { ptr(a).writeU8(v & 0xff); };
  const r32 = function (a) { return ptr(a).readU32(); };
  const S = { cur: {} };
  const cur = function (k, d) { const v = S.cur[k]; return v === undefined ? d : v; };

  function cstr(s, size) {
    const p = Memory.alloc(Math.max(size || 0, s.length + 1));
    p.writeUtf8String(s);
    return p;
  }

  // scene({stands, build(fx), save: [[addr, n]...], rows: [fn(fx) -> first argument]})
  function scene(o) {
    const FM = __frameMask.begin({ size: o.fmSize || 0x4000 });
    (o.stands || []).forEach(function (sp) { FM.stand(sp); });
    const fx = Object.assign({ FM: FM }, FM.fx());
    if (o.build) o.build(fx);
    __freeze.thread();
    (o.save || []).forEach(function (s) { __freeze.save(ptr(s[0]), s[1]); });
    if (o.init) o.init(fx);
    __freeze.negOn();
    (o.rows || []).forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, {
        enumerable: true, configurable: true,
        get: function () { S.cur = {}; const v = row(fx); return v === undefined ? 0 : v; },
      });
    });
    return fx;
  }

  // Native method stubs for fixture vtables: `mov eax, imm32; ret N` (or `mov eax, [cell]; ret N`), padded to 16 bytes so
  // Interceptor can patch them; FM.stand on one turns it into a recording stand-in.
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
    const b = bytes.concat(new Array(16 - bytes.length).fill(0x90));
    p.writeByteArray(b);
    return p;
  }
  const retN = function (pop) { return pop ? [0xc2, pop & 0xff, (pop >>> 8) & 0xff] : [0xc3]; };
  function immFn(imm, pop) { return code([0xb8].concat(le(imm), retN(pop))); }
  function cellFn(cell, pop) { return code([0xa1].concat(le(cell.toUInt32()), retN(pop))); }
  // vtable with `slots` entries; every slot a `xor eax, eax; ret` stub unless given in `fill` ({off: NativePointer})
  function vtable(slots, fill) {
    const vt = Memory.alloc(4 * slots);
    const dflt = code([0x31, 0xc0, 0xc3]);
    for (let k = 0; k < slots; k++) vt.add(4 * k).writePointer(dflt);
    Object.keys(fill || {}).forEach(function (off) { vt.add(+off).writePointer(fill[off]); });
    return vt;
  }

  // Row tables with several arguments: fx['r' + i] is a getter (selects the row, returns argument 0), fx['a' + i + '_' + k]
  // a plain property per further argument.
  function argRows(fx, list, setup) {
    list.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, {
        enumerable: true, configurable: true,
        get: function () { S.cur = row; setup(fx, row, i); return row.args[0]; },
      });
      for (let k = 1; k < row.args.length; k++) fx['a' + i + '_' + k] = row.args[k];
    });
    return fx;
  }

  // ------------------------------------------------------------------------------------------------- engineInit
  // The display loadGraphsyObject returns is a fixture object whose slot +4 records; __recDisplay.guard sends every other
  // thread's call through it to the real display while engineInit has stored it at 0x0083ad50.
  const EI_FAIL = function (addr) {
    return function (s, a, n) {
      const f = (S.cur.fail || {})[addr];
      if (f === undefined) return 0;
      return Array.isArray(f) ? (f[n] === undefined ? 0 : f[n]) : f;
    };
  };
  const EI_ROWS = [
    { flags: 0xffff, disp: 0 }, { flags: 0 }, { flags: 0x8000, d4: 5 }, { flags: 0xffff }, { flags: 0x7fffffff },
    { flags: 0xffff, fail: { 0x47cfd0: 3 } }, { flags: 0x4, fail: { 0x4b04b0: [6] } }, { flags: 0x10, fail: { 0x4b04b0: [7] } },
    { flags: 0x14, fail: { 0x4b04b0: [0, 8] } }, { flags: 0xffff, fail: { 0x49fe50: 9 } }, { flags: 0xfff0, fail: { 0x49e9d0: 10 } },
    { flags: 0x80, fail: { 0x483ac0: 11 } }, { flags: 0x82 }, { flags: 0x100, fail: { 0x490bf0: 12 } },
    { flags: 0xfe00, fail: { 0x49d3b0: 13 } }, { flags: 0x400, fail: { 0x49d1b0: 14 } }, { flags: 0x4000, fail: { 0x491c10: 15 } },
    { flags: 0x0800, fail: { 0x4884e0: 16 } }, { flags: 0x2000, fail: { 0x49d090: 17 } }, { flags: 0x3000 },
    { flags: 0x1000, fail: { 0x486fe0: 18 } }, { flags: 0x8001, d4: -1 }, { flags: 0x9002, fail: { 0x483320: 99 } },
  ];
  F.c3fa_ei = function () {
    const disp = Memory.alloc(0x40);
    const slot4 = immFn(0, 0xc);
    disp.writePointer(vtable(0x100, { 4: slot4 }));
    const stands = [
      { addr: 0x004a00a0, abi: 'mscdecl', nargs: 1, ret: function () { return S.cur.disp === 0 ? 0 : disp.toUInt32(); } },
      { addr: slot4, abi: 'thiscall', nargs: 3, ret: function () { return S.cur.d4 || 0; } },
      { addr: 0x00485740, abi: 'mscdecl', nargs: 0, ret: 0x5740 },
    ];
    // jpeg_mem_init 0x004b04b0 is `xor eax, eax; ret` (3 bytes: Interceptor cannot patch it) and runs real: its two steps
    // (flags 4 / 0x10) always fall through, so rows 6..8 only show that.
    [[0x0047cfd0, 2], [0x00483320, 1], [0x0049fe50, 0], [0x0049e9d0, 0], [0x00483ac0, 1],
     [0x004889f0, 3], [0x00490bf0, 0], [0x0049d3b0, 0], [0x0049d1b0, 0], [0x00491c10, 0], [0x004884e0, 0],
     [0x00486fe0, 0]].forEach(function (c) { stands.push({ addr: c[0], abi: 'mscdecl', nargs: c[1], ret: EI_FAIL(c[0]) }); });
    stands.push({ addr: 0x00483030, abi: 'thiscall', nargs: 0, ret: 0x3030 });
    stands.push({ addr: 0x00483060, abi: 'thiscall', nargs: 0, ret: 0x3060 });
    stands.push({ addr: 0x0049d090, abi: 'thiscall', nargs: 1, ret: EI_FAIL(0x49d090) });
    const fx = scene({
      stands: stands,
      build: function () { __recDisplay.guard(disp); },
      save: [[0x0083afd0, 4], [0x0083ad50, 4], [0x0083ad0c, 4], [0x0083ab68, 4], [0x0083ad44, 4]],
    });
    return argRows(fx, EI_ROWS.map(function (o, i) { return Object.assign({ args: [0x4d000 + i, 0x7e00 + i, o.flags] }, o); }),
      function (fx, o, i) {
        w32(0x0083afd0, 0xeeee); w32(0x0083ad0c, 0x1d0c + i); w32(0x0083ab68, 0x1b68); w32(0x0083ad44, 0xad440000 + i);
      });
  };

  // ------------------------------------------------------------------------------------------- buildLabeledControl
  F.c3fa_blc = function () {
    const fx = scene({
      stands: [{ addr: 0x004804a0, abi: 'thiscall', nargs: 0, ret: 'this' },          // View4804a0::ctor
               { addr: 0x004a13f0, abi: 'thiscall', nargs: 0, ret: 'this' }],         // ctorObj4a13f0
      build: function (fx) { fx.obj = Memory.alloc(0x1400); },
      save: [[0x0083ff1c, 0x44], [0x004e4764, 0x2c]],
      rows: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map(function (i) {
        return function (fx) {
          for (let k = 0; k < 0x1400; k += 4) fx.obj.add(k).writeU32(0xc0000000 + (i << 16) + k);
          for (let k = 0; k < 0x44; k += 4) w32(0x0083ff1c + k, (0x83000000 + 0x1111 * i + k) ^ (i & 1 ? 0xffffffff : 0));
          for (let k = 0; k < 0x2c; k += 4) w32(0x004e4764 + k, 0x4e000000 + 0x2222 * i + k);
          return fx.obj;
        };
      }),
    });
    return fx;
  };

  // --------------------------------------------------------------------------------------- Window::createBackingSurface
  // Slot +0x54 (&w, &h, &keep) may rewrite the three (row.sw); adjustSizeOuter writes (row.ao) into its two locals;
  // innerSize subtracts row.is from both; Surface_createFull's result depends on [0x00839ab0] at the call.
  const CBS_ROWS = [
    { args: [0, 5, 7, 1, 0x30] }, { args: [0, -4, 0, 1, 0x31] }, { args: [0, 1, 2, 1, 0x32], cf: 0x77 },
    { args: [0, 10, 20, 0, 0x33], ao: [4, 30] }, { args: [0, 10, 20, 0, 0x34], ao: [40, 3], is: 2 },
    { args: [0, 10, 20, 0, 0x35], ao: [10, 20], is: 0, cf: 0x55 }, { args: [0, 3, 3, 0, 0x36], sw: [50, 60, 1] },
    { args: [0, 3, 3, 1, 0x37], sw: [0, -9, 0], ao: [2, 2], is: 1 }, { args: [0, 3, 3, 1, 0x38], hi: 1 },
    { args: [0, 100, 200, 0, 0x39], ao: [1, 1], hi: 1, cf: 0x99 }, { args: [0, 0x7fffffff, -0x80000000, 0, 0x3a], ao: [5, 5] },
    { args: [0, 8, 9, 7, 0x3b], sw: [8, 9, 0], ao: [9, 8], is: 3 },
  ];
  F.c3fa_cbs = function () {
    const obj = Memory.alloc(0x400);
    const s54 = immFn(0, 0xc);
    obj.writePointer(vtable(0x40, { 0x54: s54 }));
    const fx = scene({
      stands: [
        { addr: s54, abi: 'thiscall', nargs: 3, ret: 0x54,
          write: function (s, a) { const v = S.cur.sw; if (v) for (let k = 0; k < 3; k++) ptr(a[k]).writeS32(v[k]); } },
        { addr: 0x0047ca10, abi: 'thiscall', nargs: 2, ret: 0xca10,                   // Window::adjustSizeOuter
          write: function (s, a) { const v = S.cur.ao; if (v) { ptr(a[0]).writeS32(v[0]); ptr(a[1]).writeS32(v[1]); } } },
        { addr: 0x0047cb10, abi: 'thiscall', nargs: 2, ret: 0xcb10,                   // Window::innerSize
          write: function (s, a) { const d = S.cur.is || 0; for (let k = 0; k < 2; k++) ptr(a[k]).writeS32(ptr(a[k]).readS32() - d); } },
        { addr: 0x00474dd0, abi: 'thiscall', nargs: 6,                                // Surface_createFull
          ret: function () { return S.cur.cf ? (S.cur.cf ^ (r32(0x00839ab0) << 8)) : 0; } },
        { addr: 0x0047b4e0, abi: 'thiscall', nargs: 4, ret: function (s, a) { return (0xb4e0 + a[0] - a[1]) >>> 0; } },
      ],
      build: function (fx) { fx.obj = obj; },
      save: [[0x00839ab0, 4]],
    });
    return argRows(fx, CBS_ROWS.map(function (o) { return Object.assign({}, o, { args: [obj].concat(o.args.slice(1)) }); }),
      function (fx, o) { w32(0x00839ab0, 0x0bad); obj.add(0x9c).writeU32(o.hi ? 0x80000001 : 0x7fffffff); });
  };

  // ------------------------------------------------------------------------------------------------- saveCourseJpg
  // surf +4 -> inner object whose slots +0x10 / +0xd8 / +0xdc / +0xe0 / +0xe4 return cells (pixels, width, height,
  // pitch, format record pointer).
  const SCJ_ROWS = [
    { args: [1, 1, 0, 0, 0x320, 0x258, 2] }, { args: [0, 1, 0, 0, 10, 10, 1] }, { args: [1, 0, 0, 0, 10, 10, 1] },
    { args: [1, 1, 0, 0, 0, 10, 1] }, { args: [1, 1, 0, 0, 10, 0, 1] }, { args: [1, 1, 0, 0, 10, 10, 0] },
    { args: [1, 1, -1, 0, 10, 10, 1] }, { args: [1, 1, 0, -1, 10, 10, 1] }, { args: [1, 1, 0, 0, -10, 10, 1] },
    { args: [1, 1, 0, 0, 10, -10, 1] }, { args: [1, 1, 0, 0, 10, 10, -1] }, { args: [1, 1, 0, 0, 10, 10, 21] },
    { args: [1, 1, 0, 0, 10, 10, 20] }, { args: [1, 1, 0, 0, 10, 10, 1], fmt: [0x0f, 0] }, { args: [1, 1, 0, 0, 10, 10, 1], fmt: [0x10, 1] },
    { args: [1, 1, 640, 0, 10, 10, 1] }, { args: [1, 1, 0, 480, 10, 10, 1] }, { args: [1, 1, 600, 470, 100, 100, 3] },
    { args: [1, 1, 5, 7, 0x320, 0x258, 10] }, { args: [1, 1, 5, 7, 30, 20, 1], file: 0 }, { args: [1, 1, 5, 7, 30, 20, 2], buf: 0 },
    { args: [1, 1, 639, 479, 1, 1, 1] }, { args: [1, 1, 3, 4, 7, 11, 4], close: -1 },
  ];
  F.c3fa_scj = function () {
    const cells = Memory.alloc(0x40), fmt = Memory.alloc(0x10), inner = Memory.alloc(0x20), surf = Memory.alloc(0x20);
    const C = function (k) { return cells.add(4 * k); };
    inner.writePointer(vtable(0x40, { 0x10: cellFn(C(0), 0), 0xd8: cellFn(C(1), 0), 0xdc: cellFn(C(2), 0),
                                      0xe0: cellFn(C(3), 0), 0xe4: cellFn(C(4), 0) }));
    surf.add(4).writePointer(inner);
    const path = cstr('course path.jpg', 0x40);
    const fx = scene({
      stands: [
        { addr: 0x004a614d, abi: 'mscdecl', nargs: 2, str: { 0: 4 }, ret: function () { return S.cur.file === 0 ? 0 : 0xf11e0000; } },
        { addr: 0x004a504f, abi: 'mscdecl', nargs: 1, ret: function () { return S.cur.buf === 0 ? 0 : 0xb0f0000; } },
        { addr: 0x004a5007, abi: 'mscdecl', nargs: 1, ret: 0 },
        { addr: 0x004ae020, abi: 'mscdecl', nargs: 4, ret: 0xe020 },
        { addr: 0x004a609f, abi: 'mscdecl', nargs: 1, ret: function () { return S.cur.close === undefined ? 0 : S.cur.close; } },
      ],
      build: function (fx) { fx.surf = surf; },
      save: [[0x0056125c, 4], [0x004c3074, 4], [0x00543cf0, 4], [0x0056a510, 4]],
    });
    return argRows(fx, SCJ_ROWS.map(function (o) {
      const a = o.args.slice();
      a[0] = a[0] ? path : ptr(0);
      a[1] = a[1] ? surf : ptr(0);
      return Object.assign({}, o, { args: a });
    }), function (fx, o, i) {
      C(0).writeU32(0x08000000 + 0x100 * i); C(1).writeU32(640); C(2).writeU32(480); C(3).writeU32(0x280 + i);
      const f = o.fmt || [0x10, 0];
      fmt.writeU32(f[0]); fmt.add(4).writeU32(f[1]); C(4).writePointer(fmt);
      w32(0x0056125c, 0x125c); w32(0x004c3074, 0x3074); w32(0x00543cf0, 0x3cf0); w32(0x0056a510, 0xa510);
    });
  };

  // --------------------------------------------------------------------------------------------- movieAdvanceFrame
  // mmioDescend writes the row's next chunk {id, size} into its MMCKINFO argument and returns its code; mmioAscend and
  // mmioRead answer from the row; the sink at this+4 hands out buffers (slot +0x8c) and takes them back (slot +0x90).
  const ID_END = 0x444e4523, ID_DATA = 0x41544144, LIST = 0x5453494c, OTHER = 0x5248544f;
  const MAF_ROWS = [
    { h: 0 }, { cur: 1, asc0: 3 }, { cur: 1, desc: [[ID_END, 0, 0]] }, { desc: [[ID_END, 0, 0]] },
    { desc: [[0, 0, 4]] }, { desc: [[OTHER, 8, 0], [ID_END, 0, 0]] }, { desc: [[OTHER, 8, 0]], ascFail: 1 },
    { desc: [[LIST, 0x40, 0], [ID_DATA, 0x10, 0], [ID_END, 0, 0]], bufs: [1] },
    { desc: [[ID_DATA, 0x20, 0], [ID_END, 0, 0]], bufs: [0] },
    { desc: [[ID_DATA, 0x20, 0], [ID_END, 0, 0]], bufs: [1], short: 1 },
    { desc: [[ID_DATA, 0x08, 0], [ID_DATA, 0x0c, 0], [OTHER, 4, 0], [ID_END, 0, 0]], bufs: [1, 2] },
    { desc: [[LIST, 0x40, 0], [LIST, 0x30, 0], [ID_END, 0, 0]] }, { desc: [[ID_DATA, 4, 0]], bufs: [1], ascFail: 1 },
    { cur: 1, desc: [[OTHER, 8, 0], [ID_DATA, 0x18, 0], [ID_END, 0, 0]], bufs: [2] },
  ];
  F.c3fa_maf = function () {
    const obj = Memory.alloc(0x960), sink = obj.add(4);
    const s8c = immFn(0, 4), s90 = immFn(0, 4);
    sink.writePointer(vtable(0x40, { 0x8c: s8c, 0x90: s90 }));
    const bufs = [ptr(0), Memory.alloc(0x40), Memory.alloc(0x40)];
    const imp = __importStandIn({ module: 'golf_clean.exe', imports: {
      'WINMM.dll!timeGetTime': { args: 0, ret: [1000, 1250, 1500, 1750, 2000, 2250] },
      'WINMM.dll!mmioDescend': { args: 4, impl: function (a, ctx) {
        const d = (S.cur.desc || [])[ctx.call];
        if (!d) return 0x105;
        a[1].writeU32(d[0]); a[1].add(4).writeU32(d[1]);
        return d[2];
      } },
      'WINMM.dll!mmioAscend': { args: 3, impl: function (a, ctx) {
        if (ctx.call === 0 && S.cur.asc0) return S.cur.asc0;
        return S.cur.ascFail ? 0x106 : 0;
      } },
      'WINMM.dll!mmioRead': { args: 3, impl: function (a) { return S.cur.short ? a[2].toInt32() - 1 : a[2].toInt32(); } },
    } });
    const fx = scene({
      stands: [
        { addr: 0x004a0320, abi: 'mscdecl', nargs: 5, ret: 0x320 },                       // logLine
        { addr: s8c, abi: 'thiscall', nargs: 1,
          ret: function (s, a, n) { const b = (S.cur.bufs || [])[n]; return b ? bufs[b].toUInt32() : 0; } },
        { addr: s90, abi: 'thiscall', nargs: 1, ret: 0x90 },
      ],
      build: function (fx) { fx.obj = obj; fx.implog = imp.log; fx.bufs = bufs[1]; fx.bufs2 = bufs[2]; },
    });
    return argRows(fx, MAF_ROWS.map(function (o) { return Object.assign({ args: [obj] }, o); }), function (fx, o, i) {
      obj.add(0xbc).writeU32(o.h === 0 ? 0 : 0x4d4d0001 + i);
      obj.add(0xd4).writeU32(o.cur ? OTHER : 0); obj.add(0xd8).writeU32(0x99);
      obj.add(0x1a4).writeU32(ID_END); obj.add(0x1a8).writeU32(ID_DATA); obj.add(0x940).writeU32(10 * i);
      [1, 2].forEach(function (k) {
        bufs[k].add(0x18).writeU32(0x18180000 + 0x100 * k + i); bufs[k].add(0x20).writeU32(0x2020);
      });
    });
  };

  // ------------------------------------------------------------------------------------------ deserializeCourse
  F.c3fa_dsc = function () {
    return scene({
      stands: [{ addr: 0x0040af70, abi: 'mscdecl', nargs: 2,                          // saveFieldIo
                 ret: function (s, a, n) { return (cur('k', 0) * 0x1000 + n + (a[1] << 16)) >>> 0; } }],
      save: [[0x0053e638, 4]],
      rows: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map(function (i) {
        return function () {
          w32(0x0053e638, 0x3e638000 + i);
          S.cur = { k: i };
          return [0, 1, 2, -1, 0x7fffffff, 3, 0x100, -0x80000000, 7, 0x55][i];
        };
      }),
    });
  };

  // --------------------------------------------------------------------------------------------- resetAllPanels
  F.c3fa_rap = function () {
    return scene({
      stands: [
        { addr: 0x004838b0, abi: 'thiscall', nargs: 0, ret: 0x38b0 },                 // Surface::releaseBacking
        { addr: 0x00473ae0, abi: 'thiscall', nargs: 0,                                // Surface_free
          ret: function (s, a, n) { return (cur('k', 0) * 0x10000 + n) >>> 0; } },
      ],
      rows: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map(function (i) { return function () { S.cur = { k: i + 1 }; return 0; }; }),
    });
  };

  // ---------------------------------------------------------------------------------------------- hireEmployee
  // Walker records 0x00585850 + 0x4c g for g in HIRE_G; the direction tables 0x004c2878 / 0x004c2898 are the game's.
  const HIRE_G = [1, 4, 9];
  const HIRE_ROWS = [
    { k: -1 }, { k: 12 }, { k: 5, b: 0 }, { k: 2, b: -4 }, { k: 4, b: 0, aff: 0 },
    { k: 4, b: 3, g: 1, cb8: 1, cb9: 6, ang: 0x30000000 }, { k: 5, b: 2, g: 4, rng: [1, 0], ang: 0xf0000000 },
    { k: 0, b: 1, g: 9, rng: [0x10000, 3], ang: 0x80000000 }, { k: 11, b: 5, g: 1, cb8: 1, cb9: 0xff, ang: 0x10000000 },
    { k: 8, b: 1, g: 4, rng: [2, 2], ang: 0x6fffffff, date: -0x2c01 }, { k: 3, b: 0, g: 9, cb8: 1, cb9: 2, ang: 0xa0000000 },
    { k: 7, b: 9, aff: 0 }, { k: 1, g: 1, cb8: 1, cb9: 3, date: -1, ang: 0xc0000000 },
    { k: 6, g: 4, rng: [0, 0], ang: 0xe0000000, date: 0x7ffffc00 }, { k: 10, g: 9, cb8: 1, cb9: 5, ang: 0x50000000 },
  ];
  F.c3fa_hire = function () {
    return scene({
      stands: [
        { addr: 0x00459400, abi: 'mscdecl', nargs: 0, ret: function () { return cur('k', 0); } },    // showHireEmployeeDialog
        { addr: 0x00483d30, abi: 'mscdecl', nargs: 0, ret: 0x3d30 },                                // enterModal483d30
        { addr: 0x0044faf0, abi: 'mscdecl', nargs: 1, ret: function () { return cur('b', 1); } },   // bucketValue
        { addr: 0x004481b0, abi: 'mscdecl', nargs: 5, ret: 0x81b0 },                                // playSound
        { addr: 0x0040cb00, abi: 'mscdecl', nargs: 3, ret: 0xcb00 },                                // startMessage
        { addr: 0x00406c30, abi: 'mscdecl', nargs: 1, ret: function () { return cur('aff', 1); } }, // canAfford
        { addr: 0x00402970, abi: 'mscdecl', nargs: 1, ret: function () { return cur('g', 1); } },   // spawnWalker
        { addr: 0x0045c1e0, abi: 'thiscall', nargs: 1,                                               // Random::range
          ret: function (s, a, n) { const r = cur('rng', [1, 1]); return r[n] === undefined ? 1 : r[n]; } },
        { addr: 0x004672d0, abi: 'mscdecl', nargs: 2, ret: function () { return cur('ang', 0); } }, // angleFixed
      ],
      save: [[0x005685f0, 4], [0x00575cb8, 2], [0x00575cc0, 1], [0x00575cc4, 1], [0x00578150, 1], [0x00578154, 1],
             [0x00834170, 4], [0x0051a068, 0x40]].concat(HIRE_G.map(function (g) { return [0x00585850 + 0x4c * g, 0x4c]; })),
      rows: HIRE_ROWS.map(function (o, i) {
        return function () {
          w32(0x005685f0, 3 + i); w8(0x00575cb8, o.cb8 || 0); w8(0x00575cb9, o.cb9 || 0);
          w8(0x00575cc0, 10 + i); w8(0x00575cc4, 40 - i); w8(0x00578150, 20 + i); w8(0x00578154, 30 + 2 * i);
          w32(0x00834170, o.date === undefined ? 0x2c00 + 0x123 * i : o.date);
          ptr(0x0051a068).writeUtf8String('prior ' + i);
          HIRE_G.forEach(function (g, j) {
            const r = 0x00585850 + 0x4c * g;
            // byte +0x12 (the flag byte that gets bit 3) is 0x01: neither bit 2 nor bit 3 set
            for (let q = 0; q < 0x4c; q += 4) w32(r + q, 0x01010000 + 0x100 * j + q + i);
            w32(r, 0x5000 + 0x777 * j + 0x31 * i); w32(r + 4, 0x9000 - 0x555 * j + 0x17 * i);
          });
          S.cur = o;
          return 0;
        };
      }),
    });
  };

  // ---------------------------------------------------------------------------------------- Flic::buildFrameTable
  // this +0x54 -> loader o (+0 vtable whose slot 0 records the delete, +4 header h, +8 file handle); operator new answers
  // `dst` (or NULL) and GetFileSize answers the row's size.
  const FBT_ROWS = [
    { args: [0, 0], noLoader: 1 }, { args: [0, 0], magic: 0xaf11 }, { args: [0, 4], depth: 16 },
    { args: [0, 0], size: 0x80 }, { args: [0, 4], size: 0xa0, nomem: 1 }, { args: [0, 4], conv: 1, size: 0x90 },
    { args: [0, 5], conv: 1, size: 0x70, nomem: 1 }, { args: [0, 0], conv: 1 }, { args: [0, 0xfffffffb], conv: 1 },
    { args: [0, 0xffffffff], size: 0x100 }, { args: [0, 0], size: 0x6c }, { args: [0, 4], conv: 1, size: 0x100 },
  ];
  F.c3fa_fbt = function () {
    const obj = Memory.alloc(0x80), lo = Memory.alloc(0x20), hdr = Memory.alloc(0x100), dst = Memory.alloc(0x100);
    const del = immFn(0, 4);
    lo.writePointer(vtable(0x10, { 0: del }));
    const imp = __importStandIn({ module: 'golf_clean.exe', imports: {
      'KERNEL32.dll!GetFileSize': { args: 2, impl: function () { return cur('size', 0x60); } },
    } });
    const fx = scene({
      stands: [
        { addr: 0x004a57f1, abi: 'mscdecl', nargs: 1, ret: function () { return cur('nomem', 0) ? 0 : dst.toUInt32(); } },
        { addr: 0x00481ba0, abi: 'thiscall', nargs: 0, ret: 0x1ba0 },                 // Flic::reset
        { addr: del, abi: 'thiscall', nargs: 1, ret: 0xde1 },                          // the loader's slot 0
      ],
      build: function (fx) { fx.obj = obj; fx.dst = dst; fx.hdr = hdr; fx.implog = imp.log; },
    });
    return argRows(fx, FBT_ROWS.map(function (o) { return Object.assign({}, o, { args: [obj, o.args[1]] }); }),
      function (fx, o, i) {
        for (let q = 0; q < 0x80; q += 4) obj.add(q).writeU32(0x0b000000 + q + i);
        obj.add(0x54).writePointer(o.noLoader ? ptr(0) : lo);
        lo.add(4).writePointer(hdr); lo.add(8).writeU32(0x4a4d0000 + i);
        for (let q = 0; q < 0x100; q += 4) { hdr.add(q).writeU32(0x4d000000 + 0x10 * i + q); dst.add(q).writeU32(0xd5d5d5d5); }
        hdr.add(4).writeU16(o.magic || 0xaf12); hdr.add(0xc).writeU16(o.depth || 8);
        hdr.add(6).writeU16(0x1606 + i); hdr.add(8).writeU16(0x0280 + i); hdr.add(0xa).writeU16(0x01e0 - i);
        hdr.add(0x1a).writeU32(o.conv ? 0xf1f1f2f2 : 0x12345678);
      });
  };

  // -------------------------------------------------------------------------------------------- saveGameDialog
  // textInputBox writes the row's answer into the text buffer; appendCourseTitle / appendDate append markers.
  const SGD_ROWS = [
    { typed: '' }, { typed: 'abc', san: 0 }, { typed: 'my course', san: 1, ex: 0 }, { typed: 'x', san: 1, ex: 1, pop: 0 },
    { typed: 'exists', san: 1, ex: 1, pop: 1 }, { typed: 'exists2', san: 2, ex: 7, pop: -3 },
    { typed: 'neg date', san: 1, ex: 0, date: -0x2c37 }, { typed: 'big', san: 1, ex: 0, date: 0x7fffffff },
    { typed: 'a long synthetic file name of forty chr', san: 1, ex: 1, pop: 0 }, { typed: '', date: 0x3ff },
    { typed: 'z', san: 1, ex: 0, date: 0x200 }, { typed: 'q', san: 0, date: -1 },
  ];
  F.c3fa_sgd = function () {
    const app = function (s) { const p = ptr(0x0051a068); p.writeUtf8String(p.readUtf8String() + s); };
    return scene({
      stands: [
        { addr: 0x0040cc00, abi: 'mscdecl', nargs: 5, ret: 0xcc00 },                                // drawPanelS2
        { addr: 0x004762d0, abi: 'thiscall', nargs: 4, ret: 0x62d0 },                               // setDrawTarget
        { addr: 0x00404bc0, abi: 'mscdecl', nargs: 4, ret: 0x4bc0 },                                // drawCenteredText2
        { addr: 0x0040daa0, abi: 'mscdecl', nargs: 1, ret: 0xdaa0, write: function () { app('[T]'); } },  // appendCourseTitle
        { addr: 0x0040d7b0, abi: 'mscdecl', nargs: 1, ret: 0xd7b0, write: function (s, a) { app('[D' + a[0] + ']'); } },
        { addr: 0x0045b2c0, abi: 'mscdecl', nargs: 4, str: { 0: 2 }, ret: 0xb2c0,                   // textInputBox
          write: function () { ptr(0x0051a068).writeUtf8String(cur('typed', '')); } },
        { addr: 0x00405ac0, abi: 'mscdecl', nargs: 1, str: { 0: 4 }, ret: function () { return cur('san', 1); } },
        { addr: 0x004378a0, abi: 'mscdecl', nargs: 1, str: { 0: 16 }, ret: function () { return cur('ex', 0); } },
        { addr: 0x0046d6e0, abi: 'mscdecl', nargs: 5, str: { 0: 0 }, ret: function () { return cur('pop', 0); } },
        { addr: 0x0040b4a0, abi: 'mscdecl', nargs: 1, str: { 0: 12 }, ret: 0xb4a0 },               // saveGame
        { addr: 0x0040cb00, abi: 'mscdecl', nargs: 3, ret: 0xcb00 },                                // startMessage
      ],
      save: [[0x0051a068, 0x100], [0x0058a528, 0x20], [0x00834170, 4]],
      rows: SGD_ROWS.map(function (o, i) {
        return function () {
          ptr(0x0051a068).writeUtf8String('stale ' + i);
          ptr(0x0058a528).writeByteArray(new Array(0x20).fill(0x5a));
          w32(0x00834170, o.date === undefined ? 0x2c00 + 0x51 * i : o.date);
          S.cur = o;
          return 0;
        };
      }),
    });
  };

  // ------------------------------------------------------------------------------------------- showCharacterIntro
  // Table 0x004c15a0..0x004c19c0 (22 entries, stride 0x30): row i makes the entries in mask i non-zero.
  const SCI_MASKS = [0, 1, 0x200000, 0x3fffff, 0x155555, 0x2aaaaa, 0x00ff00, 0x300003, 0x0f0f0f, 0x100001, 0x7, 0x3ff800];
  const SCI_RET = [0, 1, 0x12345678, -1, 0x8000, 0x7fff, 0xffff0001, 5, 0x10000, 2, 0xabcd, 3];
  F.c3fa_sci = function () {
    return scene({
      stands: [
        { addr: 0x0040d320, abi: 'mscdecl', nargs: 4, ret: 0x40d3 },                               // drawWindow
        { addr: 0x0045f0f0, abi: 'mscdecl', nargs: 5, ret: function () { return cur('ret', 0); } },  // skillPointDialog
      ],
      save: [[0x0051a068, 0x200], [0x004c2e08, 4], [0x004c2c9c, 2], [0x004c15a0, 0x420]],
      rows: SCI_MASKS.map(function (m, i) {
        return function () {
          for (let k = 0; k < 22; k++) w32(0x004c15a0 + 0x30 * k, (m >>> k) & 1 ? (0x101 * (k + 1)) : 0);
          w32(0x004c2e08, 0x55aa0000 + i);
          w16(0x004c2c9c, 0x1111 * (i + 1));
          S.cur = { ret: SCI_RET[i] };
          return 0;
        };
      }),
    });
  };

  // ------------------------------------------------------------------------------------------ showPreferencesMenu
  const SPM_WORDS = ['alpha', '', 'b', 'gamma delta', 'x1', 'longer synthetic option line 42', 'q', 'zz', 'r'];
  F.c3fa_spm = function () {
    const strs = SPM_WORDS.map(function (s) { return cstr(s, 0x40); });
    return scene({
      stands: [{ addr: 0x0046d6e0, abi: 'mscdecl', nargs: 5, ret: function (s, a) { return cur('ret', 0) ^ a[2]; } }],
      save: [[0x0051a068, 0x400], [0x005a5a00, 4], [0x004c2ca8, 0x18]],
      rows: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map(function (i) {
        return function () {
          for (let k = 0; k < 6; k++) ptr(0x004c2ca8 + 4 * k).writePointer(strs[(i * 5 + k * (i + 1)) % strs.length]);
          w32(0x005a5a00, [0, 0x20, 0xffffffff, 0x12345, 0x80000000, 7, 0x20 | 0x400, 1, 0x7fffffff, 0x5a5a][i]);
          S.cur = { ret: [1, 0, 0x40, 0x10001, 3, 0x80000000, 2, 0xffffffff, 9, 0x77][i] };
          return 0;
        };
      }),
    });
  };

  // --------------------------------------------------------------------------------------------- saveThumbAndFull
  const STF_PATHS = ['course.sve', 'noext', 'a.b.c', '.lead', '', 'dir\\name.x', 'trail.', 'x.y', 'long name.with.dots.z',
                     'k'];
  F.c3fa_stf = function () {
    let paths = null;
    return scene({
      stands: [{ addr: 0x00431d20, abi: 'mscdecl', nargs: 7, str: { 0: 10 },                     // saveCourseJpg
                 ret: function (s, a, n) { return 0x5c00 + 0x10 * n + cur('k', 0); } }],
      build: function (fx) {
        paths = Memory.alloc(0x100 * STF_PATHS.length);
        fx.paths = paths;
      },
      rows: STF_PATHS.map(function (p, i) {
        return function () {
          const b = paths.add(0x100 * i);
          b.writeByteArray(new Array(0x100).fill(0));
          b.writeUtf8String(p);
          S.cur = { k: i };
          return b;
        };
      }),
    });
  };

  // -------------------------------------------------------------------------------------------- clickEmployeeView
  // Golfer records 0x005794b8 + e * 0x100 (+0 x, +4 y, +8 the -1 test); list 0x005a9cf0.
  const CEV_E = [3, 77, 151];
  const CEV_ROWS = [
    { busy: 1, h: 0x11 }, { busy: 1, h: -7 },
    { r: -3 }, { r: -4 }, { r: -5, cnt: 0 }, { r: -5, cnt: 0x40 }, { r: -6, cnt: 0x30 }, { r: -1 }, { r: -2 }, { r: -7 },
    { r: 0, list: [-1] }, { r: 2, list: [5, 9, 3], tag: -1, d54: 5 }, { r: 2, list: [5, 9, 3], tag: -1, d54: 0 },
    { r: 2, list: [5, 9, 3], tag: -1, d54: -3 }, { r: 1, list: [1, 77], tag: 0, d54: 0 }, { r: 0, list: [151], tag: 2, d54: -9 },
    { r: 1, list: [8, 3], tag: -1, d54: 1, x: -0x5000, y: 0x7ffffc00 },
  ];
  F.c3fa_cev = function () {
    return scene({
      stands: [
        { addr: 0x00436c00, abi: 'mscdecl', nargs: 2, ret: function () { return cur('h', 0); } },      // handleEmployeeAction
        { addr: 0x00435570, abi: 'mscdecl', nargs: 2, ret: function () { return cur('r', -1); } },     // hitEmployeeSlot435570
      ],
      save: [[0x00561254, 4], [0x0053df54, 4], [0x004c2ba0, 8], [0x00567afc, 4], [0x0059ca54, 4], [0x005aa6c8, 4],
             [0x005a9cf0, 0x10]].concat(CEV_E.map(function (e) { return [0x005794b8 + e * 0x100, 12]; })),
      rows: CEV_ROWS.map(function (o, i) {
        return function () {
          w32(0x00561254, o.busy || 0);
          w32(0x0053df54, o.d54 === undefined ? 0x33 : o.d54);
          w32(0x004c2ba0, 0x1234 + i); w32(0x004c2ba4, 0x4321 + i);
          w32(0x00567afc, 0x99); w32(0x0059ca54, 0x98); w32(0x005aa6c8, o.cnt === undefined ? 0x20 : o.cnt);
          const list = o.list || [-1, -1, -1, -1];
          for (let k = 0; k < 4; k++) w32(0x005a9cf0 + 4 * k, k < list.length ? list[k] : -1);
          CEV_E.forEach(function (e, j) {
            const rec = 0x005794b8 + e * 0x100;
            w32(rec, o.x === undefined ? 0x2400 + 0x100 * j + 0x37 * i : o.x);
            w32(rec + 4, o.y === undefined ? 0x5100 - 0x300 * j + i : o.y);
            w32(rec + 8, o.tag === undefined ? 0 : (j === 0 ? o.tag : 0));
          });
          S.cur = { r: o.r, h: o.h };
          return 0x100 + i;
        };
      }),
    });
  };

  // ----------------------------------------------------------------------------------------- handleEmployeeAction
  // Walker list 0x005aa5b8 (count [0x005aa6c0]), walker records 0x00585850 + g * 0x4c (+0 x, +4 y, +0x12 flag byte,
  // +0x2a name), the per-day word table 0x0058421a + 20 * k (k < 100).
  const HEA_G = [2, 7, 11];
  const HEA_ROWS = [
    { r: -3 }, { r: -2 }, { r: 0 }, { r: 1 }, { r: 2, a558: 0 }, { r: 2, a558: 9 }, { r: 3, a558: -1 },
    { r: 4, sel: 1 }, { r: 4, sel: -1 }, { r: 4, sel: 2, open: 0 },
    { r: 5, sel: 0, day: 37 }, { r: 5, sel: 2, day: 199 }, { r: 5, sel: -1 }, { r: 5, sel: 1, open: 0 },
    { r: 6, sel: 1 }, { r: 6, sel: 0, typed: '' }, { r: 6, sel: -1 }, { r: 6, sel: 2, open: 0 },
    { r: 7 }, { r: -1 }, { r: -9 }, { r: 8 }, { r: 10 }, { r: 11 }, { r: 12 }, { r: 0x7fffffff },
  ];
  F.c3fa_hea = function () {
    return scene({
      stands: [
        { addr: 0x00436b00, abi: 'mscdecl', nargs: 2, ret: function () { return cur('r', -1); } },     // hitEmployee436b00
        { addr: 0x00467600, abi: 'mscdecl', nargs: 1, ret: 0x7600 },                                    // pickGolferName
        { addr: 0x0045b2c0, abi: 'mscdecl', nargs: 4, str: { 0: 2 }, ret: 0x2c0,                         // textInputBox
          write: function () { ptr(0x0051a068).writeUtf8String(cur('typed', 'Typed name')); } },
        { addr: 0x0040c890, abi: 'mscdecl', nargs: 4, ret: 0xc890 },                                    // pointsPopup
      ],
      save: [[0x00561254, 4], [0x00567afc, 4], [0x004c7aec, 4], [0x0059ca54, 4], [0x005aa558, 4], [0x004c2e10, 4],
             [0x00571fd4, 4], [0x004c2ba0, 8], [0x0051a068, 0x40], [0x005aa5b8, 0x10], [0x005aa6c0, 4], [0x005a6d3c, 2],
             [0x0058421a, 0x7d0]].concat(HEA_G.map(function (g) { return [0x00585850 + g * 0x4c, 0x4c]; })),
      rows: HEA_ROWS.map(function (o, i) {
        return function () {
          w32(0x00561254, 1); w32(0x00567afc, 0x77); w32(0x004c7aec, o.sel === undefined ? 1 : o.sel);
          w32(0x0059ca54, o.open === undefined ? 1 : o.open); w32(0x005aa558, o.a558 === undefined ? 4 : o.a558);
          w32(0x004c2e10, 0x55); w32(0x00571fd4, 1000 + i); w32(0x004c2ba0, 0x10 + i); w32(0x004c2ba4, 0x20 + i);
          ptr(0x0051a068).writeUtf8String('old text ' + i);
          HEA_G.forEach(function (g, k) { w32(0x005aa5b8 + 4 * k, g); });
          w32(0x005aa5b8 + 12, 0x7777);
          w32(0x005aa6c0, 3);
          w16(0x005a6d3c, o.day === undefined ? 5 : o.day);
          for (let k = 0; k < 100; k++) w16(0x0058421a + 20 * k, 0x100 + k);
          HEA_G.forEach(function (g, k) {
            const rec = 0x00585850 + g * 0x4c;
            w32(rec, 0x3000 + 0x411 * k + i); w32(rec + 4, 0x8000 - 0x203 * k); w8(rec + 0x12, 0xa5);
            ptr(rec + 0x2a).writeUtf8String('walker' + k);
          });
          S.cur = { r: o.r, typed: o.typed };
          return 0x30 + i;
        };
      }),
    });
  };

  // --------------------------------------------------------------------------------------------- clickGolferPanel
  const CGP_ROWS = [
    { busy: 1, h: 0x44 },
    { s: 0, ed: 5 }, { s: 0, ed: 0 }, { s: 0, ed: 0x1f },
    { s: 1, n: 2, f: 0x2000, sf8: -1 }, { s: 1, n: 1, f: 0x2000, sf8: -1 }, { s: 1, n: 2, f: 0x200000, sf8: -1 },
    { s: 1, n: 2, f: 0x2000, sf8: 3 },
    { s: 2, n: 2, f: 0x2000, e14: 4, sf8: -1 }, { s: 2, n: 2, f: 0, e14: 4, sf8: -1 }, { s: 2, n: 1, f: 0x2000, e14: 4, sf8: -1 },
    { s: 2, n: 3, f: 0x2000, e14: -1, sf8: -1 }, { s: 2, n: 3, f: 0x2000, e14: 4, sf8: 0 },
    { s: 2, n: 3, f: 0x4002000, e14: 4, sf8: -1 },
    { s: 3, f: 0x400000, sf8: -1 }, { s: 3, f: 0x400000 | 0x4000000, sf8: -1 }, { s: 3, f: 0, sf8: -1 },
    { s: 3, f: 0x400000, sf8: 7 },
    { s: 4 }, { s: 5 }, { s: 6 }, { s: 7 }, { s: 8 }, { s: 9 }, { s: 10 }, { s: 11 }, { s: -1 },
    { s: 1, n: 2, f: 0x2000, sf8: -1, flip: 0x200000 }, { s: 3, f: 0x400000, sf8: -1, flip: 0x4000000, flipAt: 1 },
  ];
  F.c3fa_cgp = function () {
    return scene({
      stands: [
        { addr: 0x00436c00, abi: 'mscdecl', nargs: 2, ret: function () { return cur('h', 0); } },      // handleEmployeeAction
        { addr: 0x00435f00, abi: 'mscdecl', nargs: 2, ret: function () { return cur('s', -1); } },     // hitGolferPanel435f00
        { addr: 0x00473e60, abi: 'thiscall', nargs: 4, ret: 0x3e60,                                     // Image_draw
          write: function (self, a, n) {     // a draw may change the flag word: the body must re-read it
            if (cur('flip', 0) && n === cur('flipAt', 0)) w32(0x0059e7b8, r32(0x0059e7b8) ^ cur('flip', 0));
          } },
        { addr: 0x004385d0, abi: 'mscdecl', nargs: 2, ret: 0x85d0 },                                    // showCharacterEditor
      ],
      save: [[0x00561254, 4], [0x005685f0, 4], [0x0059e7b8, 4], [0x004c2e14, 4], [0x005a59f8, 4], [0x004d60b4, 4],
             [0x00582cd0, 2], [0x00582d6e, 2], [0x0058f330, 4], [0x00567afc, 4], [0x0059ca54, 4]],
      rows: CGP_ROWS.map(function (o, i) {
        return function () {
          w32(0x00561254, o.busy || 0); w32(0x005685f0, o.n === undefined ? 2 : o.n);
          w32(0x0059e7b8, (o.f === undefined ? 0x2000 : o.f) | 0x81);
          w32(0x004c2e14, o.e14 === undefined ? 4 : o.e14); w32(0x005a59f8, o.sf8 === undefined ? -1 : o.sf8);
          w32(0x004d60b4, o.ed === undefined ? 0x9 : o.ed);
          w8(0x00582cd0, 0x11); w8(0x00582cd1, 0x12); w16(0x00582d6e, 0x3333);
          w32(0x0058f330, 0x66); w32(0x00567afc, 0x67); w32(0x0059ca54, 0x68);
          S.cur = { s: o.s, h: o.h, flip: o.flip, flipAt: o.flipAt };
          return 0x200 + i;
        };
      }),
    });
  };

  // ------------------------------------------------------------------------------------------------ panelDispatch
  F.c3fa_pd = function () {
    const mk = function (addr, tag) {
      return { addr: addr, abi: 'thiscall', nargs: 4, ret: function (s, a) { return (tag + a[0] + 3 * a[3]) >>> 0; } };
    };
    return scene({
      stands: [mk(0x0049ed20, 0x1000), mk(0x0048a5d0, 0x2000), mk(0x004a2400, 0x3000), mk(0x004a3880, 0x4000),
               mk(0x0049d6d0, 0x5000)],
      build: function (fx) {
        fx.obj = Memory.alloc(0x800);
        for (let k = 0; k < 0x800; k += 4) fx.obj.add(k).writeU32(0xa5000000 + k);
      },
    });
  };

  // -------------------------------------------------------------------------------------------- decodeImageChunks
  // chunk: +0 size (dword), +4 type (word), +6 sub-chunk count (word), sub-chunks from +0x10 (each +0 = its size).
  // image: +0x34 record table pointer, +0x3c, +0x40 record index (the copy test of the frame loop).
  F.c3fa_dic = function () {
    const tagged = function (addr, tag) {
      return { addr: addr, abi: 'thiscall', nargs: 3, ret: function (s, a) { return (tag ^ a[2]) >>> 0; } };
    };
    return scene({
      stands: [
        { addr: 0x004826f0, abi: 'thiscall', nargs: 1, ret: 0x4826f0 },     // Palette::decodeChunk
        tagged(0x004827d0, 0x70000), tagged(0x00487cb0, 0xc0000), tagged(0x00482940, 0xd0000),
        tagged(0x00482990, 0xf0000), tagged(0x00482a80, 0x100000),
        { addr: 0x004744c0, abi: 'thiscall', nargs: 1, ret: 0x4744c0 },     // Surface_copyFrom
      ],
      build: function (fx) {
        fx.obj = Memory.alloc(0x40);
        const leaf = function (t) {
          const c = Memory.alloc(0x40);
          c.writeU32(0x20); c.add(4).writeU16(t); c.add(6).writeU16(0);
          return c;
        };
        [0, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17, 0xf1f9, 0xf1fb, 0xffff].forEach(function (t) {
          fx['t' + t.toString(16)] = leaf(t);
        });
        // frames: n sub-chunks of types `types`, sizes 0x10 / 0x18 alternating
        const frame = function (types) {
          const c = Memory.alloc(0x200);
          c.writeU32(0x200); c.add(4).writeU16(0xf1fa); c.add(6).writeU16(types.length);
          let off = 0x10;
          types.forEach(function (t, k) {
            const sz = k & 1 ? 0x18 : 0x10;
            c.add(off).writeU32(sz); c.add(off + 4).writeU16(t); c.add(off + 6).writeU16(0);
            off += sz;
          });
          return c;
        };
        fx.f0 = frame([]); fx.f1 = frame([7]); fx.f3 = frame([4, 15, 16]); fx.f5 = frame([13, 99, 7, 11, 12]);
        const img = function (recs, x3c, idx) {
          const p = Memory.alloc(0x60);
          p.add(0x34).writePointer(recs); p.add(0x3c).writeU32(x3c); p.add(0x40).writeU32(idx);
          return p;
        };
        const recs = Memory.alloc(0x2c * 4);
        fx.i0 = img(ptr(0), 0, 0); fx.i1 = img(recs, 0, 2); fx.i2 = img(recs, 1, 1); fx.i3 = img(recs, 0, 0);
      },
    });
  };
})();
