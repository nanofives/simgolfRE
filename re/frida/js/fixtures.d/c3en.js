// c3en fixtures (round 31, 2026-10-13): the report and list screens of golf_clean.exe.
//
// Every key: __modal (re/frida/js/modal.js) stands the C3 pumps in on the A/B thread and delivers a scripted input
// queue through the root window's own handlers; __uiTree (re/frida/js/uitree.js) freezes the game's first thread,
// installs the heap and import stand-ins and puts a recording display at 0x0083ad50; M.display puts modal.js's cursor
// display over it, because these screens pass frame pointers to the display through View_toLocal 0x0047ab50.
// The drawing callees are stood in (calleeStand, c3ea) and their calls, with the text they are given, are compared in
// the fixture's own log region `clog`. Everything else (the string building, appendNumber / appendString / clamp /
// itoa, the table reads and writes, View_toLocal) runs for real.
//
// The data every screen reads is seeded by the fixture under the freeze (saved and restored before the resume) with
// our own synthetic values; no text of the game's own data files is written anywhere.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { (globalThis.__diffKeepAlive || []).push(x); return x; };
  const alloc = function (n) { const p = Memory.alloc(n); p.writeByteArray(new Array(n).fill(0)); return p; };
  const w32 = function (a, v) { ptr(a).writeU32(v >>> 0); };
  const w16 = function (a, v) { ptr(a).writeU16(v & 0xffff); };

  // ---- call log (a state region): +0 records, +4 bytes used, +8 FNV-1a over everything, records from +0x10
  const CLOG = 0x2000;
  const C = { log: null };
  function fnv(h, w) {
    for (let k = 0; k < 4; k++) { h ^= (w >>> (8 * k)) & 0xff; h = Math.imul(h, 0x01000193) >>> 0; }
    return h >>> 0;
  }
  function clog(words) {
    const l = C.log;
    if (l === null) return;
    let h = l.add(8).readU32();
    words.forEach(function (w) { h = fnv(h, w >>> 0); });
    l.add(8).writeU32(h);
    l.writeU32(l.readU32() + 1);
    const used = l.add(4).readU32();
    if (0x10 + used + 4 * words.length <= CLOG) {
      words.forEach(function (w, i) { l.add(0x10 + used + 4 * i).writeU32(w >>> 0); });
      l.add(4).writeU32(used + 4 * words.length);
    }
  }
  // the first n dwords of a NUL-terminated string (a text's ADDRESS can differ between the arms, its bytes cannot)
  function strWords(p, n) {
    const out = new Array(n).fill(0);
    if (ptr(p).isNull()) return out;
    let end = false;
    for (let q = 0; q < 4 * n; q++) {
      const c = end ? 0 : ptr(p).add(q).readU8();
      if (c === 0) end = true;
      out[q >> 2] |= c << (8 * (q & 3));
    }
    return out.map(function (x) { return x >>> 0; });
  }

  // ---- callee stand-ins: A/B thread only, logged in clog, reverted from setTimeout(0) with a per-install token
  const live = {};
  let tok = 1;
  function standIn(specs) {
    const tid = Process.getCurrentThreadId();
    const mine = tok++;
    specs.forEach(function (sp, idx) {
      const at = ptr(sp.addr), k = at.toString();
      if (live[k]) { try { Interceptor.revert(at); } catch (e) { /* gone */ } delete live[k]; }
      const abi = sp.abi || 'mscdecl';
      const types = (abi === 'thiscall' ? ['pointer'] : []).concat(Array(sp.nargs || 0).fill('uint32'));
      const orig = new NativeFunction(at, 'uint32', types, abi);
      let n = 0;
      const cb = new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments);
        if (Process.getCurrentThreadId() !== tid || C.log === null) return orig.apply(null, a);
        const self = abi === 'thiscall' ? ptr(a.shift()).toUInt32() : 0;
        // `this` and arguments that are STACK addresses must be masked: the two arms run the function on their own
        // frames, so a raw stack pointer in the log reads RED although nothing differs (c3el finding 5.5)
        const words = [0x5A000000 | idx, sp.maskSelf ? 0x57ac0000 : self];
        a.forEach(function (v, i) {
          if (sp.str && sp.str[i]) strWords(v, sp.str[i]).forEach(function (w) { words.push(w); });
          else words.push(sp.mask && sp.mask.indexOf(i) >= 0 ? 0x57ac0000 : v);
        });
        clog(words);
        const r = typeof sp.ret === 'function' ? sp.ret(n++, a, self) : (sp.ret || 0);
        return r >>> 0;
      }, 'uint32', types, abi);
      keep(cb);
      Interceptor.replace(at, cb);
      live[k] = mine;
    });
    Interceptor.flush();
    setTimeout(function () {
      Object.keys(live).forEach(function (k) {
        if (live[k] === mine) { try { Interceptor.revert(ptr(k)); } catch (e) { /* gone */ } delete live[k]; }
      });
      Interceptor.flush();
    }, 0);
  }

  // The drawing / window callees of all three screens. Index order is the log tag, so it must not change.
  //   0 Surface_drawSprite 0x00474440 (thiscall, 6)      1 Surface_stretchBlit 0x00473f60 (thiscall, 5)
  //   2 Widget_setQuad 0x00476310 (thiscall, 4)          3 Widget_draw 0x00476650 (thiscall, 5, text)
  //   4 drawStringAt 0x00476700 (thiscall, 5, text)      5 drawStringAt2 0x00476750 (thiscall, 5, text)
  //   6 Image_draw 0x00473e60 (thiscall, 4)              7 Window::refreshFull 0x00480c80 (thiscall, 1)
  //   8 enterModal483d30 0x00483d30 (cdecl, 0)           9 setDrawTarget 0x004762d0 (thiscall, 4)
  //  10 drawCenteredText 0x00404b70 (cdecl, 4, text)    11 drawText4049d0 0x004049d0 (cdecl, 4, text)
  //  12 fillWindowRect 0x00480b00 (thiscall, 5)         13 cellAt 0x0043d6f0 (cdecl, 3) -> a counted value
  //  14 addSprite 0x004628d0 (cdecl, 8)
  const STAND = [
    { addr: 0x00474440, abi: 'thiscall', nargs: 6 },
    { addr: 0x00473f60, abi: 'thiscall', nargs: 5 },
    { addr: 0x00476310, abi: 'thiscall', nargs: 4 },
    { addr: 0x00476650, abi: 'thiscall', nargs: 5, str: { 1: 12 } },
    { addr: 0x00476700, abi: 'thiscall', nargs: 5, str: { 1: 12 } },
    { addr: 0x00476750, abi: 'thiscall', nargs: 5, str: { 1: 12 } },
    { addr: 0x00473e60, abi: 'thiscall', nargs: 4 },
    { addr: 0x00480c80, abi: 'thiscall', nargs: 1 },
    { addr: 0x00483d30, abi: 'mscdecl', nargs: 0 },
    { addr: 0x004762d0, abi: 'thiscall', nargs: 4 },
    { addr: 0x00404b70, abi: 'mscdecl', nargs: 4, str: { 0: 12 } },
    { addr: 0x004049d0, abi: 'mscdecl', nargs: 4, str: { 0: 12 } },
    { addr: 0x00480b00, abi: 'thiscall', nargs: 5 },
    // cellAt's result must depend only on its arguments: a per-call counter would differ between the two arms of a
    // vector (the stand-in is installed once per key and both arms share it), which read RED on state until 2026-10-13
    { addr: 0x0043d6f0, abi: 'mscdecl', nargs: 3,
      ret: function (n, a) { return (0x0ce11000 + ((a[0] & 0xff) << 12) + ((a[1] & 0xff) << 4) + (a[2] & 0xf)) >>> 0; } },
    { addr: 0x004628d0, abi: 'mscdecl', nargs: 8 },
    // 15 buildGolferComment 0x00469b00 (cdecl, 4): a MODEL of what the real one does to the state its caller reads -
    // it appends one piece of text to the buffer 0x0051a068 and leaves a value in [0x0058b198] (the three values the
    // caller's colour test at 0x00454941 / 0x00454951 distinguishes). Standing it in keeps showPlayerCommentsReport's
    // own string and colour logic under test without running the comment builder's golfer-record walk.
    { addr: 0x00469b00, abi: 'mscdecl', nargs: 4, ret: function (n, a) {
      const p = ptr(0x0051a068);
      let e = 0;
      while (p.add(e).readU8() !== 0 && e < 0x100) e++;
      p.add(e).writeUtf8String('c' + (a[0] & 0x3f) + '_' + (a[1] & 0xffff));
      w32(0x0058b198, [0x800023e8, 0x80007d08, 0x80000000][a[0] % 3]);
      return 0;
    } },
    // 16 Surface_box 0x00475b20 (thiscall, 4)   17 Widget_fillRect 0x00478b80 (thiscall, 5): the graph frame and the
    // four curve segments of showHistographScreen. Standing them in puts their geometry in the compared log; left real
    // they would reach the widget's Surface, and the rect pointer one of its slots is given points into the caller's
    // frame, which differs between the two arms (c3el finding 5.5) and reads RED although nothing differs.
    { addr: 0x00475b20, abi: 'thiscall', nargs: 4 },
    { addr: 0x00478b80, abi: 'thiscall', nargs: 5 },
  ];

  // ---- the scene: one __modal + one __uiTree per key, in the order modal.js's contract requires
  function scene(o) {
    o = o || {};
    const fx = { clog: alloc(CLOG), seq: alloc(0x40) };
    fx.clog.add(8).writeU32(0x811c9dc5);
    C.log = fx.clog;
    const M = __modal(Object.assign({ maxCalls: 400, maxFrames: 400 }, o.modal || {}));
    standIn(o.stand || STAND);
    const T = __uiTree({ arenaSize: o.arena || 0x4000, imports: M.imports(), dispRet: o.dispRet || {} });
    __freeze.atFlush(function () { if (C.log === fx.clog) C.log = null; });
    (o.save || []).forEach(function (s) { __freeze.save(ptr(s[0]), s[1]); });
    M.arm();
    M.display(T.disp);
    fx.T = T; fx.M = M;
    Object.assign(fx, T.fx(), M.fx());
    // the cursor offset: View_toLocal 0x0047ab50 writes (cursor - the root window's origin), so a target point needs
    // the virtual cursor target - probe, where probe is what a (0, 0) cursor gives. Measured once per key through the
    // ORIGINAL View_toLocal (the probe must not depend on a hook).
    const px = alloc(4), py = alloc(4);
    fx.M.block.add(0x60).writeS32(0); fx.M.block.add(0x64).writeS32(0);
    T.orig(0x0047ab50, 'void', ['pointer', 'pointer', 'pointer'], 'thiscall')(ptr(0x00519a60), px, py);
    fx.off = [px.readS32(), py.readS32()];
    fx.cur = function (x, y) { return { k: 'cursor', x: x - fx.off[0], y: y - fx.off[1] }; };
    return fx;
  }
  // per-vector rows: fx.r0..rN select the row's script and cursor, write its globals and seed its tables
  function rows(fx, list, seed) {
    list.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, {
        enumerable: true, configurable: true,
        get: function () {
          // the seed runs first: a script can read what it computed (showPlayerCommentsReport's button y)
          Object.keys(row.set || {}).forEach(function (a) { __freeze.save(ptr(+a), 4); w32(+a, row.set[a]); });
          if (seed) seed(fx, row, i);
          fx.M.select(row.script ? row.script(fx) : [], row.cursor ? row.cursor(fx) : [0, 0]);
          return i;
        },
      });
    });
    return fx;
  }

  // input events
  const K = function (code, a) { return { k: 'key', a: a === undefined ? code : a, code: code }; };
  const ESC = K(0x1b), ENTER = K(0x0d);
  const click = function (fx, x, y) { return { k: 'press1', x: x - fx.off[0], y: y - fx.off[1] }; };
  function frames(n) { const s = []; for (let i = 0; i < n; i++) s.push([]); return s; }

  // ====================================================================================================================
  // showFinancialReport 0x0044f6b0
  // The year count [0x005a6d3c] (a short), the short rows at 0x00584210 (stride 0x14), the nine label pointers at
  // 0x004d2054 and the text at 0x004e9a84 that a zero entry copies over the number are all seeded here. The loop reads
  // the cursor through View_toLocal and leaves on a click ([0x00822d68]) or on a key (flushRedraw != 0).
  // Button rect: x 0x2d6..0x301, y 0xf9..0x124 (0x0044fa2d..0x0044fa4b).
  // ====================================================================================================================
  const FIN_TABLE = 0x00584210, FIN_ROWS = 100, FIN_BYTES = FIN_ROWS * 0x14;
  const FIN_SAVE = [[0x005a6d3c, 4], [FIN_TABLE, FIN_BYTES], [0x004d2054, 0x24], [0x004e9a84, 0x10],
                    [0x0051a068, 0x180], [0x0058a528, 0x20], [0x00821c50, 0x20], [0x00569498, 4]];
  // a deterministic value per (year, entry); `kind` picks the pattern
  function finValue(kind, year, i) {
    if (kind === 'zero') return 0;
    if (kind === 'neg') return -(11 * (i + 1) + year * 7);
    if (kind === 'mixed') return ((i + year) % 3 === 0) ? 0 : (((i + year) % 3 === 1) ? (37 * i + year * 13 + 1) : -(5 * i + year * 3 + 1));
    return 101 * i + year * 17 + 1;
  }
  const FIN_ROWSET = [
    // 0 one column, positive values, Esc
    { last: 0, kind: 'pos', script: function () { return [[], [ESC]]; } },
    // 1 four columns, hover the button then click it
    { last: 3, kind: 'pos', script: function (fx) { return [[fx.cur(0x2e0, 0x105)], [], [click(fx, 0x2e0, 0x105)]]; } },
    // 2 nine columns (first = 0)
    { last: 8, kind: 'mixed', script: function () { return [[], [], [ESC]]; } },
    // 3 thirteen years: years 0..3 are skipped (year < first)
    { last: 12, kind: 'mixed', script: function () { return [[], [ESC]]; } },
    // 4 the clamp's high end
    { last: 99, kind: 'pos', script: function () { return [[], [ESC]]; } },
    // 5 a negative year count: clamp gives 0
    { last: -5, kind: 'pos', script: function () { return [[], [ESC]]; } },
    // 6 every entry zero: the text at 0x004e9a84 replaces every number
    { last: 2, kind: 'zero', script: function () { return [[], [ESC]]; } },
    // 7 every entry negative: the second quad colour, and a negative total
    { last: 2, kind: 'neg', script: function () { return [[], [ESC]]; } },
    // 8 a NULL label pointer (entry 3 of 0x004d2054)
    { last: 1, kind: 'pos', nullLabel: 3, script: function () { return [[], [ESC]]; } },
    // 9 hover in, then out again, then Esc
    { last: 1, kind: 'pos', script: function (fx) {
      return [[fx.cur(0x2e8, 0x110)], [], [fx.cur(0x100, 0x110)], [], [ESC]]; } },
    // 10 a click outside the button: the loop still ends (the test is [0x00822d68], not the rect)
    { last: 1, kind: 'mixed', script: function (fx) { return [[], [click(fx, 0x20, 0x20)]]; } },
    // 11 hover, then leave by key with the hover image shown
    { last: 4, kind: 'mixed', script: function (fx) { return [[fx.cur(0x300, 0x124)], [], [], [ENTER]]; } },
    // 12 the cursor on the rect's upper-left corner (the >= sides of all four tests)
    { last: 1, kind: 'pos', script: function (fx) { return [[fx.cur(0x2d6, 0xf9)], [], [ESC]]; } },
    // 13 one pixel past the rect's lower-right corner (the < sides)
    { last: 1, kind: 'pos', script: function (fx) { return [[fx.cur(0x302, 0x125)], [], [ESC]]; } },
    // 14 inside the rect's x range but above it (the y >= 0xf9 side)
    { last: 1, kind: 'pos', script: function (fx) { return [[fx.cur(0x2e0, 0xf0)], [], [ESC]]; } },
    // 15 inside the rect's x range but below it (the y < 0x125 side)
    { last: 1, kind: 'mixed', script: function (fx) { return [[fx.cur(0x2e0, 0x130)], [], [ESC]]; } },
  ];
  function finSeed(fx, row) {
    ptr(0x005a6d3c).writeS16(row.last);
    for (let y = 0; y < FIN_ROWS; y++) {
      for (let i = 0; i < 10; i++) ptr(FIN_TABLE + y * 0x14 + i * 2).writeS16(finValue(row.kind, y, i));
    }
    for (let k = 0; k < 9; k++) w32(0x004d2054 + 4 * k, row.nullLabel === k ? 0 : fx.labels[k].toUInt32());
    ptr(0x004e9a84).writeByteArray([0x2d, 0x2d, 0x2d, 0x00]);     // our own placeholder for a zero entry
    ptr(0x0051a068).writeByteArray(new Array(0x180).fill(0));
    ptr(0x00821c50).writeByteArray(new Array(0x20).fill(0));
    ptr(0x0058a528).writeByteArray(new Array(0x20).fill(0));
    ptr(0x00569498).writeU8(1);
  }
  F.c3en_fin = function () {
    const fx = scene({ save: FIN_SAVE });
    fx.labels = [];
    for (let k = 0; k < 9; k++) fx.labels.push(fx.T.str('lbl' + k));
    return rows(fx, FIN_ROWSET, finSeed);
  };
  // control: no input ever arrives, so the bound must end the key as an ERROR row
  F.c3en_fin_noexit = function () {
    const fx = scene({ save: FIN_SAVE, modal: { maxCalls: 40 } });
    fx.labels = [];
    for (let k = 0; k < 9; k++) fx.labels.push(fx.T.str('lbl' + k));
    return rows(fx, [{ last: 1, kind: 'pos', script: function () { return []; } }], finSeed);
  };

  // ====================================================================================================================
  // showBestScoresScreen 0x00455a30
  // Seeded: the hole count [0x005685f0], the ten score ints 0x0056a524..0x0056a548, the two row-metric globals
  // [0x0082232c] / [0x008222d4], and appendString's offset table 0x0059d81c + id*2 with our own strings in the pool
  // 0x0056fcb0 (ids 0x14..0x1d are the ones row + 0x14 reaches). Button rect: x 0x220..0x24b, y by..by+0x2b where
  // by = [0x0082232c] * rows + [0x008222d4] + 0x3b (0x00455d8c..0x00455dca, 0x00455e2b..0x00455e46).
  // ====================================================================================================================
  const REC_TABLE = 0x0056a524, STR_OFF = 0x0059d81c, STR_POOL = 0x0056fcb0;
  const BEST_SAVE = [[0x005685f0, 4], [REC_TABLE, 0x28], [0x0082232c, 4], [0x008222d4, 4], [STR_OFF + 0x14 * 2, 0x18],
                     [STR_POOL, 0x80], [0x0051a068, 0x180], [0x0058a528, 0x20], [0x00821c50, 0x20], [0x00569498, 4]];
  const BEST_ROWSET = [
    // 0 ten rows, Esc
    { holes: 19, recs: [11, 22, 33, 44, 55, 66, 77, 88, 99, 110], script: function () { return [[], [ESC]]; } },
    // 1 the first entry is 0: no row at all
    { holes: 2, recs: [0, 5, 6, 7, 0, 0, 0, 0, 0, 0], script: function () { return [[], [ESC]]; } },
    // 2 three rows then a 0
    { holes: 4, recs: [7, 8, 9, 0, 1, 2, 3, 4, 5, 6], script: function () { return [[], [ESC]]; } },
    // 3 negative and large values
    { holes: 10, recs: [-1, -2147483648, 2147483647, -1000, 0, 0, 0, 0, 0, 0], script: function () { return [[], [ESC]]; } },
    // 4 every string id missing (offset -1): appendString appends nothing
    { holes: 7, recs: [3, 4, 5, 6, 0, 0, 0, 0, 0, 0], noStrings: true, script: function () { return [[], [ESC]]; } },
    // 5 one row, click the button (hovered first)
    { holes: 1, recs: [42, 0, 0, 0, 0, 0, 0, 0, 0, 0], step: 0x10, base: 0x20,
      script: function (fx) { const y = 0x10 * 1 + 0x20 + 0x3b; return [[fx.cur(0x230, y + 4)], [], [click(fx, 0x230, y + 4)]]; } },
    // 6 hover in and out
    { holes: 5, recs: [1, 2, 0, 0, 0, 0, 0, 0, 0, 0], step: 0x12, base: 0x30,
      script: function (fx) { const y = 0x12 * 2 + 0x30 + 0x3b; return [[fx.cur(0x222, y)], [], [fx.cur(0x10, y)], [], [ESC]]; } },
    // 7 a click with the cursor far away
    { holes: 3, recs: [9, 0, 0, 0, 0, 0, 0, 0, 0, 0], script: function (fx) { return [[], [click(fx, 1, 1)]]; } },
    // 8 zero metrics: every row at the same y
    { holes: 9, recs: [5, 5, 5, 5, 0, 0, 0, 0, 0, 0], step: 0, base: 0, script: function () { return [[], [ESC]]; } },
    // 9 negative metrics
    { holes: 12, recs: [6, 7, 0, 0, 0, 0, 0, 0, 0, 0], step: -3, base: -7, script: function () { return [[], [ESC]]; } },
    // 10 the hole count at 0 (the header number is -1)
    { holes: 0, recs: [8, 0, 0, 0, 0, 0, 0, 0, 0, 0], script: function () { return [[], [ESC]]; } },
    // 11 ten rows with half the string ids missing
    { holes: 18, recs: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10], halfStrings: true, script: function () { return [[], [], [ENTER]]; } },
    // 12 the button's upper-left corner exactly
    { holes: 2, recs: [4, 0, 0, 0, 0, 0, 0, 0, 0, 0], step: 0x14, base: 0x10,
      script: function (fx) { const y = 0x14 + 0x10 + 0x3b; return [[fx.cur(0x220, y)], [], [ESC]]; } },
    // 13 right of the button (the x < 0x24c side)
    { holes: 2, recs: [3, 0, 0, 0, 0, 0, 0, 0, 0, 0], step: 0x11, base: 0x18,
      script: function (fx) { const y = 0x11 + 0x18 + 0x3b; return [[fx.cur(0x260, y + 4)], [], [ESC]]; } },
    // 14 above the button (the y >= by side)
    { holes: 2, recs: [3, 0, 0, 0, 0, 0, 0, 0, 0, 0], step: 0x11, base: 0x18,
      script: function (fx) { const y = 0x11 + 0x18 + 0x3b; return [[fx.cur(0x230, y - 6)], [], [ESC]]; } },
    // 15 below the button (the y < by + 0x2c side)
    { holes: 2, recs: [3, 0, 0, 0, 0, 0, 0, 0, 0, 0], step: 0x11, base: 0x18,
      script: function (fx) { const y = 0x11 + 0x18 + 0x3b; return [[fx.cur(0x230, y + 0x40)], [], [ESC]]; } },
  ];
  function bestSeed(fx, row) {
    w32(0x005685f0, row.holes >>> 0);
    row.recs.forEach(function (v, i) { ptr(REC_TABLE + 4 * i).writeS32(v); });
    w32(0x0082232c, (row.step === undefined ? 0x11 : row.step) >>> 0);
    w32(0x008222d4, (row.base === undefined ? 0x18 : row.base) >>> 0);
    for (let k = 0; k < 10; k++) {
      const missing = row.noStrings || (row.halfStrings && (k & 1));
      ptr(STR_OFF + (0x14 + k) * 2).writeS16(missing ? -1 : k * 8);
    }
    ptr(STR_POOL).writeByteArray(new Array(0x80).fill(0));
    for (let k = 0; k < 10; k++) ptr(STR_POOL + k * 8).writeUtf8String('nm' + k);
    ptr(0x0051a068).writeByteArray(new Array(0x180).fill(0));
    ptr(0x00821c50).writeByteArray(new Array(0x20).fill(0));
    ptr(0x0058a528).writeByteArray(new Array(0x20).fill(0));
    ptr(0x00569498).writeU8(1);
  }
  F.c3en_best = function () {
    const fx = scene({ save: BEST_SAVE });
    return rows(fx, BEST_ROWSET, bestSeed);
  };
  F.c3en_best_noexit = function () {
    const fx = scene({ save: BEST_SAVE, modal: { maxCalls: 40 } });
    return rows(fx, [{ holes: 3, recs: [1, 0, 0, 0, 0, 0, 0, 0, 0, 0], script: function () { return []; } }], bestSeed);
  };

  // ====================================================================================================================
  // showHireEmployeeDialog 0x00459400
  // Seeded: the eight label pointers 0x004c1464..0x004c1480 (our own strings), the eight cost bytes 0x004c2e2c, the
  // eight frame-count ints 0x0053fc20 (the idiv at 0x0045977d faults on 0) and the display's slot +0xb4 result, which
  // picks the highlight colour (0x004596de). The cursor's y picks the row (bands at 0x00459451..0x004594d2) and the
  // return value is (row / 2) * 3 + 1 + (row & 1) after a click (0x00459826), -1 when only a key arrives (0x00459800).
  // ====================================================================================================================
  const HIRE_SAVE = [[0x004c1464, 0x20], [0x004c2e2c, 0x10], [0x0053fc20, 0x20], [0x0051a068, 0x180],
                     [0x0058a528, 0x20], [0x00569498, 4]];
  // y inside each of the eight rows (the eight band halves) and points outside every band
  const HIRE_ROWSET = [
    { y: 0xb0, exit: 'click' },     // 0 row 0
    { y: 0xc5, exit: 'click' },     // 1 row 1
    { y: 0xf5, exit: 'click' },     // 2 row 2
    { y: 0x110, exit: 'click' },    // 3 row 3
    { y: 0x140, exit: 'click' },    // 4 row 4
    { y: 0x155, exit: 'click' },    // 5 row 5
    { y: 0x185, exit: 'click' },    // 6 row 6
    { y: 0x1a0, exit: 'click' },    // 7 row 7
    { y: 0x50, exit: 'click' },     // 8 above every band: row -1, returns 2
    { y: 0x1b8, x: 0x240, exit: 'click' },   // 9 inside the button rect: row -1, the second Image
    { y: 0x1b8, x: 0x100, exit: 'click' },   // 10 in the button's y band but left of it
    { y: 0xab, exit: 'key' },       // 11 row 0 at the band's first pixel, left by a key: -1
    { y: 0xd2, exit: 'key' },       // 12 row 1 at the band's last pixel, left by a key
    { y: 0xbe, exit: 'click', slot: 0 },    // 13 row 0 (sub-band edge) with the display slot +0xb4 not 1
    { y: 0xbf, exit: 'click', slot: 1 },    // 14 row 1 at its first pixel, slot +0xb4 = 1
    { y: 0x16c, exit: 'click', costs: 'neg' },   // 15 row 6 with negative costs
    { y: 0x200, exit: 'click', frames: 'big' },  // 16 below every band, large frame counts
    { y: 0x50, x: 0x240, exit: 'click' },    // 17 the button's x range, above its y band (py >= 0x1a8 false)
    { y: 0x200, x: 0x240, exit: 'click' },   // 18 the button's x range, below its y band (py < 0x1cf false)
    { y: 0x1b8, x: 0x260, exit: 'click' },   // 19 right of the button (px < 0x251 false)
  ];
  function hireSeed(fx, row) {
    for (let k = 0; k < 8; k++) {
      w32(0x004c1464 + 4 * k, fx.labels[k].toUInt32());
      ptr(0x004c2e2c + k).writeS8(row.costs === 'neg' ? -(3 + 5 * k) : (7 + 13 * k));
      w32(0x0053fc20 + 4 * k, row.frames === 'big' ? 1000 + k : 2 + k);
    }
    ptr(0x0051a068).writeByteArray(new Array(0x180).fill(0));
    ptr(0x0058a528).writeByteArray(new Array(0x20).fill(0));
    ptr(0x00569498).writeU8(1);
  }
  function hireScript(row) {
    return function (fx) {
      const x = row.x === undefined ? 0x120 : row.x;
      const ev = row.exit === 'key' ? ENTER : click(fx, x, row.y);
      // frame 0 moves the cursor, frames 1-2 let the dialog redraw with it, frame 3 ends the loop
      return [[fx.cur(x, row.y)], [], [], [ev]];
    };
  }
  // ====================================================================================================================
  // showPlayerCommentsReport 0x004546b0
  // Seeded: the sprite y base [0x008223dc] and the 18 hole records 0x00575c1c..0x00578168 (stride 0x208): the dword at
  // +0xbc of each record (the record base 0x00575cd8 is the sum source while kind is 0, 0x0045481e), the 64 count shorts
  // at +0x174 (0x00575d90, 0x0045480d) and the 64 words at +0x14c (0x00575c1c + (hole + 1) * 0x208, 0x004548fa) that
  // give buildGolferComment its argument. Button rect: x 0x24f..0x27a, y by..by+0x2b where by follows the last drawn row.
  // ====================================================================================================================
  const HOLE_BASE = 0x00575c1c, HOLE_STRIDE = 0x208, HOLES = 18;
  const PC_SAVE = [[0x008223dc, 4], [HOLE_BASE, HOLES * HOLE_STRIDE + 0x100], [0x0058b198, 4], [0x0051a068, 0x180],
                   [0x0058a528, 0x20], [0x00821c50, 0x20], [0x00569498, 4]];
  // counts[kind][hole] from the row's `fill` function; the dword of each record from `weight`
  const PC_ROWSET = [
    // 0 one kind on three holes: one drawn row
    { fill: function (k, h) { return k === 5 && h < 3 ? 2 + h : 0; }, weight: function () { return 4; },
      script: function () { return [[], [ESC]]; } },
    // 1 five kinds with different sums: five rows, largest first
    { fill: function (k, h) { return k < 5 && h === k ? 10 - k : 0; }, weight: function (h) { return h + 1; },
      script: function () { return [[], [], [ESC]]; } },
    // 2 every record weight 0: the total is 0 and nothing is drawn
    { fill: function (k, h) { return k === 1 ? h : 0; }, weight: function () { return 0; },
      script: function () { return [[], [ESC]]; } },
    // 3 every count 0: no row finds a positive count
    { fill: function () { return 0; }, weight: function () { return 7; },
      script: function () { return [[], [ESC]]; } },
    // 4 counts only in kinds >= 0x32: the k < 0x32 guard excludes them all
    { fill: function (k, h) { return k >= 0x32 && h < 4 ? 5 : 0; }, weight: function () { return 3; },
      script: function () { return [[], [ESC]]; } },
    // 5 25 kinds with a count: the 20-row cap stops the list
    { fill: function (k, h) { return k < 25 && h === (k % HOLES) ? 25 - k : 0; }, weight: function () { return 2; },
      script: function () { return [[], [ESC]]; } },
    // 6 negative counts: their sums stay below the running maximum
    { fill: function (k, h) { return k === 3 ? -(h + 1) : (k === 7 && h === 0 ? 1 : 0); }, weight: function () { return 5; },
      script: function () { return [[], [ESC]]; } },
    // 7 a count larger than the total: the percentage goes over 100
    { fill: function (k, h) { return k === 2 && h === 0 ? 500 : 0; }, weight: function (h) { return h === 0 ? 1 : 0; },
      script: function () { return [[], [ESC]]; } },
    // 8 two kinds tied on the same sum: the first kind wins (counts[k] > max, not >=)
    { fill: function (k, h) { return (k === 4 || k === 9) && h === 1 ? 6 : 0; }, weight: function () { return 6; },
      script: function () { return [[], [], [ESC]]; } },
    // 9 one row, hover the button then click it
    { fill: function (k, h) { return k === 1 && h === 2 ? 3 : 0; }, weight: function () { return 9; }, by: 1,
      script: function (fx) { return [[fx.cur(0x260, fx.byPc + 4)], [], [click(fx, 0x260, fx.byPc + 4)]]; } },
    // 10 hover in and out again
    { fill: function (k, h) { return k === 0 && h === 5 ? 4 : 0; }, weight: function () { return 8; }, by: 1,
      script: function (fx) { return [[fx.cur(0x250, fx.byPc)], [], [fx.cur(0x10, fx.byPc)], [], [ESC]]; } },
    // 11 the counts of every kind on every hole: twenty rows, all of them drawn
    { fill: function (k, h) { return ((k * 7 + h * 3) % 11) + 1; }, weight: function (h) { return h * 2 + 1; },
      script: function () { return [[], [ENTER]]; } },
    // 12 a click away from the button
    { fill: function (k, h) { return k === 6 && h === 9 ? 2 : 0; }, weight: function () { return 1; },
      script: function (fx) { return [[], [click(fx, 2, 3)]]; } },
    // 13 right of the button (the x < 0x27b side)
    { fill: function (k, h) { return k === 2 && h === 1 ? 5 : 0; }, weight: function () { return 4; }, by: 1,
      script: function (fx) { return [[fx.cur(0x290, fx.byPc + 4)], [], [ESC]]; } },
    // 14 above the button (the y >= by side)
    { fill: function (k, h) { return k === 2 && h === 1 ? 5 : 0; }, weight: function () { return 4; }, by: 1,
      script: function (fx) { return [[fx.cur(0x260, fx.byPc - 8)], [], [ESC]]; } },
    // 15 below the button (the y < by + 0x2c side)
    { fill: function (k, h) { return k === 2 && h === 1 ? 5 : 0; }, weight: function () { return 4; }, by: 1,
      script: function (fx) { return [[fx.cur(0x260, fx.byPc + 0x40)], [], [ESC]]; } },
  ];
  function pcSeed(fx, row) {
    w32(0x008223dc, (row.y0 === undefined ? 0x30 : row.y0) >>> 0);
    for (let h = 0; h < HOLES; h++) {
      const rec = HOLE_BASE + (h + 1) * HOLE_STRIDE;        // the record whose counts belong to hole h + 1
      ptr(HOLE_BASE + 0xbc + h * HOLE_STRIDE).writeS32(row.weight(h));
      for (let k = 0; k < 0x40; k++) {
        ptr(HOLE_BASE + 0x174 + h * HOLE_STRIDE + k * 2).writeS16(row.fill(k, h));
        ptr(rec + k * 2).writeU16((0x1000 + k * 3 + h) & 0xffff);
      }
    }
    w32(0x0058b198, 0);
    ptr(0x0051a068).writeByteArray(new Array(0x180).fill(0));
    ptr(0x00821c50).writeByteArray(new Array(0x20).fill(0));
    ptr(0x0058a528).writeByteArray(new Array(0x20).fill(0));
    ptr(0x00569498).writeU8(1);
    // the button's y, for the rows whose script points at it: [0x008223dc] + 0x16 + 0xf per drawn row + 0xe
    if (row.by) {
      let drawn = 0, total = 0;
      const sums = [];
      for (let k = 0; k < 0x40; k++) {
        let s = 0;
        for (let h = 0; h < HOLES; h++) s += row.fill(k, h);
        sums.push(k < 0x32 ? s : 0);
      }
      for (let h = 0; h < HOLES; h++) total += row.weight(h);
      for (let r = 0; r < 20 && total !== 0; r++) {
        let bestK = -1, max = 0;
        for (let k = 0; k < 0x40; k++) if (sums[k] > max) { max = sums[k]; bestK = k; }
        if (bestK < 0) break;
        sums[bestK] = 0;
        drawn++;
      }
      fx.byPc = (row.y0 === undefined ? 0x30 : row.y0) + 0x16 + 0xf * drawn + 0xe;
    }
  }
  F.c3en_pc = function () {
    const fx = scene({ save: PC_SAVE });
    fx.byPc = 0x54;
    return rows(fx, PC_ROWSET, pcSeed);
  };
  F.c3en_pc_noexit = function () {
    const fx = scene({ save: PC_SAVE, modal: { maxCalls: 40 } });
    return rows(fx, [{ fill: function () { return 0; }, weight: function () { return 1; },
                       script: function () { return []; } }], pcSeed);
  };

  // ====================================================================================================================
  // showHistographScreen 0x00455ed0
  // Seeded: the cash [0x00571fd4] and skill [0x00541cd8] totals that pick the two axis scales, the date [0x00834170]
  // (its 1024ths are the number of weekly columns), the four short series 0x005409ae / 0x0051b38a / 0x0053e63e /
  // 0x00520642 and the event words 0x00568602, plus the three event text tables 0x004c26b0 (20 bytes per id),
  // 0x0058dd50 (56) and 0x0055d738 (0x25) and the pointer 0x004c2c18. [0x00519cd8] is the screen surface whose slot
  // +0xc4 is called after the last column (0x00456918): NULL in most rows, a recording Surface in one.
  // Button rect: x 0x2c0..0x2eb, y 0x227..0x252.
  // ====================================================================================================================
  const WK = 24;                                     // columns the fixture seeds at most
  const HG_SAVE = [[0x00571fd4, 4], [0x00541cd8, 4], [0x00834170, 4], [0x005409ae, 2 * WK], [0x0051b38a, 2 * WK],
                   [0x0053e63e, 2 * WK], [0x00520642, 2 * WK], [0x00568602, 2 * WK], [0x004c26b0, 0x20 * 20],
                   [0x0058dd50, 0x20 * 56], [0x0055d738, 0x20 * 0x25], [0x004c2c18, 4], [0x00519cd8, 4],
                   [0x0051a068, 0x180], [0x0058a528, 0x20], [0x00821c50, 0x20], [0x00569628, 4], [0x00569498, 4]];
  const HG_ROWSET = [
    // 0 two columns, small values, no event, Esc
    { cash: 100, skill: 100, weeks: 2, series: function (i, w) { return 10 * (i + 1) + w; }, events: function () { return 0; },
      script: function () { return [[], [ESC]]; } },
    // 1 the cash scale's second step and an event of kind 0x20 on every column
    { cash: 3000, skill: 100, weeks: 3, series: function (i, w) { return 40 * i + w * 3; },
      events: function (w) { return 0x20 + (w % 0x20); }, script: function () { return [[], [ESC]]; } },
    // 2 both scales at their top step, values above 0x1f4 (the tenth-and-offset arm)
    { cash: 200000, skill: 5000, weeks: 4, series: function (i, w) { return 600 + 300 * i + 50 * w; },
      events: function (w) { return 0x40 + (w % 0x10); }, script: function () { return [[], [ESC]]; } },
    // 3 negative series values (the cash colour and the absolute-value arm)
    { cash: 6000, skill: 3000, weeks: 4, series: function (i, w) { return -(100 * (i + 1) + 7 * w); },
      events: function (w) { return 0x60 + w; }, script: function () { return [[], [ESC]]; } },
    // 4 kind 0x80 and 0xa0
    { cash: 11000, skill: 100, weeks: 4, series: function (i, w) { return (w & 1) ? -20 * i : 30 * i + w; },
      events: function (w) { return (w & 1) ? 0x80 + w : 0xa0 + w; }, script: function () { return [[], [ESC]]; } },
    // 5 kind 0xc0 (the only equal-compare arm) and 0xe0
    { cash: 30000, skill: 2600, weeks: 4, series: function (i, w) { return 505 + i * 2 + w; },
      events: function (w) { return (w & 1) ? 0xc0 + w : 0xe0 + w; }, script: function () { return [[], [ESC]]; } },
    // 6 kinds 0x100, 0x120, 0x140 (landmarkName) and 0x160
    { cash: 60000, skill: 100, weeks: 4, series: function (i, w) { return 3 * i + w; },
      events: function (w) { return [0x100, 0x120, 0x140, 0x160][w % 4] + (w % 8); },
      script: function () { return [[], [ESC]]; } },
    // 7 kinds outside both windows: the buffer keeps the previous event's text
    { cash: 100, skill: 100, weeks: 5, series: function (i, w) { return 11 * i + w; },
      events: function (w) { return w === 0 ? 0x20 + 3 : (w === 1 ? 0x1c0 + w : (w === 2 ? -1 : 0x180 + w)); },
      script: function () { return [[], [ESC]]; } },
    // 8 a long history with a mixture: the event marker walks and wraps
    { cash: 26000, skill: 2500, weeks: 20, series: function (i, w) { return (i === 1 ? -1 : 1) * (37 * i + 13 * w); },
      events: function (w) { return 0x20 * (1 + (w % 7)) + (w % 0x20); },
      script: function () { return [[], [ESC]]; } },
    // 9 the screen surface present: its slot +0xc4 runs
    { cash: 100, skill: 100, weeks: 2, series: function (i, w) { return 5 * i + w; }, events: function () { return 0; },
      surface: true, script: function () { return [[], [ESC]]; } },
    // 10 no week at all (the date below one week): only the axis is drawn
    { cash: 100, skill: 100, weeks: 0, series: function () { return 0; }, events: function () { return 0; },
      script: function () { return [[], [ESC]]; } },
    // 11 hover the button and click it
    { cash: 100, skill: 100, weeks: 3, series: function (i, w) { return 9 * i + w; }, events: function () { return 0; },
      script: function (fx) { return [[fx.cur(0x2d0, 0x230)], [], [click(fx, 0x2d0, 0x230)]]; } },
    // 12 hover in and out
    { cash: 100, skill: 100, weeks: 2, series: function (i, w) { return 4 * i + w; }, events: function () { return 0; },
      script: function (fx) { return [[fx.cur(0x2c0, 0x227)], [], [fx.cur(0x2ec, 0x253)], [], [ENTER]]; } },
    // 13 the cash scale's 1388 and 2710 steps with a zero skill series
    { cash: 20000, skill: 2500, weeks: 3, series: function (i, w) { return i === 0 ? 0 : 60 * i + w; },
      events: function (w) { return w === 1 ? 0xa0 + 0x1f : 0; }, script: function () { return [[], [ESC]]; } },
    // 14 both scales at 1 with series above 0x1f4: the tenth-and-offset arm of the first two curves
    { cash: 100, skill: 100, weeks: 3, series: function (i, w) { return 1500 + 200 * i + 90 * w; },
      events: function () { return 0; }, script: function () { return [[], [ESC]]; } },
    // 15 above the button (the y >= 0x227 side)
    { cash: 100, skill: 100, weeks: 2, series: function (i, w) { return 6 * i + w; }, events: function () { return 0; },
      script: function (fx) { return [[fx.cur(0x2d0, 0x200)], [], [ESC]]; } },
    // 16 below the button (the y < 0x253 side)
    { cash: 100, skill: 100, weeks: 2, series: function (i, w) { return 7 * i + w; }, events: function () { return 0; },
      script: function (fx) { return [[fx.cur(0x2d0, 0x270)], [], [ESC]]; } },
  ];
  function hgSeed(fx, row) {
    w32(0x00571fd4, row.cash >>> 0);
    w32(0x00541cd8, row.skill >>> 0);
    w32(0x00834170, (row.weeks * 1024) >>> 0);
    for (let w = 0; w < WK; w++) {
      ptr(0x005409ae + 2 * w).writeS16(row.series(0, w));
      ptr(0x0051b38a + 2 * w).writeS16(row.series(1, w));
      ptr(0x0053e63e + 2 * w).writeS16(row.series(2, w));
      ptr(0x00520642 + 2 * w).writeS16(row.series(3, w) & 0x7f);     // the fourth series is drawn times four
      ptr(0x00568602 + 2 * w).writeS16(row.events(w));
    }
    for (let k = 0; k < 0x20; k++) {
      ptr(0x004c26b0 + k * 20).writeUtf8String('t40_' + k);
      ptr(0x0058dd50 + k * 56).writeUtf8String('ta0_' + k);
      ptr(0x0055d738 + k * 0x25).writeUtf8String('tc0_' + k);
    }
    w32(0x004c2c18, fx.ptrText.toUInt32());
    w32(0x00519cd8, row.surface ? fx.surf.toUInt32() : 0);
    ptr(0x0051a068).writeByteArray(new Array(0x180).fill(0));
    ptr(0x00821c50).writeByteArray(new Array(0x20).fill(0));
    ptr(0x0058a528).writeByteArray(new Array(0x20).fill(0));
    w32(0x00569628, 0);
    ptr(0x00569498).writeU8(1);
  }
  F.c3en_hg = function () {
    const fx = scene({ save: HG_SAVE, arena: 0x8000 });
    fx.ptrText = fx.T.str('ptr100');
    fx.surf = fx.T.surface();
    return rows(fx, HG_ROWSET, hgSeed);
  };
  F.c3en_hg_noexit = function () {
    const fx = scene({ save: HG_SAVE, arena: 0x8000, modal: { maxCalls: 40 } });
    fx.ptrText = fx.T.str('ptr100');
    fx.surf = fx.T.surface();
    return rows(fx, [{ cash: 100, skill: 100, weeks: 1, series: function () { return 1; },
                       events: function () { return 0; }, script: function () { return []; } }], hgSeed);
  };

  // ====================================================================================================================
  // selectDifficultyScreen 0x0043a400
  // Its five Surfaces and its Image live on the CALLER's stack, so the stand-ins of the calls that take them
  // (loadImage, Surface_blit, Surface_blitThunk and Image_draw) mask `this` and the object arguments: the two arms run
  // on their own frames. Nothing else is seeded - the screen reads only the cursor. The hit test of item k is
  // approxDistance((x - cx) / 2, y - cy) < 0x50 with the centres below (0x0043a718..0x0043a7b7), and the cancel button
  // is approxDistance(x - 0x2ff, y - 0x22d) < 0x19 (0x0043a7bc..0x0043a7de).
  // ====================================================================================================================
  const SD_CENTRES = [[0x15c, 0x6b], [0x13f, 0xda], [0x140, 0x16f], [0x16b, 0x1ef]];
  const SD_CANCEL = [0x2ff, 0x22d];
  // STAND with Image_draw masked (its `this` is a stack Surface here) plus the four calls only this screen makes
  const SD_STAND = STAND.map(function (sp) {
    return sp.addr === 0x00473e60 ? Object.assign({}, sp, { maskSelf: true }) : sp;
  }).concat([
    { addr: 0x00475840, abi: 'thiscall', nargs: 5, maskSelf: true, str: { 0: 10 } },   // loadImage (the asset name)
    { addr: 0x00473bf0, abi: 'thiscall', nargs: 7, maskSelf: true, mask: [0] },        // Surface_blit
    { addr: 0x00475c60, abi: 'thiscall', nargs: 7, maskSelf: true },                   // Surface_blitThunk
    { addr: 0x00432620, abi: 'mscdecl', nargs: 2 },                                    // drawPanelBackdrop
  ]);
  const SD_SAVE = [[0x0051a068, 0x180], [0x00569498, 4]];
  // a point `d` away from item k's centre in the test's units (x counts double, 0x0043a71c / 0x0043a74c)
  function sdAt(fx, k, dx, dy) {
    const c = k === 4 ? SD_CANCEL : SD_CENTRES[k];
    const sx = k === 4 ? 1 : 2;
    return fx.cur(c[0] + sx * (dx || 0), c[1] + (dy || 0));
  }
  function sdHold(n) { const s = []; for (let i = 0; i < n; i++) s.push([]); return s; }
  const SD_ROWSET = [
    // 0..3 one per difficulty, clicked on its centre: returns 0, 1, 2, 3
    { item: 0, exit: 'click' }, { item: 1, exit: 'click' }, { item: 2, exit: 'click' }, { item: 3, exit: 'click' },
    // 4 the cancel button clicked: the hovered item 4 is turned into -1 (0x0043a86b..0x0043a870)
    { item: 4, exit: 'click' },
    // 5 nothing hovered, clicked: -1
    { item: -1, exit: 'click' },
    // 6 item 0 hovered, left by a key: -1
    { item: 0, exit: 'key' },
    // 7 item 2 then item 3 (the highlight changes, the dwell counter resets), clicked: 3
    { item: 2, then: 3, exit: 'click' },
    // 8 the cancel button held for 32 frames (the dwell > 0x1e arm) then a key
    { item: 4, hold: 32, exit: 'key' },
    // 9 the same held and then clicked
    { item: 4, hold: 32, exit: 'click' },
    // 10 exactly 0x50 below item 0's centre: its own test fails (approxDistance returns 0x50, not < 0x50) and the
    // point falls inside item 1's radius, so the result is 1
    { item: 0, off: [0, 0x50], exit: 'click' },
    // 11 inside item 1's radius but off centre
    { item: 1, off: [0x20, 0x10], exit: 'click' },
    // 12 nothing hovered, left by a key
    { item: -1, exit: 'key' },
    // 13 item 3 hovered, then the cursor leaves, then a click: -1
    { item: 3, then: -1, exit: 'click' },
    // 14 one pixel inside item 2's radius, which is also inside item 3's: the later test wins and the result is 3
    { item: 2, off: [0, 0x4f], exit: 'click' },
    // 15 the cancel button held for exactly 0x1e frames: the dwell arm does not run
    { item: 4, hold: 30, exit: 'key' },
    // 16 item 0 held for 32 frames: the dwell counter passes 0x1e on an item that is not the cancel button
    { item: 0, hold: 32, exit: 'click' },
  ];
  function sdScript(row) {
    return function (fx) {
      const away = fx.cur(5, 7);
      const first = row.item < 0 ? away : sdAt(fx, row.item, (row.off || [])[0], (row.off || [])[1]);
      const s = [[first]].concat(sdHold(row.hold || 1));
      if (row.then !== undefined) s.push([row.then < 0 ? away : sdAt(fx, row.then)], []);
      const p = row.item < 0 ? [5, 7] : (row.item === 4 ? SD_CANCEL : SD_CENTRES[row.item]);
      s.push([row.exit === 'key' ? ENTER : click(fx, p[0], p[1])]);
      return s;
    };
  }
  F.c3en_sd = function () {
    const fx = scene({ save: SD_SAVE, stand: SD_STAND, modal: { maxCalls: 400, maxFrames: 400 } });
    return rows(fx, SD_ROWSET.map(function (r) { return Object.assign({}, r, { script: sdScript(r) }); }),
                function () {
                  ptr(0x0051a068).writeByteArray(new Array(0x180).fill(0));
                  ptr(0x00569498).writeU8(1);
                });
  };
  F.c3en_sd_noexit = function () {
    const fx = scene({ save: SD_SAVE, stand: SD_STAND, modal: { maxCalls: 60 } });
    return rows(fx, [{ script: function (fx2) { return [[sdAt(fx2, 0)]]; } }],
                function () { ptr(0x0051a068).writeByteArray(new Array(0x180).fill(0)); });
  };

  // ====================================================================================================================
  // showCreditsScreen 0x0044b9c0
  // The two Surfaces and the Image live on the CALLER's stack, so every stand-in that is given one of them masks `this`
  // and the object arguments. The three reader entry points are MODELS: readSectionGlobal returns the row's result,
  // readLineGlobal writes the row's next line into a fixture buffer, points [0x0083b164] at it and returns it, and
  // destroyGlobalReader records the close. Its position is the counter at `seq` + 0, a compared state region, so both
  // arms of a vector read the same lines (a counter in JavaScript would not be reset between them). loadImage's model
  // installs a recording Surface at Image + 4 for the rows that want one (that is what the real loader leaves there,
  // and the two slots +0xdc / +0xd8 the screen then calls are its height and width, 0x0044ba9b / 0x0044bab0), and
  // Surface_blitTakeDirty's model writes the width it is given into Surface + 0x10, which is what centres the logo
  // (0x0044bc89). The constructors and destructors of the three objects run for real.
  // ====================================================================================================================
  const CR = { lines: [], buf: null, surf: null, seq: null, imgSurf: false, w: 0, h: 0, open: 0 };
  // the Image's own Surface: slot +0xdc is its height and +0xd8 its width (0x0044ba9b / 0x0044bab0). Its vtable is
  // built at load time (a NativeCallback cannot be made while a key has the first thread frozen) and its two slots
  // log into the same call log as the stand-ins.
  const CR_VT = keep(Memory.alloc(0x200));
  [0xd8, 0xdc].forEach(function (slot) {
    CR_VT.add(slot).writePointer(keep(new NativeCallback(function (self) {
      clog([0x5C000000 | slot, 0]);
      return (slot === 0xd8 ? CR.w : CR.h) >>> 0;
    }, 'uint32', ['pointer'], 'thiscall')));
  });
  const CR_STAND = STAND.map(function (sp) {
    return sp.addr === 0x00473e60 ? Object.assign({}, sp, { maskSelf: true }) : sp;
  }).concat([
    { addr: 0x00475840, abi: 'thiscall', nargs: 5, maskSelf: true, str: { 0: 10 },
      ret: function (n, a, self) {
        if (CR.imgSurf) ptr(self).add(4).writePointer(CR.surf);     // what the real loader leaves at Image + 4
        return 0;
      } },
    { addr: 0x00474dd0, abi: 'thiscall', nargs: 6, maskSelf: true },                 // Surface_createFull
    { addr: 0x00473c60, abi: 'thiscall', nargs: 7, maskSelf: true, mask: [0],        // Surface_blitTakeDirty
      ret: function (n, a, self) { ptr(self).add(0x10).writeS32(a[3]); return 0; } },
    { addr: 0x00474cb0, abi: 'thiscall', nargs: 0, maskSelf: true },                 // Image_releaseAll
    { addr: 0x00477c30, abi: 'thiscall', nargs: 4, str: { 0: 12 } },                 // drawText
    { addr: 0x00473df0, abi: 'thiscall', nargs: 4, maskSelf: true },                 // Surface_drawRegion
    { addr: 0x00487e70, abi: 'mscdecl', nargs: 2, str: { 0: 4, 1: 3 },               // readSectionGlobal
      ret: function () { return CR.open; } },
    { addr: 0x00487e90, abi: 'mscdecl', nargs: 0,                                    // readLineGlobal
      ret: function () {
        const k = CR.seq.readU32();
        CR.seq.writeU32(k + 1);
        const text = k < CR.lines.length ? CR.lines[k] : '#end';
        CR.buf.writeByteArray(new Array(0x40).fill(0));
        CR.buf.writeUtf8String(text);
        w32(0x0083b164, CR.buf.toUInt32());
        return CR.buf.toUInt32();
      } },
    { addr: 0x00487e60, abi: 'mscdecl', nargs: 0 },                                  // destroyGlobalReader
  ]);
  const CR_SAVE = [[0x0083b164, 4], [0x00821028, 4], [0x0082102c, 4], [0x00821030, 4]];
  const CR_ROWSET = [
    // 0 the reader refuses the section: nothing else runs
    { open: 1, lines: [] },
    // 1 the first line already starts with '#': no line is kept and the scroll loop is skipped
    { open: 0, lines: ['#only'] },
    // 2 three lines, left by a key
    { open: 0, lines: ['alpha', 'beta', 'gamma'], exit: 'key' },
    // 3 the logo line and an empty line among the text
    { open: 0, lines: ['one', '$bink', '', '!low', 'two'], exit: 'key' },
    // 4 the same with a surface on the Image: the logo gets a width and is centred
    { open: 0, lines: ['x', '$bink', 'y'], imgSurf: true, w: 0x80, h: 0x40, exit: 'key' },
    // 5 left by a click
    { open: 0, lines: ['click me', 'second'], exit: 'click' },
    // 6 ten lines: the inner loop fills the screen and breaks on y >= 0x258
    { open: 0, lines: ['l0', 'l1', 'l2', 'l3', 'l4', 'l5', 'l6', 'l7', 'l8', 'l9'], exit: 'key' },
    // 7 no input at all: the scroll ends by itself when the list has passed the top (-y > total)
    { open: 0, lines: ['end0', 'end1'], exit: 'none' },
    // 8 the second line's allocation fails: it is not counted
    { open: 0, lines: ['kept', 'lost', 'also kept'], failAfter: 1, exit: 'key' },
    // 9 a line equal to the logo text but with a surface absent: the logo is drawn at the full width
    { open: 0, lines: ['$bink'], exit: 'key' },
    // 10 a long line and a large line height
    { open: 0, lines: ['a long credit line of text', 'short'], lineH: 0x20, exit: 'key' },
    // 11 the negative line-height branch of 0x0044bb9b ([0x00821028] below zero)
    { open: 0, lines: ['neg0', 'neg1'], negH: true, exit: 'key' },
  ];
  function crSeed(fx, row) {
    CR.lines = row.lines;
    CR.open = row.open;
    CR.imgSurf = !!row.imgSurf;
    CR.buf = fx.linebuf;
    CR.seq = fx.seq;
    fx.seq.writeU32(0);
    w32(0x0083b164, 0);
    if (row.negH) {
      ptr(0x00821028).writeS32(-1);
      ptr(0x0082102c).writeS32(row.lineH || 0x11);
      ptr(0x00821030).writeS32(7);
    } else {
      ptr(0x00821028).writeS32(5);
      ptr(0x0082102c).writeS32(0x33);
      ptr(0x00821030).writeS32((row.lineH || 0x11) - 5);
    }
    CR.w = row.w || 0;
    CR.h = row.h || 0;
    if (row.failAfter !== undefined) fx.T.failAfter(row.failAfter);
  }
  function crScript(row) {
    return function (fx) {
      if (row.exit === 'none' || row.exit === undefined) return [];
      const ev = row.exit === 'key' ? ENTER : click(fx, 100, 100);
      return [[], [], [], [ev]];
    };
  }
  F.c3en_cr = function () {
    const fx = scene({ save: CR_SAVE, stand: CR_STAND, arena: 0x8000,
                       modal: { maxCalls: 400, maxFrames: 400, frameMs: 600 } });
    fx.linebuf = fx.T.alloc(0x40);
    fx.imgSurfObj = fx.T.alloc(0x20);
    fx.imgSurfObj.writePointer(CR_VT);
    CR.surf = fx.imgSurfObj;
    return rows(fx, CR_ROWSET.map(function (r) { return Object.assign({}, r, { script: crScript(r) }); }), crSeed);
  };
  F.c3en_cr_noexit = function () {
    const fx = scene({ save: CR_SAVE, stand: CR_STAND, arena: 0x8000,
                       modal: { maxCalls: 40, maxFrames: 400, frameMs: 1 } });
    fx.linebuf = fx.T.alloc(0x40);
    fx.imgSurfObj = fx.T.alloc(0x20);
    fx.imgSurfObj.writePointer(CR_VT);
    CR.surf = fx.imgSurfObj;
    return rows(fx, [{ open: 0, lines: ['x', 'y'], exit: 'none' }].map(function (r) {
      return Object.assign({}, r, { script: crScript(r) });
    }), crSeed);
  };

  let HIRE_SLOT = 1;                       // the display slot +0xb4 result of the row being run
  F.c3en_hire = function () {
    const fx = scene({ save: HIRE_SAVE, dispRet: { 0xb4: function () { return HIRE_SLOT; } } });
    fx.labels = [];
    for (let k = 0; k < 8; k++) fx.labels.push(fx.T.str('emp' + k));
    return rows(fx, HIRE_ROWSET.map(function (r) { return Object.assign({}, r, { script: hireScript(r) }); }),
                function (f, row) { HIRE_SLOT = row.slot === undefined ? 1 : row.slot; hireSeed(f, row); });
  };
  F.c3en_hire_noexit = function () {
    const fx = scene({ save: HIRE_SAVE, modal: { maxCalls: 60 }, dispRet: { 0xb4: function () { return 1; } } });
    fx.labels = [];
    for (let k = 0; k < 8; k++) fx.labels.push(fx.T.str('emp' + k));
    return rows(fx, [{ y: 0xb0, script: function () { return []; } }], hireSeed);
  };
})();
