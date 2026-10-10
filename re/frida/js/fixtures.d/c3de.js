// Fixtures for batch c3de (round 25, 2026-10-10): golf_clean.exe functions that reach Win32 imports directly
// (capsLockOff, showMessageBox, EditBox::insertChar) or through the Timer family (HotList, Timer::scalarDtor,
// startColorCycle). Every fixture installs re/frida/js/importstandin.js over the imports the function and its callees
// call, so for the length of one key those imports are recording stubs on the A/B thread: both arms see the same
// results (decided by the ARGUMENTS, because diff_hook restores the log between vectors and the call counter restarts
// at 0), and the call sequence lands in the log `implog`, a state region of every key. Keys that free or allocate also
// install re/frida/js/heapstandin.js (the exe's malloc / operator new carve from an arena whose header is state, free
// 0x004a5007 and operator delete 0x004a4ffc only record).
//
// Values handed to the stand-ins are synthetic (ids such as 0x7101, HWND 0xbeef): nothing here is a real handle or
// timer, and no stub forwards to the real API. Functions that read or write live globals (the display 0x0083ad50,
// the input phase 0x0083ad4c, the tooltip globals 0x00083aac0/4, the keep-tips flag 0x0083d348) freeze the game's
// first thread for the key with __freeze (re/frida/js/freeze.js); the values are restored before it resumes.
//
// Reimplementations: shim/src/re/c3de.cpp. Registry: re/frida/registry.d/c3de.py. Purposes: log/c3/c3de_purpose.md.
(function () {
  const F = {};
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  function cstr(s) { return Memory.allocUtf8String(s); }
  function words(p, vals) { vals.forEach(function (v, i) { p.add(4 * i).writeU32(v >>> 0); }); }
  function junk(p, n, tag) { for (let k = 0; k < n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }

  // ------------------------------------------------------------------------------------------------ capsLockOff
  // GetKeyboardState fills the caller's 0x100-byte stack buffer: every byte 0x3c except [0x14] (VK_CAPITAL), set
  // per variant. The buffer address is a stack address and differs between the arms, so it is not recorded (the
  // per-import call counters still are). Row: [keys[0x14], GetKeyboardState result, keybd_event result].
  const CAPS = [
    [0x01, 1, 0x1111], [0x00, 1, 0x2222], [0x81, 1, 0x3333], [0x80, 1, 0x4444], [0xfe, 1, 0x5555], [0x03, 0, 0x6666],
    [0xff, 1, 0x7777], [0x7f, 1, 0x8888], [0x01, 0, 0x9999], [0x00, 0, 0xaaaa], [0xa5, 0x77, 0xbbbb], [0x40, 1, 0xcccc],
  ];
  CAPS.forEach(function (c, v) {
    F['c3de_caps_' + v] = function () {
      const keys = new Array(256).fill(0x3c);
      keys[0x14] = c[0];
      const s = __importStandIn({ module: 'golf_clean.exe', imports: {
        'USER32.dll!GetKeyboardState': { record: false, impl: function (a) { a[0].writeByteArray(keys); return c[1]; } },
        'USER32.dll!keybd_event': { ret: c[2] } } });
      return { implog: s.log };
    };
  });

  // ------------------------------------------------------------------------------------------------ showMessageBox
  // MessageBoxA's result is the first character of the text plus 0x100 times the first character of the caption
  // (0x1ff / 0x2ff for a NULL one), so it changes with each argument. Six strings and NULL give the vectors.
  F.c3de_msgbox = function () {
    const out = {};
    ['Alpha', 'Bravo', 'Charlie', 'Delta', 'Echo', 'Foxtrot'].forEach(function (t, i) { out['s' + i] = cstr(t); });
    const s = __importStandIn({ module: 'golf_clean.exe', imports: {
      'USER32.dll!MessageBoxA': { args: 4, impl: function (a) {
        const t = a[1].isNull() ? 0x1ff : a[1].readU8(), c = a[2].isNull() ? 0x2ff : a[2].readU8();
        return (t + 0x100 * c) >>> 0;
      } } } });
    out.implog = s.log;
    return out;
  };

  // ------------------------------------------------------------------------------------------------ EditBox::insertChar
  // Fourteen fake EditBoxes (0x5c0 bytes each): +0 the shared fake vtable, +0x574 the text buffer (0x40 bytes of its
  // own, 0xaa-filled after the C string), +0x578 the capacity, +0x5a4 the cursor, +0x5b0 / +0x5b4 / +0x5b8 filled by
  // the vtable slot +0x120 stub (calls, the cursor and the first buffer byte it saw, calls of any other slot).
  // Row: [text, capacity, cursor].
  const EB_ROWS = [
    ['abc', 10, 3], ['abc', 10, 1], ['abc', 10, 0], ['', 4, 0], ['abcd', 4, 2], ['ab', 3, 0], ['hello', 5, 5],
    ['x', 40, 1], ['xyz', 40, 2], ['q', 2, 0], ['mnop', 8, 4], ['ab', 9, 1], ['ab', 9, 1], ['ab', 9, 2],
  ];
  F.c3de_editbox = function () {
    const out = {};
    const blk = Memory.alloc(0x5c0 * EB_ROWS.length);
    const bufs = Memory.alloc(0x40 * EB_ROWS.length);
    const vt = Memory.alloc(0x200);
    const slot = keep(new NativeCallback(function (self) {
      self.add(0x5b0).writeU32(self.add(0x5b0).readU32() + 1);
      self.add(0x5b4).writeU32(self.add(0x5a4).readU32());
      self.add(0x5b8).writeU32(self.add(0x574).readPointer().readU8());
      return 0;
    }, 'uint32', ['pointer'], 'thiscall'));
    const other = keep(new NativeCallback(function (self) {
      self.add(0x5bc).writeU32(self.add(0x5bc).readU32() + 1);
      return 0;
    }, 'uint32', ['pointer'], 'thiscall'));
    for (let k = 0; k < 0x200; k += 4) vt.add(k).writePointer(other);
    vt.add(0x120).writePointer(slot);
    EB_ROWS.forEach(function (r, i) {
      const o = blk.add(0x5c0 * i), b = bufs.add(0x40 * i);
      junk(o, 0x5c0, 0x66000000 + i * 0x1000);
      b.writeByteArray(new Array(0x40).fill(0xaa));
      b.writeUtf8String(r[0]);
      o.writePointer(vt);
      o.add(0x574).writePointer(b);
      o.add(0x578).writeU32(r[1]);
      o.add(0x5a4).writeU32(r[2]);
      for (let k = 0x5b0; k < 0x5c0; k += 4) o.add(k).writeU32(0);
      out['e' + i] = o;
    });
    out.eblk = blk;
    out.ebufs = bufs;
    const s = __importStandIn({ module: 'golf_clean.exe', imports: { 'USER32.dll!MessageBeep': { ret: 1 } } });
    out.implog = s.log;
    return out;
  };

  // ------------------------------------------------------------------------------------------------ Timer environment
  // The Timer family's imports and the display they dispatch through, as c3cy's timer fixture builds them. The recording
  // display (+4: the HWND slot +0x20 returns, +8: calls of slot +0x150, +0xc: calls of any other slot) replaces the
  // game's display 0x0083ad50 while the first thread is frozen; 0x0083ad4c (written by pumpInputDisplay483da0
  // 0x00483da0) is saved. timeSetEvent fails (0) for a delay of 13 and SetTimer for an interval of 777, otherwise they
  // return an id made from the low byte of the delay.
  function timerEnv(out, display, extraImports) {
    const disp = Memory.alloc(0x10), vt = Memory.alloc(0x200);
    words(disp, [0, 0x0000beef, 0, 0]);
    const hwnd = keep(new NativeCallback(function (self) { return self.add(4).readU32(); }, 'uint32', ['pointer'], 'thiscall'));
    const s150 = keep(new NativeCallback(function (self) { self.add(8).writeU32(self.add(8).readU32() + 1); return 0; },
                                         'uint32', ['pointer'], 'thiscall'));
    const other = keep(new NativeCallback(function (self) { self.add(0xc).writeU32(self.add(0xc).readU32() + 1); return 0; },
                                          'uint32', ['pointer'], 'thiscall'));
    for (let k = 0; k < 0x200; k += 4) vt.add(k).writePointer(other);
    vt.add(0x20).writePointer(hwnd);
    vt.add(0x150).writePointer(s150);
    disp.writePointer(vt);
    out.disp = disp;
    const imports = {
      'WINMM.dll!timeSetEvent': function (a) { const d = a[0].toUInt32(); return d === 13 ? 0 : (0x7100 | (d & 0xff)); },
      'WINMM.dll!timeKillEvent': { ret: 0 },
      'USER32.dll!SetTimer': function (a) { const d = a[2].toUInt32(); return d === 777 ? 0 : (0x7200 | (d & 0xff)); },
      'USER32.dll!KillTimer': { ret: 1 } };
    Object.keys(extraImports || {}).forEach(function (k) { imports[k] = extraImports[k]; });
    const s = __importStandIn({ module: 'golf_clean.exe', imports: imports });
    out.implog = s.log;
    __freeze.thread();
    __freeze.setU32(ptr(0x0083ad50), display === 'fake' ? disp.toUInt32() : 0);
    __freeze.save(ptr(0x0083ad4c), 4);
    return out;
  }
  function heapFree(out, o) {
    const h = __heapStandIn(Object.assign({ module: 'golf_clean.exe', free: [0xa5007, 0xa4ffc] }, o || {}));
    out.freelog = h.log;
    out.arena = h.arena;
    return h;
  }

  // Timer sub-object rows: [flags, id, t10, t14, interval, resolution, posted]. Intervals sit on both sides of the
  // `cmp 0x32` (0x31 / 0x32, 49 / 50), 13 and 777 are the stand-ins' failure values, 1000 an ordinary SetTimer one.
  const TROWS = [
    [0, 0, 0, 0, 10, 5, 0], [1, 0x7101, 1, 0, 10, 1, 0], [0, 0x7102, 0, 1, 13, 5, 0], [1, 0x7203, 1, 1, 0x32, 5, 1],
    [0, 0x7204, 1, 0, 1000, 5, 0], [3, 0, 0, 1, 0x31, 0, 0], [0xfffffffe, 0x7206, 0x80000000, 0, 0xffffffff, 5, 0],
    [1, 5, 0, 0, 0, 5, 0], [2, 0x7208, 1, 0, 50, 2, 1], [0xffffffff, 0x7109, 0, 7, 49, 9, 0], [1, 0, 0, 0, 777, 5, 0],
    [0, 0x720b, 2, 2, 0x80000000, 5, 0],
  ];
  function writeTimer(o, r) {
    // o points at the Timer (+0 vtable, +8 flags, +0xc id, +0x10 t10, +0x14 t14, +0x20 interval, +0x24 posted, +0x28 res)
    words(o.add(8), [r[0], r[1], r[2], r[3]]);
    o.add(0x20).writeU32(r[4] >>> 0);
    o.add(0x24).writeU32(r[6]);
    o.add(0x28).writeU32(r[5]);
  }

  // Entry arrays for the HotList: a count dword, then 0x20-byte elements with the element vtable 0x004bba70 at +0 (so
  // the game's own vector deleting destructor 0x004927a0 runs over them), +0x14 / +0x18 / +0x1c the fields
  // onMouseMove compares / the tip text pointer (a fixture string or NULL). spec(i) -> [A, B, textOrNull].
  // The arrays live in ONE pool block per fixture (a state region): the game's own destructor writes into them
  // (freeEntryTip clears +0x1c, the element destructor stores its vtable), and the second arm must start from the
  // same bytes as the first.
  function arrayPool(size) {
    const base = Memory.alloc(size);
    base.writeByteArray(new Array(size).fill(0));
    return { base: base, size: size, cur: 0 };
  }
  function mkArray(pl, n, spec, tag) {
    const need = (4 + 0x20 * Math.max(n, 1) + 0x20 + 15) & ~15;
    if (pl.cur + need > pl.size) throw new Error('c3de: array pool exhausted');
    const blk = pl.base.add(pl.cur);
    pl.cur += need;
    junk(blk, need, 0x5a000000 + tag);
    blk.writeU32(n);
    const base = blk.add(4);
    base.writeU32(0x004bba70);       // an empty array still has the first dword the delete call dispatches through
    for (let i = 0; i < n; i++) {
      const e = base.add(0x20 * i), r = spec(i);
      e.writeU32(0x004bba70);
      e.add(0x14).writeU32(r[0] >>> 0);
      e.add(0x18).writeU32(r[1] >>> 0);
      e.add(0x1c).writeU32(r[2] ? r[2].toUInt32() : 0);
    }
    return base;
  }

  // Twelve HotLists, 0x80 bytes apart (the object is 0x60). Row i: Timer row TROWS[i] at +0x20, the hovered key pair
  // (+4, +8), the array, capacity (+0x54), count (+0x58) and tip delay (+0x5c).
  function hotLists(out, rows) {
    const blk = Memory.alloc(0x80 * rows.length);
    rows.forEach(function (r, i) {
      const o = blk.add(0x80 * i);
      junk(o, 0x80, 0x77000000 + i * 0x100);
      o.writeU32(0x004bba74);
      writeTimer(o.add(0x20), TROWS[i % TROWS.length]);
      words(o.add(4), [r.cur ? r.cur[0] : 0xffffffff, r.cur ? r.cur[1] : 0xffffffff]);
      o.add(0x50).writeU32(r.arr ? r.arr.toUInt32() : 0);
      o.add(0x54).writeU32(r.cap >>> 0);
      o.add(0x58).writeU32(r.count >>> 0);
      o.add(0x5c).writeU32(r.delay >>> 0);
      out['h' + i] = o;
    });
    out.hblk = blk;
    return out;
  }
  function freezeKeepTips(v) {
    __freeze.setU32(ptr(0x0083d348), v);
  }

  // ---- resetFields / ctor: junk-filled HotLists, no array
  function plainLists(display) {
    const out = {};
    const rows = [];
    // hovered key pairs other than (-1, -1) on two thirds of the lists: resetFields / the constructor must reset +4 / +8
    for (let i = 0; i < 12; i++) rows.push({ cap: 3 * i, count: i, delay: 10 + i, cur: i % 3 === 0 ? null : [i, 100 + i] });
    hotLists(out, rows);
    timerEnv(out, display);
    return out;
  }
  F.c3de_hl_plain = function () { return plainLists('fake'); };
  F.c3de_hl_plain_nodisp = function () { return plainLists('null'); };

  // ---- releaseArray / dtor / scalarDtor: arrays of real elements (vtable 0x004bba70). Row i: element count n[i] (0 =
  // an empty array, -1 = no array), texts on the odd elements (the game's freeEntryTip frees them while 0x0083d348
  // is 0). The block pointer (array - 4) and the texts are what the recorded frees show.
  const ARR_N = [3, -1, 1, 0, 5, 2, -1, 4, 2, 7, -1, 1];
  function arrayLists(display) {
    const out = {};
    const texts = [];
    for (let i = 0; i < 40; i++) texts.push(cstr('tip' + i));
    out.texts = texts[0];
    const pl = arrayPool(0x2000);
    out.ablk = pl.base;
    const rows = [];
    ARR_N.forEach(function (n, i) {
      const arr = n < 0 ? null : mkArray(pl, n, function (k) { return [k, k + 1, (k & 1) ? texts[(i * 3 + k) % 40] : null]; }, i * 0x100);
      if (arr) out['a' + i] = arr;
      rows.push({ arr: arr, cap: n < 0 ? 0 : n + 2, count: n < 0 ? 0 : n, delay: 100 + i, cur: i % 3 === 0 ? [1, 2] : null });
    });
    hotLists(out, rows);
    heapFree(out);
    timerEnv(out, display);
    freezeKeepTips(0);
    return out;
  }
  F.c3de_hl_arrays = function () { return arrayLists('fake'); };
  F.c3de_hl_arrays_nodisp = function () { return arrayLists('null'); };

  // ---- Timer::scalarDtor: twelve Timer objects (a bare Timer is 0x30 bytes; 0x40 apart), the frees recorded
  F.c3de_timers = function () {
    const out = {};
    const blk = Memory.alloc(0x40 * TROWS.length);
    TROWS.forEach(function (r, i) {
      const o = blk.add(0x40 * i);
      junk(o, 0x40, 0x78000000 + i * 0x100);
      writeTimer(o, r);
      out['t' + i] = o;
    });
    out.tblk = blk;
    heapFree(out);
    timerEnv(out, 'fake');
    return out;
  };

  // ---- onMouseMove: four entry arrays. Entry (A = +0x14, B = +0x18, text). a1 is compared with B and a2 with A.
  //   array 0 (5): (1,1,t) (2,1,NULL) (3,2,t) (1,2,t) (5,5,NULL)       array 1 (1): (9,9,t)
  //   array 2 (3): (1,1,NULL) (1,1,t) (2,2,t)                          array 3 (12): (i, 100 + i, t on odd i)
  function moveLists(display) {
    const out = {};
    const texts = [];
    for (let i = 0; i < 16; i++) texts.push(cstr('tip' + i));
    const A0 = [[1, 1, texts[0]], [2, 1, null], [3, 2, texts[1]], [1, 2, texts[2]], [5, 5, null]];
    const A1 = [[9, 9, texts[3]]];
    const A2 = [[1, 1, null], [1, 1, texts[4]], [2, 2, texts[5]]];
    const pl = arrayPool(0x2000);
    out.ablk = pl.base;
    const arrs = [mkArray(pl, 5, function (i) { return A0[i]; }, 0), mkArray(pl, 1, function (i) { return A1[i]; }, 0x100),
                  mkArray(pl, 3, function (i) { return A2[i]; }, 0x200),
                  mkArray(pl, 12, function (i) { return [i, 100 + i, (i & 1) ? texts[6 + (i % 8)] : null]; }, 0x300)];
    arrs.forEach(function (a, i) { out['arr' + i] = a; });
    // list i: [array, count, delay]; counts 0 and negative with a valid array exercise the `jle` at 0x00492cdc
    const L = [[0, 5, 10], [0, 5, 1000], [0, 5, 13], [0, 5, 777], [1, 1, 0x31], [1, 1, 0x32], [2, 3, 49], [2, 3, 50],
               [3, 12, 10], [3, 12, 1000], [3, 0, 10], [3, -1, 10]];
    const rows = L.map(function (l) { return { arr: arrs[l[0]], cap: 12, count: l[1], delay: l[2], cur: [7, 8] }; });
    hotLists(out, rows);
    timerEnv(out, display);
    return out;
  }
  F.c3de_hl_move = function () { return moveLists('fake'); };
  // MM-only delays: the SetTimer branch reads the display without a NULL test (Timer::startMmOneShot 0x00486e40 after
  // `jae` 0x00486e5e), so a NULL display is only run with delays below 0x32
  F.c3de_hl_move_nodisp = function () {
    const out = moveLists('null');
    return out;
  };

  // ---- cancelTip: the tooltip globals 0x0083aac4 (the visible tip's rectangle, tested by Tooltip::hide 0x00480360) and
  // 0x0083aac0 are set to 0 under the freeze, so Tooltip::hide returns at its first test (0x0048036a) and draws nothing.
  function cancelLists(display) {
    const out = {};
    const cur = [[-1, -1], [-1, 5], [3, -1], [3, 4], [0, 0], [-1, -1], [0xffffffff, 0x7fffffff], [-2, -1], [-1, -2],
                 [1, 1], [-1, -1], [5, 5]];
    hotLists(out, cur.map(function (c) { return { cap: 1, count: 0, delay: 10, cur: c }; }));
    timerEnv(out, display);
    __freeze.setU32(ptr(0x0083aac4), 0);
    __freeze.save(ptr(0x0083aac0), 4);
    return out;
  }
  F.c3de_hl_cancel = function () { return cancelLists('fake'); };
  F.c3de_hl_cancel_nodisp = function () { return cancelLists('null'); };

  // ---- alloc / grow: the heap stand-in carves from an arena (state) and records the frees. alloc: lists with an old
  // array (released first) or none; grow: lists with the capacity, count and old array of each row.
  function allocLists(opts) {
    const out = {};
    const texts = [];
    for (let i = 0; i < 20; i++) texts.push(cstr('tip' + i));
    const N = [3, -1, 2, 0, 4, -1, 1, 0, 2, -1, 5, 1];
    const pl = arrayPool(0x2000);
    out.ablk = pl.base;
    const rows = N.map(function (n, i) {
      const arr = n < 0 ? null : mkArray(pl, n, function (k) { return [k, k + 1, (k & 1) ? texts[(i + k) % 20] : null]; }, i * 0x100);
      return { arr: arr, cap: n < 0 ? 0 : n + 3, count: n < 0 ? 0 : n, delay: 100 + i };
    });
    hotLists(out, rows);
    heapFree(out, { alloc: [0xa57f1], arenaSize: 0x4000, exhaust: 'null', failMask: opts.failMask || 0 });
    timerEnv(out, 'fake');
    freezeKeepTips(0);
    return out;
  }
  F.c3de_hl_alloc = function () { return allocLists({}); };
  F.c3de_hl_alloc_fail = function () { return allocLists({ failMask: 1 }); };

  // grow rows: [count (+0x58), capacity (+0x54)]. The old array has `capacity` elements (its header says so, the vector
  // deleting destructor destroys that many), texts on the odd ones, so a reimplementation that leaves 0x0083d348 at 0
  // while deleting would record the text frees. [0, 0] is the first grow of a fresh list (no array).
  const GROW = [[3, 3], [10, 10], [0, 4], [0, 0], [1, 1], [7, 12], [2, 2], [5, 5], [0, 10], [4, 9], [1, 10], [9, 9]];
  function growLists(opts) {
    const out = {};
    const texts = [];
    for (let i = 0; i < 30; i++) texts.push(cstr('grow' + i));
    const pl = arrayPool(0x2000);
    out.ablk = pl.base;
    const rows = GROW.map(function (g, i) {
      const arr = g[1] === 0 ? null
        : mkArray(pl, g[1], function (k) { return [k + 1, k + 2, (k & 1) ? texts[(i * 2 + k) % 30] : null]; }, i * 0x100);
      return { arr: arr, cap: g[1], count: g[0], delay: 100 };
    });
    hotLists(out, rows);
    heapFree(out, { alloc: [0xa57f1], arenaSize: 0x8000, exhaust: 'null', failMask: opts.failMask || 0 });
    timerEnv(out, 'fake');
    freezeKeepTips(0);
    return out;
  }
  F.c3de_hl_grow = function () { return growLists({}); };
  // an allocation failure is only survivable with a count of 0 (the copy loop would write through NULL)
  F.c3de_hl_grow_fail = function () { return growLists({ failMask: 1 }); };

  // ---- startColorCycle: eight 0x80-byte tables (+8 + 16 * i: id, +0xc + 16 * i: Timer*), Timers 0x40 apart
  function colorTables() {
    const out = {};
    const timers = Memory.alloc(0x40 * 8);
    for (let i = 0; i < 8; i++) {
      const o = timers.add(0x40 * i);
      junk(o, 0x40, 0x79000000 + i * 0x100);
      // t10: non-zero (a real start) except timers 3 and 7, whose two targets stay zero (Timer::startMmPeriodic returns 7)
      writeTimer(o, [0, i === 3 ? 0x7103 : 0, (i === 3 || i === 7) ? 0 : 1, 0, [10, 1000, 13, 0x32, 49, 777, 0x31, 10][i], 5, 0]);
      out['tm' + i] = o;
    }
    out.timers = timers;
    const tblk = Memory.alloc(0x80 * 8);
    // ids per table (six entries); the sixth is only read when no earlier id matches nor is -1
    const IDS = [[10, 11, 12, 13, 14, 15], [20, 21, 22, 23, 24, 77], [30, -1, 32, 33, 34, 35], [40, 41, 42, 43, 44, 45],
                 [50, 51, 52, 53, 54, 55], [60, 61, 62, 63, 64, 65], [70, 71, -1, 73, 74, 75], [80, 81, 82, 83, 84, 85]];
    for (let t = 0; t < 8; t++) {
      const o = tblk.add(0x80 * t);
      junk(o, 0x80, 0x7a000000 + t * 0x100);
      for (let k = 0; k < 6; k++) {
        o.add(8 + 16 * k).writeU32(IDS[t][k] >>> 0);
        o.add(0xc + 16 * k).writePointer(out['tm' + ((t + k) % 8)]);
      }
      out['c' + t] = o;
    }
    out.tblk = tblk;
    timerEnv(out, 'fake');
    return out;
  }
  F.c3de_colors = colorTables;


  // ---- allocColorRange: eight 0x100-byte tables (records at this + 16 * i: +8 id, +0xc Timer*, +0x14 sample buffer),
  // twelve pre-existing Timers (vtable 0x004baea0, so slot 0 is the game's Timer::scalarDtor) and sample strings. The heap
  // stand-in carves the new buffer (malloc 0x004a504f) and the new Timer (operator new 0x004a57f1) from the arena and
  // records the frees (0x004a5007 / 0x004a4ffc); failMask bit 0 fails the malloc, bit 1 the Timer allocation.
  // Row: [six ids, six Timer indices (-1: none), buffers present].
  const CR_ROWS = [
    [[5, 6, 7, 8, 9, 0x30], [0, 1, 2, 3, 4, 5], 1],
    [[-1, -1, -1, -1, -1, -1], [-1, -1, -1, -1, -1, -1], 0],
    [[1, 2, -1, -1, -1, -1], [6, 7, -1, -1, -1, -1], 1],
    [[10, 11, 12, 13, 14, 15], [8, 9, 10, 11, 0, 1], 1],
    [[21, 22, 23, 24, 25, 26], [-1, -1, -1, -1, -1, -1], 0],
    [[31, 32, 33, 34, 35, 36], [2, 3, 4, 5, 6, 7], 0],
    [[51, 52, 53, 54, 55, 56], [-1, -1, -1, -1, -1, -1], 1],
    [[41, 42, 43, 44, 45, 46], [8, 9, 10, 11, 0, 1], 1],
  ];
  function colorRange(failMask) {
    const out = {};
    const tp = Memory.alloc(0x40 * 12);
    for (let i = 0; i < 12; i++) {
      const o = tp.add(0x40 * i);
      junk(o, 0x40, 0x7b000000 + i * 0x100);
      writeTimer(o, TROWS[i]);
      o.writeU32(0x004baea0);
      out['pt' + i] = o;
    }
    out.tpool = tp;
    const samples = [];
    for (let i = 0; i < 8; i++) samples.push(cstr('sample' + i));
    const blk = Memory.alloc(0x100 * CR_ROWS.length);
    CR_ROWS.forEach(function (r, t) {
      const o = blk.add(0x100 * t);
      junk(o, 0x100, 0x7c000000 + t * 0x100);
      for (let k = 0; k < 6; k++) {
        o.add(16 * k + 8).writeU32(r[0][k] >>> 0);
        o.add(16 * k + 0xc).writeU32(r[1][k] < 0 ? 0 : out['pt' + r[1][k]].toUInt32());
        o.add(16 * k + 0x14).writeU32(r[2] ? samples[(t + k) % 8].toUInt32() : 0);
      }
      out['d' + t] = o;
    });
    out.dblk = blk;
    heapFree(out, { alloc: [0xa504f, 0xa57f1], arenaSize: 0x4000, exhaust: 'null', failMask: failMask });
    timerEnv(out, 'fake');
    return out;
  }
  F.c3de_colorrange = function () { return colorRange(0); };
  F.c3de_colorrange_f1 = function () { return colorRange(1); };
  F.c3de_colorrange_f2 = function () { return colorRange(2); };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
