// Fixtures for batch c3ed (simulation plan phase 5, 2026-10-11): editTerrainHeight 0x00409cf0 and applyTerrainEdit 0x00470a60.
// Reimplementations: shim/src/re/c3ed.cpp. Registry: re/frida/registry.d/c3ed.py. Purposes and per-jcc tables:
// log/c3/c3ed_purpose.md.
//
// Both functions edit the live tile tables, so every key freezes the game's first thread (re/frida/js/freeze.js) and saves every
// table it writes (__freeze.save, restored by the flush). Callees are replaced for ONE key on the A/B thread only by calleeStand
// (copied from c3dy.js, with a `terse` option: count only): every call is appended to the call log fx.clog (compared state) and
// answered from a number, a sequence or a function. Random::range, sign, clamp and clearTile run for real (their results depend
// only on state that is compared). Per-vector variation uses the getter trick of c3dr/c3dw/c3dy: diff_hook resolves "$rN"
// arguments at each vector BEFORE the state snapshot, so the property getter of fx.rN applies that row's globals and tables.
(function () {
  const F = {};
  function alloc(n) {
    const p = __keepAlloc(n);
    for (let i = 0; i < n; i += 4) p.add(i).writeU32(0);
    return p;
  }
  // The negative-control build keeps its wrong bodies dormant until this flag page is set; in the normal build the address
  // is unmapped and the write is skipped.
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
  function lcg(x) { return (Math.imul(x, 1664525) + 1013904223) >>> 0; }
  function wr32(a, v) { ptr(a).writeU32(v >>> 0); }
  function wr16(a, v) { ptr(a).writeU16(v & 0xffff); }
  function wr8(a, v) { ptr(a).writeU8(v & 0xff); }
  function rd32(a) { return ptr(a).readU32(); }
  function fillMem(a, n, b) { const arr = new Uint8Array(n); arr.fill(b & 0xff); ptr(a).writeByteArray(arr.buffer); }
  function saveRange(a, n) { __freeze.save(ptr(a), n); }
  // a pseudo-random byte image of n bytes from `seed`
  function randMem(a, n, seed, mask) {
    const arr = new Uint8Array(n);
    let s = seed >>> 0;
    for (let i = 0; i < n; i++) { s = lcg(s); arr[i] = ((s >>> 16) & 0xff) & (mask === undefined ? 0xff : mask); }
    ptr(a).writeByteArray(arr.buffer);
  }

  // ---- rows: fx.r<i> is a getter that applies row i (row.on) and makes it the current row; its value is row.a (the argument)
  let cur = {};
  function rowsOn(fx, rows) {
    rows.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () {
        cur = row;
        if (row.on) row.on(fx, row);
        return row.a === undefined ? 0 : row.a;
      } });
    });
  }
  function seq(name, dflt) {
    return function (n) {
      const s = cur[name];
      if (s === undefined) return dflt === undefined ? 0 : dflt;
      return s[Math.min(n, s.length - 1)] >>> 0;
    };
  }

  // ---- a function of the exe replaced for one key on the A/B thread only (c3dw's calleeStand): impl runs on the A/B thread
  // and every call is appended to the call log fx.clog (word 0 = cursor, entries {tag, count, args...} from +0x50; word 1+k =
  // call count of stub k, up to 16 stubs). terse: only the count is kept. Other threads reach the real function.
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x800;
  function newClog(cap) { const l = alloc(cap || CLOG); l.writeU32(0x50); return l; }
  function calleeStand(fx, k, va, argc, ret, terse) {
    freezeNow();
    const tid = Process.getCurrentThreadId();
    const at = ptr(va), key = at.toString();
    if (standLive[key]) {
      try { Interceptor.revert(at); } catch (e) { /* already reverted */ }
      delete standLive[key];
      Interceptor.flush();
    }
    const argt = Array(argc).fill('uint32');
    const orig = new NativeFunction(at, 'uint32', argt, 'default');
    const log = fx.clog;
    const fn = function () {
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, arguments);
      const a = Array.prototype.slice.call(arguments);
      const cell = log.add(4 + 4 * k);
      const n = cell.readU32();
      cell.writeU32(n + 1);
      if (!terse) {
        const curs = log.readU32();
        const need = 8 + 4 * a.length;
        if (curs + need <= (fx.clogCap || CLOG)) {
          log.add(curs).writeU32(va >>> 0);
          log.add(curs + 4).writeU32(a.length);
          a.forEach(function (v, i) { log.add(curs + 8 + 4 * i).writeU32(v >>> 0); });
          log.writeU32(curs + need);
        }
      }
      let r;
      if (typeof ret === 'function') r = ret(n, a);
      else if (Array.isArray(ret)) r = ret[Math.min(n, ret.length - 1)];
      else r = ret === undefined ? 0 : ret;
      return r >>> 0;
    };
    const cb = new NativeCallback(fn, 'uint32', argt, 'default');
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

  // ====================================================================================================================
  // editTerrainHeight 0x00409cf0 (no arguments; one dummy argument carries the row)
  // ====================================================================================================================
  const QCOL = 0x00586fa8, QROW = 0x00586b50, QMODE = 0x005a8834;
  const TILEW = 0x0053caf0, TILET = 0x005722e8, LVL = 0x005a4998, F804 = 0x00578804, HOLE = 0x00575ab0, GOLF = 0x005794b0;
  const G_TARGET = 0x004c2f9c, G_CAM0 = 0x004c2ba0, G_CAM1 = 0x004c2ba4, G_T1 = 0x005685fc, G_CUR = 0x0055e920, G_LEN = 0x005a9cd4;
  const G_FLAGS = 0x0059e7b8, G_STAMP = 0x004c2854, G_E04 = 0x004c2e04, G_47E4 = 0x005a47e4, G_H = 0x005685f0, G_DATE = 0x00834170;
  const G_SEED = 0x00822d9c;

  // rows: defaults, then overrides
  const ETH_DEF = { target: -1, cam0: 10, cam1: 12, t1: 12, cursor: 3, len: 6, col: 7, row: 9, mode: 3, H: 1, date: 12,
                    flags: 0x02000000, seed: 0x1234, rec: {}, attr: {}, gm: [3, 5], lvl: 7, hole: 0 };
  function ethRows() {
    const rows = [];
    let n = 0;
    const add = function (o) {
      const r = Object.assign({}, ETH_DEF, o);
      r.seed = (0x2000 + Math.imul(n + 1, 0x9e3779b1)) >>> 0;
      n++;
      rows.push(r);
    };
    // ---- camera
    add({ target: 20, cam0: 10, cam1: 12, t1: 12 });                      // cam0 below the target, cam1 on its target
    add({ target: 5, cam0: 10, cam1: 12, t1: 12 });                       // cam0 above
    add({ target: 10, cam0: 10, cam1: 12, t1: 20 });                      // cam0 on target, cam1 below t1
    add({ target: 10, cam0: 10, cam1: 30, t1: 12 });                      // cam1 above t1
    add({ target: 10, cam0: 10, cam1: 12, t1: 12 });                      // both on target: falls to the queue
    add({ target: -1, cam0: 10, cam1: 12, t1: 99 });                      // no target: queue
    add({ target: 0, cam0: -5, cam1: 0, t1: 1000 });                      // negative camera, far target
    add({ target: 1000, cam0: 1000, cam1: -7, t1: -7 });                  // cam0 on target, cam1 on t1
    add({ target: 7, cam0: 3, cam1: 3, t1: 3 });                          // only cam0 moves
    // ---- queue length
    add({ cursor: 4, len: 5 });                                           // i + 1 == len: still processed
    add({ cursor: 5, len: 5, flags: 0x02000001 });                        // i + 1 > len: exhausted, flag bit set
    add({ cursor: 5, len: 5, flags: 0xfdffffff });                        // exhausted, bit already clear
    add({ cursor: 9, len: 5, flags: 0xffffffff, target: -1 });            // far past the end
    add({ cursor: 0, len: 0, flags: 0x02000000 });                        // empty queue
    add({ target: 10, cam0: 10, cam1: 12, t1: 12, cursor: 6, len: 5, flags: 0x02000000 });   // exhausted with a camera target: it is reset to -1
    add({ cursor: 2, len: -1, flags: 0x12345678 });                       // negative length
    // ---- xor arm (mode & 0x80)
    [0x80, 0x81, 0xff, 0x180, 0x1ff, 0x8080, 0xffffff80, 0x7f80, 0x85].forEach(function (m, k) {
      add({ mode: m, col: [0, 49, 25, 7, 49, 10, 3, 44, 17][k], row: [0, 49, 25, 8, 0, 49, 13, 2, 30][k] });
    });
    // ---- stamp arm (mode & 0x100): the eight listed types, then others
    [8, 7, 0xc, 9, 0xb, 0xd, 0xe, 4].forEach(function (t, k) {
      add({ mode: 0x100 | t, col: [3, 49, 0, 22, 41, 5, 49, 0][k], row: [4, 0, 49, 13, 48, 2, 49, 0][k] });
    });
    [0, 1, 2, 3, 5, 6, 0xa, 0xf, 0x10, 0x7e, 0x7d, 0x200 + 0x4, 0xff00].forEach(function (t, k) {
      add({ mode: 0x100 | (t & 0xff) | (t > 0xff ? t : 0), col: 12 + k, row: 5 + k });
    });
    // ---- level arm (0x7f, 0x7e)
    [[0x7f, 0, 0, 0xff], [0x7f, 49, 49, 0x00], [0x7f, 25, 17, 0x41], [0x7e, 0, 0, 0x00], [0x7e, 49, 49, 0xff], [0x7e, 9, 32, 0x10],
     [0x7f, 49, 0, 0x7f], [0x7e, 0, 49, 0x80]].forEach(function (v) {
      add({ mode: v[0], col: v[1], row: v[2], lvl: v[3] });
    });
    // ---- plain tile edits (jge 0x7d): 0x7d, 0x200, 0x1000, 0x7fff00
    [0x7d, 0x200, 0x1000, 0x7fff00].forEach(function (m, k) { add({ mode: m, col: 20 + k, row: 20 - k }); });
    // ---- hole arm: modes 3 .. 0x7c write the type; the attribute byte 1 == 0xd also flags the tile
    add({ mode: 3, attr: { 3: 0x0d } });
    add({ mode: 3, attr: { 3: 0x0c } });
    add({ mode: 0x7c, attr: { 0x7c: 0x0d }, col: 49, row: 49 });
    add({ mode: 0x7b, attr: {} });
    add({ mode: 5, attr: { 5: 0xff } });
    // mode 1 (green): record +0x18 zero / non-zero, +8 zero / non-zero (also negative)
    const recs = function (o) { return o; };
    add({ mode: 1, H: 1, col: 11, row: 6, rec: recs({ 0x18: 0, 0x1c: 55, 8: 0, 0xc: 4 }) });             // empty record: only the tile word and +0x18/+0x1c
    add({ mode: 1, H: 1, col: 11, row: 6, rec: recs({ 0x18: 0, 8: 20, 0xc: 31 }) });                       // tee known: the target is the midpoint
    add({ mode: 1, H: 2, col: 40, row: 9, rec: recs({ 0x18: 0, 8: -7, 0xc: -3 }) });                        // negative midpoint
    add({ mode: 1, H: 0, col: 2, row: 3, rec: recs({ 0x18: 0, 8: 1, 0xc: 1 }) });                           // H = 0
    add({ mode: 1, H: 1, col: 11, row: 6, rec: recs({ 0x18: 33, 0x1c: 7, 8: 20, 0xc: 31 }) });             // green already placed: nothing happens
    add({ mode: 1, H: 3, col: 49, row: 49, rec: recs({ 0x18: 0, 8: 49, 0xc: 49 }) });
    add({ mode: 1, H: 1, col: 0, row: 0, rec: recs({ 0x18: 0, 8: 1, 0xc: 0 }) });                           // odd sum
    // mode 0 (tee): record +0x18 zero / non-zero; the membership block runs for mode 0 always
    add({ mode: 0, H: 1, col: 13, row: 8, rec: recs({ 0x18: 0 }), gm: [3, 5], bump: true });               // no green yet; the second updateMembership changes [0x005685f0]
    add({ mode: 0, H: 1, col: 13, row: 8, rec: recs({ 0x18: 40, 0x1c: 17 }), gm: [3, 5], bump: true });    // green known: the target is the midpoint
    add({ mode: 0, H: 2, col: 13, row: 8, rec: recs({ 0x18: -9, 0x1c: -4 }), gm: [2, 1] });                // negative sums
    add({ mode: 0, H: 0, col: 49, row: 49, rec: recs({ 0x18: 0 }), gm: [0, 1] });
    add({ mode: 0, H: 4, col: 0, row: 0, rec: recs({ 0x18: 1, 0x1c: 1 }), gm: [6, 7] });
    add({ mode: 0, H: 5, col: 31, row: 15, rec: recs({ 0x18: 0 }), gm: [4, 3] });
    // mode 2: the membership block only when date % 6 == 0
    [0, 6, 12, 600, -6, 1, 5, 7, -1, -5, 0x7fffffff, -0x7fffffff].forEach(function (d, k) {
      add({ mode: 2, H: 1 + (k % 3), col: 4 + k, row: 40 - k, date: d, gm: [2 + (k % 4), 5 - (k % 3)], bump: k % 2 === 0 });
    });
    // mode 3 with date % 6 == 0: no membership block (mode != 0 and != 2)
    add({ mode: 3, date: 12 });
    add({ mode: 4, date: 0, attr: { 4: 0x0d } });
    return rows;
  }
  const ETH_ROWS = ethRows();
  const ETH_N = ETH_ROWS.length;

  function applyEth(fx, row) {
    // tables with random contents so every xor / or / store shows
    randMem(TILEW, 5000, row.seed, 0xff);
    randMem(TILET, 2500, row.seed ^ 0x55, 0x1f);
    randMem(F804, 2500, row.seed ^ 0xa5, 0xff);
    randMem(LVL, 2700, row.seed ^ 0x3c, 0xff);
    randMem(HOLE, 520 * 8, row.seed ^ 0x77, 0xff);
    randMem(GOLF, 0x800, row.seed ^ 0x99, 0xff);
    // the target cell's level byte is chosen by the row (0x7f / 0x7e arms)
    wr8(LVL + 51 * row.row + row.col, row.lvl);
    // the queue entry at the cursor
    const i = row.cursor;
    randMem(QCOL, 4 * 12, row.seed ^ 0x11, 0x3f); randMem(QROW, 4 * 12, row.seed ^ 0x12, 0x3f); randMem(QMODE, 4 * 12, row.seed ^ 0x13, 0xff);   // other entries
    wr32(QCOL + 4 * i, row.col); wr32(QROW + 4 * i, row.row); wr32(QMODE + 4 * i, row.mode >>> 0);
    wr32(G_CUR, i); wr32(G_LEN, row.len >>> 0);
    wr32(G_TARGET, row.target >>> 0); wr32(G_CAM0, row.cam0 >>> 0); wr32(G_CAM1, row.cam1 >>> 0); wr32(G_T1, row.t1 >>> 0);
    wr32(G_FLAGS, row.flags >>> 0); wr32(G_STAMP, 0x5a5a5a5a); wr32(G_E04, 0x11111111); wr32(G_47E4, 0x22222222);
    wr32(G_H, row.H); wr32(G_DATE, row.date >>> 0); wr32(G_SEED, row.seed);
    // hole record H: the fields the row names (dword offsets), the rest random from above
    Object.keys(row.rec).forEach(function (o) { wr32(HOLE + 520 * row.H + (+o) + 0, row.rec[o] >>> 0); });
    Object.keys(row.attr).forEach(function (m) { wr8(0x00578376 + 0x30 * (+m), row.attr[m]); });
  }

  F.c3ed_eth = function () {
    const fx = { clog: newClog(CLOG), clogCap: CLOG, plog: alloc(64) };
    freezeNow();
    saveRange(TILEW, 5000); saveRange(TILET, 2500); saveRange(F804, 2500); saveRange(LVL, 2700);
    saveRange(HOLE, 520 * 8); saveRange(GOLF, 0x800);
    saveRange(QCOL, 4 * 12); saveRange(QROW, 4 * 12); saveRange(QMODE, 4 * 12);
    [G_TARGET, G_CAM0, G_CAM1, G_T1, G_CUR, G_LEN, G_FLAGS, G_STAMP, G_E04, G_47E4, G_H, G_DATE, G_SEED].forEach(function (a) { saveRange(a, 4); });
    // attribute bytes the rows overwrite
    const attrModes = {};
    ETH_ROWS.forEach(function (r) { Object.keys(r.attr).forEach(function (m) { attrModes[m] = 1; }); });
    Object.keys(attrModes).forEach(function (m) { saveRange(0x00578376 + 0x30 * (+m), 1); });
    rowsOn(fx, ETH_ROWS.map(function (r) { return Object.assign({}, r, { a: 0, on: function (f) { applyEth(f, r); } }); }));
    calleeStand(fx, 0, 0x0040e000, 4, function (n) { return (0xaa00 + n) >>> 0; });                    // stampCourseTiles
    calleeStand(fx, 1, 0x00421bc0, 1, function (n) {                                                    // updateMembership
      const g = cur.gm;
      if (cur.bump && n === 1) wr32(G_H, rd32(G_H) + 3);                                              // the callee changes [0x005685f0]: the body must read it again
      return g[Math.min(n, g.length - 1)] >>> 0;
    });
    calleeStand(fx, 2, 0x004481b0, 5, function (n) {                                                    // playSound
      if (n < 16) fx.plog.add(4 * n).writeU32(rd32(G_STAMP));                                         // [0x004c2854] as the callee sees it
      return (0xbb00 + n) >>> 0;
    });
    calleeStand(fx, 3, 0x0042f2c0, 0, function (n) { return (0xcc00 + n) >>> 0; });                    // markTileKind11
    calleeStand(fx, 4, 0x0042f340, 0, function (n) { return (0xdd00 + n) >>> 0; });                    // rebuildTileCoverage
    calleeStand(fx, 5, 0x0042f7a0, 0, function (n) { return (0xf00d00 + n) >>> 0; });                  // rebuildHeightfield
    return fx;
  };

  // ====================================================================================================================
  // applyTerrainEdit 0x00470a60 (n). Row fields: n (course index, the argument), c0..kind..ctype (record bytes +0 .. +4), g88
  // ([0x00822c88]), flag ([0x0059e7b8] bit 24), w ([0x004c27ec]), src ([0x005a6d3c]), seed ([0x00822d9c]), the callee answers
  // (hb/sh/tb salts, ev: call numbers evalPlacementArea answers -1, wk: spawnWalker answer).
  // ====================================================================================================================
  const T_LO = 0x005722a8, T_LEN = 2692;                       // tile types with 64 bytes before and 128 after (stage D steps one row outside)
  const TB_ = 0x0053bbac, Z1 = 0x0053ea24, Z2 = 0x005a6378, Z3 = 0x0056c7e4, Z4 = 0x0056988c, COPY = 0x00542414;
  const RECS = 0x00571ff4, RECS_LEN = 46 * 17;           // courses 0 .. 16: record 17 starts inside the tile table 0x005722e8
  const OBJ = 0x0058bcb8, OBJ_LEN = 0x200, WALK = 0x00585850, WALK_LEN = 76 * 8;
  const GL = [0x0059bf90, 0x005a34e0, 0x004c2fa0, 0x00578150, 0x00578154, 0x00578158, 0x0057815c, G_CAM0, G_CAM1, 0x00822c88,
              G_FLAGS, 0x004c27ec, 0x005a6d3c, G_SEED];

  const ATE_DEF = { n: 5, c0: 0, c1: 20, c2: 0, kind: 0, ctype: 0, g88: 3, flag: 0, w: 4, src: 0x7b, seed: 1, ev: [], wk: 2, wmask: 0xff,
                    pre: 0x13572468, hbs: 0, shs: 0, tbs: 0 };
  // The clubhouse search (0x004715b9) and the c0 == 1 search (0x0047184a) repeat until a cell of the wanted type turns up in a
  // fixed window; for some seeds the generated terrain has none there and the ORIGINAL never returns. Such rows are given another
  // seed here (row index -> salt xored into the seed); found with log/c3/alt/c3ed_rows.py and a short timeout.
  const SEED_SALT = {};
  function ateRows() {
    const rows = [];
    let k = 0;
    const add = function (o) {
      const r = Object.assign({}, ATE_DEF, o);
      if (o.seed === undefined) r.seed = ((0x3000 + Math.imul(k + 1, 0x9e3779b1)) ^ (SEED_SALT[k] || 0)) >>> 0;
      if (o.hbs === undefined) r.hbs = (r.seed >>> 8) % 9;
      if (o.shs === undefined) r.shs = (r.seed >>> 12) % 11;
      if (o.tbs === undefined) r.tbs = (r.seed >>> 16) % 7;
      k++;
      rows.push(r);
    };
    // ---- A. every c0 (0 .. 15 and out of range), cycling course type, kind, difficulty selector, n and the object count
    const NS = [0, 5, 9, 14, 3, 16, 8, 12, 15, 6, 1, 10, 7, 4, 13, 11];       // n <= 16: record 17 and up lie inside the tile table 0x005722e8
    for (let v = 0; v < 3; v++) {
      for (let c0 = 0; c0 < 16; c0++) {
        const i = c0 + 16 * v;
        let n = NS[(c0 + 5 * v) % 16];
        let ctype = (c0 + 2 * v) % 4;
        if (v === 2 && c0 % 5 === 4) { ctype = 4; n = 8 + (c0 % 8); }
        // c0 == 1 searches rows 10..39, cols 0..29 for a type-0x11 cell with no bound: only the river (kind != 2, course type != 1)
        // and the shore (kind 1) put such cells there, so a row for it must have one or the original never returns
        if (c0 === 1) ctype = [0, 3, 2][v];
        const kind = v === 0 ? 0 : (v === 1 ? 1 : (c0 % 3 === 2 ? 2 : 0));
        const g88 = [3, 1, 6, 0, 2, 5, 7, 4][(c0 + v) % 8];
        add({ n: n, c0: c0, ctype: ctype, kind: kind, g88: g88, c2: i % 4, c1: [20, 100, 240, 0][i % 4],
              flag: (v === 1 && c0 % 4 === 3 && ctype !== 4) ? 1 : 0, w: [4, 3, 200, 0xff][i % 4],
              ev: [[], [0], [0, 1, 2], [1], []][(c0 + v) % 5], wk: 1 + (i % 5) });
      }
    }
    // c0 out of range (ja) and negative
    [16, 0x7f, 0x80, 0xff].forEach(function (c, j) { add({ n: 2 + j, c0: c, ctype: j % 3, kind: j % 2, g88: 2 + j }); });
    // ---- B. prologue: n (quarter q = n / 4), c2, a positive / negative preset of [0x004c2fa0], flag, course type; c0 3 = no feature
    const PN = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 15, 16];
    PN.forEach(function (n, j) {
      [0, 1, 2, 3].forEach(function (c2, m) {
        if ((j + m) % 3 !== 0) return;
        const neg = (j + m) % 2 === 1;
        const ctype = (n >= 8 && n % 5 === 0) ? 4 : (j + 2 * m) % 4;
        add({ n: n, c2: c2, c0: 3, ctype: (ctype === 4 && n < 8) ? 2 : ctype, pre: neg ? 0xfffffff3 : 0x13572468,
              flag: ((j + m) % 5 === 2 && ctype !== 4) ? 1 : 0, kind: (j + m) % 3, g88: 1 + ((j + m) % 5), c1: [0, 60, 240][(j + m) % 3] });
      });
    });
    // ---- C. kind (0 1 2 and 3 = none of them) x course type x object-count byte
    [0, 1, 2, 3].forEach(function (kind) {
      [0, 1, 2, 3].forEach(function (ctype, m) {
        add({ n: 6 + m, c0: 3, kind: kind, ctype: ctype, c1: [0, 60, 240, 255][(kind + m) % 4], g88: 2 + ((kind + m) % 4) });
      });
    });
    // ---- D. [0x00822c88] 0 .. 9 under the features that loop on it
    [5, 6, 8, 12, 13, 15, 1, 0, 10, 4, 7, 2, 9, 11, 14].forEach(function (c0, m) {
      [0, 1, 2, 4, 8, 9].forEach(function (g88, q) {
        if ((m + q) % 3 !== 0) return;
        let ctype = (m + q) % 4;
        if (c0 === 1 && ctype === 1) ctype = 0;
        add({ n: 1 + ((m * 3 + q) % 16), c0: c0, g88: g88, ctype: ctype, kind: (c0 === 1 ? (q % 2) : (m + q) % 3), c2: (m + q) % 4 });
      });
    });
    // ---- E. every c0 again with other seeds and other stand-in answers (evalPlacementArea refusing the first calls, other salts)
    for (let c0 = 0; c0 < 16; c0++) {
      let ctype = (c0 + 1) % 4;
      if (c0 === 1 && ctype === 1) ctype = 3;
      add({ n: (c0 * 7) % 17, c0: c0, ctype: ctype, kind: (c0 + 1) % 3 === 2 ? 0 : (c0 + 1) % 3, g88: [3, 2, 5, 1][c0 % 4],
            ev: [0, 1, 2, 3, 4], c1: 50 * (c0 % 5), c2: c0 % 3, w: 7 - c0 });
    }
    // ---- F. coverage additions (measured gaps of the first 134 rows)
    // course type 5 / -1 (outside 0..4) with n >= 8 and the flag clear: the base type is defined, every `ja` takes its other side
    [[5, 9], [0xff, 12], [0x80, 15], [6, 8]].forEach(function (v, j) { add({ n: v[1], ctype: v[0], c0: 3, kind: j % 3, g88: 2 + j, flag: 0 }); });
    // course type 4 with kind 1 (the shore takes tile 0x0c)
    add({ n: 10, ctype: 4, kind: 1, c0: 3, g88: 3 }); add({ n: 14, ctype: 4, kind: 1, c0: 5, g88: 2 });
    // the river climbs (sampleHeight smallest toward row 0): leaves the grid at row -1
    add({ n: 3, ctype: 0, kind: 0, c0: 3, shmode: 'up' }); add({ n: 6, ctype: 2, kind: 1, c0: 3, shmode: 'up' });
    // the clubhouse stamp paints tile types (stand-in side effect): a full water band for c0 == 7, 0x15 / 0x16 speckle, 0x11 speckle
    add({ n: 4, ctype: 0, kind: 0, c0: 7, paint: [{ rect: [14, 36, 0, 49], type: 0x11 }], g88: 3 });
    add({ n: 9, ctype: 2, kind: 1, c0: 7, paint: [{ rect: [14, 36, 0, 49], type: 0x14 }], g88: 2 });
    [8, 12, 15, 5, 6].forEach(function (c0, j) {
      add({ n: 2 + j, ctype: j % 4, kind: j % 2, c0: c0, g88: 1 + (j % 3), paint: [{ pat: 'h16', mod: 3 }], ev: [2, 3, 5] });
    });
    [0, 10].forEach(function (c0, j) { add({ n: 6 + j, ctype: j, kind: 0, c0: c0, g88: 3, paint: [{ pat: 'h16', mod: 2 }] }); });
    [13, 13, 6, 5, 8, 12, 15].forEach(function (c0, j) {
      add({ n: 5 + j, ctype: (j + 1) % 4, kind: j % 2, c0: c0, g88: 1 + (j % 4), paint: [{ pat: 'speckle', mod: 5 + (j % 3), type: 0x11 }], ev: j % 3 === 0 ? [2, 4] : [] });
    });
    // c0 4: the second placement loop refuses its first call; c0 1 with [0x00822c88] >= 8
    add({ n: 2, ctype: 0, kind: 0, c0: 4, g88: 3, ev: [1, 2] }); add({ n: 11, ctype: 2, kind: 1, c0: 4, g88: 5, ev: [1] });
    add({ n: 4, ctype: 0, kind: 0, c0: 1, g88: 8 }); add({ n: 7, ctype: 3, kind: 1, c0: 1, g88: 9 });
    // the 5-group with the placement loop refusing some calls
    [5, 6, 8, 12, 13, 15].forEach(function (c0, j) { add({ n: 1 + j, ctype: (j + 2) % 4, kind: (j + 1) % 2, c0: c0, g88: 1, ev: [2, 3, 4, 6, 7] }); });
    // the preset of [0x004c2fa0] (c2 = 3 keeps it) with low bits set and negative: the rounding of half / quarter for n / 4 = 0, 1, 3
    [[0, 0x13572469], [0, 0xfffffff4], [0, 0xffffffd5], [5, 0x1357246b], [5, 0xfffffff4], [5, 0xfffffff7], [13, 0x13572467],
     [13, 0xfffffff4], [13, 0xfffffff9]].forEach(function (v, j) { add({ n: v[0], c2: 3, c0: 3, ctype: j % 4, kind: j % 3, pre: v[1], g88: 2 + (j % 3) }); });
    // [0x00822c88] == 7 (one pass of the 5-group loop) and many c0 == 8 / 0xc / 0xf passes (Random::range(2) thresholds)
    [1, 5, 6, 8, 12, 13, 15].forEach(function (c0, j) { add({ n: 2 + j, c0: c0, g88: 7, ctype: c0 === 1 ? 0 : (j + 1) % 4, kind: c0 === 1 ? 0 : j % 2 }); });
    for (let j = 0; j < 14; j++) add({ n: 1 + (j % 15), c0: [8, 12, 15][j % 3], g88: 1 + (j % 3), ctype: j % 4, kind: j % 2, c2: j % 4 });
    // many passes of the c0 == 8 stamp block (its Random::range(2) picks the record value 6 / 2)
    for (let j = 0; j < 24; j++) add({ n: 1 + (j % 15), c0: 8, g88: 1, ctype: j % 4, kind: j % 2, c2: j % 4 });
    return rows;
  }
  const ATE_ROWS = ateRows();
  const ATE_N = ATE_ROWS.length;

  // tile-type painting done by the stampCourseTiles stand-in on its first call (a stand-in for the real function's tile writes)
  function paintT(list) {
    list.forEach(function (pt) {
      for (let r = 0; r < 50; r++) {
        for (let c = 0; c < 50; c++) {
          let ty = -1;
          if (pt.rect && r >= pt.rect[0] && r <= pt.rect[1] && c >= pt.rect[2] && c <= pt.rect[3]) ty = pt.type;
          else if (pt.pat === 'speckle' && (r + c) % pt.mod === 0) ty = pt.type;
          else if (pt.pat === 'h16') {
            const h = (Math.imul(r * 50 + c + 1, 0x9e3779b1) >>> 20) % (pt.mod * 2);
            if (h === 0) ty = 0x15; else if (h === 1) ty = 0x16;
          }
          if (ty >= 0) wr8(TILET + r * 50 + c, ty);
        }
      }
    });
  }
  function HB(a, b, salt) {
    const A = Math.trunc(a / 3), B = Math.trunc(b / 3);
    return ((((A * 5 + B * 7 + ((A * B) & 3) + salt) % 9) + 9) % 9) >>> 0;
  }
  function mod(v, m) { return ((v % m) + m) % m; }
  function applyAte(fx, row) {
    const rec = RECS + 46 * row.n;
    randMem(TILEW, 5016, row.seed ^ 0x77, row.wmask);
    randMem(T_LO, T_LEN, row.seed ^ 0x55, 0xff);
    randMem(TB_, 2500, row.seed ^ 0x31, 0xff);
    randMem(Z1, 2500, row.seed ^ 0x32, 0xff);
    randMem(Z2, 2500, row.seed ^ 0x33, 0xff);
    randMem(Z3, 2500, row.seed ^ 0x34, 0xff);
    randMem(Z4, 2500, row.seed ^ 0x35, 0xff);
    randMem(COPY, 2500, row.seed ^ 0x36, 0xff);
    randMem(LVL, 2700, row.seed ^ 0x3c, 0xff);
    randMem(OBJ, OBJ_LEN, row.seed ^ 0x3d, 0xff);
    randMem(WALK, WALK_LEN, row.seed ^ 0x3e, 0xff);
    wr32(0x0059bf90, 0x7777); wr8(0x005a34e0, 0x66); wr32(0x004c2fa0, row.pre);
    [0x00578150, 0x00578154, 0x00578158, 0x0057815c].forEach(function (a, j) { wr32(a, 0x01010101 * (j + 1) + 0x70000000); });
    wr32(G_CAM0, 0x1111); wr32(G_CAM1, 0x2222);
    wr32(0x00822c88, row.g88 >>> 0);
    wr32(G_FLAGS, (0x00ffffff | (row.flag ? 0x01000000 : 0)) >>> 0);
    wr8(0x004c27ec, row.w); wr8(0x005a6d3c, row.src);
    wr32(G_SEED, row.seed);
    // the course record last: records 15 and 16 (0x005722a6, 0x005722d4) overlap the 64 bytes in front of the tile table that
    // randMem(T_LO, ...) filled above
    randMem(rec, 46, row.seed ^ 0x1234, 0xff);
    wr8(rec + 0, row.c0); wr8(rec + 1, row.c1); wr8(rec + 2, row.c2); wr8(rec + 3, row.kind); wr8(rec + 4, row.ctype);
  }
  const ATE_STATE_SAVES = [[RECS, RECS_LEN], [TILEW, 5016], [T_LO, T_LEN], [TB_, 2516], [Z1, 2516], [Z2, 2516], [Z3, 2516], [Z4, 2516],
                           [COPY, 2516], [LVL, 2700], [OBJ, OBJ_LEN], [WALK, WALK_LEN]];

  // `real`: heightBlend, sampleHeight and tileBlocked run for real over the generated terrain; evalPlacementArea stays stood in (the real
  // function refuses every cell of synthetic terrain, and the clubhouse search then never ends: probed, 44 of 46 rows hung) as do the
  // writers (stampCourseTiles, spawnWalker, rebuildTileCoverage, markTileKind11, rebuildHeightfield). The rows are ATE_REAL (indices into ATE_ROWS).
  const ATE_REAL = [0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 20, 22, 24, 26, 27, 29, 32, 33, 35, 37, 41, 44, 48, 50, 52, 56, 60, 64, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 167];
  function ateScene(real) {
    const fx = { clog: newClog(0x2000), clogCap: 0x2000 };
    freezeNow();
    ATE_STATE_SAVES.forEach(function (s) { saveRange(s[0], s[1]); });
    GL.forEach(function (a) { saveRange(a, 4); });
    if (ATE_ROWS.length !== ATE_N) throw new Error('c3ed_ate: row count');
    const src = real ? ATE_REAL.map(function (k) { return ATE_ROWS[k]; }) : ATE_ROWS;
    rowsOn(fx, src.map(function (r) { return Object.assign({}, r, { a: r.n, on: function (f) { applyAte(f, r); } }); }));
    if (!real) {
    calleeStand(fx, 0, 0x0040c170, 3, function (n, a) { return HB(a[0], a[1], cur.hbs + 3 * a[2]); }, true);                   // heightBlend
    calleeStand(fx, 1, 0x0042dba0, 2, function (n, a) {                                                              // sampleHeight
      const x = a[0] >> 7, y = a[1] >> 7;
      if (cur.shmode === 'up') return (10 * x + 100 + mod(y + cur.shs, 5)) >>> 0;                                    // smallest toward row 0
      return (mod(x + y + cur.shs, 11) === 0 && n > 0) ? 1000 : mod(x * 13 + y * 7 + cur.shs + (x ^ y), 5);
    });
    calleeStand(fx, 2, 0x0040bf60, 2, function (n, a) { return (n < 400 && mod(a[0] * 3 + a[1] * 5 + n + cur.tbs, 7) === 0) ? 1 : 0; });   // tileBlocked
    }
    calleeStand(fx, 3, 0x0040db90, 4, function (n, a) { return (cur.ev.indexOf(n) >= 0 && n < 50) ? 0xffffffff : (0x1000 + n) >>> 0; });  // evalPlacementArea
    calleeStand(fx, 4, 0x0040e000, 4, function (n) {                                                                 // stampCourseTiles
      if (n === 0 && cur.paint) paintT(cur.paint);                                                                   // side effect on the tile types
      return (mod(n * 7 + cur.n, 28) + 1) >>> 0;
    });
    calleeStand(fx, 5, 0x0042f340, 0, function (n) { return (0xdd00 + n) >>> 0; });                                  // rebuildTileCoverage
    calleeStand(fx, 6, 0x0042f2c0, 0, function (n) { return (0xcc00 + n) >>> 0; });                                  // markTileKind11
    calleeStand(fx, 7, 0x0042f7a0, 0, function (n) { return (0xf00d00 + n) >>> 0; });                                // rebuildHeightfield
    calleeStand(fx, 8, 0x00402970, 1, function (n) { return cur.wk >>> 0; });                                        // spawnWalker
    return fx;
  }
  F.c3ed_ate = function () { return ateScene(false); };
  F.c3ed_ater = function () { return ateScene(true); };

  globalThis.__C3ED_N = { eth: ETH_N, ate: ATE_N, real: ATE_REAL.length };
  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
