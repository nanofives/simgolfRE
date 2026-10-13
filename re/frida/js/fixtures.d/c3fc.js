// Fixtures for batch c3fc (round 33, 2026-10-13): golf_clean.exe size-only rows over 2 KB.
// Reimplementations: shim/src/re/c3fc.cpp. Registry: re/frida/registry.d/c3fc.py.
// Purposes and per-jcc tables: log/c3/c3fc_purpose.md. Notes: log/c3/c3fc_notes.txt.
//
// Method (helpers copied from c3er.js, which copied c3ea.js / c3ec.js):
// - Every key freezes the game's first thread (re/frida/js/freeze.js) and saves every global it writes; the flush
//   restores them newest first before the resume. negOn() arms the negative-control build's exported C3fcNegCtl switch
//   right after the freeze; word 2 of it selects which single wrong detail is live.
// - Argument-free __cdecl functions get one unused cdecl stack word "$sN": diff_hook resolves it before the state
//   snapshot, so the getter of fx.sN applies scene N.
// - Callees that reach live objects, draw, load or play are stood in on the A/B thread only (calleeStand): each call
//   is counted, folded into a running FNV-1a hash and logged with the text of every string argument; the log ($clog)
//   is a compared state region.
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
    __freeze.negOn();
  }

  function wr32(a, v) { ptr(a).writeU32(v >>> 0); }
  function wr16(a, v) { ptr(a).writeU16(v & 0xffff); }
  function wr8(a, v) { ptr(a).writeU8(v & 0xff); }
  function saveRange(a, n) { __freeze.save(ptr(a), n); }
  function wrStr(a, s, max) {
    const p = ptr(a);
    for (let i = 0; i < max; i++) p.add(i).writeU8(i < s.length ? s.charCodeAt(i) & 0xff : 0);
  }

  // ---- calleeStand: log word 0 = cursor, words 1..32 = call count of stub k, word 0x84 = FNV-1a over every call,
  // words 0x88 / 0x8c = scene words the stubs may read (compared memory), entries {va, count, values...} from +0x90.
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x4000;
  const CHASH = 0x84, CSEED = 0x88, CHEAD = 0x90;
  function newClog(cap) { const l = alloc(cap || CLOG); l.writeU32(CHEAD); l.add(CHASH).writeU32(0x811c9dc5); return l; }
  // a scene getter starts its vector with an empty log: the stubs' call counters index the scene's scripts, and
  // diff_hook carries the log from one vector to the next otherwise
  function resetClog(l, cap) {
    for (let i = 0; i < cap; i += 4) l.add(i).writeU32(0);
    l.writeU32(CHEAD);
    l.add(CHASH).writeU32(0x811c9dc5);
  }
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

  // negative-control selector: word 2 of every exported NegCtl switch (only the negctl build has one)
  function negSel(k) {
    (__freeze.negCtlWords() || []).forEach(function (a) { __freeze.save(a.add(8), 4); a.add(8).writeU32(k); });
  }

  // ====================================================================================================================
  // loadSoundTable 0x00448220
  // ====================================================================================================================
  // 219 calls of Snd::applyMode484e30 0x00484e30 on the live sound-slot objects (0x0080d840 + 0x6c * i): the stand-in
  // keeps them away from the slots' virtual methods (slot 0x88 / 0x8c of each object) and logs (object, path text,
  // mode). Its result, which loadSoundTable returns from the last call, is a hash of the call index, the object, the
  // mode and the scene's seed (log word 0x88, compared memory), so a different call order, object or mode moves the
  // return as well as the log.
  function sndStands(fx) {
    calleeStand(fx, 0, 0x00484e30, 3, function (n, a) {
      const seed = fx.clog.add(CSEED).readU32();
      if (seed === 0) return 0;
      let h = fnvWord(seed, n);
      h = fnvWord(h, a[0]); h = fnvWord(h, a[2]);
      return h;
    }, { abi: 'thiscall', str: { 1: 13 } });
  }
  const SND_SEEDS = [0, 1, 0x12345678, 0xdeadbeef, 0x7fffffff, 0x80000000, 0xffffffff, 0x00010001, 0x0badf00d,
                     0x31415926, 0x27182818, 0x5a5a5a5a];
  F.c3fc_sound = function () {
    const fx = {};
    fx.clog = newClog(0x6000);
    fx.clogCap = 0x6000;
    sndStands(fx);          // freezes the first thread
    SND_SEEDS.forEach(function (s, i) {
      Object.defineProperty(fx, 's' + i, { enumerable: true, configurable: true, get: function () {
        resetClog(fx.clog, fx.clogCap);
        fx.clog.add(CSEED).writeU32(s);
        return 0;
      } });
    });
    return fx;
  };
  // the negctl keys: vector i selects wrong detail i + 1 (seed 1, so the return moves too)
  F.c3fc_sound_neg = function () {
    const fx = F.c3fc_sound();
    [1, 2, 3, 4].forEach(function (k, i) {
      Object.defineProperty(fx, 'n' + i, { enumerable: true, configurable: true, get: function () {
        negSel(k);
        resetClog(fx.clog, fx.clogCap);
        fx.clog.add(CSEED).writeU32(0x12345678);
        return 0;
      } });
    });
    return fx;
  };

  // ====================================================================================================================
  // loadGolferRoster 0x004659a0
  // ====================================================================================================================
  // Stand-ins (A/B thread only): loadThemeFile 0x00437fa0 (logs name / index / slot; for index 0x4c it writes the
  // golfer record fields the *.pro loop copies: the name at 0x004e06d8, bytes 0x004e06eb..0x004e06ef and the ten bytes
  // at 0x0057e1a8, all derived from its call index and the scene seed), listMatchingFiles 0x0043d2a0 (logs the
  // pattern; clears the 0x2710-byte table at 0x0080b130 as the real one does at 0x0043d2a7 and writes the scene's names
  // for that call), scrollTextWindow 0x0046de70 (logs the text), and the static CRT's fopen 0x004a614d / fgets 0x004a65ee
  // / fclose 0x004a609f over two fixture file blocks {cursor +0, length +4, bytes +8} (c3dz's file model: fgets keeps
  // the CRT contract, up to n - 1 characters, stopping after '\n', NUL-terminated, NULL at the end). strstr 0x004a64f0
  // and strtok 0x004a678b run for real.
  const GR_FILE = 0x3800;
  const GR_SAVE = [
    [0x0051a068, 0x800], [0x0053a454, 0x1388], [0x0080b130, 0x2710], [0x00838220, 0x880], [0x0055d738, 0xe74],
    [0x0058dd50, 0x15e0], [0x0059e7b8, 4], [0x004e06c8, 0x230], [0x0057e1a8, 0x10], [0x00567328, 0x20]
  ];
  function putFile(blk, text) {
    wr32(blk, 0);
    wr32(blk.add(4), text.length);
    for (let i = 0; i < text.length; i++) blk.add(8 + i).writeU8(text.charCodeAt(i) & 0xff);
    blk.add(8 + text.length).writeU8(0);
  }
  let grCur = null;
  function grStands(fx) {
    calleeStand(fx, 0, 0x00437fa0, 3, function (n, a) {
      if ((a[1] | 0) === 0x4c) {
        const seed = fx.clog.add(CSEED).readU32();
        wrStr(0x004e06d8, 'C3FC-PRO-' + n + '-' + (seed & 0xff), 0x13);
        wr8(0x004e06eb, (0x85 + 0x3b * n + seed) & 0xff);
        wr8(0x004e06ec, (0x11 * n + 3) & 0xff);
        wr8(0x004e06ed, (0x23 * n + 0xf0) & 0xff);
        wr8(0x004e06ef, (7 * n + seed) & 0xff);
        for (let i = 0; i < 10; i++) wr8(0x0057e1a8 + i, (0x1d * n + 0x31 * i + (seed >>> 8)) & 0xff);
      }
      return grCur && grCur.themeFail ? 0 : 1;
    }, { str: { 0: 8 } });
    calleeStand(fx, 1, 0x0043d2a0, 3, function (n, a) {
      const lists = grCur.lists;
      const l = lists[Math.min(n, lists.length - 1)];
      const t = ptr(0x0080b130);
      for (let i = 0; i < 0x2710; i += 4) t.add(i).writeU32(0);
      if (typeof l === 'number') return l;
      l.forEach(function (s, i) { wrStr(0x0080b130 + 0x64 * i, s, 0x64); });
      return l.length;
    }, { str: { 0: 12, 1: 1, 2: 1 } });
    calleeStand(fx, 2, 0x0046de70, 3, 0, { str: { 0: 24 } });
    calleeStand(fx, 3, 0x004a614d, 2, function (n, a) {
      const o = grCur.open[Math.min(n, grCur.open.length - 1)];
      return o === 'A' ? fx.fileA.toUInt32() : (o === 'B' ? fx.fileB.toUInt32() : 0);
    }, { str: { 0: 12, 1: 1 } });
    calleeStand(fx, 4, 0x004a65ee, 3, function (n, a) {
      const buf = ptr(a[0]), lim = a[1] | 0, f = ptr(a[2]);
      let c = f.readU32();
      const len = f.add(4).readU32();
      if (c >= len || lim <= 1) return 0;
      let w = 0;
      while (w < lim - 1 && c < len) {
        const ch = f.add(8 + c).readU8();
        buf.add(w).writeU8(ch);
        w++; c++;
        if (ch === 10) break;
      }
      buf.add(w).writeU8(0);
      f.writeU32(c);
      return a[0];
    }, { mask: [0] });
    calleeStand(fx, 5, 0x004a609f, 1, 0, {});
  }

  // Line builders. A celebrity line has six comma fields (name, a letter field read as (c - 'A') % 11, a '1'..'3' field,
  // three digit fields); a pro line seven (name, (c - '0') % 8, (c - '0') % 4, three digit fields, ten skill characters).
  const ALPHA2 = 'ABCDEFGHIJKZaz09~!';
  const ALPHA3 = '0123456x/';
  const ALPHAD = '0123456789-A~';
  const SKILLS = ['0123456789', 'ABCDEFGHIJ', 'abcdefghij', 'KLMNOPQRST', 'z9Z0aA!~ \x7f', 'UVWXYZuvwx', '5',
                  '', 'Q1w2E3r4T5', '\xc0\xff\x80@[`{0aZ'];
  function celebLine(i, k) {
    k = k || 0;
    return 'Celeb' + i + '-' + 'xyz'.substr(0, (i + k) % 4) + ',' + ALPHA2[(i + k) % ALPHA2.length] +
      ALPHA2[(i * 3) % ALPHA2.length] + ',' + ALPHA3[(i + 2 * k) % ALPHA3.length] + ',' +
      ALPHAD[(i + k) % ALPHAD.length] + ',' + ALPHAD[(2 * i + k) % ALPHAD.length] + ',' +
      ALPHAD[(3 * i + 5 + k) % ALPHAD.length] + 'q\n';
  }
  function proLine(i, k) {
    k = k || 0;
    return 'Pro' + i + '-' + 'abcd'.substr(0, (i + k) % 5) + ',' + ALPHAD[(i + k) % ALPHAD.length] + ',' +
      ALPHAD[(2 * i + 1 + k) % ALPHAD.length] + ',' + ALPHAD[(3 * i + k) % ALPHAD.length] + ',' +
      ALPHAD[(5 * i + 2) % ALPHAD.length] + ',' + ALPHAD[(7 * i + k) % ALPHAD.length] + ',' +
      SKILLS[(i + k) % SKILLS.length] + 'q\n';
  }
  function lines(f, n, k) { let s = ''; for (let i = 0; i < n; i++) s += f(i, k); return s; }
  const NOISE = '*a comment line, ignored,1,2,3,4\n' + 'short\n' + '12345678\n' + '*\n';
  const TXT = ['Alpha.txt', 'C3FC-OpeningDay.txt', 'Gamma.txt'];
  const TXT_NO = ['Alpha.txt', 'Beta.txt'];
  const PRO = ['One.pro', 'Two.pro', 'Three.pro'];

  // Scenes: flags = the flag word's bit 0x10000000 before the call; lists = listMatchingFiles' answers in call order;
  // open = fopen's answers in call order ('A' celebrity block, 'B' pro block, 0 = NULL); celeb / pro = the file texts;
  // day = the sentinel in [0x00838a9c]; theme = the theme-name string 0x00567328.
  const GR_SCENES = [
    // 0: the theme's lists, both files open first time, mixed lines, three .pro golfers after the pros
    { flags: 0, lists: [TXT, PRO], open: ['A', 'B'], celeb: NOISE + lines(celebLine, 12, 0), pro: NOISE + lines(proLine, 10, 0) },
    // 1: an empty theme list (fallback + bit set), both celebrity opens fail, the pro file on the second try
    { flags: 0, lists: [[], ['Std-OpeningDay.txt'], PRO], open: [0, 0, 0, 'B'], celeb: '', pro: lines(proLine, 6, 1) },
    // 2: no slot holds the day string (sentinel kept), celebrities on the second try, both pro opens fail, no .pro
    { flags: 0x10000000, lists: [TXT_NO, []], open: [0, 'A', 0, 0], celeb: lines(celebLine, 9, 2), pro: '' },
    // 3: 104 celebrity lines: the table stops at 100 records (0x00465e23); the pro file is empty
    { flags: 0, lists: [TXT, ['Solo.pro']], open: ['A', 'B'], celeb: lines(celebLine, 104, 3), pro: '' },
    // 4: 103 pro lines: the table stops at 100 (0x004661f3); listMatchingFiles answers 0 for .pro
    { flags: 0, lists: [TXT, 0], open: ['A', 'B'], celeb: '', pro: lines(proLine, 103, 4) },
    // 5: both files empty, a negative .pro count (the signed `jle` 0x004662b1 returns it)
    { flags: 0x10000000, lists: [TXT, -1], open: ['A', 'B'], celeb: '', pro: '' },
    // 6: only comment and short lines, and a last line without its newline (its last character is cut)
    { flags: 0, lists: [TXT, PRO], open: ['A', 'B'], celeb: NOISE + NOISE + 'Last,Bq,2,7,8,9x', pro: NOISE + 'LastPro,1,2,3,4,5,0123456789x' },
    // 7: every field alphabet position, both files, 40 .pro golfers appended after 30 pros
    { flags: 0, lists: [TXT, (function () { const r = []; for (let i = 0; i < 40; i++) r.push('G' + i + '.pro'); return r; })()],
      open: ['A', 'B'], celeb: lines(celebLine, 30, 7), pro: lines(proLine, 30, 7) },
    // 8: the day string in the last of the 100 slots, celebrities fail, pros fail, .pro list after zero pros
    { flags: 0, lists: [(function () { const r = []; for (let i = 0; i < 99; i++) r.push('T' + i + '.txt'); r.push('Z-OpeningDay'); return r; })(), PRO],
      open: [0, 0, 0, 0], celeb: '', pro: '' },
    // 9: a theme name of ours in every path, long names (the record name field takes the whole first field)
    { flags: 0, theme: 'C3FC-THEME', lists: [TXT, PRO], open: ['A', 'B'],
      celeb: 'A-very-long-celebrity-name-over-32,Cx,1,2,3,4q\n' + lines(celebLine, 5, 9),
      pro: 'A-very-long-pro-golfer-name-of-40-chars-,7,7,7,7,7,ZZZZZZZZZZq\n' + lines(proLine, 5, 9) },
    // 10: the empty theme list and the fallback list empty too (bit set, nothing found), celebrity file on the second try
    { flags: 0, lists: [[], [], PRO], open: [0, 'A', 'B'], celeb: lines(celebLine, 20, 10), pro: lines(proLine, 3, 10) },
    // 11: the skill field shorter than ten characters reads the text buffer's older bytes (the buffer is state)
    { flags: 0, lists: [TXT, ['X.pro']], open: ['A', 'B'], celeb: lines(celebLine, 2, 11),
      pro: 'Sh1,1,1,1,1,1,9q\n' + 'Sh2,2,2,2,2,2,q\n' + 'Sh3,3,3,3,3,3,AbCq\n' + lines(proLine, 4, 11) }
  ];
  function grApply(fx, sc, i) {
    grCur = sc;
    resetClog(fx.clog, fx.clogCap);
    GR_SAVE.forEach(function (s) { saveRange(s[0], s[1]); });
    const v = ptr(0x0059e7b8).readU32();
    wr32(0x0059e7b8, (v & ~0x10000000) | sc.flags);
    wrStr(0x00567328, sc.theme === undefined ? '' : sc.theme, 0x20);
    wr32(0x00838a9c, 0x5a5a0000 + i);
    for (let k = 0; k < 0x800; k += 4) wr32(0x0051a068 + k, 0x2e2e2e2e);       // the text buffer: '.' bytes
    wrStr(0x0051a068, 'pre' + i + '-ABCDEFGHIJKLMNOP', 0x20);
    putFile(fx.fileA, sc.celeb);
    putFile(fx.fileB, sc.pro);
    fx.clog.add(CSEED).writeU32(0x1234 * (i + 1));
  }
  // strtok 0x004a678b keeps its position in the CRT's per-thread data, which the static CRT allocates on a thread's
  // first use (calloc under the CRT heap lock). The first thread can be frozen inside that heap (it is golf_clean.exe
  // code, a safe point for __freeze), so a first strtok on the A/B thread while it is frozen can wait on the lock for
  // ever (the second coverage boot hung that way). One strtok before the freeze allocates it.
  let strtokWarm = false;
  function warmStrtok() {
    if (strtokWarm) return;
    const b = alloc(16);
    wrStr(b.toUInt32(), 'a,b', 8);
    new NativeFunction(ptr(0x004a678b), 'pointer', ['pointer', 'pointer'], 'mscdecl')(b, ptr(0x004c76a8));
    strtokWarm = true;
  }
  F.c3fc_roster = function () {
    warmStrtok();
    const fx = {};
    fx.clog = newClog(0x8000);
    fx.clogCap = 0x8000;
    fx.fileA = alloc(GR_FILE);
    fx.fileB = alloc(GR_FILE);
    grStands(fx);           // freezes the first thread
    GR_SCENES.forEach(function (sc, i) {
      Object.defineProperty(fx, 's' + i, { enumerable: true, configurable: true, get: function () {
        grApply(fx, sc, i);
        return 0;
      } });
    });
    return fx;
  };
  // negctl keys: vector i selects wrong detail GR_NEG[i][0] on scene GR_NEG[i][1]
  const GR_NEG = [[11, 0], [12, 7], [13, 7], [14, 3], [15, 7], [16, 1]];
  F.c3fc_roster_neg = function () {
    const fx = F.c3fc_roster();
    GR_NEG.forEach(function (p, i) {
      Object.defineProperty(fx, 'n' + i, { enumerable: true, configurable: true, get: function () {
        negSel(p[0]);
        grApply(fx, GR_SCENES[p[1]], p[1]);
        return 0;
      } });
    });
    return fx;
  };

  // ====================================================================================================================
  // drawEmployeeDetailPanel 0x00436e50
  // ====================================================================================================================
  // Stand-ins: Surface_stretchBlit 0x00473f60, Surface_drawScaled 0x00473cb0, hitEmployee436b00 0x00436b00 (answers
  // the scene's hit), drawCircle 0x00407c60 (also logs [0x004c2844], which the panel sets around the call), addSprite
  // 0x004628d0, drawCenteredText 0x00404b70 (logs the text), setDrawTarget 0x004762d0, hireEmployee 0x0040aa80 (C2;
  // answers the scene's value), playSound 0x004481b0, drawPanelBackdrop 0x00432620 (logs the text buffer it draws).
  // Real: cellAt 0x0043d6f0 (cell tables seeded), pickGolferName 0x00467600 (every record has its own name at +0x2a),
  // appendDate 0x0040d7b0, appendNumber 0x0042dc00, __itoa 0x004ad425.
  const WALK = 0x00585850, WSTR = 0x4c;          // the 64 walker records; the panel reads +0x10.. of each
  const EP_SAVE = [
    [WALK, 0x1300], [0x0051a068, 0x200], [0x005a9f6c, 0x10], [0x0058a528, 0x20], [0x005a6d3c, 4],
    [0x005aa554, 4], [0x005aa558, 4], [0x005aa5b8, 0x40], [0x005aa6c0, 8], [0x004c7aec, 4], [0x004c2844, 4],
    [0x005a9f54, 4], [0x0059ca54, 4], [0x005685f4, 4], [0x004c2e10, 4], [0x005a34e8, 4], [0x00834170, 4],
    [0x0053f3e8 + 4 * 0x200, 4 * 0x30], [0x005a9370 + 4 * 0x200, 4 * 0x30]
  ];
  let epCur = null;
  function epStands(fx) {
    calleeStand(fx, 0, 0x00473f60, 6, 0, { abi: 'thiscall' });
    calleeStand(fx, 1, 0x00473cb0, 8, 0, { abi: 'thiscall' });
    calleeStand(fx, 2, 0x00436b00, 2, function () { return epCur.hit; }, {});
    calleeStand(fx, 3, 0x00407c60, 4, 0, { extra: function () { return [ptr(0x004c2844).readU32()]; } });
    calleeStand(fx, 4, 0x004628d0, 8, 0, {});
    calleeStand(fx, 5, 0x00404b70, 4, 0, { str: { 0: 12 } });
    calleeStand(fx, 6, 0x004762d0, 5, 0, { abi: 'thiscall' });
    calleeStand(fx, 7, 0x0040aa80, 0, function () { return epCur.hireRet === undefined ? 0x1111 : epCur.hireRet; }, {});
    calleeStand(fx, 8, 0x004481b0, 5, function () { return 0x2222; }, {});
    calleeStand(fx, 9, 0x00432620, 2, function () { return 0x3333; }, {
      extra: function () {
        const w = [], p = ptr(0x0051a068);
        for (let i = 0; i < 12; i++) w.push(p.add(4 * i).readU32());
        return w;
      } });
  }
  const TYPES = [-1, -2, -3, -4, -5, -6, 3, -7, -8, 0, -4, -2, -1, -5, -3];
  function walker(k, over) {
    const r = { x: k * 7 - 20, y: k * 5 - 30, flags: k % 4 === 0 ? 0 : (k % 3 === 1 ? 8 : 1), type: TYPES[k % TYPES.length],
                b16: (k * 13) & 0xff, phase: k * 37 - 300, mode: [10, 11, 12, 13, 12][k % 5], date: k * 100,
                wage: k * 7 - 50, num: k * 1111 - 20000, name: 'EMP' + k };
    if (over) Object.keys(over).forEach(function (q) { r[q] = over[q]; });
    const b = WALK + WSTR * k;
    for (let i = 0; i < WSTR; i += 4) wr32(b + i, 0x01010101 * ((k & 0x3f) + 1));
    wr8(b + 0x10, r.x); wr8(b + 0x11, r.y); wr8(b + 0x12, r.flags); wr8(b + 0x13, r.type); wr8(b + 0x16, r.b16);
    wr16(b + 0x1c, r.phase); wr16(b + 0x1e, r.mode); wr16(b + 0x22, r.date); wr16(b + 0x24, r.wage);
    wr16(b + 0x26, r.num); wrStr(b + 0x2a, r.name, 0x10);
  }
  // Scenes: hit = hitEmployee's answer; scroll / count = [0x005aa558] / [0x005aa6c0] before the call; sel =
  // [0x004c7aec]; slots = the previous slot table 0x005aa5b8; hired = [0x0059ca54]; lastHit / ticks = [0x005a9f54] /
  // [0x005aa554] (the tooltip); same = [0x004c2e10]; recs = per-record overrides; only = the records left eligible.
  const EP_SCENES = [
    { hit: -1, scroll: 0, count: 0, sel: -1, hired: 1, lastHit: -1, ticks: 0 },
    { hit: 2, scroll: 0, count: 3, sel: 0, hired: 0, lastHit: 2, ticks: 10 },
    { hit: 2, scroll: 4, count: 12, sel: 1, hired: 1, lastHit: 2, ticks: 10, same: 'sel' },
    { hit: 3, scroll: 0, count: 9, sel: 2, hired: 1, lastHit: 3, ticks: 10 },
    { hit: 3, scroll: 6, count: 2, sel: 3, hired: 0, lastHit: 0, ticks: 0 },
    { hit: -2, scroll: 1, count: 6, sel: 4, hired: 1, lastHit: -2, ticks: 10 },
    { hit: -3, scroll: 3, count: 20, sel: 5, hired: 0, lastHit: -3, ticks: 10 },
    { hit: -4, scroll: 0, count: 0, sel: 6, hired: 1, lastHit: -4, ticks: 10 },
    { hit: 0, scroll: 0, count: 0, sel: 7, hired: 1, lastHit: 0, ticks: 10 },
    { hit: 1, scroll: 2, count: 8, sel: 0, hired: 1, lastHit: 1, ticks: 10, recs: { 2: { type: -4, flags: 8 } } },
    { hit: 4, scroll: 0, count: 0, sel: 1, hired: 1, lastHit: 4, ticks: 10, same: 'sel' },
    { hit: 5, scroll: 0, count: 0, sel: 2, hired: 1, lastHit: 5, ticks: 9 },
    { hit: 6, scroll: 0, count: 0, sel: 3, hired: 1, lastHit: 6, ticks: 10 },
    { hit: 7, scroll: 0, count: 0, sel: 0, hired: 1, lastHit: 7, ticks: 10 },
    { hit: 8, scroll: 0, count: 0, sel: 1, hired: 1, lastHit: 8, ticks: 10 },
    { hit: 15, scroll: 1, count: 0, sel: 2, hired: 1, lastHit: 1, ticks: 3 },
    // the selected slot's record has a zero flags byte after the list rebuild: the panel clears the selection
    { hit: 9, scroll: 0, count: 0, sel: 7, hired: 1, lastHit: 9, ticks: 0, only: [1, 2, 3], slots: [5, 6, 7, 8, 9, 10, 11, 12] },
    // fewer listed records than sel: the old slot entry (a type 3 / type -6 record) is used
    { hit: 6, scroll: 0, count: 0, sel: 5, hired: 1, lastHit: 6, ticks: 10, only: [1, 2], slots: [1, 2, 3, 4, 5, 6, 7, 8] },
    { hit: 5, scroll: 0, count: 0, sel: 4, hired: 1, lastHit: 5, ticks: 10, only: [1, 2], slots: [1, 2, 3, 4, 5, 6, 7, 8] },
    // a full list (20 or more listed): the `full` image and, with [0x0059ca54] = 0, the refusal sound
    { hit: -1, scroll: 0, count: 0, sel: 0, hired: 0, lastHit: 0, ticks: 0, all: true },
    { hit: 0, scroll: 9, count: 0, sel: 3, hired: 0, lastHit: 0, ticks: 30, all: true },
    // each colour arm of the circle and every role: the selected records' types -5..-2, -1, -7 and flags 8 / 1
    { hit: 4, scroll: 0, count: 0, sel: 0, hired: 1, lastHit: 2, ticks: 0, recs: { 1: { type: -5, flags: 8 } } },
    { hit: 4, scroll: 0, count: 0, sel: 0, hired: 1, lastHit: 2, ticks: 0, recs: { 1: { type: -3, flags: 1 } } },
    { hit: 4, scroll: 0, count: 0, sel: 0, hired: 1, lastHit: 2, ticks: 0, recs: { 1: { type: -1, flags: 9 } } },
    { hit: 4, scroll: 0, count: 0, sel: 0, hired: 1, lastHit: 2, ticks: 0, recs: { 1: { type: -7, flags: 2 } } },
    { hit: 4, scroll: 0, count: 0, sel: 0, hired: 1, lastHit: 2, ticks: 0, recs: { 1: { type: -4, flags: 1 } } },
    { hit: 4, scroll: 0, count: 0, sel: 0, hired: 1, lastHit: 2, ticks: 0, recs: { 1: { type: -2, flags: 8 } } },
    // the remaining role cases 3, 5 and 6 (jump table 0x00437858)
    { hit: 5, scroll: 0, count: 0, sel: 0, hired: 1, lastHit: 2, ticks: 0, recs: { 1: { type: -3, flags: 8 } } },
    { hit: 6, scroll: 0, count: 0, sel: 0, hired: 1, lastHit: 2, ticks: 0, recs: { 1: { type: -4, flags: 8 } } },
    { hit: 0, scroll: 0, count: 0, sel: 0, hired: 1, lastHit: 2, ticks: 0, recs: { 1: { type: -5, flags: 1 } } },
    // fewer than 20 listed with [0x0059ca54] = 0: hireEmployee runs and its result is returned
    { hit: -1, scroll: 0, count: 0, sel: -1, hired: 0, lastHit: 0, ticks: 0, only: [1, 2, 3, 7, 8] },
    { hit: 5, scroll: 2, count: 4, sel: 1, hired: 0, lastHit: 1, ticks: 0, only: [1, 2, 3, 7, 8, 10, 11], hireRet: 0x7f },
    // exactly 19 and exactly 20 listed: the two sides of the `full` threshold (0x00437696 / 0x00437703)
    { hit: 2, scroll: 1, count: 1, sel: 2, hired: 0, lastHit: 2, ticks: 10, hireRet: -5, firstN: 19 },
    { hit: 2, scroll: 1, count: 1, sel: 2, hired: 0, lastHit: 2, ticks: 10, hireRet: -5, firstN: 20 }
  ];
  function epApply(fx, sc, i) {
    epCur = sc;
    resetClog(fx.clog, fx.clogCap);
    EP_SAVE.forEach(function (s) { saveRange(s[0], s[1]); });
    for (let k = 0; k < 64; k++) {
      let over = (sc.recs && sc.recs[k]) || null;
      if (sc.only && sc.only.indexOf(k) < 0) over = Object.assign({}, over || {}, { type: 3 });
      if (sc.all) over = Object.assign({}, over || {}, { type: [-1, -2, -3, -4, -5][k % 5], flags: 1 + (k & 8) });
      if (sc.firstN !== undefined) over = k < sc.firstN ? { type: [-1, -2, -3, -4, -5][k % 5], flags: 1 + (k & 8) } : { type: 3 };
      walker(k, over);
    }
    // two records outside the eligible set for the stale-slot scenes: a type 3 and a type -6 record with flags set
    if (sc.only) { walker(5, { type: 3, flags: 1 }); walker(6, { type: -6, flags: 8 }); }
    wr32(0x005aa558, sc.scroll);
    wr32(0x005aa6c0, sc.count);
    wr32(0x005aa6c4, 0x77777777);
    wr32(0x004c7aec, sc.sel);
    const slots = sc.slots || [1, 2, 3, 7, 8, 10, 11, 12];
    for (let s = 0; s < 16; s++) wr32(0x005aa5b8 + 4 * s, slots[s % slots.length] + (s >= 8 ? 20 : 0));
    wr32(0x0059ca54, sc.hired);
    wr32(0x005a9f54, sc.lastHit);
    wr32(0x005aa554, sc.ticks);
    wr32(0x005685f4, 0x1234 + i);
    wr32(0x004c2e10, sc.same === 'sel' ? slots[sc.sel & 7] : 63);
    wr32(0x005a34e8, 0x55 + i);
    wr32(0x004c2844, 0x99);
    wr32(0x00834170, 1024 * (3 + i));
    for (let t = 0x200; t < 0x230; t++) {
      wr32(0x0053f3e8 + 4 * t, 1 + ((t * 5 + i) % 7));
      wr32(0x005a9370 + 4 * t, (t % 9 === 0) ? -1 : (t * 3) % 40);
    }
    for (let k = 0; k < 0x200; k += 4) wr32(0x0051a068 + k, 0x2d2d2d2d);
  }
  // negctl keys: [edit, scene]; scene objects are EP_SCENES entries or their own
  const EP_NEG = [
    [21, 4], [22, { hit: 3, scroll: 0, count: 8, sel: -1, hired: 1, lastHit: 0, ticks: 0 }], [23, 21], [24, 22],
    [25, 11], [26, 28],
    [27, { hit: -1, scroll: 0, count: 0, sel: -1, hired: 1, lastHit: 0, ticks: 0, recs: { 1: { mode: 12, phase: 0 } } }],
    [28, 16]
  ];
  F.c3fc_emp_neg = function () {
    const fx = F.c3fc_emp();
    EP_NEG.forEach(function (p, i) {
      const sc = typeof p[1] === 'number' ? EP_SCENES[p[1]] : p[1];
      const si = typeof p[1] === 'number' ? p[1] : 40 + i;
      Object.defineProperty(fx, 'n' + i, { enumerable: true, configurable: true, get: function () {
        negSel(p[0]);
        epApply(fx, sc, si);
        return 100 + 3 * si;
      } });
      Object.defineProperty(fx, 'm' + i, { enumerable: true, configurable: true, get: function () { return 400 + si; } });
    });
    return fx;
  };
  F.c3fc_emp = function () {
    const fx = {};
    fx.clog = newClog(0x6000);
    fx.clogCap = 0x6000;
    epStands(fx);           // freezes the first thread
    EP_SCENES.forEach(function (sc, i) {
      Object.defineProperty(fx, 'x' + i, { enumerable: true, configurable: true, get: function () {
        epApply(fx, sc, i);
        return sc.px === undefined ? 100 + 3 * i : sc.px;
      } });
      Object.defineProperty(fx, 'y' + i, { enumerable: true, configurable: true, get: function () { return 400 + i; } });
    });
    return fx;
  };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
