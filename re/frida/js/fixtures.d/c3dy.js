// Fixtures for batch c3dy (simulation plan phase 0, 2026-10-11): waitTicks, pumpAndRedraw, Snd::play, playSound,
// playSoundAt, inRange, rebuildTileCoverage, scanShotArea, scanNearbyGolfers, storyText.
// Reimplementations: shim/src/re/c3dy.cpp. Registry: re/frida/registry.d/c3dy.py. Purposes and per-jcc tables:
// log/c3/c3dy_purpose.md.
//
// Everything a vector touches is OURS (one arena per fixture call) or a live global saved under a frozen first thread
// (re/frida/js/freeze.js) and restored by the flush. Callees are replaced for ONE key on the A/B thread only by calleeStand
// (copied from c3dw.js): every call is appended to the call log fx.clog (compared state) and answered from a number, a
// sequence or a function. Imports (timeGetTime) go through importstandin.js. Per-vector variation uses the getter trick of
// c3dr/c3dw: diff_hook resolves "$rN" arguments at each vector BEFORE the state snapshot, so the property getter of fx.rN
// applies that row's globals and selects the row's scripted sequences (the JS variable `cur`).
(function () {
  const F = {};
  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function alloc(n) {
    const p = __keepAlloc(n);
    for (let i = 0; i < n; i += 4) p.add(i).writeU32(0);
    return p;
  }
  // The negative-control build keeps its wrong bodies dormant until this flag page is set; in the normal build the address
  // is unmapped and the write is skipped.
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
  function codeArena() {
    const size = Process.pageSize * 16;
    const base = __keepAlloc(size);
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3dy: code arena full');
      return p;
    };
  }

  // ---- row values: a numeric key writes a dword, 'b<addr>' a byte (the key is the address as a decimal string)
  function putRow(row) {
    Object.keys(row).forEach(function (a) {
      if (a.charAt(0) === 'b') ptr(+a.slice(1)).writeU8(row[a] & 0xff); else ptr(+a).writeU32(row[a] >>> 0);
    });
  }
  function saveAddrs(list) {
    const seen = {};
    list.forEach(function (a) {
      if (seen[a]) return; seen[a] = 1;
      __freeze.save(ptr(a), 4);
    });
  }
  function saveRows(rows) {
    const seen = {};
    rows.forEach(function (row) {
      Object.keys(row).forEach(function (a) {
        if (seen[a]) return; seen[a] = 1;
        __freeze.save(ptr(a.charAt(0) === 'b' ? +a.slice(1) : +a), a.charAt(0) === 'b' ? 1 : 4);
      });
    });
  }
  // Rows: fx.r<i> is a getter that applies row i's globals (row.g) and makes row i the current scripted row. Its value is
  // row.a (the first argument); fx.p<i> is row.b (the second).
  let cur = {};
  function rowsOn(fx, rows) {
    saveRows(rows.map(function (r) { return r.g || {}; }));
    rows.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () {
        putRow(row.g || {});
        cur = row;
        if (row.on) row.on(fx, row);
        return row.a === undefined ? 0 : (row.a === 'IDX' ? fx.idx : (row.a === 'SND' ? fx.snd : row.a));
      } });
      // fx.p<i> = row.b, fx.q<i> = row.c, fx.s<i> = row.d, fx.t<i> = row.e: the further arguments; the string 'IDX' stands for fx.idx
      [['p', 'b'], ['q', 'c'], ['s', 'd'], ['t', 'e']].forEach(function (pr) {
        Object.defineProperty(fx, pr[0] + i, { enumerable: true, configurable: true, get: function () { const v = row[pr[1]]; return v === undefined ? 0 : (v === 'IDX' ? fx.idx : v); } });
      });
    });
  }
  function seq(name, dflt) {
    return function (n) {
      const s = cur[name];
      if (s === undefined) return dflt === undefined ? 0 : dflt;
      return s[Math.min(n, s.length - 1)] >>> 0;
    };
  }

  // ---- a function of the exe replaced for one key on the A/B thread only (c3dw's calleeStand): impl runs on the A/B thread
  // and every call is appended to the call log fx.clog (word 0 = cursor, entries {tag, count, args...} from +0x50; word 1+k =
  // call count of stub k, up to 16 stubs). Other threads reach the real function.
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x400;
  function newClog(cap) { const l = alloc(cap || CLOG); l.writeU32(0x50); return l; }
  function calleeStand(fx, k, va, argc, abi, ret, deref) {
    freezeNow();
    const tid = Process.getCurrentThreadId();
    const at = ptr(va), key = at.toString();
    if (standLive[key]) {
      try { Interceptor.revert(at); } catch (e) { /* already reverted */ }
      delete standLive[key];
      Interceptor.flush();
    }
    const argt = Array(argc).fill('uint32');
    const orig = new NativeFunction(at, 'uint32', argt, abi);
    const log = fx.clog;
    const fn = function () {
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, arguments);
      const a = Array.prototype.slice.call(arguments);
      const raw = a.slice();
      const cell = log.add(4 + 4 * k);
      const n = cell.readU32();
      cell.writeU32(n + 1);
      // deref {argIndex: words}: the pointed-to words are logged and the (stack) pointer itself is not
      const vals = a.slice();
      if (deref) Object.keys(deref).forEach(function (ix) {
        vals[ix] = 0xfeedf00d;
        const d = deref[ix];
        if (typeof d === 'object') {                       // {str: words}: the C string up to its NUL, zero padded
          const p = ptr(raw[ix]), words = new Array(d.str).fill(0);
          let end = false;
          for (let q = 0; q < 4 * d.str; q++) {
            const c = end ? 0 : p.add(q).readU8();
            if (c === 0) end = true;
            words[q >> 2] |= c << (8 * (q & 3));
          }
          words.forEach(function (x) { vals.push(x >>> 0); });
        } else for (let w = 0; w < d; w++) vals.push(ptr(raw[ix]).add(4 * w).readU32());
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
    const cb = new NativeCallback(fn, 'uint32', argt, abi);
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
  // timeGetTime (WINMM, imported by the exe through [0x004ba21c]) stands in with the current row's sequence cur.time
  function timeOn(fx, capMode) {
    // After the row's scripted sequence plus 40 more calls a clock that would otherwise stand still jumps far enough that every
    // wait loop ends (unsigned: start - 1, elapsed 0xffffffff; signed: start + 0x7fffffff), so a wrong body that loops reads
    // as a mismatch instead of hanging the run.
    fx.implog = __importStandIn({ module: 'golf_clean.exe', imports: {
      'WINMM.dll!timeGetTime': { args: 0, impl: function (a, ctx) {
        const t = cur.time;
        if (t === undefined) return 0;
        if (capMode && ctx.call >= t.length + 40) return ((capMode === 'signed' ? t[0] + 0x7fffffff : t[0] - 1) >>> 0);
        return t[Math.min(ctx.call, t.length - 1)] >>> 0;
      } },
    } }).log;
  }

  const G68 = 0x00822d68, G94 = 0x00822d94, G7C = 0x00822d7c, G9C = 0x00822b9c, G98 = 0x00822b98, GAFCC = 0x0083afcc;
  const FIN = 0xffffffff;

  // ---- waitTicks 0x0045bf80 (ticks, pump). Row: a = ticks, b = pump; time = timeGetTime results by call number (call 0 is
  // `start`, call 1 the first elapsed test, then one per loop test; the last entry repeats), drain = drainAndRedraw results,
  // set68 = {pumpMessages call number: value written to [0x00822d68] by that stub call}
  const WT = [
    { a: 0, b: 0, time: [5, 5] },                                       // ticks 0: returns 0 before any clock read
    { a: 0, b: 1, time: [5, 5] },
    { a: 1, b: 0, time: [100, 100, 105, 111] },                         // limit 10: two loop passes, no pump
    { a: 1, b: 1, time: [100, 100, 105, 111], drain: [1, 1, 1] },       // same with the pump arm, drain keeps going
    { a: 1, b: 1, time: [100, 100, 101], drain: [0] },                  // drainAndRedraw 0 -> waitRedrawReady, 1
    { a: 1, b: 7, time: [100, 100, 101, 102], drain: [1, 1], set68: { 1: 1 } },   // [822d68] set on the second pass -> 1
    { a: 1, b: 1, time: [100, 100, 101, 102], drain: [1, 1, 0] , g: { [G68]: 9 } },   // flag already non-zero before the loop
    { a: 1, b: 0, time: [100, 111] },                                   // first elapsed 11 >= 10: returns 0 at once
    { a: 1, b: 1, time: [100, 110] },                                   // elapsed == limit (jae): returns 0 at once
    { a: 1, b: 0, time: [100, 109, 110] },                              // 9 < 10 loops once, then 10 stops
    { a: -5, b: 0, time: [0, 1, 2, 0xffffffcd, 0xffffffce] },           // limit -50 = 0xffffffce unsigned
    { a: 2, b: 0, time: [0xfffffff0, 0xfffffff5, 5, 0x1b] },            // clock wraps: elapsed 5, then 21 >= 20
    { a: 0x7fffffff, b: 0, time: [0, 1, 2, FIN] },                      // product wraps to -1000: limit -10 -> huge
    { a: 3, b: 1, time: [10, 10, 20, 30, 40], drain: [1, 1, 1, 1], set68: { 3: 5 } },   // limit 30: flag on pass 4
    { a: 1000, b: 0, time: [0, 0, 5000, 9999, 10000] },                 // limit 10000
    { a: 7, b: 1, time: [1, 1, 2], drain: [3] },                        // non-zero (not 1) drain result continues
  ];
  function waitScene() {
    const fx = { clog: newClog() };
    freezeNow();
    saveAddrs([G68]);
    rowsOn(fx, WT);
    calleeStand(fx, 0, 0x00483c30, 0, 'default', function (n) { return (0x83c30 + n) >>> 0; });
    calleeStand(fx, 1, 0x0045c030, 0, 'default', function (n) {
      if (cur.set68 && cur.set68[n] !== undefined) ptr(G68).writeU32(cur.set68[n] >>> 0);
      if (n >= 60) ptr(G68).writeU32(1);                                           // runaway cap (a wrong body that loops)
      return (0x5c030 + n) >>> 0;
    });
    const drain = seq('drain', 1);
    calleeStand(fx, 2, 0x0045af00, 0, 'default', function (n) { return n >= 60 ? 0 : drain(n); });
    calleeStand(fx, 3, 0x0045aed0, 0, 'default', function (n) { return (0x5aed0 + n) >>> 0; });
    timeOn(fx, 'unsigned');
    return fx;
  }
  F.c3dy_wt = waitScene;

  // ---- pumpAndRedraw 0x0045c0c0 (limit). Row: a = limit; g = globals (0x822b9c, 0x822b98, 0x83afcc, 0x822d94/7c junk);
  // time = timeGetTime results (call 0 = start, then one per loop pass), flush = flushRedraw results by call number,
  // set68 = {pumpMessages call number: value for [0x822d68]}, dirtyA / dirtyB = setDirtyFlag results
  const PR = [
    { a: 0, g: { [G9C]: 11, [G98]: 22, [GAFCC]: 0 }, time: [50, 50], flush: [1, 1], dirtyA: [0xa1], dirtyB: [0xb1], set68: { 0: 1 } },   // flag from the first pump
    { a: 0, g: { [G9C]: 33, [G98]: 44, [GAFCC]: 0 }, time: [50, 50], flush: [1, 0], dirtyA: [0xa2], dirtyB: [0xb2] },   // loop ends on flushRedraw; second flush 0
    { a: 0, g: { [G9C]: 55, [G98]: 66, [GAFCC]: 7 }, time: [50, 50], flush: [0, 1], dirtyA: [0xa3], dirtyB: [0xb3] },   // loop ends on [0x83afcc]; second flush 1
    { a: 0, g: { [G9C]: 77, [G98]: 88, [GAFCC]: 0 }, time: [50, 50], flush: [0, 0, 1, 1], dirtyA: [0xa4], dirtyB: [0xb4], set68: { 2: 3 } },   // limit 0 repeats twice, then the flag
    { a: 100, g: { [G9C]: 1, [G98]: 2, [GAFCC]: 0 }, time: [1000, 1010, 1050, 1099, 1100], flush: [0, 0, 0, 0, 1], dirtyA: [0xa5], dirtyB: [0xb5] },   // elapsed reaches 100
    { a: 100, g: { [G9C]: 3, [G98]: 4, [GAFCC]: 0 }, time: [1000, 1099, 1100], flush: [0], dirtyA: [0xa6], dirtyB: [0xb6] },   // 99 loops, 100 stops, final flush 0
    { a: -5, g: { [G9C]: 5, [G98]: 6, [GAFCC]: 0 }, time: [1000, 1000], flush: [0, 1], dirtyA: [0xa7], dirtyB: [0xb7] },   // negative limit: first pass elapsed 0 is not below -5
    { a: 50, g: { [G9C]: 9, [G98]: 8, [GAFCC]: 0 }, time: [0xfffffff0, 0xfffffff8, 0x1, 0x20], flush: [0, 0, 0, 1], dirtyA: [0xa8], dirtyB: [0xb8] },   // wrap: elapsed 8, 17, 48 (>= ... 0x30), then 0x30 < 50
    { a: 5, g: { [G9C]: 0x80000000, [G98]: 0x7fffffff, [GAFCC]: 0, [G94]: 0x7e7e7e7e, [G7C]: 0x7d7d7d7d }, time: [0, 5], flush: [0, 0], dirtyA: [1], dirtyB: [2] },
    { a: 5, g: { [G9C]: 0, [G98]: 0, [GAFCC]: 0xffffffff }, time: [10, 10], flush: [0, 1], dirtyA: [0xa9], dirtyB: [0xb9] },
  ];
  function pumpScene() {
    const fx = { clog: newClog(), obs: alloc(0x80) };
    freezeNow();
    saveAddrs([G68, G94, G7C]);
    rowsOn(fx, PR);
    // the pumpMessages stand-in also records the two globals the function clears before its loop ([0x00822d94], [0x00822d68])
    calleeStand(fx, 0, 0x0045c030, 0, 'default', function (n) {
      if (n < 16) { fx.obs.add(4 * n).writeU32(ptr(G94).readU32()); fx.obs.add(0x40 + 4 * n).writeU32(ptr(G68).readU32()); }
      if (cur.set68 && cur.set68[n] !== undefined) ptr(G68).writeU32(cur.set68[n] >>> 0);
      if (n >= 60) ptr(G68).writeU32(1);                                           // runaway cap (a wrong body that loops)
      return (0x5c030 + n) >>> 0;
    });
    calleeStand(fx, 1, 0x0045ae70, 0, 'default', seq('flush', 0));
    calleeStand(fx, 2, 0x0045ae30, 0, 'default', seq('dirtyA', 0));
    calleeStand(fx, 3, 0x0045ae50, 0, 'default', seq('dirtyB', 0));
    calleeStand(fx, 4, 0x00483d30, 0, 'default', function (n) { return (0x83d30 + n) >>> 0; });
    timeOn(fx, 'signed');
    return fx;
  }
  F.c3dy_par = pumpScene;


  // ====================================================================================================================
  // Sound: Snd::play 0x00484940, playSound 0x004481b0, playSoundAt 0x0040c500
  // ====================================================================================================================
  // Recording stubs (c3dw's stubBytes, thiscall, n stack arguments popped by the callee): each call appends {tag, ecx, args...}
  // at the recorder cursor ([$rec+0], entries from $rec+0x10) and answers a constant, the cell contents, or cursor ^ tag.
  const REC_SIZE = 0x800;
  function newRec() { const r = alloc(REC_SIZE); r.writeU32(0x10); return r; }
  function stubBytes(rec, tag, n, ret) {
    const r = u32(rec.toUInt32());
    const b = [0x53];                                                  // push ebx
    b.push(0x8B, 0x1D, ...r);                                          // mov ebx, [REC]
    b.push(0x81, 0xC3, ...r);                                          // add ebx, REC
    b.push(0xC7, 0x03, ...u32(tag));                                   // mov dword [ebx], tag
    b.push(0x89, 0x4B, 0x04);                                          // mov [ebx+4], ecx
    let off = 8;
    for (let i = 0; i < n; i++) {
      b.push(0x8B, 0x44, 0x24, 8 + 4 * i, 0x89, 0x43, off); off += 4;  // mov eax,[esp+8+4i]; mov [ebx+off],eax
    }
    b.push(0x81, 0x05, ...r, ...u32(off));                             // add dword [REC], off
    if (ret === undefined) b.push(0xA1, ...r, 0x35, ...u32(tag));      // mov eax, [REC]; xor eax, tag
    else if (typeof ret === 'number') b.push(0xB8, ...u32(ret));       // mov eax, ret
    else b.push(0xA1, ...u32(ret.mem.toUInt32()));                     // mov eax, [mem]
    b.push(0x5B);                                                      // pop ebx
    if (n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }
  function makeVt(emit, rec, tagBase, rets, args, slots) {
    const vt = alloc(4 * slots);
    for (let i = 0; i < slots; i++) {
      const off = 4 * i;
      vt.add(off).writePointer(emit(stubBytes(rec, (tagBase + off) >>> 0, (args && args[off]) || 0, rets && rets[off])));
    }
    return vt;
  }
  // The index playSound turns into OUR object: 0x0080d840 + idx * 108 (32-bit wrap) == addr, idx = (D / 4) * 27^-1 mod 2^30.
  function sndIndex(addr) {
    const D = (addr.toUInt32() - 0x0080d840) >>> 0;
    if (D & 3) throw new Error('c3dy: snd object not 4-aligned');
    let inv = 27;
    for (let i = 0; i < 6; i++) inv = Math.imul(inv, (2 - Math.imul(27, inv)) | 0);
    const idx = Math.imul(D >>> 2, inv) & 0x3fffffff;
    if (((0x0080d840 + Math.imul(idx, 108)) >>> 0) !== addr.toUInt32()) throw new Error('c3dy: idx conversion');
    return idx;
  }
  // g_musicDevice (0x0083ad80): bucket i has a flag byte at +0x24 + i*0x1c, a second at +0x25 and a dword at +0x28 (the table
  // Snd::setVolume scales through, 0x0083ada8 + key*0x1c). Every bucket gets a value; bucket K the ones the row names.
  const MD = 0x0083ad80;
  function buckets(K, f24, f25, vol) {
    const g = {};
    for (let i = 0; i < 16; i++) {
      g['b' + (MD + 0x24 + i * 0x1c)] = i === K ? f24 : (i & 1);
      g['b' + (MD + 0x25 + i * 0x1c)] = i === K ? f25 : ((i >> 1) & 1);
      g[MD + 0x28 + i * 0x1c] = i === K ? (vol === undefined ? 0x300 : vol) : (0x100 + i * 0x20);
    }
    return g;
  }
  // the fake Snd object of a row: every dword filled, then the fields Snd::play, the setters and playSound touch
  function applySnd(fx, row) {
    const s = fx.snd;
    for (let i = 0; i < 0x80; i += 4) s.add(i).writeU32((0x11110000 + i) >>> 0);
    s.writePointer(fx.sndVt);
    s.add(4).writeU32(row.vol === undefined ? 0x2a : row.vol);
    s.add(0x3c).writeU32(row.key >>> 0);
    s.add(0x40).writePointer(row.child ? fx.dev : ptr(0));
    s.add(0x58).writeU32(row.fl >>> 0);
    s.add(0x68).writeU32(0x6868);
    fx.cell.writeU32((row.dr || 0) >>> 0);
  }
  function sndScene(rows) {
    const fx = { clog: newClog(), rec: newRec(), cell: alloc(0x40) };
    const emit = codeArena();
    // device object: slot +0x1c answers the cell; the others record (0x40 volume, 0x44 pan, 0x4c field34, 0x9c pitch: one argument)
    fx.devVt = makeVt(emit, fx.rec, 0xDE5A0000, { 0x1c: { mem: fx.cell } }, { 0x40: 1, 0x44: 1, 0x4c: 1, 0x9c: 1, 0x04: 1 }, 0x48);
    fx.dev = alloc(0x40);
    fx.dev.writePointer(fx.devVt);
    fx.sndVt = makeVt(emit, fx.rec, 0x5ED50000, { 0x40: 0x4040, 0x80: 0x8080 }, { 0x40: 1 }, 0x48);
    fx.snd = alloc(0x80);
    fx.idx = sndIndex(fx.snd);
    freezeNow();
    rowsOn(fx, rows.map(function (r) { return Object.assign({}, r, { on: function (f, rr) { applySnd(f, rr); } }); }));
    calleeStand(fx, 0, 0x00484d60, 1, 'thiscall', function (n, a) {
      if (cur.reo) ptr(a[0]).add(0x40).writePointer(fx.dev);
      return (0x84d60 + n) >>> 0;
    });
    timeOn(fx);
    return fx;
  }
  const SPK = (k) => k;
  // Snd::play rows: a = the Snd object. key (+0x3c), fl (+0x58), child (+0x40 = the device), dr (device slot 0x1c result),
  // reo (the Snd::reopen stand-in installs the device), g (the music device's buckets), vol (+4)
  const SP = [
    { a: 'SND', key: 0x20, fl: 0x10, child: 1, dr: 7, time: [0xabc] },                       // key >= 0x10, child: slot 0x1c result, then stop
    { a: 'SND', key: 0x20, fl: 0x00, child: 1, dr: 3 },                                       // no 0x10 bit: result 3 and nothing else
    { a: 'SND', key: 0x20, fl: 0x10, child: 0, reo: 1, dr: 9, time: [0xdef] },                // no child: reopen installs one -> 9
    { a: 'SND', key: 0x20, fl: 0x10, child: 0, reo: 0, dr: 9, time: [0x123] },                // reopen leaves none -> 0
    { a: 'SND', key: 0x20, fl: 0x00, child: 0 },                                              // no child, no 0x10 bit: 0x14
    { a: 'SND', key: 0x20, fl: 0x20, child: 0 },                                              // other flag bits only: 0x14
    { a: 'SND', key: 5, fl: 0x10, child: 1, dr: 0x66, vol: 0x31, g: buckets(5, 0, 0) },       // bucketEmpty true: 0x14, no pre-hook, no stop
    { a: 'SND', key: 5, fl: 0x10, child: 1, dr: 0x66, g: buckets(5, 1, 1) },                  // bucketSet true: 0x14
    { a: 'SND', key: 5, fl: 0x10, child: 1, dr: 0x66, vol: 0x44, g: buckets(5, 1, 0) },       // free bucket: pre-hook with +4, child, stop
    { a: 'SND', key: 5, fl: 0x00, child: 1, dr: 0x77, g: buckets(5, 1, 0) },                  // free bucket, no 0x10 bit: no pre-hook
    { a: 'SND', key: 0xf, fl: 0x10, child: 0, reo: 1, dr: 0x55, vol: 0x7f, g: buckets(15, 1, 0) },   // last bucket, reopen path
    { a: 'SND', key: 0, fl: 0x10, child: 0, reo: 0, g: buckets(0, 1, 0), vol: 1 },            // bucket 0, reopen leaves no child
    { a: 'SND', key: 0x10, fl: 0x10, child: 1, dr: 0x21, g: buckets(0, 0, 0) },               // key 0x10 is not a bucket index
    { a: 'SND', key: 0xffffffff, fl: 0x30, child: 1, dr: 0x22 },                              // unsigned key far above 0x10
    { a: 'SND', key: 7, fl: 0x10, child: 0, reo: 0, g: buckets(7, 1, 0), vol: 0x10 },         // bucket 7 free, no child, no reopen
    { a: 'SND', key: 9, fl: 0x00, child: 0, g: buckets(9, 1, 0) },                            // bucket 9 free, no child, no 0x10: 0x14
  ];
  F.c3dy_sp = function () { return sndScene(SP); };

  // playSound rows: a = idx, b = volume, c = pan, d = pitch, e = field34; the Snd fields as above
  const PS = [
    { a: 'IDX', b: 0x45, c: 10, d: 100, e: 5, key: 0x20, fl: 0x10, child: 1, dr: 7, time: [0x501] },       // all setters forward to the device
    { a: 'IDX', b: 0x1ff, c: 100, d: 5000, e: -3, key: 0x20, fl: 0x10, child: 1, dr: 8, time: [0x502] },    // volume & 0x7f, pan 0x3f, pitch 0x4b0
    { a: 'IDX', b: -1, c: -100, d: -5000, e: 0x7fffffff, key: 0x20, fl: 0x00, child: 1, dr: 9 },             // pan -0x40, pitch -0x4b0
    { a: 'IDX', b: 0x30, c: 0x3f, d: 0x4b0, e: 0, key: 0x20, fl: 0x00, child: 0 },                           // limits; no device: stores only, 0x14
    { a: 'IDX', b: 0x30, c: -0x40, d: -0x4b0, e: 1, key: 0x20, fl: 0x10, child: 0, reo: 1, dr: 0x31, time: [0x503] },   // reopen installs the device
    { a: 'IDX', b: 0x30, c: 0x40, d: 0x4b1, e: 2, key: 0x20, fl: 0x10, child: 0, reo: 0, time: [0x504] },    // reopen leaves none
    { a: 'IDX', b: 0x50, c: 5, d: 50, e: 6, key: 3, fl: 0x10, child: 1, dr: 0x41, g: buckets(3, 1, 0, 0x2000), time: [0x505] },   // volume scaled through the table
    { a: 'IDX', b: 0x7f, c: 6, d: 60, e: 7, key: 3, fl: 0x10, child: 1, dr: 0x42, g: buckets(3, 1, 0, 0x37), time: [0x506] },
    { a: 'IDX', b: 0x00, c: 7, d: 70, e: 8, key: 0xb, fl: 0x00, child: 1, dr: 0x43, g: buckets(11, 1, 0, 0x10000) },
    { a: 'IDX', b: 0x20, c: 8, d: 80, e: 9, key: 4, fl: 0x10, child: 1, dr: 0x44, g: buckets(4, 0, 0) },       // bucketEmpty: 0x14
    { a: 'IDX', b: 0x21, c: 9, d: 90, e: 10, key: 4, fl: 0x10, child: 1, dr: 0x45, g: buckets(4, 1, 1) },      // bucketSet: 0x14
    { a: -1, b: 1, c: 2, d: 3, e: 4, key: 0x20, fl: 0x10, child: 1, dr: 0x46 },                              // idx -1: nothing at all
    { a: 'IDX', b: 0x22, c: 0, d: 0, e: 0, key: 0xffffffff, fl: 0x10, child: 1, dr: 0x47, time: [0x507] },
  ];
  F.c3dy_ps = function () { return sndScene(PS); };

  // playSoundAt rows: a = sound, b = wx, c = wy, d = field34; w2s = what the worldToScreen stand-in answers
  // ({r: result, sx, sy: the words it writes through its out pointers, write: false leaves the caller's slots}),
  // g = [0x005a9cbc, 0x005a9cd8, 0x005a9cdc, 0x004c2844, seed 0x00822d9c], ps = playSound stand-in results
  function gl(dbl, flag, ctr, zoom, seed) { return { 0x005a9cbc: dbl, 0x005a9cd8: flag, 0x005a9cdc: ctr, 0x004c2844: zoom, 0x00822d9c: seed }; }
  const AT = [
    { a: 5, b: 1000, c: 2000, d: 5, w2s: { r: 0, sx: 100, sy: 100 }, g: gl(0, 0, 0, 2, 0x1111) },                // off screen, field34 given: returns 0
    { a: 6, b: 1001, c: 2001, d: 7, w2s: { r: 0, sx: 100, sy: 100 }, g: gl(3, 1, 9, 5, 0x2222) },
    { a: 7, b: 1100, c: 2100, d: -1, w2s: { r: 0, sx: -50, sy: 700 }, g: gl(0, 0, 0, 2, 0x3333) },             // clamp arm, random: sx 0, sy 599
    { a: 8, b: 1200, c: 2200, d: -1, w2s: { r: 0, sx: 900, sy: -9 }, g: gl(0, 1, 0, 4, 0x4444) },             // clamp arm, counter arm: sx 799, sy 0
    { a: 9, b: 1300, c: 2300, d: -1, w2s: { r: 1, sx: 400, sy: 300 }, g: gl(0, 0, 0, 5, 0x5555) },            // result 1 with field34 -1 also clamps
    { a: 10, b: 1400, c: 2400, d: -1, w2s: { r: 1, sx: 123, sy: 299 }, g: gl(7, 1, 35, 0, 0x6666) },          // counter 35, clamp arm ignores the doubling flag
    { a: 11, b: 1500, c: 2500, d: 9, w2s: { r: 1, sx: 400, sy: 100 }, g: gl(0, 0, 0, 3, 0x7777) },            // on screen, random arm, zoom 3 (base 10)
    { a: 12, b: 1600, c: 2600, d: 9, w2s: { r: 1, sx: 799, sy: 500 }, g: gl(0, 1, 36, 1, 0x8888) },           // counter 36 -> rem 0; both edges inside
    { a: 13, b: 1700, c: 2700, d: 9, w2s: { r: 1, sx: 0, sy: 0 }, g: gl(0, 1, 1000, 4, 0x9999) },             // counter 1000 -> rem 28; zoom 4 (base 50)
    { a: 14, b: 1800, c: 2800, d: 3, w2s: { r: 1, sx: 300, sy: -1 }, g: gl(0, 0, 0, 2, 0xaaaa) },            // sy < 0: returns 0
    { a: 15, b: 1900, c: 2900, d: 3, w2s: { r: 1, sx: 300, sy: 501 }, g: gl(0, 0, 0, 2, 0xbbbb) },           // sy > 500
    { a: 16, b: 2000, c: 3000, d: 3, w2s: { r: 1, sx: -1, sy: 200 }, g: gl(0, 0, 0, 2, 0xcccc) },            // sx < 0
    { a: 17, b: 2100, c: 3100, d: 3, w2s: { r: 1, sx: 800, sy: 200 }, g: gl(0, 0, 0, 2, 0xdddd) },           // sx == 800
    { a: 18, b: 2200, c: 3200, d: 4, w2s: { r: 1, sx: 300, sy: 250 }, g: gl(1, 0, 0, 2, 0xeeee) },           // doubling: 200, 100, random arm
    { a: 19, b: 2300, c: 3300, d: 4, w2s: { r: 1, sx: 700, sy: 250 }, g: gl(1, 0, 0, 2, 0xffff) },           // doubling pushes sx to 1000: returns sx
    { a: 20, b: 2400, c: 3400, d: 4, w2s: { r: 1, sx: 400, sy: 450 }, g: gl(2, 0, 0, 6, 0x1357) },           // doubling: sy 500 is inside
    { a: 21, b: 2500, c: 3500, d: 4, w2s: { r: 1, sx: 400, sy: 451 }, g: gl(2, 0, 0, 6, 0x2468) },           // doubling: sy 502 is outside
    { a: 22, b: 2600, c: 3600, d: 5, w2s: { r: 1, sx: 250, sy: 300 }, g: gl(5, 1, -1, 2, 0x1) },             // doubling, counter -1: byte at table-1
    { a: 23, b: 2700, c: 3700, d: 5, w2s: { r: 1, sx: 250, sy: 300 }, g: gl(0, 1, -40, 2, 0x1) },            // counter -40 -> rem -4
    { a: 24, b: 300, c: 200, d: 2, w2s: { r: 1, write: false }, g: gl(0, 0, 0, 2, 0x7ab) },                  // the stand-in leaves the slots: sx = wy, sy = wx
    { a: 25, b: 1000, c: -5, d: -1, w2s: { r: 0, write: false }, g: gl(0, 1, 18, 2, 0x7ab) },                // slots left, clamp arm: wy -5 -> 0, wx 1000 -> 599
    { a: 26, b: 600, c: 700, d: 8, w2s: { r: 1, sx: 77, sy: 33 }, g: gl(0, 0, 0, -1, 0x3a3a3a3a) },          // negative zoom: base 10
    { a: 27, b: 610, c: 710, d: 8, w2s: { r: 1, sx: 640, sy: 111 }, g: gl(0, 1, 5, 2, 0x4b4b) },            // counter 5
    { a: 28, b: 3000, c: 4000, d: 4, w2s: { r: 1, sx: 300, sy: 200 }, g: gl(1, 0, 0, 2, 0xabc) },           // doubling: sy -> 0 is inside
    { a: 29, b: 3100, c: 4100, d: 4, w2s: { r: 1, sx: 200, sy: 300 }, g: gl(1, 1, 7, 2, 0xdef) },           // doubling: sx -> 0 is inside, counter arm
    { a: 30, b: 3200, c: 4200, d: 4, w2s: { r: 1, sx: 599, sy: 250 }, g: gl(1, 0, 0, 4, 0x123) },           // doubling: sx -> 798 is inside
    { a: 31, b: 3300, c: 4300, d: 4, w2s: { r: 1, sx: 600, sy: 250 }, g: gl(1, 0, 0, 4, 0x456) },           // doubling: sx -> 800 is outside (returns sx)
  ];
  F.c3dy_at = function () {
    const fx = { clog: newClog() };
    freezeNow();
    saveAddrs([0x005a9cbc, 0x005a9cd8, 0x005a9cdc, 0x004c2844, 0x00822d9c]);
    rowsOn(fx, AT);
    calleeStand(fx, 0, 0x0042fb90, 5, 'default', function (n, a) {
      const w = cur.w2s;
      if (w.write !== false) { ptr(a[2]).writeS32(w.sx); ptr(a[3]).writeS32(w.sy); }
      return w.r >>> 0;
    }, { 2: 1, 3: 1 });
    calleeStand(fx, 1, 0x004481b0, 5, 'default', function (n) { return (0x5050 + n * 7 + (cur.a & 0xff)) >>> 0; });
    return fx;
  };


  // ====================================================================================================================
  // Simulation leaves: inRange 0x004070b0, rebuildTileCoverage 0x0042f340, scanNearbyGolfers 0x00409a90,
  // scanShotArea 0x00409620, storyText 0x004668f0
  // ====================================================================================================================
  function lcg(x) { return (Math.imul(x, 1664525) + 1013904223) >>> 0; }
  function s8(v) { return v & 0xff; }
  function wr32(a, v) { ptr(a).writeU32(v >>> 0); }
  function wr16(a, v) { ptr(a).writeU16(v & 0xffff); }
  function wr8(a, v) { ptr(a).writeU8(v & 0xff); }
  function fillMem(a, n, b) { const arr = new Uint8Array(n); arr.fill(b & 0xff); ptr(a).writeByteArray(arr.buffer); }
  function saveRange(a, n) { __freeze.save(ptr(a), n); }

  // ---- inRange (type, dist, row, col). Row: a = type, b = dist, c = row, d = col; g = the signed mode byte [0x005a34e0], the
  // Random seed 0x00822d9c and (types 0x15 / 0x16) the tile word at 0x0053caf0 + 2 * (col + 50 * row)
  const INR = [];
  (function () {
    let n = 0;
    const seedOf = function () { return (0x1000 + Math.imul(n + 1, 0x9e3779b1)) >>> 0; };
    const add = function (type, dist, mode, r, c, tileByte) {
      const g = {};
      g['b' + 0x005a34e0] = mode & 0xff;
      g[0x00822d9c] = seedOf();
      if (tileByte !== undefined) {
        const a = 0x0053caf0 + 2 * (c + 50 * r);
        g['b' + a] = tileByte;
        g['b' + (a + 1)] = 0xa5;
      }
      INR.push({ a: type, b: dist, c: r, d: c, g: g });
      n++;
    };
    // modes 0..3 x types 0xd..0x10: the window (lo, hi) the original builds is modelled here only to pick distances on and next to
    // its edges (lo - 1 .. lo + 1, hi - 1 .. hi + 1 and the middle); the A/B decides whether the body agrees
    const rnd = function (seed, m) { const sd = (Math.imul(seed, 0x41c64e6d) + 0x3039) >>> 0; return Math.trunc(((sd >>> 16) & 0x7fff) / 32768 * (m & 0xffff)); };
    const ARMS = { 1: [100, 0x190, 0x32], 2: [100, 0x64, 0x14], 3: [100, 0x64, 0x32], 4: [100, 0x12c, 0x64], 5: [200, 0x190, 0x4b], 6: [100, 0x190, 0x14] };
    const armOf = function (mode, type) {
      const T = { 0: { 0xd: 1, 0xe: 2, 0xf: 3, 0x10: 5 }, 1: { 0xd: 1, 0xe: 2, 0xf: 4, 0x10: 5 }, 2: { 0xd: 2, 0xe: 1, 0xf: 3, 0x10: 5 }, 3: { 0xd: 6, 0xe: 6, 0xf: 6, 0x10: 6 } };
      return T[mode][type];
    };
    [0, 1, 2, 3].forEach(function (mode) {
      [0xd, 0xe, 0xf, 0x10].forEach(function (type) {
        for (let j = 0; j < 7; j++) {
          const sd = (0x1000 + Math.imul(n + 1, 0x9e3779b1)) >>> 0;
          const a = ARMS[armOf(mode, type)];
          const lo = a[2], hi = rnd(sd, a[0]) + a[1];
          add(type, [lo - 1, lo, lo + 1, hi - 1, hi, hi + 1, (lo + hi) >> 1][j], mode, 0, 0);
        }
      });
    });                                                                                  // 112 rows
    [0, 1, 2, 3].forEach(function (mode) {
      [1, 0x11, 0xc].forEach(function (type, k) { add(type, [0, 1, 2, 5, 0x32][(mode + k) % 5], mode, 0, 0); });
    });                                                                                  // 12 rows
    [4, 5, 0x7f, 0xff, 0x80].forEach(function (mode, k) {
      add(0xd + (k % 4), 0x70, mode, 0, 0);
      add(0x10 - (k % 4), 0x20, mode, 0, 0);
    });                                                                                  // 10 rows
    [0x15, 0x16].forEach(function (type) {
      [0, 1, 4, 5, 0x1f, 0x25, 0xe5, 0x10, 0x14, 0x11].forEach(function (v, vi) {
        [1, 0x64, 0xc7, 0xc8].forEach(function (dist, di) {
          add(type, dist, vi & 1 ? 0 : 5, 3 + ((vi + di) % 5), 7 + ((vi * 3 + di) % 11), v);
        });
      });
    });                                                                                  // 80 rows
  })();
  F.c3dy_ir = function () {
    const fx = {};
    freezeNow();
    rowsOn(fx, INR);
    return fx;
  };

  // ---- rebuildTileCoverage (no arguments; one dummy argument carries the row)
  const TILEB = 0x0053caf0, RECB = 0x0058bcb8, CNTB = 0x005a8c38;
  function rbRows() {
    const size = function (t) { return ptr(0x004c26c0 + 20 * t).readS8(); };
    const rows = [];
    const objs = function (list) { return list; };
    const six = [6, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 7].map(function (t, k) { return { i: 1 + k, t: t, r: 3 + 3 * k, c: 2 + 4 * k, cnt: k % 3, f7: 0xff }; });
    const base = { w: 4, x: 0, y: 0, tiles: 'zero', z: 0, fl: 0, cnt: {}, recs: [] };
    const mk = function (o) { return Object.assign({}, base, o); };
    rows.push(mk({ w: 0, x: 5, y: 5, tiles: 'rand', seed: 1, recs: [{ i: 1, t: 5, f7: 0xff }, { i: 2, t: 0x11, f7: 0 }, { i: 3, t: 0x7fff, f7: 0x10 }, { i: 4, t: -3, f7: 0xff }, { i: 5, t: 0x20, f7: 0x41 }, { i: 6, t: 0, f7: 0xff }] }));
    rows.push(mk({ w: 3, x: 10, y: 20, tiles: 'rand', seed: 2, z: 1 }));
    rows.push(mk({ w: -2, x: 1, y: 1, tiles: 'all40' }));
    rows.push(mk({ w: 4, x: 0, y: 0, tiles: 'sparse', sparse: { 0: 0x20, 1: 0x400, 52: 0x420, 53: 0x40, 51: 0x01, 200: 0x420 } }));
    rows.push(mk({ w: 2, x: 48, y: 48, tiles: 'rand', seed: 3 }));
    rows.push(mk({ w: 1, x: 49, y: 49, tiles: 'all420' }));
    rows.push(mk({ w: 5, x: 3, y: 4, tiles: 'rand', seed: 4, recs: six, z: 0 }));                      // z == 0: every 6..0x10 object gets the bit
    rows.push(mk({ w: 0, x: 3, y: 4, tiles: 'all40', recs: six, z: 5 }));                              // footprints land on 0x40 words
    rows.push(mk({ w: 0, x: 3, y: 4, tiles: 'zero', recs: six, z: 5, fl: 0 }));                        // nothing under the footprints
    rows.push(mk({ w: 0, x: 3, y: 4, tiles: 'zero', recs: six, z: 5, fl: 0x1000000 }));                // z != 0 with the flag bit
    rows.push(mk({ w: 0, x: 3, y: 4, tiles: 'zero', recs: six, z: 7, fl: 0x0ffffff }));                // z != 0, flag bit clear
    rows.push(mk({ w: 0, x: 3, y: 4, tiles: 'zero', recs: [{ i: 1, t: 7, r: 6, c: 6, cnt: 3, f7: 0xff }], cnt: { 7: 50 }, z: 5 }));   // t == 7 skips the table
    rows.push(mk({ w: 0, x: 3, y: 4, tiles: 'zero', recs: [{ i: 1, t: 8, r: 6, c: 6, cnt: 2, f7: 0xff }], cnt: { 8: 0 }, z: 5 }));    // table raised to cnt + 1
    rows.push(mk({ w: 0, x: 3, y: 4, tiles: 'zero', recs: [{ i: 1, t: 9, r: 6, c: 6, cnt: 5, f7: 0xff }], cnt: { 9: 200 }, z: 5 }));  // table lowered to 99
    rows.push(mk({ w: 0, x: 3, y: 4, tiles: 'zero', recs: [{ i: 1, t: 0x10, r: 6, c: 6, cnt: 98, f7: 0xff }], cnt: { 0x10: 3 }, z: 5 }));   // lo 99
    rows.push(mk({ w: 0, x: 3, y: 4, tiles: 'zero', recs: [{ i: 1, t: 6, r: 6, c: 6, cnt: 120, f7: 0xff }], cnt: { 6: 3 }, z: 5 }));   // lo above 99: hi < lo
    // 0x40 words only in chosen footprint cells. The first loop clears bit 0x40 of every tile word, so the words are set by the
    // floodMark stand-in (cur.marks: {call number: [word index]}), which a tile word with bit 0x20 / 0x400 in the window triggers.
    // v = clamp(table, cnt + 1, 99); n = v + size - 1; cells (dr, dc) of the footprint of the record at (r, c).
    const nOf = function (t, cnt, tab) {
      let v = tab, lo = cnt + 1;
      if (v < lo) v = lo;
      if (v > 99 && 99 >= lo) v = 99;
      return v + size(t) - 1;
    };
    const cell = function (r, c, dr, dc) { return (c + dc) + 50 * (r + dr); };
    [0, 1, 2].forEach(function (mode) {
      const t = 0xa, r = 12, c = 14, cnt = 1, tab = 2;
      const n = nOf(t, cnt, tab);
      const idx = [cell(r, c, n - 1, n - 1), cell(r, c, n - 1, n), cell(r, c, n, n - 1)][mode];   // last cell; one column outside; one row outside
      rows.push(mk({ w: 1, x: 0, y: 0, tiles: 'sparse', sparse: { 0: 0x20 }, marks: { 0: [idx] }, recs: [{ i: 1, t: t, r: r, c: c, cnt: cnt, f7: 0xff }], cnt: { 0xa: tab }, z: 5 }));
    });
    rows.push(mk({ w: 1, x: 0, y: 0, tiles: 'sparse', sparse: { 0: 0x20 }, marks: { 0: [cell(12, 14, 0, 0)] }, recs: [{ i: 1, t: 0xa, r: 12, c: 14, cnt: 1, f7: 0xff }], cnt: { 0xa: 2 }, z: 5 }));   // first cell
    rows.push(mk({ w: 1, x: 0, y: 0, tiles: 'sparse', sparse: { 0: 0x20 }, marks: { 0: [cell(12, 14, 1, 0)] }, recs: [{ i: 1, t: 0xa, r: 12, c: 14, cnt: 1, f7: 0xff }], cnt: { 0xa: 2 }, z: 5 }));   // second row, first column
    rows.push(mk({ w: 1, x: 0, y: 0, tiles: 'sparse', sparse: { 0: 0x20 }, marks: { 0: [cell(12, 13, 0, 0), cell(11, 14, 0, 0)] }, recs: [{ i: 1, t: 0xa, r: 12, c: 14, cnt: 1, f7: 0xff }], cnt: { 0xa: 2 }, z: 5 }));   // just left of / above the footprint
    // every object of the six-object set with a mark at its own top-left / last cell
    const marks = [];
    six.forEach(function (rc, k) {
      const tab = 2 + rc.t, n = rc.t === 7 ? size(7) : nOf(rc.t, rc.cnt, tab);
      if (k % 3 === 0) marks.push(cell(rc.r, rc.c, 0, 0));
      if (k % 3 === 1) marks.push(cell(rc.r, rc.c, n - 1, n - 1));
    });
    rows.push(mk({ w: 1, x: 0, y: 0, tiles: 'sparse', sparse: { 0: 0x20 }, marks: { 0: marks }, recs: six, z: 5 }));
    rows.push(mk({ w: 0, x: 3, y: 4, tiles: 'zero', recs: [{ i: 1, t: 5, f7: 0xff }, { i: 2, t: 0xa, r: 6, c: 6, cnt: 1, f7: 0xff }, { i: 3, t: 0x6, r: 8, c: 8, cnt: 0, f7: 0x00 }], z: 5, fl: 0 }));   // type 5 untouched, f7 bit 0x40 cleared elsewhere
    // window + objects together, flood marks on the footprint words too
    rows.push(mk({ w: 4, x: 6, y: 7, tiles: 'rand', seed: 9, recs: six, z: 3, fl: 0x1000000 }));
    return rows;
  }
  function applyRB(fx, row) {
    let s = row.seed || 0;
    const tb = new Uint8Array(5000);
    for (let i = 0; i < 2500; i++) {
      let w = 0;
      if (row.tiles === 'all40') w = 0x0040;
      else if (row.tiles === 'all420') w = 0x0420;
      else if (row.tiles === 'rand') { s = lcg(s); w = (s >>> 12) & 0xffff; }
      else if (row.tiles === 'sparse') w = row.sparse[i] || 0;
      tb[2 * i] = w & 0xff; tb[2 * i + 1] = (w >>> 8) & 0xff;
    }
    ptr(TILEB).writeByteArray(tb.buffer);
    fillMem(RECB, 0x1000, 0);
    const recs = [{ i: 0, t: row.t0 || 0, r: row.y, c: row.x, cnt: 0, f7: 0xff }].concat(row.recs);
    recs.forEach(function (rc) {
      const a = RECB + 16 * rc.i;
      wr16(a, rc.t); wr16(a + 2, rc.r === undefined ? 0 : rc.r); wr16(a + 4, rc.c === undefined ? 0 : rc.c);
      wr8(a + 7, rc.f7 === undefined ? 0xff : rc.f7); wr32(a + 8, rc.cnt || 0);
      wr16(a + 0xc, 0x7777);
    });
    for (let t = 0; t < 17; t++) wr32(CNTB + 4 * t, row.cnt[t] !== undefined ? row.cnt[t] : 2 + t);
    wr8(0x004c27ec, row.w);
    wr32(0x00822c88, row.z);
    wr32(0x0059e7b8, row.fl);
  }
  const RB_N = 25;
  F.c3dy_rb = function () {
    const fx = { clog: newClog(0x1000), clogCap: 0x1000 };
    freezeNow();
    saveRange(TILEB, 5000); saveRange(RECB, 0x1000); saveRange(CNTB, 0x4c);
    saveRange(0x004c27ec, 4); saveRange(0x00822c88, 4); saveRange(0x0059e7b8, 4);
    const rows = rbRows();
    if (rows.length !== RB_N) throw new Error('c3dy_rb: row count ' + rows.length);
    rowsOn(fx, rows.map(function (r) { return { a: 0, marks: r.marks, on: function (f) { applyRB(f, r); } }; }));
    calleeStand(fx, 0, 0x0042f120, 2, 'default', function (n) {
      if (cur.marks && cur.marks[n]) cur.marks[n].forEach(function (idx) { const a = TILEB + 2 * idx; ptr(a).writeU8(ptr(a).readU8() | 0x40); });
      return (0xf100 + n) >>> 0;
    });
    return fx;
  };

  // ---- scanNearbyGolfers (g): the 256-entry table at 0x005736b0 (stride 0x24; dword +8 == -1 marks a free entry)
  const NTAB = 0x005736b0, GREC = 0x005794c0;
  function snRows() {
    const free = function (list) { const a = new Array(256).fill(true); list.forEach(function (i) { a[i] = false; }); return a; };
    const act = function (idxs) { const a = new Array(256).fill(true); idxs.forEach(function (i) { a[i] = false; }); return a; };
    const rows = [];
    // free[i] true = free entry. mk: golfer 3 unless set
    const base = { g: 3, free: act([]), flag: 0, z: 100, date: 0x1234, gx: 1000, gy: 2000, f94: 7, f98: -8, f9c: 51, fa0: -50, ex: 0 };
    const mk = function (o) { return Object.assign({}, base, o); };
    rows.push(mk({}));                                                                   // all free: entry 0 filled
    rows.push(mk({ free: act([0, 1, 2]), flag: 1 }));                                    // three busy entries, flag bit 0 set: emit once
    rows.push(mk({ free: act([0, 1, 2]), flag: 0 }));
    rows.push(mk({ free: act([5, 9]), g: 5, gx: -33, gy: 77, f9c: -51, fa0: 50, flag: 1 }));
    rows.push(mk({ free: act([0, 1, 2, 3, 4, 5, 6, 7]), flag: 0xfe, fa0: 7, f9c: 1 }));   // flag byte without bit 0
    rows.push(mk({ free: act([0, 1]), flag: 3, date: 0x7fffffff, fa0: -7, f9c: -1, gx: 5, gy: 6 }));
    rows.push(mk({ free: act([...Array(256).keys()]), flag: 0 }));                        // none free: returns 256
    rows.push(mk({ free: act([...Array(255).keys()]), flag: 1 }));                        // only the last free
    rows.push(mk({ free: act([...Array(250).keys()].map(function (i) { return i + 6; })), g: 2, flag: 0 }));   // first six free
    rows.push(mk({ free: act([0]), g: 7, gx: 0, gy: 0, f94: -1, f98: 0, f9c: 0, fa0: 0, z: 0 }));
    rows.push(mk({ free: act([1, 2]), z: 3, gx: 12345, gy: -12345, fa0: 3, f9c: 3 }));
    rows.push(mk({ free: act([]), g: 255, z: 0x7fff, fa0: 0x7fffffff, f9c: 0x7fffffff, flag: 1 }));
    return rows;
  }
  function applySN(fx, row) {
    fillMem(NTAB, 0x2400, 0x6b);
    for (let i = 0; i < 256; i++) {
      const e = NTAB + 0x24 * i;
      if (row.free[i]) { wr32(e, 0x1111 + i); wr32(e + 4, 0x2222 + i); wr32(e + 8, 0xffffffff); }
      else { wr32(e, 700 + 37 * i); wr32(e + 4, 900 - 29 * i); wr32(e + 8, 700 + 37 * i); }
    }
    const s = GREC + (row.g << 8);
    wr32(s + 0xcc, row.gx); wr32(s + 0xd0, row.gy); wr32(s + 0xd4, row.f94); wr32(s + 0xd8, row.f98);
    wr32(s + 0xdc, row.f9c); wr32(s + 0xe0, row.fa0);
    wr32(0x0056c7b4, row.flag); wr32(0x00822c88, row.z); wr32(0x00834170, row.date);
  }
  const SN_N = 12;
  function snScene(real) {
    return function () {
      const fx = { clog: newClog(0x8000), clogCap: 0x8000 };
      freezeNow();
      saveRange(NTAB, 0x2400); saveRange(0x0056c7b4, 4); saveRange(0x00822c88, 4); saveRange(0x00834170, 4); saveRange(0x00822d9c, 4);
      [2, 3, 5, 7, 255].forEach(function (g) { saveRange(GREC + (g << 8) + 0xcc, 0x18); });
      const rows = snRows();
      if (rows.length !== SN_N) throw new Error('c3dy_sn: row count ' + rows.length);
      rowsOn(fx, rows.map(function (r, i) { return { a: r.g, g: { [0x00822d9c]: (0x600d + Math.imul(i + 1, 0x9e3779b1)) >>> 0 }, on: function (f) { applySN(f, r); } }; }));
      calleeStand(fx, 0, 0x00467a00, 3, 'default', function (n) { return (0xe700 + n) >>> 0; });
      if (!real) {
        calleeStand(fx, 1, 0x0045c1e0, 2, 'thiscall', function (n, a) { return (0x1357 + Math.imul(n, 0x3d) + (a[1] & 0xff)) >>> 0; });
        calleeStand(fx, 2, 0x0040acd0, 2, 'default', function (n, a) { return (n * 97 + (a[0] ^ a[1])) >>> 0; });
        calleeStand(fx, 3, 0x00407000, 3, 'default', function (n) { return (n % 5 === 3) ? 1 : 0; });
      }
      return fx;
    };
  }
  F.c3dy_sn = snScene(false);
  F.c3dy_snr = snScene(true);

  // ---- scanShotArea (g): the stand-in key answers every callee but clamp
  const ZT = 0x0059fc60, ATTR = 0x00578372;
  function ssRows() {
    const rows = [];
    // z = the golfer record index, stored as the short at 0x0059fc60 + g * 904. Callee answers: dist, probe (40c4b0), tt = the
    // three 40bfa0 results [T, U, W], rng = Random::range results, hole = holeQuadrants results; attr = {tileIndex: signed byte}
    const base = { g: 4, zoom: 4, e: 0xff, z: 2, cur: 2, flags: 0, b: 7, sbyte: 0, dist: 200, probe: 0x30, tt: [1, 2, 3], attr: { 1: 2, 2: -1, 3: 1 },
      rng: [0], hole: [0], z88: 3, fl: 0, sum: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10], c: 1, p: [1000, 2000, 1100, 1900, 1010, 2010] };
    const mk = function (o) { return Object.assign({}, base, o); };
    rows.push(mk({ zoom: 3 }));                                                          // zoom below 4: returns
    rows.push(mk({ zoom: -1 }));
    rows.push(mk({ e: 2 }));                                                             // z == low byte of [0x4c2e0c]
    rows.push(mk({ flags: 0x20, cur: 9 }));                                              // top bits 0x20, z != cur: returns
    rows.push(mk({ flags: 0x3f, cur: 9 }));                                              // top bits 0x20 after the mask: returns
    rows.push(mk({ flags: 0x20, cur: 2, attr: { 1: 2, 2: 4, 3: 1 }, rng: [0] }));        // 0x20 and z == cur continues: attr(U) > 0, draw 0 < attr * S: narrate 1
    rows.push(mk({ attr: { 1: 2, 2: 4, 3: 1 }, cur: 9 }));                               // attr(U) > 0, z != cur: returns
    rows.push(mk({ attr: { 1: 2, 2: 4, 3: 1 }, rng: [0x7fffffff] }));                    // draw not below attr * S: returns
    rows.push(mk({ attr: { 1: 2, 2: 4, 3: 1 }, rng: [219], sum: [10, 11, 0, 0, 0, 0, 0, 0, 0, 0] }));   // attr 4 * S 21 = 84 vs draw 219: returns
    rows.push(mk({ attr: { 1: 2, 2: 4, 3: 1 }, rng: [83], sum: [10, 11, 0, 0, 0, 0, 0, 0, 0, 0] }));    // draw 83 < 84: narrate
    rows.push(mk({ attr: { 1: -3, 2: 4, 3: 1 }, rng: [0], z88: -3 }));                   // 250 * (attr(T) + 4) / (z88 + 2): a negative divisor
    // attr(U) <= 0 branch, z != cur (no v adjustments): b and k variants
    [[0, 0], [4, 5], [5, 7], [9, 3], [10, 0x19], [11, 0x1a], [127, 0x40], [-5, 0x30]].forEach(function (bk, i) {
      rows.push(mk({ cur: 9, sbyte: bk[0], probe: bk[1], dist: 1000 + 700 * i }));
    });
    // k == 0 halves v; k > 0x19 with sbyte > 0 and attr(W) <= 0: k = 0x19, v = 200 / sbyte
    rows.push(mk({ cur: 9, sbyte: 6, probe: 0, dist: 3 }));
    rows.push(mk({ cur: 9, sbyte: 6, probe: 0x20, tt: [1, 2, 3], attr: { 1: 2, 2: -1, 3: 1 }, dist: 5000 }));   // attr(W) > 0: unchanged
    rows.push(mk({ cur: 9, sbyte: 6, probe: 0x20, tt: [1, 2, 3], attr: { 1: 2, 2: -1, 3: -2 }, dist: 5000 }));  // attr(W) <= 0: k = 0x19, v = 200 / 6
    rows.push(mk({ cur: 9, sbyte: 0, probe: 0x20, tt: [1, 2, 3], attr: { 1: 2, 2: -1, 3: -2 }, dist: 5000 }));  // sbyte 0: no change
    rows.push(mk({ cur: 9, sbyte: -4, probe: 0x20, tt: [1, 2, 3], attr: { 1: 2, 2: -1, 3: -2 }, dist: 5000 })); // sbyte < 0: no change
    rows.push(mk({ cur: 9, sbyte: 3, probe: 0x1a, attr: { 1: 2, 2: 0, 3: 0 }, dist: 77 }));       // attr(U) == 0 takes the same branch
    // z == cur: attr(T) > 0 adds a share of v (clamped to 3), then the draw and holeQuadrants adjust
    [[1, 0], [2, 0], [3, 1], [5, 0], [127, 1]].forEach(function (ah, i) {
      rows.push(mk({ sbyte: 6, probe: 0x1f, attr: { 1: ah[0], 2: -1, 3: 1 }, rng: [i * 40], hole: [ah[1]], dist: 2000 + 900 * i, z88: i * 7, sum: [i + 1, 2, 3, 4, 5, 6, 7, 8, 9, 10] }));
    });
    rows.push(mk({ sbyte: 2, probe: 0, attr: { 1: -2, 2: -1, 3: 1 }, rng: [123], hole: [1], dist: 3000, z88: 4 }));          // attr(T) <= 0: no share
    rows.push(mk({ sbyte: 12, probe: 0x22, attr: { 1: 3, 2: -1, 3: 1 }, rng: [0xffff], hole: [0], dist: 90, z88: 0x7fff }));   // draw 0xffff, large z88
    // D versus (k + 1) * v and the flag bit 0x10 route (z != cur)
    rows.push(mk({ cur: 9, sbyte: 12, probe: 0, dist: 100000 }));                        // D large: narrate 0
    rows.push(mk({ cur: 9, sbyte: 12, probe: 0, dist: 0, fl: 0x10 }));                   // D small, bit 0x10, z != cur, cur != -1: returns
    rows.push(mk({ cur: -1, sbyte: 12, probe: 0, dist: 0, fl: 0x10 }));                  // cur == -1 with bit 0x10: narrate 0
    rows.push(mk({ cur: 9, sbyte: 12, probe: 0, dist: 0, fl: 0x00 }));                   // D small, no flag: returns
    rows.push(mk({ cur: 2, sbyte: 12, probe: 0, dist: 0, fl: 0x10, attr: { 1: -2, 2: -1, 3: 1 }, rng: [10], hole: [0] }));   // z == cur with the bit
    rows.push(mk({ cur: 9, sbyte: 12, probe: 0, dist: -3, fl: 0xffffffef }));            // negative distance, bit 0x10 clear but others set
    rows.push(mk({ cur: -1, sbyte: 12, probe: 0, dist: 0, fl: 0xffffffff }));            // all bits, cur == -1: narrate 0 clears only 0x10
    rows.push(mk({ e: 0x182, cur: 9, sbyte: 12, probe: 0, dist: 0, fl: 0x10 }));         // low byte of [0x4c2e0c] 0x82 is not z (a mask to 7 bits would make it z)
    // thresholds of the first draw: attr(U) * S against the draw & 0xffff (a draw above 0x7fff, an exact tie)
    rows.push(mk({ attr: { 1: 2, 2: 127, 3: 1 }, rng: [0xf000], sum: [26, 26, 26, 26, 26, 26, 26, 26, 26, 26] }));      // 127 * 260 = 33020 > 0xf000 & 0x7fff, <= 0xf000
    rows.push(mk({ attr: { 1: 2, 2: 127, 3: 1 }, rng: [0x7000], sum: [26, 26, 26, 26, 26, 26, 26, 26, 26, 26] }));      // 0x7000 < 33020: narrate
    rows.push(mk({ attr: { 1: 2, 2: 4, 3: 1 }, rng: [84], sum: [10, 11, 0, 0, 0, 0, 0, 0, 0, 0] }));                    // tie 84 == 4 * 21: returns
    rows.push(mk({ attr: { 1: 2, 2: 4, 3: 1 }, rng: [85], sum: [10, 11, 0, 0, 0, 0, 0, 0, 0, 0] }));
    // b == 1 with k above 0x19 and attr(W) <= 0 (k = 0x19, v = 200)
    rows.push(mk({ cur: 9, sbyte: 1, probe: 0x30, attr: { 1: 2, 2: -1, 3: -2 }, dist: 5000 }));
    rows.push(mk({ cur: 9, sbyte: 1, probe: 0x1a, attr: { 1: 2, 2: -1, 3: 0 }, dist: 6000 }));
    // z == cur sweep: share of v (clamp 3, multiple of 6 products), draw size, sign of z88
    [2, 12].forEach(function (sb) {
      [1, 3, 127].forEach(function (at) {
        [0x20, 0xf000].forEach(function (rn) {
          [3, -100].forEach(function (z88) {
            rows.push(mk({ sbyte: sb, probe: 0x1f, attr: { 1: at, 2: -1, 3: 1 }, rng: [rn], hole: [0], z88: z88, dist: 100 }));
          });
        });
      });
    });
    // Edge rows: for every row that reaches the final test, three more with the distance chosen so that D = (k + 1) * v - 1, (k + 1) * v
    // and (k + 1) * v + 1 (v and k from this model of the body; the A/B decides whether the body agrees with the original).
    const attrOf = function (r, i) { const v = r.attr[i]; return v === undefined ? 0 : (v << 24) >> 24; };
    const model = function (r) {
      if (r.zoom < 4) return null;
      if ((r.e & 0xff) === r.z) return null;
      if (((r.flags & 0xe0) === 0x20) && r.z !== r.cur) return null;
      const T = r.tt[0], U = r.tt[1], W = r.tt[2];
      if (attrOf(r, U) > 0) return null;
      const b = (r.sbyte << 24) >> 24;
      let k = r.probe | 0, v = 0x14;
      if (b < 5) v = 0x19;
      if (b < 10) v += 5;
      if (b > 10) v -= 5;
      if (k === 0) v = (v - (v >> 31)) >> 1;
      else if (k > 0x19 && b > 0 && !(attrOf(r, W) > 0)) { k = 0x19; v = Math.trunc(200 / b); }
      if (r.z === r.cur) {
        const at = attrOf(r, T);
        if (at > 0) {
          const cl = Math.min(Math.max(at, 0), 3);
          let hi = Math.floor((-715827883 * (cl * v)) / 4294967296);
          hi += hi < 0 ? 1 : 0;
          v += hi;
        }
        let x = Math.imul(((r.rng[0] || 0) & 0xffff) + 4 * r.z88 + 5, v);
        x += (x >> 31) & 0x1f;
        v = x >> 5;
        if (r.hole[0]) v = (v - (v >> 31)) >> 1;
      }
      return { k: k, v: v };
    };
    const dOf = function (d) { const v25 = Math.imul(d, 25); return (v25 + ((v25 >> 31) & 0x3ff)) >> 10; };
    const distFor = function (t) { const d0 = Math.round(t * 1024 / 25); for (let d = d0 - 4; d <= d0 + 4; d++) if (dOf(d) === t) return d; return null; };
    rows.slice().forEach(function (r) {
      const m = model(r);
      if (!m) return;
      const th = (m.k + 1) * m.v;
      [th - 1, th, th + 1].forEach(function (t) {
        const d = distFor(t);
        if (d !== null) rows.push(Object.assign({}, r, { dist: d, fl: 0 }));
      });
    });
    // D = (25 d + 1023) >> 10 for d = -983 is -23 (-24 with a 0x3fe mask): a row whose threshold (k + 1) * v is -24
    let found = false;
    [12, 6, 2].forEach(function (sb) {
      for (let rn = 0; rn < 4000 && !found; rn++) {
        const r = mk({ sbyte: sb, probe: -2, attr: { 1: -1, 2: -1, 3: 1 }, rng: [rn], hole: [0], z88: 0, dist: -983, fl: 0 });
        const m = model(r);
        if (m && (m.k + 1) * m.v === -24) { rows.push(r); found = true; }
      }
    });
    // the exact multiple of 1024 case of the rounding in D = (25 d + ((25 d >> 31) & 0x3ff)) >> 10: d = -983 (25 d = -24575)
    rows.push(mk({ cur: 9, sbyte: 12, probe: 0, dist: -983, fl: 0 }));
    return rows;
  }
  // golfer record z: absolute addresses are 0x005794b8 + (z << 8) + field (fields as the disassembly names them)
  const GB = 0x005794b8;
  function applySS(fx, row) {
    const sa = GB + (row.z << 8);
    wr16(ZT + row.g * 904, row.z);
    wr32(0x004c2844, row.zoom); wr32(0x004c2e0c, row.e); wr32(0x005a59f8, row.cur); wr32(0x0056c7b4, row.fl); wr32(0x00822c88, row.z88);
    fillMem(sa, 0x100, 0x6d);
    wr8(sa + 0x18, row.flags);                                                          // 0x005794d0
    wr8(sa + 0x1b, row.sbyte);                                                          // 0x005794d3
    wr8(sa + 0x21, row.c);                                                              // 0x005794d9
    for (let i = 0; i < 10; i++) wr8(sa + 0xf0 + i, row.sum[i]);                        // 0x005795a8..
    wr32(sa + 0xc4, row.p[0]); wr32(sa + 0xc8, row.p[1]);                               // 0x0057957c / 0x00579580: P0, P1
    wr32(sa + 0xd4, row.p[2]); wr32(sa + 0xd8, row.p[3]);                               // 0x0057958c / 0x00579590: Q0, Q1
    wr32(sa + 0x0, row.hole0 === undefined ? 0x1111 : row.hole0);                       // 0x005794b8
    wr32(sa + 0x4, row.hole1 === undefined ? 0x2222 : row.hole1);                       // 0x005794bc
    if (row.attrFull) for (let t = 0; t < 64; t++) wr8(ATTR + 48 * t, row.attrFull[t]);
    else { for (let t = 0; t < 8; t++) wr8(ATTR + 48 * t, (row.attr[t] === undefined ? 0 : row.attr[t])); }
    wr32(0x00575ac8 + 520 * row.c, 0x1357); wr32(0x00575acc + 520 * row.c, 0x2468);
  }
  const SS_N = 237;
  function ssScene(real) {
    return function () {
      const fx = { clog: newClog(0x1000), clogCap: 0x1000 };
      freezeNow();
      [0x004c2844, 0x004c2e0c, 0x005a59f8, 0x0056c7b4, 0x00822c88, 0x00822d9c].forEach(function (a) { saveRange(a, 4); });
      saveRange(GB + (2 << 8), 0x100);
      saveRange(ZT + 4 * 904, 4);
      for (let t = 0; t < 64; t++) saveRange(ATTR + 48 * t, 1);
      saveRange(0x00575ac8 + 520, 8);
      let rows;
      if (!real) {
        rows = ssRows();
        if (rows.length !== SS_N) throw new Error('c3dy_ss: row count ' + rows.length);
      } else {
        rows = [];
        let s = 0x3c3c;
        for (let i = 0; i < 12; i++) {
          s = lcg(s);
          const attrFull = []; for (let t = 0; t < 64; t++) { s = lcg(s); attrFull.push(((s >>> 20) % 9) - 3); }
          const pick = function (m) { s = lcg(s); return (s >>> 8) % m; };
          rows.push({ g: 4, zoom: 4 + (i & 1), e: 0xff, z: 2, cur: (i % 3 === 0) ? 9 : 2, flags: (i % 4 === 1) ? 0x20 : 0, sbyte: pick(14) - 2, c: 1,
            dist: 0, probe: 0, tt: [], attr: {}, attrFull: attrFull, rng: [], hole: [], z88: pick(40), fl: (i % 5 === 0) ? 0x10 : 0,
            sum: [pick(9), pick(9), pick(9), pick(9), pick(9), pick(9), pick(9), pick(9), pick(9), pick(9)],
            p: [pick(50000), pick(50000), pick(50000), pick(50000)], hole0: pick(50000), hole1: pick(50000),
            seed: (0x7e57 + Math.imul(i + 1, 0x9e3779b1)) >>> 0 });
        }
      }
      rowsOn(fx, rows.map(function (r) {
        const g = r.seed !== undefined ? { [0x00822d9c]: r.seed } : {};
        return { a: r.g, g: g, dist: r.dist, probe: r.probe, tt: r.tt, rng: r.rng, hole: r.hole, on: function (f) { applySS(f, r); } };
      }));
      calleeStand(fx, 0, 0x00407e00, 4, 'default', function (n) { return (0x7e00 + n) >>> 0; });
      if (!real) {
        calleeStand(fx, 1, 0x0040acd0, 2, 'default', function () { return cur.dist >>> 0; });
        calleeStand(fx, 2, 0x0040c4b0, 4, 'default', function () { return cur.probe >>> 0; });
        calleeStand(fx, 3, 0x0040bfa0, 2, 'default', seq('tt', 0));
        calleeStand(fx, 4, 0x0045c1e0, 2, 'thiscall', seq('rng', 0));
        calleeStand(fx, 5, 0x00407000, 3, 'default', seq('hole', 0));
      }
      return fx;
    };
  }
  F.c3dy_ss = ssScene(false);
  F.c3dy_ssr = ssScene(true);

  // ---- storyText (a, i, g)
  const BUF = 0x0051a068;
  function stRows() {
    const rows = [];
    const mk = function (o) { return Object.assign({ a: 11, i: 2, g: 3, fl: 0x100000, key: 7, w: 5, wkey: 9, txt: 'alpha\nbeta\ngamma' }, o); };
    rows.push(mk({}));                                                                  // flag set: newlines become NULs, returns 'a'
    rows.push(mk({ key: 0 }));                                                          // flag set, key 0: returns 0 without a call
    rows.push(mk({ txt: '' }));
    rows.push(mk({ txt: '\nabc' }));                                                    // the first character is a newline
    rows.push(mk({ fl: 0x300000, txt: 'one\ntwo\n\nthree\n', a: 12, i: 4 }));
    rows.push(mk({ fl: 0, txt: 'first\nsecond\nthird' }));                              // flag clear: the text after the first newline moves up
    rows.push(mk({ fl: 0, wkey: 0, key: 5 }));                                          // flag clear: the key comes from the other record, 0
    rows.push(mk({ fl: 0, txt: 'no newline here' }));
    rows.push(mk({ fl: 0, txt: '\nstart' }));
    rows.push(mk({ fl: 0, txt: 'x\n' }));
    rows.push(mk({ fl: 0, txt: 'z' }));
    rows.push(mk({ fl: 0, txt: 'A'.repeat(100) + '\n' + 'BCDEFGHIJ', a: 99, i: 7 }));
    rows.push(mk({ fl: 0, g: 4, txt: 'p\nq' }));                                        // g = 4 -> 5 in the call
    rows.push(mk({ fl: 0x100000, g: 4, txt: 'p\nq' }));                                 // flag set, g = 4 stays
    rows.push(mk({ fl: 0, w: -1, wkey: 3, txt: 'neg\nrec' }));                          // a negative record word
    rows.push(mk({ fl: 0, w: 5, i: 0, txt: 'a\nbc\nd', a: 0 }));
    rows.push(mk({ fl: 0, txt: 'abc\nd'.repeat(2) + '\n' + 'tail'.repeat(5), a: 3, i: 9 }));
    rows.push(mk({ fl: 0xffffffff, txt: 'x\ny\nz', a: 2, i: 1, key: 255 }));           // all flag bits: the 0x100000 bit counts
    rows.push(mk({ fl: 0xffefffff, txt: 'x\ny\nz', a: 2, i: 1, key: 255, wkey: 200 })); // every bit but 0x100000
    return rows;
  }
  const ST_N = 19;
  F.c3dy_st = function () {
    const fx = { clog: newClog(), clogCap: CLOG };
    freezeNow();
    [3, 4, 5].forEach(function (g) { saveRange(GB + (g << 8), 0x100); });
    saveRange(0x00579402, 0x20);
    saveRange(BUF, 0x200);
    const rows = stRows();
    if (rows.length !== ST_N) throw new Error('c3dy_st: row count ' + rows.length);
    rowsOn(fx, rows.map(function (r) {
      return { a: r.a, b: r.i, c: r.g, on: function (f) {
        fillMem(BUF, 0x200, 0x55);
        [3, 4, 5].forEach(function (g) { fillMem(GB + (g << 8), 0x100, 0x31); });
        fillMem(0x00579402, 0x20, 0x42);
        const rec = GB + (r.g << 8);
        wr32(rec + 0x10, r.fl);                                                         // 0x005794c8
        wr16(rec + 0xa2, r.w);                                                          // 0x0057955a
        wr8(rec + 0x4a + r.i, r.key);                                                   // 0x00579502 + i
        wr8(((r.w << 8) + GB + 0x4a + r.i) >>> 0, r.wkey);                              // the record the word names
        cur = r;
      }, txt: r.txt };
    }));
    calleeStand(fx, 0, 0x004669f0, 4, 'default', function () {
      const t = cur.txt;
      for (let k = 0; k < t.length; k++) ptr(BUF).add(k).writeU8(t.charCodeAt(k));
      ptr(BUF).add(t.length).writeU8(0);
      return 0;
    });
    return fx;
  };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
