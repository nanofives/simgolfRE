// Frozen-thread helper for A/B fixtures (2026-10-09; the method c3cq worked out in round 22, shared here).
// Loaded before every fixture fragment. A fixture that has to write live globals the game's first thread (the one that
// owns the window, pumps messages and draws) reads suspends that thread for the length of one key:
//
//   __freeze.thread()            suspend the process's first thread at a safe point; returns an idempotent release()
//   __freeze.save(p, n)          remember n bytes at p; they are written back at the flush
//   __freeze.fill(p, n, byte)    save, then fill          __freeze.setU32(p, v)   save, then store
//   __freeze.atFlush(fn)         run fn at the flush (callbacks and saves run newest first, all BEFORE the resume)
//   __freeze.schedule()          queue a flush from setTimeout(0) (thread/save/atFlush already do)
//   __freeze.begin()             flush whatever is pending, synchronously (diff_hook.js calls it before every fixture)
//
// The flush runs from setTimeout(0): Frida's JS runtime is single-threaded, so it cannot run before the rpc call that
// drives the key's vectors has returned. It restores (newest first), then resumes every thread still held.
//
// Why each part (c3cq, round 22; earlier batches c3av..c3cr froze with a plain cooperative SuspendThread):
//   - scheduling 'exclusive': a cooperative NativeFunction call releases Frida's JS lock. The first thread, waiting
//     inside a heap stand-in forwarding callback, then takes the lock just before SuspendThread stops it, and the
//     script thread never gets the lock back (cdb: first thread suspended inside frida-agent's JS engine, gum-js-loop
//     waiting in RtlEnterCriticalSection). Sleep stays cooperative so a thread blocked on the lock can finish.
//   - safe point: the thread is kept suspended only while its eip lies in the game's own code (by default the r-x
//     ranges of golf_clean.exe). A thread stopped inside a system DLL can hold the process heap's lock (GDI/USER calls
//     allocate there), which the agent thread needs to send the rpc reply; a first full c3cq run hung that way after
//     ~100 keys. Otherwise it is resumed, the script sleeps 1 ms and tries again (10000 times, then throws).
//   - begin(): a key's timer may not have fired when the next rpc call arrives; a key that takes a CRT lock while the
//     first thread is still suspended inside that lock would deadlock, so whatever is pending is flushed first.
//   - throws instead of running the key unfrozen when the thread cannot be stopped (older fixtures returned silently).
(function () {
  const k32 = Process.getModuleByName('kernel32.dll');
  const nf = (name, ret, args) => new NativeFunction(k32.getExportByName(name), ret, args,
                                                     { abi: 'stdcall', scheduling: 'exclusive' });
  const openThread = nf('OpenThread', 'pointer', ['uint32', 'int', 'uint32']);
  const suspendThread = nf('SuspendThread', 'uint32', ['pointer']);
  const resumeThread = nf('ResumeThread', 'uint32', ['pointer']);
  const getThreadContext = nf('GetThreadContext', 'int', ['pointer', 'pointer']);
  const closeHandle = nf('CloseHandle', 'int', ['pointer']);
  const sleep = new NativeFunction(k32.getExportByName('Sleep'), 'void', ['uint32'], 'stdcall');
  const ctx = Memory.alloc(0x2d0);                 // x86 CONTEXT is 0x2cc bytes; Eip at +0xb8

  const pend = { undo: [], held: [] };
  const status = globalThis.__freezeStatus = { freezes: 0, flushes: 0, resumed: [], lastTries: 0, lastEip: 0 };

  function codeRanges(modules) {
    const out = [];
    modules.forEach(function (m) {
      Process.getModuleByName(m).enumerateRanges('r-x').forEach(function (r) {
        out.push([r.base, r.base.add(r.size)]);
      });
    });
    return out;
  }
  function inRanges(eip, ranges) {
    const p = ptr(eip);
    return ranges.some(function (r) { return p.compare(r[0]) >= 0 && p.compare(r[1]) < 0; });
  }

  function flush() {
    if (!pend.undo.length && !pend.held.length) return;
    const undo = pend.undo.splice(0);
    for (let i = undo.length - 1; i >= 0; i--) undo[i]();
    pend.held.splice(0).forEach(function (rel) { rel(); });
    status.flushes++;
  }
  let queued = false;
  function schedule() {
    if (queued) return;
    queued = true;
    setTimeout(function () { queued = false; flush(); }, 0);
  }

  // opts.modules: modules whose code counts as a safe point (default ['golf_clean.exe']); opts.tries (default 10000)
  function thread(opts) {
    opts = opts || {};
    const ranges = codeRanges(opts.modules || ['golf_clean.exe']);
    const threads = Process.enumerateThreads();
    const tid = threads.length ? threads[0].id : 0;
    if (!tid || tid === Process.getCurrentThreadId()) throw new Error('freeze: no first thread to suspend');
    const h = openThread(0x0002 | 0x0008 /* THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT */, 0, tid);
    if (h.isNull()) throw new Error('freeze: OpenThread failed');
    const tries = opts.tries || 10000;
    for (let t = 1; t <= tries; t++) {
      const prev = suspendThread(h);
      if (prev === 0xffffffff) { closeHandle(h); throw new Error('freeze: SuspendThread failed'); }
      if (prev !== 0) { resumeThread(h); closeHandle(h); throw new Error('freeze: thread already suspended (' + prev + ')'); }
      ctx.writeU32(0x00010001);                      // ContextFlags = CONTEXT_CONTROL
      const eip = getThreadContext(h, ctx) ? ctx.add(0xb8).readU32() : 0;
      if (eip && inRanges(eip, ranges)) {
        let done = false;
        const release = function () {
          if (done) return;
          done = true;
          status.resumed.push(resumeThread(h));
          closeHandle(h);
        };
        pend.held.push(release);
        status.freezes++;
        status.lastTries = t;
        status.lastEip = eip;
        schedule();
        return release;
      }
      resumeThread(h);
      sleep(1);
    }
    closeHandle(h);
    throw new Error('freeze: first thread never stopped inside the safe-point modules');
  }
  function atFlush(fn) { pend.undo.push(fn); schedule(); }
  function save(p, n) { const b = p.readByteArray(n); atFlush(function () { p.writeByteArray(b); }); }
  function fill(p, n, byte) { save(p, n); p.writeByteArray(new Array(n).fill(byte)); }
  function setU32(p, v) { save(p, 4); p.writeU32(v); }

  // Negative-control switch. A negctl build exports `volatile int <Batch>NegCtl[4] = {0, 0x5147454e, ...}` from the
  // shim (winmm.dll) and keeps its wrong bodies dormant while word 0 is 0. negOn() sets word 0 of every such export to 1
  // until the next flush; in a normal build there is none and it does nothing. Call it only after thread() froze the
  // first thread. (Before 2026-10-12 the switch was a page at the fixed address 0x7A110000; Windows Defender's ML
  // heuristic quarantined DLLs that allocated it.)
  let negWords = null;
  function negCtlWords() {
    if (negWords === null) {
      negWords = [];
      try {
        Process.getModuleByName('winmm.dll').enumerateExports().forEach(function (x) {
          if (!/NegCtl$/.test(x.name)) return;
          try { if (x.address.add(4).readU32() === 0x5147454e) negWords.push(x.address); } catch (e) { /* unreadable */ }
        });
      } catch (e) { /* no shim module */ }
    }
    return negWords;
  }
  function negOn() { negCtlWords().forEach(function (a) { setU32(a, 1); }); }

  globalThis.__freeze = { begin: flush, flush: flush, schedule: schedule, thread: thread, atFlush: atFlush,
                          save: save, fill: fill, setU32: setU32, status: status,
                          negOn: negOn, negCtlWords: negCtlWords };
})();
