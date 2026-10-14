// Fixtures for batch c3fj (round 34, 2026-10-13): golf_clean.exe size-only rows over 2 KB.
// Reimplementations: shim/src/re/c3fj.cpp. Registry: re/frida/registry.d/c3fj.py.
// Purposes and per-jcc tables: log/c3/c3fj_purpose.md. Notes: log/c3/c3fj_notes.txt.
//
// Method (helpers copied from c3fc.js, which copied c3er.js / c3ea.js):
// - Every key freezes the game's first thread (re/frida/js/freeze.js) and saves every global it writes once, right
//   after the freeze; the flush restores them before the resume. negOn() arms the negative-control build's exported
//   C3fjNegCtl switch right after the freeze; word 2 of it selects which single wrong detail is live.
// - Callees that draw, queue sprites or project to the screen are stood in on the A/B thread only (calleeStand): each
//   call is counted, folded into a running FNV-1a hash and logged; the log ($clog) is a compared state region. A
//   stand-in's answer depends only on its arguments and on the scene words in the log (compared memory).
// - Pointer arguments into the caller's frame (the screen-position out slots) are masked in the log; the stand-in
//   writes through them, so a body that passes the wrong slot reads RED in the calls that follow.
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

  // ---- calleeStand: log word 0 = cursor, words 1..32 = call count of stub k, word 0x84 = FNV-1a over every call,
  // words 0x88 / 0x8c = scene words the stubs read (compared memory), entries {va, count, values...} from +0x90.
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x4000;
  const CHASH = 0x84, CSEED = 0x88, CANS = 0x8c, CHEAD = 0x90;
  function newClog(cap) { const l = alloc(cap || CLOG); l.writeU32(CHEAD); l.add(CHASH).writeU32(0x811c9dc5); return l; }
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
  // buildSceneSprites 0x00463180(kind, variant, x, y, x2, y2, obj)
  // ====================================================================================================================
  // Stood in (all record their arguments): setDrawTarget 0x004762d0, cellAt 0x0043d6f0 (answers a hash of its
  // arguments and the scene seed, so every queued cell is distinct), enqueueSprite 0x00462a30, addSprite 0x004628d0,
  // objectAt 0x0040df80 (answers the scene's record index, log word 0x8c), tileToScreen 0x0042f940 and worldToScreen
  // 0x0042fb90 (write the out slots from their arguments and the seed, or leave them, and worldToScreen answers
  // visible / not from the same), Surface_drawSprite 0x00474440, drawCenteredText2 0x00404bc0 (logs the text).
  // Real: rankValue 0x0040e5b0, boundIndex 0x00463170, clamp 0x00467130, foldRange 0x00467270, absDiff 0x004672b0,
  // __itoa 0x004ad425.
  const OBJ = 0x0058bcb8;
  const FR = 0x0053f3e8;                      // frame-count table, one dword per cell-table id
  const BSS_SAVE = [
    [0x00834170, 4], [0x005685f4, 4], [0x005685f0, 4], [0x005a34e0, 4], [0x005a8c50, 4], [0x004c2844, 4],
    [0x004c2840, 4], [0x0059e7b8, 4], [0x00838204, 4], [0x0051a068, 0x40], [0x00834164, 12],
    [OBJ, 0x100], [0x0053caf0, 0x1388], [0x00578804, 0x9c4], [0x005722e8, 0x9c4],
    [FR + 4 * 0x160, 0x400], [0x0081b490, 0x180]
  ];
  // kind-5 answer records 8..15: [type dword +8, value dword +0xc]; values cover rankValue's five buckets
  // (v / 200 == 0, 1..2, 3..5, 6..9, >= 10) and a negative value
  const ANS = [[0, 50], [0, 300], [0, 700], [0, 1500], [0, 2500], [0, -400], [1, 900], [5, 2100]];

  function bssStands(fx) {
    calleeStand(fx, 0, 0x004762d0, 5, 0x0101, { abi: 'thiscall' });
    calleeStand(fx, 1, 0x0043d6f0, 3, function (n, a) {
      let h = fnvWord(0x811c9dc5, fx.clog.add(CSEED).readU32());
      h = fnvWord(h, a[0]); h = fnvWord(h, a[1]); h = fnvWord(h, a[2]);
      return ((h & 0x0ffffffc) | 0x20000000) >>> 0;
    });
    calleeStand(fx, 2, 0x00462a30, 8, 0);
    calleeStand(fx, 3, 0x004628d0, 8, 0);
    calleeStand(fx, 4, 0x0040df80, 2, function () { return fx.clog.add(CANS).readU32(); });
    calleeStand(fx, 5, 0x0042f940, 4, function (n, a) {
      const seed = fx.clog.add(CSEED).readU32();
      const tx = a[0] | 0, ty = a[1] | 0;
      if (((tx + ty + seed) & 3) !== 0) {
        ptr(a[2]).writeS32(tx * 37 + (seed & 0xff));
        ptr(a[3]).writeS32(ty * 29 - ((seed >>> 8) & 0xff));
      }
      return 0x5151;
    }, { mask: [2, 3] });
    calleeStand(fx, 6, 0x0042fb90, 5, function (n, a) {
      const seed = fx.clog.add(CSEED).readU32();
      const wx = a[0] | 0, wy = a[1] | 0;
      const k = (Math.imul(wx >> 10, 7) + Math.imul(wy >> 10, 3) + (wx & 0x3ff) + Math.imul(wy & 0x3ff, 5) +
                 (seed & 0xffff)) >>> 0;
      if ((k & 7) !== 7) {
        ptr(a[2]).writeS32((wx >> 5) + (seed & 0xf));
        ptr(a[3]).writeS32((wy >> 5) - 3 - ((seed >>> 4) & 0xf));
      }
      if (seed & 0x80000000) return 1;
      return (k % 5) !== 0 ? 1 : 0;
    }, { mask: [2, 3] });
    calleeStand(fx, 7, 0x00474440, 7, 0, { abi: 'thiscall' });
    calleeStand(fx, 8, 0x00404bc0, 4, 0, { str: { 0: 4 } });
  }

  // One scene: every global the body or a stand-in reads.
  function scene(i, kind, over) {
    const s = {
      kind: kind, v: 0, course: 0, bit: 0, alt: 0,
      date: 1000 + 37 * i, anim: 11 + 3 * i, holes: 5, mode6: 2, zoom: [3, -2, 5, 0][i % 4], dx: 7 + (i % 5),
      flag: 0, seed: (Math.imul(0x9e3779b9, i + 1) & 0x7fffffff) >>> 0, ans: 8 + (i % 8), obj: i % 8,
      ox: 3 + (i % 7), oy: 4 + ((i * 3) % 6), rot: (i * 5) & 0xff, type: i % 6, tbyte: (i * 7) % 26,
      x: 100 + i, y: 200 + 2 * i, x2: 300 + i, y2: 400 - i, b490: (i % 3) !== 0 ? 1 : 0, b526: (i % 4) !== 1 ? 1 : 0,
      fseed: i, big14: false, tpat: i % 2
    };
    if (over) Object.keys(over).forEach(function (q) { s[q] = over[q]; });
    return s;
  }
  // the standard cross product: course 0..3 x variant x bit 0x4000 x alt (alt 1: holes 11, [0x005a8c50] 0, the
  // 0x20 flag, a negative date, and frame counts of 250 for ids 0x1dc..0x1df)
  function cross(kind, vs, base) {
    const out = [];
    for (let course = 0; course < 4; course++)
      for (const v of vs)
        for (let bit = 0; bit < 2; bit++)
          for (let alt = 0; alt < 2; alt++) {
            const i = base + out.length;
            const o = { v: v, course: course, bit: bit, alt: alt };
            if (alt) Object.assign(o, { holes: 11, mode6: 0, flag: 1, date: -(3000 + 37 * i), big14: true });
            out.push(scene(i, kind, o));
          }
    return out;
  }
  function scenesFor(key) {
    if (key === 'misc') {
      const out = [];
      [0, 1, 2, 16, 19, 100, -1, 0x7fffffff, -0x80000000].forEach(function (k) {
        [0, 1].forEach(function (bit) { out.push(scene(out.length, k, { bit: bit, date: bit ? -7777 : 4242 })); });
      });
      return out;
    }
    const kind = parseInt(key.slice(1), 10);
    if (kind === 8) return cross(8, [0, 1, 2], 0);
    if (kind === 10) {
      const out = [];
      [0, 1, 2, -3, -4, -5, -6].forEach(function (v) {
        [0, 1].forEach(function (course) {
          [0, 1].forEach(function (tp) {
            const i = out.length;
            const o = { v: v, course: course, tpat: tp, ox: 3 + (i % 5), oy: 2 + (i % 4) };
            if (i % 3 === 0) o.seed = (scene(i, 10).seed | 0x80000000) >>> 0;   // every cell visible
            out.push(scene(i, 10, o));
          });
        });
      });
      return out;
    }
    const out = cross(kind, [0, 1], 0);
    if (kind === 12 || kind === 13) {
      // course 1, variant 1: w = (date + 13 * oy + 7 * ox) % (10 n) (idiv 0x004651d0 / 0x00463d77), folded to 5n - w - 1
      // when w >= 4n, then clamped to [0, n - 1]. These eight scenes pick the date so that w lands on each region:
      // 0, 2n, 4n - 1 (no fold), 4n, 4n + n / 2, 5n - 1 (the fold inside the clamp), 5n, 10n - 1 (folded below 0).
      // n is the frame count of id 0x1d3 (kind 12, 0x0053fb34) or 0x1eb (kind 13, 0x0053fb94), as bssApply writes it.
      const id = kind === 12 ? 0x1d3 : 0x1eb;
      [0, 2, 3.9, 4, 4.5, 4.9, 5, 9.9].forEach(function (t) {
        const i = out.length;
        const s = scene(i, kind, { v: 1, course: 1, bit: 0, alt: 0 });
        const n = 1 + ((id * 13 + s.fseed) % 9);
        const target = t === 3.9 ? 4 * n - 1 : t === 4.9 ? 5 * n - 1 : t === 9.9 ? 10 * n - 1 : Math.floor(t * n);
        const base = 13 * s.oy + 7 * s.ox;
        s.date = target - base + 10 * n * 100;
        out.push(s);
      });
    }
    if (kind === 6) {
      // [0x005a8c50] = -2 (no ring: 2 * -2 + 3 <= 0) and 1 (five ring sprites), course 1 and 3
      [-2, 1, 3, -1].forEach(function (m, j) { out.push(scene(out.length, 6, { mode6: m, course: j % 2 ? 3 : 1 })); });
    }
    return out;
  }

  function bssApply(fx, s) {
    resetClog(fx.clog, fx.clogCap);
    fx.clog.add(CSEED).writeU32(s.seed);
    fx.clog.add(CANS).writeU32(s.ans);
    wr32(0x00834170, s.date);
    wr32(0x005685f4, s.anim);
    wr32(0x005685f0, s.holes);
    wr8(0x005a34e0, s.course);
    wr32(0x005a8c50, s.mode6);
    wr32(0x004c2844, s.zoom);
    wr32(0x004c2840, s.dx);
    const fw = ptr(0x0059e7b8).readU32();
    wr32(0x0059e7b8, s.flag ? (fw | 0x20) : (fw & ~0x20));
    wr32(0x00838204, 0x13572468);
    for (let k = 0; k < 0x40; k += 4) wr32(0x0051a068 + k, 0x2d2d2d2d);
    for (let k = 0; k < 12; k += 4) wr32(0x00834164 + k, 0x3f3f3f3f);
    // records 0..7: the subject (index s.obj) and seven neighbours; 8..15: the kind-5 answers
    for (let r = 0; r < 16; r++) {
      const b = OBJ + 16 * r;
      wr16(b, r);
      if (r < 8) {
        const me = r === s.obj;
        wr16(b + 2, me ? s.ox : 20 + r);
        wr16(b + 4, me ? s.oy : 30 + r);
        wr8(b + 6, me ? s.rot : r * 9);
        wr8(b + 7, 0x5a);
        wr32(b + 8, me ? s.type : r);
        wr32(b + 12, me ? 1234 + s.fseed : r * 100);
      } else {
        wr16(b + 2, 40 + r); wr16(b + 4, 40 + r); wr8(b + 6, r); wr8(b + 7, 0xa5);
        wr32(b + 8, ANS[r - 8][0]);
        wr32(b + 12, ANS[r - 8][1]);
      }
    }
    // the subject tile's word (bit 0x4000 per scene; other bits a fixed pattern) and byte
    const tile = s.oy + 50 * s.ox;
    wr16(0x0053caf0 + 2 * tile, 0x1235 | (s.bit ? 0x4000 : 0));
    wr8(0x00578804 + tile, s.tbyte);
    // the frame counts the divisors read (ids 0x160..0x25f)
    for (let id = 0x160; id < 0x260; id++) wr32(FR + 4 * id, 1 + ((id * 13 + s.fseed) % 9));
    if (s.big14) for (let id = 0x1dc; id < 0x1e0; id++) wr32(FR + 4 * id, 250);
    // the variant byte tables kind 8 reads (0x0081b490 / 0x0081b526, 50 bytes per variant)
    for (let v = 0; v < 3; v++) { wr8(0x0081b490 + 50 * v, s.b490); wr8(0x0081b526 + 50 * v, s.b526); }
    // the tile-type map kind 10 reads at (ox + i) * 50 + oy + j: type 1 by pattern, else 2
    for (let i = 0; i < 8; i++)
      for (let j = 0; j < 8; j++) {
        const one = s.tpat ? ((i * j) % 5 === 3) : ((i + j) % 3 === 0);
        wr8(0x005722e8 + 50 * (s.ox + i) + s.oy + j, one ? 1 : 2);
      }
  }

  function bssFixture(list, negEdits) {
    const fx = {};
    fx.clog = newClog(CLOG);
    fx.clogCap = CLOG;
    bssStands(fx);            // freezes the first thread
    BSS_SAVE.forEach(function (s) { saveRange(s[0], s[1]); });
    list.forEach(function (s, i) {
      Object.defineProperty(fx, 'k' + i, { enumerable: true, configurable: true, get: function () {
        if (negEdits) negSel(negEdits[i]);
        bssApply(fx, s);
        return s.kind;
      } });
      Object.defineProperty(fx, 'v' + i, { enumerable: true, configurable: true, get: function () { return s.v; } });
      Object.defineProperty(fx, 'x' + i, { enumerable: true, configurable: true, get: function () { return s.x; } });
      Object.defineProperty(fx, 'y' + i, { enumerable: true, configurable: true, get: function () { return s.y; } });
      Object.defineProperty(fx, 'p' + i, { enumerable: true, configurable: true, get: function () { return s.x2; } });
      Object.defineProperty(fx, 'q' + i, { enumerable: true, configurable: true, get: function () { return s.y2; } });
      Object.defineProperty(fx, 'o' + i, { enumerable: true, configurable: true, get: function () { return s.obj; } });
    });
    return fx;
  }
  ['misc', 'k3', 'k4', 'k5', 'k6', 'k7', 'k8', 'k9', 'k10', 'k11', 'k12', 'k13', 'k14', 'k15', 'k17', 'k18']
    .forEach(function (key) {
      F['c3fj_bss_' + key] = function () { return bssFixture(scenesFor(key)); };
    });
  globalThis.__c3fjScenesFor = scenesFor;

  // negctl keys: buildSceneSprites_neg<N> runs every scene of one list with edit N live (EDIT_KEYS of
  // log/c3/alt/c3fj_negctl_gen.py, copied in re/frida/registry.d/c3fj.py)
  const BSS_NEG_KEYS = { 1: 'k14', 2: 'k15', 3: 'k7', 4: 'k3', 5: 'k4', 6: 'k6', 7: 'k8', 8: 'k10', 9: 'k17', 10: 'k5',
                         11: 'k13', 12: 'k14', 13: 'k9', 14: 'k11', 15: 'k18', 16: 'k7', 17: 'k12', 18: 'misc', 19: 'k6',
                         20: 'k15', 21: 'k7', 22: 'k5' };
  Object.keys(BSS_NEG_KEYS).forEach(function (e) {
    F['c3fj_bss_neg' + e] = function () {
      const list = scenesFor(BSS_NEG_KEYS[e]);
      return bssFixture(list, list.map(function () { return parseInt(e, 10); }));
    };
  });

  // ====================================================================================================================
  // netHandleControl 0x0049acf0(this, id, flag)
  // ====================================================================================================================
  // The session object is ours (0x800 bytes, rewritten per vector): vtable +0 (two recorder slots +0xc / +0x18), the
  // timeout +0xf0, the list +0xc0, peer slots +0x170 (16 x 0x58: id +0, name +0x15), +0x6f8 count, +0x77c, +0x780.
  // The dialog the body builds in its frame is stood in through framemask.js: TextView::ctor declares the 0x211c-byte
  // frame object (0x2114 bytes) and fills it with known words, so the vtable words the body writes back before each destructor show in
  // the destructors' derefs; TextView::build answers from the compared block $nhans (word n for its n-th call),
  // ListModel::destroyNodes writes $nhans word 2 into its object's word +4. netListRemove, netSendMessages and the two
  // vtable slots record. Real: netNameOf 0x0049aa30 and setHudTextSlot 0x00494cb0 (the HUD slot globals are state).
  const NH_SAVE = [[0x008400ac, 4], [0x00839650, 4], [0x0083afcc, 4], [0x0083e8b8, 0x28], [0x0083d3c8, 0x28],
                   [0x0083e8e0, 0x200]];
  function nhStubs() {
    const page = alloc(Process.pageSize);
    Memory.protect(page, Process.pageSize, 'rwx');
    const nops = Array(16).fill(0x90);
    page.writeByteArray(nops.concat([0xc3]));                         // slot +0x18: thiscall, no stack arguments
    page.add(0x40).writeByteArray(nops.concat([0xc2, 0x08, 0x00]));   // slot +0xc: thiscall, two stack arguments
    return page;
  }
  // scene: n peers (ids 0x1000 + 0x111 * k + i, names `p<k>s<i>`), the argument peer index pk (-1 = an id no slot
  // holds), flag, mode ('diff': +0x77c differs from +0x780 and from peer 1; 'eq': +0x77c == +0x780; 'p1': +0x77c ==
  // peer 1's id), the answers of the two TextView::build calls, whether id == +0x780
  function nhScene(i, o) {
    const s = { i: i, n: 6, pk: 2, flag: 0, mode: 'diff', ans: [0, 0], cur: false, f0: 0x77 + i, cnt: 6 + i };
    Object.keys(o).forEach(function (q) { s[q] = o[q]; });
    return s;
  }
  const NH = [
    nhScene(0, { pk: -1 }), nhScene(1, { pk: -1, flag: 1 }),
    nhScene(2, { ans: [0, 0] }), nhScene(3, { ans: [1, 0] }),
    nhScene(4, { ans: [2, 0] }), nhScene(5, { ans: [2, 1] }), nhScene(6, { ans: [2, 2] }), nhScene(7, { ans: [2, 3] }),
    nhScene(8, { ans: [2, 4] }), nhScene(9, { ans: [2, -1] }),
    nhScene(10, { ans: [3, 0] }), nhScene(11, { ans: [-1, 0] }), nhScene(12, { ans: [7, 0] }),
    nhScene(13, { mode: 'eq', ans: [1, 0] }),
    nhScene(14, { mode: 'eq', ans: [2, 0] }), nhScene(15, { mode: 'eq', ans: [2, 1] }),
    nhScene(16, { mode: 'eq', ans: [2, 2] }), nhScene(17, { mode: 'eq', ans: [2, 3] }),
    nhScene(18, { mode: 'eq', ans: [2, 9] }),
    nhScene(19, { mode: 'p1', ans: [2, 1] }), nhScene(20, { mode: 'p1', ans: [1, 0] }),
    nhScene(21, { mode: 'eq', ans: [0, 0], cur: true }), nhScene(22, { mode: 'eq', ans: [3, 0] }),
    nhScene(23, { mode: 'p1', ans: [-2, 0], cur: true, pk: 0 }),
    nhScene(24, { flag: 1, pk: 0, cur: true }), nhScene(25, { flag: 1, pk: 5, n: 9 }),
    nhScene(26, { flag: 1, pk: 14, n: 16, cur: true }), nhScene(27, { flag: 1, pk: 15, n: 16 }),
    nhScene(28, { flag: 1, pk: 15, n: 16, cur: true }), nhScene(29, { flag: 1, pk: 3, n: 3 }),
    nhScene(30, { flag: -1, pk: 1, n: 4, cur: true }), nhScene(31, { flag: 0x100, pk: 0, n: 1 }),
  ];
  function nhId(s, k) { return k < s.n ? (0x1000 + 0x111 * k + s.i) >>> 0 : 0; }
  function nhApply(fx, s) {
    const o = fx.no;
    for (let q = 0; q < 0x800; q += 4) o.add(q).writeU32(0);
    o.writePointer(fx.vt);
    o.add(0xf0).writeU32(s.f0);
    o.add(0xc0).writeU32(0x0c0c0000 + s.i);
    for (let k = 0; k < 16; k++) {
      const slot = o.add(0x170 + 0x58 * k);
      slot.writeU32(nhId(s, k));
      slot.add(4).writeU32(0x5a5a0000 + 0x100 * k + s.i);
      slot.add(0x15).writeUtf8String(k < s.n ? 'p' + k + 's' + s.i : '');
      slot.add(0x40).writeU32(0x40400000 + k);
    }
    const id = s.pk < 0 ? 0x7777 + s.i : nhId(s, s.pk);
    const p1 = nhId(s, 1);
    let a77c, a780;
    if (s.mode === 'eq') { a77c = s.cur ? id : 0x9100 + s.i; a780 = a77c; }
    else if (s.mode === 'p1') { a77c = p1; a780 = s.cur ? id : 0x9200 + s.i; }
    else { a77c = 0x9001 + s.i; a780 = s.cur ? id : 0x9002 + 2 * s.i; }
    o.add(0x77c).writeU32(a77c);
    o.add(0x780).writeU32(a780);
    o.add(0x6f8).writeU32(s.cnt);
    fx.ans.writeS32(s.ans[0]);
    fx.ans.add(4).writeS32(s.ans[1]);
    fx.ans.add(8).writeU32((0xd0d00000 + s.i) >>> 0);
    // the frame-mask block: every arm of this vector starts with an empty object table (epoch 0)
    const blk = fx.fmlog;
    blk.add(4).writeByteArray(new Uint8Array(0x8000 - 4).buffer);
    blk.add(0xc).writeU32(0x811c9dc5);
    wr32(0x008400ac, 0x8400ac00 + s.i);
    wr32(0x00839650, 0x83965000 + s.i);
    wr32(0x0083afcc, 0);
    s.argId = id;
  }
  // neg: the negctl edit every vector runs with (0 = none); raw: framemask's mapping off (control)
  function nhFixture(neg, raw) {
    const fx = { no: alloc(0x800), ans: alloc(0x10) };
    fx.vt = alloc(0x40);
    const code = nhStubs();
    fx.vt.add(0x18).writePointer(code);
    fx.vt.add(0xc).writePointer(code.add(0x40));
    const FM = __frameMask.begin({ size: 0x8000, raw: !!raw });
    const ans = fx.ans;
    const one = { this: 1 };
    // the object runs from [esp+0x114] to the SEH record at [esp+0x2228] (0x2114 bytes); filling past it overwrote the
    // saved fs:[0] in the original arm (first run: the epilogue at 0x0049b65c restored a fill word into fs:[0])
    FM.stand({ addr: 0x0048ce00, abi: 'thiscall', nargs: 0, region: { arg: 'this', size: 0x2114 },
               fill: { arg: 'this', words: 0x2114 / 4 }, ret: 'this' });                      // TextView::ctor
    FM.stand({ addr: 0x0048e1c0, abi: 'thiscall', nargs: 6, ret: 0 });                          // TextView::loadScript
    FM.stand({ addr: 0x00493670, abi: 'thiscall', nargs: 5, ret: 0 });                          // TextView_layoutRun
    FM.stand({ addr: 0x0048e900, abi: 'thiscall', nargs: 2,
               ret: function (self, args, n) { return ans.add(4 * Math.min(n, 1)).readU32(); } });   // TextView::build
    FM.stand({ addr: 0x0048d480, abi: 'thiscall', nargs: 0, deref: { this: 0x9e } });        // TextView::reset
    FM.stand({ addr: 0x004928d0, abi: 'thiscall', nargs: 0, deref: one });                    // HotList::dtor
    FM.stand({ addr: 0x00491500, abi: 'thiscall', nargs: 0, deref: one });                    // Dialog::viewDtor
    FM.stand({ addr: 0x00489370, abi: 'thiscall', nargs: 0, deref: one });                    // ListModel::dtor
    FM.stand({ addr: 0x004805a0, abi: 'thiscall', nargs: 0, deref: one });                    // Base4805a0::dtor
    FM.stand({ addr: 0x00489c90, abi: 'thiscall', nargs: 0, deref: { this: 0x17 } });        // modelListDtor489c90
    FM.stand({ addr: 0x00473ae0, abi: 'thiscall', nargs: 0, deref: one });                    // Surface_free
    FM.stand({ addr: 0x00491410, abi: 'thiscall', nargs: 0, deref: one });                    // LabelButton::dtor
    FM.stand({ addr: 0x00474810, abi: 'thiscall', nargs: 0, deref: one });                    // Buf_dtor
    FM.stand({ addr: 0x004914d0, abi: 'thiscall', nargs: 0, deref: one });                    // Dialog::subDtor
    FM.stand({ addr: 0x00489b30, abi: 'thiscall', nargs: 0, deref: { this: 2 },               // ListModel::destroyNodes
               write: function (self) { ptr(self).add(4).writeU32(ans.add(8).readU32()); } });
    FM.stand({ addr: 0x00491710, abi: 'thiscall', nargs: 0, deref: one });                    // Dialog::dtor
    FM.stand({ addr: 0x0049bec0, abi: 'thiscall', nargs: 1, ret: 0 });                         // netListRemove
    FM.stand({ addr: 0x00499140, abi: 'thiscall', nargs: 5, deref: { 0: 1 }, ret: 0 });        // netSendMessages
    FM.stand({ addr: code, abi: 'thiscall', nargs: 0, ret: 0 });                               // vtable +0x18
    FM.stand({ addr: code.add(0x40), abi: 'thiscall', nargs: 2, str: { 0: 16 }, ret: 0 });     // vtable +0xc
    Object.assign(fx, FM.fx());
    freezeNow();
    NH_SAVE.forEach(function (s) { saveRange(s[0], s[1]); });
    NH.forEach(function (s, i) {
      Object.defineProperty(fx, 'o' + i, { enumerable: true, configurable: true, get: function () {
        if (neg) negSel(neg);
        nhApply(fx, s);
        return fx.no;
      } });
      Object.defineProperty(fx, 'i' + i, { enumerable: true, configurable: true, get: function () { return s.argId; } });
      Object.defineProperty(fx, 'f' + i, { enumerable: true, configurable: true, get: function () { return s.flag; } });
    });
    return fx;
  }
  F.c3fj_nethandle = function () { return nhFixture(0, false); };
  F.c3fj_nethandle_raw = function () { return nhFixture(0, true); };
  for (let e = 23; e <= 34; e++) F['c3fj_nethandle_neg' + e] = (function (k) { return function () { return nhFixture(k, false); }; })(e);
  globalThis.__c3fjNh = NH;

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
