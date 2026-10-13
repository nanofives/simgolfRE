// Fixtures for batch c3ey (round 32): jgld.dll / Terrain.dll leftovers and the exe's Terrain grid sweepers.
// Reimplementations: shim/src/re/c3ey.cpp. Registry: re/frida/registry.d/c3ey.py. Purposes and per-jcc tables:
// log/c3/c3ey_purpose.md.
//
// Every key freezes the game's first thread (re/frida/js/freeze.js) before it replaces anything, and everything a vector
// touches is the fixture's own memory or a game global saved by __freeze and written back before the thread resumes.
// Machinery copied (not shared) from earlier batches: calleeStand (c3dw), the _atexit recorder (c3ds), the longjmp
// catcher, the error / warning recorders and the primed jgld stderr (c3ds).
(function () {
  const F = {};
  const u32 = (v) => (typeof v === 'number' ? v : v.toUInt32()) >>> 0;
  const le = (v) => { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; };
  const zero = (p, n) => p.writeByteArray(new Uint8Array(n).buffer);
  const fill = (p, n, seed) => { for (let i = 0; i + 4 <= n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };
  function alloc(n) { const p = __keepAlloc(n); zero(p, n); return p; }
  const jgld = () => Process.getModuleByName('jgld.dll').base;
  const jva = (a) => jgld().add(a - 0x10000000);
  const tva = (a) => Process.getModuleByName('Terrain.dll').base.add(a - 0x10000000);

  let frozen = false;
  function freezeNow() {
    if (frozen) return;
    __freeze.thread();
    frozen = true;
    __freeze.atFlush(function () { frozen = false; });
    __freeze.negOn();             // every exported <Batch>NegCtl switch (only the negative-control build has one)
  }

  // ---- calleeStand (c3dw): a function replaced for one key on the A/B thread only; every call is appended to fx.clog
  // (word 0 = byte cursor, entries {va, n, values...} from +0x50; word 1+k = call count of stub k). Other threads reach
  // the real function. ret: number | array (by call number) | function (n, args) -> number.
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x800;
  function newClog() { const l = alloc(CLOG); l.writeU32(0x50); return l; }
  function calleeStand(fx, k, at, argc, abi, ret) {
    freezeNow();
    const tid = Process.getCurrentThreadId();
    const key = at.toString();
    if (standLive[key]) {
      try { Interceptor.revert(at); } catch (e) { /* already reverted */ }
      delete standLive[key];
      Interceptor.flush();
    }
    const argt = Array(argc).fill('uint32');
    const orig = new NativeFunction(at, 'uint32', argt, abi);
    const log = fx.clog;
    const va = u32(at);
    const fn = function () {
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, arguments);
      const a = Array.prototype.slice.call(arguments);
      const cell = log.add(4 + 4 * k);
      const n = cell.readU32();
      cell.writeU32(n + 1);
      const cur = log.readU32();
      const need = 8 + 4 * a.length;
      if (cur + need <= CLOG) {
        log.add(cur).writeU32(va);
        log.add(cur + 4).writeU32(a.length);
        a.forEach(function (v, i) { log.add(cur + 8 + 4 * i).writeU32(v >>> 0); });
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

  // ==================================================================================================================
  // golf_clean.exe: syncTiles 0x00449470 / rebuildTerrainGrid 0x00449540 over a FIXTURE Terrain object.
  //
  // [0x00820ed0] is pointed at our object (width +0x14, height +0x18, tiles of 0x248 bytes from +0x3a4) with the first
  // thread frozen, and written back before it resumes. The six Terrain.dll methods both functions call are stood in at
  // the exe's IAT slots (importstandin.js, thiscall: arg 0 is ecx = the object), so no Terrain.dll code runs on the A/B
  // thread and the game's own Terrain object and Terrain.dll's arrays are never touched; every call and its arguments
  // are the compared log ($implog). pumpInput483bd0 0x00483bd0 is stood in by calleeStand (its calls are $clog).
  // The exe grids the callees read are written per vector by a row getter ($rN, resolved before the state snapshot)
  // after __freeze.save: tileType 0x005722e8 and tileByte 0x0056988c (50 x 50 bytes), the corner table 0x0051b770 that
  // cornerHeights 0x0040bfe0 returns first (8 bytes per tile; every byte nonzero so its first arm answers), the wall
  // mask 0x005619a0, the type records' wall bytes 0x00578378 + 0x30 * type (types 0..0x16) and the flag words
  // 0x0053caf0 (50 x 50 shorts).
  // ==================================================================================================================
  const TER_GLOBAL = ptr('0x00820ed0');
  const G_TYPE = 0x005722e8, G_BYTE = 0x0056988c, G_CORNER = 0x0051b770, G_WALL = 0x005619a0, G_TDEF = 0x00578372,
        G_FLAGS = 0x0053caf0;
  const TER = {
    setType: ['?setType@Terrain@@QAEXPAVTile@@HH@Z', 4],
    getVariation: ['?getVariation@Terrain@@QAEHPAVTile@@@Z', 2],
    layPath: ['?layPath@Terrain@@QAEXPAVTile@@HH@Z', 4],
    setWall: ['?setWall@Terrain@@QAEXPAVTile@@HH_N@Z', 5],
    calcNormals: ['?calcNormals@Terrain@@QAEXPAVTile@@@Z', 2],
    elevateCorner: ['?elevateCorner@Terrain@@QAEXPAVTile@@H@Z', 3],
  };
  // stand-in results: functions of the arguments only (the tile pointer, our object's, and the integers)
  function tileIdx(fx, t) {
    const p = u32(t);
    if (p === 0) return 0xffff;
    return ((p - u32(fx.tobj) - 0x3a4) / 0x248) >>> 0;
  }
  function terImports(fx, use, logSize) {
    const imp = {};
    use.forEach(function (n) {
      const spec = { args: TER[n][1], abi: 'thiscall' };
      if (n === 'getVariation') spec.impl = function (a) { return a[1].isNull() ? 0x77 : a[1].add(0x30).readU8(); };
      else if (n === 'setType') spec.impl = function (a) { return ((u32(a[2]) << 8) ^ u32(a[3]) ^ (tileIdx(fx, a[1]) << 16)) >>> 0; };
      else if (n === 'layPath') spec.impl = function (a) { return (0x1a000000 + (tileIdx(fx, a[1]) << 8) + u32(a[2]) + u32(a[3])) >>> 0; };
      else spec.ret = 0x5e000000 + Object.keys(TER).indexOf(n);
      imp['Terrain.dll!' + TER[n][0]] = spec;
    });
    const s = __importStandIn({ module: 'golf_clean.exe', imports: imp, logSize: logSize });
    fx.implog = s.log;
    fx.forwarded = s.forwarded;
  }
  function saveGrids() {
    __freeze.save(ptr(G_TYPE), 2500);
    __freeze.save(ptr(G_BYTE), 2500);
    __freeze.save(ptr(G_CORNER), 20000);
    __freeze.save(ptr(G_WALL), 2500);
    __freeze.save(ptr(G_TDEF), 0x30 * 0x17);
    __freeze.save(ptr(G_FLAGS), 5000);
    __freeze.save(ptr('0x00820f2a'), 1);
  }
  // one grid pattern per row i (deterministic in x, y, i)
  const CORNER = [-3, 1, 2, 3, 4, 5, 3, 1, 6, 2, 1, 3, -1, 4, 2, 1];
  function putGrids(i, o) {
    const ty = new Uint8Array(2500), by = new Uint8Array(2500), wm = new Uint8Array(2500), cn = new Uint8Array(20000);
    const fl = new Uint8Array(5000);
    for (let x = 0; x < 50; x++) for (let y = 0; y < 50; y++) {
      const c = x * 50 + y;
      // tileType 0..0x16 with 0x11 on about one tile in eight
      ty[c] = ((x * 7 + y * 3 + i * 5) % 8 === 0) ? 0x11 : (x * 5 + y * 11 + i * 3) % 0x17;
      by[c] = (x * 3 + y * 13 + i) & 3;
      // wall mask: bits 0, 2, 4, 6 (wallHeight's dir c is the bit), on about one tile in four, plus junk odd bits
      wm[c] = ((x + 2 * y + i) % 4 === 0 ? (1 << (2 * ((x + y + i) & 3))) : 0) | ((x * y + i) % 13 === 0 ? 0x55 : 0) |
              ((x + y) % 5 === 0 ? 0x22 : 0);
      for (let k = 0; k < 8; k++) cn[c * 8 + k] = CORNER[(x * 3 + y * 5 + k * 7 + i * 11) % 16] & 0xff;
      // flag word: 0x20 on about one tile in three, 0x40 independently; other bits patterned
      const f = ((x + y * 3 + i) % 3 === 0 ? 0x20 : 0) | ((x * 2 + y + i) % 2 === 0 ? 0x40 : 0) | ((x ^ y) & 0x9f);
      fl[c * 2] = f; fl[c * 2 + 1] = (x + i) & 0xff;
    }
    if (o.noflags) fl.fill(0);
    ptr(G_TYPE).writeByteArray(ty.buffer);
    ptr(G_BYTE).writeByteArray(by.buffer);
    ptr(G_WALL).writeByteArray(wm.buffer);
    ptr(G_CORNER).writeByteArray(cn.buffer);
    ptr(G_FLAGS).writeByteArray(fl.buffer);
    // wall bytes of the type records (+6 of each 0x30 record from 0x00578372): zero for every third type so
    // wallHeight falls back to the neighbour's type (0x00449375)
    for (let t = 0; t < 0x17; t++) ptr(G_TDEF + 0x30 * t + 6).writeS8(t % 3 === 0 ? 0 : ((t * 7 + i) % 9) - 2);
    ptr('0x00820f2a').writeU8((i * 0x35) & 0xff);
  }
  // the fixture object's tiles: type field +0x24 equal to tileType or not, variation byte +0x30 equal to tileByte or not
  function putTiles(fx, i) {
    const w = fx.tw, h = fx.th, t = fx.tobj;
    for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
      const tile = t.add(0x3a4 + (x + y * w) * 0x248);
      const inGrid = x < 50 && y < 50;
      const ty = inGrid ? ptr(G_TYPE).add(x * 50 + y).readS8() : 0;
      const by = inGrid ? ptr(G_BYTE).add(x * 50 + y).readU8() : 0;
      const k = (x * 3 + y * 7 + i) % 6;
      tile.add(0x24).writeS32(k === 0 ? ty + 1 : k === 1 ? -0x80000000 + ty : ty);
      tile.add(0x30).writeU8(k === 2 || k === 3 ? by ^ (1 + (x & 1)) : by);
    }
  }
  function terScene(o) {
    return function () {
      const fx = { clog: newClog() };
      fx.tw = o.w; fx.th = o.h;
      // syncTiles reads every tile's +0x24 / +0x30; rebuildTerrainGrid only forms tile pointers (the stood-in methods
      // record them), so its object is the header alone
      fx.tobj = alloc(o.tiles ? 0x3a4 + o.w * o.h * 0x248 + 0x40 : 0x400);
      fx.obj = fx.tobj;
      fx.tobj.add(0x14).writeS32(o.w);
      fx.tobj.add(0x18).writeS32(o.h);
      freezeNow();
      __freeze.setU32(TER_GLOBAL, fx.tobj.toUInt32());
      saveGrids();
      calleeStand(fx, 0, ptr('0x00483bd0'), 0, 'mscdecl', 0x3f00);
      terImports(fx, o.imports, o.logSize);
      for (let i = 0; i < o.rows; i++) {
        Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () {
          putGrids(i, o);
          if (o.tiles) putTiles(fx, i);
          return i;
        } });
      }
      return fx;
    };
  }
  const SYNC = ['setType', 'getVariation'];
  const REBUILD = ['setType', 'elevateCorner', 'calcNormals', 'setWall', 'layPath'];
  F.c3ey_sync_50 = terScene({ w: 50, h: 50, rows: 6, tiles: true, imports: SYNC, logSize: 0x20000 });
  F.c3ey_sync_53 = terScene({ w: 53, h: 51, rows: 6, tiles: true, imports: SYNC, logSize: 0x20000 });
  F.c3ey_rebuild_50 = terScene({ w: 50, h: 50, rows: 4, imports: REBUILD, logSize: 0x60000 });
  F.c3ey_rebuild_53 = terScene({ w: 53, h: 51, rows: 4, imports: REBUILD, logSize: 0x60000 });
  F.c3ey_rebuild_37 = terScene({ w: 37, h: 41, rows: 4, imports: REBUILD, logSize: 0x60000 });
  F.c3ey_rebuild_30 = terScene({ w: 50, h: 30, rows: 4, imports: REBUILD, logSize: 0x60000 });
  F.c3ey_rebuild_noflags = terScene({ w: 50, h: 50, rows: 2, noflags: true, imports: REBUILD, logSize: 0x60000 });

  // ==================================================================================================================
  // jgld.dll registrars: _atexit 0x1007f080 replaced for the key by a recorder (the A/B thread appends the function
  // pointer to $alog, count at +0, and returns `ret`; any other thread is forwarded to the real _atexit). c3ds's method.
  // ==================================================================================================================
  let atexitLive = null, atexitToken = 0;
  function atexitRecorder(alog, ret) {
    const at = jva(0x1007f080);
    if (atexitLive) { Interceptor.revert(at); Interceptor.flush(); atexitLive = null; }
    const tid = Process.getCurrentThreadId();
    const orig = new NativeFunction(at, 'int', ['pointer'], 'mscdecl');
    const cb = new NativeCallback(function (fn) {
      if (Process.getCurrentThreadId() !== tid) return orig(fn);
      const n = alog.readU32();
      if (n < 8) alog.add(4 + 4 * n).writePointer(fn);
      alog.writeU32(n + 1);
      return ret;
    }, 'int', ['pointer'], 'mscdecl');
    __diffKeepAlive.push(cb, orig);
    Interceptor.replace(at, cb);
    Interceptor.flush();
    const token = ++atexitToken;
    atexitLive = token;
    setTimeout(function () { if (atexitLive === token) { Interceptor.revert(at); Interceptor.flush(); atexitLive = null; } }, 0);
  }
  function atexitFx(seed, ret, pre) {
    return function () {
      const alog = alloc(0x40);
      // a log that already holds `pre` entries, so the new pointer lands in a different slot per key
      for (let k = 0; k < pre; k++) alog.add(4 + 4 * k).writeU32(((seed << 16) | k) >>> 0);
      alog.writeU32(pre);
      freezeNow();
      atexitRecorder(alog, ret);
      return { obj: alog, alog: alog };
    };
  }
  F.c3ey_atexit_a = atexitFx(0x1a11, 0, 0);
  F.c3ey_atexit_b = atexitFx(0x2b22, 0, 3);
  F.c3ey_atexit_fail = atexitFx(0x3c33, -1, 1);

  // ==================================================================================================================
  // libpng's defaults (c3ds's machinery, copied): THE LONGJMP CATCHER, the error recorders and jgld's primed stderr.
  // png_default_error 0x100792b0 longjmps through the jmp_buf at png_ptr + 0. For the key's function a native entry
  // stub over the first five bytes of the hook's trampoline AND its detour writes, when the first argument lies in the
  // fixture's png block and the return address lies outside jgld.dll and winmm.dll, the jmp_buf VC6's setjmp would have
  // saved at that call (ebp, ebx, edi, esi, entry esp, return address, fs:[0], trylevel -1, 'VC20', NULL unwind); the
  // longjmp then returns 1 to the harness. The jmp_buf (png + 0 .. + 0x3f) is never compared state.
  // ==================================================================================================================
  function code(bytes) {
    const p = __keepAlloc(Process.pageSize);
    Memory.protect(p, Process.pageSize, 'rwx');
    p.writeByteArray(bytes);
    return p;
  }
  function cblock(s, size) { const p = alloc(size || 0x80); p.writeUtf8String(s); return p; }
  function strWords(p, max) {
    const out = [];
    if (p.isNull()) return [0];
    const u = new Uint8Array(p.readByteArray(4 * max));
    let end = false;
    for (let i = 0; i < max && !end; i++) {
      let w = 0;
      for (let k = 0; k < 4; k++) { const c = end ? 0 : u[4 * i + k]; if (c === 0) end = true; w |= c << (8 * k); }
      out.push(w >>> 0);
    }
    return out;
  }
  function hookOf(rva) {
    const shim = Process.getModuleByName('winmm.dll');
    const pd = Memory.alloc(4), po = Memory.alloc(4), pi = Memory.alloc(4);
    const f = new NativeFunction(shim.getExportByName('SimGolfShim_FindHookM'), 'int',
      ['pointer', 'uint32', 'pointer', 'pointer', 'pointer'], 'stdcall');
    if (!f(Memory.allocUtf8String('jgld.dll'), rva, pd, po, pi)) throw new Error('c3ey: no hook at jgld.dll+' + rva.toString(16));
    if (pi.readS32() !== 1) return { detour: null, original: jgld().add(rva) };
    return { detour: pd.readPointer(), original: po.readPointer() };
  }
  const catchLive = {};
  let catchToken = 0;
  function unpatch(k) {
    const e = catchLive[k];
    if (!e) return;
    delete catchLive[k];
    Memory.patchCode(e.at, e.saved.byteLength, function (c) { c.writeByteArray(e.saved); });
  }
  function modRange(name) { const m = Process.getModuleByName(name); return [u32(m.base), u32(m.base.add(m.size))]; }
  function catchStub(target, lo, hi) {
    const page = __keepAlloc(Process.pageSize);
    Memory.protect(page, Process.pageSize, 'rwx');
    const w = new X86Writer(page, { pc: page });
    const jg = modRange('jgld.dll'), wm = modRange('winmm.dll');
    w.putPushReg('eax'); w.putPushReg('ecx'); w.putPushReg('edx');
    w.putMovRegRegOffsetPtr('eax', 'esp', 0x10);
    w.putCmpRegI32('eax', lo); w.putJccNearLabel('jb', 'skip', 'no-hint');
    w.putCmpRegI32('eax', hi); w.putJccNearLabel('jae', 'skip', 'no-hint');
    w.putMovRegRegOffsetPtr('ecx', 'esp', 0x0c);
    w.putCmpRegI32('ecx', jg[0]); w.putJccNearLabel('jb', 'notj', 'no-hint');
    w.putCmpRegI32('ecx', jg[1]); w.putJccNearLabel('jb', 'skip', 'no-hint');
    w.putLabel('notj');
    w.putCmpRegI32('ecx', wm[0]); w.putJccNearLabel('jb', 'fill', 'no-hint');
    w.putCmpRegI32('ecx', wm[1]); w.putJccNearLabel('jb', 'skip', 'no-hint');
    w.putLabel('fill');
    w.putMovRegOffsetPtrReg('eax', 0, 'ebp');
    w.putMovRegOffsetPtrReg('eax', 4, 'ebx');
    w.putMovRegOffsetPtrReg('eax', 8, 'edi');
    w.putMovRegOffsetPtrReg('eax', 0xc, 'esi');
    w.putLeaRegRegOffset('edx', 'esp', 0xc);
    w.putMovRegOffsetPtrReg('eax', 0x10, 'edx');
    w.putMovRegOffsetPtrReg('eax', 0x14, 'ecx');
    w.putBytes([0x64, 0x8B, 0x15, 0, 0, 0, 0]);
    w.putMovRegOffsetPtrReg('eax', 0x18, 'edx');
    w.putMovRegOffsetPtrU32('eax', 0x1c, 0xffffffff);
    w.putMovRegOffsetPtrU32('eax', 0x20, 0x56433230);
    for (let o = 0x24; o < 0x40; o += 4) w.putMovRegOffsetPtrU32('eax', o, 0);
    w.putLabel('skip');
    w.putPopReg('edx'); w.putPopReg('ecx'); w.putPopReg('eax');
    const rl = new X86Relocator(target, w);
    let n = 0;
    while (n < 5) {
      const m = rl.readOne();
      if (m === 0) throw new Error('c3ey catcher: cannot relocate ' + target);
      n = m;
    }
    rl.writeAll();
    w.putJmpAddress(target.add(n));
    w.flush();
    return page;
  }
  function catcher(rvas, lo, hi) {
    const token = ++catchToken;
    const mine = [];
    rvas.forEach(function (rva) {
      const h = hookOf(rva);
      [h.original, h.detour].forEach(function (at) {
        if (!at) return;
        const k = at.toString();
        unpatch(k);
        const stub = catchStub(at, lo, hi);
        const saved = at.readByteArray(5);
        Memory.patchCode(at, 5, function (c) { const pw = new X86Writer(c, { pc: at }); pw.putJmpAddress(stub); pw.flush(); });
        catchLive[k] = { at: at, saved: saved, token: token };
        mine.push(k);
      });
    });
    setTimeout(function () { mine.forEach(function (k) { if (catchLive[k] && catchLive[k].token === token) unpatch(k); }); }, 0);
  }
  // $hlog: +0 count, then records of 0x48 bytes: kind (1 error function), png index, the message text as 16 dwords
  function recorders(hlog, pngs) {
    zero(hlog, 0x260);
    const idx = (png) => { for (let i = 0; i < pngs.length; i++) if (pngs[i].equals(png)) return i; return 0xff; };
    const recErr = new NativeCallback(function (png, msg) {
      const n = hlog.readU32();
      if (n < 8) {
        const r = hlog.add(4 + n * 0x48);
        r.writeU32(1);
        r.add(4).writeU32(idx(png));
        strWords(msg, 16).forEach((w, i) => r.add(8 + 4 * i).writeU32(w));
      }
      hlog.writeU32(n + 1);
    }, 'void', ['pointer', 'pointer'], 'mscdecl');
    __diffKeepAlive.push(recErr);
    const longjmp = jva(0x10082f90);
    const jump = code([
      0x8B, 0x44, 0x24, 0x08, 0x50, 0x8B, 0x44, 0x24, 0x08, 0x50,
      0xB8].concat(le(u32(recErr)), [0xFF, 0xD0, 0x83, 0xC4, 0x08,
      0x6A, 0x01, 0xFF, 0x74, 0x24, 0x08,
      0xB8], le(u32(longjmp)), [0xFF, 0xD0, 0xCC]));
    const ret = code([
      0x8B, 0x44, 0x24, 0x08, 0x50, 0x8B, 0x44, 0x24, 0x08, 0x50,
      0xB8].concat(le(u32(recErr)), [0xFF, 0xD0, 0x83, 0xC4, 0x08, 0xC3]));
    return { jump: jump, ret: ret };
  }
  // jgld's stderr 0x101233f0 is buffered and gets its buffer on the first fprintf (c3dn): primed once
  function primeStderr() {
    const giob = jva(0x101233f0);
    if (giob.add(8).readPointer().isNull()) {
      const fpf = new NativeFunction(jva(0x10083010), 'int', ['pointer', 'pointer'], 'mscdecl');
      fpf(giob, cblock('\n', 8));
    }
    if (giob.add(8).readPointer().isNull() || giob.add(0x18).readU32() !== 0x1000)
      throw new Error('c3ey: stderr stream not buffered as c3dn probed');
    return { giob: giob, gbuf: giob.add(8).readPointer() };
  }
  const WRITEFILE = { args: 5, impl: function (a, ctx) {
    const n = u32(a[2]);
    ctx.record([u32(a[0]), n]);
    if (!a[3].isNull()) a[3].writeU32(n);
    return 1;
  } };

  // The png block: up to 16 png_structs of 0x220 bytes from +0, data buffers from +0x2400 (0x100 each), messages
  // from +0x3400. png_struct: error function per mode at +0x40, a warning slot +0x44 = 0, io_ptr +0x54 = a stream.
  const PNG = 0x220, PNGBLK = 0x4000, DATA = 0x2400, MSGS = 0x3400;
  function pngBlock(mode, n, seed) {
    const s = primeStderr();
    const blk = __keepAlloc(PNGBLK);
    fill(blk, PNGBLK, seed);
    const out = { obj: blk, blk: blk, png: [], giob: s.giob, gbuf: s.gbuf };
    for (let i = 0; i < n; i++) out.png.push(blk.add(i * PNG));
    out.hlog = alloc(0x260);
    const r = recorders(out.hlog, out.png);
    out.png.forEach(function (p, i) {
      zero(p, 0x40);
      p.add(0x40).writePointer(mode === 'jump' ? r.jump : mode === 'ret' ? r.ret : ptr(0));
      p.add(0x44).writePointer(ptr(0));
      out['P' + i] = p;
      out['D' + i] = blk.add(DATA + i * 0x100);
    });
    return out;
  }

  // png_default_error: eight png_structs, twelve messages; jgld's stderr primed, WriteFile stood in
  const EMSGS = ['Missing IHDR before gAMA', '', 'x', 'CRC error', 'invalid chunk type', 'Q',
                 'a message of exactly forty bytes ......', 'Unknown zTXt compression type', 'Duplicate', 'error %d',
                 'tab\tand\nnewline', 'sixty-three characters long message .........................end'];
  function defaultErrorFx(seed) {
    return function () {
      freezeNow();
      const out = pngBlock('null', 8, seed);
      EMSGS.forEach(function (m, i) { const p = out.blk.add(MSGS + i * 0x80); zero(p, 0x80); p.writeUtf8String(m); out['m' + i] = p; });
      out.implog = __importStandIn({ module: 'jgld.dll', logSize: 0x1000, imports: { 'KERNEL32.dll!WriteFile': WRITEFILE } }).log;
      catcher([0x792b0], u32(out.blk), u32(out.blk.add(16 * PNG)));
      return out;
    };
  }
  F.c3ey_deferr_a = defaultErrorFx(0x4e01);
  F.c3ey_deferr_b = defaultErrorFx(0x4e02);

  // png_default_read_data: each png_struct's io_ptr +0x54 is a stream of the file stand-in (filestandin.js) over its own
  // fixture file; jgld's fread 0x10082d80 is served by the stand-in (CRT table entry added for jgld.dll at run time).
  // mode: the error function at +0x40 (jump = record and longjmp; ret = record and return, png_default_error then
  // prints and longjmps; null = png_default_error directly).
  const FILES = [37, 0, 300, 5, 64, 129, 1, 255];
  function fileBytes(n, k) { const a = new Uint8Array(n); for (let i = 0; i < n; i++) a[i] = (i * 29 + k * 53 + 7) & 0xff; return a; }
  function readFx(mode, seed) {
    return function () {
      freezeNow();
      const out = pngBlock(mode, FILES.length, seed);
      const tbl = __fileStandIn.CRT['jgld.dll'] || (__fileStandIn.CRT['jgld.dll'] = {});
      tbl.fread = u32(jva(0x10082d80));
      const files = {};
      FILES.forEach(function (n, i) { files['png' + i + '.bin'] = fileBytes(n, i + seed); });
      const fs = __fileStandIn({ module: 'jgld.dll', crt: ['fread'], files: files, size: 0x2000,
                                 imports: { 'KERNEL32.dll!WriteFile': WRITEFILE }, implogSize: 0x1000 });
      out.fsblock = fs.block;
      out.implog = fs.implog;
      FILES.forEach(function (n, i) { out.png[i].add(0x54).writePointer(fs.fopen('png' + i + '.bin', 'rb')); });
      catcher([0x78d40], u32(out.blk), u32(out.blk.add(16 * PNG)));
      return out;
    };
  }
  F.c3ey_read_jump = readFx('jump', 0x5d01);
  F.c3ey_read_ret = readFx('ret', 0x5d02);
  F.c3ey_read_null = readFx('null', 0x5d03);

  // ==================================================================================================================
  // Terrain.dll forwarders. Terrain::normalArray / vertexArray: the static Tile::normalArray 0x10013ff0 /
  // Tile::vertexArray 0x10013fc0 stood in by calleeStand with a result taken from a table indexed by the stub's call
  // count, which the vector's getter ($tN) writes into the compared log before the snapshot (so each vector gets its
  // own result and both arms the same). std::allocator<Tile*>::destroy: std::_Destroy 0x1000bb40 stood in, recording its
  // argument and returning a value of it.
  // ==================================================================================================================
  const SEQ = [0x10070a18, 0, 1, 0xffffffff, 0x80000000, 0x12345678, 0x0badf00d, 0x7fffffff, 0x100b28c8, 0xcccccccc,
               0x40, 0xfffffffc];
  function arrayFx(rva) {
    return function () {
      const fx = { clog: newClog(), obj: alloc(0x40) };
      calleeStand(fx, 0, tva(0x10000000 + rva), 0, 'mscdecl', function (n) { return SEQ[n % SEQ.length]; });
      for (let i = 0; i < SEQ.length; i++) {
        Object.defineProperty(fx, 't' + i, { enumerable: true, configurable: true, get: function () {
          fx.clog.add(4).writeU32(i);
          return fx.obj.add(4 * (i % 8));
        } });
      }
      return fx;
    };
  }
  F.c3ey_normal = arrayFx(0x13ff0);
  F.c3ey_vertex = arrayFx(0x13fc0);
  // the real callees, for the record: one constant result whatever `this` is
  F.c3ey_array_real = function () { const o = alloc(0x40); return { obj: o, t0: o, t1: o.add(8), t2: ptr(0) }; };
  F.c3ey_destroy = function () {
    const fx = { clog: newClog(), obj: alloc(0x100) };
    calleeStand(fx, 0, tva(0x1000bb40), 1, 'mscdecl', function (n, a) { return (a[0] ^ 0x5a5a0000) + n; });
    for (let i = 0; i < 6; i++) fx['p' + i] = fx.obj.add(0x10 * i);
    return fx;
  };
  F.c3ey_destroy_real = function () {
    freezeNow();
    const o = alloc(0x100);
    return { obj: o, p0: o, p1: o.add(0x10) };
  };

  // ==================================================================================================================
  // jgld.dll Sprite::lineTo / dashTo / drawO. The sprites and destinations are ours, with vtables of small native stubs
  // (no JS on the call): sprite slot +0x20 returns the payload word +0x14 (`mov eax, [ecx+0x14]; ret`), slot +0x24
  // counts its calls at +0x40 and keeps its argument at +0x44 (`ret 4`); a destination's slot +0xe4 returns
  // &dst[+0x24], the depth word drawO reads. Every other slot returns 0x0badc0de. lineTo's and dashTo's callees
  // (Sprite::blitMask 0x1002cec0, Sprite::blitMaskScaled 0x1002d390) are stood in by calleeStand (thiscall, every
  // argument recorded, a result made from them); drawO runs its real callees: Sprite::getFlag18 0x100180e0,
  // blitMask (handed a NULL mask, it returns 3 at 0x1002cf03 before touching anything), 0x1001b5d0 (returns 0) and
  // _CrtDbgReport 0x10081f90, with jgld's _CrtDbgMode[_CRT_WARN] (0x10123378) set to 0 for the key so the reports
  // write nothing and return 0 (c3ci's finding: mode 2 reaches OutputDebugStringA).
  // ==================================================================================================================
  function vtStubs(slots) {
    const trap = code([0xB8].concat(le(0x0badc0de), [0xC3]));
    const vt = alloc(0x100);
    for (let i = 0; i < 0x40; i++) vt.add(4 * i).writePointer(trap);
    Object.keys(slots).forEach(function (o) { vt.add(+o).writePointer(code(slots[o])); });
    return vt;
  }
  //    bpp, payload, flags +0x18, palette word +0x10
  const SPRS = [[8, 1, 0, 0x11110010], [8, 1, 1, 0x22220020], [16, 1, 0, 0x33330030], [16, 1, 1, 0x44440040],
                [24, 1, 0, 0x55550050], [8, 0, 0, 0x66660060], [0, 1, 0, 0x77770070], [16, 1, 2, 0x88880080]];
  function spriteDx() {
    freezeNow();
    const q = jva(0x10123378);
    __freeze.save(q, 4);
    q.writeS32(0);
    const blk = alloc(0x800);
    const svt = vtStubs({ 0x20: [0x8B, 0x41, 0x14, 0xC3],
                          0x24: [0xFF, 0x41, 0x40, 0x8B, 0x44, 0x24, 0x04, 0x89, 0x41, 0x44, 0xC2, 0x04, 0x00] });
    const dvt = vtStubs({ 0xe4: [0x8D, 0x41, 0x24, 0xC3] });
    const fx = { obj: blk, blk: blk, clog: newClog() };
    SPRS.forEach(function (s, i) {
      const p = blk.add(0x80 * i);
      p.writePointer(svt);
      p.add(0x10).writeU32(s[3]);
      p.add(0x14).writePointer(s[1] ? blk.add(0x600 + 0x10 * i) : ptr(0));
      p.add(0x18).writeU32(s[2]);
      p.add(0x20).writeS32(s[0]);
      fx['s' + i] = p;
    });
    [8, 16, 24, 0].forEach(function (d, i) {
      const p = blk.add(0x400 + 0x40 * i);
      p.writePointer(dvt);
      p.add(0x24).writeS32(d);
      fx['d' + d] = p;
    });
    return fx;
  }
  F.c3ey_drawO = spriteDx;
  F.c3ey_lineTo = function () {
    const fx = spriteDx();
    calleeStand(fx, 0, jva(0x1002cec0), 6, 'thiscall', function (n, a) { return (a[1] ^ a[2] ^ (a[3] << 8) ^ a[5]) >>> 0; });
    return fx;
  };
  F.c3ey_dashTo = function () {
    const fx = spriteDx();
    calleeStand(fx, 0, jva(0x1002d390), 8, 'thiscall', function (n, a) { return (a[1] + a[4] * 3 + a[5] * 5 + a[6] * 7 + a[7]) >>> 0; });
    return fx;
  };

  // ==================================================================================================================
  // jgld.dll Sprite::blitMask. One block $bm (state) holds sprites, stub-vtable surfaces, palettes, colour tables,
  // payloads and pixel buffers; the 2 MB lookup table is read only. Surface stubs: +0x10 returns [+0x50] (the lock
  // base), +0x20 returns [+0x54] (the pixel base), +0x24 counts at +0x40 and keeps its argument at +0x44 (`ret 4`),
  // +0xcc returns &[+0x60] (the clip RECT), +0xe0 returns [+0x58] (the pitch in pixels), +0xe4 returns &[+0x70] (the
  // format pair, [+0x74] the format). Palette stubs: +0x18 returns [+0x10], +0x1c returns [+0x14]. IntersectRect
  // (jgld's IAT slot 0x1012c5e4) is stood in by an exact intersection of the two rectangles' CONTENTS (the arguments
  // are frame addresses, so they are not recorded; the input and output values are). The first thread is frozen;
  // the scale globals 0x10122dc0 / dc4 / dc8, the palette client 0x1012873c and the loop globals 0x10128538..0x1012857f
  // are saved and written back before it resumes.
  // ==================================================================================================================
  let bmTable = null;
  function bmTableGet() {
    if (bmTable) return bmTable;
    const n = 0x100080;
    const u = new Uint16Array(n);
    for (let i = 0; i < n; i++) u[i] = (i * 7919 + 13) & 0x1fff;
    bmTable = __keepAlloc(2 * n);
    bmTable.writeByteArray(u.buffer);
    return bmTable;
  }
  function blitMaskFx(scale, client) {
    return function () {
      const T = bmTableGet();
      const blk = alloc(0xc000);
      const svt = vtStubs({ 0x10: [0x8B, 0x41, 0x50, 0xC3], 0x20: [0x8B, 0x41, 0x54, 0xC3],
                            0x24: [0xFF, 0x41, 0x40, 0x8B, 0x44, 0x24, 0x04, 0x89, 0x41, 0x44, 0xC2, 0x04, 0x00],
                            0xcc: [0x8D, 0x41, 0x60, 0xC3], 0xe0: [0x8B, 0x41, 0x58, 0xC3], 0xe4: [0x8D, 0x41, 0x70, 0xC3] });
      const pvt = vtStubs({ 0x18: [0x8B, 0x41, 0x10, 0xC3], 0x1c: [0x8B, 0x41, 0x14, 0xC3] });
      const fx = { obj: blk, bm: blk, T: T, T2: T.add(0x80) };
      // colour tables (+0x1000, 4 x 0x200) and palettes (+0x800)
      for (let t = 0; t < 4; t++) for (let i = 0; i < 256; i++) blk.add(0x1000 + t * 0x200 + 2 * i).writeU16((i * (t * 2 + 3) + t * 0x111) & 0x1fff);
      [0, 1].forEach(function (k) {
        const p = blk.add(0x800 + 0x40 * k);
        p.writePointer(pvt);
        p.add(0x10).writePointer(blk.add(0x1000 + 0x400 * k));
        p.add(0x14).writePointer(blk.add(0x1200 + 0x400 * k));
        fx['pal' + k] = p;
      });
      // payloads (+0x2000, 4 x 0x400): colour bytes, the two blend ranges and the transparent 0xff
      const ALPH = [0x00, 0x05, 0x3c, 0xa5, 0xdf, 0xe0, 0xe7, 0xef, 0xf0, 0xf8, 0xfe, 0xff, 0x7f];
      for (let s = 0; s < 4; s++) for (let j = 0; j < 0x400; j++) blk.add(0x2000 + s * 0x400 + j).writeU8(ALPH[(j * (s + 3) + s) % ALPH.length]);
      // pixel buffers (+0x4000, 8 x 0x1000): dst pixels below 0x2000; out pixels with the key colour 0x7c1f on one in five
      for (let b = 0; b < 8; b++) for (let k = 0; k < 0x800; k++)
        blk.add(0x4000 + b * 0x1000 + 2 * k).writeU16(b >= 5 && (k * 3 + b) % 5 === 0 ? 0x7c1f : (k * 4099 + b * 311 + 7) & 0x1fff);
      //   name, lock base?, pixel buffer, pitch, clip, format
      const SURF = [['D0', 1, 0, 32, [0, 0, 32, 16], 0], ['D1', 1, 1, 32, [0, 0, 32, 16], 1], ['D2', 1, 2, 32, [0, 0, 32, 16], 2],
                    ['D3', 0, 3, 32, [0, 0, 32, 16], 0], ['D4', 1, 4, 32, [4, 2, 20, 12], 0],
                    ['O0', 1, 5, 32, [0, 0, 32, 16], 0], ['O1', 1, 6, 24, [0, 0, 24, 12], 0], ['O2', 1, 7, 40, [0, 0, 40, 20], 0]];
      SURF.forEach(function (s, i) {
        const p = blk.add(0x400 + 0x80 * i);
        p.writePointer(svt);
        const pix = blk.add(0x4000 + 0x1000 * s[2]);
        p.add(0x50).writePointer(s[1] ? pix : ptr(0));
        p.add(0x54).writePointer(pix);
        p.add(0x58).writeS32(s[3]);
        s[4].forEach(function (v, j) { p.add(0x60 + 4 * j).writeS32(v); });
        p.add(0x74).writeS32(s[5]);
        fx[s[0]] = p;
      });
      //   palette (-1 none), payload slice (-1 none), pitch, width, height
      const SPR = [[0, 0, 16, 16, 8], [-1, 1, 16, 16, 8], [0, -1, 16, 16, 8], [1, 2, 24, 20, 10], [1, 3, 4, 1, 1], [0, 1, 20, 9, 5]];
      SPR.forEach(function (s, i) {
        const p = blk.add(0x80 * i);
        p.add(0x10).writePointer(s[0] < 0 ? ptr(0) : fx['pal' + s[0]]);
        p.add(0x14).writePointer(s[1] < 0 ? ptr(0) : blk.add(0x2000 + 0x400 * s[1] + 0x40));
        p.add(0x2c).writeS32(s[2]);
        p.add(0x30).writeS32(s[3]);
        p.add(0x34).writeS32(s[4]);
        fx['S' + i] = p;
      });
      // blitMaskScaled's sprites read 2 source bytes per pixel and move by a row skip that can be negative: their
      // payload sits in the middle of an 0x8000-byte block filled end to end (read only)
      const bp = __keepAlloc(0x8000);
      const bu = new Uint8Array(0x8000);
      for (let j = 0; j < 0x8000; j++) bu[j] = ALPH[(j * 5 + 3) % ALPH.length];
      bp.writeByteArray(bu.buffer);
      [[0, 16, 16, 8], [1, 24, 12, 6], [-1, 16, 10, 5]].forEach(function (s, i) {
        const p = blk.add(0x300 + 0x40 * i);
        p.add(0x10).writePointer(s[0] < 0 ? ptr(0) : fx['pal' + s[0]]);
        p.add(0x14).writePointer(bp.add(0x4000 + 0x100 * i));
        p.add(0x2c).writeS32(s[1]);
        p.add(0x30).writeS32(s[2]);
        p.add(0x34).writeS32(s[3]);
        fx['S' + (6 + i)] = p;
      });
      const cl = blk.add(0xb000);
      cl.add(4).writePointer(client ? fx.pal1 : ptr(0));
      freezeNow();
      const gs = jva(0x10122dc0), gc = jva(0x1012873c), gl = jva(0x10128538);
      __freeze.save(gs, 0xc);
      __freeze.save(gc, 4);
      __freeze.save(gl, 0x5c);
      // the row skip blitMaskScaled writes through the module pointer 0x10122e20 (0x1002d6c4)
      fx.rowskip = jva(0x10122e20).readPointer();
      __freeze.save(fx.rowskip, 4);
      gs.writeS32(scale[0]); gs.add(4).writeS32(scale[1]); gs.add(8).writeS32(scale[2]);
      gc.writePointer(cl);
      fx.gl = gl;
      const rd = (p) => [p.readS32(), p.add(4).readS32(), p.add(8).readS32(), p.add(12).readS32()];
      fx.implog = __importStandIn({ module: 'jgld.dll', imports: { 'USER32.dll!IntersectRect': { args: 3, impl: function (a, ctx) {
        const r1 = rd(a[1]), r2 = rd(a[2]);
        const l = Math.max(r1[0], r2[0]), t = Math.max(r1[1], r2[1]), r = Math.min(r1[2], r2[2]), b = Math.min(r1[3], r2[3]);
        const ok = l < r && t < b;
        const o = ok ? [l, t, r, b] : [0, 0, 0, 0];
        o.forEach(function (v, j) { a[0].add(4 * j).writeS32(v); });
        ctx.record(r1.concat(r2, o).map(function (v) { return v >>> 0; }));
        return ok ? 1 : 0;
      } } } }).log;
      return fx;
    };
  }
  F.c3ey_bm_s111 = blitMaskFx([1, 1, 1], true);
  F.c3ey_bm_nopal = blitMaskFx([1, -1, 1], false);
  F.c3ey_bm_s121 = blitMaskFx([1, 2, 1], true);
  F.c3ey_bm_s212 = blitMaskFx([2, 1, 2], true);
  F.c3ey_bms_11 = blitMaskFx([1, 1, 5], true);
  F.c3ey_bms_32 = blitMaskFx([3, 2, 9], true);
  F.c3ey_bms_neg = blitMaskFx([-2, 3, 0], false);

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
