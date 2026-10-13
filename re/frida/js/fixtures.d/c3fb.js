// Fixtures for batch c3fb (round 33, 2026-10-13): golf_clean.exe size-only rows of 1-2 KB.
// Reimplementations: shim/src/re/c3fb.cpp. Registry: re/frida/registry.d/c3fb.py.
// Purposes and per-jcc tables: log/c3/c3fb_purpose.md. Notes: log/c3/c3fb_notes.txt.
//
// Method (c3er.js's, which comes from c3ea.js / c3ec.js):
// - Every key freezes the game's first thread (re/frida/js/freeze.js) and saves every global it writes; the flush
//   restores them newest first before the resume.
// - Argument-free functions get one unused cdecl stack word "$cN": diff_hook resolves it before the state snapshot, so
//   the getter of fx.cN applies scene N.
// - Callees that draw, load, play, read files or enter a screen are stood in on the A/B thread only (calleeStand): each
//   call is counted, folded into a running FNV-1a hash and logged with its arguments (and the text of string
//   arguments); the log ($clog) is compared state. Arguments that point into the caller's own frame are masked.
(function () {
  const F = {};

  function alloc(n) {
    const p = __keepAlloc(n);
    p.writeByteArray(new Uint8Array(n).buffer);
    return p;
  }

  let frozen = false;
  function freezeNow() {
    if (frozen) return;
    __freeze.thread();
    frozen = true;
    __freeze.atFlush(function () { frozen = false; });
    __freeze.negOn();   // the negative-control build's exported switch (re/frida/js/freeze.js); nothing otherwise
  }

  function wr32(a, v) { ptr(a).writeU32(v >>> 0); }
  function wr16(a, v) { ptr(a).writeU16(v & 0xffff); }
  function wr8(a, v) { ptr(a).writeU8(v & 0xff); }
  function saveRange(a, n) { __freeze.save(ptr(a), n); }
  function wrStr(a, s, max) {
    const p = ptr(a);
    for (let i = 0; i < max; i++) p.add(i).writeU8(i < s.length ? s.charCodeAt(i) & 0xff : 0);
  }

  // ---- calleeStand (c3er.js's): log word 0 = cursor, words 1..32 = call count of stub k, word 0x84 = FNV-1a over every
  // call (the callee address and the logged values), entries {va, count, values...} from +0x90 while they fit.
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

  // ---- scenes: fx.<prefix><i> applies scene i and returns 0 (the unused cdecl stack word, or a value for an argument)
  function scenesOn(fx, prefix, scenes) {
    scenes.forEach(function (sc, i) {
      Object.defineProperty(fx, prefix + i, { enumerable: true, configurable: true, get: function () {
        const v = sc(fx);
        return v === undefined ? 0 : v;
      } });
    });
  }

  // ---- negative controls: word 2 of the exported C3fbNegCtl switch picks the single wrong detail that is live
  function negSel(k) {
    (__freeze.negCtlWords() || []).forEach(function (a) { __freeze.save(a.add(8), 4); a.add(8).writeU32(k); });
  }
  globalThis.__c3fbNegSel = negSel;

  // ====================================================================================================================
  // serializeCourse 0x0040afa0: a save-file tape under saveFieldIo 0x0040af70
  // ====================================================================================================================
  // saveFieldIo (C3, c3dw) calls the static CRT's __read 0x004a583a (mode non-zero) or __write 0x004a5b58 (mode zero)
  // with the descriptor [0x00568d08]. Both are stood in on the A/B thread: __write appends the caller's bytes to the tape,
  // __read copies the next bytes of the tape into the caller's buffer, except that the 4-byte read into the flag word
  // 0x0059e7b8 gets the scene's flag value, so the scene decides the two flag-gated tails (0x0040b451, 0x0040b46f).
  // Each call is also logged (fd, buf, count). The answer is the count, the count minus one, or -1 (the scene's `ret`),
  // so the value saveFieldIo hands back, and with it serializeCourse's eax, differs between scenes.
  // Tape block: +0 cursor, +4 bytes moved, +8 calls; data from +0x10 (0x70000 bytes: the 89 fields total 0x6d98c).
  const TAPE = 0x70010, T_DATA = 0x10;
  const G_FD = 0x00568d08, G_MODE = 0x0053e638, G_FLAGS = 0x0059e7b8;
  // every field serializeCourse can read into (the table in shim/src/re/c3fb.cpp, 89 rows): saved for the flush
  const SER_FIELDS = [
    [0x00822c78, 4], [0x005685f0, 4], [0x0058d36c, 4], [0x0059aafc, 4], [0x005a34e0, 1], [0x0059ae78, 4], [0x00541cd8, 4],
    [0x005a882c, 4], [0x0056949c, 4], [0x005a636c, 4], [0x005a9cac, 4], [0x005a9cb0, 4], [0x005787cc, 4], [0x00561258, 4],
    [0x00571fd4, 4], [0x0059bf90, 4], [0x00575ab0, 0x28a0], [0x005722e8, 0x9c4], [0x0056988c, 0x9c4], [0x0053caf0, 0x1388],
    [0x0053bbac, 0x9c4], [0x005a4998, 0xa29], [0x00838c1c, 0x169], [0x0058bcb8, 0x1000], [0x004c284c, 4], [0x00561250, 4],
    [0x005794b8, 0x9c00], [0x00585850, 0x1300], [0x00834170, 4], [0x005a6364, 4], [0x0053a450, 4], [0x00822c88, 4],
    [0x005409ac, 0x3e8], [0x0051b388, 0x3e8], [0x0053e63c, 0x3e8], [0x004c2850, 4], [0x0059b730, 4], [0x0059ae7c, 4],
    [0x0059dea0, 0x80], [0x0059c08c, 4], [0x00568600, 0x3e8], [0x00822c70, 4], [0x005685f8, 4], [0x005a5a24, 4],
    [0x00584210, 0x7d0], [0x00543cfc, 4], [0x004c2e14, 4], [0x00572cac, 4], [0x0059aaf8, 4], [0x0056ae70, 0x1700],
    [0x005422f8, 0xf0], [0x0059ae80, 0x1c8], [0x0059b048, 4], [0x005736b0, 0x2400], [0x0059fc60, 0x3880], [0x005849e0, 0xe70],
    [0x005a59f8, 4], [0x005a5a04, 0x20], [0x0056d1b8, 0x1770], [0x004c2c94, 0x12], [0x005689e8, 0x320], [0x005419d0, 0x40],
    [0x00572cb0, 0xa00], [0x004d6088, 0xa640], [0x00543d10, 0x19a28], [0x00567b04, 4], [0x005a6378, 0x9c4],
    [0x0056c7e4, 0x9c4], [0x0053ea24, 0x9c4], [0x00542414, 0x9c4], [0x0059e7b8, 4], [0x004c1578, 0x420], [0x00571fd8, 0x30e],
    [0x00520640, 0x3e8], [0x00567328, 0x64], [0x00571d38, 4], [0x0056fcb0, 0x1002], [0x0059d81c, 0x100], [0x005a46b8, 0x100],
    [0x0056a51c, 4], [0x005a6374, 4], [0x0056a524, 0x28], [0x005a47e0, 4], [0x005830b8, 0x9c4], [0x0059c090, 0x9c4],
    [0x005a5a00, 4], [0x004e06c8, 0x8c0], [0x0058f338, 0xb7c0], [0x00520a28, 0x19a28]
  ];

  // scenes: fd, flags (written to the live flag word in write mode, served by the tape in read mode), ret (0 count,
  // 1 count - 1, 2 = -1), seed (the read tape's byte pattern; the write tape starts zeroed), a value for 0x00822c78
  // (the first field, so the write tape's first word differs between write scenes)
  const SER_SCENES = [
    { fd: 3, flags: 0x00000000, ret: 0, seed: 0x11, first: 0x1001 },
    { fd: 4, flags: 0x40000000, ret: 0, seed: 0x23, first: 0x1002 },
    { fd: 5, flags: 0x00200000, ret: 0, seed: 0x35, first: 0x1003 },
    { fd: 6, flags: 0x40200000, ret: 1, seed: 0x47, first: 0x1004 },
    { fd: 7, flags: 0x40200000, ret: 2, seed: 0x59, first: 0x1005 },
    { fd: 3, flags: 0xbfdfffff, ret: 0, seed: 0x6b, first: 0x1006 },   // every other bit set: no tail
    { fd: 3, flags: 0x40000000, ret: 1, seed: 0x7d, first: 0x1007 },
    { fd: 9, flags: 0x00200001, ret: 2, seed: 0x8f, first: 0x1008 },
  ];
  let serCur = SER_SCENES[0];

  function tapeStands(fx) {
    const tape = fx.tape;
    const answer = function (count) {
      return serCur.ret === 0 ? count : serCur.ret === 1 ? count - 1 : 0xffffffff;
    };
    // k0 __read(fd, buf, count) (saveFieldIo's call 0x0040af8a)
    calleeStand(fx, 0, 0x004a583a, 3, function (n, a) {
      const buf = ptr(a[1]), count = a[2] | 0, cur = tape.readU32();
      if (a[1] === G_FLAGS && count === 4) {
        buf.writeU32(serCur.flags >>> 0);
      } else if (count > 0 && cur + count <= TAPE - T_DATA) {
        Memory.copy(buf, tape.add(T_DATA + cur), count);
      }
      tape.writeU32(cur + Math.max(0, count));
      tape.add(4).writeU32(tape.add(4).readU32() + Math.max(0, count));
      tape.add(8).writeU32(tape.add(8).readU32() + 1);
      return answer(count);
    });
    // k1 __write(fd, buf, count) (saveFieldIo's call 0x0040af95)
    calleeStand(fx, 1, 0x004a5b58, 3, function (n, a) {
      const count = a[2] | 0, cur = tape.readU32();
      if (count > 0 && cur + count <= TAPE - T_DATA) Memory.copy(tape.add(T_DATA + cur), ptr(a[1]), count);
      tape.writeU32(cur + Math.max(0, count));
      tape.add(4).writeU32(tape.add(4).readU32() + Math.max(0, count));
      tape.add(8).writeU32(tape.add(8).readU32() + 1);
      return answer(count);
    });
  }

  function serScene(sc) {
    return function (fx) {
      serCur = sc;
      saveRange(G_FD, 4); wr32(G_FD, sc.fd);
      saveRange(G_MODE, 4);
      saveRange(G_FLAGS, 4); wr32(G_FLAGS, sc.flags);
      saveRange(0x00822c78, 4); wr32(0x00822c78, sc.first);
      // the read tape: a byte pattern from the seed (the write tape is cleared by the same fill: a write overwrites it)
      const n = TAPE - T_DATA, u = new Uint8Array(n);
      for (let i = 0; i < n; i++) u[i] = (sc.seed + i * 7 + (i >> 9)) & 0xff;
      fx.tape.add(T_DATA).writeByteArray(u.buffer);
      fx.tape.writeU32(0); fx.tape.add(4).writeU32(0); fx.tape.add(8).writeU32(0);
      return 0;
    };
  }

  F.c3fb_serial = function () {
    const fx = { clog: newClog(0x1000), clogCap: 0x1000, tape: alloc(TAPE) };
    freezeNow();
    SER_FIELDS.forEach(function (f) { saveRange(f[0], f[1]); });   // read scenes overwrite them; restored at the flush
    tapeStands(fx);
    scenesOn(fx, 'c', SER_SCENES.map(serScene));
    return fx;
  };

  // ====================================================================================================================
  // showTitleScreen 0x0043cd70: every callee a recorder
  // ====================================================================================================================
  // The function reads no global and takes no argument: one call covers every branch (the two four-way row switches and
  // the loops). Scenes vary only the value the Image destructor stand-in returns, which is the function's eax
  // (0x0043d261 is the last call before the ret).
  const TITLE_DTOR = [0, 1, 0x1234, 0x7fffffff, 0x80000000, 0xffffffff, 0x0043cd70, 0x55aa55aa, 2, 0xdeadbeef];
  let titleCur = 0;
  function titleStands(fx) {
    calleeStand(fx, 0, 0x0043cce0, 2, 0, { str: { 0: 8 } });                                  // playAudioFile
    calleeStand(fx, 1, 0x00474ae0, 1, function (n, a) { return a[0]; }, { abi: 'thiscall', mask: [0] });   // Image_ctor
    calleeStand(fx, 2, 0x00474dd0, 7, 0, { abi: 'thiscall', mask: [0] });                     // Surface_createFull
    calleeStand(fx, 3, 0x00475840, 6, 0, { abi: 'thiscall', mask: [0], str: { 1: 8 } });     // loadImage
    calleeStand(fx, 4, 0x00473bf0, 8, 0, { abi: 'thiscall', mask: [1] });                     // Surface_blit (dest kept)
    calleeStand(fx, 5, 0x00474c40, 1, function () { return TITLE_DTOR[titleCur]; },
                { abi: 'thiscall', mask: [0] });                                              // Image_dtor
  }
  F.c3fb_title = function () {
    const fx = { clog: newClog() };
    titleStands(fx);
    scenesOn(fx, 'c', TITLE_DTOR.map(function (v, i) { return function () { titleCur = i; return 0; }; }));
    return fx;
  };

  // ====================================================================================================================
  // buildGolferBioText 0x004315e0: the new-course reset
  // ====================================================================================================================
  // Random::range 0x0045c1e0 (C3) and clearBuffers 0x0045b880 run for real; initRecords 0x00401750 is a recorder whose
  // answer is the function's eax (its call 0x00431ba1 is the last instruction before the ret). The 76 golfer-type
  // records 0x004d6088 + 0x230 * r get synthetic names (+0x10, the bio's first words) and class flags (+0x20 / +0x21,
  // read by the pick loop 0x00431762..0x004317b0); the pattern guarantees every class in every residue mod 3, so the
  // twelve picks always end. Every region the body writes is saved for the flush and is compared state.
  const BIO_REGIONS = [
    [0x005a8c38, 0x50], [0x00543ca0, 0x50], [0x00578804, 0x9c4], [0x005794b8, 0x9c00], [0x00575ab0, 0x28a0],
    [0x00585850, 0x1300], [0x0058bcb8, 0x1000], [0x004c1578, 0x420], [0x005689e8, 0x320], [0x005419d0, 0x40],
    [0x0053caf0, 0x1388], [0x0056988c, 0x9c4], [0x005849e0, 0xe70], [0x00567b34, 0x260], [0x005830b8, 0x9c4],
    [0x0056a524, 0x28], [0x0059ae80, 0x1c8], [0x004c2c9c, 2], [0x0053ea24, 0x9c4], [0x0056fcb0, 0x1002],
    [0x0059d81c, 0x100], [0x00568600, 0x3e8], [0x0051a068, 0x200], [0x00822d9c, 4], [0x004d6088, 0xa640],
    [0x005736b0, 0x2410], [0x0059fc60, 0x3880], [0x00572cb0, 0xa00], [0x005685f0, 4], [0x00567b04, 4], [0x00572cac, 4],
    [0x0059b730, 4], [0x004c284c, 4], [0x0059aaf8, 4], [0x0053a450, 4], [0x005787cc, 4], [0x00561258, 4],
    [0x0056a51c, 4], [0x0059ae7c, 4], [0x004c2e28, 4], [0x005a59f8, 4]
  ];
  globalThis.__c3fbBioRegions = BIO_REGIONS;
  // flag patterns by r % 4: [a8, a9] per residue. A: every class from bits; B: class 5 from the no-bit even arm
  // (0x0043179c..0x004317b0), class 3 from bit 0x01 alone; C: classes 3 and 5 from the parity arm only for r % 4 in {0, 1}
  const BIO_FLAGS = {
    A: [[0x0c, 0x00], [0x11, 0x00], [0x00, 0x80], [0x1d, 0x00]],
    B: [[0x00, 0x00], [0x01, 0x00], [0x40, 0x80], [0x15, 0x01]],
    C: [[0x00, 0x00], [0x00, 0x00], [0xff, 0x80], [0x0d, 0x7f]],
  };
  const BIO_SCENES = [
    { seed: 0x00000001, flags: 'A', name: 'Bob', ret: 0 },
    { seed: 0x12345678, flags: 'A', name: 'Al', ret: 1 },
    { seed: 0xdeadbeef, flags: 'B', name: 'Carmen', ret: 7 },
    { seed: 0x0000ffff, flags: 'B', name: 'Q', ret: -1 },
    { seed: 0x7fffffff, flags: 'C', name: 'Dee', ret: 0x1234 },
    { seed: 0x80000000, flags: 'C', name: 'Evangeline', ret: 2 },
    { seed: 0x00c3fb00, flags: 'A', name: '', ret: 3 },
    { seed: 0x31415926, flags: 'B', name: 'Zed', ret: 0x7fffffff },
  ];
  let bioCur = BIO_SCENES[0];
  function bioScene(sc) {
    return function () {
      bioCur = sc;
      wr32(0x00822d9c, sc.seed);
      const pat = BIO_FLAGS[sc.flags];
      for (let r = 0; r < 76; r++) {
        const rec = 0x004d6088 + 0x230 * r;
        wrStr(rec + 0x10, sc.name === '' ? '' : sc.name + r, 0x10);
        wr8(rec + 0x20, pat[r & 3][0]);
        wr8(rec + 0x21, pat[r & 3][1]);
      }
      return 0;
    };
  }
  F.c3fb_bio = function () {
    const fx = { clog: newClog(0x400), clogCap: 0x400 };
    freezeNow();
    BIO_REGIONS.forEach(function (r) { saveRange(r[0], r[1]); });
    calleeStand(fx, 0, 0x00401750, 0, function () { return bioCur.ret; });   // initRecords
    scenesOn(fx, 'c', BIO_SCENES.map(bioScene));
    return fx;
  };

  // ---- shared by customizeCharacter and loadGolfPro: the pro table (100 records of 0x38 at 0x0058dd50: name +0, byte
  // +0x20 (0xff = unused), +0x21..+0x24, twelve bytes +0x25, skill word +0x36) and the golfer table pattern
  const PRO_TAB = 0x0058dd50, PRO_N = 100, PRO_SZ = 0x38;
  function proTable(seed, unusedEvery) {
    for (let r = 0; r < PRO_N; r++) {
      const b = PRO_TAB + PRO_SZ * r;
      wrStr(b, 'PRO' + r + '-' + seed, 0x20);
      wr8(b + 0x20, (unusedEvery && r % unusedEvery === 3) ? 0xff : ((r * 5 + seed) & 0x07));
      wr8(b + 0x21, (r * 3 + seed) & 0xff);
      wr8(b + 0x22, (r * 7 + 1) & 0x0f);
      wr8(b + 0x23, (r + seed) & 0xff);
      wr8(b + 0x24, (r * 11) & 0xff);
      for (let i = 0; i < 12; i++) wr8(b + 0x25 + i, ((r + i + seed) % 4 === 0) ? 0 : ((r * 13 + i) & 0xff));
      wr16(b + 0x36, 10 + ((r * 17 + seed) % 50));
    }
  }
  const GOLFERS = 0x005794b8, GOLFER_N = 152;
  function golferPattern(seed) {
    const u = new Uint8Array(0x100 * GOLFER_N);
    for (let i = 0; i < u.length; i++) u[i] = (i * 31 + seed + (i >> 8)) & 0xff;
    ptr(GOLFERS).writeByteArray(u.buffer);
  }

  // ====================================================================================================================
  // customizeCharacter 0x0040f190
  // ====================================================================================================================
  // playSound 0x004481b0, showCharacterIntro 0x004065c0 (C2, a screen), updateMembership 0x00421bc0 (the scene's
  // golfer indices, in call order) and loadThemeFile 0x00437fa0 (the scene's answer) are recorders; resetGolfer
  // 0x00426670, Random::range 0x0045c1e0 and clamp 0x00467130 run for real.
  const CC_REGIONS = [
    [GOLFERS - 0x200, 0x9c00 + 0x200],   // the golfer table, and the 0x200 bytes below it that a -1 index writes
    [0x005689e8, 0x320], [0x0051a068, 0x200], [0x004e06c8, 0x40], [0x0055b028, 0x4e4], [0x004c2c9c, 2],
    [0x0059ae7c, 4], [0x005a5a24, 4], [0x004c2e14, 4], [0x0059e7b8, 4], [0x005a59f8, 4], [0x00822d9c, 4],
    // read only (seeded): the pro table, the player's club bytes, level, the three start words, the type flag byte
    [PRO_TAB, PRO_SZ * PRO_N], [0x005a5a04, 0x0c], [0x00822c88, 4], [0x0059b730, 4], [0x00834170, 4],
    [0x00585850, 0x18], [0x004d60a9, 1]
  ];
  // scenes: mask (0x004c2c9c), clubs (12 bytes at 0x005a5a04), pro index, cycle (0x0059ae7c), um (updateMembership
  // answers), load (loadThemeFile answer), level, flags, seed (pattern)
  const CC_SCENES = [
    { mask: 0, clubs: [], pro: 5, cycle: 2, um: [4, 9], load: 0, level: 1, flags: 0x00006000, seed: 1, b30: 0 },
    { mask: 0x10, clubs: [1, 0, 3, 0, 0, 7, 0, 0, 0, 0, 0, 2], pro: 12, cycle: 0x95, um: [3, 6, 11], load: 1, level: 0,
      flags: 0xffffffff, seed: 2, b30: 3 },
    { mask: 0, clubs: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9], pro: 7, cycle: 0x97, um: [5, -1, 8], load: 0, level: 3,
      flags: 0x12345678, seed: 3, b30: 1 },
    { mask: 0x0fff, clubs: [], pro: 99, cycle: 0x7fffffff, um: [150, 151], load: 0, level: 2, flags: 0, seed: 4, b30: 2 },
    { mask: 0x8000, clubs: [5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5], pro: 0, cycle: -2, um: [7, 7, 7], load: 0, level: 5,
      flags: 0x00002000, seed: 5, b30: 15 },
    { mask: 0, clubs: [0, 1], pro: 33, cycle: 6, um: [2, 0], load: 2, level: -1, flags: 0x00004000, seed: 6, b30: 0 },
  ];
  let ccCur = CC_SCENES[0];
  function ccScene(sc) {
    return function () {
      ccCur = sc;
      golferPattern(sc.seed);
      // the reset loop's status bytes +0x21 / +0x22 (0x0040f204..0x0040f213): every combination by g % 6
      for (let g = 0; g < GOLFER_N; g++) {
        const st = [1, 1, 0xff, 0, 2, 0xff][g % 6], nx = [0, 4, 0, 0, 0, 1][g % 6];
        wr8(GOLFERS + 0x100 * g + 0x21, st);
        wr8(GOLFERS + 0x100 * g + 0x22, nx);
      }
      proTable(sc.seed, 0);
      wr8(PRO_TAB + PRO_SZ * sc.pro + 0x20, (sc.seed & 1) ? 0x07 : 0x03);   // bit 2 set / clear (0x0040f38c)
      wr16(0x004c2c9c, sc.mask);
      for (let i = 0; i < 12; i++) wr8(0x005a5a04 + i, sc.clubs[i] || 0);
      wr32(0x004c2e14, sc.pro);
      wr32(0x0059ae7c, sc.cycle);
      wr32(0x00822c88, sc.level);
      wr32(0x0059e7b8, sc.flags);
      wr32(0x0059b730, sc.b30);
      wr32(0x00834170, 0x4000 + sc.seed);
      wr32(0x00585850, 0x1111 * sc.seed); wr32(0x00585854, 0x2222 * sc.seed); wr8(0x00585866, 0x30 + sc.seed);
      wr8(0x004d60a9, 0x80 | sc.seed);
      wr32(0x00822d9c, 0x00c3fb00 + sc.seed);
      return 0;
    };
  }
  F.c3fb_custom = function () {
    const fx = { clog: newClog(0x800), clogCap: 0x800 };
    freezeNow();
    CC_REGIONS.forEach(function (r) { saveRange(r[0], r[1]); });
    calleeStand(fx, 0, 0x004481b0, 5, 0, {});                                                // playSound
    calleeStand(fx, 1, 0x004065c0, 0, 0, {});                                                // showCharacterIntro
    calleeStand(fx, 2, 0x00421bc0, 1, function (n) {                                         // updateMembership
      return ccCur.um[Math.min(n, ccCur.um.length - 1)];
    });
    calleeStand(fx, 3, 0x00437fa0, 3, function () { return ccCur.load; }, { str: { 0: 8 } });   // loadThemeFile
    scenesOn(fx, 'c', CC_SCENES.map(ccScene));
    return fx;
  };
  globalThis.__c3fbCcRegions = CC_REGIONS;

  // ====================================================================================================================
  // loadGolfPro 0x0046c970
  // ====================================================================================================================
  // loadThemeFile 0x00437fa0 is a recorder (answer by call index: the scene's list, cycled); Random::range and clamp
  // run for real. The 18 hole records (0x208 bytes from 0x00575cb8: open byte +0, start byte +1) decide which golfer
  // pairs are built.
  const GP_REGIONS = [
    [0x0058f338, 0xb7c0], [0x00520a28, 0x19a28], [GOLFERS, 0x9c00], [0x00543d10, 0x19a28], [0x004d6088, 0xa640],
    [0x0051a068, 0x200], [0x00572cac, 4], [0x0056a51c, 4], [0x004c2c9c, 2], [0x0059e7b8, 4], [0x005a47e0, 4],
    [0x00822d9c, 4],
    // read only (seeded)
    [PRO_TAB, PRO_SZ * PRO_N], [0x00575cb8, 0x208 * 18], [0x005685f0, 4], [0x0058bcba, 4], [0x00834170, 4],
    [0x00822c88, 4], [0x00543cc4, 4], [0x00571fd4, 4], [0x00567b04, 4], [0x005a5a04, 0x0c], [0x00585850, 0x18]
  ];
  // scenes: holes (open hole indices), level, cash, rep, extra (0x00543cc4), flags, load (answers), seed, unused
  // (every r % unused == 3 gets 0xff), holes1 (0x005685f0)
  const ALL18 = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17];
  const GP_SCENES = [
    { holes: ALL18, level: 0, cash: 4000, rep: 100, extra: 0, flags: 0, load: [0], seed: 1, unused: 0, holes1: 19 },
    { holes: [0, 2, 5], level: 1, cash: 8000, rep: 300, extra: 0, flags: 0x04000000, load: [1, 0], seed: 2, unused: 7,
      holes1: 4 },
    { holes: [1, 3, 17], level: 3, cash: -2000, rep: 50, extra: 2, flags: 0x00200000, load: [0, 1, 1], seed: 3,
      unused: 5, holes1: 1 },
    { holes: [0], level: 2, cash: 20000, rep: 0, extra: 0, flags: 0x04000000, load: [1], seed: 4, unused: 0, holes1: 2 },
    { holes: [], level: 0, cash: 0, rep: 0, extra: 0, flags: 0x00000001, load: [0], seed: 5, unused: 0, holes1: 9 },
    { holes: [0, 1, 2, 3, 4, 5, 6, 7, 8], level: 1, cash: 600, rep: 900, extra: -3, flags: 0, load: [0, 0, 1], seed: 6,
      unused: 4, holes1: 10 },
  ];
  let gpCur = GP_SCENES[0];
  function gpScene(sc) {
    return function () {
      gpCur = sc;
      golferPattern(sc.seed + 0x40);
      proTable(sc.seed, sc.unused);
      for (let h = 0; h < 18; h++) {
        wr8(0x00575cb8 + 0x208 * h, sc.holes.indexOf(h) >= 0 ? 1 + (h & 1) : 0);
        wr8(0x00575cb9 + 0x208 * h, 0x10 + h);
      }
      wr32(0x005685f0, sc.holes1);
      wr16(0x0058bcba, 3 + sc.seed); wr16(0x0058bcbc, 0xfffe + sc.seed);
      wr32(0x00834170, 0x6000 + sc.seed);
      wr32(0x00822c88, sc.level);
      wr32(0x00543cc4, sc.extra);
      wr32(0x00571fd4, sc.cash * 200);
      wr32(0x00567b04, sc.rep);
      wr32(0x0059e7b8, sc.flags);
      for (let i = 0; i < 12; i++) wr8(0x005a5a04 + i, (i + sc.seed) % 3 === 0 ? 0 : i + 1);
      wr32(0x00585850, 0x1357 * sc.seed); wr32(0x00585854, 0x2468 * sc.seed); wr8(0x00585866, 0x40 + sc.seed);
      wr32(0x00822d9c, 0x5eed0000 + sc.seed);
      return 0;
    };
  }
  F.c3fb_golfpro = function () {
    const fx = { clog: newClog(0x1000), clogCap: 0x1000 };
    freezeNow();
    GP_REGIONS.forEach(function (r) { saveRange(r[0], r[1]); });
    calleeStand(fx, 0, 0x00437fa0, 3, function (n) { return gpCur.load[n % gpCur.load.length]; }, { str: { 0: 8 } });
    scenesOn(fx, 'c', GP_SCENES.map(gpScene));
    return fx;
  };
  globalThis.__c3fbGpRegions = GP_REGIONS;

  // ====================================================================================================================
  // buildTournamentPrep 0x0046d200
  // ====================================================================================================================
  // showPopup 0x0046d6e0 (its answer is the scene's option mask) and stampCourseTiles 0x0040e000 (its answer, the
  // placed object's index, cycles through the scene's list) are recorders; __itoa, Random::range and tileBlocked run for
  // real. Seeded: the 18 hole records (0x208 bytes from 0x00575cb8: par +0, walk direction +1, length word +4, marker x
  // +0x18, y +0x1c), the tile-type grid 0x005722e8 (50 x 50), the type-class bytes 0x00578376 + 0x30 * type, the tile
  // flag words 0x0053caf0, the hole count 0x005685f0 and the flag word.
  const HOLE0 = 0x00575cb8, HSZ = 0x208;
  const TP_REGIONS = [
    [0x0051a068, 0x600], [HOLE0 - HSZ, HSZ * 19], [0x005a47e0, 4], [0x0058bcb8, 0x1000], [0x00838eac, 0x10],
    [0x00822d9c, 4],
    // read only (seeded)
    [0x005722e8, 0x9c4], [0x00578376, 0x30 * 32], [0x0053caf0, 0x1388], [0x005685f0, 4], [0x0059e7b8, 4]
  ];
  const LENS = [100, 299, 300, 499, 500, 650, -5, 0x7fff];
  const PARS = [3, 4, 5, 3, 4, 5, 2, 6, 3, 4, 5, 4, 3, 5, 4, 3, 5, 4];
  const TP_SCENES = [
    { open: ALL18, lenShift: 0, popup: 0xfffff, flags: 0, holes1: 19, stamp: [3, 0, 255, 17], seed: 1, start: [10, 10] },
    { open: ALL18, lenShift: 1, popup: 0x000a9, flags: 0, holes1: 10, stamp: [5], seed: 2, start: [0, 49] },
    { open: [0, 3, 6, 9], lenShift: 2, popup: 0, flags: 0, holes1: 4, stamp: [1], seed: 3, start: [25, 25] },
    { open: ALL18, lenShift: 3, popup: 0x12345, flags: 0x04000000, holes1: 18, stamp: [7, 8], seed: 4, start: [48, 1] },
    { open: [1, 2, 16, 17], lenShift: 4, popup: 0xffffe, flags: 0, holes1: 17, stamp: [2], seed: 5, start: [3, 40] },
    { open: [0, 1, 2], lenShift: -1, popup: 0x00001, flags: 0, holes1: 3, stamp: [9, 4], seed: 6, start: [20, 30] },
  ];
  let tpCur = TP_SCENES[0];
  function tpScene(sc) {
    return function () {
      tpCur = sc;
      for (let h = 0; h < 18; h++) {
        const b = HOLE0 + HSZ * h;
        const open = sc.open.indexOf(h) >= 0;
        // lenShift -1: every par already matches its length class (no change line, n = 0 -> the jle at 0x0046d4c7)
        const len = sc.lenShift < 0 ? [100, 350, 600][h % 3] : LENS[(h + sc.lenShift) % LENS.length];
        const par = sc.lenShift < 0 ? [3, 4, 5][h % 3] : PARS[h];
        wr8(b, open ? par : 0);
        wr8(b + 1, (h * 3 + sc.seed) & 7);
        wr16(b + 4, len);
        wr32(b + 0x18, (sc.start[0] + 3 * h) % 50);
        wr32(b + 0x1c, (sc.start[1] + 5 * h) % 50);
      }
      wr32(0x005685f0, sc.holes1);
      wr32(0x0059e7b8, sc.flags);
      // tile types: 0x14 (blocked by tileBlocked), 0x16 (skipped at 0x0046d5ea) and a spread of others
      const u = new Uint8Array(2500);
      for (let i = 0; i < 2500; i++) {
        const v = (i * 7 + (i / 50 | 0) * 3 + sc.seed) % 26;
        u[i] = v;
      }
      ptr(0x005722e8).writeByteArray(u.buffer);
      for (let t = 0; t < 32; t++) wr8(0x00578376 + 0x30 * t, [4, 0x0d, 0, 4, 0x0d, 7, 4, 0][t % 8]);
      wr8(0x00578376 + 0x30 * 0x14, 4); wr8(0x00578376 + 0x30 * 0x16, 0x0d);
      const w = new Uint8Array(0x1388);
      for (let i = 0; i < 2500; i++) if ((i * 13 + sc.seed) % 11 === 0) { w[2 * i] = 1; w[2 * i + 1] = 0x80; }
      ptr(0x0053caf0).writeByteArray(w.buffer);
      wr32(0x00822d9c, 0x70000000 + sc.seed * 0x1111);
      return 0;
    };
  }
  F.c3fb_tourprep = function () {
    const fx = { clog: newClog(0x800), clogCap: 0x800 };
    freezeNow();
    TP_REGIONS.forEach(function (r) { saveRange(r[0], r[1]); });
    calleeStand(fx, 0, 0x0046d6e0, 5, function () { return tpCur.popup; });                       // showPopup
    calleeStand(fx, 1, 0x0040e000, 4, function (n) { return tpCur.stamp[n % tpCur.stamp.length]; });   // stampCourseTiles
    scenesOn(fx, 'c', TP_SCENES.map(tpScene));
    return fx;
  };

  // ====================================================================================================================
  // demolishObject 0x0040a4e0
  // ====================================================================================================================
  // tileBlocked 0x0040bf60 runs for real (golf_math); objectAt 0x0040df80, showPopup 0x0046d6e0, clearObjectFootprint
  // 0x0040e400, placeHabitat 0x00405e30, pointsPopup 0x0040c890, playSound 0x004481b0, rebuildHeightfield 0x0042f7a0
  // and markTileKind11 0x0042f2c0 are recorders (objectAt answers the case's object index, showPopup its answer, the
  // three that end a path answer a distinct word so the function's eax shows which exit ran). One case per vector: the
  // getter writes the cell (x, y) of the four 50 x 50 grids and the globals the case names.
  const DM_REGIONS = [
    [0x0053caf0, 0x1388], [0x005830b8, 0x9c4], [0x005722e8, 0x9c4], [0x0059c090, 0x9c4], [HOLE0 - HSZ, HSZ * 33],
    [0x005685f0, 4], [0x00571fd4, 4], [0x00584216 - 20 * 99, 20 * 199 + 2], [0x004c2e04, 4], [0x005a47e4, 4],
    [0x0051a068, 0x200], [0x0059e7b8, 4], [0x005a6d3c, 2], [0x0058bcb8, 0x1000], [0x004c26b0, 0x14 * 8]
  ];
  // w flag word, s structure byte, t tile type, c refund, obj objectAt answers, pop showPopup answer, fl flag word,
  // h1 hole count, yr year word
  const DM_CASES = [
    { x: -1, y: 5 },                                                         // tileBlocked (x < 0)
    { x: 12, y: 50 },                                                        // tileBlocked (y > 49)
    { x: 10, y: 10, t: 0x14 },                                               // tileBlocked (type 0x14)
    { x: 10, y: 11, w: 0x8123 },                                             // bit 15: the refusal sound
    { x: 10, y: 12, w: 0x0401, obj: [3], pop: 0 },                           // object, confirmed
    { x: 10, y: 13, w: 0x0402, obj: [3], pop: 1 },                           // object, declined
    { x: 10, y: 14, w: 0x0400, obj: [-1], s: 0x90, c: 7 },                   // no object, habitat building 0x10
    { x: 11, y: 14, w: 0x1020, s: 0x93, c: -5 },                             // building 0x13, flag 0x1000 set
    { x: 11, y: 15, w: 0x0222, s: 0x81, c: 3 },                              // building 1: clears bit 0x200
    { x: 11, y: 16, w: 0x00ff, s: 0x80, c: 1 },                              // building 0: clears bit 0x20
    { x: 11, y: 17, w: 0x0fff, s: 0x85, c: 2, yr: 2007 },                    // other building, no 0x1000
    { x: 11, y: 18, w: 0x0003, s: 0x7d, c: 4 },                              // structure >= 0x7d: refund only
    { x: 12, y: 1, w: 0x0003, s: 0x20, t: 0, fl: 0x200000 },                 // flag 0x200000, tile 0: sound
    { x: 12, y: 2, w: 0x0083, s: 0x20, t: 1, fl: 0x200000 },                 // flag, tile 1 with bit 0x80: sound
    { x: 12, y: 3, w: 0x0003, s: 0x21, t: 1, fl: 0x200000, h1: 9, c: 6 },    // flag, tile 1, hole 3 < 9
    { x: 12, y: 4, w: 0x0045, s: 0x22, t: 0, h1: 9, c: -9 },                 // tile 0, hole 5 < 9
    { x: 12, y: 5, w: 0x0040, s: 0x23, t: 0, h1: 9 },                        // tile 0, hole 0
    { x: 12, y: 6, w: 0x0007, s: 0x24, t: 2, h1: 2 },                        // tile 2: neither
    { x: 12, y: 7, w: 0x0007, s: 0x25, t: 1, h1: 4 },                        // tile 1, hole 7 >= 4
    { x: 12, y: 8, w: 0x0047, s: 0x26, t: 0, h1: 7 },                        // tile 0, hole 7 == holes
    { x: 12, y: 9, w: 0x0001, s: 0x30, t: 1, h1: 18, yr: -150, c: 11 },      // negative year: slot -50
    { x: 13, y: 1, w: 0x0207, s: 0xff },                                     // nothing built, bit 0x200
    { x: 13, y: 2, w: 0x0403, s: 0xff, obj: [2], pop: 0 },                   // nothing built, object confirmed
    { x: 13, y: 3, w: 0x0423, s: 0xff, obj: [-1] },                          // object lookup fails, bit 0x20
    { x: 13, y: 4, w: 0x1400, s: 0xff, obj: [-1] },                          // lookup fails, bit 0x1000
    { x: 13, y: 5, w: 0x1003, s: 0xff },                                     // bit 0x1000
    { x: 13, y: 6, w: 0x0000, s: 0xff, t: 0, h1: 5 },                        // tile 0, hole 0 (no h != 0 test here)
    { x: 13, y: 7, w: 0x0083, s: 0xff, t: 1, h1: 6 },                        // tile 1, bit 0x80, hole 3
    { x: 13, y: 8, w: 0x0003, s: 0xff, t: 1, h1: 6 },                        // tile 1 without 0x80
    { x: 13, y: 9, w: 0x0009, s: 0xff, t: 3, h1: 6 },                        // tile 3
    { x: 13, y: 10, w: 0x0402, s: 0xff, obj: [1], pop: 2 },                  // object declined (answer 2)
    { x: 49, y: 49, w: 0x0011, s: 0xff, t: 0, h1: 30 },                      // corner tile, hole 17
    { x: 14, y: 1, w: 0x0003, s: 0x27, t: 2, fl: 0x200000 },                 // flag, tile 2: neither sound test
    { x: 14, y: 2, w: 0x0040, s: 0x28, t: 1 },                               // tile 1, hole 0 (0x0040a703 taken)
    { x: 14, y: 3, w: 0x0402, s: 0xff, obj: [-1, 4], pop: 0 },               // first lookup fails, the second finds 4
    { x: 14, y: 4, w: 0x0009, s: 0xff, t: 0, h1: 5 },                        // nothing built, tile 0, hole 9 >= 5
    { x: 14, y: 5, w: 0x0089, s: 0xff, t: 1, h1: 5 },                        // nothing built, tile 1 + 0x80, hole 9 >= 5
  ];
  let dmCur = DM_CASES[0];
  function dmScene(cs) {
    return function () {
      dmCur = cs;
      for (let i = 0; i < 8; i++) wrStr(0x004c26b0 + 0x14 * i, 'OBJ' + i, 0x14);
      for (let k = 0; k < 256; k++) wr16(0x0058bcb8 + 16 * k, k & 7);   // the placed objects' type words
      // a neutral grid around the cell, then the case's own values
      const x = cs.x, y = cs.y;
      if (x >= 0 && x < 50 && y >= 0 && y < 50) {
        const i = x * 50 + y;
        wr16(0x0053caf0 + 2 * i, cs.w || 0);
        wr8(0x005830b8 + i, cs.s === undefined ? 0xff : cs.s);
        wr8(0x005722e8 + i, cs.t === undefined ? 2 : cs.t);
        wr8(0x0059c090 + i, cs.c || 0);
      }
      wr32(0x0059e7b8, cs.fl || 0);
      wr32(0x005685f0, cs.h1 === undefined ? 10 : cs.h1);
      wr16(0x005a6d3c, cs.yr === undefined ? 1234 : cs.yr);
      wr32(0x00571fd4, 50000);
      for (let h = 0; h < 32; h++) { wr8(HOLE0 - HSZ + HSZ * h, 3 + (h & 1)); wr32(HOLE0 - HSZ + HSZ * h + 8, 0x1000 + h); wr32(HOLE0 - HSZ + HSZ * h + 0x18, 0x2000 + h); }
      return 0;
    };
  }
  F.c3fb_demolish = function () {
    const fx = { clog: newClog(0x800), clogCap: 0x800 };
    freezeNow();
    DM_REGIONS.forEach(function (r) { saveRange(r[0], r[1]); });
    calleeStand(fx, 0, 0x0040df80, 2, function (n) { const o = dmCur.obj || [-1]; return o[Math.min(n, o.length - 1)]; });   // objectAt
    calleeStand(fx, 1, 0x0046d6e0, 5, function () { return dmCur.pop || 0; });                  // showPopup
    calleeStand(fx, 2, 0x0040e400, 1, 0x0c1ea400);                                              // clearObjectFootprint
    calleeStand(fx, 3, 0x00405e30, 3, 0);                                                       // placeHabitat
    calleeStand(fx, 4, 0x0040c890, 4, 0);                                                       // pointsPopup
    calleeStand(fx, 5, 0x004481b0, 5, 0x50d00018);                                              // playSound
    calleeStand(fx, 6, 0x0042f7a0, 0, 0);                                                       // rebuildHeightfield
    calleeStand(fx, 7, 0x0042f2c0, 0, 0x4b11d011);                                              // markTileKind11
    scenesOn(fx, 'c', DM_CASES.map(dmScene));
    return fx;
  };
  globalThis.__c3fbDmCases = DM_CASES;

  // ====================================================================================================================
  // OptionsMenuBuild 0x00432720
  // ====================================================================================================================
  // Every callee is a recorder: nearestMenuSpot 0x004326a0 answers the case's spot; showPopup 0x0046d6e0 answers the
  // case's list in call order; promptAndSaveFile 0x00437910 likewise; textInputBox 0x0045b2c0 writes the case's typed
  // name into the text buffer; sanitizeFileName 0x00405ac0 answers the case's value.
  const OM_REGIONS = [
    [0x0051a068, 0x200], [0x00567afc, 4], [0x004c2848, 4], [0x0053df54, 4], [0x004c2854, 4], [0x0059b734, 4],
    [0x00822d68, 4], [0x004d6098, 0x20]
  ];
  const OM_CASES = [];
  // spots 0..2 against each speed they test, spot 3 with 0x00822d68 = 2, every Information answer, every System answer
  [0, 1, 2, 5].forEach(function (sp) { OM_CASES.push({ spot: 0, speed: sp }); });
  [0, 1, 2, 5].forEach(function (sp) { OM_CASES.push({ spot: 1, speed: sp }); });
  [0, 1, 2, 5].forEach(function (sp) { OM_CASES.push({ spot: 2, speed: sp }); });
  OM_CASES.push({ spot: 3, mode: 2 });
  for (let a = 0; a <= 12; a++) OM_CASES.push({ spot: 3, pop: [a] });
  OM_CASES.push({ spot: 3, pop: [-1] });
  for (let a = 0; a <= 8; a++) if (a !== 3 && a !== 4) OM_CASES.push({ spot: 5, pop: [a] });
  OM_CASES.push({ spot: 5, pop: [-1] });
  OM_CASES.push({ spot: 5, pop: [3], prompt: [1] });                         // saved at once
  OM_CASES.push({ spot: 5, pop: [3, 3, 0], prompt: [0, 0, 1] });             // two failed saves, then answer 0
  OM_CASES.push({ spot: 5, pop: [3, 3], prompt: [0, 1] });                   // a failed save, then saved
  OM_CASES.push({ spot: 5, pop: [3, 9], prompt: [0] });                      // a failed save, then an answer above 7
  OM_CASES.push({ spot: 5, pop: [4], typed: 'New Name', san: 1 });           // renamed
  OM_CASES.push({ spot: 5, pop: [4], typed: 'Bad', san: 0 });                // rejected name: the sound
  OM_CASES.push({ spot: 5, pop: [4], typed: '', san: 1 });                   // nothing typed: the sound
  [4, 6, 7, 8, 9, 10, 11, -1].forEach(function (sp) { OM_CASES.push({ spot: sp }); });
  let omCur = OM_CASES[0];
  function omScene(cs) {
    return function () {
      omCur = cs;
      wr32(0x00567afc, cs.speed === undefined ? 3 : cs.speed);
      wr32(0x00822d68, cs.mode === undefined ? 0 : cs.mode);
      wrStr(0x004d6098, 'C3FB Links', 0x20);
      wrStr(0x0051a068, 'stale text', 0x20);
      return 0;
    };
  }
  function seq(list, n, dflt) { return list ? list[Math.min(n, list.length - 1)] : dflt; }
  F.c3fb_options = function () {
    const fx = { clog: newClog(0x800), clogCap: 0x800 };
    freezeNow();
    OM_REGIONS.forEach(function (r) { saveRange(r[0], r[1]); });
    calleeStand(fx, 0, 0x004326a0, 2, function () { return omCur.spot; });                           // nearestMenuSpot
    calleeStand(fx, 1, 0x0046d6e0, 5, function (n) { return seq(omCur.pop, n, 0); },
                { extra: function () { return [ptr(0x0051a068).readU32(), ptr(0x0051a068).add(4).readU32()]; } });   // showPopup
    calleeStand(fx, 2, 0x00455a30, 0, 0);                                                            // showBestScoresScreen
    calleeStand(fx, 3, 0x00473470, 0, 0);                                                            // drawScoreSummary
    calleeStand(fx, 4, 0x00437910, 3, function (n) { return seq(omCur.prompt, n, 0); });             // promptAndSaveFile
    calleeStand(fx, 5, 0x0040cb00, 3, 0);                                                            // startMessage
    calleeStand(fx, 6, 0x0040daa0, 1, 0);                                                            // appendCourseTitle
    calleeStand(fx, 7, 0x0045b2c0, 4, function () {                                                  // textInputBox
      wrStr(0x0051a068, omCur.typed === undefined ? 'X' : omCur.typed, 0x20);
      return 0;
    }, { str: { 0: 6 } });
    calleeStand(fx, 8, 0x00405ac0, 1, function () { return omCur.san === undefined ? 1 : omCur.san; }, { str: { 0: 6 } });   // sanitizeFileName
    calleeStand(fx, 9, 0x0045b8b0, 1, 0x11);                                                         // appendToBuffer45b8b0
    calleeStand(fx, 10, 0x004481b0, 5, 0);                                                           // playSound
    calleeStand(fx, 11, 0x00432560, 0, 0);                                                           // showPreferencesMenu
    scenesOn(fx, 'c', OM_CASES.map(omScene));
    return fx;
  };
  globalThis.__c3fbOmCases = OM_CASES;

  // ====================================================================================================================
  // showCourseRanking 0x0045f870
  // ====================================================================================================================
  // fillWindowRect 0x00480b00, setDrawTarget 0x004762d0, drawCenteredText2 0x00404bc0, drawTextSurface 0x00404ad0 and
  // Image_draw 0x00473e60 are recorders (the drawn text is logged); appendCourseTitle 0x0040daa0 is a recorder that
  // appends a fixed title, so the player's row has a name; __itoa runs for real. Every score is >= 0, so each of the ten
  // rounds picks a record and the player's record 9 is always drawn (the rank byte is written before it is read).
  const CR_REGIONS = [
    [0x0051a068, 0x100], [0x00824134, 0x10], [0x005422f8, 0x18 * 10], [0x005a6d3c, 2], [0x005a9cac, 0x10],
    [0x00561258, 4], [0x00834170, 4]
  ];
  // scenes: counts (9 records x 5), scores (9), player's five counts, year word, date
  const CR_SCENES = [
    { scores: [900, 800, 700, 600, 500, 400, 300, 200, 100], me: [50, 50, 50, 50, 50], yr: 5, date: 5 * 8192 + 77 },
    { scores: [10, 20, 30, 40, 50, 60, 70, 80, 90], me: [0, 0, 0, 0, 0], yr: 0, date: 0 },
    { scores: [100, 100, 100, 100, 100, 100, 100, 100, 100], me: [20, 20, 20, 20, 20], yr: 12, date: 12 * 8192 },
    { scores: [5, 9, 1, 7, 3, 8, 2, 6, 4], me: [1, 1, 1, 1, 1], yr: -3, date: -1 },
    { scores: [0, 0, 0, 0, 0, 0, 0, 0, 0], me: [0, 0, 0, 0, 0], yr: 1, date: 8192 },
    { scores: [1000, 2000, 3000, 1500, 2500, 3500, 500, 4000, 4500], me: [700, 600, 500, 400, 300], yr: 30, date: 30 * 8192 + 8191 },
    { scores: [0x7fffffff, 1, 2, 3, 4, 5, 6, 7, 8], me: [1, 0, 0, 0, 0], yr: 2, date: 2 * 8192 },
    { scores: [50, 49, 48, 47, 46, 45, 44, 43, 42], me: [10, 10, 10, 10, 9], yr: 7, date: 7 * 8192 },
  ];
  function crScene(sc, i) {
    return function () {
      for (let k = 0; k < 9; k++) {
        const b = 0x005422f8 + 0x18 * k;
        for (let j = 0; j < 5; j++) wr32(b + 4 * j, [0, 1, 2, 3, -1, 0x80000000][(k + j + i) % 6]);
        wr32(b + 0x14, sc.scores[k]);
      }
      for (let j = 0; j < 4; j++) wr32(0x005a9cac + 4 * j, sc.me[j]);
      wr32(0x00561258, sc.me[4]);
      wr16(0x005a6d3c, sc.yr);
      wr32(0x00834170, sc.date);
      wrStr(0x0051a068, 'stale', 8);
      return 0;
    };
  }
  F.c3fb_ranking = function () {
    const fx = { clog: newClog(0x4000) };
    freezeNow();
    CR_REGIONS.forEach(function (r) { saveRange(r[0], r[1]); });
    calleeStand(fx, 0, 0x00480b00, 6, 0, { abi: 'thiscall' });                                   // fillWindowRect
    calleeStand(fx, 1, 0x004762d0, 5, 0, { abi: 'thiscall' });                                   // setDrawTarget
    calleeStand(fx, 2, 0x00404bc0, 4, function (n) { return 0x2c0 + n; }, { str: { 0: 16 } });   // drawCenteredText2
    calleeStand(fx, 3, 0x0040daa0, 1, function () {                                              // appendCourseTitle
      const p = ptr(0x0051a068); let l = 0; while (l < 0x80 && p.add(l).readU8() !== 0) l++;
      p.add(l).writeUtf8String('C3FB Links');
      return 0;
    });
    calleeStand(fx, 4, 0x00404ad0, 4, 0, { str: { 0: 8 } });                                     // drawTextSurface
    calleeStand(fx, 5, 0x00473e60, 5, 0, { abi: 'thiscall' });                                   // Image_draw
    scenesOn(fx, 'c', CR_SCENES.map(crScene));
    return fx;
  };

  // ====================================================================================================================
  // drawEmployeeViewPanel 0x00435760
  // ====================================================================================================================
  // Recorders: drawEmployeeDetailPanel 0x00436e50, Surface_stretchBlit 0x00473f60, hitEmployeeSlot 0x00435570 (the
  // scene's two answers in call order), Image_draw 0x00473e60, setDrawTarget 0x004762d0, Surface_drawScaled 0x00473cb0,
  // buildGolferName 0x004676e0 (appends `G<g>` to the text buffer, so each drawn name is the golfer's), drawText4049d0
  // 0x004049d0 and drawCenteredText 0x00404b70 (text logged), fillWindowRect 0x00480b00, drawMapLine 0x004493d0.
  // clamp and __itoa run for real. The golfer table is a per-scene pattern (status +0x21, partner +0xa2, mood +0xa4,
  // colour words +0xa6 / +0xa8 / +0xaa, flag dword +0x10, clubs +0xb0 / +0xb2).
  const EV_REGIONS = [
    [GOLFERS, 0x9c00], [0x005a9cf0, 0x260], [0x005aa6c8, 4], [0x005a9f54, 4], [0x005aa554, 4], [0x0051a068, 0x80],
    [0x005a9f6c, 0x10], [0x00561254, 4], [0x005a5a24, 4], [0x004c2844, 4], [0x004d6098, 0x20]
  ];
  function golferPanelPattern(seed, allIdle) {
    for (let g = 0; g < GOLFER_N; g++) {
      const b = GOLFERS + 0x100 * g, k = (g + seed) % 8;
      wr8(b + 0x21, allIdle ? 0 : [1, 0, 0x13, 0xff, 5, 12, 0, 1][k]);
      // partners: g % 6 = 0 / 1 a mutual pair (g, g + 1), 2 and 5 none, 3 and 4 one-sided
      const pm = g % 6;
      wr16(b + 0xa2, pm === 0 ? g + 1 : pm === 1 ? g - 1 : (pm === 2 || pm === 5) ? 0xffff : (g + 3) % GOLFER_N);
      wr16(b + 0xa4, [-5, 0, 3, 8, 12, -1, 1, 9][(g * 3 + seed) % 8]);
      wr16(b + 0xa6, [0, 0x11, 5, 0x10][(g + seed) % 4]);
      wr16(b + 0xa8, [0x10, 0, 0x11, 3][(g * 5 + seed) % 4]);
      wr16(b + 0xaa, [0xa0, 0xa1, 0, 0x7fff][(g * 7 + seed) % 4]);
      wr32(b + 0x10, ((g + seed) % 5 === 0) ? 0x20000000 : 0x00000001);
      wr16(b + 0xb0, (g % 8 === 3) ? 0xffff : 0);
      wr16(b + 0xb2, [0, 1, 3, 5, -2][(g + seed) % 5]);
    }
  }
  const EV_SCENES = [
    { detail: 1, mode: 2, scroll: 0, hits: [-3, -3], last: -3, held: 10, newest: 5, seed: 1 },
    { detail: 0, mode: 2, scroll: 0, hits: [-3, -3], last: -3, held: 10, newest: 5, seed: 1 },
    { detail: 0, mode: 3, scroll: 5, hits: [-4, -4], last: -4, held: 12, newest: 100, seed: 2 },
    { detail: 0, mode: 4, scroll: 0, hits: [-5, -2], last: -2, held: 20, newest: 151, seed: 3 },
    { detail: 0, mode: 4, scroll: 200, hits: [-6, 7], last: 7, held: 3, newest: 0, seed: 4 },
    { detail: 0, mode: 5, scroll: 2, hits: [-1, -1], last: -1, held: 50, newest: 77, seed: 5 },
    { detail: 0, mode: 0, scroll: 0, hits: [0, 5], last: 9, held: 0, newest: 33, seed: 6 },
    { detail: 0, mode: 4, scroll: 0, hits: [-2, -2], last: -2, held: 30, newest: 12, seed: 7, idle: true },
    { detail: 0, mode: 1, scroll: 1, hits: [-2, -2], last: -2, held: 11, newest: 140, seed: 8 },
    { detail: 0, mode: 4, scroll: 3, hits: [-7, -7], last: -7, held: 15, newest: 64, seed: 9 },
    { detail: 0, mode: 2, scroll: 40, hits: [-4, -3], last: -4, held: 2, newest: 90, seed: 10 },
  ];
  let evCur = EV_SCENES[0];
  function evScene(sc) {
    return function () {
      evCur = sc;
      golferPanelPattern(sc.seed, sc.idle);
      wr32(0x00561254, sc.detail);
      wr32(0x004c2844, sc.mode);
      wr32(0x005aa6c8, sc.scroll);
      wr32(0x005a9f54, sc.last);
      wr32(0x005aa554, sc.held);
      wr32(0x005a5a24, sc.newest);
      wrStr(0x004d6098, 'C3FB Links', 0x20);
      wrStr(0x0051a068, 'stale', 8);
      return 0;
    };
  }
  function appendName(n, a) {
    const p = ptr(0x0051a068); let l = 0; while (l < 0x60 && p.add(l).readU8() !== 0) l++;
    p.add(l).writeUtf8String('G' + (a[0] | 0));
    return 0;
  }
  F.c3fb_empview = function () {
    const fx = { clog: newClog(0x4000) };
    freezeNow();
    EV_REGIONS.forEach(function (r) { saveRange(r[0], r[1]); });
    calleeStand(fx, 0, 0x00436e50, 2, 0x0de7a11);                                               // drawEmployeeDetailPanel
    calleeStand(fx, 1, 0x00473f60, 6, 0, { abi: 'thiscall' });                                  // Surface_stretchBlit
    calleeStand(fx, 2, 0x00435570, 2, function (n) { return evCur.hits[Math.min(n, 1)]; });      // hitEmployeeSlot
    calleeStand(fx, 3, 0x00473e60, 5, 0, { abi: 'thiscall' });                                  // Image_draw
    calleeStand(fx, 4, 0x004762d0, 5, 0, { abi: 'thiscall' });                                  // setDrawTarget
    calleeStand(fx, 5, 0x00473cb0, 8, 0, { abi: 'thiscall' });                                  // Surface_drawScaled
    calleeStand(fx, 6, 0x004676e0, 2, appendName);                                              // buildGolferName
    calleeStand(fx, 7, 0x004049d0, 4, 0, { str: { 0: 6 } });                                    // drawText4049d0
    calleeStand(fx, 8, 0x00480b00, 6, 0, { abi: 'thiscall' });                                  // fillWindowRect
    calleeStand(fx, 9, 0x004493d0, 7, 0);                                                       // drawMapLine
    calleeStand(fx, 10, 0x00404b70, 4, 0x7e47, { str: { 0: 6 } });                              // drawCenteredText
    scenesOn(fx, 'c', EV_SCENES.map(evScene));
    return fx;
  };

  // ====================================================================================================================
  // drawGolferActionPanel 0x004362f0
  // ====================================================================================================================
  // Recorders: drawEmployeeDetailPanel 0x00436e50, Surface_stretchBlit 0x00473f60, hitGolferPanel 0x00435f00 (the
  // scene's button), Image_draw 0x00473e60, drawMatchScoreboard 0x00461110, typeAtPos 0x0040bfa0 (answers type 3, whose
  // class byte 0x00578372 + 0x30 * 3 is the scene's), setDrawTarget 0x004762d0, drawText4049d0 0x004049d0 (text logged),
  // drawPanelBackdrop 0x00432620 (logs the tooltip text it would draw). __itoa runs for real.
  const GA_REGIONS = [
    [0x005a9f54, 4], [0x005aa554, 4], [0x0051a068, 0x80], [0x005a9f6c, 0x10], [0x00561254, 4], [0x00567afc, 4],
    [0x005a59f8, 4], [GOLFERS, 0x9c00], [0x00578372 + 0x30 * 3, 1], [0x0058f330, 4], [0x005685f0, 4], [0x0059e7b8, 4],
    [0x004c2e14, 4], [0x005a5a04, 0x0c], [PRO_TAB, PRO_SZ * PRO_N]
  ];
  const GA_SCENES = [
    { det: 1, spd: 0, pg: -1, hit: 1, last: 1, held: 20 },                                        // hand-off
    { det: 1, spd: 3, pg: 5, match: 1, hit: 2, last: 2, held: 3 },                                // speed 3: no hand-off; in a match
    { pg: 7, cls: 1, sel: 0, hit: 4, last: 4, held: 11 },                                         // greyed shots; Straight tip
    { pg: 9, cls: 0, sel: -1, hit: 5, last: 5, held: 10 },                                        // rough, sel -1; Fade tip
    { pg: 11, cls: -2, sel: 1, hit: 6, last: 6, held: 30 },                                       // rough, sel 1; Draw tip
    { pg: 13, cls: 0, sel: 3, hit: 7, last: 7, held: 12 },                                        // rough, sel 3; High tip
    { pg: 15, cls: 0, sel: 4, hit: 8, last: 8, held: 99 },                                        // rough, sel 4; Low tip
    { pg: 17, cls: 2, sel: 2, hit: -1, last: -1, held: 4 },                                       // no hover
    { pg: -1, h1: 1, fl: 0, pro: -1, clubs: [1, 0, 2, 0, 0, 0, 0, 0, 0, 3], hit: 1, last: 1, held: 20 },   // Practice tip
    { pg: -1, h1: 5, fl: 0x402000, pro: 3, clubs: [0, 4], hit: 2, last: 2, held: 11 },             // Play <pro> tip
    { pg: -1, h1: 5, fl: 0x002000, pro: -1, clubs: [], hit: 2, last: 2, held: 11 },               // Play vs. a pro
    { pg: -1, h1: 0, fl: 0x000000, pro: 2, clubs: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10], hit: 3, last: 9, held: 5 },   // column wrap
    { pg: 19, cls: 1, sel: 0, hit: 0, last: 0, held: 11 },                                        // hit 0: no tip text
    { pg: -1, h1: 2, fl: 0x2000, pro: 1, clubs: [-3, 12], hit: 9, last: 9, held: 11 },            // Golfers tip
    { pg: 21, cls: 5, sel: 4, hit: 10, last: 10, held: 11 },                                      // Hire tip
    { pg: 23, cls: -1, sel: 2, hit: 11, last: 11, held: 11 },                                     // above the table
    { pg: 25, cls: 1, sel: 1, hit: 3, last: 3, held: 11 },                                        // Begin Tournament
    { pg: 27, cls: 0, sel: 0, hit: 4, last: 3, held: 50 },                                        // rough, sel 0; changed hover
    { pg: 29, cls: 0, sel: 2, hit: 8, last: 8, held: 1 },                                         // rough, sel 2
    { pg: 31, cls: 1, sel: 2, hit: 4, last: 4, held: 11 },                                        // hover on 4 with a shot chosen
    { pg: 33, cls: 0, sel: 0, hit: 8, last: 8, held: 2 },                                         // no shot chosen, hover on 8
    { pg: -1, h1: 1, fl: 0x2000, pro: 2, clubs: [1], hit: 2, last: 2, held: 11 },                 // flag 0x2000 with one hole
  ];
  let gaCur = GA_SCENES[0];
  function gaScene(sc) {
    return function () {
      gaCur = sc;
      wr32(0x00561254, sc.det || 0);
      wr32(0x00567afc, sc.spd || 0);
      wr32(0x005a59f8, sc.pg);
      if (sc.pg >= 0) {
        const b = GOLFERS + 0x100 * sc.pg;
        wr32(b + 0xcc, sc.match ? 2 : -1);
        wr32(b + 0xd4, 10 + sc.pg); wr32(b + 0x04, 20 + sc.pg);
      }
      wr8(0x00578372 + 0x30 * 3, sc.cls === undefined ? 1 : sc.cls);
      wr32(0x0058f330, sc.sel === undefined ? 0 : sc.sel);
      wr32(0x005685f0, sc.h1 === undefined ? 3 : sc.h1);
      wr32(0x0059e7b8, sc.fl || 0);
      wr32(0x004c2e14, sc.pro === undefined ? -1 : sc.pro);
      for (let i = 0; i < 12; i++) wr8(0x005a5a04 + i, (sc.clubs && sc.clubs[i]) || 0);
      for (let r = 0; r < 4; r++) wrStr(PRO_TAB + PRO_SZ * r, 'PRO' + r, 0x10);
      wr32(0x005a9f54, sc.last);
      wr32(0x005aa554, sc.held);
      wrStr(0x0051a068, 'stale', 8);
      return 0;
    };
  }
  F.c3fb_golfaction = function () {
    const fx = { clog: newClog(0x2000), clogCap: 0x2000 };
    freezeNow();
    GA_REGIONS.forEach(function (r) { saveRange(r[0], r[1]); });
    calleeStand(fx, 0, 0x00436e50, 2, 0x0de7a11);                                               // drawEmployeeDetailPanel
    calleeStand(fx, 1, 0x00473f60, 6, 0, { abi: 'thiscall' });                                  // Surface_stretchBlit
    calleeStand(fx, 2, 0x00435f00, 2, function () { return gaCur.hit; });                       // hitGolferPanel
    calleeStand(fx, 3, 0x00473e60, 5, 0, { abi: 'thiscall' });                                  // Image_draw
    calleeStand(fx, 4, 0x00461110, 1, 0);                                                       // drawMatchScoreboard
    calleeStand(fx, 5, 0x0040bfa0, 2, 3);                                                       // typeAtPos
    calleeStand(fx, 6, 0x004762d0, 5, 0, { abi: 'thiscall' });                                  // setDrawTarget
    calleeStand(fx, 7, 0x004049d0, 4, 0, { str: { 0: 8 } });                                    // drawText4049d0
    calleeStand(fx, 8, 0x00432620, 2, 0xbac4d0, {                                               // drawPanelBackdrop
      extra: function () { const w = []; for (let i = 0; i < 6; i++) w.push(ptr(0x0051a068).add(4 * i).readU32()); return w; } });
    scenesOn(fx, 'c', GA_SCENES.map(gaScene));
    return fx;
  };

  // ====================================================================================================================
  // panelCreate4a2400 0x004a2400 (thiscall, 4 stack arguments)
  // ====================================================================================================================
  // A synthetic object: `this` = OBJ + 0x800, its vbtable (three dwords [0, o4, o8], at VB) at this - 0x604; the scene
  // picks o4 / o8 (the real offsets of the class are not established here, so two settings are A/B'd, both keeping the
  // field block F(-0x5e0..-0x54c) = [this + o8 + off] clear of the per-box arrays at this - 0x600 / - 0x5d8 / - 0x5b0).
  // Every callee is a recorder: panelLayoutLabeledText 0x004a2a60, Window::create 0x004806c0 and EditBox::create
  // 0x00486110 (the scene's answers), Widget_setQuad / 70 / 74 0x00476310 / 0x00476340 / 0x00476370, setDrawTarget
  // 0x004762d0, operator_new 0x004a57f1 (block n of the box arena, or NULL at the scene's failing call), EditBox::ctor
  // 0x00486070 (installs the recording vtable VT and an empty text at +0x574), EditBox::setText 0x00486200 (stores its
  // argument at +0x574 when non-zero), EditBox::resizeBuffer 0x00486250. The vtable slots the body calls (0: the
  // deleting destructor of an old box, 1: +4, 0x48: +0x120) log like calleeStand under the tags 0xc3fb0000 + slot.
  const PC_OBJ = 0x1000, PC_SELF = 0x800, PC_ARENA = 0x600 * 12, PC_OLD = 0x100 * 4;
  const PC_SCENES = [
    { a1: 11, o4: 0, o8: 0x100, parentNull: 1 },
    { a1: 12, o4: 0, o8: 0x100, win: 7, rows: 2, cols: 2 },
    { a1: 13, o4: 0, o8: 0x100, rows: 2, cols: 3, pflag: 0x1000, f5e0: 0x1000, s: 6, h: 40, pitch: 30 },
    { a1: 14, o4: 0x40, o8: 0x200, rows: 3, cols: 3, pflag: 0, f5e0: 0, s: 5, h: 25, pitch: 18, old: [0, 4], nullText: [1, 5] },
    { a1: 15, o4: 0, o8: 0x100, rows: 2, cols: 2, failNew: 2, old: [1] },
    { a1: 16, o4: 0x40, o8: 0x200, rows: 2, cols: 3, failCreate: 1, createCode: 0x15 },
    { a1: 17, o4: 0, o8: 0x100, rows: 0, cols: 4 },
    { a1: 18, o4: 0x40, o8: 0x200, rows: 3, cols: 0, f5e0: 0x1001 },
    { a1: 19, o4: 0, o8: 0x100, rows: 2, cols: 2, s: -7, h: -9, pitch: -11, pflag: 0x0fff },
    { a1: 20, o4: 0x40, o8: 0x200, rows: 5, cols: 2, s: 3, h: 12, pitch: 14, f5e0: 0x7000, old: [0, 1, 2, 3, 9] },
    { a1: 21, o4: 0, o8: 0x100, rows: 1, cols: 5, s: 1, h: 2, pitch: 3, pflag: 0x1000, nullText: [0, 2, 4] },
  ];
  let pcCur = PC_SCENES[0];
  function pcVtRecorder(fx, slot, argc, ret) {
    const tid = Process.getCurrentThreadId();
    const log = fx.clog, cap = fx.clogCap || CLOG, tag = (0xc3fb0000 + slot) >>> 0;
    const cb = new NativeCallback(function () {
      const a = Array.prototype.slice.call(arguments);
      if (Process.getCurrentThreadId() === tid) {
        let h = log.add(CHASH).readU32();
        h = fnvWord(h, tag);
        a.forEach(function (v) { h = fnvWord(h, v >>> 0); });
        log.add(CHASH).writeU32(h);
        const curs = log.readU32(), need = 8 + 4 * a.length;
        if (curs + need <= cap) {
          log.add(curs).writeU32(tag); log.add(curs + 4).writeU32(a.length);
          a.forEach(function (v, i) { log.add(curs + 8 + 4 * i).writeU32(v >>> 0); });
          log.writeU32(curs + need);
        }
      }
      return ret >>> 0;
    }, 'uint32', Array(argc).fill('uint32'), 'thiscall');
    __diffKeepAlive.push(cb);
    return cb;
  }
  function pcScene(fx) {
    return function (sc) {
      return function () {
        pcCur = sc;
        const obj = fx.obj, self = obj.add(PC_SELF);
        obj.writeByteArray(new Uint8Array(PC_OBJ).buffer);
        fx.vb.writeU32(0); fx.vb.add(4).writeU32(sc.o4); fx.vb.add(8).writeU32(sc.o8);
        self.sub(0x604).writePointer(fx.vb);
        // the field block F(-0x5e0..-0x54c): a pattern, then the scene's geometry
        for (let off = -0x5e0; off <= -0x54c; off += 4) self.add(sc.o8 + off).writeS32(0x1000 + off + sc.a1);
        const Fw = function (off, v) { self.add(sc.o8 + off).writeS32(v | 0); };
        Fw(-0x5ac, sc.rows === undefined ? 1 : sc.rows);
        Fw(-0x5a8, sc.cols === undefined ? 1 : sc.cols);
        Fw(-0x5bc, sc.s === undefined ? 4 : sc.s);
        Fw(-0x59c, sc.h === undefined ? 20 : sc.h);
        Fw(-0x598, sc.pitch === undefined ? 16 : sc.pitch);
        Fw(-0x5cc, 100 + sc.a1);
        Fw(-0x5e0, sc.f5e0 === undefined ? 0 : sc.f5e0);
        self.sub(0x588).writeS32(7 * sc.a1);                                    // the word read directly (0x004a2582)
        for (let k = 0; k < 10; k++) {
          const isNull = sc.nullText && sc.nullText.indexOf(k) >= 0;
          self.sub(0x5d8).add(4 * k).writePointer(isNull ? ptr(0) : fx.texts.add(0x20 * k));
          self.sub(0x5b0).add(4 * k).writeU32(8 + k);
          self.sub(0x600).add(4 * k).writePointer(sc.old && sc.old.indexOf(k) >= 0 ? fx.old.add(0x40 * k) : ptr(0));
        }
        for (let k = 0; k < 10; k++) fx.old.add(0x40 * k).writePointer(fx.vt);
        fx.parent.add(0x9c).writeU32(sc.pflag === undefined ? 0 : sc.pflag);
        fx.arena.writeByteArray(new Uint8Array(PC_ARENA).buffer);
        return sc.a1;
      };
    };
  }
  F.c3fb_pcreate = function () {
    const fx = { clog: newClog(0x4000), obj: alloc(PC_OBJ), vb: alloc(0x10), parent: alloc(0x100), arena: alloc(PC_ARENA),
                 old: alloc(0x40 * 10), texts: alloc(0x20 * 10), vt: alloc(0x140), empty: alloc(0x10) };
    fx.self = fx.obj.add(PC_SELF);
    for (let k = 0; k < 10; k++) wrStr(fx.texts.add(0x20 * k), 'BOX' + k + '-' + 'x'.repeat(k), 0x20);
    for (let i = 0; i < 0x50; i++) fx.vt.add(4 * i).writePointer(pcVtRecorder(fx, i, 1, 0));
    fx.vt.writePointer(pcVtRecorder(fx, 0, 2, 0));              // slot 0: deleting destructor (this, 1)
    fx.vt.add(4).writePointer(pcVtRecorder(fx, 1, 2, 0));       // slot 1: (this, 0) at 0x004a280a
    fx.vt.add(0x120).writePointer(pcVtRecorder(fx, 0x48, 1, 0x48));   // slot 0x48: (this) at 0x004a27fe
    calleeStand(fx, 0, 0x004a2a60, 1, 0, { abi: 'thiscall' });                                    // panelLayoutLabeledText
    calleeStand(fx, 1, 0x004806c0, 10, function () { return pcCur.win || 0; }, { abi: 'thiscall' });   // Window::create
    calleeStand(fx, 2, 0x00476310, 5, 0, { abi: 'thiscall' });                                    // Widget_setQuad
    calleeStand(fx, 3, 0x00476340, 5, 0, { abi: 'thiscall' });                                    // Widget_setQuad70
    calleeStand(fx, 4, 0x00476370, 5, 0, { abi: 'thiscall' });                                    // Widget_setQuad74
    calleeStand(fx, 5, 0x004762d0, 5, 0, { abi: 'thiscall' });                                    // setDrawTarget
    calleeStand(fx, 6, 0x004a57f1, 1, function (n) {                                              // operator_new
      if (pcCur.failNew !== undefined && n === pcCur.failNew) return 0;
      return fx.arena.add(0x600 * (n % 12)).toUInt32();
    });
    calleeStand(fx, 7, 0x00486070, 1, function (n, a) {                                           // EditBox::ctor
      const p = ptr(a[0]);
      p.writePointer(fx.vt);
      p.add(0x574).writePointer(fx.empty);
      p.add(0x580).writeU32(0x10);
      return a[0];
    }, { abi: 'thiscall' });
    calleeStand(fx, 8, 0x00486110, 7, function (n) {                                              // EditBox::create
      return (pcCur.failCreate !== undefined && n === pcCur.failCreate) ? pcCur.createCode : 0;
    }, { abi: 'thiscall' });
    calleeStand(fx, 9, 0x00486200, 2, function (n, a) {                                           // EditBox::setText
      if (a[1] !== 0) ptr(a[0]).add(0x574).writePointer(ptr(a[1]));
      return 0;
    }, { abi: 'thiscall' });
    calleeStand(fx, 10, 0x00486250, 2, 0, { abi: 'thiscall' });                                   // EditBox::resizeBuffer
    scenesOn(fx, 'c', PC_SCENES.map(pcScene(fx)));
    return fx;
  };

  // ====================================================================================================================
  // Flic::open 0x00481f40 (thiscall, 4 stack arguments)
  // ====================================================================================================================
  // A fixture Flic (+0x44, +0x54 old MappedFile, +0x58 header with the words +0x60 / +0x62, +0x74, +0x78 cache), a
  // recording vtable for the old file (slot 0 = deleting destructor, this + 1) and one for caches (slot 0, this only).
  // Recorders: Flic::reset 0x00481ba0, operator_new 0x004a57f1 (arena block n, or NULL at the scene's failing call),
  // MappedFile::ctor 0x00492d80 / ::open 0x00492dd0 (the scene's answer), Cache482fd0::ctor 0x00482fd0 (installs the
  // cache vtable), Cache483010::dtor 0x00483010, operatorDelete_4a4ffc, setNewHeap 0x004823c0 (block n of a second
  // arena), the two Decoder constructors 0x00482ae0 / 0x00482b20, createCache 0x0049c830 (the pool's slot 0, reached
  // through the live pool vtable 0x004babac: it stores the fixture cache at [arg + 4] and at [0x0083aca4], the two
  // stores its body makes, 0x0049c874 / 0x0049c894), Flic::buildFrameTable 0x00481ca0 / ::indexFrames 0x00481e40 (the
  // scene's answers; only the low words of indexFrames' arguments are logged, see the reimplementation) /
  // ::createStream 0x00482b90. The pool globals 0x0083ac88..0x0083acb0, 0x00839650 and 0x0083ac24 are saved, seeded and
  // compared.
  const FO_ARENA = 0x100 * 8;
  const FO_SCENES = [
    { flags: 0, name: 0, f44: 0 },
    { flags: 0, name: 0, f44: 1, mode: 3 },
    { flags: 0, name: 0, f44: 1, index: 9 },
    { flags: 1, name: 1, oldFile: 1, open: 0 },
    { flags: 1, name: 1, open: 1, source: 0x55 },
    { flags: 1, name: 1, open: 1, failNew: 1 },
    { flags: 1, name: 1, open: 1, f44: 1, mode: 2 },
    { flags: 5, name: 1, open: 1, cache: 1, build: 5 },
    { flags: 2, name: 1, open: 1, cache: 1, count: 0, heap: 0x1234, f44: 1, mode: 1 },
    { flags: 2, name: 1, open: 1, count: 0, heap: 0, f44: 1, mode: 6 },
    { flags: 2, name: 1, open: 1, oldFile: 1, count: 3, heap: 0, f44: 1 },
    { flags: 6, name: 1, open: 1, cache: 1, count: 1, heap: 0x55, f44: 1, index: 2 },
    { flags: 0, name: 1, open: 1, source: 0x777, f44: 0 },
    { flags: 3, name: 1, open: 1, f44: 1, mode: 7 },
    { flags: 0x80000002, name: 1, open: 1, oldFile: 1, cache: 1, count: 2, heap: 0, f44: 1, mode: 2, build: 0 },
    { flags: 1, name: 1, open: 0, failNew: 0 },                     // operator_new(0x14) NULL: open gets this = NULL
  ];
  let foCur = FO_SCENES[0];
  function foVt(fx, tagBase, argc) {
    const vt = alloc(0x10);
    vt.writePointer(pcVtRecorder(fx, tagBase, argc, 0));
    return vt;
  }
  function foScene(fx) {
    return function (sc) {
      return function () {
        foCur = sc;
        const fl = fx.fl;
        fl.writeByteArray(new Uint8Array(0x100).buffer);
        fl.add(0x44).writeU32(sc.f44 || 0);
        fl.add(0x54).writePointer(sc.oldFile ? fx.oldFile : ptr(0));
        fl.add(0x58).writePointer(fx.hdr);
        fl.add(0x78).writePointer(sc.cache ? fx.cache : ptr(0));
        fx.hdr.add(0x60).writeU16(0x10 + (sc.mode || 0)); fx.hdr.add(0x62).writeU16(0x2000 + (sc.flags & 0xff));
        fx.oldFile.writePointer(fx.vtDel);
        fx.cache.writePointer(fx.vtCache);
        fx.cur.writePointer(fx.vtCache);
        fx.arena.writeByteArray(new Uint8Array(FO_ARENA).buffer);
        fx.arena2.writeByteArray(new Uint8Array(FO_ARENA).buffer);
        // the pool: head node with the tail at +0x10, the count, the heap word, the virtual base word 0x0083acac
        fx.head.writeByteArray(new Uint8Array(0x40).buffer);
        fx.head.add(0x10).writePointer(fx.tail);
        fx.tail.writeByteArray(new Uint8Array(0x40).buffer);
        wr32(0x0083ac90, (sc.count || 0) ? fx.head.toUInt32() : 0);
        wr32(0x0083ac94, (sc.count || 0) ? fx.tail.toUInt32() : 0);
        wr32(0x0083ac98, sc.count || 0);
        wr32(0x0083ac9c, 0x99);
        wr32(0x0083aca0, sc.heap || 0);
        wr32(0x0083aca4, 0x0bad);
        wr32(0x0083acac, 0x00c3fb00 + (sc.mode || 0));
        wr32(0x00839650, 0x00839650 + 0x10);
        wr32(0x0083ac24, 0x24);
        return sc.flags >>> 0;
      };
    };
  }
  F.c3fb_flicopen = function () {
    const fx = { clog: newClog(0x2000), clogCap: 0x2000, fl: alloc(0x100), hdr: alloc(0x80), oldFile: alloc(0x40),
                 cache: alloc(0x40), cur: alloc(0x40), arena: alloc(FO_ARENA), arena2: alloc(FO_ARENA),
                 head: alloc(0x40), tail: alloc(0x40), name: alloc(0x20) };
    wrStr(fx.name, 'C3FB.flc', 0x20);
    fx.vtDel = foVt(fx, 0x100, 2);
    fx.vtCache = foVt(fx, 0x200, 1);
    freezeNow();
    [[0x0083ac88, 0x28], [0x00839650, 4], [0x0083ac24, 4]].forEach(function (r) { saveRange(r[0], r[1]); });
    calleeStand(fx, 0, 0x00481ba0, 1, 0, { abi: 'thiscall' });                                   // Flic::reset
    calleeStand(fx, 1, 0x004a57f1, 1, function (n) {                                            // operator_new
      return (foCur.failNew !== undefined && n === foCur.failNew) ? 0 : fx.arena.add(0x100 * (n % 8)).toUInt32();
    });
    calleeStand(fx, 2, 0x00492d80, 1, function (n, a) { return a[0]; }, { abi: 'thiscall' });   // MappedFile::ctor
    calleeStand(fx, 3, 0x00492dd0, 3, function () { return foCur.open || 0; }, { abi: 'thiscall', str: { 1: 4 } });   // MappedFile::open
    calleeStand(fx, 4, 0x00482fd0, 1, function (n, a) { ptr(a[0]).writePointer(fx.vtCache); return a[0]; }, { abi: 'thiscall' });   // Cache482fd0::ctor
    calleeStand(fx, 5, 0x00483010, 1, 0, { abi: 'thiscall' });                                   // Cache483010::dtor
    calleeStand(fx, 6, 0x004a4ffc, 1, 0);                                                       // operatorDelete_4a4ffc
    calleeStand(fx, 7, 0x004823c0, 2, function (n) { return fx.arena2.add(0x100 * (n % 8)).toUInt32(); });   // setNewHeap
    calleeStand(fx, 8, 0x00482ae0, 2, function (n, a) { return a[0]; }, { abi: 'thiscall' });   // Decoder482ae0::ctor
    calleeStand(fx, 9, 0x00482b20, 2, function (n, a) { return a[0]; }, { abi: 'thiscall' });   // Decoder482b20::ctor
    calleeStand(fx, 10, 0x0049c830, 2, function (n, a) {                                        // createCache (pool slot 0)
      ptr(a[1]).add(4).writePointer(fx.cur);
      ptr(0x0083aca4).writePointer(fx.cur);
      return 0;
    }, { abi: 'thiscall' });
    calleeStand(fx, 11, 0x00481ca0, 2, function () { return foCur.build || 0; }, { abi: 'thiscall' });   // Flic::buildFrameTable
    calleeStand(fx, 12, 0x00481e40, 3, function () { return foCur.index || 0; }, { abi: 'thiscall', mask: [1, 2],
      extra: function (a) { return [a[1] & 0xffff, a[2] & 0xffff]; } });                       // Flic::indexFrames
    calleeStand(fx, 13, 0x00482b90, 3, 0, { abi: 'thiscall' });                                  // Flic::createStream
    scenesOn(fx, 'c', FO_SCENES.map(foScene(fx)));
    fx.nm = fx.name;
    return fx;
  };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
