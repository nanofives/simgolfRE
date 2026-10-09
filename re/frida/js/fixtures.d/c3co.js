// Fixtures for batch c3co (round 22, golf_clean.exe static-initialization families, low half).
// Reimplementations: shim/src/re/c3co.cpp. Registry: re/frida/registry.d/c3co.py.
//
// Every key is one argument-free call. A fixture can do three things, all undone from setTimeout(0) (after the rpc
// call that ran the key's two arms), or at the start of the next c3co fixture if that timer has not fired yet:
//
//   onexit   stand-in for _onexit 0x004a5654 (the only callee of _atexit 0x004a56d2, 0x004a56d6). On the A/B
//            thread it RECORDS the pointer (count at log+0, pointers from log+4) and returns it ('ok': _atexit
//            then returns 0) or NULL ('fail': _atexit returns -1); nothing reaches the CRT's onexit table
//            (0x00842170 / 0x0084216c), so no destructor is really registered. Other threads are forwarded.
//   regions  the live globals the constructors write. The game's first thread (the one that runs the game loop
//            and reads these objects) is SUSPENDED, the real bytes saved, and the regions filled with a pattern
//            unique per dword, so every constructor store shows in the compared state. The real bytes are written
//            back before the thread is resumed. With the thread down the arms only run constructors over those
//            regions: no allocation, no lock, no Win32 call.
//   display  View/Image constructors reach 0x00483da0, which ORs 0x3f into 0x0083ad4c, calls [0x0083ad50]
//            vtable +0x150 and clears 0x0083ad4c. For the key 0x0083ad50 points at a fixture display whose slot
//            +0x150 is `inc dword [ecx+4]; ret` (every other slot `xor eax, eax; ret`); the live display pointer
//            is restored before the thread resumes.
(function () {
  const T = {"iga_00403f20":{"regions":[[5203384,78044]],"onexit":null,"display":false},"iga_00403f80":{"regions":[[5307408,40220]],"onexit":null,"display":false},"iga_00403fe0":{"regions":[[5281400,26036]],"onexit":null,"display":false},"iga_00404040":{"regions":[[5151376,52040]],"onexit":null,"display":false},"iga_00404c20":{"regions":[[5507392,120],[5514200,76],[5519824,736],[5629344,912],[5667576,76],[5677632,736],[5736336,124],[5738936,76],[5813824,76],[5879600,76],[5884488,3552],[5914864,76]],"onexit":null,"display":false},"iga_00404e00":{"regions":[[5645144,1440],[5678928,76],[5687208,76],[5813064,76],[5876648,736],[5881736,76],[5888272,1440],[5916888,208]],"onexit":null,"display":false},"iga_00404f50":{"regions":[[5487160,384],[5508488,1440],[5646552,19040],[5678400,560],[5809448,3200],[5877824,1792],[5892016,5312]],"onexit":null,"display":false},"iga_004050b0":{"regions":[[5516864,208],[5630240,560],[5666984,384],[5669448,912],[5679016,384],[5799576,9888]],"onexit":null,"display":false},"iga_004051f0":{"regions":[[5511688,736],[5672888,1264],[5704872,4256],[5782128,1968],[5795648,120],[5796760,1880],[5813120,736],[5880408,1352],[5929976,1088]],"onexit":null,"display":false},"iga_004053a0":{"regions":[[5504312,3112],[5709104,692],[5812624,472],[5879656,780],[5911784,3112],[5934208,1792]],"onexit":null,"display":false},"iga_004054e0":{"regions":[[5375360,208],[5490016,1440],[5517080,208],[5876464,208],[5889808,2232],[5914920,1440]],"onexit":null,"display":false},"iga_00405620":{"regions":[[5640920,1748],[5665664,1352],[5679416,1352],[5813872,1616],[5819560,1748],[5919672,1616]],"onexit":null,"display":false},"iga_00405760":{"regions":[[5486544,648],[5496424,2016],[5510512,1136],[5640784,164],[5667344,252],[5738984,736]],"onexit":null,"display":false},"dyn_00403f10_ok":{"regions":[[5203384,78044]],"onexit":"ok","display":false},"dyn_00403f10_fail":{"regions":[[5203384,78044]],"onexit":"fail","display":false},"dyn_00403f70_ok":{"regions":[[5307408,40220]],"onexit":"ok","display":false},"dyn_00403f70_fail":{"regions":[[5307408,40220]],"onexit":"fail","display":false},"dyn_00403fd0_ok":{"regions":[[5281400,26036]],"onexit":"ok","display":false},"dyn_00403fd0_fail":{"regions":[[5281400,26036]],"onexit":"fail","display":false},"dyn_00404030_ok":{"regions":[[5151376,52040]],"onexit":"ok","display":false},"dyn_00404030_fail":{"regions":[[5151376,52040]],"onexit":"fail","display":false},"dyn_00404310_ok":{"regions":[[5347920,1432]],"onexit":"ok","display":true},"dyn_00404310_fail":{"regions":[[5347920,1432]],"onexit":"fail","display":true},"dyn_004043a0_ok":{"regions":[[5353560,728]],"onexit":"ok","display":true},"dyn_004043a0_fail":{"regions":[[5353560,728]],"onexit":"fail","display":true},"dyn_004043e0_ok":{"regions":[[5347608,40]],"onexit":"ok","display":false},"dyn_004043e0_fail":{"regions":[[5347608,40]],"onexit":"fail","display":false},"dyn_00404430_ok":{"regions":[[5349320,40]],"onexit":"ok","display":false},"dyn_00404430_fail":{"regions":[[5349320,40]],"onexit":"fail","display":false},"dyn_00404480_ok":{"regions":[[5354256,40]],"onexit":"ok","display":false},"dyn_00404480_fail":{"regions":[[5354256,40]],"onexit":"fail","display":false},"dyn_004044d0_ok":{"regions":[[5354320,40]],"onexit":"ok","display":false},"dyn_004044d0_fail":{"regions":[[5354320,40]],"onexit":"fail","display":false},"dyn_00404520_ok":{"regions":[[5347640,40]],"onexit":"ok","display":false},"dyn_00404520_fail":{"regions":[[5347640,40]],"onexit":"fail","display":false},"dyn_00404570_ok":{"regions":[[5347888,40]],"onexit":"ok","display":false},"dyn_00404570_fail":{"regions":[[5347888,40]],"onexit":"fail","display":false},"dyn_004045c0_ok":{"regions":[[5349400,40]],"onexit":"ok","display":false},"dyn_004045c0_fail":{"regions":[[5349400,40]],"onexit":"fail","display":false},"dyn_00404610_ok":{"regions":[[5354288,40]],"onexit":"ok","display":false},"dyn_00404610_fail":{"regions":[[5354288,40]],"onexit":"fail","display":false},"dyn_00404660_ok":{"regions":[[5347672,40]],"onexit":"ok","display":false},"dyn_00404660_fail":{"regions":[[5347672,40]],"onexit":"fail","display":false},"dyn_004046b0_ok":{"regions":[[5347768,120]],"onexit":"ok","display":false},"dyn_004046b0_fail":{"regions":[[5347768,120]],"onexit":"fail","display":false},"dyn_004046f0_ok":{"regions":[[5347856,40]],"onexit":"ok","display":false},"dyn_004046f0_fail":{"regions":[[5347856,40]],"onexit":"fail","display":false},"dyn_00404740_ok":{"regions":[[5349368,40]],"onexit":"ok","display":false},"dyn_00404740_fail":{"regions":[[5349368,40]],"onexit":"fail","display":false},"dyn_00404790_ok":{"regions":[[5347704,40]],"onexit":"ok","display":false},"dyn_00404790_fail":{"regions":[[5347704,40]],"onexit":"fail","display":false},"dyn_004047e0_ok":{"regions":[[5347736,40]],"onexit":"ok","display":false},"dyn_004047e0_fail":{"regions":[[5347736,40]],"onexit":"fail","display":false},"dyn_00404830_ok":{"regions":[[5349432,40]],"onexit":"ok","display":false},"dyn_00404830_fail":{"regions":[[5349432,40]],"onexit":"fail","display":false},"dyn_00404c10_ok":{"regions":[[5507392,120],[5514200,76],[5519824,736],[5629344,912],[5667576,76],[5677632,736],[5736336,124],[5738936,76],[5813824,76],[5879600,76],[5884488,3552],[5914864,76]],"onexit":"ok","display":false},"dyn_00404c10_fail":{"regions":[[5507392,120],[5514200,76],[5519824,736],[5629344,912],[5667576,76],[5677632,736],[5736336,124],[5738936,76],[5813824,76],[5879600,76],[5884488,3552],[5914864,76]],"onexit":"fail","display":false},"dyn_00404df0_ok":{"regions":[[5645144,1440],[5678928,76],[5687208,76],[5813064,76],[5876648,736],[5881736,76],[5888272,1440],[5916888,208]],"onexit":"ok","display":false},"dyn_00404df0_fail":{"regions":[[5645144,1440],[5678928,76],[5687208,76],[5813064,76],[5876648,736],[5881736,76],[5888272,1440],[5916888,208]],"onexit":"fail","display":false},"dyn_00404f40_ok":{"regions":[[5487160,384],[5508488,1440],[5646552,19040],[5678400,560],[5809448,3200],[5877824,1792],[5892016,5312]],"onexit":"ok","display":false},"dyn_00404f40_fail":{"regions":[[5487160,384],[5508488,1440],[5646552,19040],[5678400,560],[5809448,3200],[5877824,1792],[5892016,5312]],"onexit":"fail","display":false},"dyn_004050a0_ok":{"regions":[[5516864,208],[5630240,560],[5666984,384],[5669448,912],[5679016,384],[5799576,9888]],"onexit":"ok","display":false},"dyn_004050a0_fail":{"regions":[[5516864,208],[5630240,560],[5666984,384],[5669448,912],[5679016,384],[5799576,9888]],"onexit":"fail","display":false},"dyn_004051e0_ok":{"regions":[[5511688,736],[5672888,1264],[5704872,4256],[5782128,1968],[5795648,120],[5796760,1880],[5813120,736],[5880408,1352],[5929976,1088]],"onexit":"ok","display":false},"dyn_004051e0_fail":{"regions":[[5511688,736],[5672888,1264],[5704872,4256],[5782128,1968],[5795648,120],[5796760,1880],[5813120,736],[5880408,1352],[5929976,1088]],"onexit":"fail","display":false},"dyn_00405390_ok":{"regions":[[5504312,3112],[5709104,692],[5812624,472],[5879656,780],[5911784,3112],[5934208,1792]],"onexit":"ok","display":false},"dyn_00405390_fail":{"regions":[[5504312,3112],[5709104,692],[5812624,472],[5879656,780],[5911784,3112],[5934208,1792]],"onexit":"fail","display":false},"dyn_004054d0_ok":{"regions":[[5375360,208],[5490016,1440],[5517080,208],[5876464,208],[5889808,2232],[5914920,1440]],"onexit":"ok","display":false},"dyn_004054d0_fail":{"regions":[[5375360,208],[5490016,1440],[5517080,208],[5876464,208],[5889808,2232],[5914920,1440]],"onexit":"fail","display":false},"dyn_00405610_ok":{"regions":[[5640920,1748],[5665664,1352],[5679416,1352],[5813872,1616],[5819560,1748],[5919672,1616]],"onexit":"ok","display":false},"dyn_00405610_fail":{"regions":[[5640920,1748],[5665664,1352],[5679416,1352],[5813872,1616],[5819560,1748],[5919672,1616]],"onexit":"fail","display":false},"dyn_00405750_ok":{"regions":[[5486544,648],[5496424,2016],[5510512,1136],[5640784,164],[5667344,252],[5738984,736]],"onexit":"ok","display":false},"dyn_00405750_fail":{"regions":[[5486544,648],[5496424,2016],[5510512,1136],[5640784,164],[5667344,252],[5738984,736]],"onexit":"fail","display":false},"onexit_ok":{"regions":[],"onexit":"ok","display":false},"onexit_fail":{"regions":[],"onexit":"fail","display":false}};

  let pending = null;
  function flush() { if (pending) { const f = pending; pending = null; f(); } }

  function k32(name, ret, args) {
    return new NativeFunction(Process.getModuleByName('kernel32.dll').getExportByName(name), ret, args, 'stdcall');
  }
  // Suspends the process's first thread (Toolhelp lists threads in creation order); returns its resume function.
  function freezeFirstThread() {
    const openThread = k32('OpenThread', 'pointer', ['uint32', 'int', 'uint32']);
    const suspend = k32('SuspendThread', 'uint32', ['pointer']);
    const resume = k32('ResumeThread', 'uint32', ['pointer']);
    const closeHandle = k32('CloseHandle', 'int', ['pointer']);
    const threads = Process.enumerateThreads();
    const tid = threads.length ? threads[0].id : 0;
    if (!tid || tid === Process.getCurrentThreadId()) throw new Error('c3co: no first thread to suspend');
    const h = openThread(0x0002 /* THREAD_SUSPEND_RESUME */, 0, tid);
    if (h.isNull()) throw new Error('c3co: OpenThread failed');
    const prev = suspend(h);
    if (prev === 0xffffffff) { closeHandle(h); throw new Error('c3co: SuspendThread failed'); }
    if (prev !== 0) { resume(h); closeHandle(h); throw new Error('c3co: thread already suspended (' + prev + ')'); }
    return function () { globalThis.__c3coResumed = resume(h); closeHandle(h); };
  }

  const ONEXIT = ptr('0x004a5654');
  function installOnexit(mode) {
    const tid = Process.getCurrentThreadId();
    const log = __keepAlloc(0x40);
    log.writeByteArray(new Array(0x40).fill(0));
    const orig = new NativeFunction(ONEXIT, 'pointer', ['pointer']);
    const cb = new NativeCallback(function (fn) {
      if (Process.getCurrentThreadId() !== tid) return orig(fn);
      const n = log.readU32();
      if (n < 15) { log.add(4 + 4 * n).writePointer(fn); log.writeU32(n + 1); }
      return mode === 'ok' ? fn : ptr(0);
    }, 'pointer', ['pointer']);
    __diffKeepAlive.push(cb, orig);
    Interceptor.replace(ONEXIT, cb);
    Interceptor.flush();
    return { log: log, undo: function () { Interceptor.revert(ONEXIT); Interceptor.flush(); } };
  }

  function makeDisplay() {
    const code = __keepAlloc(Process.pageSize);
    Memory.protect(code, Process.pageSize, 'rwx');
    code.writeByteArray([0x33, 0xC0, 0xC3,                  // +0: xor eax, eax; ret
                         0x90, 0xFF, 0x41, 0x04, 0xC3]);    // +4: inc dword [ecx+4]; ret
    const vt = __keepAlloc(0x200);
    for (let i = 0; i < 0x200; i += 4) vt.add(i).writePointer(code);
    vt.add(0x150).writePointer(code.add(4));
    const disp = __keepAlloc(8);
    disp.writePointer(vt);
    disp.add(4).writeU32(0);
    return disp;
  }

  function pattern(addr, len, seed) {
    const u = new Uint32Array(len >>> 2);
    for (let i = 0; i < u.length; i++) u[i] = (0xC3000000 | (((addr + 4 * i) >>> 2) & 0x00ffffff)) ^ seed;
    return u.buffer;
  }

  function setup(key) {
    flush();
    const t = T[key];
    const out = {};
    const undo = [];   // run in reverse order
    let onexit = null;
    if (t.onexit) { onexit = installOnexit(t.onexit); out.log = onexit.log; }
    const disp = t.display ? makeDisplay() : null;
    if (disp) out.disp = disp;
    if (t.regions.length || disp) {
      const resume = freezeFirstThread();
      undo.push(resume);
      const saved = t.regions.map(function (r) { return [ptr(r[0]), ptr(r[0]).readByteArray(r[1])]; });
      undo.push(function () { saved.forEach(function (s) { s[0].writeByteArray(s[1]); }); });
      t.regions.forEach(function (r) { ptr(r[0]).writeByteArray(pattern(r[0], r[1], 0x5a17c3c0)); });
      if (disp) {
        const slot = ptr('0x0083ad50');
        const real = slot.readPointer();
        slot.writePointer(disp);
        undo.push(function () { slot.writePointer(real); });
      }
    }
    if (onexit) undo.unshift(onexit.undo);   // reverted last, after the thread is running again
    pending = function () { for (let i = undo.length - 1; i >= 0; i--) undo[i](); };
    setTimeout(flush, 0);
    return out;
  }

  const fx = {};
  Object.keys(T).forEach(function (k) { fx['c3co_' + k] = function () { return setup(k); }; });
  Object.assign(globalThis.DIFF_FIXTURES, fx);
})();
