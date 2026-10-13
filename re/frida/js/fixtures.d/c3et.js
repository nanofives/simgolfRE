// Fixtures for batch c3et (round 32, 2026-10-13): the large exe screens and round 31's drops.
// Reimplementations: shim/src/re/c3et.cpp. Registry: re/frida/registry.d/c3et.py.
// Purposes and per-jcc tables: log/c3/c3et_purpose.md. Notes: log/c3/c3et_notes.txt.
(function () {
  const F = {};
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };

  function zalloc(n) {
    const p = __keepAlloc(n);
    for (let i = 0; i < n; i += 4) p.add(i).writeU32(0);
    return p;
  }
  function fnvWord(h, w) {
    for (let k = 0; k < 4; k++) { h ^= (w >>> (8 * k)) & 0xff; h = Math.imul(h, 0x01000193) >>> 0; }
    return h >>> 0;
  }

  // ---- a call log owned by the current key: +0 cursor, +4 calls, +8 FNV-1a over every record, entries from +0x10
  // {tag, n, values...} while they fit. Compared state ($tlog).
  const TLOG = 0x2000;
  const L = { log: null };
  function newLog() {
    const l = zalloc(TLOG);
    l.writeU32(0x10);
    l.add(8).writeU32(0x811c9dc5);
    L.log = l;
    __freeze.atFlush(function () { if (L.log === l) L.log = null; });
    return l;
  }
  function rec(tag, vals) {
    const l = L.log;
    if (l === null) return;
    l.add(4).writeU32(l.add(4).readU32() + 1);
    let h = fnvWord(l.add(8).readU32(), tag >>> 0);
    vals.forEach(function (v) { h = fnvWord(h, v >>> 0); });
    l.add(8).writeU32(h);
    const c = l.readU32(), need = 8 + 4 * vals.length;
    if (c + need > TLOG) return;
    l.add(c).writeU32(tag >>> 0);
    l.add(c + 4).writeU32(vals.length);
    vals.forEach(function (v, i) { l.add(c + 8 + 4 * i).writeU32(v >>> 0); });
    l.writeU32(c + need);
  }
  // the bytes of a counted run as words (at most `cap` bytes), plus the run's FNV over all its bytes
  function runWords(p, n, cap) {
    const out = [];
    if (n <= 0) return [0];
    let h = 0x811c9dc5;
    for (let i = 0; i < n; i++) { h ^= p.add(i).readU8(); h = Math.imul(h, 0x01000193) >>> 0; }
    out.push(h >>> 0);
    const m = Math.min(n, cap);
    for (let i = 0; i < m; i += 4) {
      let w = 0;
      for (let k = 0; k < 4 && i + k < m; k++) w |= p.add(i + k).readU8() << (8 * k);
      out.push(w >>> 0);
    }
    return out;
  }
  // a recording vtable: slots {off: [nargs, fn(self, args) -> value]}; every slot not given is NULL
  function vtable(slots, size) {
    const vt = keep(Memory.alloc(size || 0x100));
    for (let s = 0; s < (size || 0x100); s += 4) vt.add(s).writeU32(0);
    Object.keys(slots).forEach(function (k) {
      const off = parseInt(k, 10), n = slots[k][0], body = slots[k][1];
      const types = ['pointer'].concat(Array(n).fill('uint32'));
      vt.add(off).writePointer(keep(new NativeCallback(function (self) {
        return body(self, Array.prototype.slice.call(arguments, 1)) >>> 0;
      }, 'uint32', types, 'thiscall')));
    });
    return vt;
  }

  // ====================================================================================================================
  // allocTextBuffer 0x004767a0 (thiscall: this, text, x, y, len)
  // ====================================================================================================================
  // Everything the function reaches is fixture-owned: the text object `this`, its recording surface (+4), four fonts
  // (+0x5c..+0x68) whose measurers (+4 of a font, slot +0x10, reached through measureTextWidth 0x00483930, C3 c3ab) record
  // the run they are given, the three palette objects (the global [0x0083ad18] + 4, the surface's slot +0xe8 result, the
  // global [0x0083ad0c] + 4) and the override callback (+0xc). The display [0x0083ad50] is uitree.js's recording display;
  // its slot +0xb4 (the 5-6-5 test) answers the scene's value. malloc 0x004a504f / free 0x004a5007 go through uitree's
  // heap stand-in, so the working buffer lives in the compared arena at the same address in both arms, and a scene can
  // make that allocation fail. Runs copied into the function's frame buffer are logged by CONTENT, never by address.
  const TB = { ctl: zalloc(0x40) };   // +0 pixel format dword, +4 display slot +0xb4 answer, +8 callback result, +0xc e8
  const PF = TB.ctl;                  // surface slot +0xe4 returns a pointer to this dword

  const SURF_VT = vtable({
    0xa8: [1, function (s, a) { rec(0xa8, [a[0]]); return 0; }],                       // setFont(font + 4)
    0xac: [0, function () { rec(0xac, []); return 0; }],                                 // end of the run block
    0xb0: [1, function (s, a) { rec(0xb0, [a[0]]); return 0; }],                       // colour by index
    0xb4: [3, function (s, a) { rec(0xb4, [a[0], a[1], a[2]]); return 0; }],           // colour by components
    0xb8: [4, function (s, a) {                                                          // draw(x, y, text, count)
      rec(0xb8, [a[0], a[1], a[3]].concat(runWords(ptr(a[2]), a[3] | 0, 0x40)));
      return 0;
    }],
    0xe4: [0, function () { rec(0xe4, []); return PF.toUInt32(); }],                   // pixel format
    0xe8: [0, function () { rec(0xe8, []); return TB.ctl.add(0xc).readU32(); }],       // the surface's palette
  });
  const SURF = zalloc(0x40);
  SURF.writePointer(SURF_VT);

  // measurer k: width = 5 * n + k + (the first byte & 3); an empty run is 0 wide, a negative count is not read
  function measurerVt(k) {
    return vtable({ 0x10: [2, function (s, a) {
      const n = a[1] | 0;
      rec(0x100 + k, [n].concat(runWords(ptr(a[0]), n, 0x20)));
      if (n === 0) return 0;
      if (n < 0) return 1000 + n;
      return 5 * n + k + (ptr(a[0]).readU8() & 3);
    }] });
  }
  const MEAS = [0, 1, 2, 3].map(function (k) { const m = zalloc(0x10); m.writePointer(measurerVt(k)); return m; });
  const FONTS = [0, 1, 2, 3].map(function () { return zalloc(0x20); });

  // palette object q: slot +0x20(index) returns the address of a 3-byte entry of its own table
  function palette(q) {
    const tab = zalloc(0x100);
    for (let i = 0; i < 0x100; i++) tab.add(i).writeU8((q * 0x51 + i * 7) & 0xff);
    const o = zalloc(0x10);
    o.writePointer(vtable({ 0x20: [1, function (s, a) {
      rec(0x200 + q, [a[0]]);
      return tab.add(4 * (a[0] & 0x3f)).toUInt32();
    }] }));
    return o;
  }
  const PALS = [palette(1), palette(2), palette(3)];
  const HOLD18 = zalloc(0x10), HOLD0C = zalloc(0x10);   // the objects the two globals point at: +4 = the palette
  HOLD18.add(4).writePointer(PALS[0]);
  HOLD0C.add(4).writePointer(PALS[1]);

  const CB = keep(new NativeCallback(function (t, x, y, n) {
    rec(0xcb, [ptr(t).toUInt32(), x, y, n]);
    return TB.ctl.add(8).readU32();
  }, 'int', ['pointer', 'int', 'int', 'int'], 'mscdecl'));

  const OBJ = zalloc(0xc0);

  // scene defaults; `fonts` heights with null for a missing font, `meas` false for a font without a measurer
  function tbScene(o) {
    const s = {
      cb: null, fmt: 8, g18: 0, e8: 0, g0c: 1, disp: 0, multi: 0, state: 0, flags: 0,
      fonts: [12, 10, 8, 6], meas: [true, true, true, true],
      colours: [5, 6, 7, 8], shadows: [-1, -1, -1, -1], dx: [1, 2, 3, 4], dy: [2, 3, 4, 5],
      text: 'AB_CD', len: undefined, x: 10, y: 20, fail: 0, surf: 1,
    };
    Object.keys(o).forEach(function (k) { s[k] = o[k]; });
    if (s.len === undefined) s.len = s.text.length;
    return s;
  }
  const TB_SCENES = [
    tbScene({}),                                                                                   // 0
    tbScene({ cb: 77, text: 'XYZ', x: 3 }),                                                        // 1 callback answers
    tbScene({ cb: 0, shadows: [3, -1, -1, -1], text: 'ab\tcd' }),                                  // 2 callback 0, shadow
    tbScene({ text: 'Q', len: 0 }),                                                                // 3 len 0
    tbScene({ surf: 0, text: 'abc' }),                                                             // 4 no surface
    tbScene({ fail: 1, fmt: 0x10, g18: 1, shadows: [0xff, -1, -1, -1], colours: [7, 0, 0, 0], text: 'a_b' }),   // 5
    tbScene({ fmt: 0x10, e8: 1, disp: 1, shadows: [9, -1, -1, -1], colours: [0x80001234, 0, 0, 0],
              text: 'one\ttwo_three' }),                                                           // 6
    tbScene({ fmt: 0x10, g0c: 1, disp: 0, shadows: [0x8000abcd | 0, -1, -1, -1],
              colours: [0x80005555 | 0, 0, 0, 0], text: 'x\ty' }),                                 // 7
    tbScene({ multi: 1, state: 2, fonts: [3, 9, 5, 4], colours: [6, 1, -1, 2], shadows: [2, 1, 1, 1],
              dx: [-1, 0, 0, 0], dy: [-1, 0, 0, 0], text: 'M_N' }),                                // 8
    tbScene({ multi: 1, state: 1, fonts: [0, null, 7, 2], colours: [1, 8, 0, 0], shadows: [-1, -1, 0, 0],
              text: 'hello' }),                                                                    // 9
    tbScene({ multi: 1, state: 3, fonts: [-4, 2, null, 6], colours: [1, 1, 1, 0x80000f0f | 0], disp: 1,
              shadows: [0, 0, 0, 0x10], dx: [0, 0, 0, 7], dy: [0, 0, 0, -9], text: 'state3' }),    // 10
    tbScene({ flags: 4, colours: [11, 0, 0, 0], text: 'flag' }),                                   // 11 flag 4, 8 bpp
    tbScene({ text: '\t\tX\t', x: -40, colours: [1, 0, 0, 0] }),                                   // 12 empty runs, x < 0
    tbScene({ fail: 1, text: 'neg', len: -3 }),                                                    // 13 len < 0
    tbScene({ fmt: 0x10, g18: 1, shadows: [0x10, -1, -1, -1], colours: [0x80000001 | 0, 0, 0, 0], disp: 0,
              text: 'pal\tette' }),                                                                // 14
    tbScene({ text: 'AB\u0000CD', len: 5 }),                                                       // 15 NUL in the run
    tbScene({ meas: [false, true, true, true], text: 'm\tn', shadows: [4, -1, -1, -1] }),          // 16 no measurer
    tbScene({ text: (function () {                                                                 // 17 long text
      let t = '';
      for (let i = 0; i < 200; i++) t += (i % 37 === 36) ? '\t' : (i % 11 === 5 ? '_' : String.fromCharCode(0x41 + (i * 7) % 26));
      return t;
    })(), x: 0, y: -5 }),
    tbScene({ fmt: 0x10, e8: 1, disp: 1, shadows: [0x80007c1f | 0, -1, -1, -1], colours: [21, 0, 0, 0],
              text: 'sh_bit' }),                                                                   // 18
    tbScene({ multi: 1, state: 0, fonts: [5, 5, 5, 5], colours: [-1, 0, 0, 0], shadows: [0xff, 0, 0, 0],
              text: 'eq' }),                                                                       // 19
    tbScene({ multi: 1, state: 1, fonts: [null, 4, 3, 2], colours: [2, 3, 4, 5], shadows: [-1, 6, -1, -1],
              text: 'f0_null' }),                                                                  // 20 font 0 missing
  ];
  const TB_TEXT = TB_SCENES.map(function (s) {
    const p = zalloc(s.text.length + 8);
    for (let i = 0; i < s.text.length; i++) p.add(i).writeU8(s.text.charCodeAt(i) & 0xff);
    return p;
  });

  function tbApply(s, arena) {
    // the object
    for (let i = 0; i < 0xc0; i += 4) OBJ.add(i).writeU32(0);
    OBJ.add(4).writePointer(s.surf ? SURF : ptr(0));
    OBJ.add(0xc).writePointer(s.cb === null ? ptr(0) : CB);
    OBJ.add(0x20).writeU8(s.flags);
    OBJ.add(0x30).writeU32(s.state);
    OBJ.add(0x40).writeU32(s.multi);
    for (let k = 0; k < 4; k++) {
      const f = FONTS[k];
      if (s.fonts[k] === null) { OBJ.add(0x5c + 4 * k).writeU32(0); continue; }
      f.add(4).writePointer(s.meas[k] ? MEAS[k] : ptr(0));
      f.add(0x10).writeS32(s.fonts[k]);
      OBJ.add(0x5c + 4 * k).writePointer(f);
      OBJ.add(0x6c + 4 * k).writeS32(s.colours[k]);
      OBJ.add(0x7c + 4 * k).writeS32(s.shadows[k]);
      OBJ.add(0x8c + 4 * k).writeS32(s.dx[k]);
      OBJ.add(0x9c + 4 * k).writeS32(s.dy[k]);
    }
    for (let k = 0; k < 4; k++) {   // a missing font still has its colour words
      OBJ.add(0x6c + 4 * k).writeS32(s.colours[k]);
      OBJ.add(0x7c + 4 * k).writeS32(s.shadows[k]);
      OBJ.add(0x8c + 4 * k).writeS32(s.dx[k]);
      OBJ.add(0x9c + 4 * k).writeS32(s.dy[k]);
    }
    // the controls
    PF.writeU32(s.fmt);
    TB.ctl.add(4).writeU32(s.disp);
    TB.ctl.add(8).writeU32(s.cb === null ? 0 : s.cb);
    TB.ctl.add(0xc).writePointer(s.e8 ? PALS[2] : ptr(0));
    ptr(0x0083ad18).writePointer(s.g18 ? HOLD18 : ptr(0));
    ptr(0x0083ad0c).writePointer(s.g0c ? HOLD0C : ptr(0));
    // the working buffer's allocation fails when the scene says so (arena +4 call counter, +8 fail mask)
    arena.add(8).writeU32(s.fail ? (1 << arena.add(4).readU32()) >>> 0 : 0);
  }

  // ---- negative controls (the build of log/c3/alt/c3et_negctl_build.ps1): word 2 of the exported C3etNegCtl switch
  // picks which single wrong detail is live. fx.n<j> selects edit table[j][0] and then applies scene table[j][1]
  // through the key's own getter (base + scene). In the normal build there is no such export and these getters only
  // apply the scene. The tables are mirrored in re/frida/registry.d/c3et.py.
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
  const NEG_TB = [[1, 0], [2, 5], [3, 5], [4, 2], [5, 8], [6, 0], [7, 6], [8, 11], [9, 8]];
  const NEG_GP = [[10, 9], [11, 2], [12, 2], [13, 4], [14, 1]];
  const NEG_LB = [[15, 3], [16, 3], [17, 6], [18, 3]];
  const NEG_TV = [[19, 0], [20, 21], [21, 0], [22, 23], [23, 0], [24, 0]];
  const NEG_RM = [[25, 0], [26, 2], [27, 6], [28, 3], [29, 3], [30, 4], [31, 9], [32, 3], [33, 11], [34, 4],
                  [35, 1], [36, 8]];

  F.c3et_textrun = function () {
    const T = __uiTree({ arenaSize: 0x2000, dispRet: { 0xb4: function () { return TB.ctl.add(4).readU32(); } } });
    const tlog = newLog();
    __freeze.save(ptr(0x0083ad18), 4);
    __freeze.save(ptr(0x0083ad0c), 4);
    const fx = T.fx({ tlog: tlog });
    TB_SCENES.forEach(function (s, i) {
      Object.defineProperty(fx, 'o' + i, { enumerable: true, configurable: true, get: function () {
        tbApply(s, T.arena);
        return OBJ;
      } });
      fx['t' + i] = TB_TEXT[i];
      fx['x' + i] = s.x;
      fx['y' + i] = s.y;
      fx['l' + i] = s.len;
    });
    negGetters(fx, NEG_TB, 'o');
    return fx;
  };

  // ====================================================================================================================
  // shared: calleeStand (c3er.js's, with Interceptor.replaceFast where it exists so a foreign thread reaches the real
  // body through the trampoline instead of re-entering the stub). Log word 0 = cursor, words 1..32 = call count of stub
  // k, word 0x84 = FNV-1a over every call (callee address and logged values), entries {va, count, values...} from +0x90.
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
  function wr32(a, v) { ptr(a).writeU32(v >>> 0); }
  function wr16(a, v) { ptr(a).writeU16(v & 0xffff); }
  function wr8(a, v) { ptr(a).writeU8(v & 0xff); }
  function wrStr(a, s, max) {
    const p = ptr(a);
    for (let i = 0; i < max; i++) p.add(i).writeU8(i < s.length ? s.charCodeAt(i) & 0xff : 0);
  }
  function press(x, y) { return { k: 'press1', x: x, y: y }; }
  function press2(x, y) { return { k: 'press2', x: x, y: y }; }
  function keyEsc() { return { k: 'key', a: 0x1b, code: 0x1b }; }

  // ====================================================================================================================
  // showGolferPairSelect 0x00459850 (__cdecl, no arguments; one vector = one whole run, scene applied by fx.gN)
  // ====================================================================================================================
  const GREC = 0x005794b8, GSTRIDE = 0x100, NGOLF = 0x98;     // golfer records (swapTableEntry 0x0045de30's base)
  const TREC = 0x004d6088, TSTRIDE = 0x230;                    // golfer-type records
  const GP_SAVE = [[GREC, GSTRIDE * NGOLF + 0x100], [TREC, TSTRIDE * 8], [0x005a5a24, 4], [0x00821164, 4],
                   [0x0051a068, 0x200], [0x00821c50, 0x40]];
  // one synthetic golfer type per index: names, trait bits (+0x20), the bit-7 byte typeBit7Clear reads (+0x21) and the
  // image column (+0x22)
  const GP_TYPES = [
    { name: 'TYPEA', t: 0x00, b21: 0x00, col: 0 }, { name: 'TYPEB', t: 0x1f, b21: 0x80, col: 1 },
    { name: 'TYPEC', t: 0x07, b21: 0x00, col: 2 }, { name: 'TYPED', t: 0x18, b21: 0x80, col: -1 },
    { name: 'TYPEE', t: 0x15, b21: 0x7f, col: 3 }, { name: 'TYPEF', t: 0x0a, b21: 0x00, col: 4 },
    { name: 'TYPEG', t: 0x1c, b21: 0x81, col: 5 }, { name: 'TYPEH', t: 0x03, b21: 0x00, col: 6 },
  ];
  function gpTypes() {
    GP_TYPES.forEach(function (t, i) {
      const r = TREC + TSTRIDE * i;
      wrStr(r, t.name, 0x10);
      wrStr(r + 0x10, 'NAME' + t.name.slice(4), 0x10);   // the name buildGolferName's default arm appends (+0x10)
      wr8(r + 0x20, t.t);
      wr8(r + 0x21, t.b21);
      wr8(r + 0x22, t.col);
    });
  }
  // golfers: [{g, type, flags, kind}]; every other record's +0x21 is cleared
  function gpGolfers(list) {
    for (let g = 0; g < NGOLF; g++) wr8(GREC + GSTRIDE * g + 0x21, 0);
    list.forEach(function (o) {
      const r = GREC + GSTRIDE * o.g;
      wr8(r + 0x21, 0xff);
      wr16(r + 0xb6, o.type);
      wr16(r + 0xb8, o.flags || 0);
      wr8(r + 0x18, o.kind || 0);
      wr32(r + 0x10, 0x100 * o.g + 3);
    });
  }
  // slot k's centre (columns of 0x14e from x 6, rows of 0x88 from y 0x32); OUT is left of the list
  function slotPt(k) { return [(k & 1) ? 6 + 0x14e + 40 : 46, 0x32 + 0x88 * (k >> 1) + 30]; }
  const OUT = [0, 0];

  // scenes: golfers, start [0x005a5a24], bound [0x00821164], frames (script) and view (one cursor per loop iteration)
  function gpScene(golfers, start, bound, steps) {
    return { golfers: golfers, start: start, bound: bound,
             script: steps.map(function (s) { return s[1] ? [s[1]] : []; }),
             view: steps.map(function (s) { return s[0]; }) };
  }
  function G(g, type, flags, kind) { return { g: g, type: type, flags: flags, kind: kind }; }
  const GP_SCENES = [
    // 0: three golfers from start 12 (slots 10, 7, 3); hover slot 0, select it, hover and select slot 1, leave, key:
    //    exactly two selected and the second is not the last drawn -> two swaps, the flags and the pairing
    gpScene([G(3, 0, 0x08), G(7, 1, 0x10), G(10, 2, 0x20)], 12, 0x200,
            [[slotPt(0)], [slotPt(0), press(46, 80)], [slotPt(1)], [slotPt(1), press(400, 80)], [OUT], [OUT, keyEsc()]]),
    // 1: a click outside the list with nothing selected: the exit returns the click word
    gpScene([G(0, 3, 0x40), G(1, 4, 0)], 5, 0x200, [[OUT, press(1, 1)]]),
    // 2: select two, a third click is refused (sound), a click that arrives with a hover change is dropped, deselect
    //    one, a key: one selected -> the refusal sound and return
    gpScene([G(20, 5, 0x18), G(21, 6, 0x60), G(22, 7, 0x48), G(23, 0, 0x30)], 30, 0x200,
            [[slotPt(0)], [slotPt(0), press(46, 80)], [slotPt(1)], [slotPt(1), press(400, 80)], [slotPt(2)],
             [slotPt(2), press(46, 220)], [slotPt(3), press(400, 220)], [slotPt(3)], [slotPt(1)],
             [slotPt(1), press(400, 80)], [OUT], [OUT, keyEsc()]]),
    // 3: a click on an empty slot (index >= the number drawn) is refused, then a click outside ends it
    gpScene([G(40, 1, 0)], 41, 0x200, [[slotPt(3)], [slotPt(3), press(400, 220)], [OUT], [OUT, press(1, 1)]]),
    // 4: the second selected is the last golfer drawn -> one swap (the first into lastGolfer ^ 1)
    gpScene([G(50, 2, 0), G(51, 3, 0x08), G(52, 4, 0x10)], 52, 0x200,
            [[slotPt(0)], [slotPt(0), press(46, 80)], [slotPt(2)], [slotPt(2), press(46, 220)], [OUT],
             [OUT, keyEsc()]]),
    // 5: two of the same trait set (rating 0), then a key
    gpScene([G(60, 1, 0), G(61, 1, 0x20), G(62, 0, 0)], 70, 0x200,
            [[slotPt(0)], [slotPt(0), press(46, 80)], [slotPt(1)], [slotPt(1), press(400, 80)], [OUT],
             [OUT, keyEsc()]]),
    // 6: two types sharing no trait bit (0x00 and 0x1f: rating 2), selected with right clicks
    gpScene([G(80, 0, 0), G(81, 1, 0)], 90, 0x200,
            [[slotPt(0)], [slotPt(0), press2(46, 80)], [slotPt(1)], [slotPt(1), press2(400, 80)], [OUT],
             [OUT, keyEsc()]]),
    // 7: the start wraps (2, 1, 0, 151, 150): slots 1 and 150; the cursor above the list (y < 0x32) and below the box
    //    (y >= bound + 0x1ca); a key with nothing selected returns 0
    gpScene([G(1, 5, 0x40), G(150, 6, 0x10)], 2, 0x200,
            [[[100, 10]], [[100, 0x3d0]], [slotPt(0)], [OUT], [OUT, keyEsc()]]),
    // 8: a small bound [0x00821164] = 0: the right column is outside the box, so its click is a click outside
    gpScene([G(90, 2, 0), G(91, 3, 0)], 95, 0, [[slotPt(1)], [slotPt(1), press(400, 80)]]),
    // 9: twelve golfers (six rows), select slots 9 and 11 (golfers 102 and 100, types 2 and 0: two shared trait
    //    bits, rating 1), key
    gpScene([G(100, 0), G(101, 1), G(102, 2), G(103, 3), G(104, 4), G(105, 5), G(106, 6), G(107, 7), G(108, 0x0),
             G(109, 7), G(110, 2), G(111, 5)], 111, 0x200,
            [[slotPt(9)], [slotPt(9), press(400, 0x32 + 0x88 * 4 + 30)], [slotPt(11)],
             [slotPt(11), press(400, 0x32 + 0x88 * 5 + 30)], [OUT], [OUT, keyEsc()]]),
    // 10: no golfer at all: a click on slot 0 is refused (0 drawn), then a key
    gpScene([], 0, 0x200, [[slotPt(0)], [slotPt(0), press(46, 80)], [OUT], [OUT, keyEsc()]]),
    // 11: the kinds buildGolferName branches on (0x80, 0x81, 0x60); select slot 0, click it again (deselected), a
    //     click outside with the mask back at 0
    gpScene([G(120, 4, 0x08, 0x80), G(121, 5, 0x10, 0x81), G(122, 6, 0, 0x60)], 125, 0x200,
            [[slotPt(0)], [slotPt(0), press(46, 80)], [slotPt(0), press(46, 80)], [OUT], [OUT, press(1, 1)]]),
  ];

  let gpCur = 0;
  function gpView(n) {
    const v = GP_SCENES[gpCur].view;
    return v[Math.min(n, v.length - 1)];
  }
  function gpApply(sc) {
    GP_SAVE.forEach(function (s) { __freeze.save(ptr(s[0]), s[1]); });
    gpTypes();
    gpGolfers(sc.golfers);
    wr32(0x005a5a24, sc.start);
    wr32(0x00821164, sc.bound);
  }
  // argc counts `this` for the thiscall stand-ins (Frida passes ecx as the first argument)
  function gpStands(fx) {
    calleeStand(fx, 0, 0x00474440, 7, 0, { abi: 'thiscall' });                 // Surface_drawSprite (ret 0x18)
    calleeStand(fx, 1, 0x00473e60, 5, 0, { abi: 'thiscall' });                 // Image_draw (ret 0x10)
    calleeStand(fx, 2, 0x004762d0, 5, 0, { abi: 'thiscall' });                 // setDrawTarget (ret 0x10)
    calleeStand(fx, 3, 0x00404b70, 4, 0, { str: { 0: 8 } });                   // drawCenteredText
    calleeStand(fx, 4, 0x004049d0, 4, 0, { str: { 0: 8 } });                   // drawText4049d0
    calleeStand(fx, 5, 0x00473cb0, 8, 0, { abi: 'thiscall' });                 // Surface_drawScaled (ret 0x1c)
    calleeStand(fx, 6, 0x00474380, 10, 0, { abi: 'thiscall' });                // Surface_draw (ret 0x24)
    calleeStand(fx, 7, 0x00480c80, 2, 0, { abi: 'thiscall' });                 // Window::refreshFull (ret 4)
    calleeStand(fx, 8, 0x00483d30, 0, 0, {});                                  // enterModal483d30
    // View_toLocal(this, &x, &y): writes the scene's cursor for loop iteration n; the out-pointers are frame
    // addresses (masked), the written pair is logged instead
    calleeStand(fx, 9, 0x0047ab50, 3, function (n, a) {
      const p = gpView(n);
      ptr(a[1]).writeS32(p[0]); ptr(a[2]).writeS32(p[1]);
      return 0;
    }, { abi: 'thiscall', mask: [1, 2], extra: function (a, n) { return gpView(n); } });
    calleeStand(fx, 10, 0x004481b0, 5, 0x5a, {});                              // playSound
    calleeStand(fx, 11, 0x0045de80, 2, 0x77, {});                              // pairGolfersWithStory
  }
  F.c3et_pairsel = function () {
    const fx = {};
    fx.clog = newClog();
    const M = __modal({ maxCalls: 400, maxFrames: 400 });
    __importStandIn({ module: 'golf_clean.exe', imports: M.imports() });
    gpStands(fx);              // freezes the first thread
    M.arm();
    GP_SCENES.forEach(function (sc, i) {
      Object.defineProperty(fx, 'g' + i, { enumerable: true, configurable: true, get: function () {
        gpCur = i;
        gpApply(sc);
        M.select(sc.script);
        return 0;
      } });
    });
    negGetters(fx, NEG_GP, 'g');
    return Object.assign(fx, M.fx());
  };

  // ====================================================================================================================
  // LabelButton::drawContents 0x004a1580 (thiscall, no stack arguments; one vector = one scene, applied by fx.bN)
  // ====================================================================================================================
  // The object is fixture memory (0x700 bytes) with only the fields the function reads; every callee is a recording
  // stand-in (the function draws nothing itself), the jgld surface at +0x274 + 4 and the owner at +0x130 are recording
  // objects, and the display [0x0083ad50] is a recording display under recdisplay.js's guard (other threads reach the
  // real one). Rect and Image pointers into the caller's frame are masked and the rect's four words are logged instead.
  const LB = { s: null };
  const LB_OBJ = zalloc(0x700);
  const LB_SURF_VT = vtable({
    0xc0: [0, function () { rec(0x5c0, []); return LB.s.bg; }],
    0xc4: [0, function () { rec(0x5c4, []); return 0; }],
    0xd0: [1, function (s, a) {
      rec(0x5d0, []);
      const p = ptr(a[0]);
      LB.s.rect.forEach(function (v, i) { p.add(4 * i).writeS32(v); });
      return 0;
    }],
    0xd8: [0, function () { rec(0x5d8, []); return LB.s.w; }],
    0xdc: [0, function () { rec(0x5dc, []); return LB.s.h; }],
  });
  const LB_SURF = zalloc(0x20);
  LB_SURF.writePointer(LB_SURF_VT);
  const LB_OWNER_VT = vtable({ 0x14: [3, function (s, a) {
    rec(0x614, [a[0], a[2]].concat(runWords(ptr(a[1]), 8, 8)));
    return LB.s.ownerRet;
  }] });
  const LB_OWNER = zalloc(0x100);
  LB_OWNER.writePointer(LB_OWNER_VT);
  const LB_DISP_VT = vtable({ 0x9c: [3, function (s, a) { rec(0xd9c, [a[0], a[1], a[2]]); return 0; }] }, 0x400);
  const LB_DISP = zalloc(0x40);
  LB_DISP.writePointer(LB_DISP_VT);
  // images: +0x18 width, +0x1c height; LB_IMG[k] has width 8 + k, height 20 - k
  const LB_IMG = [];
  for (let k = 0; k < 16; k++) { const p = zalloc(0x40); p.add(0x18).writeS32(8 + k); p.add(0x1c).writeS32(20 - k); LB_IMG.push(p); }
  const LB_CAP = zalloc(0x40), LB_CAP2 = zalloc(0x40);
  wrStr(LB_CAP.toUInt32(), 'CAPTION', 0x40);
  wrStr(LB_CAP2.toUInt32(), 'A', 0x40);

  function rectWords(p) { const q = ptr(p); return [0, 4, 8, 12].map(function (o) { return q.add(o).readU32(); }); }

  // scene: {st (+0x634), pressed (+0x614), f578, f5e4, single, masked, comp, nine: {tl,tr,bl,br,l,r,t,b,c} (image
  // indices), bevel: {focus (+0x60c), c600, c604, count}, ownerFlags, focusChild, cap, capColour, ownerRet, lines,
  // value, get, f290, rect, w, h, bg, noSurf, noOwner}
  function lbScene(o) {
    const s = { st: 0, pressed: 0, f578: 0, f5e4: 0, single: -1, masked: false, comp: 0, nine: null, bevel: null,
                ownerFlags: 0, focusChild: 0, cap: 1, capColour: 3, ownerRet: 0, lines: 10, value: 2, get: 1,
                f290: 0, rect: [10, 20, 110, 60], w: 100, h: 40, bg: 0x44, noSurf: false, noOwner: false,
                dx: 2, dy: 3 };
    Object.keys(o).forEach(function (k) { s[k] = o[k]; });
    return s;
  }
  const NINE_ALL = { tl: 0, tr: 1, bl: 2, br: 3, l: 4, r: 5, t: 6, b: 7, c: 8 };
  const LB_SCENES = [
    lbScene({ single: 9 }),                                                                  // 0 plain image, fits
    lbScene({ single: 10, masked: true, cap: 0 }),                                           // 1 masked image, no caption
    lbScene({ st: 5, pressed: 1, comp: 0x1234, f290: 2 }),                                   // 2 composite, pressed
    lbScene({ st: -1, f5e4: 1, nine: NINE_ALL, ownerRet: 7 }),                               // 3 nine pieces, owner draws
    lbScene({ st: 1, nine: { tl: 0, l: 4, c: 8, b: 7 } }),                                   // 4 corner + edges missing
    lbScene({ st: 2, nine: { tl: 0, tr: 1, br: 3, t: 6, r: 5, l: 4, b: 7, c: 8 }, capColour: -1 }),   // 5 no bottom-left
    lbScene({ bevel: { focus: -1, c600: 0x11, c604: 0x22, count: 3 }, lines: 500 }),         // 6 bevels, top < 0
    lbScene({ st: 9, pressed: 1, bevel: { focus: 0x33, c600: 0x11, c604: 0x22, count: 2 }, focusChild: 1,
              cap: 2, rect: [10, 20, 30, 60] }),                                             // 7 two rings, too wide
    lbScene({ bevel: { focus: 0x33, c600: 1, c604: 2, count: 0 }, ownerFlags: 0x1000, capColour: -5 }),   // 8 bit 12
    lbScene({ noOwner: true, single: 9 }),                                                   // 9 no owner
    // 10: no surface. Without one slot +0xd0 never fills the frame's rect (0x004a15ed), so the caption code would read
    //     uninitialised stack: these scenes have no caption, and the arms they reach never read the rect
    lbScene({ noSurf: true, single: 11, cap: 0 }),
    lbScene({ bevel: { focus: 0x44, c600: 5, c604: 6, count: 1 }, focusChild: 0, f290: 3, cap: 2,
              rect: [0, 0, 12, 40] }),                                                       // 11 one ring, no fit test
    lbScene({ st: 3, f578: 1, single: 12, rect: [-50, -30, 70, 90], w: 33, h: 77 }),         // 12 +0x578: state 2
    lbScene({ st: 3, single: 13, w: 7, h: 9, bg: 0xff }),                                    // 13 state 0 by +0x5e4
    lbScene({ st: 0, nine: { tl: 0, tr: 1, bl: 2, br: 3, l: 4, r: 5, t: 6, b: 7 } }),        // 14 eight, no centre
    lbScene({ noSurf: true, nine: NINE_ALL, cap: 0 }),                                       // 15 no surface, nine
    lbScene({ noSurf: true, single: 10, masked: true, cap: 0 }),                             // 16 no surface, masked
  ];

  function lbApply(s) {
    const o = LB_OBJ;
    for (let i = 0; i < 0x700; i += 4) o.add(i).writeU32(0);
    o.add(0x130).writePointer(s.noOwner ? ptr(0) : LB_OWNER);
    o.add(0x1ac).writeS32(5);
    o.add(0x1b0).writeS32(6);
    o.add(0x274 + 4).writePointer(s.noSurf ? ptr(0) : LB_SURF);
    o.add(0x290).writeS32(s.f290);
    o.add(0x578).writeS32(s.f578);
    o.add(0x5e4).writeS32(s.f5e4);
    o.add(0x5e8).writeS32(0x5e8);
    o.add(0x5ec).writeS32(s.dx);
    o.add(0x5f0).writeS32(s.dy);
    o.add(0x5f4).writePointer(s.cap === 0 ? ptr(0) : (s.cap === 1 ? LB_CAP : LB_CAP2));
    o.add(0x614).writeS32(s.pressed);
    o.add(0x634).writeS32(s.st);
    const b = s.bevel || { focus: -1, c600: 0, c604: 0, count: 0 };
    o.add(0x600).writeU32(b.c600);
    o.add(0x604).writeU32(b.c604);
    o.add(0x608).writeS32(b.count);
    o.add(0x60c).writeS32(b.focus);
    // the per-state tables: every state gets the same pieces, so the state only selects the colour and the offsets
    for (let k = 0; k < 3; k++) {
      o.add(0x638 + 4 * k).writeS32(s.capColour + k);
      if (s.comp) o.add(0x644 + 4 * k).writeU32(s.comp + k);
      if (s.single >= 0) o.add(0x650 + 4 * k).writePointer(LB_IMG[s.single]);
      if (s.nine) {
        const n = s.nine;
        const put = function (off, ix) { if (ix !== undefined) o.add(off + 4 * k).writePointer(LB_IMG[ix]); };
        put(0x694, n.tl); put(0x6a0, n.tr); put(0x6ac, n.bl); put(0x6b8, n.br);
        put(0x664, n.l); put(0x670, n.r); put(0x67c, n.t); put(0x688, n.b); put(0x6c4, n.c);
      }
    }
    if (s.masked) { o.add(0x65c).writePointer(LB_IMG[14]); o.add(0x660).writePointer(LB_IMG[15]); }
    LB_OWNER.add(0x9c).writeU32(s.ownerFlags);
  }

  function lbStands(fx) {
    const rx = function (ix) { return function (a) { return rectWords(a[ix]); }; };
    calleeStand(fx, 0, 0x004789f0, 2, 0, { abi: 'thiscall' });                            // Widget_applyPalette
    calleeStand(fx, 1, 0x004808c0, 2, 0, { abi: 'thiscall' });                            // Window::paint
    calleeStand(fx, 2, 0x00474ae0, 1, function (n, a) { return a[0]; }, { abi: 'thiscall', mask: [0] });   // Image_ctor
    calleeStand(fx, 3, 0x00474dd0, 7, 0, { abi: 'thiscall', mask: [0] });                 // Surface_createFull
    calleeStand(fx, 4, 0x00475d00, 10, 0, { abi: 'thiscall', mask: [1] });                // Surface_blit3
    calleeStand(fx, 5, 0x00473fc0, 7, 0, { abi: 'thiscall', mask: [2] });                 // Surface_blitMasked
    calleeStand(fx, 6, 0x00474c40, 1, function (n, a) { return a[0]; }, { abi: 'thiscall', mask: [0] });   // Image_dtor
    calleeStand(fx, 7, 0x00473e60, 5, 0, { abi: 'thiscall' });                            // Image_draw
    calleeStand(fx, 8, 0x004791b0, 3, 0, { abi: 'thiscall', mask: [1], extra: rx(1) });   // drawComposite
    calleeStand(fx, 9, 0x00476140, 8, 0, { abi: 'thiscall' });                            // blitRight
    calleeStand(fx, 10, 0x004795d0, 4, 0, { abi: 'thiscall', mask: [1], extra: rx(1) });  // Widget_drawBevelBoxR
    calleeStand(fx, 11, 0x0047d7d0, 1, function () { return LB.s.focusChild; }, { abi: 'thiscall' });   // isFocusChild
    calleeStand(fx, 12, 0x00488930, 5, 0x1234, { abi: 'thiscall' });                     // Button::setColorNormal
    calleeStand(fx, 13, 0x00476e20, 5, function () { return LB.s.lines; }, { abi: 'thiscall', str: { 1: 4 } });   // wrapText
    calleeStand(fx, 14, 0x00477580, 1, function () { return LB.s.value; }, { abi: 'thiscall' });   // Widget_value
    calleeStand(fx, 15, 0x00477560, 1, function () { return LB.s.get; }, { abi: 'thiscall' });     // Widget_get
    calleeStand(fx, 16, 0x00477250, 2, function (n, a) {                                  // Text_put: 7 * length + 3
      let len = 0; const p = ptr(a[1]);
      while (len < 0x40 && p.add(len).readU8() !== 0) len++;
      return 7 * len + 3;
    }, { abi: 'thiscall', str: { 1: 4 } });
    calleeStand(fx, 17, 0x00478530, 5, 0x3333, { abi: 'thiscall', str: { 1: 4 } });      // measureText3
    calleeStand(fx, 18, 0x00475b00, 2, 0, { abi: 'thiscall', mask: [1], extra: rx(1) });  // Widget_fillRectR
    calleeStand(fx, 19, 0x00477cd0, 4, 0x4444, { abi: 'thiscall', str: { 1: 4 }, mask: [2], extra: rx(2) });   // drawCentered
  }

  F.c3et_labelbtn = function () {
    const fx = { clog: newClog(), tlog: newLog() };
    lbStands(fx);                         // freezes the first thread
    __freeze.save(ptr(0x0083ad50), 4);
    __freeze.save(ptr(0x0083ab2c), 4);
    ptr(0x0083ad50).writePointer(__recDisplay.guard(LB_DISP));
    LB_SCENES.forEach(function (s, i) {
      Object.defineProperty(fx, 'b' + i, { enumerable: true, configurable: true, get: function () {
        LB.s = s;
        lbApply(s);
        return LB_OBJ;
      } });
    });
    negGetters(fx, NEG_LB, 'b');
    return fx;
  };

  // ====================================================================================================================
  // TextView::build 0x0048e900 (thiscall: this, quiet, modalArg; one vector = one scene applied by fx.vN)
  // ====================================================================================================================
  // The TextView is a fixture block of 0x2600 bytes (compared state, $tv) holding only the fields the function reads,
  // with its own recording vtable; the vbtable at +0x1464 puts the window part at +0x2464 (+8 = 0x1000) and the panel
  // part at +0x2564 (+4 = 0x1100). Its two button parts (+0x5dc, +0xcac), the panel part, the parent window (+0x590),
  // the owner (+0x5bc when not itself), the surfaces (+0x278) and the display (guarded) are recording objects. Every
  // callee is a recording stand-in; operator new / malloc / free go through the heap stand-in (arena compared), and
  // ctorObj4a13f0's stand-in gives each new list button the recording button vtable.
  const TVX = { s: null, n190: 0 };
  const TV_SIZE = 0x2600;
  const TV = zalloc(TV_SIZE);
  function tvOff(p) { const d = ptr(p).sub(TV).toInt32(); return (d >= 0 && d < TV_SIZE) ? (0x7e000000 | d) >>> 0 : ptr(p).toUInt32(); }
  const TV_VT = vtable({
    0x04: [1, function (s, a) { rec(0x7004, [tvOff(s), a[0]]); return 0x41; }],
    0x08: [0, function (s) { rec(0x7008, [tvOff(s)]); return 0x42; }],
    0x58: [0, function (s) { rec(0x7058, [tvOff(s)]); return 0x43; }],
    0x10c: [3, function (s, a) { rec(0x710c, [tvOff(s), a[0], a[1], a[2]]); return TVX.s.modalRet; }],
    0x16c: [0, function (s) { rec(0x716c, [tvOff(s)]); return 0x44; }],
    0x17c: [0, function (s) { rec(0x717c, [tvOff(s)]); return TVX.s.r17c; }],
    0x184: [0, function (s) { rec(0x7184, [tvOff(s)]); return 0x45; }],
    0x188: [0, function (s) { rec(0x7188, [tvOff(s)]); return 0x46; }],
    // slot +0x190 (&x, &y, &z): logs the three values and moves them by the scene's deltas
    0x190: [3, function (s, a) {
      const d = TVX.s.d190;
      const v = [0, 1, 2].map(function (k) { return ptr(a[k]).readS32(); });
      rec(0x7190, [tvOff(s)].concat(v));
      [0, 1, 2].forEach(function (k) { ptr(a[k]).writeS32(v[k] + d[k]); });
      return 0x47;
    }],
    0x194: [0, function (s) { rec(0x7194, [tvOff(s)]); return TVX.s.r194; }],
  }, 0x200);
  const TV_BTN_VT = vtable({
    0x04: [1, function (s, a) { rec(0x8004, [tvOff(s), a[0]]); return 0x51; }],
    0x120: [0, function (s) { rec(0x8120, [tvOff(s)]); return 0x52; }],
  }, 0x200);
  const TV_PANEL_VT = vtable({ 0x04: [1, function (s, a) { rec(0x9004, [tvOff(s), a[0]]); return 0x61; }] });
  const TV_PARENT_VT = vtable({ 0x10c: [3, function (s, a) { rec(0x910c, [a[0], a[1], a[2]]); return TVX.s.modalRet; }] }, 0x200);
  const TV_PARENT = zalloc(0x40); TV_PARENT.writePointer(TV_PARENT_VT);
  const TV_SURF_VT = vtable({
    0xd8: [0, function (s) { rec(0xa0d8, [tvOff(s)]); return TVX.s.sw; }],
    0xdc: [0, function (s) { rec(0xa0dc, [tvOff(s)]); return TVX.s.sh; }],
  });
  const TV_SURF = zalloc(0x20); TV_SURF.writePointer(TV_SURF_VT);
  const TV_OWNER = zalloc(0x300);
  const TV_DISP_VT = vtable({
    0xa8: [0, function () { rec(0xd0a8, []); return TVX.s.dw; }],
    0xac: [0, function () { rec(0xd0ac, []); return TVX.s.dh; }],
    0xb4: [0, function () { rec(0xd0b4, []); return TVX.s.fmt; }],
  }, 0x400);
  const TV_DISP = zalloc(0x40); TV_DISP.writePointer(TV_DISP_VT);
  const TV_VBT = zalloc(0x10); TV_VBT.add(4).writeU32(0x1100); TV_VBT.add(8).writeU32(0x1000);
  const TV_CB = keep(new NativeCallback(function (p, flag) {
    rec(0xcb1f, [tvOff(p), flag]);
  }, 'void', ['pointer', 'int'], 'mscdecl'));
  // images (+0x18 width, +0x1c height)
  const TV_IMG = [];
  for (let k = 0; k < 8; k++) { const p = zalloc(0x40); p.add(0x18).writeS32(11 + 3 * k); p.add(0x1c).writeS32(7 + 2 * k); TV_IMG.push(p); }
  function tvStr(s) { const p = zalloc(s.length + 4); wrStr(p.toUInt32(), s, s.length + 1); return p; }
  const TV_S994 = tvStr('OKAY'), TV_S998 = tvStr('CANCEL'), TV_TIP1 = tvStr('TIP1'), TV_TIP2 = tvStr('TIP2');
  // list entries: a circular list of nodes {+8 item, +0xc next, +0x10 prev}, items {+4 text}
  const TV_NODES = [0, 1, 2, 3].map(function () { return zalloc(0x20); });
  const TV_ITEMS = [0, 1, 2, 3].map(function (k) { const it = zalloc(0x10); it.add(4).writePointer(tvStr('ROW' + k)); return it; });
  function tvList(n) {
    for (let k = 0; k < n; k++) {
      const node = TV_NODES[k];
      node.add(8).writePointer(TV_ITEMS[k]);
      node.add(0xc).writePointer(TV_NODES[(k + 1) % n]);
      node.add(0x10).writePointer(TV_NODES[(k + n - 1) % n]);
    }
    return n ? TV_NODES[0] : ptr(0);
  }
  const TV_GLOBALS_W = [0x0083b8f0, 0x0083b990, 0x0083b9e4, 0x0083b9e8, 0x0083b8ec];
  const TV_GLOBALS_R = [0x0083abe4, 0x0083b8d8, 0x0083b8dc, 0x0083b994, 0x0083b998, 0x0083b99c, 0x0083b9a0, 0x004e44e0];

  function tvScene(o) {
    const s = {
      r194: 1, r17c: 0, quiet: 0, modalArg: 0x55, modalRet: 0, create: 0, visible: 1, parent: false,
      f: 0x40, f1f64: 0x100, own: 'self', ownA0: 2, fb8: 0x1000, fbc: 0x1000, f1654: 2, f1534: 1, f1578: 0,
      list: 2, head: true, imgs2078: false, imgs2084: false, img13cc: false, f184: 30, s994: true, s998: true,
      tip1: false, tip2: false, cb: false, f1fa0: 6, f145c: 40, f1f94: 0, d190: [0, 0, 0], g8f0: 1,
      sw: 300, sh: 200, dw: 800, dh: 600, fmt: 1, f2108: false, f210c: false, g83abe4: 0, f137c: 2, f1f70: 3,
      f590none: false,
    };
    Object.keys(o).forEach(function (k) { s[k] = o[k]; });
    return s;
  }
  const TV_SCENES = [
    tvScene({ modalRet: 1 }),                                                                     // 0
    tvScene({ r194: 0 }),                                                                         // 1 not built
    tvScene({ r17c: 5 }),                                                                         // 2 slot +0x17c
    tvScene({ quiet: 1, parent: true, visible: 1 }),                                              // 3 quiet
    tvScene({ f: 0x8040 | 0x100000, parent: true, visible: 0 }),                                  // 4 bit 15
    tvScene({ create: 7, img13cc: true, f184: 0, g83abe4: 1, f: 0x40 | 0x20 | 0x4100 | 0x20000 }),   // 5 create fails
    tvScene({ own: 'other', fb8: -20, fbc: -30, f: 0x10040, f1534: 0, s998: false }),            // 6 other owner
    tvScene({ ownA0: 0, fb8: 0x1000, fbc: -40, f1578: 2, f: 0x800040 | 0x10000, f1654: 3 }),     // 7 bit clear
    tvScene({ ownA0: 0, fb8: -15, fbc: 77, imgs2078: true, imgs2084: true, f2108: true, f210c: true,
              f: 0x1000040 }),                                                                    // 8 images, frees
    tvScene({ f: 0x80140, cb: true, parent: true, visible: 1, modalRet: 3 }),                     // 9 no buttons, cb
    tvScene({ fb8: -15, fbc: -25, f1f94: 1, list: 3, s994: false, s998: false, tip1: true, tip2: true,
              f1654: 4 }),                                                                        // 10 defaults
    tvScene({ f: 0x4140, cb: true }),                                                             // 11 no modal
    tvScene({ f1fa0: -9, d190: [-500, 4, 2], list: 1 }),                                          // 12 negative x
    // 13: a negative count: the seek (0x0048f51f) and the loop (0x0048f55f) are both skipped. (A NULL head with a
    //     positive count faults in the original at 0x0048f544, reading [0 + 0x10].)
    tvScene({ list: -1, head: false }),
    tvScene({ modalRet: 9, f: 0x40, g8f0: 1, list: 0, fb8: 12, fbc: 0x1000, f5c8: 0 }),           // 14 empty list, -1
    tvScene({ ownA0: 0, fb8: 25, fbc: 0x1000, own: 'other', ownSurf: false, list: 4 }),           // 15 owner no surf
    tvScene({ ownA0: 2, fb8: -5, fbc: 9, fmt: 0, cb: true, f: 0x400040, f145c: -1 }),             // 16 cb off, fmt 0
    tvScene({ f: 0, modalRet: 5, g8f0: 2, ownA0: 0, fb8: 33, fbc: 0x1000 }),                      // 17 modal 5, no 0x40
    tvScene({ own: 'other', ownSurf: false, fb8: 0x1000, fbc: 44 }),                              // 18 other, no surface
    tvScene({ own: 'other', ownSurf: false, fb8: -6, fbc: -7 }),                                  // 19 other, offsets
    tvScene({ parent: true, visible: 1, modalRet: 2, f: 0x240, cb: true }),                       // 20 parent runs it
    tvScene({ cb: true, f: 0x200040, g8f0: 0 }),                                                  // 21 both callbacks
    tvScene({ selfSurf: false, ownA0: 0, fb8: 0x1000, fbc: -10, imgs2078: true, imgs2084: true }),   // 22 no surface
    tvScene({ selfSurf: false, ownA0: 0, fb8: -12, fbc: 5, imgs2078: 'partial', imgs2084: 'partial' }),   // 23 partial
    tvScene({ f: 0x1000040, img13cc: true, f194: 40 }),                                           // 24 flag, no strings
    tvScene({ f2108: true, f210c: true, parent: true, visible: 1, f1578: 2 }),                    // 25 strings kept
    tvScene({ imgs2078: true, f: 0x10040, list: 1, brokenNext: true }),                           // 26 0x10000, node end
  ];

  function tvApply(s, h) {
    const o = TV;
    for (let i = 0; i < TV_SIZE; i += 4) o.add(i).writeU32(0);
    o.writePointer(TV_VT);
    o.add(0x1464).writePointer(TV_VBT);
    o.add(0x2564).writePointer(TV_PANEL_VT);
    o.add(0x5dc).writePointer(TV_BTN_VT);
    o.add(0xcac).writePointer(TV_BTN_VT);
    o.add(0x278).writePointer(s.selfSurf === false ? ptr(0) : TV_SURF);
    o.add(0x180).writeS32(50); o.add(0x184).writeS32(s.f184); o.add(0x188).writeS32(60);
    o.add(0x194).writeS32(s.f194 === undefined ? 4 : s.f194);
    o.add(0x1bc).writeS32(640); o.add(0x1c0).writeS32(480);
    o.add(0x590).writePointer(s.parent ? TV_PARENT : ptr(0));
    o.add(0x594).writeU32(0x594); o.add(0x598).writeU32(0x598); o.add(0x59c).writeU32(0x59c); o.add(0x5a0).writeU32(0x5a0);
    o.add(0x5c0).writeU32(0x5c0);
    o.add(0x5c8).writeU32(s.f5c8 === undefined ? 0x5c8 : s.f5c8);
    o.add(0x2110).writeU32(0x2110);
    if (s.img13cc) o.add(0x13cc).writePointer(TV_IMG[7]);
    o.add(0x137c).writeS32(s.f137c);
    o.add(0x145c).writeS32(s.f145c);
    o.add(0x13e8).writePointer(s.head ? tvList(s.list) : ptr(0));
    if (s.brokenNext && s.list > 0) TV_NODES[s.list - 1].add(0xc).writeU32(0);   // the last node has no successor
    o.add(0x13f0).writeS32(s.list);
    o.add(0x13f4).writeS32(7);
    o.add(0x2534).writeU32(s.f1534);       // the window part's +0xd0 (vbtable +8 = 0x1000)
    o.add(0x2488).writeU32(0x22);          // the window part's +0x24 (|= 0x1000 at 0x0048ef8a)
    o.add(0x1578).writeS32(s.f1578);
    o.add(0x1654).writeS32(s.f1654);
    o.add(0x1f00).writePointer(s.cb ? TV_CB : ptr(0));
    o.add(0x1f64).writeU32(s.f1f64);
    o.add(0x1f68).writeU32(s.f);
    o.add(0x1f70).writeS32(s.f1f70);
    o.add(0x1f7c).writeS32(120); o.add(0x1f80).writeS32(90);
    o.add(0x1f8c).writeS32(40); o.add(0x1f90).writeS32(16); o.add(0x1f94).writeS32(s.f1f94);
    o.add(0x1fa0).writeS32(s.f1fa0); o.add(0x1fa4).writeS32(5); o.add(0x1fac).writeS32(3);
    o.add(0x1fb8).writeS32(s.fb8); o.add(0x1fbc).writeS32(s.fbc);
    for (let k = 0; k < 0x30; k += 4) o.add(0x1fd0 + k).writeU32(0x1fd0 + k);    // +0x1fd0..+0x1ffc
    for (let k = 0; k < 0x30; k += 4) o.add(0x2000 + k).writeU32(0x2000 + k);    // +0x2000..+0x202c
    for (let k = 0x2030; k < 0x2078; k += 4) o.add(k).writeU32(k * 3);          // style and colours
    o.add(0x2038).writeU32(0x0c0b0a09);
    // image sets: true = all three, 'partial' = only the first (the others NULL)
    if (s.imgs2078) {
      o.add(0x2078).writePointer(TV_IMG[0]);
      if (s.imgs2078 !== 'partial') { o.add(0x207c).writePointer(TV_IMG[1]); o.add(0x2080).writePointer(TV_IMG[2]); }
    }
    if (s.imgs2084) {
      o.add(0x2084).writePointer(TV_IMG[3]);
      if (s.imgs2084 !== 'partial') { o.add(0x2088).writePointer(TV_IMG[4]); o.add(0x208c).writePointer(TV_IMG[5]); }
    }
    for (let k = 0x2090; k < 0x2108; k += 4) o.add(k).writeU32(0x55000000 + k);  // the skin triplets
    // existing caption strings (in the arena so a free is recorded there)
    if (s.f2108) o.add(0x2108).writePointer(h.arena.add(0x10 + 0x3f00));
    if (s.f210c) o.add(0x210c).writePointer(h.arena.add(0x10 + 0x3f80));
    // the owner
    if (s.own === 'self') {
      o.add(0x5bc).writePointer(o);
      o.add(0xa0).writeU8(s.ownA0);
      o.add(0x1c4).writeS32(8); o.add(0x1c8).writeS32(9);
    } else {
      o.add(0x5bc).writePointer(TV_OWNER);
      TV_OWNER.add(0xa0).writeU8(s.ownA0);
      TV_OWNER.add(0x278).writePointer(s.ownSurf === false ? ptr(0) : TV_SURF);
    }
    // globals
    wr32(0x0083abe4, s.g83abe4);
    wr32(0x0083b8d8, 33); wr32(0x0083b8dc, 44);
    ptr(0x0083b994).writePointer(s.s994 ? TV_S994 : ptr(0));
    ptr(0x0083b998).writePointer(s.s998 ? TV_S998 : ptr(0));
    ptr(0x0083b99c).writePointer(s.tip1 ? TV_TIP1 : ptr(0));
    ptr(0x0083b9a0).writePointer(s.tip2 ? TV_TIP2 : ptr(0));
    wr32(0x004e44e0, 0x4e44e0);
    wr32(0x0083b8f0, s.g8f0); wr32(0x0083b990, 0x990); wr32(0x0083b9e4, 0); wr32(0x0083b9e8, 0); wr32(0x0083b8ec, 0xec);
  }

  function tvStands(fx) {
    calleeStand(fx, 0, 0x004801f0, 1, function () { return TVX.s.visible; }, { abi: 'thiscall' });   // Window::visible
    calleeStand(fx, 1, 0x0048e120, 2, 0, { abi: 'thiscall' });                       // TextView::setFontSize
    calleeStand(fx, 2, 0x0048e0b0, 2, 0, { abi: 'thiscall' });                       // TextView::setLineSpacing
    calleeStand(fx, 3, 0x0048e190, 3, 0, { abi: 'thiscall' });                       // TextView::setSize
    calleeStand(fx, 4, 0x00491490, 2, 0, { abi: 'thiscall' });                       // Widget::setFlagBit0
    calleeStand(fx, 5, 0x004914b0, 2, 0, { abi: 'thiscall' });                       // Widget::setFlagBit1
    calleeStand(fx, 6, 0x004806c0, 10, function () { return TVX.s.create; },        // Window::create (rect in frame)
                { abi: 'thiscall', mask: [9], extra: function (a) { const p = ptr(a[9]); return [p.readU32(), p.add(4).readU32(), p.add(8).readU32()]; } });
    calleeStand(fx, 7, 0x00476310, 5, 0, { abi: 'thiscall' });                       // Widget_setQuad
    calleeStand(fx, 8, 0x00476340, 5, 0, { abi: 'thiscall' });                       // Widget_setQuad70
    calleeStand(fx, 9, 0x00476370, 5, 0, { abi: 'thiscall' });                       // Widget_setQuad74
    calleeStand(fx, 10, 0x004763a0, 5, 0, { abi: 'thiscall' });                      // Widget_setQuad78
    calleeStand(fx, 11, 0x004762d0, 5, 0, { abi: 'thiscall' });                      // setDrawTarget
    calleeStand(fx, 12, 0x0047b8f0, 2, 0, { abi: 'thiscall' });                      // Label::setText
    calleeStand(fx, 13, 0x004a08a0, 6, 0, { abi: 'thiscall' });                      // panelDispatch
    // Button::create: the text argument can be NULL (a list entry without a node), so its words are read here
    calleeStand(fx, 14, 0x004887c0, 9, 0, { abi: 'thiscall', extra: function (a) {
      if (a[1] === 0) return [0];
      return runWords(ptr(a[1]), 8, 8);
    } });
    calleeStand(fx, 15, 0x00488930, 5, 0, { abi: 'thiscall' });                      // Button::setColorNormal
    calleeStand(fx, 16, 0x00488970, 5, 0, { abi: 'thiscall' });                      // Button::setColorHover
    calleeStand(fx, 17, 0x004889b0, 5, 0, { abi: 'thiscall' });                      // Button::setColorPressed
    calleeStand(fx, 18, 0x00488a20, 2, 0, { abi: 'thiscall', str: { 1: 2 } });      // Button::setTooltip
    calleeStand(fx, 19, 0x004a13f0, 1, function (n, a) {                             // ctorObj4a13f0
      ptr(a[0]).writePointer(TV_BTN_VT);
      return a[0];
    }, { abi: 'thiscall' });
    calleeStand(fx, 20, 0x004a2980, 2, function (n, a) { return 0x500 + a[1]; }, { abi: 'thiscall' });   // 0x004a2980
    calleeStand(fx, 21, 0x00494cb0, 4, 0, {});                                       // setHudTextSlot
  }

  F.c3et_tvbuild = function () {
    // the heap stand-in BEFORE the freeze; the arena (and its call counter / fail mask) and the free log are compared
    const h = __heapStandIn({ module: 'golf_clean.exe', alloc: [0xa504f, 0xa57f1], free: [0xa5007, 0xa4ffc],
                              arenaSize: 0x4000 });
    const fx = { clog: newClog(), tlog: newLog(), tv: TV, heap: h.arena, freelog: h.log };
    tvStands(fx);                         // freezes the first thread
    __freeze.save(ptr(0x0083ad50), 4);
    ptr(0x0083ad50).writePointer(__recDisplay.guard(TV_DISP));
    TV_GLOBALS_W.concat(TV_GLOBALS_R).forEach(function (g) { __freeze.save(ptr(g), 4); });
    TV_SCENES.forEach(function (s, i) {
      Object.defineProperty(fx, 'v' + i, { enumerable: true, configurable: true, get: function () {
        TVX.s = s;
        tvApply(s, h);
        return TV;
      } });
      fx['q' + i] = s.quiet;
      fx['m' + i] = s.modalArg;
    });
    negGetters(fx, NEG_TV, 'v');
    return fx;
  };

  // ====================================================================================================================
  // showRoutingMapScreen 0x00456be0 (__cdecl, no arguments; one vector = one whole run, scene applied by fx.rN)
  // ====================================================================================================================
  // The world the function reads is written by the scene getter (every region saved first and restored at the flush):
  // the tile grid 0x005722e8 (a 12 x 12 block of types 0..19 at x 10..21, y 20..31, every other tile 0x14), the two
  // per-type bytes it tests (0x0058dd34 + t and 0x00578376 + 0x30 t), the shade grids 0x00822da0 / 0x0082376c, the 18
  // hole records 0x00575ab0 + 0x208 h and the waypoint records 0x0059ae80 + 0x18 h, the hole word map 0x0053caf0, the 64
  // walker records 0x00585850 + 0x4c i, the 152 golfers' hole byte 0x005794d9 + 0x100 g, the mode [0x004d20dc], the
  // map-only flag [0x00822b80], [0x005685f4] and [0x005a34e0].
  // Recording stand-ins (calleeStand): every drawing callee, Window::refreshFull, enterModal483d30, spreadAllShade (mode
  // 1; its shade grids are the scene's), rateLot / evalPlacementArea (mode 2; results from their arguments only) and
  // View_toLocal, whose cursor is the scene's view entry for the CURRENT VISIT COUNT of modal.js's block (+4, compared
  // memory): cursor i is what the function hit-tests while script frame i is delivered, however many redraws happen in
  // between. The pumps are modal.js's models (pumpMessages is the only visit). Run for real: mapToScreen, cellAt,
  // sampleHeight, tileBlocked, overlayCharAt, clamp, foldRange, absDiff, setLinePoint, __itoa, appendCourseTitle,
  // appendDate, appendNumber, pickGolferName. The display [0x0083ad50] is a guarded recorder (slot +0x9c).
  const RM = { s: null, blk: null };
  const RM_SAVE = [[0x005722e8, 2500], [0x00578340, 0x30 * 21], [0x0058dd34, 0x18], [0x00822da0, 2500],
                   [0x0082376c, 2500], [0x00575ab0, 0x208 * 19], [0x0059ae80, 0x18 * 19], [0x0053caf0, 2 * 2500],
                   [0x00585850, 0x4c * 64], [0x005794b8, 0x100 * 152], [0x004d20dc, 4], [0x00822b80, 4],
                   [0x005685f4, 4], [0x005a34e0, 4], [0x005685f0, 4], [0x00822b84, 4], [0x0051a068, 0x200],
                   [0x00821c50, 0x40], [0x005a6d3c, 4], [0x005a412c, 8]];
  // the per-type bytes: the live values (log/c3/alt/c3et_peek.py at the menu) with five changed so every arm of the
  // pass-2 colour switch is taken (t 5 -> 10, t 8 -> 18, t 11 -> 0x30 above the table, t 12 -> 0xf0 negative, t 19 -> 3)
  const RM_T36 = [0, 1, 2, 2, 4, 10, 4, 7, 18, 7, 4, 0x30, 0xf0, 13, 13, 13, 13, 17, 4, 3];
  const RM_DD34 = [0x00, 0x01, 0x08, 0x09, 0x03, 0x0a, 0xff, 0x02, 0x0b, 0x04, 0x05, 0x0c, 0x0d, 0x07, 0x0e, 0xff,
                   0xff, 0x06, 0x0f, 0xff];
  // "full": types 15 and 16 lose their 0xff byte, so the pass-2 cell arm reaches its 0x197 / 0xf9 tables (live: dead)
  const RM_DD34_FULL = RM_DD34.map(function (v, t) { return t === 15 ? 0x01 : (t === 16 ? 0x02 : v); });
  const RM_PAR = [3, 4, 5, 0, 4, 3, 6, 2, 0, 4, 5, 3, 4, 0, 3, 5, 4, 4];
  const RM_PAR_ALL = [3, 4, 5, 3, 4, 3, 6, 2, 4, 4, 5, 3, 4, 5, 3, 5, 4, 4];

  function rmGrid() {
    for (let i = 0; i < 2500; i++) wr8(0x005722e8 + i, 0x14);
    for (let x = 10; x < 22; x++)
      for (let y = 20; y < 32; y++) {
        const t = (x * 7 + y * 3) % 20;
        wr8(0x005722e8 + x * 50 + y, (x === 15 && y === 25) || (x === 21 && y === 31) ? 0x14 : t);
      }
  }
  function rmHoles(par) {
    for (let h = 0; h < 19; h++) {
      const r = 0x00575ab0 + 0x208 * h;
      for (let o = 0; o < 0x208; o += 4) wr32(r + o, (0x01010101 * (h + 1)) ^ (o * 0x10001));
      if (h === 0) continue;
      wr8(r, par[h - 1]);
      wr16(r + 4, 100 + 23 * h);
      wr32(r + 8, 10 + (h % 12)); wr32(r + 0xc, 20 + ((h * 5) % 12));
      wr32(r + 0x18, 10 + ((h * 7) % 12)); wr32(r + 0x1c, 20 + ((h * 3) % 12));
      wr32(r + 0x20, (h % 3) ? 2 * h : 0);
      wr32(r + 0x1ec, 4000 + 517 * h);
      const w = 0x0059ae80 + 0x18 * h;
      for (let o = 0; o < 0x18; o += 4) wr32(w + o, 0x11110000 + 0x100 * h + o);
      const kind = h % 3;   // 0: both waypoints, 1: only the first, 2: none
      wr32(w + 8, kind === 2 ? -1 : (12 + h % 9) << 10); wr32(w + 0xc, kind === 2 ? -1 : (22 + h % 7) << 10);
      wr32(w + 0x10, kind === 0 ? (14 + h % 5) << 10 : -1); wr32(w + 0x14, kind === 0 ? (24 + h % 6) << 10 : -1);
    }
    for (let o = 0; o < 0x18; o += 4) wr32(0x0059ae80 + o, 0x22220000 + o);
    for (let i = 0; i < 2500; i++) wr16(0x0053caf0 + 2 * i, (i * 0x9e37) & 0xffff);
  }
  // walkers: {i, f (+0x12), st (+0x13), kind (+0x1e), w1c, b16, x, y, name, w22, w24, w26}
  function W(i, f, st, kind, w1c, name) {
    return { i: i, f: f, st: st, kind: kind, w1c: w1c, name: name || '', b16: (i * 3) & 0xff,
             x: 10 + (i % 12), y: 20 + ((i * 5) % 12), w22: 3 + i, w24: 17 * i - 40, w26: 2 * i - 9 };
  }
  // slots 0..9 of pickGolferName's two pointer tables 0x004c1464 / 0x004c148c are strings (log/c3/c3et_notes.txt);
  // kinds whose slot is outside them (state -1, -7) carry a name so the tables are never read
  const RM_WALKERS = [
    W(0, 1, -2, 5, 7), W(1, 9, -3, 0xc, 0, 'ANNA'), W(2, 2, -4, 0xc, 5), W(3, 8, -5, 0xd, 11),
    W(4, 1, -6, 3, 2), W(5, 9, -1, 4, 6, 'BOB'), W(6, 0, -2, 4, 6), W(7, 1, 2, 4, 6),
    W(8, 8, -7, 4, 9, 'CARL'), W(9, 0xb, -2, 0xb, 13), W(10, 1, -3, 0xe, 20), W(11, 8, -4, 2, 31),
    W(12, 5, -5, 0xc, 26), W(13, 1, -2, 0xc, 39), W(14, 9, -2, 0xa, 1), W(15, 2, -3, 1, 4),
  ];
  function rmWalkers(list) {
    for (let o = 0; o < 0x4c * 64; o += 4) wr32(0x00585850 + o, 0);
    list.forEach(function (w) {
      const r = 0x00585850 + 0x4c * w.i;
      wr32(r, 0x31000000 + w.i); wr32(r + 4, 0x32000000 + w.i);
      wr8(r + 0x10, w.x); wr8(r + 0x11, w.y); wr8(r + 0x12, w.f); wr8(r + 0x13, w.st); wr8(r + 0x16, w.b16);
      wr16(r + 0x1c, w.w1c); wr16(r + 0x1e, w.kind); wr16(r + 0x22, w.w22); wr16(r + 0x24, w.w24);
      wr16(r + 0x26, w.w26);
      wrStr(r + 0x2a, w.name, 0x10);
    });
  }
  function rmGolfers() {
    for (let g = 0; g < 152; g++) {
      const v = (g % 7 === 0) ? 0xff : (g % 5 === 1 ? 1 + (g % 18) : (g % 11 === 3 ? 19 : 0));
      wr8(0x005794d9 + 0x100 * g, v);
    }
  }
  // the cell tables the function reaches through cellAt 0x0043d6f0 (width 0x0053f3e8 + 4 t, base 0x005a9370 + 4 t), with
  // the values they hold at the menu 8 s after the window (log/c3/alt/c3et_peek.py). Seeded because the walker list
  // DIVIDES by the width (idiv at 0x00457a9a): without the seeding the key ERRORed (arithmetic error) whenever it ran
  // first in a boot and was GREEN when it ran later, so it depended on when the game had filled the table.
  const RM_CELLS = [[0xf9, 0xe, 0x58d], [0x189, 0x10, 0x43f], [0x18a, 0x10, 0x48d], [0x18b, 0x10, 0x4db],
    [0x18d, 0xc, 0x569], [0x18e, 0xc, 0x575], [0x18f, 0xc, 0x581], [0x194, 0, -1], [0x197, 0, -1], [0x19d, 0, -1],
    [0x20e, 0xd, 0x7a41], [0x20f, 0xc, 0x7bd1], [0x210, 0xd, 0x7d69], [0x211, 0x10, 0x7f29], [0x212, 0x10, 0x80f1],
    [0x213, 0x16, 0x82c1], [0x214, 0x10, 0x84d9], [0x215, 0xc, 0x8701], [0x216, 0xe, 0x7aa9], [0x217, 0x14, 0x7c31],
    [0x218, 0xd, 0x7dd1], [0x219, 0x15, 0x7fa9], [0x21a, 0x11, 0x8171], [0x21b, 0x11, 0x8371], [0x21c, 0x17, 0x8559],
    [0x21d, 0x11, 0x8761], [0x21e, 0x17, 0x7b19], [0x21f, 0x13, 0x7cd1], [0x220, 0x1e, 0x7e39], [0x221, 0x14, 0x8051],
    [0x222, 0x19, 0x81f9], [0x223, 0x1c, 0x83f9], [0x224, 0x1e, 0x8611], [0x225, 0x23, 0x87e9]];
  function rmApply(s) {
    RM_SAVE.forEach(function (e) { __freeze.save(ptr(e[0]), e[1]); });
    RM_CELLS.forEach(function (c) {
      __freeze.save(ptr(0x0053f3e8 + 4 * c[0]), 4); wr32(0x0053f3e8 + 4 * c[0], c[1]);
      __freeze.save(ptr(0x005a9370 + 4 * c[0]), 4); wr32(0x005a9370 + 4 * c[0], c[2]);
    });
    rmGrid();
    for (let t = 0; t < 20; t++) { wr8(0x00578376 + 0x30 * t, RM_T36[t]); wr8(0x0058dd34 + t, s.dd34[t]); }
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++) {
        wr8(0x00822da0 + x * 50 + y, (x * 13 + y * 7) & 0xff);
        wr8(0x0082376c + x * 50 + y, (x * 5 + y * 11 + 0x40) & 0xff);
      }
    rmHoles(s.par);
    rmWalkers(s.walkers);
    rmGolfers();
    wr32(0x004d20dc, s.mode);
    wr32(0x00822b80, s.mapOnly);
    wr32(0x005685f4, 5);
    wr32(0x005a34e0, s.g5);
    wr32(0x005685f0, 7);
    wr32(0x00822b84, 0);
    wr8(0x0051a068, 0);
  }

  // points (the hit tests of 0x004581c2..0x004582dc): the six buttons, the scroll arrows (mode 0, > 8 walkers), a hole
  // row (column 1 for h <= 9, column 2 at x >= 0x190), and three points on no button: above the list (y < 0x101), in
  // the exit band (0x101 <= y <= 0x118), below it (y > 0x118)
  const BTN = { 0: [0xc0, 0x120], 1: [0x50, 0x150], 2: [0x220, 0x120], 3: [0x2a0, 0x150], q: [0x2a0, 0x220],
                up: [0x310, 0x50], down: [0x310, 0xd0] };
  const OUTP = [0x150, 0x30], MIDP = [0x150, 0x110], LOWP = [0x150, 0x200];
  function holePt(h) { return h <= 9 ? [0x60, 0x68 + 17 * (h - 1) + 5] : [0x200, 0x68 + 17 * (h - 10) + 5]; }
  function rmScene(o, steps) {
    const s = { mode: -1, mapOnly: 0, g5: 0, par: RM_PAR, dd34: RM_DD34, walkers: RM_WALKERS.slice(0, 6) };
    Object.keys(o).forEach(function (k) { s[k] = o[k]; });
    s.view = steps.map(function (st) { return st[0]; });
    s.script = steps.map(function (st) { return st.slice(1); });
    return s;
  }
  function P1(p) { return press(p[0], p[1]); }
  function P2(p) { return press2(p[0], p[1]); }
  const RM_SCENES = [
    // 0..2: map only ([0x00822b80] = 1): the two passes, then the exit (no legend, no input)
    rmScene({ mapOnly: 1, mode: -1 }, []),
    rmScene({ mapOnly: 1, mode: 1, g5: 2 }, []),
    rmScene({ mapOnly: 1, mode: 2 }, []),
    // 3: mode -1 (the hole list): select 3, move it to 7 (down), select 12, move 12 to 2 (up), then 12 -> 5, 12 -> 14,
    //    12 -> 12 (no shift), hover the quit button, click it
    rmScene({ mode: -1 }, [[OUTP], [holePt(3), P1(holePt(3))], [holePt(7), P2(holePt(7))],
      [holePt(12), P1(holePt(12))], [holePt(2), P2(holePt(2))], [holePt(5), P2(holePt(5))],
      [holePt(14), P2(holePt(14))], [holePt(12), P2(holePt(12))], [BTN.q], [BTN.q, P1(BTN.q)]]),
    // 4: mode 0, twelve listed walkers: scroll down twice, up once, a key on the up arrow
    rmScene({ mode: 0, walkers: RM_WALKERS }, [[BTN.down], [BTN.down, P1(BTN.down)], [BTN.down, P1(BTN.down)],
      [BTN.up], [BTN.up, P1(BTN.up)], [BTN.up, keyEsc()]]),
    // 5: mode 1, through every mode by its button (3 -> 2, 0 -> -1, 2 -> 1, 1 -> 0), a click below the band on no
    //    button (redraw), a click in the exit band
    rmScene({ mode: 1, g5: 2 }, [[BTN[3]], [BTN[3], P1(BTN[3])], [BTN[0]], [BTN[0], P1(BTN[0])], [BTN[2]],
      [BTN[2], P1(BTN[2])], [BTN[1]], [BTN[1], P1(BTN[1])], [LOWP], [LOWP, P1(LOWP)], [MIDP, P1(MIDP)]]),
    // 6: mode -2 (outside the buttons' range but defined): the else backdrop image, a key
    rmScene({ mode: -2 }, [[OUTP, keyEsc()]]),
    // 7: mode 0, few walkers, every hole in use (the exit scan finds none free), types 15 / 16 without 0xff, a key
    rmScene({ mode: 0, par: RM_PAR_ALL, dd34: RM_DD34_FULL }, [[OUTP], [OUTP, keyEsc()]]),
    // 8: mode 2: select 4, move it to 11 (column 2, +9), a click that is neither button (3), quit by its button
    rmScene({ mode: 2 }, [[holePt(4), P1(holePt(4))], [holePt(11), P2(holePt(11))],
      [OUTP, { k: 'raw', set: { 0x00822d68: 3, 0x00822d74: 1, 0x00822d64: 0x50 } }], [BTN.q], [BTN.q, P1(BTN.q)]]),
    // 9: mode 0, nine listed walkers: up at scroll 0 (no change), down (more), down again (no more), a key
    rmScene({ mode: 0, walkers: RM_WALKERS.slice(0, 12) }, [[BTN.up], [BTN.up, P1(BTN.up)], [BTN.down],
      [BTN.down, P1(BTN.down)], [BTN.down, P1(BTN.down)], [BTN.down, P1(BTN.down)], [OUTP], [OUTP, keyEsc()]]),
    // 10: mode -1: a right click above the list (target clamps to 1 = the selected hole), a key on the quit button
    rmScene({ mode: -1 }, [[OUTP, P2(OUTP)], [BTN.q], [BTN.q, keyEsc()]]),
    // 11: mode 1, the walkers in mode 1 are not listed; the hover over every button (each state's image), then quit
    rmScene({ mode: 1, walkers: RM_WALKERS }, [[BTN[0]], [BTN[1]], [BTN[2]], [BTN[3]], [BTN.q], [BTN.q, P1(BTN.q)]]),
    // 12: mode 0, ten listed walkers: the cursor on every edge the hit tests check (each lands on no button, so no
    //     redraw), then down to the end of the list (scroll 2 with ten walkers makes the thumb height equal its limit,
    //     0x004579c5), one more down (no more to show), a key
    rmScene({ mode: 0, walkers: RM_WALKERS.slice(0, 13) }, [[[0x30, 0x150]], [[0x50, 0x1a0]], [[0xc0, 0x80]],
      [[0xc0, 0x160]], [[0x220, 0x80]], [[0x220, 0x160]], [[0x2a0, 0x100]], [[0x2a0, 0x200]], [[0x2a0, 0x260]],
      [[0x320, 0x50]], [[0x310, 0x30]], [[0x310, 0x100]], [BTN.down], [BTN.down, P1(BTN.down)],
      [BTN.down, P1(BTN.down)], [BTN.down, P1(BTN.down)], [OUTP], [OUTP, keyEsc()]]),
  ];
  function rmView(n) {
    const v = RM.s.view;
    if (v.length === 0) return OUTP;
    return v[Math.min(n, v.length - 1)];
  }
  function rmStands(fx) {
    const words = function (ix, nw) { return function (a) {
      const p = ptr(a[ix]), out = [];
      for (let k = 0; k < nw; k++) out.push(p.add(4 * k).readU32());
      return out;
    }; };
    calleeStand(fx, 0, 0x00473e60, 5, 0, { abi: 'thiscall' });                       // Image_draw (ret 0x10)
    calleeStand(fx, 1, 0x004762d0, 5, 0, { abi: 'thiscall' });                       // setDrawTarget (ret 0x10)
    calleeStand(fx, 2, 0x00404b70, 4, 0, { str: { 0: 8 } });                         // drawCenteredText
    calleeStand(fx, 3, 0x004049d0, 4, 0, { str: { 0: 8 } });                         // drawText4049d0
    calleeStand(fx, 4, 0x00404bc0, 4, 0, { str: { 0: 8 } });                         // drawCenteredText2
    calleeStand(fx, 5, 0x00475df0, 4, 0, { abi: 'thiscall', mask: [1], extra: words(1, 8) });   // drawRoutingPoly
    calleeStand(fx, 6, 0x0040bf20, 5, 0, {});                                        // lineTo
    calleeStand(fx, 7, 0x004628d0, 8, 0, {});                                        // addSprite
    calleeStand(fx, 8, 0x00480b00, 6, 0, { abi: 'thiscall' });                       // fillWindowRect (ret 0x14)
    calleeStand(fx, 9, 0x00476310, 5, function (n, a) { return a[3]; }, { abi: 'thiscall' });   // Widget_setQuad
    calleeStand(fx, 10, 0x00478530, 5, 0, { abi: 'thiscall', str: { 1: 8 } });      // drawCenteredTextLines
    calleeStand(fx, 11, 0x00480c80, 2, 0, { abi: 'thiscall' });                      // Window::refreshFull (ret 4)
    calleeStand(fx, 12, 0x00483d30, 0, 0, {});                                       // enterModal483d30
    calleeStand(fx, 13, 0x0047ab50, 3, function (n, a) {                             // View_toLocal (thiscall, ret 8)
      const p = rmView(RM.blk.add(4).readU32());
      ptr(a[1]).writeS32(p[0]); ptr(a[2]).writeS32(p[1]);
      return 0;
    }, { abi: 'thiscall', mask: [1, 2], extra: function () { return rmView(RM.blk.add(4).readU32()); } });
    calleeStand(fx, 14, 0x004616f0, 0, 0, {});                                       // spreadAllShade
    calleeStand(fx, 15, 0x0042ef40, 2, function (n, a) {                             // rateLot(x, y)
      return ((a[0] * 37 + a[1] * 11) % 400) - 40;
    }, {});
    calleeStand(fx, 16, 0x0040db90, 4, function (n, a) {                             // evalPlacementArea(x, y, 2, 5)
      return ((a[0] + 2 * a[1]) % 9 === 0) ? -1 : ((a[0] * 5 + a[1] * 3) % 60) - 10;
    }, {});
  }
  const RM_DISP_VT = vtable({ 0x9c: [3, function (s, a) { rec(0xd9c, [a[0], a[1], a[2]]); return 0; }] }, 0x400);
  const RM_DISP = zalloc(0x40);
  RM_DISP.writePointer(RM_DISP_VT);

  F.c3et_routing = function () {
    const fx = { clog: newClog(0x10000), clogCap: 0x10000, tlog: newLog() };
    const M = __modal({ maxCalls: 600, maxFrames: 600 });
    __importStandIn({ module: 'golf_clean.exe', imports: M.imports() });
    RM.blk = M.fx().modal;
    rmStands(fx);              // freezes the first thread
    M.arm();
    __freeze.save(ptr(0x0083ad50), 4);
    ptr(0x0083ad50).writePointer(__recDisplay.guard(RM_DISP));
    RM_SCENES.forEach(function (sc, i) {
      Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () {
        RM.s = sc;
        rmApply(sc);
        M.select(sc.script);
        return 0;
      } });
    });
    negGetters(fx, NEG_RM, 'r');
    return Object.assign(fx, M.fx());
  };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
