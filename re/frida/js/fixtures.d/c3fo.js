// C3 batch c3fo (round 35): fixtures for shim/src/re/c3fo.cpp (registry re/frida/registry.d/c3fo.py).
// Synthetic values and records only.
//
// THE x87 PROBE (log/c3/c3fo_notes.txt section 1). A key whose function takes st(0) or leaves its result in the x87 unit
// calls probe(mode, ndw, cw): it builds the probe block $blk and, for this key only, replaces globalThis.NativeFunction so
// that the two arms diff_hook.js builds after the fixture returns (`new NativeFunction(armAddress, ret, args, abi)`) are
// C3foX87Probe(armAddress, $blk, args...) instead. Restored at the key's flush (freeze.js atFlush), which diff_hook runs
// before the next fixture. Every key arms the wrong-body switch (__freeze.negOn) and gives word 2 of the switch the A/B
// thread's id: the negctl mutants also test the calling thread, so game threads entering a hooked CRT routine are never
// sent into a wrong body.
(function () {
  const F = {};
  const alloc = function (n) { const p = __keepAlloc(n); p.writeByteArray(new Uint8Array(n).buffer); return p; };

  function armNeg() {
    __freeze.negOn();
    const tid = Process.getCurrentThreadId();
    __freeze.negCtlWords().forEach(function (a) { __freeze.setU32(a.add(8), tid); });
  }

  // mode bit 0: the first argument is an input record (not forwarded); bit 2: return st(0) as a double. ndw = argument
  // dwords forwarded. cw = the control word loaded before each arm.
  function probe(mode, ndw, cw) {
    const blk = alloc(0x100);
    blk.writeU32(mode);
    blk.add(4).writeU32(ndw);
    blk.add(8).writeU16(cw);
    const target = Process.getModuleByName('winmm.dll').getExportByName('C3foX87Probe');
    const Real = globalThis.NativeFunction;
    globalThis.NativeFunction = function (p, ret, args, abi) {
      const nf = new Real(target, ret, ['pointer', 'pointer'].concat(args), abi);
      return function () { return nf.apply(null, [p, blk].concat(Array.prototype.slice.call(arguments))); };
    };
    __freeze.atFlush(function () { globalThis.NativeFunction = Real; });
    return blk;
  }
  globalThis.__c3foProbe = probe;

  // The same arm replacement for C3foSehCall5(target, blk, a0..a4): the arm runs inside __try / __except in the shim, so
  // an exception raised under it unwinds the arm's own frames (their __finally blocks run) and is caught there; the
  // NativeFunction is built with exceptions: 'propagate' so Frida's own handler does not take the exception first.
  function sehArms(blk) {
    const target = Process.getModuleByName('winmm.dll').getExportByName('C3foSehCall5');
    const Real = globalThis.NativeFunction;
    globalThis.NativeFunction = function (p, ret, args, abi) {
      const nf = new Real(target, ret, ['pointer', 'pointer'].concat(args), { abi: 'default', exceptions: 'propagate' });
      return function () { return nf.apply(null, [p, blk].concat(Array.prototype.slice.call(arguments))); };
    };
    __freeze.atFlush(function () { globalThis.NativeFunction = Real; });
  }

  // 10-byte register images: from a double (exact: every double is an extended value) or raw [sign/exponent, significand
  // hex]. Record: 10 bytes of value, byte 10 = `empty` (the probe leaves st(0) empty: stack underflow), 16 bytes apart.
  const dv = new DataView(new ArrayBuffer(8));
  function x80OfDouble(v) {
    dv.setFloat64(0, v, true);
    const lo = BigInt(dv.getUint32(0, true)), hi = BigInt(dv.getUint32(4, true));
    const bits = (hi << 32n) | lo;
    const sign = (bits >> 63n) & 1n, e = Number((bits >> 52n) & 0x7ffn);
    let f = bits & ((1n << 52n) - 1n);
    let se, m;
    if (e === 0x7ff) { se = 0x7fff; m = (1n << 63n) | (f << 11n); }
    else if (e === 0 && f === 0n) { se = 0; m = 0n; }
    else if (e === 0) {                       // double denormal: normalise
      let ex = 1 - 1023;
      while (!(f & (1n << 52n))) { f <<= 1n; ex--; }
      se = ex + 16383; m = f << 11n;
    } else { se = e - 1023 + 16383; m = (1n << 63n) | (f << 11n); }
    return { se: se | (Number(sign) << 15), m: m };
  }
  function writeX80(p, x) {
    p.writeU32(Number(x.m & 0xffffffffn));
    p.add(4).writeU32(Number((x.m >> 32n) & 0xffffffffn));
    p.add(8).writeU16(x.se);
  }
  function records(list, out) {
    const blk = alloc(16 * list.length);
    list.forEach(function (v, i) {
      const r = blk.add(16 * i);
      if (v === 'empty') r.add(10).writeU8(1);
      else if (Array.isArray(v)) writeX80(r, { se: v[0], m: BigInt('0x' + v[1]) });
      else writeX80(r, x80OfDouble(v));
      out['r' + i] = r;
    });
    out.recs = blk;
  }

  // ------------------------------------------------------------------------------------------------- A. __ftol
  const FTOL = [
    0, -0, 0.5, -0.5, 0.9999999999999999, 1, -1, 1.5, -1.5, 2.5, -2.5, 3.75, -3.75, 123456.789, -123456.789,
    2147483647.75, 2147483648, -2147483648.5, -2147483649, 4294967295.5, 4294967296.25, 9007199254740992,
    1e18, -1e18, 9.2e18, 9.3e18, -9.3e18, 1e19, 1e300, -1e300, Infinity, -Infinity, NaN, 1e-310, -5e-324,
    2.2250738585072014e-308,
    [0x403D, 'FFFFFFFFFFFFFFFE'],    // 2^63 - 1
    [0x403D, 'FFFFFFFFFFFFFFFF'],    // 2^63 - 0.5
    [0x403E, '8000000000000000'],    // 2^63
    [0xC03E, '8000000000000000'],    // -2^63
    [0xC03E, '8000000000000001'],    // -2^63 - 1
    [0xC03D, 'FFFFFFFFFFFFFFFF'],    // -(2^63 - 0.5)
    [0x3FFE, 'FFFFFFFFFFFFFFFF'],    // 1 - 2^-64
    [0x4000, 'C000000000000001'],    // 3 + 2^-62
    [0xC000, 'C000000000000001'],    // -(3 + 2^-62)
    [0x7FFF, '8000000000000001'],    // signaling NaN
    [0xFFFF, 'C000000000000000'],    // -quiet NaN (the real indefinite)
    [0x7FFF, '0000000000000000'],    // pseudo-infinity
    [0x4000, '4000000000000000'],    // unnormal
    [0x0000, '0000000000000001'],    // extended denormal
    [0x8000, '8000000000000000'],    // negative pseudo-denormal
    [0x7FFE, 'FFFFFFFFFFFFFFFF'],    // largest extended
    [0x0001, '8000000000000000'],    // smallest normal extended
    [0x403D, '8000000000000001'],    // 2^62 + 0.5
    'empty', 'empty',
  ];
  const WORDS = { w27f: 0x027f, wf7f: 0x0f7f, w37f: 0x037f, wa7f: 0x0a7f, w67f: 0x067f };
  Object.keys(WORDS).forEach(function (w) {
    F['c3fo_ftol_' + w] = function () {
      const out = {};
      records(FTOL, out);
      out.blk = probe(1, 0, WORDS[w]);
      armNeg();
      return out;
    };
  });

  // ---------------------------------------------------------------------------- B. __strcmpi / __strnicmp (exe)
  // STR: the strings, each in its own 0x40 slot prefilled with 0xEE (bytes after a terminator are never 0). Keys:
  //   _c     the C locale: __lc_handle[LC_CTYPE] 0x00840b44 = 0 (its value in the running game; set explicitly)
  //   _loc   0x00840b44 = 0x409 and __setlc_active 0x00841008 = 0: the _tolower path under the reader count 0x0084100c
  //   _lock  0x00840b44 = 0x409 and 0x00841008 = 1: the _tolower path under the setlocale lock 0x13
  // Both globals are written with the first thread frozen and restored at the flush. _lock 0x004a79b2 / _unlock 0x004a7a13
  // are recording stand-ins (framemask.js FM.stand, A/B thread only); _tolower 0x004a7241 is recorded and forwarded to
  // the real routine (with the handle 0x409 it maps 'A'..'Z' through __crtLCMapStringA, a pure Win32 mapping).
  const STR = [
    [], [0x61], [0x41], 'abc', 'ABC', 'abd', 'ab', 'aBc', 'Hello, World', 'hello, world', 'hello, worle', '_x', 'Ax',
    '@', '`', '[', '{', [0xC0, 0x41], [0xE0, 0x61], [0xFF], [0x7F], 'Z', 'z', 'zZ', 'ZZ', 'MiXeD-123', 'mixed-123',
    'mixed-124', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz', 'abcdefghijklmnopqrstuvwxyZ_',
  ];
  function strBlock(out) {
    const blk = alloc(0x40 * STR.length);
    blk.writeByteArray(new Uint8Array(0x40 * STR.length).fill(0xee).buffer);
    STR.forEach(function (s, i) {
      const b = typeof s === 'string' ? Array.from(s, function (c) { return c.charCodeAt(0); }) : s.slice();
      b.push(0);
      blk.add(0x40 * i).writeByteArray(b);
      out['s' + i] = blk.add(0x40 * i);
    });
    out.strblk = blk;
  }
  function cmpFx(lc, setlc) {
    return function () {
      const out = {};
      strBlock(out);
      const FM = __frameMask.begin({ size: 0x2000 });
      FM.stand({ addr: 0x004a79b2, abi: 'mscdecl', nargs: 1, ret: 0 });         // _lock
      FM.stand({ addr: 0x004a7a13, abi: 'mscdecl', nargs: 1, ret: 0 });         // _unlock
      FM.stand({ addr: 0x004a7241, abi: 'mscdecl', nargs: 1, forward: true });  // _tolower
      __freeze.thread();
      __freeze.setU32(ptr(0x00840b44), lc);
      __freeze.setU32(ptr(0x00841008), setlc);
      armNeg();
      return Object.assign(out, FM.fx());
    };
  }
  F.c3fo_cmp_c = cmpFx(0, 0);
  F.c3fo_cmp_loc = cmpFx(0x409, 0);
  F.c3fo_cmp_lock = cmpFx(0x409, 1);

  // ------------------------------------------------------------------------------------------ C. _fseek / _rewind
  // FILE records (0x20 bytes, VC6 layout) in $fblk, one per vector: _ptr / _cnt / _base / _bufsiz are marker words, _flag
  // and _file (descriptor) from SFILES. Descriptors >= 0 index a fixture ioinfo block (32 records of 0x24 bytes, osfile
  // bytes 0xff) that the fixture puts into a NULL slot k of __pioinfo 0x00842040 with the first thread frozen; -1 uses
  // __badioinfo 0x004e4d60 (its osfile byte at 0x004e4d64 is saved, set to 0xff for the key and restored). Callees are
  // recording stand-ins: __lock_file 0x004a9874, __unlock_file 0x004a98c6, __fseek_lk 0x004a7145 (answers from its
  // arguments: origin 0..2 -> offset & 7, else -1), __flush 0x004a99a0 (0), __lseek 0x004a95bb (0); each logs the FILE's
  // eight words where it takes one.
  const SFILES = [   // [flag, descriptor slot j (-1 = the descriptor -1)]
    [0x01, 0], [0x02, 5], [0x81, 31], [0xb3, 1], [0x31, 2], [0x80, 3], [0xffffffff, 4], [0x00, -1], [0x83, -1],
    [0x10a, 6], [0x20, 7], [0x4001, 8],
  ];
  function streamFx() {
    return function () {
      const out = {};
      const FM = __frameMask.begin({ size: 0x2000 });
      FM.stand({ addr: 0x004a9874, abi: 'mscdecl', nargs: 1, ret: 0 });                     // __lock_file
      FM.stand({ addr: 0x004a98c6, abi: 'mscdecl', nargs: 1, ret: 0 });                     // __unlock_file
      FM.stand({ addr: 0x004a7145, abi: 'mscdecl', nargs: 3, deref: { 0: 8 },              // __fseek_lk
                 ret: function (s, a) { return (a[2] >>> 0) <= 2 ? (a[1] & 7) : 0xffffffff; } });
      FM.stand({ addr: 0x004a99a0, abi: 'mscdecl', nargs: 1, deref: { 0: 8 }, ret: 0 });   // __flush
      FM.stand({ addr: 0x004a95bb, abi: 'mscdecl', nargs: 3, ret: 0 });                     // __lseek
      const io = alloc(0x24 * 32);
      io.writeByteArray(new Uint8Array(0x24 * 32).fill(0xff).buffer);
      const fblk = alloc(0x20 * SFILES.length);
      __freeze.thread();
      const tab = ptr(0x00842040);
      let k = -1;
      for (let i = 1; i < 64; i++) if (tab.add(4 * i).readU32() === 0) { k = i; break; }
      if (k < 0) throw new Error('c3fo: no free __pioinfo slot');
      __freeze.setU32(tab.add(4 * k), io.toUInt32());
      __freeze.save(ptr(0x004e4d60), 0x24);
      ptr(0x004e4d64).writeU8(0xff);
      SFILES.forEach(function (d, i) {
        const f = fblk.add(0x20 * i);
        f.writeU32(0x5a000000 | i); f.add(4).writeU32(0x5b000000 | i); f.add(8).writeU32(0x5c000000 | i);
        f.add(0xc).writeU32(d[0] >>> 0);
        f.add(0x10).writeS32(d[1] < 0 ? -1 : k * 32 + d[1]);
        f.add(0x14).writeU32(0); f.add(0x18).writeU32(0x1000); f.add(0x1c).writeU32(0);
        out['f' + i] = f;
      });
      out.fblk = fblk;
      out.io = io;
      armNeg();
      return Object.assign(out, FM.fx());
    };
  }
  F.c3fo_stream = streamFx();

  // ------------------------------------------------------------------- D. eh vector constructor iterator (exe, sound)
  // $D: +0 constructor calls, +4 destructor calls, +8 the constructor call index that raises (-1: none), +0x10 the `this`
  // of every constructor call (64 slots), +0x110 the `this` of every destructor call (64 slots). $ctor (thiscall): writes
  // its call index into the element's first dword, logs `this`, and when the index equals +8 raises exception 0xE0C3F00F
  // through KERNEL32!RaiseException. $dtor (thiscall): logs `this` and inverts the element's first dword. $arr: the array
  // (0x2000 bytes of 0xA5). Every key runs its arms through C3foSehCall5 (sehArms), which writes the code it caught to
  // $blk +0x30 (0 when the arm returned).
  function ehvecFx(throwAt) {
    return function () {
      const D = alloc(0x210);
      D.add(8).writeS32(throwAt);
      const slot = alloc(4);
      slot.writePointer(Process.getModuleByName('kernel32.dll').getExportByName('RaiseException'));
      const code = alloc(0x80);
      const u = function (v) { const b = []; v = v >>> 0; for (let k = 0; k < 4; k++) b.push((v >>> (8 * k)) & 0xff); return b; };
      const d = D.toUInt32();
      const ctor = [0xA1].concat(u(d), [0xFF, 0x05], u(d), [0x89, 0x01, 0x89, 0x0C, 0x85], u(d + 0x10),
                                 [0x3B, 0x05], u(d + 8), [0x75, 0x11, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x68],
                                 u(0xE0C3F00F), [0xFF, 0x15], u(slot.toUInt32()), [0xC3]);
      const dtor = [0xA1].concat(u(d + 4), [0xFF, 0x05], u(d + 4), [0x89, 0x0C, 0x85], u(d + 0x110), [0xF7, 0x11, 0xC3]);
      code.writeByteArray(ctor);
      code.add(0x40).writeByteArray(dtor);
      Memory.protect(code, 0x80, 'rwx');
      const arr = alloc(0x2000);
      arr.writeByteArray(new Uint8Array(0x2000).fill(0xa5).buffer);
      const blk = alloc(0x100);
      sehArms(blk);
      armNeg();
      return { D: D, arr: arr, blk: blk, ctor: code, dtor: code.add(0x40), slot: slot };
    };
  }
  F.c3fo_ehvec = ehvecFx(-1);
  F.c3fo_ehvec_t0 = ehvecFx(0);
  F.c3fo_ehvec_t3 = ehvecFx(3);

  // ----------------------------------------------------------------------------------------------- E. jgld _fabs
  // The value is passed as two dwords (lo, hi), returned in st(0); the probe forwards both (ndw 2) and returns st(0) as a
  // double (mode bit 2), under the thread word 0x027f (fabs restores whatever word it entered with). FAB raw (lo, hi).
  const FAB = [
    [0, 0], [0, 0x80000000], [0, 0x3ff00000], [0, 0xbff00000], [0x9999999a, 0x3ff19999], [0x9999999a, 0xbff19999],
    [0x54442d18, 0x400921fb], [0x54442d18, 0xc00921fb], [1, 0], [1, 0x80000000], [0xffffffff, 0x000fffff],
    [0xffffffff, 0x800fffff], [0, 0x00100000], [0, 0x80100000], [0, 0x7ff00000], [0, 0xfff00000],
    [0, 0x7ff80000], [0, 0xfff80000], [1, 0x7ff00000], [1, 0xfff00000], [0x12345678, 0x7ff12345],
    [0x12345678, 0xfff12345], [0xffffffff, 0x7fefffff], [0xffffffff, 0xffefffff], [0x33333333, 0x3fb99999],
    [0x33333333, 0xbfb99999], [0, 0x43300000], [0, 0xc3300000],
  ];
  F.c3fo_fabs = function () {
    const out = {};
    FAB.forEach(function (v, i) { out['lo' + i] = v[0] >>> 0; out['hi' + i] = v[1] >>> 0; });
    out.blk = probe(4, 2, 0x027f);
    armNeg();
    return out;
  };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
