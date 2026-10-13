// c3em fixtures (round 31: the modal helpers and popups of the build / amenity panels).
//
// Every key runs under __uiTree (re/frida/js/uitree.js: frozen first thread, recording display at 0x0083ad50 guarded for
// other threads, heap and import stand-ins, neutral window-tree globals) and under __modal (re/frida/js/modal.js: the C3
// pumps stood in on the A/B thread by the models, a scripted input queue through the root window's own handlers, a
// virtual clock, a bound that turns a runaway loop into an ERROR). Fixture-owned state regions: seq (scripted results of
// the stood-in callees) and clog (their calls, with the arguments).
//
// Stood in for every key of the batch (they draw, play sound or queue a message; their arguments are the evidence):
//   drawWindow 0x0040d320, Window::refreshFull 0x00480c80, playSound 0x004481b0, startMessage 0x0040cb00.
// Run for real: the pure hit tests (hitBuildTool433c60 0x00433c60, pickAmenity434980 0x00434980, hitBuildingTool
//   0x004340a0, charEditorZone 0x004382f0, all over approxDistance 0x00467170), appendNumber 0x0042dc00 (it writes only
//   the text buffer), Random::range 0x0045c1e0 (its seed 0x00822d9c is state), bucketValue 0x0044faf0.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { (globalThis.__diffKeepAlive || []).push(x); return x; };
  const alloc = function (n) { const p = Memory.alloc(n); p.writeByteArray(new Array(n).fill(0)); return p; };
  const w32 = function (a, v) { ptr(a).writeU32(v >>> 0); };

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
  const CLOG = 0x1000;
  const C = { log: null, seq: null };
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
  // the first n dwords of a NUL-terminated string (a stack buffer has different addresses in the two arms)
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

  // ---- callee stand-ins (calleeStand, c3ea): A/B thread only, logged in clog, reverted from setTimeout(0) with a
  // per-install token. spec: {addr, abi, nargs, ret: number | function(n, args), str: {argIndex: words}}.
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
        let self = abi === 'thiscall' ? ptr(a.shift()).toUInt32() : 0;
        // sp.norm: a value the two arms cannot share (a frame address) replaced by a tag; 'self' or an argument index
        if (sp.norm && sp.norm.self) self = sp.norm.self(self);
        const words = [0x5A000000 | idx, self];
        a.forEach(function (v, i) {
          if (sp.norm && sp.norm[i]) words.push(sp.norm[i](v));
          else if (sp.str && sp.str[i]) strWords(v, sp.str[i]).forEach(function (w) { words.push(w); });
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

  // ---- the globals every key of the batch saves under the freeze (and the registry compares)
  const SAVE = [
    [0x0051a068, 0x100],      // the message text buffer the bodies fill
    [0x004c2848, 4], [0x004c2854, 4], [0x004c2e08, 4],
    [0x00542f20, 4], [0x0055e924, 4], [0x0055e928, 4], [0x00567afc, 4], [0x0056d1b0, 4],
    [0x00571fd4, 4], [0x005685f0, 4], [0x0059e7b8, 4],
    [0x005a6364, 4], [0x005a8c38, 0x40], [0x005a9f50, 4], [0x005a9f60, 4], [0x005a9f64, 4],
    [0x00822d9c, 4],          // the Random object's seed
    [0x005aa55c, 4],          // the character-editor scroll offset
    [0x0059b76c, 0x20],       // the per-row counts pickCharEditor reads
  ];
  // the four callees stood in for every key (their call records are the drawing / sound / message evidence)
  const STAND = [
    { addr: 0x0040d320, abi: 'mscdecl', nargs: 4, ret: 0x0d320000 },       // drawWindow
    { addr: 0x00480c80, abi: 'thiscall', nargs: 1, ret: 0x80c80000 },      // Window::refreshFull
    { addr: 0x004481b0, abi: 'mscdecl', nargs: 5, ret: 0x481b0000 },       // playSound
    { addr: 0x0040cb00, abi: 'mscdecl', nargs: 3, ret: 0x0cb00000 },       // startMessage
  ];

  // ---- scene: o.modal (opts for __modal), o.stand (extra callee stand-ins), o.save, o.set
  function scene(o) {
    o = o || {};
    const fx = { seq: alloc(SEQ_N * SEQ_SZ), clog: alloc(CLOG) };
    fx.clog.add(8).writeU32(0x811c9dc5);
    C.log = fx.clog; C.seq = fx.seq;
    const M = __modal(o.modal || { maxCalls: 64, maxFrames: 200 });
    standIn(STAND.concat(o.stand || []));
    const T = __uiTree({ arenaSize: o.arena || 0x2000, imports: Object.assign({}, M.imports(), o.imports || {}) });
    __freeze.atFlush(function () { if (C.log === fx.clog) { C.log = null; C.seq = null; } });
    SAVE.concat(o.save || []).forEach(function (s) { __freeze.save(ptr(s[0]), s[1]); });
    Object.keys(o.set || {}).forEach(function (a) { __freeze.save(ptr(+a), 4); w32(+a, o.set[a]); });
    M.arm();
    if (o.cursorDisplay) M.display(T.disp);
    fx.T = T; fx.M = M;
    Object.assign(fx, T.fx(), M.fx());
    return fx;
  }
  // row getters r0..rN: select the row's script, reset its sequences, write its globals, return its first argument
  function rows(fx, list, fn) {
    list.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, {
        enumerable: true, configurable: true,
        get: function () {
          fx.M.select(row.script, row.cursor);
          fx.seq.writeByteArray(new Array(SEQ_N * SEQ_SZ).fill(0));
          Object.keys(row.seq || {}).forEach(function (k) { seqSet(fx.seq, +k, row.seq[k]); });
          Object.keys(row.set || {}).forEach(function (a) { __freeze.save(ptr(+a), 4); w32(+a, row.set[a]); });
          if (row.text !== undefined) {
            ptr(0x0051a068).writeByteArray(new Array(0x100).fill(0));
            ptr(0x0051a068).writeUtf8String(row.text);
          }
          if (fn) fn(fx, row, i);
          const r = row.ret === undefined ? 0 : row.ret;
          return typeof r === 'function' ? r(fx) : r;
        },
      });
    });
    return fx;
  }
  const seqOf = function (k) { return function () { return seqNext(C.seq, k); }; };
  // a two-argument key: the row getter returns x, fx.y<i> is y
  function xy(fx, list) {
    list.forEach(function (row, i) { fx['y' + i] = row.y; });
    return rows(fx, list.map(function (row) { return Object.assign({}, row, { ret: row.x }); }));
  }

  // ====================================================================================================================
  // canAfford 0x00406c30 (cost): the four early returns and the refusal panel. A row: the cost (the argument) and the
  // three globals it tests ([0x00571fd4] the purse, [0x0059e7b8] the flag word, [0x005685f0]); the text buffer starts
  // from the row's text, so the two appendNumber calls (run for real) are visible in it.
  // ====================================================================================================================
  const CA_ROWS = [
    { cost: 0, cash: 1000, flags: 0, g5f0: 9, text: 'c3em-ca-0' },                 // cost <= 0
    { cost: -5, cash: 1000, flags: 0, g5f0: 9, text: 'c3em-ca-1' },                // negative cost
    { cost: 10, cash: 1000, flags: 0, g5f0: 9, text: 'c3em-ca-2' },                // affordable
    { cost: 100, cash: 100, flags: 0, g5f0: 9, text: 'c3em-ca-3' },                // exactly the purse
    { cost: 500, cash: 100, flags: 0x1000000, g5f0: 9, text: 'c3em-ca-4' },        // the flag bit
    { cost: 500, cash: 100, flags: 0, g5f0: 3, text: 'c3em-ca-5' },                // [0x005685f0] == 3
    { cost: 500, cash: 100, flags: 0, g5f0: 4, text: 'c3em-ca-6' },                // refusal
    { cost: 7, cash: 6, flags: 0x2000000, g5f0: 9, text: '' },                     // refusal, another flag bit
    { cost: 123456, cash: 7, flags: 0, g5f0: 0x7fffffff, text: 'c3em-ca-8' },      // refusal, six-digit price
    { cost: 1, cash: 0, flags: 0, g5f0: 4, text: 'c3em-ca-9' },                    // refusal, empty purse
    { cost: 0x40000, cash: -3, flags: 0, g5f0: 5, text: 'c3em' },                  // refusal, negative purse
    { cost: 99, cash: 98, flags: 0, g5f0: 1, text: 'c3em-ca-11' },                 // [0x005685f0] below 3
  ];
  F.c3em_ca = function () {
    const fx = scene({});
    return rows(fx, CA_ROWS.map(function (r) {
      return { set: { 0x00571fd4: r.cash, 0x0059e7b8: r.flags, 0x005685f0: r.g5f0 }, text: r.text, ret: r.cost };
    }));
  };

  // ====================================================================================================================
  // clickBuildTool433d30 0x00433d30 (x, y): one key per result of hitBuildTool433c60 0x00433c60 (run for real; the
  // coordinates come from log/c3/alt/c3em_hits.py, which models its approxDistance tests). A row also sets the flag word
  // [0x0059e7b8] and the globals the body rewrites.
  // ====================================================================================================================
  const CBT_ROWS = [
    { x: 264, y: 488, flags: 0xffffffff, note: 'r -2' },
    { x: 264, y: 488, flags: 2, note: 'r -2, flag set' },
    { x: 264, y: 552, flags: 0x12345678, note: 'r -3: the refusal panel' },
    { x: 264, y: 552, flags: 0, note: 'r -3 again' },
    { x: 0, y: 0, flags: 0x55, note: 'r -1' },
    { x: 344, y: 528, flags: 0, note: 'r 0' },
    { x: 456, y: 528, flags: 1, note: 'r 1' },
    { x: 576, y: 528, flags: 0xfffffffd, note: 'r 2' },
    { x: 712, y: 536, flags: 0x80000000, note: 'r 3' },
    { x: 800, y: 700, flags: 0, note: 'r -1 far away' },
  ];
  F.c3em_cbt = function () {
    const fx = scene({});
    return xy(fx, CBT_ROWS.map(function (r) {
      return { x: r.x, y: r.y, text: 'c3em-cbt', set: { 0x0059e7b8: r.flags, 0x0055e928: 7, 0x00567afc: 7,
                                                        0x00542f20: 0x5a5a5a, 0x004c2848: 0x4848, 0x004c2e08: 0x2e08 } };
    }));
  };

  // ====================================================================================================================
  // clickAmenityPanel 0x00434ac0 (x, y): one row per switch case of pickAmenity434980 0x00434980 (run for real) plus the
  // out-of-switch tail, whose result depends on [0x004c2854] and [0x005a9f50]. The Random::range 0x0045c1e0 cases share
  // the seed 0x00822d9c, which is state, so both arms draw the same number.
  // ====================================================================================================================
  const CAP_ROWS = [
    { x: 288, y: 552, note: 'case 0' },
    { x: 328, y: 512, note: 'case 1' },
    { x: 352, y: 552, g: { 0x0056d1b0: 5 }, note: 'case 2, [0x56d1b0] > 0' },
    { x: 352, y: 552, g: { 0x0056d1b0: 0 }, note: 'case 2, [0x56d1b0] == 0' },
    { x: 352, y: 552, g: { 0x0056d1b0: -4 }, note: 'case 2, negative' },
    { x: 472, y: 560, note: 'case 3' },
    { x: 592, y: 560, note: 'case 4' },
    { x: 648, y: 560, note: 'case 5: Random::range(4)' },
    { x: 712, y: 560, note: 'case 6: Random::range(7)' },
    { x: 240, y: 560, g: { 0x005a9f64: 0x9f640001 }, note: 'case 7: the refusal panel' },
    { x: 528, y: 576, note: 'case 9: Random::range(8)' },
    { x: 264, y: 488, g: { 0x0055e924: 1, 0x00567afc: 1, 0x004c2854: 3 }, note: 'r -2, mode 3' },
    { x: 264, y: 488, g: { 0x0055e924: 1, 0x00567afc: 1, 0x004c2854: 4, 0x005a9f50: -1 }, note: 'r -2, mode 4, sel -1' },
    { x: 0, y: 0, g: { 0x004c2854: 2, 0x005a9f50: -1 }, note: 'r -1, mode 2, sel -1' },
    { x: 0, y: 0, g: { 0x004c2854: 2, 0x005a9f50: 0x1234 }, note: 'r -1, mode 2, sel set' },
    { x: 0, y: 0, g: { 0x004c2854: 0x10, 0x005a9f50: 9 }, note: 'r -1, mode 0x10' },
    { x: 0, y: 0, g: { 0x004c2854: 0x13, 0x005a9f50: 0 }, note: 'r -1, mode 0x13' },
    { x: 0, y: 0, g: { 0x004c2854: 1, 0x005a9f50: 5 }, note: 'r -1, mode 1' },
    { x: 0, y: 0, g: { 0x004c2854: 0x20, 0x005a9f50: 5 }, note: 'r -1, mode outside the list' },
  ];
  const CAP_BASE = { 0x004c2854: 0x285400, 0x005a9f50: 0x9f5000, 0x005a9f60: 0x9f6000, 0x005a9f64: 0x9f6400,
                     0x0056d1b0: 1, 0x0055e924: 0, 0x00567afc: 0, 0x00822d9c: 0x13572468, 0x004c2e08: 0x2e08 };
  // each row gets its own Random seed, so the three Random::range cases return different numbers (one seed for every
  // row would give the same small result in each of them)
  const SEED = function (i) { return (0x13572468 + Math.imul(i, 0x7f4a7c15)) >>> 0; };
  F.c3em_cap = function () {
    const fx = scene({});
    return xy(fx, CAP_ROWS.map(function (r, i) {
      return { x: r.x, y: r.y, text: 'c3em-cap',
               set: Object.assign({}, CAP_BASE, { 0x00822d9c: SEED(i) }, r.g || {}) };
    }));
  };

  // ====================================================================================================================
  // clickSlotPanel 0x00433040 (x, y): hitTestSlot432f90 0x00432f90 reads the live slot array at 0x00570cd4, so it is
  // stood in (sequence 0) and every one of the body's arms gets a row. The [0x0055e924] != 0 rows delegate to
  // clickAmenityPanel 0x00434ac0 (this batch's hook: the verifier re-runs them with it off).
  // ====================================================================================================================
  const CSP_ROWS = [
    { x: 10, y: 20, hit: -1, note: 'no slot' },
    { x: 11, y: 21, hit: 0, note: 'slot 0' },
    { x: 12, y: 22, hit: 5, note: 'slot 5' },
    { x: 13, y: 23, hit: 0x13, note: 'the last indexed slot' },
    { x: 14, y: 24, hit: 0x14, note: '[0x004c2848] = -1' },
    { x: 15, y: 25, hit: 0x15, note: 'the refusal panel' },
    { x: 16, y: 26, hit: 0x16, note: 'switch to the amenity panel' },
    { x: 17, y: 27, hit: 0x20, note: 'a value past every arm' },
    { x: 18, y: 28, hit: -7, note: 'a negative value that is not -1' },
    { x: 288, y: 552, deleg: 1, note: 'delegated: amenity case 0' },
    { x: 648, y: 560, deleg: 1, note: 'delegated: amenity case 5' },
    { x: 0, y: 0, deleg: 1, note: 'delegated: amenity tail' },
  ];
  F.c3em_csp = function () {
    const fx = scene({ stand: [{ addr: 0x00432f90, abi: 'mscdecl', nargs: 2, ret: seqOf(0) }] });
    return xy(fx, CSP_ROWS.map(function (r, i) {
      return { x: r.x, y: r.y, text: 'c3em-csp', seq: { 0: [r.hit === undefined ? 0 : r.hit] },
               set: Object.assign({}, CAP_BASE, { 0x0055e924: r.deleg ? 1 : 0, 0x004c2848: 0x4848,
                                                  0x00822d9c: SEED(i + 7) }) };
    }));
  };

  // ====================================================================================================================
  // pickBuilding 0x00434140 (x, y): hitBuildingTool 0x004340a0 is pure and runs for real (coordinates from
  // log/c3/alt/c3em_hits.py). A row seeds the two tables the body indexes (they overlap: [0x005a8c50 + 4 * r] and
  // [0x005a8c38 + 4 * (r + 6)] are the same dwords), the count [0x005a6364], the bucket input [0x005685f0] and
  // [0x005a9f64] (the result). The [0x0055e928] != 0 rows delegate to clickBuildTool433d30 (this batch's hook).
  // ====================================================================================================================
  // the 16 dwords from 0x005a8c38: index k is [0x005a8c38 + 4k], so slot r is k = r + 6
  function tbl(over) {
    const t = new Array(16).fill(0);
    Object.keys(over || {}).forEach(function (k) { t[+k] = over[k]; });
    return t;
  }
  // bucketValue 0x0044faf0 of [0x005685f0] - 1: <= 5 -> 0, <= 9 -> 1, <= 0x11 -> 2, <= 0x12 -> 3, above -> -1.
  const PB_ROWS = [
    { x: 272, y: 560, note: 'r -3: the refusal panel' },
    { x: 224, y: 520, note: 'r -2: switch to the build tool' },
    { x: 0, y: 0, note: 'r -1' },
    { x: 384, y: 512, t: tbl({ 6: 0 }), limit: 9, note: 'r 0, entry 0, under the count: no message' },
    { x: 464, y: 512, t: tbl({ 7: 3 }), limit: 9, g5f0: 0x12, note: 'r 1, bucket 2, entry 3: refusal A' },
    { x: 544, y: 512, t: tbl({ 8: 1 }), limit: 4, g5f0: 0x12, note: 'r 2, entry 1: only the count refusal' },
    { x: 544, y: 512, t: tbl({ 8: 3 }), limit: -5, g5f0: 0x12, note: 'r 2, both refusals' },
    { x: 624, y: 512, t: tbl({ 9: 1 }), limit: 20, g5f0: 20, note: 'r 3, bucket -1: refusal B' },
    { x: 704, y: 512, t: tbl({ 10: 0 }), limit: 20, g5f0: 3, note: 'r 4, entry 0, bucket skipped' },
    { x: 424, y: 552, t: tbl({ 11: 0 }), limit: 5, g5f0: 0x12, note: 'r 5, only the count refusal' },
    { x: 504, y: 552, t: tbl({ 12: 5 }), limit: 20, g5f0: 0x12, note: 'r 6, entry 5: refusal A' },
    { x: 584, y: 552, t: tbl({ 13: -1 }), limit: 20, g5f0: 0x12, note: 'r 7, a negative entry: no refusal' },
    { x: 664, y: 552, t: tbl({ 14: 1 }), limit: 15, g5f0: 9, note: 'r 8, bucket 1: refusal B' },
    { x: 384, y: 512, t: tbl({ 6: 0 }), limit: 0, note: 'r 0, count 0: the count refusal' },
    { x: 264, y: 552, deleg: 1, note: 'delegated: build tool r -3' },
    { x: 344, y: 528, deleg: 1, note: 'delegated: build tool r 0' },
  ];
  F.c3em_pb = function () {
    const fx = scene({});
    return xy(fx, PB_ROWS.map(function (r) {
      const set = Object.assign({}, CAP_BASE, {
        0x0055e928: r.deleg ? 1 : 0, 0x00567afc: 0, 0x005a6364: r.limit === undefined ? 9 : r.limit,
        0x005685f0: r.g5f0 === undefined ? 9 : r.g5f0, 0x0059e7b8: 0, 0x00542f20: 0x5a5a5a,
      });
      // the slot table: a non-zero [0x005a8c50 + 4r] (= index r + 12 of the saved block) runs the bucket test
      const t = r.t || tbl({});
      t.forEach(function (v, k) { set[0x005a8c38 + 4 * k] = v; });
      return { x: r.x, y: r.y, text: 'c3em-pb', set: set };
    }));
  };

  // ====================================================================================================================
  // pickCharEditor 0x00438390 (row): a loop over waitTicks(5, 0) + pumpMessages, so script frame k is loop iteration k;
  // the row's `cursor` is the point iteration 0 computes its zone from. Stood in: Image_draw 0x00473e60 and
  // Surface_drawScaled 0x00473cb0 (their `this` says which sprite and which highlight, their arguments where it lands).
  // Run for real: View_toLocal 0x0047ab50 (the point comes from __modal's display slot +0x2c) and charEditorZone
  // 0x004382f0 over the ten (x, y) pairs at 0x004c7be0.
  // The body sees the virtual cursor shifted by View::fromParent's root-window offset: it is measured once per key by
  // calling View_toLocal with the virtual cursor at (0, 0), and every row's point is corrected by it.
  // ====================================================================================================================
  const PCE_STAND = [
    { addr: 0x00473e60, abi: 'thiscall', nargs: 4, ret: 0x73e60000 },        // Image_draw
    { addr: 0x00473cb0, abi: 'thiscall', nargs: 7, ret: 0x73cb0000 },        // Surface_drawScaled
  ];
  const TL = keep(new NativeFunction(ptr(0x0047ab50), 'void', ['pointer', 'pointer', 'pointer'], 'thiscall'));
  const TLBUF = alloc(8);
  const Z = { z0: [80, 312], z1: [144, 432], z2: [216, 312], z5: [416, 432], z7: [552, 432], z9: [688, 432],
              z10: [768, 312], z11: [16, 440], none: [400, 600], high: [100, 100] };
  const PRESS1 = { k: 'press1', x: 1, y: 2 }, PRESS2 = { k: 'press2', x: 1, y: 2 };
  const PCE_ROWS = [
    { row: 0, count: 100, scroll: 0, at: 'z0', evs: [[PRESS1]], note: 'zone 0, scroll 0' },
    { row: 1, count: 100, scroll: 20, at: 'z9', evs: [[PRESS1]], note: 'zone 9' },
    { row: 2, count: 15, scroll: 5, at: 'z2', evs: [[PRESS1]], note: 'zone 2, part of the row drawn' },
    { row: 0, count: 3, scroll: 0, at: 'z5', evs: [[PRESS1]], note: 'zone 5, three items drawn' },
    { row: 0, count: 100, scroll: 0, at: 'z10', evs: [[PRESS1], [PRESS1], ['z0'], [PRESS1]], note: 'zone 10 twice' },
    { row: 0, count: 100, scroll: 0x46, at: 'z10', evs: [[PRESS1], ['z0'], [PRESS1]], note: 'zone 10 at the stop' },
    { row: 1, count: 100, scroll: 20, at: 'z11', evs: [[PRESS1], ['z0'], [PRESS1]], note: 'zone 11' },
    { row: 1, count: 100, scroll: 0, at: 'z11', evs: [[PRESS1], ['z0'], [PRESS1]], note: 'zone 11 at zero' },
    { row: 3, count: 5, scroll: 60, at: 'none', evs: [[PRESS1], ['z0'], [PRESS1]], note: 'no zone: the clamp to 0' },
    { row: 0, count: 100, scroll: 7, at: 'z0', evs: [[PRESS2]], note: 'the second button' },
    { row: 0, count: 100, scroll: 7, at: 'high', evs: [[PRESS1]], note: 'above the grid' },
    { row: 0, count: 100, scroll: 7, at: 'z0',
      evs: [[{ k: 'raw', set: { 0x00822d68: 3, 0x00822d74: 1 } }]], note: 'a third button value' },
    { row: 2, count: 0, scroll: 0, at: 'z0', evs: [[PRESS1]], note: 'an empty row' },
    { row: 3, count: 12, scroll: 0, at: 'z10', evs: [[PRESS1], ['z0'], [PRESS1]], note: 'zone 10 then the clamp' },
    { row: 0, count: 100, scroll: 3, at: 'z7', evs: [[PRESS1]], note: 'zone 7' },
  ];
  F.c3em_pce = function () {
    const fx = scene({ modal: { maxCalls: 64, maxFrames: 200 }, stand: PCE_STAND });
    fx.M.display(fx.T.disp);
    fx.M.select([], [0, 0]);
    TLBUF.writeU32(0); TLBUF.add(4).writeU32(0);
    TL(ptr(0x00519a60), TLBUF, TLBUF.add(4));
    const ox = TLBUF.readS32(), oy = TLBUF.add(4).readS32();
    const cur = function (name) { return [Z[name][0] - ox, Z[name][1] - oy]; };
    const ev = function (e) { return typeof e === 'string' ? { k: 'cursor', x: cur(e)[0], y: cur(e)[1] } : e; };
    return rows(fx, PCE_ROWS.map(function (r) {
      const set = { 0x005aa55c: r.scroll };
      set[0x0059b76c + 4 * r.row] = r.count;
      return { script: r.evs.map(function (f) { return f.map(ev); }), cursor: cur(r.at), set: set, ret: r.row };
    }));
  };
  // ====================================================================================================================
  // showPopup 0x0046d6e0 (x, y, mask, flag4, disabled): one pass is waitTicks(0xa, 0) + pumpMessages (+ flushRedraw when
  // flag4 != 0), so script frame k is pass k. Stood in: setDrawTarget 0x004762d0, Text_put 0x00477250 (8 pixels per
  // character, which fixes the box geometry), pixelRed / pixelGreen / pixelBlue 0x00479830 / 0x004798c0 / 0x00479950
  // (they sample the live image 0x00519cd4), drawPanelS2 0x0040cc00, fillWindowRect 0x00480b00, Surface_stretchBlit
  // 0x00473f60, Surface_drawScaled 0x00473cb0, drawText4049d0 0x004049d0, drawTextSurface 0x00404ad0,
  // drawCenteredText2 0x00404bc0. Run for real: enterModal483d30 0x00483d30, setDirtyFlagA 0x0045ae30 (it returns and
  // clears the key code) and View_toLocal 0x0047ab50 (the hover point, from __modal's display slot +0x2c).
  // The geometry each row needs is computed from the text the same way the body does, so the scripts can aim at a line:
  // widest = 8 * the longest segment, inner = (widest - 1) | 0xf, width = inner + 0x31, height = (lines * 3 + 3) * 8,
  // half = width / 2, left = x - half, the first option line at y + 0xc + 0x12 * firstIdx and the next ones 0x18 apart.
  // A click ([0x00822d60] / [0x00822d64], written by the press1 handler) is compared against those window coordinates
  // directly; the hover point goes through View_toLocal, so it is corrected by the measured root-window offset.
  // ====================================================================================================================
  const SP_STAND = [
    { addr: 0x004762d0, abi: 'thiscall', nargs: 4, ret: 0 },                                      // setDrawTarget
    { addr: 0x00477250, abi: 'thiscall', nargs: 1, str: { 0: 8 },                                 // Text_put
      ret: function (n, a) { return 8 * ptr(a[0]).readCString().length; } },
    { addr: 0x00479830, abi: 'thiscall', nargs: 3, ret: 0x5b },                                   // pixelRed
    { addr: 0x004798c0, abi: 'thiscall', nargs: 3, ret: 0xa4 },                                   // pixelGreen
    { addr: 0x00479950, abi: 'thiscall', nargs: 3, ret: 0x27 },                                   // pixelBlue
    { addr: 0x0040cc00, abi: 'mscdecl', nargs: 5, ret: 0 },                                       // drawPanelS2
    { addr: 0x00480b00, abi: 'thiscall', nargs: 5, ret: 0 },                                      // fillWindowRect
    { addr: 0x00473f60, abi: 'thiscall', nargs: 5, ret: 0 },                                      // Surface_stretchBlit
    { addr: 0x00473cb0, abi: 'thiscall', nargs: 7, ret: 0 },                                      // Surface_drawScaled
    { addr: 0x004049d0, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },                        // drawText4049d0
    { addr: 0x00404ad0, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },                        // drawTextSurface
    { addr: 0x00404bc0, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },                        // drawCenteredText2
  ];
  // the body's own measurements, with Text_put = 8 pixels per character
  function spGeom(text, x, y) {
    const segs = text.split('\n');
    const lines = segs.length - 1;                       // one line per terminator; the tail is measured too
    let widest = 0;
    segs.forEach(function (s) { if (8 * s.length > widest) widest = 8 * s.length; });
    const inner = (widest - 1) | 0xf;
    const boxW = inner + 0x31, boxH = (lines * 3 + 3) * 8;
    const half = (boxW / 2) | 0, left = x - half;
    let firstIdx = -1;
    for (let i = 0; i < lines; i++) if (segs[i][0] === ' ' && firstIdx < 0) firstIdx = i;
    const firstY = y + 0xc + 0x12 * (firstIdx < 0 ? 0 : firstIdx);
    return { lines: lines, boxW: boxW, boxH: boxH, half: half, left: left, firstIdx: firstIdx, firstY: firstY,
             count: firstIdx < 0 ? 0 : lines - firstIdx,
             lineY: function (n) { return firstY + 0x18 * n + 4; },
             inX: x - 20, closeX: x + half - 10, closeY: y + boxH - 10 };
  }
  const SP_T1 = 'c3em popup\n option one\n option two\n option three\n';
  const SP_T2 = 'c3em\n alpha\n beta\n';
  const SP_T3 = ' only option\nc3em tail\n';
  // its unterminated tail is the widest segment, so the box width comes from the measuring pass's last call
  const SP_T4 = 'c3em\n opt a\n opt b\nthis unterminated tail is the widest segment';
  const KEY = function (code) { return { k: 'key', a: code, code: code }; };
  const SP_ROWS = [
    { t: SP_T1, x: 400, y: 200, mask: 0, f: 0, d: 0, hit: [0], note: 'option 0 toggled, then the close box' },
    { t: SP_T1, x: 400, y: 200, mask: 0, f: 0, d: 0, hit: [0, 2], note: 'options 0 and 2' },
    { t: SP_T1, x: 400, y: 200, mask: 7, f: 0, d: 0, hit: [1], note: 'option 1 cleared from a full mask' },
    { t: SP_T1, x: 400, y: 201, mask: 0, f: 0, d: 0, hit: [1], note: 'odd y: the two rect fills' },
    { t: SP_T2, x: 300, y: 100, mask: 0x10, f: 0, d: 0, hit: [], out: 1, note: 'a click outside the band' },
    { t: SP_T3, x: 500, y: 150, mask: 0, f: 0, d: 0, hit: [0], note: 'the first line is the first option' },
    { t: SP_T1, x: 400, y: 200, mask: 0x55, f: 0, d: 0, hit: [], note: 'the close box alone' },
    { t: SP_T1, x: 400, y: 200, mask: 0, f: 0, d: 1, hit: [0], note: 'a disabled line is still toggled by a click' },
    { t: SP_T1, x: 400, y: 200, mask: 0, f: 1, d: 0, hover: 1, note: 'flag4: hover option 1 and press' },
    { t: SP_T1, x: 400, y: 200, mask: 0, f: 1, d: 0, hover: 2, note: 'flag4: hover option 2 and press' },
    { t: SP_T1, x: 400, y: 200, mask: 0, f: 1, d: 2, hover: 1, then: 0, note: 'flag4: a disabled line, then option 0' },
    { t: SP_T1, x: 400, y: 200, mask: 0, f: 1, d: 0, keys: [0x28, 0x28, 0x26, 0x0d],
      note: 'flag4: Down, Down, Up, Enter' },
    { t: SP_T1, x: 400, y: 200, mask: 0, f: 1, d: 0, keys: [0x26, 0x1b], note: 'flag4: Up at -1, then Esc' },
    { t: SP_T1, x: 400, y: 200, mask: 0, f: 1, d: 0, keys: [0x20], note: 'flag4: Space with no selection' },
    { t: SP_T2, x: 300, y: 101, mask: 0, f: 1, d: 0, hover: 0,
      note: 'flag4: the shorter popup on an odd y, so the exit repaints the box' },
    { t: SP_T3, x: 500, y: 151, mask: 0, f: 1, d: 0, keys: [0x28, 0x28, 0x28, 0x0d], note: 'flag4: Down past the end' },
    { t: SP_T2, x: 300, y: 101, mask: 2, f: 0, d: 0, hover: 0, hit: [1], note: 'odd y, hover then a click' },
    { t: SP_T1, x: 400, y: 200, mask: 0, f: 1, d: 0, keys: [0x41, 0x1b], note: 'flag4: an unmapped key, then Esc' },
    { t: SP_T4, x: 400, y: 200, mask: 1, f: 0, d: 0, hover: 0,
      pre: function (g, h) { return [[h.click(g.left - 30, g.lineY(0))]]; },
      note: 'the widest segment is the unterminated tail; the selected line also carries the mask bit; a click left of the box' },
    { t: SP_T1, x: 400, y: 200, mask: 0, f: 0, d: 0,
      pre: function (g, h) {
        return [[h.cur(g.x + g.half + 10, g.lineY(0))], [h.cur(g.inX, g.firstY - 6)],
                [h.cur(g.inX, g.firstY + 0x18 * g.count + 6)],
                [h.click(g.inX, g.firstY - 6)], [h.click(g.inX, g.firstY + 0x18 * g.count + 6)],
                [h.click(g.closeX, g.y + 4)]];
      },
      note: 'the hover and click points outside the option band on every side' },
  ];
  F.c3em_sp = function () {
    const fx = scene({ modal: { maxCalls: 120, maxFrames: 300 }, stand: SP_STAND });
    fx.M.display(fx.T.disp);
    fx.M.select([], [0, 0]);
    TLBUF.writeU32(0); TLBUF.add(4).writeU32(0);
    TL(ptr(0x00519a60), TLBUF, TLBUF.add(4));
    const ox = TLBUF.readS32(), oy = TLBUF.add(4).readS32();
    SP_ROWS.forEach(function (r, i) { fx['y' + i] = r.y; fx['m' + i] = r.mask; fx['f' + i] = r.f; fx['d' + i] = r.d; });
    return rows(fx, SP_ROWS.map(function (r) {
      const g = spGeom(r.t, r.x, r.y);
      const script = [];
      const press = function (px, py) { return { k: 'press1', x: px, y: py }; };
      const hoverAt = function (n) { return { k: 'cursor', x: g.inX - ox, y: g.lineY(n) - oy }; };
      const H = { cur: function (px, py) { return { k: 'cursor', x: px - ox, y: py - oy }; }, click: press };
      g.x = r.x;
      if (r.f === 0) {
        (r.hit || []).forEach(function (n) { script.push([press(g.inX, g.lineY(n))]); });
        if (r.out) script.push([press(r.x + g.half + 100, g.lineY(0))]);
        if (r.pre) r.pre(g, H).forEach(function (f) { script.push(f); });
        script.push([press(g.closeX, g.closeY)]);                 // the close box: the only exit with flag4 == 0
        if (r.hover !== undefined) script.unshift([hoverAt(r.hover)]);
      } else if (r.keys) {
        r.keys.forEach(function (c) { script.push([KEY(c)]); script.push([]); });   // an empty frame between keys
      } else {
        script.push([hoverAt(r.hover)]);
        script.push([press(g.inX, g.lineY(r.hover))]);
        if (r.then !== undefined) {                                // the first press hit a disabled line
          script.push([hoverAt(r.then)]);
          script.push([press(g.inX, g.lineY(r.then))]);
        }
      }
      return { script: script, cursor: [0 - ox, 0 - oy], text: r.t, ret: r.x };
    }));
  };
  // ====================================================================================================================
  // skillPointDialog 0x0045f0f0 (title, mask, points, golfer, xbase): one pass is waitTicks(5, 0) + pumpMessages (the
  // waits make no visit), so script frame k is pass k + 1, and the arrow under the cursor is read at the START of a
  // pass: a cursor frame must come one frame before the press that uses it. Stood in: drawFrameR3 0x0040cef0,
  // setDrawTarget 0x004762d0, drawCenteredText2 0x00404bc0, Surface_drawScaled 0x00473cb0, drawThoughtBubble
  // 0x0045c200, Surface_stretchBlit 0x00473f60 (the row and arrow sprites: which one is drawn is the highlight
  // evidence), drawText4049d0 0x004049d0, the frame Image chain 0x00474ae0 / 0x00474dd0 / 0x00475c60 / 0x00474c40 (the
  // object lives on the body's frame, so its address differs between the arms and is logged as a tag) and showPopup
  // 0x0046d6e0 (this batch's own hook; its answer comes from sequence 1). Run for real: View_toLocal 0x0047ab50,
  // enterModal483d30 0x00483d30 and the CRT itoa 0x004ad425 (its buffer 0x00824134 is state).
  // ====================================================================================================================
  const SPD_TAG = function (v) { return (v >>> 0) === ptr(0x004c1570).readU32() >>> 0 ? 0x11115570 : 0x2222f2a3; };
  const SPD_STAND = [
    { addr: 0x0040cef0, abi: 'mscdecl', nargs: 5, ret: 0 },                                   // drawFrameR3
    { addr: 0x004762d0, abi: 'thiscall', nargs: 4, ret: 0 },                                  // setDrawTarget
    { addr: 0x00404bc0, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },                    // drawCenteredText2
    { addr: 0x00473cb0, abi: 'thiscall', nargs: 7, ret: 0 },                                  // Surface_drawScaled
    { addr: 0x0045c200, abi: 'mscdecl', nargs: 4, ret: 0 },                                   // drawThoughtBubble
    { addr: 0x00473f60, abi: 'thiscall', nargs: 5, ret: 0 },                                  // Surface_stretchBlit
    { addr: 0x004049d0, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },                    // drawText4049d0
    { addr: 0x00474ae0, abi: 'thiscall', nargs: 0, ret: 0, norm: { self: SPD_TAG } },         // Image ctor
    { addr: 0x00474dd0, abi: 'thiscall', nargs: 6, ret: 0, norm: { self: SPD_TAG } },         // Image create
    { addr: 0x00475c60, abi: 'thiscall', nargs: 7, ret: 0, norm: { self: SPD_TAG, 0: SPD_TAG } },   // blit
    { addr: 0x00474c40, abi: 'thiscall', nargs: 0, ret: 0, norm: { self: SPD_TAG } },         // Image dtor
    { addr: 0x0046d6e0, abi: 'mscdecl', nargs: 5, ret: seqOf(1) },                            // showPopup
  ];
  // the ten skill bytes at 0x005a5a04 as three dwords
  function skillSet(vals) {
    const b = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    (vals || []).forEach(function (v, i) { b[i] = v & 0xff; });
    const s = {};
    for (let d = 0; d < 3; d++) {
      s[0x005a5a04 + 4 * d] = (b[4 * d] | (b[4 * d + 1] << 8) | (b[4 * d + 2] << 16) | (b[4 * d + 3] << 24)) >>> 0;
    }
    return s;
  }
  function golferSet(g, vals) {                        // the ten bytes at 0x005795a8 + 0x100 * g
    const b = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    (vals || []).forEach(function (v, i) { b[i] = v & 0xff; });
    const s = {};
    for (let d = 0; d < 3; d++) {
      s[0x005795a8 + 0x100 * g + 4 * d] =
        (b[4 * d] | (b[4 * d + 1] << 8) | (b[4 * d + 2] << 16) | (b[4 * d + 3] << 24)) >>> 0;
    }
    return s;
  }
  const SPD_ROWS = [
    { p: -1, g: -1, xb: 0x40, s: [1, 2, 3], evs: [], note: 'points -1: the early return' },
    { p: 0, g: -1, xb: 0x40, s: [1, 0, 0], evs: ['ok'], note: 'points 0: OK closes at once' },
    { p: 3, g: -1, xb: 0x40, s: [0, 0, 0], evs: [['+', 0], 'press', ['+', 0], 'press', 'ok'], pop: [0],
      note: 'two increments, then OK with points left: the popup answers 0' },
    { p: 2, g: -1, xb: 0x40, s: [1, 1, 1], evs: [['+', 1], 'press', ['+', 1], 'press', 'ok'],
      note: 'both points spent, then OK returns' },
    { p: 3, g: -1, xb: 0x40, s: [0, 0, 0, 7], evs: [['+', 3], 'press', ['-', 3], 'press', ['-', 3], 'press', 'ok'],
      pop: [0], note: 'increment then two decrements: the second is refused at the entry value' },
    { p: 2, g: -1, xb: 0x40, s: [0, 0, 0, 0, 10], evs: [['+', 4], 'press', 'ok'], pop: [0],
      note: 'a row already at ten' },
    { p: 1, g: -1, xb: 0x40, s: [0], evs: [['+', 5], 'press', ['+', 5], 'press', 'ok'],
      note: 'the second increment has no point left' },
    { p: 2, g: 0, gw: 1, gs: [4, 5, 6, 7], xb: 0x40, s: [0, 0, 0], evs: [['+', 0], 'press', ['+', 2], 'press', 'ok'],
      note: 'the values come from the golfer record' },
    { p: 2, g: 0, gw: 0, gs: [4, 5, 6, 7], xb: 0x40, s: [2, 2, 2], evs: [['+', 0], 'press', ['+', 2], 'press', 'ok'],
      note: 'the golfer word is 0: the shared values' },
    { p: 2, g: -1, xb: -0x20, s: [0, 0, 0], evs: [['+', 6], 'press', ['+', 6], 'press', 'ok'],
      note: 'a negative xbase: the narrow frame' },
    { p: 2, g: -1, xb: 0x40, s: [0, 0, 0], evs: ['key', ['+', 7], 'press', ['+', 7], 'press', 'ok'],
      note: 'a key breaks the wait without a button' },
    { p: 2, g: -1, xb: 0x40, s: [0, 0, 0], evs: [['-', 8], 'press', ['+', 8], 'press', ['+', 8], 'press', 'ok'],
      note: 'a decrement at zero plays the refusal sound' },
    { p: 3, g: -1, xb: 0x40, s: [0, 0, 0], evs: [['+', 9], 'press', 'ok', ['+', 9], 'press', ['+', 9], 'press', 'ok'],
      pop: [1, 0], note: 'the popup answers 1 once: the dialog comes back' },
    { p: 2, g: 1, gw: 1, gs: [9, 9, 9, 9, 9, 9, 9, 9, 9, 9], xb: 0x40, s: [1, 1, 1],
      evs: [['+', 1], 'press', 'ok'], pop: [0], note: 'the second golfer record' },
    { p: 2, g: -1, xb: 0x40, s: [1, 1, 1], evs: [['at', 0xa0, 0x5a + 6], 'press', 'dy', 'ok'], pop: [0],
      note: 'the cursor right of the arrow column (no hover), and a click in the OK column but far above it' },
    { p: 0, g: -1, xb: 0x40, s: [1, 0, 0], evs: ['press', 'ok'],
      note: 'points 0: a press that is not the OK button starts another pass' },
    { p: 2, g: -1, xb: 0x40, s: [0, 0, 0], evs: [['+', 2], 'press', ['-', 2], 'press', 'ok'], pop: [0],
      note: 'the decrement takes the row back to zero, which clears its bit' },
  ];
  F.c3em_spd = function () {
    const fx = scene({ modal: { maxCalls: 160, maxFrames: 400 }, stand: SPD_STAND,
                       save: [[0x005a5a04, 0x20], [0x004c2c9c, 4], [0x00582e6c, 8], [0x00824134, 0x20],
                              [0x005794c0, 0x300]] });
    fx.M.display(fx.T.disp);
    fx.M.select([], [0, 0]);
    TLBUF.writeU32(0); TLBUF.add(4).writeU32(0);
    TL(ptr(0x00519a60), TLBUF, TLBUF.add(4));
    const ox = TLBUF.readS32(), oy = TLBUF.add(4).readS32();
    fx.title = fx.T.str('c3em skills');
    SPD_ROWS.forEach(function (r, i) {
      fx['k' + i] = 0x5a5a0000 | i;                   // the mask argument, which no path reads back
      fx['p' + i] = r.p; fx['g' + i] = r.g; fx['b' + i] = r.xb;
    });
    return rows(fx, SPD_ROWS.map(function (r) {
      const script = r.evs.map(function (e) {
        if (e === 'ok') return [{ k: 'press1', x: r.xb + 0x15e, y: 0x14e }];
        if (e === 'press') return [{ k: 'press1', x: 0, y: 0 }];
        if (e === 'key') return [{ k: 'key', a: 0x41, code: 0x41 }];
        if (e === 'dy') return [{ k: 'press1', x: r.xb + 0x15e, y: 0x14e - 50 }];   // the OK column, 50 pixels above
        if (e[0] === 'at') return [{ k: 'cursor', x: r.xb + e[1] - ox, y: e[2] - oy }];
        const rowY = 0x5a + 0x18 * e[1];
        return [{ k: 'cursor', x: r.xb + 0x40 - ox, y: (e[0] === '+' ? rowY + 6 : rowY + 0x12) - oy }];
      });
      const set = Object.assign({ 0x004c2c9c: 0x1234 }, skillSet(r.s));
      if (r.g >= 0) {
        Object.assign(set, golferSet(r.g, r.gs));
        set[0x0057956e + 0x100 * r.g] = r.gw ? 0x0007 : 0;
      }
      return { script: script, cursor: [0 - ox, 0 - oy], set: set, seq: { 1: r.pop || [0] },
               ret: function (f) { return f.title; } };
    }));
  };
  // ====================================================================================================================
  // showBulletinBoard 0x0046e810 (no arguments, no result): one key per scenario, each one call. The exit loop calls
  // pumpInput483bd0, so that pump is the key's visit (opts.visits) and one scripted press or key ends it.
  // Stood in: worldToScreen 0x0042fb90 (it writes the two screen coordinates, so the stand-in writes them from the
  // row), saveBmp 0x00475580, loadImage 0x00475840, loadBmpSurface 0x00475210 (file I/O), Surface_fillRegion
  // 0x00475c90, Surface_blitThunk 0x00475c60, Surface_stretchBlit 0x00473f60, Surface_drawSprite 0x00474440,
  // setDrawTarget 0x004762d0, drawText4049d0 0x004049d0, the two frame Images 0x00474ae0 / 0x00474c40 (their address is
  // a frame address, logged as a tag), scrollTextWindow 0x0046de70 and skillPointDialog 0x0045f0f0 (this batch's own
  // hook; its result comes from sequence 2). Run for real: clamp 0x00467130, the CRT itoa 0x004ad425,
  // appendCourseTitle 0x0040daa0 and appendDate 0x0040d7b0 (they write only the text buffer).
  // ====================================================================================================================
  const BB_TAG = function (v) { return (v >>> 0) === ptr(0x004c1570).readU32() >>> 0 ? 0x11115570 : 0x3333f2a3; };
  function bbStand(sx, sy, w2s) {
    return [
      { addr: 0x0042fb90, abi: 'mscdecl', nargs: 5,                                     // worldToScreen
        ret: function (n, a) { ptr(a[2]).writeS32(sx); ptr(a[3]).writeS32(sy); return w2s; }, norm: { 2: BB_TAG, 3: BB_TAG } },
      { addr: 0x00475580, abi: 'thiscall', nargs: 2, str: { 0: 10 }, ret: 0, norm: { 1: BB_TAG } },   // saveBmp
      { addr: 0x00475840, abi: 'thiscall', nargs: 5, ret: 0, norm: { self: BB_TAG } },  // loadImage
      { addr: 0x00475c90, abi: 'thiscall', nargs: 5, ret: 0, norm: { self: BB_TAG } },  // Surface_fillRegion
      { addr: 0x00475210, abi: 'thiscall', nargs: 1, str: { 0: 10 }, ret: 0, norm: { self: BB_TAG } },   // loadBmpSurface
      { addr: 0x00475c60, abi: 'thiscall', nargs: 7, ret: 0, norm: { self: BB_TAG } },  // Surface_blitThunk
      { addr: 0x00473f60, abi: 'thiscall', nargs: 5, ret: 0 },                          // Surface_stretchBlit
      { addr: 0x00474440, abi: 'thiscall', nargs: 6, ret: 0 },                          // Surface_drawSprite
      { addr: 0x004762d0, abi: 'thiscall', nargs: 4, ret: 0 },                          // setDrawTarget
      { addr: 0x004049d0, abi: 'mscdecl', nargs: 4, str: { 0: 10 }, ret: 0 },           // drawText4049d0
      { addr: 0x00474ae0, abi: 'thiscall', nargs: 0, ret: 0, norm: { self: BB_TAG } },  // Image ctor
      { addr: 0x00474c40, abi: 'thiscall', nargs: 0, ret: 0, norm: { self: BB_TAG } },  // Image dtor
      { addr: 0x0046de70, abi: 'mscdecl', nargs: 3, str: { 0: 10 }, ret: 0 },           // scrollTextWindow
      { addr: 0x0045f0f0, abi: 'mscdecl', nargs: 5, str: { 0: 4 }, ret: seqOf(2) },     // skillPointDialog
    ];
  }
  // the 22 event records of 0x30 bytes at 0x004c15a0: [date, value] per listed index, the rest zero
  function bbEvents(list) {
    const s = {};
    for (let i = 0; i < 22; i++) { s[0x004c15a0 + 0x30 * i] = 0; s[0x004c15a4 + 0x30 * i] = 0; }
    (list || []).forEach(function (e) {
      s[0x004c15a0 + 0x30 * e[0]] = e[1] >>> 0;
      s[0x004c15a4 + 0x30 * e[0]] = e[2] >>> 0;
    });
    return s;
  }
  // the short steps at 0x004c2d9e (stride 4), 22 of them
  function bbSteps() {
    const s = {};
    for (let i = 0; i < 22; i++) s[0x004c2d9c + 4 * i] = ((0x10 + 2 * i) << 16) >>> 0;
    return s;
  }
  const BB_ROWS = [
    { k: 'none', g: { 0x004e3db8: 0xffffffff }, note: 'no pending photo: the early return' },
    { k: 'photo', g: { 0x004e3db8: 5, 0x004e3dbc: 0x4c00, 0x008392a4: 0x3800, 0x00839338: 0x12 }, sx: 300, sy: 400,
      w2s: 1, note: 'the snapshot is written, then the counter is still below the board' },
    { k: 'camera', g: { 0x004e3db8: 6, 0x004e3dbc: 0x4c00, 0x008392a4: 0x3800, 0x00839338: 5 }, sx: 0, sy: 0, w2s: 0,
      note: 'worldToScreen fails: the camera tile is stored' },
    { k: 'clamp', g: { 0x004e3db8: 0x63, 0x004e3dbc: 0x1000, 0x008392a4: 0x2000, 0x00839338: 0x11 }, sx: -400,
      sy: 900, w2s: 1, note: 'the snapshot rectangle is clamped at both ends' },
    { k: 'board', g: { 0x004e3db8: 7, 0x004e3dbc: 0xffffffff, 0x00839338: 0x13, 0x004c2c9c: 0, 0x00822c88: 3 },
      ev: [[2, 0x2001, 0x11], [5, 0x1001, 0x22], [9, 0x3001, 0x33]], wait: 1,
      note: 'three events, no award; the first pass of the wait loop sees no input' },
    { k: 'award', g: { 0x004e3db8: 8, 0x004e3dbc: 0xffffffff, 0x00839338: 0x20, 0x004c2c9c: 5, 0x00822c88: 3 },
      ev: [[0, 0x2401, 0x44], [1, 0x801, 0x55]], pop: [0x12], note: 'the skill-point dialog runs' },
    { k: 'remap', g: { 0x004e3db8: 0x63, 0x004e3dbc: 0xffffffff, 0x00839338: 0x13, 0x004c2c9c: 3, 0x00822c88: 1 },
      ev: [[0, 0x1801, 1], [1, 0x1802, 2], [2, 0x1803, 3], [3, 0x1804, 4], [4, 0x1805, 5]], pop: [0],
      note: 'the listed records 0, 1 and 4 are remapped in the reverse list and the photo index 0x63 skips the sound and the award' },
    { k: 'empty', g: { 0x004e3db8: 9, 0x004e3dbc: 0xffffffff, 0x00839338: 0x13, 0x004c2c9c: 0, 0x00822c88: 1 },
      ev: [], note: 'no events at all' },
    { k: 'key', g: { 0x004e3db8: 10, 0x004e3dbc: 0xffffffff, 0x00839338: 0x13, 0x004c2c9c: 0, 0x00822c88: 3 },
      ev: [[4, 0x900, 7]], key: 1, note: 'the loop leaves on a key instead of a button' },
    { k: 'slot4', g: { 0x004e3db8: 11, 0x004e3dbc: 0xffffffff, 0x00839338: 0x13, 0x004c2c9c: 0, 0x00822c88: 1 },
      ev: [[0, 0x1001, 1], [1, 0x1002, 2], [2, 0x1003, 3], [3, 0x1004, 4]],
      note: 'the first empty record is 4, so the slot sprites take the 0x1f remap' },
    { k: 'neg', g: { 0x004e3db8: 12, 0x004e3dbc: 0xffffffff, 0x00839338: 0x13, 0x004c2c9c: 0, 0x00822c88: 3 },
      ev: [[6, 0x80000005, 9], [8, 0x1200, 10]],
      note: 'a date with the top bit set: the month is computed from the negative remainder' },
  ];
  const BB_SAVE = [[0x004e3db8, 4], [0x004e3dbc, 4], [0x008392a4, 4], [0x00839338, 4], [0x004c2ba0, 8],
                   [0x004c15a0, 0x420], [0x00834170, 4], [0x0059bf90, 4], [0x004c2c9c, 4], [0x0058316c, 8],
                   [0x00838f70, 4], [0x00822c88, 4], [0x004c2d9c, 0x58], [0x008392ac, 0x20], [0x005a5a04, 0x20]];
  function bbKey(row) {
    return function () {
      const fx = scene({ modal: { maxCalls: 160, maxFrames: 300, visits: ['pumpInput483bd0'] },
                         stand: bbStand(row.sx || 0, row.sy || 0, row.w2s === undefined ? 1 : row.w2s),
                         save: BB_SAVE });
      // the scenario's globals: written once per key (the function takes no argument, so there is no row getter)
      const set = Object.assign({ 0x004e3dbc: 0xffffffff, 0x008392a4: 0, 0x00839338: 0x13, 0x004c2c9c: 0,
                                  0x00822c88: 3, 0x00834170: 0x2345, 0x0059bf90: 0x6789, 0x00838f70: 0 },
                                bbSteps(), bbEvents(row.ev), row.g);
      Object.keys(set).forEach(function (a) { w32(+a, set[a]); });
      seqSet(fx.seq, 2, row.pop || [0]);
      const ev = [row.key ? { k: 'key', a: 0x41, code: 0x41 } : { k: 'press1', x: 3, y: 4 }];
      fx.M.select(row.wait ? [[], ev] : [ev]);
      return fx;
    };
  }
  BB_ROWS.forEach(function (r) { F['c3em_bb_' + r.k] = bbKey(r); });
})();
