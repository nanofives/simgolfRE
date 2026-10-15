// Fixtures for batch c3fr (round 35, 2026-10-14): golf_clean.exe's large screens left after round 34.
// Reimplementations: shim/src/re/c3fr.cpp. Registry: re/frida/registry.d/c3fr.py.
// Purposes and per-jcc tables: log/c3/c3fr_purpose.md. Notes: log/c3/c3fr_notes.txt.
(function () {
  const F = {};

  function zalloc(n) {
    const p = __keepAlloc(n);
    for (let i = 0; i < n; i += 4) p.add(i).writeU32(0);
    return p;
  }
  function fnvWord(h, w) {
    for (let k = 0; k < 4; k++) { h ^= (w >>> (8 * k)) & 0xff; h = Math.imul(h, 0x01000193) >>> 0; }
    return h >>> 0;
  }
  function wr32(a, v) { ptr(a).writeU32(v >>> 0); }
  function wr16(a, v) { ptr(a).writeU16(v & 0xffff); }
  function wr8(a, v) { ptr(a).writeU8(v & 0xff); }
  function rd32(a) { return ptr(a).readU32(); }
  function wrStr(a, s, max) {
    const p = ptr(a);
    for (let i = 0; i < max; i++) p.add(i).writeU8(i < s.length ? s.charCodeAt(i) & 0xff : 0);
  }
  function press(x, y) { return { k: 'press1', x: x, y: y }; }
  function press2(x, y) { return { k: 'press2', x: x, y: y }; }
  function raw(set) { return { k: 'raw', set: set }; }

  // ---- negative controls (log/c3/alt/c3fr_build.txt `negctl`): word 2 of the exported C3frNegCtl switch picks which
  // single wrong detail is live. fx.n<j> selects edit table[j][0] and then applies scene table[j][1] through the key's
  // own getter (base + scene). In the normal build there is no such export and these getters only apply the scene. The
  // tables are mirrored in re/frida/registry.d/c3fr.py.
  function negSel(k) {
    (__freeze.negCtlWords() || []).forEach(function (a) { __freeze.save(a.add(8), 4); a.add(8).writeU32(k); });
  }
  function negGetters(fx, table, base) {
    table.forEach(function (p, j) {
      Object.defineProperty(fx, 'n' + j, { enumerable: true, configurable: true, get: function () {
        negSel(p[0]);
        return fx[base + p[1]];
      } });
    });
  }

  // ====================================================================================================================
  // shared: calleeStand (c3et.js's copy of c3er.js's, with Interceptor.replaceFast where it exists so a foreign thread
  // reaches the real body through the trampoline instead of re-entering the stub). Log word 0 = cursor, words 1..32 =
  // call count of stub k, word 0x84 = FNV-1a over every call (callee address and logged values), entries
  // {va, count, values...} from +0x90.
  // ====================================================================================================================
  let frozen = false;
  function freezeNow() {
    if (frozen) return;
    __freeze.thread();
    frozen = true;
    __freeze.atFlush(function () { frozen = false; });
    __freeze.negOn();
  }
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x4000, CHASH = 0x84, CHEAD = 0x90;
  function newClog(cap) {
    const l = zalloc(cap || CLOG);
    l.writeU32(CHEAD);
    l.add(CHASH).writeU32(0x811c9dc5);
    return l;
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
    const abi = opts.abi || 'default';
    const log = fx.clog;
    const cap = fx.clogCap || CLOG;
    let orig = null;
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
      if (opts.extra) opts.extra(a, n).forEach(function (x) { vals.push(x >>> 0); });
      let h = log.add(CHASH).readU32();
      h = fnvWord(h, va);
      vals.forEach(function (v) { h = fnvWord(h, v >>> 0); });
      log.add(CHASH).writeU32(h);
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
    const cb = new NativeCallback(fn, 'uint32', argt, abi);
    let tramp = at;
    if (typeof Interceptor.replaceFast === 'function') tramp = Interceptor.replaceFast(at, cb);
    else Interceptor.replace(at, cb);
    orig = new NativeFunction(tramp, 'uint32', argt, abi);
    __diffKeepAlive.push(cb, orig);
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

  // the cell tables cellAt 0x0043d6f0 reads (width 0x0053f3e8 + 4 t, base 0x005a9370 + 4 t): the values c3et peeked at
  // the menu (log/c3/alt/c3et_peek.py), for the tables this file's functions reach
  const CELLS = [[0x189, 0x10, 0x43f], [0x18a, 0x10, 0x48d], [0x18b, 0x10, 0x4db], [0x18d, 0xc, 0x569],
                 [0x18e, 0xc, 0x575], [0x18f, 0xc, 0x581]];
  function seedCells() {
    CELLS.forEach(function (c) {
      __freeze.save(ptr(0x0053f3e8 + 4 * c[0]), 4); wr32(0x0053f3e8 + 4 * c[0], c[1]);
      __freeze.save(ptr(0x005a9370 + 4 * c[0]), 4); wr32(0x005a9370 + 4 * c[0], c[2]);
    });
  }

  // ====================================================================================================================
  // showBuyLandScreen 0x004587a0 (__cdecl, no arguments; one vector = one whole run, scene applied by fx.bN)
  // ====================================================================================================================
  // The world the function reads is written by the scene getter (every region saved first and restored at the flush):
  // the tile map 0x005722e8 (50 x 50, every tile 3 except the scene's tiles for sale, 0x14), the per-tile type map
  // 0x00542414 (the type a bought tile becomes, 0..22), the 18 hole records 0x00575ab0 + 0x208 h (+0 par byte, +8 / +0xc
  // tee, +0x18 / +0x1c green) and their waypoint records 0x0059ae80 + 0x18 h (+8 / +0xc and +0x10 / +0x14, tile << 10,
  // -1 = none), the type records 0x00578340 + 0x30 t (+0x10 and +0x20 two names, +0x36 the byte that picks one), the
  // shift [0x0053a450], the mode [0x004d20dc], the byte [0x005a34e0], the money [0x00571fd4], the year word [0x005a6d3c],
  // the date [0x00834170] (logTick's index), the RNG seed [0x00822d9c] and the screen font [0x00519d30] (a fixture font).
  // Recording stand-ins (calleeStand): every drawing callee, showRoutingMapScreen (its map-only flag [0x00822b80] is
  // logged), Window::refreshFull, playSound, canAfford (answers cost <= [0x00571fd4], read from compared memory) and
  // View_toLocal, whose cursor is the scene's view entry for modal.js's CURRENT VISIT COUNT (block +4, compared memory).
  // The pumps are modal.js's models (pumpMessages is the only visit; waitTicks(1, 0) makes none). Run for real:
  // mapToScreen456b70, setLinePoint, clamp, cellAt, __itoa, appendNumber, Random::range, logTick.
  const BL = { s: null, blk: null };
  const BL_SAVE = [[0x005722e8, 2500], [0x00542414, 2500], [0x00575ab0, 0x208 * 19], [0x0059ae80, 0x18 * 19],
                   [0x00578340, 0x30 * 24], [0x0053a450, 4], [0x004d20dc, 4], [0x005a34e0, 4], [0x00571fd4, 4],
                   [0x005a6d3c, 4], [0x00834170, 4], [0x00822d9c, 4], [0x00519d30, 4], [0x00822b80, 4],
                   [0x00822b84, 4], [0x0051a068, 0x200], [0x00821c50, 0x40], [0x005a412c, 8], [0x00568600, 1000],
                   [0x0058421e, 2000], [0x0058a528, 0x40]];
  const FONTS = [zalloc(0x20), zalloc(0x20)];
  // font 0: +8 = 3 >= 0, height +0x10 + +8 = 14 (0x0045906b); font 1: +8 = -1 < 0, height +0xc = 9 (0x00459072)
  FONTS[0].add(8).writeS32(3); FONTS[0].add(0xc).writeS32(40); FONTS[0].add(0x10).writeS32(11);
  FONTS[1].add(8).writeS32(-1); FONTS[1].add(0xc).writeS32(9); FONTS[1].add(0x10).writeS32(50);

  // lot patterns: fn(i, j) -> null (not for sale) or the type byte (0..22); i = row offset, j = column offset in the
  // lot's 16 x 16 block (row = (n % 3) * 16 + 1 + i, column = (n / 3) * 16 + 1 + j)
  const PAT = {
    E: function () { return null; },                                                   // nothing for sale
    C: function (i, j) { return (i === 8 && j === 8) ? 5 : null; },                    // the centre tile only
    A: function (i, j) { return (i + j) % 4 + 1; },                                    // all 256, four types tied
    B: function (i, j) { return (i % 2 === 0) ? (i >> 2) + 7 : null; },                // half, counts 32/32/32/32
    D: function (i, j) { return ((i * 3 + j) % 5 === 0 || (i === 8 && j === 8)) ? [12, 13, 13, 14, 14, 14][(i + j) % 6] : null; },
    N: function (i, j) { return (j < 6 && i !== 8) ? [20, 21, 22][i % 3] : null; },   // no centre tile, three types
    T: function (i, j) { return (i < 3 && j < 3) ? [0, 0, 1][(i + j) % 3] : null; },   // nine tiles, two types
    Z: function (i, j) { return (i === 8 && j === 8) || (i === 0 && j === 15) ? 22 : null; },   // two of one type
  };
  function blWorld(s) {
    BL_SAVE.forEach(function (e) { __freeze.save(ptr(e[0]), e[1]); });
    seedCells();
    for (let k = 0; k < 2500; k++) { wr8(0x005722e8 + k, 3); wr8(0x00542414 + k, (k * 7) % 23); }
    s.lots.forEach(function (pn, n) {
      const pat = PAT[pn], r = n % 3, q = (n / 3) | 0;
      for (let i = 0; i < 16; i++)
        for (let j = 0; j < 16; j++) {
          const t = pat(i, j);
          if (t === null) continue;
          const idx = (r * 16 + 1 + i) * 50 + (q * 16 + 1 + j);
          wr8(0x005722e8 + idx, 0x14);
          wr8(0x00542414 + idx, t);
        }
    });
    // holes: par bytes from the scene (0 = no hole); tee, green, waypoints of three kinds (both, the +8 one only, none)
    for (let h = 0; h < 19; h++) {
      const rr = 0x00575ab0 + 0x208 * h;
      for (let o = 0; o < 0x208; o += 4) wr32(rr + o, (0x01010101 * (h + 1)) ^ (o * 0x10001));
      const w = 0x0059ae80 + 0x18 * h;
      for (let o = 0; o < 0x18; o += 4) wr32(w + o, 0x11110000 + 0x100 * h + o);
      if (h === 0) continue;
      wr8(rr, s.par[h - 1]);
      wr32(rr + 8, 4 + (h * 5) % 40); wr32(rr + 0xc, 3 + (h * 11) % 44);
      wr32(rr + 0x18, 6 + (h * 7) % 38); wr32(rr + 0x1c, 2 + (h * 13) % 45);
      const kind = h % 3;   // 0: both waypoints (the +0x10 pair is drawn), 1: only +8, 2: none
      wr32(w + 8, kind === 2 ? -1 : (5 + h % 30) << 10); wr32(w + 0xc, kind === 2 ? -1 : (7 + (h * 3) % 30) << 10);
      wr32(w + 0x10, kind === 0 ? (9 + h % 25) << 10 : -1); wr32(w + 0x14, kind === 0 ? (11 + (h * 2) % 25) << 10 : -1);
    }
    // type records: two names each (+0x10, +0x20), the selector +0x36 = 0xd for every third type
    for (let t = 0; t < 24; t++) {
      const rr = 0x00578340 + 0x30 * t;
      wrStr(rr + 0x10, 'tA' + t, 0x10);
      wrStr(rr + 0x20, 'tB' + t, 0x10);
      wr8(rr + 0x36, (t % 3 === 1) ? 0xd : t);
    }
    wr32(0x0053a450, s.shift);
    wr32(0x004d20dc, s.mode);
    wr32(0x005a34e0, s.b);
    wr32(0x00571fd4, s.money);
    wr16(0x005a6d3c, s.year);
    wr32(0x00834170, s.date);
    wr32(0x00822d9c, s.seed);
    ptr(0x00519d30).writePointer(FONTS[s.font]);
    wr32(0x00822b80, 0);
    wr32(0x00822b84, 0);
    wr8(0x0051a068, 0);
  }

  // points: lot n's centre (column n / 3, band n % 3), the done button, outside, and edge points of every hit test
  function LOT(n) { return [[0x50, 0x180, 0x280][(n / 3) | 0], 0x36 + 68 * (n % 3) + 30]; }
  const DONE = [0x2b0, 0x230], OUT = [0x30, 0x2a0];
  const EDGES = [[5, 0x80], [0x310, 0x80], [0x50, 0x20], [0x50, 0x110], [0x108, 0x80], [0x210, 0x80],
                 [0x2e0, 0x230], [0x2b0, 0x200], [0x2b0, 0x260], [0x290, 0x230]];
  function P1(p) { return press(p[0], p[1]); }
  function P2(p) { return press2(p[0], p[1]); }
  const PAR = [3, 4, 5, 0, 1, 7, 2, 0, 4, 3, 6, 5, 0, 4, 3, 2, 8, 4];
  const LOTS0 = ['A', 'E', 'B', 'C', 'D', 'N', 'E', 'T', 'Z'];
  function blScene(o, steps) {
    const s = { lots: LOTS0, par: PAR, shift: 2, mode: 1, b: 1, money: 100000, year: 2003, date: 0x00235000,
                seed: 0x1234567, font: 0 };
    Object.keys(o).forEach(function (k) { s[k] = o[k]; });
    s.view = steps.map(function (st) { return st[0]; });
    s.script = steps.map(function (st) { return st.slice(1); });
    return s;
  }
  const BL_SCENES = [
    // 0: hover lot 4, wait a frame, click it: affordable, bought (return 1)
    blScene({}, [[LOT(4)], [LOT(4), P1(LOT(4))]]),
    // 1: mode 0, the second font, shift 0: hover the done button, click it (return 0)
    blScene({ mode: 0, font: 1, shift: 0, b: 0, seed: 77 }, [[DONE], [DONE, P1(DONE)]]),
    // 2: a right click outside every region (return 0)
    blScene({ b: 2, seed: 0x7fffffff }, [[OUT, P2(OUT)]]),
    // 3: a click outside (playSound, no redraw), hover lot 0, lot 8, click lot 8 with too little money (canAfford 0:
    //    redraw), hover and click the done button
    blScene({ money: 10, lots: ['C', 'A', 'E', 'B', 'E', 'D', 'T', 'N', 'A'] },
      [[OUT, P1(OUT)], [OUT], [LOT(0)], [LOT(8)], [LOT(8), P1(LOT(8))], [LOT(8)], [DONE], [DONE, P1(DONE)]]),
    // 4: a click word of 3 over lot 2 (the `s = sel` arm): bought
    blScene({ shift: 31, year: 2057 }, [[LOT(2)], [LOT(2), raw({ 0x00822d68: 3, 0x00822d74: 1 })]]),
    // 5: hover every lot in turn, then every edge point (all -1: no redraw), then the done button, right click
    blScene({ shift: 5, mode: 2, lots: ['D', 'B', 'A', 'N', 'C', 'Z', 'T', 'A', 'B'], seed: 4242 },
      [[LOT(0)], [LOT(1)], [LOT(2)], [LOT(3)], [LOT(4)], [LOT(5)], [LOT(6)], [LOT(7)], [LOT(8)], [OUT]]
        .concat(EDGES.map(function (p) { return [p]; })).concat([[DONE], [DONE, P2(DONE)]])),
    // 6: click an empty lot (nothing for sale: its stored price is 5 * 10 = 50): bought, no tile changes. The frames
    //    after the click are never reached by the original (it returns 1 there); they end a body that refuses the
    //    purchase through the done button instead of the modal bound
    blScene({ lots: ['E', 'E', 'A', 'E', 'B', 'E', 'C', 'E', 'D'] },
      [[LOT(3)], [LOT(3), P1(LOT(3))], [LOT(3)], [DONE], [DONE, P1(DONE)]]),
    // 7: a right click while hovering lot 7 (return 0); every lot empty
    blScene({ lots: ['E', 'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E'], font: 1 }, [[LOT(7)], [LOT(7), P2(LOT(7))]]),
    // 8: lot 6 bought, year 2099 (the last stats row), shift 1
    blScene({ year: 2099, shift: 1, lots: ['Z', 'T', 'N', 'E', 'C', 'B', 'A', 'D', 'E'] },
      [[LOT(6)], [LOT(6), P1(LOT(6))]]),
    // 9: click lot 1 while too poor (redraw), then click the done button without hovering it first
    blScene({ money: 0, shift: 3, seed: 99 }, [[LOT(1)], [LOT(1), P1(LOT(1))], [LOT(1)], [DONE, P1(DONE)]]),
  ];
  function blView(n) {
    const v = BL.s.view;
    if (v.length === 0) return OUT;
    return v[Math.min(n, v.length - 1)];
  }
  function blStands(fx) {
    calleeStand(fx, 0, 0x00473e60, 5, 0, { abi: 'thiscall' });                       // Image_draw (ret 0x10)
    calleeStand(fx, 1, 0x004762d0, 5, 0, { abi: 'thiscall' });                       // setDrawTarget (ret 0x10)
    calleeStand(fx, 2, 0x00404b70, 4, 0, { str: { 0: 8 } });                         // drawCenteredText
    calleeStand(fx, 3, 0x004049d0, 4, 0, { str: { 0: 8 } });                         // drawText4049d0
    calleeStand(fx, 4, 0x00404bc0, 4, 0, { str: { 0: 8 } });                         // drawCenteredText2
    calleeStand(fx, 5, 0x0040bf20, 5, 0, {});                                        // lineTo
    calleeStand(fx, 6, 0x004628d0, 8, 0, {});                                        // addSprite
    calleeStand(fx, 7, 0x00476310, 5, function (n, a) { return a[3]; }, { abi: 'thiscall' });   // Widget_setQuad
    calleeStand(fx, 8, 0x00477c30, 5, 0, { abi: 'thiscall', str: { 1: 8 } });      // drawText (ret 0x10)
    calleeStand(fx, 9, 0x00476700, 6, 0, { abi: 'thiscall', str: { 2: 2 } });      // drawStringAt (ret 0x14), frame label
    calleeStand(fx, 10, 0x00478430, 5, 0, { abi: 'thiscall', str: { 1: 16 } });    // drawTextLinesAt (ret 0x10)
    calleeStand(fx, 11, 0x00478080, 6, 0, { abi: 'thiscall', str: { 1: 8 } });     // drawRight (ret 0x14)
    calleeStand(fx, 12, 0x00480c80, 2, 0, { abi: 'thiscall' });                      // Window::refreshFull (ret 4)
    calleeStand(fx, 13, 0x0047ab50, 3, function (n, a) {                             // View_toLocal (thiscall, ret 8)
      const p = blView(BL.blk.add(4).readU32());
      ptr(a[1]).writeS32(p[0]); ptr(a[2]).writeS32(p[1]);
      return 0;
    }, { abi: 'thiscall', mask: [1, 2], extra: function () { return blView(BL.blk.add(4).readU32()); } });
    calleeStand(fx, 14, 0x00456be0, 0, 0, { extra: function () { return [rd32(0x00822b80)]; } });   // showRoutingMapScreen
    calleeStand(fx, 15, 0x004481b0, 5, 0, {});                                       // playSound
    calleeStand(fx, 16, 0x00406c30, 1, function (n, a) {                             // canAfford(cost)
      return ((a[0] | 0) <= (rd32(0x00571fd4) | 0)) ? 1 : 0;
    }, { extra: function () { return [rd32(0x00571fd4)]; } });
  }

  F.c3fr_buyland = function () {
    const fx = { clog: newClog(0x10000), clogCap: 0x10000 };
    const M = __modal({ maxCalls: 400, maxFrames: 400 });
    __importStandIn({ module: 'golf_clean.exe', imports: M.imports() });
    BL.blk = M.fx().modal;
    blStands(fx);              // freezes the first thread
    M.arm();
    BL_SCENES.forEach(function (sc, i) {
      Object.defineProperty(fx, 'b' + i, { enumerable: true, configurable: true, get: function () {
        BL.s = sc;
        blWorld(sc);
        M.select(sc.script);
        return 0;
      } });
    });
    negGetters(fx, NEG_BL, 'b');
    return Object.assign(fx, M.fx());
  };
  const NEG_BL = [[1, 0], [2, 0], [3, 0], [4, 0], [5, 0], [6, 0], [7, 0], [8, 4], [9, 6], [10, 0], [11, 2], [12, 3],
                  [13, 5], [14, 5], [15, 3]];

  // ====================================================================================================================
  // narrateShot 0x00407e00 (__cdecl (shot, dist, yards, flag); one vector = one call, scene applied by fx.sN)
  // ====================================================================================================================
  // The scene getter writes (every region saved first, restored at the flush): the shot record 0x0059fc60 + 0x388 s
  // (golfer word, point count, x / y / height of the points, the two 0x100-byte golfer snapshots at +0x188 / +0x288),
  // the golfer records 0x005794b8 + 0x100 g of golfers 0..9 (a pattern fill, then every field the function and its real
  // callees read: box corner +8 / +0xc, flags +0x10, mask +0x16, kind +0x18, lie +0x1b, club +0x1c, hole +0x21, +0x22,
  // +0x36, +0x70, partner +0xa2, +0xac, reaction +0xae, type +0xb6, name index +0xbb, target +0xc4 / +0xc8, ball +0xd4 /
  // +0xd8), the golfer-type records 0x004d6088 + 0x230 t (name +0x10, flag bytes +0x20 / +0x21), the name slots
  // 0x0058dd50 + 0x38 n, the tile-type records 0x00578340 + 0x30 t (name +0x10, attribute +0x32), the tile map, the hole
  // records' green tile, the hole-flag table 0x005849e3 + 0x2c t + h, the two skill tables 0x005a5a04 / 0x005a5a14, and
  // the globals the function reads (followed golfer [0x005a59f8], flag bit 0x200000 of [0x0059e7b8] and bit 2 of the
  // byte [0x005a5a00] read-modify-written, [0x005685f0], [0x00822c88], the seed, the date, the year word, the zoom pair
  // [0x004c2e00] / [0x004c2844], [0x00543cc8]).
  // Recording stand-ins: emptyFunc_4a0890, worldToScreen (a scene formula of its arguments; its two out-pointers are
  // frame slots, masked), lineTo, startMessage and drawWindow (the text buffer logged), playSound and emitGolferThought
  // (answers that depend on their call index only, the index being compared memory in $clog), fillWindowRect,
  // Widget_fillRect, drawCenteredText, drawTextSurface, Window::refreshFull, pumpAndRedraw (writes the scene's key into
  // [0x00822d94]), saveBmp (text and rect words logged), postEvent. Run for real: buildGolferName, typeAtPos +
  // tileBlocked, appendClubName, appendHoleName, __itoa, typeBit7Clear, bucketValue, clamp, rateTile, Random::range,
  // setLinePoint.
  const NS = { s: null };
  const NS_SAVE = [[0x0059fc60, 0x388 * 4], [0x005794b8, 0x100 * 10], [0x00582cb8, 0x200], [0x004d6088, 0x230 * 8],
                   [0x0058dd50, 0x38 * 24], [0x00578340, 0x30 * 24], [0x005722e8, 2500], [0x00575ab0, 0x208 * 19],
                   [0x00575aa8, 8], [0x005849e0, 0xd40], [0x005a5a04, 0x20], [0x004c2c9c, 4], [0x004c2e08, 4],
                   [0x005a59f8, 4], [0x005685f0, 4], [0x00822c88, 4], [0x00822d9c, 4], [0x00834170, 4], [0x005a6d3c, 4],
                   [0x004c2e00, 4], [0x004c2844, 4], [0x00543cc8, 4], [0x00822d94, 4], [0x0051a068, 0x200],
                   [0x0058a528, 0x40], [0x005a412c, 8]];
  function gSpec(o) {
    const s = { x: 120, y: 160, flags: 0, mask: 0xffff, kind: 0x21, b1a: 3, lie: 5, club: 3, d5: 1, hole: 2, b22: 0,
                b36: 5, b70: 0, partner: 3, ac: 0, kind2: 0, type: 1, name: 1, tx: 10 << 10, ty: 12 << 10,
                bx: 11 << 10, by: 13 << 10, p0: 0x1234, p4: 0x5678, skills: 0x01020304 };
    Object.keys(o || {}).forEach(function (k) { s[k] = o[k]; });
    return s;
  }
  function wrGolfer(a, s, seed) {
    for (let o = 0; o < 0x100; o += 4) wr32(a + o, (((seed * 0x11 + o) & 0x3f) * 0x01010101) & 0x1f1f1f1f);
    wr32(a + 0, s.p0); wr32(a + 4, s.p4);
    wr32(a + 8, s.x); wr32(a + 0xc, s.y);
    wr32(a + 0x10, s.flags); wr16(a + 0x16, s.mask);
    wr8(a + 0x18, s.kind); wr8(a + 0x1a, s.b1a); wr8(a + 0x1b, s.lie); wr8(a + 0x1c, s.club); wr8(a + 0x1d, s.d5);
    wr8(a + 0x21, s.hole); wr8(a + 0x22, s.b22); wr8(a + 0x36, s.b36); wr8(a + 0x70, s.b70);
    wr16(a + 0xa2, s.partner); wr16(a + 0xac, s.ac); wr16(a + 0xae, s.kind2); wr16(a + 0xb6, s.type);
    wr8(a + 0xbb, s.name);
    wr32(a + 0xc4, s.tx); wr32(a + 0xc8, s.ty); wr32(a + 0xd4, s.bx); wr32(a + 0xd8, s.by);
    wr32(a + 0xf0, s.skills); wr32(a + 0xf4, s.skills ^ 0x05050505);
  }
  // types: +0x20 bits 8 (emptyFunc of the golfer) and 0x10 (of its partner); +0x21 bit 7 (typeBit7Clear's 0)
  const NS_T20 = [0x00, 0x08, 0x10, 0x18, 0x00, 0x08, 0x10, 0x00];
  const NS_T21 = [0x00, 0x80, 0x00, 0x80, 0x00, 0x00, 0x80, 0x80];
  function nsWorld(s) {
    NS_SAVE.forEach(function (e) { __freeze.save(ptr(e[0]), e[1]); });
    // golfer types, name slots, tile types
    for (let t = 0; t < 8; t++) {
      const r = 0x004d6088 + 0x230 * t;
      wrStr(r + 0x10, 'Ty' + t, 0x10);
      wr8(r + 0x20, NS_T20[t]); wr8(r + 0x21, NS_T21[t]);
    }
    for (let n = 0; n < 24; n++) wrStr(0x0058dd50 + 0x38 * n, 'Gname' + n, 0x10);
    for (let t = 0; t < 24; t++) {
      const r = 0x00578340 + 0x30 * t;
      wrStr(r + 0x10, 'tile' + t, 0x10);
      wr8(r + 0x32, (t % 3 === 0) ? 2 : (t % 3 === 1 ? 0 : 0xfe));
    }
    // tiles: type (x + 2 y) % 20, row 10 all type 1, row 11 columns 0..4 blocked (0x14)
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++) {
        let t = (x + 2 * y) % 20;
        if (x === 10) t = 1;
        if (x === 11 && y < 5) t = 0x14;
        wr8(0x005722e8 + x * 50 + y, t);
      }
    // holes: green tile (h + 4, h + 6); the appendHoleName flag byte (+0x200) clear
    for (let h = 0; h < 19; h++) {
      const r = 0x00575ab0 + 0x208 * h;
      wr32(r + 0x18, h + 4); wr32(r + 0x1c, h + 6);
      wr8(r + 0x200, 0);
    }
    wr32(0x00575aa8, 0);
    // the hole-flag table: bit 0 set where the scene asks (t, h)
    for (let o = 0; o < 0xd40; o += 4) wr32(0x005849e0 + o, 0);
    (s.hflags || []).forEach(function (p) { wr8(0x005849e3 + 0x2c * p[0] + p[1], 1); });
    for (let k = 0; k < 0x20; k++) wr8(0x005a5a04 + k, s.lv[k % s.lv.length] + (k >= 0x10 ? s.lv2 : 0));
    wr32(0x004c2c9c, 0x00010000);
    wr32(0x004c2e08, 0x77);
    // golfers 0..9 from the scene's specs (golfers without one get the defaults with their own partner)
    for (let g = 0; g < 10; g++) {
      const sp = gSpec(Object.assign({ partner: g ^ 1, name: g, type: g % 8 }, (s.gl && s.gl[g]) || {}));
      wrGolfer(0x005794b8 + 0x100 * g, sp, g);
    }
    // the shot record
    for (let sh = 0; sh < 4; sh++) {
      const S = 0x0059fc60 + 0x388 * sh;
      for (let o = 0; o < 0x388; o += 4) wr32(S + o, 0);
      wr16(S, s.g);
      const pts = s.pts || [];
      wr16(S + 4, s.n === undefined ? pts.length : s.n);
      pts.forEach(function (p, i) { wr32(S + 8 + 4 * i, p[0]); wr32(S + 0x88 + 4 * i, p[1]); wr32(S + 0x108 + 4 * i, p[2]); });
      wrGolfer(S + 0x188, gSpec(Object.assign({ partner: 0x77, name: 12, type: 5 }, s.snap || {})), 12);
      wrGolfer(S + 0x288, gSpec(Object.assign({ partner: 0x98, name: 13, type: 6 }, s.psnap || {})), 13);
    }
    wr32(0x005a59f8, s.followed);
    const fl = rd32(0x0059e7b8) >>> 0;
    wr32(0x0059e7b8, s.f200000 ? (fl | 0x200000) : (fl & ~0x200000));
    const b0 = ptr(0x005a5a00).readU8();
    wr8(0x005a5a00, s.f2 ? (b0 | 2) : (b0 & ~2));
    wr32(0x005685f0, s.holes);
    wr32(0x00822c88, s.diff);
    wr32(0x00822d9c, s.seed);
    wr32(0x00834170, s.date);
    wr16(0x005a6d3c, s.year);
    wr32(0x004c2e00, s.zoom[0]); wr32(0x004c2844, s.zoom[1]);
    wr32(0x00543cc8, 2);
    wr32(0x00822d94, 0);
    wr8(0x0051a068, 0);
  }
  function nsScene(o) {
    const s = { shot: 1, g: 2, followed: 2, flag: 0, dist: 3, yards: 140, n: undefined, pts: null, gl: {},
                snap: {}, psnap: {}, w2s: [0, 0], f200000: 0, f2: 1, holes: 8, diff: 1, seed: 0x2468ace,
                date: 0x00012345, year: 3, zoom: [3, 2], key: 0x79, hflags: [], lv: [3, 4, 5, 0, 9, 1, 2, 7, 10, 6, 8, 3],
                lv2: 0 };
    Object.keys(o).forEach(function (k) { s[k] = o[k]; });
    if (s.pts === null)
      s.pts = [[100 << 8, 140 << 8, 0], [140 << 8, 160 << 8, 30], [(180 << 8) | 0x3ff, 150 << 8, 50],
               [200 << 8, 170 << 8, -40], [230 << 8, 180 << 8, 7]];
    return s;
  }
  // worldToScreen stand-in: visible unless the low 10 bits of x are all set; sx = (x >> 8) + ox, sy = (y >> 8) + oy
  function w2s(a) {
    const x = a[0] | 0, y = a[1] | 0, s = NS.s;
    if ((x & 0x3ff) === 0x3ff) return 0;
    ptr(a[2]).writeS32((x >> 8) + s.w2s[0]);
    ptr(a[3]).writeS32((y >> 8) + s.w2s[1]);
    return 1;
  }
  // levels: ONE = every skill at 1 (with cap 8 no bit is cleared), FIVE = every skill at 5
  const ONE = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1], FIVE = [5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5];
  const T3 = { tx: 3 << 10, ty: 0 };               // target tile (3, 0): type 3, attribute 2 (> 0), not type 1
  // the first draw of Random::range(12) for these seeds (log/c3/alt/c3fr_seeds.py table 12)
  const SEED = { 0: 0x2, 1: 0x8, 2: 0xe, 3: 0x14, 4: 0x1a, 5: 0x20, 6: 0x1, 7: 0x7, 8: 0xd, 9: 0x13, 10: 0x19, 11: 0x1f };
  const NS_SCENES = [
    // 0: golfer 2 (followed) shot drawn, a good drive, reward, SimFoto, key 0x79 (saveBmp), emitGolferThought
    nsScene({ snap: { club: 0, lie: 12, mask: 0x0004, kind2: 0, b22: 0 }, psnap: { hole: 3 } }),
    // 1: no point on screen (box corner x 0), not followed: the hole flag is set and it returns
    nsScene({ followed: 5, gl: { 2: { x: 0 } } }),
    // 2: no point on screen, followed: text, no draw, ok cleared, startMessage + playSound
    nsScene({ gl: { 2: { y: -3, club: 7, lie: 10, mask: 0x0001 } }, dist: 0, yards: 5 }),
    // 3: a missed shot (flag 1) of the snapshot, club 6, odd hole: mask rebuilt from levels > 3, a level lost
    nsScene({ flag: 1, snap: { club: 6, hole: 5, mask: 0, kind: 0x22 }, lv: FIVE }),
    // 4: a missed shot, every level 2 (mask 0): 100 draws, the last one (seed 4: case 8, club 6) picks bit 3, whose
    //    level 2 ends the function with that level in eax
    nsScene({ flag: 1, seed: 0x4, snap: { club: 6, hole: 4 }, lv: [2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2] }),
    // 5: box past the right edge, not followed: returns the box bottom
    nsScene({ followed: 7, gl: { 3: { x: 0x300 } } }),
    // 6..15: the first draw takes switch case k and its test passes (levels 1, cap 8: every bit in the mask)
    nsScene({ seed: SEED[0], lv: ONE, snap: Object.assign({ club: 3 }, T3) }),
    nsScene({ seed: SEED[1], lv: ONE, snap: Object.assign({ flags: 2 }, T3) }),
    nsScene({ seed: SEED[2], lv: ONE, snap: Object.assign({ club: 1, b22: 0 }, T3) }),
    nsScene({ seed: SEED[3], lv: ONE, snap: Object.assign({ club: 3, b22: 0 }, T3) }),
    nsScene({ seed: SEED[4], lv: ONE, snap: Object.assign({ ac: 1 }, T3) }),
    nsScene({ seed: SEED[5], lv: ONE, snap: Object.assign({ ac: -1 }, T3) }),
    nsScene({ seed: SEED[6], lv: ONE, snap: Object.assign({ ac: 3 }, T3) }),
    nsScene({ seed: SEED[7], lv: ONE, yards: 2000, snap: T3 }),
    nsScene({ seed: SEED[8], lv: ONE, snap: Object.assign({ club: 5 }, T3) }),
    nsScene({ seed: SEED[9], lv: ONE, dist: 0, snap: T3 }),
    // 16..21: the same first cases whose tests fail (the loop goes on), and the two empty draws 10 / 11
    nsScene({ seed: SEED[0], lv: ONE, snap: { tx: 4 << 10, ty: 0, club: 12, b22: 1, ac: 2 }, yards: 1 }),
    nsScene({ seed: SEED[1], lv: ONE, dist: 6, snap: { tx: 5 << 10, ty: 0, flags: 0, club: 11, b22: 1 } }),
    nsScene({ seed: SEED[2], lv: ONE, snap: { tx: 4 << 10, ty: 0, b22: 2, club: 0 } }),
    nsScene({ seed: SEED[10], lv: ONE, snap: { tx: 4 << 10, ty: 0, club: 4, ac: 7 } }),
    nsScene({ seed: SEED[11], lv: ONE, snap: { tx: 6 << 10, ty: 0, club: 2, ac: 4 } }),
    nsScene({ seed: SEED[9], lv: ONE, dist: 5, snap: { tx: 4 << 10, ty: 0, club: 13, ac: 6, bx: 10 << 10 } }),
    // 22..27: the reward's outcomes: the type-1 target shortcut (bit 0x10) with level 0 (a new skill, the followed
    //    golfer's 0x004c2c9c bit), a level reaching the cap (cap 8 from 8 holes), level 12 with cap 0x63 (no
    //    reward text), kind 0 (no reward), the loss shortcut, and every bit at or above the cap (100 empty draws)
    nsScene({ lv: [1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1], diff: 0, snap: { tx: 10 << 10, ty: 12 << 10 } }),
    nsScene({ lv: [1, 1, 1, 1, 7, 1, 1, 1, 1, 1, 1, 1], snap: { tx: 10 << 10, ty: 3 << 10 } }),
    nsScene({ holes: 0x13, lv: [1, 1, 1, 1, 12, 1, 1, 1, 1, 1, 1, 1], snap: { tx: 10 << 10, ty: 3 << 10 } }),
    nsScene({ lv: ONE, snap: { kind: 0, tx: 10 << 10, ty: 3 << 10 } }),
    nsScene({ flag: 1, lv: FIVE, snap: { tx: 10 << 10, ty: 3 << 10, type: 1, club: 4, hole: 3 } }),
    nsScene({ holes: 3, lv: [6, 7, 8, 9, 6, 6, 7, 8, 9, 6, 7, 8], seed: 0x55, snap: T3 }),
    // 28..31: who is rewarded: the partner of the followed golfer (second table, bit 0x200000 clear), the same with the
    //    bit set (no reward), a followed golfer not in the pair, and -1 with the frame forced (SimFoto flag, no hole bit)
    nsScene({ followed: 3, lv: ONE, lv2: 2, seed: SEED[3], snap: { club: 2, b22: 0, tx: 4 << 10, ty: 0 } }),
    nsScene({ followed: 3, f200000: 1, lv: ONE }),
    nsScene({ followed: 6, lv: ONE, key: 0 }),
    nsScene({ followed: -1, lv: ONE, gl: { 2: { b70: 1 } } }),
    // 32..37: the sentence: clubs, lies, distances, the in-cup test, the partner's reaction kinds, hole <= 0
    nsScene({ snap: { club: 13, lie: 10, kind2: 1 }, psnap: { hole: 1, type: 2 }, dist: 0,
              gl: { 2: { bx: ((2 + 4) << 10) + 0x200, by: ((2 + 6) << 10) + 0x200 } } }),
    nsScene({ snap: { club: 7, lie: 5, kind2: 2 }, psnap: { hole: 4, type: 1 }, dist: 0 }),
    nsScene({ snap: { club: 9, lie: 11, kind2: 3 }, psnap: { hole: 2, type: 0 }, dist: 9,
              gl: { 2: { bx: 10 << 10, by: 20 << 10 } } }),
    nsScene({ snap: { club: 3, lie: 1, kind2: 4 }, psnap: { hole: 5, type: 3 }, dist: 7,
              gl: { 2: { bx: 10 << 10, by: 30 << 10 } } }),
    nsScene({ snap: { club: 4, lie: 0, kind2: 5 }, psnap: { hole: 6, type: 4 }, dist: 2 }),
    nsScene({ snap: { club: 6, kind2: 7 }, psnap: { hole: 0 } }),
    // 38..43: missed shots by club and hole parity (club > 4 / <= 4; hole even, odd, -1)
    nsScene({ flag: 1, lv: FIVE, snap: { club: 8, hole: 6 } }),
    nsScene({ flag: 1, lv: FIVE, snap: { club: 9, hole: -1 }, gl: { 2: { b36: 0xf0 } } }),
    nsScene({ flag: 1, lv: FIVE, snap: { club: 1, hole: 7, kind2: -1 } }),
    nsScene({ flag: 1, lv: FIVE, snap: { club: 0, hole: -3 } }),
    nsScene({ flag: 1, lv: FIVE, snap: { club: 4, hole: 2 }, followed: 3 }),
    nsScene({ flag: 1, lv: [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1], seed: SEED[5], snap: { club: 3, hole: 2 } }),
    // 44..51: the box and the SimFoto: no points (the corners only), every point hidden, a near edge moved to 0x38,
    //    an edge below 0 (ok cleared, followed: startMessage), the bottom past 0x230, the top below 0xc0 with a tall
    //    box (drawWindow's y clamped), the top at 0xc0 or more, a negative date, no SimFoto flag, a hole bit set
    nsScene({ pts: [], gl: { 2: { x: 300, y: 250 }, 3: { x: 340, y: 300 } } }),
    nsScene({ pts: [[(50 << 8) | 0x3ff, 50 << 8, 0], [(60 << 8) | 0x3ff, 60 << 8, 0]] }),
    nsScene({ w2s: [-80, -100], gl: { 2: { x: 300, y: 300 }, 3: { x: 320, y: 310 } } }),
    nsScene({ w2s: [-150, 0] }),
    nsScene({ w2s: [0, 420], gl: { 2: { y: 470 }, 3: { y: 480 } } }),
    nsScene({ w2s: [0, 250], date: -0x00034567, gl: { 2: { x: 140, y: 100 }, 3: { x: 160, y: 120 } } }),
    nsScene({ w2s: [0, 60], f2: 0, key: 0x78, gl: { 2: { y: 200 }, 3: { y: 210 } } }),
    nsScene({ hflags: [[3, 2], [40, 2]], followed: 2 }),
    // 52: the partner left of and above the golfer, the golfer's type with bit 8, the partner's without 0x10, not followed
    nsScene({ followed: 5, gl: { 2: { x: 200, y: 220, type: 1 }, 3: { x: 150, y: 180, type: 0 } } }),
    // 53: in the cup on x but not on y
    nsScene({ dist: 0, gl: { 2: { bx: (6 << 10) + 0x200, by: 0x5000 } } }),
    // 54..58: reaction kinds 7 and -1 (past the table), case 4 with a type whose bit 7 is clear, cases 3 and 2 the other way
    nsScene({ snap: { kind2: 7 }, psnap: { hole: 2 } }),
    nsScene({ snap: { kind2: -1 }, psnap: { hole: 2 } }),
    nsScene({ snap: { kind2: 4 }, psnap: { hole: 2, type: 0 } }),
    nsScene({ snap: { kind2: 3 }, psnap: { hole: 2, type: 1 } }),
    nsScene({ snap: { kind2: 2 }, psnap: { hole: 2, type: 0 } }),
    // 59..64: the first draw's case passes its own test but its mask bit is cleared (level 8 = the cap)
    nsScene({ seed: SEED[1], lv: [1, 1, 1, 1, 1, 1, 1, 1, 1, 8, 1, 1], snap: Object.assign({ flags: 2 }, T3) }),
    nsScene({ seed: SEED[2], lv: [1, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1], snap: Object.assign({ club: 1, b22: 0 }, T3) }),
    nsScene({ seed: SEED[4], lv: [1, 1, 1, 1, 1, 8, 1, 1, 1, 1, 1, 1], snap: Object.assign({ ac: 1 }, T3) }),
    nsScene({ seed: SEED[5], lv: [1, 1, 1, 1, 1, 1, 8, 1, 1, 1, 1, 1], snap: Object.assign({ ac: -1 }, T3) }),
    nsScene({ seed: SEED[6], lv: [1, 1, 1, 1, 1, 1, 1, 8, 1, 1, 1, 1], snap: Object.assign({ ac: 3 }, T3) }),
    nsScene({ seed: SEED[7], lv: [8, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1], yards: 2000, snap: T3 }),
    // 65: case 9 with distance 0 and the named golfer's ball on a type-1 tile (bit 0x200)
    nsScene({ seed: SEED[9], lv: ONE, dist: 0, snap: { tx: 3 << 10, ty: 0, bx: 10 << 10, by: 7 << 10 } }),
    // 66: a new skill for the partner of the followed golfer (second table): the 0x004c2c9c bit is not set
    nsScene({ followed: 3, lv: [1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1], snap: { tx: 10 << 10, ty: 12 << 10 } }),
    // 67: the bottom past 0x230, not followed: the exit through 0x004088f4 returns the box bottom
    nsScene({ followed: 5, w2s: [0, 420], gl: { 2: { y: 470 }, 3: { y: 480 } } }),
  ];
  function nsStands(fx) {
    const textWords = function () { return runText(0x0051a068, 24); };
    // emptyFunc_4a0890 is a lone `ret` (too short to intercept): it runs for real and has no effect to compare
    calleeStand(fx, 1, 0x0042fb90, 5, function (n, a) { return w2s(a); }, { mask: [2, 3] });   // worldToScreen
    calleeStand(fx, 2, 0x0040bf20, 5, 0, {});                                        // lineTo
    calleeStand(fx, 3, 0x0040cb00, 3, 1, { extra: textWords });                      // startMessage
    calleeStand(fx, 4, 0x004481b0, 5, function (n) { return 0x5100 + n; }, {});      // playSound
    calleeStand(fx, 5, 0x0040d320, 4, 0, { extra: textWords });                      // drawWindow
    calleeStand(fx, 6, 0x00480b00, 6, 0, { abi: 'thiscall' });                       // fillWindowRect (ret 0x14)
    calleeStand(fx, 7, 0x00478b80, 6, 0, { abi: 'thiscall' });                       // Widget_fillRect (ret 0x14)
    calleeStand(fx, 8, 0x00404b70, 4, 0, { str: { 0: 4 } });                         // drawCenteredText
    calleeStand(fx, 9, 0x00404ad0, 4, 0, { str: { 0: 8 } });                         // drawTextSurface
    calleeStand(fx, 10, 0x00480c80, 2, 0, { abi: 'thiscall' });                      // Window::refreshFull (ret 4)
    calleeStand(fx, 11, 0x0045c0c0, 1, function () { wr32(0x00822d94, NS.s.key); return 0; }, {});   // pumpAndRedraw
    calleeStand(fx, 12, 0x00475580, 3, 0, { abi: 'thiscall', mask: [2], str: { 1: 16 },   // saveBmp (ret 8)
      extra: function (a) { const p = ptr(a[2]); return [p.readU32(), p.add(4).readU32(), p.add(8).readU32(), p.add(12).readU32()]; } });
    calleeStand(fx, 13, 0x0046e7b0, 3, 0, {});                                       // postEvent
    calleeStand(fx, 14, 0x00467a00, 3, function (n) { return 0x7a00 + n; }, {});     // emitGolferThought
  }
  // the text at a, as words (up to 4 n bytes, zero after the NUL), plus its length
  function runText(a, n) {
    const p = ptr(a), words = new Array(n).fill(0);
    let len = 0, end = false;
    for (let q = 0; q < 4 * n; q++) {
      const c = end ? 0 : p.add(q).readU8();
      if (c === 0) end = true; else len++;
      words[q >> 2] |= c << (8 * (q & 3));
    }
    return [len].concat(words.map(function (w) { return w >>> 0; }));
  }

  F.c3fr_narrate = function () {
    const fx = { clog: newClog(0x8000), clogCap: 0x8000 };
    nsStands(fx);              // freezes the first thread
    NS_SCENES.forEach(function (sc, i) {
      Object.defineProperty(fx, 's' + i, { enumerable: true, configurable: true, get: function () {
        NS.s = sc;
        nsWorld(sc);
        return sc.shot;
      } });
      fx['d' + i] = sc.dist;
      fx['y' + i] = sc.yards;
      fx['f' + i] = sc.flag;
    });
    negGetters(fx, NEG_NS, 's');
    return fx;
  };
  // ====================================================================================================================
  // GolferPanelEject 0x0045c560 (__cdecl (g, mode); one vector = one call, scene applied by fx.pN)
  // ====================================================================================================================
  // The scene getter writes golfer records 0..9 (pattern fill, then every field the panel and its real callees read),
  // the golfer-type records 0x004d6088 + 0x230 t (+0 / +0x10 names, +0x20 trait byte), the name slots, the hole records'
  // par byte, and [0x00824144] (story pages on), [0x005409a8] (double comment), [0x00822c74], the hover id [0x00824130]
  // and its frame counter [0x00823768]. Recording stand-ins: skillPointDialog, Image_draw, Surface_blit2,
  // Surface_stretchBlit, drawThoughtBubble, setDrawTarget, drawCenteredText(2), fillWindowRect, loadStoryStrings,
  // storyText (answers 1 for (story * 3 + line + golfer) % 4 != 0 and then copies `S<story>.<line>.<golfer>` into the
  // text), substituteStoryNames (appends `~<golfer>`), drawText4049d0, Widget_fillRect, View_toLocal (the scene's
  // cursor), drawPanelBackdrop (the tooltip text logged), buildGolferComment (appends `C<kind>.<word>.<p>`), Text_put
  // (20 pixels per character of its argument, compared memory) and drawTextSurface. Real: buildGolferName, golferScore,
  // appendOpinion, clamp, thoughtBalance, approxDistance, __itoa.
  const GP = { s: null };
  const GP_SAVE = [[0x005794b8, 0x100 * 10], [0x004d6088, 0x230 * 8], [0x0058dd50, 0x38 * 24], [0x00575ab0, 0x208 * 19],
                   [0x00824144, 4], [0x005409a8, 4], [0x00822c74, 4], [0x00824130, 4], [0x00823768, 4],
                   [0x0051a068, 0x200], [0x00824134, 0x40], [0x0058a528, 0x40]];
  function pSpec(o) {
    const s = { kind: 0x21, mask: 0x1234, story: -1, type: 1, marital: 0x08, mood: 3, likes: 5, bba: 1, baa: 120,
                ba6: 12, ba8: 20, ba0: 0x2a5b, hole: 4, b22: 2, scores: [4, 5, 3, 0, 6, 2, 7, 4, 3, 5, 4, 3, 6, 5, 4, 3, 2, 8],
                sflags: [1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1], flags: 0, b2: 0, partner: 3, name: 1,
                tk: [0, 3, 0x13, 0x32, 7, 0, 9, 0x13, 4, 5], tp: [12, 25, 7, 40, 3, 0, 18, 33, 61, 9],
                tw: [0x4001, 0xc002, 0x0003, 0x4004, 0xc005, 0x0006, 0x4007, 0x0008, 0xc009, 0x400a] };
    Object.keys(o || {}).forEach(function (k) { s[k] = o[k]; });
    return s;
  }
  function wrPanelGolfer(a, s, seed) {
    for (let o = 0; o < 0x100; o += 4) wr32(a + o, (((seed * 0x13 + o) & 0x3f) * 0x01010101) & 0x0f0f0f0f);
    wr16(a + 0x16, s.mask); wr8(a + 0x18, s.kind); wr8(a + 0x19, s.likes); wr32(a + 0x10, s.flags);
    wr8(a + 0x21, s.hole); wr8(a + 0x22, s.b22);
    for (let h = 1; h <= 18; h++) wr8(a + 0x23 + h, s.scores[h - 1]);
    for (let k = 1; k <= 18; k++) wr8(a + 0x37 + k, s.sflags[k - 1]);
    for (let k = 0; k < 10; k++) { wr8(a + 0x70 + k, s.tk[k]); wr8(a + 0x7a + k, s.tp[k]); wr16(a + 0x88 + 2 * k, s.tw[k]); }
    wr16(a + 0xa0, s.ba0); wr16(a + 0xa2, s.partner); wr16(a + 0xa4, s.mood); wr16(a + 0xa6, s.ba6); wr16(a + 0xa8, s.ba8);
    wr16(a + 0xaa, s.baa); wr16(a + 0xb0, s.story); wr16(a + 0xb2, s.b2); wr16(a + 0xb6, s.type); wr16(a + 0xb8, s.marital);
    wr8(a + 0xba, s.bba); wr8(a + 0xbb, s.name);
  }
  const GP_TRAITS = [0x13, 0x00, 0x1f, 0x93, 0x01, 0x0c, 0x10, 0x08];
  function gpWorld(s) {
    GP_SAVE.forEach(function (e) { __freeze.save(ptr(e[0]), e[1]); });
    for (let t = 0; t < 8; t++) {
      const r = 0x004d6088 + 0x230 * t;
      wrStr(r, 'Kind' + t, 0x10);
      wrStr(r + 0x10, 'Ty' + t, 0x10);
      wr8(r + 0x20, s.traits !== undefined && t === s.g % 8 ? s.traits : GP_TRAITS[t]);
      wr8(r + 0x21, 0);
    }
    for (let n = 0; n < 24; n++) wrStr(0x0058dd50 + 0x38 * n, 'Pname' + n, 0x10);
    for (let h = 0; h < 19; h++) wr8(0x00575ab0 + 0x208 * h, s.par[h % s.par.length]);
    for (let g = 0; g < 10; g++) {
      const sp = pSpec(Object.assign({ partner: g ^ 1, name: g, type: g % 8 }, (s.gl && s.gl[g]) || {}));
      wrPanelGolfer(0x005794b8 + 0x100 * g, sp, g);
    }
    wr32(0x00824144, s.storyOn);
    wr32(0x005409a8, s.dbl);
    wr32(0x00822c74, 0x00c0ffee);
    wr32(0x00824130, s.prev);
    wr32(0x00823768, s.ticks);
    wr8(0x0051a068, 0);
  }
  function gpScene(o) {
    const s = { g: 2, mode: 0, gl: {}, storyOn: 1, dbl: 0, prev: -1, ticks: 0, cur: [0x10, 0x10],
                par: [3, 4, 5, 4, 3, 4, 5, 4, 3, 4, 4, 3, 5, 4, 4, 3, 5, 4, 4] };
    Object.keys(o).forEach(function (k) { s[k] = o[k]; });
    return s;
  }
  const NB = { 0: [0x13e, 0x106], 1: [0x17f, 0x106], 2: [0x1c0, 0x106], 3: [0x201, 0x106], 4: [0x253, 0x106] };
  const SB = { 0: [0x126, 0x11f], 5: [0x167, 0x11f], 2: [0x1a8, 0x11f], 3: [0x1e9, 0x11f], 4: [0x221, 0x11f] };
  const GP_SCENES = [
    // 0: the other page, kind set (skill dialog), button 1 hovered for the seventh frame (tooltip 1), thoughts from 4
    gpScene({ cur: NB[1], prev: 1, ticks: 6 }),
    // 1: the story page, partner pair at +0xb2 = 4 hovering button 5 (it becomes -1), bit 0x100000, tooltip of -1
    gpScene({ gl: { 2: { story: 2, hole: 3, flags: 0x100000, b2: 4 }, 3: { b2: 4 } }, cur: SB[5], prev: -1, ticks: 9 }),
    // 2: the story page with hole 0 (no lines), hovering button 0 for the second frame
    gpScene({ gl: { 2: { story: 1, hole: 0, kind: 0 } }, cur: SB[0], prev: 0, ticks: 0 }),
    // 3: the story page with eight lines (the block is drawn again until the limit passes its height), hover changes
    gpScene({ gl: { 2: { story: 3, hole: 14, marital: 0x10 } }, cur: SB[2], prev: 3, ticks: 4 }),
    // 4: the other page, mode 1 (thoughts from slot 9, only 0xc000 ones), the double comment, long texts
    gpScene({ mode: 1, dbl: 1, gl: { 2: { marital: 0x20, tp: [200, 25, 7, 40, 3, 0, 18, 250, 61, 230] } } }),
    // 5: hole -1: the panel stops after the flag text
    gpScene({ gl: { 2: { hole: -1, marital: 0x40, likes: 0, bba: 2 } } }),
    // 6: kind 0, a story index with story pages off ([0x00824144] = 0): button 3 drawn, its tooltip
    gpScene({ storyOn: 0, gl: { 2: { kind: 0, story: 1, marital: 0x78, likes: 7, bba: 0 } }, cur: NB[3], prev: 3, ticks: 7 }),
    // 7: button 4 hovered past six frames (an empty tooltip), mood 9 (meters at or below 0x28)
    gpScene({ gl: { 2: { mood: 9, baa: 300, ba6: 40, ba8: 30 } }, cur: NB[4], prev: 4, ticks: 20 }),
    // 8: nothing hovered for many frames (the -1 test), a negative trait byte, hole 19 (no stroke write)
    gpScene({ traits: 0x93, gl: { 2: { hole: 19, mood: -2, baa: -40, ba6: -3, ba8: 0, likes: 2 } }, prev: -1, ticks: 30 }),
    // 9: strokes against par on every side (par 4: 2, 3, 4, 5, 6, 0), the current hole 6, mode 1 thoughts
    gpScene({ mode: 1, par: [4], gl: { 2: { hole: 6, b22: 3, scores: [2, 3, 4, 5, 6, 0, 1, 7, 4, 4, 3, 5, 2, 6, 4, 0, 0, 3] } },
              cur: NB[0], prev: 0, ticks: 6 }),
    // 10: the story page: golfer 3 (odd: partner 2), lines on every other hole, button 3 for the seventh frame
    gpScene({ g: 3, gl: { 3: { story: 4, hole: 6, sflags: [1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] } },
              cur: SB[3], prev: 3, ticks: 6 }),
    // 11: the story page, button 4 and button 2 tooltips' neighbours: button 2 at frame 7, no pair-4
    gpScene({ gl: { 2: { story: 5, hole: 2, b2: 4 }, 3: { b2: 1 } }, cur: SB[2], prev: 2, ticks: 6 }),
    // 12: the story page, button 4 for the seventh frame
    gpScene({ gl: { 2: { story: 6, hole: 1 } }, cur: SB[4], prev: 4, ticks: 6 }),
    // 13: the story page, button 0 for the seventh frame (no pair: button 5 drawn)
    gpScene({ gl: { 2: { story: 7, hole: 2 } }, cur: SB[0], prev: 0, ticks: 6 }),
    // 14: the other page, button 2 for the seventh frame, every thought kind 0 but one
    gpScene({ gl: { 2: { tk: [0, 0, 0, 0, 5, 0, 0, 0, 0, 0] } }, cur: NB[2], prev: 2, ticks: 6 }),
    // 15: the story page with the pair at 4 but button 0 hovered (not 5)
    gpScene({ gl: { 2: { story: 2, hole: 2, b2: 4 }, 3: { b2: 4 } }, cur: SB[0], prev: 0, ticks: 2 }),
    // 16: the other page, the hover moved (prev 2, now 0): the counter resets
    gpScene({ cur: NB[0], prev: 2, ticks: 5 }),
    // 17: the other page, mode 0, five non-zero thoughts in slots 4..0 (the fifth drawn ends the call)
    gpScene({ gl: { 2: { tk: [6, 3, 8, 0x32, 7, 0, 0, 0, 0, 0] } }, cur: [0x300, 0x10] }),
    // 18: the other page, button 1 for the sixth frame (no tooltip yet)
    gpScene({ cur: NB[1], prev: 1, ticks: 5 }),
  ];
  function gpCurView() { return GP.s.cur; }
  function gpStands(fx) {
    calleeStand(fx, 0, 0x0045f0f0, 5, 0, { str: { 0: 8 } });                         // skillPointDialog
    calleeStand(fx, 1, 0x00473e60, 5, 0, { abi: 'thiscall' });                       // Image_draw
    calleeStand(fx, 2, 0x004740f0, 6, 0, { abi: 'thiscall' });                       // Surface_blit2
    calleeStand(fx, 3, 0x00473f60, 6, 0, { abi: 'thiscall' });                       // Surface_stretchBlit
    calleeStand(fx, 4, 0x0045c200, 4, 0, {});                                        // drawThoughtBubble
    calleeStand(fx, 5, 0x004762d0, 5, 0, { abi: 'thiscall' });                       // setDrawTarget
    calleeStand(fx, 6, 0x00404bc0, 4, 0, { str: { 0: 12 } });                        // drawCenteredText2
    calleeStand(fx, 7, 0x00404b70, 4, 0, { str: { 0: 12 } });                        // drawCenteredText
    calleeStand(fx, 8, 0x00480b00, 6, 0, { abi: 'thiscall' });                       // fillWindowRect
    calleeStand(fx, 9, 0x00466b70, 4, 0, {});                                        // loadStoryStrings
    calleeStand(fx, 10, 0x004668f0, 3, function (n, a) {                             // storyText(story, line, golfer)
      if (((a[0] | 0) * 3 + (a[1] | 0) + (a[2] | 0)) % 4 === 0) return 0;
      wrStr(0x0051a068, 'S' + (a[0] | 0) + '.' + (a[1] | 0) + '.' + (a[2] | 0), 0x20);
      return 1;
    }, {});
    calleeStand(fx, 11, 0x0045c460, 1, function (n, a) {                             // substituteStoryNames(golfer)
      const t = ptr(0x0051a068).readUtf8String() || '';
      wrStr(0x0051a068, t + '~' + (a[0] | 0), t.length + 8);
      return 0;
    }, {});
    calleeStand(fx, 12, 0x004049d0, 4, 0, { str: { 0: 8 } });                        // drawText4049d0
    calleeStand(fx, 13, 0x00478b80, 6, 0, { abi: 'thiscall' });                      // Widget_fillRect
    calleeStand(fx, 14, 0x0047ab50, 3, function (n, a) {                             // View_toLocal(&x, &y)
      const p = gpCurView();
      ptr(a[1]).writeS32(p[0]); ptr(a[2]).writeS32(p[1]);
      return 0;
    }, { abi: 'thiscall', mask: [1, 2], extra: function () { return gpCurView(); } });
    calleeStand(fx, 15, 0x00432620, 2, function () { return 0x2620; },              // drawPanelBackdrop(x, y)
      { extra: function () { return runText(0x0051a068, 6); } });
    calleeStand(fx, 16, 0x00469b00, 4, function (n, a) {                             // buildGolferComment
      const t = ptr(0x0051a068).readUtf8String() || '';
      const add = 'C' + (a[0] | 0) + '.' + (a[1] | 0) + '.' + (a[2] | 0);
      wrStr(0x0051a068, t + add, t.length + add.length + 4);
      return 0;
    }, {});
    calleeStand(fx, 17, 0x00477250, 2, function (n, a) {                             // Text_put(text): 20 per character
      let k = 0;
      while (k < 0x200 && ptr(a[1]).add(k).readU8() !== 0) k++;
      return 20 * k;
    }, { abi: 'thiscall', str: { 1: 4 } });
    calleeStand(fx, 18, 0x00404ad0, 4, 0, { str: { 0: 12 } });                       // drawTextSurface
  }

  F.c3fr_panel = function () {
    const fx = { clog: newClog(0x8000), clogCap: 0x8000 };
    gpStands(fx);              // freezes the first thread
    GP_SCENES.forEach(function (sc, i) {
      Object.defineProperty(fx, 'p' + i, { enumerable: true, configurable: true, get: function () {
        GP.s = sc;
        gpWorld(sc);
        return sc.g;
      } });
      fx['m' + i] = sc.mode;
    });
    negGetters(fx, NEG_GP, 'p');
    return fx;
  };
  const NEG_GP = [[36, 0], [37, 3], [38, 0], [39, 0], [40, 0], [41, 9], [42, 9], [43, 1], [44, 3], [45, 1], [46, 1],
                  [47, 18], [48, 4], [49, 4], [50, 0], [51, 17], [52, 0], [53, 10], [54, 6]];

  // ====================================================================================================================
  // showCourseReport 0x0044fb30 (__cdecl (mode); one vector = one whole run, scene applied by fx.cN)
  // ====================================================================================================================
  // The scene getter writes the 18 hole records 0x00575ab0 + 0x208 h from a deterministic generator (par, length, the
  // 8 x 11 word table at +0x28, the words +0xee / +0x104 / +0x110 / +0x158, the dwords +0x20 / +0x24 / +0x1ec / +0x1f0 /
  // +0x1f4 / +0x1f8 / +0x1fc, the flag byte +0x200), the hole-name offsets 0x0059d81c (all -1, so appendHoleName uses its
  // par tables), [0x00822c88], [0x005685f0], [0x005a6370], bit 0x40 of [0x0059e7b8] (read-modify-written; the function
  // also writes bits 0x400000 / 0x2000 there, so the dword is compared), and three Image height fields the layout reads
  // ([0x00821324], [0x008213d4], [0x0082142c]); all saved and restored. Recording stand-ins: Surface_drawSprite,
  // Widget_setQuad, Surface_stretchBlit, Image_draw, Widget_draw / drawStringAt / drawStringAt2 (text logged),
  // Window::refreshFull, enterModal483d30, postEvent, View_toLocal (the scene's cursor for modal.js's visit count; the
  // button's position is computed here the way the function lays it out). The display [0x0083ad50] is a guarded recorder
  // of slot +0x9c. Real: appendHoleName (+ appendString, answering 0), appendNumber, appendCents, __itoa, clamp,
  // bucketValue; the pumps are modal.js's models.
  function vtableR(slots, size) {
    const vt = __keepAlloc(size || 0x100);
    for (let s = 0; s < (size || 0x100); s += 4) vt.add(s).writeU32(0);
    Object.keys(slots).forEach(function (k) {
      const off = parseInt(k, 10), n = slots[k][0], body = slots[k][1];
      const types = ['pointer'].concat(Array(n).fill('uint32'));
      const cb = new NativeCallback(function (self) {
        return body(self, Array.prototype.slice.call(arguments, 1)) >>> 0;
      }, 'uint32', types, 'thiscall');
      __diffKeepAlive.push(cb);
      vt.add(off).writePointer(cb);
    });
    return vt;
  }
  const CR = { s: null, blk: null, clog: null };
  const CR_SAVE = [[0x00575ab0, 0x208 * 19], [0x0059d81c, 0x30], [0x00822c88, 4], [0x005685f0, 4], [0x005a6370, 4],
                   [0x0059e7b8, 4], [0x00821324, 4], [0x008213d4, 4], [0x0082142c, 4], [0x00561250, 4], [0x00567b04, 4],
                   [0x00569498, 4], [0x0051a068, 0x200], [0x00821c50, 0x40], [0x0058a528, 0x40]];
  function lcg(st) { st.v = (Math.imul(st.v, 0x41c64e6d) + 0x3039) >>> 0; return (st.v >>> 16) & 0x7fff; }
  function rng(st, lo, hi) { return lo + lcg(st) % (hi - lo + 1); }
  // hole h's record from the scene: s.kind 'mixed' (random over every range), 'good' (every criterion met), 'zero'
  // (every par 0), with per-hole overrides s.over[h]
  function crHole(s, h, st) {
    const r = 0x00575ab0 + 0x208 * h;
    for (let o = 0; o < 0x208; o += 4) wr32(r + o, 0);
    const o = Object.assign({}, (s.over && s.over[h]) || {});
    let par, len, tab = [], ee, f04, f10, f158, f20, f24, f1ec, f1f0, f1f4, f1f8, f1fc, fl;
    if (s.kind === 'good') {
      // row 7 at column gl, rows 6 / 5 / 3 at column gh (gl 1, gh 9: differences of several hundred; gl 4, gh 5:
      // about 70), the other rows at the par
      // s.weakFrom: holes above it have no difference (gl = gh = 4), a low +0xee / +0x104 / +0x110 sum and +0x1fc = 2;
      // s.tph: the time per hole; s.f158: the pace numerator
      const weak = s.weakFrom !== undefined && h > s.weakFrom;
      const gl = weak ? 4 : (s.gl || 1), gh = weak ? 4 : (s.gh || 9);
      par = 4; len = 400 + 10 * h;
      for (let row = 0; row < 8; row++) {
        tab.push([]);
        for (let j = 0; j < 11; j++) tab[row].push(row === 7 ? (j === gl ? 20 : 0) : (row === 6 || row === 5 || row === 3) ? (j === gh ? 20 : 0) : (j === 4 ? 3 : 0));
      }
      ee = weak ? 0 : 3; f04 = weak ? 0 : 6; f10 = weak ? 0 : 4;
      f158 = s.slow ? 0 : (s.f158 !== undefined ? s.f158 : 30); f20 = 20; f24 = 0; f1ec = (s.tph || 0) * 800;
      f1f0 = 10; f1f4 = 30; f1f8 = 5; f1fc = weak ? 2 : 0; fl = 0x21;
    } else {
      const p = rng(st, 0, 9);
      par = s.kind === 'zero' ? 0 : (p === 0 ? 0 : [3, 4, 5][p % 3]);
      len = rng(st, 80, 620);
      for (let row = 0; row < 8; row++) {
        tab.push([]);
        // rows 0, 1, 2 and 4 are divisors under bit 0x40: never negative there
        const neg = (row === 3 || row >= 5) && s.neg;
        for (let j = 0; j < 11; j++) tab[row].push(rng(st, neg ? -2 : 0, 6));
      }
      ee = rng(st, -2, 6); f04 = rng(st, -4, 12); f10 = rng(st, -2, 6);
      f158 = rng(st, -10, 70); f20 = rng(st, 0, 3) === 0 ? 0 : rng(st, 1, 30); f24 = rng(st, 0, 40);
      f1ec = rng(st, 0, 9000); f1f0 = rng(st, -50, 200); f1f4 = rng(st, -50, 300); f1f8 = rng(st, -30, 80);
      f1fc = rng(st, 0, 4); fl = rng(st, 0, 255);
      if (s.slow) f158 = 0;
    }
    if (o.par !== undefined) par = o.par;
    if (o.rowSum) o.rowSum.forEach(function (rs) {   // [row, total]: a row whose words add up to total (-8 zeroes it)
      for (let j = 0; j < 11; j++) tab[rs[0]][j] = 0;
      tab[rs[0]][1] = rs[1];
    });
    if (o.zeroTab) tab = tab.map(function (row) { return row.map(function () { return 0; }); });
    ['f20', 'f1ec', 'f158', 'f1fc', 'fl', 'f1f4', 'f1f0', 'f1f8'].forEach(function (k) {
      if (o[k] !== undefined) { if (k === 'f20') f20 = o[k]; if (k === 'f1ec') f1ec = o[k]; if (k === 'f158') f158 = o[k];
        if (k === 'f1fc') f1fc = o[k]; if (k === 'fl') fl = o[k]; if (k === 'f1f4') f1f4 = o[k]; if (k === 'f1f0') f1f0 = o[k];
        if (k === 'f1f8') f1f8 = o[k]; }
    });
    wr8(r, par); wr16(r + 4, len);
    wr32(r + 8, 3 + h); wr32(r + 0xc, 20 + h);
    wr32(r + 0x20, f20); wr32(r + 0x24, f24);
    for (let row = 0; row < 8; row++) for (let j = 0; j < 11; j++) wr16(r + 0x28 + 2 * (11 * row + j), tab[row][j]);
    wr16(r + 0xee, ee); wr16(r + 0x104, f04); wr16(r + 0x110, f10); wr16(r + 0x158, f158);
    wr32(r + 0x1ec, f1ec); wr32(r + 0x1f0, f1f0); wr32(r + 0x1f4, f1f4); wr32(r + 0x1f8, f1f8); wr32(r + 0x1fc, f1fc);
    wr8(r + 0x200, fl);
    return par;
  }
  function crWorld(s) {
    CR_SAVE.forEach(function (e) { __freeze.save(ptr(e[0]), e[1]); });
    const st = { v: s.seed >>> 0 };
    let drawn = 0;
    for (let h = 0; h < 19; h++) {
      const par = crHole(s, h, st);
      if (h >= 1 && par !== 0) drawn++;
    }
    for (let i = 0; i < 0x30; i += 2) wr16(0x0059d81c + i, -1);
    wr32(0x00822c88, s.diff);
    wr32(0x005685f0, s.holes);
    wr32(0x005a6370, s.fac);
    const fl = rd32(0x0059e7b8) >>> 0;
    wr32(0x0059e7b8, s.f40 ? (fl | 0x40) : (fl & ~0x40));
    wr32(0x00821324, 0x40); wr32(0x008213d4, 0x14); wr32(0x0082142c, 0x50);
    wr8(0x0051a068, 0);
    // the button (0x004530fa / 0x004530fd for mode 0; 0x2bd / 0x18e otherwise): mode 0 rows are 0x11 apart from
    // [0x00821324]; the subtotal (with any hole counted) moves y to [0x008213d4] + y + 9 - 0xb; the foot's line is
    // y (- 2 without holes) + [0x0082142c] - 0x3f, the button 0xf below it
    if (s.mode === 0) {
      let y = 0x40 + 0x11 * drawn;
      if (drawn > 0) y = 0x14 + y + 9 - 0xb; else y -= 2;
      s.btn = [0x2de, y + 0x50 - 0x3f + 0xf];
    } else {
      s.btn = [0x2bd, 0x18e];
    }
  }
  function crScene(o) {
    const s = { mode: 0, kind: 'mixed', seed: 1, diff: 0, holes: 19, fac: 0x000ffc0, f40: 0, neg: false, over: {},
                exit: 'click' };
    Object.keys(o).forEach(function (k) { s[k] = o[k]; });
    return s;
  }
  // the cursor for visit n, from the scene's list of places around the button (left of it, on it, right of it,
  // above, below); the click or key arrives on the last frame
  const CR_VIEW = ['out', 'in', 'in', 'right', 'above', 'in', 'below', 'out', 'in'];
  function crPoint(s, k) {
    const b = s.btn;
    return { out: [b[0] - 30, b[1] + 10], in: [b[0] + 10, b[1] + 10], right: [b[0] + 0x2c, b[1] + 10],
             above: [b[0] + 10, b[1] - 1], below: [b[0] + 10, b[1] + 0x2c] }[k];
  }
  function crView(n) {
    const s = CR.s, v = s.view || CR_VIEW;
    return crPoint(s, v[Math.min(n, v.length - 1)]);
  }
  function crScript(s) {
    const v = s.view || CR_VIEW, p = crPoint(s, v[v.length - 1]);
    const last = s.exit === 'key' ? [{ k: 'key', a: 0x1b, code: 0x1b }] : [press(p[0], p[1])];
    return v.map(function (x, i) { return i === v.length - 1 ? last : []; });
  }
  const CR_SCENES = [
    // 0..3: the per-hole table, mixed holes, both thresholds, bit 0x40 off and on
    crScene({ seed: 11 }),
    crScene({ seed: 23, diff: 1, f40: 1 }),
    crScene({ seed: 37, neg: true, over: { 5: { rowSum: [[7, -8]] }, 6: { rowSum: [[6, -8]] }, 7: { rowSum: [[5, -8]] },
                                          8: { rowSum: [[3, -8]] }, 9: { zeroTab: true }, 10: { f20: 0 } } }),
    crScene({ seed: 41, f40: 1, exit: 'key', over: { 18: { par: 0 } } }),
    // 4: no hole with a par (no rows, no subtotal; the foot 2 higher)
    crScene({ kind: 'zero' }),
    // 5: mode 2: drawn and returned before the loop
    crScene({ mode: 2, seed: 53 }),
    // 6..9: the grading page by course size (bucketValue of 5, 9, 14, 18) with mixed holes
    crScene({ mode: 1, holes: 6, seed: 61 }),
    crScene({ mode: 1, holes: 10, seed: 67, f40: 1, fac: 0x3c0 }),
    crScene({ mode: 1, holes: 15, seed: 71, diff: 1 }),
    crScene({ mode: 1, holes: 19, seed: 73, fac: 0 }),
    // 10: every criterion met on 18 holes: score 100 (the three tour events)
    crScene({ mode: 1, holes: 19, kind: 'good' }),
    // 11: the same with 10 holes counted (the first event only)
    crScene({ mode: 3, holes: 11, kind: 'good' }),
    // 12: the grading page without any hole (no grades)
    crScene({ mode: 1, holes: 10, kind: 'zero' }),
    // 13, 14: the table with every criterion met (subtotal time under an hour, averages of several hundred), and with
    //    averages near 70
    crScene({ kind: 'good', view: ['in', 'in', 'out', 'in'] }),
    crScene({ kind: 'good', gl: 4, gh: 5 }),
    // 15: more than 19 holes: bucketValue -1, the page keeps the loop's last grade values
    crScene({ mode: 1, holes: 25, kind: 'good' }),
    // 16: score 100 on a 9-hole size ([0x005685f0] = 10): the first event and the 100 test below 19
    crScene({ mode: 1, holes: 10, kind: 'good' }),
    // 17: no pace at all (pace grade 0)
    crScene({ mode: 1, holes: 19, kind: 'good', slow: true }),
    // 18..20: mixed grading pages for low scores
    crScene({ mode: 1, holes: 19, seed: 83, fac: 0x3fc0 }),
    crScene({ mode: 1, holes: 15, seed: 89, kind: 'good', gl: 4, gh: 5, fac: 0x7c0 }),
    crScene({ mode: 1, holes: 6, seed: 97, slow: true, fac: 0xfffc0 }),
    // 21: every criterion low but none 0: a score of 42 (the grade table's default entry)
    crScene({ mode: 1, holes: 19, kind: 'good', weakFrom: 11, tph: 16, f158: 4, fac: 0x1fc0 }),
  ];
  function crStands(fx) {
    calleeStand(fx, 0, 0x00474440, 7, 0, { abi: 'thiscall' });                       // Surface_drawSprite
    calleeStand(fx, 1, 0x00476310, 5, function (n, a) { return a[3]; }, { abi: 'thiscall' });   // Widget_setQuad
    calleeStand(fx, 2, 0x00473f60, 6, 0, { abi: 'thiscall' });                       // Surface_stretchBlit
    calleeStand(fx, 3, 0x00473e60, 5, 0, { abi: 'thiscall' });                       // Image_draw
    calleeStand(fx, 4, 0x00476650, 6, 0, { abi: 'thiscall', str: { 2: 10 } });      // Widget_draw
    calleeStand(fx, 5, 0x00476700, 6, 0, { abi: 'thiscall', str: { 2: 10 } });      // drawStringAt
    calleeStand(fx, 6, 0x00476750, 6, 0, { abi: 'thiscall', str: { 2: 10 } });      // drawStringAt2
    calleeStand(fx, 7, 0x00480c80, 2, 0, { abi: 'thiscall' });                       // Window::refreshFull
    calleeStand(fx, 8, 0x00483d30, 0, 0, {});                                        // enterModal483d30
    calleeStand(fx, 9, 0x0046e7b0, 3, 0, {});                                        // postEvent
    calleeStand(fx, 10, 0x0047ab50, 3, function (n, a) {                             // View_toLocal(&x, &y)
      const p = crView(CR.blk.add(4).readU32());
      ptr(a[1]).writeS32(p[0]); ptr(a[2]).writeS32(p[1]);
      return 0;
    }, { abi: 'thiscall', mask: [1, 2], extra: function () { return crView(CR.blk.add(4).readU32()); } });
  }
  const CR_DISP = zalloc(0x40);
  CR_DISP.writePointer(vtableR({ 0x9c: [3, function (s, a) {
    const l = CR.clog;
    if (l) { const c = l.add(4 + 4 * 31); c.writeU32(c.readU32() + 1 + (a[0] << 4) + (a[1] << 8) + (a[2] << 12)); }
    return 0;
  }] }, 0x400));

  F.c3fr_report = function () {
    const fx = { clog: newClog(0x10000), clogCap: 0x10000 };
    CR.clog = fx.clog;
    const M = __modal({ maxCalls: 400, maxFrames: 400 });
    __importStandIn({ module: 'golf_clean.exe', imports: M.imports() });
    CR.blk = M.fx().modal;
    crStands(fx);              // freezes the first thread
    M.arm();
    __freeze.save(ptr(0x0083ad50), 4);
    ptr(0x0083ad50).writePointer(__recDisplay.guard(CR_DISP));
    CR_SCENES.forEach(function (sc, i) {
      Object.defineProperty(fx, 'c' + i, { enumerable: true, configurable: true, get: function () {
        CR.s = sc;
        crWorld(sc);
        M.select(crScript(sc));
        return sc.mode;
      } });
    });
    negGetters(fx, NEG_CR, 'c');
    return Object.assign(fx, M.fx());
  };
  const NEG_CR = [[55, 0], [56, 0], [57, 0], [58, 0], [59, 0], [60, 0], [61, 6], [62, 10], [63, 21], [64, 21], [65, 21],
                  [66, 21], [67, 10], [68, 10], [69, 10], [70, 10], [71, 0], [72, 0], [73, 10], [74, 7], [75, 0]];

  const NEG_NS = [[16, 5], [17, 0], [18, 38], [19, 2], [20, 35], [21, 32], [22, 32], [23, 46], [24, 28], [25, 23],
                  [26, 14], [27, 66], [28, 3], [29, 0], [30, 3], [31, 0], [32, 0], [33, 67], [34, 4], [35, 23]];

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
