// Fixtures for batch c3dp (Terrain.dll, round 27). Registry: re/frida/registry.d/c3dp.py; bodies:
// shim/src/re/c3dp.cpp. Every buffer here is our own allocation: nothing of the running game's Terrain object, tile
// list, ios objects or draw state is handed to an arm (Terrain::getInstance 0x100031a0 is never called).
//
// Terrain.dll loads at its preferred base 0x10000000, so module addresses are plain numbers below.
(function () {
  function fillPattern(p, n, seed) {
    const a = [];
    for (let i = 0; i < n; i++) a.push((i * 37 + seed * 11 + 0x5d) & 0xff);
    p.writeByteArray(a);
  }
  function zero(p, n) { p.writeByteArray(new Array(n).fill(0)); }

  // The GL imports a draw key stands in for: name -> stack dword count (all stdcall).
  const GL_ARGS = {
    glEnableClientState: 1, glHint: 2, glFrontFace: 1, glEnable: 1, glNormal3fv: 1, glVertex3fv: 1, glTexCoord2fv: 1,
    glArrayElement: 1, glTexParameterf: 3, glGenTextures: 2, glTexParameteri: 3, glTexEnvi: 3, glTexImage2D: 9,
    glVertexPointer: 4, glNormalPointer: 3, glViewport: 4, glScalef: 3, glBindTexture: 2, glNormal3f: 3,
    glTexCoord2f: 2, glVertex3f: 3, glRotated: 8, glRotatef: 4, glTranslatef: 3, glVertex2fv: 1, glMatrixMode: 1,
    glPushMatrix: 0, glLoadIdentity: 0, glOrtho: 12, glBlendFunc: 2, glLineWidth: 1, glColor4f: 4, glBegin: 1,
    glVertex2i: 2, glEnd: 0, glPopMatrix: 0, glFlush: 0, glDeleteTextures: 2, glMaterialfv: 3, glMateriali: 3,
    glDisable: 1, glLightfv: 3,
  };
  globalThis.__c3dpGL = function (names, special) {
    const imp = {};
    names.forEach(function (n) {
      imp['OPENGL32.dll!' + n] = Object.assign({ args: GL_ARGS[n], ret: 0 }, (special || {})[n] || {});
    });
    return imp;
  };

  // ---------------------------------------------------------------------------------------------------
  // Shared by the draw-family fixtures. Terrain.dll is at its preferred base 0x10000000.
  const TB = Process.getModuleByName('Terrain.dll').base;
  const TSTRIDE = 0x260;                          // a Tile is 0x248 bytes; 0x18 of canary after each

  // Module data the draw functions read, seeded and put back by the freeze helper's flush after the key:
  //   g_vertices[0..127] (0x100b28c8), g_normals[0..127] (0x10070a18)  distinct, non-dyadic floats
  //   g_textures (0x100687f8, 0x25 * 0x384 bytes)                       distinct non-zero ids (never 0xcccccccc)
  //   g_viewAngleIndex (0x10070a14) = view,  g_screenScale (0x10070a10) = 38.682186f
  function seedModuleData(view) {
    const vtx = TB.add(0xb28c8), nrm = TB.add(0x70a18), tex = TB.add(0x687f8);
    __freeze.save(vtx, 128 * 12); __freeze.save(nrm, 128 * 12); __freeze.save(tex, 0x25 * 0x384);
    __freeze.save(TB.add(0x70a10), 8);
    for (let i = 0; i < 128; i++) {
      vtx.add(i * 12).writeFloat(i * 3.7 + 0.13);
      vtx.add(i * 12 + 4).writeFloat(i * 1.1 - 5.3);
      vtx.add(i * 12 + 8).writeFloat(i * 2.9 - 7.77);
      nrm.add(i * 12).writeFloat(0.01 * i);
      nrm.add(i * 12 + 4).writeFloat(1.0 - 0.003 * i);
      nrm.add(i * 12 + 8).writeFloat(-0.02 * i);
    }
    for (let k = 0; k < 0x25 * 0x19 * 9; k++) tex.add(4 * k).writeU32(0x2000 + k * 5);
    TB.add(0x70a14).writeS32(view);
    TB.add(0x70a10).writeFloat(38.682186);
  }

  // One tile, every field the draw functions read. o: type, v28, x, y, heights[4], faces [{v:[3], uv:[6], tex, slot}],
  // hasPath, connected, drawPath, wallKind[9], wallOn[9], v240, n[4] (pointers or null).
  function putTile(p, o) {
    p.add(0x24).writeS32(o.type);
    p.add(0x28).writeS32(o.v28);
    p.add(0x2c).writeS32(o.x);
    p.add(0x30).writeS32(o.y);
    [4, 0xc, 0x14, 0x1c].forEach(function (off, i) { p.add(off).writeS32(o.heights ? o.heights[i] : 0); });
    p.add(0x44).writeS32(o.faces.length);
    for (let f = 0; f < 8; f++) {
      const fa = p.add(0x48 + f * 0x38);
      const F = o.faces[f] || { v: [0, 0, 0], uv: [0, 0, 0, 0, 0, 0], tex: 0, slot: 0 };
      for (let k = 0; k < 3; k++) fa.add(4 * k).writeS32(F.v[k]);
      for (let k = 0; k < 6; k++) fa.add(0xc + 4 * k).writeS32(F.uv[k]);
      fa.add(0x24).writeU32(F.tex >>> 0);
      fa.add(0x28).writeU8(F.slot);
    }
    p.add(0x208).writeU8(o.hasPath ? 1 : 0);
    p.add(0x209).writeU8(o.connected ? 1 : 0);
    p.add(0x20a).writeU8(o.drawPath ? 1 : 0);
    for (let k = 0; k < 9; k++) {
      p.add(0x210 + 4 * k).writeS32(o.wallKind ? o.wallKind[k] : 0);
      p.add(0x234 + k).writeU8(o.wallOn ? o.wallOn[k] : 0);
    }
    p.add(0x240).writeS32(o.v240);
    for (let k = 0; k < 4; k++) p.add(0x34 + 4 * k).writePointer(o.n && o.n[k] ? o.n[k] : ptr(0));
  }

  // The deterministic tile pool: NP tiles in one block (stride TSTRIDE), tile i described by integer arithmetic on i.
  //   type      0x14 (hidden) for i % 3 == 0, 7 for i % 5 == 1, 4 otherwise (so about a third of the neighbours are hidden)
  //   v28       i - 9 (negative for i < 9: the signed remainder of Tile::render)
  //   x, y      i % 7, (i / 7) | 0       face count i % 9 (0..8)      v240 = i % 25
  //   faces     vertex indices (i * 5 + f * 3 + k) % 120, uv rows (i + f + k) % 4, columns (f + k) % 3, texture
  //             0x10000 + i * 16 + f, slot (i + f) % 9; tile 3's face 0 carries 0xcccccccc (the frame-fill value), tile 5's
  //             faces all carry the same texture (the cached-bind skip), tile 8's alternate between two
  //   path bytes  hasPath i % 3 == 0, connected i % 2, drawPath i % 4 != 3   walls: kind (i + k) % 3 (1 selects the fence
  //             texture 1), flag set where (i * 7 + k * 3) % 5 < 3
  //   neighbours n[k] = tile (i * 3 + k * 7 + 1) % NP, NULL where (i + k) % 5 == 0
  function buildPool(NP) {
    const blk = __keepAlloc(NP * TSTRIDE);
    for (let i = 0; i < NP * TSTRIDE; i += 4) blk.add(i).writeU32(0x55aa0000 + (i >> 2));
    const tiles = [];
    for (let i = 0; i < NP; i++) tiles.push(blk.add(i * TSTRIDE));
    for (let i = 0; i < NP; i++) {
      const faces = [];
      for (let f = 0; f < i % 9; f++) {
        const v = [], uv = [];
        for (let k = 0; k < 3; k++) { v.push((i * 5 + f * 3 + k) % 120); uv.push((i + f + k) % 4, (f + k) % 3); }
        let tex = 0x10000 + i * 16 + f;
        if (i === 3 && f === 0) tex = 0xcccccccc;
        if (i === 5) tex = 0x20000 + 5;
        if (i === 8) tex = 0x30000 + (f % 2);
        faces.push({ v, uv, tex, slot: (i + f) % 9 });
      }
      const n = [];
      for (let k = 0; k < 4; k++) n.push((i + k) % 5 === 0 ? null : tiles[(i * 3 + k * 7 + 1) % NP]);
      putTile(tiles[i], {
        type: i % 3 === 0 ? 0x14 : (i % 5 === 1 ? 7 : 4), v28: i - 9, x: i % 7, y: (i / 7) | 0,
        heights: [i % 11, (i * 3) % 13, (i * 5) % 17, (i * 7) % 19], faces,
        hasPath: i % 3 === 0, connected: i % 2 === 1, drawPath: i % 4 !== 3,
        wallKind: [0, 1, 2, 3, 4, 5, 6, 7, 8].map(function (k) { return (i + k) % 3; }),
        wallOn: [0, 1, 2, 3, 4, 5, 6, 7, 8].map(function (k) { return (i * 7 + k * 3) % 5 < 3 ? 1 : 0; }),
        v240: i % 25, n,
      });
    }
    return { blk, tiles };
  }

  function drawImports(extra) {
    return __c3dpGL(['glEnable', 'glDisable', 'glBlendFunc', 'glBindTexture', 'glBegin', 'glEnd', 'glNormal3fv',
      'glVertex3fv', 'glVertex3f', 'glTexCoord2f', 'glTexCoord2fv', 'glArrayElement', 'glPushMatrix', 'glPopMatrix',
      'glFlush', 'glLoadIdentity', 'glRotated', 'glRotatef', 'glTranslatef', 'glLightfv'].concat(extra || []), {
      glLightfv: { impl: function (a, ctx) {
        const f = []; for (let k = 0; k < 4; k++) f.push(a[2].add(4 * k).readU32());
        ctx.record([a[0], a[1]].concat(f)); return 0; } } });
  }

  // The tile-pool fixture for the Tile::draw* family under view quadrant `view`: $tN = tile N (40 tiles), $blk the pool block
  // (a state region: nothing may write a tile), $implog the GL call log (0x10000 bytes).
  function poolFixture(view, logSize) {
    const s = __importStandIn({ module: 'Terrain.dll', logSize: logSize || 0x20000, imports: drawImports() });
    seedModuleData(view);
    const pool = buildPool(40);
    const out = { blk: pool.blk, obj: pool.blk, implog: s.log };
    pool.tiles.forEach(function (t, i) { out['t' + i] = t; });
    return out;
  }

  // ---------------------------------------------------------------------------------------------------
  // isCulled: Terrain stand-ins ($ts0..$ts9, only the dwords +0x1c zoom, +0x20 width, +0x24 height are read) and 36 tiles
  // ($c0..$c35) placed relative to the view centre tile $ctr = (25, 25). Offsets (dx, dy) reach far enough to leave every
  // screen window; heights (the four corner dwords maxHeight reads) are 0 except for the last tiles.
  const CULL_OFFS = [[0, 0], [1, 0], [0, 1], [-1, 0], [0, -1], [3, 2], [-3, 2], [3, -2], [-3, -2], [8, 1], [1, 8], [-8, 1],
    [1, -8], [14, 0], [0, 14], [-14, 0], [0, -14], [10, 10], [-10, -10], [10, -10], [-10, 10], [25, 5], [5, 25], [-25, 5],
    [5, -25], [40, 40], [-40, -40], [40, -40], [-40, 40], [100, 0], [0, 100], [-100, 0], [0, -100], [2, 2], [-2, -2], [6, -6]];
  const CULL_H = { 33: [100, 0, 0, 0], 34: [0, 0, 0, 300], 35: [7, 9, 11, 13] };
  function cullFixture() {
    const ts = __keepAlloc(10 * 0x40);
    for (let i = 0; i < 10 * 0x40; i += 4) ts.add(i).writeU32(0x66000000 + i);
    // zoom, width, height
    const P = [[0, 800, 600], [1, 800, 600], [2, 800, 600], [0, 1024, 768], [1, 1024, 768], [0, 1280, 1024], [1, 1280, 1024],
      [0, 640, 480], [3, 800, 600], [-1, 801, 600]];
    const out = { ts, obj: ts };
    P.forEach(function (p, i) {
      const o = ts.add(i * 0x40);
      o.add(0x1c).writeS32(p[0]); o.add(0x20).writeS32(p[1]); o.add(0x24).writeS32(p[2]);
      out['ts' + i] = o;
    });
    const tiles = __keepAlloc((CULL_OFFS.length + 1) * TSTRIDE);
    for (let i = 0; i < (CULL_OFFS.length + 1) * TSTRIDE; i += 4) tiles.add(i).writeU32(0x77000000 + i);
    out.tiles = tiles;
    CULL_OFFS.forEach(function (d, i) {
      const t = tiles.add(i * TSTRIDE);
      t.add(0x2c).writeS32(25 + d[0]); t.add(0x30).writeS32(25 + d[1]);
      const h = CULL_H[i] || [0, 0, 0, 0];
      [4, 0xc, 0x14, 0x1c].forEach(function (off, k) { t.add(off).writeS32(h[k]); });
      out['c' + i] = t;
    });
    const ctr = tiles.add(CULL_OFFS.length * TSTRIDE);
    ctr.add(0x2c).writeS32(25); ctr.add(0x30).writeS32(25);
    out.ctr = ctr;
    out.nan = NaN;
    return out;
  }

  // drawTile / drawTileObjects / stripRender / lowerEdge: the Terrain object, the draw list and the heap stand-in.
  //   the heap stand-in replaces Terrain.dll's operator new 0x10018c90 and operator delete 0x10018be0 (list nodes)
  //   $arena / $freelog are state regions; $list = the std::list<Tile*> at object + 0x164ac4.
  function heapForLists(arena) {
    return __heapStandIn({ module: 'Terrain.dll', alloc: [0x18c90], free: [0x18be0], arenaSize: arena, fill: 0xcd });
  }
  // an empty std::list<Tile*> whose sentinel node lives at `head` and whose object lives at `at`
  function initList(at, head) {
    at.writeU32(0); at.add(4).writePointer(head); at.add(8).writeS32(0);
    head.writePointer(head); head.add(4).writePointer(head); head.add(8).writeS32(0);
  }
  // a std::list<Tile*> holding `tiles` (array of pointers) in order; nodes live at nodeBase + 0x10 * k
  function fillList(at, head, nodeBase, tiles) {
    initList(at, head);
    const nodes = tiles.map(function (t, k) { const n = nodeBase.add(0x10 * k); n.add(8).writePointer(t); return n; });
    const ring = [head].concat(nodes);
    ring.forEach(function (n, k) {
      n.writePointer(ring[(k + 1) % ring.length]);
      n.add(4).writePointer(ring[(k + ring.length - 1) % ring.length]);
    });
    at.add(8).writeS32(tiles.length);
    return nodes;
  }

  // drawTile: 10 draw lists ($L0..$L9) whose `this` is a fake object address (the list at this + 0x164ac4 is the only
  // thing drawTile touches), and a tile pool ($p0..$p39) whose (x, y) are the lattice points listed in DT_XY.
  const DT_XY = [[5, 5], [7, 3], [5, 9], [5, 4], [3, 8], [3, 2], [2, 2], [2, 6], [4, 1], [4, 5], [6, 3], [9, 9], [9, 7], [8, 8],
    [8, 2], [1, 1], [0, 0], [10, 10], [5, 6], [3, 3], [7, 7], [4, 4], [6, 6], [2, 9], [9, 2], [5, 5], [7, 3], [3, 8], [2, 2],
    [9, 9], [8, 2], [1, 1], [12, 5], [5, 12], [-1, 5], [5, -1], [0, 5], [5, 0], [3, 5], [5, 3]];
  const DT_LISTS = [[], [0], [1, 2, 3, 4, 5], [6, 7, 8, 9, 10], [11, 12, 13, 14, 15], [15, 14, 13, 12, 11], [5, 4, 3, 2, 1],
    [10, 9, 8, 7, 6], [0, 1, 6, 9, 11, 13, 15, 16], [16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5]];
  function dtileFixture() {
    const h = heapForLists(0x1000);
    const blk = __keepAlloc(0x4000);
    zero(blk, 0x4000);
    const out = { blk, obj: blk, arena: h.arena, freelog: h.log };
    const pool = __keepAlloc(DT_XY.length * TSTRIDE);
    fillPattern(pool, DT_XY.length * TSTRIDE, 21);
    const tiles = DT_XY.map(function (xy, i) {
      const t = pool.add(i * TSTRIDE);
      t.add(0x2c).writeS32(xy[0]); t.add(0x30).writeS32(xy[1]);
      out['p' + i] = t;
      return t;
    });
    out.pool = pool;
    DT_LISTS.forEach(function (members, li) {
      const at = blk.add(li * 0x300), head = blk.add(li * 0x300 + 0x20), nodes = blk.add(li * 0x300 + 0x40);
      fillList(at, head, nodes, members.map(function (m) { return tiles[m]; }));
      out['L' + li] = at.sub(0x164ac4);                 // `this`: the list lies at this + 0x164ac4
    });
    out.ctr = tiles[0];
    return out;
  }

  // A 50 x 50 (or smaller, `w` x `h`) Terrain object: width +0x14, height +0x18, zoom +0x1c, screen 800 x 600 at +0x20 /
  // +0x24, flip +0x28, tiles at +0x3a4 (stride 0x248), the draw list at +0x164ac4 (sentinel node at obj + 0x1650c0).
  //   tile type 4, except 0x14 (hidden) where (x * 3 + y * 5) % 7 == 0; coordinates x, y; walls: flag k set where
  //   (x + y + k) % 4 == 0 with kind (x + k) % 3; heights (x * y) % 5 at the four corners.
  const TERR_SIZE = 0x165100;
  function sceneTerrain(w, h, zoom, openTiles) {
    const obj = __keepAlloc(TERR_SIZE);                // Memory.alloc memory is zero-filled
    if (obj.add(0x3a4).readU32() !== 0 || obj.add(TERR_SIZE - 4).readU32() !== 0) zero(obj, TERR_SIZE);
    obj.add(0x14).writeS32(w); obj.add(0x18).writeS32(h); obj.add(0x1c).writeS32(zoom);
    obj.add(0x20).writeS32(800); obj.add(0x24).writeS32(600);
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        const t = obj.add(0x3a4 + (x + y * w) * 0x248);
        t.add(0x24).writeS32(openTiles || ((x * 3 + y * 5) % 7 !== 0) ? 4 : 0x14);
        t.add(0x2c).writeS32(x); t.add(0x30).writeS32(y);
        const ht = (x * y) % 5;
        [4, 0xc, 0x14, 0x1c].forEach(function (off) { t.add(off).writeS32(ht); });
        for (let k = 0; k < 9; k++) {
          t.add(0x210 + 4 * k).writeS32((x + k) % 3);
          t.add(0x234 + k).writeU8((x + y + k) % 4 === 0 ? 1 : 0);
        }
        t.add(0x208).writeU8((x * 7 + y * 3) % 5 === 0 ? 1 : 0);          // hasPath
        t.add(0x209).writeU8(x % 2);                                        // connected
        t.add(0x20a).writeU8((x + y) % 3 === 0 ? 1 : 0);                    // drawPath
        if ((x + y) % 6 === 0) {                                            // one textured face on every sixth tile
          t.add(0x44).writeS32(1);
          const fa = t.add(0x48);
          fa.writeS32((x * 5 + y) % 100); fa.add(4).writeS32((x + y * 3) % 100); fa.add(8).writeS32((x * 2 + y * 7) % 100);
          fa.add(0x24).writeU32(0x40000 + x + y * 50);
        }
      }
    }
    // neighbours as buildArrays wires them
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        const t = obj.add(0x3a4 + (x + y * w) * 0x248);
        const at = function (xx, yy) { return xx < 0 || yy < 0 || xx >= w || yy >= h ? ptr(0) : obj.add(0x3a4 + (xx + yy * w) * 0x248); };
        t.add(0x34 + 0).writePointer(at(x, y - 1)); t.add(0x34 + 4).writePointer(at(x, y + 1));
        t.add(0x34 + 8).writePointer(at(x - 1, y)); t.add(0x34 + 12).writePointer(at(x + 1, y));
      }
    }
    initList(obj.add(0x164ac4), obj.add(0x1650c0));
    return obj;
  }

  // drawTileObjects: an 8 x 8 grid ($tile<N>, N = x + y * 8), arena for the draw list's nodes
  function dobjFixture() {
    const h = heapForLists(0x4000);
    const obj = sceneTerrain(8, 8, 0, false);
    const out = { obj, blk: obj, arena: h.arena, freelog: h.log, grid: obj.add(0x3a4), list: obj.add(0x164ac4), head: obj.add(0x1650c0) };
    for (let i = 0; i < 64; i++) out['tile' + i] = obj.add(0x3a4 + i * 0x248);
    return out;
  }

  // stripRender / lowerEdge: the full 50 x 50 grid, the GL import stand-in over the draw imports, the heap stand-in
  function sceneFixture(zoom) {
    const s = __importStandIn({ module: 'Terrain.dll', logSize: 0x30000, imports: drawImports() });
    const h = heapForLists(0x8000);
    seedModuleData(0);
    const obj = sceneTerrain(50, 50, zoom, false);
    const out = { obj, blk: obj, arena: h.arena, freelog: h.log, implog: s.log, list: obj.add(0x164ac4), head: obj.add(0x1650c0),
                  view: TB.add(0x70a14) };
    for (let i = 0; i < 2500; i++) out['tile' + i] = obj.add(0x3a4 + i * 0x248);
    return out;
  }

  // drawLine: 8 Terrain stand-ins ($f0..$f7, screen size at +0x20 / +0x24, flip byte at +0x28) and the GL import stand-in
  function lineFixture() {
    const s = __importStandIn({ module: 'Terrain.dll', logSize: 0x1000, imports: __c3dpGL(
      ['glMatrixMode', 'glPushMatrix', 'glLoadIdentity', 'glOrtho', 'glDisable', 'glEnable', 'glBlendFunc', 'glLineWidth',
       'glColor4f', 'glBegin', 'glVertex2i', 'glEnd', 'glPopMatrix', 'glFlush']) });
    const blk = __keepAlloc(0x400);
    fillPattern(blk, 0x400, 15);
    const out = { blk, obj: blk, implog: s.log };
    const P = [[800, 600, 0], [800, 600, 1], [1024, 768, 0], [1024, 768, 1], [640, 480, 1], [0, 0, 0], [1, 1, 0x100], [-5, 7, 0xff]];
    P.forEach(function (p, i) {
      const o = blk.add(i * 0x40);
      o.add(0x20).writeS32(p[0]); o.add(0x24).writeS32(p[1]); o.add(0x28).writeU32(p[2]);
      out['f' + i] = o;
    });
    return out;
  }
  // setZoomLevel: 12 Terrain stand-ins ($z0..$z11) = {screen size, flip, starting zoom}, GL stand-in over matrix mode, identity, ortho and viewport
  function zoomFixture() {
    const s = __importStandIn({ module: 'Terrain.dll', logSize: 0x1000, imports: __c3dpGL(
      ['glMatrixMode', 'glLoadIdentity', 'glOrtho', 'glViewport']) });
    const blk = __keepAlloc(0x800);
    fillPattern(blk, 0x800, 16);
    const out = { blk, obj: blk, implog: s.log };
    const P = [[800, 600, 0, 0], [800, 600, 1, 1], [1024, 768, 0, 2], [1024, 768, 1, 0], [1280, 1024, 0, 1], [1280, 1024, 1, 2],
               [640, 480, 0, 0], [640, 480, 1, 5], [800, 768, 0, 7], [800, 600, 0x100, 9], [1024, 768, 0xff, -1], [1280, 1025, 0, 3]];
    P.forEach(function (p, i) {
      const o = blk.add(i * 0x60);
      o.add(0x20).writeS32(p[0]); o.add(0x24).writeS32(p[1]); o.add(0x28).writeU32(p[2]); o.add(0x1c).writeS32(p[3]);
      out['z' + i] = o;
    });
    return out;
  }
  // initTerrain / resetTerrain: c3dp_grid's eleven grids plus the stand-in over glVertexPointer / glNormalPointer
  function gridGlFixture() {
    const s = __importStandIn({ module: 'Terrain.dll', logSize: 0x400, imports: __c3dpGL(['glVertexPointer', 'glNormalPointer']) });
    const out = globalThis.DIFF_FIXTURES.c3dp_grid();
    out.implog = s.log;
    return out;
  }

  // ---------------------------------------------------------------------------------------------------
  // The texture file loaders (LoadDIBitmap, LoadTGA, textureLoad): REAL files in the temp directory, written by the fixture and deleted by the
  // freeze helper's flush after the key; both arms read the same bytes through the module's own CRT. The heap stand-in replaces Terrain.dll's
  // malloc 0x100158e0 / free 0x10016470 / operator new 0x10018c90 / operator delete 0x10018be0 (arena 0x10000, a request that does not fit
  // returns NULL like the CRT does for an impossible size); `failMask` fails the allocation calls it names (bit k = call k).
  const kernel32 = Process.getModuleByName('kernel32.dll');
  const deleteFileA = new NativeFunction(kernel32.getExportByName('DeleteFileA'), 'int', ['pointer'], 'stdcall');
  let fileSeq = 0;
  function tempFile(name, bytes) {
    const path = Process.getTmpDir() + '\\c3dp_' + Process.id + '_' + (fileSeq++) + '_' + name;
    const f = new File(path, 'wb');
    f.write(new Uint8Array(bytes).buffer);
    f.flush(); f.close();
    __freeze.atFlush(function () { const s = Memory.allocUtf8String(path); deleteFileA(s); });
    const p = __keepAlloc(path.length + 1);
    p.writeUtf8String(path);
    return p;
  }
  function missingPath(name) {
    const p = __keepAlloc(0x100);
    p.writeUtf8String(Process.getTmpDir() + '\\c3dp_missing_' + Process.id + '_' + name);
    return p;
  }
  function pat(n, seed) { const a = []; for (let i = 0; i < n; i++) a.push((i * 29 + seed * 7 + 0x11) & 0xff); return a; }
  function le(v, n) { const a = []; for (let i = 0; i < n; i++) a.push((v >>> (8 * i)) & 0xff); return a; }
  // a .bmp: o = { magic, w, h, bpp, sizeImage, pal (entries), offBits, dataLen }
  function bmp(o) {
    const pal = o.pal || 0;
    const offBits = o.offBits !== undefined ? o.offBits : 14 + 40 + pal * 4;
    const head = (o.magic || [0x42, 0x4d]).concat(le(0, 4), le(0, 4), le(offBits >>> 0, 4));
    const info = le(40, 4).concat(le(o.w >>> 0, 4), le(o.h >>> 0, 4), le(1, 2), le(o.bpp, 2), le(0, 4), le(o.sizeImage >>> 0, 4),
                                  le(0, 4), le(0, 4), le(pal, 4), le(0, 4));
    return head.concat(o.truncAt === 14 ? [] : info, pat(pal * 4, 3), pat(o.dataLen || 0, o.w & 0xf));
  }
  // a .tga: header 12 bytes {0,0,2,0...}, then w lo, w hi, h lo, h hi, bpp, descriptor, then dataLen bytes
  function tga(o) {
    const h12 = o.h12 || [0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    const h6 = [o.w & 255, (o.w >> 8) & 255, o.h & 255, (o.h >> 8) & 255, o.bpp, 0];
    return h12.concat(o.cut === 12 ? [] : h6, pat(o.dataLen || 0, o.w & 0xf));
  }
  function fileHeapFixture(failMask) {
    const h = __heapStandIn({ module: 'Terrain.dll', alloc: [0x158e0, 0x18c90], free: [0x16470, 0x18be0], arenaSize: 0x10000,
                              fill: 0xcd, exhaust: 'null', failMask: failMask || 0 });
    return h;
  }
  function glFileImports() {
    return Object.assign(__c3dpGL(['glBindTexture', 'glTexParameterf', 'glTexParameteri', 'glTexEnvi', 'glTexImage2D'], {}), {
      'OPENGL32.dll!glGenTextures': { args: 2, impl: function (a, ctx) {
        const id = 0x5000 + ctx.call; a[1].writeU32(id); ctx.record([a[0], id]); return 0; } },
      'GLU32.dll!gluBuild1DMipmaps': { args: 6, ret: 0 },
      'GLU32.dll!gluBuild2DMipmaps': { args: 7, ret: 0 },
    });
  }
  function fileFixtureBase(failMask) {
    const s = __importStandIn({ module: 'Terrain.dll', logSize: 0x1000, imports: glFileImports() });
    const h = fileHeapFixture(failMask);
    const slots = __keepAlloc(0x200);                  // out slots for LoadDIBitmap's BITMAPINFO** (one dword each)
    fillPattern(slots, 0x200, 17);
    return { implog: s.log, arena: h.arena, freelog: h.log, obj: h.arena, slots: slots };
  }
  function dibFixture(failMask) {
    const out = fileFixtureBase(failMask);
    const F = [
      ['missing', null],
      ['empty', []],
      ['short13', pat(13, 1)],
      ['badmagic', bmp({ magic: [0x58, 0x58], w: 2, h: 2, bpp: 24, sizeImage: 12, dataLen: 12 })],
      ['noinfo', bmp({ w: 2, h: 2, bpp: 24, sizeImage: 12, truncAt: 14 })],
      ['ok24', bmp({ w: 4, h: 2, bpp: 24, sizeImage: 24, dataLen: 24 })],
      ['ok24calc', bmp({ w: 3, h: 3, bpp: 24, sizeImage: 0, dataLen: 27 })],
      ['ok8', bmp({ w: 2, h: 2, bpp: 8, sizeImage: 0, pal: 256, dataLen: 4 })],
      ['okneg', bmp({ w: 3, h: -2, bpp: 24, sizeImage: 0, dataLen: 18 })],
      ['truncdata', bmp({ w: 4, h: 2, bpp: 24, sizeImage: 24, dataLen: 10 })],
      ['offbits4', bmp({ w: 2, h: 2, bpp: 24, sizeImage: 12, offBits: 4, dataLen: 12 })],
      ['offbits14', bmp({ w: 2, h: 2, bpp: 24, sizeImage: 12, offBits: 14, dataLen: 12 })],
      ['ok1', bmp({ w: 16, h: 1, bpp: 1, sizeImage: 0, pal: 2, dataLen: 2 })],
      ['ok32', bmp({ w: 2, h: 2, bpp: 32, sizeImage: 0, dataLen: 16 })],
      ['zerowidth', bmp({ w: 0, h: 5, bpp: 24, sizeImage: 0, dataLen: 0 })],
      ['okh1', bmp({ w: 4, h: 1, bpp: 24, sizeImage: 12, dataLen: 12 })],
    ];
    F.forEach(function (f, i) {
      out['p' + i] = f[1] === null ? missingPath(f[0]) : tempFile(f[0] + '.bmp', f[1]);
      out['o' + i] = out.slots.add(i * 4);
    });
    return out;
  }
  function tgaFixture(failMask) {
    const out = fileFixtureBase(failMask);
    const F = [
      ['missing', null],
      ['empty', []],
      ['short11', pat(11, 2)],
      ['badtype', tga({ h12: [0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0], w: 2, h: 2, bpp: 24, dataLen: 12 })],
      ['no6', tga({ w: 2, h: 2, bpp: 24, cut: 12 })],
      ['short6', tga({ w: 2, h: 2, bpp: 24, cut: 12 }).concat(pat(5, 4))],
      ['w0', tga({ w: 0, h: 2, bpp: 24, dataLen: 12 })],
      ['h0', tga({ w: 2, h: 0, bpp: 24, dataLen: 12 })],
      ['bpp16', tga({ w: 2, h: 2, bpp: 16, dataLen: 8 })],
      ['bpp8', tga({ w: 2, h: 2, bpp: 8, dataLen: 4 })],
      ['ok24', tga({ w: 2, h: 2, bpp: 24, dataLen: 12 })],
      ['ok32', tga({ w: 3, h: 1, bpp: 32, dataLen: 12 })],
      ['ok1x1', tga({ w: 1, h: 1, bpp: 24, dataLen: 3 })],
      ['trunc', tga({ w: 2, h: 2, bpp: 24, dataLen: 6 })],
      ['ok16x4', tga({ w: 16, h: 4, bpp: 32, dataLen: 256 })],
      ['w256', tga({ w: 256, h: 1, bpp: 24, dataLen: 768 })],
      ['big300', tga({ w: 300, h: 1, bpp: 32, dataLen: 1200 })],
    ];
    F.forEach(function (f, i) { out['p' + i] = f[1] === null ? missingPath(f[0]) : tempFile(f[0] + '.tga', f[1]); });
    return out;
  }
  function texFixture() {
    const out = fileFixtureBase(0);
    const F = [
      ['missing', null],
      ['bmp2d', bmp({ w: 2, h: 2, bpp: 24, sizeImage: 12, dataLen: 12 })],
      ['bmp1d', bmp({ w: 4, h: 1, bpp: 24, sizeImage: 12, dataLen: 12 })],
      ['bmp5x3', bmp({ w: 5, h: 3, bpp: 24, sizeImage: 0, dataLen: 48 })],
      ['badmagic', bmp({ magic: [0x4e, 0x4e], w: 2, h: 2, bpp: 24, sizeImage: 12, dataLen: 12 })],
    ];
    F.forEach(function (f, i) { out['p' + i] = f[1] === null ? missingPath(f[0]) : tempFile(f[0] + '.bmp', f[1]); });
    return out;
  }

  // ---------------------------------------------------------------------------------------------------
  // passCollarInfo / closeSystem / ~Terrain
  //   g_collarInfo (0x10106b48) and the dword 0x10106b4c are module globals: saved and put back by the freeze helper's flush, and state regions
  //   by absolute address so each arm starts from the same value.
  function collarFixture(preset) {
    const h = __heapStandIn({ module: 'Terrain.dll', alloc: [0x18c90], free: [0x18be0], arenaSize: 0x4000, fill: 0xcd, exhaust: 'null' });
    const g = Process.getModuleByName('Terrain.dll').base.add(0x106b48);
    __freeze.save(g, 8);
    const old = __keepAlloc(0x100);
    fillPattern(old, 0x100, 18);
    g.writePointer(preset ? old : ptr(0));
    g.add(4).writeU32(preset ? 1 : 0);
    const info = __keepAlloc(0x400);
    for (let i = 0; i < 0x100; i++) info.add(4 * i).writeS32((i * 2654435761) | 0);
    return { arena: h.arena, freelog: h.log, obj: h.arena, info: info, old: old, g: g };
  }
  function closeFixture(mc, del) {
    const s = __importStandIn({ module: 'Terrain.dll', logSize: 0x10000, imports: Object.assign(__c3dpGL(['glDeleteTextures'], {
      glDeleteTextures: { impl: function (a, ctx) {
        const v = [a[0]]; for (let k = 0; k < 9; k++) v.push(a[1].add(4 * k).readU32());
        ctx.record(v); return 0; } } }), {
      'OPENGL32.dll!wglMakeCurrent': { args: 2, ret: mc },
      'OPENGL32.dll!wglDeleteContext': { args: 1, ret: del },
      'USER32.dll!MessageBoxA': { args: 4, ret: 1 },
    }) });
    const tbl = Process.getModuleByName('Terrain.dll').base.add(0x687f8);
    __freeze.save(tbl, 0x25 * 0x384);
    for (let k = 0; k < 0x25 * 0x19 * 9; k++) tbl.add(4 * k).writeU32(0x1000 + k * 3);
    const blk = __keepAlloc(4 * 0x400);
    fillPattern(blk, 4 * 0x400, 19);
    const out = { blk, obj: blk, implog: s.log, tbl };
    const P = [[0, 0], [0x1234, 0], [0x1234, 1], [0, 1]];
    P.forEach(function (p, i) {
      const o = blk.add(i * 0x400);
      o.writeU32(p[0]); o.add(0x29).writeU8(p[1]); o.add(0x28).writeU8(0);
      for (let t = 0; t < 0x25; t++) o.add(0x40 + t * 0x18).writeU32(0x700 + t);
      out['s' + i] = o;
    });
    return out;
  }
  // ~Terrain: two full-size Terrain objects ($d0 without and $d1 with the textures-loaded flag), each with a draw list of three nodes whose tiles are in the
  // array and tiles wired to each other, so the vector destructor's Tile::~Tile calls run setNeighbour on real neighbours.
  function dtorFixture(preset) {
    const c = collarFixture(preset);
    const s = __importStandIn({ module: 'Terrain.dll', logSize: 0x10000, imports: __c3dpGL(['glDeleteTextures'], {
      glDeleteTextures: { impl: function (a, ctx) {
        const v = [a[0]]; for (let k = 0; k < 9; k++) v.push(a[1].add(4 * k).readU32());
        ctx.record(v); return 0; } } }) });
    const tbl = Process.getModuleByName('Terrain.dll').base.add(0x687f8);
    __freeze.save(tbl, 0x25 * 0x384);
    for (let k = 0; k < 0x25 * 0x19 * 9; k++) tbl.add(4 * k).writeU32(0x1000 + k * 3);
    const out = { arena: c.arena, freelog: c.freelog, obj: c.obj, implog: s.log, tbl: tbl, g: c.g };
    for (let d = 0; d < 2; d++) {
      const o = __keepAlloc(0x165400);
      o.add(0x14).writeS32(50); o.add(0x18).writeS32(50);
      o.add(0x29).writeU8(d);
      for (let t = 0; t < 0x25; t++) o.add(0x40 + t * 0x18).writeU32(0x700 + t);
      for (let i = 0; i < 0x9c4; i++) {
        const tile = o.add(0x3a4 + i * 0x248);
        tile.add(0x2c).writeS32(i % 50); tile.add(0x30).writeS32((i / 50) | 0); tile.add(0x24).writeS32(4);
        if (i % 7 === 0 && i + 1 < 0x9c4) {                         // wire tile i and i + 1 as east/west neighbours
          const nb = o.add(0x3a4 + (i + 1) * 0x248);
          tile.add(0x34 + 12).writePointer(nb); nb.add(0x34 + 8).writePointer(tile);
        }
      }
      // the draw list: a head node and three nodes holding tiles 0, 1, 2
      initList(o.add(0x164ac4), o.add(0x1650c0));
      fillList(o.add(0x164ac4), o.add(0x1650c0), o.add(0x165100), [0, 1, 2].map(function (i) { return o.add(0x3a4 + i * 0x248); }));
      out['d' + d] = o;
    }
    return out;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dp_collar_null() { return collarFixture(false); },
    c3dp_collar_set() { return collarFixture(true); },
    c3dp_close_ok() { return closeFixture(1, 1); },
    c3dp_close_mc0() { return closeFixture(0, 1); },
    c3dp_close_del0() { return closeFixture(1, 0); },
    c3dp_close_both0() { return closeFixture(0, 0); },
    c3dp_dtor_null() { return dtorFixture(false); },
    c3dp_dtor_set() { return dtorFixture(true); },
  });

  // ---------------------------------------------------------------------------------------------------
  // The lighting file (loadLighting, rebuild, initLists): temp files read through the module's old-iostream ifstream, the heap stand-in (the
  // filebuf buffer comes from operator new), and the OPENGL32 light / material calls. glLightfv records the light, the parameter name and the floats
  // it points at: four for GL_POSITION (0x1203), three for the colours (the original reads a fourth float from beyond a float[3]). The course type
  // global 0x10070a0c is set per key and put back by the freeze helper's flush.
  function strBytes(t) { const a = []; for (let i = 0; i < t.length; i++) a.push(t.charCodeAt(i) & 0xff); return a; }
  function lightImports() {
    return __c3dpGL(['glEnable', 'glLightfv', 'glMaterialfv', 'glMateriali'], {
      glLightfv: { impl: function (a, ctx) {
        const n = a[1].toUInt32() === 0x1203 ? 4 : 3, v = [a[0], a[1]];
        for (let k = 0; k < n; k++) v.push(a[2].add(4 * k).readU32());
        ctx.record(v); return 0; } },
      glMaterialfv: { impl: function (a, ctx) {
        const v = [a[0], a[1]]; for (let k = 0; k < 4; k++) v.push(a[2].add(4 * k).readU32());
        ctx.record(v); return 0; } },
    });
  }
  function lightFixture(courseType) {
    const s = __importStandIn({ module: 'Terrain.dll', logSize: 0x1000, imports: lightImports() });
    // no heap stand-in: the stdio and filebuf blocks come from the CRT's own debug heap and are freed by the CRT, in both arms alike
    const base = Process.getModuleByName('Terrain.dll').base;
    __freeze.save(base.add(0x70a0c), 4);
    base.add(0x70a0c).writeS32(courseType);
    const out = { implog: s.log, obj: s.log };
    const F = [
      ['std', strBytes('#AMBIENT\r\n200 200 180\r\n#DIFFUSE\r\n240 240 240\r\n#SPECULAR\r\n255 255 245\r\n')],
      ['lf', strBytes('#AMBIENT\n10 20 30\n#DIFFUSE\n40 50 60\n#SPECULAR\n70 80 90\n')],
      ['junk', strBytes('junk one\nmore junk here\n#AMBIENT\n0 0 0\nx\n#DIFFUSE\n255 255 255\ny z\n#SPECULAR\n128 129 130\n')],
      ['trail', strBytes('#AMBIENT extra\n1 2 3\n#DIFFUSE extra\n4 5 6\n#SPECULAR extra\n7 8 9\n')],
      ['wild', strBytes('#AMBIENT\n300 -5 abc\n#DIFFUSE\n1000 0 999\n#SPECULAR\n-1 -2 -3\n')],
      ['space', strBytes('#AMBIENT\n  10   20   30\n#DIFFUSE\n 255 128 64 \n#SPECULAR\n 1 1 1\n')],
      ['hdr', strBytes('# lighting\n# course\n#AMBIENT\n100 110 120\n#DIFFUSE\n130 140 150\n#SPECULAR\n160 170 180\n#HIGHLIGHT\nxyz\n')],
    ];
    F.forEach(function (f, i) { out['p' + i] = tempFile(f[0] + '.txt', f[1]); });
    out.pmissing = missingPath('nolight.txt');
    // the four real files by their relative names, and the default name, as rebuild builds them
    [['pPark', 0x5f1c4], ['pDesert', 0x5f190], ['pTrop', 0x5f1a8], ['pLinks', 0x5f178], ['pDef', 0x5f168]].forEach(function (e) { out[e[0]] = base.add(e[1]); });
    out.ctype = base.add(0x70a0c);
    if (courseType < 0 || courseType > 3) {
      // the default arm of rebuild asks for the relative name "lighting.txt": write one next to the game and delete it at the flush
      const f = new File('lighting.txt', 'wb');
      f.write(new Uint8Array(strBytes('#AMBIENT\r\n120 130 140\r\n#DIFFUSE\r\n150 160 170\r\n#SPECULAR\r\n180 190 200\r\n')).buffer);
      f.flush(); f.close();
      __freeze.atFlush(function () { deleteFileA(Memory.allocUtf8String('lighting.txt')); });
    }
    return out;
  }

  // istream::getline: four ifstreams ($ifs0..$ifs3, 0x200 bytes each) built by the module's own constructor on temp files and PRIMED with the line reader
  // so that each stream's buffer already holds its whole file (the second arm of a vector then never reads the file again: the OS file offset is not
  // part of the restored state). $ifs0 has read 3 characters, $ifs1 is at the start of its file (get pointer put back), $ifs2 has read the whole file,
  // $ifs3 is at the start of a file of empty lines. The streams are
  // closed and destroyed by the freeze helper's flush after the key. The import stand-in over EnterCriticalSection / LeaveCriticalSection is installed
  // AFTER the priming, so only the calls of the two arms are logged. $dst is the 0x200-byte destination buffer.
  function getlineFixture() {
    const base = Process.getModuleByName('Terrain.dll').base;
    const thiscallCtor = function (obj, name) {
      // __thiscall: ecx = obj, four stack arguments; Frida's 'thiscall' puts the first argument in ecx
      const f = new NativeFunction(base.add(0x33980), 'pointer', ['pointer', 'pointer', 'int', 'int', 'int'], 'thiscall');
      return f(obj, name, 0x20, base.add(0x612c4).readS32(), 1);
    };
    const line = new NativeFunction(base.add(0x348a0), 'pointer', ['pointer', 'pointer', 'int', 'int'], 'thiscall');
    const closeF = new NativeFunction(base.add(0x33e90), 'void', ['pointer'], 'thiscall');
    const dtorF = new NativeFunction(base.add(0x46a0), 'void', ['pointer'], 'thiscall');
    const texts = [
      'first line\nsecond line is a bit longer than the first\n\nx\nlast line without a newline',
      'alpha beta\ngamma\nxyz x xx\n',
      'one\ntwo\nthree\n',
      '\n\n\n\n',
    ];
    const out = {};
    const scratch = __keepAlloc(0x200);
    const state = [];
    texts.forEach(function (t, i) {
      const ifs = __keepAlloc(0x200);
      fillPattern(ifs, 0x200, 30 + i);
      thiscallCtor(ifs, tempFile('gl' + i + '.txt', strBytes(t)));
      if (i === 0) line(ifs, scratch, 4, 10);                 // 3 characters
      else if (i === 2) { for (let k = 0; k < 5; k++) line(ifs, scratch, 100, 10); }   // drain to the end
      else line(ifs, scratch, 2, 10);                          // fill the buffer ...
      out['ifs' + i] = ifs;
      out['fb' + i] = ifs.add(0x10).readPointer();     // the filebuf (a 0x54-byte CRT-heap block) holds the get pointers
      if (i === 1 || i === 3) out['fb' + i].add(0x28).writePointer(out['fb' + i].add(0x24).readPointer());   // ... and put the get pointer back at the start (gptr = eback)
      __freeze.atFlush(function () { closeF(ifs); dtorF(ifs); });
    });
    const dst = __keepAlloc(0x200);
    fillPattern(dst, 0x200, 41);
    out.dst = dst;
    const s = __importStandIn({ module: 'Terrain.dll', logSize: 0x400, imports: {
      'KERNEL32.dll!EnterCriticalSection': { ret: 0 },
      'KERNEL32.dll!LeaveCriticalSection': { ret: 0 },
    } });
    out.implog = s.log;
    out.obj = dst;
    return out;
  }

  // ifstream::{vbase dtor}: four ifstreams ($ifs0..$ifs3) built by the module's constructor on files that do not exist (ios::nocreate), i.e. FAILED
  // streams with no open descriptor: destroying one twice (once per arm) closes nothing. The CRT's frees go through free 0x10016470 / operator delete
  // 0x10018be0, which the heap stand-in only records (the filebuf blocks are the CRT heap's, the same addresses in both arms). The ios static counter
  // 0x10110b48 and the static section 0x10110b10 are state regions; DeleteCriticalSection is stood in. The streams are leaked on purpose.
  function ifsDtorFixture() {
    const base = Process.getModuleByName('Terrain.dll').base;
    const ctor = new NativeFunction(base.add(0x33980), 'pointer', ['pointer', 'pointer', 'int', 'int', 'int'], 'thiscall');
    const out = {};
    for (let i = 0; i < 4; i++) {
      const ifs = __keepAlloc(0x200);
      fillPattern(ifs, 0x200, 50 + i);
      ctor(ifs, missingPath('nofile' + i), 0x20, base.add(0x612c4).readS32(), 1);
      out['ifs' + i] = ifs;
      out['fb' + i] = ifs.add(0x10).readPointer();
    }
    const h = __heapStandIn({ module: 'Terrain.dll', alloc: [], free: [0x16470, 0x18be0], arenaSize: 0x100, fill: 0xcd });
    const s = __importStandIn({ module: 'Terrain.dll', logSize: 0x400, imports: {
      'KERNEL32.dll!DeleteCriticalSection': { args: 1, ret: 0 },
      'KERNEL32.dll!EnterCriticalSection': { ret: 0 },
      'KERNEL32.dll!LeaveCriticalSection': { ret: 0 },
    } });
    out.freelog = h.log; out.arena = h.arena; out.implog = s.log; out.obj = h.arena;
    return out;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dp_ifsdtor() { return ifsDtorFixture(); },
  });

  // Terrain::relight: loads every texture of the course (about 600 .bmp / .tga files under ./Data/Textures/<course>/, relative to the game directory)
  // through textureLoad / LoadTGA, which are this batch's own hooks or the original code, under the heap stand-in (arena 20 MB; only its 0x10-byte header
  // is compared: relight does not read the pixels) and the stand-in over the texture GL calls and GLU's mipmap builders. Two Terrain stand-ins ($r0 with
  // the loaded flag clear, $r1 with it set, so reloadTextures runs first) carry the type names of the real constructor (strcpy table at 0x10002ae0).
  // g_textures (0x687f8, 0x8214 bytes) is seeded and put back; g_courseType is set per key.
  function relightFixture(courseType) {
    const imports = Object.assign(__c3dpGL(['glBindTexture', 'glTexParameterf', 'glTexParameteri', 'glTexEnvi', 'glTexImage2D'], {}), {
      'OPENGL32.dll!glGenTextures': { args: 2, impl: function (a, ctx) {
        const id = 0x5000 + ctx.call; a[1].writeU32(id); ctx.record([a[0], id]); return 0; } },
      'OPENGL32.dll!glDeleteTextures': { args: 2, impl: function (a, ctx) {
        const v = [a[0]]; for (let k = 0; k < 9; k++) v.push(a[1].add(4 * k).readU32());
        ctx.record(v); return 0; } },
      'GLU32.dll!gluBuild1DMipmaps': { args: 6, ret: 0 },
      'GLU32.dll!gluBuild2DMipmaps': { args: 7, ret: 0 },
    });
    const s = __importStandIn({ module: 'Terrain.dll', logSize: 0x40000, imports: imports });
    const h = __heapStandIn({ module: 'Terrain.dll', alloc: [0x158e0, 0x18c90], free: [0x16470, 0x18be0], arenaSize: 0x1400000, fill: 0xcd, exhaust: 'null' });
    const base = Process.getModuleByName('Terrain.dll').base;
    __freeze.save(base.add(0x70a0c), 4);
    base.add(0x70a0c).writeS32(courseType);
    const tbl = base.add(0x687f8);
    __freeze.save(tbl, 0x25 * 0x384);
    for (let k = 0; k < 0x25 * 0x19 * 9; k++) tbl.add(4 * k).writeU32(0x900 + k);
    const NAMES = { 0: 'Tee', 1: 'PuttingGreen', 2: 'Fairway', 3: 'FirmFairway', 4: 'Rough', 5: 'DeepRough', 8: 'GrassySand', 9: 'PotSandBunker',
      10: 'Overgrowth', 11: 'Brush', 12: 'Rock', 13: 'Woods', 17: 'WaterShallow', 18: 'Marsh', 19: 'Overgrowth', 22: 'Building', 23: 'WaterMiddle',
      24: 'WaterDeep', 25: 'WaterShallowDesert', 26: 'TrickyGreen', 27: 'SandBunker1', 28: 'SandBunker2', 29: 'SandBunker3', 30: 'SandBunker4' };
    const out = { implog: s.log, arena: h.arena, tbl: tbl, obj: h.arena };
    for (let d = 0; d < 2; d++) {
      const o = __keepAlloc(0x3a8);
      zero(o, 0x3a8);
      o.add(0x29).writeU8(d);
      Object.keys(NAMES).forEach(function (i) { o.add(0x2c + i * 0x18).writeUtf8String(NAMES[i]); });
      out['r' + d] = o;
    }
    return out;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dp_relight_c0() { return relightFixture(0); },
    c3dp_relight_c1() { return relightFixture(1); },
    c3dp_relight_c2() { return relightFixture(2); },
    c3dp_relight_c3() { return relightFixture(3); },
    c3dp_relight_cdef() { return relightFixture(9); },
  });

  // Terrain::initSystem: the relight fixture's imports plus everything initGL, loadLighting, resize and the GL-context setup call (ChoosePixelFormat,
  // SetPixelFormat, wglCreateContext, wglMakeCurrent, MessageBoxA), whose results are the key's (cp, sp, cc, mc). Two Terrain stand-ins: $s0 with no GL
  // context (rc = 0) and $s1 with one (rc = 0x777, the early return). g_screenScale 0x10070a10 is a state region and is put back at the flush.
  function initSystemFixture(cp, sp, cc, mc) {
    const imports = Object.assign(__c3dpGL(['glBindTexture', 'glTexParameterf', 'glTexParameteri', 'glTexEnvi', 'glTexImage2D', 'glEnable', 'glLightfv',
      'glMaterialfv', 'glMateriali', 'glViewport', 'glMatrixMode', 'glLoadIdentity', 'glOrtho', 'glHint', 'glFrontFace', 'glEnableClientState'], {
      glLightfv: { impl: function (a, ctx) {
        const n = a[1].toUInt32() === 0x1203 ? 4 : 3, v = [a[0], a[1]];
        for (let k = 0; k < n; k++) v.push(a[2].add(4 * k).readU32());
        ctx.record(v); return 0; } },
      glMaterialfv: { impl: function (a, ctx) {
        const v = [a[0], a[1]]; for (let k = 0; k < 4; k++) v.push(a[2].add(4 * k).readU32());
        ctx.record(v); return 0; } },
    }), {
      'OPENGL32.dll!glGenTextures': { args: 2, impl: function (a, ctx) {
        const id = 0x5000 + ctx.call; a[1].writeU32(id); ctx.record([a[0], id]); return 0; } },
      'OPENGL32.dll!glDeleteTextures': { args: 2, ret: 0 },
      'GLU32.dll!gluBuild1DMipmaps': { args: 6, ret: 0 },
      'GLU32.dll!gluBuild2DMipmaps': { args: 7, ret: 0 },
      'GDI32.dll!ChoosePixelFormat': { args: 2, ret: cp },
      'GDI32.dll!SetPixelFormat': { args: 3, ret: sp },
      'OPENGL32.dll!wglCreateContext': { args: 1, ret: cc },
      'OPENGL32.dll!wglMakeCurrent': { args: 2, ret: mc },
      'USER32.dll!MessageBoxA': { args: 4, ret: 1 },
    });
    const s = __importStandIn({ module: 'Terrain.dll', logSize: 0x40000, imports: imports });
    const h = __heapStandIn({ module: 'Terrain.dll', alloc: [0x158e0, 0x18c90], free: [0x16470, 0x18be0], arenaSize: 0x1400000, fill: 0xcd, exhaust: 'null' });
    const base = Process.getModuleByName('Terrain.dll').base;
    __freeze.save(base.add(0x70a0c), 4);
    base.add(0x70a0c).writeS32(0);
    __freeze.save(base.add(0x70a10), 4);
    const tbl = base.add(0x687f8);
    __freeze.save(tbl, 0x25 * 0x384);
    for (let k = 0; k < 0x25 * 0x19 * 9; k++) tbl.add(4 * k).writeU32(0x900 + k);
    const NAMES = { 0: 'Tee', 1: 'PuttingGreen', 2: 'Fairway', 3: 'FirmFairway', 4: 'Rough', 5: 'DeepRough', 8: 'GrassySand', 9: 'PotSandBunker',
      10: 'Overgrowth', 11: 'Brush', 12: 'Rock', 13: 'Woods', 17: 'WaterShallow', 18: 'Marsh', 19: 'Overgrowth', 22: 'Building', 23: 'WaterMiddle',
      24: 'WaterDeep', 25: 'WaterShallowDesert', 26: 'TrickyGreen', 27: 'SandBunker1', 28: 'SandBunker2', 29: 'SandBunker3', 30: 'SandBunker4' };
    const out = { implog: s.log, arena: h.arena, tbl: tbl, obj: h.arena };
    for (let d = 0; d < 2; d++) {
      const o = __keepAlloc(0x3a8);
      zero(o, 0x3a8);
      o.writeU32(d ? 0x777 : 0);
      Object.keys(NAMES).forEach(function (i) { o.add(0x2c + i * 0x18).writeUtf8String(NAMES[i]); });
      out['s' + d] = o;
    }
    return out;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dp_initsys_ok() { return initSystemFixture(3, 1, 0x5555, 1); },
    c3dp_initsys_cp0() { return initSystemFixture(0, 1, 0x5555, 1); },
    c3dp_initsys_sp0() { return initSystemFixture(3, 0, 0x5555, 1); },
    c3dp_initsys_cc0() { return initSystemFixture(3, 1, 0, 1); },
    c3dp_initsys_mc0() { return initSystemFixture(3, 1, 0x5555, 0); },
  });

  // Terrain::Terrain: the relight fixture's stand-ins plus a full-size Terrain object $t (0x165400 bytes of pattern) for the constructor to initialise.
  // g_collarInfo 0x10106b48 starts at 0x1234 and g_courseType at 9, so the stores of 0 are visible; both are put back at the flush.
  function ctorFixture() {
    const out = relightFixture(9);
    const base = Process.getModuleByName('Terrain.dll').base;
    __freeze.save(base.add(0x106b48), 4);
    base.add(0x106b48).writeU32(0x1234);
    const t = __keepAlloc(0x165400);
    fillPattern(t, 0x165400, 77);
    out.t = t;
    out.obj = t;
    return out;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dp_ctor() { return ctorFixture(); },
  });

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dp_getline() { return getlineFixture(); },
  });

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dp_light_t0() { return lightFixture(0); },
    c3dp_light_t1() { return lightFixture(1); },
    c3dp_light_t2() { return lightFixture(2); },
    c3dp_light_t3() { return lightFixture(3); },
    c3dp_light_t4() { return lightFixture(4); },
    c3dp_light_tneg() { return lightFixture(-1); },
  });

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dp_dib() { return dibFixture(0); },
    c3dp_dib_f1() { return dibFixture(1); },
    c3dp_dib_f2() { return dibFixture(2); },
    c3dp_tga() { return tgaFixture(0); },
    c3dp_tga_f1() { return tgaFixture(1); },
    c3dp_tex() { return texFixture(); },
  });

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dp_cull() { return cullFixture(); },
    c3dp_gl_line() { return lineFixture(); },
    c3dp_gl_zoom() { return zoomFixture(); },
    c3dp_grid_gl() { return gridGlFixture(); },
    c3dp_dtile() { return dtileFixture(); },
    c3dp_dobj() { return dobjFixture(); },
    c3dp_scene_z0() { return sceneFixture(0); },
    c3dp_scene_z1() { return sceneFixture(1); },
    c3dp_scene_z2() { return sceneFixture(2); },
  });

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dp_pool_v0() { return poolFixture(0, 0x20000); },
    c3dp_pool_v1() { return poolFixture(1, 0x20000); },
    c3dp_pool_v2() { return poolFixture(2, 0x20000); },
    c3dp_pool_v3() { return poolFixture(3, 0x20000); },
  });

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ---------------------------------------------------------------------------------------------------
    // c3dp_lists: std::list<Tile*> under the heap stand-in over Terrain.dll's operator new 0x10018c90 and operator
    // delete 0x10018be0 (the allocation entry points _Buynode / _Freenode reach through _Charalloc -> _Allocate and
    // allocator::deallocate). Block layout ($blk, 0x3000 bytes, the first state region):
    //   +0x0000  8 list objects, stride 0x20 ($L0..$L7): +0 allocator byte, +4 head node, +8 count
    //   +0x0100  8 sentinel (head) nodes, stride 0x10 ($S0..$S7)
    //   +0x0200  node pools, stride 0x100 per list, 0x10 per node ($N<i>_<j>)
    //   +0x0a00  16 value slots ($v0..$v15), each a distinct fake Tile pointer (what push_back / insert copy)
    //   +0x0b00  32 return slots ($r0..$r31), seeded 0xdead0000 + k so a missing store would show
    //   +0x0c00  16 one-dword iterator `this` objects for the constructor keys ($c0..$c15), pattern filled
    // List i holds COUNTS[i] nodes. Node j's stored value is 0x7e000 + i * 0x100 + j. The arena (operator new) and
    // the free log (operator delete) are the other two state regions.
    c3dp_lists() {
      const h = __heapStandIn({ module: 'Terrain.dll', alloc: [0x18c90], free: [0x18be0],
                                arenaSize: 0x1000, fill: 0xcd });
      const blk = __keepAlloc(0x3000);
      zero(blk, 0x3000);
      const out = { blk, obj: blk, arena: h.arena, freelog: h.log };
      const COUNTS = [0, 1, 2, 3, 4, 5, 6, 9];
      for (let i = 0; i < 8; i++) {
        const L = blk.add(i * 0x20), S = blk.add(0x100 + i * 0x10);
        L.writeU8(0x41 + i);
        L.add(4).writePointer(S);
        L.add(8).writeS32(COUNTS[i]);
        const nodes = [];
        for (let j = 0; j < COUNTS[i]; j++) nodes.push(blk.add(0x200 + i * 0x100 + j * 0x10));
        const ring = [S].concat(nodes);
        for (let k = 0; k < ring.length; k++) {
          ring[k].writePointer(ring[(k + 1) % ring.length]);                      // next
          ring[k].add(4).writePointer(ring[(k + ring.length - 1) % ring.length]); // prev
          ring[k].add(8).writeS32(k === 0 ? 0 : 0x7e000 + i * 0x100 + (k - 1));
        }
        out['L' + i] = L; out['S' + i] = S;
        nodes.forEach(function (n, j) { out['N' + i + '_' + j] = n; });
      }
      for (let k = 0; k < 16; k++) {
        const v = blk.add(0xa00 + k * 4);
        v.writeS32(0x5a000 + k * 0x31);
        out['v' + k] = v;
      }
      for (let k = 0; k < 32; k++) {
        const r = blk.add(0xb00 + k * 4);
        r.writeU32(0xdead0000 + k);
        out['r' + k] = r;
      }
      for (let k = 0; k < 16; k++) {
        const c = blk.add(0xc00 + k * 0x10);
        fillPattern(c, 0x10, k);
        out['c' + k] = c;
      }
      return out;
    },

    // ---------------------------------------------------------------------------------------------------
    // c3dp_ios: 16 fake ios objects ($io0..$io15, stride 0x80) and the import stand-in over Terrain.dll's
    // EnterCriticalSection / LeaveCriticalSection slots (0x10113510 / 0x10113514), whose log is $implog. Fields read:
    // the state word at +8 (ios::fail), the lock count at +0x34 (ios::lock / unlock) and the section address +0x38.
    c3dp_ios() {
      const s = __importStandIn({ module: 'Terrain.dll', imports: {
        'KERNEL32.dll!EnterCriticalSection': { ret: 0 },
        'KERNEL32.dll!LeaveCriticalSection': { ret: 0 },
      } });
      const blk = __keepAlloc(0x800);
      fillPattern(blk, 0x800, 3);
      const out = { blk, obj: blk, implog: s.log };
      const FLAGS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 0x16, 0xfffffff9, 0x80000006, 0x40, 0x0f, 0xffffffff, 0x10];
      const LOCK = [-1, 0, 1, -2, 0x7fffffff, -0x80000000, 5, -5, 2, 0, -100, 100, -1, -0x7fffffff, 0x10, -3];
      for (let i = 0; i < 16; i++) {
        const o = blk.add(i * 0x80);
        o.add(8).writeU32(FLAGS[i] >>> 0);
        o.add(0x34).writeS32(LOCK[i]);
        out['io' + i] = o;
      }
      return out;
    },

    // ---------------------------------------------------------------------------------------------------
    // c3dp_tiles: 12 standalone tiles ($t0..$t11, stride 0x260, 0x248 bytes of Tile plus a canary tail) filled with
    // a pattern, for Tile::Tile 0x1000c210 (which rewrites the three member regions and leaves the rest).
    c3dp_tiles() {
      const blk = __keepAlloc(12 * 0x260);
      fillPattern(blk, 12 * 0x260, 7);
      const out = { blk, obj: blk };
      for (let i = 0; i < 12; i++) out['t' + i] = blk.add(i * 0x260);
      return out;
    },

    // ---------------------------------------------------------------------------------------------------
    // The GL group. Every key stands in for the OPENGL32.dll slots its function calls (importstandin.js, module
    // Terrain.dll), so the A/B thread never reaches a real GL context: the stub's result is 0 and every call is
    // appended to $implog (import index, value count, values). Pointer arguments are recorded by CONTENT through an
    // impl that reads the floats / texture ids the pointer names (a stack address would differ between the arms).
    // $obj blocks are small stand-ins for a Terrain object: only the bytes the function reads are set.
    // c3dp_gl_init: 8 objects ($g0..$g7) whose BYTE at +0x28 is 0, 1, 2, 0xff, then dwords whose low byte is zero
    // (0x100, 0x01000000) and non-zero (0x80, 0x00010001), for initGL's front-face branch (0x10003473).
    c3dp_gl_init() {
      const s = __importStandIn({ module: 'Terrain.dll', imports: __c3dpGL(
        ['glHint', 'glEnable', 'glFrontFace', 'glEnableClientState']) });
      const blk = __keepAlloc(0x400);
      fillPattern(blk, 0x400, 5);
      const out = { blk, obj: blk, implog: s.log };
      const V = [0x00, 0x01, 0x02, 0xff, 0x100, 0x01000000, 0x80, 0x00010001];
      for (let i = 0; i < 8; i++) {
        const o = blk.add(i * 0x40);
        o.add(0x28).writeU32(V[i]);
        out['g' + i] = o;
      }
      return out;
    },

    // c3dp_gl_light: 4 Terrain stand-ins ($g0..$g3); light1Off never reads `this`. glLightfv records the light, the
    // parameter name and the four floats it points at.
    c3dp_gl_light() {
      const s = __importStandIn({ module: 'Terrain.dll', imports: __c3dpGL(
        ['glDisable', 'glLightfv', 'glEnable'],
        { glLightfv: { impl: function (a, ctx) {
          const f = []; for (let k = 0; k < 4; k++) f.push(a[2].add(4 * k).readU32());
          ctx.record([a[0], a[1]].concat(f)); return 0; } } }) });
      const blk = __keepAlloc(0x100);
      fillPattern(blk, 0x100, 6);
      const out = { blk, obj: blk, implog: s.log };
      for (let i = 0; i < 4; i++) out['g' + i] = blk.add(i * 0x40);
      return out;
    },

    // c3dp_gl_size: 4 objects ($f0..$f3, flip byte at +0x28: 0, 1, 0xff, and a dword 0x100 whose byte is 0) for
    // Terrain::resize, with the viewport / matrix / ortho calls recorded (glOrtho's six doubles are 12 dwords).
    c3dp_gl_size() {
      const s = __importStandIn({ module: 'Terrain.dll', imports: __c3dpGL(
        ['glViewport', 'glMatrixMode', 'glLoadIdentity', 'glOrtho']) });
      const blk = __keepAlloc(0x400);
      fillPattern(blk, 0x400, 8);
      const out = { blk, obj: blk, implog: s.log };
      const V = [0, 1, 0xff, 0x100];
      for (let i = 0; i < 4; i++) {
        const o = blk.add(i * 0x80);
        o.add(0x28).writeU32(V[i]);
        out['f' + i] = o;
      }
      return out;
    },

    // c3dp_gl_tex: reloadTextures. 8 Terrain stand-ins ($T0..$T7, 0x3a4 bytes of type table used at +0x40 stride 0x18),
    // the loaded-flag byte at +0x29 being 0 for T0, T2, T5 and 1, 0xff, 0x80, 2, 0x101 for the others. g_textures
    // (0x100687f8, 0x25 * 0x384 bytes) is seeded with distinct non-zero ids and put back by the freeze helper's flush
    // after the key. glDeleteTextures records the count and the nine ids the pointer names; the log holds 925 calls of
    // 48 bytes.
    c3dp_gl_tex() {
      const s = __importStandIn({ module: 'Terrain.dll', logSize: 0x10000, imports: __c3dpGL(
        ['glDeleteTextures'],
        { glDeleteTextures: { impl: function (a, ctx) {
          const v = [a[0]]; for (let k = 0; k < 9; k++) v.push(a[1].add(4 * k).readU32());
          ctx.record(v); return 0; } } }) });
      const tbl = Process.getModuleByName('Terrain.dll').base.add(0x687f8);
      __freeze.save(tbl, 0x25 * 0x384);
      for (let k = 0; k < 0x25 * 0x19 * 9; k++) tbl.add(4 * k).writeU32(0x1000 + k * 3);
      const blk = __keepAlloc(8 * 0x400);
      fillPattern(blk, 8 * 0x400, 12);
      const out = { blk, obj: blk, implog: s.log, tbl };
      const FLAG = [0, 1, 0, 0xff, 0x80, 0, 2, 0x101];
      for (let i = 0; i < 8; i++) {
        const o = blk.add(i * 0x400);
        o.add(0x29).writeU8(FLAG[i] & 0xff);
        o.add(0x28).writeU8(FLAG[i] === 0x101 ? 1 : 0);
        for (let t = 0; t < 0x25; t++) o.add(0x40 + t * 0x18).writeU32(0x700 + t);
        out['T' + i] = o;
      }
      return out;
    },

    // c3dp_grid: buildArrays. 11 Terrain stand-ins ($G0..$G10, stride 0x3000) of sizes 1x1, 2x1, 1x2, 3x3, 4x3, 3x4,
    // 0x3, 3x0, 5x2, 2x5 and 0x0 (width at +0x14, height at +0x18, tile array at +0x3a4, stride 0x248), pattern filled.
    // g_tileW / g_tileH (0x10063e4c / 0x10063e50) are set to 6.5 and 5.25 so a swapped width / height reaches the
    // vertex array; g_vertices (0x100b28c8) and g_normals (0x10070a18) are the state windows $vtx / $nrm (0x2000 bytes),
    // put back by the freeze helper's flush together with the two tile sizes.
    c3dp_grid() {
      const base = Process.getModuleByName('Terrain.dll').base;
      const vtx = base.add(0xb28c8), nrm = base.add(0x70a18), cfg = base.add(0x63e4c);
      __freeze.save(vtx, 0x2000); __freeze.save(nrm, 0x2000); __freeze.save(cfg, 8);
      cfg.writeFloat(6.5); cfg.add(4).writeFloat(5.25);
      for (let i = 0; i < 0x2000 / 4; i++) { vtx.add(4 * i).writeFloat(-7.5 - i); nrm.add(4 * i).writeFloat(3.25 + i); }
      const SZ = 0x3000;
      const blk = __keepAlloc(11 * SZ);
      fillPattern(blk, 11 * SZ, 14);
      const out = { blk, obj: blk, vtx, nrm, cfg };
      const WH = [[1, 1], [2, 1], [1, 2], [3, 3], [4, 3], [3, 4], [0, 3], [3, 0], [5, 2], [2, 5], [0, 0]];
      for (let i = 0; i < 11; i++) {
        const o = blk.add(i * SZ);
        o.add(0x14).writeS32(WH[i][0]);
        o.add(0x18).writeS32(WH[i][1]);
        out['G' + i] = o;
      }
      return out;
    },

    // c3dp_angle: the three float values JSON cannot carry (NaN and both infinities) for Terrain::setViewAngle.
    c3dp_angle() { return { nan: NaN, pinf: Infinity, ninf: -Infinity }; },

    // c3dp_tile_dtor: 12 subject tiles ($t0..$t11) wired to neighbour tiles by a mask (bit k = neighbour k present)
    // and 48 neighbour tiles ($nb<i>_<k>), each pointing back at its subject through the slot setNeighbour clears
    // (side 1 for n[0], 0 for n[1], 3 for n[2], 2 for n[3], the sides ~Tile 0x1000c3d0 passes).
    c3dp_tile_dtor() {
      const N = 12 + 48;
      const blk = __keepAlloc(N * 0x260);
      fillPattern(blk, N * 0x260, 9);
      const out = { blk, obj: blk };
      const MASK = [15, 1, 2, 4, 8, 3, 12, 5, 10, 0, 9, 6];
      const BACK = [1, 0, 3, 2];
      for (let i = 0; i < 12; i++) {
        const t = blk.add(i * 0x260);
        out['t' + i] = t;
        for (let k = 0; k < 4; k++) {
          const slot = t.add(0x34 + 4 * k);
          if (MASK[i] & (1 << k)) {
            const nb = blk.add((12 + i * 4 + k) * 0x260);
            slot.writePointer(nb);
            nb.add(0x34 + 4 * BACK[k]).writePointer(t);
            out['nb' + i + '_' + k] = nb;
          } else {
            slot.writePointer(ptr(0));
          }
        }
      }
      return out;
    },
  });
})();
