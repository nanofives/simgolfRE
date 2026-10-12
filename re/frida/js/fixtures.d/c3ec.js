// Fixtures for batch c3ec (simulation plan phase 4, 2026-10-11): updateGolfers 0x004289e0. Reimplementation: shim/src/re/c3ec.cpp.
// Registry: re/frida/registry.d/c3ec.py. Purpose and per-jcc table: log/c3/c3ec_purpose.md.
//
// Method (helpers copied from c3ea.js / c3eb.js):
// - Every key freezes the game's first thread (re/frida/js/freeze.js) and saves every table a row writes; the flush restores
//   them newest first before the resume.
// - One vector is one whole call: updateGolfers walks all 152 golfer records, so a row is a scene. Each row builds a synthetic
//   50 x 50 course from a seed (tile types 0x005722e8, tile flag words 0x0053caf0, wall bits 0x005619a0, hole bytes 0x0056988c,
//   the corner-height cache 0x0051b770, the height table 0x005a4998, the type table 0x00578370, 20 hole records 0x00575ab0,
//   the placed objects 0x0058bcb8, the story pairs 0x0059fc60, golfer types 0x005849e0 / 0x004d60a9), writes the 152 golfer
//   records (all dead except the scene's golfers) and the scalar globals the function reads.
// - Callees that draw, play sound, pop text up, open a dialog, start a shot or run another golfer's state are stood in on the
//   A/B thread only (calleeStand): every call is counted, folded into a running FNV-1a hash and logged while there is room;
//   the log ($clog) is compared state. Random::range is a scripted stand-in (values by argument, see rngStand). The pure
//   leaves (distance, tileDistance, angleFixed, direction8, typeAtPos, tileBlocked, slopeMix, heightAt42fa30, objectAt,
//   nearestPlaced, findNearestTargetTile, canStep, inRange, foldRange, absDiff, clamp, typeBit7Clear, holeQuadrants, rateTile,
//   buildGolferName, __itoa, thoughtFlag) run for real: their C3 hooks, or the originals under SIMGOLF_HOOKS_OFF.
// - skillPointDialog 0x0045f0f0 (C2, a modal dialog) is always a recording stub: its five arguments and the text it is given
//   are logged.
// - The key c3ec_real also runs pathStep, updatePairSnapshot, addGolferPair and resetGolfer for real.
// Rows: diff_hook resolves "$rN" before the state snapshot, so the getter of fx.rN applies row N.
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
    // c3ec's own negative-control build exports its switch instead (log/c3/alt/c3ec_negctl_gen.py)
    try {
      const e = Process.getModuleByName('winmm.dll').findExportByName('C3ecNegCtl');
      if (e && e.add(4).readU32() === 0x5147454e) __freeze.setU32(e, 1);
    } catch (e2) { /* not that build */ }
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
  function saveRange(a, n) { __freeze.save(ptr(a), n); }

  // the per-row coverage build (log/c3/c3ec_cov_instrument.py) maps a page at 0x7A1F0000 with a magic in word 1: the row index
  // goes to word 0 so the hits of each row are kept apart (only that build has the page)
  function rowMark(i) {
    try { if (ptr('0x7A1F0000').add(4).readU32() === 0x574f5243) ptr('0x7A1F0000').writeU32(i); } catch (e) { /* no page */ }
  }
  // ---- rows: fx.r<i> applies row i (row.on) and returns 0
  let cur = {};
  function rowsOn(fx, rows) {
    rows.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () {
        cur = row;
        if (row.on) row.on(fx, row);
        rowMark(i);
        return 0;
      } });
    });
  }

  // ---- calleeStand (c3ea's): log word 0 = cursor, words 1..16 = call count of stub k, word 17 = FNV-1a hash over every call
  // (va and logged values), entries {va, count, values...} from +0x50 while they fit.
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x4000;
  function newClog(cap) { const l = alloc(cap || CLOG); l.writeU32(0x50); l.add(0x44).writeU32(0x811c9dc5); return l; }
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
    const orig = new NativeFunction(at, 'uint32', argt, opts.abi || 'default');
    const log = fx.clog;
    const cap = fx.clogCap || CLOG;
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
      if (opts.extra) opts.extra(a).forEach(function (x) { vals.push(x >>> 0); });
      let h = log.add(0x44).readU32();
      h = fnvWord(h, va);
      vals.forEach(function (v) { h = fnvWord(h, v >>> 0); });
      log.add(0x44).writeU32(h);
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
  // The synthetic course (c3ea.js course(), extended: full hole records, type table bytes +5 / +7)
  // ====================================================================================================================
  function rng(seed) {
    let s = (seed >>> 0) || 1;
    return function () { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; return s; };
  }
  const GOLF = 0x005794b8, GOLF_N = 152 * 0x100;
  const T_SAVE = [
    [0x005722e8, 2500], [0x0053caf0, 5000], [0x005619a0, 2500], [0x0056988c, 2500], [0x0051b770, 20000],
    [0x005a4998, 0xa00], [0x00578370, 0x6f0], [0x00575ab0, 0x208 * 20], [0x0058bcb8, 0x1000], [0x0059fc60, 0x3880],
    [GOLF, GOLF_N], [0x005849e0, 0x2c * 8], [0x004d60a9, 0x230 * 8], [0x0056ae70, 0x1a00], [0x00584214, 0x7e0],
    [0x0056962c, 0x260], [0x0053ea24, 2500], [0x00578804, 2500], [0x00585850, 0x18], [0x0051a068, 0x200], [0x0058a528, 0x20],
    [0x0059b73c, 4], [0x004c2e0c, 4], [0x00569498, 4], [0x005a59f8, 4], [0x00571fd4, 4], [0x004c2ba0, 8], [0x0058f330, 4],
    [0x005a34e4, 4], [0x00567afc, 4], [0x00561254, 4], [0x005787fc, 8], [0x004c2e28, 4], [0x0059e7b8, 4], [0x00822d9c, 4],
    [0x00568d0c, 4], [0x0056a91c, 8], [0x00541318, 4], [0x00834170, 4], [0x00822c88, 4], [0x005a34e0, 4], [0x0059b048, 4],
    [0x0059b738, 4], [0x0059b04c, 4], [0x00543cb8, 4], [0x00543cc0, 4], [0x00543cc8, 4], [0x00543ccc, 4], [0x004c2844, 4],
    [0x005685f0, 4], [0x0056a51c, 4], [0x005a5a24, 4], [0x005a6d3c, 4], [0x00578150, 8], [0x005a8c50, 4], [0x005a8c58, 4],
    [0x005a8c60, 4], [0x0058bcb8, 0x10], [0x005689e8, 0x320], [0x0058d370, 2500], [0x005a7404, 0x800], [0x005a8028, 0x800],
    [0x004c2ba0, 8], [0x005756a0, 0x20]
  ];
  function saveAll() { T_SAVE.forEach(function (r) { saveRange(r[0], r[1]); }); }

  // type records (0x30 bytes from 0x00578370): +0 bounce, +1 friction, +2 attr (signed; > 0 rough-like, <= 0 fairway-like),
  // +5 fatigue (signed), +6 path class, +7 class (7 / 8 / 0xe: water-side tiles), +0xc height flags
  function typeTable(variant) {
    const buf = new Uint8Array(0x6f0);
    const put = function (t, o) {
      const b = t * 0x30;
      buf[b] = o.bo & 0xff; buf[b + 1] = o.fr & 0xff; buf[b + 2] = o.at & 0xff; buf[b + 5] = o.fa & 0xff;
      buf[b + 6] = (o.pc || 0) & 0xff; buf[b + 7] = (o.cl || 0) & 0xff; buf[b + 0xc] = (o.hf || 0) & 0xff;
    };
    for (let t = 0; t < 0x25; t++) {
      const k = (t * 7 + variant * 3) % 11;
      put(t, { bo: k % 5, fr: 1 + (k % 4), at: (k % 4) - 1, fa: (k % 5) - 1, pc: k % 3, cl: [0, 7, 0, 8, 0, 0xe, 0, 0, 3, 0, 0][k], hf: [0, 0, 2, 4, 0, 8][k % 6] });
    }
    put(0, { bo: 2, fr: 3, at: 0, fa: 1 });
    put(1, { bo: variant === 3 ? 6 : 1, fr: 2, at: -1, fa: 0 });            // green
    put(2, { bo: 2, fr: 3, at: 0, fa: 1 });                                // fairway
    put(3, { bo: 3, fr: 4, at: 1, fa: 2, cl: variant === 1 ? 7 : 0 });
    put(4, { bo: 1, fr: 5 + (variant & 1), at: 1, fa: 2 });                // rough
    put(5, { bo: 1, fr: 7, at: 2, fa: 3, cl: 8 });
    put(6, { bo: 0, fr: 9, at: 2, fa: 4 });                                // sand
    put(7, { bo: 2, fr: 3, at: variant === 2 ? 2 : 0, fa: 1, cl: 0xe });   // variant 2: attr 2 (the type-7 walk test 0x0042b4fb)
    put(10, { bo: 2, fr: 3, at: 1, fa: 1 });                               // the 0xa type with the green-side halving
    put(0xc, { bo: 3, fr: 3, at: 2, fa: 2 });                              // the 0xc type with the kick
    put(0x11, { bo: 0, fr: variant === 2 ? -2 : 9, at: 4, fa: -1, hf: 8 });   // water
    put(0x14, { bo: 0, fr: 9, at: variant === 3 ? -1 : 3, fa: 5 });       // blocked; variant 3: attr -1 (0x0042d5c5 / 0x0042d5e1)
    put(0x16, { bo: 2, fr: 2, at: 0, fa: 0 });                             // object tile (objectAt)
    if (variant === 4) put(0x12, { bo: 1, fr: 3, at: 30, fa: 1 });        // a heavy type for the water-drop score (0x0042d976)
    if (variant === 4) put(0x13, { bo: 1, fr: 3, at: 127, fa: 1 });       // the heaviest attr: a later drop sample that scores no better
    if (variant === 4) put(0xe, { bo: 1, fr: 3, at: -128, fa: 1 });       // the lightest attr: a first drop sample a second one only ties under x4
    return buf;
  }

  // s = {seed, water, rough, sand, block, misc, flags, walls, hills, flat, tv, holes:[{x,y,...}], objs, pairs, patch, tf}
  function course(s) {
    const R = rng(s.seed * 2654435761);
    const fr = function () { return R() / 4294967296; };
    const tiles = new Uint8Array(2500), flags = new Uint16Array(2500), walls = new Uint8Array(2500), hb = new Uint8Array(2500);
    const misc = [0, 3, 7, 8, 9, 10, 0xc, 0xd, 0xe, 0x12, 0x13, 0x15, 0x16];
    for (let i = 0; i < 2500; i++) {
      const r = fr();
      let t = 2;
      if (r < s.water) t = 0x11;
      else if (r < s.water + s.rough) t = (R() & 1) ? 4 : 5;
      else if (r < s.water + s.rough + s.sand) t = 6;
      else if (r < s.water + s.rough + s.sand + s.block) t = 0x14;
      else if (r < s.water + s.rough + s.sand + s.block + s.misc) t = misc[R() % misc.length];
      tiles[i] = t;
      let f = 0;
      const q = R();
      if ((q & 0xff) < s.flags) f |= [0x20, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x20 | 0x100, 0x1800, 0x4000][((q >>> 8) & 0xff) % 10];
      if (f & 0x80) f |= (q >>> 12) & 0x1f;
      flags[i] = f;
      if ((R() & 0xff) < s.walls) walls[i] = [1, 4, 0x10, 0x40, 5, 0x50, 0x11, 0x44][(R() >>> 5) & 7];
      if ((R() & 0xff) < 16) hb[i] = 0x80;
    }
    const holes = s.holes;
    holes.forEach(function (h, k) {
      if (!h.green) return;
      for (let dx = -h.green; dx <= h.green; dx++)
        for (let dy = -h.green; dy <= h.green; dy++) {
          const x = h.x + dx, y = h.y + dy;
          if (x >= 0 && x < 50 && y >= 0 && y < 50) tiles[x * 50 + y] = 1;
        }
      const hc = h.x * 50 + h.y;
      flags[hc] = (flags[hc] & ~0x1f) | 0x80 | (k & 0x1f);
    });
    if (s.patch) s.patch.forEach(function (p) {
      const x0 = p[0], y0 = p[1], x1 = p[4] === undefined ? p[0] : p[4], y1 = p[5] === undefined ? p[1] : p[5];
      for (let x = x0; x <= x1; x++) for (let y = y0; y <= y1; y++) {
        if (x < 0 || x >= 50 || y < 0 || y >= 50) continue;
        if (p[2] !== null && p[2] !== undefined) tiles[x * 50 + y] = p[2];
        if (p[3] !== null && p[3] !== undefined) flags[x * 50 + y] = p[3];
      }
    });
    if (s.wpatch) s.wpatch.forEach(function (p) { walls[p[0] * 50 + p[1]] = p[2]; });
    if (s.hbpatch) s.hbpatch.forEach(function (p) { hb[p[0] * 50 + p[1]] = p[2]; });
    ptr(0x005722e8).writeByteArray(tiles.buffer);
    ptr(0x0053caf0).writeByteArray(flags.buffer);
    ptr(0x005619a0).writeByteArray(walls.buffer);
    ptr(0x0056988c).writeByteArray(hb.buffer);
    const ch = new Uint8Array(20000), bl = new Uint8Array(0xa00);
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++) {
        const base = s.steep ? (((x + y) & 1) ? 1 : 12) : (s.flat ? 4 : 4 + Math.round(s.hills * Math.sin((x + s.seed) * 0.37) * Math.cos((y - s.seed) * 0.29)));
        for (let k = 0; k < 8; k++) ch[(x * 50 + y) * 8 + k] = s.steep ? ((k === 3 || k === 5) ? 12 : 1) : Math.max(1, Math.min(12, base + (s.flat ? 0 : ((x * 3 + y * 5 + k * 3 + s.seed) % 3) - 1)));
        bl[x * 51 + y] = Math.max(3, Math.min(15, 6 + (s.flat ? 0 : Math.round(s.hills * Math.sin(x * 0.31 + y * 0.17 + s.seed)))));
      }
    ptr(0x0051b770).writeByteArray(ch.buffer);
    ptr(0x005a4998).writeByteArray(bl.buffer);
    ptr(0x00578370).writeByteArray(typeTable(s.tv || 0).buffer);
    // hole records 0..19 (0x208 bytes from 0x00575ab0): +0 par byte, +1 direction byte, +8/+0xc alt tee, +0x10/+0x14 tee,
    // +0x18/+0x1c cup tile, +0x20 count, +0x15a.. +0x166 statistics words
    const hr = new Uint8Array(0x208 * 20);
    const hv = new DataView(hr.buffer);
    for (let k = 0; k < 20; k++) {
      const h = holes[k] || {};
      const o = 0x208 * k;
      for (let q = 0; q < 0x208; q += 4) hv.setUint32(o + q, (0x01010101 * ((k * 13 + q) & 0x3f)) >>> 0, true);
      hv.setUint8(o, h.par === undefined ? (k === 0 ? 0 : 3 + (k % 3)) : h.par);
      hv.setUint8(o + 1, h.dir === undefined ? (k * 3) & 0xff : h.dir);
      hv.setInt32(o + 8, h.ax === undefined ? (k * 7 + 9) % 50 : h.ax, true);
      hv.setInt32(o + 0xc, h.ay === undefined ? (k * 3 + 4) % 50 : h.ay, true);
      hv.setInt32(o + 0x10, h.tx === undefined ? (k * 5 + 6) % 50 : h.tx, true);
      hv.setInt32(o + 0x14, h.ty === undefined ? (k * 9 + 2) % 50 : h.ty, true);
      hv.setInt32(o + 0x18, h.x === undefined ? (k * 11 + 3) % 50 : h.x, true);
      hv.setInt32(o + 0x1c, h.y === undefined ? (k * 13 + 7) % 50 : h.y, true);
      hv.setInt32(o + 0x20, k * 3, true);
      for (let q = 0x15a; q <= 0x166; q += 2) hv.setInt16(o + q, (k * 5 + q) & 0x3f, true);
    }
    ptr(0x00575ab0).writeByteArray(hr.buffer);
    // placed objects: all free (type -1) except the scene's list {x, y, type, val, f7}
    const ob = new Uint8Array(0x1000);
    for (let i = 0; i < 256; i++) { ob[i * 16] = 0xff; ob[i * 16 + 1] = 0xff; }
    ptr(0x0058bcb8).writeByteArray(ob.buffer);
    (s.objs || []).forEach(function (o, i) {
      const b = 0x0058bcb8 + 16 * (o.i === undefined ? i : o.i);
      wr16(b, o.type); wr16(b + 2, o.x); wr16(b + 4, o.y); wr8(b + 7, o.f7 === undefined ? 0x40 : o.f7); wr32(b + 8, o.val || 0);
    });
    // story pairs: all free (id -1) except {slot, id, partner}
    const pb = new Uint8Array(0x3880);
    for (let i = 0; i < 16; i++) { pb[i * 0x388] = 0xff; pb[i * 0x388 + 1] = 0xff; }
    ptr(0x0059fc60).writeByteArray(pb.buffer);
    (s.pairs || []).forEach(function (p) {
      const b = 0x0059fc60 + 0x388 * p.slot;
      wr16(b, p.id); wr16(b + 2, p.partner === undefined ? (p.id ^ 1) : p.partner); wr16(b + 4, p.count || 0);
    });
    // golfer types: bit 7 of 0x004d60a9 + 0x230 * type (typeBit7Clear), the 0x2c-byte membership records 0x005849e0
    for (let t = 0; t < 8; t++) {
      wr8(0x004d60a9 + 0x230 * t, (t & 2) ? 0x80 : 0);
      for (let h = 0; h < 0x2c; h++) wr8(0x005849e0 + 0x2c * t + h, ((t + h) % 3) === 0 ? 1 : (h === 2 ? (t + 1) % 4 : 0));
    }
  }

  // golfer record g: every field the function reads; o = {x, y, ...} with names by record offset
  const F32 = { c0: 0x08, c4: 0x0c, fl: 0x10, x57c: 0xc4, x580: 0xc8, x584: 0xcc, x588: 0xd0, bx: 0xd4, by: 0xd8, bz: 0xdc,
                dir: 0xe0, sp: 0xe4, vz: 0xe8, spin: 0xec };
  const F16 = { cc: 0x14, ce: 0x16, w540: 0x88, w556: 0x9e, w558: 0xa0, w55c: 0xa4, w55e: 0xa6, w560: 0xa8, w562: 0xaa,
                w566: 0xae, w56a: 0xb2, type: 0xb6, w576: 0xbe };
  const F8 = { d0: 0x18, d1: 0x19, d2: 0x1a, d3: 0x1b, d4: 0x1c, d5: 0x1d, d6: 0x1e, d8: 0x20, d9: 0x21, da: 0x22, db: 0x23,
               b528: 0x70, b529: 0x71, b53c: 0x84, b53d: 0x85, b572: 0xba, name: 0xbb, b574: 0xbc, b5b1: 0xf9 };
  function golferBytes(gv, g, o) {
    const b = 0x100 * g;
    gv.setInt32(b, o.x | 0, true); gv.setInt32(b + 4, o.y | 0, true);
    Object.keys(F32).forEach(function (k) { if (o[k] !== undefined) gv.setInt32(b + F32[k], o[k] | 0, true); });
    Object.keys(F16).forEach(function (k) { if (o[k] !== undefined) gv.setInt16(b + F16[k], o[k] | 0, true); });
    Object.keys(F8).forEach(function (k) { if (o[k] !== undefined) gv.setUint8(b + F8[k], o[k] & 0xff); });
    gv.setInt16(b + 0xa2, (o.p === undefined ? (g ^ 1) : o.p) | 0, true);
    if (o.ef) o.ef.forEach(function (v, k) { gv.setUint8(b + 0x37 + k, v & 0xff); });
  }
  // a dead record still carries non-zero junk outside +0x21 (the loop skips it at 0x004289fe) and partner word g ^ 1
  function deadBytes(gv, g, R) {
    const b = 0x100 * g;
    for (let q = 0; q < 0x100; q += 4) gv.setUint32(b + q, (R() & 0x0f0f0f0f) >>> 0, true);
    gv.setUint8(b + 0x21, 0);
    gv.setInt16(b + 0xa2, g ^ 1, true);
    gv.setInt16(b + 0xb6, g & 7, true);
    gv.setUint8(b + 0x18, 0);
    gv.setUint8(b + 0xba, g & 3);
  }

  // globals: o = {date, gf (0x0059e7b8 bits 0x20 | 0x10 | 0x200000 | 0x20000 | 0x4000000 | 0x80000 | 0x1000), lvl (0x00822c88),
  // sel (0x005a59f8), ...}
  const GF_MASK = 0x20 | 0x10 | 0x1000 | 0x20000 | 0x80000 | 0x200000 | 0x4000000 | 0x100 | 1;   // bit 0: slopeCorners answers 0 (0x0040c2f0)
  function globals(o) {
    wr32(0x00834170, o.date === undefined ? 0x1234 : o.date);
    wr32(0x0059e7b8, ((ptr(0x0059e7b8).readU32() & ~GF_MASK) | ((o.gf || 0) & GF_MASK)) >>> 0);
    wr32(0x00822c88, o.lvl === undefined ? 1 : o.lvl);
    wr32(0x005a59f8, o.sel === undefined ? -1 : o.sel);
    wr32(0x00571fd4, o.cash === undefined ? 5000 : o.cash);
    wr8(0x005a34e0, o.ctype === undefined ? 0 : o.ctype);
    wr32(0x0059b048, o.b048 === undefined ? 0 : o.b048);
    wr32(0x0059b738, o.b738 || 0);
    wr32(0x0059b04c, o.b04c || 0);
    wr32(0x00543cb8, o.cb8 === undefined ? 1 : o.cb8);
    wr32(0x00543cc0, o.cc0 === undefined ? 1 : o.cc0);
    wr32(0x00543cc8, o.cc8 === undefined ? 1 : o.cc8);
    wr32(0x00543ccc, o.ccc === undefined ? 2 : o.ccc);
    wr32(0x004c2844, o.zoom === undefined ? 4 : o.zoom);
    wr8(0x00569498, o.b498 || 0);
    wr32(0x005685f0, o.f0 === undefined ? 40 : o.f0);
    wr32(0x0056a51c, o.a51c === undefined ? 1000 : o.a51c);
    wr32(0x005a5a24, o.a24 === undefined ? 0 : o.a24);
    wr16(0x005a6d3c, o.d3c === undefined ? 1234 : o.d3c);
    wr32(0x00578150, o.chx === undefined ? 25 : o.chx);
    wr32(0x00578154, o.chy === undefined ? 25 : o.chy);
    wr32(0x005a8c50, o.c50 === undefined ? 1 : o.c50);
    wr32(0x005a8c58, o.c58 === undefined ? 2 : o.c58);
    wr32(0x005a8c60, o.c60 === undefined ? 1 : o.c60);
    wr32(0x00822d9c, (o.seed || 0x1234567) >>> 0);
    wr32(0x0059b73c, 0x77); wr32(0x004c2e0c, -5); wr32(0x004c2ba0, 11); wr32(0x004c2ba4, 12); wr32(0x0058f330, 9);
    wr32(0x005a34e4, 3); wr32(0x00567afc, 1); wr32(0x00561254, 7); wr32(0x005787fc, 0x1111); wr32(0x00578800, 0x2222);
    wr32(0x004c2e28, 5); wr32(0x00568d0c, 0x3333); wr32(0x0056a91c, 0x4444); wr32(0x0056a920, 0x5555); wr32(0x00541318, 0x66);
    for (let k = 0; k < 152; k++) wr32(0x0056962c + 4 * k, o.t62c === undefined ? (k % 3 ? 0 : 0x1000 + k) : o.t62c);
    const s1 = new Uint8Array(0x1a00), s2 = new Uint8Array(0x7e0), s3 = new Uint8Array(2500), s4 = new Uint8Array(2500);
    for (let i = 0; i < s1.length; i += 4) { s1[i] = (i * 7) & 0x3f; s1[i + 1] = (i & 0x40) ? 1 : 0; }
    for (let i = 0; i < s2.length; i++) s2[i] = (i * 5) & 0x1f;
    for (let i = 0; i < 2500; i++) { s3[i] = i % 7 === 0 ? 0xff : (i * 3) & 0x7f; s4[i] = (i * 11) & 0x3f; }
    ptr(0x0056ae70).writeByteArray(s1.buffer); ptr(0x00584214).writeByteArray(s2.buffer);
    ptr(0x0053ea24).writeByteArray(s3.buffer); ptr(0x00578804).writeByteArray(s4.buffer);
    const t = new Uint8Array(0x200); t.fill(0x2e); t[0] = 0; ptr(0x0051a068).writeByteArray(t.buffer);
    const u = new Uint8Array(0x20); u.fill(0x23); ptr(0x0058a528).writeByteArray(u.buffer);
    const w = new Uint8Array(0x18); for (let i = 0; i < 0x18; i++) w[i] = 0x40 + i; ptr(0x00585850).writeByteArray(w.buffer);
    for (let q = 0x005689e8; q < 0x00568d08; q += 8) { wr16(q, (q >> 3) & 0xff); wr16(q + 4, (q >> 4) & 7); }
  }

  // ---- a scene: {seed, course:{...}, glob:{...}, golfers:[{g, ...fields}], rngN, rngDef, ps, shot, w2s, t2s}
  function applyScene(sc) {
    course(sc.course);
    globals(sc.glob || {});
    const R = rng(sc.seed ^ 0x5a5a5a5a);
    const gb = new ArrayBuffer(GOLF_N), gv = new DataView(gb);
    for (let g = 0; g < 152; g++) deadBytes(gv, g, R);
    sc.golfers.forEach(function (o) { golferBytes(gv, o.g, o); });
    if (sc.neg2) {
      const b = 0x005756a0, n = sc.neg2;
      wr8(b, n.par); wr8(b + 1, n.dir); wr32(b + 8, n.ax); wr32(b + 0xc, n.ay); wr32(b + 0x10, n.tx); wr32(b + 0x14, n.ty);
      wr32(b + 0x18, n.x); wr32(b + 0x1c, n.y);
    }
    ptr(GOLF).writeByteArray(gb);
    cur = sc;
  }

  // ---- Random::range(this = 0x00822d9c, n): the scene's value for this argument (rngN[n], a list consumed in order, the
  // last value repeating), else a value from the scene's sequence reduced by n; one call in five carries a high word that
  // every caller must drop (`and eax, 0xffff` / 16-bit stores).
  function rngStand(fx, k) {
    calleeStand(fx, k, 0x0045c1e0, 2, function (n, a) {
      const m = a[1] >>> 0, sc = cur;
      // per-argument call counters live in state memory ($rc: 64 slots {arg, count}), so both arms see the same sequence
      let slot = 0;
      while (slot < 63) {
        const ka = fx.rc.add(8 * slot).readU32(), kc = fx.rc.add(8 * slot + 4).readU32();
        if (kc === 0 || ka === m) break;
        slot++;
      }
      const cnt = fx.rc.add(8 * slot + 4).readU32();
      fx.rc.add(8 * slot).writeU32(m); fx.rc.add(8 * slot + 4).writeU32(cnt + 1);
      if (sc.rngN && sc.rngN[m] !== undefined) {
        const l = sc.rngN[m];
        const v = Array.isArray(l) ? l[Math.min(cnt, l.length - 1)] : l;
        return v >>> 0;
      }
      const base = (Math.imul((sc.seed + n * 0x9e3779b1) >>> 0, 0x85ebca6b) >>> 7) >>> 0;
      let v = m ? base % m : base & 0xffff;
      if ((n % 5) === 3) v = (v | 0x30000) >>> 0;
      return v;
    }, { abi: 'thiscall' });
  }

  const ST = {};
  function standAll(fx, real) {
    rngStand(fx, 0);
    calleeStand(fx, 1, 0x00467a00, 3, function (n, a) {                          // emitGolferThought(g, code, arg)
      if (cur.emit528) wr8(GOLF + 0x100 * (a[0] | 0) + 0x70, a[1]);              // the row lets the thought set the golfer's thought byte
      return 0;
    });
    calleeStand(fx, 2, 0x00466370, 2, function (n, a) { return (cur.story || 0) >>> 0; });   // advanceStory(g, 0)
    calleeStand(fx, 3, 0x00406670, 1, 0);                                        // membershipReport(g)
    calleeStand(fx, 4, 0x00427380, 1, 0);                                        // matchUpdate(g)
    calleeStand(fx, 5, 0x004266b0, 1, 0);                                        // patronEvent(g)
    calleeStand(fx, 6, 0x0040cb00, 3, 0, { extra: function () { return Array.from(new Uint32Array(ptr(0x0051a068).readByteArray(0x60))); } });   // startMessage: the text it sends
    calleeStand(fx, 7, 0x004481b0, 5, 0);                                        // playSound
    calleeStand(fx, 8, 0x0040c500, 4, 0);                                        // playSoundAt
    calleeStand(fx, 9, 0x0040c890, 4, 0);                                        // pointsPopup
    calleeStand(fx, 10, 0x0042fb90, 5, function (n, a) {                         // worldToScreen(wx, wy, &sx, &sy, margin)
      if (!(cur.w2s & (1 << (n & 7)))) return 0;
      ptr(a[2]).writeS32(((a[0] | 0) >> 6) + 3 * n); ptr(a[3]).writeS32(((a[1] | 0) >> 7) - n);
      return 1;
    }, { mask: [2, 3] });
    calleeStand(fx, 11, 0x0042f940, 4, function (n, a) {                         // tileToScreen(tx, ty, &sx, &sy)
      if (!(cur.t2s & (1 << (n & 7)))) return 0;
      ptr(a[2]).writeS32(((a[0] | 0) * 16 + 8 + n) | 0); ptr(a[3]).writeS32(((a[1] | 0) * 8 + 4) | 0);
      return 1;
    }, { mask: [2, 3] });
    calleeStand(fx, 12, 0x00478b80, 6, 0, { abi: 'thiscall' });                  // Widget_fillRect(this, x0, y0, x1, y1, color)
    calleeStand(fx, 13, 0x0045f0f0, 5, function (n) { return (0x45f0f0 + n) >>> 0; }, { str: { 0: 12 } });   // skillPointDialog
    calleeStand(fx, 14, 0x00480c80, 2, 0, { abi: 'thiscall' });                  // Window::refreshFull(this, 0)
    calleeStand(fx, 15, 0x0045c0c0, 1, 0);                                       // pumpAndRedraw(0)
    // the second log: stubs 16.. count from word 0x50 of $clog2 (same layout, separate log so the first one keeps room)
    const fx2 = { clog: fx.clog2, clogCap: fx.clog2Cap };
    calleeStand(fx2, 0, 0x00424120, 5, function (n, a) {                         // prepareShot(g, mode, -1, 0, 0): writes the shot direction
      const l = cur.shot || [0x30000000];
      wr32(GOLF + 0x100 * (a[0] | 0) + 0xe0, l[Math.min(n, l.length - 1)]);
      if (cur.shotD5) wr8(GOLF + 0x100 * (a[0] | 0) + 0x1d, cur.shotD5[n % cur.shotD5.length]);
      return 0;
    });
    calleeStand(fx2, 1, 0x00409620, 1, 0);                                       // scanShotArea(slot)
    calleeStand(fx2, 2, 0x00409a90, 1, 0);                                       // addGolferToTable(g)
    if (!real) {
      calleeStand(fx2, 3, 0x0042e7e0, 5, function (n) {                          // pathStep(tx, ty, wx, wy, g)
        const l = cur.ps || [2];
        if (cur.ps80) wr32(0x0059e7b8, ptr(0x0059e7b8).readU32() | 0x80000);      // the real one sets it on a redirect (0x0042ee18)
        return l[n % l.length] >>> 0;
      });
      calleeStand(fx2, 4, 0x004099f0, 1, 0);                                     // addGolferPair(g)
      calleeStand(fx2, 5, 0x00409950, 1, 0);                                     // updatePairSnapshot(slot)
      calleeStand(fx2, 6, 0x00426670, 1, 0);                                     // resetGolfer(g)
    } else {
      calleeStand(fx2, 7, 0x00404b70, 4, 0, { str: { 0: 2 } });                  // pathStep's drawCenteredText (flag 0x1000)
    }
  }

  function base(real) {
    freezeNow();
    const fx = { clog: newClog(CLOG), clogCap: CLOG, clog2: newClog(0x2000), clog2Cap: 0x2000, rc: alloc(0x200) };
    saveAll();
    return fx;
  }

  // ====================================================================================================================
  // Scene generators
  // ====================================================================================================================
  function W(t) { return t * 1024 + 512; }
  function pick(R, arr) { return arr[R() % arr.length]; }
  function defCourse(seed, o) {
    const R = rng(seed);
    const holes = [];
    for (let k = 0; k < 20; k++) {
      const hx = 6 + R() % 38, hy = 6 + R() % 38;
      holes.push({ x: hx, y: hy, tx: (hx + 9 + R() % 20) % 50, ty: (hy + 7 + R() % 20) % 50, ax: (hx + 30) % 50, ay: (hy + 25) % 50,
                   par: k === 0 ? 0 : 3 + R() % 3, dir: R() & 0xff, green: k === 0 ? 0 : 1 + (R() & 1) });
    }
    return Object.assign({ seed: seed, water: 0.06, rough: 0.2, sand: 0.05, block: 0.03, misc: 0.1, flags: 50, walls: 30, hills: 3,
                           flat: false, tv: seed % 4, holes: holes, objs: [], pairs: [] }, o || {});
  }
  // a live golfer near tile (tx, ty) on hole h with randomized secondary fields
  function liveGolfer(R, g, h, tx, ty, o) {
    const q = {
      g: g, x: tx * 1024 + R() % 1024, y: ty * 1024 + R() % 1024, d9: h, d2: R() & 7, d5: pick(R, [0, 7, 8, 9, 0xb, 0xc, 0xd, 0x10, 0x11]),
      d0: pick(R, [0, 0, 0, 0x20, 0x21, 0x40, 0x60, 0x62, 0x80]), d1: R() & 0xff, d3: R() % 12, d4: pick(R, [0, 0xd, 3, 0xa, 0xb]),
      d6: R() & 3, d8: 0, da: R() % 3, db: 0, fl: 0, c0: 100 + R() % 500, c4: 100 + R() % 300, cc: 0, ce: R() & 0x3ff,
      w540: pick(R, [0, 0x4000, 0x8000, 0xc000, 0x1234]), w556: 0, w558: 0, w55c: pick(R, [0, 3, 0, 5]), w55e: R() % 12, w560: R() % 12,
      w562: R() % 120, w566: R() & 3, w56a: R() % 6, type: R() % 8, w576: R() % 40, b528: pick(R, [0, 1, 0x24, 0x8b, 0x0b]),
      b529: pick(R, [0, 2, 0x0b]), b53c: 0, b53d: R() % 0x20, b572: R() & 3, name: R() % 4, b574: R() & 0xff, b5b1: R() % 0x60,
      x57c: 0, x580: 0, x584: 0, x588: 0, bx: 0, by: 0, bz: 0, dir: (R() & 0xffffffff) | 0, sp: 0, vz: 0, spin: (R() % 0x4000000) - 0x2000000
    };
    return Object.assign(q, o || {});
  }

  // ---- the scene families. Each returns a scene; the row list for a key is a family's scenes over several seeds plus
  // hand-made boundary rows.
  const FAM = {};
  const S = 0x10000;

  // tick: the per-tick bookkeeping at the top (0x004289ef..0x00429178)
  FAM.tick = function (seed, v) {
    const R = rng(seed * 7919);
    const c = defCourse(seed, { flags: 70 });
    const gl = [];
    const used = {};
    const n = 4 + R() % 8;
    for (let k = 0; k < n; k++) {
      let g = (R() % 76) * 2;
      while (used[g]) g = (g + 2) % 152;
      used[g] = 1;
      g ^= R() & 1;
      const h = 1 + R() % 18;
      const tx = 4 + R() % 42, ty = 4 + R() % 42;
      const fam = (k + v) % 7;
      const a = liveGolfer(R, g, h, tx, ty), b = liveGolfer(R, g ^ 1, h, tx + 1, ty);
      if (fam === 0) { a.d9 = 0xff; a.b53c = pick(R, [0, 0, 1, 3]); b.d9 = pick(R, [0xff, 0]); }       // arriving
      if (fam === 1) { a.b53c = pick(R, [1, 2, 7, 3]); a.b53d = pick(R, [0x32, 0x32, 5]); a.w56a = pick(R, [4, 4, 2]); }
      if (fam === 2) {                                                         // story advance
        a.fl = 0x100000 | pick(R, [0, 0, 0x200000]); a.d5 = pick(R, [7, 8, 0xa, 0x10, 3]); b.d5 = pick(R, [7, 9, 0x10, 6]);
        a.w56a = pick(R, [0, 2, 5, -6 + 1]); b.d9 = pick(R, [h, h, 0]); a.c0 = pick(R, [0x33, 0x100, 0x2ed, 0x2ee, 0x32]);
        a.c4 = pick(R, [0x33, 0x100, 0x1c1, 0x1c2, 0x32]); b.x = a.x + pick(R, [0, 0x200, 0x800, 0x3000]);
      }
      if (fam === 3) { a.cc = pick(R, [0, 5, -3]); a.w540 = pick(R, [0, 0x4000, 0x8000, 0xc000]); }   // the tile thought
      if (fam === 4) { a.d9 = pick(R, [1, 2, 3, 0x13, 5]); a.db = pick(R, [0, 0, 1]); a.w558 = pick(R, [0, 0, 2]); }
      if (fam === 5) { a.w556 = pick(R, [-1, -5, -100]); a.fl = pick(R, [0, 0x40000]); a.sp = pick(R, [0, 0x100]); }
      if (fam === 6) { a.fl = pick(R, [0x80000000, 0x8000, 0x80008000]) | 0; a.da = pick(R, [9, 10, 0x7f, 0x80]); }
      gl.push(a, b);
    }
    // pair slots naming some golfers, with and without flag 0x40000
    const pairs = [];
    for (let s = 0; s < 3; s++) { const o = gl[(R() % gl.length)]; pairs.push({ slot: (s * 5 + seed) % 16, id: o.g }); if (R() & 1) o.fl |= 0x40000; }
    c.pairs = pairs;
    // tile 0x16 under objects of type 2 / 4 next to some golfers, and 0x1000 / 0x800 tile flags
    c.patch = [];
    c.objs = [];
    gl.forEach(function (o, i) {
      if (i % 3) return;
      const tx = o.x >> 10, ty = o.y >> 10, d = R() & 7;
      const DX = [0, 1, 1, 1, 0, -1, -1, -1], DY = [-1, -1, 0, 1, 1, 1, 0, -1];
      const ox = tx + DX[d], oy = ty + DY[d];
      c.patch.push([ox, oy, pick(R, [0x16, 0x16, 2]), pick(R, [0x1000, 0x1800, 0x800, 0])]);
      c.objs.push({ x: ox, y: oy, type: pick(R, [2, 4, 4, 5]), val: pick(R, [0xf, 0x10, 3]) });
    });
    // water-side tiles (class 7 / 8 / 0xe) around some golfers
    gl.forEach(function (o, i) { if (i % 4 === 1) c.patch.push([(o.x >> 10) - 1, (o.y >> 10), pick(R, [7, 5, 3]), null]); });
    const date = pick(R, [0, 8, 0x40, 0x3f8, 0x1000]) + (v * 37) % 0x100;
    return { seed: seed, course: c, golfers: gl,
             glob: { date: date, lvl: pick(R, [0, 1, 3]), ctype: pick(R, [0, 3, 3]), b048: date - pick(R, [0x96, 0x97, 0x200]), b498: pick(R, [0, 0, 1]),
                     zoom: pick(R, [3, 4, 4]), f0: pick(R, [0, 10, 40]), seed: seed },
             rngN: (v & 1) ? { 100: [0, 3], 2: [1, 0], 6: [0, 7], 5: [2, 0, 4] } : undefined, story: seed & 3 };
  };

  // ---- shared pieces of the other families
  const DX = [0, 1, 1, 1, 0, -1, -1, -1], DY = [-1, -1, 0, 1, 1, 1, 0, -1];
  let angleFn = null;
  function angle(dx, dy) {                                   // angleFixed 0x004672d0 (pure): targets computed like the function does
    if (!angleFn) angleFn = new NativeFunction(ptr('0x004672d0'), 'int', ['int', 'int']);
    return angleFn(dx | 0, dy | 0);
  }
  function adir(a) { return ((((a >> 28) & 0xf) + 1) >> 1) & 7; }
  // n golfer indexes from distinct pairs (from..to), each the even or the odd one of its pair
  function distinctGs(R, n, from, to) {
    const used = {}, out = [];
    const span = ((to - from) >> 1) + 1;
    while (out.length < n) {
      let q = R() % span;
      while (used[q]) q = (q + 1) % span;
      used[q] = 1; out.push(from + 2 * q + (R() & 1));
    }
    return out;
  }
  function inb(t) { return Math.max(3, Math.min(46, t)); }
  // the hole's tee target that updateGolfers computes at 0x0042a0aa..0x0042a181 (no ball, L38 = 0)
  function teeTarget(hole, o) {
    let ex, ey;
    if ((o.d1 & 0xff) <= 4 && (o.d1 & 0xff) !== 3 && (o.d0 & 0xff) === 0) { ex = hole.tx * 1024 + 0x200; ey = hole.ty * 1024 + 0x200; }
    else { ex = hole.ax * 1024 + 0x200; ey = hole.ay * 1024 + 0x200; }
    const d = adir(angle(hole.x * 1024 - ex + 0x200, hole.y * 1024 - ey + 0x200));
    const m = ((d & 1) ? 3 : 5) << 6;
    return [ex - DX[d] * m, ey - DY[d] * m];
  }
  // the ball target of 0x00429f08..0x0042a09a (ball lying, L24 = 0)
  function ballTarget(hole, o, l14) {
    const a = angle(hole.x * 1024 - o.bx + 0x200, hole.y * 1024 - o.by + 0x200);
    const e = ((a >> 28) + 1) >> 1 & 7;
    const m = (l14 !== 1) ? ((e & 1) ? 3 : 5) : ((~e & 1) | 2);
    return [o.bx - DX[e] * (m << 6), o.by - DY[e] * (m << 6)];
  }
  function sceneOf(seed, c, gl, glob, extra) {
    return Object.assign({ seed: seed, course: c, golfers: gl, glob: Object.assign({ seed: seed }, glob || {}) }, extra || {});
  }

  // leave: the leaving message (0x00429419), the leaving walk (0x20000000, 0x00429847), the clubhouse reset (0x0042990d),
  // a hole without par (0x004299b7), the patron loop over golfers on the same hole (0x004299d7)
  FAM.leave = function (seed, v) {
    const R = rng(seed * 104729);
    const c = defCourse(seed, { flags: 30 });
    const gl = [];
    const gs = distinctGs(R, 3 + R() % 5, 0, 150);
    const glob = { date: 0x2000 + R() % 0x400, gf: pick(R, [0, 0, 0x200000, 0x4000000]), sel: -1, chx: 10 + R() % 30, chy: 10 + R() % 30,
                   a51c: 500 + R() % 100 };
    const parZero = 1 + R() % 18;
    if (v & 1) c.holes[parZero].par = 0;
    gs.forEach(function (g, k) {
      const h = 1 + R() % 18, tx = 5 + R() % 40, ty = 5 + R() % 40;
      const a = liveGolfer(R, g, h, tx, ty), b = liveGolfer(R, g ^ 1, pick(R, [h, h, 0, 0x13]), tx + 1, ty);
      const mode = (k + v) % 6;
      if (mode === 0) {
        a.w55c = pick(R, [-1, -3, -1]); a.b53c = pick(R, [0, 0, 0, 2]);
        a.b53d = pick(R, [4, 8, 9, 0xc, 0xd, 0xe, 0xf, 0x15, 0x17, 0x1a, 0x1e, 2, 0x84, 0x8c, 0x1b, 0x3a]);
        a.d9 = pick(R, [h, h, h, 0x13, 0x12]); a.fl = pick(R, [0, 0, 0x200, 0x10000, 0x10200]); a.d0 = pick(R, [0, 0, 0, 0x40, 0x20, 0x21]);
        if (R() & 1) glob.sel = g;
      } else if (mode === 1) {
        a.fl = 0x20000000 | pick(R, [0, 0x20000, 0x200]); a.w55c = pick(R, [-9, -10, -11, 0, 4]); a.d9 = pick(R, [0x13, 0x13, h]);
        if (R() % 3 === 0) { a.x = glob.chx * 1024 + 0x200 + (R() % 0x60) - 0x30; a.y = glob.chy * 1024 + 0x200 + (R() % 0x60) - 0x30; }
        if (R() & 1) glob.date = 100 * (100 + (R() % 50)) - ((35 * g) % 100);
      } else if (mode === 2) {
        a.d9 = 0x13; a.fl = pick(R, [0, 0x200, 0]); a.c0 = pick(R, [-1, 200, 300]); b.fl = pick(R, [0, 0x200]);
        b.d9 = pick(R, [0, 0x13, h]);
        if (R() & 1) { a.x = glob.chx * 1024 + R() % 1024; a.y = glob.chy * 1024 + R() % 1024; }
        if (R() & 1) glob.sel = pick(R, [g, g ^ 1, 77]);
      } else if (mode === 3) {
        a.d9 = parZero; a.da = pick(R, [0, 1]);
      } else {
        a.da = 0; a.d9 = h; a.w576 = R() % 30;
        if (R() & 1) glob.sel = pick(R, [g, g ^ 1]);
        // other golfers on the same hole
        const more = 1 + R() % 4;
        for (let q = 0; q < more; q++) {
          const og = (g + 20 + 2 * q * 7 + (R() % 3) * 2) % 152;
          if (gl.some(function (x) { return x.g === og || x.g === (og ^ 1); }) || gs.indexOf(og) >= 0 || gs.indexOf(og ^ 1) >= 0) continue;
          gl.push(liveGolfer(R, og, pick(R, [h, h, h + 1 > 18 ? 1 : h + 1]), tx + 2, ty + 2, { da: pick(R, [0, 0, 1]), w576: R() % 40 }));
        }
      }
      gl.push(a, b);
    });
    // the golfers reach the target code afterwards: amenities for some
    c.objs = [];
    for (let q = 0; q < 4; q++) c.objs.push({ x: 5 + R() % 40, y: 5 + R() % 40, type: pick(R, [7, 3, 6, 8, 10]), f7: 0x40 });
    return sceneOf(seed, c, gl, glob, { ps: [R() & 7, (R() & 7) | 0x1200, 1, 6] });
  };

  // walk: target selection (0x00429a75..0x0042a815) and the step (0x0042afb8..0x0042bc6e) with crowds
  FAM.walk = function (seed, v) {
    const R = rng(seed * 15485863);
    const c = defCourse(seed, { flags: 60 });
    const gl = [];
    const gs = distinctGs(R, 3 + R() % 6, 0, 150);
    const glob = { date: 0x3000 + R() % 0x800, gf: pick(R, [0, 0, 0x200000, 0x20000, 0x80000, 0x10, 0x220000]), sel: -1,
                   lvl: pick(R, [0, 1, 3]), b04c: pick(R, [0, 0, 1]), a51c: 900 + R() % 100, a24: R() % 152,
                   cb8: pick(R, [0, 1, 1]), cc0: pick(R, [0, 1, 1]), cc8: pick(R, [0, 1, 1]), ccc: R() % 4 };
    c.objs = [];
    c.patch = [];
    gs.forEach(function (g, k) {
      const h = 1 + R() % 18, tx = 5 + R() % 40, ty = 5 + R() % 40;
      const hole = c.holes[h];
      const a = liveGolfer(R, g, h, tx, ty), b = liveGolfer(R, g ^ 1, pick(R, [h, h, h, 0, 0x13]), inb(tx + (R() % 5) - 2), inb(ty + (R() % 5) - 2));
      a.fl = pick(R, [0, 0, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x10000, 0x40000000, 0x2000000, 0x4000000, 0x8000, 0x1400, 0x10400, 0x40000]);
      b.fl = pick(R, [0, 0x400, 0x40000, 0x20000000, 0x4000]);
      a.cc = pick(R, [0, 0, 0, 0x100, 5, -1, 0x40]); a.w55e = R() % 14; a.w560 = R() % 14; a.w562 = pick(R, [0, 10, 119, 120, 121, 160, 200, 30]);
      a.d5 = pick(R, [6, 7, 8, 9, 0xa, 0xb, 0x11]);
      const ph = R() % 4;
      if (ph === 0) { /* no ball yet */ }
      else {
        a.bx = inb(tx + (R() % 9) - 4) * 1024 + R() % 1024; a.by = inb(ty + (R() % 9) - 4) * 1024 + R() % 1024;
        a.d8 = 0;
        if (R() & 1) { b.bx = a.bx + (R() % 0x1000) - 0x800; b.by = a.by + (R() % 0x1000) - 0x800; }
        if (R() % 3 === 0) { b.bz = pick(R, [0, 5]); b.d5 = pick(R, [3, 0xb, 0xc, 0xd, 0x10, 0x11]); b.dir = angle(b.bx - a.x, b.by - a.y) + pick(R, [0, 0x40000000, 0x5000000]); }
        if (ph === 3) { a.x = a.bx + (R() % 0x400) - 0x200; a.y = a.by + (R() % 0x400) - 0x200; }
      }
      if (R() % 4 === 0) { a.w556 = pick(R, [-2, 0]); }
      if (R() % 5 === 0) { a.d2 = 0xff; }
      // amenities and target tiles near this golfer
      if (R() % 2 === 0) c.objs.push({ x: inb(tx + (R() % 11) - 5), y: inb(ty + (R() % 11) - 5), type: pick(R, [7, 3, 6, 8, 10, 1, 7]), f7: pick(R, [0x40, 0x40, 0]) });
      if (R() % 3 === 0) c.patch.push([inb(tx + (R() % 5) - 2), inb(ty + (R() % 5) - 2), null, 0x200]);
      // the tee or ball target nearby for some golfers (arrival)
      if (R() % 4 === 0) {
        const t = (a.bx ? ballTarget(hole, a, 2) : teeTarget(hole, a));
        a.x = t[0] + (R() % 0x60) - 0x30; a.y = t[1] + (R() % 0x60) - 0x30;
      }
      gl.push(a, b);
      // a crowd: golfers right next to this one (the 0x0042b052 loop)
      if (R() % 2 === 0) {
        const nn = 1 + R() % 5;
        for (let q = 0; q < nn; q++) {
          const og = (g + 2 * (q + 3) * (1 + (R() % 7))) % 152;
          if (gl.some(function (x) { return x.g === og || x.g === (og ^ 1); }) || gs.indexOf(og) >= 0 || gs.indexOf(og ^ 1) >= 0) continue;
          const d = R() & 7;
          gl.push(liveGolfer(R, og, pick(R, [h, h, h - 1 > 0 ? h - 1 : h, 0x13]), tx, ty, {
            x: a.x + DX[d] * (R() % 0x300), y: a.y + DY[d] * (R() % 0x300), da: pick(R, [0, 0, 1]), w576: R() % 40,
            fl: pick(R, [0, 0, 0x20000000, 0x400]), d2: pick(R, [a.d2, R() & 7]), d5: pick(R, [0xb, 7]) }));
        }
      }
      if (R() % 3 === 0) glob.sel = pick(R, [g, g ^ 1]);
    });
    const ps = []; for (let q = 0; q < 8; q++) ps.push(pick(R, [0, 1, 2, 3, 4, 5, 6, 7, 0x12345606, 0xff]));
    return sceneOf(seed, c, gl, glob, { ps: ps });
  };

  // shot: the ball lies still and the golfer is at it (0x0042bc8f..0x0042c0bd)
  FAM.shot = function (seed, v) {
    const R = rng(seed * 32452843);
    const c = defCourse(seed, { flags: 60 });
    const gl = [];
    const gs = distinctGs(R, 3 + R() % 5, 0, 150);
    const glob = { date: 0x5000 + R() % 0x800, gf: pick(R, [0, 0, 0x200000]), sel: -1, b738: pick(R, [0, 0, 0, 1]) };
    gs.forEach(function (g, k) {
      const h = 1 + R() % 18;
      const hole = c.holes[h];
      const nearCup = R() % 4 === 0;
      const bt = nearCup ? [hole.x, hole.y] : [inb(hole.x + (R() % 21) - 10), inb(hole.y + (R() % 21) - 10)];
      const a = liveGolfer(R, g, h, bt[0], bt[1]);
      a.bx = nearCup ? hole.x * 1024 + 0x200 + (R() % 0x180) - 0xc0 : bt[0] * 1024 + R() % 1024;
      a.by = nearCup ? hole.y * 1024 + 0x200 + (R() % 0x180) - 0xc0 : bt[1] * 1024 + R() % 1024;
      a.d8 = pick(R, [1, 1, 2, 5]); a.sp = 0; a.fl = pick(R, [0, 0, 0x200, 0x200, 0x1800, 0x400]); a.x584 = pick(R, [0, 0, -1, 7]);
      a.c0 = pick(R, [-1, 300]); a.d0 = pick(R, [0, 0, 0x20, 0x40]); a.d9 = pick(R, [h, h, 1]); a.da = pick(R, [0, 0, 1]);
      const b = liveGolfer(R, g ^ 1, pick(R, [h, a.d9, 0]), bt[0], bt[1], { d5: pick(R, [5, 6, 7, 0xb]), d0: pick(R, [0, 0x20, 0x21]) });
      if (R() & 1) b.d9 = a.d9;
      gl.push(a, b);
      if (R() % 3 === 0) glob.sel = g;
    });
    // ball tiles: some greens (L14 == 1), cup flags
    c.patch = gl.filter(function (o) { return o.bx; }).map(function (o) { return [o.bx >> 10, o.by >> 10, pick(R, [1, 2, 4, 1]), pick(R, [0, 0x80, 0])]; });
    const l = []; for (let q = 0; q < 6; q++) l.push((R() & 0xffffffff) | 0);
    return sceneOf(seed, c, gl, glob, { shot: l });
  };

  // swing: the ball moving with the golfer's swing counter d8 (0x0042c0c2..0x0042c65b)
  FAM.swing = function (seed, v) {
    const R = rng(seed * 49979687);
    const c = defCourse(seed, { flags: 40 });
    const gl = [];
    const gs = distinctGs(R, 3 + R() % 5, 0, 150);
    const glob = { date: 0x6000 + R() % 0x800, gf: pick(R, [0, 0x200000]), sel: -1, b04c: pick(R, [0, 0, 1]) };
    gs.forEach(function (g, k) {
      const h = 1 + R() % 18;
      const hole = c.holes[h];
      const tx = inb(hole.tx + (R() % 7) - 3), ty = inb(hole.ty + (R() % 7) - 3);
      const a = liveGolfer(R, g, h, tx, ty);
      a.bx = a.x + 0x100; a.by = a.y; a.x57c = a.bx; a.x580 = a.by; a.sp = pick(R, [0x100, 0x400, 0x40]);
      a.d8 = pick(R, [1, 1, 1, 2, 3, 6, 7, 8, 0x1f, 0x78, 0x7f, 0x80, 0xff, 0x10]); a.d5 = pick(R, [0x10, 0xb, 4, 5, 0]);
      a.fl = pick(R, [0, 0x200, 0x4000, 0x400000, 0x40000, 0x4200, 0x4000 | 0x40000]); a.d4 = pick(R, [0, 0xd, 0xa, 3]);
      a.d1 = pick(R, [0, 1, 2, 4, 7, 0x10, 0x17, 0xf0]); a.da = pick(R, [0, 0, 1, 2]); a.c0 = pick(R, [-1, 300]);
      a.x584 = inb(hole.x + (R() % 5) - 2); a.x588 = inb(hole.y + (R() % 5) - 2); a.dir = (R() & 0xffffffff) | 0;
      const b = liveGolfer(R, g ^ 1, h, tx, ty, { w556: pick(R, [0, -3, 5]) });
      gl.push(a, b);
      // others on the hole further along, with 0x400 (the follow loop 0x0042c178)
      const nn = R() % 4;
      for (let q = 0; q < nn; q++) {
        const og = (g + 30 + 2 * q * 5) % 152;
        if (gl.some(function (x) { return x.g === og || x.g === (og ^ 1); }) || gs.indexOf(og) >= 0 || gs.indexOf(og ^ 1) >= 0) continue;
        const near = R() % 3;
        const ox = near === 0 ? hole.ax : (near === 1 ? a.x584 : inb(hole.x + (R() % 11) - 5));
        const oy = near === 0 ? hole.ay : (near === 1 ? a.x588 : inb(hole.y + (R() % 11) - 5));
        gl.push(liveGolfer(R, og, pick(R, [h, h, h, h + 1 > 18 ? 1 : h + 1]), ox, oy, {
          fl: pick(R, [0x400, 0x400, 0]), da: a.da + pick(R, [1, 2, 0]), w576: pick(R, [0, 10, 40]),
          bx: (ox + pick(R, [0, 1, 6])) * 1024 + 0x200, by: oy * 1024 + 0x200 }));
      }
      if (R() % 3 === 0) glob.sel = g;
      if (R() & 1) a.w576 = R() % 40;
    });
    return sceneOf(seed, c, gl, glob, { rngN: (v % 3 === 0) ? { 0: [0, 1], 5: [0, 3] } : undefined });
  };

  // flight: the ball in the air and rolling (0x0042c661..0x0042d35f)
  FAM.flight = function (seed, v) {
    const R = rng(seed * 86028121);
    const c = defCourse(seed, { flags: 70, walls: 60, water: 0.1 });
    const gl = [];
    const gs = distinctGs(R, 3 + R() % 6, 0, 150);
    const glob = { date: 0x7000 + R() % 0x800, gf: pick(R, [0, 0x200000]), lvl: pick(R, [0, 1, 2]), ctype: pick(R, [0, 1, 2, 3, 4]), t62c: pick(R, [0, 0x6f00]) };
    c.patch = [];
    gs.forEach(function (g, k) {
      const h = 1 + R() % 18;
      const hole = c.holes[h];
      const tx = 4 + R() % 42, ty = 4 + R() % 42;
      const a = liveGolfer(R, g, h, tx, ty);
      const near = R() % 3 === 0;
      const bx = near ? hole.x * 1024 + 0x200 + (R() % 0x300) - 0x180 : tx * 1024 + R() % 1024;
      const by = near ? hole.y * 1024 + 0x200 + (R() % 0x300) - 0x180 : ty * 1024 + R() % 1024;
      a.bx = bx; a.by = by; a.x57c = bx - 0x1000 + R() % 0x2000; a.x580 = by - 0x1000 + R() % 0x2000;
      a.d8 = 0; a.sp = pick(R, [0x40, 0x100, 0x13f, 0x140, 0x200, 0x800, 0x41, 0x3ff]); a.bz = pick(R, [0, 0, 1, 2, 30, 100, -3]);
      a.vz = pick(R, [0, -1, -0x40, -0x101, -0x200, 0x10, 0x40, 0x3f, 0xc9, 0x200, -0xc1]); a.spin = pick(R, [0, 0x1000000, -0x2000000, 0x123456]);
      a.dir = (R() & 0xffffffff) | 0; a.fl = pick(R, [0, 0x40000, 0x100, 0x80, 0x2, 0x40080, 0x40100]); a.d0 = pick(R, [0, 0, 0x21]); a.ce = pick(R, [0, 0x200]);
      a.d4 = pick(R, [0, 0xd]); a.b5b1 = pick(R, [0, 0x30, 0x63]);
      if (R() & 1) { a.w556 = -1; a.fl |= 0x40000; } else { a.d8 = 0; }
      const tt = pick(R, [1, 2, 4, 0xa, 0xc, 0x11, 0xd, 0xe, 0xf, 0x10, 0x15, 0x16, 6, 1]);
      c.patch.push([bx >> 10, by >> 10, tt, pick(R, [0, 0x80, 0x80 | 3, 0x1, 0x5])]);
      if (R() & 1) c.patch.push([(bx >> 10) + pick(R, [-1, 1, 0]), (by >> 10) + pick(R, [-1, 1, 0]), pick(R, [0xa, 1, 0x11, 4]), null]);
      if (R() % 4 === 0) c.patch.push([a.x57c >> 10, a.x580 >> 10, pick(R, [1, 2, 4]), null]);
      const b = liveGolfer(R, g ^ 1, pick(R, [h, h, 0]), tx, ty);
      gl.push(a, b);
      // golfers nearby on other holes (the duck loop 0x0042d2eb)
      if (R() % 2 === 0) gl.push(liveGolfer(R, (g + 50) % 152, pick(R, [h, h + 1 > 18 ? 1 : h + 1, 0x13]), bx >> 10, by >> 10,
        { bx: bx + 0x100, by: by, x: bx + (R() % 0x800) - 0x400, y: by + (R() % 0x800) - 0x400 }));
    });
    return sceneOf(seed, c, gl, glob, { rngN: (v % 4 === 1) ? { 0x180: [0, 5], 8: [0, 1], 2: [0, 1], 0x64: [0, 0x50] } : undefined });
  };

  // stop: the ball comes to rest (0x0042d361..0x0042db26)
  FAM.stop = function (seed, v) {
    const R = rng(seed * 179424673);
    const c = defCourse(seed, { flags: 70, water: 0.15 });
    const gl = [];
    const gs = distinctGs(R, 3 + R() % 6, 0, 150);
    const glob = { date: 0x8000 + R() % 0x800, lvl: pick(R, [0, 1, 3]), gf: pick(R, [0, 0x200000]) };
    c.patch = [];
    gs.forEach(function (g, k) {
      const h = 1 + R() % 18;
      const hole = c.holes[h];
      const tx = 4 + R() % 42, ty = 4 + R() % 42;
      const a = liveGolfer(R, g, h, tx, ty);
      const bx = tx * 1024 + 0x100 + R() % 0x200, by = ty * 1024 + 0x100 + R() % 0x200;
      a.bx = bx; a.by = by; a.sp = pick(R, [1, 2, 0x10, 0x20, 0x3f]); a.bz = 0; a.vz = 0; a.d8 = 0; a.fl = pick(R, [0x40000, 0x40000 | 0x4000000, 0x40002, 0x40020, 0x40060, 0x40004]);
      a.w556 = -1;
      const sx = inb(pick(R, [tx - 6, tx + 7, hole.x, tx])), sy = inb(pick(R, [ty - 5, ty + 3, hole.y, ty]));
      a.x57c = sx * 1024 + 0x200; a.x580 = sy * 1024 + 0x200;
      a.da = pick(R, [0, 0, 1, 2, (c.holes[h].par - 3) & 0xff, 7, 8]); a.d3 = pick(R, [0, 3, 8, 9, 12]); a.b53c = pick(R, [0, 0, 1]);
      a.b528 = pick(R, [0, 1]); a.d9 = pick(R, [h, h, 2, 4]);
      const tt = pick(R, [1, 2, 4, 5, 6, 0x11, 0x11, 0x14, 0xa, 2, 0]);
      c.patch.push([tx, ty, tt, pick(R, [0, 0x100, 0x80, 0])]);
      c.patch.push([sx, sy, pick(R, [1, 2, 4, 5, 0x14]), null]);
      if (tt === 0x11) for (let q = 1; q < 6; q++) c.patch.push([inb(tx + q * pick(R, [-1, 1])), ty, pick(R, [2, 0x11, 0xa, 4]), null]);
      gl.push(a, liveGolfer(R, g ^ 1, h, tx, ty));
    });
    return sceneOf(seed, c, gl, glob);
  };

  // draw: the debug line (flag 0x20 of 0x0059e7b8, 0x0042917d..0x00429270)
  FAM.draw = function (seed, v) {
    const R = rng(seed * 2038074743);
    const sc = FAM.walk(seed, v);
    sc.glob.gf = (sc.glob.gf | 0x20) >>> 0;
    sc.w2s = R() & 0xff; sc.t2s = R() & 0xff;
    sc.golfers.forEach(function (o) { if (R() & 1) o.fl = (o.fl | 0x10000000) >>> 0; o.x584 = R() % 50; o.x588 = R() % 50; if (R() & 1) o.sp = 0x100; });
    return sc;
  };

  // real: the walk, swing, flight and stop scenes with pathStep, updatePairSnapshot, addGolferPair and resetGolfer running
  FAM.real = function (seed, v) {
    const sc = FAM[['walk', 'swing', 'flight', 'stop', 'leave', 'tick'][v % 6]](seed, v);
    const R = rng(seed * 7);
    sc.course.pairs = (sc.course.pairs || []).concat([{ slot: 1 + R() % 14, id: sc.golfers[0].g }, { slot: 15, id: sc.golfers[sc.golfers.length - 1].g }]);
    sc.glob.gf = (sc.glob.gf & ~0x1000) >>> 0;
    return sc;
  };

  // ====================================================================================================================
  // edge scenes: one builder per gap of the first coverage run (log/c3/alt/c3ec_cov_report.txt), each with variants k
  // ====================================================================================================================
  function blank(seed, o) {
    return defCourse(seed, Object.assign({ water: 0, rough: 0, sand: 0, block: 0, misc: 0, flags: 0, walls: 0, hills: 0, flat: true }, o || {}));
  }
  // a quiet golfer: every field the function reads, explicit
  function walker(g, h, x, y, o) {
    return Object.assign({ g: g, x: x, y: y, d9: h, d2: 2, d5: 7, d0: 0, d1: 0, d3: 0, d4: 0, d6: 0, d8: 0, da: 0, db: 0, fl: 0,
      c0: 300, c4: 200, cc: 0, ce: 0, w540: 0, w556: 0, w558: 0, w55c: 0, w55e: 0, w560: 0, w562: 0, w566: 0, w56a: 1, type: 1,
      w576: 10, b528: 0, b529: 0, b53c: 0, b53d: 0, b572: 0, name: 0, b574: 0, b5b1: 0, x57c: 0, x580: 0, x584: 0, x588: 0,
      bx: 0, by: 0, bz: 0, dir: 0, sp: 0, vz: 0, spin: 0 }, o || {});
  }
  // a date with (date + mult * i) % mod == 0
  function dateHit(i, mult, mod, k) { return mod * (k + 60) - ((mult * i) % mod); }
  function hdir(c, h) { return c.holes[h].dir & 0xff; }
  const EDGE = [];

  // 0: the 0x0057953c countdown reaching 6 / 1 (0x00428ade..0x00428b52)
  EDGE.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    distinctGs(R, 6, 0, 150).forEach(function (g, q) {
      const v = (q + k) % 6;
      gl.push(walker(g, 1 + q, W(8 + 5 * q), W(20), { b53c: [7, 7, 7, 7, 2, 7][v], b53d: [0x32, 0x32, 0x32, 0x31, 0x32, 0x32][v],
        c0: [300, -1, 300, 300, 300, 300][v], fl: [0, 0, 0x100000, 0, 0, 0][v], w56a: [4, 4, 4, 4, 4, 3][v] }));
      gl.push(walker(g ^ 1, 1 + q, W(9 + 5 * q), W(20)));
    });
    return sceneOf(seed, c, gl, { date: 8 * (300 + k), lvl: 1 }, { rngN: { 6: [k & 1 ? 0 : 5, 5, 0] } });
  });

  // 1: the story advance of a pair (0x00428b58..0x00428cfe): one condition fails per variant
  EDGE.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const i0 = R() % 24;
    const date = 64 * (200 + k) + ((31 * i0) % 64);
    const objs = [];
    [i0, i0 + 64, i0 + 128].forEach(function (g, q) {
      const v = (k * 3 + q) % 19;
      const h = v === 2 || v === 3 ? 0x13 : 2 + q;
      const tx = 6 + 12 * q, ty = 10 + (k % 20);
      const a = walker(g, h, W(tx), W(ty), { fl: 0x100000 | (v === 4 ? 0x200000 : 0), d5: [9, 9, 9, 9, 9, 6, 0x10, 9, 9][v] || 9,
        w56a: v === 2 ? 0 : (v === 3 ? 2 : 0), c0: [0x100, 0x32, 0x2ee, 0x100, 0x100, 0x33][Math.max(0, v - 12)] || 0x100,
        c4: v === 15 ? 0x32 : (v === 16 ? 0x1c2 : (v === 17 ? 0x1c1 : 0x100)) });
      if (v === 17) a.c0 = 0x33;
      const b = walker(g ^ 1, h, W(tx) + (v === 11 ? 0x900 : 0x100), W(ty), { d5: v === 7 ? 6 : (v === 8 ? 0x10 : 0xa) });
      gl.push(a, b);
      if (v !== 1) objs.push({ x: tx, y: ty, type: 4, val: [3, 7, 11][q], f7: 0 });
    });
    c.objs = objs;
    const v0 = (k * 3) % 19;
    return sceneOf(seed, c, gl, { date: date + (k % 9 === 8 ? 1 : 0), b048: date - (v0 === 9 ? 0x96 : 0x97), b498: v0 === 10 ? 1 : 0,
                                  zoom: v0 === 12 ? 3 : 4 }, { story: k });
  });

  // 2: the thought about the tile next to the golfer (0x00428d00..0x00428e70)
  EDGE.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const g = R() % 152, tx = 10 + R() % 30, ty = 10 + R() % 30;
    const v = k % 24;
    const a = walker(g, 3, W(tx), W(ty), { w540: [0, 0x8000, 0x4000, 0xc000][v & 3], cc: (v & 4) ? 3 : 0,
      b528: v === 20 ? 0x8b : 0, b529: v === 21 ? 0x0b : 0, d2: R() & 7 });
    gl.push(a, walker(g ^ 1, 3, W(tx + 5), W(ty)));
    c.patch = []; c.objs = [];
    const tt = v === 22 ? 2 : 0x16, tf = [0x1000, 0x1800, 0x800, 0, 0x1000, 0x1800][(v >> 2) % 6];
    for (let d = 0; d < 8; d++) {
      const ox = tx + DX[d], oy = ty + DY[d];
      c.patch.push([ox, oy, tt, tf]);
      if (v !== 23) c.objs.push({ x: ox, y: oy, type: [2, 4, 4, 5, 4, 2][(v + d) % 6], val: [3, 3, 0x10, 0, 0xf, 0][(v + d) % 6], f7: 0 });
    }
    return sceneOf(seed, c, gl, { date: dateHit(g, 33, 0x200, k) });
  });

  // 3: the water-side and course-type counters every 0xa0 / 0x78 ticks (0x00428ee5..0x004290cc)
  EDGE.push(function (seed, k) {
    const R = rng(seed); const v = k % 16;
    const c = blank(seed, { tv: v & 1 ? 1 : 0 }); const gl = [];
    const g = R() % 152, tx = 10 + R() % 30, ty = 10 + R() % 30;
    const a = walker(g, [3, 2, 5, 4][v & 3], W(tx), W(ty), { cc: (v & 2) ? 7 : 0, db: v === 5 ? 1 : 0, w558: v === 6 ? 2 : 0,
      w55e: [0xf, 0x13, 0x10, 0x17, 0xe, 0x1b][v % 6], w560: [0xf, 0x13, 0x10, 0x17, 0x1b, 0xe][v % 6], d0: (v & 4) ? 0x20 : 0 });
    gl.push(a, walker(g ^ 1, a.d9, W(tx + 6), W(ty)));
    c.patch = [[tx, ty, [7, 3, 2, 2][(v >> 2) & 3], null]];
    if (((v >> 2) & 3) === 2) c.patch.push([tx + DX[v & 7], ty + DY[v & 7], 5, null]);
    const div = (v & 2) ? 0x78 : 0xa0;
    return sceneOf(seed, c, gl, { date: dateHit(g, 37, div, k), ctype: (v & 8) ? 3 : 0, f0: 40 },
                   { rngN: { 2: [[1, 0], [0, 0], [1, 1], [0, 1]][(k >> 1) & 3], 20: [0, 3] } });
  });

  // 4: a leaving golfer that still has a ball and a swing state (0x00429386..0x004293ac) and the leaving walk
  EDGE.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    distinctGs(R, 4, 0, 150).forEach(function (g, q) {
      const v = (q + k) % 4;
      const a = walker(g, [0x13, 4, 0x13, 4][v], W(10 + 6 * q), W(15), { fl: 0x20000000 | (v === 3 ? 0x400 : 0), bx: W(11 + 6 * q), by: W(15),
        d8: [1, 1, 2, 0][v], w55c: -3 });
      gl.push(a, walker(g ^ 1, a.d9, W(10 + 6 * q), W(16), { bx: W(11 + 6 * q) + 0x100, by: W(15) }));
    });
    return sceneOf(seed, c, gl, { date: 100 * (k + 70) + 7, chx: 30, chy: 30 });
  });

  // 5: leaving messages for every reason byte (0x00429419..0x00429844)
  const REASONS = [4, 8, 9, 0xc, 0xd, 0xe, 0xf, 0x15, 0x17, 0x1a, 0x1e, 0x84, 0, 3, 0x8d, 0x9a];
  EDGE.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const gs = distinctGs(R, 8, 0, 150);
    gs.forEach(function (g, q) {
      const n = q + 8 * k;
      gl.push(walker(g, (n % 3 === 2) ? 0x13 : 1 + (n % 18), W(5 + 5 * (q % 8)), W(10 + 4 * (n % 7)), {
        w55c: -1 - (n % 12), b53d: REASONS[n % REASONS.length], type: n % 8, fl: (n % 5 === 4) ? 0x200 : 0, d0: [0, 0x40, 0x60, 0x80][n & 3] }));
      gl.push(walker(g ^ 1, 2, W(5 + 5 * (q % 8)), W(30)));
    });
    return sceneOf(seed, c, gl, { date: 100 * (k + 50) - ((35 * gs[k % 8]) % 100) });
  });

  // 6: the clubhouse reset (0x0042990d..0x004299a8)
  EDGE.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const gs = distinctGs(R, 4, 0, 150);
    let sel = -1;
    gs.forEach(function (g, q) {
      const v = (q + 4 * k) % 16;
      const at = v & 1, fl = (v & 2) ? 0x200 : 0;
      const a = walker(g, 0x13, at ? W(25) + 0x80 : W(5 + 4 * q), at ? W(25) : W(40), { fl: fl, c0: (v === 4) ? -1 : 300 });
      const b = walker(g ^ 1, [0, 0x13, 5, 0][(v >> 2) & 3], W(6 + 4 * q), W(40), { fl: (v & 8) ? 0x200 : 0 });
      gl.push(a, b);
      if (v === 3 || v === 7) sel = g;
    });
    return sceneOf(seed, c, gl, { chx: 25, chy: 25, sel: sel });
  });

  // 7: the partner's ball and flight (0x00429b1b..0x00429ec2, 0x00429f31..0x0042a058)
  EDGE.push(function (seed, k) {
    const R = rng(seed); const v = k % 12;
    const c = blank(seed); const gl = [];
    const h = 2 + (k % 10); const hole = c.holes[h];
    const g = R() % 152;
    const cupx = W(hole.x), cupy = W(hole.y);
    let a, b;
    if (v < 4) {           // own ball closer to the cup, the partner standing next to it
      a = walker(g, h, cupx + 0x2400, cupy, { bx: cupx + 0x1000 + (v & 1) * 0x200, by: cupy });
      b = walker(g ^ 1, h, a.bx + [0x100, 0x1ff, 0x200, 0x900][v], a.by, { bx: cupx + 0x2000, by: cupy + 0x400 });
      c.patch = [[a.bx >> 10, a.by >> 10, (v & 2) ? 1 : 2, null]];
    } else if (v < 10) {   // the partner's ball in the air
      a = walker(g, h, W(10), W(10), { bx: W(20), by: W(20) });
      b = walker(g ^ 1, h, W(10) + 0x200, W(10), { bx: W(11), by: W(10), bz: 7, d5: [0xd, 0xe, 0xa, 0xc, 0x10, 0xb][v - 4] });
      b.dir = angle(b.bx - a.x, b.by - a.y) + ((k & 1) ? 0 : 0x50000000);
      c.patch = [[20, 20, 2, null]];
    } else {               // own ball on the green, the partner's elsewhere
      a = walker(g, h, cupx + 0x1800, cupy, { bx: cupx + 0x800, by: cupy, fl: 0 });
      b = walker(g ^ 1, h, cupx + 0x1800, cupy + 0x100, { bx: cupx + 0x900 + (v & 1) * 0x3000, by: cupy + 0x100 });
      c.patch = [[a.bx >> 10, a.by >> 10, 1, null], [(b.bx >> 10), b.by >> 10, (k & 2) ? 1 : 2, null]];
    }
    gl.push(a, b);
    return sceneOf(seed, c, gl, {}, { ps: [1, 2] });
  });

  // 8: the amenity searches with their 0x10 / 8 / 4 / 0x20 / 0x40 / 0x100 flags (0x0042a548..0x0042a70c)
  EDGE.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const FL = [0x10, 8, 4, 0x20, 0x40, 0x100, 0x18, 0x1c, 0, 0x60, 0x120, 0x140];
    c.objs = [];
    distinctGs(R, 4, 0, 150).forEach(function (g, q) {
      const v = (q + 4 * k) % 12;
      const tx = 6 + 10 * q, ty = 10 + (k % 25);
      gl.push(walker(g, 5, W(tx), W(ty), { d1: [7, 4, 2, 1, 6, 5][(v + k) % 6], fl: FL[v], w576: 10 }));
      gl.push(walker(g ^ 1, 5, W(tx), W(ty + 15), { w576: 10 }));
      const dd = [3, 5, 6, 8, 10][(v + q) % 5];
      c.objs.push({ x: tx + dd, y: ty, type: 6, f7: 0x40 }, { x: tx, y: ty + dd, type: 8, f7: 0x40 }, { x: tx - dd, y: ty, type: 10, f7: 0x40 });
    });
    return sceneOf(seed, c, gl, { cb8: 1, cc0: 1, cc8: 1 });
  });

  // 9: arriving at a placed object (0x0042aa0c..0x0042ad54)
  EDGE.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const v = k % 10;
    const g = R() % 152, m = g & 1;
    const ox = 10 + R() % 25, oy = 10 + R() % 25;
    const type = [7, 3, 6, 8, 10, 7, 6, 8, 10, 3][v];
    let tx, ty;
    const o = { d1: { 6: 4, 8: 2, 10: 1 }[type] || 0, w55e: type === 7 ? 10 : 0, w560: 0, w576: 10 };
    if (type === 7 || type === 3) { tx = (ox << 10) + 0x200; ty = (oy << 10) + 0x200; }
    if (type === 6) { tx = (ox + m + 1) << 10; ty = (oy + m + 1) << 10; }
    if (type === 8) { tx = ((ox + m) << 10) + 0x200; ty = ((oy + m) << 10) + 0x200; }
    if (type === 10) { tx = ((ox + m) << 10) + 0xa00; ty = ((oy + m) << 10) + 0xa00; }
    const a = walker(g, 6, tx + (R() % 0x40) - 0x20, ty + (R() % 0x40) - 0x20, o);
    const b = walker(g ^ 1, 6, W(45), W(45), { w576: type === 3 ? 5 : 10 });
    gl.push(a, b);
    c.objs = [{ x: ox, y: oy, type: type, f7: 0x40 }];
    c.patch = [[ox, oy, 2, null]];
    return sceneOf(seed, c, gl, { c50: v < 5 ? 1 : 2, c58: v < 5 ? 1 : 3, c60: v < 5 ? 0 : 2, d3c: [0, 57, 99, 100, 1234, -7][k % 6] });
  });

  // 10: arriving with no object: the tee box, the partner, the queue (0x0042ad59..0x0042afb3)
  EDGE.push(function (seed, k) {
    const R = rng(seed); const v = k % 14;
    const c = blank(seed); const gl = [];
    const h = 3 + (k % 12), hole = c.holes[h];
    const odd = [1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1][v];
    const g = 2 * (R() % 70) + 2 + odd, p = g ^ 1;
    const a = walker(g, h, 0, 0, { w576: 10, d1: 5 });
    const b = walker(p, h, W(45), W(45), { w576: 10 });
    if (v === 0) { b.d9 = h - 1; b.bx = W(40); b.by = W(40); }                       // partner on an earlier hole with a ball
    if (v === 1) { b.d9 = h - 1; b.bx = W(40); b.by = W(40); a.fl = 0x400; }
    if (v === 2 || v === 3) { b.da = (v === 3) ? 1 : 0; }                             // same hole, i > p / i < p
    if (v === 4) { a.fl = 0x400; }
    if (v === 5) { a.fl = 0x4000; }
    if (v === 6) { b.da = 1; a.bx = W(hole.x) + 0x1400; a.by = W(hole.y); b.bx = W(hole.x) + 0x2000; b.by = W(hole.y); }
    if (v === 7) { b.da = 1; a.bx = W(hole.x) + 0x1400; a.by = W(hole.y); b.bx = W(hole.x) + 0x200; b.by = W(hole.y) - 0x300; }
    if (v === 8) { b.d9 = 0; }
    if (v === 9) { b.d9 = 0; a.fl = 0x400; }
    if (v === 10) { b.d9 = h + 1; a.bx = W(hole.x) + 0x1400; a.by = W(hole.y); }
    if (v === 11) { b.d9 = 0; a.b53c = 0; }
    if (v === 12) { b.d9 = h; b.fl = 0x400; }
    if (v === 13) { b.d9 = 0x13; }
    let t;
    if (a.bx) t = ballTarget(hole, a, 2);
    else t = teeTarget(hole, a);
    // the queue shift of 0x0042a715 when the partner is on this hole or an earlier one, i < p and the partner has no 0x400
    const pb = b.d9, mb = a.d9;
    if (pb !== 0 && pb <= mb && g < p && (!(b.fl & 0x400) || pb < mb || b.d5 === 0xc)) {
      const dd = (hdir(c, h) - 1) & 7;
      t = [t[0] + Math.trunc((-DX[dd] * 1024) / 2), t[1] + Math.trunc((-DY[dd] * 1024) / 2)];
    }
    a.x = t[0] + (R() % 0x40) - 0x20; a.y = t[1] + (R() % 0x40) - 0x20;
    c.patch = [];
    if (a.bx) c.patch.push([a.bx >> 10, a.by >> 10, 2, null]);
    gl.push(a, b);
    return sceneOf(seed, c, gl, {}, { rngN: { 4: [2, 0] } });
  });

  // 11: a new step on water, on tile type 7 and the 0x40000000 walk flag, the slope thought (0x0042b2da..0x0042b5ca)
  EDGE.push(function (seed, k) {
    const R = rng(seed); const v = k % 12;
    const c = blank(seed, { tv: 2, hills: (v >= 8) ? 9 : 0, flat: v < 8 }); const gl = [];
    const g = R() % 152, tx = 10 + R() % 25, ty = 10 + R() % 25;
    const a = walker(g, 4, W(tx), W(ty), { b53d: [7, 3, 0xa, 0x2b, 0xa, 3][v % 6], fl: (v & 1) ? 0x40000000 : 0, d2: v & 1 ? 0 : 2, da: v % 3 === 2 ? 2 : 0 });
    gl.push(a, walker(g ^ 1, 4, W(45), W(3)));
    c.patch = [[tx, ty, v < 4 ? 0x11 : (v < 8 ? 7 : 2), v < 4 ? [0x120, 0x127, 0x20, 0x100][v] : 0]];
    c.holes[4].tx = 45; c.holes[4].ty = 45;
    return sceneOf(seed, c, gl, { lvl: v === 7 ? 0 : 1 }, { ps: [[0, 2, 4, 6, 1, 3][k % 6]], rngN: { 5: [k % 8] } });
  });

  // 12: a crowd ahead and the pair that gives up waiting (0x0042b052..0x0042b7fe)
  EDGE.push(function (seed, k) {
    const R = rng(seed); const v = k % 12;
    const c = blank(seed); const gl = [];
    const g = 2 * (5 + R() % 60) + (k & 1), tx = 20, ty = 20;
    const a = walker(g, 7, W(tx), W(ty), { d2: 2, w576: 30, fl: [0, 0x1000, 0x4000, 0x2000, 0, 0x1000, 0, 0][v % 8],
      w55c: (v & 4) ? -1 : 0, d0: (v === 6) ? 0x20 : 0, da: (v === 7) ? 1 : 0 });
    gl.push(a, walker(g ^ 1, 7, W(tx), W(ty + 1), { w576: 30 }));
    const n = (v === 8) ? 3 : 5;
    for (let q = 0; q < n; q++) {
      const og = (g + 10 + 2 * q) % 152;
      gl.push(walker(og, 7, W(tx + 1) - 0x100 + 0x40 * q, W(ty) + 0x20 * q, { w576: [10, 30, 29, 5, 31][q] - (v === 9 ? 25 : 0), da: (v === 10 && q === 0) ? 1 : 0,
        fl: (v === 11 && q === 1) ? 0x20000000 : 0 }));
    }
    c.holes[7].tx = 45; c.holes[7].ty = 45;
    return sceneOf(seed, c, gl, { a24: g, lvl: (v & 2) ? 0 : 1 }, { ps: [2], rngN: { 4: [1] } });
  });

  // 13: the step: tile flag 0x20, the 0x8000 run flag, the tiredness counter every 24 ticks (0x0042b803..0x0042bc6e)
  EDGE.push(function (seed, k) {
    const R = rng(seed); const v = k % 16;
    const c = blank(seed); const gl = [];
    const g = R() % 152, tx = 15 + R() % 15, ty = 15 + R() % 15;
    const a = walker(g, 8, W(tx), W(ty), { d2: v & 7, cc: (v & 8) ? 0x200 : 0, fl: [0x10000, 0x18000, 0x8000, 0, 0x10000, 0x18000][v % 6],
      da: (v % 3 === 1) ? 2 : 0, w562: [0x9e, 0x9f, 0xa0, 0x77, 0xc7, 0x50][v % 6], d0: (v === 5) ? 0x20 : 0, w566: v & 3 });
    gl.push(a, walker(g ^ 1, 8, W(tx), W(ty + 1), { da: (v === 9) ? 2 : 0 }));
    c.patch = [[tx, ty, [2, 4, 1, 0, 5, 6][v % 6], (v & 1) ? 0x20 : 0]];
    const d = v & 7;
    c.patch.push([tx + DX[d], ty + DY[d], null, (v & 2) ? 0x20 : 0], [tx - DX[d], ty - DY[d], null, (v & 4) ? 0x20 : 0]);
    c.holes[8].tx = (v === 12) ? tx : 45; c.holes[8].ty = (v === 12) ? ty : 45;
    return sceneOf(seed, c, gl, { date: dateHit(g, 11, 24, k), gf: (v & 1) ? 0x20000 : 0, lvl: (v & 2) ? 0 : 1 }, { ps: [v & 7] });
  });

  // 14: at the ball: the partner further out, the cup, the player's golfer, the shot (0x0042bc8f..0x0042c0bd)
  EDGE.push(function (seed, k) {
    const R = rng(seed); const v = k % 12;
    const c = blank(seed); const gl = [];
    const h = 1 + (k % 15), hole = c.holes[h];
    const g = R() % 152;
    const cupx = W(hole.x), cupy = W(hole.y);
    const a = walker(g, h, 0, 0, { d8: 1, fl: [0x400, 0, 0x200, 0x200, 0x200, 0][v % 6], bx: cupx + [0x800, 0x80, 0x600, 0x3000, 0x80, 0xff][v % 6], by: cupy,
      x584: (v === 3) ? -1 : 0, c0: (v & 1) ? -1 : 200, d9: h });
    a.x = a.bx - 0x100; a.y = a.by;
    const b = walker(g ^ 1, (v === 1 || v === 4 || v === 5) ? 0 : h, cupx + 0x2000, cupy, { bx: cupx + 0x1800, by: cupy, d5: 9, d0: (v === 2) ? 0x20 : 0 });
    if (v === 2) { a.d9 = 1; b.d9 = 1; a.bx = W(c.holes[1].x) + 0x3000; a.by = W(c.holes[1].y); a.x = a.bx; a.y = a.by; b.bx = 0; }
    gl.push(a, b);
    c.patch = [[a.bx >> 10, a.by >> 10, (v & 1) ? 1 : 2, (v & 2) ? 0 : 0x80]];
    return sceneOf(seed, c, gl, { b738: (v >= 6) ? 1 : 0 }, { shot: [0x10000000 * (v + 1)], shotD5: (v & 1) ? [0xe, 0xb] : [0xb] });
  });

  // 15: the swing's look at the golfers ahead (0x0042c14e..0x0042c35d)
  EDGE.push(function (seed, k) {
    const R = rng(seed); const v = k % 10;
    const c = blank(seed); const gl = [];
    const h = 2 + (k % 14), hole = c.holes[h];
    const g = R() % 100;
    const a = walker(g, h, W(hole.ax + 3), W(hole.ay), { d8: 1, d5: 0xb, sp: 0x100, bx: W(hole.ax + 3) + 0x100, by: W(hole.ay), x584: hole.x, x588: hole.y,
      fl: (v === 9) ? 0x200 : 0, w576: 20, da: 0 });
    gl.push(a, walker(g ^ 1, h, W(45), W(45), { w576: 20 }));
    const pos = [[hole.ax, hole.ay], [hole.ax + 8, hole.ay], [hole.x, hole.y + 1], [hole.x + 1, hole.y], [hole.ax + 6, hole.ay + 1], [hole.ax + 7, hole.ay + 3],
                 [hole.ax + 30 % 50, hole.ay], [hole.ax + 6, hole.ay], [hole.ax + 5, hole.ay - 1], [hole.ax + 9, hole.ay]][v];
    const og = (g + 101) % 152;
    gl.push(walker(og, h, W(inb(pos[0])), W(inb(pos[1])), { fl: 0x400, da: 1, w576: 15, bx: (v === 3) ? W(hole.x) : W(inb(pos[0] + 6)), by: W(inb(pos[1])) }));
    return sceneOf(seed, c, gl, { b04c: 1 });
  });

  // 16: flight gaps: slow upward ball with a pair slot, tree hits without skill flag 0x200, 0xa and 0xc tiles, high bounces
  EDGE.push(function (seed, k) {
    const R = rng(seed); const v = k % 12;
    const c = blank(seed, { tv: 0 }); const gl = [];
    const h = 3 + (k % 12), hole = c.holes[h];
    const g = R() % 150;
    const bx = W(15) + [0, 0x1c0, 0x100, 0x200][v & 3], by = W(15) + [0x200, 0x1c0, 0x100, 0][(v >> 2) & 3];
    const a = walker(g, h, W(12), W(15), { bx: bx, by: by, x57c: W(10), x580: W(15), sp: [0x80, 0x200, 0x101, 0x140, 0x60][v % 5],
      bz: [0, 0, 5, 0][v & 3], vz: [0x30, -0x400, -0x900, -0x30, 0x10][v % 5], fl: 0x40000, w556: -1, d0: 0x21, ce: (v & 1) ? 0x200 : 0,
      b5b1: 0x63, dir: (R() & 0xffffffff) | 0 });
    gl.push(a, walker(g ^ 1, h, W(13), W(15)));
    c.pairs = [{ slot: 2, id: g }];
    const tt = [0xa, 0xc, 0xd, 0xe, 0xf, 0x10, 0x15, 4, 0xa, 0xc, 1, 2][v];
    c.patch = [[15, 15, tt, (v === 6) ? 0x3 : 0], [14, 15, 0xa, null], [15, 14, 0xa, null], [16, 15, 0xa, null], [15, 16, 0xa, null]];
    // a golfer on another hole close by (ducks from a high bounce)
    gl.push(walker((g + 2) % 152, h + 1, W(15), W(16), { bx: W(30), by: W(30) }));
    return sceneOf(seed, c, gl, { lvl: 1 }, { rngN: { 0x180: [0], 0x64: [0], 3: [1], 8: [0], 2: [k & 1] } });
  });

  // 17: stop gaps: a stop with vertical speed, statistics maxima, blocked fairway, water at the shot origin, the thought byte
  EDGE.push(function (seed, k) {
    const R = rng(seed); const v = k % 12;
    const c = blank(seed, { tv: 0 }); const gl = [];
    const h = 2 + (k % 12);
    const g = R() % 150;
    const bx = W(20) + 0x40, by = W(20);
    const a = walker(g, h, W(18), W(20), { bx: bx, by: by, x57c: (v === 4) ? bx : W(15), x580: (v === 4) ? by : W(20), sp: 0x20,
      vz: (v === 1) ? -0x300 : 0, fl: 0x40000 | ((v & 2) ? 4 : 0), w556: -1, da: (v & 1) ? 0 : 1, d3: 9, b528: 3, d9: (v < 6) ? 2 : h });
    gl.push(a, walker(g ^ 1, h, W(19), W(21)));
    const tt = [2, 2, 2, 0x14, 0x11, 0x11, 1, 2, 0x14, 2, 0x11, 2][v];
    c.patch = [[20, 20, tt, (v === 9) ? 0x100 : 0]];
    if (v === 10) for (let q = 15; q < 20; q++) c.patch.push([q, 20, 2, null]);
    if (v === 5) for (let q = 15; q < 20; q++) c.patch.push([q, 20, 0x11, null]);
    const sc = sceneOf(seed, c, gl, { lvl: 1 }, { emit528: (v & 4) ? 1 : 0 });
    if (v === 3 || v === 8) sc.course.tv = 3;     // blocked tile type with attr -1 (typeTable variant 3 below)
    return sc;
  });

  FAM.edge = function (seed, v) { return EDGE[v % EDGE.length](seed, Math.floor(v / EDGE.length)); };

  // ====================================================================================================================
  // edge scenes, second round (gaps left by the first edge key). A partner that must keep its fields while the scene's
  // golfer runs is "frozen": w556 < 0 makes its own iteration stop at 0x00429130.
  // ====================================================================================================================
  const FROZEN = -50;
  function sgn8(b) { b &= 0xff; return b > 0x7f ? b - 256 : b; }
  // the target updateGolfers walks to with no ball and no object (0x0042a0aa..0x0042a812): the tee, the L38 step and the queue
  // shifts of 0x0042a715 / 0x0042a7a6 (L38 = the number of golfers the patron loop counted)
  function noObjTarget(c, a, b, L38) {
    const hole = c.holes[a.d9];
    let t = teeTarget(hole, a);
    if (L38) { const dd = (hole.dir + (a.g & 1)) & 7; t = [t[0] - DX[dd] * 1024, t[1] - DY[dd] * 1024]; }
    const pb = sgn8(b.d9), mb = sgn8(a.d9);
    if (pb !== 0 && pb <= mb && a.g < b.g && (!(b.fl & 0x400) || pb < mb || b.d5 === 0xc)) {
      const dd = (hole.dir - 1) & 7;
      t = [t[0] + Math.trunc((-DX[dd] * 1024) / 2), t[1] + Math.trunc((-DY[dd] * 1024) / 2)];
    }
    const L34 = pb !== 0 && pb === mb && (a.fl & 0x400) && !(b.fl & 0x400);
    if (pb !== 0 && L34) {
      const dd = hole.dir & 7;
      t = [t[0] + Math.trunc((-DX[dd] * 1024) / 2), t[1] + Math.trunc((-DY[dd] * 1024) / 2)];
    }
    return t;
  }
  const EDGE2 = [];

  // 0: the story advance with the partner kept still (p.d5 < 7, p.d5 == 0x10), the 0x00569498 flag
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const i0 = R() % 24; const date = 64 * (300 + k) + ((31 * i0) % 64);
    c.objs = [];
    [i0, i0 + 64, i0 + 128].forEach(function (g, q) {
      const v = (k + q) % 4;
      const tx = 6 + 12 * q, ty = 12;
      gl.push(walker(g, 3 + q, W(tx), W(ty), { fl: 0x100000, d5: 9, w56a: 0, c0: 0x100, c4: 0x100 }));
      gl.push(walker(g ^ 1, 3 + q, W(tx) + 0x100, W(ty), { d5: [6, 0x10, 9, 0xa][v], w556: FROZEN }));
      c.objs.push({ x: tx, y: ty, type: 4, val: 3, f7: 0 });
    });
    return sceneOf(seed, c, gl, { date: date, b048: date - 0x200, b498: (k & 4) ? 1 : 0 });
  });

  // 1: the tile thought with type-4 objects of value >= 0x10, water-class 7 neighbours (type 3 in table variant 1)
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed, { tv: 1 }); const gl = [];
    const g = R() % 152, tx = 10 + R() % 30, ty = 10 + R() % 30;
    gl.push(walker(g, 4, W(tx), W(ty), { d2: R() & 7 }), walker(g ^ 1, 4, W(tx + 6), W(ty)));
    c.patch = []; c.objs = [];
    for (let d = 0; d < 8; d++) {
      const ox = tx + DX[d], oy = ty + DY[d];
      if (k & 1) { c.patch.push([ox, oy, 0x16, 0]); c.objs.push({ x: ox, y: oy, type: 4, val: 0x10 + k, f7: 0 }); }
      else c.patch.push([ox, oy, 3, 0]);
    }
    const date = (k & 1) ? dateHit(g, 33, 0x200, k) : dateHit(g, 37, 0xa0, k);
    return sceneOf(seed, c, gl, { date: date, ctype: 0 }, { rngN: { 2: [0, 1] } });
  });

  // 2: the 0x579560 counter without water (0x00429087..0x004290cc)
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const g = R() % 152, tx = 10 + R() % 30, ty = 10 + R() % 30;
    gl.push(walker(g, 5, W(tx), W(ty), { w560: [0x10, 0x11, 0x12, 0x13, 0x1f, 0x0f][k % 6], d0: (k & 1) ? 0x20 : 0, cc: 0 }), walker(g ^ 1, 5, W(tx + 6), W(ty)));
    return sceneOf(seed, c, gl, { date: dateHit(g, 37, 0xa0, k), ctype: 0 });
  });

  // 3: a target tile (findNearestTargetTile, 0x0042a2f1) with a successful canStep walk-off (0x0042a96b..0x0042a9f1)
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const g = R() % 152, tx = 10 + R() % 30, ty = 10 + R() % 30;
    gl.push(walker(g, 6, W(tx) + (R() % 0x30), W(ty), { w562: [120, 121, 140, 200, 3, 2][k % 6], fl: (k % 6 >= 4) ? 0x1000 : 0, w55e: 0, w560: 0 }),
            walker(g ^ 1, 6, W(45), W(45), { w576: 10 }));
    c.patch = [[tx - 2, ty - 2, 4, 0, tx + 2, ty + 2], [tx, ty, 4, 0x200]];
    if (k & 1) c.patch.push([tx - 1, ty - 1, null, 0x100, tx + 1, ty - 1], [tx, ty, null, 0x200]);
    if (k & 2) c.patch.push([tx + 1, ty, 0x14, null]);
    return sceneOf(seed, c, gl, {}, { rngN: { 4: [k & 3, 1, 2, 3, 0] } });
  });

  // 4: arriving with no object: partner further on, self-partner, the cup comparison, L34 / L38 / L40 (0x0042ad59..0x0042afb3)
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const v = k % 9;
    const c = blank(seed); const gl = [];
    const h = 3 + (k % 12), hole = c.holes[h];
    const g = 2 * (10 + R() % 60) + ((k >> 3) & 1) + (v === 6 ? 1 : 0) - (v === 6 && (k & 8) ? 1 : 0);
    const p = g ^ 1;
    const a = walker(g, h, 0, 0, { w576: 10, d1: 5 });
    const b = walker(p, h, W(45), W(45), { w576: 10, w556: FROZEN });
    let L38 = 0;
    if (v === 0) { b.d9 = h + 1; b.bx = W(40); b.by = W(40); }                     // partner on a later hole with a ball
    if (v === 1) { a.p = g; b.da = 1; }                                              // the golfer is its own partner (b is not counted: da 1)
    if (v === 2) { b.da = 1; a.bx = W(hole.x) + 0x1400; a.by = W(hole.y); b.bx = W(hole.x); b.by = W(hole.y) + 0x4000; }
    if (v === 3) { a.fl = 0x400; b.da = 1; }                                         // L34 (partner without 0x400, partner not done: no face-to-partner exit)
    if (v === 4 || v === 5) {                                                        // L38: another golfer on the hole the patron loop counts
      const og = (g + 40) % 152 ^ ((g + 40) % 152 === p ? 2 : 0);
      gl.push(walker(og, h, W(44), W(3), { w576: 5, w556: FROZEN }));
      L38 = 1;
      if (v === 5) { a.fl = 0x400; b.da = 1; }
    }
    if (v === 6) { b.d9 = h; b.fl = 0x400; a.fl = 0x400; }
    if (v === 7) { a.bx = W(hole.x) + 0x1400; a.by = W(hole.y); b.d9 = 0; }         // with a ball, nothing else
    if (v === 8) { b.d9 = h; b.d5 = 0xc; }
    let t = a.bx ? ballTarget(hole, a, 2) : noObjTarget(c, a, v === 1 ? a : b, L38);
    a.x = t[0] + (R() % 0x30) - 0x18; a.y = t[1] + (R() % 0x30) - 0x18;
    c.patch = a.bx ? [[a.bx >> 10, a.by >> 10, 2, null]] : [];
    gl.push(a, b);
    return sceneOf(seed, c, gl, { a51c: 1000 }, { rngN: { 4: [3] } });
  });

  // 5: the walk flag 0x40000000 with reason byte 0xa, the steep slope thought, the game flag 0x80000 from pathStep
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const v = k % 6;
    const c = blank(seed, { steep: v >= 3, flat: v < 3 }); const gl = [];
    const g = R() % 152, tx = 10 + R() % 25, ty = 10 + R() % 25;
    gl.push(walker(g, 4, W(tx) + 0x100, W(ty) + 0x80, { b53d: [0xa, 0xa, 3, 0, 0, 0x2b][v], fl: [0x40000000, 0x40000000, 0x40000000, 0, 0x40000000, 0][v], d2: 0 }),
            walker(g ^ 1, 4, W(45), W(3)));
    c.patch = [[tx, ty, v < 3 ? 5 : 2, 0]];
    c.holes[4].tx = 45; c.holes[4].ty = 45;
    return sceneOf(seed, c, gl, { lvl: 1 }, { ps: [v >= 3 ? 4 : [0, 2, 4, 6][k & 3]], ps80: (k & 1) ? 1 : 0 });
  });

  // 6: a crowd right ahead and the pair that gives up (0x0042b052..0x0042b7fe)
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const v = k % 10;
    const c = blank(seed); const gl = [];
    const g = 2 * (5 + R() % 60) + (k & 1), tx = 20, ty = 20;
    gl.push(walker(g, 7, W(tx), W(ty), { d2: 2, w576: 30, fl: [0, 0x1000, 0x4000, 0, 0, 0, 0x1000, 0, 0, 0x1000][v],
      w55c: [0, 0, 0, 0, -1, -1, -1, -2, -1, -3][v], d0: (v === 4) ? 0x20 : 0, da: 0, b53c: (v >= 8) ? 2 : 0 }),
      walker(g ^ 1, 7, W(tx), W(ty + 1), { w576: 30, w556: FROZEN }));
    for (let q = 0; q < 5; q++) {
      let og = (g + 10 + 2 * q) % 152;
      gl.push(walker(og, 7, W(tx) + 0x100 + 0x20 * q, W(ty), { w576: [10, 30, 29, 5, 31][q], w556: FROZEN }));
    }
    c.holes[7].tx = 45; c.holes[7].ty = 45;
    return sceneOf(seed, c, gl, { a24: g, lvl: (v === 3 || v === 6) ? 0 : 1 }, { ps: [2], rngN: { 4: [1] } });
  });

  // 7: the step: tile flag 0x20 around the golfer with the 0x10000 flag (0x0042b892..0x0042b944), the tiredness word crossing 0xa0
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const v = k % 8;
    const c = blank(seed); const gl = [];
    const g = R() % 152, tx = 15 + R() % 15, ty = 15 + R() % 15;
    gl.push(walker(g, 8, W(tx), W(ty), { d2: 2, cc: 0x200, fl: [0x10000, 0x18000, 0x10000, 0x18000, 0, 0, 0x10000, 0x8000][v],
      da: (v === 2) ? 1 : 0, w562: 0x9f, d0: (v >= 4 && v < 6) ? 0x20 : 0 }),
      walker(g ^ 1, 8, W(tx), W(ty + 2), { w556: FROZEN }));
    c.patch = [[tx, ty, [2, 1, 4, 2, 2, 4, 0, 2][v], (v & 1) ? 0x20 : 0], [tx + 1, ty, null, 0x20], [tx - 1, ty, null, 0x20]];
    c.holes[8].tx = 45; c.holes[8].ty = 45;
    return sceneOf(seed, c, gl, { date: dateHit(g, 11, 24, k), lvl: 1 }, { ps: [2] });
  });

  // 8: at the ball: the partner's ball further out (L74), the player's golfer next to the cup (0x0042bc8f..0x0042bfdd)
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const v = k % 4;
    const c = blank(seed); const gl = [];
    const h = 2 + (k % 14), hole = c.holes[h];
    const g = R() % 152;
    const cx = W(hole.x), cy = W(hole.y);
    if (v < 2) {
      gl.push(walker(g, h, cx + 0x700, cy, { d8: 1, fl: 0x400, bx: cx + 0x800, by: cy }),
              walker(g ^ 1, h, cx + 0x1800, cy, { fl: 0x400, bx: cx + (v ? 0x400 : 0x1800), by: cy, w556: FROZEN }));
    } else {
      gl.push(walker(g, h, cx + 0x180, cy, { d8: 1, fl: 0x200, bx: cx + [0x180, 0x400][v - 2], by: cy, x584: 0 }),
              walker(g ^ 1, 0, W(45), W(45)));
    }
    c.patch = [[hole.x, hole.y, 2, 0], [hole.x + 1, hole.y, 2, 0]];
    return sceneOf(seed, c, gl, { b738: k & 4 ? 1 : 0 }, { shot: [0x20000000] });
  });

  // 9: the swing: d8 2 with d4 0xd, the club meeting the ball with a pair slot, a green lie and the player (0x0042c3d8..0x0042c561)
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const v = k % 6;
    const c = blank(seed); const gl = [];
    const h = 2 + (k % 14), hole = c.holes[h];
    const g = R() % 152;
    const a = walker(g, h, W(20), W(20), { bx: W(20) + 0x100, by: W(20), sp: 0x100, d8: [2, 2, 7, 7, 7, 7][v], d4: [0xd, 3, 0, 0, 0, 0xa][v],
      c0: [300, 300, 300, -1, 300, 300][v], fl: (v === 4) ? 0x40000 : 0, d1: [0, 0, 0x10, 0xf1, 0x13, 0x20][v] });
    gl.push(a, walker(g ^ 1, h, W(30), W(30), { w556: FROZEN }));
    c.patch = [[20, 20, (v === 3 || v === 5) ? 1 : 2, 0]];
    return sceneOf(seed, c, gl, { sel: (v >= 3) ? g : -1 });
  });

  // 10: the swing's look ahead: a golfer whose ball lies at the target, and one on the line to it (0x0042c224..0x0042c346)
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const v = k % 4;
    const c = blank(seed); const gl = [];
    const h = 3 + (k % 12), hole = c.holes[h];
    hole.ax = 10; hole.ay = 25; hole.x = 40; hole.y = 25;
    const g = R() % 100;
    gl.push(walker(g, h, W(11), W(25), { d8: 1, d5: 0xb, sp: 0x100, bx: W(11) + 0x100, by: W(25), x584: 40, x588: 25, w576: 20 }),
            walker(g ^ 1, h, W(45), W(45), { w576: 20, w556: FROZEN }));
    const og = (g + 101) % 152;
    if (v === 0) gl.push(walker(og, h, W(25), W(40), { fl: 0x400, da: 1, w576: 15, bx: W(40), by: W(25), w556: FROZEN }));
    if (v === 1) gl.push(walker(og, h, W(18), W(25) + 0x100, { fl: 0x400, da: 1, w576: 15, bx: W(30), by: W(40), w556: FROZEN }));
    if (v === 2) gl.push(walker(og, h, W(18), W(40), { fl: 0x400, da: 1, w576: 15, bx: W(30), by: W(45), w556: FROZEN }));
    if (v === 3) { gl.push(walker(og, h, W(25), W(40), { fl: 0x400, da: 1, w576: 15, bx: W(40), by: W(25), w556: FROZEN }));
                   gl.push(walker((og + 2) % 152, h, W(18), W(25) + 0x100, { fl: 0x400, da: 2, w576: 15, bx: W(30), by: W(40), w556: FROZEN })); }
    return sceneOf(seed, c, gl, { b04c: 1 });
  });

  // 11: rolling on type 0xa and 0xc tiles, into the cup, the high bounce others duck from (0x0042ca04..0x0042d35f)
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const v = k % 16;
    const c = blank(seed); const gl = [];
    const h = 3 + (k % 12);
    const g = R() % 150;
    const tx = 15, ty = 15;
    const FXY = [[8, 8], [8, 3], [8, 12], [3, 8], [12, 8], [3, 3], [8, 3], [8, 12], [3, 8], [12, 8]];
    let o = { fl: 0x40000, w556: -1, sp: 8, bz: 0, vz: 0, d0: 0x21, ce: 0x200, b5b1: 0, x57c: W(10), x580: W(15) };
    c.patch = [];
    if (v < 10) {                     // 0xa tile at the given fraction, neighbours 0xa (v < 5) or 2
      const f = FXY[v];
      o.bx = tx * 1024 + f[0] * 64 + 32; o.by = ty * 1024 + f[1] * 64 + 32;
      c.patch.push([tx, ty, 0xa, 0]);
      const nt = v < 5 ? 0xa : 2;
      c.patch.push([tx - 1, ty, nt, null], [tx + 1, ty, nt, null], [tx, ty - 1, nt, null], [tx, ty + 1, nt, null]);
    } else if (v < 13) {              // landing on 0xc with speed: the kick (0x0042d12a)
      o.bx = tx * 1024 + 8 * 64; o.by = ty * 1024 + 8 * 64; o.sp = 0x200; o.vz = -0x20; o.ce = (v === 11) ? 0 : 0x200; o.d0 = (v === 12) ? 0 : 0x21;
      c.patch.push([tx, ty, 0xc, 0]);
    } else {                          // the cup (0x0042cc41..0x0042cd64) and the duck loop
      o.bx = tx * 1024 + 0x200 + (v === 15 ? 0x40 : 8); o.by = ty * 1024 + 0x200; o.d4 = 0xd;
      c.patch.push([tx, ty, v === 13 ? 2 : 1, v === 13 ? 0 : 0x80]);
      if (v === 13) { o.sp = 0x30; o.vz = -0x1200; o.bx = tx * 1024 + 0x200; o.d4 = 0; o.bz = 0; }
    }
    const a = walker(g, h, W(12), W(15), o);
    gl.push(a, walker(g ^ 1, h, W(13), W(15), { w556: FROZEN }));
    if (v >= 13) {
      gl.push(walker((g + 4) % 152, 0x13, W(15), W(16), { bx: W(30), by: W(30), w556: FROZEN }));
      gl.push(walker((g + 6) % 152, h + 1, W(40), W(40), { bx: W(41), by: W(40), w556: FROZEN }));
      gl.push(walker((g + 8) % 152, h + 1, W(15), W(15) + 0x100, { bx: W(41), by: W(41), w556: FROZEN }));
      c.pairs = (k & 1) ? [{ slot: 4, id: g }] : [];
    }
    return sceneOf(seed, c, gl, { lvl: 1 }, { rngN: { 0x50: [3], 0xa0: [7], 3: [1], 0x64: [5] } });
  });

  // 12: stops: a bounce left in the vertical speed, a water drop at the shot origin, the drop score (0x0042d361..0x0042d9ab)
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const v = k % 6;
    const c = blank(seed, { tv: (v >= 4) ? 4 : 0 }); const gl = [];
    const h = 2 + (k % 12);
    const g = R() % 150;
    const bx = W(20), by = W(20);
    const o = { bx: bx, by: by, x57c: W(15), x580: W(20), sp: 0x20, fl: 0x40000, w556: -1, da: 1, d3: 9 };
    if (v === 0 || v === 1) { o.vz = -0x300; o.bz = 0; }
    if (v === 2 || v === 3) { o.sp = 8; o.x57c = bx; o.x580 = by; }
    if (v >= 4) { o.sp = 8; }
    gl.push(walker(g, h, W(18), W(20), o), walker(g ^ 1, h, W(19), W(21), { w556: FROZEN }));
    c.patch = [[20, 20, [3, 4, 0x11, 0x11, 0x11, 0x11][v], 0]];
    if (v >= 4) c.patch.push([14, 20, 0x12, 0, 16, 20], [17, 20, 2, 0, 19, 20]);
    return sceneOf(seed, c, gl, { lvl: 1 });
  });

  // 13: hole byte 0xfe (signed -2): the crowd loop meets the golfer's own record with d9 <= 0 and leaves at 0x0042b26c. Hole
  // "-2" is the 0x208 bytes at 0x005756a0 (below the table): the scene writes a par byte, tees and a cup there (saved and
  // restored like the other tables)
  EDGE2.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const g = 2 * (5 + R() % 60) + (k & 1);
    gl.push(walker(g, 0xfe, W(20), W(20), { d2: 2, w576: 30, cc: 0 }), walker(g ^ 1, 7, W(40), W(40), { w556: FROZEN }));
    for (let q = 0; q < 2; q++) gl.push(walker((g + 10 + 2 * q) % 152, 7, W(20) + 0x100, W(20), { w556: FROZEN }));
    return sceneOf(seed, c, gl, { a24: (g + 151 - (k % 3) * 20) % 152 }, { ps: [2], neg2: { par: 4, dir: k & 7, tx: 30, ty: 30, ax: 31, ay: 31, x: 40, y: 20 } });
  });

  // ---- third round (survivors of the mutation run); a separate list so EDGE2's row numbering stays as recorded
  const EDGE3 = [];
  // 0: at the ball with the partner's animation byte at the 6 / 7 boundary of 0x0042bccd..0x0042bce3 (6 when the golfer has
  // strokes, 7 when not), and the partner-closer flag L74 with and without flag 0x400 (0x004292f0..0x00429382, 0x0042bc8f)
  EDGE3.push(function (seed, k) {
    const R = rng(seed); const v = k % 8;
    const c = blank(seed); const gl = [];
    const h = 2 + (k % 14), hole = c.holes[h];
    const g = R() % 152;
    const cx = W(hole.x), cy = W(hole.y);
    if (v < 4) {
      gl.push(walker(g, h, cx + 0x2000, cy, { d8: 1, bx: cx + 0x2000, by: cy, da: (v & 1) ? 1 : 0 }),
              walker(g ^ 1, h, cx + 0x3000, cy, { d5: (v & 2) ? 7 : 6, w556: FROZEN }));
    } else {
      gl.push(walker(g, h, cx + 0x700, cy, { d8: 1, fl: (v & 1) ? 0x400 : 0, bx: cx + 0x800, by: cy }),
              walker(g ^ 1, h, cx + 0x1800, cy, { fl: (v & 2) ? 0x400 : 0, bx: cx + 0x1800, by: cy, w556: FROZEN, d5: 9 }));
    }
    c.patch = [[hole.x, hole.y, 2, 0]];
    return sceneOf(seed, c, gl, {}, { shot: [0x30000000] });
  });

  // 1: a hard bounce on a slope: the slope term of 0x0042cff6..0x0042d00e with a non-zero vertical speed
  EDGE3.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed, { steep: true, flat: false }); const gl = [];
    const g = R() % 150, h = 2 + (k % 12);
    const bx = W(20) + 0x40 * (k & 3), by = W(20) + 0x40 * ((k >> 2) & 1);
    gl.push(walker(g, h, W(18), W(20), { bx: bx, by: by, x57c: W(15), x580: W(20), sp: [0x30, 0x200, 0x30, 0x80][k & 3], bz: 0,
      vz: [-0x1200, -0x900, -0x2000, -0x600][(k >> 1) & 3], dir: [0x80000000, 0, 0x40000000, -0x40000000][k & 3] | 0, fl: 0x40000, w556: -1 }),
      walker(g ^ 1, h, W(19), W(21), { w556: FROZEN }));
    c.patch = [[20, 20, [2, 4, 3, 2][k & 3], 0]];
    return sceneOf(seed, c, gl, { lvl: 1 });
  });
  // 2: the water drop where a later sample scores no better than the first (0x0042d96f..0x0042d97a): the first sample on a type
  // with attr -1, the next ones on attr 127 (type table variant 4), the cup beyond the ball
  EDGE3.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed, { tv: 4 }); const gl = [];
    const g = R() % 150, h = 2 + (k % 12);
    c.holes[h].x = 45; c.holes[h].y = 20;
    gl.push(walker(g, h, W(18), W(20), { bx: W(20), by: W(20), x57c: W(15), x580: W(20), sp: 8, fl: 0x40000, w556: -1, da: 1, d3: 9 }),
            walker(g ^ 1, h, W(19), W(22), { w556: FROZEN }));
    c.patch = [[20, 20, 0x11, 0], [15, 20, (k & 1) ? 1 : 0x12, 0], [16, 20, 0x13, 0, 19, 20]];
    return sceneOf(seed, c, gl, { lvl: 1 });
  });
  // 3: a stop whose thought changes the thought byte before the 0x0042da4a comparison, with the drive-thought conditions met
  EDGE3.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const g = R() % 150, h = 1 + (k % 3);
    c.holes[h].x = 40; c.holes[h].y = 20;
    gl.push(walker(g, h, W(18), W(20), { bx: W(30), by: W(20), x57c: W(10), x580: W(20), sp: 8, fl: 0x40004, w556: -1, da: 0, d3: 9, b528: 3 }),
            walker(g ^ 1, h, W(19), W(22), { w556: FROZEN }));
    c.patch = [[30, 20, 2, 0], [10, 20, 2, 0]];
    return sceneOf(seed, c, gl, { lvl: 1 }, { emit528: (k & 1) ? 1 : 0 });
  });

  // 4: the run flag on a fairway with the partner's strokes above 1 (the 0x0042b932 limit matters: 0x0042b965 does not set it again)
  EDGE3.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const g = R() % 152, tx = 15 + R() % 15, ty = 15 + R() % 15;
    gl.push(walker(g, 8, W(tx), W(ty), { d2: 2, cc: 0x200, fl: 0x18000, da: 0 }),
            walker(g ^ 1, 8, W(tx), W(ty + 2), { w556: FROZEN, da: (k & 1) ? 2 : 3 }));
    c.patch = [[tx, ty, (k & 2) ? 2 : 0, 0x20], [tx + 1, ty, null, 0x20], [tx - 1, ty, null, 0x20]];
    c.holes[8].tx = 45; c.holes[8].ty = 45;
    return sceneOf(seed, c, gl, { lvl: 1 }, { ps: [2] });
  });
  // 5: a story-pair slot without flag 0x40000 and a moving ball reached through the swing branch (L30 at 0x0042c76a)
  EDGE3.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const g = R() % 150, h = 2 + (k % 12);
    gl.push(walker(g, h, W(18), W(20), { bx: W(20), by: W(20), x57c: W(15), x580: W(20), sp: 0x100, bz: (k & 1) ? 0 : 20, vz: 0x10, d8: 0 }),
            walker(g ^ 1, h, W(19), W(22), { w556: FROZEN }));
    c.pairs = [{ slot: 1 + (k % 14), id: g }];
    return sceneOf(seed, c, gl, { lvl: 1, date: 8 * (400 + k) });
  });
  // 6: a step just outside the tee arrival radius (0x88 / 0x90 from the target), the speed clamp of 0x0042ba39..0x0042ba53 binding
  EDGE3.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const g = R() % 152, h = 3 + (k % 10), hole = c.holes[h];
    const a = walker(g, h, 0, 0, { w576: 10, d1: 5, cc: 0x200, d2: 2 });
    const b = walker(g ^ 1, h, W(45), W(45), { w576: 10, w556: FROZEN, da: 1 });
    const t = noObjTarget(c, a, b, 0);
    const off = [0x88, 0x90, 0x84, 0x9c][k & 3];
    a.x = t[0] - off; a.y = t[1];
    gl.push(a, b);
    return sceneOf(seed, c, gl, { sel: g, date: ((g & 3) << 4) + 0x400 * (k + 1) }, { ps: [2] });
  });
  // 7: holing out with a non-zero player category (+0x19 & 7): the statistics index of 0x0042bd9e..0x0042bdc8
  EDGE3.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const h = 2 + (k % 14), hole = c.holes[h];
    const g = R() % 152;
    gl.push(walker(g, h, W(hole.x) + 0x80, W(hole.y), { d8: 1, bx: W(hole.x) + 0x40, by: W(hole.y), d1: 1 + (k % 7), b572: k & 3 }),
            walker(g ^ 1, 0, W(45), W(45)));
    return sceneOf(seed, c, gl, {});
  });
  // 8: a tree hit with the skill bonus (+0x18 != 0, +0x16 bit 0x200, +0xf9): the distance against Random::range(0x180) at 0x0042c942
  EDGE3.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const g = R() % 150, h = 2 + (k % 12);
    gl.push(walker(g, h, W(12), W(15), { bx: W(15) + 8, by: W(15), x57c: W(10), x580: W(15), sp: 8, bz: 0x100, vz: 0x10,
      fl: 0x40000, w556: -1, d0: 0x21, ce: 0x200, b5b1: 0x60 }), walker(g ^ 1, h, W(13), W(15), { w556: FROZEN }));
    c.patch = [[15, 15, 0xd + (k & 3), 0]];
    return sceneOf(seed, c, gl, { ctype: 0 }, { rngN: { 100: [0], 0x180: [[0xf0, 0xd0, 0x100, 0x60][(k >> 2) & 3]] } });
  });
  // 9: the cup radius under game flag 0x200000 (0x0042ccc0..0x0042ccd7): the ball 0x21 / 0x22 from the tile centre
  EDGE3.push(function (seed, k) {
    const R = rng(seed); const c = blank(seed); const gl = [];
    const g = R() % 150, h = 2 + (k % 12);
    gl.push(walker(g, h, W(12), W(15), { bx: W(15) + [0x21, 0x22, 0x20, 0x23][k & 3], by: W(15), sp: 8, bz: 0, vz: 0, fl: 0x40000, w556: -1,
      d4: (k & 4) ? 0 : 0xd }), walker(g ^ 1, h, W(13), W(15), { w556: FROZEN }));
    c.patch = [[15, 15, 2, 0x80]];
    return sceneOf(seed, c, gl, { gf: 0x200000 });
  });
  // 10: the water drop over a box of attr-127 tiles around an attr -1 origin (both axes), then the drive thought against rateTile
  // over shot lengths 8..23 tiles (0x0042da63..0x0042dacf)
  EDGE3.push(function (seed, k) {
    const R = rng(seed); const gl = [];
    const g = R() % 150;
    if (k < 4) {
      // origin (15.75, 20.5) on type 0xe (attr -128), the second sample (~16.1, ~20.9) on a type-2 tile, the ball (16.25, 21.0) in
      // water: x4 scores tie (512 / 512, the first stays), x3 do not
      const c = blank(seed, { tv: 4 }); const h = 2 + k;
      c.holes[h].x = 16; c.holes[h].y = 40;
      const ox = 15 * 1024 + 0x300, oy = W(20);
      gl.push(walker(g, h, W(18), W(20), { bx: ox + 0x200, by: oy + [0x200, 0x200, 0x1e0, 0x220][k], x57c: ox, x580: oy, sp: 8, fl: 0x40000, w556: -1, da: 1, d3: 9 }),
              walker(g ^ 1, h, W(19), W(25), { w556: FROZEN }));
      c.patch = [[15, 19, 0xe, 0, 15, 21], [16, 19, 2, 0, 16, 20], [16, 21, 0x11, 0, 17, 21]];
      return sceneOf(seed, c, gl, { lvl: 1 });
    }
    const c = blank(seed); const h = 1 + (k % 3);
    const len = 4 + (k - 4) / 8;                           // shot length in tiles, 4.0 .. 5.875 (rateTile here is ~120..135)
    gl.push(walker(g, h, W(18), W(20), { bx: W(30), by: W(20), x57c: W(30) - Math.round(len * 1024), x580: W(20), sp: 8, fl: 0x40004, w556: -1, da: 0, d3: 0,
      b528: 3 }), walker(g ^ 1, h, W(19), W(22), { w556: FROZEN }));
    return sceneOf(seed, c, gl, { lvl: 1 });
  });

  FAM.edge2 = function (seed, v) { return EDGE2[v % EDGE2.length](seed, Math.floor(v / EDGE2.length)); };
  FAM.edge3 = function (seed, v) { return EDGE3[v % EDGE3.length](seed, Math.floor(v / EDGE3.length)); };

  // ---- key builders
  function familyRows(fam, n, seed0) {
    const rows = [];
    for (let k = 0; k < n; k++) rows.push(FAM[fam](seed0 + k * 101, k));
    return rows;
  }
  function key(fam, n, seed0, real, extra) {
    return function () {
      const fx = base(real);
      let rows = familyRows(fam, n, seed0);
      if (extra) rows = rows.concat(extra());
      fx.n = rows.length;
      rowsOn(fx, rows.map(function (sc) { return { on: function () { applyScene(sc); fx.rc.writeByteArray(new Uint8Array(0x200).buffer); } }; }));
      standAll(fx, real);
      return fx;
    };
  }
  F.c3ec_tick = key('tick', 60, 0x1001, false);
  F.c3ec_leave = key('leave', 60, 0x2001, false);
  F.c3ec_walk = key('walk', 80, 0x3001, false);
  F.c3ec_shot = key('shot', 60, 0x4001, false);
  F.c3ec_swing = key('swing', 60, 0x5001, false);
  F.c3ec_flight = key('flight', 80, 0x6001, false);
  F.c3ec_stop = key('stop', 80, 0x7001, false);
  F.c3ec_draw = key('draw', 40, 0x8001, false);
  F.c3ec_real = key('real', 60, 0x9001, true);
  F.c3ec_edge = key('edge', 216, 0xa001, false);
  F.c3ec_edge2 = key('edge2', 224, 0xb001, false);
  F.c3ec_edge3 = key('edge3', 220, 0xc001, false);

  Object.assign(globalThis.DIFF_FIXTURES, F);
  globalThis.__c3ec = { FAM: FAM, familyRows: familyRows, applyScene: applyScene, curRow: function () { return cur; } };
})();
