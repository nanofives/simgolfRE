// Heap stand-in for one A/B key (2026-10-09). Functions that call the CRT allocator directly (operator new/delete,
// malloc/free, jgld's jalloc/jfree) could not be A/B'd: the two arms got different blocks, and a free in the first arm
// released a block the second arm still used. For the length of one key this replaces the module's allocation entry
// points with:
//   - an ARENA allocator: blocks are carved from a fixture-owned arena at arena+0x10+cursor (16-byte aligned, filled
//     with `fill`), the cursor living at arena+0; put [arena, 0, size] in the key's state regions and diff_hook
//     restores the cursor between the arms, so both arms receive the SAME addresses;
//   - a RECORDING deallocator: each pointer handed to it is appended to the log (count at log+0, pointers from log+4);
//     nothing is freed, so the order and identity of the frees are compared state.
// Only calls made on the thread that installed the stand-in (the rpc thread that runs both arms) take these paths;
// every other thread (the game's main, render and sound threads) is forwarded to the real allocator, so the game keeps
// running normally and nothing has to be frozen for the heap's sake. The stand-in removes itself from setTimeout(0),
// i.e. right after the rpc call that ran the key returns (the pattern of the frozen-thread fixtures).
//
//   const h = __heapStandIn({ module: 'jgld.dll', alloc: [0x7f0f0], free: [0x7e6d0], arenaSize: 0x10000 });
//   fixture.arena = h.arena; fixture.freelog = h.log;      // state: [('$arena', 0, size), ('$freelog', 0, 0x404)]
//
// alloc / free entries are RVAs of cdecl functions whose FIRST argument is the size (alloc) or the pointer (free);
// for an allocator with more arguments (debug _malloc_dbg-style), pass {rva, args: N} so the callback forwards them.
// An entry may also carry:
//   sizeArgs: K   the block size is the PRODUCT of the first K arguments (calloc(num, size) is {args: 2, sizeArgs: 2})
//   fill: V       the byte this entry's blocks are filled with, overriding opts.fill (calloc zeroes: fill: 0)
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
    const arena = __keepAlloc(arenaSize + 0x10);
    arena.writeU32(0);
    const log = __keepAlloc(0x404);
    log.writeU32(0);
    const reverts = [];
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
        for (let i = 1; i < nsize; i++) size *= arguments[i].toUInt32();
        const cur = arena.readU32();
        const need = (size + 15) & ~15;
        if (cur + need > arenaSize) throw new Error('heap stand-in: arena of ' + arenaSize + ' bytes exhausted');
        const p = arena.add(0x10 + cur);
        if (size) p.writeByteArray(new Array(size).fill(byte));
        arena.writeU32(cur + need);
        return p;
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
        const n = log.readU32();
        if (n < 0x100) { log.add(4 + 4 * n).writePointer(arguments[0]); log.writeU32(n + 1); }
      }, 'void', argt);
      __diffKeepAlive.push(cb, orig);
      install(at, cb);
      reverts.push(at);
    });
    Interceptor.flush();
    setTimeout(function () { reverts.forEach(uninstall); Interceptor.flush(); }, 0);
    return { arena: arena, log: log, arenaSize: arenaSize };
  };
})();
