// Fixtures for batch c3ea (simulation plan phase 2, 2026-10-11): the shot physics cycle trialShot 0x004226a0,
// simulateBallFlight 0x00422fb0, simulateShot 0x00424120. Reimplementations: shim/src/re/c3ea.cpp. Registry:
// re/frida/registry.d/c3ea.py. Purposes and per-jcc tables: log/c3/c3ea_purpose.md.
//
// Method (helpers copied from c3dz.js / c3dy.js and extended):
// - Every key freezes the game's first thread (re/frida/js/freeze.js) and saves every live table a row writes; the flush
//   restores them newest first before the resume.
// - Each row builds its own synthetic course from a seed: the tile types 0x005722e8 (50 x 50), the tile flag words 0x0053caf0,
//   the wall bits 0x005619a0, the hole bytes 0x0056988c, the corner-height cache 0x0051b770 that cornerHeights 0x0040bfe0
//   returns first, the height table 0x005a4998 that heightBlend 0x0040c170 returns while [0x00834170] != 0, the type table
//   0x00578370 (0x25 records of 0x30), the hole records 0x00575ac8, the placed objects 0x0058bcb8, the pair snapshots
//   0x0059fc60 and the golfer records 2..9 (0x005796b8..0x00579eb8). Only synthetic values are written.
// - The cycle partners run for real (their hooks or, with SIMGOLF_HOOKS_OFF, their originals). The display and pump callees
//   (tileToScreen 0x0042f940, setDrawTarget 0x004762d0, drawCenteredText2 0x00404bc0, Window::refreshFull 0x00480c80,
//   pumpAndRedraw 0x0045c0c0, pumpInput483bd0 0x00483bd0) and emitGolferThought 0x00467a00 are stood in on the A/B thread
//   only (calleeStand): each call is counted, folded into a running FNV-1a hash and, while there is room, logged; the log is
//   compared state.
// - simulateBallFlight reads three stack slots it never initialises ([F+0xb4], [F+0x98], [F+0x9c], see c3ea.cpp). For the
//   keys that can reach it, __alloca_probe 0x004a6070 is patched to a stub that, on the A/B thread and only for the size
//   0x7cb4, does the probe itself and writes the row's three values into those slots of the new frame, so both arms start
//   from the same stack bytes; every other call runs a byte copy of the CRT routine (0x004a6070..0x004a609e).
// Rows: diff_hook resolves "$rN" before the state snapshot, so the getter of fx.rN applies row N (c3dy's getter trick).
(function () {
  const F = {};
  function alloc(n) {
    const p = __keepAlloc(n);
    for (let i = 0; i < n; i += 4) p.add(i).writeU32(0);
    return p;
  }
  // The negative-control build keeps its wrong bodies dormant until this flag page is set (memory note negctl-wrong-bodies).
  function negOn() {
    try {
      if (ptr('0x7A110000').add(4).readU32() === 0x5147454e) __freeze.setU32(ptr('0x7A110000'), 1);
    } catch (e) { /* normal build: no flag page */ }
  }
  let frozen = false;
  function freezeNow() {
    if (frozen) return;
    __freeze.thread();
    frozen = true;
    __freeze.atFlush(function () { frozen = false; });
    negOn();
  }
  function wr32(a, v) { ptr(a).writeU32(v >>> 0); }
  function wr16(a, v) { ptr(a).writeU16(v & 0xffff); }
  function wr8(a, v) { ptr(a).writeU8(v & 0xff); }
  function saveRange(a, n) { __freeze.save(ptr(a), n); }
  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }

  // ---- rows: fx.r<i> applies row i (row.on) and returns row.a; fx.b<i>..fx.e<i> are the further arguments
  let cur = {};
  function rowsOn(fx, rows) {
    rows.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () {
        cur = row;
        if (row.on) row.on(fx, row);
        return (row.a === undefined ? 0 : row.a) >>> 0 | 0;
      } });
      ['b', 'c', 'd', 'e'].forEach(function (k) {
        Object.defineProperty(fx, k + i, { enumerable: true, configurable: true, get: function () {
          const v = row[k]; return v === undefined ? 0 : (v >>> 0 | 0);
        } });
      });
    });
  }

  // ---- calleeStand (c3dz's, with a running hash): log word 0 = cursor, words 1..16 = call count of stub k, word 17 = FNV-1a
  // hash over every call (va and logged values), entries {va, count, values...} from +0x50 while they fit.
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x2000;
  function newClog(cap) { const l = alloc(cap || CLOG); l.writeU32(0x50); l.add(0x44).writeU32(0x811c9dc5); return l; }
  function fnvWord(h, w) {
    for (let k = 0; k < 4; k++) { h ^= (w >>> (8 * k)) & 0xff; h = Math.imul(h, 0x01000193) >>> 0; }
    return h >>> 0;
  }
  function calleeStand(fx, k, va, argc, ret, opts) {
    opts = opts || {};
    freezeNow();
    const tid = Process.getCurrentThreadId();
    const at = ptr(va), key = at.toString();
    if (standLive[key]) {
      try { Interceptor.revert(at); } catch (e) { /* already reverted */ }
      delete standLive[key];
      Interceptor.flush();
    }
    const argt = Array(argc).fill('uint32');
    const orig = new NativeFunction(at, 'uint32', argt, opts.abi || 'default');
    const log = fx.clog;
    const cap = fx.clogCap || CLOG;
    const fn = function () {
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, arguments);
      const a = Array.prototype.slice.call(arguments);
      const cell = log.add(4 + 4 * k);
      const n = cell.readU32();
      cell.writeU32(n + 1);
      const vals = a.map(function (v, ix) { return (opts.mask && opts.mask.indexOf(ix) >= 0) ? 0 : v; });
      if (opts.str) Object.keys(opts.str).forEach(function (ix) {
        vals[ix] = 0xfeedf00d;
        const nw = opts.str[ix], p = ptr(a[ix]), words = new Array(nw).fill(0);
        let end = false;
        for (let q = 0; q < 4 * nw; q++) {
          const c = end ? 0 : p.add(q).readU8();
          if (c === 0) end = true;
          words[q >> 2] |= c << (8 * (q & 3));
        }
        words.forEach(function (x) { vals.push(x >>> 0); });
      });
      let h = log.add(0x44).readU32();
      h = fnvWord(h, va);
      vals.forEach(function (v) { h = fnvWord(h, v >>> 0); });
      log.add(0x44).writeU32(h);
      const curs = log.readU32();
      const need = 8 + 4 * vals.length;
      if (curs + need <= cap) {
        log.add(curs).writeU32(va >>> 0);
        log.add(curs + 4).writeU32(vals.length);
        vals.forEach(function (v, i) { log.add(curs + 8 + 4 * i).writeU32(v >>> 0); });
        log.writeU32(curs + need);
      }
      let r;
      if (typeof ret === 'function') r = ret(n, a);
      else if (Array.isArray(ret)) r = ret[Math.min(n, ret.length - 1)];
      else r = ret === undefined ? 0 : ret;
      return r >>> 0;
    };
    const cb = new NativeCallback(fn, 'uint32', argt, opts.abi || 'default');
    __diffKeepAlive.push(cb, orig);
    Interceptor.replace(at, cb);
    const tok = standTok++;
    standLive[key] = tok;
    Interceptor.flush();
    setTimeout(function () {
      if (standLive[key] !== tok) return;
      Interceptor.revert(at);
      delete standLive[key];
      Interceptor.flush();
    }, 0);
  }

  // ---- __alloca_probe stand-in that seeds simulateBallFlight's three uninitialised slots (hand-encoded; offsets in the
  // comments are from the stub start). cfg: +0 value for [F+0xb4], +4 for [F+0x98], +8 for [F+0x9c], +0xc count of seeded
  // frames (state).
  //   +00 50                  push eax              +3f 52                    push edx
  //   +01 64 a1 24 00 00 00   mov eax, fs:[0x24]    +40 8b 15 <cfg>           mov edx, [cfg]
  //   +07 3d <tid>            cmp eax, tid          +46 89 94 24 a8 00 00 00  mov [esp+0xa8], edx    ; F+0xb4
  //   +0c 58                  pop eax               +4d 8b 15 <cfg+4>         mov edx, [cfg+4]
  //   +0d 0f 85 <GEN>         jne GEN               +53 89 94 24 8c 00 00 00  mov [esp+0x8c], edx    ; F+0x98
  //   +13 3d b4 7c 00 00      cmp eax, 0x7cb4       +5a 8b 15 <cfg+8>         mov edx, [cfg+8]
  //   +18 0f 85 <GEN>         jne GEN               +60 89 94 24 90 00 00 00  mov [esp+0x90], edx    ; F+0x9c
  //   +1e 51                  push ecx              +67 ff 05 <cfg+0xc>       inc dword [cfg+0xc]
  //   +1f 8d 4c 24 08         lea ecx, [esp+8]      +6d 5a                    pop edx
  //   +23 81 e9 00 10 00 00   sub ecx, 0x1000       +6e 8b 08                 mov ecx, [eax]
  //   +29 2d 00 10 00 00      sub eax, 0x1000       +70 8b 40 04              mov eax, [eax+4]
  //   +2e 85 01               test [ecx], eax       +73 50                    push eax
  //   +30 3d 00 10 00 00      cmp eax, 0x1000       +74 c3                    ret
  //   +35 73 ec               jae +23               +75 GEN: the CRT routine 0x004a6070..0x004a609e, byte for byte with
  //   +37 2b c8               sub ecx, eax              its two short jumps (jb +0x14, jae -0x14) position independent
  //   +39 8b c4               mov eax, esp
  //   +3b 85 01               test [ecx], eax
  //   +3d 8b e1               mov esp, ecx
  // After `mov esp, ecx` esp = E - 0x7cb4 (E = the caller's esp at simulateBallFlight's entry); the four register pushes
  // that follow make F = E - 0x7cc4, so after `push edx` [F+0xb4] = [esp+0xa8], [F+0x98] = [esp+0x8c], [F+0x9c] = [esp+0x90].
  function chkstkOn(fx) {
    freezeNow();
    const cfg = alloc(0x10);
    fx.slot = cfg;
    const tid = Process.getCurrentThreadId();
    const stub = __keepAlloc(Process.pageSize);
    const A = cfg.toUInt32();
    const head = [0x50, 0x64, 0xa1, 0x24, 0x00, 0x00, 0x00]
      .concat([0x3d], u32(tid))
      .concat([0x58])
      .concat([0x0f, 0x85], u32(0x75 - 0x13))
      .concat([0x3d, 0xb4, 0x7c, 0x00, 0x00])
      .concat([0x0f, 0x85], u32(0x75 - 0x1e))
      .concat([0x51, 0x8d, 0x4c, 0x24, 0x08])
      .concat([0x81, 0xe9, 0x00, 0x10, 0x00, 0x00, 0x2d, 0x00, 0x10, 0x00, 0x00, 0x85, 0x01, 0x3d, 0x00, 0x10, 0x00, 0x00, 0x73, 0xec])
      .concat([0x2b, 0xc8, 0x8b, 0xc4, 0x85, 0x01, 0x8b, 0xe1])
      .concat([0x52])
      .concat([0x8b, 0x15], u32(A)).concat([0x89, 0x94, 0x24, 0xa8, 0x00, 0x00, 0x00])
      .concat([0x8b, 0x15], u32(A + 4)).concat([0x89, 0x94, 0x24, 0x8c, 0x00, 0x00, 0x00])
      .concat([0x8b, 0x15], u32(A + 8)).concat([0x89, 0x94, 0x24, 0x90, 0x00, 0x00, 0x00])
      .concat([0xff, 0x05], u32(A + 12))
      .concat([0x5a, 0x8b, 0x08, 0x8b, 0x40, 0x04, 0x50, 0xc3]);
    if (head.length !== 0x75) throw new Error('c3ea chkstk stub: head is ' + head.length.toString(16));
    const gen = Array.from(new Uint8Array(ptr('0x004a6070').readByteArray(0x2f)));
    const expect = [0x51, 0x3d, 0x00, 0x10, 0x00, 0x00, 0x8d, 0x4c, 0x24, 0x08, 0x72, 0x14];
    expect.forEach(function (b, i) { if (gen[i] !== b) throw new Error('c3ea chkstk stub: 0x004a6070 is not the CRT routine'); });
    const bytes = head.concat(gen);
    Memory.patchCode(stub, bytes.length, function (code) { code.writeByteArray(bytes); });
    const back = new Uint8Array(stub.readByteArray(bytes.length));
    if (back.some(function (b, i) { return b !== bytes[i]; })) throw new Error('c3ea chkstk stub: write failed');
    const site = ptr('0x004a6070');
    const saved = site.readByteArray(5);
    const rel = stub.sub(site.add(5)).toUInt32();
    Memory.patchCode(site, 5, function (code) { code.writeByteArray([0xe9].concat(u32(rel))); });
    __freeze.atFlush(function () { Memory.patchCode(site, 5, function (code) { code.writeByteArray(saved); }); });
  }

  // ====================================================================================================================
  // The synthetic course
  // ====================================================================================================================
  function rng(seed) {
    let s = (seed >>> 0) || 1;
    return function () { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; return s; };
  }
  const T_SAVE = [
    [0x005722e8, 2500], [0x0053caf0, 5000], [0x005619a0, 2500], [0x0056988c, 2500], [0x0051b770, 20000],
    [0x005a4998, 0xa00], [0x00578370, 0x6f0], [0x00575ac8, 0x208 * 20], [0x0058bcb8, 0x1000], [0x0059fc60, 0x3880],
    [0x005796b8, 0x800], [0x00582fb8, 0x100], [0x00822d9c, 4], [0x005a47b8, 0x2c], [0x005685c8, 0x28], [0x0053fd20, 0x28],
    [0x005a9cc4, 0x28], [0x005a8828, 0xc], [0x0056a78c, 8], [0x0059b738, 4], [0x0059e7b8, 4], [0x004c2e1c, 4],
    [0x004c2fc8, 8], [0x005a7140, 4], [0x0051a068, 0x100], [0x0058a528, 0x20], [0x00834170, 4], [0x00822c88, 4],
    [0x0058f330, 4], [0x005a59f8, 4], [0x00543cb8, 4], [0x00543cc0, 4], [0x00543cc8, 4], [0x005a34e0, 4], [0x0053df54, 4],
    [0x005849e0, 0x2c * 8], [0x004d60a9, 0x230 * 8], [0x005783a2, 1]
  ];
  function saveCourse() { T_SAVE.forEach(function (r) { saveRange(r[0], r[1]); }); }

  // type records: +0 bounce, +1 friction, +2 penalty (b2), +6 marker (0xd), +7 class (0x10 = object), +0xc height flags
  // (8 flat, 2 min corner, 4 max corner, 0 corners). Rows pick one of three tables.
  function typeTable(variant) {
    const buf = new Uint8Array(0x6f0);
    const put = function (t, bounce, fr, b2, b6, b7, fl) {
      const o = t * 0x30;
      buf[o] = bounce & 0xff; buf[o + 1] = fr & 0xff; buf[o + 2] = b2 & 0xff; buf[o + 6] = b6 & 0xff; buf[o + 7] = b7 & 0xff;
      buf[o + 0xc] = fl & 0xff;
    };
    for (let t = 0; t < 0x25; t++) {
      const k = (t * 7 + variant * 3) % 11;
      put(t, k % 5, 1 + (k % 4), (k % 3) - 1, (k === 4 || k === 9) ? 0xd : 0, (k === 6) ? 0x10 : 0, [0, 0, 2, 4, 0, 8][k % 6]);
    }
    put(0, 2, 3, 0, 0, 0, 0);
    put(1, variant === 3 ? 6 : 1, 2, -1, 0, 0, 0);   // green (scanTileLine leaves its +2 at 0xff); variant 3 bounces
    put(2, 2, 3, 0, 0, 0, 0);                     // fairway
    put(3, 3, 4, 1, 0, 0, 0);
    put(4, 1, 5 + (variant & 1), 1, 0xd, 0, 0);   // rough: friction 5 or 6
    put(5, 1, 7, 2, 0xd, 0, 0);
    put(6, 0, 9, 2, 0, 0, 0);
    put(7, 2, 3, 0, 0, 0, 0);
    put(8, 2, 4, 1, 0, 0, 2);
    put(9, 2, 4, 0, 0, 0, 4);
    put(10, 2, 3, 0, 0, 0, 0);
    put(0xc, 3, 3, 2, 0, 0, 0);
    put(0x11, 0, variant === 2 ? -2 : 9, 4, 0, 0, 8);   // water
    put(0x13, 1, 5, 1, 0, 0x10, 0);
    put(0x14, 0, 9, 3, 0, 0, 0);                  // blocked (tileBlocked); +2 at 0x00578732 is rewritten by simulateShot
    put(0x15, 1, 4, 1, 0, 0x10, 0);               // object tile
    put(0x16, 2, 2, 0, 0xd, 0, 0);
    return buf;
  }

  // a scene: s = {seed, water, rough, sand, block, misc, flags, walls, hills, flat, tv, hole, hx, hy, objs, pairs}
  function course(s) {
    const R = rng(s.seed * 2654435761);
    const fr = function () { return R() / 4294967296; };
    const tiles = new Uint8Array(2500), flags = new Uint16Array(2500), walls = new Uint8Array(2500), hb = new Uint8Array(2500);
    const misc = [0, 3, 7, 8, 9, 10, 0xc, 0xd, 0xe, 0x12, 0x13, 0x15, 0x16];
    for (let i = 0; i < 2500; i++) {
      const r = fr();
      let t = 2;
      if (r < s.water) t = 0x11;
      else if (r < s.water + s.rough) t = (R() & 1) ? 4 : 5;
      else if (r < s.water + s.rough + s.sand) t = 6;
      else if (r < s.water + s.rough + s.sand + s.block) t = 0x14;
      else if (r < s.water + s.rough + s.sand + s.block + s.misc) t = misc[R() % misc.length];
      tiles[i] = t;
      let f = 0;
      const q = R();
      if ((q & 0xff) < s.flags) f |= [0x20, 0x80, 0x100, 0x400, 0x800, 0x4000, 0x20 | 0x100, 0x800 | 0x4000][(q >>> 8) & 7];
      if (f & 0x80) f |= (q >>> 12) & 0x1f;
      flags[i] = f;
      if ((R() & 0xff) < s.walls) walls[i] = [1, 4, 0x10, 0x40, 5, 0x50, 0x11, 0x44][(R() >>> 5) & 7];
      if ((R() & 0xff) < 8) hb[i] = 0x80;
      if (s.greens && fr() < s.greens) tiles[i] = 1;
    }
    // the green around the hole
    for (let dx = -s.green; dx <= s.green; dx++)
      for (let dy = -s.green; dy <= s.green; dy++) {
        const x = s.hx + dx, y = s.hy + dy;
        if (x >= 0 && x < 50 && y >= 0 && y < 50) tiles[x * 50 + y] = 1;
      }
    const DX = [0, 1, 1, 1, 0, -1, -1, -1], DY = [-1, -1, 0, 1, 1, 1, 0, -1];
    const inb = function (x, y) { return x >= 0 && x < 50 && y >= 0 && y < 50; };
    // ring: the 8 tiles around the hole, k = octant (kDirX/kDirY order)
    if (s.ring) for (let k = 0; k < 8; k++) { const x = s.hx + DX[k], y = s.hy + DY[k]; if (inb(x, y)) tiles[x * 50 + y] = s.ring[k]; }
    // flagRing: tiles within 2 of the hole become rough (penalty 1) flagged 0x80 | hole
    if (s.flagRing) for (let dx = -2; dx <= 2; dx++) for (let dy = -2; dy <= 2; dy++) {
      const x = s.hx + dx, y = s.hy + dy;
      if ((dx || dy) && inb(x, y)) { tiles[x * 50 + y] = 4; flags[x * 50 + y] = 0x80 | (s.hole & 0x1f); }
    }
    // objField: a box around the hole of object tiles (0x15 / 0x13) under placed objects of type 5 / 4
    if (s.objField) {
      s.objs = [];
      for (let dx = -5; dx <= 5; dx++) for (let dy = -5; dy <= 5; dy++) {
        const x = s.hx + dx, y = s.hy + dy;
        if (!inb(x, y) || (Math.abs(dx) <= 1 && Math.abs(dy) <= 1)) continue;
        const odd = (x + y) & 1;
        const v = s.objField;
        // 1: 0x13 under type 4 / 0x15 under type 5 (some cells without an object); 2: only 0x15 under type 5 with a value;
        // 3: 0x15 and 0x13 both under type 4
        tiles[x * 50 + y] = v === 2 ? 0x15 : (odd ? 0x13 : 0x15);
        if ((v !== 1 || ((x * 3 + y) % 4) !== 0) && s.objs.length < 250) {
          if (v === 2) s.objs.push({ x: x, y: y, type: 5, val: 2 + ((x + y) % 5) });
          else if (v === 3) s.objs.push({ x: x, y: y, type: 4, val: (x * y) % 3 ? 8 : 20 });
          else s.objs.push({ x: x, y: y, type: odd ? 4 : 5, val: odd ? ((x * y) % 3 ? 8 : 20) : ((x * y) % 3 ? 3 + (x % 5) : 0) });
        }
      }
    }
    if (s.patch) s.patch.forEach(function (p) { tiles[p[0] * 50 + p[1]] = p[2]; if (p[3] !== undefined) flags[p[0] * 50 + p[1]] = p[3]; });
    const hcell = s.hx * 50 + s.hy;
    flags[hcell] = (flags[hcell] & ~0x1f) | 0x80 | (s.hole & 0x1f);
    hb[hcell] = s.holeByte === undefined ? hb[hcell] : s.holeByte;
    ptr(0x005722e8).writeByteArray(tiles.buffer);
    ptr(0x0053caf0).writeByteArray(flags.buffer);
    ptr(0x005619a0).writeByteArray(walls.buffer);
    ptr(0x0056988c).writeByteArray(hb.buffer);
    // corner heights (never 0, so the cache always answers) and the heightBlend table (3..15)
    const ch = new Uint8Array(20000), bl = new Uint8Array(0xa00);
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++) {
        const base = s.flat ? 4 : 4 + Math.round(s.hills * Math.sin((x + s.seed) * 0.37) * Math.cos((y - s.seed) * 0.29));
        for (let k = 0; k < 8; k++) ch[(x * 50 + y) * 8 + k] = Math.max(1, Math.min(12, base + (s.flat ? 0 : ((x * 3 + y * 5 + k * 3 + s.seed) % 3) - 1)));
        bl[x * 51 + y] = Math.max(3, Math.min(15, 6 + (s.flat ? 0 : Math.round(s.hills * Math.sin(x * 0.31 + y * 0.17 + s.seed)))));
      }
    if (s.blz) s.blz.forEach(function (p) { bl[p[0] * 51 + p[1]] = p[2]; });
    ptr(0x0051b770).writeByteArray(ch.buffer);
    ptr(0x005a4998).writeByteArray(bl.buffer);
    ptr(0x00578370).writeByteArray(typeTable(s.tv || 0).buffer);
    // hole records 0..19: x, y, counter +0xc, +0x1e4 (0x00575cac)
    for (let h = 0; h < 20; h++) {
      const b = 0x00575ac8 + 0x208 * h;
      wr32(b, h === s.hole ? s.hx : (h * 7 + 3) % 50);
      wr32(b + 4, h === s.hole ? s.hy : (h * 11 + 5) % 50);
      wr32(b + 0xc, h);
      wr32(b + 0x1e4, (s.hrec === undefined ? h % 4 : s.hrec));
    }
    // placed objects: all free (type -1) except the scene's list {x, y, type, val}
    const ob = new Uint8Array(0x1000);
    for (let i = 0; i < 256; i++) { ob[i * 16] = 0xff; ob[i * 16 + 1] = 0xff; }
    ptr(0x0058bcb8).writeByteArray(ob.buffer);
    (s.objs || []).forEach(function (o, i) {
      const b = 0x0058bcb8 + 16 * i;
      wr16(b, o.type); wr16(b + 2, o.x); wr16(b + 4, o.y); wr32(b + 8, o.val);
    });
    // pair snapshots: all free (id -1) except {id, partner, count}
    const pb = new Uint8Array(0x3880);
    for (let i = 0; i < 16; i++) { pb[i * 0x388] = 0xff; pb[i * 0x388 + 1] = 0xff; }
    ptr(0x0059fc60).writeByteArray(pb.buffer);
    (s.pairs || []).forEach(function (p, i) {
      const b = 0x0059fc60 + 0x388 * (i * 3 + 2);
      wr16(b, p.id); wr16(b + 2, p.partner); wr16(b + 4, p.count);
    });
    // golfer types (record word +0xb6): bit 7 of 0x004d60a9 + 0x230 * type for typeBit7Clear; membership bytes
    for (let t = 0; t < 8; t++) {
      wr8(0x004d60a9 + 0x230 * t, (t & 2) ? 0x80 : 0);
      for (let h = 0; h < 0x2c; h++) wr8(0x005849e0 + 0x2c * t + h, ((t + h) % 3) === 0 ? 1 : 0);
    }
  }

  // golfer record g (2..9): o = {x, y (world), hole, flags, w16, b18, b19, b22, b36, b7a, b84, a0, partner, type, skills,
  // speed..spin, z, thought}
  function golfer(g, o) {
    const b = 0x005794b8 + 0x100 * g;
    const z = new Uint8Array(0x100);
    ptr(b).writeByteArray(z.buffer);
    wr32(b + 0x00, o.x + (o.ox || 0)); wr32(b + 0x04, o.y + (o.oy || 0));
    wr32(b + 0x10, o.flags || 0);
    wr16(b + 0x16, o.w16 || 0);
    wr8(b + 0x18, o.b18 || 0); wr8(b + 0x19, o.b19 || 0); wr8(b + 0x1b, o.b1b || 0); wr8(b + 0x1c, 0);
    wr8(b + 0x21, o.hole); wr8(b + 0x22, o.b22 || 0); wr8(b + 0x36, o.b36 === undefined ? 3 : o.b36);
    wr8(b + 0x7a, o.b7a || 0); wr8(b + 0x84, o.b84 || 0);
    wr16(b + 0x88, o.thought || 0);
    wr16(b + 0xa0, o.a0 || 0); wr16(b + 0xa2, o.partner === undefined ? (g ^ 1) : o.partner); wr16(b + 0xb6, o.type || 0);
    wr8(b + 0xba, o.bba || 0);
    wr32(b + 0xcc, o.tx || 0); wr32(b + 0xd0, o.ty || 0);
    wr32(b + 0xd4, o.x); wr32(b + 0xd8, o.y); wr32(b + 0xdc, o.z || 0);
    wr32(b + 0xe0, o.dir || 0); wr32(b + 0xe4, o.speed || 0); wr32(b + 0xe8, o.vz || 0); wr32(b + 0xec, o.spin || 0);
    const sk = o.skills || [3, 4, 5, 2, 6, 1, 4, 7];
    for (let k = 0; k < 8; k++) wr8(b + 0xf2 + k, sk[k]);
    wr8(b + 0xf0, o.bf0 || 0); wr8(b + 0xf1, o.bf1 || 0);
  }
  // globals: o = {gf (0x0059e7b8), slope (0x004c2e1c), cc4 (0x005a9cc4), lvl (0x00822c88), m330 (0x0058f330), player
  // (0x005a59f8), cb8, cc0, e0 (byte 0x005a47e0), ctype (byte 0x005a34e0), dbg (0x004c2fc8), sel (0x0053df54), seed}
  function globals(o) {
    wr32(0x0059e7b8, o.gf || 0);
    wr32(0x004c2e1c, o.slope === undefined ? 7 : o.slope);
    wr32(0x005a9cc4, o.cc4 || 0);
    wr32(0x00822c88, o.lvl || 0);
    wr32(0x0058f330, o.m330 || 0);
    wr32(0x005a59f8, o.player === undefined ? 0 : o.player);
    wr32(0x00543cb8, o.cb8 || 0); wr32(0x00543cc0, o.cc0 || 0); wr32(0x00543cc8, 1);
    wr8(0x005a47e0, o.e0 || 0);
    wr8(0x005a34e0, o.ctype === undefined ? 0 : o.ctype);
    wr32(0x004c2fc8, o.dbg === undefined ? -1 : o.dbg);
    wr32(0x0053df54, o.sel || 0);
    wr32(0x00822d9c, o.seed >>> 0);
    wr32(0x00834170, 1);                                 // heightBlend answers from 0x005a4998
    wr32(0x005a9ce0, 0);                                 // shotPower's next cache slot
    for (let i = 0; i < 10; i++) { wr32(0x005a47b8 + 4 * i, 0x7fff0000 + i); wr32(0x005685c8 + 4 * i, 0); wr32(0x0053fd20 + 4 * i, 0); }
    wr32(0x0056a78c, o.land0 || 0); wr32(0x0056a790, o.land1 || 0);
    wr32(0x005a7140, o.a7140 || 0);
    wr32(0x005a9ce4, o.ce4 || 0); wr32(0x005a9ce8, o.ce8 || 0);
    wr32(0x005a8828, 0); wr32(0x005a8830, 0);
    wr32(0x0059b738, 0);
    wr8(0x0051a068, 0);
  }

  // ---- display, pump and thought stand-ins (all keys install the same set; k = stub index in the log)
  function standAll(fx, row0) {
    calleeStand(fx, 0, 0x0042f940, 4, function (n, a) {             // tileToScreen(tx, ty, &sx, &sy)
      const tx = a[0] | 0, ty = a[1] | 0;
      if (((tx + 2 * ty) % 5 + 5) % 5 === 0) return 0;               // a tile it does not cache: nothing written
      ptr(a[2]).writeS32(tx * 16 + 8); ptr(a[3]).writeS32(ty * 8 + 4);
      return 1;
    }, { mask: [2, 3] });
    calleeStand(fx, 1, 0x004762d0, 5, function () { return 0; }, { abi: 'thiscall' });
    calleeStand(fx, 2, 0x00404bc0, 4, function (n) { return (0x404bc0 + n) >>> 0; }, { str: { 0: 4 } });
    calleeStand(fx, 3, 0x00480c80, 2, function (n) { return (0x480c80 + n) >>> 0; }, { abi: 'thiscall' });
    calleeStand(fx, 4, 0x0045c0c0, 1, function (n) { return (0x45c0c0 + n) >>> 0; });
    calleeStand(fx, 5, 0x00483bd0, 0, function (n) { return 0; });
    calleeStand(fx, 6, 0x00467a00, 3, function (n, a) {             // emitGolferThought(g, kind, arg)
      if (cur.emitBump && n === 0) {                                  // the row moves the record's +0x7a / +0x84 byte
        const b = 0x005794b8 + 0x100 * (a[0] | 0);
        if (cur.emitBump === 1) wr8(b + 0x7a, ptr(b + 0x7a).readU8() + 1);
        else wr8(b + 0x84, 1);
      }
      return 0;
    });
  }

  function slotsOn(fx, r) {
    if (!fx.slot) return;
    fx.slot.writeU32(r.s0 === undefined ? 0 : r.s0 >>> 0);
    fx.slot.add(4).writeU32(r.s1 === undefined ? 0x11111111 : r.s1 >>> 0);
    fx.slot.add(8).writeU32(r.s2 === undefined ? 0x22222222 : r.s2 >>> 0);
    fx.slot.add(12).writeU32(0);
  }

  function base(opts) {
    freezeNow();
    const fx = { clog: newClog(opts.clogCap || CLOG), clogCap: opts.clogCap || CLOG };
    saveCourse();
    if (opts.chkstk) chkstkOn(fx);
    standAll(fx);
    return fx;
  }

  // ====================================================================================================================
  // Scenes and rows
  // ====================================================================================================================
  function scn(seed, o) {
    return Object.assign({ seed: seed, water: 0.08, rough: 0.2, sand: 0.05, block: 0.03, misc: 0.1, flags: 40, walls: 30,
      hills: 3, flat: false, tv: seed % 3, hole: 4 + (seed % 9), hx: 20 + (seed * 7) % 15, hy: 18 + (seed * 5) % 15, green: 1 + (seed & 1) }, o || {});
  }
  function W(t) { return t * 1024 + 512; }

  // ---- trialShot rows: (g, x, y, aim). The golfer stands a few tiles from the target; the roll after simulateShot's setup is
  // what is compared (landing point, pair snapshots, RNG; the record itself is restored by trialShot).
  function tsRows() {
    const rows = [];
    const mk = function (i, o) {
      const s = scn(100 + i, o.scene);
      const g = 2 + (i % 8);
      const bx = Math.max(1, Math.min(48, s.hx + (o.dx === undefined ? -6 + (i % 5) : o.dx)));
      const by = Math.max(1, Math.min(48, s.hy + (o.dy === undefined ? -5 + ((i * 3) % 7) : o.dy)));
      const gr = Object.assign({ x: W(bx) + ((i * 97) % 300) - 150, y: W(by) + ((i * 53) % 300) - 150, hole: s.hole, b19: 4, b18: 0 }, o.g || {});
      const gl = Object.assign({ seed: 0x1234567 + i * 7919, slope: (i % 3) ? 7 : 3, cc4: 0 }, o.gl || {});
      const tx = o.tx === undefined ? W(s.hx) + (o.tox || 0) : o.tx, ty = o.ty === undefined ? W(s.hy) + (o.toy || 0) : o.ty;
      if (o.pair) s.pairs = [{ id: g, partner: g ^ 1, count: o.pair - 1 }];
      rows.push({ a: g, b: tx, c: ty, d: o.aim === undefined ? (i % 3) - 1 : o.aim, on: function () {
        course(s); globals(gl); golfer(g, gr); golfer(g ^ 1, { x: gr.x + 2048, y: gr.y, hole: s.hole });
      } });
    };
    let i = 0;
    // plain: varied scenes, aims, slope mode on/off
    for (let k = 0; k < 24; k++) mk(i++, { });
    // pair snapshots (count 0 / 5 / 0x1f)
    [1, 6, 0x20].forEach(function (p) { for (let k = 0; k < 3; k++) mk(i++, { pair: p }); });
    // modes of 0x005a9cc4: 1 and 2
    [1, 2].forEach(function (c) { for (let k = 0; k < 4; k++) mk(i++, { gl: { cc4: c } }); });
    // golfer flags: 0x100 (bounce turn, byte18 0 / set), 0x80, 2, word +0x16 bit 0x200 with byte18
    [[0x100, 0], [0x100, 0x21], [0x80, 0], [0x180, 0x40], [0, 0x30]].forEach(function (fb, k) {
      for (let q = 0; q < 3; q++) mk(i++, { g: { flags: fb[0], b18: fb[1], w16: 0x200 | 0x60 } });
    });
    // scene variants: water-heavy, wall-heavy, flat, hilly, many 0xc tiles, friction table 2
    const variants = [
      { water: 0.35 }, { walls: 200 }, { flat: true }, { hills: 6 }, { misc: 0.5 }, { tv: 2, water: 0.2 },
      { rough: 0.6 }, { block: 0.2 }, { flags: 200 }, { sand: 0.4 }
    ];
    variants.forEach(function (v) { for (let q = 0; q < 3; q++) mk(i++, { scene: v }); });
    // short pitches (the ball starts 1..2 tiles away) and long drives
    for (let k = 0; k < 6; k++) mk(i++, { dx: -1 - (k & 1), dy: (k % 3) - 1 });
    for (let k = 0; k < 6; k++) mk(i++, { dx: -12 - k, dy: -6 + k });
    // targeted (coverage pass 2): lobs onto the green (aim 0, class 0) so a later bounce finds flag 0x100 with byte +0x18 == 0
    const quiet = { water: 0, rough: 0, sand: 0, block: 0, misc: 0, walls: 0 };
    for (let k = 0; k < 8; k++) mk(i++, { aim: 0, dx: -3 - (k % 4), dy: (k % 3) - 1, scene: quiet, g: { b18: 0, b19: 4 } });
    // coverage pass 3: the same lobs onto a bouncy green (type table variant 3)
    for (let k = 0; k < 8; k++) mk(i++, { aim: 0, dx: -3 - (k % 4), dy: (k % 3) - 1, scene: Object.assign({ tv: 3, green: 2 }, quiet), g: { b18: 0, b19: 4 } });
    return rows;
  }

  // ---- simulateShot rows: (g, mode, x, y, aim). kind 'trial': x, y given (trialShot's call); 'real': x = -1, mode 0 (the
  // aim search runs when the hole is out of a short shot); 'mode': x = -1, mode != 0 (aim from [0x0058f330]).
  function ssRows(kind) {
    const rows = [];
    let i = 0;
    const mk = function (o) {
      const n = i++;
      const s = scn(300 + n + (kind === 'trial' ? 0 : kind === 'real' ? 1000 : 2000), o.scene);
      const g = o.gsel === undefined ? 2 + (n % 8) : o.gsel;
      const far = o.far === undefined ? 3 + (n % 9) : o.far;
      let bx = Math.max(1, Math.min(48, s.hx - far)), by = Math.max(1, Math.min(48, s.hy - ((n * 3) % 5) + 2));
      if (o.near) { bx = s.hx; by = s.hy; }
      if (o.parity) s.hole = (s.hole & ~1) | (g & 1);
      if (o.holeTile !== undefined) s.patch = (s.patch || []).concat([[s.hx, s.hy, o.holeTile]]);
      if (o.lie !== undefined) s.patch = (s.patch || []).concat([[bx, by, o.lie, o.lieFlags]]);
      if (o.blz) s.blz = [[bx, by, 0]];
      if (o.pair) s.pairs = [{ id: g, partner: g ^ 1, count: 3 }];
      const gr = Object.assign({ x: o.near ? W(bx) + o.near[0] : W(bx) + ((n * 37) % 200) - 100, y: o.near ? W(by) + o.near[1] : W(by) + ((n * 71) % 200) - 100, hole: s.hole,
        b19: [4, 6, 7, 0, 2, 5][n % 6], b18: [0, 0x21, 0x40, 0x20, 0x11][n % 5], w16: [0x10, 0x70, 0, 0x260][n % 4],
        b22: n % 8, b36: [3, 1, -2, 5, 0, -1][n % 6], type: n % 6, a0: (n * 0x1357) & 0xffff,
        skills: [n % 7, (n + 2) % 9, 3, (n * 5) % 8, 1 + n % 4, (n * 3) % 7, 2, (n * 11) % 13], thought: [0, 0x4000, 0x8000][n % 3],
        bba: (n % 5) - 1, bf0: n % 6, bf1: n % 11, tx: s.hx, ty: s.hy }, o.g || {});
      if (o.b22g) gr.b22 = (g & 1) + 1;
      const gl = Object.assign({ seed: 0x2468ace + n * 104729, slope: [7, 3, 4][n % 3], cc4: [0, 0, 1, 0, 2][n % 5], lvl: n % 4,
        m330: n % 5, player: (n % 4 === 0) ? g : 1, cb8: n % 3, cc0: n % 4, e0: [0, 2, 4, 6][n % 4], ctype: n % 2,
        gf: [0, 0x20, 0x200000, 0x800000, 0x20 | 0x200000][n % 5], sel: [0, g + 1, 3][n % 3], a7140: n % 4,
        ce4: n & 1 }, o.gl || {});
      let x = -1, y = 0;
      if (kind === 'trial') { x = W(s.hx) + (o.tox || 0) - 256 + ((n * 131) % 512); y = W(s.hy) + (o.toy || 0) - 256 + ((n * 61) % 512); }
      let mode = kind === 'mode' ? (o.mode === undefined ? 1 + (n % 3) : o.mode) : (o.mode || 0);
      if (o.mode === 'g') mode = g;
      if (o.mode === 'g1') mode = g & ~1;
      rows.push({ a: g, b: mode, c: x, d: y, e: o.aim === undefined ? (n % 3) - 1 : o.aim, emitBump: o.emitBump || 0,
        s0: o.s0 === undefined ? (n % 2 ? 0 : 7) : o.s0, on: function (fx, r) {
          course(s); globals(gl); golfer(g, gr);
          golfer(g ^ 1, { x: gr.x + 4096, y: gr.y, hole: s.hole, type: o.ptype === undefined ? (n + 1) % 6 : o.ptype, b84: o.p84 || 0, b18: o.p18 || 0 });
          slotsOn(fx, r);
        } });
    };
    return { mk: mk, rows: rows };
  }

  // trial: 64 rows (lies, aims, modes, scenes)
  function sstRows() {
    const b = ssRows('trial');
    for (let k = 0; k < 24; k++) b.mk({});
    [1, 2, 4, 6, 9, 10, 0xb, 0xc, 0xd, 0x11, 0x15, 0x17, -1, 0x30].forEach(function (lie) { b.mk({ lie: lie }); });
    [1, 2, 3, 4].forEach(function (m) { b.mk({ mode: m }); b.mk({ mode: m, g: { flags: 1 } }); });
    [{ water: 0.4 }, { flat: true }, { hills: 6 }, { misc: 0.5 }, { block: 0.25 }, { rough: 0.7 }].forEach(function (v) { b.mk({ scene: v }); b.mk({ scene: v, far: 12 }); });
    [0, 1, 2].forEach(function (f) { b.mk({ far: f }); });
    [0x4000000, 0x10000001, 0x2, 0x80].forEach(function (fl) { b.mk({ g: { flags: fl } }); });
    // targeted (coverage pass 2): aims outside -1..1 with a classed golfer, long shots aimed +-1, mode equal to g / g & ~1,
    // the +0x4000000 flag with class 0x20
    [2, -3, 5].forEach(function (a) { b.mk({ aim: a, g: { b18: 0x21 } }); b.mk({ aim: a, g: { b18: 0x41 }, gl: { m330: 3 } }); });
    [1, -1, 1, -1, 1, -1].forEach(function (a, k) { b.mk({ far: 11 + (k % 3), aim: a, g: { b19: 5 + 2 * (k & 1), bba: 9, b18: 0 }, gl: { lvl: 1 + (k % 3) } }); });
    ['g', 'g1', 'g', 'g1'].forEach(function (m, k) { b.mk({ mode: m, far: 1 + (k & 1), gl: { lvl: 0 }, g: { b19: 0 } }); });
    [0x21, 0x20, 0x40].forEach(function (c) { b.mk({ g: { flags: 0x4000000, b18: c } }); });
    // controls pass: the ball beside the cup with blocked tiles all round, so the difficulty is clamped to 10 (dist <= 0x28)
    for (let k = 0; k < 4; k++) b.mk({ far: 0, near: [60 + 40 * k, -80 + 30 * k], holeTile: 2, scene: { green: 0, ring: [0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14] } });
    return b.rows;
  }
  // real shots (x = -1, mode 0): far ones run the aim search; near ones, green lies and putts take the other arms
  function ssrRows() {
    const b = ssRows('real');
    for (let k = 0; k < 10; k++) b.mk({ far: 6 + (k % 6) });                         // aim search (dist > 0x4b / 0x19)
    for (let k = 0; k < 6; k++) b.mk({ far: 1 + (k % 2) });                          // short: the green-slope arm
    for (let k = 0; k < 4; k++) b.mk({ far: 1, lie: 1 });                            // on the green: putts
    for (let k = 0; k < 4; k++) b.mk({ far: 3, lie: 1, g: { flags: 1 } });
    [1, 2].forEach(function (eb) { for (let k = 0; k < 3; k++) b.mk({ far: 2 + k, emitBump: eb }); });
    for (let k = 0; k < 4; k++) b.mk({ far: 2 + k, p84: 1, g: { b22: 3 } });
    for (let k = 0; k < 4; k++) b.mk({ far: 7, scene: { water: 0.3, misc: 0.4 }, g: { flags: 2 } });   // chip-out probes
    for (let k = 0; k < 4; k++) b.mk({ far: 8, g: { flags: 1, b22: 7 } });            // lie -1 via +0x22 > 6
    // targeted (coverage pass 2)
    const quiet = { water: 0, rough: 0, sand: 0, block: 0, misc: 0, flags: 0, walls: 0 };
    for (let k = 0; k < 3; k++) b.mk({ far: 0, lie: 2 });                            // short, not on the green
    const R8 = function (k, a, c) { const r = []; for (let q = 0; q < 8; q++) r.push(((q - k + 8) % 8) < 3 ? a : c); return r; };
    for (let k = 0; k < 8; k++) b.mk({ far: 2 + (k & 1), lie: k & 2 ? 4 : 2, scene: { green: 0, ring: R8(k, 0x11, 2) }, g: { b19: 0, flags: 0 } });
    for (let k = 0; k < 8; k++) b.mk({ far: 2 + (k & 1), lie: 2, scene: { green: 0, ring: R8(k, 2, 0x11) }, g: { b19: 0, flags: 0 } });
    for (let k = 0; k < 4; k++) b.mk({ far: 0, lie: 1, scene: { holeByte: 0x80 }, gl: { e0: 2, gf: 0x200000 }, g: { b36: 0, w16: 0, b19: 0 } });
    for (let k = 0; k < 3; k++) b.mk({ far: 0, lie: 1, scene: { holeByte: 0x80 }, gl: { e0: 2, gf: 0 }, g: { b36: 1, w16: 0, b19: 0, flags: 0 } });
    for (let k = 0; k < 2; k++) b.mk({ far: 0, lie: 1, g: { flags: 1 } });              // flagged golfer on the green
    for (let k = 0; k < 3; k++) b.mk({ far: 1, lie: 1, holeTile: 2, g: { flags: 0, b19: 0 } });   // a putt at a non-green target
    for (let k = 0; k < 2; k++) b.mk({ far: 2, lie: 2, blz: true, g: { b19: 0, flags: 0 } });     // heightBlend 0 at the ball
    for (let k = 0; k < 8; k++) b.mk({ far: 2 + (k % 4), lie: 2, scene: { objField: 1 }, g: { b19: 0, flags: 0, ox: (k - 4) * 300 } });
    for (let k = 0; k < 2; k++) b.mk({ far: 1, lie: 2, holeTile: 4, gl: { ce4: 1 }, g: { b19: 0, flags: 0 } });
    for (let k = 0; k < 2; k++) b.mk({ far: 2, lie: 1, holeTile: 4, gl: { ce4: 1 }, g: { flags: 1 } });
    [0x4800, 0x800, 0x4800].forEach(function (lf) { b.mk({ far: 2, lie: 2, lieFlags: lf, g: { b19: 0, flags: 0, thought: 0 } }); });
    [0, 1].forEach(function (h) { [0, 6].forEach(function (hr) { b.mk({ far: 2, lie: 2, b22g: true, scene: Object.assign({ hole: h, hrec: hr }, quiet), g: { b19: 0, flags: 0 }, gl: { lvl: 1 } }); }); });
    [0, 6].forEach(function (hr) { b.mk({ far: 2, lie: 2, scene: Object.assign({ hole: 1, hrec: hr }, quiet), g: { b19: 0, flags: 0, b22: 1 }, gl: { lvl: 2 } }); });
    for (let k = 0; k < 4; k++) b.mk({ far: 1, lie: 2, scene: { green: 0, ring: R8(k * 2, 0x11, 0x11) }, g: { b19: 0, flags: 0 }, gl: { lvl: 0 } });
    for (let k = 0; k < 8; k++) b.mk({ far: 2 + (k & 1), lie: 2, scene: { green: 0, ring: R8(k, 0x11, 2), water: 0.3 }, g: { b19: 0, flags: 0 }, gl: { lvl: 0 } });
    [1, 2].forEach(function (eb) { b.mk({ far: 2, lie: 2, lieFlags: 0x800, emitBump: eb, g: { b19: 0, flags: 0, thought: 0 } }); });
    [[2, 0, 0, 0], [2, 0x20, 0, 0], [0, 0, 0, 2], [0, 0, 1, 0], [0, 0, 0, 0]].forEach(function (v) {
      b.mk({ far: 2, lie: 2, aim: 1, parity: true, emitBump: v[0], p18: v[1], p84: v[2], ptype: v[3], scene: Object.assign({ hole: 1 }, quiet),
        g: { b19: 0, flags: 0, b22: 3, b18: 0, type: 0 }, gl: { lvl: 0 } });
    });
    // coverage pass 3: putts from beside the cup with the smallest accuracy roll; object layouts 2 and 3; +0x22 tied to g
    // with a hole above 1
    const lowErr = { scene: { holeByte: 0x80 }, gl: { e0: 2 }, g: { b36: 0, w16: 0, b19: 0, flags: 0 } };
    [[150, 0, 1], [0, 200, 1], [-180, 60, 1], [120, -150, 1], [210, 0, 0], [400, 0, 1], [-420, 100, 0]].forEach(function (q) {
      b.mk(Object.assign({}, lowErr, { far: 0, near: [q[0], q[1]], gl: { e0: 2, gf: q[2] ? 0x200000 : 0 } }));
    });
    // controls pass: putts with the 20-point accuracy roll (0x005a47e0 bit 2 clear) at graded distances
    for (let k = 0; k < 10; k++) b.mk({ far: 0, near: [100 + 110 * k, 40], gl: { e0: 0, gf: k & 1 ? 0x200000 : 0 }, g: { b36: 3, w16: 0, b19: 0, flags: 0 } });
    [2, 3].forEach(function (v) { for (let k = 0; k < 6; k++) b.mk({ far: 2 + (k % 4), lie: 2, scene: { objField: v }, g: { b19: 0, flags: 0, ox: (k - 3) * 300 } }); });
    [0, 6, 0, 6].forEach(function (hr, k) { b.mk({ far: 2, lie: 2, b22g: true, gsel: 2 + k, scene: Object.assign({ hrec: hr }, quiet), g: { b19: 0, flags: 0 }, gl: { lvl: 1 } }); });
    [6, 0, 6, 0].forEach(function (hr, k) { b.mk({ far: 2, lie: 2, gsel: 2 + k, scene: Object.assign({ hrec: hr }, quiet), g: { b19: 0, flags: 0, b22: 1 }, gl: { lvl: 2 } }); });
    return b.rows;
  }
  // mode != 0 (x = -1): aim from [0x0058f330], the 0x0058f330 == 3 / 4 arms
  function ssmRows() {
    const b = ssRows('mode');
    for (let k = 0; k < 20; k++) b.mk({ far: 1 + (k % 12) });
    [0, 1, 2, 3, 4].forEach(function (m) { for (let k = 0; k < 4; k++) b.mk({ far: 2 + k * 3, gl: { m330: m } }); });
    for (let k = 0; k < 6; k++) b.mk({ far: 1, lie: 1, gl: { m330: k % 5 } });
    for (let k = 0; k < 6; k++) b.mk({ far: 6, scene: { water: 0.35 }, gl: { m330: 4 }, g: { b19: 4 } });
    // targeted (coverage pass 2)
    for (let k = 0; k < 4; k++) b.mk({ far: 1 + (k & 1), gl: { m330: 3 } });
    ['g', 'g1', 'g', 'g1', 'g', 'g1'].forEach(function (m, k) { b.mk({ mode: m, far: 1 + (k % 3), gl: { lvl: 0 }, g: { b19: k & 1 ? 7 : 0 } }); });
    [2, 4].forEach(function (lie) { [4, 8].forEach(function (a) { b.mk({ far: 5, lie: lie, gl: { a7140: a } }); }); });
    [3, 5, 7].forEach(function (g) { b.mk({ mode: 'g1', gsel: g, far: 1, gl: { lvl: 0 }, g: { b19: 0 } }); });
    // coverage pass 4: a target with a penalty (ce4 set) from a fairway / rough lie; chip-outs (m330 4) with +0x22 0 and 3;
    // the record's own far target tile (0, 0)
    [2, 4].forEach(function (lie) { b.mk({ far: 2, lie: lie, holeTile: 4, gl: { ce4: 1, m330: 1 }, g: { b19: 0, flags: 0 } }); });
    [0, 3, 0, 3].forEach(function (b22, k) { b.mk({ far: 4 + k, lie: 2, scene: { water: 0.3, rough: 0.5 }, gl: { m330: 4 }, g: { b19: 4, b22: b22, flags: 0 } }); });
    for (let k = 0; k < 6; k++) b.mk({ far: 1 + k, g: { tx: 0, ty: 0 } });
    return b.rows;
  }
  // simulateBallFlight called directly (g): the records as simulateShot leaves them before its call
  function sbfRows(overlay) {
    const b = ssRows('real');
    const n = overlay ? 6 : 30;
    for (let k = 0; k < n; k++) {
      const v = [{}, { water: 0.3 }, { flat: true }, { block: 0.2 }, { misc: 0.5 }, { rough: 0.6 }][k % 6];
      b.mk({ far: 4 + (k % 9), scene: v, s0: [0, 1, 0x55][k % 3],
        g: { b19: [4, 6, 0, 5, 7][k % 5], flags: [0, 1, 0, 0x100][k % 4] },
        gl: overlay ? { cc4: 0 } : { cc4: [0, 0, 1, 2, 0][k % 5] } });
    }
    if (!overlay) {
      [[0, 0], [1, 1], [3, 3], [5, 4]].forEach(function (h) { b.mk({ far: 4, scene: { hx: h[0], hy: h[1], green: 1 } }); });
      for (let k = 0; k < 3; k++) b.mk({ far: 6 + k, scene: { greens: 0.08 } });
      for (let k = 0; k < 3; k++) b.mk({ far: 6 + k, scene: { flagRing: true }, g: { b19: 4, b22: 0 } });
      for (let k = 0; k < 2; k++) b.mk({ far: 9, scene: { block: 0.97 } });
      for (let k = 0; k < 6; k++) b.mk({ far: 3 + (k % 3), scene: { flagRing: true, green: 0 }, g: { b19: 4, b22: 0, flags: 0 } });
    }
    if (overlay) {
      // controls pass: the drawn scores show the divisor (holes near the map corner give 2 and 4) and the strict flag (pass 4)
      [[0, 0], [1, 1], [3, 3], [5, 4]].forEach(function (h, k) { b.mk({ far: 4, s0: k & 1 ? 0 : 1, scene: { hx: h[0], hy: h[1], green: 1 }, g: { b19: 4, flags: 0 }, gl: { cc4: 0 } }); });
      for (let k = 0; k < 4; k++) b.mk({ far: 5 + k, s0: [0, 1, 0x55, 7][k], scene: { flagRing: k & 1, green: 0 }, g: { b19: 4, b22: 0, flags: 0 }, gl: { cc4: 0 } });
    }
    if (overlay) b.rows.forEach(function (r) { const on = r.on; r.on = function (fx, rr) { on(fx, rr); wr32(0x004c2fc8, r.a); }; });
    return b.rows;
  }

  F.c3ea_ts = function () { const fx = base({}); rowsOn(fx, tsRows()); return fx; };
  F.c3ea_sst = function () { const fx = base({}); rowsOn(fx, sstRows()); return fx; };
  F.c3ea_ssr = function () { const fx = base({ chkstk: true }); rowsOn(fx, ssrRows()); return fx; };
  F.c3ea_ssm = function () { const fx = base({ chkstk: true }); rowsOn(fx, ssmRows()); return fx; };
  F.c3ea_sbf = function () { const fx = base({ chkstk: true }); rowsOn(fx, sbfRows(false)); return fx; };
  F.c3ea_sbfo = function () { const fx = base({ chkstk: true, clogCap: 0x4000 }); rowsOn(fx, sbfRows(true)); return fx; };
  // row counts for the registry (log/c3/alt/c3ea_rowcount.js prints them)
  F.c3ea_counts = function () {
    return { ts: tsRows().length, sst: sstRows().length, ssr: ssrRows().length, ssm: ssmRows().length, sbf: sbfRows(false).length, sbfo: sbfRows(true).length };
  };

  // ====================================================================================================================
  // saveBmp 0x00475580 (thiscall: holder, path, rect). The holder's surface (+4) and the live display [0x0083ad50] answer
  // from native stubs `mov eax, [cfg + k]; ret` (A1 <abs32> C3): surface slots +0x20 pixels, +0xd8 row length, +0xdc height;
  // display slot +0xb4 the pixel format. operator new 0x004a57f1 / delete 0x004a4ffc go through heapstandin.js (arena and
  // free log are state). fopen 0x004a614d / fwrite 0x004a6268 / fclose 0x004a609f are stood in: fopen logs the path and
  // mode and answers the fake FILE or NULL; fwrite appends size * n bytes to fx.file (cursor +0, bytes from +4, state) and
  // answers the row's count for that write (n, or a short count); fclose logs.
  // ====================================================================================================================
  function retStub(cell) {
    const p = __keepAlloc(16);
    const bytes = [0xa1].concat(u32(cell.toUInt32())).concat([0xc3]);
    Memory.patchCode(p, bytes.length, function (code) { code.writeByteArray(bytes); });
    return p;
  }
  function bmpRows() {
    const rows = [];
    const add = function (o) { rows.push(o); };
    // rect NULL: the surface's own size (row length = width), formats 1 and 0
    [[4, 3], [1, 1], [7, 5], [5, 2], [3, 6], [8, 1], [2, 9]].forEach(function (wh, k) { add({ w: wh[0], h: wh[1], pitch: wh[0], fmt: k & 1 ? 0 : 1, rect: null }); });
    // a rect inside a wider surface, with negative and odd heights
    [[1, 2, 5, 6], [0, 0, 3, 3], [2, 1, 7, 2], [3, 4, 4, 9], [1, 5, 6, 2], [0, 3, 8, 0], [2, 2, 2, 7], [4, 0, 9, 5]].forEach(function (r, k) {
      add({ w: 10, h: 10, pitch: 12, fmt: [1, 0, 2, 1][k % 4], rect: r });
    });
    // NULL surface without a rect: width and height 0, nothing read
    add({ nosurf: true, fmt: 1, rect: null });
    add({ nosurf: true, fmt: 0, rect: null });
    // fopen fails; each of the three writes short (by one, or 0)
    add({ w: 3, h: 2, pitch: 3, fmt: 1, rect: null, nofile: true });
    add({ w: 6, h: 3, pitch: 8, fmt: 0, rect: [1, 0, 5, 3], nofile: true });
    [0, 1, 2].forEach(function (which) {
      [1, 'all'].forEach(function (cut) { add({ w: 5, h: 4, pitch: 5, fmt: 1, rect: null, shortAt: which, cut: cut }); });
    });
    // width 0 / height 0 rects (empty image)
    add({ w: 10, h: 10, pitch: 10, fmt: 1, rect: [3, 3, 3, 8] });
    add({ w: 10, h: 10, pitch: 10, fmt: 0, rect: [2, 4, 7, 4] });
    return rows;
  }
  F.c3ea_bmp = function () {
    freezeNow();
    const fx = { clog: newClog(0x2000), clogCap: 0x2000 };
    const cfg = alloc(0x20);                       // +0 pixels, +4 row length, +8 height, +0xc format
    const svt = alloc(0x100), dvt = alloc(0x100);
    svt.add(0x20).writePointer(retStub(cfg)); svt.add(0xd8).writePointer(retStub(cfg.add(4))); svt.add(0xdc).writePointer(retStub(cfg.add(8)));
    dvt.add(0xb4).writePointer(retStub(cfg.add(0xc)));
    const surf = alloc(0x40); surf.writePointer(svt);
    const disp = alloc(0x40); disp.writePointer(dvt);
    const holder = alloc(0x10);
    fx.holder = holder;
    fx.file = alloc(0x1000);
    fx.pix = alloc(0x400);
    fx.path = alloc(0x40);
    const p = 'c3ea_test.bmp';
    for (let k = 0; k < p.length; k++) fx.path.add(k).writeU8(p.charCodeAt(k));
    fx.rect = alloc(0x10);
    fx.fakeFile = alloc(0x40);
    __freeze.save(ptr(0x0083ad50), 4);
    ptr(0x0083ad50).writePointer(disp);
    const h = __heapStandIn({ module: 'golf_clean.exe', alloc: [0xa57f1], free: [0xa4ffc], arenaSize: 0x4000 });
    fx.arena = h.arena; fx.freelog = h.log;
    const rows = bmpRows();
    calleeStand(fx, 0, 0x004a614d, 2, function () { return cur.nofile ? 0 : fx.fakeFile.toUInt32(); }, { str: { 0: 8, 1: 1 } });
    calleeStand(fx, 1, 0x004a6268, 4, function (n, a) {
      const total = (a[1] >>> 0) * (a[2] >>> 0);
      const k = n;                                 // this stub's call count, kept in the log (state)
      let cnt = a[2] >>> 0;
      if (cur.shortAt === k) cnt = cur.cut === 'all' ? 0 : cnt - 1;
      const c = fx.file.readU32();
      const take = Math.min(total, 0xff0 - c);
      if (take > 0) fx.file.add(4 + c).writeByteArray(ptr(a[0]).readByteArray(take));
      fx.file.writeU32(c + Math.max(take, 0));
      return cnt >>> 0;
    }, { mask: [0, 3] });
    calleeStand(fx, 2, 0x004a609f, 1, function () { return 0; }, { mask: [0] });
    rows.forEach(function (r, i) {
      Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () {
        cur = r;
        holder.add(4).writePointer(r.nosurf ? ptr(0) : surf);
        cfg.writePointer(r.nosurf ? ptr(0) : fx.pix);
        cfg.add(4).writeU32(r.pitch || 0); cfg.add(8).writeU32(r.h || 0); cfg.add(0xc).writeU32(r.fmt);
        for (let q = 0; q < 0x200; q++) fx.pix.add(2 * q).writeU16((Math.imul(q + 1, 0x9e37) ^ (i * 0x1111)) & 0xffff);
        if (r.rect) for (let q = 0; q < 4; q++) fx.rect.add(4 * q).writeS32(r.rect[q]);
        return holder;
      } });
      Object.defineProperty(fx, 'p' + i, { enumerable: true, configurable: true, get: function () { return fx.path; } });
      Object.defineProperty(fx, 'q' + i, { enumerable: true, configurable: true, get: function () { return r.rect ? fx.rect : ptr(0); } });
    });
    return fx;
  };
  F.c3ea_counts_bmp = function () { return bmpRows().length; };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
