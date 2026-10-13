// Fixtures for batch c3er (round 31, 2026-10-13): the end-of-career screens of golf_clean.exe.
// Reimplementations: shim/src/re/c3er.cpp. Registry: re/frida/registry.d/c3er.py.
// Purposes and per-jcc tables: log/c3/c3er_purpose.md. Notes: log/c3/c3er_notes.txt.
//
// Method (helpers copied from c3ea.js / c3ec.js):
// - Every key freezes the game's first thread (re/frida/js/freeze.js) and saves every global it writes; the flush
//   restores them newest first before the resume.
// - drawScoreSummary 0x00473470 is __cdecl with no arguments, so one vector is one whole call. The registry passes an
//   unused cdecl stack word "$sN": diff_hook resolves it before the state snapshot, so the getter of fx.sN applies
//   scene N (the ten 0x9c scoreboard rows at 0x00541ce0 and the globals addScore 0x004732d0 reads).
// - The callees that draw, load an image, build the Image object, refresh the window or pump are stood in on the A/B
//   thread only (calleeStand): each call is counted, folded into a running FNV-1a hash and logged while there is room,
//   with the text of every string argument. The log ($clog) is compared state, so a different call, argument, order or
//   text reads RED. The `this` pointer of the local Image object is masked: the original's frame and the
//   reimplementation's frame put it at different addresses.
// - addScore 0x004732d0, appendCents 0x0042dd50 and __itoa 0x004ad425 run for real (their C3 hooks, or the originals
//   under SIMGOLF_HOOKS_OFF): they produce the row the screen highlights and the text it draws.
(function () {
  const F = {};

  function alloc(n) {
    const p = __keepAlloc(n);
    for (let i = 0; i < n; i += 4) p.add(i).writeU32(0);
    return p;
  }

  let frozen = false;
  function freezeNow() {
    if (frozen) return;
    __freeze.thread();
    frozen = true;
    __freeze.atFlush(function () { frozen = false; });
    __freeze.negOn();   // the negative-control build's exported switch (re/frida/js/freeze.js)
  }

  function wr32(a, v) { ptr(a).writeU32(v >>> 0); }
  function wr16(a, v) { ptr(a).writeU16(v & 0xffff); }
  function wr8(a, v) { ptr(a).writeU8(v & 0xff); }
  function saveRange(a, n) { __freeze.save(ptr(a), n); }
  function wrStr(a, s, max) {
    const p = ptr(a);
    for (let i = 0; i < max; i++) p.add(i).writeU8(i < s.length ? s.charCodeAt(i) & 0xff : 0);
  }

  // ---- calleeStand (c3ea's / c3ec's, with Interceptor.replaceFast where it exists so a foreign thread reaches the
  // real body through the trampoline instead of re-entering the stub): log word 0 = cursor, words 1..32 = call count
  // of stub k, word 0x84 = FNV-1a over every call (the callee address and the logged values), entries
  // {va, count, values...} from +0x90 while they fit.
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x4000;
  const CHASH = 0x84, CHEAD = 0x90;
  function newClog(cap) { const l = alloc(cap || CLOG); l.writeU32(CHEAD); l.add(CHASH).writeU32(0x811c9dc5); return l; }
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

  // ---- scenes: fx.s<i> applies scene i and returns 0 (the unused cdecl stack word)
  function scenesOn(fx, scenes) {
    scenes.forEach(function (sc, i) {
      Object.defineProperty(fx, 's' + i, { enumerable: true, configurable: true, get: function () {
        sc(fx);
        return 0;
      } });
    });
  }

  // ====================================================================================================================
  // drawScoreSummary 0x00473470
  // ====================================================================================================================
  // The scoreboard table: ten rows of 0x9c bytes from 0x00541ce0 (stride at 0x0047350c..0x0047351b, base 0x00473522).
  const ROWS = 0x00541ce0, STRIDE = 0x9c, NROWS = 10;
  // addScore 0x004732d0 (shim/src/re/c3ah.cpp) reads these and writes 0x008392a8 and the table.
  const G_ID = 0x00822c78, G_CASH = 0x00571fd4, G_SKILL = 0x00541cd8, G_FUN = 0x0059ae78, G_LEVEL = 0x00822c88;
  const G_NAME = 0x004d6098;   // the course-name string addScore copies into the new row
  // appendCourseTitle 0x0040daa0, reached from addScore: the course index, the site byte and the hole count.
  const G_COURSE = 0x0059bf90, G_SITE = 0x00571ff4, G_HOLES = 0x005685f0;

  const SS_SAVE = [
    [ROWS, STRIDE * NROWS], [0x0051a068, 0x200], [0x008392a8, 0x48], [0x0058a528, 0x20], [0x00569628, 4],
    [G_ID, 4], [G_CASH, 4], [G_SKILL, 4], [G_FUN, 4], [G_LEVEL, 4], [G_NAME, 0x40],
    [G_COURSE, 4], [G_SITE, 4], [G_HOLES, 4]
  ];

  // row: {name, text, fun, skill, cash, level, id}
  function putRow(i, r) {
    const b = ROWS + STRIDE * i;
    wrStr(b, r.name === undefined ? '' : r.name, 0x40);
    wrStr(b + 0x40, r.text === undefined ? '' : r.text, 0x40);
    wr32(b + 0x80, r.fun | 0);
    wr32(b + 0x84, r.skill | 0);
    wr32(b + 0x88, r.cash | 0);
    wr16(b + 0x96, r.level | 0);
    wr32(b + 0x98, r.id | 0);
  }
  function globals(g) {
    wr32(G_ID, g.id | 0);
    wr32(G_CASH, g.cash | 0);
    wr32(G_SKILL, g.skill | 0);
    wr32(G_FUN, g.fun | 0);
    wr32(G_LEVEL, g.level | 0);
    wrStr(G_NAME, g.name === undefined ? 'C3ER-NEW' : g.name, 0x40);
    wr32(G_COURSE, 0);
    wr8(G_SITE, 0);
    wr32(G_HOLES, g.holes === undefined ? 5 : g.holes);
  }

  // A descending default table: rowScore(i) = (cash/10 + fun + skill) * (level + 1) falls with i, so a new score
  // places at a row chosen by the scene's globals.
  function defaultRow(i, over) {
    const r = { name: 'C3ER' + i + '-' + 'XY'.repeat(i % 4), text: 'note' + i, fun: 40 - 4 * i, skill: 50 - 5 * i,
                cash: 100000 - 10000 * i, level: 2, id: 100 + i };
    if (over) Object.keys(over).forEach(function (k) { r[k] = over[k]; });
    return r;
  }
  function table(make) {
    for (let i = 0; i < NROWS; i++) putRow(i, make(i));
  }

  function ssScene(build) {
    return function (fx) {
      SS_SAVE.forEach(function (s) { saveRange(s[0], s[1]); });
      build();
    };
  }

  // The twelve scenes. `place` chooses where addScore inserts: a score above row 0 selects row 0, a score between two
  // row scores selects that row, a score under every row never places (0x008392a8 stays -1).
  const SS_SCENES = [
    // 0: a full table, the new score above every row -> row 0 is the selected one
    ssScene(function () { table(function (i) { return defaultRow(i); }); globals({ id: 7, cash: 2000000, skill: 60, fun: 50, level: 2 }); }),
    // 1: the new score between rows 3 and 4 -> row 4 is selected (level 0: the multiplier is 1)
    ssScene(function () { table(function (i) { return defaultRow(i); }); globals({ id: 7, cash: 195000, skill: 0, fun: 0, level: 0 }); }),
    // 2: the new score under every row -> addScore returns -1 and no row is highlighted
    ssScene(function () { table(function (i) { return defaultRow(i); }); globals({ id: 7, cash: 1000, skill: 0, fun: 0, level: 0 }); }),
    // 3: five empty names (the skip at 0x00473530) with the new score at the top
    ssScene(function () { table(function (i) { return defaultRow(i, i % 2 ? { name: '' } : null); }); globals({ id: 7, cash: 2000000, skill: 60, fun: 50, level: 2 }); }),
    // 4: row 0 empty and nothing placed: the i == 0 block at 0x004739c2 is never reached
    ssScene(function () { table(function (i) { return defaultRow(i, i === 0 ? { name: '' } : null); }); globals({ id: 7, cash: 1000, skill: 0, fun: 0, level: 0 }); }),
    // 5: negative money and ratings (appendCents' '-' arm) with nothing placed
    ssScene(function () {
      table(function (i) { return defaultRow(i, { cash: -123456 - 1000 * i, skill: -4567 + 100 * i, fun: -89 - i, level: 1 }); });
      globals({ id: 7, cash: -100000000, skill: -1000, fun: -1000, level: 0 });
    }),
    // 6: cents under ten (the zero-padding arm of appendCents) and a zero level
    ssScene(function () {
      table(function (i) { return defaultRow(i, { cash: 10 * i, skill: 400 + i, fun: i, level: 0 }); });
      globals({ id: 7, cash: -100000000, skill: -1000, fun: -1000, level: 0 });
    }),
    // 7: large values: the total (cash / 10 + skill + fun) * (level + 1) wraps
    ssScene(function () {
      table(function (i) { return defaultRow(i, { cash: 2000000000 - 1000 * i, skill: 1000000 * i, fun: 999999, level: 300 + i }); });
      globals({ id: 7, cash: -100000000, skill: -1000, fun: -1000, level: 0 });
    }),
    // 8: one-character and long names (the highlighted row's Text_put width and the rule it draws)
    ssScene(function () {
      table(function (i) { return defaultRow(i, { name: i === 0 ? 'Z' : 'LONGNAME-LONGNAME-LONGNAME-' + i }); });
      globals({ id: 7, cash: 2000000, skill: 60, fun: 50, level: 2 });
    }),
    // 9: the new course id already in row 2, with a score under row 2 -> addScore returns -1 at 0x00473348
    ssScene(function () { table(function (i) { return defaultRow(i, i === 2 ? { id: 7 } : null); }); globals({ id: 7, cash: 1000, skill: 0, fun: 0, level: 0 }); }),
    // 10: every row the same score; the new score equals it, so row 0 is taken
    ssScene(function () {
      table(function (i) { return defaultRow(i, { cash: 50000, skill: 10, fun: 10, level: 1 }); });
      globals({ id: 7, cash: 50000, skill: 10, fun: 10, level: 1 });
    }),
    // 11: negative level words (level + 1 == 0 kills the total) and empty row texts
    ssScene(function () {
      table(function (i) { return defaultRow(i, { level: -1, text: '', cash: 1234 * (i + 1), skill: -i, fun: 7 * i }); });
      globals({ id: 7, cash: -100000000, skill: -1000, fun: -1000, level: 0 });
    })
  ];

  function ssStands(fx) {
    // k0 Image_ctor(this) -> this (0x00473498); the `this` pointer is a frame address, masked in the log
    calleeStand(fx, 0, 0x00474ae0, 1, function (n, a) { return a[0]; }, { abi: 'thiscall', mask: [0] });
    // k1 loadImage(this, name, pal, ofs, count, flags) (0x004734bf, 0x00473501)
    calleeStand(fx, 1, 0x00475840, 6, 0, { abi: 'thiscall', mask: [0], str: { 1: 10 } });
    // k2 Surface_fillRegion(this, src, x, y, w, h) (0x004734e5, 0x00473595)
    calleeStand(fx, 2, 0x00475c90, 6, 0, { abi: 'thiscall', mask: [0] });
    // k3 setDrawTarget(this, surf, a, b, c) (0x004735c0, 0x004736ac, 0x00473844, 0x0047390d)
    calleeStand(fx, 3, 0x004762d0, 5, 0, { abi: 'thiscall' });
    // k4 Text_put(this, text) -> the run width (0x004735d9): 7 * length + 3, so the rule the highlight draws moves
    // with the name and the two arms must ask for the same name at the same point
    calleeStand(fx, 4, 0x00477250, 2, function (n, a) {
      let len = 0; const p = ptr(a[1]);
      while (len < 0x40 && p.add(len).readU8() !== 0) len++;
      return 7 * len + 3;
    }, { abi: 'thiscall', str: { 1: 10 } });
    // k5 Widget_fillRect(this, x1, y1, x2, y2, colour) (0x0047360a, 0x00473628)
    calleeStand(fx, 5, 0x00478b80, 6, 0, { abi: 'thiscall' });
    // k6 drawCenteredText(text, x, y, colour), __cdecl (eleven call sites 0x00473638..0x004739f8)
    calleeStand(fx, 6, 0x00404b70, 4, 0, { str: { 0: 12 } });
    // k7 Window::refreshFull(this, 0) (0x00473a19)
    calleeStand(fx, 7, 0x00480c80, 2, 0, { abi: 'thiscall' });
    // k8 pumpAndRedraw(0), __cdecl (0x00473a20)
    calleeStand(fx, 8, 0x0045c0c0, 1, 0, {});
    // k9 Image_dtor(this) (0x00473a37)
    calleeStand(fx, 9, 0x00474c40, 1, function (n, a) { return a[0]; }, { abi: 'thiscall', mask: [0] });
  }

  F.c3er_scores = function () {
    const fx = {};
    fx.clog = newClog();
    ssStands(fx);          // freezes the first thread
    scenesOn(fx, SS_SCENES);
    return fx;
  };

  // ====================================================================================================================
  // CareerRetirementFlow 0x0044cff0
  // ====================================================================================================================
  // The end-of-year screen: six Surface captures of two images, four statistics rows, a highlights list over the months
  // of the year, the board's debt warnings and a modal loop that ends on a click or a key. Every drawing, image, sound
  // and story callee is a recording stand-in; the text builders (appendNumber, appendCents, __itoa, landmarkName,
  // clamp) and addScore run for real; the pumps are modal.js's models, fed by a script of input frames.
  const MONTH = 18;                       // g_date / 1024; this year's slot is 18, last year's 10 (eight steps back)
  const EV = 0x00568600;                  // the per-month event word table (0x0044daff)
  const A_CASH = 0x0051b388, A_FUN = 0x0053e63c, A_SKILL = 0x005409ac, A_MEMB = 0x00520640;
  const HOLES = 0x00575ab0, NAMES_C0 = 0x0055d738, NAMES_40 = 0x004c26b0, NAMES_60 = 0x0058dd50;

  const CR_SAVE = [
    [0x00834170, 4], [0x005a6d3c, 4], [0x0058b388, 4], [0x00519d40, 4], [0x00571fd4, 4], [0x0059e7b8, 4],
    [0x005a6374, 4], [A_CASH, 0x3e8], [A_FUN, 0x3e8], [A_SKILL, 0x3e8], [A_MEMB, 0x3e8], [EV, 0x3e8],
    [HOLES, 0x208 * 20], [NAMES_C0, 0x25 * 8], [NAMES_40, 0x14 * 8], [NAMES_60, 0x38 * 8],
    [0x0051a068, 0x200], [0x00821c50, 0x40], [0x0058a528, 0x20], [0x00569628, 4],
    [ROWS, STRIDE * NROWS], [0x008392a8, 0x48], [G_ID, 4], [G_SKILL, 4], [G_FUN, 4], [G_LEVEL, 4], [G_NAME, 0x40],
    [G_COURSE, 4], [G_SITE, 4], [G_HOLES, 4]
  ];

  // One synthetic name per slot of the three name tables the switch arms read. Only the first eight slots are written:
  // the pathway table 0x004c26b0 is 0x258 bytes (0x14 * 30) ahead of the month-name POINTER table 0x004c2908 that the
  // date prefix reads, and a first run that filled 32 slots overwrote it (faults reading 'PATH' and 0 as pointers).
  function fillNames() {
    for (let i = 0; i < 8; i++) {
      wrStr(NAMES_C0 + 0x25 * i, 'HOME' + i, 0x25);
      wrStr(NAMES_40 + 0x14 * i, 'PATH' + i, 0x14);
      wrStr(NAMES_60 + 0x38 * i, 'RIVAL' + i, 0x38);
    }
  }
  function stats(li, ti, cash, fun, skill, memb) {
    // li is last year's slot (month - 8) % 500, ti this year's month % 500
    const put = function (base, last, now) {
      for (let i = 0; i < 500; i++) wr16(base + 2 * i, 0);
      wr16(base + 2 * li, last); wr16(base + 2 * ti, now);
    };
    put(A_CASH, cash[0], cash[1]);
    put(A_FUN, fun[0], fun[1]);
    put(A_SKILL, skill[0], skill[1]);
    put(A_MEMB, memb[0], memb[1]);
  }
  function events(list) {
    for (let i = 0; i < 500; i++) wr16(EV + 2 * i, 0);
    list.forEach(function (e) { wr16(EV + 2 * e[0], e[1]); });
  }
  function holes(nonZero) {
    for (let i = 0; i < 20; i++) wr8(HOLES + 0x208 * i, nonZero.indexOf(i) >= 0 ? 0x41 : 0);
  }

  // the recording stand-ins of CareerRetirementFlow; `view` is the per-vector View_toLocal script
  let crCur = 0;
  const CR_VIEW = [];
  function viewPoint(n) {
    const s = CR_VIEW[crCur] || [[0, 0]];
    return s[Math.min(n, s.length - 1)];
  }
  function crStands(fx) {
    calleeStand(fx, 0, 0x00474ae0, 1, function (n, a) { return a[0]; }, { abi: 'thiscall', mask: [0] });   // Image_ctor
    // Surface_ctor: the body is skipped, so the stub fills the 0x2c object; +0x1c is the line height the screen reads
    // back for its y step (0x0044dad0 reads the first Surface's, 0x0044e002 the line strip's).
    calleeStand(fx, 1, 0x00473ab0, 1, function (n, a) {
      const p = ptr(a[0]);
      for (let i = 0; i < 0x2c; i += 4) p.add(i).writeU32(0);
      p.add(0x1c).writeU32(20 + n);
      return a[0];
    }, { abi: 'thiscall', mask: [0] });
    calleeStand(fx, 2, 0x00474dd0, 7, 0, { abi: 'thiscall', mask: [0] });                   // Surface_createFull
    calleeStand(fx, 3, 0x00475840, 6, 0, { abi: 'thiscall', mask: [0], str: { 1: 12 } });   // loadImage
    calleeStand(fx, 4, 0x00473c60, 8, 0, { abi: 'thiscall', mask: [0, 1] });                // Surface_blitTakeDirty
    calleeStand(fx, 5, 0x00474cb0, 1, 0, { abi: 'thiscall', mask: [0] });                   // Image_releaseAll
    calleeStand(fx, 6, 0x004481b0, 5, 0, {});                                               // playSound
    calleeStand(fx, 7, 0x00474440, 7, 0, { abi: 'thiscall', mask: [0] });                   // Surface_drawSprite
    calleeStand(fx, 8, 0x00473f60, 6, 0, { abi: 'thiscall', mask: [0, 1] });                // Surface_stretchBlit
    calleeStand(fx, 9, 0x00476310, 5, 0, { abi: 'thiscall' });                              // Widget_setQuad
    calleeStand(fx, 10, 0x00476700, 6, 0, { abi: 'thiscall', str: { 2: 18 } });             // drawStringAt
    calleeStand(fx, 11, 0x00476750, 6, 0, { abi: 'thiscall', str: { 2: 18 } });             // drawStringAt2
    calleeStand(fx, 12, 0x00473e60, 5, 0, { abi: 'thiscall', mask: [0] });                  // Image_draw
    calleeStand(fx, 13, 0x00480c80, 2, 0, { abi: 'thiscall' });                             // Window::refreshFull
    calleeStand(fx, 14, 0x00483d30, 0, 0, {});                                              // enterModal483d30
    // View_toLocal(this, &x, &y): the stub writes the vector's scripted cursor position; the two out-pointers are
    // frame addresses (masked) and the written pair is logged instead.
    calleeStand(fx, 15, 0x0047ab50, 3, function (n, a) {
      const p = viewPoint(n);
      ptr(a[1]).writeS32(p[0]); ptr(a[2]).writeS32(p[1]);
      return 0;
    }, { abi: 'thiscall', mask: [0, 1, 2], extra: function (a, n) { return viewPoint(n); } });
    calleeStand(fx, 16, 0x00466b70, 4, 0, {});                                              // loadStoryStrings
    calleeStand(fx, 17, 0x00473470, 0, 0, {});                                              // drawScoreSummary
    calleeStand(fx, 18, 0x00448200, 1, 0, {});                                              // startSoundSlot
    calleeStand(fx, 19, 0x00473ae0, 1, 0, { abi: 'thiscall', mask: [0] });                  // Surface_free
    // Image_dtor must be stood in too: Image_ctor is, so the frame's Image object is never built and the real
    // destructor faults on it (a read of 0x31cb in the first probe run).
    calleeStand(fx, 20, 0x00474c40, 1, function (n, a) { return a[0]; }, { abi: 'thiscall', mask: [0] });
  }

  // Scenes: [stats, events, holes, money, flags, warn, memberCount, script, viewScript]
  // The modal script: one frame per loop iteration (pumpMessages is the only visit of the loop); a press1 sets
  // [0x00822d68] and ends the loop through the running flag, a key makes flushRedraw return 1 and ends it there.
  // The button's y is 0x28 + the first Surface's +0x1c (20) plus 8, raised by 24 for every line the screen drew, so a
  // scene with no highlight line and no warning puts the rect at [68, 112) and one with two lines at [116, 160).
  function press(x, y) { return [{ k: 'press1', x: x, y: y }]; }
  function keyEsc() { return [{ k: 'key', a: 0x1b, code: 0x1b }]; }

  const CR_SCENES = [
    // 0: every statistic up, no events, money positive (no warning), two hovers then a click
    { st: [[10, 20], [5, 9], [100, 250], [3, 8]], ev: [], holes: [], money: 5000, flags: 0, warn: 0, memb: 8,
      script: [[], [], [], press(0x230, 100)], view: [[0, 0], [0x230, 100], [0x230, 100], [0, 0]] },
    // 1: every statistic down, money negative with the counter at 0 (the first warning pair)
    { st: [[20, 10], [9, 5], [250, 100], [8, 3]], ev: [], holes: [], money: -5000, flags: 0, warn: 0, memb: 3,
      script: [[], press(0x230, 100)], view: [[0, 0], [0x230, 100]] },
    // 2: nothing changed, the counter at 1 (the second warning pair), a hole-opened event
    { st: [[10, 10], [7, 7], [100, 100], [4, 4]], ev: [[12, 0x20 | 3]], holes: [3], money: -1, flags: 0, warn: 1, memb: 4,
      script: [[], [], press(0x230, 100)], view: [[0, 0], [0, 0], [0x230, 100]] },
    // 3: the built / won / top-100 / top-18 arms, money positive
    { st: [[1, 2], [2, 1], [3, 3], [4, 5]], ev: [[10, 0x40 | 2], [12, 0x60 | 5], [14, 0x80 | 7], [16, 0xa0 | 9]],
      holes: [], money: 10, flags: 0, warn: 0, memb: 5,
      script: [[], press(0x230, 100)], view: [[0, 0], [0x230, 100]] },
    // 4: the home / tournament / board / land arms
    { st: [[0, 0], [0, 1], [1, 0], [2, 2]], ev: [[10, 0xc0 | 4], [13, 0xe0 | 6], [15, 0x100], [17, 0x140]],
      holes: [], money: 1, flags: 0, warn: 0, memb: 2,
      script: [[], press(0x230, 100)], view: [[0, 0], [0x230, 100]] },
    // 5: the story and landmark arms (loadStoryStrings is a stand-in, landmarkName runs for real)
    { st: [[5, 5], [1, 9], [9, 1], [0, 7]], ev: [[11, 0x120 | 2], [16, 0x160 | 3]], holes: [], money: 2, flags: 0,
      warn: 0, memb: 7, script: [[], press(0x230, 100)], view: [[0, 0], [0x230, 100]] },
    // 6: a hole-opened event whose hole record byte is zero: the line is dropped (0x0044dc5f)
    { st: [[3, 4], [4, 3], [5, 5], [6, 6]], ev: [[10, 0x20 | 5], [14, 0x20 | 6]], holes: [6], money: 3, flags: 0,
      warn: 0, memb: 6, script: [[], press(0x230, 100)], view: [[0, 0], [0x230, 100]] },
    // 7: the counter at 2 (the termination wording), money negative
    { st: [[9, 8], [8, 9], [7, 7], [1, 1]], ev: [], holes: [], money: -99999, flags: 0, warn: 2, memb: 1,
      script: [[], press(0x230, 100)], view: [[0, 0], [0x230, 100]] },
    // 8: the counter at 3: no warning line is drawn, the counter still rises (0x0044e078)
    { st: [[2, 3], [3, 2], [4, 4], [5, 6]], ev: [], holes: [], money: -1, flags: 0, warn: 3, memb: 6,
      script: [[], press(0x230, 100)], view: [[0, 0], [0x230, 100]] },
    // 9: money negative but bit 0x01000000 of the flag word set: no warning, the counter is cleared
    { st: [[1, 1], [2, 2], [3, 4], [5, 5]], ev: [], holes: [], money: -50, flags: 0x01000000, warn: 2, memb: 5,
      script: [[], press(0x230, 100)], view: [[0, 0], [0x230, 100]] },
    // 10: the loop leaves through flushRedraw instead of the click (a key event sets the key words)
    { st: [[4, 6], [6, 4], [8, 8], [2, 9]], ev: [[18, 0x20 | 1]], holes: [1], money: 7, flags: 0, warn: 0, memb: 9,
      script: [[], [], keyEsc()], view: [[0, 0], [0x230, 100], [0, 0]] },
    // 11: an event word whose kind bits are zero (the default arm: only the date prefix is drawn), and a hover that
    // enters and leaves the button rect before the click
    { st: [[7, 7], [5, 4], [2, 3], [9, 9]], ev: [[10, 0x1f], [12, 0x10]], holes: [], money: 4, flags: 0, warn: 0,
      memb: 3, script: [[], [], [], [], press(0x230, 126)],
      view: [[0x230, 126], [0, 0], [0x230, 126], [0x100, 126], [0x230, 126]] },
    // 12: the month wraps (502 % 500 = 2 against (502 - 8) % 500 = 494), so last year's slot is above this year's and
    // the highlights loop is skipped altogether (0x0044daf9); the money is high enough for addScore to place the
    // course, so drawScoreSummary runs (0x0044e511); the cursor is scripted to the right of and below the button rect
    { month: 502, st: [[1, 2], [3, 4], [5, 6], [7, 8]], ev: [], holes: [], money: 2000000, flags: 0, warn: 0, memb: 4,
      script: [[], [], [], press(0x230, 100)],
      view: [[0x300, 100], [0x230, 300], [0, 0], [0x230, 100]] },
    // 13: an event kind above 0x160 (the second switch's default arm, 0x0044de19) next to a known one
    { st: [[2, 1], [4, 3], [6, 5], [8, 7]], ev: [[11, 0x180 | 2], [15, 0x140]], holes: [], money: 11, flags: 0,
      warn: 0, memb: 2, script: [[], press(0x230, 150)], view: [[0x300, 300], [0x230, 150]] },
    // 14: used only by the negative control: the cursor sits exactly on the button's right edge (0x252), which the
    // original leaves outside (0x0044e479 is a jge)
    { st: [[1, 2], [2, 3], [3, 4], [4, 5]], ev: [], holes: [], money: 12, flags: 0, warn: 0, memb: 5,
      script: [[], [], press(0x230, 100)], view: [[0x252, 100], [0x252, 100], [0x230, 100]] }
  ];

  function crApply(sc) {
    const month = sc.month === undefined ? MONTH : sc.month;
    const ti = month % 500, li = (month - 8) % 500;
    CR_SAVE.forEach(function (s) { saveRange(s[0], s[1]); });
    wr32(0x00834170, month * 1024);          // the game date (0x0044d257)
    wr16(0x005a6d3c, 7);                     // the year word (0x0044d2c2)
    wr16(0x0058b388, sc.memb);               // the live membership count (0x0044d904)
    wr32(0x00519d40, 0x80003412);            // the button's quad colour (0x0044d1f9)
    wr32(0x00571fd4, sc.money);              // the money the board judges (0x0044e026) and addScore reads
    wr32(0x0059e7b8, sc.flags);              // the flag word (0x0044e033)
    wr32(0x005a6374, sc.warn);               // the warning counter (0x0044e058)
    stats(li, ti, sc.st[0], sc.st[1], sc.st[2], sc.st[3]);
    events(sc.ev);
    holes(sc.holes);
    fillNames();
    // addScore 0x004732d0 runs for real at the end of the loop: seed its table and inputs as the scoreboard key does
    table(function (i) { return defaultRow(i); });
    globals({ id: 7, cash: sc.money, skill: 60, fun: 50, level: 2 });
  }

  function crScenes(fx, M) {
    CR_SCENES.forEach(function (sc, i) {
      CR_VIEW[i] = sc.view;
      Object.defineProperty(fx, 'c' + i, { enumerable: true, configurable: true, get: function () {
        crCur = i;
        crApply(sc);
        M.select(sc.script);
        return 0;
      } });
    });
  }

  // ---- negative controls (the build of log/c3/alt/c3er_negctl_build.ps1): word 2 of the exported C3erNegCtl switch
  // picks which single wrong detail is live, so one build proves each of them on its own. In the normal build there is
  // no such export and these keys read the same as their main keys.
  function negSel(k) {
    (__freeze.negCtlWords() || []).forEach(function (a) { __freeze.save(a.add(8), 4); a.add(8).writeU32(k); });
  }

  F.c3er_scores_neg = function () {
    const fx = {};
    fx.clog = newClog();
    ssStands(fx);
    [1, 2, 3, 4, 5].forEach(function (k, i) {
      Object.defineProperty(fx, 'n' + i, { enumerable: true, configurable: true, get: function () {
        negSel(k);
        SS_SCENES[0](fx);     // the full table with row 0 selected: every edit changes something in it
        // edit 3 is (w + 4) / 2 against (w + 3) / 2: those differ only for an even run width, and the stand-in
        // returns 7 * length + 3, so the selected row's name needs an odd length
        if (k === 3) wrStr(G_NAME, 'C3ERNAME9', 0x40);
        return 0;
      } });
    });
    return fx;
  };

  F.c3er_career = function () {
    const fx = {};
    fx.clog = newClog(0x4000);
    // the pumps, before the freeze; 400 calls is far above the ~20 a scripted run makes
    const M = __modal({ maxCalls: 400, maxFrames: 400 });
    // the press handlers read GetAsyncKeyState and the models the clock: both through the import stand-in
    __importStandIn({ module: 'golf_clean.exe', imports: M.imports() });
    crStands(fx);            // freezes the first thread
    M.arm();                 // input globals saved and set neutral
    crScenes(fx, M);
    return Object.assign(fx, M.fx());
  };

  // one wrong detail per vector: (selector, the scene that shows it)
  const CR_NEG = [[6, 2], [7, 10], [8, 14], [9, 1]];

  F.c3er_career_neg = function () {
    const fx = {};
    fx.clog = newClog(0x4000);
    const M = __modal({ maxCalls: 400, maxFrames: 400 });
    __importStandIn({ module: 'golf_clean.exe', imports: M.imports() });
    crStands(fx);
    M.arm();
    CR_NEG.forEach(function (p, i) {
      const sc = CR_SCENES[p[1]];
      CR_VIEW[p[1]] = sc.view;
      Object.defineProperty(fx, 'm' + i, { enumerable: true, configurable: true, get: function () {
        negSel(p[0]);
        crCur = p[1];
        crApply(sc);
        M.select(sc.script);
        return 0;
      } });
    });
    return Object.assign(fx, M.fx());
  };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
