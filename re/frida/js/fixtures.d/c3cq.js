// Fixtures for batch c3cq (round 22, golf_clean.exe CRT static-initialisation rows).
// Reimplementations: shim/src/re/c3cq.cpp; registry: re/frida/registry.d/c3cq.py; Purposes and branch tables:
// log/c3/c3cq_purpose.md. This file is assembled by log/c3/c3cq_gen.py from log/c3/c3cq_manual/fx_*.js plus the
// generated table of global objects.
//
// Two kinds of fixture:
//   - c3cq_onexit: the atexit registrations append to the CRT exit table. The table pointers __onexitend 0x0084216c
//     and __onexitbegin 0x00842170 and the slot at the table end are the key's state regions, so diff_hook puts them
//     back between the arms and after the run. The fixture only makes sure the heap block holding the table has room
//     for one more entry (it grows it with the CRT's own realloc 0x004a6980 if _msize 0x004a9054 says it is full),
//     so neither arm reallocates. No thread is suspended: _onexit takes the exit lock and _msize the heap lock.
//   - frozen globals: the static constructors write live global objects the game's first thread (the one that owns
//     the window and draws) reads. The fixture suspends that thread, saves the bytes it is about to change, and fills
//     the object with 0xa5 so that every store the constructor makes is visible as a state change; a setTimeout(0)
//     callback (it runs only after the rpc call that drives the key's vectors has returned, Frida's JS runtime being
//     single-threaded) writes the saved bytes back, newest save first, and only then resumes the thread. Everything
//     the vectors call while the thread is down is plain stores (constructors, the vector iterator) or a heap
//     stand-in path: no lock, no allocation, no Win32 call.

// The frozen-thread method these fixtures introduced (exclusive-scheduled Suspend/GetContext/Resume, safe point in
// golf_clean.exe code, newest-first restore, synchronous flush before the next key) now lives in the shared helper
// re/frida/js/freeze.js; __c3cq keeps the names the fixtures below use.
globalThis.__c3cq = { begin: __freeze.begin, schedule: __freeze.schedule, freezeMain: function () { __freeze.thread(); },
                      save: __freeze.save, fill: __freeze.fill, setU32: __freeze.setU32 };

Object.assign(globalThis.DIFF_FIXTURES, {
  // atexit registrations: room for one more entry in the exit table, the slot at its end returned as $slot.
  c3cq_onexit() {
    __c3cq.begin();
    const END = ptr(0x0084216c), BEGIN = ptr(0x00842170);
    const msize = new NativeFunction(ptr(0x004a9054), 'uint32', ['pointer']);
    const realloc = new NativeFunction(ptr(0x004a6980), 'pointer', ['pointer', 'uint32']);
    const b = BEGIN.readPointer(), e = END.readPointer();
    const used = e.sub(b).toUInt32();
    if (msize(b) < used + 8) {
      // the same growth _onexit itself performs at 0x004a567d..0x004a56b4 (it adds 0x10 bytes), done once here
      const nb = realloc(b, used + 0x40);
      if (nb.isNull()) throw new Error('c3cq_onexit: realloc failed');
      BEGIN.writePointer(nb);
      END.writePointer(nb.add(used));
    }
    return { slot: END.readPointer(), used: used };
  },
});
  const C3CQ_GLOBALS = [
    [0x00519928, 0x8],
    [0x00519fd8, 0x8],
    [0x0051b320, 0x8],
    [0x0051b360, 0x8],
    [0x00519948, 0x8],
    [0x00519a40, 0x8],
    [0x0051a028, 0x8],
    [0x0051b340, 0x8],
    [0x00519968, 0x8],
    [0x005199c8, 0x58],
    [0x00519a20, 0x8],
    [0x0051a008, 0x8],
    [0x00519988, 0x8],
    [0x005199a8, 0x8],
    [0x0051a048, 0x8],
    [0x005aa560, 0x58],
    [0x005aa6f0, 0x58],
    [0x005aa6d0, 0x1c],
    [0x00820e90, 0x28],
    [0x00820ed8, 0x28],
    [0x00821040, 0x28],
    [0x008210c8, 0x58],
    [0x00821c60, 0x58],
    [0x00821bf8, 0x58],
    [0x00821070, 0x58],
    [0x00821200, 0x108],
    [0x008223c0, 0x108],
    [0x008222b8, 0x108],
    [0x00821308, 0x8f0],
    [0x00821f48, 0x370],
    [0x008224c8, 0x6b4],
    [0x00821120, 0xdc],
    [0x00821cb8, 0x210],
    [0x00822cc8, 0x14],
    [0x00822c98, 0x28],
    [0x0083acb0, 0x58],
    [0x0083ac30, 0x58],
    [0x0083ad80, 0x1e4],
    [0x0083ad58, 0x20],
    [0x0083af98, 0x20],
    [0x0081ca10, 0x4150],
    [0x00820f40, 0xdc],
    [0x0083b170, 0x480],
  ];

// One fixture per global object / array written by a static constructor (generated table above).
Object.assign(globalThis.DIFF_FIXTURES, (function () {
  const out = {};
  C3CQ_GLOBALS.forEach(function (row) {
    const g = row[0], n = row[1];
    out['c3cq_g' + g.toString(16).padStart(6, '0')] = function () {
      __c3cq.begin();
      __c3cq.freezeMain();
      __c3cq.fill(ptr(g), n, 0xa5);
      return { obj: ptr(g) };
    };
  });
  return out;
})());

(function () {
  const EXE = 'golf_clean.exe';
  const HEAP_G = ptr(0x00839650);

  // The address of this thread's TLS slot for index idx: TEB+0xe10 for the first 64, TlsExpansionSlots (TEB+0xf94)
  // after that. The TEB comes from fs:[0x18] through a three-instruction stub.
  let tebFn = null;
  function teb() {
    if (!tebFn) {
      const code = Memory.alloc(Process.pageSize);
      code.writeByteArray([0x64, 0xa1, 0x18, 0x00, 0x00, 0x00, 0xc3]);   // mov eax, fs:[0x18]; ret
      Memory.protect(code, Process.pageSize, 'rwx');
      __diffKeepAlive.push(code);
      tebFn = new NativeFunction(code, 'pointer', []);
    }
    return tebFn();
  }
  function tlsSlot(idx) {
    const t = teb();
    return idx < 64 ? t.add(0xe10 + 4 * idx) : t.add(0xf94).readPointer().add(4 * (idx - 64));
  }
  // __getptd 0x004a8e53 reads the index from 0x004e4bf0 (0x004a8e5b).
  function ptdSlot() { return tlsSlot(ptr(0x004e4bf0).readU32()); }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // errnoPtr / doserrnoPtr with a per-thread block already installed: this thread's slot points at a fixture
    // block (0x74 bytes, the size __getptd allocates at 0x004a8e6f), so __getptd returns it at 0x004a8eb5 without
    // allocating. Results are reported relative to the block ($obj).
    c3cq_ptd_installed() {
      __c3cq.begin();
      const blk = __keepAlloc(0x80);
      blk.writeByteArray(new Array(0x80).fill(0x5a));
      const slot = ptdSlot();
      __c3cq.save(slot, 4);
      slot.writePointer(blk);
      return { obj: blk, tls: slot };
    },
    // ... and with this thread's slot empty: __getptd takes its first-call path (0x004a8e6d), calloc(1, 0x74)
    // 0x004a8e73 - here the heap stand-in's arena (calloc's two arguments multiply, zero fill) - TlsSetValue
    // 0x004a8e87 into the slot, __initptd 0x004a8e92, GetCurrentThreadId into +0 (0x004a8ea2) and -1 into +4
    // (0x004a8e9e). The arena and the slot are state; results are relative to the arena ($obj).
    c3cq_ptd_fresh() {
      __c3cq.begin();
      const h = __heapStandIn({ module: EXE, alloc: [{ rva: 0xab534, args: 2, sizeArgs: 2, fill: 0 }],
                                arenaSize: 0x100 });
      const slot = ptdSlot();
      __c3cq.save(slot, 4);
      slot.writePointer(ptr(0));
      return { obj: h.arena, arena: h.arena, tls: slot };
    },

    // initFpControl: the six-slot table 0x004e4b00 filled with zeros / 0xa5 / its own six values rotated by one,
    // with the first thread suspended (the float paths of printf read the table).
    c3cq_fp_zero() { __c3cq.begin(); __c3cq.freezeMain(); __c3cq.fill(ptr(0x004e4b00), 0x18, 0x00); return {}; },
    c3cq_fp_a5() { __c3cq.begin(); __c3cq.freezeMain(); __c3cq.fill(ptr(0x004e4b00), 0x18, 0xa5); return {}; },
    c3cq_fp_rot() {
      __c3cq.begin();
      __c3cq.freezeMain();
      const t = ptr(0x004e4b00);
      const v = [0, 1, 2, 3, 4, 5].map((i) => t.add(4 * i).readU32());
      __c3cq.save(t, 0x18);
      for (let i = 0; i < 6; i++) t.add(4 * i).writeU32(v[(i + 1) % 6]);
      return {};
    },

    // unwindCleanup_4021a0: twelve 8-byte guard objects (vtable dword, saved value) and 0x00839650 set to a value
    // none of them holds, first thread suspended (it reads 0x00839650).
    c3cq_guard() {
      __c3cq.begin();
      __c3cq.freezeMain();
      __c3cq.setU32(HEAP_G, 0x5a5a0003);
      const saved = [0, 1, 0x00839650, 0xffffffff, 0x80000000, 0x7fffffff, 0x12345678, 0x004ba278, 0x00400000,
                     0xcdcdcdcd, 0x5a5a0003, 0x00010000];
      const fx = {};
      saved.forEach(function (v, i) {
        const o = __keepAlloc(8);
        o.writeU32(0xa5a5a5a5 - i);
        o.add(4).writeU32(v);
        fx['o' + i] = o;
      });
      return fx;
    },
  });

  // unwindFree_4021b0: the CRT free 0x004a5007 only records its argument (heap stand-in), the six blocks are
  // fixture memory that is never really freed, and 0x00839650 is 0 (_g0) or a nonzero value (_gset).
  // Heap keys: begin(), then schedule our flush BEFORE __heapStandIn schedules its revert, so the first thread is
  // resumed while the replacement is still installed and the revert runs with it running (the regime c3cn proved:
  // foreign threads are forwarded to the real allocator), then suspend.
  function freeFixture(g) {
    __c3cq.begin();
    __c3cq.schedule();
    const h = __heapStandIn({ module: EXE, free: [0xa5007] });
    __c3cq.freezeMain();
    __c3cq.setU32(HEAP_G, g);
    const fx = { freelog: h.log };
    for (let i = 0; i < 6; i++) fx['p' + i] = __keepAlloc(0x20);
    return fx;
  }
  // The global Reader 0x0083b000: fclose 0x004a609f only records its argument; +0x158 holds a fixture block (_open)
  // or 0 (_closed).
  function readerCloseFixture(open) {
    __c3cq.begin();
    __c3cq.schedule();
    const h = __heapStandIn({ module: EXE, free: [0xa609f] });
    __c3cq.freezeMain();
    __c3cq.setU32(ptr(0x0083b158), open ? __keepAlloc(0x40).toUInt32() : 0);
    return { freelog: h.log };
  }
  // Cache482f60::ctor on 0x0083ac88: the object filled with 0xa5 and 0x00839650 = 0 (_g0) or nonzero (_gset).
  function cacheFixture(g) {
    __c3cq.begin();
    __c3cq.freezeMain();
    __c3cq.fill(ptr(0x0083ac88), 0x28, 0xa5);
    __c3cq.setU32(HEAP_G, g);
    return { obj: ptr(0x0083ac88) };
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3cq_free_g0() { return freeFixture(0); },
    c3cq_free_gset() { return freeFixture(0x5a5a0001); },
    c3cq_reader_open() { return readerCloseFixture(true); },
    c3cq_reader_closed() { return readerCloseFixture(false); },
    // initGlobalReader: malloc 0x004a504f carves from the arena, free / fclose only record; the Reader is filled
    // with 0xa5.
    c3cq_reader_init() {
      __c3cq.begin();
      __c3cq.schedule();
      const h = __heapStandIn({ module: EXE, alloc: [0xa504f], free: [0xa5007, 0xa609f], arenaSize: 0x1000 });
      __c3cq.freezeMain();
      __c3cq.fill(ptr(0x0083b000), 0x164, 0xa5);
      return { obj: ptr(0x0083b000), arena: h.arena, freelog: h.log };
    },
    c3cq_cache_g0() { return cacheFixture(0); },
    c3cq_cache_gset() { return cacheFixture(0x5a5a0002); },
  });
})();
