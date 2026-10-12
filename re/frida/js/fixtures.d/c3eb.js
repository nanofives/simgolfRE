// Fixtures for batch c3eb (simulation plan phase 3, 2026-10-11): placeAnimal 0x004017d0, pathStep 0x0042e7e0 and
// updateWalkers 0x00402a40. Reimplementations: shim/src/re/c3eb.cpp. Registry: re/frida/registry.d/c3eb.py. Purposes and
// per-jcc tables: log/c3/c3eb_purpose.md.
//
// Method (copied from c3dy.js / c3dz.js): every key freezes the game's first thread (re/frida/js/freeze.js); every live
// global a row writes is saved first and restored by the flush. Callees that draw, play sound, pop text up or are random are
// replaced for ONE key on the A/B thread only (calleeStand); every call is appended to the call log fx.clog (compared state)
// and answered from the row. Rows: diff_hook resolves "$rN" before the state snapshot, so the getter of fx.rN applies row N.
(function () {
  const F = {};
  function alloc(n) {
    const p = __keepAlloc(n);
    for (let i = 0; i < n; i += 4) p.add(i).writeU32(0);
    return p;
  }
  // The negative-control build keeps its wrong bodies dormant until its exported NegCtl switch is set (memory note negctl-wrong-bodies).
  function negOn() {
    __freeze.negOn();   // every exported <Batch>NegCtl switch (re/frida/js/freeze.js)
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
  function rd32(a) { return ptr(a).readU32(); }
  function fillMem(a, n, b) { const arr = new Uint8Array(n); arr.fill(b & 0xff); ptr(a).writeByteArray(arr.buffer); }
  function saveRange(a, n) { __freeze.save(ptr(a), n); }
  function lcg(seed) {
    let s = seed >>> 0;
    return function (n) { s = (Math.imul(s, 1103515245) + 12345) >>> 0; return n ? ((s >>> 8) % n) : (s >>> 8); };
  }

  // ---- rows: fx.r<i> applies row i (row.on) and returns row.a; fx.b<i>.. are the further arguments
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

  // ---- a function of the exe replaced for one key on the A/B thread only (c3dy/c3dz calleeStand). Log layout: word 0 =
  // cursor, word 1 + k = call count of stub k (16 stubs), entries {va, count, values...} from +0x50. opts.mask: argument
  // indexes logged as 0 (stack pointers that differ between the arms).
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x4000;
  function newClog(cap) { const l = alloc(cap || CLOG); l.writeU32(0x50); return l; }
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
    const fn = function () {
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, arguments);
      const a = Array.prototype.slice.call(arguments);
      const cell = log.add(4 + 4 * k);
      const n = cell.readU32();
      cell.writeU32(n + 1);
      const vals = a.map(function (v, ix) { return (opts.mask && opts.mask.indexOf(ix) >= 0) ? 0 : v; });
      if (opts.str) Object.keys(opts.str).forEach(function (ix) {      // the C string an argument points to, zero padded words
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
      if (opts.extra) opts.extra(a).forEach(function (x) { vals.push(x >>> 0); });   // words read from memory at call time
      const curs = log.readU32();
      const need = 8 + 4 * vals.length;
      if (curs + need <= (fx.clogCap || CLOG)) {
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
  // Random::range(this = 0x00822d9c, n) as a stand-in: the row's rng sequence by call number (cur.rng), else a default that
  // is non-zero as a word. The call log records (this, n), so a wrong n or a wrong call count reads RED.
  function rngStand(fx, k) {
    calleeStand(fx, k, 0x0045c1e0, 2, function (n, a) {
      if (cur.rng && n < cur.rng.length) return cur.rng[n] >>> 0;
      return cur.rngDef === undefined ? 9 : cur.rngDef;
    }, { abi: 'thiscall' });
  }

  const FLAGS = 0x0059e7b8, DATE = 0x00834170, SEL = 0x005a59f8, ABORT = 0x0083afcc;
  // read-modify-write of the shared flag word: only the bits in `mask` are replaced by `bits`
  function setFlags(mask, bits) { wr32(FLAGS, ((rd32(FLAGS) & ~mask) | (bits & mask)) >>> 0); }

  // ====================================================================================================================
  // placeAnimal 0x004017d0 (idx)
  // ====================================================================================================================
  const ANI = 0x0056d1d8, ANI_N = 100 * 0x3c;
  const ENT = 0x0058bcb8, ENT_N = 0x1000;
  const FRM = 0x0053f3e8, FRM_N = 0x800 * 4;
  const MAPC = 0x004e6d70, MAPC_N = 0x74 * 4;
  const PBUF = 0x0051a068, PBUF_N = 0x40, IBUF = 0x004e9a70, IBUF_N = 0x20;
  const ROT = 0x005685f4, CELLOUT = 0x004e9a80;

  function animalRows() {
    const rows = [];
    const base = { idx: 7, m0: 3, m2: 1, m6: 10, m8: 12, ma: 0, mc: 0x140, p10: 0x123, p14: 0x2c5, hd: 2, st: 0, fr: 1,
                   scr: 1, sx: 321, sy: 222, cells: 4, rot: 5, flags: 0, rng: [], tbl: [], cellv: [], cellRet: 0x7777 };
    const add = function (o) { rows.push(Object.assign({}, base, o)); };
    // on-screen drawing arm (ma < 0x80)
    add({});                                                                     // plain draw, frame advances
    add({ idx: 0, tbl: [{ i: 3, t: 5, x: 10, y: 12, c: 4 }] });                   // one table match: n = 3
    add({ idx: 99, tbl: [{ i: 3, t: 5, x: 10, y: 12, c: 4 }, { i: 200, t: 5, x: 10, y: 12, c: 9 }] });   // last match wins
    add({ tbl: [{ i: 255, t: 5, x: 10, y: 12, c: 6 }, { i: 0, t: 5, x: 10, y: 12, c: 2 }] });             // first and last slots
    add({ tbl: [{ i: 5, t: 4, x: 10, y: 12, c: 4 }] });                            // wrong type
    add({ tbl: [{ i: 5, t: 5, x: 10, y: 12, c: 0 }] });                            // count 0: not taken
    add({ tbl: [{ i: 5, t: 5, x: 10, y: 12, c: -3 }] });                           // count negative: not taken
    add({ tbl: [{ i: 5, t: 5, x: 11, y: 12, c: 4 }] });                            // x differs
    add({ tbl: [{ i: 5, t: 5, x: 10, y: 13, c: 4 }] });                            // y differs
    add({ tbl: [{ i: 5, t: 5, x: 10, y: 12, c: 1 }] });                            // n = 0
    add({ tbl: [{ i: 9, t: 5, x: 10, y: 12, c: 0x41 }] });                         // n = 0x40
    add({ tbl: [{ i: 3, t: 5, x: 10, y: 12, c: 5 }, { i: 9, t: 5, x: 10, y: 12, c: 1 }] });   // a later count of 1 sets n back to 0
    add({ tbl: [{ i: 3, t: 5, x: 10, y: 12, c: 5 }, { i: 9, t: 5, x: 10, y: 12, c: 0 }] });   // a later count of 0 leaves n = 4
    for (let r = 0; r < 8; r++) add({ rot: r, hd: (r * 3) & 7 });                  // heading (rot - hd - 2) & 7
    add({ rot: 2, hd: 0xfe, fr: 1 });                                              // heading byte above 0x7f
    add({ rot: 0x1235, hd: 9 });                                                   // large rotation word
    add({ fr: 0, cells: 4 });                                                      // frame 0
    add({ fr: 3, cells: 4 });                                                      // last frame: wraps to 0
    add({ fr: 2, cells: 4 });                                                      // middle frame
    add({ fr: 9, cells: 4 });                                                      // frame above the count: remainder 1
    add({ fr: -3, cells: 4 });                                                     // negative frame: remainder -3
    add({ fr: -128, cells: 5 });                                                   // signed byte minimum
    add({ fr: 6, cells: 1 });                                                      // one cell: remainder 0, wraps
    add({ fr: 126, cells: 127 });                                                  // frame + 1 = 127 < 127 false
    add({ fr: 125, cells: 127 });                                                  // frame + 1 = 126 < 127
    add({ fr: 3, cells: 4, flags: 4 });                                            // paused: frame stays (remainder only)
    add({ fr: 9, cells: 4, flags: 4 });                                            // paused, remainder written
    add({ p10: 0x40000000, p14: -0x40000000 });                                    // (x << 1) overflow
    add({ p10: -1, p14: -5000 });                                                  // negative sub-tile values
    add({ p10: 0, p14: 0 });
    add({ p10: 0x7fffffff, p14: 0x1ff });
    add({ p10: 5000, p14: 777777 });
    add({ m0: -2, mc: 0x140, cells: 6 });                                          // sprite index from a negative type
    add({ m0: 7, mc: 0x15a, cells: 3, m19: 0 });
    add({ m0: 7, mc: 0x14d, cells: 5 });
    add({ m0: 0, mc: 0, cells: 2 });
    add({ m6: -4, m8: 600 });                                                      // negative / large tile words
    add({ m6: 0x7fff, m8: -0x8000, tbl: [{ i: 5, t: 5, x: 0x7fff, y: -0x8000, c: 5 }] });
    add({ m2: 3, m6: 200 });
    add({ ma: 0x7f });                                                             // wait 0x7f: still draws, then counts down
    add({ ma: 0x7f, flags: 4 });                                                   // paused: wait stays
    add({ ma: 0x80 });                                                             // wait 0x80: no draw, still counts
    add({ ma: 0x80, flags: 4 });
    add({ ma: 0x1f4 });
    add({ ma: -1 });                                                               // 0xffff is below 0x80 (signed): draws, wait --
    add({ ma: 1, flags: 4 });
    add({ ma: 1, scr: 0 });                                                         // off screen with a wait
    add({ ma: 1, scr: 0, flags: 4 });
    // off-screen arm: the 1-in-500 roll
    add({ scr: 0, rng: [0] });                                                      // roll 0: wait 500
    add({ scr: 0, rng: [1] });
    add({ scr: 0, rng: [0x10000] });                                                // low word zero: wait 500 too
    add({ scr: 0, rng: [0x10001] });
    add({ scr: 0, rng: [0], flags: 4 });                                            // paused: no roll
    add({ scr: 0, rng: [0], flags: 4, ma: 3 });
    add({ scr: 0, rng: [1], ma: 0, st: 0, fr: 0, mc: 0x141 });
    // wait == 0: state reset when the frame is 0
    add({ ma: 0, fr: 0, mc: 0x141, rng: [], scr: 0 });                              // frame 0, other sprite: 0x140 and a wait roll
    add({ ma: 0, fr: 0, mc: 0x141, scr: 0, rng: [1, 0x30] });
    add({ ma: 0, fr: 0, mc: 0x15a, scr: 0, rng: [1] });                             // 0x15a: kept
    add({ ma: 0, fr: 0, mc: 0x140, scr: 0, rng: [1] });                             // 0x140: kept
    add({ ma: 0, fr: 0, mc: 0x141, scr: 0, rng: [1, 0x1ffff] });                    // roll with a high word
    add({ ma: 0, fr: 1, mc: 0x141, scr: 0, rng: [1] });                             // frame != 0: kept
    add({ ma: 0, fr: -1, mc: 0x141, scr: 0, rng: [1] });
    // wait == 0 and idle (step 0): the move roll
    // an off-screen row spends its first Random::range call on the 1-in-500 roll (unless paused), so the scripted values start
    // with a non-zero roll
    const idle = function (o) {
      const q = Object.assign({ scr: 0, ma: 0, fr: 2, mc: 0x140, st: 0, cells: 4 }, o);
      if (!(q.flags & 4)) q.rng = [1].concat(q.rng || []);
      add(q);
    };
    idle({ rng: [1] });                                                            // roll 8 != 0: nothing, returns
    idle({ rng: [0x10000] });                                                      // low word zero: moves on
    idle({ mc: 0x141, rng: [0] });                                                 // sprite not 0x140: nothing
    idle({ mc: 0x15a, rng: [0] });
    // move roll hits: heading from the roll, bounds, map cell
    const tiles = [[0, 0], [3, 3], [1, 2], [2, 1], [0, 3], [3, 0]];
    tiles.forEach(function (t) {
      for (let h = 0; h < 8; h++) {
        const p10 = t[0] * 512 + 100, p14 = t[1] * 512 + 100;
        idle({ p10: p10, p14: p14, hd: 0, rng: [0, h + 1, 5], cellv: [{ m: 1, x: 1, y: 1, v: 0xff }, { m: 1, x: 2, y: 1, v: 0xff }, { m: 1, x: 1, y: 2, v: 0xff },
                                                                      { m: 1, x: 0, y: 2, v: 0xff }, { m: 1, x: 3, y: 2, v: 0xff }, { m: 1, x: 2, y: 3, v: 0xff }] });
      }
    });
    idle({ p10: 100, p14: 100, hd: 0, rng: [0, 1, 5], m2: 1, cellv: [{ m: 1, x: 0, y: 0, v: 0xfe }] });   // in bounds, cell not 0xff
    idle({ p10: 100, p14: 100, hd: 0, rng: [0, 1, 5], m2: 2, cellv: [{ m: 2, x: 0, y: 0, v: 0xff }] });   // other map
    idle({ p10: 100, p14: 100, hd: 0, rng: [0, 1, 5], m2: 2, cellv: [{ m: 1, x: 0, y: 0, v: 0xff }] });   // map index matters
    idle({ p10: 100, p14: 100, hd: 0, rng: [0, 1, 5], m2: 0, cellv: [{ m: 0, x: 0, y: 0, v: 0xff }] });
    idle({ p10: 100, p14: 100, hd: 5, rng: [0, 0x103, 5] });                       // heading adds the low byte of the roll only
    idle({ p10: 100, p14: 100, hd: 0xf9, rng: [0, 3, 5] });                        // heading byte above 7 is masked
    idle({ p10: 100, p14: 100, hd: 7, rng: [0, 0x10000, 5] });                     // roll with only the high word: low byte 0
    idle({ p10: 1500, p14: 100, hd: 0, rng: [0, 3, 0] });                          // out right (tile 2.9 -> 2): in bounds
    idle({ p10: 2047, p14: 100, hd: 0, rng: [0, 3, 0], cellv: [{ m: 1, x: 3, y: 0, v: 0xff }] });
    // out of bounds: the sound roll (limit 0x18, or 0x40 with the 0x200000 flag)
    const oob = function (o) { idle(Object.assign({ p10: 100, p14: 100, hd: 0, rng: [0, 1, 0] }, o)); };   // heading 0 -> dy = -1: out
    oob({});                                                                       // roll 0: sound, state 0x14d, frame 0
    oob({ rng: [0, 1, 1] });                                                       // roll 1: nothing
    oob({ rng: [0, 1, 0x10000] });                                                 // low word zero: sound
    oob({ flags: 0x200000 });                                                      // limit 0x40
    oob({ flags: 0x200000, rng: [0, 1, 7] });
    oob({ flags: 0x200000 | 4, rng: [0, 1, 0] });
    oob({ m0: 9, p10: 8000, p14: 6000, hd: 2, rng: [0, 2, 0] });                   // far positions
    oob({ m0: -5, rng: [0, 1, 0] });                                               // negative type: sound number 0x82 + type
    // moving (step counter != 0)
    const mov = function (o) { add(Object.assign({ scr: 0, ma: 0, fr: 2, mc: 0x140, st: 5, p10: 300, p14: 400 }, o)); };
    for (let h = 0; h < 8; h++) mov({ hd: h });
    mov({ st: 1 });                                                                // 1 -> 0: back to 0x140
    mov({ st: 2 });                                                                // 2 -> 1: stays 0x15a
    mov({ st: 0x10 });
    mov({ st: 0x7f });
    mov({ st: 0x80 });                                                             // 0x80 - 1 = 0x7f? (signed byte -128 - 1 wraps to 127)
    mov({ st: 0xff });                                                             // -1 -> -2: <= 0
    mov({ st: 0x81, hd: 3 });
    mov({ st: 3, scr: 1, ma: 0x90 });                                               // on screen, no draw (wait 0x90 > ... but not zero)
    mov({ st: 4, mc: 0x15a, fr: 0, hd: 6 });
    mov({ st: 4, mc: 0x141, fr: 0, hd: 6 });                                       // frame 0 reset first, then the step
    mov({ st: 4, p10: -999, p14: 999999, hd: 1 });
    mov({ st: 6, flags: 4, hd: 4 });                                               // paused flag does not stop the step arm
    // index and sprite variety
    [0, 1, 2, 17, 50, 98, 99].forEach(function (ix) { add({ idx: ix }); });
    [0, 33, 99].forEach(function (ix) { mov({ idx: ix, hd: ix & 7 }); });
    add({ idx: 42, scr: 1, cells: 7, fr: 6, m0: 11, mc: 0x15a });
    return rows;
  }

  function animalScene() {
    const fx = { clog: newClog(0x400), clogCap: 0x400 };
    freezeNow();
    saveRange(ANI, ANI_N); saveRange(ENT, ENT_N); saveRange(FRM, FRM_N); saveRange(MAPC, MAPC_N);
    saveRange(PBUF, PBUF_N); saveRange(IBUF, IBUF_N); saveRange(ROT, 4); saveRange(CELLOUT, 4); saveRange(FLAGS, 4);
    const rows = animalRows();
    fx.n = rows.length;
    rowsOn(fx, rows.map(function (r) {
      return Object.assign({}, r, { a: r.idx, on: function () { animalOn(r); } });
    }));
    // worldToScreen(wx, wy, &sx, &sy, margin): answers the row's `on` and writes the row's screen position
    calleeStand(fx, 0, 0x0042fb90, 5, function (n, a) {
      if (cur.scr) { ptr(a[2]).writeS32(cur.sx); ptr(a[3]).writeS32(cur.sy); }
      return cur.scr;
    }, { mask: [2, 3] });
    calleeStand(fx, 1, 0x0043d6f0, 3, function (n, a) { return (cur.cellRet + n) >>> 0; });
    calleeStand(fx, 2, 0x00462a30, 8, 0);
    calleeStand(fx, 3, 0x0040c500, 4, 0);
    rngStand(fx, 4);
    return fx;
  }
  function animalOn(r) {
    // the table: every record gets a pattern, then ours
    for (let i = 0; i < 100; i++) {
      for (let q = 0; q < 0x3c; q += 4) wr32(ANI + i * 0x3c + q, (0x10101010 * ((i % 13) + 1) + q) >>> 0);
      wr16(ANI + i * 0x3c, 0x30 + (i & 7));                                      // m_0
    }
    const a = ANI + r.idx * 0x3c;
    wr16(a + 0, r.m0); wr16(a + 2, r.m2); wr16(a + 6, r.m6); wr16(a + 8, r.m8); wr16(a + 0xa, r.ma); wr16(a + 0xc, r.mc);
    wr32(a + 0x10, r.p10); wr32(a + 0x14, r.p14); wr8(a + 0x18, r.hd); wr8(a + 0x19, r.m19 === undefined ? r.st : r.m19); wr8(a + 0x1a, r.fr);
    // sprite frames: junk everywhere, then the row's count at the sprite index
    for (let i = 0; i < 0x800; i++) wr32(FRM + 4 * i, 0x1000 + i);
    const sprite = (r.m0 + r.mc) | 0;
    if (sprite < 0 || sprite >= 0x800) throw new Error('c3eb_pa: sprite index out of range ' + sprite);
    wr32(FRM + 4 * sprite, r.cells);
    // placed entries: 256 x 16 bytes with type 1 everywhere, then the row's
    for (let i = 0; i < 256; i++) { wr16(ENT + 16 * i, 1); wr16(ENT + 16 * i + 2, 10); wr16(ENT + 16 * i + 4, 12); wr32(ENT + 16 * i + 8, 7); }
    r.tbl.forEach(function (e) { wr16(ENT + 16 * e.i, e.t); wr16(ENT + 16 * e.i + 2, e.x); wr16(ENT + 16 * e.i + 4, e.y); wr32(ENT + 16 * e.i + 8, e.c); });
    // the map cells
    fillMem(MAPC, MAPC_N, 0x07);
    r.cellv.forEach(function (c) { wr8(MAPC + c.m * 0x74 + c.x * 6 + c.y, c.v); });
    wr32(ROT, r.rot);
    fillMem(PBUF, PBUF_N, 0x55); wr8(PBUF, 0x70);
    fillMem(IBUF, IBUF_N, 0x66);
    wr32(CELLOUT, 0xdddd0001);
    setFlags(0x200004, r.flags);
  }
  F.c3eb_pa = animalScene;

  // ====================================================================================================================
  // pathStep 0x0042e7e0 (p1, p2, p3, p4, p5)
  // ====================================================================================================================
  // Tile maps (50 x 50, index x * 50 + y): flags words 0x0053caf0, type bytes 0x005722e8, cost bytes 0x0053bbac. Rows describe a
  // scene by a seed, a tile mix and patches; the arrays are built in JS and written in one call each.
  const TF = 0x0053caf0, TT = 0x005722e8, TC = 0x0053bbac, GRID = 0x0058d370, QX = 0x005a7404, QY = 0x005a8028;
  const ATTRB = 0x00578370, ATTR_N = 0x30 * 0x18, GOLF = 0x005794b8, GOLF_N = 0x800, B58A = 0x0058a528;
  const MIX = {
    plain: { tt: [0, 1, 2, 3, 4, 5, 7, 2, 1], tf: [0, 0, 0x400, 0x800], cmax: 3 },
    mixed: { tt: [0, 1, 2, 3, 4, 5, 7, 0x11, 0x11, 0x14, 0x15, 2], tf: [0, 0x20, 0x400, 0x420, 0x800, 0x20, 0x20, 0], cmax: 4 },
    heavy: { tt: [0, 1, 2, 3, 4, 5, 7, 0x11, 0x14], tf: [0, 0x20, 0x400, 0x420, 0], cmax: 60 },
    flat: { tt: [2, 2, 2, 2, 1], tf: [0, 0, 0, 0x20], cmax: 0 },
    green: { tt: [0x11, 0x11, 0x11, 0x11, 0x14, 0x15, 2], tf: [0x20, 0x20, 0x20, 0x20, 0, 0x420, 0x400], cmax: 2 },
  };
  function rect(x0, y0, x1, y1, o) {
    return function (tt, tf, tc) {
      for (let x = x0; x <= x1; x++) for (let y = y0; y <= y1; y++) {
        if (x < 0 || y < 0 || x > 49 || y > 49) continue;
        const i = x * 50 + y;
        if (o.tt !== undefined) tt[i] = o.tt;
        if (o.tf !== undefined) { tf[2 * i] = o.tf & 0xff; tf[2 * i + 1] = o.tf >> 8; }
        if (o.tc !== undefined) tc[i] = o.tc;
      }
    };
  }
  function cell(x, y, o) { return rect(x, y, x, y, o); }
  const W = function (tx, ty) { return tx * 1024 + ty; };                      // helper for world values: tile * 1024 + offset
  function psRows() {
    const rows = [];
    const R = lcg(0xC3EB0001);
    const base = { seed: 1, mix: 'mixed', patches: [], fl: 0, date: 0, abort: 0, b9: 0x55, partner: [0x7000000, 0x7000000], me: [0x7100000, 0x7100000],
                   rng: [5], a: 0, b: 0, c: 0, d: 0, e: 0 };
    const add = function (o) { rows.push(Object.assign({}, base, o)); };
    const pos = function (tx, ty, ox, oy) { return [tx * 1024 + (ox === undefined ? 300 : ox), ty * 1024 + (oy === undefined ? 600 : oy)]; };
    const args = function (s, t, p5) {        // s = start (target) tile, t = walker tile
      const a = pos(s[0], s[1], s[2], s[3]), c = pos(t[0], t[1], t[2], t[3]);
      return { a: a[0], b: a[1], c: c[0], d: c[1], e: p5 === undefined ? 0 : p5 };
    };
    // ---- early returns
    add(Object.assign(args([5, 7], [5, 7]), { seed: 2 }));
    add({ a: W(5, 10), b: W(7, 20), c: W(5, 900), d: W(7, 1000), e: 1, seed: 3 });
    add({ a: -5000, b: 12345, c: 10, d: 12000, e: 2, seed: 4 });                  // clamped to the same tile (0, 11) vs (0, 11)
    add({ a: -5000, b: -70000, c: 10, d: 20, e: 3, seed: 5 });                    // both (0, 0) after the clamp
    add({ a: 90000, b: 90000, c: 51200, d: 52000, e: 0, seed: 6 });               // (49, 49) clamps
    add({ a: W(49, 1023), b: W(49, 5), c: W(49, 1000), d: W(49, 900), e: 5, seed: 7 });
    add(Object.assign(args([20, 20], [20, 20, 0, 0]), { partner: pos(20, 20), seed: 8 }));   // same tile and the partner on it
    // ---- the partner on the start tile (ox = -1) and next to it (cost + 2)
    [0, 1, 2, 3].forEach(function (p5) {
      add(Object.assign(args([10, 10], [20, 25], p5), { partner: pos(10, 10, 5, 900), seed: 10 + p5, mix: 'plain' }));
      add(Object.assign(args([10, 10], [20, 25], p5), { partner: pos(11, 11), seed: 14 + p5, mix: 'plain' }));
      add(Object.assign(args([10, 10], [20, 25], p5), { partner: pos(20, 24), seed: 18 + p5, mix: 'plain' }));
      add(Object.assign(args([10, 10], [20, 25], p5), { partner: pos(19, 25), seed: 22 + p5, mix: 'flat' }));
    });
    // ---- plain / mixed / flat scenes over distances and positions
    const spacing = [1, 2, 3, 4, 6, 9, 15, 30];
    for (let n = 0; n < 64; n++) {
      const s = [R(50), R(50)], dd = spacing[R(spacing.length)];
      const dxs = R(3) - 1, dys = R(3) - 1;
      const t = [Math.max(0, Math.min(49, s[0] + dd * dxs + (dxs ? 0 : R(2)))), Math.max(0, Math.min(49, s[1] + dd * dys + (dys ? 0 : R(2))))];
      const mix = ['plain', 'mixed', 'mixed', 'flat', 'green'][n % 5];
      add(Object.assign(args(s.concat([R(1024), R(1024)]), t.concat([R(1024), R(1024)]), R(4)), {
        seed: 100 + n, mix: mix, fl: [0, 0, 0x100, 0x100][R(4)], date: [0, 0x40, 0xff, 0x41][R(4)],
        b9: R(3) === 0 ? 0x13 : 0x55, partner: pos(s[0] + R(5) - 2, s[1] + R(5) - 2), me: pos(t[0], t[1], R(1024), R(1024)),
        rng: [R(0x30000)],
      }));
    }
    // ---- edges and corners
    [[0, 0], [49, 49], [0, 49], [49, 0], [0, 25], [49, 25], [25, 0], [25, 49]].forEach(function (t, i) {
      add(Object.assign(args([25, 25], t, i & 3), { seed: 300 + i, mix: i & 1 ? 'plain' : 'mixed', rng: [i] }));
      add(Object.assign(args(t, [25, 25], i & 3), { seed: 320 + i, mix: 'flat', rng: [i + 3] }));
    });
    // ---- the random start direction: all eight offsets on a flat map (the pick among equal candidates depends on it)
    for (let r = 0; r < 9; r++) add(Object.assign(args([30, 30], [34, 36]), { seed: 400, mix: 'flat', rng: [[r, 0x10000 + r, 0x1fff8 + r][r % 3]] }));
    for (let r = 0; r < 8; r++) add(Object.assign(args([30, 30], [34, 36]), { seed: 401, mix: 'plain', rng: [0x20000 + r] }));
    // ---- a walker on a green: the odd directions are skipped, the golfer's offset can redirect the pick
    const gOff = [0, 0x100, 0x101, -0x100, -0x101, 0x500, -0x500, 0x3ff, -0x3ff];
    for (let n = 0; n < 72; n++) {
      const t = [10 + R(30), 10 + R(30)], s = [t[0] + R(13) - 6, t[1] + R(13) - 6];
      const gdx = gOff[R(gOff.length)], gdy = gOff[R(gOff.length)];
      const me = [t[0] * 1024 + 0x200 + gdx, t[1] * 1024 + 0x200 + gdy];
      add(Object.assign(args(s, t, R(4)), { seed: 500 + n, mix: n & 1 ? 'green' : 'mixed', me: me, fl: 0, date: [0, 0x40][R(2)], rng: [R(0x40000)],
        patches: [rect(Math.max(0, t[0] - 3), Math.max(0, t[1] - 3), Math.min(49, t[0] + 3), Math.min(49, t[1] + 3), { tt: 0x11, tf: 0x20 }),
                  cell(t[0], t[1], { tt: 0x11, tf: 0x20 })] }));
    }
    // green walker with the 0x100 flag (no redirect), and with a non-green tile
    add(Object.assign(args([20, 20], [24, 24]), { seed: 600, fl: 0x100, patches: [rect(22, 22, 26, 26, { tt: 0x11, tf: 0x20 })], me: pos(24, 24, 0, 0) }));
    add(Object.assign(args([20, 20], [24, 24]), { seed: 601, fl: 0, patches: [cell(24, 24, { tt: 0x11, tf: 0 })], me: pos(24, 24, 0, 0) }));
    add(Object.assign(args([20, 20], [24, 24]), { seed: 602, fl: 0, patches: [cell(24, 24, { tt: 0x12, tf: 0x20 })], me: pos(24, 24, 0, 0) }));
    // ---- the type adjustment (flag 0x100 or the record byte 0x13), tile types 0, 1, 2, attr 7 and the rest
    for (let n = 0; n < 24; n++) {
      const t = [5 + R(40), 5 + R(40)], s = [t[0] + R(9) - 4, t[1] + R(9) - 4];
      add(Object.assign(args(s, t, R(4)), { seed: 700 + n, mix: ['mixed', 'plain', 'heavy'][n % 3], fl: n & 1 ? 0x100 : 0, b9: n & 1 ? 0x55 : 0x13,
        rng: [R(0x30000)], date: [0, 0x40][R(2)] }));
    }
    // ---- heavy costs (totals above 0xff are refused) with the larger limit
    for (let n = 0; n < 16; n++) {
      const s = [R(50), R(50)], t = [R(50), R(50)];
      add(Object.assign(args(s, t, R(4)), { seed: 800 + n, mix: 'heavy', fl: n & 1 ? 0x100 : 0, rng: [R(0x30000)] }));
    }
    // a corridor: 50 long, cost 5 per tile at flags 0x100 (limit 0xfa) reaches totals above 0xff
    add(Object.assign(args([0, 0], [49, 49]), { seed: 900, mix: 'flat', fl: 0x100, patches: [rect(0, 0, 49, 49, { tc: 9 })] }));
    add(Object.assign(args([0, 0], [49, 49]), { seed: 901, mix: 'flat', fl: 0, patches: [rect(0, 0, 49, 49, { tc: 9 })] }));
    add(Object.assign(args([0, 0], [49, 49]), { seed: 902, mix: 'flat', fl: 0x100, patches: [rect(0, 0, 49, 49, { tc: 40 })] }));
    add(Object.assign(args([0, 0], [3, 3]), { seed: 903, mix: 'flat', fl: 0x100, patches: [rect(0, 0, 49, 49, { tc: 120 })] }));
    add(Object.assign(args([25, 25], [26, 26]), { seed: 904, mix: 'flat', fl: 0, patches: [rect(0, 0, 49, 49, { tc: 127 })] }));
    // ---- cost 0x80 and above is negative (movsx): isolated cells away from the start only
    add(Object.assign(args([10, 10], [20, 20]), { seed: 910, mix: 'flat', patches: [cell(15, 15, { tc: 0xff }), cell(13, 18, { tc: 0xff }), cell(18, 13, { tc: 0xfe })] }));
    add(Object.assign(args([10, 10], [20, 20]), { seed: 911, mix: 'plain', patches: [cell(12, 13, { tc: 0xff }), cell(16, 16, { tc: 0xff }), cell(19, 19, { tc: 0xff })] }));
    // ---- tile type 0x14 around the walker (blocked), and the wave limited by a wall
    add(Object.assign(args([10, 10], [20, 20]), { seed: 920, mix: 'plain', patches: [rect(19, 19, 21, 21, { tt: 0x14 })] }));
    add(Object.assign(args([10, 10], [20, 20]), { seed: 921, mix: 'plain', patches: [rect(19, 19, 21, 21, { tt: 0x14 }), cell(20, 21, { tt: 2 }), cell(21, 20, { tt: 2 })] }));
    add(Object.assign(args([10, 10], [20, 20]), { seed: 922, mix: 'plain', patches: [rect(19, 19, 21, 21, { tt: 2, tc: 127 }), cell(20, 20, { tt: 2, tc: 0 })] }));
    // ---- the abort flag: the wave stops after the first cell
    [0, 1, 2, 3].forEach(function (n) {
      add(Object.assign(args([20, 20], [21 + (n & 1), 20 + (n >> 1)]), { seed: 930 + n, mix: 'plain', abort: 5, rng: [n] }));
      add(Object.assign(args([20, 20], [30, 30]), { seed: 934 + n, mix: 'plain', abort: 1 << n, rng: [n + 2] }));
    });
    // ---- the debug draw (flag 0x1000): small waves only (abort or heavy walls), the text and the call log are compared
    add(Object.assign(args([20, 20], [22, 22]), { seed: 940, mix: 'plain', fl: 0x1000, abort: 1 }));
    add(Object.assign(args([20, 20], [22, 22]), { seed: 941, mix: 'plain', fl: 0x1100, abort: 1 }));
    add(Object.assign(args([20, 20], [22, 22]), { seed: 942, mix: 'heavy', fl: 0x1000, patches: [rect(0, 0, 49, 49, { tc: 100 })] }));
    add(Object.assign(args([0, 0], [1, 1]), { seed: 943, mix: 'flat', fl: 0x1000, abort: 1 }));
    add(Object.assign(args([49, 49], [48, 48]), { seed: 944, mix: 'flat', fl: 0x1000, abort: 1 }));
    add(Object.assign(args([20, 20], [20, 22]), { seed: 945, mix: 'flat', fl: 0x1000, patches: [rect(0, 0, 49, 49, { tc: 126 })] }));
    // ---- date bit 6 (the distance threshold 2 or 3) with start and walker 1..5 tiles apart
    [1, 2, 3, 4, 5].forEach(function (dd) {
      [0, 0x40].forEach(function (dt) {
        add(Object.assign(args([20, 20], [20 + dd, 20 + dd]), { seed: 950 + dd, mix: 'green', date: dt, patches: [rect(15, 15, 30, 30, { tt: 0x11, tf: 0x20 })], rng: [dd] }));
        add(Object.assign(args([20, 20], [20 + dd, 20]), { seed: 960 + dd, mix: 'green', date: dt | 1, patches: [rect(15, 15, 30, 30, { tt: 0x11, tf: 0x20 })], rng: [dd + 1] }));
      });
    });
    return rows;
  }

  function psOn(r) {
    const g = lcg(r.seed >>> 0);
    const mix = MIX[r.mix];
    const tt = new Uint8Array(2500), tf = new Uint8Array(5000), tc = new Uint8Array(2500);
    for (let i = 0; i < 2500; i++) {
      tt[i] = mix.tt[g(mix.tt.length)];
      const f = mix.tf[g(mix.tf.length)];
      tf[2 * i] = f & 0xff; tf[2 * i + 1] = f >> 8;
      tc[i] = mix.cmax ? g(mix.cmax + 1) : 0;
    }
    r.patches.forEach(function (p) { p(tt, tf, tc); });
    ptr(TT).writeByteArray(tt.buffer); ptr(TF).writeByteArray(tf.buffer); ptr(TC).writeByteArray(tc.buffer);
    // type attributes: byte +6 of a 0x30 record is 7 for types 3, 7, 11, 15, 19
    for (let t = 0; t < 0x18; t++) { wr8(0x00578376 + 0x30 * t, (t & 3) === 3 ? 7 : (t & 3)); wr8(0x00578375 + 0x30 * t, t % 5); }
    // golfer records 0..7: junk positions far from the scene, the record byte, then the row's partner and golfer positions
    for (let k = 0; k < 8; k++) { wr32(GOLF + 0x100 * k, 0x6000000 + k); wr32(GOLF + 0x100 * k + 4, 0x6100000 + k); wr8(GOLF + 0x100 * k + 0x21, 0x55); }
    wr8(GOLF + 0x100 * r.e + 0x21, r.b9);
    wr32(GOLF + 0x100 * (r.e ^ 1), r.partner[0]); wr32(GOLF + 0x100 * (r.e ^ 1) + 4, r.partner[1]);
    wr32(GOLF + 0x100 * r.e, r.me[0]); wr32(GOLF + 0x100 * r.e + 4, r.me[1]);
    setFlags(0x1100 | 0x80000, r.fl);
    wr32(DATE, r.date); wr32(ABORT, r.abort);
    fillMem(GRID, 2500, 0x77); fillMem(QX, 0x800, 0x66); fillMem(QY, 0x800, 0x99);
    fillMem(PBUF, PBUF_N, 0x55); wr8(PBUF, 0x70); fillMem(B58A, 0x20, 0x66);
  }
  function pathScene() {
    const fx = { clog: newClog(0x2000), clogCap: 0x2000 };
    freezeNow();
    saveRange(TF, 5000); saveRange(TT, 2500); saveRange(TC, 2500); saveRange(ATTRB, ATTR_N); saveRange(GOLF, GOLF_N);
    saveRange(GRID, 2500); saveRange(QX, 0x800); saveRange(QY, 0x800); saveRange(PBUF, PBUF_N); saveRange(B58A, 0x20);
    saveRange(FLAGS, 4); saveRange(DATE, 4); saveRange(ABORT, 4);
    const rows = psRows();
    fx.n = rows.length;
    rowsOn(fx, rows.map(function (r) { return Object.assign({}, r, { on: function () { psOn(r); } }); }));
    rngStand(fx, 0);
    // tileToScreen(x, y, &a, &b): writes a screen position computed from the tile
    calleeStand(fx, 1, 0x0042f940, 4, function (n, a) {
      ptr(a[2]).writeS32(100 + 7 * a[0] + a[1]); ptr(a[3]).writeS32(40 + 3 * a[1] + a[0]);
      return 0;
    }, { mask: [2, 3] });
    calleeStand(fx, 2, 0x00404b70, 4, 0, { str: { 0: 2 } });                    // drawCenteredText(text, x, y, color)
    calleeStand(fx, 3, 0x00480c80, 2, 0, { abi: 'thiscall' });                    // Window::refreshFull(this, 0)
    calleeStand(fx, 4, 0x0045c0c0, 1, 1);                                        // pumpAndRedraw(0)
    return fx;
  }
  F.c3eb_ps = pathScene;

  // ====================================================================================================================
  // updateWalkers 0x00402a40 ()
  // ====================================================================================================================
  // One call walks all 64 records, so a row is a whole scene: 64 walkers (0x4c bytes at 0x00585850), the 152 golfer records
  // (0x100 bytes at 0x005794b8), the 256-entry list at 0x005736b0 (0x24 bytes), the tile maps and the scalar globals. A scene is
  // generated from a seed (lcg) with a bias that favours one family of states; rows may edit the generated spec. Walker 0 is the
  // "definer": a mode -4 walker that always reaches 0x0040317d, so the stack slots the original keeps from record to record
  // ([esp+0x30] / [esp+0x28]) hold the same values in both arms before any flagged walker reads them at 0x00403661.
  const WLK = 0x00585850, WLK_N = 0x1300, LST = 0x005736b0, LST_N = 0x2400, T804 = 0x00578804, STAT = 0x00584214, STAT_N = 0x7e0;
  const GOLF_FULL = 152 * 0x100;
  const SC_G = [0x00575cb8, 0x00543d08, 0x00543d0c, 0x00578150, 0x00578154, 0x005a6d3c, 0x0053fa28, 0x00571fd4, SEL, DATE, ABORT];
  function pick(g, arr) { return arr[g(arr.length)]; }

  function genWalker(g, bias, tileLo, tileHi) {
    const span = tileHi - tileLo + 1;
    const tx = tileLo + g(span), ty = tileLo + g(span);
    const w = {};
    w.x = tx * 1024 + g(1024); w.y = ty * 1024 + g(1024);
    w.fl = pick(g, [1, 1, 2, 8, 9, 0x10, 0x11, 0x18, 0x80, 0x19, 0x0a, 8]);
    const modes = {
      mixed: [1, 2, 0, 0xfd, 0xfe, 0xfb, 0xfa, 0xfc, 0xff, 0xf9, 0xfa, 0xfc],
      follow: [1, 1, 2, 3, 1, 0xfc, 0xfa],
      tile: [0xfc, 0xfc, 0xfa, 0xfa, 0xfc, 0xfd],
      golfer: [0xfd, 0xfe, 0xfb, 0xfa, 0xfa, 0xfd, 0xfe, 0xfb, 0x00, 0xff],
      tail: [0xfd, 0xfe, 0xfc, 0xfa, 0xfb, 1],
    }[bias];
    w.mode = pick(g, modes);
    const tm = g(3);
    if (tm === 0) { w.tgx = g(8) === 0 ? 0xff : (tx + g(9) - 4) & 0xff; w.tgy = (ty + g(9) - 4) & 0xff; }
    else { w.tgx = tx; w.tgy = ty; }                                       // the walker's own tile (a target within 0x400)
    w.sub = pick(g, [0, 0xfe, 0xfb, 0xfd, 0x23, 0xfc, 0xfe, 0xfb]);
    w.timer = pick(g, bias === 'tail' ? [0, 0, 1, 4, 5, 5, 8, 5] : [0, 0, 0, 1, 4, 5, 5, 8]);
    w.dir = g(8);
    w.steps = pick(g, bias === 'tail' ? [0, 0, 3, 0x10, -2, 0x200, 1] : [0, 0, 0, 0, 0, 3, 0x10, -2, 0x200]);
    w.wait = pick(g, [0, 0, 0, 0, 0, 0, 1, 2, 0x40, -3]);
    w.w1c = g(100);
    w.anim = pick(g, [0, 5, 6, 7, 8, 9, 10, 11, 12, -4]);
    w.who = g(12);
    w.cnt = g(5);
    return w;
  }
  function genScene(r) {
    const g = lcg(r.seed >>> 0);
    const sp = { walkers: [], golfers: [], list: [] };
    const lo = r.lo === undefined ? 18 : r.lo, hi = r.hi === undefined ? 30 : r.hi;
    // tile maps
    sp.tt = new Uint8Array(2500); sp.tf = new Uint8Array(5000); sp.t804 = new Uint8Array(2500); sp.tc = new Uint8Array(2500);
    const ttMix = r.ttMix || [1, 2, 3, 4, 5, 7, 0x11, 0x11, 0x15, 0x14, 0, 0x16, 2, 3];
    const tfMix = r.tfMix || [0, 0, 0x20, 0x800, 0x4000, 0x820, 0x4800, 0x400, 0x800, 0];
    for (let i = 0; i < 2500; i++) {
      sp.tt[i] = pick(g, ttMix);
      const f = pick(g, tfMix); sp.tf[2 * i] = f & 0xff; sp.tf[2 * i + 1] = f >> 8;
      sp.t804[i] = pick(g, [0, 1, 2, 3, 7, 0x30]);
      sp.tc[i] = g(4);
    }
    // golfer records: junk everywhere, then ~14 live ones near the walkers
    for (let k = 0; k < 152; k++) {
      sp.golfers.push({ x: (3 + g(44)) * 1024 + g(1024), y: (3 + g(44)) * 1024 + g(1024), f10: 0, b1a: 0x11, b1d: 0, b21: 0, b22: 0, b70: 0, w9e: 0, wa4: 0, wa6: 0x40, wa8: 0, waa: 0,
                        d4: 0, d8: 0 });
    }
    for (let k = 0; k < 14; k++) {
      const q = sp.golfers[k];
      q.x = (lo + g(hi - lo + 1)) * 1024 + g(1024); q.y = (lo + g(hi - lo + 1)) * 1024 + g(1024);
      q.f10 = pick(g, [0, 0x4000, 0x20000000, 0x20004000, 0x2000, 0]);
      q.b1d = pick(g, [7, 9, 3, 0x7f, 7, 8]);
      q.b21 = pick(g, [1, 2, 5, 0, 1, 1, 0x7f, 0x80, 0x13]);
      q.b22 = pick(g, [0, 1, 2, 3, 0xff, 0]);
      q.b70 = pick(g, [0x22, 0x3a, 0x10, 0, 0x22, 0x3b]);
      q.wa4 = pick(g, [0, 3, -2, 0]); q.w9e = pick(g, [0, 1, 5, -3]);
      q.wa6 = pick(g, [0x10, 0x11, 0x40, 0, -1]); q.wa8 = pick(g, [0x10, 0x11, 0x40, 0, -1]); q.waa = pick(g, [0xa0, 0xa1, 0x20, 0, 0x7fff]);
      q.d4 = g(3) === 0 ? 0 : (lo + g(hi - lo + 1)) * 1024; q.d8 = (lo + g(hi - lo + 1)) * 1024;
      q.b1a = g(256);
    }
    sp.golfers[0].wa8 = pick(g, [0, 3, 0x10, 0x11, -1]);
    // walkers
    for (let i = 0; i < 64; i++) {
      const bias = r.bias === 'mixed' ? 'mixed' : (g(3) === 0 ? 'mixed' : r.bias);
      const w = genWalker(g, bias, lo, hi);
      if (i < r.active) {
        if (i > 0 && g(4) === 0) { w.x = sp.walkers[i - 1].x + g(200) - 100; w.y = sp.walkers[i - 1].y + g(200) - 100; }   // clustered with the previous one
        else if (g(3) === 0) {                                                   // next to one of the live golfers, aimed at its tile
          const q = sp.golfers[g(14)];
          w.x = q.x + g(300) - 150; w.y = q.y + g(300) - 150; w.tgx = (q.x >> 10) & 0xff; w.tgy = (q.y >> 10) & 0xff;
        }
        sp.walkers.push(w);
      } else {
        w.fl = 0;
        sp.walkers.push(w);
      }
    }
    // the definer
    const d = sp.walkers[0];
    d.fl = 1; d.mode = 0xfc; d.steps = 0; d.wait = 0; d.timer = 0;
    // the list of 256 entries
    for (let e = 0; e < 256; e++) {
      const near = g(5) === 0;
      sp.list.push({ x: near ? (lo + g(hi - lo + 1)) * 1024 + g(1024) : 0x4000 + e, y: near ? (lo + g(hi - lo + 1)) * 1024 + g(1024) : 0x5000 + e,
                     v: g(3) === 0 ? -1 : (near ? 3 : 0x77), w: 0x1234 + e });
    }
    // scalars
    sp.sel = r.sel !== undefined ? r.sel : pick(g, [-1, 0, 1, 2, 3, 5]);
    sp.date = r.date !== undefined ? r.date : pick(g, [0x100, 0x104, 0x201, 0x3fc, 0x2f0, 0x1000, 0x4, 0x8]) + g(100) * 4 * (g(2));
    sp.fl = pick(g, [0, 0x200000, 0, 0x200000]);
    sp.g575cb8 = pick(g, [0, 0, 1]);
    sp.g543d08 = lo + g(hi - lo + 1); sp.g543d0c = lo + g(hi - lo + 1);
    sp.g578150 = lo + g(hi - lo + 1); sp.g578154 = lo + g(hi - lo + 1);
    sp.s5a6d3c = pick(g, [0, 57, 99, 100, 1234, 4000, 7]);
    sp.b53fa28 = pick(g, [5, 10, 0, 0xff]);
    sp.c571fd4 = 1000 + g(1000);
    sp.stat = new Uint8Array(STAT_N); for (let i = 0; i < STAT_N; i++) sp.stat[i] = g(256);
    // scripted sequences
    sp.rngB = []; sp.rngH = [];
    for (let i = 0; i < 1024; i++) { sp.rngB.push(g(1 << 20)); sp.rngH.push(g(4) === 0 ? 1 : 0); }
    sp.ps = [];
    for (let i = 0; i < 128; i++) sp.ps.push(pick(g, [0xff, 0, 1, 2, 3, 4, 5, 6, 7, 0xff, 0x12345602, 0xabcdef05, 0x3, 0xff, 0x77ffff01]));
    if (r.edit) r.edit(sp, g);
    return sp;
  }
  function w32(dv, off, v) { dv.setUint32(off, v >>> 0, true); }
  function w16(dv, off, v) { dv.setUint16(off, v & 0xffff, true); }
  function uwOn(r) {
    const sp = genScene(r);
    // walkers
    const wb = new ArrayBuffer(WLK_N), wv = new DataView(wb);
    sp.walkers.forEach(function (w, i) {
      const o = 0x4c * i;
      w32(wv, o, w.x); w32(wv, o + 4, w.y); wv.setUint8(o + 0x10, w.tgx); wv.setUint8(o + 0x11, w.tgy); wv.setUint8(o + 0x12, w.fl); wv.setUint8(o + 0x13, w.mode);
      wv.setUint8(o + 0x14, w.sub); wv.setUint8(o + 0x15, w.timer); wv.setUint8(o + 0x16, w.dir); w16(wv, o + 0x18, w.steps); w16(wv, o + 0x1a, w.wait);
      w16(wv, o + 0x1c, w.w1c); w16(wv, o + 0x1e, w.anim); w16(wv, o + 0x20, w.who); w16(wv, o + 0x26, w.cnt);
      for (let q = 8; q < 0x10; q += 4) w32(wv, o + q, 0x0a0b0c0d + q + i);          // fields the function never reads
      w16(wv, o + 0x22, 0x2222 + i); w16(wv, o + 0x24, 0x2424 + i); w16(wv, o + 0x28, 0x2828 + i);
      for (let q = 0x2a; q < 0x4c; q += 2) w16(wv, o + q, 0x3000 + i + q);
    });
    ptr(WLK).writeByteArray(wb);
    // golfers: junk first (every byte), then the fields the function uses
    const gb = new ArrayBuffer(GOLF_FULL), gv = new DataView(gb);
    for (let i = 0; i < GOLF_FULL; i += 4) w32(gv, i, 0x5a5a0000 + (i >> 2));
    sp.golfers.forEach(function (q, k) {
      const o = 0x100 * k;                                  // GOLF = 0x005794b8 is record offset 0
      w32(gv, o, q.x); w32(gv, o + 4, q.y);
      w32(gv, o + 0x10, q.f10); gv.setUint8(o + 0x1a, q.b1a); gv.setUint8(o + 0x1d, q.b1d); gv.setUint8(o + 0x21, q.b21); gv.setUint8(o + 0x22, q.b22);
      gv.setUint8(o + 0x70, q.b70); w16(gv, o + 0x9e, q.w9e); w16(gv, o + 0xa4, q.wa4); w16(gv, o + 0xa6, q.wa6); w16(gv, o + 0xa8, q.wa8); w16(gv, o + 0xaa, q.waa);
      w32(gv, o + 0xd4, q.d4); w32(gv, o + 0xd8, q.d8);
    });
    ptr(GOLF).writeByteArray(gb);
    // the list
    const lb = new ArrayBuffer(LST_N), lv = new DataView(lb);
    sp.list.forEach(function (e, i) { w32(lv, 0x24 * i, e.x); w32(lv, 0x24 * i + 4, e.y); w32(lv, 0x24 * i + 8, e.v); w32(lv, 0x24 * i + 12, e.w); for (let q = 16; q < 0x24; q += 4) w32(lv, 0x24 * i + q, 0x77770000 + i); });
    ptr(LST).writeByteArray(lb);
    ptr(TT).writeByteArray(sp.tt.buffer); ptr(TF).writeByteArray(sp.tf.buffer); ptr(T804).writeByteArray(sp.t804.buffer); ptr(STAT).writeByteArray(sp.stat.buffer);
    ptr(TC).writeByteArray(sp.tc.buffer);                                       // pathStep's cost bytes (non-negative: a negative cost can make the wave loop)
    fillMem(GRID, 2500, 0x77); fillMem(QX, 0x800, 0x66); fillMem(QY, 0x800, 0x99); fillMem(PBUF, PBUF_N, 0x55); fillMem(B58A, 0x20, 0x66);
    for (let t = 0; t < 0x18; t++) { wr8(0x00578375 + 0x30 * t, ((t * 3) % 8) - 1 + (t === 5 ? -4 : 0)); wr8(0x00578376 + 0x30 * t, (t & 3) === 3 ? 7 : (t & 3)); }
    wr32(SEL, sp.sel); wr32(DATE, sp.date); wr32(ABORT, 0);
    setFlags(0x200000 | 0x100, sp.fl);
    wr8(0x00575cb8, sp.g575cb8); wr32(0x00543d08, sp.g543d08); wr32(0x00543d0c, sp.g543d0c); wr32(0x00578150, sp.g578150); wr32(0x00578154, sp.g578154);
    wr16(0x005a6d3c, sp.s5a6d3c); wr8(0x0053fa28, sp.b53fa28); wr32(0x00571fd4, sp.c571fd4);
    cur = Object.assign({}, r, { sp: sp });
    r.sp = sp;
  }
  function uwScene(realPath) {
    const fx = { clog: newClog(0x4000), clogCap: 0x4000 };
    freezeNow();
    saveRange(WLK, WLK_N); saveRange(GOLF, GOLF_FULL); saveRange(LST, LST_N); saveRange(TT, 2500); saveRange(TF, 5000); saveRange(T804, 2500);
    saveRange(STAT, STAT_N); saveRange(ATTRB, ATTR_N); saveRange(0x00578375 - 0x30, 0x30 * 0x19); saveRange(TC, 2500);
    saveRange(GRID, 2500); saveRange(QX, 0x800); saveRange(QY, 0x800); saveRange(PBUF, PBUF_N); saveRange(B58A, 0x20);
    SC_G.forEach(function (a) { saveRange(a, 4); });
    saveRange(FLAGS, 4); saveRange(0x00575cb8, 4);
    const rows = uwRows();
    fx.n = rows.length;
    rowsOn(fx, rows.map(function (r) { return Object.assign({}, r, { a: 0, on: function () { uwOn(r); } }); }));
    // Random::range: the scene's scripted values, reduced by the argument; one in four carries a high word
    calleeStand(fx, 0, 0x0045c1e0, 2, function (n, a) {
      const sp = cur.sp, m = a[1] >>> 0;
      const base = sp.rngB[n % 1024];
      let v = m ? base % m : base;
      if (sp.rngH[n % 1024]) v = (v | 0x10000) >>> 0;
      return v;
    }, { abi: 'thiscall' });
    calleeStand(fx, 1, 0x004675d0, 1, function (n, a) { return (a[0] * 5 + 3) & 0xff; });                // thoughtFlag(who)
    calleeStand(fx, 2, 0x00467a00, 3, 0, { extra: function (a) { return [ptr(0x00579560 + ((a[0] << 8) >>> 0)).readU16()]; } });   // emitGolferThought; logs the golfer's word 0x579560 at the call
    calleeStand(fx, 3, 0x0040c890, 4, 0);                                                                 // pointsPopup
    calleeStand(fx, 4, 0x0040c500, 4, 0);                                                                 // playSoundAt
    calleeStand(fx, 5, 0x00402930, 2, 0);                                                                 // freeAtTile
    if (!realPath) {
      calleeStand(fx, 6, 0x0042e7e0, 5, function (n) { return cur.sp.ps[n % 128]; });                    // pathStep
    } else {
      calleeStand(fx, 7, 0x0042f940, 4, function (n, a) { ptr(a[2]).writeS32(100 + 7 * a[0] + a[1]); ptr(a[3]).writeS32(40 + 3 * a[1] + a[0]); return 0; }, { mask: [2, 3] });
      calleeStand(fx, 8, 0x00404b70, 4, 0, { str: { 0: 2 } });
      calleeStand(fx, 9, 0x00480c80, 2, 0, { abi: 'thiscall' });
      calleeStand(fx, 10, 0x0045c0c0, 1, 1);
    }
    return fx;
  }
  F.c3eb_uw = function () { return uwScene(false); };
  F.c3eb_uwr = function () { return uwScene(true); };

  function uwRows() {
    const rows = [];
    const add = function (o) { rows.push(Object.assign({ seed: 1, bias: 'mixed', active: 24 }, o)); };
    // random scenes by bias
    const biases = ['mixed', 'follow', 'tile', 'golfer', 'tail'];
    for (let n = 0; n < 60; n++) add({ seed: 0x10000 + n * 7919, bias: biases[n % 5], active: 6 + (n % 5) * 6 });
    // no tile with the 0x800 bit: the grid scan finds nothing, the list scan and the golfer search run
    const NO800 = [0, 0x20, 0x400, 0x4000, 0x20, 0];
    for (let n = 0; n < 24; n++) add({ seed: 0x20000 + n * 104729, bias: ['tile', 'golfer', 'mixed'][n % 3], active: 8 + (n % 4) * 8, tfMix: NO800, sel: -1,
      edit: function (sp) { sp.fl = 0; sp.g575cb8 = 0; sp.golfers[1].b21 = n & 1 ? 1 : 0; sp.date = [0x100, 0x300, 0x204, 0x500][n & 3]; } });
    // flags / golfer 1 / the course scalars for the mode -6 target override
    for (let n = 0; n < 8; n++) add({ seed: 0x30000 + n * 6007, bias: 'golfer', active: 20, sel: -1, tfMix: NO800,
      edit: function (sp) { sp.fl = n & 1 ? 0x200000 : 0; sp.golfers[1].b21 = (n >> 1) & 1; sp.g575cb8 = (n >> 2) & 1; sp.date = n & 2 ? 0x200 : 0x100; sp.g543d08 = 22; sp.g543d0c = 22 + n; sp.tt[22 * 50 + 22 + n] = n % 3 === 0 ? 0x11 : 2; } });
    // a followed golfer with b22 = 0 and b21 = 1, not near, and w9e <= 0: the move arm only with the b22/b21 test false
    for (let n = 0; n < 6; n++) add({ seed: 0x40000 + n * 3571, bias: 'follow', active: 16, sel: n & 1,
      edit: function (sp) { const q = sp.golfers[n & 1]; q.b22 = 0; q.b21 = n < 4 ? 1 : 2; q.w9e = n & 2 ? -3 : 0; q.d4 = n & 4 ? 0 : 20 * 1024; } });
    // kind -3 (mode 0xfd): a golfer with 0x20000000 and a walker without flag 8, and with it
    for (let n = 0; n < 8; n++) add({ seed: 0x60000 + n * 1299709, bias: 'golfer', active: 24, sel: -1, tfMix: NO800,
      edit: function (sp) {
        for (let i = 1; i < 24; i++) { sp.walkers[i].mode = 0xfd; sp.walkers[i].fl = n & 1 ? 9 : 1; sp.walkers[i].steps = 0; sp.walkers[i].wait = 0; sp.walkers[i].timer = 0; }
        for (let k = 0; k < 14; k++) { const q = sp.golfers[k]; q.f10 = n & 2 ? 0x20000000 : 0x20004000 * (k & 1); q.b22 = 0; q.b21 = 1; q.b1d = 7; }
      } });
    // dense scenes with many targets next to their walkers (flags 8 on all of them)
    for (let n = 0; n < 12; n++) add({ seed: 0x50000 + n * 2713, bias: ['tile', 'golfer', 'mixed', 'tail'][n % 4], active: 40,
      edit: function (sp) { for (let i = 1; i < 40; i++) { sp.walkers[i].fl = (n & 1) ? (sp.walkers[i].fl | 8) : (sp.walkers[i].fl & ~8) | 1; } } });
    // ---- boundary scenes: one walker (or a few) in an otherwise empty world
    const TC = function (t) { return t * 1024 + 0x200; };
    const blank = function (sp) {
      for (let i = 1; i < 64; i++) sp.walkers[i].fl = 0;
      sp.walkers[0].fl = 1; sp.walkers[0].mode = 0xfc; sp.walkers[0].tgx = 5; sp.walkers[0].tgy = 5; sp.walkers[0].x = TC(5); sp.walkers[0].y = TC(5);
      sp.golfers.forEach(function (q) { q.b21 = 0; });
      sp.list.forEach(function (e) { e.v = -1; });
      sp.tf.fill(0); sp.t804.fill(0); sp.tt.fill(2);
      sp.sel = -1; sp.fl = 0; sp.g575cb8 = 0; sp.date = 0x100;
    };
    const setW = function (sp, i, o) {
      Object.assign(sp.walkers[i], { fl: 1, mode: 0xfc, sub: 0, timer: 0, steps: 0, wait: 0, dir: 0, anim: 0, who: 0, cnt: 0, w1c: 0 }, o);
    };
    const setTF = function (sp, x, y, v) { const i = x * 50 + y; sp.tf[2 * i] = v & 0xff; sp.tf[2 * i + 1] = v >> 8; };
    // follow: the golfer 0x7ff / 0x800 / 0x801 away (dx only) from mode 1 walkers
    [0x7ff, 0x800, 0x801].forEach(function (off, k) {
      add({ seed: 0x70000 + k, bias: 'follow', active: 4, edit: function (sp) {
        blank(sp);
        sp.sel = 0; const q = sp.golfers[0]; q.x = TC(30); q.y = TC(30); q.d4 = 0; q.d8 = 0; q.w9e = 0; q.b22 = 0; q.b21 = 2;
        [1, 2].forEach(function (i) { setW(sp, i, { mode: i, x: q.x - off, y: q.y, tgx: 5, tgy: 5 }); });
      } });
    });
    // a target tile at the walker's own tile, walker offset 0x3ff / 0x400 / 0x401 from its centre (no 0x800 tile anywhere)
    [0x3ff, 0x400, 0x401].forEach(function (off, k) {
      add({ seed: 0x70010 + k, bias: 'tile', active: 4, edit: function (sp) {
        blank(sp);
        [1, 2, 3, 4].forEach(function (i) { setW(sp, i, { mode: 0xfc, tgx: 10 + 3 * i, tgy: 20, x: TC(10 + 3 * i) + (i & 1 ? off : -off), y: TC(20) + (i > 2 ? off : 0) }); });
      } });
    });
    // flagged walkers 0x5ff / 0x600 / 0x601 from the tile [0x00578150] / [0x00578154]
    [0x5ff, 0x600, 0x601].forEach(function (off, k) {
      add({ seed: 0x70020 + k, bias: 'tail', active: 4, edit: function (sp) {
        blank(sp);
        sp.g578150 = 20; sp.g578154 = 20;
        [1, 2, 3, 4].forEach(function (i) { setW(sp, i, { fl: 0x10, mode: 0xfc, x: TC(20) + (i & 1 ? off : -off), y: TC(20) + (i > 2 ? off : 0) }); });
        sp.date = 0x104 + 4 * k;
      } });
    });
    // keep-clear: walkers 341 +- 1 units from a dormant earlier one, headings 0..7
    [0x154, 0x155, 0x156].forEach(function (off, k) {
      add({ seed: 0x70030 + k, bias: 'tail', active: 4, edit: function (sp) {
        blank(sp);
        setW(sp, 1, { wait: 0x7000, x: TC(25), y: TC(25), mode: 0xfc });
        for (let d = 0; d < 8; d++) setW(sp, 2 + d, { steps: 5, dir: d, x: TC(25) + (d & 1 ? off : -off), y: TC(25) + (d & 2 ? off : 0), mode: 0xfc, tgx: 25, tgy: 25 });
      } });
    });
    // tile scan limits: one 0x800 tile at distance 23, 24, 31, 32 tiles, with and without flag bit 8
    [23, 24, 31, 32].forEach(function (dist) {
      [1, 9].forEach(function (fl) {
        add({ seed: 0x70040 + dist + fl, bias: 'tile', active: 4, edit: function (sp) {
          blank(sp);
          setW(sp, 1, { fl: fl, mode: 0xfc, tgx: 8 + dist, tgy: 20, x: TC(10), y: TC(20) });   // the flagged tile is one beyond the target: dd = dist
          setTF(sp, 9 + dist, 20, 0x800);
        } });
      });
    });
    // tile scan range: the 0x800 tile at +-16 on either axis
    [[16, 0], [-16, 0], [0, 16], [0, -16]].forEach(function (o, k) {
      add({ seed: 0x70050 + k, bias: 'tile', active: 4, edit: function (sp) {
        blank(sp);
        const t = [25, 25];
        setW(sp, 1, { fl: 1, mode: 0xfc, tgx: t[0], tgy: t[1], x: TC(t[0] + 2 * o[0]), y: TC(t[1] + 2 * o[1]) });
        setTF(sp, t[0] + o[0], t[1] + o[1], 0x800);
      } });
    });
    // list scan: two identical entries, the walker next to them (flag bit 8), plus a farther one
    [0, 1].forEach(function (k) {
      add({ seed: 0x70060 + k, bias: 'tile', active: 4, edit: function (sp) {
        blank(sp);
        setW(sp, 1, { fl: 9, mode: k ? 0xfa : 0xfc, tgx: 20, tgy: 20, x: TC(20) + 30, y: TC(20) + 40 });
        sp.list[7] = { x: TC(20), y: TC(20), v: 3, w: 1 }; sp.list[40] = { x: TC(20), y: TC(20), v: 3, w: 2 }; sp.list[9] = { x: TC(21), y: TC(20), v: 3, w: 3 };
      } });
    });
    // kind -5 against the threshold of the golfer word 0x579560 (0xa8): values 3..9, walkers with and without flag bit 8
    [1, 9].forEach(function (fl, k) {
      add({ seed: 0x70070 + k, bias: 'golfer', active: 4, edit: function (sp) {
        blank(sp);
        for (let q = 0; q < 14; q++) {
          const gq = sp.golfers[q]; gq.b21 = 1; gq.b1d = 7; gq.b22 = 0; gq.b70 = 0x10; gq.f10 = 0; gq.wa8 = 3 + (q % 7);
          gq.x = TC(30 + (q % 7)); gq.y = TC(30 + (q >> 3)); sp.tt[(30 + (q % 7)) * 50 + 30 + (q >> 3)] = 2;
        }
        for (let i = 1; i < 8; i++) setW(sp, i, { fl: fl, mode: 0xfb, tgx: 30 + (i % 7), tgy: 30, x: TC(30 + (i % 7)) + 20, y: TC(30) + 20 });
      } });
    });
    // golfer search threshold: tileDistance exactly 0x63 / 0x64 / 0x65 (offsets found with the real tileDistance)
    [0x63, 0x64, 0x65].forEach(function (want, k) {
      add({ seed: 0x70080 + k, bias: 'golfer', active: 4, edit: function (sp) {
        blank(sp);
        const td = new NativeFunction(ptr(0x0040c4b0), 'int', ['int', 'int', 'int', 'int'], 'default');
        const tgx = 20, tgy = 20;
        let off = 0;
        while (off < 60000 && td(TC(tgx) + off, TC(tgy), tgx, tgy) < want) off++;
        if (td(TC(tgx) + off, TC(tgy), tgx, tgy) !== want) throw new Error('c3eb: no offset gives tileDistance ' + want);
        const gq = sp.golfers[3]; gq.b21 = 1; gq.b1d = 7; gq.b22 = 0; gq.b70 = 0x10; gq.f10 = 0; gq.x = TC(tgx) + off; gq.y = TC(tgy);
        sp.tt[Math.min(49, (gq.x >> 10)) * 50 + 20] = 2;
        setW(sp, 1, { fl: 1, mode: 0xfe, tgx: tgx, tgy: tgy, x: TC(tgx) + off - 10, y: TC(tgy) + 5 });
      } });
    });
    // counter byte against the 0x4000 tile: t804 0..4, walkers of mode 0xfc next to flagged tiles
    for (let k = 0; k < 3; k++) {
      add({ seed: 0x70090 + k, bias: 'tile', active: 4, edit: function (sp) {
        blank(sp);
        for (let i = 1; i < 9; i++) {
          const tx = 8 + 4 * i, ty = 10 + 3 * k;
          setW(sp, i, { fl: i & 1 ? 1 : 9, mode: 0xfc, tgx: tx, tgy: ty, x: TC(tx), y: TC(ty) });
          setTF(sp, tx, ty, 0x4800); sp.t804[tx * 50 + ty] = (i + 3 * k) % 5;
        }
      } });
    }
    return rows;
  }

  Object.assign(globalThis.DIFF_FIXTURES, F);
  globalThis.__c3eb = { rowsOn, calleeStand, rngStand, newClog, freezeNow, alloc, wr32, wr16, wr8, rd32, fillMem, saveRange, lcg, setFlags,
                        curRow: function () { return cur; }, animalRows, psRows, uwRows };
})();
