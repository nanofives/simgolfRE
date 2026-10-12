// Fixtures for batch c3cp (golf_clean.exe, round 22): the C++ static-initialization families, high half.
// Reimplementations: shim/src/re/c3cp.cpp. Registry: re/frida/registry.d/c3cp.py. Purposes: log/c3/c3cp_purpose.md.
//
// Every function here writes FIXED globals the running game owns (the Images, Surfaces, caches, lists and readers its
// static initializers build). One generic fixture serves all of them, keyed by the function's address and a variant:
//   1. it suspends the game's FIRST thread (Toolhelp order: the thread that created the window, the one that draws
//      from these objects), and saves every region the function writes;
//   2. it seeds the regions for the variant (the game's own bytes, a dword pattern, zeros; for the exit routines an
//      object built by the game's own constructor stub and then given handles, buffers and caches of ours);
//   3. it replaces, for the A/B thread only, _atexit 0x004a56d2 with a recording stand-in (the routine pushed is
//      appended to $atexitlog, count at +0; it returns the variant's value and registers nothing), and where the
//      function frees or allocates it installs heapstandin.js's __heapStandIn over golf_clean.exe's operator new
//      0x004a57f1 / malloc 0x004a504f (arena) and _free 0x004a5007 / operator delete 0x004a4ffc (recorded only);
//   4. where the code reaches the display object 0x0083ad50 (Surface_init 0x00473ae0 slot +0x94, Cache483070::reset
//      0x00483070 slot +0x90, the Image path's 0x00483da0 slot +0x150) it swaps that pointer for $sink, a display of
//      ours whose every slot is a recording stub, or for NULL;
//   5. from setTimeout(0), i.e. after diff_hook's rpc call has run both arms and restored the state regions, it
//      writes the saved bytes and the display pointer back, reverts the _atexit stand-in and resumes the thread (a
//      following fixture that runs before that timer runs the pending undo first).
// With the thread down the vectors call only exe code over the game's globals and the fixture's memory: no lock, no
// CRT heap (the stand-ins), no Win32 call. A key whose suspend fails throws, so it is recorded as ERROR instead of
// running against the live game.
(function () {
  const ATEXIT = ptr('0x004a56d2');
  const DISPLAY = ptr('0x0083ad50');
  const SURF = 0x2c;

  // [address]: k = kind (plain | disp | heap_new | heap_malloc | atexitdtor | cachedtor | register | store),
  // r = the regions the function writes (the registry's state regions minus the fixture blocks), sv = the regions to
  // save and restore when they differ from r, s = the constructor stub that builds the object for an exit routine,
  // n = the number of 0x2c-byte Surface members of that object.
  const T = {
    0x004058e0: { k: 'disp', r: [[0x005a7148, 0x2b4], [0x0083ad4c, 0x4]] },
    0x00405900: { k: 'register', r: [] },
    0x004061a0: { k: 'disp', r: [[0x00568d10, 0x2b4], [0x0083ad4c, 0x4]] },
    0x004061c0: { k: 'register', r: [] },
    0x004061e0: { k: 'disp', r: [[0x00587ae8, 0x2b4], [0x0083ad4c, 0x4]] },
    0x00406200: { k: 'register', r: [] },
    0x004378d0: { k: 'plain', r: [[0x005aa560, 0x58]] },
    0x00438590: { k: 'disp', r: [[0x005a9f78, 0x5dc], [0x0083ad4c, 0x4]] },
    0x0043cc00: { k: 'plain', r: [[0x0081ca10, 0x4150]] },
    0x0043cc60: { k: 'plain', r: [[0x005aa6f0, 0x58]] },
    0x0043cca0: { k: 'plain', r: [[0x005aa6d0, 0x1c]] },
    0x0043d4d0: { k: 'plain', r: [[0x00820e90, 0x28]] },
    0x0043d630: { k: 'disp', r: [[0x005aa778, 0x2b4], [0x0083ad4c, 0x4]] },
    0x00448150: { k: 'plain', r: [[0x0080d840, 0x7e90]] },
    0x004491b0: { k: 'plain', r: [[0x00820ed8, 0x28]] },
    0x00449200: { k: 'heap_new', r: [[0x00820f1c, 0x8]], sv: [[0x00820f18, 0xc]] },
    0x0044ac50: { k: 'plain', r: [[0x00821020, 0x8], [0x00821ec8, 0x68]] },
    0x0044ad10: { k: 'plain', r: [[0x00820f40, 0xd8]] },
    0x0044ad70: { k: 'plain', r: [[0x00821040, 0x28]] },
    0x0044adc0: { k: 'plain', r: [[0x008210c8, 0x54]] },
    0x0044adf0: { k: 'atexitdtor', r: [[0x008210c8, 0x58]], s: 0x0044add0, n: 2 },
    0x0044aeb0: { k: 'plain', r: [[0x00821c60, 0x54]] },
    0x0044aee0: { k: 'atexitdtor', r: [[0x00821c60, 0x58]], s: 0x0044aec0, n: 2 },
    0x0044aef0: { k: 'plain', r: [[0x00821bf8, 0x54]] },
    0x0044af20: { k: 'atexitdtor', r: [[0x00821bf8, 0x58]], s: 0x0044af00, n: 2 },
    0x0044af30: { k: 'plain', r: [[0x00821070, 0x54]] },
    0x0044af60: { k: 'atexitdtor', r: [[0x00821070, 0x58]], s: 0x0044af40, n: 2 },
    0x0044af70: { k: 'plain', r: [[0x00821200, 0x104]] },
    0x0044afa0: { k: 'atexitdtor', r: [[0x00821200, 0x108]], s: 0x0044af80, n: 6 },
    0x0044b0f0: { k: 'plain', r: [[0x008223c0, 0x104]] },
    0x0044b120: { k: 'atexitdtor', r: [[0x008223c0, 0x108]], s: 0x0044b100, n: 6 },
    0x0044b130: { k: 'plain', r: [[0x008222b8, 0x104]] },
    0x0044b160: { k: 'atexitdtor', r: [[0x008222b8, 0x108]], s: 0x0044b140, n: 6 },
    0x0044b170: { k: 'plain', r: [[0x00821308, 0x8ec]] },
    0x0044b1a0: { k: 'atexitdtor', r: [[0x00821308, 0x8f0]], s: 0x0044b180, n: 52 },
    0x0044b3a0: { k: 'plain', r: [[0x00821f48, 0x36c]] },
    0x0044b3d0: { k: 'atexitdtor', r: [[0x00821f48, 0x370]], s: 0x0044b3b0, n: 20 },
    0x0044b4b0: { k: 'plain', r: [[0x008224c8, 0x6b0]] },
    0x0044b4e0: { k: 'atexitdtor', r: [[0x008224c8, 0x6b4]], s: 0x0044b4c0, n: 39 },
    0x0044b650: { k: 'plain', r: [[0x00821120, 0xd8]] },
    0x0044b680: { k: 'atexitdtor', r: [[0x00821120, 0xdc]], s: 0x0044b660, n: 5 },
    0x0044b750: { k: 'plain', r: [[0x00821cb8, 0x20c]] },
    0x0044b780: { k: 'atexitdtor', r: [[0x00821cb8, 0x210]], s: 0x0044b760, n: 12 },
    0x0045ba50: { k: 'plain', r: [[0x00822cc8, 0x14]] },
    0x0045baa0: { k: 'plain', r: [[0x00822c98, 0x28]] },
    0x0046de30: { k: 'disp', r: [[0x00838f98, 0x2b4], [0x0083ad4c, 0x4]] },
    0x00473a60: { k: 'disp', r: [[0x00839348, 0x2b4], [0x0083ad4c, 0x4]] },
    0x00474a80: { k: 'plain', r: [[0x00839a68, 0x28]] },
    0x004799d0: { k: 'store', r: [[0x0083a4f8, 0x4]] },
    0x004799e0: { k: 'disp', r: [[0x0083a500, 0x2b4], [0x0083ad4c, 0x4]] },
    0x00479a30: { k: 'disp', r: [[0x0083a7b8, 0x2b4], [0x0083ad4c, 0x4]] },
    0x00482e70: { k: 'plain', r: [[0x0083acb0, 0x58]] },
    0x00482eb0: { k: 'plain', r: [[0x0083ac30, 0x58]] },
    0x00482ef0: { k: 'plain', r: [[0x00839650, 0x4], [0x0083ac88, 0x28]] },
    0x00482f20: { k: 'cachedtor', r: [[0x0083ac28, 0x1], [0x0083ac88, 0x28], [0x00839650, 0x4]], s: 0x00482f00 },
    0x004837a0: { k: 'plain', r: [[0x0083ad20, 0x8]] },
    0x00483dd0: { k: 'plain', r: [[0x0083ad80, 0x1e4]] },
    0x00483e10: { k: 'plain', r: [[0x0083ad58, 0x20]] },
    0x00483e50: { k: 'plain', r: [[0x0083af98, 0x20]] },
    0x00487d70: { k: 'heap_malloc', r: [[0x0083b000, 0x164]] },
    0x00487e00: { k: 'plain', r: [[0x0083b170, 0x480]] },
    0x0049bfe0: { k: 'disp', r: [[0x0083ad4c, 0x4], [0x008400c8, 0x5c8]] },
    0x0049d000: { k: 'plain', r: [[0x008406e8, 0x1c]] },
    0x0049d150: { k: 'plain', r: [[0x00840710, 0x28]] },
    0x0049d290: { k: 'plain', r: [[0x008407c0, 0x28]] },
    0x0049d2f0: { k: 'plain', r: [[0x00840790, 0x28]] },
    0x0049d350: { k: 'plain', r: [[0x008407f0, 0x28]] },
    0x0049d480: { k: 'plain', r: [[0x00840830, 0x28]] },
    0x0049d4e0: { k: 'plain', r: [[0x00840890, 0x28]] },
    0x0049d540: { k: 'plain', r: [[0x00840860, 0x28]] },
    0x0049eaf0: { k: 'plain', r: [[0x008408c8, 0x28]] },
    0x0049eb50: { k: 'plain', r: [[0x008408f8, 0x28]] },
  };

  // ------------------------------------------------------------------ the first thread
  let pending = null;   // the previous key's undo when its timer has not fired yet
  function runPending() { if (pending) { const p = pending; pending = null; p(); } }
  // Shared frozen-thread helper (re/frida/js/freeze.js): safe-point suspend; returns an idempotent release().
  function freezeFirstThread() {
    return __freeze.thread();
  }

  // ------------------------------------------------------------------ shared code: recording stubs
  // __thiscall recording stub (c3cd's sinkStub): appends `tag` and its n dword arguments at ecx + [ecx+4], advances
  // the cursor [ecx+4], returns its first stack argument and pops its n arguments (`ret 4n`, plain `ret` for none).
  function sinkStub(tag, n) {
    const b = [0x8B, 0x41, 0x04, 0x03, 0xC1,
               0xC7, 0x00, tag & 0xff, (tag >>> 8) & 0xff, (tag >>> 16) & 0xff, (tag >>> 24) & 0xff];
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 * i);
    b.push(0x8B, 0x51, 0x04, 0x83, 0xC2, 4 * (n + 1), 0x89, 0x51, 0x04);
    b.push(0x8B, 0x44, 0x24, 0x04);
    if (n) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }
  let shared = null;
  function getShared() {
    if (shared) return shared;
    const page = Process.pageSize;
    const code = Memory.alloc(page * 4);
    Memory.protect(code, page * 4, 'rwx');
    // The display: 0x60 slots, each a stub with its own tag 0xD5000000 | slot offset. The argument counts come from
    // the call sites: slot +0x20 `push eax` 0x00486eec, +0x90 `push eax` 0x0048308a, +0x94 `push eax` 0x00473af9,
    // +0x150 no push before 0x00483dbb; every other slot takes none (none of them is called by this batch's code).
    const dispVt = Memory.alloc(0x60 * 4);
    const nargs = { 0x20: 1, 0x90: 1, 0x94: 1, 0x150: 0 };
    let off = 0;
    for (let s = 0; s < 0x60; s++) {
      const slot = s * 4;
      const b = sinkStub((0xD5000000 | slot) >>> 0, nargs[slot] || 0);
      code.add(off).writeByteArray(b);
      dispVt.add(slot).writePointer(code.add(off));
      off += (b.length + 15) & ~15;
    }
    // The caches' entry objects: slot 0 is the deleting destructor Cache483070::reset calls with `push 1`
    // (0x004830ab / 0x004830ad): one argument.
    const objVt = Memory.alloc(0x10);
    code.add(off).writeByteArray(sinkStub(0xDE1E7E00, 1));
    for (let s = 0; s < 4; s++) objVt.add(s * 4).writePointer(code.add(off));
    shared = { code: code, dispVt: dispVt, objVt: objVt };
    return shared;
  }

  // ------------------------------------------------------------------ seeding
  const pattern = (p, n, seed) => {
    for (let i = 0; i + 4 <= n; i += 4) p.add(i).writeU32((seed ^ (0x00C30000 + p.add(i).toUInt32())) >>> 0);
  };
  const zero = (p, n) => { for (let i = 0; i < n; i++) p.add(i).writeU8(0); };

  // The exit routines' objects: built by the game's constructor stub (cdecl, no arguments), then seeded per member k.
  //   full / nodisplay: handle +4 = 0x0d000001 + k, buffer +8 = 0x7e000000 + k * 0x100 (never dereferenced: _free
  //                     only records it), cache +0xc = cache k for the first three members, a pattern over
  //                     +0x10..+0x24
  //   mixed:            member k % 4 == 1 gets only the handle, == 2 only the buffer, == 3 only a cache (cache k/4
  //                     while k/4 < 3), == 0 nothing; the pattern over +0x10..+0x24 for all
  //   empty:            as constructed
  function seedSurfaces(e, variant, caches) {
    new NativeFunction(ptr(e.s), 'int', [])();
    const g = ptr(e.r[0][0]);
    for (let k = 0; k < e.n; k++) {
      const m = g.add(k * SURF);
      if (variant === 'empty') continue;
      pattern(m.add(0x10), 0x18, 0x51D00000 + k);
      const handle = 0x0d000001 + k, buf = 0x7e000000 + k * 0x100;
      if (variant === 'full' || variant === 'nodisplay') {
        m.add(4).writeU32(handle); m.add(8).writeU32(buf);
        m.add(0xc).writePointer(k < 3 ? caches[k] : ptr(0));
      } else if (variant === 'mixed') {
        if (k % 4 === 1) m.add(4).writeU32(handle);
        else if (k % 4 === 2) m.add(8).writeU32(buf);
        else if (k % 4 === 3 && (k >> 2) < 3) m.add(0xc).writePointer(caches[k >> 2]);
      }
    }
  }
  // Three caches of 0x58 bytes (the layout Cache482fd0::ctor 0x00482fd0 writes: +0 vtable, +4 handle, five entries of
  // 0x10 from +8: +0 dword, +4 object, +8/+9 bytes, +0xc buffer) at $caches + 0, 0x100, 0x200, and up to ten entry
  // objects of 0x40 (+0 vtable, +4 record cursor) from $caches + 0x400.
  //   cache 0: handle, every entry with an object and a buffer; cache 1: handle, objects in even entries, buffers in
  //   odd ones; cache 2: no handle, entries empty.
  function buildCaches(blk) {
    const sh = getShared();
    pattern(blk, 0x800, 0xCAC00000);
    const caches = [blk, blk.add(0x100), blk.add(0x200)];
    let j = 0;
    for (let c = 0; c < 3; c++) {
      const C = caches[c];
      C.writeU32(0xCAC0E000 + c);
      C.add(4).writeU32(c < 2 ? 0x0CA00001 + c : 0);
      for (let k = 0; k < 5; k++) {
        const E = C.add(8 + k * 0x10);
        E.writeU32(0x11110000 + c * 0x10 + k);
        let obj = ptr(0);
        if (c === 0 || (c === 1 && k % 2 === 0)) {
          obj = blk.add(0x400 + j * 0x40); j++;
          zero(obj, 0x40); obj.writePointer(sh.objVt); obj.add(4).writeU32(8);
        }
        E.add(4).writePointer(obj);
        E.add(8).writeU32(0x5A5A0000 + k);
        E.add(0xc).writeU32((c === 0 || (c === 1 && k % 2 === 1)) ? 0x7F000000 + c * 0x100 + k * 0x10 : 0);
      }
    }
    return caches;
  }

  // ------------------------------------------------------------------ the _atexit stand-in
  // For the A/B thread only: the routine is appended to the log (count at +0, up to 0x10 pointers from +4) and `ret`
  // is returned; any other thread reaches the real _atexit (a NativeFunction of a replaced address calls the original).
  function atexitStandIn(log, ret) {
    const tid = Process.getCurrentThreadId();
    const orig = new NativeFunction(ATEXIT, 'int', ['pointer']);
    const cb = new NativeCallback(function (fn) {
      if (Process.getCurrentThreadId() !== tid) return orig(fn);
      const n = log.readU32();
      if (n < 0x10) { log.add(4 + 4 * n).writePointer(fn); log.writeU32(n + 1); }
      return ret;
    }, 'int', ['pointer']);
    __diffKeepAlive.push(cb, orig);
    try { Interceptor.revert(ATEXIT); } catch (err) { /* not replaced */ }
    Interceptor.replace(ATEXIT, cb);
    Interceptor.flush();
    return function () { Interceptor.revert(ATEXIT); Interceptor.flush(); };
  }

  // ------------------------------------------------------------------ one key
  const ATEXIT_RET = { live: 0, pat: -1, zero: 0, ok: 0, fail: -1 };
  const STORE_SEEDS = [0x00000000, 0x004BA87C, 0xFFFFFFFF, 0x00000001, 0x80000000, 0x12345678, 0x004BA878, 0x004BA880,
                       0x7FFFFFFF, 0x0083A4F8];
  const GUARD = { clear: 0x00, guard: 0x01, high: 0xFE, guardhigh: 0xFF };

  function env(addr, variant) {
    runPending();
    const e = T[addr];
    const out = {};
    // everything is allocated before the suspend
    const sh = getShared();
    out.atexitlog = Memory.alloc(0x44); zero(out.atexitlog, 0x44);
    const sink = Memory.alloc(0x800); zero(sink, 0x800); sink.writePointer(sh.dispVt); sink.add(4).writeU32(8);
    out.sink = sink;
    let caches = null;
    if (e.k === 'atexitdtor') { out.caches = Memory.alloc(0x800); caches = buildCaches(out.caches); }
    const saveRegs = (e.sv || e.r).map((r) => [ptr(r[0]), r[1]]);

    const resume = freezeFirstThread();
    const saved = saveRegs.map(([p, n]) => p.readByteArray(n));
    const dispSaved = DISPLAY.readPointer();
    const undos = [];
    const undo = function () {
      undos.forEach((u) => u());
      saveRegs.forEach(([p, n], i) => p.writeByteArray(saved[i]));
      DISPLAY.writePointer(dispSaved);
      resume();
    };
    try {
      if (e.k === 'register' || e.k === 'plain' || e.k === 'disp' || e.k === 'heap_new' || e.k === 'heap_malloc') {
        if (variant === 'pat') e.r.forEach((r) => pattern(ptr(r[0]), r[1], 0x5EED0000));
        if (variant === 'zero') e.r.forEach((r) => zero(ptr(r[0]), r[1]));
        undos.push(atexitStandIn(out.atexitlog, ATEXIT_RET[variant]));
        if (e.k === 'disp') DISPLAY.writePointer(variant === 'zero' ? ptr(0) : __recDisplay.guard(sink));
        if (e.k === 'heap_new' || e.k === 'heap_malloc') {
          const h = __heapStandIn({ module: 'golf_clean.exe', alloc: [e.k === 'heap_new' ? 0xa57f1 : 0xa504f],
                                    free: [0xa5007, 0xa4ffc], arenaSize: 0x1000, fill: 0xcd });
          out.arena = h.arena; out.freelog = h.log;
        }
      } else if (e.k === 'atexitdtor') {
        seedSurfaces(e, variant, caches);
        const h = __heapStandIn({ module: 'golf_clean.exe', free: [0xa5007, 0xa4ffc] });
        out.freelog = h.log;
        DISPLAY.writePointer(variant === 'nodisplay' ? ptr(0) : __recDisplay.guard(sink));
      } else if (e.k === 'cachedtor') {
        new NativeFunction(ptr(e.s), 'int', [])();
        ptr('0x0083ac28').writeU8(GUARD[variant]);
      } else if (e.k === 'store') {
        ptr(e.r[0][0]).writeU32(STORE_SEEDS[parseInt(variant.slice(1), 10)]);
      }
    } catch (err) {
      undo();
      throw err;
    }
    // run at the shared flush (freeze.js), before the frozen thread is resumed (undo calls resume() itself)
    __freeze.atFlush(undo);
    out.obj = sink;
    return out;
  }

  const V = { plain: ['live', 'pat', 'zero'], disp: ['live', 'pat', 'zero'], heap_new: ['live', 'pat', 'zero'],
              heap_malloc: ['live', 'pat', 'zero'], atexitdtor: ['full', 'mixed', 'nodisplay', 'empty'],
              cachedtor: ['clear', 'guard', 'high', 'guardhigh'], register: ['ok', 'fail'],
              store: ['s0', 's1', 's2', 's3', 's4', 's5', 's6', 's7', 's8', 's9'] };
  const fx = {};
  Object.keys(T).forEach(function (a) {
    const hex = ('00000000' + parseInt(a, 10).toString(16)).slice(-8);
    V[T[a].k].forEach(function (v) { fx['c3cp_' + hex + '_' + v] = function () { return env(a, v); }; });
  });
  Object.assign(globalThis.DIFF_FIXTURES, fx);
})();
