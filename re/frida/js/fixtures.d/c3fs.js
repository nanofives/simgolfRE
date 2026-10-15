// Fixtures for batch c3fs (round 35, 2026-10-14): golf_clean.exe large rows whose originals read frame slots they never
// wrote. Reimplementations: shim/src/re/c3fs.cpp. Registry: re/frida/registry.d/c3fs.py.
// Purposes and per-jcc tables: log/c3/c3fs_purpose.md. Notes: log/c3/c3fs_notes.txt.
//
// Method (helpers copied from c3fj.js, which copied c3fc.js / c3er.js / c3ea.js):
// - Every key freezes the game's first thread (re/frida/js/freeze.js) and saves every global it writes once, right
//   after the freeze; the flush restores them before the resume. __freeze.negOn() arms the negative-control build's
//   exported C3fsNegCtl switch right after the freeze; word 2 of it selects which single wrong detail is live.
// - Callees that draw, queue sprites or project to the screen are stood in on the A/B thread only (calleeStand): each
//   call is counted, folded into a running FNV-1a hash and logged; the log ($clog) is a compared state region. A
//   stand-in's answer depends only on its arguments and on the scene words in the log (compared memory), never on a
//   JS-side counter.
// - Pointer arguments into the caller's frame are masked in the log; the stand-in writes through them, so a body that
//   passes the wrong slot reads RED in the calls that follow.
// - Uninitialised frame slots: a slot the original reads before any write is seeded by a hand-encoded native stand-in
//   over the callee the original calls just before the read (seedStand). The reimplementation calls that callee with
//   esp where the original has it (CallOnFrame in c3fs.cpp), so the stub's [esp + d] is the same frame slot in both arms.
// Scenes: the getter of $sN applies scene N (c3ec's trick: the function takes no argument, the word is an unused cdecl
// stack argument).
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
  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function rng(seed) {
    let s = (seed >>> 0) || 1;
    return function (n) { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; return n ? (s >>> 8) % n : s; };
  }

  // ---- calleeStand: log word 0 = cursor, words 1..32 = call count of stub k, word 0x84 = FNV-1a over every call,
  // words 0x88 / 0x8c = scene words the stubs read (compared memory), entries {va, count, values...} from +0x90.
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x8000;
  const CHASH = 0x84, CSEED = 0x88, CANS = 0x8c, CHEAD = 0x90;
  function newClog(cap) { const l = alloc(cap || CLOG); l.writeU32(CHEAD); l.add(CHASH).writeU32(0x811c9dc5); return l; }
  function resetClog(l, cap) {
    l.writeByteArray(new Uint8Array(cap).buffer);
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

  // ---- seedStand(va, d): a native stand-in over the cdecl callee at va. On the A/B thread it stores cfg+0 into
  // [esp + d] (esp at the callee's entry: d = 4 + the caller's frame offset of the slot, measured from the slot's base
  // after the caller's `push arg`), counts the call in cfg+4, then jumps to the callee's own code (Frida's trampoline;
  // the callee's hook, or its original with the hook off, runs for real). Other threads jump straight through.
  //   +00 50                 push eax           +10 a1 <cfg>          mov eax, [cfg]
  //   +01 64 a1 24 00 00 00  mov eax, fs:[0x24] +15 89 44 24 <d+4>    mov [esp + d + 4], eax
  //   +07 3d <tid>           cmp eax, tid       +19 ff 05 <cfg+4>     inc dword [cfg + 4]
  //   +0c 58                 pop eax            +1f 58                pop eax
  //   +0d 75 11              jne +0x20          +20 ff 25 <cell>      jmp [cell]   (cell = cfg+8 holds the trampoline)
  //   +0f 50                 push eax
  const seedLive = {};
  let seedTok = 1;
  function seedStand(fx, va, d) {
    freezeNow();
    const at = ptr(va), key = at.toString();
    if (seedLive[key]) {
      try { Interceptor.revert(at); } catch (e) { /* already reverted */ }
      delete seedLive[key];
      Interceptor.flush();
    }
    const cfg = fx.seed;
    const tid = Process.getCurrentThreadId();
    const C = cfg.toUInt32();
    const bytes = [0x50, 0x64, 0xa1, 0x24, 0x00, 0x00, 0x00]
      .concat([0x3d], u32(tid))
      .concat([0x58, 0x75, 0x11, 0x50])
      .concat([0xa1], u32(C))
      .concat([0x89, 0x44, 0x24, (d + 4) & 0xff])
      .concat([0xff, 0x05], u32(C + 4))
      .concat([0x58])
      .concat([0xff, 0x25], u32(C + 8));
    if (bytes.length !== 0x26 || d + 4 > 0x7f) throw new Error('c3fs seedStand: bad stub');
    const page = __keepAlloc(Process.pageSize);
    Memory.protect(page, Process.pageSize, 'rwx');
    page.writeByteArray(bytes);
    const tramp = Interceptor.replaceFast(at, page);
    cfg.add(8).writePointer(tramp);
    const tok = seedTok++;
    seedLive[key] = tok;
    Interceptor.flush();
    setTimeout(function () {
      if (seedLive[key] !== tok) return;
      Interceptor.revert(at);
      delete seedLive[key];
      Interceptor.flush();
    }, 0);
  }

  // negative-control selector: word 2 of every exported NegCtl switch (only the negctl build has one)
  function negSel(k) {
    (__freeze.negCtlWords() || []).forEach(function (a) { __freeze.save(a.add(8), 4); a.add(8).writeU32(k); });
  }

  // ====================================================================================================================
  // drawFlyingBall 0x004038f0()
  // ====================================================================================================================
  // Stood in (all record their arguments): drawCenteredText 0x00404b70 (logs the text), drawCircle 0x00407c60,
  // worldToScreen 0x0042fb90 and screenToTile 0x00430020 (write both out slots from their arguments and the scene seed;
  // worldToScreen answers visible / not from the same), drawPanelBackdrop 0x00432620, cellAt 0x0043d6f0 (answers a hash
  // of its arguments and the seed), composeGolferSprite 0x00462020, addSprite 0x004628d0, enqueueSprite 0x00462a30,
  // setDrawTarget 0x004762d0, View_toLocal 0x0047ab50 (writes both in-place slots from the seed and its call count).
  // Seeded: typeBit7Clear 0x0046c940 runs for real behind seedStand, which stores the scene's word into the slot the
  // original reads at 0x00403965 ([F + 0x14]; the call at 0x00403937 enters with esp = F - 8, so d = 0x1c).
  // Real: clamp 0x00467130, pickGolferName 0x00467600, __itoa 0x004ad425.
  const WALK = 0x00585850;                       // 64 walker records of 0x4c
  const GOLF = 0x005794b8;                       // golfer records, 0x100 each
  const TYPES = 0x004d60a9;                      // golfer type records, 0x230 each (+0 flags, +2 animation nibble)
  const FR = 0x0053f3e8;                         // frame-count table, one dword per cell-table id
  const FB_SAVE = [
    [0x004c2e10, 4], [0x005685f4, 4], [0x00834170, 4], [0x0059e7b8, 4], [0x004c2844, 4], [0x005a59f8, 4],
    [0x005795d9, 1], [0x00567afc, 4], [0x004e9a94, 4], [0x004e9a9c, 4], [0x0051a068, 0x80], [0x004e9a88, 0x10],
    [GOLF, 0x1000], [0x00582db8, 0x100], [TYPES, 8 * 0x230], [WALK, 64 * 0x4c], [FR, 4 * 0x2c0]
  ];
  const KINDS = [-6, -5, -4, -3, -2, -1, 0, 3, -7, -128, 127, -2, -3, -4, -5];

  function fbStands(fx, noSeed) {
    calleeStand(fx, 0, 0x00404b70, 4, 0, { str: { 0: 8 } });
    calleeStand(fx, 1, 0x00407c60, 4, 0);
    calleeStand(fx, 2, 0x0042fb90, 5, function (n, a) {
      const seed = fx.clog.add(CSEED).readU32();
      const wx = a[0] | 0, wy = a[1] | 0;
      ptr(a[2]).writeS32((wx >> 5) + (seed & 0xf) + n);
      ptr(a[3]).writeS32((wy >> 5) - 3 - ((seed >>> 4) & 0xf));
      const k = (Math.imul(wx >> 10, 7) + Math.imul(wy >> 10, 3) + (wx & 0x3ff) + (seed & 0xffff)) >>> 0;
      if (seed & 0x80000000) return 1;
      return (k % 5) !== 0 ? 1 : 0;
    }, { mask: [2, 3] });
    calleeStand(fx, 3, 0x00430020, 5, function (n, a) {
      const seed = fx.clog.add(CSEED).readU32();
      ptr(a[2]).writeS32(((a[0] | 0) >> 4) + 3 * n + (seed & 7));
      ptr(a[3]).writeS32(((a[1] | 0) >> 4) - n - ((seed >>> 3) & 7));
      return 0;
    }, { mask: [2, 3] });
    calleeStand(fx, 4, 0x00432620, 2, 0);
    calleeStand(fx, 5, 0x0043d6f0, 3, function (n, a) {
      let h = fnvWord(0x811c9dc5, fx.clog.add(CSEED).readU32());
      h = fnvWord(h, a[0]); h = fnvWord(h, a[1]); h = fnvWord(h, a[2]);
      return ((h & 0x0ffffffc) | 0x20000000) >>> 0;
    });
    calleeStand(fx, 6, 0x00462020, 1, 0);
    calleeStand(fx, 7, 0x004628d0, 8, 0);
    calleeStand(fx, 8, 0x00462a30, 8, 0);
    calleeStand(fx, 9, 0x004762d0, 5, 0x0101, { abi: 'thiscall' });
    calleeStand(fx, 10, 0x0047ab50, 3, function (n, a) {
      const seed = fx.clog.add(CSEED).readU32();
      ptr(a[1]).writeS32(100 + 7 * n + (seed & 0xff));
      ptr(a[2]).writeS32(200 - 3 * n - ((seed >>> 8) & 0x7f));
      return 0;
    }, { abi: 'thiscall', mask: [1, 2] });
    if (!noSeed) seedStand(fx, 0x0046c940, 0x1c);
  }

  // One scene. mode: 'g' (selection = golfer 0x80 + g), 'w' (selection = walker index), 'n' (no selection).
  function fbScene(i, mode, over) {
    const r = rng(0x5eed0000 + 977 * i + (mode === 'g' ? 1 : mode === 'w' ? 2 : 3));
    const s = {
      i: i, mode: mode, g: i % 12, t: i % 8, bit7: (i % 3) === 0, nib: (i * 5) & 0xff, gdir: (i * 3) & 0xff,
      slot: [-5, 0, 3, 9, 10, 0x7fffffff, -0x80000000, 4, 7, 1, 2, 8][i % 12],
      rot: [0, 2, 5, 7, 9, -3, 4, 1][i % 8], date: (i % 4) === 1 ? 2 : (i % 4) === 3 ? 3 : 0x40,
      f4: (i % 5) === 2, f20: (i % 3) !== 1, f80: (i % 7) === 4, f200000: (i % 2) === 1,
      zoom: [4, 5, 3, 6, 4, 2][i % 6], gary: (i % 4) === 0 || (i % 4) === 3 ? -1 : 5 + i, g1b: (i % 3) === 2 ? 1 : 0,
      afc: (i % 5) === 3 ? 3 : 1, cnt: [0, 17, -5, 0x7fffffff, 1234, -0x80000000 + 3][i % 6],
      seed: (Math.imul(0x9e3779b9, i + 1 + (mode === 'w' ? 100 : mode === 'n' ? 200 : 0)) & 0x7fffffff) >>> 0,
      t0: [0x00, 0x80, 0x35, 0xb7][i % 4], t0b: [0x10, 0x25, 0xf3, 0x3c][(i + 1) % 4], fseed: i,
      sel: 0, walkers: []
    };
    if (i % 6 === 5) s.seed = (s.seed | 0x80000000) >>> 0;        // every record visible
    if (over) Object.keys(over).forEach(function (q) { s[q] = over[q]; });
    // walkers: kind, flags, label-off, direction, animation, state; a label (label-off 0 and kind < 0) only for
    // kinds -2..-6, because pickGolferName indexes its name tables by the kind (other kinds read before / past them)
    for (let k = 0; k < 64; k++) {
      const kind = KINDS[(k + i) % KINDS.length];
      const w = {
        kind: kind, flags: r(5) === 0 ? 0 : (r(0x100) | 1), off: r(3) === 0 ? 1 : 0, dir: r(0x100),
        anim: [0, 1, 2, -1, 0x7fff, -0x8000, 37, 100][r(8)] + (k & 3), st: [0, 5, 0xa, 0xb, 0xc, 0xd, -1, 0x7fff][r(8)],
        x: 0x400 * r(50) + r(0x400), y: 0x400 * r(50) + r(0x400), name: r(4) === 0 ? 'n' + i + '_' + k : ''
      };
      if (!(kind >= -6 && kind <= -2) && kind < 0) w.off = 1 + r(3);
      s.walkers.push(w);
    }
    if (mode === 'g') s.sel = 0x80 + s.g;
    else if (mode === 'w') {
      s.sel = (i * 13 + 5) % 64;
      const w = s.walkers[s.sel];
      w.flags |= 1;
      w.kind = [-6, -5, -4, -3, -2, -1, 0, 9][i % 8];
      if (i % 8 === 2 && (i & 8)) w.flags |= 8;
      if (!(w.kind >= -6 && w.kind <= -2) && w.kind < 0) w.off = 1;
    } else s.sel = [-1, 0x7f, 64, 100, 0x40000000, -0x80000000][i % 6];
    if (over && over.post) over.post(s);
    return s;
  }

  function fbApply(fx, s) {
    resetClog(fx.clog, fx.clogCap);
    fx.clog.add(CSEED).writeU32(s.seed);
    fx.seed.writeS32(s.slot);
    fx.seed.add(4).writeU32(0);
    wr32(0x004c2e10, s.sel);
    wr32(0x005685f4, s.rot);
    wr32(0x00834170, s.date);
    let fw = ptr(0x0059e7b8).readU32() & ~(4 | 0x20 | 0x80 | 0x200000);
    if (s.f4) fw |= 4;
    if (s.f20) fw |= 0x20;
    if (s.f80) fw |= 0x80;
    if (s.f200000) fw |= 0x200000;
    wr32(0x0059e7b8, fw);
    wr32(0x004c2844, s.zoom);
    wr32(0x005a59f8, s.gary);
    wr8(0x005795d9, s.g1b);
    wr32(0x00567afc, s.afc);
    wr32(0x004e9a94, s.cnt);
    wr32(0x004e9a9c, 0x4e9a9c00 + s.i);
    for (let k = 0; k < 0x80; k += 4) wr32(0x0051a068 + k, 0x2d2d2d2d);
    for (let k = 0; k < 0x10; k += 4) wr32(0x004e9a88 + k, 0x3f3f3f3f);
    // golfer records 0..15 and 0x99: position, direction byte +0x1a, type word +0xb6
    for (let g = 0; g < 16; g++) {
      const b = GOLF + 0x100 * g;
      wr32(b, 0x1000 * g + 0x123 + s.i);
      wr32(b + 4, 0x2000 + 0x800 * g - s.i);
      wr8(b + 0x1a, g === s.g ? s.gdir : g * 11);
      wr16(b + 0xb6, g === s.g ? s.t : (g % 8));
    }
    // type records 0..7: +0 flags (bit 7 = typeBit7Clear's answer 0), +2 animation byte (high nibble)
    for (let t = 0; t < 8; t++) {
      const b = TYPES + 0x230 * t;
      if (t === 0) { wr8(b, s.t0); wr8(b + 2, s.t0b); continue; }
      wr8(b, (t === s.t ? (s.bit7 ? 0x80 : 0) : ((t & 1) ? 0x80 : 0)) | (t * 3));
      wr8(b + 2, t === s.t ? s.nib : (t * 0x37));
    }
    if (s.t === 0) wr8(TYPES, (s.t0 & 0x7f) | (s.bit7 ? 0x80 : 0));
    for (let id = 0; id < 0x2c0; id++) wr32(FR + 4 * id, 1 + ((id * 13 + s.fseed) % 9));
    // walker records
    s.walkers.forEach(function (w, k) {
      const b = WALK + 0x4c * k;
      for (let q = 0; q < 0x4c; q += 4) wr32(b + q, 0);
      wr32(b, w.x); wr32(b + 4, w.y);
      wr32(b + 8, 0x08080000 + k); wr32(b + 0xc, 0x0c0c0000 + k);
      wr8(b + 0x12, w.flags); wr8(b + 0x13, w.kind); wr8(b + 0x15, w.off); wr8(b + 0x16, w.dir);
      wr16(b + 0x1c, w.anim); wr16(b + 0x1e, w.st);
      if (w.name) ptr(b + 0x2a).writeUtf8String(w.name);
    });
  }

  // noSeed (control key drawFlyingBall_golfer_noseed): typeBit7Clear without the seeding stub, so the slot read at
  // 0x00403965 holds whatever the stack held in each arm
  function fbFixture(list, negEdits, noSeed) {
    const fx = {};
    fx.clog = newClog(CLOG);
    fx.clogCap = CLOG;
    fx.seed = alloc(0x10);
    fbStands(fx, noSeed);    // freezes the first thread
    FB_SAVE.forEach(function (s) { saveRange(s[0], s[1]); });
    list.forEach(function (s, i) {
      Object.defineProperty(fx, 's' + i, { enumerable: true, configurable: true, get: function () {
        if (negEdits) negSel(negEdits[i]);
        fbApply(fx, s);
        return s.i;
      } });
    });
    return fx;
  }
  function fbList(mode, n) {
    const out = [];
    for (let i = 0; i < n; i++) out.push(fbScene(i, mode));
    return out;
  }
  F.c3fs_fb_golfer = function () { return fbFixture(fbList('g', 24)); };
  F.c3fs_fb_walker = function () { return fbFixture(fbList('w', 24)); };
  F.c3fs_fb_none = function () { return fbFixture(fbList('n', 12)); };
  F.c3fs_fb_golfer_noseed = function () { return fbFixture(fbList('g', 24), null, true); };
  // negctl keys: drawFlyingBall_neg<N> runs every scene of one list with edit N live (EDIT_KEYS of
  // log/c3/alt/c3fs_negctl_gen.py, copied in re/frida/registry.d/c3fs.py)
  const FB_LISTS = { fb_golfer: ['g', 24], fb_walker: ['w', 24], fb_none: ['n', 12] };
  const FB_NEG = { 1: 'fb_golfer', 2: 'fb_golfer', 3: 'fb_golfer', 4: 'fb_golfer', 5: 'fb_golfer', 6: 'fb_none',
                   7: 'fb_walker', 8: 'fb_none', 9: 'fb_walker', 10: 'fb_none', 11: 'fb_walker', 12: 'fb_none',
                   13: 'fb_walker', 14: 'fb_walker', 15: 'fb_walker', 16: 'fb_walker', 17: 'fb_walker',
                   18: 'fb_walker', 19: 'fb_walker', 20: 'fb_walker' };
  Object.keys(FB_NEG).forEach(function (e) {
    F['c3fs_fb_neg' + e] = function () {
      const l = FB_LISTS[FB_NEG[e]];
      const list = fbList(l[0], l[1]);
      return fbFixture(list, list.map(function () { return parseInt(e, 10); }));
    };
  });
  globalThis.__c3fsFbScene = fbScene;

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
