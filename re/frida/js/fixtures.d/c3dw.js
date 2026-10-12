// Fixtures for batch c3dw (round 28, 2026-10-11): golf_clean.exe rows earlier rounds dropped for heap use, imports, live
// globals or file I/O. Reimplementations: shim/src/re/c3dw.cpp. Registry: re/frida/registry.d/c3dw.py. Purposes and per-jcc
// tables: log/c3/c3dw_purpose.md.
//
// Everything a vector touches is OURS (one arena per fixture call, the state regions of the keys). Keys that read live
// globals freeze the game's first thread (re/frida/js/freeze.js), point the globals at our objects and let the flush restore
// them. Machine-code recording stubs (c3dq's builder, extended with call-indexed result sequences) sit in a private rwx page:
// no JavaScript runs inside a vector. Per-vector variation of live globals uses c3dr's getter trick: diff_hook resolves a
// "$rN" argument at each vector BEFORE the state snapshot, so the property getter of fx.rN sets that row's globals.
(function () {
  const F = {};
  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function alloc(n) {
    const p = __keepAlloc(n);
    for (let i = 0; i < n; i += 4) p.add(i).writeU32(0);
    return p;
  }
  // The negative-control build keeps its wrong bodies dormant until its exported NegCtl switch is set; in the normal build the address
  // is unmapped and the write is skipped.
  function negOn() {
    __freeze.negOn();   // every exported <Batch>NegCtl switch (re/frida/js/freeze.js)
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
    const size = Process.pageSize * 96;
    const base = __keepAlloc(size);
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3dw: code arena full');
      return p;
    };
  }
  const REC_SIZE = 0x2000;
  function newRec(size) {
    const rec = alloc(size || REC_SIZE);
    rec.writeU32(0x10);
    return rec;
  }

  // Recording stub: thiscall, n stack arguments (callee pops), appends {tag, ecx, args...} at the recorder cursor ($rec+0,
  // entries from $rec+0x10). Result: undefined -> cursor ^ tag; a number -> that constant; {tbl, cell} -> the entry of the
  // sequence table indexed by the call number held in the counter cell (the last entry repeats); the cell lives in a state
  // region so both arms start from call 0.
  function stubBytes(rec, tag, n, ret, cdecl) {
    const r = u32(rec.toUInt32());
    const b = [0x53];                                                  // push ebx
    b.push(0x8B, 0x1D, ...r);                                          // mov ebx, [REC]
    b.push(0x81, 0xC3, ...r);                                          // add ebx, REC
    b.push(0xC7, 0x03, ...u32(tag));                                   // mov dword [ebx], tag
    if (cdecl) b.push(0xC7, 0x43, 0x04, ...u32(0xC0C0C0C0));            // cdecl stub: ecx is not an argument
    else b.push(0x89, 0x4B, 0x04);                                     // mov [ebx+4], ecx
    let off = 8;
    for (let i = 0; i < n; i++) {
      b.push(0x8B, 0x44, 0x24, 8 + 4 * i, 0x89, 0x43, off); off += 4;  // mov eax,[esp+8+4i]; mov [ebx+off],eax
    }
    b.push(0xA1, ...u32(0x0083ad4c), 0x89, 0x43, off); off += 4;       // mov eax,[0x83ad4c]; mov [ebx+off],eax (the pump mode word)
    b.push(0x81, 0x05, ...r, ...u32(off));                             // add dword [REC], off
    if (ret === undefined) b.push(0xA1, ...r, 0x35, ...u32(tag));      // mov eax, [REC]; xor eax, tag
    else if (typeof ret === 'number') b.push(0xB8, ...u32(ret));       // mov eax, ret
    else if (ret.mem) b.push(0xA1, ...u32(ret.mem.toUInt32()));       // mov eax, [mem] (a cell a row sets)
    else {
      const c = u32(ret.cell.toUInt32()), t = u32(ret.tbl.toUInt32());
      b.push(0xA1, ...c);                                              // mov eax, [cell]
      b.push(0xFF, 0x05, ...c);                                        // inc dword [cell]
      b.push(0x83, 0xF8, ret.n - 1);                                   // cmp eax, n-1
      b.push(0x76, 0x05);                                              // jbe +5
      b.push(0xB8, ...u32(ret.n - 1));                                 // mov eax, n-1
      b.push(0x8B, 0x04, 0x85, ...t);                                  // mov eax, [tbl + eax*4]
    }
    b.push(0x5B);                                                      // pop ebx
    if (n > 0 && !cdecl) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }

  // A recording display: 0x100 slots. rets: {offset: number | [sequence]}; args: {offset: stack argument count}.
  // cnt (state region, 0x80 bytes) holds the call counters of the sequence slots.
  function makeDisplay(emit, rec, cnt, rets, args, size) {
    const dvt = alloc(0x400);
    let cell = 0;
    for (let i = 0; i < 0x100; i++) {
      const off = i * 4;
      let ret;
      const rv = rets && rets[off];
      if (Array.isArray(rv)) {
        const tbl = alloc(4 * rv.length);
        rv.forEach(function (v, k) { tbl.add(4 * k).writeU32(v >>> 0); });
        ret = { tbl: tbl, cell: cnt.add(4 * cell++), n: rv.length };
      } else if (rv !== undefined && typeof rv === 'object') ret = rv;
      else if (rv !== undefined) ret = rv >>> 0;
      const nargs = (args && args[off]) || 0;
      dvt.add(off).writePointer(emit(stubBytes(rec, 0xD15A0000 + off, nargs, ret)));
    }
    const disp = alloc(size || 0x40);
    disp.writePointer(dvt);
    disp.add(4).writeU32(0xD15D15D1);
    return disp;
  }

  // Scene: a frozen first thread, our display at [0x0083ad50], no net object, no movies, and the junk globals of the row.
  // o: disp (false = NULL display), rets / args (display slot results / stack argument counts), glob {addr: value} saved
  // and set once, rows [{addr: value}] (getters fx.r0 .. rN apply a row: the vector's setup).
  function scene(o) {
    const emit = codeArena();
    const rec = newRec();
    const cnt = alloc(0x80);
    const fx = { rec: rec, cnt: cnt, obj: rec, clog: newClog() };
    fx.cell = alloc(0x40);
    const rets = typeof o.rets === 'function' ? o.rets(emit, rec, cnt, fx) : o.rets;
    fx.disp = makeDisplay(emit, rec, cnt, rets, o.args);
    freezeNow();
    __freeze.setU32(ptr('0x0083ad50'), o.disp === false ? 0 : __recDisplay.guard(fx.disp).toUInt32());
    __freeze.save(ptr('0x008400b0'), 0x18);
    ptr('0x008400b8').writePointer(ptr(0));
    ptr('0x008400b0').writeU32(0);
    __freeze.setU32(ptr('0x008406e4'), 0);
    const glob = o.glob || {};
    Object.keys(glob).forEach(function (a) { __freeze.save(ptr(+a), 4); ptr(+a).writeU32(glob[a] >>> 0); });
    if (o.rows) rowsOn(fx, typeof o.rows === 'function' ? o.rows(fx) : o.rows, o.rets2 ? o.rets2(fx) : undefined);
    if (o.after) o.after(fx);
    return fx;
  }
  function sceneKey(o) { return function () { return scene(o); }; }


  // ---- row values: a numeric key writes a dword, 'b<addr>' a byte (the key is the address as a decimal string)
  function putRow(row) {
    Object.keys(row).forEach(function (a) {
      if (a.charAt(0) === 'b') ptr(+a.slice(1)).writeU8(row[a] & 0xff); else ptr(+a).writeU32(row[a] >>> 0);
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
  function rowsOn(fx, rows, rets, onRow) {
    saveRows(rows);
    rows.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () { putRow(row); if (onRow) onRow(i); return rets ? rets[i] : 0; } });
    });
  }

  // ---- a function of the exe replaced for one key on the A/B thread only (the crt stand-in's pattern, c3dl): impl runs on
  // the A/B thread and every call is appended to the call log fx.clog (word 0 = cursor, entries {tag, args...} from +0x10;
  // word 1 = call count of stub k at +4+4k, up to 16 stubs). Other threads reach the real function.
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x400;
  function newClog() { const l = alloc(CLOG); l.writeU32(0x50); return l; }
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
          for (let k = 0; k < 4 * d.str; k++) {
            const c = end ? 0 : p.add(k).readU8();
            if (c === 0) end = true;
            words[k >> 2] |= c << (8 * (k & 3));
          }
          words.forEach(function (x) { vals.push(x >>> 0); });
        } else for (let w = 0; w < d; w++) vals.push(ptr(raw[ix]).add(4 * w).readU32());
      });
      const cur = log.readU32();
      const need = 8 + 4 * vals.length;
      if (cur + need <= CLOG) {
        log.add(cur).writeU32(va >>> 0);
        log.add(cur + 4).writeU32(vals.length);
        vals.forEach(function (v, i) { log.add(cur + 8 + 4 * i).writeU32(v >>> 0); });
        log.writeU32(cur + need);
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

  // ---- heap: the exe's CRT free 0x004a5007 and operator delete 0x004a4ffc only record (heapstandin.js)
  const HEAP_FREE = [0xa5007, 0xa4ffc];
  function heapOn(fx, arenaSize) {
    const base = Process.getModuleByName('golf_clean.exe').base;
    HEAP_FREE.concat([0xa504f, 0xa57f1, 0xa6980]).forEach(function (r) { try { Interceptor.revert(base.add(r)); } catch (e) { /* not replaced */ } });
    Interceptor.flush();
    const h = __heapStandIn({ module: 'golf_clean.exe', free: HEAP_FREE, arenaSize: arenaSize || 0x4000, exhaust: 'null' });
    fx.arena = h.arena;
    fx.freelog = h.log;
    return h;
  }
  const TER = '0x00820ed0';

  const HIDE = 0x12c, SHOW = 0x128, REDRAW = 0x134, TICK = 0x13c;
  const G_AD4C = 0x0083ad4c, G_9C = 0x00822b9c, G_98 = 0x00822b98, G_68 = 0x00822d68, G_74 = 0x00822d74, G_D3834 = 0x004d3834;
  const J = 0x7e7e7e7e;       // junk word

  // ---- resetScrollText 0x00465560: three globals cleared whatever they held
  const RST = [
    { 0x00822d68: 0, 0x00822b9c: 0, 0x00822b98: 0 }, { 0x00822d68: 1, 0x00822b9c: 0, 0x00822b98: 0 },
    { 0x00822d68: 0, 0x00822b9c: 1, 0x00822b98: 0 }, { 0x00822d68: 0, 0x00822b9c: 0, 0x00822b98: 1 },
    { 0x00822d68: J, 0x00822b9c: J + 1, 0x00822b98: J + 2 }, { 0x00822d68: 0xffffffff, 0x00822b9c: 0x80000000, 0x00822b98: 5 },
    { 0x00822d68: 3, 0x00822b9c: 0, 0x00822b98: 9 }, { 0x00822d68: 0, 0x00822b9c: 0x7fffffff, 0x00822b98: 0x7fffffff },
    { 0x00822d68: 2, 0x00822b9c: 4, 0x00822b98: 0 }, { 0x00822d68: 0x100, 0x00822b9c: 0x200, 0x00822b98: 0x300 },
    { 0x00822d68: 0x55, 0x00822b9c: 0, 0x00822b98: 0 }, { 0x00822d68: 0, 0x00822b9c: 0, 0x00822b98: 0xaa },
  ];
  F.c3dw_rst = sceneKey({ rows: RST });

  // ---- drainAndRedraw 0x0045af00 / flushRedraw 0x0045ae70 / waitRedrawReady 0x0045aed0
  // rows: [822b9c, 822b98, 4d3834]
  const FLAGS = [[0, 0, 0], [1, 0, 0], [0, 1, 0], [5, 7, 0], [0x80000000, 0, 0], [0, 0xffffffff, 0],
                 [0, 0, 1], [3, 0, 1], [0, 2, 1], [1, 1, 1], [0, 0, 0x100], [9, 0, 0x100]];
  const FLAGROWS = FLAGS.map(function (r) { return { 0x00822b9c: r[0], 0x00822b98: r[1], 0x004d3834: r[2] }; });
  // hideScreen (slot +0x12c) results drive pumpInput483c30's loop; redrawAll is slot +0x134
  F.c3dw_drain_h0 = sceneKey({ rows: FLAGROWS, rets: { [HIDE]: [0] } });
  F.c3dw_drain_h1 = sceneKey({ rows: FLAGROWS, rets: { [HIDE]: [1, 0] } });
  F.c3dw_drain_h3 = sceneKey({ rows: FLAGROWS, rets: { [HIDE]: [5, 1, 1, 0] } });
  F.c3dw_flush_h0 = sceneKey({ rows: FLAGROWS, rets: { [HIDE]: [0] } });
  F.c3dw_flush_h2 = sceneKey({ rows: FLAGROWS, rets: { [HIDE]: [1, 1, 0] } });
  // waitRedrawReady: frameTick's slot (+0x13c) drives 0x00483cf0's loop
  F.c3dw_wait_t0 = sceneKey({ rows: FLAGROWS, rets: { [TICK]: [0], [HIDE]: [0] } });
  F.c3dw_wait_t2 = sceneKey({ rows: FLAGROWS, rets: { [TICK]: [1, 1, 0], [HIDE]: [1, 0] } });

  // ---- pumpInput483bd0 0x00483bd0 (showScreen slot +0x128) / pumpInput483c30 0x00483c30 (hideScreen slot +0x12c)
  const AD = [{ 0x0083ad4c: 0 }, { 0x0083ad4c: J }, { 0x0083ad4c: 0x3f }, { 0x0083ad4c: 0x20 }];
  F.c3dw_pi_bd0_s0 = sceneKey({ rows: AD, rets: { [SHOW]: [0] } });
  F.c3dw_pi_bd0_s1 = sceneKey({ rows: AD, rets: { [SHOW]: [1, 0] } });
  F.c3dw_pi_bd0_s3 = sceneKey({ rows: AD, rets: { [SHOW]: [7, 0x80000000, 1, 0] } });
  F.c3dw_pi_c30_s0 = sceneKey({ rows: AD, rets: { [HIDE]: [0], [SHOW]: [0] } });
  F.c3dw_pi_c30_s1 = sceneKey({ rows: AD, rets: { [HIDE]: [1, 0], [SHOW]: [0] } });
  F.c3dw_pi_c30_s3 = sceneKey({ rows: AD, rets: { [HIDE]: [7, 0x80000000, 1, 0], [SHOW]: [0] } });
  // the net globals set: the gfxFlush tails (inlined) reach netPump 0x0049aa70 / netServiceControl 0x0049ab40 (thiscall, stood in)
  function netScene(o) {
    return function () {
      const fx = scene({ rows: AD, rets: o.rets });
      const net = alloc(0x400);
      fx.net = net;
      __freeze.save(ptr('0x008400b0'), 0x18);
      ptr('0x008400b8').writePointer(net);
      ptr('0x008400b0').writeU32(o.b0 >>> 0);
      if (o.b0) {
        calleeStand(fx, 0, 0x0049aa70, 1, 'thiscall', function (n, a) { return (0x7a00 + n + (a[0] & 0xff)) >>> 0; });
        calleeStand(fx, 1, 0x0049ab40, 1, 'thiscall', function (n, a) { return (0x7b00 + n + (a[0] & 0xff)) >>> 0; });
      }
      return fx;
    };
  }
  F.c3dw_pi_bd0_n0 = netScene({ b0: 0, rets: { [SHOW]: [1, 0] } });
  F.c3dw_pi_bd0_n1 = netScene({ b0: 1, rets: { [SHOW]: [1, 0] } });
  F.c3dw_pi_c30_n0 = netScene({ b0: 0, rets: { [HIDE]: [1, 0], [SHOW]: [0] } });
  F.c3dw_pi_c30_n1 = netScene({ b0: 1, rets: { [HIDE]: [1, 0], [SHOW]: [0] } });
  // waitRedrawReady with a redrawAll stand-in that raises [0x00822b9c] twice: the loop of 0x0045aedc repeats
  F.c3dw_wait_rep = function () {
    const fx = scene({ rows: FLAGROWS, rets: { [TICK]: [0], [HIDE]: [0] } });
    calleeStand(fx, 0, 0x00483c70, 0, 'default', function (n, a) { if (n < 2) ptr(0x00822b9c).writeU32(1); return 0x83c70 + n; });
    return fx;
  };

  // ---- pumpLight 0x0045c150 / pumpMessages 0x0045c030 (PeekMessageA, TranslateMessage, DispatchMessageA stood in)
  function pumpScene(o) {
    return function () {
      const fx = scene({ rows: o.rows, rets: o.rets, glob: o.glob });
      let mi = 0;
      const imp = {
        'USER32.dll!PeekMessageA': { args: 5, record: [1, 2, 3, 4], impl: function (a, ctx) {
          const peek = o.peek;
          const r = peek[Math.min(ctx.call, peek.length - 1)];
          if (r) {                                      // the message the original dispatches: hwnd, message, wParam, lParam
            const m = a[0];
            m.writeU32(0x1000 + ctx.call); m.add(4).writeU32(0x200 + ctx.call);
            m.add(8).writeU32(0x300 + ctx.call); m.add(12).writeU32(0x400 + ctx.call);
          }
          return r;
        } },
        'USER32.dll!TranslateMessage': { args: 1, impl: function (a, ctx) {
          const m = a[0]; ctx.record([m.readU32(), m.add(4).readU32(), m.add(8).readU32(), m.add(12).readU32()]); return 1;
        } },
        'USER32.dll!DispatchMessageA': { args: 1, impl: function (a, ctx) {
          const m = a[0]; ctx.record([m.readU32(), m.add(4).readU32(), m.add(8).readU32(), m.add(12).readU32()]); return 0x5000 + ctx.call;
        } },
      };
      fx.implog = __importStandIn({ module: 'golf_clean.exe', imports: imp }).log;
      return fx;
    };
  }
  // rows: [822d68, 822d74]
  const PMROWS = [[0, 0], [1, 0], [J, 0], [0, 1], [1, 1], [J, 5], [0x80000000, 0], [0, 0xffffffff]]
    .map(function (r) { return { 0x00822d68: r[0], 0x00822d74: r[1] }; });
  F.c3dw_pm_k0 = pumpScene({ rows: PMROWS, peek: [0], rets: { [TICK]: [0x11] } });
  F.c3dw_pm_k1 = pumpScene({ rows: PMROWS, peek: [1, 0], rets: { [TICK]: [0x22] } });
  F.c3dw_pm_k3 = pumpScene({ rows: PMROWS, peek: [1, 1, 1, 0], rets: { [TICK]: [0x33] } });
  // pumpLight: loop exits when flushRedraw returns 0: rows [4d3834, 822b9c, 822b98]
  const PLROWS = [[0, 0, 0], [0, 1, 0], [0, 0, 1], [1, 1, 1], [0x100, 0, 5], [0, 5, 5]]
    .map(function (r) { return { 0x004d3834: r[0], 0x00822b9c: r[1], 0x00822b98: r[2], 0x00822d74: 0, 0x00822d68: 0 }; });
  F.c3dw_pl_a = pumpScene({ rows: PLROWS, peek: [0], rets: { [SHOW]: [0], [HIDE]: [0], [TICK]: [0] } });
  F.c3dw_pl_b = pumpScene({ rows: PLROWS, peek: [1, 0], rets: { [SHOW]: [1, 0], [HIDE]: [1, 0], [TICK]: [0] } });


  // ====================================================================================================================
  // Terrain glue: the exe's calls into Terrain.dll go through `jmp [slot]` import thunks, so the IAT slots are stood in
  // (importstandin.js, thiscall: arg 0 is ecx = the Terrain object [0x00820ed0]); the object is our own block.
  // ====================================================================================================================
  const TI = {
    drawLine: ['?drawLine@Terrain@@QAEXHHHHHHH@Z', 8], loadNew: ['?loadNewCourseType@Terrain@@QAEXH@Z', 2],
    passCollar: ['?passCollarInfo@Terrain@@QAEXQAHH@Z', 3], reset: ['?resetTerrain@Terrain@@QAEXXZ', 1],
    dtor: ['??1Terrain@@QAE@XZ', 1], close: ['?closeSystem@Terrain@@QAEXXZ', 1],
  };
  function terImports(use, impl) {
    const imp = {};
    use.forEach(function (n, i) {
      const spec = { args: TI[n][1], abi: 'thiscall' };
      const f = impl && impl[n];
      if (f) spec.impl = f; else spec.ret = 0x7000 + i + 1;
      imp['Terrain.dll!' + TI[n][0]] = spec;
    });
    return __importStandIn({ module: 'golf_clean.exe', imports: imp }).log;
  }
  function terScene(o) {
    return function () {
      const fx = { clog: newClog() };
      freezeNow();
      fx.tobj = alloc(0x40);
      fx.obj = fx.tobj;
      __freeze.setU32(ptr(TER), o.noterrain ? 0 : fx.tobj.toUInt32());
      if (o.rows) rowsOn(fx, o.rows, o.rets);
      if (o.heap) heapOn(fx, 0x1000);
      if (o.imports) fx.implog = terImports(o.imports, o.impl);
      if (o.setup) o.setup(fx);
      return fx;
    };
  }
  const W = [0, 1, -1, 5, 0x7fffffff, -0x80000000, 100, 639, 7, 0x1234, -77, 3];
  const DLARGS = ['drawLine'];
  // drawMapLine 0x004493d0: seven arguments forwarded in order to Terrain::drawLine; the result follows the arguments
  F.c3dw_dml = terScene({ imports: DLARGS, impl: { drawLine: function (a) { return (a[1].toUInt32() + 3 * a[7].toUInt32()) >>> 0; } } });
  // lineTo 0x0040bf20: the previous end point [0x005a412c] / [0x005a4130] is the segment's start
  const LT = [[0, 0], [5, 7], [0x7fffffff, -1], [-0x80000000, 0x40000000], [100, 200], [1, 1], [0, 0], [-5, 9], [12, -12], [0x1234, 0x4321], [77, 0], [0, 77]]
    .map(function (r) { return { 0x005a412c: r[0], 0x005a4130: r[1] }; });
  F.c3dw_lt = terScene({ imports: DLARGS, rows: LT,
    impl: { drawLine: function (a) { return (a[1].toUInt32() ^ a[2].toUInt32() ^ a[7].toUInt32()) >>> 0; } } });
  // refreshTerrain 0x00449520: resetTerrain, then rebuildTerrainGrid 0x00449540 (stood in), then [0x00820f2a] = 1
  const RT = [0, 1, 0xff, 2, 0x80, 0, 1, 0x7f].map(function (v) { return { ['b' + 0x00820f2a]: v }; });
  F.c3dw_rt = terScene({ imports: ['reset'], rows: RT, setup: function (fx) { calleeStand(fx, 0, 0x00449540, 0, 'default', [0x61, 0x62]); } });
  F.c3dw_rt_n = terScene({ imports: ['reset'], rows: RT, noterrain: true, setup: function (fx) { calleeStand(fx, 0, 0x00449540, 0, 'default', [0]); } });
  // freeTerrain 0x00449860: closeSystem, then (object present) ~Terrain and operator delete 0x004a4ffc (recorded), then
  // [0x00820ed0] = 0. The row decides whether the object is there.
  F.c3dw_ft = function () {
    const fx = terScene({ imports: ['close', 'dtor'], heap: true })();
    return fx;
  };
  F.c3dw_ft_none = terScene({ imports: ['close', 'dtor'], heap: true, noterrain: true });

  // applyCourseType 0x00449400: 23 records of 0x30 bytes from 0x00578372 (+0 a signed byte flag, +4 a signed byte value),
  // [0x005a34e0] a mode byte; Terrain::passCollarInfo(table, 23) then loadNewCourseType(arg)
  const CT_BASE = 0x00578372, CT_N = 23;
  function ctRow(r) {
    const row = { ['b' + 0x005a34e0]: [1, 1, 2, 1, 1, 0, 1][r % 7] };           // 0x005a34e0
    for (let k = 0; k < CT_N; k++) {
      const flag = [1, 0, 0x7f, 0x80, 0xff, 2, 0][(k + r) % 7];
      row['b' + (CT_BASE + 0x30 * k)] = flag;
      row['b' + (CT_BASE + 0x30 * k + 4)] = (r * 37 + k * 11 + (k % 3 === 0 ? 0x80 : 0)) & 0xff;
    }
    return row;
  }
  const CTROWS = [];
  for (let r = 0; r < 12; r++) CTROWS.push(ctRow(r));
  F.c3dw_act = terScene({ rows: CTROWS, rets: CTROWS.map(function (_, i) { return i === 0 ? 0 : (i * 0x1111 - 3) | 0; }), imports: ['passCollar', 'loadNew'], impl: {
    passCollar: function (a, ctx) {
      const t = []; for (let i = 0; i < 23; i++) t.push(a[1].add(4 * i).readU32());
      ctx.record([a[0].toUInt32(), a[2].toUInt32()].concat(t)); return t.reduce(function (x, y) { return (x + y) >>> 0; }, 0);
    },
    loadNew: function (a) { return (a[1].toUInt32() * 3) >>> 0; } } });


  // ====================================================================================================================
  // Window glue. A window is a 0x300-byte block of ours: +0 a vtable of recording stubs (tags 0xD15A0000 + slot offset), +0x9c /
  // +0xa0 flag words, +0xb0 a link, +0x130 the parent, +0x224 / +0x22c the child array and its count. The exe's callees
  // are stood in by calleeStand (A/B thread only, calls logged in $clog) or run for real over these objects.
  // ====================================================================================================================
  function winEnv(vtArgs, vtRets) {
    const emit = codeArena();
    const rec = newRec();
    const cnt = alloc(0x80);
    const blk = alloc(0x300 * 16);
    const fx = { rec: rec, cnt: cnt, clog: newClog(), wblk: blk, obj: blk, emit: emit };
    fx.W = function (i) { return blk.add(0x300 * i); };
    // every window of this env shares one recording vtable
    const w0 = makeDisplay(emit, rec, cnt, vtRets, vtArgs, 0x20);
    fx.vt = w0.readPointer();
    for (let i = 0; i < 16; i++) fx.W(i).writePointer(fx.vt);
    fx.arrs = alloc(0x40 * 8);
    for (let i = 0; i < 16; i++) fx['w' + i] = fx.W(i);
    return fx;
  }
  const PTR = function (p) { return p.toUInt32(); };

  // ---- fwdMouseEvent238 0x0047eff0 (callee 0x0047be40 stood in: thiscall, this + 3 arguments)
  F.c3dw_fwd238 = function () {
    const fx = winEnv();
    calleeStand(fx, 0, 0x0047be40, 4, 'thiscall', function (n, a) { return (a[1] ^ (a[2] * 3) ^ a[3] ^ 0x5a5a0000) >>> 0; });
    return fx;
  };

  // ---- ChildArray::bringToFront 0x0047e700: parents P0 (6 children), P1 (1), P2 (count 0 with an array), P3 (no array)
  function arrEnv() {
    const fx = winEnv();
    fx.P = [fx.W(8), fx.W(9), fx.W(10), fx.W(11)];
    const kids = [0, 1, 2, 3, 4, 5].map(function (i) { return fx.W(i); });
    const a0 = fx.arrs, a1 = fx.arrs.add(0x40), a2 = fx.arrs.add(0x80);
    kids.forEach(function (k, i) { a0.add(4 * i).writePointer(k); });
    a0.add(24).writePointer(fx.W(6));                              // the flagged window sits at index 6
    a1.writePointer(kids[0]);
    a2.writePointer(kids[3]);
    fx.P[0].add(0x224).writePointer(a0); fx.P[0].add(0x22c).writeU32(7);
    fx.P[1].add(0x224).writePointer(a1); fx.P[1].add(0x22c).writeU32(1);
    fx.P[2].add(0x224).writePointer(a2); fx.P[2].add(0x22c).writeU32(0);
    fx.P[3].add(0x224).writePointer(ptr(0)); fx.P[3].add(0x22c).writeU32(0);
    fx.kids = kids;
    kids.forEach(function (k, i) { fx['k' + i] = k; });
    fx.flagged = fx.W(6); fx.flagged.add(0x9c).writeU32(0x2000000);
    fx.other = fx.W(7);
    fx.p0 = fx.P[0]; fx.p1 = fx.P[1]; fx.p2 = fx.P[2]; fx.p3 = fx.P[3];
    return fx;
  }
  F.c3dw_btf = function () {
    const fx = arrEnv();
    freezeNow();
    calleeStand(fx, 0, 0x0047e450, 0, 'default', [0x91, 0x92, 0x93]);
    return fx;
  };
  F.c3dw_btf_early = F.c3dw_btf;

  // ---- zRaise 0x0047e580: the top-level array 0x0083a2d8 (count 0x0083ab94) rows: 6 windows in order, 1, 0, and shuffled
  function zEnv(rowSpecs, stubRet) {
    return function () {
      const fx = arrEnv();
      freezeNow();
      const rows = [], rets = [];
      rowSpecs.forEach(function (rs) {
        const row = {};
        row[0x0083ab94] = rs.order.length;
        for (let k = 0; k < 32; k++) row[0x0083a2d8 + 4 * k] = k < rs.order.length ? PTR(fx.W(rs.order[k])) : 0xdead0000 + k;
        rows.push(row);
        rets.push(rs.win === 'none' ? 0 : rs.win === 'flag' ? PTR(fx.flagged) : rs.win === 'other' ? PTR(fx.other) : PTR(fx.W(rs.win)));
      });
      rowsOn(fx, rows, rets);
      calleeStand(fx, 0, 0x0047e450, 0, 'default', stubRet);
      return fx;
    };
  }
  const ZR = [
    { order: [0, 1, 2, 3, 4, 5], win: 0 }, { order: [0, 1, 2, 3, 4, 5], win: 3 }, { order: [0, 1, 2, 3, 4, 5], win: 5 },
    { order: [0, 1, 2, 3, 4, 5], win: 'other' }, { order: [2], win: 2 }, { order: [2], win: 4 }, { order: [], win: 1 },
    { order: [5, 3, 1], win: 1 }, { order: [5, 3, 1], win: 3 }, { order: [4, 4, 4], win: 4 }, { order: [0, 1, 2, 3, 4, 5], win: 1 },
    { order: [1, 0], win: 0 },
  ];
  F.c3dw_zr = zEnv(ZR, [0x71, 0x72, 0x73]);
  F.c3dw_zr_early = zEnv([{ order: [0, 1, 2], win: 'none' }, { order: [0, 1, 6], win: 'flag' }, { order: [], win: 'none' },
                           { order: [6, 1, 2], win: 'flag' }, { order: [0, 6], win: 'flag' }, { order: [0, 1, 2], win: 2 }], [0x74]);

  // ---- Panel::notify 0x0047e120: windows with a parent (bringToFront over the parent's array) and without (zRaise)
  F.c3dw_pn = function () {
    const fx = arrEnv();
    freezeNow();
    // kid k0..k5 sit in P0's array (their parent is P0); k5 also in the global array; the lone windows have no parent
    fx.kids.forEach(function (k) { k.add(0x130).writePointer(fx.P[0]); });
    const lone = [fx.W(12), fx.W(13), fx.W(14)];
    lone[2].add(0x9c).writeU32(0x2000000);
    __freeze.save(ptr(0x0083ab94), 4);
    __freeze.save(ptr(0x0083a2d8), 0x40);
    ptr(0x0083ab94).writeU32(3);
    [lone[1], lone[0], lone[2]].forEach(function (w, i) { ptr(0x0083a2d8 + 4 * i).writeU32(PTR(w)); });
    fx.flagged.add(0x130).writePointer(fx.P[0]);                   // a flagged window with a parent
    fx.l0 = lone[0]; fx.l1 = lone[1]; fx.l2 = lone[2]; fx.l3 = fx.other;
    fx.k0p3 = fx.W(15); fx.k0p3.add(0x130).writePointer(fx.P[3]);
    calleeStand(fx, 0, 0x0047e450, 0, 'default', [0x81, 0x82]);
    return fx;
  };

  // ---- Window::onCommand 0x0047ee70: net object [0x008400b8] with the command ids at +0x54 / +0x58 and arguments at
  // +0x5c / +0x60; callees stood in: netBeginOp0 / netBeginOp1 (thiscall + 1), netOp0 (thiscall), dispatchCommand and
  // dispatchAccel (thiscall + 1)
  function cmdEnv(withNet) {
    return function () {
      const fx = winEnv();
      freezeNow();
      const net = alloc(0x100);
      net.add(0x54).writeU32(0x41); net.add(0x58).writeU32(0x42); net.add(0x5c).writeU32(0x5c5c5c01); net.add(0x60).writeU32(0x60606002);
      fx.net = net;
      __freeze.save(ptr(0x008400b8), 4);
      __freeze.save(ptr(0x0083ab88), 4);
      ptr(0x008400b8).writePointer(withNet ? net : ptr(0));
      ptr(0x0083ab88).writeU32(0x77777777);
      calleeStand(fx, 0, 0x00497c20, 2, 'thiscall', function (n, a) { return (a[1] + 0x100) >>> 0; });
      calleeStand(fx, 1, 0x00497c70, 2, 'thiscall', function (n, a) { return (a[1] + 0x200) >>> 0; });
      calleeStand(fx, 2, 0x00497cc0, 1, 'thiscall', function (n, a) { return 0x300 + n; });
      calleeStand(fx, 3, 0x0047c970, 2, 'thiscall', function (n, a) { return (a[1] ^ 0x400) >>> 0; });
      calleeStand(fx, 4, 0x0047c6c0, 2, 'thiscall', function (n, a) { return (a[1] ^ 0x500) >>> 0; });
      return fx;
    };
  }
  F.c3dw_cmd = cmdEnv(true);
  F.c3dw_cmd_nonet = cmdEnv(false);

  // ---- Window::setFocus 0x0047f1b0 (callees stood in: Panel::notify, 0x0047c010 and 0x0047c430, each thiscall + 3 arguments,
  // Panel::notify thiscall alone); the parent's vtable slot +0x12c is a recording stub (1 argument)
  F.c3dw_sf = function () {
    const fx = winEnv({ 0x12c: 1 }, { 0x12c: [0x1e1, 0x1e2] });
    freezeNow();
    fx.pw = fx.W(1); fx.pw.add(0x130).writePointer(fx.W(2));        // has a parent
    fx.np = fx.W(3);                                                // no parent
    fx.fl = fx.W(4); fx.fl.add(0x130).writePointer(fx.W(2)); fx.fl.add(0x9c).writeU32(0x200);   // bit 0x200 set
    fx.fl2 = fx.W(5); fx.fl2.add(0x9c).writeU32(0xfffffdff);        // every other bit set
    [0x0083ab40, 0x0083ab44, 0x0083ab48].forEach(function (a) { __freeze.save(ptr(a), 4); ptr(a).writeU32(0x5e5e0000 + (a & 0xff)); });
    calleeStand(fx, 0, 0x0047e120, 1, 'thiscall', 0x11);
    calleeStand(fx, 1, 0x0047c010, 4, 'thiscall', function (n, a) { return (a[1] + a[2] + a[3]) >>> 0; });
    calleeStand(fx, 2, 0x0047c430, 4, 'thiscall', function (n, a) { return (a[1] ^ a[2] ^ (a[3] * 5)) >>> 0; });
    return fx;
  };

  // ---- Window::refreshRect 0x00480c20: Window::visible (real, flag 1 of +0xa0 and the parent chain); stood in:
  // 0x0047fa30 and 0x0047b0d0 (thiscall + rect pointer, the rect's words are logged) and drawTooltip 0x0047cdb0 (cdecl, rect)
  F.c3dw_rr = function () {
    const fx = winEnv();
    freezeNow();
    fx.v = fx.W(1); fx.v.add(0xa0).writeU32(1);                           // visible, no parent
    fx.iv = fx.W(2);                                                      // flag clear
    fx.vp = fx.W(3); fx.vp.add(0xa0).writeU32(1); fx.vp.add(0x130).writePointer(fx.W(1));   // visible parent
    fx.vpi = fx.W(4); fx.vpi.add(0xa0).writeU32(1); fx.vpi.add(0x130).writePointer(fx.W(2)); // invisible parent
    calleeStand(fx, 0, 0x0047fa30, 2, 'thiscall', 0x21, { 1: 4 });
    calleeStand(fx, 1, 0x0047b0d0, 2, 'thiscall', 0x22, { 1: 4 });
    calleeStand(fx, 2, 0x0047cdb0, 1, 'default', function (n, a) { return 0x23 + n; }, { 0: 4 });
    return fx;
  };

  // ---- currentFocusOwner 0x0047f2f0 / uiKeyEvent148 0x00495990 / uiKeyEvent 0x00495910
  // owners: window objects whose +0xb0 link is the target (visible: +0xa0 bit 0) ; rows set the four globals
  function focusEnv(rowSpecs, vtRets, extra, vtArgs) {
    return function () {
      const fx = winEnv(vtArgs || { 0x144: 2, 0x148: 1 }, vtRets);
      freezeNow();
      const own = [fx.W(1), fx.W(2), fx.W(3), fx.W(4)];
      const tgt = [fx.W(5), fx.W(6), fx.W(7), fx.W(8)];
      tgt[0].add(0xa0).writeU32(1);                                      // visible target
      tgt[1].add(0xa0).writeU32(0);                                      // invisible
      tgt[2].add(0xa0).writeU32(1); tgt[2].add(0x130).writePointer(tgt[1]);   // parent invisible
      tgt[3].add(0xa0).writeU32(1);                                      // visible, second
      own[0].add(0xb0).writePointer(tgt[0]); own[1].add(0xb0).writePointer(tgt[1]);
      own[2].add(0xb0).writePointer(tgt[2]); own[3].add(0xb0).writePointer(ptr(0));
      fx.own = own; fx.tgt = tgt;
      const rows = rowSpecs.map(function (r) {
        const row = {};
        row[0x0083ab54] = r[0] < 0 ? 0 : PTR(own[r[0]]);
        row[0x0083b990] = r[1] < 0 ? 0 : PTR(own[r[1]]);
        row[0x0083ab38] = r[2] < 0 ? 0 : PTR(own[r[2]]);
        row[0x0083ab60] = r[3] < 0 ? 0 : PTR(own[r[3]]);
        row[0x0083ab3c] = 0x3c3c3c3c;
        return row;
      });
      const onRow = extra && extra.onRow;
      rowsOn(fx, rows, rowSpecs.map(function (_, i) { return i; }), onRow);
      if (extra && extra.setup) extra.setup(fx);
      return fx;
    };
  }
  // [0x83ab54, 0x83b990, 0x83ab38, 0x83ab60] each: index of an owner window or -1
  const FO = [[0, -1, -1, -1], [0, 3, -1, -1], [0, 1, -1, -1], [1, -1, -1, -1], [2, -1, -1, -1], [3, -1, -1, -1], [-1, -1, 0, -1],
              [-1, -1, -1, 0], [-1, -1, -1, -1], [-1, 0, -1, 1], [-1, -1, 3, 0], [-1, -1, 1, 0]];
  F.c3dw_cfo = focusEnv(FO, {});
  F.c3dw_uk148 = focusEnv(FO, { 0x148: [0x4801, 0x4802, 0x4803] }, undefined, { 0x144: 1, 0x148: 1 });
  // uiKeyEvent: owner slot 0x144 (2 arguments), GetKeyState results per modifier, callbacks [0x83ab00] (cdecl, 1 argument)
  // and [0x83aafc] (cdecl, none) as machine-code stubs
  function keyEnv(rowSpecs) {
    return function () {
      const ks = { 0x10: 0, 0x11: 0, 0x12: 0 };
      const cbs = { shift: 0 };
      const fx = focusEnv(rowSpecs.map(function (r) { return r.own; }), { 0x144: [0x1441, 0x1442] }, {
        onRow: function (i) { const r = rowSpecs[i]; ks[0x10] = r.ks[0]; ks[0x11] = r.ks[1]; ks[0x12] = r.ks[2]; },
        setup: function (fx) {
          const emit = fx.emit;
          const cb1 = emit(stubBytes(fx.rec, 0xCB010000, 1, 0xCB1, true));
          const cb2 = emit(stubBytes(fx.rec, 0xCB020000, 0, 0xCB2, true));
          fx.cb1 = cb1; fx.cb2 = cb2;
          const rows2 = rowSpecs.map(function (r) {
            const row = {};
            row[0x0083ab00] = r.cb1 ? PTR(cb1) : 0;
            row[0x0083aafc] = r.cb2 ? PTR(cb2) : 0;
            return row;
          });
          saveRows(rows2);
          // fold the callback pointers into the row getters
          rowSpecs.forEach(function (r, i) {
            const g = Object.getOwnPropertyDescriptor(fx, 'r' + i).get;
            Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () { putRow(rows2[i]); return g(); } });
          });
          fx.implog = __importStandIn({ module: 'golf_clean.exe', imports: {
            'USER32.dll!GetKeyState': { args: 1, impl: function (a) { return ks[a[0].toUInt32()] >>> 0; } } } }).log;
        },
      })();
      return fx;
    };
  }
  const KR = [
    { own: [0, -1, -1, -1], ks: [0, 0, 0], cb1: 0, cb2: 0 }, { own: [0, -1, -1, -1], ks: [0x8000, 0, 0], cb1: 1, cb2: 0 },
    { own: [0, -1, -1, -1], ks: [0, 0x8000, 0], cb1: 1, cb2: 1 }, { own: [0, -1, -1, -1], ks: [0, 0, 0x8000], cb1: 0, cb2: 1 },
    { own: [0, -1, -1, -1], ks: [0xffff, 0xffff, 0xffff], cb1: 1, cb2: 1 }, { own: [1, -1, -1, -1], ks: [0x7fff, 0x0100, 0x00ff], cb1: 1, cb2: 1 },
    { own: [-1, -1, -1, -1], ks: [0x8000, 0x8000, 0], cb1: 1, cb2: 0 }, { own: [-1, -1, -1, -1], ks: [0x8001, 0x8000, 0x8000], cb1: 0, cb2: 1 },
    { own: [2, -1, -1, -1], ks: [0, 0, 0], cb1: 1, cb2: 1 }, { own: [3, -1, -1, -1], ks: [0, 0, 0], cb1: 1, cb2: 1 },
    { own: [0, 3, -1, -1], ks: [0x8000, 0x8000, 0x8000], cb1: 1, cb2: 1 }, { own: [-1, -1, 0, -1], ks: [0, 0x8000, 0], cb1: 0, cb2: 0 },
  ];
  F.c3dw_uk = keyEnv(KR);

  // ---- drawBox 0x00477e60 (callee 0x00477eb0 stood in: stdcall, 3 arguments; the rect's four words are logged)
  F.c3dw_box = function () {
    const fx = winEnv();
    calleeStand(fx, 0, 0x00477eb0, 3, 'stdcall', function (n, a) { return (a[0] * 7 + a[2]) >>> 0; }, { 1: 4 });
    return fx;
  };

  // ---- TextView_beginMeasure 0x004785e0 (callee 0x00478610 stood in: thiscall + 1 argument); views are 0x80-byte blocks
  F.c3dw_tvbm = function () {
    const fx = winEnv();
    freezeNow();
    fx.views = alloc(0x80 * 12);
    for (let i = 0; i < 12; i++) {
      const v = fx.views.add(0x80 * i);
      for (let o = 0; o < 0x80; o += 4) v.add(o).writeU32((0x56000000 + i * 0x10000 + o) >>> 0);
      fx['v' + i] = v;
    }
    [0x00839aa0, 0x00839aa4, 0x00839aa8].forEach(function (a) { __freeze.save(ptr(a), 4); ptr(a).writeU32(0x6d6d0000 + (a & 0xff)); });
    calleeStand(fx, 0, 0x00478610, 2, 'thiscall', function (n, a) { return (a[1] + 0x40) >>> 0; });
    return fx;
  };


  // ====================================================================================================================
  // Display glue: scene() swaps a recording display into [0x0083ad50]; a slot whose result a row decides returns the word at
  // fx.cell (rows write it). Slot offsets and argument counts are read from the instructions.
  // ====================================================================================================================
  function cellRows(vals, extra) {
    return function (fx) {
      const a = fx.cell.toUInt32();
      return vals.map(function (v, i) {
        const row = {};
        row[a] = v;
        if (extra) Object.keys(extra(i)).forEach(function (k) { row[k] = extra(i)[k]; });
        return row;
      });
    };
  }
  const CELLV = [0, 1, 0x7fffffff, 0xffffffff, 0x80000000, 5, 0x400, 0x3ff, 0x1234, 0, 7, 0x55aa55aa];
  const JUNK12 = function (i) { return (0x30000000 + i * 0x01010101) >>> 0; };
  function cellScene(slot, o) {
    o = o || {};
    return function () {
      const fx = scene({ disp: o.disp, rets: function (emit, rec, cnt, f) { const r = {}; r[slot] = { mem: f.cell }; return r; },
                         args: o.args, rows: cellRows(CELLV, function (i) { const e = {}; (o.junk || []).forEach(function (a) { e[a] = JUNK12(i); }); return e; }),
                         glob: o.glob });
      return fx;
    };
  }
  F.c3dw_cps = cellScene(0xbc, { junk: [0x0083ad44] });
  F.c3dw_cps_nodisp = cellScene(0xbc, { junk: [0x0083ad44], disp: false });
  F.c3dw_rss = cellScene(0xc0, { args: { 0xc0: 2 }, junk: [0x0083ac98, 0x0083ac9c], glob: { 0x0083ac90: 0 } });
  F.c3dw_pid = cellScene(0x150, { junk: [0x0083ad4c] });
  F.c3dw_pid_nodisp = cellScene(0x150, { junk: [0x0083ad4c], disp: false });

  // Cache483030::attach 0x00483030: the display's slot +0x78 (1 argument) returns the object a row selects (three objects of ours
  // with a recording vtable); 0x00483070 is stood in
  F.c3dw_cat = function () {
    const emit = codeArena();
    let objs = [];
    const fx = scene({
      args: { 0x78: 1, 0x7c: 1 },
      rets: function (em, rec, cnt, f) {
        for (let k = 0; k < 3; k++) objs.push(makeDisplay(em, rec, cnt, { 4: [0xa0 + k] }, {}, 0x40));
        f.o0 = objs[0]; f.o1 = objs[1]; f.o2 = objs[2];
        return { 0x78: { mem: f.cell }, 0x7c: { mem: f.cell } };
      },
      rows: function (f) {
        f.sblk = alloc(0x40 * 12);
        for (let i = 0; i < 12; i++) { for (let o = 0; o < 0x40; o += 4) f.sblk.add(0x40 * i + o).writeU32((0x77000000 + i * 0x100 + o) >>> 0); f['s' + i] = f.sblk.add(0x40 * i); }
        return [0, 1, 2, 1, 0, 2, 2, 0, 1, 1, 0, 2].map(function (k) { const r = {}; r[f.cell.toUInt32()] = PTR(objs[k]); return r; });
      },
      rets2: function (f) { return [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11].map(function (i) { return PTR(f.sblk.add(0x40 * i)); }); },
      after: function (f) { calleeStand(f, 0, 0x00483070, 1, 'thiscall', 0x70); f.clog = f.clog; },
    });
    return fx;
  };
  // (calleeStand logs into fx.clog: give the scene one)
  const _scene = scene;

  // freeGraphsyObject 0x004a0060: [0x0084092c] is a module handle; FreeLibrary [0x4ba0ac] stood in (result = handle ^ 1)
  function fgoScene(disp) {
    return function () {
      const HV = [0, 0x1234, 0, 0xffffffff, 0x80000000, 1, 0, 0x7fffffff, 0x4000, 0, 0x55, 0xdeadbeef];
      const fx = scene({ disp: disp, rows: HV.map(function (h) { return { 0x0084092c: h }; }) });
      fx.implog = __importStandIn({ module: 'golf_clean.exe', imports: {
        'KERNEL32.dll!FreeLibrary': { args: 1, impl: function (a) { return (a[0].toUInt32() ^ 1) >>> 0; } } } }).log;
      return fx;
    };
  }
  F.c3dw_fgo = fgoScene(true);
  F.c3dw_fgo_nodisp = fgoScene(false);

  // TextView::setFontSize 0x0048e120: views of 0x1500 bytes (+0x590, +0x1f68 is outside: the flag word is read at +0x1f68, so
  // the views are 0x2000 bytes), +0x1464 points at a q block whose +8 is the field offset; the width slot +0xa8 returns a cell
  function tvScene(disp) {
    return function () {
      const WID = [0x400, 0x3ff, 0x1000, 0, 0xffffffff, 0x7fffffff, 0x400, 0x401, 0x400, 0x500, 0x3ff, 0x2000];
      const SZ = [10, 10, 7, 3, -9, 1, -1, 0x7fffffff, 100, 0x40000000, -0x80000000, 5];
      const fx = scene({
        disp: disp,
        rets: function (em, rec, cnt, f) { return { 0xa8: { mem: f.cell } }; },
        rows: function (f) {
          f.vblk = alloc(0x2000 * 12);
          f.qblk = alloc(0x40 * 12);
          for (let i = 0; i < 12; i++) {
            const v = f.vblk.add(0x2000 * i), q = f.qblk.add(0x40 * i);
            for (let o = 0; o < 0x2000; o += 0x100) v.add(o).writeU32((0x66000000 + i * 0x1000 + o) >>> 0);
            q.add(8).writeU32([0, 4, 0x10, 0x20, 8, 0x40, 0, 4, 0x10, 0x20, 8, 0][i]);
            v.add(0x1464).writePointer(q);
            v.add(0x590).writeU32(i === 3 || i === 7 ? 0x11 : 0);
            v.add(0x1f68).writeU32(i === 4 || i === 8 ? 0x400 : (i === 5 ? 0xfffffbff : 0));
          }
          return WID.map(function (w) { const r = {}; r[f.cell.toUInt32()] = w; return r; });
        },
        rets2: function (f) { return [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11].map(function (i) { return PTR(f.vblk.add(0x2000 * i)); }); },
      });
      fx.sizes = SZ;
      return fx;
    };
  }
  F.c3dw_tvfs = tvScene(true);
  F.c3dw_tvfs_nodisp = tvScene(false);

  // Text_flush 0x00478970: the line buffer 0x00839658 (+ geometry words), drawText 0x00477c30 (thiscall + 4; the first words of
  // the text are logged) and widgetValue 0x00477580 (thiscall) stood in; views of 0x80 bytes
  F.c3dw_tfl = function () {
    const LENS = [0, 1, 2, 7, 15, 31, 40, 0, 3, 9, 20, 63];
    const fx = winEnv();
    freezeNow();
    fx.sblk = alloc(0x80 * 12);
    for (let i = 0; i < 12; i++) { for (let o = 0; o < 0x80; o += 4) fx.sblk.add(0x80 * i + o).writeU32((0x4c000000 + i * 0x1000 + o) >>> 0); }
    __freeze.save(ptr(0x00839658), 0x80);
    [0x00839a9c, 0x00839aa0, 0x00839aa4].forEach(function (a) { __freeze.save(ptr(a), 4); });
    const rets = [];
    for (let i = 0; i < 12; i++) {
      Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () {
        const b = ptr(0x00839658);
        b.writeByteArray(new Array(0x80).fill(0xee));
        for (let k = 0; k < LENS[i]; k++) b.add(k).writeU8(0x41 + ((k * 7 + i) % 26));
        b.add(LENS[i]).writeU8(0);
        ptr(0x00839a9c).writeU32((i * 13 - 5) >>> 0); ptr(0x00839aa0).writeU32((i * 100 + 7) >>> 0); ptr(0x00839aa4).writeU32(0x1111 * i);
        return PTR(fx.sblk.add(0x80 * i));
      } });
    }
    calleeStand(fx, 0, 0x00477c30, 5, 'thiscall', function (n, a) { return (a[3] + a[4]) >>> 0; }, { 1: 8 });
    calleeStand(fx, 1, 0x00477580, 1, 'thiscall', function (n, a) { return (0x11 * (n + 1)) >>> 0; });
    return fx;
  };

  // drawPanelBackdrop 0x00432620: the text buffer 0x0051a068, setDrawTarget 0x004762d0 (thiscall + 4), drawMapLine 0x004493d0
  // (cdecl, 7) and drawStringAt 0x00404b70 (cdecl, 4; the text's first words are logged) stood in
  F.c3dw_dpb = function () {
    const LENS = [0, 1, 3, 10, 50, 100, 150, 200, 230, 5, 0, 20];
    const fx = winEnv();
    freezeNow();
    __freeze.save(ptr(0x0051a068), 0x120);
    for (let i = 0; i < 12; i++) {
      const A1 = [0, 50, 100, 400, 799, -5, 0x7fffffff, 160, 30, 640, 320, 20];
      Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () {
        const b = ptr(0x0051a068);
        for (let k = 0; k < LENS[i]; k++) b.add(k).writeU8(0x61 + ((k * 5 + i) % 26));
        b.add(LENS[i]).writeU8(0);
        return A1[i];
      } });
    }
    calleeStand(fx, 0, 0x004762d0, 5, 'thiscall', 0x62);
    calleeStand(fx, 1, 0x004493d0, 7, 'default', function (n, a) { return a[0] + a[2]; });
    calleeStand(fx, 2, 0x00404b70, 4, 'default', function (n, a) { return (a[1] ^ a[2]) >>> 0; }, { 0: 8 });
    return fx;
  };


  // ====================================================================================================================
  // File / CRT glue. The CRT's stdio and low-level I/O functions the bodies call are stood in with calleeStand (A/B thread
  // only, every call logged in $clog with the pointed-to words, results chosen by the fixture); no real file, FILE or file
  // descriptor is touched.
  // ====================================================================================================================
  function ioEnv() { const fx = winEnv(); freezeNow(); return fx; }
  const STR = function (s) { const p = alloc(0x40); p.writeUtf8String(s); return p; };
  function words(p, n, tag) { for (let k = 0; k < n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }

  // ---- Reader::readLine 0x00488230: 12 readers; FILE flag word +0xc (bit 0x10 = end of file); fgets 0x004a65ee decides per FILE
  F.c3dw_rdl = function () {
    const fx = ioEnv();
    const KINDS = ['eof', 'eof', 'null', 'line', 'line', 'long', 'null', 'line', 'eof', 'line', 'line', 'null'];
    const LINES = ['hello', 'a,b,c', '', 'x', 'line seven\r\n', 'ten', 'eleven'];
    fx.rdblk = alloc(0x170 * 12); fx.bufs = alloc(0x200 * 12); fx.files = alloc(0x20 * 12);
    fx.obj = fx.bufs;
    const kindOf = {};
    for (let i = 0; i < 12; i++) {
      const r = fx.rdblk.add(0x170 * i), f = fx.files.add(0x20 * i), b = fx.bufs.add(0x200 * i);
      words(r, 0x170, 0x52000000 + i * 0x1000);
      r.add(0x158).writePointer(f); r.add(0x15c).writePointer(b); r.add(0x154).writeU32(0x5a5a5a5a);
      f.add(0xc).writeU32((KINDS[i] === 'eof' ? 0x10 : 0) | (i % 3 === 0 ? 0xe0 : 0x01));
      b.writeUtf8String('stale buffer ' + i);
      kindOf[PTR(f)] = [KINDS[i], i];
      fx['rd' + i] = r;
    }
    calleeStand(fx, 0, 0x004a65ee, 3, 'default', function (n, a) {
      const k = kindOf[a[2]];
      if (!k || k[0] === 'null') return 0;
      const text = k[0] === 'long' ? 'L'.repeat(0x1fe) : LINES[k[1] % LINES.length];
      ptr(a[0]).writeUtf8String(text);
      return a[0];
    }, { 2: 4 });
    calleeStand(fx, 1, 0x004925d0, 1, 'default', 0, { 0: { str: 8 } });
    calleeStand(fx, 2, 0x004925b0, 1, 'default', 0, { 0: { str: 8 } });
    return fx;
  };

  // ---- readLineGlobal 0x00487e90 / readSectionGlobal 0x00487e70: the global reader 0x0083b000 as ecx; callee results from a cell
  function cellEnv(rows, setup) {
    return function () {
      const fx = ioEnv();
      fx.cellb = alloc(0x40);
      rowsOn(fx, rows(fx));
      setup(fx);
      return fx;
    };
  }
  const CELLS = [0, 1, 0x7fffffff, 0xffffffff, 0x80000000, 5, 0x1234, 0, 9, 0x55555555, 3, 0x40];
  F.c3dw_rlg = cellEnv(function (fx) { return CELLS.map(function (v) { const r = {}; r[PTR(fx.cellb)] = v; return r; }); }, function (fx) {
    calleeStand(fx, 0, 0x00488230, 1, 'thiscall', function (n, a) { return (ptr(PTR(fx.cellb)).readU32() ^ a[0]) >>> 0; });
  });
  F.c3dw_rsg = cellEnv(function (fx) { return CELLS.map(function (v) { const r = {}; r[PTR(fx.cellb)] = v; return r; }); }, function (fx) {
    calleeStand(fx, 0, 0x00487fb0, 3, 'thiscall', function (n, a) { return (ptr(PTR(fx.cellb)).readU32() + a[1] * 3 + a[2]) >>> 0; });
  });

  // ---- term_destination 0x004ae590: cinfo blocks (+0 err, +0x14 dest); err (+0 error_exit stub, +0xc, +0x14 msg_code); dest
  // (+4 free_in_buffer, +0x14 FILE, +0x18 buffer); fwrite 0x004a6268 / fflush 0x004a9943 stood in
  F.c3dw_tdest = function () {
    const fx = ioEnv();
    const V = [  // [free_in_buffer, FILE flags, fwrite mode]
      [0x1000, 0, 'full'], [0x1000, 0x20, 'full'], [0x800, 0, 'full'], [0x800, 0, 'short'], [0x800, 0x20, 'short'],
      [0, 0, 'full'], [0xfff, 0x01, 'full'], [1, 0, 'short'], [0x1001, 0, 'full'], [0x400, 0xff, 'full'], [0x400, 0x1f, 'zero'],
      [0x7fffffff, 0x20, 'full'],
    ];
    const errx = fx.emit(stubBytes(fx.rec, 0xE8E80000, 1, 0xE1E1, true));
    fx.tblk = alloc(0x100 * 12); fx.obj = fx.tblk;
    const modeOf = {};
    for (let i = 0; i < 12; i++) {
      const base = fx.tblk.add(0x100 * i);
      const cinfo = base, err = base.add(0x40), dest = base.add(0x80), file = base.add(0xc0);
      cinfo.writePointer(err); cinfo.add(0x14).writePointer(dest);
      err.writePointer(errx); err.add(0x14).writeU32(0x13131313);
      dest.add(4).writeU32(V[i][0] >>> 0); dest.add(0x14).writePointer(file); dest.add(0x18).writePointer(base.add(0xe0));
      file.add(0xc).writeU32(V[i][1]);
      modeOf[PTR(file)] = V[i][2];
      fx['ci' + i] = cinfo;
    }
    calleeStand(fx, 0, 0x004a6268, 4, 'default', function (n, a) {
      const m = modeOf[a[3]];
      return m === 'full' ? a[2] : m === 'short' ? (a[2] - 1) >>> 0 : 0;
    });
    calleeStand(fx, 1, 0x004a9943, 1, 'default', function (n, a) { return (a[0] & 0xff) >>> 0; });
    return fx;
  };

  // ---- output_message 0x004aecf0: err->format_message (a NativeCallback of ours, A/B thread only) writes a message per cinfo
  F.c3dw_omsg = function () {
    const fx = ioEnv();
    const MSGS = ['Not a JPEG file', '', 'Bogus buffer control', 'x', 'Wrong JPEG library version: library is 62, caller expects %d', 'Image too wide',
                  'Quantization table 0x%02x was not defined', 'unexpected end', 'A', 'two words', 'tab\there', 'last message'];
    const fmt = new NativeCallback(function (cinfo, buf) {
      ptr(buf).writeUtf8String(MSGS[ptr(cinfo).add(8).readU32()]); return 0;
    }, 'int', ['pointer', 'pointer'], 'default');
    __diffKeepAlive.push(fmt);
    fx.cblk = alloc(0x100 * 12); fx.obj = fx.cblk;
    for (let i = 0; i < 12; i++) {
      const c = fx.cblk.add(0x100 * i), err = c.add(0x40);
      c.writePointer(err); c.add(8).writeU32(i);
      err.add(0xc).writePointer(fmt);
      fx['ci' + i] = c;
    }
    calleeStand(fx, 0, 0x004a622c, 3, 'default', function (n, a) {
      const b = ptr(a[2]); return (b.readU8() * 3 + n + 100) >>> 0;
    }, { 2: { str: 16 } });
    return fx;
  };

  // ---- saveFieldIo 0x0040af70: mode [0x0053e638] (non-zero = __read), fd [0x00568d08]; __read 0x004a583a / __write 0x004a5b58
  F.c3dw_sfio = function () {
    const fx = ioEnv();
    fx.fbuf = alloc(0x100);
    const MODES = [0, 1, 0, 1, -1, 0x100, 0, 0, 1, 2, 0x80000000, 0], FDS = [3, 3, 0, 7, -1, 0x7fffffff, 1, 5, 9, 0, 4, 12];
    const rows = MODES.map(function (m, i) { const r = {}; r[0x0053e638] = m; r[0x00568d08] = FDS[i]; return r; });
    rowsOn(fx, rows, rows.map(function () { return PTR(fx.fbuf); }));
    calleeStand(fx, 0, 0x004a583a, 3, 'default', function (n, a) { return (a[0] * 5 + a[2]) >>> 0; });
    calleeStand(fx, 1, 0x004a5b58, 3, 'default', function (n, a) { return (a[0] * 7 - a[2]) >>> 0; });
    return fx;
  };

  // ---- flushCourseInfo 0x00432170: the course-info buffer 0x00587da0 (first byte tested, then cleared)
  F.c3dw_fci = function () {
    const fx = ioEnv();
    __freeze.save(ptr(0x00587da0), 0x40);
    const B0 = [0, 0x41, 0x80, 0xff, 1, 0, 0x7f, 0x20, 0, 0x61, 0xfe, 2];
    for (let i = 0; i < 12; i++) {
      Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () {
        const b = ptr(0x00587da0);
        b.writeByteArray(new Array(0x40).fill(0));
        b.writeU8(B0[i]);
        b.add(1).writeUtf8String('Course ' + i);
        return 0;
      } });
    }
    calleeStand(fx, 0, 0x00431fa0, 1, 'default', 0x31, { 0: { str: 8 } });
    calleeStand(fx, 1, 0x00431ee0, 1, 'default', 0x32, { 0: { str: 8 } });
    return fx;
  };

  // ---- resolveAndOpenFile 0x004a00f0: resolveDataPath 0x00491da0 (even path index resolves to another string, odd returns 0) and
  // fopen 0x004a614d stood in
  F.c3dw_rof = function () {
    const fx = ioEnv();
    const paths = [], res = {}, modes = [];
    for (let i = 0; i < 6; i++) { paths.push(STR('data\\file' + i + '.txt')); }
    for (let i = 0; i < 3; i++) modes.push(STR(['r', 'rb', 'a+'][i]));
    paths.forEach(function (p, i) { fx['p' + i] = p; if (i % 2 === 0) res[PTR(p)] = STR('C:\\game\\data\\file' + i + '.txt'); });
    modes.forEach(function (m, i) { fx['m' + i] = m; });
    calleeStand(fx, 0, 0x00491da0, 1, 'default', function (n, a) { return res[a[0]] ? PTR(res[a[0]]) : 0; }, { 0: { str: 8 } });
    calleeStand(fx, 1, 0x004a614d, 2, 'default', function (n, a) { return (a[0] ^ a[1] ^ 0x1100) >>> 0; }, { 0: { str: 8 }, 1: { str: 2 } });
    return fx;
  };

  // ---- logLine 0x004a0320: logWrite 0x004a0280 (thiscall + 5) stood in
  F.c3dw_logl = function () {
    const fx = ioEnv();
    calleeStand(fx, 0, 0x004a0280, 6, 'thiscall', function (n, a) { return (a[1] + a[2] * 2 + a[3] * 3 + a[4] * 4 + a[5] * 5) >>> 0; });
    return fx;
  };

  // ---- KeyTable::lookup 0x00488420: the four tables' words +0x104 (0x0083b274, 0x0083b394, 0x0083b4b4, 0x0083b5d4) decide which
  // are asked; KeyTable::find 0x00488310 (thiscall + 2) answers from a sequence
  function ktScene(seq) {
    return function () {
      const fx = ioEnv();
      const ENA = [0x0083b274, 0x0083b394, 0x0083b4b4, 0x0083b5d4];
      const PAT = [0xf, 0x0, 0x1, 0x2, 0x4, 0x8, 0x5, 0xa, 0x3, 0xc, 0x7, 0xe];
      const rows = PAT.map(function (m) { const r = {}; ENA.forEach(function (a, k) { r[a] = (m >> k) & 1 ? 0x77000000 + k : 0; }); return r; });
      rowsOn(fx, rows, PAT.map(function (_, i) { return i * 11 - 5; }));
      calleeStand(fx, 0, 0x00488310, 3, 'thiscall', function (n, a) { return seq[Math.min(n, seq.length - 1)] >>> 0; });
      return fx;
    };
  }
  F.c3dw_ktl_a = ktScene([-1, -1, 7, -1]);
  F.c3dw_ktl_b = ktScene([-1]);
  F.c3dw_ktl_c = ktScene([0, 5, 6, 7]);
  F.c3dw_ktl_d = ktScene([-3, 2, -1, 9]);

  // ---- MsgBox::freeLabels 0x00490c30: the label pointers [0x0083b994] / [0x0083b998] go to the CRT free (recorded)
  F.c3dw_mbfl = function () {
    const fx = ioEnv();
    heapOn(fx, 0x1000);
    fx.lb = alloc(0x40 * 4); fx.obj = fx.lb;
    const P = [[0, 0], [1, 0], [0, 2], [1, 2], [3, 3], [2, 1], [0, 3], [1, 1], [0, 0], [3, 0], [2, 0], [1, 3]];
    const rows = P.map(function (p, i) {
      const r = {};
      r[0x0083b994] = p[0] ? PTR(fx.lb.add(0x40 * (p[0] - 1))) : 0;
      r[0x0083b998] = p[1] ? PTR(fx.lb.add(0x40 * (p[1] - 1))) : 0;
      r[0x0083b9b4] = 0x6b6b0000 + i; r[0x0083b9c4] = 0x7c7c0000 + i;
      return r;
    });
    rowsOn(fx, rows);
    return fx;
  };

  // ---- initGolferState 0x0046d040: 19 course records (first byte non-zero = used), the golfer records at 0x005794d0, random
  // pick 0x00421bc0 stood in (cdecl, 1 argument) with a sequence of record indexes
  F.c3dw_igs = function () {
    const fx = ioEnv();
    __freeze.save(ptr(0x005794d0), 0x9800);
    __freeze.save(ptr(0x0056a51c), 4); __freeze.save(ptr(0x0059ae7c), 4);
    ptr(0x005794d0).writeByteArray(new Array(0x9800).fill(0x5d));
    const rows = [];
    for (let i = 0; i < 12; i++) {
      const r = {};
      for (let j = 0; j < 19; j++) r['b' + (0x00577f40 - 0x208 * j)] = ((i + 1) * (j + 3) % 5) < (i % 4 + 1) ? 0 : 0x41 + j;
      r['b' + 0x005794d9] = 0xaa; r[0x0056a51c] = 0x1111 * i; r[0x0059ae7c] = 0x2222 * i;
      rows.push(r);
    }
    rows[0] = {}; for (let j = 0; j < 19; j++) rows[0]['b' + (0x00577f40 - 0x208 * j)] = 0;     // no course record in use
    rows[1] = {}; for (let j = 0; j < 19; j++) rows[1]['b' + (0x00577f40 - 0x208 * j)] = 1;     // every record in use
    rowsOn(fx, rows);
    const SEQ = [0, 1, 2, 3, 0, 5, 150, 151, 17, 64, 9, 1, 2, 3, 100, 40, 41];
    calleeStand(fx, 0, 0x00421bc0, 1, 'default', function (n, a) { return SEQ[n % SEQ.length]; });
    return fx;
  };

  // pumpLight with a frameTick stand-in that raises [0x00822d68] after pumpMessages cleared it: the 0x0045c164 test repeats the loop
  F.c3dw_pl_c = function () {
    const fx = pumpScene({ rows: PLROWS, peek: [0], rets: { [SHOW]: [0], [HIDE]: [0] } })();
    calleeStand(fx, 0, 0x00483cd0, 0, 'default', function (n, a) { if (n === 0) ptr(0x00822d68).writeU32(1); return 0x83cd0 + n; });
    return fx;
  };
  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
