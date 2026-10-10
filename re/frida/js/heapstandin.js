// Heap stand-in for one A/B key (2026-10-09; realloc and scheduled failures added 2026-10-10 from c3cz's copy).
// Functions that call the CRT allocator directly (operator new/delete, malloc/free/realloc, jgld's jalloc/jfree) could not
// be A/B'd: the two arms got different blocks, and a free in the first arm released a block the second arm still used. For
// the length of one key this replaces the module's allocation entry points with:
//   - an ARENA allocator: blocks are carved from a fixture-owned arena at arena+0x10+cursor (16-byte aligned, filled
//     with `fill`); put [arena, 0, 0x10 + size] in the key's state regions and diff_hook restores it between the arms,
//     so both arms receive the SAME addresses;
//   - a RECORDING deallocator: each pointer handed to it is appended to the log (count at log+0, pointers from log+4);
//     nothing is freed, so the order and identity of the frees are compared state.
// Only calls made on the thread that installed the stand-in (the rpc thread that runs both arms) take these paths;
// every other thread (the game's main, render and sound threads) is forwarded to the real allocator. The stand-in
// removes itself from setTimeout(0), i.e. right after the rpc call that ran the key returns.
//
//   const h = __heapStandIn({ module: 'jgld.dll', alloc: [0x7f0f0], free: [0x7e6d0], arenaSize: 0x10000 });
//   fixture.arena = h.arena; fixture.freelog = h.log;      // state: [('$arena', 0, 0x10 + size), ('$freelog', 0, 0x404)]
//
// Arena header (all state, so both arms see the same sequence):
//   +0   cursor (bytes used)
//   +4   allocation call counter: every alloc / realloc call on the A/B thread, failed or not, counts
//   +8   fail mask: call k (0-based, k < 32) returns NULL when bit k is set          (opts.failMask, default 0)
//   +0xc fail size: a request of exactly this many bytes returns NULL               (opts.failSize, default none)
// A request that does not fit throws ("arena exhausted"), unless opts.exhaust === 'null', when it returns NULL like the
// CRT does for an impossible size (Vec_release's capacity 0 asks malloc for 0xffffffd8 bytes, c3cz).
//
// alloc / free / realloc entries are RVAs of cdecl functions; alloc's FIRST argument is the size, free's the pointer,
// realloc's (pointer, size). For an allocator with more arguments (debug _malloc_dbg-style) pass {rva, args: N}. An alloc
// entry may also carry:
//   sizeArgs: K   the block size is the PRODUCT of the first K arguments (calloc(num, size) is {args: 2, sizeArgs: 2})
//   fill: V       the byte this entry's blocks are filled with, overriding opts.fill (calloc zeroes: fill: 0)
// realloc(p, n): p NULL behaves as an allocation of n; n 0 records p in the free log and returns NULL (the VC6 CRT frees
// and returns NULL); otherwise a new arena block of n bytes (counted, and failable like any allocation: on failure the
// old block is untouched and NULL returned), min(old size, n) bytes copied, the old pointer recorded in the free log. The
// old size is known for blocks this stand-in carved; a block the fixture made itself must be declared with
// h.register(ptr, size) first, and an unknown block throws rather than copy a guessed length.
(function () {
  function resolve(module, rva) {
    const m = Process.getModuleByName(module);
    return m.base.add(rva);
  }
  // Addresses this module has replaced and not yet reverted. The revert runs from setTimeout(0), i.e.
  // after the rpc call that ran the key; when the next key's fixture runs before that timer fires,
  // Interceptor.replace would throw "already replaced this function" and the key would be recorded as
  // ERROR. Installing through here reverts a leftover replacement first, so the race is harmless.
  const live = {};
  function install(at, cb) {
    const k = at.toString();
    if (live[k]) { Interceptor.revert(at); delete live[k]; }
    Interceptor.replace(at, cb);
    live[k] = true;
  }
  function uninstall(at) {
    const k = at.toString();
    if (!live[k]) return;
    Interceptor.revert(at);
    delete live[k];
  }
  globalThis.__heapStandIn = function (opts) {
    const tid = Process.getCurrentThreadId();
    const arenaSize = opts.arenaSize || 0x10000;
    const fill = opts.fill === undefined ? 0xcd : opts.fill;
    const exhaustNull = opts.exhaust === 'null';
    const arena = __keepAlloc(arenaSize + 0x10);
    arena.writeU32(0);
    arena.add(4).writeU32(0);
    arena.add(8).writeU32((opts.failMask || 0) >>> 0);
    arena.add(0xc).writeU32(opts.failSize === undefined ? 0xffffffff : (opts.failSize >>> 0));
    const log = __keepAlloc(0x404);
    log.writeU32(0);
    const sizes = new Map();                       // block address string -> size, for realloc's copy
    const reverts = [];

    // One allocation on the A/B thread: counts the call, applies the schedule, carves the block (or NULL).
    function carve(size, byte) {
      const k = arena.add(4).readU32();
      arena.add(4).writeU32(k + 1);
      const mask = arena.add(8).readU32();
      if ((k < 32 && ((mask >>> k) & 1)) || size === arena.add(0xc).readU32()) return ptr(0);
      const cur = arena.readU32();
      const need = ((size + 15) & ~15) >>> 0;
      if (size > arenaSize || cur + need > arenaSize) {
        if (exhaustNull) return ptr(0);
        throw new Error('heap stand-in: arena of ' + arenaSize + ' bytes exhausted');
      }
      const p = arena.add(0x10 + cur);
      if (size) p.writeByteArray(new Array(size).fill(byte));
      arena.writeU32(cur + need);
      sizes.set(p.toString(), size);
      return p;
    }
    function recordFree(p) {
      const n = log.readU32();
      if (n < 0x100) { log.add(4 + 4 * n).writePointer(p); log.writeU32(n + 1); }
    }

    (opts.alloc || []).forEach(function (spec) {
      const rva = typeof spec === 'number' ? spec : spec.rva;
      const nargs = typeof spec === 'number' ? 1 : (spec.args || 1);
      const nsize = typeof spec === 'number' ? 1 : (spec.sizeArgs || 1);
      const byte = (typeof spec === 'number' || spec.fill === undefined) ? fill : spec.fill;
      const at = resolve(opts.module, rva);
      const argt = Array(nargs).fill('pointer');
      const orig = new NativeFunction(at, 'pointer', argt);
      const cb = new NativeCallback(function () {
        if (Process.getCurrentThreadId() !== tid) return orig.apply(null, arguments);
        let size = arguments[0].toUInt32();
        for (let i = 1; i < nsize; i++) size = (size * arguments[i].toUInt32()) >>> 0;
        return carve(size, byte);
      }, 'pointer', argt);
      __diffKeepAlive.push(cb, orig);
      install(at, cb);
      reverts.push(at);
    });
    (opts.free || []).forEach(function (spec) {
      const rva = typeof spec === 'number' ? spec : spec.rva;
      const nargs = typeof spec === 'number' ? 1 : (spec.args || 1);
      const at = resolve(opts.module, rva);
      const argt = Array(nargs).fill('pointer');
      const orig = new NativeFunction(at, 'void', argt);
      const cb = new NativeCallback(function () {
        if (Process.getCurrentThreadId() !== tid) { orig.apply(null, arguments); return; }
        recordFree(arguments[0]);
      }, 'void', argt);
      __diffKeepAlive.push(cb, orig);
      install(at, cb);
      reverts.push(at);
    });
    (opts.realloc || []).forEach(function (spec) {
      const rva = typeof spec === 'number' ? spec : spec.rva;
      const nargs = typeof spec === 'number' ? 2 : (spec.args || 2);
      const byte = (typeof spec === 'number' || spec.fill === undefined) ? fill : spec.fill;
      const at = resolve(opts.module, rva);
      const argt = Array(nargs).fill('pointer');
      const orig = new NativeFunction(at, 'pointer', argt);
      const cb = new NativeCallback(function () {
        if (Process.getCurrentThreadId() !== tid) return orig.apply(null, arguments);
        const old = arguments[0];
        const n = arguments[1].toUInt32();
        if (old.isNull()) return carve(n, byte);
        if (n === 0) { recordFree(old); return ptr(0); }
        const key = old.toString();
        if (!sizes.has(key)) throw new Error('heap stand-in: realloc of an undeclared block ' + key + ' (use h.register)');
        const p = carve(n, byte);
        if (p.isNull()) return p;
        const copy = Math.min(sizes.get(key), n);
        if (copy) p.writeByteArray(old.readByteArray(copy));
        recordFree(old);
        return p;
      }, 'pointer', argt);
      __diffKeepAlive.push(cb, orig);
      install(at, cb);
      reverts.push(at);
    });
    Interceptor.flush();
    setTimeout(function () { reverts.forEach(uninstall); Interceptor.flush(); }, 0);
    return { arena: arena, log: log, arenaSize: arenaSize,
             register: function (p, size) { sizes.set(ptr(p).toString(), size >>> 0); } };
  };
})();
