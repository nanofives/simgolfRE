// C3 batch c3fza (round 37, sound.dll): fixtures for shim/src/re/c3fza.cpp (registry re/frida/registry.d/c3fza.py).
// Synthetic values and records only.
//
// SoundWindowObj_ctor: twelve 0x20-byte objects, each pre-filled with its own per-dword pattern (the constructor writes
// the dwords at +0, +8, +0xc and only the BYTE at +4, so bytes +5..+7 and +0x10..+0x1f keep the pattern).
//
// dynInit_soundWindowObj100b49b8: the 0x10-byte object at the module global 0x100b49b8 is saved, pattern-filled and
// restored at the freeze flush; _onexit (RVA 0x425c9, the only callee of _atexit 0x42647) records its argument on the A/B
// thread and returns it ('ok': _atexit returns 0) or NULL ('fail': _atexit returns -1); other threads are forwarded to the
// real function (the method of c3do's c3do_dyn).
//
// __ArrayUnwind: $D (0x210 bytes): +0 destructor calls, +8 the call index that raises (-1: none), +0x10 the `this` of each
// destructor call (64 slots). $dtor (thiscall) logs `this`, inverts the element's first dword, and raises exception
// 0xE0C3F00F through KERNEL32!RaiseException when its call index equals +8. Every arm runs through C3fzaSehCall4.
(function () {
  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v >>> 0); }
  function alloc(n, v) { const p = Memory.alloc(n); fill(p, n, v === undefined ? 0 : v); return p; }
  function snd(rva) { return Process.getModuleByName('sound.dll').base.add(rva); }

  function armNeg() {
    __freeze.negOn();
    const tid = Process.getCurrentThreadId();
    __freeze.negCtlWords().forEach(function (a) { __freeze.setU32(a.add(8), tid); });
  }

  function ctorFixture() {
    const out = {};
    for (let i = 0; i < 12; i++) {
      const o = alloc(0x20);
      for (let k = 0; k < 0x20; k += 4) o.add(k).writeU32(((0xC3A00000 + i * 0x01010101) ^ (k * 0x10001)) >>> 0);
      out['o' + i] = o;
    }
    armNeg();
    return out;
  }

  function dynFixture(mode) {
    return function () {
      const out = {};
      const g = snd(0xb49b8);
      __freeze.save(g, 0x10);
      const pat = new Uint32Array(4);
      for (let i = 0; i < 4; i++) pat[i] = (0xC3000000 | (((0xb49b8 >>> 2) + i) & 0x00ffffff)) ^ 0x5a17c3c0;
      g.writeByteArray(pat.buffer);
      out.g = g;
      const log = alloc(0x40);
      out.log = log;
      const tid = Process.getCurrentThreadId();
      const at = snd(0x425c9);
      const orig = new NativeFunction(at, 'pointer', ['pointer']);
      const cb = new NativeCallback(function (fn) {
        if (Process.getCurrentThreadId() !== tid) return orig(fn);
        const n = log.readU32();
        if (n < 15) { log.add(4 + 4 * n).writePointer(fn); log.writeU32(n + 1); }
        return mode === 'ok' ? fn : ptr(0);
      }, 'pointer', ['pointer']);
      __diffKeepAlive.push(cb, orig);
      Interceptor.replace(at, cb);
      Interceptor.flush();
      __freeze.atFlush(function () { Interceptor.revert(at); Interceptor.flush(); });
      armNeg();
      return out;
    };
  }

  function sehArms(blk) {
    const target = Process.getModuleByName('winmm.dll').getExportByName('C3fzaSehCall4');
    const Real = globalThis.NativeFunction;
    globalThis.NativeFunction = function (p, ret, args, abi) {
      const nf = new Real(target, ret, ['pointer', 'pointer'].concat(args), { abi: 'default', exceptions: 'propagate' });
      return function () { return nf.apply(null, [p, blk].concat(Array.prototype.slice.call(arguments))); };
    };
    __freeze.atFlush(function () { globalThis.NativeFunction = Real; });
  }

  function unwindFixture(throwAt) {
    return function () {
      const D = alloc(0x210);
      D.add(8).writeS32(throwAt);
      const slot = alloc(4);
      slot.writePointer(Process.getModuleByName('kernel32.dll').getExportByName('RaiseException'));
      const code = alloc(0x80);
      const d = D.toUInt32();
      const dtor = [0xA1].concat(u32(d), [0xFF, 0x05], u32(d), [0x89, 0x0C, 0x85], u32(d + 0x10), [0xF7, 0x11, 0x3B, 0x05],
                                 u32(d + 8), [0x75, 0x11, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x68], u32(0xE0C3F00F),
                                 [0xFF, 0x15], u32(slot.toUInt32()), [0xC3]);
      code.writeByteArray(dtor);
      Memory.protect(code, 0x80, 'rwx');
      const arr = alloc(0x2000);
      arr.writeByteArray(new Uint8Array(0x2000).fill(0xa5).buffer);
      const blk = alloc(0x100);
      sehArms(blk);
      armNeg();
      return { D: D, arr: arr, blk: blk, dtor: code, slot: slot, arrend: arr.add(0x1000) };
    };
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3fza_ctor: ctorFixture,
    c3fza_dyn_ok: dynFixture('ok'),
    c3fza_dyn_fail: dynFixture('fail'),
    c3fza_unwind_none: unwindFixture(-1),
    c3fza_unwind_t0: unwindFixture(0),
    c3fza_unwind_t2: unwindFixture(2),
  });
})();
