// c3eo fixtures (round 31: the map, roster and statistics screens, and resetGolferState).
//
// Every key runs under __uiTree (re/frida/js/uitree.js): the game's first thread is frozen, the heap and import
// stand-ins are installed, the window-tree globals are neutral and the live display 0x0083ad50 is replaced by a
// recording one whose foreign-thread calls are forwarded to the real display. Keys of functions that loop on the pumps
// also run under __modal (re/frida/js/modal.js): the pumps are models on the A/B thread, input arrives as a scripted
// queue through the root window's own handlers, and a bound turns a runaway loop into an ERROR row.
//
// Fixture-owned compared regions: `seq` (scripted result sequences for the stood-in callees) and `clog` (every call a
// stood-in callee or recording object received, with its arguments).
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { (globalThis.__diffKeepAlive || []).push(x); return x; };
  const alloc = function (n) { const p = Memory.alloc(n); p.writeByteArray(new Array(n).fill(0)); return p; };
  const w32 = function (a, v) { ptr(a).writeU32(v >>> 0); };
  const w16 = function (a, v) { ptr(a).writeU16(v & 0xffff); };
  const w8 = function (a, v) { ptr(a).writeU8(v & 0xff); };

  // ---- result sequences (a state region): sequence k at +k*0x48: +0 calls, +4 length, +8.. up to 16 values (last repeats)
  const SEQ_N = 16, SEQ_SZ = 0x48;
  function seqSet(blk, k, values) {
    const b = blk.add(k * SEQ_SZ);
    b.writeByteArray(new Array(SEQ_SZ).fill(0));
    b.add(4).writeU32(values.length);
    values.forEach(function (v, i) { b.add(8 + 4 * i).writeU32(v >>> 0); });
  }
  function seqNext(blk, k) {
    const b = blk.add(k * SEQ_SZ);
    const n = b.readU32(), len = b.add(4).readU32();
    b.writeU32(n + 1);
    if (len === 0) return 0;
    return b.add(8 + 4 * Math.min(n, len - 1)).readU32();
  }

  // ---- call log (a state region): +0 records, +4 bytes used, +8 FNV-1a over everything, records from +0x10
  const CLOG = 0x8000;
  const C = { log: null, seq: null, obj: {} };
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
  // the first n dwords of a NUL-terminated string (a stack buffer has a different address in the two arms)
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

  // ---- callee stand-ins (calleeStand, c3ea): for one key a callee that is not under test becomes a recorder on the
  // A/B thread (other threads reach the original), its calls and arguments compared through clog. Installed BEFORE the
  // freeze; reverted from setTimeout(0) with a per-install token.
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
        const words = [0x5A000000 | idx, self];
        a.forEach(function (v, i) {
          if (sp.str && sp.str[i]) strWords(v, sp.str[i]).forEach(function (w) { words.push(w); });
          else words.push(sp.mask && sp.mask.indexOf(i) >= 0 ? 0 : v);
        });
        clog(words);
        const r = typeof sp.ret === 'function' ? sp.ret(n++, a) : (sp.ret || 0);
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
  const seqOf = function (k) { return function () { return seqNext(C.seq, k); }; };

  // ---- scene: o.modal (opts for __modal or absent), o.stand, o.imports, o.dispRet(fx), o.arena,
  // o.save [[addr, bytes]] (saved under the freeze), o.set {addr: dword} (written once under the freeze)
  function scene(o) {
    o = o || {};
    const fx = { seq: alloc(SEQ_N * SEQ_SZ), clog: alloc(CLOG) };
    fx.clog.add(8).writeU32(0x811c9dc5);
    C.log = fx.clog; C.seq = fx.seq; C.obj = {};
    const M = o.modal ? __modal(o.modal) : null;
    if (o.stand) standIn(o.stand);
    const imports = Object.assign({}, M ? M.imports() : {}, o.imports || {});
    const T = __uiTree({ arenaSize: o.arena || 0x4000, imports: imports, dispRet: o.dispRet ? o.dispRet(fx, M) : {} });
    __freeze.atFlush(function () { if (C.log === fx.clog) { C.log = null; C.seq = null; C.obj = {}; } });
    (o.save || []).forEach(function (s) { __freeze.save(ptr(s[0]), s[1]); });
    Object.keys(o.set || {}).forEach(function (a) { __freeze.save(ptr(+a), 4); w32(+a, o.set[a]); });
    if (M) M.arm();
    fx.T = T; fx.M = M;
    Object.assign(fx, T.fx(), M ? M.fx() : {});
    return fx;
  }
  // row getters r0..rN (c3el's): each selects the row's script, refills its sequences, writes its globals and returns
  // the vector's argument. diff_hook reads the getter once per vector, before the snapshot, so both arms start equal.
  function rows(fx, list, fn) {
    list.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, {
        enumerable: true, configurable: true,
        get: function () {
          if (fx.M) fx.M.select(row.script, row.cursor);
          fx.seq.writeByteArray(new Array(SEQ_N * SEQ_SZ).fill(0));
          Object.keys(row.seq || {}).forEach(function (k) { seqSet(fx.seq, +k, row.seq[k]); });
          Object.keys(row.set || {}).forEach(function (a) { __freeze.save(ptr(+a), 4); w32(+a, row.set[a]); });
          if (fn) fn(fx, row, i);
          const r = row.ret === undefined ? 0 : row.ret;
          return typeof r === 'function' ? r(fx) : r;
        },
      });
    });
    return fx;
  }

  // ====================================================================================================================
  // resetGolferState 0x0046d0c0 (__cdecl, no arguments): one key per scenario, each a single call.
  //
  // Seeded: the 256 placed-object records 0x0058bcb8 (0x10 each, the word at +0 is the type the scan tests), the course
  // record selector [0x005685f0], the scale [0x005a8c60] and the one course record the loop works on (0x00575ab0 +
  // 0x208 * [0x005685f0]: byte +0, word +4, byte +0x200). The two block copies (0x0046d0f3..0x0046d115) are made
  // observable by writing a per-key pattern over the first and last dwords of each DESTINATION (state regions, restored
  // by diff_hook) and over three dwords of each SOURCE (saved under the freeze).
  // Stood in: clearObjectFootprint 0x0040e400 (one int argument; it would edit the live tile tables) and
  // initGolferState 0x0046d040 (it rebuilds the 152 golfer records through the RNG); both calls are compared in clog,
  // and initGolferState's result is the function's own return value (0x0046d1ed, then `ret`).
  // ====================================================================================================================
  const RG_SRC_A = 0x0058F338, RG_DST_A = 0x004D6088, RG_LEN_A = 0x2DF0;   // 0x0046d0f3..0x0046d102
  const RG_SRC_B = 0x00520A28, RG_DST_B = 0x00543D10, RG_LEN_B = 0x668A;   // 0x0046d104..0x0046d115
  const RG_TYPES = 0x0058BCB8, RG_REC = 0x00575AB0;
  // dwords of each source a key perturbs (saved under the freeze), and of each destination (state, restored by diff_hook)
  const RG_SRC_OFF = [0, 0x100, RG_LEN_A - 1], RG_SRC_OFF_B = [0, 0x2000, RG_LEN_B - 1];
  const RG_DST_OFF = [0, 0x100, RG_LEN_A - 1], RG_DST_OFF_B = [0, 0x2000, RG_LEN_B - 1];

  function rgKey(o) {
    return function () {
      const fx = scene({
        save: [[RG_TYPES, 0x1000], [RG_REC, 0x2698],
               [RG_SRC_A + 4 * RG_SRC_OFF[0], 4], [RG_SRC_A + 4 * RG_SRC_OFF[1], 4], [RG_SRC_A + 4 * RG_SRC_OFF[2], 4],
               [RG_SRC_B + 4 * RG_SRC_OFF_B[0], 4], [RG_SRC_B + 4 * RG_SRC_OFF_B[1], 4],
               [RG_SRC_B + 4 * RG_SRC_OFF_B[2], 4]],
        set: { 0x005685F0: o.rec, 0x005A8C60: o.mult, 0x005A47E0: 0x5A47E000 },
        stand: [{ addr: 0x0040E400, abi: 'mscdecl', nargs: 1, ret: 0 },
                { addr: 0x0046D040, abi: 'mscdecl', nargs: 0, ret: seqOf(0) }],
      });
      seqSet(fx.seq, 0, [o.initRet === undefined ? 0x1 : o.initRet]);
      // placed objects: `types` lists [index, type] pairs, every other record gets the filler type
      for (let i = 0; i < 256; i++) {
        for (let k = 0; k < 0x10; k += 4) w32(RG_TYPES + 0x10 * i + k, 0);
        w16(RG_TYPES + 0x10 * i, o.filler === undefined ? 3 : o.filler);
      }
      (o.types || []).forEach(function (t) { w16(RG_TYPES + 0x10 * t[0], t[1]); });
      // the course records: every record gets o.other, the selected one o.rec0 / o.rec4 / o.rec200
      for (let h = 0; h < 19; h++) {
        w8(RG_REC + 0x208 * h, o.other === undefined ? 1 : o.other);
        w16(RG_REC + 0x208 * h + 4, 0x1111);
        w8(RG_REC + 0x208 * h + 0x200, 0);
      }
      if (o.rec >= 0 && o.rec < 19) {
        w8(RG_REC + 0x208 * o.rec, o.rec0);
        w16(RG_REC + 0x208 * o.rec + 4, o.rec4 & 0xffff);
        w8(RG_REC + 0x208 * o.rec + 0x200, o.rec200);
      }
      // make both copies observable: the destination dwords differ from the source dwords before the call
      RG_SRC_OFF.forEach(function (k, i) { w32(RG_SRC_A + 4 * k, 0x5A000000 + 0x10 * i + o.tag); });
      RG_SRC_OFF_B.forEach(function (k, i) { w32(RG_SRC_B + 4 * k, 0x5B000000 + 0x10 * i + o.tag); });
      RG_DST_OFF.forEach(function (k, i) { w32(RG_DST_A + 4 * k, 0xDA000000 + 0x10 * i + o.tag); });
      RG_DST_OFF_B.forEach(function (k, i) { w32(RG_DST_B + 4 * k, 0xDB000000 + 0x10 * i + o.tag); });
      return fx;
    };
  }
  // rec0 = the record's byte +0 (0 ends the iteration at 0x0046d134), rec4 = the word +4 the thresholds test,
  // rec200 = the byte +0x200 whose 0x60 bits gate the +0x19 at 0x0046d15a.
  F.c3eo_rg_low = rgKey({ tag: 0, rec: 0, mult: 1, rec0: 1, rec4: 0x20, rec200: 0 });            // v = 32 <= 0x32: 2
  F.c3eo_rg_skip = rgKey({ tag: 1, rec: 3, mult: 2, rec0: 0, rec4: 0x300, rec200: 0x60 });        // byte 0: no write
  F.c3eo_rg_bonus = rgKey({ tag: 2, rec: 5, mult: 1, rec0: 2, rec4: 0x140, rec200: 0x60 });       // 0x60 set, v > 0xfa
  F.c3eo_rg_nobonus = rgKey({ tag: 3, rec: 2, mult: 3, rec0: 7, rec4: 0x80, rec200: 0x20 });      // 0x60 set, v <= 0xfa
  F.c3eo_rg_mid = rgKey({ tag: 4, rec: 1, mult: 0, rec0: 4, rec4: 0x100, rec200: 0 });            // 0xfa <= v < 0x1db: 4
  F.c3eo_rg_high = rgKey({ tag: 5, rec: 4, mult: 0, rec0: 5, rec4: 0x200, rec200: 0 });           // 0x1db <= v <= 0x271: 5
  F.c3eo_rg_top = rgKey({ tag: 6, rec: 6, mult: 0, rec0: 6, rec4: 0x400, rec200: 0 });            // v > 0x271: 6
  F.c3eo_rg_sub = rgKey({ tag: 7, rec: 7, mult: 5, rec0: 3, rec4: 0x300, rec200: 0 });            // v > 0x12c: v -= 125
  F.c3eo_rg_neg = rgKey({ tag: 8, rec: 9, mult: -4, rec0: 3, rec4: -0x40, rec200: 0x40 });        // negative word, mult < 0
  F.c3eo_rg_edge = rgKey({ tag: 9, rec: 18, mult: 1, rec0: 1, rec4: 0x32, rec200: 0x60,           // last record, v == 0x32
                           types: [[0, 0x11], [5, 0x12], [9, 0x10], [200, 0x11], [255, 0x12]], filler: 0x13,
                           initRet: 0x1234 });
  F.c3eo_rg_objs = rgKey({ tag: 10, rec: 12, mult: 1, rec0: 1, rec4: 0x271, rec200: 0,            // v == 0x271 (not > )
                           types: [[1, 0x11], [2, 0x12], [3, 0x11], [4, 0x12]], filler: 0, initRet: -1 });
  F.c3eo_rg_fa = rgKey({ tag: 11, rec: 8, mult: 1, rec0: 1, rec4: 0xFA, rec200: 0x60 });          // v == 0xfa with bonus


  // ====================================================================================================================
  // showMembershipRoster 0x00454c50 (__cdecl, no arguments) under __modal: waitTicks / pumpMessages / flushRedraw /
  // pumpLight are the models, so script frame k is loop iteration k (one pumpMessages per iteration, 0x00455945).
  // The hit tests read the pointer through View_toLocal 0x0047ab50 -> the display's slot +0x2c (modal.js's cursor
  // display) and View::fromParent 0x0047b200 on the root window 0x00519a60; the four offset fields the latter
  // subtracts (+0x1ac, +0x1b0, +0x1bc, +0x1c0) and the flag word +0x9c (bit 0x20 makes it recurse to the parent) are
  // zeroed under the freeze, so the local point is the virtual cursor.
  // Stood in (calls and arguments in clog): Surface_drawSprite 0x00474440, Surface_stretchBlit 0x00473f60,
  // Widget_setQuad 0x00476310, Widget_draw 0x00476650, Widget_draw2 0x004766a0, drawStringAt 0x00476700,
  // Image_draw 0x00473e60, fillWindowRect 0x00480b00, Window::refreshFull 0x00480c80. Run for real: _itoa 0x004ad425
  // (into 0x00821c50, state), clamp 0x00467130, View_toLocal, enterModal483d30 0x00483d30.
  // Seeded: the 76 golfer-type records 0x005849e0 (0x2c each) and their names 0x004d6098 (0x230 each).
  // ====================================================================================================================
  const MR_TYPES = 0x005849E0, MR_STRIDE = 0x2C, MR_N = 76;
  const MR_NAMES = 0x004D6098, MR_NSTRIDE = 0x230;
  const MR_WIN = 0x00519A60;
  const MR_STAND = [
    { addr: 0x00474440, abi: 'thiscall', nargs: 6, ret: 0 },
    { addr: 0x00473F60, abi: 'thiscall', nargs: 5, ret: 0 },
    { addr: 0x00476310, abi: 'thiscall', nargs: 4, ret: 0 },
    { addr: 0x00476650, abi: 'thiscall', nargs: 5, str: { 1: 8 }, ret: 0 },
    { addr: 0x004766A0, abi: 'thiscall', nargs: 6, str: { 1: 8 }, ret: 0 },
    { addr: 0x00476700, abi: 'thiscall', nargs: 5, str: { 1: 8 }, ret: 0 },
    { addr: 0x00473E60, abi: 'thiscall', nargs: 4, ret: 0 },
    { addr: 0x00480B00, abi: 'thiscall', nargs: 5, ret: 0 },
    { addr: 0x00480C80, abi: 'thiscall', nargs: 1, ret: 0 },
  ];
  const MR_SAVE = [[MR_TYPES, MR_STRIDE * MR_N], [MR_NAMES, MR_NSTRIDE * MR_N],
                   [MR_WIN + 0x9C, 4], [MR_WIN + 0x1AC, 8], [MR_WIN + 0x1BC, 8]];
  // a record: {a: byte +0, b: byte +1, st: the low three bits of byte +2, ban: the 0x80 bit, n: word +0x2a,
  //            ff: byte +0x29 == 0xff, holes: {hole: flagByte}, name: the 0x230-byte name slot}
  function mrSeed(recs) {
    for (let k = 0; k < MR_N; k++) {
      const r = recs[k] || {};
      const base = MR_TYPES + MR_STRIDE * k;
      for (let o = 0; o < MR_STRIDE; o += 1) w8(base + o, 0);
      w8(base + 0, r.a || 0);
      w8(base + 1, r.b === undefined ? 0 : r.b);
      w8(base + 2, ((r.st || 0) & 7) | (r.ban ? 0x80 : 0));
      Object.keys(r.holes || {}).forEach(function (h) { w8(base + 3 + (+h), r.holes[h]); });
      w8(base + 0x29, r.ff ? 0xff : 0);
      w16(base + 0x2a, r.n || 0);
      const nm = r.name === undefined ? 'nam' + ('0' + k).slice(-2) : r.name;
      ptr(MR_NAMES + MR_NSTRIDE * k).writeUtf8String(nm);
    }
  }
  // script helpers: an event frame, then an empty one (so each frame is one loop iteration)
  const CUR = function (x, y) { return { k: 'cursor', x: x, y: y }; };
  const PRESS = function (x, y) { return { k: 'press1', x: x, y: y }; };
  const KEY = function (code, a) { return { k: 'key', a: a === undefined ? code : a, code: code }; };
  // the four hit zones (0x004557ad, 0x00455804, 0x00455890, else)
  const Z_OK = [0x2e8, 0x230], Z_UP = [0x305, 0x60], Z_DOWN = [0x305, 0x1f8], Z_NONE = [0x10, 0x10];
  function mrKey(o) {
    return function () {
      const fx = scene({
        modal: Object.assign({ maxCalls: 400, maxFrames: 120, cursor: o.cursor || Z_NONE, script: o.script },
                             o.modal || {}),
        save: MR_SAVE, stand: MR_STAND,
      });
      fx.M.display(fx.disp);                       // slots +0x2c / +0x38 from the block, the rest to uitree's recorder
      // the root window's parent offsets and flags: the local point is then the virtual cursor
      w32(MR_WIN + 0x9C, 0); w32(MR_WIN + 0x1AC, 0); w32(MR_WIN + 0x1B0, 0);
      w32(MR_WIN + 0x1BC, 0); w32(MR_WIN + 0x1C0, 0);
      mrSeed(o.recs);
      return fx;
    };
  }
  // record sets
  function mrRow(k, extra) { return Object.assign({ a: 10 + k, b: k - 2, n: k + 1, st: k & 7 }, extra || {}); }
  const MR_EMPTY = [];                                                     // no record has members
  const MR_FEW = (function () {
    const r = [];
    r[0] = { a: 5, b: 3, n: 7, st: 0, name: 'cherry' };
    r[1] = { a: 0, b: 0, n: 1, st: 1, name: 'apple' };
    r[2] = { a: 200, b: -4, n: 300, st: 2, name: 'banana', holes: { 1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 6: 6, 7: 7 } };
    r[3] = { a: 9, b: 9, n: 9, st: 3, name: 'date', ff: true };
    r[4] = { a: 1, b: 1, n: 1, st: 4, name: 'elder' };
    r[5] = { a: 2, b: 2, n: 2, st: 5, name: 'fig' };
    r[6] = { a: 3, b: 3, n: 3, st: 6, name: 'grape' };
    r[7] = { a: 4, b: 4, n: 4, st: 7, name: 'hazel' };
    r[8] = { a: 4, b: 4, n: 4, st: 2, name: 'banana' };                    // a name equal to another (the tie arm)
    r[9] = { a: 6, b: 6, n: 6, st: 2, name: 'ignored', ban: true };        // excluded by the 0x80 bit
    r[10] = { a: 7, b: 7, n: 0, st: 2, name: 'nomember' };                 // no members
    return r;
  })();
  const MR_MANY = (function () {
    const r = [];
    for (let k = 0; k < 40; k++) r[k] = mrRow(k, { name: 'm' + String.fromCharCode(97 + (k % 26)) + k });
    return r;
  })();
  const MR_STATS = (function () {
    const r = [];
    for (let k = 0; k < 14; k++) {
      const holes = {};
      for (let h = 1; h <= 18; h++) holes[h] = ((h + k) % 3) === 0 ? 4 : ((h + k) % 5) === 0 ? 7 : 0;
      r[k] = { a: k, b: k, n: k + 1, st: k & 7, ff: true, holes: holes, name: 's' + ('0' + k).slice(-2) };
    }
    return r;
  })();
  // scripts (frame k = loop iteration k; the hit test of iteration k uses the cursor delivered at frame k-1)
  function mrClick(zone, lead) {
    const s = [];
    (lead || []).forEach(function (f) { s.push(f); });
    s.push([CUR(zone[0], zone[1])]);
    s.push([]);
    s.push([PRESS(zone[0], zone[1])]);
    return s;
  }
  F.c3eo_mr_empty = mrKey({ recs: MR_EMPTY, script: mrClick(Z_OK) });
  F.c3eo_mr_few = mrKey({ recs: MR_FEW, script: mrClick(Z_OK) });
  F.c3eo_mr_many = mrKey({ recs: MR_MANY, script: mrClick(Z_OK) });
  F.c3eo_mr_stats = mrKey({ recs: MR_STATS, script: mrClick(Z_OK) });
  // scroll: down three times (the clamp at 3 and the count - 0x16 limit), then up twice, then close
  F.c3eo_mr_scroll = mrKey({ recs: MR_MANY, script: [].concat(
    [[CUR(Z_DOWN[0], Z_DOWN[1])], [], [PRESS(Z_DOWN[0], Z_DOWN[1])]],     // hover down, click: offset += 3
    [[], [], [PRESS(Z_DOWN[0], Z_DOWN[1])]],                              // again
    [[], [], [PRESS(Z_DOWN[0], Z_DOWN[1])]],                              // again (the limit)
    [[CUR(Z_UP[0], Z_UP[1])], [], [PRESS(Z_UP[0], Z_UP[1])]],             // hover up (through the bl path), click
    [[], [], [PRESS(Z_UP[0], Z_UP[1])]],                                  // again, down to 0
    [[], [], [PRESS(Z_UP[0], Z_UP[1])]],                                  // again, offset reaches 0
    [[], [], [PRESS(Z_UP[0], Z_UP[1])]],                                  // offset already 0: redraw only
    [[CUR(Z_OK[0], Z_OK[1])], [], [PRESS(Z_OK[0], Z_OK[1])]]) });         // close
  // 23 members: count - 0x16 is 1, so the first down click moves the offset by clamp(1, 1, 3) = 1 (the clamp's lower
  // bound) and the second finds count - 0x16 <= offset (the `jle` at 0x004559cb), which redraws without scrolling
  const MR_LIMIT = (function () {
    const r = [];
    for (let k = 0; k < 23; k++) r[k] = mrRow(k, { name: 'L' + String.fromCharCode(97 + (k % 26)) + k });
    return r;
  })();
  F.c3eo_mr_scrolllimit = mrKey({ recs: MR_LIMIT, script: [].concat(
    [[CUR(Z_DOWN[0], Z_DOWN[1])], [], [PRESS(Z_DOWN[0], Z_DOWN[1])]],     // offset 0 -> 1
    [[], [], [PRESS(Z_DOWN[0], Z_DOWN[1])]],                              // count - 0x16 <= offset: no move
    [[CUR(Z_OK[0], Z_OK[1])], [], [PRESS(Z_OK[0], Z_OK[1])]]) });         // close
  // every zone transition: the pointer enters a zone from none, moves from the close button to each arrow WITHOUT a
  // redraw (the arms at 0x0045582d / 0x004558a9 that un-highlight the close button first), and moves from a zone to
  // another one, which sets bl and leaves the loop at once (0x004557d3 / 0x00455857 / 0x004558d3 / 0x00455934)
  F.c3eo_mr_zones = mrKey({ recs: MR_FEW, script: [
    [CUR(Z_OK[0], Z_OK[1])],                                              // none -> close button
    [],
    [CUR(Z_UP[0], Z_UP[1])],                                              // close button -> up arrow (hover 0)
    [],
    [CUR(Z_DOWN[0], Z_DOWN[1])],                                          // up -> down: bl, redraw (then none -> down)
    [CUR(Z_NONE[0], Z_NONE[1])],                                          // down -> none: bl, redraw
    [CUR(Z_OK[0], Z_OK[1])],                                              // none -> close button
    [],
    [CUR(Z_DOWN[0], Z_DOWN[1])],                                          // close button -> down arrow (hover 0)
    [],
    [CUR(Z_OK[0], Z_OK[1])],                                              // down -> close button: bl, redraw
    [],
    [CUR(Z_NONE[0], Z_NONE[1])],                                          // close button -> none (hover 0, no bl)
    [CUR(Z_OK[0], Z_OK[1])],                                              // none -> close button
    [],
    [PRESS(Z_OK[0], Z_OK[1])]] });                                        // close
  // a press while the pointer is in no zone (hover -1: the loop redraws and goes on), then close
  F.c3eo_mr_pressnone = mrKey({ recs: MR_FEW, script: [].concat(
    [[CUR(Z_NONE[0], Z_NONE[1])], [], [PRESS(Z_NONE[0], Z_NONE[1])]],
    [[CUR(Z_OK[0], Z_OK[1])], [], [PRESS(Z_OK[0], Z_OK[1])]]) });
  // a key: flushRedraw returns 1, the button is not 1 and bl is 0, so the screen closes (0x004559f4)
  F.c3eo_mr_key = mrKey({ recs: MR_FEW, script: [[], [KEY(0x1b)], []] });
  // a key while the pointer hovers the up arrow: flushRedraw ends the loop with hover 1 and bl 0, so the screen closes
  F.c3eo_mr_keyhover = mrKey({ recs: MR_FEW, cursor: Z_UP, script: [[], [], [KEY(0x1b)], []] });


  // ====================================================================================================================
  // showHoleStatsScreen 0x00453330 (__cdecl, one argument: the hole index) under __modal. The vector IS the hole, so
  // each key runs 18 calls (holes 1..18) over one seeded course table.
  // The page it shows is (timeGetTime() >> 10) % ((the record's dword +0x20 != 0) ? 5 : 3) (0x00453362..0x0045337b);
  // modal.js's virtual clock answers timeGetTime, so opts.clock0 picks the page and a clock0 near a 1024 boundary makes
  // the page change inside the input loop (0x00454588 / 0x00454655), which re-draws the whole screen.
  // Stood in (calls and arguments in clog): appendHoleName 0x00407280, appendCents 0x0042dd50, appendRatingLabel
  // 0x004532a0, buildGolferComment 0x00469b00 and the same drawing calls as the roster. Run for real: _itoa,
  // View_toLocal, enterModal483d30; the pumps are modal.js models.
  // Seeded: the 19 course records 0x00575ab0 (0x208 each), the 152 golfer records' bytes 0x005794d9 / 0x005794da
  // (0x100 each, the loop at 0x004535d5 that takes a round off per golfer on this hole), the layout metrics
  // [0x0082121c] and [0x00821274], the rating argument [0x00569628] and the colour selector [0x0058b198].
  // ====================================================================================================================
  const HS_REC = 0x00575AB0, HS_RECSZ = 0x208, HS_RECN = 19;
  const HS_GOLF = 0x005794D9, HS_GOLFSZ = 0x100, HS_GOLFN = 152;
  const HS_Y0 = 0x0082121C, HS_YSTEP = 0x00821274;
  const HS_STAND = [
    { addr: 0x00474440, abi: 'thiscall', nargs: 6, ret: 0 },
    { addr: 0x00473F60, abi: 'thiscall', nargs: 5, ret: 0 },
    { addr: 0x00476310, abi: 'thiscall', nargs: 4, ret: 0 },
    { addr: 0x00476650, abi: 'thiscall', nargs: 5, str: { 1: 8 }, ret: 0 },
    { addr: 0x00476700, abi: 'thiscall', nargs: 5, str: { 1: 8 }, ret: 0 },
    { addr: 0x00473E60, abi: 'thiscall', nargs: 4, ret: 0 },
    { addr: 0x00480C80, abi: 'thiscall', nargs: 1, ret: 0 },
    { addr: 0x00407280, abi: 'mscdecl', nargs: 1, ret: 0 },          // appendHoleName
    { addr: 0x0042DD50, abi: 'mscdecl', nargs: 1, ret: 0 },          // appendCents
    { addr: 0x004532A0, abi: 'mscdecl', nargs: 1, ret: 0 },          // appendRatingLabel
    { addr: 0x00469B00, abi: 'mscdecl', nargs: 4, ret: 0 },          // buildGolferComment
  ];
  const HS_SAVE = [[HS_REC, HS_RECSZ * HS_RECN], [HS_GOLF, HS_GOLFSZ * HS_GOLFN],
                   [HS_Y0, 4], [HS_YSTEP, 4], [0x00569628, 4], [0x0058B198, 4]];

  // one course record: o.par (byte +0), o.w4 (word +4), o.rounds (dword +0x20), o.d24 (dword +0x24),
  // o.b200 (byte +0x200), o.stats {offset: word} for the +0x158..+0x166 words, o.hist (col, score) -> word,
  // o.comment (index) -> word, o.flags (index) -> word
  function hsRecord(h, o) {
    const b = HS_REC + HS_RECSZ * h;
    for (let k = 0; k < HS_RECSZ; k += 4) w32(b + k, 0);
    w8(b + 0, o.par);
    w16(b + 4, o.w4 & 0xffff);
    w32(b + 0x20, o.rounds >>> 0);
    w32(b + 0x24, o.d24 >>> 0);
    w8(b + 0x200, o.b200);
    w16(b + 0x158, o.c08 & 0xffff);                                   // 0x00575c08
    w16(b + 0x15c, o.c0c & 0xffff);                                   // 0x00575c0c
    w16(b + 0x15e, o.c0e & 0xffff);                                   // 0x00575c0e
    w16(b + 0x160, o.c10 & 0xffff);                                   // 0x00575c10
    w16(b + 0x162, o.c12 & 0xffff);                                   // 0x00575c12
    w16(b + 0x166, o.c16 & 0xffff);                                   // 0x00575c16
    for (let col = 0; col < 8; col++)                                 // the histogram at +0x28, 8 columns of 11 words
      for (let s = 0; s < 11; s++) w16(b + 0x28 + 2 * (col * 11 + s), o.hist(col, s) & 0xffff);
    for (let k = 0; k < 0x40; k++) w16(b + 0xd8 + 2 * k, o.comment(k) & 0xffff);   // the comment counters at +0xd8
    for (let k = 0; k < 0x32; k++) w16(b + 0x16c + 2 * k, o.flags(k) & 0xffff);    // the flag words at +0x16c
  }
  // the default record pattern: every value depends on the hole, so the 18 vectors give 18 different results
  function hsDefault(h, over) {
    return Object.assign({
      par: 3 + (h % 3), w4: 100 * h, rounds: 10 + h, d24: (h % 4 === 0) ? 0 : (h * 7 - 20),
      b200: (h & 1) ? 0x81 : 0x02, c08: 50 * h - 200, c0c: 3 * h, c0e: h, c10: 2 * h, c12: 5 * h, c16: 11 * h,
      hist: function (col, s) { return ((col * 3 + s * 5 + h) % 7 === 0) ? 0 : (col + s + h) % 9; },
      comment: function (k) { return (k % 5 === (h % 5)) ? (k + h) % 23 : 0; },
      flags: function (k) { return (0x4000 * (k & 1)) | ((k * 37 + h) & 0x3fff); },
    }, over || {});
  }
  function hsKey(o) {
    return function () {
      const fx = scene({
        modal: Object.assign({ maxCalls: 400, maxFrames: 200, cursor: o.cursor || [0x10, 0x10],
                               script: o.script, clock0: o.clock0 }, o.modal || {}),
        save: HS_SAVE, stand: HS_STAND,
      });
      fx.M.display(fx.disp);
      w32(MR_WIN + 0x9C, 0); w32(MR_WIN + 0x1AC, 0); w32(MR_WIN + 0x1B0, 0);
      w32(MR_WIN + 0x1BC, 0); w32(MR_WIN + 0x1C0, 0);
      w32(HS_Y0, o.y0 === undefined ? 0x80 : o.y0);
      w32(HS_YSTEP, o.ystep === undefined ? 0x14 : o.ystep);
      w32(0x00569628, o.rating === undefined ? 7 : o.rating);
      w32(0x0058B198, o.quad === undefined ? 0x800023e8 : o.quad);
      for (let h = 0; h < HS_RECN; h++) hsRecord(h, hsDefault(h, o.rec ? o.rec(h) : {}));
      // the golfer records: four of them name each hole, with the next byte positive, zero and negative
      for (let k = 0; k < HS_GOLFN; k++) {
        w8(HS_GOLF + HS_GOLFSZ * k, (k % 19));
        w8(HS_GOLF + HS_GOLFSZ * k + 1, (k % 3 === 0) ? 0 : ((k % 3 === 1) ? 5 : 0xfb));
      }
      return fx;
    };
  }
  const HS_HOLES = [];
  for (let h = 1; h <= 18; h++) HS_HOLES.push(h);
  // (clock >> 10) % 5 == page, with bit 0x200 clear (the `test ah, 2` banner test at 0x00454239 reads it)
  const hsClock = function (page, bit200) { return (100 + page) * 1024 + (bit200 ? 0x300 : 0x100); };
  const HS_KEYEXIT = [[], [KEY(0x1b)], []];
  F.c3eo_hs = hsKey({ clock0: hsClock(0), script: HS_KEYEXIT });
  F.c3eo_hs_p1 = hsKey({ clock0: hsClock(1), script: HS_KEYEXIT, quad: 0x80007d08 });
  F.c3eo_hs_p2 = hsKey({ clock0: hsClock(2), script: HS_KEYEXIT, quad: 0x11223344 });
  F.c3eo_hs_p3 = hsKey({ clock0: hsClock(3), script: HS_KEYEXIT });
  F.c3eo_hs_p4 = hsKey({ clock0: hsClock(4), script: HS_KEYEXIT });
  // the close button: with a non-zero par and no comment counter the panel y never moves, so the button box is
  // (0x23e..0x269, [0x0082121c] + 8 .. + 0x33)
  F.c3eo_hs_click = hsKey({
    clock0: hsClock(0), y0: 0x80,
    rec: function () { return { comment: function () { return 0; } }; },
    script: [[CUR(0x240, 0x90)], [], [PRESS(0x240, 0x90)]] });
  // the pointer enters the button box and leaves it again before the key arrives (both hover arms)
  F.c3eo_hs_hover = hsKey({
    clock0: hsClock(0), y0: 0x80,
    rec: function () { return { comment: function () { return 0; } }; },
    script: [[CUR(0x240, 0x90)], [], [CUR(0x10, 0x10)], [], [KEY(0x1b)], []] });
  // the clock crosses a 1024 boundary inside the loop: the page changes and the screen is re-drawn (0x0045465f)
  F.c3eo_hs_phase = hsKey({ clock0: 100 * 1024 + 1000, script: [[], [], [], [], [], [KEY(0x1b)], []] });
  // rounds 0: the divisor is 3 and the whole value block at 0x004535cc is skipped
  F.c3eo_hs_norounds = hsKey({ clock0: hsClock(0), script: HS_KEYEXIT,
                               rec: function () { return { rounds: 0 }; } });
  // par 0: the banner block at 0x004541fe runs, and the clock's bit 0x200 is set so its text is drawn too
  F.c3eo_hs_nopar = hsKey({ clock0: hsClock(0, true), script: HS_KEYEXIT,
                            rec: function () { return { par: 0 }; } });
  // the rating word (U-1000): d24 2 and rounds 95 make the divisor 1 + 95 + 4 = 100 (0x00453e3e), so v = c08 = 5h - 10
  // runs from -5 to 80 over the 18 holes and reaches every arm of the v / 20 switch at 0x00453eb9..0x00453ed5
  F.c3eo_hs_rating = hsKey({ clock0: hsClock(0), script: HS_KEYEXIT,
                             rec: function (h) { return { d24: 2, rounds: 95, c08: 5 * h - 10 }; } });
  // every record field zero: no histogram total, no comment, the rating line skipped
  F.c3eo_hs_zero = hsKey({ clock0: hsClock(2), script: HS_KEYEXIT, quad: 0x80007d08,
                           rec: function () { return { d24: 0, c08: 0, c0c: 0, c0e: 0, c10: 0, c12: 0, c16: 0,
                                                       hist: function () { return 0; },
                                                       comment: function () { return 0; } }; } });


  // ====================================================================================================================
  // showTournamentResults 0x0045a090 (__cdecl, no arguments): one key per scenario, each a single call.
  // Seeded: the 19 course records 0x00575ab0 (byte +0 is BOTH the hole-live flag the test at 0x0045a0eb reads and
  // the par the scoring loop at 0x0045a111 and the stroke colours at 0x0045a903 compare against: 0x00575cb8 is record
  // 1's byte +0, because 0x00575ab0 + 0x208 is 0x00575cb8), the 152 golfer records
  // 0x005794d9 (byte +0 the hole the golfer is on, bytes +2 + hole its strokes), the hole count [0x005685f0], the
  // purse [0x00567b04], the layout flags [0x0059e7b8], the tournament id [0x00822c88], the scores-only flag
  // [0x005795d9], the year word [0x005a6d3c], the course-name index [0x0059bf90], the event ids [0x005795b8] /
  // [0x005795bc] / [0x005795d6] / [0x005a34e0], and the row heights [0x00821d2c] / [0x00821ddc] / [0x00821e34] /
  // [0x00821e8c] and the button size [0x00820f58] / [0x00820f5c].
  // Stood in (calls and arguments in clog): setDrawTarget 0x004762d0, drawFrameR3 0x0040cef0, drawCenteredText
  // 0x00404b70, drawText4049d0 0x004049d0, appendCourseTitle 0x0040daa0, buildGolferName 0x004676e0, appendNumber
  // 0x0042dc00, drawStringRightAt 0x00478140, logTick 0x0040c6f0, postEvent 0x0046e7b0, playSound 0x004481b0, the
  // drawing calls and resetGolferState 0x0046d0c0 (this batch's own hook: the verifier re-runs these keys with it off).
  // ====================================================================================================================
  const TR_STAND = [
    { addr: 0x004762D0, abi: 'thiscall', nargs: 4, ret: 0 },
    { addr: 0x0040CEF0, abi: 'mscdecl', nargs: 5, ret: 0 },
    { addr: 0x00404B70, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },
    { addr: 0x004049D0, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },
    { addr: 0x0040DAA0, abi: 'mscdecl', nargs: 1, ret: 0 },
    { addr: 0x00474440, abi: 'thiscall', nargs: 6, ret: 0 },
    { addr: 0x00473F60, abi: 'thiscall', nargs: 5, ret: 0 },
    { addr: 0x004676E0, abi: 'mscdecl', nargs: 2, ret: 0 },
    { addr: 0x0042DC00, abi: 'mscdecl', nargs: 1, ret: 0 },
    { addr: 0x00476310, abi: 'thiscall', nargs: 4, ret: 0 },
    { addr: 0x00478140, abi: 'thiscall', nargs: 4, str: { 0: 8 }, ret: 0 },
    { addr: 0x0040C6F0, abi: 'mscdecl', nargs: 2, ret: 0 },
    { addr: 0x0046E7B0, abi: 'mscdecl', nargs: 3, ret: 0 },
    { addr: 0x004481B0, abi: 'mscdecl', nargs: 5, ret: 0 },
    { addr: 0x00473E60, abi: 'thiscall', nargs: 4, ret: 0 },
    { addr: 0x00480C80, abi: 'thiscall', nargs: 1, ret: 0 },
    { addr: 0x0046D0C0, abi: 'mscdecl', nargs: 0, ret: 0 },
  ];
  const TR_SAVE = [[HS_REC, HS_RECSZ * HS_RECN], [HS_GOLF, HS_GOLFSZ * HS_GOLFN],
                   [0x005685F0, 4], [0x0059E7B8, 4], [0x00822C88, 4], [0x005795D6, 8], [0x005A6D3C, 4],
                   [0x0059BF90, 4], [0x005A34E0, 4], [0x005795B8, 8], [0x00821D2C, 4], [0x00821DDC, 4],
                   [0x00567B04, 4],
                   [0x00821E34, 4], [0x00821E8C, 4], [0x00820F58, 8]];
  function trKey(o) {
    return function () {
      const fx = scene({
        modal: Object.assign({ maxCalls: 400, maxFrames: 200, cursor: o.cursor || [0x10, 0x10],
                               script: o.script || [[], [KEY(0x1b)], []] }, o.modal || {}),
        save: TR_SAVE, stand: TR_STAND,
      });
      fx.M.display(fx.disp);
      w32(MR_WIN + 0x9C, 0); w32(MR_WIN + 0x1AC, 0); w32(MR_WIN + 0x1B0, 0);
      w32(MR_WIN + 0x1BC, 0); w32(MR_WIN + 0x1C0, 0);
      // the course records: byte +0 marks a live hole, byte +8 is its par
      for (let h = 0; h < HS_RECN; h++) {
        const b = HS_REC + HS_RECSZ * h;
        for (let k = 0; k < HS_RECSZ; k += 4) w32(b + k, 0);
        w8(b + 0, (h === 0 || (o.deadHole !== undefined && h === o.deadHole)) ? 0 : 1 + (h % 3));
      }
      // the golfer records: `players` of them are on a hole, with per-hole strokes that make distinct scores
      const players = o.players === undefined ? 12 : o.players;
      for (let k = 0; k < HS_GOLFN; k++) {
        const g = HS_GOLF + HS_GOLFSZ * k;
        for (let j = 0; j < 0x40; j++) w8(g + j, 0);
        if (k >= players) continue;
        w8(g + 0, o.alt ? (k % 18) + 1 : 0);                           // 0 keeps the scorecard layout
        for (let h = 1; h <= 18; h++) {
          const played = h <= (o.holesPlayed === undefined ? 9 : o.holesPlayed);
          // o.ace names the golfer who takes one stroke per hole, the lowest score any record can give, so that
          // golfer is rank 1 (an empty record also scores 0, but its index is higher)
          const v = (o.ace === k) ? 1 : (3 + ((h + k) % 4));
          w8(g + 2 + h, played ? v : 0);
        }
      }
      w32(0x005685F0, o.holes === undefined ? 19 : o.holes);
      w32(0x00567B04, o.purse === undefined ? 5000 : o.purse);
      w32(0x0059E7B8, o.flags === undefined ? 0 : o.flags);
      w32(0x00822C88, o.tid === undefined ? 1 : o.tid);
      w8(0x005795D9, o.scoresOnly ? 1 : 0);
      w16(0x005A6D3C, o.year === undefined ? 7 : o.year);
      w32(0x0059BF90, o.courseName === undefined ? 2 : o.courseName);
      w8(0x005A34E0, 3);
      w32(0x005795B8, 0x1111); w32(0x005795BC, 0x2222); w8(0x005795D6, 0x33);
      w32(0x00821D2C, o.step1 === undefined ? 0x18 : o.step1);
      w32(0x00821DDC, o.step2 === undefined ? 0x10 : o.step2);
      w32(0x00821E34, 0x40); w32(0x00821E8C, 0x44);
      w32(0x00820F58, o.btn === undefined ? 0x2c : o.btn);
      w32(0x00820F5C, o.btn === undefined ? 0x2c : o.btn);
      return fx;
    };
  }
  F.c3eo_tr = trKey({});
  // the leader-board layout: a golfer's hole byte is non-zero, so no scorecard and no modal loop
  F.c3eo_tr_alt = trKey({ alt: true });
  // the same with the 0x4000000 flag: the tournament-name switch at 0x0045a1cd (ids 0..3 and a default)
  F.c3eo_tr_alt0 = trKey({ alt: true, flags: 0x4000000, tid: 0 });
  F.c3eo_tr_alt2 = trKey({ alt: true, flags: 0x4000000, tid: 2 });
  F.c3eo_tr_alt3 = trKey({ alt: true, flags: 0x4000000, tid: 3 });
  F.c3eo_tr_altd = trKey({ alt: true, flags: 0x4000000, tid: 7 });
  // the scores-only flag: the other title and no prize line, and the close loop is skipped
  F.c3eo_tr_scores = trKey({ scoresOnly: true });
  // the purse starts at 0, so it is computed from the hole count at 0x0045a5e1
  F.c3eo_tr_nopurse = trKey({ purse: 0 });
  // golfer 1 pays the prize (the test at 0x0045aa9c is on the golfer index, not the rank), with 19 holes and a
  // purse above 100000 but a rank above 1: logTick, the cash and the table run, the three postEvents do not
  F.c3eo_tr_prize = trKey({ purse: 200000, holes: 19 });
  // golfer 1 beats every par, so it is rank 1 too: all three postEvent calls at 0x0045ab51 / 0x0045ab76 / 0x0045abad
  F.c3eo_tr_win = trKey({ purse: 200000, holes: 19, ace: 1 });
  // the same winner with 10 holes: only the first postEvent runs (0x0045ab59 takes the `jle`)
  F.c3eo_tr_win10 = trKey({ purse: 200000, holes: 10, ace: 1 });
  // the winner with a small purse: the third postEvent's 100000 test at 0x0045ab87 fails
  F.c3eo_tr_winpoor = trKey({ purse: 5000, holes: 19, ace: 1 });
  // 36 players and a small row height: the rank limit 0x12 at 0x0045ac0d ends the pass
  F.c3eo_tr_many = trKey({ players: 36, step1: 2, step2: 2 });
  // a tall row height: the y limit 0x225 at 0x0045abf1 ends the pass instead
  F.c3eo_tr_tall = trKey({ players: 36, step1: 0x60, step2: 0x60 });
  // a dead hole in the middle: the record test at 0x0045a626 skips those golfers
  F.c3eo_tr_dead = trKey({ deadHole: 5 });
  // the close button is clicked instead of a key being pressed. Its y depends on how many rows were drawn, so the
  // button size [0x00820f58] / [0x00820f5c] is widened to 0x400: any point at x 0x2e0 and y 0x300 is then inside the
  // box whatever the rows did, and a point at (0x10, 0x10) is outside it.
  F.c3eo_tr_click = trKey({ btn: 0x400,
                            script: [[CUR(0x2e0, 0x300)], [CUR(0x10, 0x10)], [CUR(0x2e0, 0x300)],
                                     [PRESS(0x2e0, 0x300)]] });

  globalThis.__c3eo = { scene: scene, rows: rows, standIn: standIn, seqSet: seqSet, seqNext: seqNext, seqOf: seqOf,
                        clog: clog, C: C, strWords: strWords, SEQ_N: SEQ_N, SEQ_SZ: SEQ_SZ, CLOG: CLOG };
})();
