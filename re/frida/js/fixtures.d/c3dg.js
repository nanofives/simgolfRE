// Fixtures for batch c3dg (round 25, jgld.dll). Reimplementations: shim/src/re/c3dg.cpp. Registry:
// re/frida/registry.d/c3dg.py. Purposes and branch tables: log/c3/c3dg_purpose.md.
//
// Every key installs re/frida/js/importstandin.js's __importStandIn over the imports its function calls (ONE call
// per key), so those imports are recording stubs on the A/B thread for the length of the key: their results are
// ours and the call sequence lands in `$implog`, a state region of every key. Values handed to the stubs are
// synthetic (handles such as 0x3a6101, DCs such as 0xdc10); no stub forwards to the real API.
//
// Where a body or a callee allocates or frees, the heap stand-in (re/frida/js/heapstandin.js) serves jgld's
// operator new 0x1007f0f0, malloc 0x1007fb40 (jalloc's), calloc 0x1007ffa0, operator delete 0x1007e6d0 and free
// 0x100806d0 (c3dc's set), so blocks come from the compared arena and frees are compared log entries.
//
// Module globals the bodies read or write (the palette 0x10128738 and its two clients 0x1012873c / 0x10128740,
// the font DC 0x10128724, the event sink 0x1012872c, the display 0x10128420, the two mode arrays 0x101286c8 /
// 0x101286f8, the tracked-object list 0x10128428 and the sprite counters 0x101284c8 / 0x101284cc) are only written
// with the game's first thread frozen (re/frida/js/freeze.js), which writes the game's values back before resuming.
// Objects that a body dispatches through get vtables of our own whose slots are small native stubs (no JS on the
// call), so nothing here can block on Frida's JS lock while the first thread is down.
//
// jgld.dll is relocated at run time, so every module address is built from the running base.

(function () {
  const LOG = 0x400;     // implog size (c3dg.py: _LOG)
  const jgld = () => Process.getModuleByName('jgld.dll').base;
  const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };
  const words = (p, vals) => vals.forEach(function (v, i) { p.add(4 * i).writeU32(v >>> 0); });
  const cstr = (s) => { const p = __keepAlloc(s.length + 1); p.writeUtf8String(s); return p; };
  const le = (v) => { const a = []; for (let i = 0; i < 4; i++) a.push((v >>> (8 * i)) & 0xff); return a; };
  const addrBytes = (p) => le(p.toUInt32());

  // Executable stub in its own page (kept alive).
  function code(bytes) {
    const page = Process.pageSize;
    const p = __keepAlloc(page);
    Memory.protect(p, page, 'rwx');
    p.writeByteArray(bytes);
    return p;
  }
  // A vtable of n slots all pointing at `def`, with `slots` {offset: stub} overriding.
  function vtable(n, def, slots) {
    const vt = __keepAlloc(4 * n);
    for (let i = 0; i < n; i++) vt.add(4 * i).writePointer(def);
    Object.keys(slots || {}).forEach(function (o) { vt.add(Number(o)).writePointer(slots[o]); });
    return vt;
  }
  // `int3` would kill the game: an unexpected slot returns 0x0badc0de and counts the call at its own word.
  function trapStub() {
    const ctr = __keepAlloc(4); ctr.writeU32(0);
    return code([0xFF, 0x05].concat(addrBytes(ctr), [0xB8], le(0x0badc0de), [0xC3]));
  }

  function standIn(size, extra) {
    const o = { module: 'jgld.dll', alloc: [0x7f0f0, 0x7fb40, { rva: 0x7ffa0, args: 2, sizeArgs: 2, fill: 0 }],
                free: [0x7e6d0, 0x806d0], arenaSize: size || 0x400, fill: 0xcd };
    Object.assign(o, extra || {});
    return __heapStandIn(o);
  }

  // c3dc's settle guard (from c3cw): wait until the tracked list's count (+0xc) has not moved for 0.5 s before
  // freezing; a freezing key inside the start-up burst of Tracked::ctor hung the game once.
  function settle() {
    const glist = jgld().add(0x128428);
    let last = glist.add(0xc).readS32(), still = 0;
    for (let t = 0; still < 5; t++) {
      if (t >= 600) throw new Error('c3dg: tracked list still changing after 60 s; not freezing');
      Thread.sleep(0.1);
      const c = glist.add(0xc).readS32();
      still = c === last ? still + 1 : 0;
      last = c;
    }
  }

  // ============================================================================ group M: MappedFile writers
  // Object layout (MappedFile::close 0x10002e10): +0 vtable, +4 view, +8 file (-1 none), +0xc mapping, +0x10 size.
  // Rows [view, file, mapping] for openWrite / create: every combination of what the leading close releases.
  const MF_ROWS = [
    [0, -1, 0], [0x7a000001, -1, 0], [0, 0x3a02, 0], [0, -1, 0x5a03],
    [0x7a000004, 0x3a04, 0], [0x7a000005, -1, 0x5a05], [0, 0x3a06, 0x5a06], [0x7a000007, 0x3a07, 0x5a07],
    [0, 0, 0], [0x7a000009, 0, 0x5a09], [0, -1, 0], [0x7a00000b, 0x3a0b, 0x5a0b],
  ];
  // File names drive the stubs: name[0] is the mode, name[1] the view index ('a' + k).
  //   '1' CreateFileA fails (-1)    '2' CreateFileMappingA fails    '3' MapViewOfFile fails
  //   '5' the first SetFilePointer fails (every call)               '6' only a SetFilePointer to 0 fails
  //   anything else succeeds. The file handle is 0x3a0000 | mode << 8 | k, the mapping handle + 0x1000000, the view
  //   is $views + 0x100 * k (a real block: create's memset writes it).
  function mfImports(views) {
    const mode = (h) => (h >>> 8) & 0xff;
    let lastK = 0;                        // view index of the last CreateFileA name (set before GetLastError runs)
    return {
      'KERNEL32.dll!CreateFileA': function (a) {
        const n = a[0].readU8(), k = a[0].add(1).readU8() - 0x61;
        lastK = k;
        return n === 0x31 ? -1 : ((0x3a0000 | (n << 8) | (k & 0xff)) >>> 0);
      },
      // ERROR_FILE_NOT_FOUND 2 after an even view index, 5 after an odd one: both sides of `cmp [ebp-0x10], 2`
      'KERNEL32.dll!GetLastError': { impl: function () { return (lastK & 1) ? 5 : 2; } },
      'KERNEL32.dll!CreateFileMappingA': function (a) {
        const h = a[0].toUInt32();
        return mode(h) === 0x32 ? 0 : (h + 0x1000000) >>> 0;
      },
      'KERNEL32.dll!MapViewOfFile': function (a) {
        const m = a[0].toUInt32();
        return mode(m) === 0x33 ? ptr(0) : views.add(0x100 * (m & 0xff));
      },
      'KERNEL32.dll!GetFileSize': function (a) { return ((a[0].toUInt32() & 0xff) * 0x1000 + 0x123) >>> 0; },
      'KERNEL32.dll!SetFilePointer': function (a) {
        const h = a[0].toUInt32(), d = a[1].toUInt32();
        if (mode(h) === 0x35 || h === 0x5f5f) return -1;
        if (mode(h) === 0x36 && d === 0) return -1;
        return d;
      },
      'KERNEL32.dll!SetEndOfFile': { ret: [1, 0] },
      'KERNEL32.dll!UnmapViewOfFile': { ret: 1 },
      'KERNEL32.dll!CloseHandle': { ret: 1 },
    };
  }
  function mfBase(out) {
    const blk = __keepAlloc(0x20 * 12);
    MF_ROWS.forEach(function (r, i) {
      const o = blk.add(0x20 * i);
      words(o, [0x1d01c000 + i, r[0], r[1], r[2], 0x11110000 + i, 0x22220000 + i, 0x33330000 + i, 0x44440000 + i]);
      out['mf' + i] = o;
    });
    out.mfblk = blk;
    const views = __keepAlloc(0x100 * 12);
    fill(views, 0x100 * 12, 0x7e77);
    out.views = views;
    return out;
  }

  // closeTruncate objects: [view index or -1, file, mapping, end] where end is 'v+N' (view + N), 'v-N', or a number
  const TR_ROWS = [
    [0, 0x3a01, 0x5a01, 'v+64'],     // full path, the file is closed and +8 becomes 0
    [1, 0x3a02, 0, 'v+0'],           // length 0, no mapping
    [2, -1, 0x5a03, 'v+16'],         // file -1 passes the `!= 0` test: CloseHandle(-1), +8 = 0
    [3, 0, 0x5a04, 'v+32'],          // file 0: SetFilePointer(0, ...), not closed
    [4, 0x5f5f, 0x5a05, 'v+8'],      // SetFilePointer fails: close() after the view and mapping are gone
    [5, 0x3a06, 0x5a06, 'v-1'],      // end below the view: close() only
    [-1, 0x3a07, 0x5a07, 0x1000],    // no view: length is the end value itself
    [-1, 0x3a08, 0, 0],              // no view, end 0
    [8, 0x3a09, 0x5a09, 'v+256'],    // the whole view
    [9, -1, 0, 'v-128'],             // below the view with no file: close() closes nothing
    [10, 0x5f5f, 0, 'v+0'],          // SetFilePointer fails at length 0
    [11, 0, 0, 'v+4'],               // file 0 and no mapping
  ];
  function truncBase(out) {
    const blk = __keepAlloc(0x20 * 12);
    const views = __keepAlloc(0x100 * 12);
    fill(views, 0x100 * 12, 0x7e78);
    TR_ROWS.forEach(function (r, i) {
      const o = blk.add(0x20 * i);
      const v = r[0] < 0 ? ptr(0) : views.add(0x100 * r[0]);
      words(o, [0x1d01c100 + i, v.toUInt32(), r[1], r[2], 0x100, 0x55550000 + i, 0x66660000 + i, 0x77770000 + i]);
      let e;
      if (typeof r[3] === 'number') e = ptr(r[3]);
      else {
        const n = parseInt(r[3].slice(2), 10);
        e = r[3][1] === '+' ? v.add(n) : v.sub(n);
      }
      out['t' + i] = o;
      out['e' + i] = e;
    });
    out.mfblk = blk;
    out.views = views;
    return out;
  }

  // ============================================================================ group P / F: palette and font DC
  // Palette clients for createPalette: objects whose vtable slot 0 is the stub
  //   mov eax, [ctr]; inc eax; mov [ctr], eax; mov [ecx+8], eax; ret       (thiscall, no argument)
  // so which client was called, and in which order, is compared state ($clients).
  function clients(out) {
    const blk = __keepAlloc(0x200);
    fill(blk, 0x200, 0x0c1e);
    const ctr = blk.add(0x1f0);
    ctr.writeU32(0);
    const s = code([0xA1].concat(addrBytes(ctr), [0x40, 0xA3], addrBytes(ctr), [0x89, 0x41, 0x08, 0xC3]));
    const vt = vtable(4, trapStub(), { 0: s });
    for (let i = 0; i < 8; i++) {
      const o = blk.add(0x20 * i);
      o.writePointer(vt);
      o.add(4).writeU32((0x9a100000 + i) >>> 0);       // the +4 field Display::present hands to slot +0xec
      out['c' + i] = o;
    }
    out.clients = blk;
    return out;
  }
  // The three palette globals (handle, client a, client b) with the first thread frozen.
  function palGlobals(out, pal, a, b) {
    const g = jgld().add(0x128738);
    out.gpal = g;
    __freeze.save(g, 12);
    g.writeU32(pal >>> 0);
    g.add(4).writePointer(a || ptr(0));
    g.add(8).writePointer(b || ptr(0));
  }
  function dcImports() {
    return {
      // DC 0 for a window handle whose low byte is 0 (the body goes on with it)
      'USER32.dll!GetDC': { args: 1, impl: function (a) {
        const h = a[0].toUInt32();
        return (h & 0xff) === 0 ? 0 : ((0xdc000000 | (h & 0xffff)) >>> 0);
      } },
      'GDI32.dll!SelectPalette': { args: 3, impl: function (a) { return (a[1].toUInt32() ^ 0x00ff00ff) >>> 0; } },
      'GDI32.dll!RealizePalette': { args: 1, ret: [3, 0, 0xffffffff, 7] },
      'USER32.dll!ReleaseDC': { args: 2, ret: 1 },
    };
  }

  // ============================================================================ group B: GDI blits
  // Surfaces of our own (stride 0x40) on one vtable of native stubs:
  //   +0x28 acquire   mov eax, [ecx+4]; inc dword [ecx+8]; ret                  (+4 the DC it returns, 0 = fail)
  //   +0x2c release   mov eax, [esp+4]; mov [ecx+0xc], eax; inc dword [ecx+0x10]; ret 4
  //   +0xd4 bounds    lea eax, [ecx+0x20]; inc dword [ecx+0x14]; ret            (the RECT at +0x20)
  //   +0xec palette   mov eax, [esp+4]; mov [ecx+0x18], eax; inc dword [ecx+0x1c]; ret 4
  // Every other slot is a trap stub that counts and returns 0x0badc0de.
  function surfVtable() {
    const acq = code([0x8B, 0x41, 0x04, 0xFF, 0x41, 0x08, 0xC3]);
    const rel = code([0x8B, 0x44, 0x24, 0x04, 0x89, 0x41, 0x0C, 0xFF, 0x41, 0x10, 0xC2, 0x04, 0x00]);
    const bnd = code([0x8D, 0x41, 0x20, 0xFF, 0x41, 0x14, 0xC3]);
    const pal = code([0x8B, 0x44, 0x24, 0x04, 0x89, 0x41, 0x18, 0xFF, 0x41, 0x1C, 0xC2, 0x04, 0x00]);
    return vtable(0x60, trapStub(), { 0x28: acq, 0x2c: rel, 0xd4: bnd, 0xec: pal });
  }
  // [DC, bounds left, top, right, bottom]
  const SURF = [[0xdc10, 0, 0, 320, 200], [0xdc11, 0, 0, 640, 480], [0, 0, 0, 100, 100], [0xdc13, 10, 20, 110, 220],
                [0xdc14, -50, -50, 50, 50], [0, 5, 5, 6, 6], [0xdc16, 0, 0, 0, 0], [0xdc17, 100, 100, 50, 50]];
  // RECTs (left, top, right, bottom)
  const RECTS = [[0, 0, 320, 200], [10, 20, 30, 60], [-5, -5, 5, 5], [300, 180, 400, 260], [0, 0, 0, 0],
                 [50, 40, 10, 5], [400, 300, 500, 400], [0, 0, 1, 1], [100, 100, 200, 300], [-100, -100, -50, -50],
                 [0x7fff0000, 0, 0x7fffffff, 10], [1, 2, 3, 4]];
  function surfaces(out) {
    const vt = surfVtable();
    const blk = __keepAlloc(0x40 * SURF.length);
    fill(blk, 0x40 * SURF.length, 0x5afe);
    SURF.forEach(function (s, i) {
      const o = blk.add(0x40 * i);
      o.writePointer(vt);
      words(o.add(4), [s[0], 0, 0, 0, 0, 0, 0]);
      words(o.add(0x20), s.slice(1));
      out['s' + i] = o;
    });
    out.surfs = blk;
    const rects = __keepAlloc(0x10 * RECTS.length);
    RECTS.forEach(function (r, i) { words(rects.add(0x10 * i), r); out['r' + i] = rects.add(0x10 * i); });
    out.rects = rects;
    return out;
  }
  // Displays for present: +0x148 -> holder whose +4 is a surface; +0x138 the window DC.
  function displays(out) {
    const blk = __keepAlloc(0x200 * 4);
    fill(blk, 0x200 * 4, 0xd15b);
    for (let i = 0; i < 4; i++) {
      const d = blk.add(0x200 * i), holder = d.add(0x180);
      d.add(0x138).writeU32((0x5dc00000 + i) >>> 0);
      d.add(0x148).writePointer(holder);
      holder.add(4).writePointer(out['s' + [0, 1, 3, 4][i]]);
      out['d' + i] = d;
    }
    out.disps = blk;
    return out;
  }

  // ============================================================================ group K: onKey
  // Event sink (for 0x1012872c): slot +0xc appends (key, down) at +8 + 8 * count and counts at +4:
  //   mov eax,[ecx+4]; mov edx,[esp+4]; mov [ecx+eax*8+8],edx; mov edx,[esp+8]; mov [ecx+eax*8+0xc],edx;
  //   inc dword [ecx+4]; ret 8
  // Display (for 0x10128420): slot +0x138 returns 1 while its call count +4 is below its limit +8 (counting),
  // then 0 and counts the final call at +0xc:
  //   mov eax,[ecx+4]; cmp eax,[ecx+8]; jae +9; inc dword [ecx+4]; mov eax,1; ret; inc dword [ecx+0xc]; xor eax,eax; ret
  function keyFx(limit) {
    settle();
    const base = jgld();
    const out = {};
    const sinkStub = code([0x8B, 0x41, 0x04, 0x8B, 0x54, 0x24, 0x04, 0x89, 0x54, 0xC1, 0x08, 0x8B, 0x54, 0x24, 0x08,
                           0x89, 0x54, 0xC1, 0x0C, 0xFF, 0x41, 0x04, 0xC2, 0x08, 0x00]);
    const pumpStub = code([0x8B, 0x41, 0x04, 0x3B, 0x41, 0x08, 0x73, 0x09, 0xFF, 0x41, 0x04, 0xB8, 0x01, 0x00, 0x00,
                           0x00, 0xC3, 0xFF, 0x41, 0x0C, 0x31, 0xC0, 0xC3]);
    const trap = trapStub();
    const sink = __keepAlloc(0x100), disp = __keepAlloc(0x40);
    fill(sink, 0x100, 0x51c0);
    fill(disp, 0x40, 0xd1d0);
    sink.writePointer(vtable(0x10, trap, { 0xc: sinkStub }));
    sink.add(4).writeU32(0);
    disp.writePointer(vtable(0x60, trap, { 0x138: pumpStub }));
    words(disp.add(4), [0, limit, 0]);
    out.sink = sink; out.disp = disp; out.obj = sink;
    // PeekMessageA: 1 for the first (hwnd & 0xf) calls of the vector, then 0; the MSG buffer (a stack address)
    // is not recorded
    const s = __importStandIn({ module: 'jgld.dll', imports: {
      'USER32.dll!PeekMessageA': { args: 5, record: [1, 2, 3, 4], impl: function (a, ctx) {
        return ctx.call < (a[1].toUInt32() & 0xf) ? 1 : 0;
      } } } });
    out.implog = s.log;
    __freeze.thread();
    const gs = base.add(0x12872c), gd = base.add(0x128420);
    __freeze.save(gs, 4);
    __freeze.save(gd, 4);
    gs.writePointer(sink);
    gd.writePointer(disp);
    return out;
  }

  // ============================================================================ group E: enumModes
  // The two module arrays (0x14-byte headers {vtable, data, capacity, count, grow}, re/match/jgld_raw_*.cpp,
  // c3bb's ArrayHdr) are overwritten in place (first thread frozen; the game's headers are written back). Their
  // data blocks are ours ($mdata) or NULL; growth goes through the heap stand-in. EnumDisplaySettingsA answers
  // modes 0..n-1 and writes every DEVMODE byte except dmSize / dmDriverExtra / dmFields (+0x24..+0x2b, the ones
  // the caller sets), so the 0x94-byte copies Array_Rec94::add receives are fully defined in both arms.
  function modesFx(n, cap14, count14, grow14, cap94, count94, grow94) {
    settle();
    const base = jgld();
    const h = standIn(0x2000);
    const data = __keepAlloc(0x800);
    fill(data, 0x800, 0x3d0d);
    const out = { arena: h.arena, freelog: h.log, mdata: data, obj: data };
    const s = __importStandIn({ module: 'jgld.dll', imports: {
      'USER32.dll!EnumDisplaySettingsA': { args: 3, impl: function (a, ctx) {
        const i = a[1].toUInt32(), dm = a[2];
        ctx.record([a[0].toUInt32(), i, dm.add(0x24).readU16(), dm.add(0x26).readU16(), dm.add(0x28).readU32()]);
        if (i >= n) return 0;
        for (let off = 0; off < 0x94; off += 4) {
          if (off === 0x24 || off === 0x28) continue;
          dm.add(off).writeU32(((0xde000000 | (i << 16) | off)) >>> 0);
        }
        dm.add(0x68).writeU32([8, 16, 24, 32, 15][i % 5]);
        dm.add(0x6c).writeU32(640 + 160 * i);
        dm.add(0x70).writeU32(480 + 120 * i);
        dm.add(0x78).writeU32(60 + i);
        return 1;
      } } } });
    out.implog = s.log;
    __freeze.thread();
    const g14 = base.add(0x1286c8), g94 = base.add(0x1286f8);
    __freeze.save(g14, 0x14);
    __freeze.save(g94, 0x14);
    g14.add(4).writePointer(cap14 ? data : ptr(0));
    words(g14.add(8), [cap14, count14, grow14]);
    g94.add(4).writePointer(cap94 ? data.add(0x100) : ptr(0));
    words(g94.add(8), [cap94, count94, grow94]);
    out.g14 = g14; out.g94 = g94;
    return out;
  }

  // ============================================================================ group S: Sprite construction
  // c3cw / c3dc's swapped tracked list: the 0x1c-byte header of 0x10128428 replaced by a list of our own (first
  // thread frozen). Its nodes' vtable slot 0 is c3cw's counting "deleting destructor" stub
  //   eax = ++*ctr; node[+0x14] = eax; ret 4
  // so the node removeCurrent deletes is compared state.
  function nodeVtable(ctr) {
    ctr.writeU32(0);
    const c = code([0xA1].concat(addrBytes(ctr), [0x40, 0xA3], addrBytes(ctr), [0x89, 0x41, 0x14, 0xC2, 0x04, 0x00]));
    const vt = __keepAlloc(0x20);
    for (let i = 0; i < 8; i++) vt.add(i * 4).writePointer(c);
    return vt;
  }
  function buildList(list, nodes, datas, cur, vt) {
    const n = datas.length, ps = [];
    for (let k = 0; k < n; k++) {
      const p = nodes.add(k * 0x20);
      p.writePointer(vt);
      p.add(0xc).writePointer(ptr(datas[k]));
      p.add(0x10).writeU8(0x40 + k);
      ps.push(p);
    }
    for (let k = 0; k < n; k++) {
      ps[k].add(4).writePointer(ps[(k + n - 1) % n]);
      ps[k].add(8).writePointer(ps[(k + 1) % n]);
    }
    list.add(4).writePointer(n && cur >= 0 ? ptr(datas[cur]) : ptr(0x5a5a0001));
    list.add(8).writeU32(n && cur >= 0 ? (0x40 + cur) : 0x5a5a0002);
    list.add(0xc).writeS32(n);
    list.add(0x10).writePointer(n ? ps[0] : ptr(0));
    list.add(0x14).writePointer(n && cur >= 0 ? ps[cur] : ptr(0));
    list.add(0x18).writeU32(0);           // ordered word 0: LinkedList::add never sends its debug message
  }
  // Sprite fields Sprite::release reads (c3dc's RSPEC): payload +0x14, flags +0x18, span table +0x1c, span count
  // +0x24, +0x2c, width +0x30, height +0x34. Owners: the dword the sprite's +0xc points at (dtor clears it).
  const SPR = [
    [1, 0x00, 0, 0x10, 16, 16, 8, 1],
    [1, 0x01, 0, 0x30, 16, 16, 8, 1],
    [1, 0x03, 1, 0x44, 20, 20, 10, 0],
    [0, 0x01, 1, 0x50, 8, 8, 4, 1],
    [0, 0x00, 0, 0x00, 8, 8, 4, 0],
    [1, 0x02, 1, 0x20, 4, 3, 7, 1],
    [1, 0x01, 0, 0x7fffffff, 9, 9, 9, 1],
    [0, 0xfe, 0, 0x12, 3, 4, 5, 0],
    [1, 0x01, 1, 0x13, 100, 640, 480, 1],
    [0, 0x00, 0, 0x00, 1, 1, 1, 1],
    [1, 0xffffffff, 1, 5, 1, 1, 1, 0],
    [1, 0x00, 1, 0x11, 7, 2, 5, 1],
  ];
  // shape: 'empty' | 'five' (the list holds $p0..$p4, current node 2) | 'all' (all twelve, current = head)
  function spriteFx(shape, opts) {
    opts = opts || {};
    settle();
    const base = jgld();
    const h = standIn(opts.arena || 0x400, opts.heap);
    const blk = __keepAlloc(0x2000);
    fill(blk, 0x2000, 0x5b17);
    const vt = nodeVtable(blk.add(0x1ff0));
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk };
    // sprites $p0..$p11 at blk + i*0x60; owners $w0..$w11 at blk + 0x600 + i*4; payloads at 0x800+, spans 0xc00+
    SPR.forEach(function (s, i) {
      const p = blk.add(0x60 * i);
      const w = blk.add(0x600 + 4 * i);
      p.writePointer(base.add(0x11d380));
      p.add(0xc).writePointer(s[7] ? w : ptr(0));
      p.add(0x14).writePointer(s[0] ? blk.add(0x800 + i * 0x20) : ptr(0));
      p.add(0x18).writeU32(s[1] >>> 0);
      p.add(0x1c).writePointer(s[2] ? blk.add(0xc00 + i * 0x20) : ptr(0));
      p.add(0x24).writeU32(s[3] >>> 0);
      p.add(0x2c).writeS32(s[4]);
      p.add(0x30).writeS32(s[5]);
      p.add(0x34).writeS32(s[6]);
      out['p' + i] = p;
      out['w' + i] = w;
    });
    const s = __importStandIn({ module: 'jgld.dll', imports: {
      'KERNEL32.dll!InitializeCriticalSection': { args: 1 },
      'KERNEL32.dll!DeleteCriticalSection': { args: 1 },
      // LinkedList::add 0x10006f8b (not reached: the swapped list's ordered word is 0)
      'KERNEL32.dll!OutputDebugStringA': { args: 1 } } });
    out.implog = s.log;
    const glist = base.add(0x128428), gcount = base.add(0x1284c8);
    out.glist = glist;
    out.gcount = gcount;
    const n = { empty: 0, five: 5, all: 12 }[shape];
    const datas = [];
    for (let k = 0; k < n; k++) datas.push(out['p' + k]);
    __freeze.thread();
    __freeze.save(glist, 0x1c);
    buildList(glist, blk.add(0x1000), datas, shape === 'five' ? 2 : 0, vt);
    __freeze.save(gcount, 8);
    gcount.writeU32(0x00123456);
    gcount.add(4).writeU32(0x00654321);
    return out;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ---------------------------------------------------------------------- group M
    c3dg_mf_open() {
      const out = mfBase({});
      // openWrite names: mode, view index
      ['4a', '1b', '2c', '3d', '4e', '4f', '1g', '2h', '3i', '4j', '4k', '2l'].forEach(function (n, i) {
        out['n' + i] = cstr(n + '.dat');
      });
      // create names add the SetFilePointer modes
      ['4a', '1b', '2c', '3d', '5e', '6f', '1g', '5h', '6i', '4j', '3k', '4l'].forEach(function (n, i) {
        out['cn' + i] = cstr(n + '.pcx');
      });
      const s = __importStandIn({ module: 'jgld.dll', imports: mfImports(out.views) });
      out.implog = s.log;
      out.obj = out.mfblk;
      return out;
    },
    c3dg_mf_trunc() {
      const out = truncBase({});
      const s = __importStandIn({ module: 'jgld.dll', imports: mfImports(out.views) });
      out.implog = s.log;
      out.obj = out.mfblk;
      return out;
    },

    // ---------------------------------------------------------------------- group P
    c3dg_realize_pal0() { return realizeFx(0); },
    c3dg_realize_palX() { return realizeFx(0x5a1e0007); },
    c3dg_deletepal_0() { return deleteFx(0, 1); },
    c3dg_deletepal_1() { return deleteFx(1, 1); },
    c3dg_deletepal_2() { return deleteFx(0x5a1e0001, 0); },
    c3dg_deletepal_3() { return deleteFx(0x7fffffff, 1); },
    c3dg_deletepal_4() { return deleteFx(0x80000000, 0); },
    c3dg_deletepal_5() { return deleteFx(0xffffffff, 1); },
    c3dg_deletepal_6() { return deleteFx(0x12345678, 1); },
    c3dg_deletepal_7() { return deleteFx(0xdeadbeef, 0); },
    c3dg_deletepal_8() { return deleteFx(0x00010000, 1); },
    c3dg_deletepal_9() { return deleteFx(0xfffffffe, 0); },
    c3dg_createpal_ok() { return createPalFx({ ret: 0x5a1e0042 }, 0); },
    c3dg_createpal_null() { return createPalFx({ ret: 0 }, 0); },
    c3dg_createpal_oom() { return createPalFx({ ret: 0x5a1e0043 }, 1); },

    // ---------------------------------------------------------------------- group F
    c3dg_fonts_0_0() { return fontFx(0, 0); },
    c3dg_fonts_0_dc() { return fontFx(0, 0xdc77); },
    c3dg_fonts_dc_0() { return fontFx(0xdc01, 0); },
    c3dg_fonts_dc_dc() { return fontFx(0xdc01, 0xdc78); },
    c3dg_fonts_m1_1() { return fontFx(0xffffffff, 1); },
    c3dg_fonts_1_m1() { return fontFx(1, 0xffffffff); },

    // ---------------------------------------------------------------------- group B
    c3dg_stretch() {
      const out = surfaces({});
      const s = __importStandIn({ module: 'jgld.dll', imports: {
        'GDI32.dll!StretchBlt': { args: 11, ret: [1, 0, 1] } } });
      out.implog = s.log;
      out.obj = out.surfs;
      return out;
    },
    c3dg_present() {
      const out = displays(surfaces({}));
      clients(out);
      // IntersectRect (reached through intersect 0x10008590) is pure: it runs for real and is recorded without its
      // first argument, present's local RECT (a stack address that differs between the arms).
      const s = __importStandIn({ module: 'jgld.dll', imports: {
        'GDI32.dll!BitBlt': { args: 9, ret: [1, 0] },
        'USER32.dll!IntersectRect': { args: 3, forward: true, record: [1, 2] } } });
      out.implog = s.log;
      out.obj = out.surfs;
      __freeze.thread();
      palGlobals(out, 0x5a1e0099, out.c3, out.c5);
      return out;
    },

    // ---------------------------------------------------------------------- group K
    c3dg_key_d0() { return keyFx(0); },
    c3dg_key_d3() { return keyFx(3); },

    // ---------------------------------------------------------------------- group E
    c3dg_modes_0() { return modesFx(0, 4, 2, 2, 4, 2, 2); },
    c3dg_modes_1() { return modesFx(1, 4, 0, 2, 4, 0, 2); },
    c3dg_modes_3grow() { return modesFx(3, 2, 1, 2, 2, 1, 2); },
    c3dg_modes_7null() { return modesFx(7, 0, 0, 3, 0, 0, 4); },

    // ---------------------------------------------------------------------- group S
    c3dg_spr_empty() { return spriteFx('empty'); },
    c3dg_spr_five() { return spriteFx('five'); },
    c3dg_spr_all() { return spriteFx('all'); },
    c3dg_spr_new() { return spriteFx('five', { arena: 0x1000 }); },
    c3dg_spr_new_oom() { return spriteFx('five', { arena: 0x1000, heap: { failMask: 1 } }); },
  });

  function realizeFx(pal) {
    const out = {};
    const s = __importStandIn({ module: 'jgld.dll', imports: dcImports() });
    out.implog = s.log;
    out.obj = s.log;
    __freeze.thread();
    palGlobals(out, pal, ptr(0x0c0c0001), ptr(0x0c0c0002));
    return out;
  }
  function deleteFx(pal, ret) {
    const out = {};
    const s = __importStandIn({ module: 'jgld.dll', imports: { 'GDI32.dll!DeleteObject': { args: 1, ret: ret } } });
    out.implog = s.log;
    out.obj = s.log;
    __freeze.thread();
    palGlobals(out, pal, ptr(0x0c0c0003), ptr(0x0c0c0004));
    return out;
  }
  // createPalette: CreatePalette's result from `cp`; failMask 1 makes jalloc's malloc return NULL.
  function createPalFx(cp, failMask) {
    const out = clients({});
    const h = standIn(0x420, { failMask: failMask });
    out.arena = h.arena; out.freelog = h.log;
    const s = __importStandIn({ module: 'jgld.dll', imports: {
      'GDI32.dll!DeleteObject': { args: 1, ret: [1, 0] },              // deleteLogPalette 0x1006ad89
      'GDI32.dll!CreatePalette': Object.assign({ args: 1 }, cp) } });
    out.implog = s.log;
    out.obj = out.clients;
    __freeze.thread();
    palGlobals(out, 0x5a1e0001, ptr(0x0c0c0005), ptr(0x0c0c0006));
    return out;
  }
  // initFonts: the previous font DC in 0x10128724 and CreateCompatibleDC's result.
  function fontFx(prev, dc) {
    const out = {};
    const s = __importStandIn({ module: 'jgld.dll', imports: {
      'GDI32.dll!DeleteDC': { args: 1, ret: 1 },
      'GDI32.dll!CreateCompatibleDC': { args: 1, ret: dc } } });
    out.implog = s.log;
    out.obj = s.log;
    const g = jgld().add(0x128724);
    out.gfont = g;
    __freeze.thread();
    __freeze.save(g, 4);
    g.writeU32(prev >>> 0);
    return out;
  }


  // ============================================================================ group C: Sprite::create / create8 / create16
  // jgld's CRT _expand 0x10080610 (called by the span form to shrink the payload) would walk the debug heap's block
  // header in front of an ARENA block. For the key it is replaced, on the A/B thread only, by a recorder that keeps
  // the block where it is and returns it: (pointer, size) pairs land in `$exlog` (+0 count, then 8 bytes each), a
  // state region. Calls from any other thread reach the real _expand through `orig` (Interceptor bypasses a
  // replacement for calls made from inside it). Reverted through the freeze helper's flush, i.e. BEFORE the frozen
  // first thread (the only thread that runs Sprite::create8, ~3,100 calls/s at the menu, log/c3/alt/c3dg_probe.py)
  // resumes: a first version reverted from its own setTimeout(0), which can fire after the flush has resumed that
  // thread, and the game died during the fourth create key of a run (alone, the same key was GREEN).
  let expandLive = null;
  function expandStandIn() {
    const at = jgld().add(0x80610);
    if (expandLive) { Interceptor.revert(expandLive); expandLive = null; }
    const log = __keepAlloc(0x104);
    log.writeByteArray(new Array(0x104).fill(0));
    const orig = new NativeFunction(at, 'pointer', ['pointer', 'uint32']);
    const tid = Process.getCurrentThreadId();
    const cb = new NativeCallback(function (p, n) {
      if (Process.getCurrentThreadId() !== tid) return orig(p, n);
      const k = log.readU32();
      if (k < 32) { log.add(4 + 8 * k).writePointer(p); log.add(8 + 8 * k).writeU32(n); }
      log.writeU32(k + 1);
      return p;
    }, 'pointer', ['pointer', 'uint32']);
    __diffKeepAlive.push(cb, orig);
    Interceptor.replace(at, cb);
    expandLive = at;
    __freeze.atFlush(function () { if (expandLive) { Interceptor.revert(expandLive); expandLive = null; } });
    return log;
  }
  // Source images. 8-bit: a = mixed (about 3 pixels in 8 are the key 0xee), b = rows alternating all-key /
  // no-key / one pixel at the right edge / one at the left edge, c = no key pixel at all. 16-bit: the same shapes
  // with the key 0xf81f.
  function images(out) {
    const n8 = 0x10000, n16 = 0x20000;
    const a8 = __keepAlloc(n8), b8 = __keepAlloc(0x2000), c8 = __keepAlloc(0x2000);
    const a16 = __keepAlloc(n16), b16 = __keepAlloc(0x4000), c16 = __keepAlloc(0x4000);
    let r = 0x1234567;
    const rnd = () => { r = (Math.imul(r, 1103515245) + 12345) >>> 0; return r >>> 16; };
    const ba = new Uint8Array(n8);
    for (let i = 0; i < n8; i++) { const v = rnd(); ba[i] = (v & 7) < 3 ? 0xee : (v >>> 3) & 0xff; }
    a8.writeByteArray(ba.buffer);
    const wa = new Uint16Array(n16 / 2);
    for (let i = 0; i < n16 / 2; i++) { const v = rnd(); wa[i] = (v & 7) < 3 ? 0xf81f : (v * 7) & 0xffff; }
    a16.writeByteArray(wa.buffer);
    // b: row kind by (offset / 64) % 4 over a 64-pixel stride; the vectors use widths that make rows straddle
    const bb = new Uint8Array(0x2000), wb = new Uint16Array(0x2000);
    for (let i = 0; i < 0x2000; i++) {
      const k = (i >> 6) & 3, x = i & 63;
      const key8 = k === 0 || (k === 2 && x !== 63) || (k === 3 && x !== 0);
      bb[i] = key8 ? 0xee : (0x10 + i) & 0xff;
      if (bb[i] === 0xee && !key8) bb[i] = 0x11;
      wb[i] = key8 ? 0xf81f : (0x1000 + i) & 0xffff;
    }
    b8.writeByteArray(bb.buffer);
    b16.writeByteArray(wb.buffer);
    const cb_ = new Uint8Array(0x2000), wc = new Uint16Array(0x2000);
    for (let i = 0; i < 0x2000; i++) { cb_[i] = (i % 0xed) + 1; wc[i] = (i * 3 + 1) & 0x7fff; }
    c8.writeByteArray(cb_.buffer);
    c16.writeByteArray(wc.buffer);
    Object.assign(out, { s8a: a8, s8b: b8, s8c: c8, s16a: a16, s16b: b16, s16c: c16 });
    return out;
  }
  // Sprites $q0..$q11 (stride 0x60) on jgld's Sprite vtable 0x1011d380 (slot +8 Sprite::release, slot +0x10
  // Sprite::setField10). Rows: [old payload, old span table, flags, key colour]; release frees the old blocks through
  // jfree (recorded by the heap stand-in) and lowers the counters.
  const QROWS = [
    [1, 0, 0x00, 0xee], [1, 1, 0x01, 0xee], [0, 0, 0x00, 0xee], [1, 1, 0x03, 0xee],
    [0, 1, 0x01, 0xee], [1, 0, 0x02, 0xee], [0, 0, 0x00, 0xee], [1, 0, 0x00, 0xee],
    [0, 0, 0x00, 0xee], [1, 1, 0x01, 0xee], [0, 0, 0x00, 0x1ff], [1, 0, 0x00, 0xee],
  ];
  function createFx(depth, enable, failMask) {
    settle();
    const base = jgld();
    const h = standIn(0x22000, { failMask: failMask || 0 });
    const blk = __keepAlloc(0x1000);
    fill(blk, 0x1000, 0xc8e7);
    const out = images({ arena: h.arena, freelog: h.log, blk: blk, obj: blk });
    QROWS.forEach(function (q, i) {
      const p = blk.add(0x60 * i);
      p.writePointer(base.add(0x11d380));
      p.add(0x14).writePointer(q[0] ? blk.add(0x800 + 0x40 * i) : ptr(0));
      p.add(0x18).writeU32(q[2]);
      p.add(0x1c).writePointer(q[1] ? blk.add(0xb00 + 0x20 * i) : ptr(0));
      p.add(0x24).writeU32(0x10 + i);
      p.add(0x28).writeU32(depth === 16 ? (q[3] === 0xee ? 0xf81f : 0x1ffff) : q[3]);
      out['q' + i] = p;
    });
    out.exlog = expandStandIn();
    const g = base.add(0x1284c4);
    out.gspr = g;
    __freeze.thread();
    __freeze.save(g, 12);
    g.writeU32(enable);
    g.add(4).writeU32(0x00100000);
    g.add(8).writeU32(0x00200000);
    return out;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dg_create8_span() { return createFx(8, 1, 0); },
    c3dg_create8_flat() { return createFx(8, 0, 0); },
    c3dg_create8_oom() { return createFx(8, 1, 1); },
    c3dg_create8_spanoom() { return createFx(8, 1, 2); },
    c3dg_create16_span() { return createFx(16, 1, 0); },
    c3dg_create16_flat() { return createFx(16, 0, 0); },
    c3dg_create16_oom() { return createFx(16, 1, 1); },
    c3dg_create16_spanoom() { return createFx(16, 1, 2); },
  });
})();
