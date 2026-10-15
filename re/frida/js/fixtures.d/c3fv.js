// Fixtures for batch c3fv (port readiness, 2026-10-15): gameMain 0x0045baf0 and the C2 callees of mainLoop.
// Reimplementations: shim/src/re/c3fv.cpp. Registry: re/frida/registry.d/c3fv.py. Purposes: log/c3/c3fv_purpose.md.
//
// Method (the loaders): every callee is a recording stand-in from re/frida/js/framemask.js (TID-gated: only the A/B
// thread is stood in, game threads run the real code). A pointer into the subject's frame (its Image) is logged as an
// (object ordinal, offset) token, so the two arms compare which object each call targets. The Image's surface pointer
// (+4) is written by the stood-in loadImage per a row mask, pointing at a fixture Surface whose vtable slots +0xd8
// (width) / +0xdc (height) are native stubs returning counters kept in compared memory ($cnt); with the row's `zap`
// flag set, the height stub also clears the Image's +4 (reaching the second NULL test 0x0044c54a). Rows are applied by
// the getter of fx.r<N>, which diff_hook resolves before the state snapshot.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { (globalThis.__diffKeepAlive || []).push(x); return x; };
  const le = function (v) { v = v >>> 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; };

  // cnt block (compared): +0 height counter, +4 width counter, +8 zap flag; imgp (not compared: a stack address that
  // differs between the arms) holds the Image whose +4 the height stub clears
  function fakeSurface() {
    const cnt = keep(__keepAlloc(0x20));
    cnt.writeByteArray(new Uint8Array(0x20).buffer);
    cnt.writeU32(0x100);
    cnt.add(4).writeU32(0x200);
    const imgp = keep(__keepAlloc(0x10));
    imgp.writeU32(0);
    const code = keep(__keepAlloc(Process.pageSize));
    Memory.protect(code, Process.pageSize, 'rwx');
    // height (+0xdc): mov eax,[cnt]; add [cnt],7; cmp [zap],0; je +13; mov ecx,[img]; mov [ecx+4],0; ret
    const h = [0xA1].concat(le(cnt.toUInt32()))
      .concat([0x81, 0x05], le(cnt.toUInt32()), le(7))
      .concat([0x83, 0x3D], le(cnt.add(8).toUInt32()), [0x00])
      .concat([0x74, 0x0D])
      .concat([0x8B, 0x0D], le(imgp.toUInt32()))
      .concat([0xC7, 0x41, 0x04, 0, 0, 0, 0])
      .concat([0xC3]);
    code.writeByteArray(h);
    // width (+0xd8): mov eax,[cnt+4]; add [cnt+4],3; ret
    const w = [0xA1].concat(le(cnt.add(4).toUInt32())).concat([0x81, 0x05], le(cnt.add(4).toUInt32()), le(3), [0xC3]);
    code.add(0x40).writeByteArray(w);
    // any other slot: return 0x5D000000 | slot (never reached by these bodies; a call would show in the args)
    const vt = keep(__keepAlloc(0x200));
    for (let s = 0; s < 0x200; s += 4) {
      const stub = code.add(0x80 + 2 * s);
      stub.writeByteArray([0xB8].concat(le(0x5D000000 | s), [0xC3]));
      vt.add(s).writePointer(stub);
    }
    vt.add(0xdc).writePointer(code);
    vt.add(0xd8).writePointer(code.add(0x40));
    const obj = keep(__keepAlloc(0x40));
    obj.writePointer(vt);
    return { cnt: cnt, surf: obj, imgp: imgp };
  }

  function rows(fx, list, apply) {
    list.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, {
        enumerable: true, configurable: true,
        get: function () { apply(row); return 0; },
      });
    });
    return fx;
  }

  // -------------------------------------------------------------------------------------- loadInfoScreenArt 0x0044bde0
  // Rows: mask bit (n % 32) of the n-th loadImage call (in this arm) leaves the fixture surface in the Image (+4), a 0
  // bit leaves NULL; zap = 1 makes every height query clear the Image's +4 (0x0044c54a's NULL side).
  const LISA_ROWS = [
    { mask: 0xffffffff, zap: 0 }, { mask: 0, zap: 0 }, { mask: 0x55555555, zap: 0 },
    { mask: 0xffffffff, zap: 1 }, { mask: 0xaaaaaaaa, zap: 1 }, { mask: 0x0f0f0f0f, zap: 0 },
    { mask: 0xf0f0f0f0, zap: 1 }, { mask: 0x33333333, zap: 0 },
  ];
  F.c3fv_lisa = function () { return lisa({}); };
  F.c3fv_lisa_raw = function () { return lisa({ raw: true }); };
  function lisa(o) {
    const S = fakeSurface();
    const cur = { mask: 0xffffffff, zap: 0 };
    const FM = __frameMask.begin({ size: 0x8000, raw: !!o.raw });
    const setSurf = function (self, args, n) {
      S.imgp.writeU32(self >>> 0);
      ptr(self).add(4).writePointer(((cur.mask >>> (n & 31)) & 1) ? S.surf : ptr(0));
    };
    FM.stand({ addr: 0x00474AE0, abi: 'thiscall', nargs: 0, region: { arg: 'this', size: 0x2b8 }, ret: 'this',
               write: function (self) { ptr(self).add(4).writePointer(ptr(0)); } });       // Image_ctor
    FM.stand({ addr: 0x00483800, abi: 'thiscall', nargs: 3, str: { 0: 6 }, ret: 1 });      // Surface::createFromId
    FM.stand({ addr: 0x00474DD0, abi: 'thiscall', nargs: 6, ret: 1 });                    // Surface_createFull
    FM.stand({ addr: 0x00475840, abi: 'thiscall', nargs: 5, str: { 0: 12 }, ret: 1, write: setSurf });   // loadImage
    FM.stand({ addr: 0x00473C60, abi: 'thiscall', nargs: 7, ret: 1 });                    // Surface_blitTakeDirty
    FM.stand({ addr: 0x00474CB0, abi: 'thiscall', nargs: 0, ret: 0 });                    // Image_releaseAll
    FM.stand({ addr: 0x00474C40, abi: 'thiscall', nargs: 0, retire: 'this', ret: 0 });    // Image_dtor
    if (globalThis.__freeze && __freeze.negOn) __freeze.negOn();
    const fx = Object.assign({ cnt: S.cnt }, FM.fx());
    return rows(fx, LISA_ROWS, function (row) {
      cur.mask = row.mask; cur.zap = row.zap;
      S.cnt.add(8).writeU32(row.zap);
    });
  }

  // ---------------------------------------------------------------------------------------------- REC: an ordered recorder
  // framemask.js allows 32 stubs per key; gameMain has 38 callees plus display slots. REC is the same TID-gated native
  // gate (framemask.js's stand, copied: the A/B thread enters JavaScript, every other thread jumps to the trampoline)
  // with one log for every stub, so the ORDER of all calls is compared. A value in the A/B thread's stack (TEB
  // DeallocationStack .. StackBase) is logged as 0xF5000000 (gameMain's only frame object is its Image).
  // Block (compared): +0 bytes used, +4 records, +8 FNV-1a over every record, +0x10 .. +0x10f call count per stub (64),
  // +0x110 records {addr, nwords, words...}. Counts and records live in the block, so both arms start equal.
  const REC_HEAD = 0x110;
  const recCode = keep(__keepAlloc(Process.pageSize));
  Memory.protect(recCode, Process.pageSize, 'rwx');
  recCode.writeByteArray([0x64, 0xa1, 0x04, 0x00, 0x00, 0x00, 0xc3, 0x90, 0x64, 0xa1, 0x0c, 0x0e, 0x00, 0x00, 0xc3, 0x90]);
  const recStackBase = keep(new NativeFunction(recCode, 'uint32', []));
  const recStackLow = keep(new NativeFunction(recCode.add(8), 'uint32', []));
  const recLive = {};
  let recTok = 1;
  function fnv(h, w) {
    for (let k = 0; k < 4; k++) { h ^= (w >>> (8 * k)) & 0xff; h = Math.imul(h, 0x01000193) >>> 0; }
    return h >>> 0;
  }
  function mkRec(size) {
    const blk = keep(__keepAlloc(size));
    blk.writeByteArray(new Uint8Array(size).buffer);
    blk.add(8).writeU32(0x811c9dc5);
    const R = { blk: blk, size: size, n: 0, tid: Process.getCurrentThreadId(), hi: recStackBase() >>> 0, lo: recStackLow() >>> 0,
                on: true };
    if (globalThis.__freeze) __freeze.atFlush(function () { R.on = false; });
    R.map = function (v) { v = v >>> 0; return (v >= R.lo && v < R.hi) ? 0xF5000000 : v; };
    R.record = function (addr, words) {
      let h = blk.add(8).readU32();
      h = fnv(h, addr >>> 0);
      words.forEach(function (w) { h = fnv(h, w >>> 0); });
      blk.add(8).writeU32(h);
      blk.add(4).writeU32(blk.add(4).readU32() + 1);
      const used = blk.readU32();
      const need = 8 + 4 * words.length;
      if (REC_HEAD + used + need <= size) {
        const e = blk.add(REC_HEAD + used);
        e.writeU32(addr >>> 0);
        e.add(4).writeU32(words.length);
        words.forEach(function (w, i) { e.add(8 + 4 * i).writeU32(w >>> 0); });
        blk.writeU32(used + need);
      }
    };
    // spec: addr, abi ('thiscall' | 'mscdecl' | 'stdcall'), nargs, ret (number | 'this' | function(self, args, n)),
    // write(self, args, n) side effects, log (default: the address) the record's tag, peek: [global addresses] whose
    // dwords are appended to the record (read at call time)
    R.stand = function (spec) {
      const idx = R.n++;
      if (idx >= 64) throw new Error('REC: more than 64 stubs');
      const at = ptr(spec.addr), k = at.toString();
      if (recLive[k]) { try { Interceptor.revert(at); } catch (e) { /* gone */ } delete recLive[k]; Interceptor.flush(); }
      const abi = spec.abi || 'mscdecl';
      const thiscall = abi === 'thiscall';
      const types = (thiscall ? ['uint32'] : []).concat(Array(spec.nargs || 0).fill('uint32'));
      let orig = null;
      const cb = keep(new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments).map(function (x) { return x >>> 0; });
        if (!R.on) return orig.apply(null, arguments);
        const self = thiscall ? a[0] : 0;
        const args = thiscall ? a.slice(1) : a;
        const cnt = 0x10 + 4 * idx;
        const n = blk.add(cnt).readU32();
        blk.add(cnt).writeU32(n + 1);
        const words = a.map(R.map);
        (spec.peek || []).forEach(function (g) { words.push(ptr(g).readU32()); });
        R.record(spec.log === undefined ? spec.addr : spec.log, words);
        if (spec.write) spec.write(self, args, n);
        const r = spec.ret;
        if (r === 'this') return self >>> 0;
        if (typeof r === 'function') return r(self, args, n) >>> 0;
        return (r === undefined ? 0 : r) >>> 0;
      }, 'uint32', types, abi));
      const stub = keep(__keepAlloc(Process.pageSize));
      Memory.protect(stub, Process.pageSize, 'rwx');
      stub.writeByteArray([0xEB, 0xFE]);
      const tidAddr = stub.add(0x38), origAddr = stub.add(0x3c);
      const body = [0x50, 0x64, 0xa1, 0x24, 0x00, 0x00, 0x00]
        .concat([0x3b, 0x05], le(tidAddr.toUInt32()))
        .concat([0x58])
        .concat([0x0f, 0x85], le(stub.add(0x2a).sub(stub.add(0x16)).toUInt32()))
        .concat([0xe9], le(cb.sub(stub.add(0x1b)).toUInt32()));
      stub.add(0x02).writeByteArray(body);
      stub.add(0x2a).writeByteArray([0xff, 0x25].concat(le(origAddr.toUInt32())));
      tidAddr.writeU32(R.tid >>> 0);
      let tramp;
      try {
        tramp = Interceptor.replaceFast(at, stub);
      } catch (e) {
        Interceptor.revert(at);
        Interceptor.flush();
        tramp = Interceptor.replaceFast(at, stub);
      }
      orig = keep(new NativeFunction(tramp, 'uint32', types, abi));
      origAddr.writePointer(tramp);
      const token = recTok++;
      recLive[k] = token;
      Interceptor.flush();
      stub.add(1).writeU8(0x00);
      setTimeout(function () {
        if (recLive[k] !== token) return;
        try { Interceptor.revert(at); } catch (e) { /* gone */ }
        delete recLive[k];
        Interceptor.flush();
      }, 0);
      return idx;
    };
    return R;
  }

  // --------------------------------------------------------------------------------------------- gameMain 0x0045baf0
  // Every callee except scaleX 0x00404970 (a pure C4 leaf, run for real), the two user32 imports it calls through its IAT
  // (stood in at the user32 exports, so the original and the reimplementation, which both call through the IAT slots
  // 0x004ba1fc / 0x004ba1e4, reach the same stand-in) and a fixture display swapped into 0x0083ad50 (guarded by
  // recdisplay.js: other threads reach the real display) whose slots +0x58 / +0x24 / +0x20 / +0x10 are recorded code
  // stubs. Row: engine (engineInit result), mode (slot +0x58 result), create (Window::create result), mem (malloc
  // returns the fixture table or NULL), icon (LoadImageA result base).
  const GM_ROWS = [
    { engine: 0, mode: 0, create: 0, mem: 1, icon: 0x1000 },
    { engine: 1, mode: 0, create: 0, mem: 1, icon: 0x1000 },
    { engine: 0, mode: 1, create: 0, mem: 1, icon: 0x1000 },
    { engine: 0, mode: 0, create: 1, mem: 1, icon: 0x1000 },
    { engine: 0, mode: 0, create: 0, mem: 0, icon: 0x2000 },
    { engine: -1, mode: 0, create: 0, mem: 1, icon: 0x3000 },
    { engine: 0, mode: -5, create: 0, mem: 0, icon: 0x4000 },
    { engine: 0, mode: 0, create: 0x80000000, mem: 1, icon: 0x5000 },
    { engine: 0, mode: 0, create: 0, mem: 1, icon: 0x6000 },
    { engine: 0, mode: 0, create: 0, mem: 0, icon: 0x7000 },
  ];
  // written globals (also the registry's state; saved under the frozen first thread and restored after the key)
  const GM_REGIONS = [[0x004E42C8, 0x2C8], [0x0083AB6C, 0x0C], [0x0083ABDC, 4], [0x0083AD44, 4], [0x0083AFD8, 0x0C],
                      [0x0083B604, 8], [0x0083B9D0, 4], [0x0083B9F0, 4], [0x00822C74, 4], [0x00822C8C, 8]];
  F.c3fv_gm = function () {
    const cur = { row: GM_ROWS[0] };
    const R = mkRec(0x4000);
    const table = keep(__keepAlloc(0x20000));
    table.writeByteArray(new Uint8Array(0x20000).buffer);
    const S = function (addr, abi, nargs, ret, extra) { R.stand(Object.assign({ addr: addr, abi: abi, nargs: nargs, ret: ret }, extra || {})); };
    S(0x004855B0, 'mscdecl', 3, function () { return cur.row.engine; });          // engineInit
    S(0x00481870, 'mscdecl', 1, 0);                                               // allocResampleTable
    S(0x004806C0, 'thiscall', 9, function () { return cur.row.create; });         // Window::create
    S(0x00485740, 'mscdecl', 0, 0);                                               // engineShutdown
    S(0x0047D840, 'mscdecl', 1, 0);                                               // setDragTarget
    S(0x004808C0, 'thiscall', 1, 0);                                              // Window::paint
    S(0x0047B670, 'thiscall', 1, 0);                                              // Window::show
    S(0x00483BD0, 'mscdecl', 0, 0);                                               // pumpInput483bd0
    S(0x0048DE00, 'mscdecl', 1, 0);                                               // MsgBox::setLabelA
    S(0x0048DE90, 'mscdecl', 1, 0);                                               // MsgBox::setLabelB
    S(0x00490C80, 'mscdecl', 4, 0);                                               // MsgBox::setButtonA
    S(0x00490CC0, 'mscdecl', 4, 0);                                               // MsgBox::setColorA
    S(0x00490D20, 'mscdecl', 4, 0);                                               // MsgBox::setColorB
    S(0x00490CF0, 'mscdecl', 3, 0);                                               // MsgBox::setButtonB
    S(0x00483E90, 'mscdecl', 2, 0);                                               // initSound
    S(0x00467110, 'mscdecl', 0, 0);                                               // seedFromTime
    S(0x00483800, 'thiscall', 3, 1);                                              // Surface::createFromId
    S(0x00483850, 'thiscall', 4, 1);                                              // Surface::createSized
    S(0x00474DD0, 'thiscall', 6, 1);                                              // Surface_createFull
    S(0x004789F0, 'thiscall', 1, 0);                                              // Widget_applyPalette
    S(0x0047B310, 'thiscall', 0, 0);                                              // Window::center
    S(0x00474AE0, 'thiscall', 0, 'this');                                         // Image_ctor
    S(0x00483060, 'thiscall', 0, 0);                                              // Cache483060::detach
    S(0x004A504F, 'mscdecl', 1, function () { return cur.row.mem ? table.toUInt32() : 0; });   // _malloc
    S(0x00461830, 'mscdecl', 0, 0);                                               // build16BitColorTable
    S(0x0043CD70, 'mscdecl', 0, 0);                                               // showTitleScreen
    S(0x00448220, 'mscdecl', 0, 0);                                               // loadSoundTable
    S(0x00483190, 'thiscall', 4, 0);                                              // allocColorRange
    S(0x004832E0, 'thiscall', 1, 0);                                              // startColorCycle
    S(0x0040F5C0, 'mscdecl', 0, 0);                                               // mainLoop
    S(0x004490B0, 'mscdecl', 0, 0);                                               // shutdownSoundSystem
    S(0x00481B20, 'mscdecl', 0, 0);                                               // freeResampleTable
    S(0x00449860, 'mscdecl', 0, 0);                                               // freeTerrain
    S(0x0044CCE0, 'mscdecl', 0, 0);                                               // resetAllPanels
    S(0x00474C40, 'thiscall', 0, 0);                                              // Image_dtor
    S(Process.getModuleByName('user32.dll').getExportByName('LoadImageA'), 'stdcall', 6,
      function (self, args, n) { return cur.row.icon + n; }, { log: 0x7FFF0001 });
    S(Process.getModuleByName('user32.dll').getExportByName('PostMessageA'), 'stdcall', 4, 1, { log: 0x7FFF0002 });
    // fixture display: slots +0x58 (3 args), +0x24, +0x20, +0x10 are code stubs (stood in, so recorded); any other slot
    // returns 0x5D000000 | slot unrecorded (none is called by these bodies: a call would show in a later argument)
    const code = keep(__keepAlloc(Process.pageSize));
    Memory.protect(code, Process.pageSize, 'rwx');
    const vt = keep(__keepAlloc(0x400));
    for (let s = 0; s < 0x400; s += 4) {
      const st = code.add(0x100 + 2 * s);
      st.writeByteArray([0xB8].concat(le(0x5D000000 | s), [0xC3]));
      vt.add(s).writePointer(st);
    }
    [[0x58, 3], [0x24, 0], [0x20, 0], [0x10, 0]].forEach(function (sl, i) {
      const st = code.add(0x10 * i);
      st.writeByteArray([0x31, 0xC0, 0xC2].concat([4 * sl[1], 0]).concat([0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90]));
      vt.add(sl[0]).writePointer(st);
    });
    const disp = keep(__keepAlloc(0x40));
    disp.writePointer(vt);
    S(code.add(0x00), 'thiscall', 3, function () { return cur.row.mode; }, { log: 0x7FFE0058 });
    S(code.add(0x10), 'thiscall', 0, function (self, a, n) { return 0x11110000 + n; }, { log: 0x7FFE0024 });
    S(code.add(0x20), 'thiscall', 0, function (self, a, n) { return 0x22220000 + n; }, { log: 0x7FFE0020 });
    S(code.add(0x30), 'thiscall', 0, 0, { log: 0x7FFE0010 });
    __freeze.thread();
    GM_REGIONS.forEach(function (r) { __freeze.save(ptr(r[0]), r[1]); });
    __freeze.save(ptr(0x0083AD50), 4);
    __recDisplay.guard(disp);
    ptr(0x0083AD50).writePointer(disp);
    if (__freeze.negOn) __freeze.negOn();
    const fx = { rec: R.blk, table: table };
    return rows(fx, GM_ROWS, function (row) { cur.row = row; });
  };

  // --------------------------------------------------------------------------------------- loadInterfaceArt 0x00442180
  // Every callee on REC (genNoise, clearBuffers, Random::range, loadWorldAssets, listMatchingFiles, the Image and
  // Surface methods, loadImage, Surface_blit, loadFlic); loadFlic / loadImage / Surface_blit records also carry the
  // dword 0x00820b6c read at the call, so the stores interleaved with the calls are compared. Random::range returns
  // (5 n + seed) % arg (n = this stub's call index in the arm), a value in the callee's range that depends only on the
  // row and the call count. listMatchingFiles returns the row's name count and writes the names (100-byte records) at
  // 0x0080b130. Rows: `full` is the argument the getter returns.
  const LIA_REGIONS = [
    [0x00575AB0, 0x28A0], [0x005794B8, 0x9C00], [0x00585850, 0x1300], [0x005409AC, 0x3E8], [0x0051B388, 0x3E8],
    [0x0053E63C, 0x3E8], [0x00520640, 0x3E8], [0x00568600, 0x3E8], [0x0059DEA0, 0x80], [0x00584210, 0x7D0],
    [0x0056AE70, 0x1700], [0x005422F8, 0xF0], [0x005685F0, 4], [0x0059AE80, 0x1C8], [0x0053CAF0, 0x1388],
    [0x0053EA24, 0x9C4], [0x005A4998, 0xA2C], [0x0059BFC4, 0x20], [0x0058BCB8, 0x1000], [0x0059B730, 4],
    [0x0059AE7C, 4], [0x00543D10, 0x19A28], [0x005A9370, 0x938], [0x0053F3E8, 0x938], [0x008156D0, 0x733C],
    [0x00820B6C, 4], [0x00820B70, 0x190], [0x0059B76C, 8], [0x0051A068, 0x100], [0x0080B130, 0x1C00],
  ];
  function liaNames(spec) {          // spec: list of [count, first letter cycle] -> names "<L><index>"
    const out = [];
    spec.forEach(function (g) { for (let i = 0; i < g[0]; i++) out.push(g[1][i % g[1].length] + 'n' + out.length); });
    return out;
  }
  const LIA_ROWS = [
    { full: 0, seed: 1, names: [] },
    { full: 1, seed: 2, names: [] },
    { full: 1, seed: 3, names: liaNames([[8, 'FmMfXF']]) },
    { full: 1, seed: 4, names: liaNames([[55, 'F'], [15, 'M']]) },
    { full: 0, seed: 7, names: [] },
    { full: 1, seed: 11, names: liaNames([[3, 'fmz']]) },
    { full: -1, seed: 5, names: liaNames([[2, 'Mf'], [60, 'm']]) },
  ];
  F.c3fv_lia = function () {
    const cur = { row: LIA_ROWS[0] };
    const R = mkRec(0x30000);
    const PEEK = [0x00820B6C];
    const S = function (addr, abi, nargs, ret, extra) { R.stand(Object.assign({ addr: addr, abi: abi, nargs: nargs, ret: ret }, extra || {})); };
    S(0x004673E0, 'mscdecl', 0, 0);                                               // genNoise
    S(0x0045B880, 'mscdecl', 0, 0);                                               // clearBuffers
    S(0x0045C1E0, 'thiscall', 1, function (self, args, n) { return (5 * n + cur.row.seed) % args[0]; });   // Random::range
    S(0x0043DBE0, 'mscdecl', 0, 0);                                               // loadWorldAssets
    S(0x0043D2A0, 'mscdecl', 3, function () { return cur.row.names.length; },    // listMatchingFiles
      { write: function () {
          cur.row.names.forEach(function (nm, i) { ptr(0x0080B130 + 100 * i).writeUtf8String(nm); });
        } });
    S(0x00474AE0, 'thiscall', 0, 'this', { write: function (self) { ptr(self).add(4).writePointer(ptr(0)); } });  // Image_ctor
    S(0x00474DD0, 'thiscall', 6, 1);                                              // Surface_createFull
    S(0x00475840, 'thiscall', 5, 1, { peek: PEEK });                              // loadImage
    S(0x00473BF0, 'thiscall', 7, 1, { peek: PEEK });                              // Surface_blit
    S(0x0043D740, 'mscdecl', 5, 1, { peek: PEEK });                               // loadFlic
    S(0x00474C40, 'thiscall', 0, 0);                                              // Image_dtor
    __freeze.thread();
    LIA_REGIONS.forEach(function (r) { __freeze.save(ptr(r[0]), r[1]); });
    if (__freeze.negOn) __freeze.negOn();
    const fx = { rec: R.blk };
    LIA_ROWS.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true,
        get: function () { cur.row = row; return row.full; } });
    });
    return fx;
  };

  // --------------------------------------------------------------------------------------- drawWorldObjects 0x00430360
  // Every callee on REC with a return that depends only on its arguments, the row and its call index n in the arm:
  // Random::range(a) = 0 when n % z == 0 (row.z), else (7 n + seed) % a; distance = (|dx| + |dy|) / 2; tileBlocked =
  // (3 tx + ty) % 5 == 0; tileKind = (7 (x >> 10) + 3 (y >> 10) + tk) % 24 (a range holding 4, 0x11, 0x14 and the home
  // kinds; 0x11 from the row's bandX / bandY on); directionTo = ((dx >> 8) ^ (dy >> 8)) & 7; worldToScreen writes (x >> 6, y >> 6) and returns 0 when
  // ((x ^ y) >> 10) % 7 == 0; cellAt returns 0x40000000 | id << 12 | frame << 4 | view; clamp clamps; heightBlend = (tx +
  // ty) & 3; __itoa writes the decimal string. Tables (frozen first thread, saved, compared): the 128 animal records and
  // record -1's last bytes (0x00572cac..0x005736b0), the flyers 0x005419d0, the golfers 0x005794b8 (152 x 0x100), the frame counts
  // 0x0053f3e8, tiles 0x005722e8, type table 0x00578350, g_flags 0x0059e7b8, 0x005a5a00, the date 0x00834170, camera
  // 0x005685f4, 0x005a60b0, the divisors 0x0053fa6c / 0x0053fc00 / 0x0053fc04, 0x005a9cc8, 0x0051a068, 0x0058a528.
  const DWO_REGIONS = [[0x00572CAC, 0xA04], [0x005419D0, 0x40], [0x005794B8, 0x9800], [0x0053F3E8, 0x4D0],
                       [0x005722E8, 0x9C4], [0x00578350, 0x6F0], [0x0059E7B8, 4], [0x005A5A00, 4], [0x00834170, 4],
                       [0x005685F4, 4], [0x005A60B0, 4], [0x0053FA6C, 4], [0x0053FC00, 8], [0x005A9CC8, 4],
                       [0x0051A068, 0x40], [0x0058A528, 0x10]];
  // rows (one JSON literal, also read by log/c3/alt/c3fv_dwo_cov.py): optional `flyers` [kind, x, y, counter, dir] x 4
  // replace the generated flyers; `bandX` / `bandY` make tileKind return 0x11 (water) from that tile column / row on;
  // `cluster` pairs the animals from index 64 (odd ones follow the even one at 0, 1 or 2 times the offset)
  const DWO_ROWS = /*DWO_ROWS*/[
    {"seed": 1, "z": 7, "gf": 0, "a5": 32, "date": 9216, "cam": 3, "v8": 8, "lead0": 1, "tk": 0, "fly": [0, 1, 2, 3], "flyX": 1, "gol": 6},
    {"seed": 2, "z": 3, "gf": 32, "a5": 32, "date": 9344, "cam": 6, "v8": 7, "lead0": 255, "tk": 5, "fly": [0, 1, 2, 3], "flyX": 0, "gol": 9},
    {"seed": 3, "z": 1, "gf": 0, "a5": 32, "date": 4096, "cam": 1, "v8": 8, "lead0": 1, "tk": 11, "fly": [3, 3, 1, 1], "flyX": 0, "gol": 3},
    {"seed": 4, "z": 13, "gf": 4, "a5": 32, "date": 9220, "cam": 2, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 2, 1, 3], "flyX": 1, "gol": 5,
     "flyers": [[1, 20480, 21504, 16, 2], [0, 12288, 8192, 16, 0], [2, 4096, 8192, 40, 0], [3, 24576, 16384, 0, 4]]},
    {"seed": 5, "z": 2, "gf": 0, "a5": 0, "date": 9217, "cam": 5, "v8": 8, "lead0": 1, "tk": 8, "fly": [2, 0, 3, 1], "flyX": 1, "gol": 2},
    {"seed": 6, "z": 5, "gf": 32, "a5": 32, "date": 12288, "cam": 7, "v8": 1, "lead0": 255, "tk": 17, "fly": [1, 0, 0, 2], "flyX": 1, "gol": 8, "cluster": 1280},
    {"seed": 7, "z": 4, "gf": 0, "a5": 32, "date": 9224, "cam": 0, "v8": 8, "lead0": 1, "tk": 20, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 4, "cluster": 1280},
    {"seed": 8, "z": 9, "gf": 0, "a5": 32, "date": 11264, "cam": 4, "v8": 8, "lead0": 1, "tk": 2, "fly": [1, 1, 1, 1], "flyX": 1, "gol": 7, "cluster": 2048},
    {"seed": 9, "z": 2, "gf": 0, "a5": 32, "date": 9216, "cam": 2, "v8": 8, "lead0": 1, "tk": 4, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 3, "cluster": 1280,
     "flyers": [[0, 52160, 8192, 0, 0], [0, 12288, 52288, 16, 0], [0, 20480, 20480, 40, 0], [4, 256, 256, 0, 0]]},
    {"seed": 10, "z": 3, "gf": 0, "a5": 32, "date": -4096, "cam": 5, "v8": 8, "lead0": 1, "tk": 6, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2,
     "flyers": [[0, 8192, 12288, 0, 0], [0, 16384, 21504, 0, 0], [0, 28672, 9216, 30, 0], [255, 0, 0, 0, 0]]},
    {"seed": 11, "z": 7, "gf": 0, "a5": 32, "date": 9232, "cam": 3, "v8": 8, "lead0": 1, "tk": 9, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2,
     "flyers": [[0, 20480, 21504, 16, 0], [2, 20480, 21504, 16, 0], [3, 20480, 21504, 16, 2], [3, 4096, 4096, 0, 0]]},
    {"seed": 12, "z": 1, "gf": 0, "a5": 32, "date": 4096, "cam": 6, "v8": 8, "lead0": 1, "tk": 1, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2,
     "flyers": [[0, -1, 0, 0, 0], [2, -1, 0, 0, 0], [3, -1, 0, 0, 0], [1, -1, 0, 0, 0]]},
    {"seed": 13, "z": 11, "gf": 0, "a5": 32, "date": 9248, "cam": 1, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2,
     "flyers": [[3, 52208, 20480, 0, 2], [3, 32, 20480, 0, 6], [3, 20480, 52208, 0, 4], [3, 20480, 32, 0, 0]]},
    {"seed": 14, "z": 11, "gf": 0, "a5": 32, "date": 9248, "cam": 1, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2,
     "flyers": [[2, 52208, 20480, 40, 0], [2, 20480, 52480, 40, 0], [0, 20480, 9216, 0, 0], [0, 33792, 9216, 0, 0]]},
    {"seed": 15, "z": 50, "gf": 0, "a5": 32, "date": 9224, "cam": 2, "v8": 7, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2,
     "bandX": 30, "bandY": 30,
     "flyers": [[1, 30208, 30208, 0, 0], [1, 30208, 30208, 0, 6], [1, 12288, 256, 0, 0], [1, 30208, 20480, 0, 4]]},
    {"seed": 16, "z": 50, "gf": 0, "a5": 32, "date": 9224, "cam": 5, "v8": 8, "lead0": 1, "tk": 7, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2,
     "bandX": 30,
     "flyers": [[1, 30208, 20480, 0, 0], [1, 4096, 4096, 0, 2], [1, 8192, 640, 0, 4], [1, 30208, 24576, 0, 4]]},
    {"z": 11, "gf": 0, "a5": 32, "cam": 1, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2, "seed": 17, "date": 8448, "flyers": [[0, 2560, 2560, 0, 0], [0, 52218, 8192, 40, 0], [2, 12288, 12288, 16, 0], [1, 8192, 256, 0, 4]]},
    {"z": 11, "gf": 0, "a5": 32, "cam": 1, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2, "seed": 17, "date": 8192, "flyers": [[0, 2560, 8704, 0, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0]]},
    {"z": 11, "gf": 0, "a5": 32, "cam": 1, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2, "seed": 17, "date": 8960, "flyers": [[0, 2560, 6656, 0, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0]]},
    {"z": 11, "gf": 0, "a5": 32, "cam": 1, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2, "seed": 17, "date": 8704, "flyers": [[0, 2560, 4608, 0, 0], [0, 2560, 4608, 5, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0]]},
    {"z": 11, "gf": 0, "a5": 32, "cam": 1, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2, "seed": 17, "date": 8316, "flyers": [[0, 2560, 13824, 0, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0]]},
    {"z": 11, "gf": 0, "a5": 32, "cam": 1, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2, "seed": 19, "date": 8800, "flyers": [[0, 2560, 37376, 0, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0]]},
    {"z": 11, "gf": 0, "a5": 32, "cam": 1, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2, "seed": 19, "date": 8192, "flyers": [[0, 2560, 9728, 0, 0], [0, 2560, 25088, 0, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0]]},
    {"z": 11, "gf": 0, "a5": 32, "cam": 1, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2, "seed": 19, "date": 8424, "flyers": [[0, 2560, 14848, 0, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0]]},
    {"z": 11, "gf": 0, "a5": 32, "cam": 1, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2, "seed": 20, "date": 9217, "flyers": [[0, 20480, 8704, 16, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0]]},
    {"z": 11, "gf": 0, "a5": 32, "cam": 1, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2, "seed": 21, "date": 10080, "flyers": [[0, 20480, 8704, 0, 0], [0, 24576, 8704, 96, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0]]},
    {"z": 1, "gf": 0, "a5": 32, "cam": 1, "v8": 8, "lead0": 1, "tk": 3, "fly": [0, 0, 0, 0], "flyX": 1, "gol": 2, "seed": 22, "date": 9224, "flyers": [[1, 12288, 12288, 0, 2], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0], [255, 0, 0, 0, 0]]}
  ]/*END*/;
  function dwoSetup(row) {
    const w32 = function (a, v) { ptr(a).writeS32(v | 0); };
    const w16 = function (a, v) { ptr(a).writeS16(v); };
    const w8 = function (a, v) { ptr(a).writeU8(v & 0xff); };
    const S = row.seed;
    // record -1 (the leader of an animal whose leader index is -1): its x / y / flags / leader / state are the last 16
    // bytes of the tile table (0x00572c9c..0x00572cab, written below with the tiles); its frame, direction and kind
    // bytes 0x00572cac..0x00572cae lie past the table (row.lead0: 0xff = no virtual leader)
    w8(0x00572CAC, 0); w8(0x00572CAD, S & 7); w8(0x00572CAE, row.lead0); w8(0x00572CAF, 0);
    const states = [0x103, 0x10C, 0x115, 0x11E];
    for (let i = 0; i < 128; i++) {
      const b = 0x00572CB0 + 0x14 * i;
      const k = (i * 5 + S) % 12 - 1;                       // -1 .. 10 (>= 9 is reset to 1)
      w32(b, ((i * 0x4D3 + S * 0x111) % 0xC000) + 0x200);
      w32(b + 4, ((i * 0x2B9 + S * 0x95) % 0xC000) + 0x200);
      let fl = [0, 1, 2, 3, -20, -1, -7, 0][(i + S) % 8];
      w32(b + 8, fl);
      w16(b + 0xC, (i % 5 === 0) ? -1 : ((i % 5 === 1) ? -1 : (i + 3 * S) % 128));
      w16(b + 0xE, states[(i + 2 * S) % 4]);
      w8(b + 0x10, (i * 3 + S) % 9);
      w8(b + 0x11, (i + S) % 8);
      w8(b + 0x12, k);
      w8(b + 0x13, 0);
    }
    if (row.cluster) {                                      // pairs from 64: odd i follows i - 1 at 0 / 1 / 2 offsets
      for (let i = 64; i < 128; i++) {
        const b = 0x00572CB0 + 0x14 * i;
        const base = 0x4000 + (i >> 1) * 0x100;
        w32(b, base + ((i & 1) ? row.cluster * ((i >> 1) % 3) : 0));
        w32(b + 4, 0x6000);
        if (i & 1) w16(b + 0xC, i - 1);
      }
    }
    for (let j = 0; j < 4; j++) {
      const b = 0x005419D0 + 0x10 * j;
      if (row.flyers) {
        const f = row.flyers[j];
        w32(b, f[1]); w32(b + 4, f[2]); w32(b + 8, f[3]); w8(b + 0xC, f[4]); w8(b + 0xD, f[0]);
        continue;
      }
      const active = row.flyX && ((j + S) % 3 !== 0);
      w32(b, active ? 0x1000 * (j + 1) + 0x80 * S : -1);
      w32(b + 4, active ? 0x900 * (j + 2) + (S & 1 ? 0xCC80 - 0x900 * 6 : 0) : 0);
      w32(b + 8, [0, 0x10, 0x28, 0x40][(j + S) % 4]);
      w8(b + 0xC, [0, 2, 4, 6, 1][(j + S) % 5]);
      w8(b + 0xD, row.fly[j]);
    }
    for (let g = 0; g < 152; g++) {
      const b = 0x005794B8 + 0x100 * g;
      for (let o = 0; o < 0x100; o += 4) w32(b + o, 0);
      const on = g < row.gol;
      w8(b + 0x21, on ? 1 : 0xff);
      w32(b, ((g * 0x1357 + S * 0x400) % 0xC000));
      w32(b + 4, ((g * 0x0A31 + S * 0x300) % 0xC000));
      w32(b + 0xE4, (g % 3) - 1);
      w32(b + 0xDC, [0x10, 0x95, 0x96, 0x200][g % 4]);
      w32(b + 0xD4, ((g * 0x777) % 0xC000)); w32(b + 0xD8, ((g * 0x555) % 0xC000));
      w8(b + 0x84, g % 4 === 0 ? 1 : 0);
      w8(b + 0x85, g % 5 === 0 ? 0x27 : 3);
    }
    for (let k = 0; k < 0x134; k++) w32(0x0053F3E8 + 4 * k, (k * 7 + S) % 5);
    for (let t = 0; t < 0x9C4; t++) w8(0x005722E8 + t, (t * t * 7 + t * 13 + S * 3) % 0x25);   // 50 x 50
    for (let e = 0; e < 0x25; e++) {
      for (let o = 0; o < 0x30; o += 4) w32(0x00578350 + 0x30 * e + o, 0);
      w8(0x00578350 + 0x30 * e + 0x22, (e % 4) - 1);
      w8(0x00578350 + 0x30 * e + 0x26, [0x0D, 0x11, 0, 3, 7][e % 5]);
    }
    w32(0x0059E7B8, row.gf); w32(0x005A5A00, row.a5); w32(0x00834170, row.date); w32(0x005685F4, row.cam);
    w32(0x005A60B0, row.v8); w32(0x0053FA6C, 3); w32(0x0053FC00, 5); w32(0x0053FC04, 4);
    w32(0x005A9CC8, 0); for (let o = 0; o < 0x40; o += 4) w32(0x0051A068 + o, 0); for (let o = 0; o < 0x10; o += 4) w32(0x0058A528 + o, 0);
  }
  F.c3fv_dwo = function () {
    const cur = { row: DWO_ROWS[0] };
    const R = mkRec(0x40000);
    const S = function (addr, abi, nargs, ret, extra) { R.stand(Object.assign({ addr: addr, abi: abi, nargs: nargs, ret: ret }, extra || {})); };
    const s32 = function (v) { return v | 0; };
    S(0x0045C1E0, 'thiscall', 1, function (self, a, n) { return (n % cur.row.z === 0) ? 0 : (7 * n + cur.row.seed) % a[0]; });
    S(0x0040ACD0, 'mscdecl', 2, function (self, a) { return (Math.abs(s32(a[0])) + Math.abs(s32(a[1]))) >> 1; });
    S(0x0040BF60, 'mscdecl', 2, function (self, a) { return ((3 * s32(a[0]) + s32(a[1])) % 5 === 0) ? 1 : 0; });
    S(0x0040BFA0, 'mscdecl', 2, function (self, a) {
      const tx = s32(a[0]) >> 10, ty = s32(a[1]) >> 10;
      if ((cur.row.bandX && tx >= cur.row.bandX) || (cur.row.bandY && ty >= cur.row.bandY)) return 0x11;
      const v = (7 * tx + 3 * ty + cur.row.tk) % 24; return v < 0 ? v + 24 : v; });
    S(0x004671A0, 'mscdecl', 2, function (self, a) { return ((s32(a[0]) >> 8) ^ (s32(a[1]) >> 8)) & 7; });
    S(0x00467A00, 'mscdecl', 3, 0);                                                // emitGolferThought
    S(0x00405A00, 'mscdecl', 3, 0);                                                // playHabitatSound
    S(0x0042FB90, 'mscdecl', 5, function (self, a) {                               // worldToScreen
      return ((((s32(a[0]) ^ s32(a[1])) >> 10) % 7) === 0) ? 0 : 1; },
      { write: function (self, a) { ptr(a[2]).writeS32(s32(a[0]) >> 6); ptr(a[3]).writeS32(s32(a[1]) >> 6); } });
    S(0x0043D6F0, 'mscdecl', 3, function (self, a) { return (0x40000000 | (a[0] << 12) | ((a[1] & 0xff) << 4) | (a[2] & 0xf)) >>> 0; });
    S(0x00474440, 'thiscall', 6, 0);                                               // Surface_drawSprite
    S(0x00462A30, 'mscdecl', 8, 0);                                                // enqueueSprite
    S(0x004AD425, 'mscdecl', 3, function (self, a) { return a[1]; },               // __itoa
      { write: function (self, a) { ptr(a[1]).writeUtf8String(String(s32(a[0]))); } });
    S(0x00404BC0, 'mscdecl', 4, 0);                                                // drawCenteredText2
    S(0x00467130, 'mscdecl', 3, function (self, a) { return Math.min(Math.max(s32(a[0]), s32(a[1])), s32(a[2])); });
    S(0x0040C500, 'mscdecl', 4, 0);                                                // playSoundAt
    S(0x0040C170, 'mscdecl', 3, function (self, a) { return (s32(a[0]) + s32(a[1])) & 3; });   // heightBlend
    __freeze.thread();
    DWO_REGIONS.forEach(function (r) { __freeze.save(ptr(r[0]), r[1]); });
    if (__freeze.negOn) __freeze.negOn();
    const fx = { rec: R.blk };
    return rows(fx, DWO_ROWS, function (row) { cur.row = row; dwoSetup(row); });
  };

  // ---------------------------------------------------------------------------------------- loadWorldAssets 0x0043dbe0
  // Globals it writes directly (also the registry's state regions, saved and restored under the frozen first thread):
  const LWA_REGIONS = [
    [0x005A34E0, 4],            // g_course_type (rows set it)
    [0x004C89A8, 4], [0x00820B6C, 4],
    [0x005A9A48, 0x148], [0x0081AC5C, 82 * 0x32],      // flic range 0x1b6.. (slots, flags)
    [0x005A9C28, 0x3C], [0x0081C3CC, 15 * 0x32],       // flic range 0x22e..
    [0x005A99B0, 0x90], [0x0081A4F0, 36 * 0x32],       // flic range 0x190..
    [0x00578350, 0x450],        // object-type table
    [0x004C273C, 0xA8],         // building names (0x004c273c / 0x004c2764 / 0x004c27a0 / 0x004c27c8)
  ];
  // Rows: ct = course type byte; slots = which flic slots hold an id (bit k of the pattern for slot k, else -1);
  // flipImg / flipFlic / flipFree = [trigger, new course type]: the stood-in loadImage (path), loadFlic (id) or
  // freeTable (id) changes the course type when called with the trigger, reaching the re-read branches 0x00440de7,
  // 0x00442037 and 0x004416e6.
  const LWA_ROWS = [
    { ct: 0, slots: 0 }, { ct: 1, slots: 0x5555 }, { ct: 2, slots: 0xa5a5 }, { ct: 3, slots: 0xffff },
    { ct: 4, slots: 0x0f0f }, { ct: 0xff, slots: 0 }, { ct: 0x80, slots: 0x1234 },
    { ct: 3, slots: 0x3333, flipImg: [0x004C98A4, 1], flipFlic: [0x19A, 2] },
    { ct: 3, slots: 0x00ff, flipImg: [0x004C98A4, 0], flipFlic: [0x19A, 0] },
    { ct: 2, slots: 0xffff, flipFree: [0x195, 0] }, { ct: 0, slots: 0xffff, flipFree: [0x1a0, 3] },
    { ct: 1, slots: 0xffff, flipFree: [0x190, 2] },
  ];
  F.c3fv_lwa = function () { return lwa({}); };
  F.c3fv_lwa_raw = function () { return lwa({ raw: true }); };
  function lwa(o) {
    const cur = { row: LWA_ROWS[0] };
    const FM = __frameMask.begin({ size: 0x10000, raw: !!o.raw });
    const flip = function (spec, v) { if (spec && spec[0] === v) ptr(0x005A34E0).writeU8(spec[1]); };
    FM.stand({ addr: 0x00474AE0, abi: 'thiscall', nargs: 0, region: { arg: 'this', size: 0x2b8 }, ret: 'this',
               write: function (self) { ptr(self).add(4).writePointer(ptr(0)); } });       // Image_ctor
    FM.stand({ addr: 0x00474DD0, abi: 'thiscall', nargs: 6, ret: 1 });                    // Surface_createFull
    FM.stand({ addr: 0x00475840, abi: 'thiscall', nargs: 5, ret: 1,                       // loadImage
               write: function (self, args) { flip(cur.row.flipImg, args[0]); } });
    FM.stand({ addr: 0x00473BF0, abi: 'thiscall', nargs: 7, ret: 1 });                    // Surface_blit
    FM.stand({ addr: 0x0043D740, abi: 'mscdecl', nargs: 5, ret: 1,                        // loadFlic
               write: function (self, args) { flip(cur.row.flipFlic, args[0]); } });
    FM.stand({ addr: 0x0043D670, abi: 'mscdecl', nargs: 1, ret: 0,                        // freeTable
               write: function (self, args) { flip(cur.row.flipFree, args[0]); } });
    FM.stand({ addr: 0x00474C40, abi: 'thiscall', nargs: 0, retire: 'this', ret: 0 });    // Image_dtor
    __freeze.thread();
    LWA_REGIONS.forEach(function (r) { __freeze.save(ptr(r[0]), r[1]); });
    if (__freeze.negOn) __freeze.negOn();
    const fx = Object.assign({}, FM.fx());
    return rows(fx, LWA_ROWS, function (row) {
      cur.row = row;
      ptr(0x005A34E0).writeU8(row.ct);
      const fill = function (base, n, id0) {
        for (let k = 0; k < n; k++) ptr(base + 4 * k).writeS32(((row.slots >>> (k % 16)) & 1) ? id0 + k : -1);
      };
      fill(0x005A9A48, 82, 0x1b6); fill(0x005A9C28, 15, 0x22e); fill(0x005A99B0, 36, 0x190);
    });
  }
})();
