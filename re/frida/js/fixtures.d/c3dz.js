// Fixtures for batch c3dz (simulation plan phase 1, 2026-10-11): the golfer text chain. loadStoryStrings 0x00466b70,
// computeStoryReply 0x004669f0, advanceStory 0x00466370, emitGolferThought 0x00467a00, buildGolferComment 0x00469b00,
// announceVipGolfer 0x0045de80. Reimplementations: shim/src/re/c3dz.cpp. Registry: re/frida/registry.d/c3dz.py.
// Purposes and per-jcc tables: log/c3/c3dz_purpose.md.
//
// Method (copied from c3dy.js / c3dw.js and extended): every key freezes the game's first thread (re/frida/js/freeze.js);
// every live global a row writes is saved first and restored by the flush. Callees are replaced for ONE key on the A/B thread
// only (calleeStand): each call is appended to the call log fx.clog (compared state) and answered from the row. The story
// file is served by stand-ins of the static CRT's fopen 0x004a614d / fgets 0x004a65ee / fclose 0x004a609f: the "file" lives in
// the fixture block fx.file ({cursor, length, bytes}), which is compared state, so both arms read the same lines and the
// cursor is part of the snapshot. (The real CRT path would malloc a stream buffer and take the stream locks: it is not used.)
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
  function fillMem(a, n, b) { const arr = new Uint8Array(n); arr.fill(b & 0xff); ptr(a).writeByteArray(arr.buffer); }
  function saveRange(a, n) { __freeze.save(ptr(a), n); }
  function putStr(a, s) {
    const p = ptr(a);
    for (let k = 0; k < s.length; k++) p.add(k).writeU8(s.charCodeAt(k) & 0xff);
    p.add(s.length).writeU8(0);
  }

  // ---- rows: fx.r<i> applies row i (row.on) and returns row.a; fx.b<i>, fx.c<i>, fx.d<i>, fx.e<i> are the further arguments
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

  // ---- a function of the exe replaced for one key on the A/B thread only (c3dw/c3dy calleeStand, plus opts.mask: argument
  // indexes logged as 0 (stack pointers that differ between the arms), opts.str {ix: words}: the C string an argument points to
  // is logged (zero padded) instead of the pointer). Log layout: word 0 = cursor, word 1 + k = call count of stub k (16 stubs),
  // entries {va, count, values...} from +0x50.
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x1000;
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

  // ====================================================================================================================
  // The story file: fopen / fgets / fclose stand-ins over fx.file = {cursor +0, length +4, bytes +8}
  // ====================================================================================================================
  const FILE_CAP = 0x4000;
  function loadFile(fx, text) {
    wr32(fx.file, 0);
    wr32(fx.file.add(4), text.length);
    putStr(fx.file.add(8), text);
  }
  // fopen: logs the path (32 words) and the mode (1 word); answers NULL when the row says fopenFails, else the file block.
  // fgets(buf, n, f): the CRT's contract: up to n - 1 characters, stopping after a '\n', NUL-terminated; NULL at end of file.
  // fclose: answers the row's fcloseRet (default 0).
  function storyFileOn(fx, kOpen, kGets, kClose) {
    calleeStand(fx, kOpen, 0x004a614d, 2, function () {
      return cur.fopenFails ? 0 : fx.file.toUInt32();
    }, { str: { 0: 32, 1: 1 } });
    calleeStand(fx, kGets, 0x004a65ee, 3, function (n, a) {
      const buf = ptr(a[0]), lim = a[1] | 0, f = fx.file;
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
    calleeStand(fx, kClose, 0x004a609f, 1, function () {
      return cur.fcloseRet === undefined ? 0 : cur.fcloseRet;
    });
  }

  // Story-file names of ours: index `file` such that 0x0053a454 + 0x32 * file is a 50-byte slot of the block fx.names.
  function namesOn(fx, names) {
    const blk = alloc(0x40 + 50 * names.length);
    const base = blk.toUInt32();
    let o = 0;
    while (((base + o - 0x0053a454) % 50 + 50) % 50 !== 0) o++;
    const first = (base + o - 0x0053a454) / 50;
    names.forEach(function (s, i) { putStr(base + o + 50 * i, s); });
    fx.names = blk;
    return function (i) { return (first + i) | 0; };
  }

  // ---- the story cache: 8 entries of 0x10c bytes at 0x00838228 ({file, key, sel, text[0x100]}); slot word [0x00838220]
  const CACHE = 0x00838220, CACHE_N = 0x900;            // the slot word, the eight entries, [0x00838a94] and slack
  const BUF = 0x0051a068, BUF_N = 0x800;
  const FLAGS = 0x0059e7b8;                             // bit 0x10000000: the "Standard" theme
  function entry(i) { return 0x00838228 + 0x10c * i; }
  function cacheOn(row) {
    fillMem(entry(0), 0x10c * 8, 0x2e);
    for (let i = 0; i < 8; i++) { wr32(entry(i), 0x7f00 + i); wr32(entry(i) + 4, -2); wr32(entry(i) + 8, 0x7e00 + i); putStr(entry(i) + 0xc, 'slot' + i); }
    (row.cache || []).forEach(function (c) {
      wr32(entry(c[0]), c[1]); wr32(entry(c[0]) + 4, c[2]); wr32(entry(c[0]) + 8, c[3]); putStr(entry(c[0]) + 0xc, c[4]);
    });
    wr16(CACHE, row.slot === undefined ? 2 : row.slot);
    wr32(0x00838a94, 0x5a5a5a5a);
  }
  function bufOn(row) {
    fillMem(BUF, BUF_N, 0x55);
    putStr(BUF, row.pre === undefined ? 'pre|' : row.pre);
  }
  function flagsOn(row) {
    const v = ptr(FLAGS).readU32();
    wr32(FLAGS, row.standard ? (v | 0x10000000) : (v & ~0x10000000));
  }
  function textSaves() {
    saveRange(CACHE, CACHE_N);
    saveRange(BUF, BUF_N);
    saveRange(FLAGS, 4);
  }

  // ---- story files (our own synthetic lines). Headers are lines not starting with a space; their variants start with one.
  const FA = ' intro note\n' + 'Hone opening\n' + ' oa first\n' + ' ob second\n' + ' oc third\n' + 'Htwo second header\n' +
             ' ta\n' + ' tb\n' + '\n' + 'x\n' + 'Hfour\n' + ' fa only\n' + 'Hfive last';
  const FB = '\n' + 'q' + '\n' + ' lead space\n' + 'Hb1\n';                // a "\n" line, then "q\n" is two characters
  function many(n) { let s = ' s0\n'; for (let k = 1; k < n; k++) s += 'H' + k + '\n'; return s; }
  const LONG = 'L' + 'abcdefghij'.repeat(30) + '\n';                      // 302 characters: fgets returns it in two pieces
  const FC = ' pre\n' + LONG + ' after long\n' + 'Hnext\n' + ' ' + 'v'.repeat(260) + '\n' + 'Hend\n';
  const FD = ' p0\n' + 'H1\n' + ' a\n' + ' b\n' + ' c\n' + ' d\n' + 'H2\n';

  // ---- loadStoryStrings (file, key, sel, mode)
  F.c3dz_lss = function () {
    const fx = { clog: newClog(), clogCap: CLOG, file: alloc(FILE_CAP) };
    freezeNow();
    textSaves();
    const nm = namesOn(fx, ['stA.txt', 'storyB.txt', 'c.txt', 'longname_d.txt']);
    const A = nm(0), B = nm(1), C = nm(2), D = nm(3);
    const rows = [];
    const miss = function (o) { rows.push(Object.assign({ file: FA }, o)); };
    // misses read the file
    miss({ a: A, b: 1, c: 0, d: -1 });                    // header 1, countdown 1 -> 0: " ob" appended, sel -1 stops; cached
    miss({ a: A, b: 1, c: 1, d: -1, slot: 7 });          // " oa" appended at once; slot 7 -> 0
    miss({ a: A, b: 2, c: 0, d: 0 });                    // header 2, " ta" " tb" count down, "x" with mode 0: nothing appended
    miss({ a: A, b: 2, c: 0, d: 2 });                    // same, mode 2: the saved line " tb" is appended at "x"
    miss({ a: A, b: 2, c: 5, d: 1 });                    // countdown never meets sel: at "x" the saved line goes in (mode 1)
    miss({ a: A, b: 5, c: 9, d: 2 });                    // "Hfour" is header 4, "Hfive last" 5 (last character cut), end of file
    miss({ a: A, b: 4, c: 9, d: 2 });                    // header 4: " fa only" counts down, then header 5 appends the saved line
    miss({ a: A, b: 5, c: 9, d: 1 });                    // end of file with mode 1: nothing more
    miss({ a: A, b: 5, c: 9, d: -1, slot: 5 });          // end of file, mode -1: appends the saved line, cached in slot 5
    miss({ a: A, b: 9, c: 0, d: -1 });                   // no header 9: the whole file is read, the unchanged buffer is cached
    miss({ a: A, b: 0, c: 0, d: 3 });                    // key 0 never matches a header
    miss({ a: A, b: 3, c: 0, d: -1, pre: '' });          // header 3 is "x"; "Hfour" then appends the saved " tb" (empty prefix)
    miss({ a: A, b: -1, c: 0, d: -1 });                  // key -1: the first line of two or more characters is appended
    miss({ a: B, b: -1, c: 4, d: 0, file: FB, fcloseRet: 0xffffffff });   // "\n" skipped; "q\n" is the first line taken
    miss({ a: B, b: 1, c: 0, d: -1, file: FB });         // header 1 is "q"; end of file right after the variants
    miss({ a: C, b: 1, c: 0, d: -1, fopenFails: 1 });    // fopen NULL: returns 0, the slot keeps {file, -2, sel}
    miss({ a: C, b: 1, c: 0, d: 2, fopenFails: 1, standard: 1 });
    miss({ a: A, b: 1, c: 1, d: -1, standard: 1 });      // the "Standard" theme path
    miss({ a: D, b: 1, c: 0, d: 2, file: FC });          // header 1 is the first piece of the long line, header 2 its tail
    miss({ a: D, b: 2, c: 0, d: -1, file: FC });         // header 2 = the tail piece; " after long" is variant 0 ... countdown
    miss({ a: D, b: 3, c: 1, d: 0, file: FC });          // header 3: the long variant is cut at 249 characters, its tail is header 4
    miss({ a: D, b: 3, c: 2, d: 2, file: FC });
    miss({ a: A, b: 300, c: 0, d: -1, file: many(200) }); // exactly 200 counted lines read, then end of file: cached
    miss({ a: A, b: 300, c: 0, d: -1, file: many(201), fcloseRet: 7 });   // the 201st counted line closes: fclose's result
    miss({ a: A, b: 150, c: 0, d: 2, file: many(260) }); // header 149 found, then the next header appends the saved " s0"
    miss({ a: B, b: 1, c: 3, d: 2, file: FD });          // countdown 1 -> 0, -1, -2: sel 3 never met; "H2" appends the saved " d"
    miss({ a: B, b: 1, c: -2, d: 2, file: FD });         // sel -2: the countdown reaches -2 at " d"
    miss({ a: B, b: 1, c: 0, d: 2, file: FD, slot: 0 });
    miss({ a: B, b: 1, c: -1, d: -1, file: FD });        // sel -1 from the start: the header alone, then stop
    miss({ a: 1, b: 1, c: 0, d: -1, file: FD });         // a game file-name slot (index 1 of 0x0053a454), read only
    miss({ a: 2, b: 2, c: 0, d: 2, file: FD });
    miss({ a: A, b: 1, c: 0, d: -1, file: '' });         // an empty file: the first fgets is NULL
    miss({ a: A, b: 1, c: 0, d: 1 });                    // the selected variant with mode 1: not appended, sel -1 stops
    miss({ a: B, b: -1, c: -1, d: -1, file: '' });       // key -1, sel -1, empty file: end of file with sel -1, cached
    // partial matches are misses
    miss({ a: A, b: 1, c: 1, d: -1, cache: [[2, A, 1, 0, 'nope'], [4, A, 0, 1, 'nope2'], [6, B, 1, 1, 'nope3']] });
    // hits: the cached text is appended; no file is opened
    const hit = function (o) { rows.push(Object.assign({ file: FA }, o)); };
    hit({ a: A, b: 3, c: 4, d: -1, cache: [[3, A, 3, 4, 'abc\ndef']] });
    hit({ a: A, b: 3, c: 4, d: 1, cache: [[3, A, 3, 4, 'a\nb\n\nc']] });
    hit({ a: A, b: 3, c: 4, d: 1, cache: [[3, A, 3, 4, '']] });                         // nothing appended: returns len0
    hit({ a: A, b: 3, c: 4, d: 0, cache: [[3, A, 3, 4, 'one\ntwo\nthree']] });
    hit({ a: A, b: 3, c: 4, d: 2, cache: [[3, A, 3, 4, 'no newline']] });
    hit({ a: A, b: 3, c: 4, d: 5, cache: [[3, A, 3, 4, '']] });                         // nothing appended: returns mode
    hit({ a: A, b: 3, c: 4, d: 0, cache: [[0, A, 3, 4, '\n\n\nx']] });
    hit({ a: A, b: 3, c: 4, d: 2, cache: [[7, A, 3, 4, 'end\n']], pre: '' });
    hit({ a: A, b: 3, c: 4, d: 0x1234, cache: [[1, A, 3, 4, 'm\nn'], [5, A, 3, 4, 'second copy']], pre: 'longer prefix text ' });
    hit({ a: B, b: -2, c: 9, d: -1, cache: [[6, B, -2, 9, 'claimed slot']] });         // an entry still marked -2 matches key -2
    hit({ a: A, b: 3, c: 4, d: 0, cache: [[4, A, 3, 4, 'ab\ncdef\ng\nhijk\n']], pre: 'XYZ' });
    hit({ a: A, b: 3, c: 4, d: -7, cache: [[4, A, 3, 4, 'q\n']], pre: 'p' });
    if (rows.length !== LSS_N) throw new Error('c3dz_lss: row count ' + rows.length);
    rowsOn(fx, rows.map(function (r) {
      return Object.assign({}, r, { on: function () {
        bufOn(r); cacheOn(r); flagsOn(r); loadFile(fx, r.file);
      } });
    }));
    storyFileOn(fx, 0, 1, 2);
    return fx;
  };
  const LSS_N = 47;

  // ====================================================================================================================
  // Golfer records: 0x100 bytes each from GB = 0x005794b8. The keys use records 2..9 only (saved, filled, compared).
  // ====================================================================================================================
  const GB = 0x005794b8;
  const REC_LO = 2, REC_HI = 10;
  function rec(g) { return GB + 0x100 * g; }
  function recsSave() { saveRange(rec(REC_LO), 0x100 * (REC_HI - REC_LO)); }
  function recsFill(seed) {
    for (let g = REC_LO; g < REC_HI; g++) {
      const p = ptr(rec(g));
      for (let k = 0; k < 0x100; k += 4) p.add(k).writeU32((Math.imul(seed + g * 0x101 + k, 0x9e3779b1) >>> 0) & 0x7f7f7f7f);
    }
  }
  const REC_STATE = [rec(REC_LO), 0, 0x100 * (REC_HI - REC_LO)];

  // ---- computeStoryReply (file, key, hole, g). Row: g, hole, key, file; o = the other record (g ^ 1)'s hole-state byte;
  // p = partner index (word +0xa2 of g); ph = partner's hole byte +0x21; ps = partner's hole-state byte +0x37 + hole; run = the
  // number of 0x13 bytes from partner +0x70 (then a different byte); mood +0xa4, cnt +0xb4 (shorts); t = the top two bits of the
  // word at partner +0x88 + 2 * min(run, 4); hq = holeQuadrants' answer; lret = loadStoryStrings' answer (stand-in key).
  function csrRows() {
    const rows = [];
    const mk = function (o) {
      return Object.assign({ g: 4, hole: 3, key: 6, file: 0x21, o: 0x2a, p: 7, ph: 3, ps: 0, run: 0, mood: 9, cnt: 1, t: 0, hq: 0, lret: 0x77 }, o);
    };
    rows.push(mk({ ph: 5 }));                                   // partner on another hole: r from the other record's byte
    rows.push(mk({ ph: 3, ps: 9 }));                            // partner shown this hole already
    rows.push(mk({ ph: -3 & 0xff, hole: -3 & 0xff, ps: 1 }));  // signed hole byte: 0xfd != 253
    rows.push(mk({ o: 0xff, key: 3 }));                         // 0xff >> 3 = 31 clamped to 3
    rows.push(mk({ o: 0x07, key: 3, ph: 1 }));                  // 0 stays 0
    rows.push(mk({ o: 0x40, key: -2, ph: 1 }));                 // key negative: clamp(8, 0, -2)
    // the computed arm
    [0, 1, 2, 3, 4, 5, 7].forEach(function (run) { rows.push(mk({ run: run, mood: 20 + run, cnt: 3 })); });
    [[0, 0], [1, 0], [2, 0], [3, 0], [9, 2], [40, 10], [-5, 0], [120, 50], [99, 120], [7, 7]].forEach(function (mc) {
      rows.push(mk({ mood: mc[0], cnt: mc[1], key: 20 }));     // v = (clamp(mood, cnt, 99) - 1) / 2, key large
    });
    [[1, 50], [2, 50], [3, 50], [6, 0], [6, 9], [6, 13], [0, 30], [-1, 30]].forEach(function (km) {
      rows.push(mk({ key: km[0], mood: km[1], cnt: 0 }));      // the key - 1 cap (key >= 2) and its absence
    });
    [0x0000, 0x4000, 0x8000, 0xc000].forEach(function (t) {
      [1, 2, 3, 5, 11].forEach(function (m) { rows.push(mk({ t: t, mood: m, cnt: 0, key: 9 })); });   // v = 0, 0, 1, 2, 5
    });
    rows.push(mk({ t: 0x4000 | 0x123, run: 2, mood: 9 }));     // only the top two bits count
    rows.push(mk({ t: 0xc000 | 0x3fff, run: 4, mood: 15 }));
    rows.push(mk({ hq: 1, mood: 3 }));                          // holeQuadrants non-zero: v = key
    rows.push(mk({ hq: 0x80000000, key: 4 }));
    rows.push(mk({ hq: 1, key: -1 }));                          // v = key = -1, clamp(-1, 0, -1)
    rows.push(mk({ hq: 2, key: 0 }));
    rows.push(mk({ g: 5, p: 4, hole: 0, ph: 0, mood: 30 }));    // g odd: the other record is 4
    rows.push(mk({ g: 8, p: 9, hole: 6, ph: 6, o: 0x31, mood: 50, cnt: 40, t: 0x8000, run: 1 }));
    rows.push(mk({ g: 3, p: 3, hole: 2, ph: 2, mood: 25 }));    // partner = itself
    rows.push(mk({ g: 6, p: 7, hole: 1, ph: 1, mood: 70, key: 4, t: 0xc000 }));
    rows.push(mk({ lret: 0xffffffff, ph: 0 }));
    return rows;
  }
  function csrOn(fx, r) {
    recsFill(0x51);
    const own = rec(r.g), other = rec(r.g ^ 1), pr = rec(r.p);
    wr8(other + 0x37 + (r.hole & 0xff), r.o);
    wr16(own + 0xa2, r.p);
    wr8(pr + 0x21, r.ph);
    wr8(pr + 0x37 + (r.hole & 0xff), r.ps);
    for (let k = 0; k < 8; k++) wr8(pr + 0x70 + k, k < r.run ? 0x13 : 0x12 + k);
    wr16(pr + 0xa4, r.mood);
    wr16(pr + 0xb4, r.cnt);
    const n = Math.min(r.run, 4);
    for (let k = 0; k < 6; k++) wr16(pr + 0x88 + 2 * k, k === n ? r.t : (0x1111 * (k + 1)) & 0x3fff | 0x8000);
    wr32(pr + 0x00, 0x30 + r.g);
    wr32(pr + 0x04, 0x40 + r.hole);
    wr32(0x00838a94, 0x5a5a5a5a);
  }
  const CSR_N = 62;
  function csrScene(chain) {
    return function () {
      const fx = { clog: newClog(), clogCap: CLOG, file: alloc(FILE_CAP) };
      freezeNow();
      recsSave();
      saveRange(0x00838a94, 4);
      if (chain) textSaves();
      const rows = csrRows();
      if (rows.length !== CSR_N) throw new Error('c3dz_csr: row count ' + rows.length);
      const nm = chain ? namesOn(fx, ['reply.txt']) : null;
      rowsOn(fx, rows.map(function (r) {
        const file = chain ? nm(0) : r.file;
        return Object.assign({}, r, { a: file, b: r.key, c: r.hole, d: r.g, fcloseRet: 0, on: function () {
          csrOn(fx, r);
          if (chain) {
            bufOn({ pre: '' });
            cacheOn({ slot: r.g & 7, cache: (r.g === 5) ? [[1, file, r.key, 3, 'cached reply\nline']] : [] });
            loadFile(fx, FD + ' e\n f\n g\n h\n i\n j\nH3\n k\n');
          }
        } });
      }));
      calleeStand(fx, 0, 0x00407000, 3, function () { return cur.hq || 0; });
      if (chain) storyFileOn(fx, 1, 2, 3);
      else calleeStand(fx, 1, 0x00466b70, 4, function () { return cur.lret; });
      return fx;
    };
  }
  F.c3dz_csr = csrScene(false);
  F.c3dz_csrc = csrScene(true);

  // ---- type-flag table 0x004d60a8 (stride 0x230, read only): the first types (< 32) with bit 1 set and clear
  function typesByBit(bit) {
    const set = [], clr = [];
    for (let t = 0; t < 32; t++) ((ptr(0x004d60a8 + 0x230 * t).readU8() & bit) ? set : clr).push(t);
    return { set: set, clr: clr };
  }
  const TYPETAB = 0x005849e0, TYPETAB_N = 0x600;      // 44-byte rows written by advanceStory at +0x29 and +3 + hole

  // ---- advanceStory (g, force). Row: g, force, p (partner), story (+0xb0), hole (+0x21), b22, stage (+0xb2), ps (partner stage),
  // cnt (+0xb4), mood (+0xa4), ty / pty (type words +0xb6: 's' = a type with bit 1 of 0x004d60a8 set, 'c' = clear), xy / pxy,
  // st (storyText answers by call), q ([0x00838a94] written by each storyText call), t7 (typeBit7Clear answers), ang
  // (angleFixed's answer), mode ([0x005a34e0]).
  function advRows() {
    const rows = [];
    const mk = function (o) {
      return Object.assign({ g: 4, force: 0, p: 5, story: 6, hole: 3, b22: 0, stage: 1, ps: 1, cnt: 40, mood: 10, ty: 'c', pty: 'c',
        xy: [0x1400, 0x2800], pxy: [0x1c00, 0x2000], st: [1, 1], q: [2, 3], t7: [1, 0, 1], ang: 0x30000000, mode: 0 }, o);
    };
    rows.push(mk({ story: -1 }));                                     // no story: 0
    rows.push(mk({ hole: 1, b22: 0 }));                               // force 0, hole <= 1, byte +0x22 clear: 0
    rows.push(mk({ hole: -2, b22: 0 }));
    rows.push(mk({ hole: 1, b22: 7 }));                               // byte +0x22 set: goes on
    rows.push(mk({ hole: 0, b22: 0, force: 1 }));                     // forced
    rows.push(mk({ stage: 4, ps: 4 }));                               // both stages above 3: 0
    rows.push(mk({ stage: 5, ps: 9, force: 1 }));
    rows.push(mk({ stage: 4, ps: 3, cnt: 40 }));                      // partner at 3: goes on
    rows.push(mk({ stage: 2, cnt: 12 }));                             // counter 12 <= 12, force 0: 0
    rows.push(mk({ stage: 2, cnt: 13 }));                             // 13 > 12: goes on
    rows.push(mk({ stage: 2, cnt: 12, force: 1 }));
    rows.push(mk({ stage: -3, cnt: -8 }));                            // 4 * -3 + 4 = -8: 0
    // storyText answers
    [[0, 0], [1, 0], [0, 1], [5, 9]].forEach(function (st) { rows.push(mk({ st: st, stage: 1, ps: 3 })); });
    // the second storyText non-zero, partner stage not below: stage == 1 picks a sound by mode; odd / even story
    [0, 1, 2, 3, 4, -1].forEach(function (m) {
      rows.push(mk({ stage: 0, ps: 2, mode: m, story: 6 }));
      rows.push(mk({ stage: 0, ps: 1, mode: m, story: 7 }));
    });
    rows.push(mk({ stage: 2, ps: 3, ty: 's', pty: 'c' }));            // type flag bit 1 on g only
    rows.push(mk({ stage: 2, ps: 3, ty: 'c', pty: 's' }));
    rows.push(mk({ stage: 2, ps: 3, ty: 's', pty: 's' }));
    // partner stage below the new stage: equal to stage - 1, or lower (mood drops)
    rows.push(mk({ stage: 2, ps: 2 }));
    rows.push(mk({ stage: 2, ps: 1 }));
    rows.push(mk({ stage: 3, ps: -4 }));
    rows.push(mk({ stage: 2, ps: 2, st: [0, 1] }));
    // the tail: partner stage below lowers stage and counter by trunc(c / 3)
    [40, 41, 42, -7, -9, 0, 0x7fff].forEach(function (c) { rows.push(mk({ stage: 2, ps: 0, cnt: c, force: 1 })); });
    // stage 4 with the partner at 4: sound by mode, logTick, type-table bits, LandmarkAvailableNotice
    [0, 1, 2, 3, 5, -2].forEach(function (m) { rows.push(mk({ stage: 3, ps: 4, mode: m, ty: 'c', pty: 's', hole: 2 + (m & 7) })); });
    rows.push(mk({ stage: 3, ps: 4, st: [0, 0], hole: 5 }));          // both storyTexts 0: stage 4 = partner's
    rows.push(mk({ stage: 3, ps: 5, st: [0, 0] }));                   // stage 4, partner 5: result 1, no sound
    rows.push(mk({ stage: 4, ps: 3, st: [0, 0], force: 1 }));         // stage 5, partner 3: lowered
    // facing from angleFixed's top nibble
    for (let n = 0; n < 16; n++) rows.push(mk({ ang: (n << 28) | (0x01234567 * n & 0x0fffffff), st: [0, 0], ps: 3, stage: 1 }));
    rows.push(mk({ g: 3, p: 2, st: [1, 1], q: [0x7fffffff, 1], stage: 0, ps: 1 }));   // g odd: g ^ 1 = 2
    rows.push(mk({ g: 6, p: 9, st: [3, 4], q: [-1, -2], stage: 1, ps: 6, mood: -5 })); // partner not g ^ 1
    rows.push(mk({ g: 8, p: 9, story: 0x123, xy: [-50, 70], pxy: [80, -90], stage: 3, ps: 4, mode: 1 }));
    return rows;
  }
  const ADV_N = 70;
  F.c3dz_adv = function () {
    const fx = { clog: newClog(), clogCap: CLOG };
    freezeNow();
    recsSave();
    saveRange(TYPETAB, TYPETAB_N);
    saveRange(0x0051a068, 4);
    [0x0059b048, 0x00561258, 0x00838a94].forEach(function (a) { saveRange(a, 4); });
    saveRange(0x005a34e0, 1);
    const tb = typesByBit(2);
    if (!tb.set.length || !tb.clr.length) throw new Error('c3dz_adv: no type with / without bit 1 below 32');
    const rows = advRows();
    if (rows.length !== ADV_N) throw new Error('c3dz_adv: row count ' + rows.length);
    rowsOn(fx, rows.map(function (r) {
      return Object.assign({}, r, { a: r.g, b: r.force, on: function () {
        recsFill(0x77);
        fillMem(TYPETAB, TYPETAB_N, 0x10);
        const a = rec(r.g), pa = rec(r.p);
        wr16(a + 0xb0, r.story); wr8(a + 0x21, r.hole); wr8(a + 0x22, r.b22);
        wr16(a + 0xb2, r.stage); wr16(pa + 0xb2, r.ps); wr16(a + 0xb4, r.cnt); wr16(a + 0xa4, r.mood);
        wr16(a + 0xa2, r.p);
        wr16(a + 0xb6, r.ty === 's' ? tb.set[0] : tb.clr[0]);
        wr16(pa + 0xb6, r.pty === 's' ? tb.set[tb.set.length > 1 ? 1 : 0] : tb.clr[tb.clr.length > 1 ? 1 : 0]);
        wr32(a + 0, r.xy[0]); wr32(a + 4, r.xy[1]); wr32(pa + 0, r.pxy[0]); wr32(pa + 4, r.pxy[1]);
        wr8(0x005a34e0, r.mode);
        wr32(0x00838a94, 0x11111111);
        wr32(0x0059b048, 0x22222222);
        wr32(0x00561258, 0x40);
        wr8(0x0051a068, 0x41);
      } });
    }));
    calleeStand(fx, 0, 0x004668f0, 3, function (n) {
      wr32(0x00838a94, cur.q[Math.min(n, cur.q.length - 1)]);
      return cur.st[Math.min(n, cur.st.length - 1)];
    });
    calleeStand(fx, 1, 0x00467a00, 3, 0);
    calleeStand(fx, 2, 0x0046c940, 1, function (n) { return cur.t7[Math.min(n, cur.t7.length - 1)]; });
    calleeStand(fx, 3, 0x0040c500, 4, 0x31);
    calleeStand(fx, 4, 0x004481b0, 5, 0x32);
    calleeStand(fx, 5, 0x0040c6f0, 2, 0);
    calleeStand(fx, 6, 0x004722c0, 2, 0);
    calleeStand(fx, 7, 0x004672d0, 2, function () { return cur.ang; });
    return fx;
  };

  // ====================================================================================================================
  // announceVipGolfer (p1, p2): the whole golfer table (records 0..151 and the slot 0x98 at 0x00582cb8) is saved, filled with a
  // pattern whose hole bytes +0x21 are 0, and compared. The story-file name table 0x0053a454 (100 names of 50 bytes, read by the
  // draw) gets synthetic eight-letter codes. Random::range is stood in (thiscall; row.rng by call, then (37 n + 11) mod arg).
  // ====================================================================================================================
  const GT = GB, GT_N = 0x9900;
  const NAMES = 0x0053a450, NAMES_N = 4 + 50 * 100;
  const VIPG = [0x0056a51c, 0x00572cac, 0x00543ccc, 0x00838a9c, 0x00834170, 0x00822c88, 0x00541cd8, 0x0059ae78, 0x0059bf90,
                0x0059aaf8, 0x00543cfc, 0x00561258];
  function typeFlagsAll(n) { const f = []; for (let t = 0; t < n; t++) f.push(ptr(0x004d60a8 + 0x230 * t).readU8()); return f; }
  function popc(x) { let c = 0; for (; x; x &= x - 1) c++; return c; }
  // a pair of types (both below 0x4c) whose flag bytes differ in exactly d of the low five bits, the first matching `want(t1)`
  function typePair(f, d, want1, want2) {
    for (let a = 0; a < 0x4c; a++) for (let b = 0; b < 0x4c; b++)
      if (popc((f[a] ^ f[b]) & 31) === d && (!want1 || want1(f[a])) && (!want2 || want2(f[b]))) return [a, b];
    return null;
  }
  function vipRows(f) {
    const rows = [];
    const NEUTRAL = 'xGECXGEC';
    const mk = function (o) {
      return Object.assign({ p1: 4, p2: 8, d: 3, names: [NEUTRAL], rng: [2, 0, 17], a9c: 99, tb: {}, sc: {}, sm: [1], fl1: 0x00, fl2: 0x00,
        c81: 0x00000000, c82: 0x00000000, d0: 0, w16: 0, date: 0x1234, ccc: 0, c88: 2, cd8: 5000, ae78: 77, cac: 9, ec0: 0, bf90: 1,
        crs0: 0, crs3: 0, a450: 1, aaf8: 1, cfc: 5, s1258: 4, vipAll: 0, others: [], d0p1: 0x41 }, o);
    };
    // the draw: accepted at once (neutral letters), the [0x00838a9c] retry, empty names, all 100 draws used up (three attempts)
    rows.push(mk({}));
    rows.push(mk({ rng: [1, 0, 7, 7, 31], a9c: 7 }));
    rows.push(mk({ names: ['', '', NEUTRAL], rng: [3, 0, 3, 4, 5] }));
    rows.push(mk({ names: [''], rng: [] }));
    rows.push(mk({ names: [''], rng: [], p1: 1, date: 0x7ff }));          // p1 < 2, date below 0x800: story [0x00838a9c] forced
    rows.push(mk({ p1: 1, date: 0x800 }));
    rows.push(mk({ p1: 0, date: -5 }));
    // a story another golfer already has (positive hole byte), and the same with its hole byte 0 / negative
    rows.push(mk({ others: [[20, 3, 17, 1]] }));
    rows.push(mk({ others: [[20, 0, 17, 1], [21, -2, 17, 1]] }));
    rows.push(mk({ rng: [0, 0, 17, 0, 18, 0, 19], others: [[20, 1, 17, 1], [33, 9, 18, 1], [151, 1, 19, 1]] }));   // all three taken
    // letters 1 and 5 (typeBit7Clear F / M / O / S) on both values, lower case too
    ['F', 'M', 'O', 'S', 'f', 's', 'Z'].forEach(function (c) {
      [[0, 0], [0, 1], [1, 0], [1, 1]].forEach(function (tb) {
        rows.push(mk({ names: ['x' + c + 'ECXGEC'], tb: { 4: tb[0], 8: tb[1] }, rng: [1, 0, 5, 0, 6, 0, 7] }));
        rows.push(mk({ names: ['xGECX' + c + 'EC'], tb: { 4: tb[0], 8: tb[1] }, rng: [1, 0, 5, 0, 6, 0, 7] }));
      });
    });
    // letters 2 and 6 (flag byte +0xb8)
    ['D', 'M', 'N', 'S', 'W', 'w', 'E'].forEach(function (c) {
      [0x00, 0x08, 0x10, 0x20, 0x40, 0x78].forEach(function (fl) {
        rows.push(mk({ names: ['xG' + c + 'CXGEC'], fl1: fl, rng: [1, 0, 5] }));
        rows.push(mk({ names: ['xGECXG' + c + 'C'], fl2: fl, rng: [1, 0, 5] }));
      });
    });
    // letters 3 and 7 (type flags): every letter against a type with and without its bit
    const bits = { T: 1, M: 1, O: 2, S: 2, A: 4, L: 4, P: 8, B: 8, N: 0x10, G: 0x10 };
    Object.keys(bits).concat(['C', 'q']).forEach(function (c) {
      const b = bits[c.toUpperCase()] || 1;
      [true, false].forEach(function (has) {
        const pr1 = typePair(f, 0, function (x) { return ((x & b) !== 0) === has; });
        rows.push(mk({ names: ['xGE' + c + 'XGEC'], ty: pr1, rng: [1, 0, 5] }));
        const pr2 = typePair(f, 0, null, function (x) { return ((x & b) !== 0) === has; });
        rows.push(mk({ names: ['xGECXGE' + c], ty: pr2, rng: [1, 0, 5] }));
      });
    });
    // letter 4 (golferScore O / S / Y) around each bound
    [['O', 50, 40], ['O', 50, 41], ['O', 49, 40], ['S', 30, 20], ['S', 30, 40], ['S', 30, 19], ['S', -30, -40], ['S', 31, 41], ['S', 0, 0],
     ['Y', 30, 40], ['Y', 31, 40], ['Y', 29, 40], ['o', 60, 1], ['y', 1, 60], ['X', 0, 99]].forEach(function (s) {
      rows.push(mk({ names: ['xGEC' + s[0] + 'GEC'], sc: { 4: s[1], 8: s[2] }, rng: [1, 0, 5] }));
    });
    // the tail: p2's bit 0x20000 skips the VIP part (and the record-5 mood), mask scan, rating texts, VIP kinds by k = (p2 / 2) % 6
    rows.push(mk({ c82: 0x20000, crs0: 3 }));
    rows.push(mk({ c82: 0x20000, crs0: 3, d0: 0x41, w16: 0x0a5a }));       // byte +0x18 kept from before: twelve-bit random fill
    rows.push(mk({ crs0: 3 }));                                            // the slot 0x98 mood
    rows.push(mk({ c88: 1 }));                                             // itoa text
    rows.push(mk({ c88: -4, ae78: -12345 }));
    [2, 3].forEach(function (p2) {                                         // k = 1: the CEO
      rows.push(mk({ p2: p2, ec0: 1, sm: [1], cac: 3 }));
      rows.push(mk({ p2: p2, ec0: 1, sm: [0], cac: 3 }));
      rows.push(mk({ p2: p2, ec0: 0, cac: 3 }));
      rows.push(mk({ p2: p2, ec0: 1, sm: [1], cac: 7, d0p1: 0 }));
      rows.push(mk({ p2: p2, ec0: 1, cac: 8 }));
      rows.push(mk({ p2: p2, ec0: 1, cac: -1, c88: 0 }));
      rows.push(mk({ p2: p2, ec0: 1, cac: 2, others: [[30, 2, 5, 0x4e]] }));   // a type-0x4e golfer on the course: mask bit 2
      rows.push(mk({ p2: p2, ec0: 1, cac: 2, vipAll: 1 }));
    });
    [6, 7].forEach(function (p2) {                                         // k = 3: the commissioner
      rows.push(mk({ p2: p2, cd8: 1000, a450: 3, c88: 2, sm: [1] }));       // 1000 is not above (3 + 2) * (2 + 2) * 50
      rows.push(mk({ p2: p2, cd8: 1001, a450: 3, c88: 2, sm: [1] }));
      rows.push(mk({ p2: p2, cd8: 1001, a450: 3, c88: 2, sm: [0] }));
      rows.push(mk({ p2: p2, cd8: 9999, crs3: 2 }));
      rows.push(mk({ p2: p2, cd8: 9999, others: [[40, 1, 66, 0x4d]] }));    // mask bit 1
      rows.push(mk({ p2: p2, c88: 0, ae78: 400, a450: 2 }));               // the itoa rating: 400 is not above 4 * 2 * 50
      rows.push(mk({ p2: p2, c88: 0, ae78: 401, a450: 2 }));
    });
    [10, 11].forEach(function (p2) {                                       // k = 5: the heiress
      rows.push(mk({ p2: p2, cd8: 100, aaf8: 1 }));                        // 100 is not above 4 * 25
      rows.push(mk({ p2: p2, cd8: 101, aaf8: 1, sm: [1] }));
      rows.push(mk({ p2: p2, cd8: 101, aaf8: 1, sm: [0] }));
      rows.push(mk({ p2: p2, cd8: 9999, cfc: 0xffff }));
      rows.push(mk({ p2: p2, cd8: 9999, cfc: 0xfffe, others: [[41, 4, 66, 0x4e]] }));   // mask bit 2
    });
    // compat 0..5 and the words +0xae
    for (let d = 0; d <= 5; d++) {
      const pr = typePair(f, d);
      [[1, 1], [1, 0]].forEach(function (tb) {
        rows.push(mk({ ty: pr, tb: { 4: tb[0], 8: tb[1] }, fl1: 0x10, fl2: 0x10 }));
        rows.push(mk({ ty: pr, tb: { 4: tb[0], 8: tb[1] }, fl1: 0x10, fl2: 0x00 }));
      });
    }
    // the roll Random::range([0x00561258]) above 6 with / without p1's bit 0x20000, byte +0x38 of p1 or p2
    [6, 7, 0x10006].forEach(function (roll) {
      [0, 0x20000, 0x100000].forEach(function (c81) { rows.push(mk({ rng: [1, 0, 17, roll], c81: c81, c82: 0x20000, s1258: 9 })); });
    });
    // the date bit 6: emitGolferThought twice, the +10 / +8 bytes; eax
    [0x40, 0x41, 0xbf, 0x7c0].forEach(function (dt) { rows.push(mk({ date: dt, p1: 5, p2: 8 })); });
    rows.push(mk({ date: 0x40, p1: 9, p2: 4 }));
    rows.push(mk({ date: 0x40, p1: 2, p2: 3, ccc: 7 }));                   // [0x00543ccc] != 0 sets bit 0x10000
    [0, 0x41].forEach(function (d) { rows.push(mk({ d0p1: d, p1: 5, p2: 8 })); rows.push(mk({ d0p1: d, p1: 9, p2: 6, c82: 0x20000, d0: 0 })); });
    return rows;
  }
  function vipOn(fx, r, f) {
    fillMem(GT, GT_N, 0);
    for (let g = 0; g < 153; g++) {
      const a = ptr(GT + 0x100 * g);
      for (let k = 0; k < 0x100; k += 4) a.add(k).writeU32((Math.imul(g * 0x100 + k + 0x77, 0x2545f491) >>> 0) & 0x3f3f3f3f);
      a.add(0x21).writeU8(0);
    }
    const ty = r.ty || [3, 3];
    const r1 = rec(r.p1), r2 = rec(r.p2);
    wr16(r1 + 0xb6, ty[0]); wr16(r2 + 0xb6, ty[1]);
    wr8(r1 + 0xb8, r.fl1); wr8(r2 + 0xb8, r.fl2);
    wr32(r1 + 0x10, r.c81); wr32(r2 + 0x10, r.c82);
    wr8(r2 + 0x18, r.d0); wr16(r2 + 0x16, r.w16); wr8(r1 + 0x18, r.d0p1);
    r.others.forEach(function (o) { const a = rec(o[0]); wr8(a + 0x21, o[1]); wr16(a + 0xb0, o[2]); wr16(a + 0xb6, o[3]); });
    for (let i = 0; i < 100; i++) {
      const nm = r.names[i % r.names.length];
      fillMem(NAMES + 4 + 50 * i, 50, 0);
      putStr(NAMES + 4 + 50 * i, nm === '' ? '' : nm + i);
    }
    fillMem(TYPETAB, 0xe00, 0x20);
    fillMem(BUF, BUF_N, 0x55); putStr(BUF, 'pre|');
    fillMem(0x00824134, 0x20, 0x66);
    wr32(NAMES, r.a450);
    wr32(0x0056a51c, 0x300); wr32(0x00572cac, r.cac); wr32(0x00543ccc, r.ccc); wr32(0x00838a9c, r.a9c); wr32(0x00834170, r.date);
    wr32(0x00822c88, r.c88); wr32(0x00541cd8, r.cd8); wr32(0x0059ae78, r.ae78); wr32(0x0059bf90, r.bf90); wr32(0x0059aaf8, r.aaf8);
    wr32(0x00543cfc, r.cfc); wr32(0x00561258, r.s1258);
    const fl = ptr(FLAGS).readU32();
    wr32(FLAGS, r.vipAll ? (fl | 0x1000000) : (fl & ~0x1000000));
    for (let k = 0; k < 8; k++) wr8(0x00575ec0 + 0x410 * k, r.ec0);
    wr8(0x00571ff4 + 0x2e * r.bf90, r.crs0); wr8(0x00571ff7 + 0x2e * r.bf90, r.crs3);
  }
  const VIP_CLOG = 0x8000;
  F.c3dz_vip = function () {
    const fx = { clog: newClog(VIP_CLOG), clogCap: VIP_CLOG };
    freezeNow();
    saveRange(GT, GT_N); saveRange(NAMES, NAMES_N); saveRange(TYPETAB, 0xe00); saveRange(BUF, BUF_N); saveRange(0x00824134, 0x20);
    VIPG.forEach(function (a) { saveRange(a, 4); });
    saveRange(FLAGS, 4);
    for (let k = 0; k < 8; k++) saveRange(0x00575ec0 + 0x410 * k, 1);
    saveRange(0x00571ff4 + 0x2e, 4);
    const f = typeFlagsAll(0x4c);
    const rows = vipRows(f);
    rows.forEach(function (r, i) {
      if (r.ty === null) throw new Error('c3dz_vip: no type pair for row ' + i);
      if (r.bf90 !== 1) throw new Error('c3dz_vip: course index other than 1 is not saved');
    });
    fx.vipN = rows.length;
    rowsOn(fx, rows.map(function (r) {
      return Object.assign({}, r, { a: r.p1, b: r.p2, on: function () { vipOn(fx, r, f); } });
    }));
    calleeStand(fx, 0, 0x0045c1e0, 2, function (n, a) {
      if (cur.rng && n < cur.rng.length) return cur.rng[n];
      const m = a[1] & 0xffff;
      return m ? (Math.imul(n, 37) + 11) % m : 0;
    }, { abi: 'thiscall' });
    calleeStand(fx, 1, 0x004a6650, 1, function (n, a) {
      const c = a[0] | 0;
      return (c >= 0x61 && c <= 0x7a) ? c - 0x20 : c;
    });
    calleeStand(fx, 2, 0x0046c940, 1, function (n, a) {
      const g = a[0] | 0;
      return cur.tb[g] !== undefined ? cur.tb[g] : (g & 1);
    });
    calleeStand(fx, 3, 0x00453260, 1, function (n, a) {
      const g = a[0] | 0;
      return cur.sc[g] !== undefined ? cur.sc[g] : 20 + g;
    });
    calleeStand(fx, 4, 0x004676e0, 2, function (n, a) { const p = ptr(BUF); putStr(p.add(lenAt(p)), '<' + (a[0] | 0) + '>'); return 0; });
    calleeStand(fx, 5, 0x0040cb00, 3, function (n) { return cur.sm[Math.min(n, cur.sm.length - 1)]; });
    calleeStand(fx, 6, 0x0042dd50, 1, function (n, a) { const p = ptr(BUF); putStr(p.add(lenAt(p)), '$' + (a[0] | 0)); return 0; });
    calleeStand(fx, 7, 0x004481b0, 5, 0x33);
    calleeStand(fx, 8, 0x00467a00, 3, 0);
    return fx;
  };
  function lenAt(p) { let n = 0; while (p.add(n).readU8() !== 0) n++; return n; }

  // ====================================================================================================================
  // emitGolferThought (g, kind, arg). Records 2..9; the hole tables 0x00575ab0 + 0x208 * h (h < 20), the tile maps (0x0053caf0
  // words, 0x00578804 / 0x005a6378 / 0x0056c7e4 bytes, 50 x 50) and the per-kind words 0x00838da8 are saved, filled and compared.
  // buildGolferComment, buildStatusText, typeBit7Clear, playSoundAt, playSound, holeQuadrants, Random::range, decorationName,
  // buildGolferName and startMessage are stood in; sign 0x00467150 and clamp 0x00467130 run (C3 hooks; off in the callees-off run).
  // ====================================================================================================================
  const HOLES = 0x00575ab0, HOLES_N = 0x208 * 20;
  const TILEW = 0x0053caf0, TILEW_N = 5000;
  const TILE_B = [0x00578804, 0x005a6378, 0x0056c7e4];
  const KINDW = 0x00838da0, KINDW_N = 0x120;
  const EGT_G = [0x00543d04, 0x0059c08c, 0x00571d38, 0x0055e5ac, 0x00834170, 0x00543cf4, 0x00822c88, 0x005a59f8, 0x005409a8];
  function egtRows(prefix) {
    const rows = [];
    const mk = function (o) {
      return Object.assign({ g: 4, kind: 1, arg: 3, p: 5, sub: 2, hole: 3, d0: 0, ee: 0, c8: 0, ty: 3, fa6: 0, fa8: 0, faa: 0, x: 4 << 10,
        y: 5 << 10, sx: 300, sy: 200, hist: [9, 9, 9, 9, 9, 9, 9, 9, 9, 9], w8a: 0, pf: [0, 0], tb: { 4: 1, 5: 0 }, rng: [9], hq: 0,
        date: 2000, last: 100, seen: [0, 0], ae: 0, c88: 2, cf4: 0, a5a00: 4, sm: [1], a59f8: -1, tw: 0x0100, tt: 3, f2: 0, f4: 0,
        rem: 0, remD: 0, remF: 0, pHole: 0, p84: 0, pMood: 3, t3b: 0, mood: 0, w88: 0 }, o);
    };
    if (prefix === 'kinds') {
      // every kind 0..0x42 twice: a plain record and one with the pairing flag, byte +0x18, typeBit7Clear 0, the second flag set
      for (let k = 0; k <= 0x42; k++) {
        rows.push(mk({ kind: k, arg: k + 1 }));
        rows.push(mk({ kind: k, arg: (k * 7) % 40, pf: [1, 1], d0: 0x41, ee: -3, tb: { 4: 0, 5: 1 }, c8: 0x20000, fa6: 9, fa8: 9, faa: 0x40,
          c88: 0, t3b: 1, tt: 0x11, rng: [0] }));
      }
      return rows;
    }
    if (prefix === 'partner') {
      // the recursive path 0x00468664..0x0046868a: every row passes the partner test, so emitGolferThought(partner, 0x2f, 0x14) runs
      [[4, 5], [5, 4], [6, 7], [2, 9], [9, 3]].forEach(function (gp) {
        [9, 4, 0x24, 3, 0x1e].forEach(function (k, i) {
          rows.push(mk({ g: gp[0], p: gp[1], kind: k, arg: 7 + i, pHole: 1 + i, p84: 0, pMood: 1 + i, c88: 3, d0: i === 2 ? 0x40 : 0,
            pf: i === 3 ? [0, 1] : [0, 0], rng: [i], sm: [i & 1] }));
        });
      });
      rows.push(mk({ kind: 0xe, hist: [0, 0xe, 0, 0, 0, 0, 0, 0, 0, 0], pHole: 2, p84: 0, pMood: 2, c88: 3 }));   // delta -1 from history
      rows.push(mk({ kind: 9, pHole: 2, p84: 0, pMood: 2, c88: 3, date: 300 }));                                   // no tip either time
      return rows;
    }
    // guards
    rows.push(mk({ sub: 10 }));                                         // byte +0x22 above 9
    rows.push(mk({ sub: -5 }));
    rows.push(mk({ sub: 9 })); rows.push(mk({ sub: 8 }));     // the bound of the signed > 9 test
    rows.push(mk({ g: 0x98, kind: 1 }));                                // g >= 0x98 (its byte +0x22 is read first)
    rows.push(mk({ f2: 1 }));                                           // bit 0x2000000 of [0x0059e7b8]
    // kind 0x13 and its 0x17 remap: the hole table's count, bits 2 / 3 and the difference
    [[10, 4, 2], [10, 4, 1], [10, 8, -1], [10, 8, 0], [9, 4, 5], [12, 12, -3], [12, 0, 5]].forEach(function (c) {
      rows.push(mk({ kind: 0x13, rem: c[0], remF: c[1], remD: c[2] }));
    });
    rows.push(mk({ kind: 0x13, rem: 10, remF: 4, remD: 3, d0: 0x20 }));  // class 0x20 keeps 0x13
    rows.push(mk({ kind: 0x13, rem: 10, remF: 4, remD: 3, d0: 0x3f }));
    // kind 0x23 with the latest thought 0x23 / other
    rows.push(mk({ kind: 0x23, hist: [0x23, 1, 2, 3, 4, 5, 6, 7, 8, 9] }));
    rows.push(mk({ kind: 0x23, hist: [0x22, 0x23, 2, 3, 4, 5, 6, 7, 8, 9] }));
    // the pairing flag [0x005409a8] (record t = partner) and kind 0x28's restore
    rows.push(mk({ kind: 6, pf: [1, 0] }));
    rows.push(mk({ kind: 0x28, pf: [1, 1], d0: 1 }));
    rows.push(mk({ kind: 0x28, d0: 0 }));
    // byte +0x18: the +0x36 counter against sign(delta), kinds 2 / 3 / 0xc / 0xd, the [0x005a59f8] sound
    [[2, -1], [2, -5], [3, 0], [3, -1], [0xc, -2], [0xd, 1], [0xd, -9], [9, -9], [6, -1], [6, 1], [6, 0], [5, 4]].forEach(function (c) {
      rows.push(mk({ kind: c[0], d0: 0x21, ee: c[1] }));
    });
    rows.push(mk({ kind: 4, d0: 0x21, a59f8: 4, c8: 0x40000 }));
    rows.push(mk({ kind: 4, d0: 0x21, a59f8: 4, c8: 0 }));
    rows.push(mk({ kind: 1, d0: 0x21, a59f8: 4, c8: 0x40000 }));
    // delta -1 recomputed from the history (n = 3 / 5 / 10) and bit 15 of +0x8a; [0x00543cf4] 0 / 1 / 2
    [0, 1, 2, 3].forEach(function (c88) {
      [1, 2, 4, 9].forEach(function (slot) {
        const hist = [0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39];
        hist[slot] = 0xe;
        rows.push(mk({ kind: 0xe, c88: c88, hist: hist }));
      });
    });
    rows.push(mk({ kind: 0xe, w88: 0x8000 }));                         // bit 15 of the latest word moves to slot 1 (+0x8a)
    rows.push(mk({ kind: 0xe, w88: 0x7fff, w8a: 0x8000 }));
    rows.push(mk({ kind: 7, arg: 0 })); rows.push(mk({ kind: 7, arg: 0, tb: { 4: 0 }, c8: 0x20000 }));
    [0x12, 0x19, 0x1b].forEach(function (k) { rows.push(mk({ kind: k, ty: 'b1', fa6: 9, fa8: 9, faa: 0x40 })); });
    [0x33, 0x34, 0x35].forEach(function (k) { rows.push(mk({ kind: k, ty: 'b4', c88: 0 })); rows.push(mk({ kind: k, ty: 'b4', c88: 1 })); });
    rows.push(mk({ kind: 0xe, cf4: 1, hist: [0, 0xe, 0, 0, 0, 0, 0, 0, 0, 0] }));
    rows.push(mk({ kind: 0xe, cf4: 2 }));
    rows.push(mk({ kind: 0xe, d0: 0x61 }));
    // the partner thought: class 0x40 keeps delta, 0x20 blocks, the parity bound, kinds 0x2f / 0x15 / 0x18, the partner's fields
    [[9, 0x00], [9, 0x40], [9, 0x20], [4, 0x00], [0x24, 0x40], [0x2f, 0], [0x15, 0], [0x18, 0], [3, 0x41]].forEach(function (c) {
      rows.push(mk({ kind: c[0], d0: c[1], pHole: 2, p84: 0, pMood: 3, c88: 3 }));
    });
    [[0, 0, 3], [2, 1, 3], [2, 0, 0], [-1, 0, 3], [2, 0, -2]].forEach(function (c) {
      rows.push(mk({ kind: 9, pHole: c[0], p84: c[1], pMood: c[2], c88: 3 }));
    });
    [[2, 2], [3, 2], [2, 3], [1, 2]].forEach(function (c) { rows.push(mk({ kind: 9, c88: c[0], sub: c[1], pHole: 2, p84: 0, pMood: 3 })); });
    // the mood clamp
    [-10, -9, 9, 10, 100, -100].forEach(function (m) { rows.push(mk({ kind: m > 0 ? 6 : 9, mood: m })); });
    // the tile mark: roll vs [0x00822c88], bit 0x20000000, bit 0x4000000, kind 0x2f, holeQuadrants, the tile word and type
    [[0, 0, 0, 0, 0x0100, 3], [3, 0, 0, 0, 0x0100, 3], [3, 0x20000000, 0, 0, 0x0100, 3], [0, 0, 1, 0, 0x0100, 3], [0, 0, 0, 1, 0x0100, 3],
     [0, 0, 0, 0, 0x0400, 3], [0, 0, 0, 0, 0x0800, 3], [0, 0, 0, 0, 0x0100, 0x11], [2, 0, 0, 0, 0x3001, 7]].forEach(function (c) {
      rows.push(mk({ kind: 9, rng: [c[0]], c8: c[1], f4: c[2], hq: c[3], tw: c[4], tt: c[5], c88: 2 }));
    });
    rows.push(mk({ kind: 0x2f, rng: [0], c88: 2 }));
    // the tip: date gap, +0xae, the seen bits, screen bounds, kind 0x32, message codes, [0x005a5a00] bit 2, startMessage
    [[2000, 1500, 0, 0], [2000, 1499, 0, 0], [2000, 100, 1, 0], [300, 100, 0, 1]].forEach(function (c) {
      rows.push(mk({ kind: 6, date: c[0], last: c[1], ae: c[2], seen: c[3] ? [0x40, 0] : [0, 0] }));
    });
    rows.push(mk({ kind: 0x22, seen: [0, 0x4] }));
    rows.push(mk({ kind: 0x22, seen: [0, 0x2] }));
    [[100, 200], [101, 200], [699, 200], [700, 200], [300, 100], [300, 101], [300, 399], [300, 400]].forEach(function (c) {
      rows.push(mk({ kind: 1, sx: c[0], sy: c[1] }));
    });
    rows.push(mk({ kind: 0x32 }));
    rows.push(mk({ kind: 2, c88: 0 })); rows.push(mk({ kind: 3, c88: 1 })); rows.push(mk({ kind: 3, c88: 0 }));
    rows.push(mk({ kind: 1, a5a00: 0 })); rows.push(mk({ kind: 1, sm: [0] })); rows.push(mk({ kind: 7, arg: 5 }));
    rows.push(mk({ kind: 0x16, arg: 2 })); rows.push(mk({ kind: 0xb, arg: 123 })); rows.push(mk({ kind: 0x14, arg: -7 }));
    return rows;
  }
  function egtType(t) {
    if (typeof t === 'number') return t;
    const bit = t === 'b1' ? 1 : 4;
    for (let k = 0; k < 32; k++) if (ptr(0x004d60a8 + 0x230 * k).readU8() & bit) return k;
    throw new Error('c3dz_egt: no type with bit ' + bit);
  }
  function egtOn(fx, r) {
    r = Object.assign({}, r, { ty: egtType(r.ty) });
    recsFill(0x5d);
    const a = rec(r.g), pa = rec(r.p);
    wr8(a + 0x22, r.sub); wr8(a + 0x21, r.hole); wr16(a + 0xa2, r.p); wr8(a + 0x18, r.d0); wr8(a + 0x36, r.ee);
    wr32(a + 0x10, r.c8); wr16(a + 0xb6, r.ty); wr16(a + 0xa6, r.fa6); wr16(a + 0xa8, r.fa8); wr16(a + 0xaa, r.faa);
    wr32(a + 0, r.x); wr32(a + 4, r.y); wr32(a + 8, r.sx); wr32(a + 0xc, r.sy); wr16(a + 0xa4, r.mood);
    for (let k = 0; k < 10; k++) wr8(a + 0x70 + k, r.hist[k]);
    wr16(a + 0x8a, r.w8a | (ptr(a + 0x8a).readU16() & 0x7fff)); wr16(a + 0xae, r.ae);
    wr16(a + 0x88, r.w88 | (ptr(a + 0x88).readU16() & 0x7fff));
    wr8(pa + 0x21, r.pHole); wr8(pa + 0x84, r.p84); wr16(pa + 0xa4, r.pMood); wr16(pa + 0xa2, r.g);
    // the partner as the recursive call sees it: a small step byte, no flags, the default type, the same tile, on screen
    wr8(pa + 0x22, 1); wr8(pa + 0x18, 0); wr32(pa + 0x10, 0); wr16(pa + 0xb6, r.ty); wr32(pa + 0, r.x); wr32(pa + 4, r.y);
    wr32(pa + 8, 250); wr32(pa + 0xc, 250); wr16(pa + 0xae, 0); wr16(pa + 0x8a, 0);
    fillMem(HOLES, HOLES_N, 0x01);
    const hs = HOLES + 0x208 * r.hole;
    wr32(hs + 0x20, r.rem);                                           // 0x00575ad0 + h * 0x208
    wr8(hs + 0, 0x10);                                                // 0x00575ab0 + h * 0x208
    wr8(a + 0x23 + r.hole, 0x10 + r.remD);                            // signed byte +0x23 + h
    wr8(hs + 0x200, r.remF);                                          // 0x00575cb0 + h * 0x208
    fillMem(TILEW, TILEW_N, 0); TILE_B.forEach(function (t) { fillMem(t, 2500, 0x02); });
    const tile = (r.x >> 10) * 50 + (r.y >> 10);
    wr16(TILEW + 2 * tile, r.tw);
    fillMem(KINDW, KINDW_N, 0x3c);
    fillMem(BUF, BUF_N, 0x55); putStr(BUF, 'pre|');
    wr32(0x00543d04, 5); wr32(0x0059c08c, r.seen[0]); wr32(0x00571d38, r.seen[1]); wr32(0x0055e5ac, r.last); wr32(0x00834170, r.date);
    wr32(0x00543cf4, r.cf4); wr32(0x00822c88, r.c88); wr32(0x005a59f8, r.a59f8); wr32(0x005409a8, 0x5a);
    const fl = ptr(FLAGS).readU32() & ~0x6000000;
    wr32(FLAGS, fl | (r.f2 ? 0x2000000 : 0) | (r.f4 ? 0x4000000 : 0));
    const b = ptr(0x005a5a00).readU8();
    wr8(0x005a5a00, (b & ~4) | (r.a5a00 & 4));
    fillMem(TYPETAB, 0x600, 0);
    wr8(TYPETAB + 3 + 44 * r.ty + r.hole, r.t3b);                    // 0x005849e3 + type * 44 + hole (kind 0x3b)
  }
  function egtScene(prefix) {
    return function () {
      const fx = { clog: newClog(0x2000), clogCap: 0x2000 };
      freezeNow();
      recsSave(); saveRange(rec(0x98), 0x100); saveRange(HOLES, HOLES_N); saveRange(TILEW, TILEW_N); TILE_B.forEach(function (t) { saveRange(t, 2500); });
      saveRange(KINDW, KINDW_N); saveRange(BUF, BUF_N); saveRange(TYPETAB, 0x600); saveRange(0x005a5a00, 1); saveRange(FLAGS, 4);
      EGT_G.forEach(function (a) { saveRange(a, 4); });
      // the tile type byte 0x005722e8 of the rows' tiles (x >> 10 = 4, y >> 10 = 5 by default): saved, set per row
      saveRange(0x005722e8, 2500);
      const rows = egtRows(prefix);
      fx.n = rows.length;
      rowsOn(fx, rows.map(function (r) {
        return Object.assign({}, r, { a: r.g, b: r.kind, c: r.arg, on: function () {
          egtOn(fx, r);
          fillMem(0x005722e8, 2500, 0x03);
          wr8(0x005722e8 + (r.x >> 10) * 50 + (r.y >> 10), r.tt);
        } });
      }));
      calleeStand(fx, 0, 0x00469b00, 4, function (n) {
        wr32(0x005409a8, cur.pf[Math.min(n, cur.pf.length - 1)]);
        const p = ptr(BUF); putStr(p.add(lenAt(p)), '{c' + n + '}');
        return 0;
      });
      calleeStand(fx, 1, 0x00406d50, 2, 0);
      calleeStand(fx, 2, 0x0046c940, 1, function (n, a) { const g = a[0] | 0; return cur.tb[g] !== undefined ? cur.tb[g] : (g & 1); });
      calleeStand(fx, 3, 0x0040c500, 4, 0x21);
      calleeStand(fx, 4, 0x004481b0, 5, 0x22);
      calleeStand(fx, 5, 0x00407000, 3, function () { return cur.hq; });
      calleeStand(fx, 6, 0x0045c1e0, 2, function (n, a) {
        if (n < cur.rng.length) return cur.rng[n];
        const m = a[1] & 0xffff; return m ? (Math.imul(n, 37) + 11) % m : 0;
      }, { abi: 'thiscall' });
      calleeStand(fx, 7, 0x00407700, 3, function (n, a) { const p = ptr(BUF); putStr(p.add(lenAt(p)), '[d' + (a[0] | 0) + ',' + (a[1] | 0) + ']'); return 0; });
      calleeStand(fx, 8, 0x004676e0, 2, function (n, a) { const p = ptr(BUF); putStr(p.add(lenAt(p)), '<' + (a[0] | 0) + '>'); return 0; });
      calleeStand(fx, 9, 0x0040cb00, 3, function (n) { return cur.sm[Math.min(n, cur.sm.length - 1)]; });
      return fx;
    };
  }
  // ====================================================================================================================
  // buildGolferComment (kind, arg, place, g). Records 2..9 and the slots 0x98 / 0x99 (0x00582cb8, 0x200 bytes); the hole tables
  // from 0x005758a8 (hole - 1's row is read); the per-type override strings 0x00543d10 + (type*25 + n)*50 for types 4..7; the type
  // rows 0x00578350 (0x30 bytes: name +0, second name +0x10, attribute byte +0x26) and the 37-byte rows 0x0055d738 for arg 0..15
  // get synthetic strings. typeBit7Clear, buildGolferName, appendEndearment, appendHoleName, appendClubName, appendOpinion,
  // decorationName and loadStoryStrings are stood in (each appends a marker to the buffer); replaceInText 0x0045b7c0 and _itoa run.
  // ====================================================================================================================
  const OVR = 0x00543d10 + 4 * 25 * 50, OVR_N = 4 * 25 * 50;
  const TROW = 0x00578350, TROW_N = 25 * 0x30;                // 25 rows: the tile map 0x00578804 follows
  const VROW = 0x0055d738, VROW_N = 16 * 37;
  const BHOLES = 0x005758a8, BHOLES_N = 0x208 * 21;
  const BGC_G = [0x00834170, 0x00822c88, 0x005a8c50, 0x005a8c58, 0x005a8c60, 0x005409a8, 0x0058b198];
  function bgcRows(prefix) {
    const rows = [];
    const mk = function (o) {
      return Object.assign({ kind: 1, arg: 2, hole: 3, sub: 4, g: 4, p: 5, cls: 0, pair: 0, ty: 4, c10: 0, d1: 0, story: 3, own: 2,
        theirs: 0, par: 2, fl: 0, flA8: 0, prevPar: 1, t3b: 0, t3f: 2, date: 1000, c88: 2, sel: [1, 2, 3], tb: { 4: 1, 5: 0 }, ovr: null,
        pty: 6 }, o);
    };
    if (prefix === 'grid') {
      // every kind 0..0x42 with six variants: pairing word, type (low bits), parity, typeBit7Clear, arg, cls
      for (let k = 0; k <= 0x42; k++) {
        for (let v = 0; v < 6; v++) {
          const g = 4 + (v & 1), p = g ^ 1;
          const tb = {}; tb[g] = (v >> 1) & 1; tb[p] = v & 1;
          rows.push(mk({ kind: k, g: g, p: p, pair: v, ty: 4 + ((v + k) & 3), tb: tb, sub: (2 * v + k) % 11, arg: (k === 0x3d) ? 2 + v : (v * 3 + k) % 13,
            cls: v === 3 ? 0x20 : (v === 4 ? 0x40 : 0), c10: v & 2 ? 0x110000 : 0, story: v, date: 400 + 37 * v + 3 * k }));
        }
      }
      return rows;
    }
    // the override list 0x004c2d10: a kind in the list with its string set, one with it empty, and g at / above 0x98
    [[0x3e, 0], [1, 1], [4, 2], [0x1b, 19], [0x12, 17]].forEach(function (c) {
      rows.push(mk({ kind: c[0], ovr: [4, c[1], 'ovr' + c[1]] }));
      rows.push(mk({ kind: c[0], ovr: [5, c[1], 'other type'] }));
    });
    rows.push(mk({ kind: 1, g: 0x98, p: 0x99, ovr: [4, 1, 'skip'] }));
    rows.push(mk({ kind: 6, g: 0x99, p: 0x98 }));
    rows.push(mk({ kind: 0x13, g: 0x98, p: 0x99, fl: 4, own: 1, par: 1 }));
    rows.push(mk({ kind: 0x13, g: 0x98, p: 0x99, fl: 8, own: 3, par: 3 }));
    // arg -1: no tail
    [1, 6, 0xa, 0x13, 0x32].forEach(function (k) { rows.push(mk({ kind: k, arg: -1 })); });
    // kind 0x13 / 0x17: every difference -5..4, the hole flag bits 4 / 8, kind 0x17, the (g + hole) & 3 bits, the pairing compare
    for (let d = -5; d <= 4; d++) {
      rows.push(mk({ kind: 0x13, own: 4 + d, par: 4 }));
      rows.push(mk({ kind: 0x13, own: 4 + d, par: 4, fl: 4 }));
      rows.push(mk({ kind: 0x13, own: 4 + d, par: 4, fl: 8 }));
      rows.push(mk({ kind: 0x17, own: 4 + d, par: 4 }));
    }
    [[4, 0, 1, 0x100], [5, 0, 2, 0x200], [6, 0, 4, 0x400], [7, 0, 7, 0x700], [4, 1, 1, 0x100], [4, 0, 1, 0x200], [5, 0, 2, 0x600]].forEach(function (c) {
      // g, hole offset, byte +0x19 bits, hole flags: (g + hole) & 3 selects 0x100 / 0x200 / 0x400
      rows.push(mk({ kind: 0x13, g: c[0], p: c[0] ^ 1, hole: 4 - c[1], d1: c[2], fl: c[3], own: 3, par: 4 }));
      rows.push(mk({ kind: 0x13, g: c[0], p: c[0] ^ 1, hole: 4 - c[1], d1: c[2], fl: c[3], own: 5, par: 4 }));
    });
    [[2, 3, 5], [2, 5, 3], [4, 4, 4], [4, 4, 0], [3, 4, 5], [2, -2, 1]].forEach(function (c) {
      rows.push(mk({ kind: 0x13, pair: c[0], own: c[1], theirs: c[2], par: c[1] }));   // d = 0: past the flag tests
    });
    // kind 0x1e: hole flags 0x20 / 0x40 with the byte 0x00575aa8, equal par with the previous hole (itoa), else
    [[0x20, 0x20, 0, 0], [0x20, 0, 0, 0], [0x40, 0x40, 0, 0], [0x40, 0x20, 3, 3], [0, 0, 4, 4], [0, 0, 4, 5], [0x60, 0x60, -2, -2]].forEach(function (c) {
      rows.push(mk({ kind: 0x1e, fl: c[0], flA8: c[1], par: c[2], prevPar: c[3] }));
    });
    // kind 0x3b: the type-table byte, hole flags 1 / 2, the type's bit 0, the second byte (itoa)
    [[1, 0, 4], [0, 3, 4], [0, 1, 4], [0, 1, 5], [0, 0, 4], [0, 0, 7]].forEach(function (c) {
      rows.push(mk({ kind: 0x3b, t3b: c[0], fl: c[1], ty: c[2], t3f: 9 + c[2] }));
    });
    // kinds 0x33 / 0x34 / 0x35: the date bit 3 and the selectors 1..3 and others
    [0x33, 0x34, 0x35].forEach(function (k) {
      [[0, [1, 1, 1]], [8, [1, 1, 1]], [8, [2, 2, 2]], [8, [3, 3, 3]], [8, [0, 4, -1]], [3, [2, 2, 2]]].forEach(function (c) {
        rows.push(mk({ kind: k, date: 800 + c[0], sel: c[1] }));
      });
    });
    // kind 0x23 by season, 0x22 / 0x24 / 0x3a by arg, 0xa (a name ending in s), 0x3c / the tail (attribute byte 0xd)
    [0, 80, 160, 240, 320, -81, -100, -20, 60, 140].forEach(function (dt) { rows.push(mk({ kind: 0x23, date: dt, g: 4 })); });   // date + 180 at 80 k too
    [0, 1, 2, 3].forEach(function (a) { rows.push(mk({ kind: 0x22, arg: a })); rows.push(mk({ kind: 0x3a, arg: a })); rows.push(mk({ kind: 0x24, arg: a })); });
    [1, 3, 4, 6].forEach(function (a) { rows.push(mk({ kind: 0xa, arg: a })); rows.push(mk({ kind: 0x3c, arg: a })); rows.push(mk({ kind: 0xc, arg: a })); });
    [0x100, 0x101, 0xff].forEach(function (a) { rows.push(mk({ kind: 0xb, arg: a % 16, argHi: a })); });
    // kind 0x28: type % 8 for negative types, arg <= 0; kind 0x32: the story selector
    [[-3, 0], [9, 1], [12, 5], [-8, 2]].forEach(function (c) { rows.push(mk({ kind: 0x28, ty: c[0], arg: c[1] })); });
    [[0x13, 0x100000], [0x17, 0], [0x07, 0x100000], [0x10, 0]].forEach(function (c) { rows.push(mk({ kind: 0x32, arg: c[0], c10: c[1] })); });
    // kind 0x36 by [0x00822c88]; kind 7 by arg and bit 0x10000; kind 0x41 by [0x00822c88]
    [0, 1, 2].forEach(function (c) { rows.push(mk({ kind: 0x36, c88: c, arg: 3 })); rows.push(mk({ kind: 0x41, c88: c })); });
    [[0, 0], [0, 0x10000], [1, 0x10000]].forEach(function (c) { rows.push(mk({ kind: 7, arg: c[0], c10: c[1] })); });
    // kind 1: the partner's type bit 0 and typeBit7Clear, the type bits 0x40, g == -1
    [[7, 1], [7, 0], [6, 1], [6, 0]].forEach(function (c) { rows.push(mk({ kind: 1, pair: 1, pty: c[0], tb: { 4: 1, 5: c[1] } })); });
    [6, 7].forEach(function (t) { rows.push(mk({ kind: 1, pair: 0, ty: t })); });
    rows.push(mk({ kind: 1, pair: 0, ty: 4, g: -1, p: -2 }));
    // kinds 5 / 0x25 / 0x26: pairing word 2 with an odd golfer, words outside 0..5
    [5, 0x25, 0x26].forEach(function (k) { rows.push(mk({ kind: k, pair: 2, g: 5, p: 4 })); rows.push(mk({ kind: k, pair: 6 })); rows.push(mk({ kind: k, pair: -1 })); });
    // kind 0xd with a negative type (remainder -1 / -2), kind 0x1c with odd types
    [-1, -2].forEach(function (t) { rows.push(mk({ kind: 0xd, pair: 0, ty: t })); });
    [[5, 1], [5, 0], [7, 1], [7, 0]].forEach(function (c) { rows.push(mk({ kind: 0x1c, pair: 0, ty: c[0], tb: { 4: c[1] } })); });
    // kind 0x3d: both parities for args 3..8
    for (let a = 3; a <= 8; a++) { rows.push(mk({ kind: 0x3d, arg: a, g: 4, p: 5 })); rows.push(mk({ kind: 0x3d, arg: a, g: 5, p: 4 })); }
    // the tail's g > 0x98 brackets with kinds 2 / 0xb / 0x27
    [2, 0xb, 0x27, 0xc].forEach(function (k) { rows.push(mk({ kind: k, g: 0x99, p: 0x98, arg: 3 })); });
    [[5, 0], [0x25, 4], [0xb, 0]].forEach(function (c) { rows.push(mk({ kind: c[0], pair: c[1], g: 0x98, p: 0x99, arg: 3 })); });   // DATA placeholder at g == 0x98
    return rows;
  }
  function bgcOn(fx, r) {
    recsFill(0x2b);
    fillMem(0x00582cb8, 0x200, 0x11);
    fillMem(0x005792b8, 0x200, 0x13);                                 // records -2 / -1 (the g == -1 row)
    const a = rec(r.g), pa = rec(r.p);
    wr8(a + 0x18, r.cls); wr16(a + 0xae, r.pair); wr16(a + 0xb6, r.ty); wr16(a + 0xa2, r.p); wr32(a + 0x10, r.c10); wr8(a + 0x19, r.d1);
    wr16(a + 0xb0, r.story); wr8(a + 0x23 + r.hole, r.own); wr16(pa + 0xb6, r.pty); wr8(pa + 0x23 + r.hole, r.theirs);
    fillMem(BHOLES, BHOLES_N, 0x02);
    const hs = 0x00575ab0 + 0x208 * r.hole;
    wr8(hs, r.par); wr8(hs - 8, r.flA8); wr8(hs - 0x208, r.prevPar); wr32(hs + 0x200, r.fl);
    fillMem(TYPETAB, 0x600, 0);
    if (r.ty >= 0 && r.ty < 32) { wr8(TYPETAB + 3 + 44 * r.ty + r.hole, r.t3b); wr8(TYPETAB + 0x16 + 44 * r.ty + r.hole, r.t3f); }
    fillMem(OVR, OVR_N, 0);
    if (r.ovr) putStr(0x00543d10 + (r.ovr[0] * 25 + r.ovr[1]) * 50, r.ovr[2]);
    if (r.ty < 4 || r.ty > 7) {
      // a type outside the saved window: the override slots of kind for it are saved and emptied (byte 0)
      for (let n = 0; n < 20; n++) if (ptr(0x004c2d10 + n).readS8() === r.kind) {
        const sl = 0x00543d10 + (r.ty * 25 + n) * 50;
        saveRange(sl, 1); wr8(sl, 0);
      }
    }
    for (let k = 0; k < 25; k++) {
      fillMem(TROW + 0x30 * k, 0x30, 0);
      putStr(TROW + 0x30 * k, 'tn' + k + (k % 2 ? 's' : ''));
      putStr(TROW + 0x30 * k + 0x10, 'alt' + k);
      wr8(TROW + 0x30 * k + 0x26, k % 3 === 0 ? 0xd : 5);
    }
    for (let k = 0; k < 16; k++) { fillMem(VROW + 37 * k, 37, 0); putStr(VROW + 37 * k, 'v' + k); }
    fillMem(BUF, BUF_N, 0x55); putStr(BUF, 'pre|');
    fillMem(0x00838d9c, 0x20, 0x44);
    wr32(0x00834170, r.date); wr32(0x00822c88, r.c88);
    wr32(0x005a8c60, r.sel[0]); wr32(0x005a8c58, r.sel[1]); wr32(0x005a8c50, r.sel[2]);
    wr32(0x005409a8, 7); wr32(0x0058b198, 0x12345678);
  }
  function bgcScene(prefix) {
    return function () {
      const fx = { clog: newClog(0x2000), clogCap: 0x2000 };
      freezeNow();
      recsSave(); saveRange(0x005792b8, 0x200); saveRange(0x00582cb8, 0x200); saveRange(BHOLES, BHOLES_N); saveRange(TYPETAB, 0x600);
      saveRange(OVR, OVR_N);
      saveRange(TROW, TROW_N); saveRange(VROW, VROW_N); saveRange(BUF, BUF_N); saveRange(0x00838d9c, 0x20);
      BGC_G.forEach(function (x) { saveRange(x, 4); });
      const rows = bgcRows(prefix);
      fx.n = rows.length;
      rowsOn(fx, rows.map(function (r) {
        const arg = r.argHi !== undefined ? r.argHi : r.arg;
        return Object.assign({}, r, { a: r.kind, b: arg, c: r.hole * 11 + r.sub, d: r.g, on: function () { bgcOn(fx, r); } });
      }));
      const app = function (s) { const p = ptr(BUF); putStr(p.add(lenAt(p)), s); };
      calleeStand(fx, 0, 0x0046c940, 1, function (n, a) { const g = a[0] | 0; return cur.tb[g] !== undefined ? cur.tb[g] : (g & 1); });
      calleeStand(fx, 1, 0x004676e0, 2, function (n, a) { app('<' + (a[0] | 0) + ',' + (a[1] | 0) + '>'); return 0; });
      calleeStand(fx, 2, 0x00467560, 1, function (n, a) { app('~' + (a[0] | 0)); return 0; });
      calleeStand(fx, 3, 0x00407280, 1, function (n, a) { app('#' + (a[0] | 0)); return 0; });
      calleeStand(fx, 4, 0x0040a9a0, 1, function (n, a) { app('%' + (a[0] | 0)); return 0; });
      calleeStand(fx, 5, 0x00469a20, 2, function (n, a) { app('!' + (a[0] | 0) + ',' + (a[1] | 0)); return 0; });
      calleeStand(fx, 6, 0x00407700, 3, function (n, a) { app('[' + (a[0] | 0) + ',' + (a[1] | 0) + ',' + (a[2] | 0) + ']'); return 0; });
      calleeStand(fx, 7, 0x00466b70, 4, function (n, a) { app('{' + (a[0] | 0) + ',' + (a[1] | 0) + ',' + (a[2] | 0) + ',' + (a[3] | 0) + '}'); return 0; });
      return fx;
    };
  }
  F.c3dz_bgc = bgcScene('main');
  F.c3dz_bgcg = bgcScene('grid');

  F.c3dz_egt = egtScene('main');
  F.c3dz_egtk = egtScene('kinds');
  F.c3dz_egtp = egtScene('partner');

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
