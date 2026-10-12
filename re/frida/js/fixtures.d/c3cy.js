// Fixtures for batch c3cy (round 24, 2026-10-10): the IMPORT STAND-IN pilot. Every fixture installs
// re/frida/js/importstandin.js's __importStandIn over the imports its function calls, so for the length of one key
// those imports are recording stubs on the A/B thread: both arms see the same results and the call sequence (import,
// order, arguments) lands in the log `implog`, which every key lists as a state region. Other threads keep reaching
// the real APIs through the stub's native forwarding path.
//
// Values handed to the stand-ins are synthetic (handle numbers such as 0x3a01, view addresses such as 0x7a000001):
// nothing here is a real handle, and no stub forwards to the real API, so no real file, timer, font or cursor is
// touched. Functions that write live globals (the cursor-clip owner, the cursor metrics, the RNG seed) freeze the
// game's first thread for the key with __freeze (re/frida/js/freeze.js) and have the globals restored before it
// resumes.
//
// Reimplementations: shim/src/re/c3cy.cpp. Registry: re/frida/registry.d/c3cy.py.

(function () {
  const LOG = 0x400;     // implog size; c3cy.py's state regions use the same number


  function cstr(s) { return Memory.allocUtf8String(s); }
  function words(p, vals) { vals.forEach(function (v, i) { p.add(4 * i).writeU32(v >>> 0); }); }

  // ---- MappedFile objects, shared by the exe and jgld families. Layout read by both close bodies:
  //   +0 vtable, +4 view (UnmapViewOfFile), +8 file handle (-1 = none), +0xc mapping handle, +0x10 size.
  // Rows: [view, file, mapping]. Every combination of the three tests, file handle 0 included (not -1, so it is
  // closed), and two all-empty objects.
  const MF_ROWS = [
    [0, -1, 0], [0x7a000001, -1, 0], [0, 0x3a02, 0], [0, -1, 0x5a03],
    [0x7a000004, 0x3a04, 0], [0x7a000005, -1, 0x5a05], [0, 0x3a06, 0x5a06], [0x7a000007, 0x3a07, 0x5a07],
    [0, 0, 0], [0x7a000009, 0, 0x5a09], [0, -1, 0], [0x7a00000b, 0x3a0b, 0x5a0b],
  ];
  function mfObjects(out, vtbl) {
    const blk = Memory.alloc(0x20 * MF_ROWS.length);
    MF_ROWS.forEach(function (r, i) {
      const o = blk.add(0x20 * i);
      words(o, [vtbl, r[0], r[1], r[2], 0x11110000 + i, 0x22220000 + i, 0x33330000 + i, 0x44440000 + i]);
      out['mf' + i] = o;
    });
    out.mfblk = blk;
    return out;
  }

  // CreateFileA / CreateFileMappingA / MapViewOfFile results decided by the file name, so one key reaches every exit
  // of MappedFile::open: name[0] '1' = CreateFileA fails (-1), '2' = CreateFileMappingA fails (NULL), '3' =
  // MapViewOfFile fails (NULL), anything else succeeds. The handles carry name[0] / name[1] so later calls can tell.
  function openImports() {
    return {
      'KERNEL32.dll!CreateFileA': function (a) {
        const n = a[0].readU8(), m = a[0].add(1).readU8();
        return n === 0x31 ? -1 : (0x30000 | (n << 8) | m);
      },
      'KERNEL32.dll!GetLastError': { ret: 2 },
      'KERNEL32.dll!CreateFileMappingA': function (a) {
        const h = a[0].toUInt32();
        return ((h >>> 8) & 0xff) === 0x32 ? 0 : (h + 0x1000000);
      },
      'KERNEL32.dll!MapViewOfFile': function (a) {
        const m = a[0].toUInt32();
        return ((m >>> 8) & 0xff) === 0x33 ? 0 : (0x7b000000 | (m & 0xffff));
      },
      'KERNEL32.dll!GetFileSize': function (a) { return (a[0].toUInt32() & 0xff) * 0x1000 + 0x123; },
      'KERNEL32.dll!UnmapViewOfFile': { ret: 1 },
      'KERNEL32.dll!CloseHandle': { ret: 1 },
    };
  }

  // ---- variants of the argument-free functions: one fixture per stand-in setting
  const METRICS = [[32, 32], [0, 0], [64, 48], [1, 2], [-1, 7], [0x7fffffff, 0x80000000], [16, 0], [0, 16],
                   [48, 64], [0x12345, 0x6789a]];
  const KEYSTATE = [[0, 0], [0x8000, 0], [0, 1], [0x8000, 1], [0x8001, 0], [0x7fff, 1], [0xffff, 0], [1, 1],
                    [0x8000, 5], [0x4000, 0xfffffffe]];
  const TIMES = [0, 1, 37, 1000, 0x10000, 0x6f2a1c3b, 0x80000000, 0xffffffff, 0x0ad3f001, 0x7ffffffe];

  const F = {
    // ------------------------------------------------------------------------- exe MappedFile family
    c3cy_mf_exe() {
      const out = mfObjects({}, 0xdeadbe00);
      const s = __importStandIn({ module: 'golf_clean.exe', imports: {
        'KERNEL32.dll!UnmapViewOfFile': { ret: 1 }, 'KERNEL32.dll!CloseHandle': { ret: 1 } } });
      out.implog = s.log;
      return out;
    },
    // MappedFile::open: names for every exit, the objects above as `this` (so the leading close has work to do)
    c3cy_mf_open() {
      const out = mfObjects({}, 0xdeadbe00);
      ['4a', '1a', '2b', '3c', '4d', '4e', '1f', '2g', '3h', '4i', '4j', '2k'].forEach(function (n, i) {
        out['name' + i] = cstr(n + '.dat');
      });
      const s = __importStandIn({ module: 'golf_clean.exe', imports: openImports() });
      out.implog = s.log;
      return out;
    },

    // ------------------------------------------------------------------------- releaseCursorClip
    // g_cursorClipOwner 0x0083aa98 set to window w0 (or NULL), with the first thread frozen.
    c3cy_clip_w0() { return clipFixture(0); },
    c3cy_clip_w3() { return clipFixture(3); },
    c3cy_clip_null() { return clipFixture(-1); },

    // ------------------------------------------------------------------------- fileExists
    c3cy_files() {
      const out = {};
      ['yes.txt', 'no.txt', 'yA', 'nB', 'y', 'n', 'yes\\dir\\file.bin', 'nothing', 'y*.sve', 'n*.*', 'Yupper', 'Nupper']
        .forEach(function (n, i) { out['f' + i] = cstr(n); });
      out.f12 = cstr('');
      const s = __importStandIn({ module: 'golf_clean.exe', imports: {
        // 'n' / 'N' first, or an empty name: INVALID_HANDLE_VALUE; otherwise a handle built from the name
        'KERNEL32.dll!FindFirstFileA': { record: [0], impl: function (a) {
          const c = a[0].readU8();
          return (c === 0x6e || c === 0x4e || c === 0) ? -1 : (0x4f000 | c);
        } } } });
      out.implog = s.log;
      return out;
    },

    // ------------------------------------------------------------------------- Snd::busy
    c3cy_snd_t100k() { return sndFixture(100000); },
    c3cy_snd_t5() { return sndFixture(5); },

    // ------------------------------------------------------------------------- jgld MappedFile family
    c3cy_mf_jgld() {
      const base = Process.getModuleByName('jgld.dll').base;
      const out = mfObjects({}, base.add(0x11d01c).toUInt32() ^ 0x5a5a);   // not the class vtable before the dtor
      const s = __importStandIn({ module: 'jgld.dll', imports: {
        'KERNEL32.dll!UnmapViewOfFile': { ret: 1 }, 'KERNEL32.dll!CloseHandle': { ret: 1 } } });
      out.implog = s.log;
      return out;
    },
    // deletingDtor: operator delete 0x1007e6d0 records instead of freeing (heap stand-in, free side only)
    c3cy_mf_jgld_del() {
      const out = F.c3cy_mf_jgld();
      const h = __heapStandIn({ module: 'jgld.dll', alloc: [], free: [0x7e6d0, 0x806d0], arenaSize: 0x100 });
      out.freelog = h.log;
      return out;
    },

    // ------------------------------------------------------------------------- jgld Font::release
    // Font objects: +0x18 GDI font handle, +0x20 font-file name (a fixture string). jfree 0x10014a90 hands the name
    // to the CRT free 0x100806d0, which the heap stand-in records instead of freeing.
    c3cy_font() {
      const out = {};
      const blk = Memory.alloc(0x40 * 12);
      const rows = [[0, 0], [0x1f01, 0], [0, 1], [0x1f03, 1], [0xffffffff, 0], [0, 1], [0x1f06, 1], [1, 0],
                    [0x80000000, 1], [0, 0], [0x1f0a, 1], [0x1f0b, 0]];
      rows.forEach(function (r, i) {
        const o = blk.add(0x40 * i);
        for (let k = 0; k < 0x40; k += 4) o.add(k).writeU32((0x66000000 + i * 0x100 + k) >>> 0);
        o.add(0x18).writeU32(r[0] >>> 0);
        if (r[1]) { const nm = cstr('font' + i + '.ttf'); out['name' + i] = nm; o.add(0x20).writePointer(nm); }
        else o.add(0x20).writeU32(0);
        out['ft' + i] = o;
      });
      out.ftblk = blk;
      const s = __importStandIn({ module: 'jgld.dll', imports: {
        'GDI32.dll!DeleteObject': { ret: 1 }, 'GDI32.dll!RemoveFontResourceA': { ret: 1 } } });
      out.implog = s.log;
      const h = __heapStandIn({ module: 'jgld.dll', alloc: [], free: [0x7e6d0, 0x806d0], arenaSize: 0x100 });
      out.freelog = h.log;
      return out;
    },
  };

  function clipFixture(owner) {
    const out = {};
    const blk = Memory.alloc(0x100);
    for (let i = 0; i < 6; i++) out['w' + i] = blk.add(0x20 * i);
    const s = __importStandIn({ module: 'golf_clean.exe', imports: { 'USER32.dll!ClipCursor': { ret: 1 } } });
    out.implog = s.log;
    __freeze.thread();
    __freeze.setU32(ptr(0x0083aa98), owner < 0 ? 0 : out['w' + owner].toUInt32());
    return out;
  }

  // Snd-like objects: +0x40 device, +0x58 flags (bit 4 tested), +0x64 duration, +0x68 start time. The device is a
  // fixture object whose vtable slot +0x5c (the one Snd::queryDevice4845d0 0x004845d0 jumps through) returns the
  // dword at device+4.
  function sndFixture(now) {
    const out = {};
    const dev = Memory.alloc(0x20), vt = Memory.alloc(0x80);
    const slot = new NativeCallback(function (self) { return self.add(4).readU32(); }, 'uint32', ['pointer'], 'thiscall');
    __diffKeepAlive.push(slot);
    vt.add(0x5c).writePointer(slot);
    dev.writePointer(vt);
    dev.add(4).writeU32(7);
    const blk = Memory.alloc(0x80 * 12);
    // [device, flags, duration, start]
    const rows = [
      [1, 0x10, 50, now - 10],          // device present: the device's answer (7), timer fields ignored
      [1, 0x00, 0, 0],                  // device present, bit 4 clear
      [0, 0x00, 50, now - 10],          // no device, bit 4 clear: queryDevice with no device returns 0
      [0, 0x10, 50, 0],                 // bit 4 set, start 0: 0
      [0, 0x10, 50, now - 10],          // elapsed 10 < 50: 1
      [0, 0x10, 10, now - 10],          // elapsed == duration: 0
      [0, 0x10, 9, now - 10],           // duration < elapsed: 0
      [0, 0x10, 50, now + 10],          // start after now: elapsed wraps to 0xfffffff6: 0
      [0, 0x10, 0xffffffff, now + 10],  // duration above the wrapped elapsed: 1
      [0, 0xffffffef, 50, now - 10],    // every flag but bit 4: queryDevice, 0
      [0, 0x30, 1, now],                // elapsed 0 < 1: 1
      [0, 0x10, 0, now],                // elapsed 0, duration 0: 0
    ];
    rows.forEach(function (r, i) {
      const o = blk.add(0x80 * i);
      for (let k = 0; k < 0x80; k += 4) o.add(k).writeU32((0x55000000 + i * 0x100 + k) >>> 0);
      o.add(0x40).writeU32(r[0] ? dev.toUInt32() : 0);
      o.add(0x58).writeU32(r[1] >>> 0);
      o.add(0x64).writeU32(r[2] >>> 0);
      o.add(0x68).writeU32(r[3] >>> 0);
      out['s' + i] = o;
    });
    out.sblk = blk;
    const s = __importStandIn({ module: 'golf_clean.exe', imports: { 'WINMM.dll!timeGetTime': { ret: now >>> 0 } } });
    out.implog = s.log;
    return out;
  }

  // cacheCursorMetrics: GetSystemMetrics answers per variant; g_cursorWidth / g_cursorHeight 0x0083b5f8 / 0x0083b5fc
  // are written, so the first thread is frozen and both are restored before it resumes.
  METRICS.forEach(function (m, v) {
    F['c3cy_metrics_' + v] = function () {
      const s = __importStandIn({ module: 'golf_clean.exe', imports: {
        'USER32.dll!GetSystemMetrics': function (a) { const k = a[0].toUInt32(); return k === 0xd ? m[0] : k === 0xe ? m[1] : 0x99; } } });
      __freeze.thread();
      __freeze.save(ptr(0x0083b5f8), 8);
      return { implog: s.log };
    };
  });
  // isPrimaryMouseDown: GetAsyncKeyState(VK_LBUTTON) and GetSystemMetrics(SM_SWAPBUTTON) answers per variant. No
  // global is written.
  KEYSTATE.forEach(function (k, v) {
    F['c3cy_mouse_' + v] = function () {
      const s = __importStandIn({ module: 'golf_clean.exe', imports: {
        'USER32.dll!GetAsyncKeyState': function (a) { return a[0].toUInt32() === 1 ? k[0] : 0x5555; },
        'USER32.dll!GetSystemMetrics': function (a) { return a[0].toUInt32() === 0x17 ? k[1] : 0x99; } } });
      return { implog: s.log };
    };
  });
  // seedFromTime: timeGetTime answers per variant; 0x0051b380 and the RNG seed 0x00822d9c are written, so the first
  // thread (which draws random numbers from that seed) is frozen and both are restored before it resumes.
  TIMES.forEach(function (t, v) {
    F['c3cy_seed_' + v] = function () {
      const s = __importStandIn({ module: 'golf_clean.exe', imports: { 'WINMM.dll!timeGetTime': { ret: t } } });
      __freeze.thread();
      __freeze.save(ptr(0x0051b380), 4);
      __freeze.save(ptr(0x00822d9c), 4);
      return { implog: s.log };
    };
  });

  // ---------------------------------------------------------------------------------- Timer family (exe)
  // Twelve Timer objects (0x40 apart; layout in shim/src/re/c3cy.cpp): +4 callback, +8 flags, +0xc id, +0x10 / +0x14
  // targets, +0x20 interval, +0x24 posted flag, +0x28 resolution. Row: [flags, id, t10, t14, interval, res, posted,
  // callback on]. Intervals cover both sides of the `cmp 0x32` (0x31 / 0x32, 49 / 50, 0, 0xffffffff, 0x80000000);
  // ids zero and non-zero; flag bit 0 set and clear; 13 makes the timeSetEvent stand-in fail, 1000 the SetTimer one.
  const TIMER_ROWS = [
    [0, 0, 0, 0, 10, 5, 0, 0], [1, 0x7101, 1, 0, 10, 1, 0, 0], [0, 0x7102, 0, 1, 13, 5, 0, 0],
    [1, 0x7203, 1, 1, 0x32, 5, 1, 0], [0, 0x7204, 1, 0, 1000, 5, 0, 1], [3, 0, 0, 1, 0x31, 0, 0, 0],
    [0xfffffffe, 0x7206, 0x80000000, 0, 0xffffffff, 5, 0, 0], [1, 5, 0, 0, 0, 5, 0, 1], [2, 0x7208, 1, 0, 50, 2, 1, 0],
    [0xffffffff, 0x7109, 0, 7, 49, 9, 0, 0], [1, 0, 0, 0, 0x32, 5, 0, 0], [0, 0x720b, 2, 2, 0x80000000, 5, 0, 0],
  ];
  // display: 'fake' (a recording display swapped in at 0x0083ad50) or 'null' (0x0083ad50 = 0); owner: the value
  // written to the global 0x0083aff0 Timer::callback compares with: -1 = 0, else a timer index, or 'other'.
  function timerFixture(display, owner) {
    const out = {};
    const blk = Memory.alloc(0x40 * 12);
    const cbctr = Memory.alloc(4);
    cbctr.writeU32(0);
    // the no-argument callback at Timer+4: counts its calls
    const cb = new NativeCallback(function () { cbctr.writeU32(cbctr.readU32() + 1); }, 'void', []);
    __diffKeepAlive.push(cb);
    TIMER_ROWS.forEach(function (r, i) {
      const o = blk.add(0x40 * i);
      for (let k = 0; k < 0x40; k += 4) o.add(k).writeU32((0x77000000 + i * 0x100 + k) >>> 0);
      o.add(4).writeU32(r[7] ? cb.toUInt32() : 0);
      words(o.add(8), [r[0], r[1], r[2], r[3]]);
      o.add(0x20).writeU32(r[4] >>> 0);
      o.add(0x24).writeU32(r[6]);
      o.add(0x28).writeU32(r[5]);
      out['t' + i] = o;
    });
    out.tblk = blk;
    out.cbctr = cbctr;
    // recording display: +0 vtable, +4 the HWND slot +0x20 returns, +8 calls of slot +0x150, +0xc calls of any other
    // slot (none expected)
    const disp = Memory.alloc(0x10), vt = Memory.alloc(0x200);
    words(disp, [0, 0x0000beef, 0, 0]);
    const hwnd = new NativeCallback(function (self) { return self.add(4).readU32(); }, 'uint32', ['pointer'], 'thiscall');
    const s150 = new NativeCallback(function (self) { self.add(8).writeU32(self.add(8).readU32() + 1); return 0; },
                                    'uint32', ['pointer'], 'thiscall');
    const other = new NativeCallback(function (self) { self.add(0xc).writeU32(self.add(0xc).readU32() + 1); return 0; },
                                     'uint32', ['pointer'], 'thiscall');
    __diffKeepAlive.push(hwnd, s150, other);
    for (let k = 0; k < 0x200; k += 4) vt.add(k).writePointer(other);
    vt.add(0x20).writePointer(hwnd);
    vt.add(0x150).writePointer(s150);
    disp.writePointer(vt);
    out.disp = disp;
    const s = __importStandIn({ module: 'golf_clean.exe', imports: {
      'WINMM.dll!timeSetEvent': function (a) { const d = a[0].toUInt32(); return d === 13 ? 0 : (0x7100 | (d & 0xff)); },
      'WINMM.dll!timeKillEvent': { ret: 0 },
      'USER32.dll!SetTimer': function (a) { const d = a[2].toUInt32(); return d === 1000 ? 0 : (0x7200 | (d & 0xff)); },
      'USER32.dll!KillTimer': { ret: 1 },
      'USER32.dll!PostMessageA': { args: 4, ret: 1 } } });
    out.implog = s.log;
    __freeze.thread();
    __freeze.setU32(ptr(0x0083ad50), display === 'fake' ? __recDisplay.guard(disp).toUInt32() : 0);
    __freeze.save(ptr(0x0083ad4c), 4);
    __freeze.setU32(ptr(0x0083aff0), owner === -1 ? 0 : owner === 'other' ? 0x0abc0000 : out['t' + owner].toUInt32());
    return out;
  }
  F.c3cy_timer = function () { return timerFixture('fake', -1); };
  F.c3cy_timer_nodisp = function () { return timerFixture('null', -1); };
  F.c3cy_timer_own3 = function () { return timerFixture('fake', 3); };
  F.c3cy_timer_own1 = function () { return timerFixture('fake', 1); };
  F.c3cy_timer_other = function () { return timerFixture('fake', 'other'); };

  // ---------------------------------------------------------------------------------- jgld Font destructor chain
  // Twelve Font objects 0x40 apart: +4 a pointer to a fixture dword (even indices) or NULL, +8 the Tracked sub-object,
  // +0x18 a GDI handle, +0x20 a font-file name (a fixture string) or NULL. Font::~Font reaches Tracked::dtor 0x10006b80
  // on this+8, which looks this+8 up in jgld's tracked-object list 0x10128428 and removes a node: as in c3cw's
  // c3cw_tracked_* fixtures the list is swapped for one of fixture nodes (data = obj+8; node vtable slot 0 a counting
  // stub, so node deletes are recorded) with the game's first thread frozen, after waiting until the list's count
  // stops moving. shape: 'one' (one node holding $ft0+8) or 'five' ($ft0+8..$ft4+8, current = the third).
  function fontChain(shape) {
    const base = Process.getModuleByName('jgld.dll').base;
    const glist = base.add(0x128428);
    let last = glist.add(0xc).readS32(), still = 0;
    for (let t = 0; still < 5; t++) {
      if (t >= 600) throw new Error('c3cy: tracked list still changing after 60 s; not freezing');
      Thread.sleep(0.1);
      const c = glist.add(0xc).readS32();
      still = c === last ? still + 1 : 0;
      last = c;
    }
    const out = { glist: glist };
    const blk = Memory.alloc(0x800);
    for (let k = 0; k < 0x800; k += 4) blk.add(k).writeU32((0x6a000000 + k) >>> 0);
    const rows = [[0x1f00, 1], [0, 0], [0x1f02, 0], [0, 1], [0x1f04, 1], [0x1f05, 0], [0, 1], [0x1f07, 1],
                  [0, 0], [0x1f09, 1], [0x1f0a, 0], [0, 1]];
    rows.forEach(function (r, i) {
      const o = blk.add(0x40 * i);
      o.add(4).writePointer(i % 2 === 0 ? blk.add(0x600 + 4 * i) : ptr(0));
      o.add(0x18).writeU32(r[0]);
      if (r[1]) { const nm = cstr('chain' + i + '.ttf'); out['name' + i] = nm; o.add(0x20).writePointer(nm); }
      else o.add(0x20).writeU32(0);
      out['ft' + i] = o;
      out['tt' + i] = o.add(8);
    });
    out.fblk = blk;
    // node delete stub: `mov eax, [ctr]; inc eax; mov [ctr], eax; mov [ecx+0x14], eax; ret 4` (c3cw's)
    const ctr = blk.add(0x7f0);
    ctr.writeU32(0);
    const code = Memory.alloc(Process.pageSize);
    Memory.protect(code, Process.pageSize, 'rwx');
    code.writeByteArray([0xA1, 0, 0, 0, 0, 0x40, 0xA3, 0, 0, 0, 0, 0x89, 0x41, 0x14, 0xC2, 0x04, 0x00]);
    code.add(1).writePointer(ctr);
    code.add(7).writePointer(ctr);
    const nodeVt = Memory.alloc(0x20);
    for (let i = 0; i < 8; i++) nodeVt.add(i * 4).writePointer(code);
    const n = shape === 'one' ? 1 : 5;
    const nodes = [];
    for (let k = 0; k < n; k++) {
      const p = blk.add(0x400 + k * 0x20);
      p.writePointer(nodeVt);
      p.add(0xc).writePointer(out['tt' + k]);
      p.add(0x10).writeU8(0x40 + k);
      nodes.push(p);
    }
    for (let k = 0; k < n; k++) {
      nodes[k].add(4).writePointer(nodes[(k + n - 1) % n]);
      nodes[k].add(8).writePointer(nodes[(k + 1) % n]);
    }
    const s = __importStandIn({ module: 'jgld.dll', imports: {
      'GDI32.dll!DeleteObject': { ret: 1 }, 'GDI32.dll!RemoveFontResourceA': { ret: 1 } } });
    out.implog = s.log;
    const h = __heapStandIn({ module: 'jgld.dll', alloc: [], free: [0x7e6d0, 0x806d0], arenaSize: 0x100 });
    out.freelog = h.log;
    __freeze.thread();
    __freeze.save(glist, 0x1c);
    glist.add(4).writeU32(0x5a5a0001);
    glist.add(8).writeU32(0x5a5a0002);
    glist.add(0xc).writeS32(n);
    glist.add(0x10).writePointer(nodes[0]);
    glist.add(0x14).writePointer(shape === 'five' ? nodes[2] : nodes[0]);
    glist.add(0x18).writeU32(0);
    return out;
  }
  F.c3cy_fontchain_one = function () { return fontChain('one'); };
  F.c3cy_fontchain_five = function () { return fontChain('five'); };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
