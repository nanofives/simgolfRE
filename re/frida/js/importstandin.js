// Import stand-in for one A/B key (batch c3cy, 2026-10-10). The Win32 counterpart of heapstandin.js. Functions whose
// only obstacle was a Win32 import (CloseHandle, UnmapViewOfFile, timeSetEvent, GetSystemMetrics, DeleteObject...)
// could not be A/B'd: the original arm would act on the real system with fixture values (close a handle number that
// belongs to someone else, start a real timer), the second arm would see what the first arm did, and the import's
// result (a clock, a key state) differs between the arms. For the length of one key this replaces the listed imports
// OF ONE MODULE with recording stubs:
//   - the stub's result is the fixture's: a constant `ret`, an array `ret: [...]` indexed by that import's call number
//     in this arm, or `impl(args, ctx)`;
//   - every call is appended to a LOG that is meant to be a state region: diff_hook restores it between the arms, so
//     both arms start from call number 0 and see the same results, and the call sequence (which import, in which
//     order, with which arguments) is compared state. A body that skips a call, passes a wrong argument or makes an
//     extra call reads RED on state alone even when its return value is right (log/c3/alt/c3cy_negctl.py).
//
//   const s = __importStandIn({ module: 'golf_clean.exe', imports: {
//       'KERNEL32.dll!CloseHandle':     { ret: 1 },                     // record every argument (the default)
//       'KERNEL32.dll!FindFirstFileA':  { ret: -1, record: [0] },       // arg 1 is a stack buffer: record arg 0 only
//       'USER32.dll!GetSystemMetrics':  { impl: (a) => a[0] * 3 },      // computed result, every argument recorded
//       'WINMM.dll!timeGetTime':        { ret: [1000, 1250] },          // first call 1000, second 1250, then 1250
//   }});
//   fixture.implog = s.log;     // state: [('$implog', 0, s.logSize)]
//
// ONE CALL PER KEY. A call first reverts every patch still installed (a previous key's revert timer may not have fired
// when the next key's fixture runs), so a second call in the same fixture would undo the first. To stand in for
// imports of several modules in one key, prefix the key with the module: 'jgld.dll:KERNEL32.dll!CloseHandle' (keys
// without a prefix belong to opts.module). A first live check made two calls and found every golf_clean.exe counter
// at 0 for that reason (log/c3/alt/c3cy_live_check.py).
//
// HOW (and why the IAT, not Interceptor.replace on the API): the stand-in rewrites the module's IMPORT ADDRESS TABLE
// slot. golf_clean.exe and jgld.dll call imports as `call dword ptr [slot]` (golf 0x00492e8c `call [0x4ba0d0]`, jgld
// 0x10002e3f `call [0x1012c430]`) or load the slot into a register first (golf 0x004884e1 `mov esi, [0x4ba1c8]`), so
// only that module's code sees the stand-in. Interceptor.replace on the API body would also catch every other caller
// ON THE SAME THREAD: freeze.js's CloseHandle/SuspendThread, Frida's own runtime, the shim (whose imports are its own),
// and a stub that records the agent's own CloseHandle would put the agent's traffic into the compared log.
//   The slot points at a small NATIVE stub, not straight at a JS callback:
//       mov eax, fs:[0x24]       ; current thread id (TEB+0x24)
//       cmp eax, <installer tid>
//       jne forward
//       jmp <NativeCallback>     ; A/B thread only: JS records and returns the fixture's result
//     forward:
//       lock inc dword [counter] ; forwarded calls, for the live check
//       jmp <previous slot value>
//   so every other thread (the game's main, render, sound and timer threads) jumps to the real API without entering
//   JS or taking Frida's JS lock (eax is caller-clobbered at any call, and the stub leaves the stack untouched). That
//   keeps the stand-in safe next to __freeze: a frozen first thread can never be holding the JS lock inside a stub.
//   Only the thread that called __importStandIn (the rpc thread that runs both arms) takes the JS path.
//
// RULES for the reimplementation under test: it must call each import through the SAME IAT slot the original uses
// (`(*(Fn*)0x004ba0cc)(h)` in the exe, jgld base + slot RVA in jgld.dll), never through the shim's own import: the shim
// is another module and would reach the real API with fixture values. The missing-call negative control catches a
// body that does. Code that reaches an API through a pointer obtained from GetProcAddress is not covered.
//
// Spec per import ('DLL!Name', DLL matched case-insensitively against the module's import descriptors):
//   args: N        stack arguments (the stub's callback must pop exactly that many for stdcall; jgld's debug code
//                  checks esp after every import call with __chkesp 0x1007e780, so a wrong count faults there).
//                  Known APIs are in ARGS below; any other needs `args`.
//   abi:  'stdcall' (default) | 'mscdecl'
//   ret:  number | NativePointer | array (indexed by call number, the last entry repeating)   default 0
//   impl: function (args, ctx) -> number | NativePointer; args are NativePointers; ctx = {call, orig, record, log}
//         (ctx.orig(...args) calls the real API; ctx.record(values) replaces this call's recorded values)
//   record: 'all' (default) | number of leading arguments | array of argument indices | false (log the call only)
//   forward: true  call the real API and return its result (still recorded): for a harmless, deterministic import
//
// LOG layout (logSize bytes, default 0x400, zero-filled at install):
//   +0x00 u32 records written        +0x04 u32 byte cursor into the records area
//   +0x08 u32 calls not recorded (records area full)                    +0x0c reserved
//   +0x10 u32 per-import call counters, in the order the imports are listed (up to 32)
//   +0x90 records: u32 import index, u32 value count, u32 values...
// The stand-in reverts itself from setTimeout(0), right after the rpc call that ran the key returns (the heap stand-in's
// pattern); a leftover patch from the previous key is reverted before a new one is installed.
(function () {
  // stack argument counts of the APIs used so far (all stdcall)
  const ARGS = {
    CloseHandle: 1, UnmapViewOfFile: 1, CreateFileA: 7, CreateFileMappingA: 6, MapViewOfFile: 5, GetFileSize: 2,
    GetLastError: 0, FindFirstFileA: 2, FindClose: 1, SetFilePointer: 4, SetEndOfFile: 1, OutputDebugStringA: 1,
    EnterCriticalSection: 1, LeaveCriticalSection: 1,
    timeGetTime: 0, timeSetEvent: 5, timeKillEvent: 1,
    GetSystemMetrics: 1, GetAsyncKeyState: 1, GetKeyState: 1, ClipCursor: 1, SetTimer: 4, KillTimer: 2,
    MessageBeep: 1, PostMessageA: 4, MapVirtualKeyA: 2, LoadCursorA: 2, DestroyCursor: 1, GetKeyboardState: 1, keybd_event: 4,
    DeleteObject: 1, RemoveFontResourceA: 1, AddFontResourceA: 1, TextOutA: 5,
  };
  const HDR = 0x90, MAXIMP = 32;
  // slot address (string) -> { slot, prev, prot } for patches not yet reverted
  const live = {};

  function unpatch(k) {
    const e = live[k];
    if (!e) return;
    delete live[k];
    Memory.protect(e.slot, 4, 'rw-');
    e.slot.writePointer(e.prev);
    Memory.protect(e.slot, 4, e.prot);
  }
  function flush() { Object.keys(live).forEach(unpatch); }

  // The IAT slot of dll!name, read from the module's own import descriptors in memory: entry i of the name table
  // (OriginalFirstThunk) names slot i of the address table (FirstThunk). Frida's Module.enumerateImports() is not
  // used: on golf_clean.exe (unwrapped by Safedisc2Cleaner) it returned address undefined and, as `slot`, the first
  // slot of each DLL's table for every import (KERNEL32's CloseHandle and FindFirstFileA both at 0x4ba000; the real
  // slots are 0x4ba0cc and 0x4ba090), so the first version of this tool patched the wrong dwords
  // (log/c3/alt/c3cy_probe.py, 2026-10-10).
  function findSlot(module, dll, name) {
    const base = Process.getModuleByName(module).base;
    const nt = base.add(base.add(0x3c).readU32());
    const impRva = nt.add(0x80).readU32();                         // OptionalHeader.DataDirectory[1] (PE32)
    for (let d = base.add(impRva); ; d = d.add(20)) {
      const oft = d.readU32(), nameRva = d.add(12).readU32(), ft = d.add(16).readU32();
      if (!nameRva && !ft) break;
      if (base.add(nameRva).readCString().toLowerCase() !== dll.toLowerCase()) continue;
      if (!oft) throw new Error('import stand-in: ' + module + '!' + dll + ' has no name table');
      for (let i = 0; ; i++) {
        const e = base.add(oft + 4 * i).readU32();
        if (!e) break;
        if (e & 0x80000000) continue;                               // by ordinal
        if (base.add(e + 2).readCString() === name) return base.add(ft + 4 * i);
      }
    }
    throw new Error('import stand-in: ' + module + ' imports no ' + dll + '!' + name);
  }

  // Encoded by hand. A first version used X86Writer with a label for the `jne`; the label was left unresolved and the
  // jne jumped to itself, so the first FOREIGN thread to enter a stub (the game's first thread calling
  // GetSystemMetrics / timeGetTime) spun there for ever (sampled eip = stub+0xb, log/c3/alt/c3cy_freeze_probe.py).
  //   +0x00  64 a1 24 00 00 00     mov eax, fs:[0x24]
  //   +0x06  3d <tid>              cmp eax, tid
  //   +0x0b  0f 85 05 00 00 00     jne +0x16
  //   +0x11  e9 <rel cb>           jmp cb
  //   +0x16  f0 ff 05 <counter>    lock inc dword ptr [counter]
  //   +0x1d  e9 <rel prev>         jmp prev
  function makeStub(tid, cb, prev, counter) {
    const stub = __keepAlloc(Process.pageSize);
    const rel = function (target, next) { return target.sub(stub.add(next)).toUInt32(); };
    const u32 = function (v) { return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; };
    const bytes = [0x64, 0xa1, 0x24, 0x00, 0x00, 0x00]
      .concat([0x3d], u32(tid >>> 0))
      .concat([0x0f, 0x85], u32(5))
      .concat([0xe9], u32(rel(cb, 0x16)))
      .concat([0xf0, 0xff, 0x05], u32(counter.toUInt32()))
      .concat([0xe9], u32(rel(prev, 0x22)));
    Memory.patchCode(stub, bytes.length, function (code) { code.writeByteArray(bytes); });
    const back = new Uint8Array(stub.readByteArray(bytes.length));
    if (back.some(function (b, i) { return b !== bytes[i]; })) throw new Error('import stand-in: stub write failed');
    return stub;
  }

  globalThis.__importStandIn = function (opts) {
    flush();                                                        // a previous key's timer may not have fired yet
    const tid = Process.getCurrentThreadId();
    const logSize = opts.logSize || 0x400;
    const log = __keepAlloc(logSize);
    log.writeByteArray(new Array(logSize).fill(0));
    const counters = __keepAlloc(4 * MAXIMP);                       // forwarded-call counters (not state)
    counters.writeByteArray(new Array(4 * MAXIMP).fill(0));
    const names = Object.keys(opts.imports);
    if (names.length > MAXIMP) throw new Error('import stand-in: at most ' + MAXIMP + ' imports per key');
    names.forEach(function (full, idx) {
      let spec = opts.imports[full];
      if (typeof spec === 'function') spec = { impl: spec };
      // 'module:DLL!Name' stands in for another module's import in the same call (default module: opts.module)
      const colon = full.indexOf(':'), bang = full.indexOf('!');
      const mod = colon > 0 && colon < bang ? full.slice(0, colon) : opts.module;
      const dll = full.slice(colon > 0 && colon < bang ? colon + 1 : 0, bang), api = full.slice(bang + 1);
      const nargs = spec.args !== undefined ? spec.args : ARGS[api];
      if (nargs === undefined) throw new Error('import stand-in: give `args` for ' + full);
      const abi = spec.abi || 'stdcall';
      const slot = findSlot(mod, dll, api);
      const k = slot.toString();
      if (live[k]) unpatch(k);
      const prev = slot.readPointer();
      const argt = Array(nargs).fill('pointer');
      const orig = new NativeFunction(prev, 'uint32', argt, abi);
      const which = spec.record === undefined ? 'all' : spec.record;
      const cb = new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments);
        const n = log.add(0x10 + 4 * idx).readU32();
        log.add(0x10 + 4 * idx).writeU32(n + 1);
        let vals;
        if (which === false) vals = [];
        else if (which === 'all') vals = a;
        else if (typeof which === 'number') vals = a.slice(0, which);
        else vals = which.map(function (i) { return a[i]; });
        const ctx = { call: n, orig: orig, log: log, record: function (v) { vals = v; } };
        let r;
        if (spec.impl) r = spec.impl(a, ctx);
        else if (spec.forward) r = orig.apply(null, a);
        else if (Array.isArray(spec.ret)) r = spec.ret[Math.min(n, spec.ret.length - 1)];
        else r = spec.ret === undefined ? 0 : spec.ret;
        const cur = log.add(4).readU32();
        const need = 8 + 4 * vals.length;
        if (HDR + cur + need > logSize) {
          log.add(8).writeU32(log.add(8).readU32() + 1);
        } else {
          const p = log.add(HDR + cur);
          p.writeU32(idx);
          p.add(4).writeU32(vals.length);
          vals.forEach(function (v, i) {
            p.add(8 + 4 * i).writeU32(typeof v === 'number' ? (v >>> 0) : ptr(v).toUInt32());
          });
          log.add(4).writeU32(cur + need);
          log.writeU32(log.readU32() + 1);
        }
        if (r === undefined || r === null) return 0;
        return typeof r === 'number' ? (r >>> 0) : ptr(r).toUInt32();
      }, 'uint32', argt, abi);
      const stub = makeStub(tid, cb, prev, counters.add(4 * idx));
      __diffKeepAlive.push(cb, orig, stub);
      const range = Process.findRangeByAddress(slot);
      const prot = range ? range.protection : 'r--';
      Memory.protect(slot, 4, 'rw-');
      slot.writePointer(stub);
      Memory.protect(slot, 4, prot);
      live[k] = { slot: slot, prev: prev, prot: prot };
    });
    // opts.keep (live checks only, never in a fixture): stay installed until __importStandIn.flush()
    if (!opts.keep) setTimeout(flush, 0);
    return {
      log: log, logSize: logSize,
      // calls other threads made through the stubs while installed (the live check reads these)
      forwarded: function () {
        const o = {};
        names.forEach(function (n, i) { o[n] = counters.add(4 * i).readU32(); });
        return o;
      },
    };
  };
  globalThis.__importStandIn.flush = flush;
  globalThis.__importStandIn.ARGS = ARGS;
})();
