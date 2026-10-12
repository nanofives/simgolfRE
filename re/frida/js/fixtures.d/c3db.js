// Fixtures for C3 batch c3db (round 24, 2026-10-10): golf_clean.exe drawing helpers that reach jgld Surfaces, the
// palette holder [0x0083ad0c], the display object [0x0083ad50], the polygon globals 0x0083d34c..0x0083d393 and the
// window array 0x0083a2d8 / count 0x0083ab94. Reimplementations: shim/src/re/c3db.cpp. Registry:
// re/frida/registry.d/c3db.py. Purposes and branch tables: log/c3/c3db_purpose.md.
//
// Everything a vector touches is OURS, in one arena ($arena, the state region of every key):
//   - eleven Surfaces (0x500 bytes each) whose +0 is jgld's REAL Surface vtable (RVA 0x11d0b0), so every slot these
//     bodies call is jgld's own code: +0x24 depth, +0x28 16-bit format code (slot +0xe4 returns this+0x24), +0x40
//     pitch in pixels (slot +0xe0), +0x44 clip RECT (slot +0xcc), +0x54 bounds RECT (width/height slots +0xd8/+0xdc),
//     +0x7c Palette (read by jgld's 16-bit primitives instead of the client global 0x1012873c), +0x4c0 pixel base,
//     +0x4c8 use count, +0x4cc current base (acquireDC2 0x10008730 / releaseDC2 0x100087b0, c3ce);
//   - each Surface's pixel buffer (0x800 bytes, pattern (i * 37 + 11) & 0xff);
//   - one Palette (0x820 bytes, real Palette vtable RVA 0x11db10: slot +0x18 returns this+0x40c, slot +0x1c this+0x60c,
//     two synthetic 256-word tables) and its holder {+0 0, +4 Palette};
//   - widgets w<k> = {+0 0x57570000 + k, +4 Surface k}, wnull = {.., +4 0}; windows win<k> with the Surface at +0x278
//     (fillWindowRect passes this + 0x274 on);
//   - a recording display (0x80-slot vtable of machine-code stubs; slot +0xb4 returns the fixture's format value,
//     slot +0xa8 its width value, every other slot 0) and eight window-array objects whose slot +8 records;
//   - the call recorder ($rec, cursor at +0, entries from +0x10: tag, ecx);
//   - RECTs and vertex tables (read only).
// Stubs are machine code in a separate rwx page (no JavaScript runs inside a vector).
//
// Every key freezes the game's first thread (re/frida/js/freeze.js) and, while it is down, points [0x0083ad0c] at
// our palette holder (or 0), [0x0083ad50] at the recording display (or 0), writes our window array (0x0083a2d8,
// 16 entries) and count (0x0083ab94), and saves the polygon globals 0x0083d34c..0x0083d393 and jgld's fillRectC8 row
// global 0x1012847c (c3av). All of them are written back, newest first, before the thread resumes.
(function () {
  const ARENA = 0xc000;
  const NSURF = 11, SSIZE = 0x500, PIXSZ = 0x800;
  const OFF_SURF = 0x0000;                  // 11 * 0x500 = 0x3700
  const OFF_PIX = 0x3800;                   // 11 * 0x800 = 0x5800 -> 0x9000
  const OFF_PAL = 0x9000;                   // 0x820
  const OFF_MISC = 0x9900;                  // widgets, windows, holder, display, objects, rects, verts
  const OFF_REC = 0xbc00;                   // recorder, 0x400

  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }

  function codeArena() {
    const size = Process.pageSize * 2;
    const base = Memory.alloc(size);
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3db: code arena full');
      return p;
    };
  }
  // thiscall stub with no stack arguments: records (tag, ecx) at the recorder cursor, returns `ret`.
  //   mov eax, [REC]; add eax, REC; mov dword [eax], tag; mov [eax+4], ecx; add dword [REC], 8; mov eax, ret; ret
  function stub0(rec, tag, ret) {
    const r = u32(rec.toUInt32());
    return [0xA1, ...r, 0x05, ...r, 0xC7, 0x00, ...u32(tag), 0x89, 0x48, 0x04,
            0x81, 0x05, ...r, ...u32(8), 0xB8, ...u32(ret), 0xC3];
  }

  // depth, format, clip RECT, has base; bounds are {0, 0, 32, 16} and the pitch 32 for all of them.
  const SPECS = [
    [8, 0, [0, 0, 32, 16], 1],     // s0  8-bit, clip = bounds
    [8, 0, [4, 2, 24, 12], 1],     // s1  8-bit, clip inside the bounds
    [16, 0, [0, 0, 32, 16], 1],    // s2  16-bit RGB565 code 0
    [16, 1, [4, 2, 24, 12], 1],    // s3  16-bit code 1, clip inside
    [16, 2, [0, 0, 32, 16], 1],    // s4  16-bit code 2 (no palette table)
    [24, 0, [0, 0, 32, 16], 1],    // s5  24-bit
    [32, 0, [0, 0, 32, 16], 1],    // s6  32-bit
    [12, 0, [0, 0, 32, 16], 1],    // s7  depth 12 (no case anywhere)
    [8, 0, [0, 0, 32, 16], 0],     // s8  8-bit, pixel base 0
    [16, 0, [0, 0, 32, 16], 0],    // s9  16-bit, pixel base 0
    [8, 0, [0, 0, 40, 20], 1],     // s10 8-bit, clip past the bounds (pixelAddr refuses x >= 32 / y >= 16)
  ];
  const RECTS = { r0: [2, 2, 13, 10], r1: [0, 0, 32, 16], r2: [-5, -5, 11, 11], r3: [6, 4, 7, 5], r4: [20, 9, 30, 15] };
  // vertex tables {x, y}
  const VERTS = {
    v_tri: [[2, 1], [28, 6], [10, 14]],
    v_quad: [[0, 0], [31, 0], [31, 15], [0, 15]],
    v_out: [[-10, -5], [40, 3], [5, 30]],
    v_flat: [[1, 5], [10, 5], [20, 5]],
    v_pent: [[16, 0], [30, 6], [24, 15], [8, 15], [2, 6]],
    v_diam: [[16, -4], [36, 8], [16, 20], [-4, 8]],
    v_thin: [[3, 2], [5, 2], [5, 13], [3, 13]],
    // below the clip: the first row is already >= clip bottom - 1 (0x0049324d jge)
    v_below: [[2, 20], [10, 30], [5, 25]],
    // not convex: e2 (walking +1) reaches v1 after 8 rows and its next edge v1 -> v2 runs upward, which polyEdgeStep
    // refuses (dy < 0 returns 0, c3x), while e1 (walking -1) still has 4 rows to v3 = 0x0083d350; so
    // polyEdgeAdvance(e2) returns 0 after polyEdgeAdvance(e1) returned 1 (0x00493291 / 0x004934d2)
    v_hook: [[10, 0], [20, 8], [18, 4], [12, 12]],
    // every y <= -0x7ffd: the scan never stores 0x0083d350 (0x004931e4 jle), so the fixture's value (1) stays.
    // v_low starts at v1 = 0x0083d350: polyEdgeStep(e1) returns 0 at once; v_low2 starts at v0 and e2 meets v1 with
    // dy 0: polyEdgeStep(e2) returns 0.
    v_low: [[5, -40000], [10, -50000], [20, -40000]],
    v_low2: [[0, -50000], [10, -50000], [5, -40000]],
  };

  // opts: pal (holder in 0x0083ad0c), fmt (display slot +0xb4), width (slot +0xa8), disp (display present),
  // arr (window array contents: object indices, or -1 for a value no object has), count (0x0083ab94)
  function build(opts) {
    const jgld = Process.getModuleByName('jgld.dll').base;
    const vtSurface = jgld.add(0x11d0b0);
    const vtPalette = jgld.add(0x11db10);
    const emit = codeArena();
    const arena = Memory.alloc(ARENA);
    for (let i = 0; i < ARENA; i += 4) arena.add(i).writeU32(0);
    const fx = { arena: arena, obj: arena };
    const rec = arena.add(OFF_REC);
    rec.writeU32(0x10);
    fx.rec = rec;

    // Palette and its holder
    const pal = arena.add(OFF_PAL);
    pal.writePointer(vtPalette);
    for (let i = 0; i < 256; i++) {
      pal.add(0x40c + i * 2).writeU16((i * 0x0105 + 0x1234) & 0xffff);
      pal.add(0x60c + i * 2).writeU16((i * 0x0203 + 0x5678) & 0xffff);
    }
    let m = OFF_MISC;
    const take = (n) => { const p = arena.add(m); m += (n + 15) & ~15; if (m > OFF_REC) throw new Error('c3db: misc full'); return p; };
    const holder = take(0x10);
    holder.writeU32(0x484f4c44);
    holder.add(4).writePointer(pal);

    // Surfaces and pixels
    const PIXLEN = NSURF * PIXSZ;
    for (let i = 0; i < PIXLEN; i++) arena.add(OFF_PIX + i).writeU8((i * 37 + 11) & 0xff);
    SPECS.forEach(function (sp, k) {
      const [depth, fmt, clip, hasBase] = sp;
      const s = arena.add(OFF_SURF + k * SSIZE);
      s.writePointer(vtSurface);
      s.add(0x24).writeS32(depth);
      s.add(0x28).writeS32(fmt);
      s.add(0x40).writeS32(32);
      for (let j = 0; j < 4; j++) s.add(0x44 + j * 4).writeS32(clip[j]);
      [0, 0, 32, 16].forEach(function (v, j) { s.add(0x54 + j * 4).writeS32(v); });
      s.add(0x7c).writePointer(pal);
      s.add(0x4c0).writePointer(hasBase ? arena.add(OFF_PIX + k * PIXSZ) : ptr(0));
      fx['s' + k] = s;
      const w = take(0x10);
      w.writeU32(0x57570000 + k);
      w.add(4).writePointer(s);
      fx['w' + k] = w;
    });
    const wnull = take(0x10);
    wnull.writeU32(0x5757ffff);
    fx.wnull = wnull;
    // windows: the Surface pointer at +0x278 (= (this + 0x274) + 4)
    [[0, 0], [1, 2], [2, -1], [3, 5]].forEach(function (r) {
      const win = take(0x280);
      win.writeU32(0x57490000 + r[0]);
      win.add(0x278).writePointer(r[1] < 0 ? ptr(0) : fx['s' + r[1]]);
      fx['win' + r[0]] = win;
    });

    Object.keys(RECTS).forEach(function (k) {
      const p = take(0x10);
      RECTS[k].forEach(function (v, j) { p.add(j * 4).writeS32(v); });
      fx[k] = p;
    });
    Object.keys(VERTS).forEach(function (k) {
      const vs = VERTS[k];
      const p = take(vs.length * 8);
      vs.forEach(function (v, j) { p.add(j * 8).writeS32(v[0]); p.add(j * 8 + 4).writeS32(v[1]); });
      fx[k] = p;
    });

    // Recording display
    const dvt = take(0x200);
    for (let i = 0; i < 0x80; i++) dvt.add(i * 4).writePointer(emit(stub0(rec, 0xD0000000 + i * 4, 0)));
    dvt.add(0xb4).writePointer(emit(stub0(rec, 0xD00000B4, opts.fmt)));
    dvt.add(0xa8).writePointer(emit(stub0(rec, 0xD00000A8, opts.width)));
    const disp = take(0x40);
    disp.writePointer(dvt);
    fx.disp = disp;

    // Window-array objects: slot +8 records (tag 0xE0080000), ecx says which object was released.
    const ovt = take(0x40);
    const nul = emit([0x33, 0xC0, 0xC3]);
    for (let i = 0; i < 0x10; i++) ovt.add(i * 4).writePointer(nul);
    ovt.add(8).writePointer(emit(stub0(rec, 0xE0080000, 0)));
    for (let k = 0; k < 8; k++) {
      const o = take(0x10);
      o.writePointer(ovt);
      o.add(4).writeU32(0x4f420000 + k);
      fx['o' + k] = o;
    }
    const stranger = take(0x10);
    stranger.writePointer(ovt);
    fx.ostranger = stranger;

    // TextView objects for setLineSpacing (0x2000 bytes each, outside the arena): +0x590, +0x1f68, +0x1464 -> an
    // object whose +8 is the byte offset added to this + 0x1498. The state region is tv<k> + 0x1498, 0x40 bytes.
    //               +0x590  +0x1f68  offset
    [[0, 0, 0], [1, 0, 4], [0, 0x400, 8], [0, 0xfbff, 0x10], [-1, 0x400, 0x3c], [0, 0xffff0000, 0x20]]
      .forEach(function (r, k) {
        const tv = Memory.alloc(0x2000);
        for (let i = 0; i < 0x2000; i += 4) tv.add(i).writeU32(0x7a7a7a7a);
        tv.add(0x590).writeS32(r[0]);
        tv.add(0x1f68).writeU32(r[1] >>> 0);
        const ix = Memory.alloc(0x10);
        ix.writeU32(0); ix.add(4).writeU32(0); ix.add(8).writeS32(r[2]); ix.add(12).writeU32(0);
        tv.add(0x1464).writePointer(ix);
        fx['tv' + k] = tv;
      });

    // Freeze the first thread, then point the live globals at our objects.
    __freeze.thread();
    __freeze.save(jgld.add(0x12847c), 4);
    __freeze.save(ptr('0x0083d34c'), 0x48);
    if (opts.g350 !== undefined) ptr('0x0083d350').writeS32(opts.g350);
    __freeze.setU32(ptr('0x0083ad0c'), opts.pal ? holder.toUInt32() : 0);
    __freeze.setU32(ptr('0x0083ad50'), opts.disp ? __recDisplay.guard(disp).toUInt32() : 0);
    const arr = ptr('0x0083a2d8');
    __freeze.save(arr, 0x40);
    for (let i = 0; i < 16; i++) {
      const v = i < opts.arr.length ? opts.arr[i] : -2;
      arr.add(i * 4).writePointer(v >= 0 ? fx['o' + v] : (v === -1 ? stranger : ptr(0x5a5a0000 + i)));
    }
    __freeze.setU32(ptr('0x0083ab94'), opts.count >>> 0);
    return fx;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // palette holder present, display format 1 (RGB565), width 0x400, array of five objects
    c3db_a() { return build({ pal: 1, fmt: 1, width: 0x400, disp: 1, arr: [0, 1, 2, 3, 4], count: 5 }); },
    // no palette holder, display format 0, width 0x3ff, nine entries with object 1 twice and a stranger
    c3db_b() { return build({ pal: 0, fmt: 0, width: 0x3ff, disp: 1, arr: [0, 1, 2, 1, 3, -1, 5, 6, 7], count: 9 }); },
    // palette holder present, display format 2, width 0x500, the count 0 over a filled array
    c3db_c() { return build({ pal: 1, fmt: 2, width: 0x500, disp: 1, arr: [0, 1, 2, 3], count: 0 }); },
    // palette holder present, NO display, a negative count; one entry
    c3db_n() { return build({ pal: 1, fmt: 1, width: 0x400, disp: 0, arr: [3], count: -3 }); },
    // as c3db_a with 0x0083d350 (index of the lowest vertex) set to 1 for the polygons whose scan never stores it
    c3db_g350() { return build({ pal: 1, fmt: 1, width: 0x400, disp: 1, arr: [0, 1, 2, 3, 4], count: 5, g350: 1 }); },
    // single-entry array: object 6 alone (count 1)
    c3db_one() { return build({ pal: 1, fmt: 1, width: 0x400, disp: 1, arr: [6], count: 1 }); },
  });
})();
