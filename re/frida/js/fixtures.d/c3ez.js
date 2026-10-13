// Fixtures for batch c3ez (round 33): golf_clean.exe rows whose blocker a later tool removed. Reimplementations:
// shim/src/re/c3ez.cpp. Registry: re/frida/registry.d/c3ez.py. Purposes and per-jcc tables: log/c3/c3ez_purpose.md.
//
// Every key freezes the game's first thread (re/frida/js/freeze.js) before it replaces anything; every game global a
// vector writes is saved by __freeze and written back before the thread resumes. Machinery copied (not shared) from
// c3ey (calleeStand, the exe grids, the fixture Terrain object at [0x00820ed0]) and extended: calleeStand takes a
// record function, and the Terrain stand-ins model the height changes their real methods make so the loops end.
(function () {
  const F = {};
  const u32 = (v) => (typeof v === 'number' ? v : v.toUInt32()) >>> 0;
  const s32 = (v) => u32(v) | 0;
  const le = (v) => { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; };
  const zero = (p, n) => p.writeByteArray(new Uint8Array(n).buffer);
  function alloc(n) { const p = __keepAlloc(n); zero(p, n); return p; }
  function code(bytes) {
    const p = __keepAlloc(Process.pageSize);
    Memory.protect(p, Process.pageSize, 'rwx');
    p.writeByteArray(bytes);
    return p;
  }

  let frozen = false;
  function freezeNow() {
    if (frozen) return;
    __freeze.thread();
    frozen = true;
    __freeze.atFlush(function () { frozen = false; });
    __freeze.negOn();             // every exported <Batch>NegCtl switch (only the negative-control build has one)
  }

  // ---- calleeStand (c3dw / c3ey): a function replaced for one key on the A/B thread only; every call is appended to
  // fx.clog (word 0 = byte cursor, entries {va, n, values...} from +0x50; word 1+k = call count of stub k). Other
  // threads reach the real function. ret: number | function (n, args) -> number. rec(args) -> the values recorded
  // (default every argument).
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x1000;
  function newClog() { const l = alloc(CLOG); l.writeU32(0x50); return l; }
  function calleeStand(fx, k, at, argc, abi, ret, rec) {
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
      const a = Array.prototype.slice.call(arguments).map((v) => v >>> 0);
      const cell = log.add(4 + 4 * k);
      const n = cell.readU32();
      cell.writeU32(n + 1);
      const vals = rec ? rec(a) : a;
      const cur = log.readU32();
      const need = 8 + 4 * vals.length;
      if (cur + need <= CLOG) {
        log.add(cur).writeU32(va);
        log.add(cur + 4).writeU32(vals.length);
        vals.forEach(function (v, i) { log.add(cur + 8 + 4 * i).writeU32(v >>> 0); });
        log.writeU32(cur + need);
      }
      const r = typeof ret === 'function' ? ret(n, a) : (ret === undefined ? 0 : ret);
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
  // The Terrain grid. [0x00820ed0] points at a FIXTURE Terrain object (width +0x14, height +0x18, tiles of 0x248 bytes
  // from +0x3a4) while the first thread is frozen. Every Terrain.dll method the rows call is stood in at the exe's IAT
  // (importstandin.js, thiscall: a[0] is ecx), so no Terrain.dll code runs on the A/B thread and neither the game's
  // Terrain object nor Terrain.dll's arrays are touched; the call log $implog is compared state.
  // Exe grids (c3ey's layout): tileType 0x005722e8 and tileByte 0x0056988c (x * 50 + y), the corner cache 0x0051b770
  // ((y + x * 50) * 8 + corner, nonzero = cornerHeights' answer, so tileQuery449310 = that byte), the wall mask
  // 0x005619a0, the type records 0x00578372 + 0x30 * type, the flag words 0x0053caf0 ((x * 50 + y) * 2).
  // Tile fields the rows read: +0x24 type, +0x30 variation byte, heights +4 / +0xc / +0x14 / +0x1c (corners 1 3 5 7),
  // walls +0x234 + s, and +0x40, the byte the hasPath stand-in answers from.
  // ==================================================================================================================
  const TER_GLOBAL = ptr('0x00820ed0');
  const G_TYPE = 0x005722e8, G_BYTE = 0x0056988c, G_CORNER = 0x0051b770, G_WALL = 0x005619a0, G_TDEF = 0x00578372,
        G_FLAGS = 0x0053caf0;
  const TILE = 0x248, TILES0 = 0x3a4;
  const NULLX = 7, NULLY = 9;     // the cell whose corner bytes the stand-ins move for a NULL tile
  const TER = {
    passCollarInfo: ['?passCollarInfo@Terrain@@QAEXQAHH@Z', 3],
    setType: ['?setType@Terrain@@QAEXPAVTile@@HH@Z', 4],
    getVariation: ['?getVariation@Terrain@@QAEHPAVTile@@@Z', 2],
    layPath: ['?layPath@Terrain@@QAEXPAVTile@@HH@Z', 4],
    setWall: ['?setWall@Terrain@@QAEXPAVTile@@HH_N@Z', 5],
    elevateCorner: ['?elevateCorner@Terrain@@QAEXPAVTile@@H@Z', 3],
    initTerrain: ['?initTerrain@Terrain@@QAEXXZ', 1],
    initSystem: ['?initSystem@Terrain@@QAEXHHPAUHDC__@@_N@Z', 5],
    pathUpdateRender: ['?pathUpdateRender@Terrain@@QAEXPAVTile@@M@Z', 3],
    localRender: ['?localRender@Terrain@@QAEXPAVTile@@0M@Z', 4],
    calcAllNormals: ['?calcAllNormals@Terrain@@QAEXPAVTile@@@Z', 2],
    setZoomLevel: ['?setZoomLevel@Terrain@@QAEXH@Z', 2],
    render: ['?render@Terrain@@QAE_NPAVTile@@M@Z', 3],
    lowerEdgeCorner: ['?lowerEdgeCorner@Terrain@@QAEXPAVTile@@H0M@Z', 5],
    lowerCorner: ['?lowerCorner@Terrain@@QAEXPAVTile@@H@Z', 3],
    hasConnectedPath: ['?hasConnectedPath@Terrain@@QAE_NHH@Z', 3],
    updatePath: ['?updatePath@Terrain@@QAEXHHH@Z', 4],
    hasPath: ['?hasPath@Terrain@@QAE_NPAVTile@@@Z', 2],
    stripRender: ['?stripRender@Terrain@@QAEXPAVTile@@HM@Z', 4],
  };
  const cell = (x, y) => x * 50 + y;
  function cornerAt(x, y, c) { return ptr(G_CORNER + (y + x * 50) * 8 + c); }
  // the height a corner stand-in moves: the tile's own field, or for a NULL tile the corner byte of (NULLX, NULLY)
  function bump(a, d, raise) {
    const t = a[1], c = u32(a[2]);
    if (!t.isNull()) {
      const p = t.add(4 + 4 * (c - 1));
      p.writeS32(p.readS32() + d);
    } else {
      const p = cornerAt(NULLX, NULLY, c);
      p.writeS8(p.readS8() + (raise ? -1 : 1));
    }
  }
  function tileIdx(fx, t) {
    const p = u32(t);
    if (p === 0) return 0xffff;
    return ((p - u32(fx.tobj) - TILES0) / TILE) >>> 0;
  }
  function terSpec(fx, n) {
    const spec = { args: TER[n][1], abi: 'thiscall' };
    switch (n) {
      case 'getVariation': spec.impl = (a) => (a[1].isNull() ? 0x77 : a[1].add(0x30).readU8()); break;
      case 'setType': spec.impl = (a) => ((u32(a[2]) << 8) ^ u32(a[3]) ^ (tileIdx(fx, a[1]) << 16)) >>> 0; break;
      case 'layPath': spec.impl = (a) => (0x1a000000 + (tileIdx(fx, a[1]) << 8) + u32(a[2]) + u32(a[3])) >>> 0; break;
      case 'hasPath': spec.impl = (a) => ((a[1].isNull() ? 0 : (a[1].add(0x40).readU8() ? 1 : 0)) | 0x3300) >>> 0; break;
      case 'hasConnectedPath': spec.impl = (a) => (((s32(a[1]) * 3 + s32(a[2])) % 2 === 0 ? 0x101 : 0x200)) >>> 0; break;
      case 'elevateCorner': spec.impl = (a) => { bump(a, 1, true); return 0x5e010000 + u32(a[2]); }; break;
      case 'lowerCorner': spec.impl = (a) => { bump(a, -1, false); return 0x5e020000 + u32(a[2]); }; break;
      case 'lowerEdgeCorner': spec.impl = (a) => { bump(a, -1, false); return 0x5e030000 + u32(a[2]); }; break;
      case 'render': spec.impl = (a) => (0x77000000 + (tileIdx(fx, a[1]) << 4) + (u32(a[2]) >>> 28)) >>> 0; break;
      case 'passCollarInfo':
        // the array is a stack buffer: record its 0x17 ints, not its address
        spec.impl = function (a, ctx) {
          const vals = [u32(a[0]), u32(a[2])];
          for (let i = 0; i < 0x17; i++) vals.push(a[1].add(4 * i).readU32());
          ctx.record(vals);
          return 0x5e040000;
        };
        break;
      case 'initSystem':
        // the fourth argument is a bool pushed from a byte (0x00449832): only its low byte is defined
        spec.impl = function (a, ctx) { ctx.record([u32(a[0]), u32(a[1]), u32(a[2]), u32(a[3]), u32(a[4]) & 0xff]); return 0x5e050000; };
        break;
      default: spec.ret = 0x5e000000 + Object.keys(TER).indexOf(n);
    }
    return spec;
  }
  function terImports(fx, use, logSize, extra) {
    const imp = {};
    use.forEach(function (n) { imp['Terrain.dll!' + TER[n][0]] = terSpec(fx, n); });
    Object.assign(imp, extra || {});
    const s = __importStandIn({ module: 'golf_clean.exe', imports: imp, logSize: logSize });
    fx.implog = s.log;
  }
  // every game global the Terrain rows read or write, saved once per key (all written back before the resume)
  function saveGlobals() {
    __freeze.save(ptr(G_TYPE), 2500);
    __freeze.save(ptr(G_BYTE), 2500);
    __freeze.save(ptr(G_CORNER), 20000);
    __freeze.save(ptr(G_WALL), 2500);
    __freeze.save(ptr(G_TDEF), 0x30 * 0x17);
    __freeze.save(ptr(G_FLAGS), 5000);
    __freeze.save(ptr('0x00820f14'), 0x28);     // list iterator, list 0x00820f18, the bytes 0x00820f28..2c, w, h, rot
    __freeze.save(ptr('0x004d2044'), 0x10);     // first-run byte, last camera x / y, last zoom
    __freeze.save(ptr('0x005685f4'), 4);        // g_cameraRot
    __freeze.save(ptr('0x004c2844'), 4);        // g_zoomScale
    __freeze.save(ptr('0x00519cd8'), 4);        // the window surface's jgld Surface pointer
    __freeze.save(ptr('0x00822c8c'), 8);        // screen width / height initTerrain gets
    __freeze.save(ptr('0x005a34e0'), 1);
  }
  // a deterministic grid pattern per seed i (c3ey's putGrids; flags sparser: f20 on one tile in five, 0x40 in seven)
  const CORNER = [-3, 1, 2, 3, 4, 5, 3, 1, 6, 2, 1, 3, -1, 4, 2, 1];
  function putGrids(i) {
    const ty = new Uint8Array(2500), by = new Uint8Array(2500), wm = new Uint8Array(2500), cn = new Uint8Array(20000);
    const fl = new Uint8Array(5000);
    for (let x = 0; x < 50; x++) for (let y = 0; y < 50; y++) {
      const c = x * 50 + y;
      ty[c] = ((x * 7 + y * 3 + i * 5) % 8 === 0) ? 0x11 : ((x * 5 + y * 11 + i * 3) % 29 === 0 ? 0x14 : (x * 5 + y * 11 + i * 3) % 0x17);
      by[c] = (x * 3 + y * 13 + i) & 3;
      wm[c] = ((x + 2 * y + i) % 4 === 0 ? (1 << (2 * ((x + y + i) & 3))) : 0) | ((x * y + i) % 13 === 0 ? 0x55 : 0);
      for (let k = 0; k < 8; k++) cn[(y + x * 50) * 8 + k] = CORNER[(x * 3 + y * 5 + k * 7 + i * 11) % 16] & 0xff;
      const f = ((x + y * 3 + i) % 5 === 0 ? 0x20 : 0) | ((x * 2 + y + i) % 7 === 0 ? 0x40 : 0) | ((x ^ y) & 0x9f);
      fl[c * 2] = f; fl[c * 2 + 1] = (x + i) & 0xff;
    }
    ptr(G_TYPE).writeByteArray(ty.buffer);
    ptr(G_BYTE).writeByteArray(by.buffer);
    ptr(G_WALL).writeByteArray(wm.buffer);
    ptr(G_CORNER).writeByteArray(cn.buffer);
    ptr(G_FLAGS).writeByteArray(fl.buffer);
    for (let t = 0; t < 0x17; t++) ptr(G_TDEF + 0x30 * t + 6).writeS8(t % 3 === 0 ? 0 : ((t * 7 + i) % 9) - 2);
  }
  function tileOf(fx, x, y) { return fx.tobj.add(TILES0 + (x + y * fx.tw) * TILE); }
  // tile fields per seed: type +0x24 equal to tileType or not, variation +0x30, heights, walls, the hasPath byte
  function putTile(fx, x, y, i) {
    const tile = tileOf(fx, x, y);
    const ty = ptr(G_TYPE).add(cell(x, y)).readS8(), by = ptr(G_BYTE).add(cell(x, y)).readU8();
    const k = (x * 3 + y * 7 + i) % 6;
    tile.add(0x24).writeS32(k === 0 ? ty + 1 : k === 1 ? -0x80000000 + ty : ty);
    tile.add(0x30).writeU8(k === 2 ? by ^ 1 : by);
    for (let c = 1; c < 8; c += 2) {
      const q = cornerAt(x, y, c).readS8() - 3;
      const m = (x + y * 2 + c + i) % 5;
      tile.add(4 + 4 * (c - 1)).writeS32(m === 0 ? q + 2 : m === 1 ? q - 1 : q);
    }
    for (let s = 0; s < 8; s += 2) tile.add(0x234 + s).writeU8((x + y + s + i) % 3 === 0 ? 1 : 0);
    // hasPath mostly where tileFlag20 is set, so togglePath's all-grid updatePath sweeps stay rare
    const f20 = ptr(G_FLAGS).add(cell(x, y) * 2).readU8() & 0x20;
    tile.add(0x40).writeU8(f20 ? ((x + y + i) % 2 === 0 ? 1 : 0) : ((x * 7 + y + i) % 23 === 0 ? 1 : 0));
  }
  function terObject(fx, w, h, tiles) {
    fx.tw = w; fx.th = h;
    fx.tobj = alloc(tiles ? TILES0 + w * h * TILE + 0x40 : 0x400);
    fx.tobj.add(0x14).writeS32(w);
    fx.tobj.add(0x18).writeS32(h);
    __freeze.setU32(TER_GLOBAL, fx.tobj.toUInt32());
  }
  function terBase(o) {
    const fx = { clog: newClog(), obj: null };
    freezeNow();
    saveGlobals();
    terObject(fx, o.w || 50, o.h || 50, o.tiles);
    fx.obj = fx.tobj;
    putGrids(o.seed || 0);
    return fx;
  }
  // a small block of loose tiles for the per-tile rows (state $tiles)
  function looseTiles(fx, n) {
    fx.tiles = alloc(n * TILE);
    for (let i = 0; i < n; i++) fx['T' + i] = fx.tiles.add(i * TILE);
  }

  // ---- applyTileVariation(tile, x, y): vector i writes tile i's +0x24 / +0x30 and the grid cell (x, y)
  //   [cur, var, gridType, gridByte, x, y]
  const ATV = [[3, 1, 3, 1, 4, 5], [3, 1, 3, 2, 4, 6], [7, 0, 7, 3, 10, 11], [6, 2, 6, 0, 12, 13], [5, 0, 7, 0, 14, 15],
               [7, 3, 4, 3, 16, 17], [5, 1, 6, 1, 18, 19], [6, 1, 2, 1, 20, 21], [2, 2, 9, 2, 22, 23],
               [-1, 0, 0x10, 0, 24, 25], [0x16, 3, 0x16, 3, 0, 0], [0x11, 1, 0x11, 0, 49, 49], [8, 0, 9, 1, 30, 2]];
  F.c3ez_atv = function () {
    const fx = terBase({ seed: 1 });
    looseTiles(fx, ATV.length);
    terImports(fx, ['getVariation', 'setType'], 0x1000);
    ATV.forEach(function (v, i) {
      Object.defineProperty(fx, 't' + i, { enumerable: true, configurable: true, get: function () {
        const t = fx['T' + i];
        t.add(0x24).writeS32(v[0]);
        t.add(0x30).writeU8(v[1]);
        ptr(G_TYPE).add(cell(v[4], v[5])).writeS8(v[2]);
        ptr(G_BYTE).add(cell(v[4], v[5])).writeU8(v[3]);
        ptr('0x00820f2c').writeU8(0);
        return t;
      } });
      fx['x' + i] = v[4]; fx['y' + i] = v[5];
    });
    return fx;
  };

  // ---- togglePath(tile, x, y): vector i sets the flag word of (x, y) and tile i's hasPath byte.
  //   [flagByte, hasPathByte, x, y]   (hasConnectedPath(x, y) answers 1 when (3x + y) is even)
  const TGP = [[0x60, 0, 10, 10], [0x20, 0, 11, 12], [0x20, 1, 12, 12], [0x00, 0, 13, 13], [0x40, 1, 14, 0],
               [0x00, 1, 15, 1], [0x60, 1, 16, 16], [0x40, 0, 17, 18], [0x20, 0, 0, 0], [0x00, 1, 49, 48],
               [0x40, 1, 3, 1], [0x9f, 0, 20, 20], [0x1f, 1, 21, 22]];
  F.c3ez_tgp = function () {
    const fx = terBase({ seed: 2 });
    looseTiles(fx, TGP.length);
    terImports(fx, ['hasPath', 'hasConnectedPath', 'updatePath', 'layPath'], 0x40000);
    TGP.forEach(function (v, i) {
      Object.defineProperty(fx, 't' + i, { enumerable: true, configurable: true, get: function () {
        const t = fx['T' + i];
        t.add(0x40).writeU8(v[1]);
        ptr(G_FLAGS).add(cell(v[2], v[3]) * 2).writeU8(v[0]);
        ptr('0x00820f2b').writeU8(0);
        return t;
      } });
      fx['x' + i] = v[2]; fx['y' + i] = v[3];
    });
    return fx;
  };

  // ---- applyTileElevation(tile, x, y, a3, x2, y2). Vector i: tile heights h[4], corner bytes b[4] (q = b - 3), a
  // blocked neighbour index (-1 none; 0..7 in the order of the tileBlocked calls) made by a type 0x14 cell, the camera
  // rotation, (x2, y2). nullT: a NULL tile at (NULLX, NULLY).
  const NB = [[-1, 0], [0, -1], [1, 0], [0, 1], [-1, -1], [1, -1], [1, 1], [-1, 1]];
  const ATE = [
    { h: [0, 0, 0, 0], b: [3, 3, 3, 3], nb: -1, rot: 0, x: 20, y: 20, x2: 1, y2: 1 },
    { h: [0, 1, 2, 0], b: [5, 4, 5, 3], nb: -1, rot: 2, x: 21, y: 21, x2: 2, y2: 3 },
    { h: [2, 0, 0, 1], b: [3, 3, 1, 2], nb: -1, rot: 2, x: 22, y: 22, x2: 3, y2: 3 },
    { h: [2, 2, 2, 2], b: [3, 6, 2, 1], nb: 0, rot: 4, x: 23, y: 23, x2: 4, y2: 60 },
    { h: [3, 3, 0, 0], b: [3, 2, 3, 3], nb: 1, rot: 6, x: 24, y: 24, x2: -1, y2: 5 },
    { h: [2, 0, 0, 0], b: [3, 3, 3, 3], nb: 2, rot: 3, x: 25, y: 25, x2: 49, y2: 49 },
    { h: [0, 2, 0, 0], b: [3, 3, 3, 3], nb: 3, rot: 7, x: 26, y: 26, x2: 50, y2: 0 },
    { h: [0, 0, 2, 0], b: [3, 3, 3, 3], nb: 4, rot: 1, x: 27, y: 27, x2: 0, y2: 0 },
    { h: [0, 0, 0, 2], b: [3, 3, 3, 3], nb: 5, rot: 5, x: 28, y: 28, x2: 10, y2: 11 },
    { h: [2, 2, 0, 0], b: [3, 3, 3, 3], nb: 6, rot: -1, x: 29, y: 29, x2: 12, y2: -1 },
    { h: [0, 0, 2, 2], b: [3, 3, 3, 3], nb: 7, rot: 2, x: 30, y: 30, x2: 14, y2: 15 },
    { h: [1, 1, 1, 1], b: [1, 1, 1, 1], nb: -1, rot: 4, x: 0, y: 30, x2: 16, y2: 17 },
    { h: [5, -2, 4, 0], b: [3, 6, 3, 4], nb: -1, rot: 0, x: -1, y: 5, x2: 18, y2: 19 },
    { nullT: true, b: [5, 3, 1, 2], nb: -1, rot: 6, x: NULLX, y: NULLY, x2: 20, y2: 21 },
    { nullT: true, b: [3, 3, 3, 3], nb: -1, rot: 0, x: NULLX, y: NULLY, x2: 22, y2: 23 },
    { h: [-6, 7, -1, 9], b: [0x7f, 0x81, 3, 9], nb: -1, rot: 2, x: 31, y: 31, x2: 24, y2: 25 },
  ];
  F.c3ez_ate = function () {
    const fx = terBase({ seed: 3 });
    looseTiles(fx, ATE.length);
    terImports(fx, ['elevateCorner', 'lowerCorner', 'lowerEdgeCorner'], 0x2000);
    fx.ncorner = cornerAt(NULLX, NULLY, 0);
    ATE.forEach(function (v, i) {
      Object.defineProperty(fx, 't' + i, { enumerable: true, configurable: true, get: function () {
        // a free 3 x 3 neighbourhood (type 1), then the blocked neighbour
        for (let dx = -1; dx <= 1; dx++) for (let dy = -1; dy <= 1; dy++) {
          const xx = v.x + dx, yy = v.y + dy;
          if (xx >= 0 && xx < 50 && yy >= 0 && yy < 50) ptr(G_TYPE).add(cell(xx, yy)).writeS8(1);
        }
        if (v.nb >= 0) ptr(G_TYPE).add(cell(v.x + NB[v.nb][0], v.y + NB[v.nb][1])).writeS8(0x14);
        if (v.x >= 0 && v.x < 50 && v.y >= 0 && v.y < 50)
          for (let k = 0; k < 4; k++) cornerAt(v.x, v.y, 2 * k + 1).writeS8(v.b[k]);
        ptr('0x005685f4').writeS32(v.rot);
        ptr('0x00820f2c').writeU8(0);
        if (v.nullT) return ptr(0);
        const t = fx['T' + i];
        for (let k = 0; k < 4; k++) t.add(4 + 8 * k).writeS32(v.h[k]);
        return t;
      } });
      fx['x' + i] = v.x; fx['y' + i] = v.y; fx['a' + i] = 0x11110000 + i; fx['u' + i] = v.x2; fx['v' + i] = v.y2;
    });
    return fx;
  };

  // ---- rebuildTileData(x0, y0): a full 50 x 50 fixture object; g_zoomScale per key; the list 0x00820f18 is ours
  // (an empty sentinel at $lst), nodes come from the heap stand-in's arena (operator_new 0x004a57f1); vector i rewrites
  // the grids (seed i) and every tile, and empties the list.
  function listSentinel(fx) {
    fx.lst = alloc(0x20);
    fx.lst.writePointer(fx.lst);
    fx.lst.add(4).writePointer(fx.lst);
    ptr('0x00820f1c').writePointer(fx.lst);
    ptr('0x00820f20').writeU32(0);
  }
  // (x0, y0) per vector for d = 13 * (4 / zoom): immediate return; every y skipped; windows of 4 x 4, 50 x 2 (x0 - d
  // < 0), 10 x 5, 2 x 3. The ylow key: y0 - d < 0 with x 49 only, and x 48..49.
  // A third field 1 makes every tile of the window with x + y even QUIET: grids and tile agree everywhere, so none of
  // the four callees changes anything and the tile is not pushed.
  const RTD = (d) => [[d + 50, 0], [d + 47, d + 50], [d + 46, d + 46], [0, d + 48], [d + 40, d + 45], [d + 48, d + 47],
                      [d + 44, d + 45, 1]];
  const RTD_YLOW = (d) => [[d + 49, 0], [d + 48, d - 5]];
  function quietTile(fx, x, y) {
    const tile = tileOf(fx, x, y);
    tile.add(0x24).writeS32(ptr(G_TYPE).add(cell(x, y)).readS8());
    tile.add(0x30).writeU8(ptr(G_BYTE).add(cell(x, y)).readU8());
    for (let c = 1; c < 8; c += 2) tile.add(4 + 4 * (c - 1)).writeS32(cornerAt(x, y, c).readS8() - 3);
    ptr(G_WALL).add(cell(x, y)).writeU8(0);
    for (let s = 0; s < 8; s += 2) tile.add(0x234 + s).writeU8(0);
    ptr(G_FLAGS).add(cell(x, y) * 2).writeU8(0);
    tile.add(0x40).writeU8(0);
  }
  function rebuildFx(zoom, table, nullPrev) {
    return function () {
      const fx = terBase({ seed: 0, tiles: true });
      ptr('0x004c2844').writeS32(zoom);
      listSentinel(fx);
      if (nullPrev) fx.lst.add(4).writePointer(ptr(0));
      const h = __heapStandIn({ module: 'golf_clean.exe', alloc: [0x000a57f1], arenaSize: 0x10000, fill: 0xcd });
      fx.arena = h.arena; fx.freelog = h.log;
      terImports(fx, ['getVariation', 'setType', 'elevateCorner', 'lowerCorner', 'lowerEdgeCorner', 'setWall',
                      'hasPath', 'hasConnectedPath', 'updatePath', 'layPath'], 0x40000);
      const vecs = table(13 * (4 / zoom));
      vecs.forEach(function (v, i) { fx['q' + i] = v[1]; });
      for (let i = 0; i < vecs.length; i++) {
        Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () {
          putGrids(i + 10);
          for (let x = 0; x < 50; x++) for (let y = 0; y < 50; y++) putTile(fx, x, y, i);
          if (vecs[i][2]) {
            const x1 = Math.max(vecs[i][0] - 13 * (4 / zoom), 0), y1 = Math.max(vecs[i][1] - 13 * (4 / zoom), 0);
            for (let x = x1; x < 50; x++) for (let y = y1; y < 50; y++) if ((x + y) % 2 === 0) quietTile(fx, x, y);
          }
          fx.lst.writePointer(fx.lst);
          fx.lst.add(4).writePointer(nullPrev ? ptr(0) : fx.lst);
          ptr('0x00820f20').writeU32(0);
          ptr('0x00820f28').writeU32(0);
          ptr('0x00820f2c').writeU8(0);
          return vecs[i][0];
        } });
      }
      return fx;
    };
  }
  F.c3ez_rtd_z1 = rebuildFx(1, RTD);
  F.c3ez_rtd_z2 = rebuildFx(2, RTD);
  F.c3ez_rtd_z4 = rebuildFx(4, RTD);
  F.c3ez_rtd_ylow = rebuildFx(4, RTD_YLOW);
  // the list sentinel's prev is NULL: the first insert takes the `prev ? prev : node` NULL side (0x0044a6a4)
  F.c3ez_rtd_nullprev = rebuildFx(4, (d) => [[d + 47, d + 47]], true);

  // ---- initTerrain(w, h). GetVersionExA answers dwMajorVersion = major (key); [0x00519cd8] is a fake surface whose
  // slot +0x28 returns its +0x10 and counts at +0x14 (guarded: other threads reach the real surface); rebuildTerrainGrid
  // 0x00449540 is stood in. Vector i writes the 0x17 type records' bytes +0 / +4 and the mode byte [0x005a34e0].
  function initFx(major) {
    return function () {
      const fx = terBase({ seed: 4 });
      const real = ptr('0x00519cd8').readPointer();
      fx.surf = alloc(0x40);
      const vt = alloc(0x40);
      const stub = code([0x8B, 0x41, 0x10, 0xFF, 0x41, 0x14, 0xC3]);   // mov eax, [ecx+0x10]; inc dword [ecx+0x14]; ret
      for (let s = 0; s < 0x10; s++) vt.add(4 * s).writePointer(stub);
      fx.surf.writePointer(vt);
      fx.surf.add(0x10).writeU32(0x0dc00000 + major);
      if (!real.isNull()) __recDisplay.guard(fx.surf, real);
      ptr('0x00519cd8').writePointer(fx.surf);
      calleeStand(fx, 0, ptr('0x00449540'), 0, 'mscdecl', function (n) { return 0x3e000000 + n; });
      terImports(fx, ['passCollarInfo', 'initSystem', 'initTerrain'], 0x1000, {
        'KERNEL32.dll!GetVersionExA': { args: 1, record: false, impl: function (a) { a[0].add(4).writeU32(major); return 1; } },
      });
      for (let i = 0; i < 6; i++) {
        Object.defineProperty(fx, 'w' + i, { enumerable: true, configurable: true, get: function () {
          for (let t = 0; t < 0x17; t++) {
            const r = ptr(G_TDEF + 0x30 * t);
            r.writeS8(((t * 5 + i * 3) % 7) - 2);
            r.add(4).writeS8(((t * 11 + i) % 9) - 3);
          }
          ptr('0x005a34e0').writeU8(i % 3 === 2 ? 2 : (i & 1));
          ptr('0x00820f28').writeU8(0);
          return [800, 1024, 1280, 640, 0, -5][i];
        } });
      }
      return fx;
    };
  }
  F.c3ez_init_v5 = initFx(5);
  F.c3ez_init_v4 = initFx(4);
  F.c3ez_init_v6 = initFx(6);

  // ---- renderTerrainStrip: the screen size [0x00820f30] x [0x00820f34] per key; Surface_blitThunk 0x00475c60 stood in
  // (thiscall, this + 7 arguments recorded, result a mix of them); stripRender stood in at the IAT.
  function blitRet(n, a) { let r = 0x2b000000; a.forEach(function (v, i) { r = (r + Math.imul(v | 0, 2 * i + 3)) | 0; }); return r >>> 0; }
  function stripFx(w, h) {
    return function () {
      const fx = terBase({ seed: 5 });
      ptr('0x00820f30').writeS32(w);
      ptr('0x00820f34').writeS32(h);
      calleeStand(fx, 0, ptr('0x00475c60'), 8, 'thiscall', blitRet);
      terImports(fx, ['stripRender'], 0x1000);
      return fx;
    };
  }
  F.c3ez_strip_800 = stripFx(800, 600);
  F.c3ez_strip_1024 = stripFx(1024, 768);
  F.c3ez_strip_1280 = stripFx(1280, 1024);
  // sizes outside the three: their vectors take the default strip, the only case that reads no width local
  F.c3ez_strip_bad800 = stripFx(800, 500);
  F.c3ez_strip_bad1024 = stripFx(1024, 700);
  F.c3ez_strip_bad1280 = stripFx(1280, 1000);
  F.c3ez_strip_bad = stripFx(640, 480);

  // ---- updateTerrainRender(x, y, zoom, rot). Stood in (calleeStand, every call recorded): Window::paint 0x004808c0,
  // Surface_blitThunk 0x00475c60, initTerrain 0x00449790, rebuildTileData 0x0044a5b0, renderTerrainStrip 0x0044a6e0;
  // the Terrain methods at the IAT; listEraseRange 0x0044ac00 runs, with operator delete 0x004a4ffc recording ($freelog).
  // Vector i writes the globals of UTR[i] and a list of n fixture nodes (values = tile pointers of our object).
  //   [x, y, zoom, rot, first, synced, lastX, lastY, lastRot, lastZoom, wallBit2, n, normals, paths]
  const UTR = [
    [10, 12, 2, 0, 1, 0, 0, 0, 0, 2, 0, 0, 0, 0],      // first run: initTerrain, render
    [11, 13, 2, 2, 0, 1, 0, 0, 2, 2, 0, 0, 0, 0],      // synced byte: render
    [12, 14, 1, 4, 0, 0, 12, 14, 2, 1, 0, 0, 0, 0],    // rotation changed
    [13, 15, 4, 6, 0, 0, 13, 15, 6, 2, 0, 0, 0, 0],    // zoom changed
    [14, 16, 2, 0, 0, 0, 14, 16, 0, 2, 0, 0, 0, 0],    // no move, wall bit clear: rebuildTileData, empty list -> 0
    [15, 17, 2, 2, 0, 0, 15, 17, 2, 2, 2, 3, 1, 1],    // no move, list of 3, normals, path render
    [16, 18, 2, 4, 0, 0, 16, 18, 4, 2, 2, 1, 0, 1],    // no move, list of 1, path render only
    [17, 19, 2, 6, 0, 0, 17, 19, 6, 2, 0, 2, 1, 0],    // no move, wall bit clear, list of 2, normals only
    [20, 20, 2, 0, 0, 0, 17, 19, 0, 2, 0, 0, 0, 0],    // moved |3| + |1| > 2: render
    [20, 20, 2, 2, 0, 0, 19, 20, 2, 2, 0, 0, 0, 0],    // moved by (1, 0): renderTerrainStrip
    [20, 20, 2, 4, 0, 0, 21, 21, 4, 2, 0, 0, 0, 0],    // moved by (-1, -1)
    [20, 20, 2, 6, 0, 0, 20, 22, 6, 2, 0, 0, 0, 0],    // moved by (0, -2)
    [-3, 60, 2, 0, 0, 0, 0, 60, 0, 2, 0, 0, 0, 0],     // moved by (-3, 0), tile out of the grid
    [5, 5, 3, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0],        // both bytes, rotation 1
    [8, 9, 2, 5, 0, 0, 8, 9, 5, 2, 2, 4, 0, 0],        // no move, list of 4, no flags
  ];
  // Every path that builds a tile and an angle, once per rotation 0 / 2 / 4 / 6 / 7 paired with a camera tile in the grid,
  // x = 50, x = -1, y = 50, y = -1 (the four bound tests of each inline tileAt and the four arms of each angle switch).
  const XYS = [[10, 12], [50, 5], [-1, 5], [5, 50], [5, -1]];
  [0, 2, 4, 6, 7].forEach(function (rot, j) {
    const x = XYS[j][0], y = XYS[j][1];
    UTR.push([x, y, 2, rot, 1, 0, 0, 0, rot, 2, 2, 0, 0, 0]);             // first run
    UTR.push([x, y, 2, rot, 0, 0, x, y, rot + 1, 2, 2, 0, 0, 0]);         // rotation changed
    UTR.push([x, y, 1, rot, 0, 0, x, y, rot, 2, 2, 0, 0, 0]);             // zoom changed
    UTR.push([x, y, 2, rot, 0, 0, x, y, rot, 2, 2, 2, 1, 1]);             // no move: normals, two nodes, path render
    UTR.push([x, y, 2, rot, 0, 0, x - 3, y - 1, rot, 2, 2, 0, 0, 0]);     // moved by more than 2
  });
  // no move with [0x00820f20] = 1 but an empty list: the walk's `je` on the head (0x00449b45) is taken
  UTR.push([9, 9, 2, 0, 0, 0, 9, 9, 0, 2, 2, 0, 1, 0, 1]);
  F.c3ez_utr = function () {
    const fx = terBase({ seed: 6 });
    fx.nodes = alloc(0x100);
    fx.lst = alloc(0x20);
    fx.lst.writePointer(fx.lst); fx.lst.add(4).writePointer(fx.lst);
    ptr('0x00820f1c').writePointer(fx.lst);
    ptr('0x00820f30').writeS32(800);
    ptr('0x00820f34').writeS32(600);
    ptr('0x00822c8c').writeS32(800);
    ptr('0x00822c90').writeS32(600);
    const h = __heapStandIn({ module: 'golf_clean.exe', free: [0x000a4ffc], arenaSize: 0x100 });
    fx.freelog = h.log;
    calleeStand(fx, 0, ptr('0x004808c0'), 2, 'thiscall', function (n, a) { return 0x9a000000 + n; });
    calleeStand(fx, 1, ptr('0x00475c60'), 8, 'thiscall', blitRet);
    calleeStand(fx, 2, ptr('0x00449790'), 2, 'mscdecl', function (n, a) { return (0x4e000000 + a[0] + a[1]) >>> 0; });
    calleeStand(fx, 3, ptr('0x0044a5b0'), 2, 'mscdecl', function (n, a) { return (0x4f000000 + a[0] * 7 + a[1]) >>> 0; });
    calleeStand(fx, 4, ptr('0x0044a6e0'), 7, 'mscdecl', blitRet);
    terImports(fx, ['render', 'setZoomLevel', 'calcAllNormals', 'localRender', 'pathUpdateRender'], 0x1000);
    UTR.forEach(function (v, i) {
      Object.defineProperty(fx, 'ux' + i, { enumerable: true, configurable: true, get: function () {
        ptr('0x004d2044').writeU8(v[4]);
        ptr('0x00820f2a').writeU8(v[5]);
        ptr('0x004d2048').writeS32(v[6]);
        ptr('0x004d204c').writeS32(v[7]);
        ptr('0x00820f38').writeS32(v[8]);
        ptr('0x004d2050').writeS32(v[9]);
        ptr(G_WALL).writeU8((ptr(G_WALL).readU8() & ~2) | v[10]);
        ptr('0x00820f2c').writeU8(v[12]);
        ptr('0x00820f2b').writeU8(v[13]);
        ptr('0x00820f14').writeU32(0);
        // the list: n nodes in $nodes (0x10 apart), values = tiles (k, i) of our object's grid
        let prev = fx.lst;
        for (let k = 0; k < v[11]; k++) {
          const nd = fx.nodes.add(0x10 * k);
          nd.add(4).writePointer(prev);
          prev.writePointer(nd);
          nd.add(8).writePointer(tileOf(fx, k + 1, i));
          prev = nd;
        }
        prev.writePointer(fx.lst);
        fx.lst.add(4).writePointer(prev);
        ptr('0x00820f20').writeU32(v[14] === undefined ? v[11] : v[14]);
        return v[0];
      } });
      fx['uy' + i] = v[1]; fx['uz' + i] = v[2]; fx['ur' + i] = v[3];
    });
    return fx;
  };

  // ==================================================================================================================
  // The net family. Every object is ours: a session object of 0x800 bytes (vtable +0, sub-object +0x64 with its own
  // vtable, list +0xc0, queue +0x148, peer slots +0x170), and every vtable slot the rows call is a JS recorder that
  // appends {tag, this, stack arguments} to fx.clog (calleeStand's layout) and answers a value of its arguments or of
  // compared memory. Callees outside the batch are stood in by calleeStand: netService 0x00497fc0, netSendMessages
  // 0x00499140, netHandleControl 0x0049acf0, S3List_addVoiceRx 0x0049b970 (and netSendShutdown for netPoll). malloc
  // 0x004a504f / free 0x004a5007 go through the heap stand-in (arena and free log compared); the KERNEL32 / WINMM
  // imports through the import stand-in. The interface [0x008400b0] and the guards [0x008400bc] / [0x008400c0] are saved
  // and set per vector by the getters.
  // ==================================================================================================================
  function clogPut(fx, vals) {
    const log = fx.clog;
    const cur = log.readU32();
    const need = 8 + 4 * (vals.length - 1);
    if (cur + need > CLOG) return;
    log.add(cur).writeU32(vals[0] >>> 0);
    log.add(cur + 4).writeU32(vals.length - 1);
    for (let i = 1; i < vals.length; i++) log.add(cur + 4 + 4 * i).writeU32(vals[i] >>> 0);
    log.writeU32(cur + need);
  }
  // slots: { off: [tag, stack args, ret(self, args) | number] }; abi thiscall (this in ecx) or stdcall (COM: this first)
  function recVt(fx, slots, abi) {
    const vt = alloc(0x100);
    const trap = code([0xB8].concat(le(0x0badc0de), [0xC3]));
    for (let i = 0; i < 0x40; i++) vt.add(4 * i).writePointer(trap);
    Object.keys(slots).forEach(function (o) {
      const s = slots[o];
      const n = s[1] + 1;
      const cb = new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments).map((v) => v >>> 0);
        clogPut(fx, [s[0]].concat(a));
        const r = typeof s[2] === 'function' ? s[2](a[0], a.slice(1)) : (s[2] === undefined ? 0 : s[2]);
        return r >>> 0;
      }, 'uint32', Array(n).fill('uint32'), abi || 'thiscall');
      __diffKeepAlive.push(cb);
      vt.add(+o).writePointer(cb);
    });
    return vt;
  }
  const rd = (p, off) => ptr(p).add(off).readU32();
  function cstr(blk, off, s) { const p = blk.add(off); p.writeUtf8String(s); return p; }
  function netBase(fx, n) {
    freezeNow();
    __freeze.save(ptr('0x008400b0'), 0x14);      // interface, session pointer +8, guards +0xc / +0x10
    __freeze.save(ptr('0x0083ff9c'), 0x10);      // the name record netPoll's GetPlayerName call fills
    fx.objs = alloc(0x800 * n);
    fx.strs = alloc(0x1000);
    for (let i = 0; i < n; i++) fx['o' + i] = fx.objs.add(0x800 * i);
  }
  function peer(o, k, id, name, flags, t) {
    const s = o.add(0x170 + 0x58 * k);
    s.writeU32(id >>> 0);
    s.add(0x10).writeU32((t || 0) >>> 0);
    s.add(0x14).writeU8(flags || 0);
    s.add(0x15).writeUtf8String(name || '');
  }

  // ---- netPump: the sub-object's slot +0x94 answers its +0x40 (pending count), +0x88 its +0x44 (message), +0x20 its
  // +0x48; +0x8c records the message. Rows: [flags +0xe8, count, message?, mode +0x60, msg +0x18, msg +0x20, +0xe4]
  const PUMP = [[0x40000000, 0x40, 0, 0, 0, 0, 1], [0x40000000, 0x10, 0, 0, 0, 0, 2], [0x40000000, 0x32, 0, 0, 0, 0, 3],
                [0, 0x65, 0, 0, 0, 0, 4], [0, 0x64, 0, 0, 0, 0, 5], [0, 5, 1, 1, 0x1000, 0x1040, 6],
                [0, 0, 1, 0, 0x2000, 0x2010, 7], [0x40000000, 0x31, 0, 0, 0, 0, 8], [0, 0, 1, 2, 0x100, 0x80, 9],
                [0xffffffff, 0x31, 0, 0, 0, 0, 10], [0xbfffffff, 0x64, 1, 0, 7, 9, 11], [0x0, 0xffffffff, 0, 0, 0, 0, 12]];
  F.c3ez_netpump = function () {
    const fx = { clog: newClog() };
    netBase(fx, PUMP.length);
    const svt = recVt(fx, { 0x94: [0x6494, 0, (s) => rd(s, 0x40)], 0x88: [0x6488, 0, (s) => rd(s, 0x44)],
                            0x20: [0x6420, 0, (s) => rd(s, 0x48)], 0x8c: [0x648c, 1, (s, a) => a[0] ^ 0x8c8c] });
    PUMP.forEach(function (r, i) {
      const o = fx['o' + i], sub = o.add(0x64);
      sub.writePointer(svt);
      o.add(0xe8).writeU32(r[0] >>> 0);
      sub.add(0x40).writeU32(r[1] >>> 0);
      const msg = o.add(0x600);
      msg.add(0x18).writeU32(r[4]); msg.add(0x20).writeU32(r[5]);
      sub.add(0x44).writePointer(r[2] ? msg : ptr(0));
      sub.add(0x48).writeU32(0x20200000 + i);
      o.add(0x60).writeU32(r[3]);
      o.add(0xe4).writeU32(r[6]);
    });
    calleeStand(fx, 0, ptr('0x00497fc0'), 6, 'thiscall', (n, a) => (a[1] + a[2]) >>> 0);
    calleeStand(fx, 1, ptr('0x00499140'), 6, 'thiscall', (n, a) => (a[1] ^ a[2]) >>> 0);
    return fx;
  };

  // ---- netSendList: rows [+0x77c, +0x780, count +0x6f8, peers [id, name, flags]...]
  const LONG = 'a name of forty characters ............';
  const SENDL = [
    [1, 1, 1, [[0x11, 'alpha', 1]]], [1, 2, 3, [[0x11, 'alpha', 1]]], [5, 5, 0, []],
    [5, 5, 3, [[0x21, 'bravo', 0], [0x22, '', 7], [0x23, LONG, 0x80]]],
    [7, 7, 2, [[0x31, LONG, 1], [0, 'zero id', 2]]], [9, 9, 16, Array.from({ length: 16 }, (_, k) => [0x40 + k, 'p' + k, k])],
    [3, 3, 17, Array.from({ length: 16 }, (_, k) => [0x60 + k, 'q' + k + 'x', 0xff - k])],
    [0, 0, 1, [[0, '', 0]]], [2, 2, 4, [[0x71, 'thirty-one characters exactly..', 3], [0x72, 'c', 4], [0x73, 'd', 5], [0x74, 'e', 6]]],
    [4, 4, 2, [[0x81, 'name-a', 0x10], [0x82, 'name-b', 0x20]]],
  ];
  F.c3ez_netsendlist = function () {
    const fx = { clog: newClog() };
    netBase(fx, SENDL.length);
    SENDL.forEach(function (r, i) {
      const o = fx['o' + i];
      o.add(0x77c).writeU32(r[0]); o.add(0x780).writeU32(r[1]); o.add(0x6f8).writeU32(r[2]);
      r[3].forEach((p, k) => peer(o, k, p[0], p[1], p[2]));
      o.add(0x724).writeU32(0x72472400 + i);    // the field the 17th record's byte 0x1f clear lands in
    });
    const h = __heapStandIn({ module: 'golf_clean.exe', alloc: [0x000a504f], free: [0x000a5007], arenaSize: 0x4000 });
    fx.arena = h.arena; fx.freelog = h.log;
    calleeStand(fx, 0, ptr('0x00499140'), 6, 'thiscall', (n, a) => (a[1] + a[2] * 3) >>> 0);
    return fx;
  };

  // ---- netQueueMessage(name, id, flags): rows [+0x77c, +0x780, count, peers [id, name, flags]...]; +0xc8 set and
  // [+0xcc] + 8 a payload whose slot +0x84 records; the object's slot +8 records (name pointer, id).
  const NQM = [
    [1, 2, 0, [[0x11, 'a', 1], [0x12, 'b', 2], [0x13, 'c', 3]], 'found', 0x12, 0],
    [1, 2, 3, [[0x11, 'a', 1]], 'new peer', 0x14, 0],
    [1, 2, 3, [[0x11, 'a', 1], [0x12, 'b', 2]], 'flag one', 0x15, 1],
    [5, 5, 1, [[0x11, 'a', 1]], 'host', 0x16, 0x81],
    [5, 6, 2, [], 'first', 0x17, 3],
    [8, 9, 16, Array.from({ length: 16 }, (_, k) => [0x40 + k, 'p' + k, k]), 'no room', 0x99, 1],
    [8, 9, 16, Array.from({ length: 16 }, (_, k) => [0x40 + k, 'p' + k, k]), 'last', 0x4f, 0],
    [3, 3, 0, [], '', 0x21, 0xfe],
    [1, 2, 0, [[0x11, 'a', 1], [0, 'gap', 0], [0x13, 'c', 3]], 'into the gap', 0x13, 2],
    [7, 0, 0, [], 'becomes host', 7, 1],
  ];
  F.c3ez_netqueue = function () {
    const fx = { clog: newClog() };
    netBase(fx, NQM.length);
    const ovt = recVt(fx, { 8: [0x0808, 2, (s, a) => a[1]] });
    const pvt = recVt(fx, { 0x84: [0x0884, 3, (s, a) => (s ^ a[0]) >>> 0] });
    NQM.forEach(function (r, i) {
      const o = fx['o' + i];
      o.writePointer(ovt);
      o.add(0x77c).writeU32(r[0]); o.add(0x780).writeU32(r[1]); o.add(0x6f8).writeU32(r[2]);
      r[3].forEach((p, k) => peer(o, k, p[0], p[1], p[2]));
      const pay = o.add(0x780 + 0x20), node = o.add(0x780 + 0x40);
      pay.writePointer(pvt);
      node.add(8).writePointer(pay);
      o.add(0xc8).writeU32(1);
      o.add(0xcc).writePointer(node);
      fx['n' + i] = cstr(fx.strs, 0x80 * i, r[4]);
      fx['i' + i] = r[5]; fx['f' + i] = r[6];
    });
    const h = __heapStandIn({ module: 'golf_clean.exe', alloc: [0x000a504f], free: [0x000a5007], arenaSize: 0x4000 });
    fx.arena = h.arena; fx.freelog = h.log;
    calleeStand(fx, 0, ptr('0x00499140'), 6, 'thiscall', (n, a) => (a[1] + a[2] * 3) >>> 0);
    calleeStand(fx, 1, ptr('0x0049b970'), 2, 'thiscall', (n, a) => (a[0] + a[1]) >>> 0);
    return fx;
  };

  // ---- netServiceControl: timeGetTime stood in at the IAT (a scripted clock from compared memory: a counter at
  // $objs+0x7f0 indexes the array at $objs+0x7c0, so both arms read the same sequence); netSendMessages, netHandleControl
  // stood in (calleeStand). The getter writes the per-vector interface [0x008400b0] and guards [0x008400bc] / [0x008400c0]
  // (saved by netBase) and resets the counter.
  // row: [iface?, bc, c0, host 0x77c, 0x780, 0x1c8, flags 0xe8, lastTime 0xec, timeout 0xf0, peers [id, name, fl, t]...]
  const NSC = [
    [0, 0, 0, 1, 1, 0, 0x40000, 0, 1000, []],                           // no interface -> 0
    [1, 0, 0, 1, 1, 0, 0x40000, 0xfffff000, 1000, []],                  // clock - 2000 below lastTime -> early
    [1, 0, 1, 1, 1, 0, 0x40000, 0, 1000, []],                           // guard c0 set -> early
    [1, 1, 0, 1, 1, 0, 0x40000, 0, 1000, []],                           // guard bc set after the send -> early
    [1, 0, 0, 1, 1, 0, 0, 0, 1000, []],                                 // flag 0x40000 clear
    [1, 0, 0, 5, 5, 0, 0x40000, 0, 1000, [[0x11, 'a', 0, 0x100010], [0x12, 'b', 0, 0x10], [0, 'c', 0, 5], [5, 'h', 0, 5]]],  // host drop
    [1, 0, 0, 7, 9, 7, 0x40000, 0, 1000, []],                           // 0x77c == 0x1c8, +0x180 unset
    [1, 0, 0, 7, 9, 7, 0x40000, 0, 1000, [[0x70, 'p', 0, 0x10]]],       // 0x77c == 0x1c8, +0x180 stale (set below)
    [1, 0, 0, 7, 9, 0, 0x40000, 0, 1000, [[0x70, 'p0', 0, 0], [0x71, 'p1', 0, 0]]],  // peer 0 time 0 -> early
    [1, 0, 0, 7, 9, 0, 0x40000, 0, 1000, [[0x70, 'p0', 0, 0x10], [0x71, 'p1', 0, 0x10]]],  // both stale -> drop -1
    [1, 0, 0, 7, 9, 0, 0x40000, 0, 1000, [[0x70, 'p0', 0, 0x100000], [0x71, 'p1', 0, 0x100000]]],  // both recent -> early
  ];
  const CLOCK = [0x100000, 0x100300, 0x100600, 0x100900, 0x100c00, 0x100f00, 0x101200, 0x101500];
  F.c3ez_netsvcctl = function () {
    const fx = { clog: newClog() };
    netBase(fx, NSC.length);
    const cnt = fx.objs.add(0x7f0), arr = fx.objs.add(0x7c0);
    CLOCK.forEach((v, i) => arr.add(4 * i).writeU32(v >>> 0));
    cnt.writeU32(0);
    const tgt = new NativeCallback(function () {
      const k = cnt.readU32();
      cnt.writeU32(k + 1);
      return arr.add(4 * Math.min(k, CLOCK.length - 1)).readU32();
    }, 'uint32', []);
    __diffKeepAlive.push(tgt);
    fx.implog = __importStandIn({ module: 'golf_clean.exe',
      imports: { 'WINMM.dll!timeGetTime': { impl: () => tgt(), record: false } } }).log;
    NSC.forEach(function (r, i) {
      const o = fx['o' + i];
      o.add(0x77c).writeU32(r[3]); o.add(0x780).writeU32(r[4]); o.add(0x1c8).writeU32(r[5]);
      o.add(0xe8).writeU32(r[6] >>> 0); o.add(0xec).writeU32(r[7] >>> 0); o.add(0xf0).writeU32(r[8] >>> 0);
      r[9].forEach((p, k) => peer(o, k, p[0], p[1], p[2], p[3]));
      if (i === 7) o.add(0x180).writeU32(0x10);   // +0x180 stale time for the 0x1c8 branch
      Object.defineProperty(fx, 'g' + i, { enumerable: true, configurable: true, get: function () {
        ptr('0x008400b0').writeU32(r[0] ? o.toUInt32() : 0);
        ptr('0x008400bc').writeU32(r[1]);
        ptr('0x008400c0').writeU32(r[2]);
        cnt.writeU32(0);
        return o;
      } });
    });
    calleeStand(fx, 0, ptr('0x00499140'), 6, 'thiscall', (n, a) => (a[1] + a[4]) >>> 0);
    calleeStand(fx, 1, ptr('0x0049acf0'), 3, 'thiscall', (n, a) => (a[1] % 2));  // (this, id, flag); 0 on even ids
    return fx;
  };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
