// Fixtures for batch c3dr (round 27, golf_clean.exe non-drawing rows).
// Reimplementations: shim/src/re/c3dr.cpp. Registry: re/frida/registry.d/c3dr.py. Purposes: log/c3/c3dr_purpose.md.
//
// Four families:
//   c3dr_s_<stub>_<variant>   the static-initialisation stubs (`mov ecx, <global>; jmp <constructor>`): they construct
//                             a LIVE global object the game's first thread reads. The fixture suspends that thread
//                             (re/frida/js/freeze.js), saves the object (freeze.js writes it back before the thread
//                             resumes) and seeds it: live (the game's own bytes), pat (a dword pattern unique per
//                             dword) or zero. The Image / EditBox / GlobalObj constructors reach 0x00483da0, which ORs
//                             0x3f into 0x0083ad4c, calls [0x0083ad50] vtable +0x150 and clears 0x0083ad4c: for those
//                             ("disp" stubs) 0x0083ad50 points at a fixture display whose slot +0x150 is
//                             `inc dword [ecx+4]; ret` (live, pat) or at 0 (zero), restored before the thread resumes.
//   c3dr_view / c3dr_view_frozen   c3cu's composite-view objects (fixture c3cu_view / c3cu_view_frozen) plus, for every
//                             object D, a vtordisp thunk input W = D + 0x40 with the displacement 0x40 stored at D + 0x3c
//                             (the thunk computes this - [this - 4] = D).
//   globals                   blitError / setDirtyFlag* / wobble / matchInputCode / zPush: live globals set under the
//                             frozen first thread and put back at the flush.
//   objects                   fixture-owned blocks for the constructors, Decoder ctors and Window::releaseScrollbars.
(function () {
  const DISPLAY = ptr('0x0083ad50');
  const U32 = (v) => [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff];

  // ---------------------------------------------------------------- Part 1: static-initialisation stubs
  const S = {
    0x004043b0: [0x0051b068, 0x2b4, 1], 0x004058f0: [0x005a7148, 0x2b4, 1], 0x004061b0: [0x00568d10, 0x2b4, 1],
    0x004061f0: [0x00587ae8, 0x2b4, 1], 0x0043d640: [0x005aa778, 0x2b4, 1], 0x0046de40: [0x00838f98, 0x2b4, 1],
    0x00473a70: [0x00839348, 0x2b4, 1], 0x004799f0: [0x0083a500, 0x2b4, 1], 0x00479a40: [0x0083a7b8, 0x2b4, 1],
    0x004385a0: [0x005a9f78, 0x5dc, 1], 0x0049bff0: [0x008400c8, 0x5c8, 1], 0x004837b0: [0x0083ad20, 8, 0],
    0x0049d160: [0x00840710, 0x28, 0], 0x0049d2a0: [0x008407c0, 0x28, 0], 0x0049d300: [0x00840790, 0x28, 0],
    0x0049d360: [0x008407f0, 0x28, 0], 0x0049d490: [0x00840830, 0x28, 0], 0x0049d4f0: [0x00840890, 0x28, 0],
    0x0049d550: [0x00840860, 0x28, 0], 0x0049eb00: [0x008408c8, 0x28, 0], 0x0049eb60: [0x008408f8, 0x28, 0],
  };
  // initPanelState: regions 0x00821020 (8 bytes) and 0x00821ec8 (0x68 bytes, holds 0x00821ee8 / 0x00821f08 / 0x00821f28)
  const PANEL = [[0x00821020, 8], [0x00821ec8, 0x68]];

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
  function patternBytes(addr, n, seed) {
    const u = new Uint32Array(n >>> 2);
    for (let i = 0; i < u.length; i++) u[i] = ((0xC3000000 | (((addr + 4 * i) >>> 2) & 0x00ffffff)) ^ seed) >>> 0;
    return u.buffer;
  }

  function stubFixture(regions, disp, variant) {
    const out = {};
    const d = disp ? makeDisplay() : null;
    if (d) out.disp = d;
    __freeze.thread();
    regions.forEach(function (r) {
      const p = ptr(r[0]);
      __freeze.save(p, r[1]);
      if (variant === 'pat') p.writeByteArray(patternBytes(r[0], r[1], 0x5a17c3c0));
      else if (variant === 'zero') p.writeByteArray(new Array(r[1]).fill(0));
    });
    if (disp) {
      __freeze.save(DISPLAY, 4);
      __freeze.save(ptr('0x0083ad4c'), 4);
      DISPLAY.writePointer(variant === 'zero' ? ptr(0) : __recDisplay.guard(d));
      ptr('0x0083ad4c').writeU32(0);
    }
    out.obj = ptr(regions[0][0]);
    return out;
  }

  const fx = {};
  Object.keys(S).forEach(function (a) {
    const hex = ('00000000' + parseInt(a, 10).toString(16)).slice(-8);
    ['live', 'pat', 'zero'].forEach(function (v) {
      fx['c3dr_s_' + hex + '_' + v] = function () { return stubFixture([[S[a][0], S[a][1]]], S[a][2] === 1, v); };
    });
  });
  ['live', 'pat', 'zero'].forEach(function (v) {
    fx['c3dr_s_0044ac60_' + v] = function () { return stubFixture(PANEL, false, v); };
  });

  // ---------------------------------------------------------------- Part 2: vtordisp thunks over c3cu's view objects
  function withThunkInputs(base) {
    const out = base;
    Object.keys(base).forEach(function (k) {
      const m = /^d(8|4|2|16|1|x)_(\d+)$/.exec(k);
      if (!m) return;
      const D = base[k];
      D.add(0x3c).writeS32(0x40);
      out['w' + m[1] + '_' + m[2]] = D.add(0x40);
    });
    return out;
  }
  fx.c3dr_view = function () { return withThunkInputs(globalThis.DIFF_FIXTURES.c3cu_view()); };
  fx.c3dr_view_frozen = function () { return withThunkInputs(globalThis.DIFF_FIXTURES.c3cu_view_frozen()); };

  // ---------------------------------------------------------------- Part 3: live globals under the frozen thread
  // blitError: the dword 0x0083c310. setDirtyFlagA / B: 0x00822b98 / 0x00822b9c.
  const GVALS = [0, 1, 2, 8, -1, 0x7fffffff, -0x80000000, 0x100, 0x1234, 5, 0xff, 3];
  function globalKeys(prefix, addr) {
    GVALS.forEach(function (v, i) {
      fx[prefix + i] = function () {
        __freeze.thread();
        __freeze.setU32(ptr(addr), v >>> 0);
        return {};
      };
    });
  }
  // blitError has no argument and no state change, so a key with one call could never show a second result. The key
  // c3dr_blit passes an ignored cdecl argument: each $v<i> is a getter that sets the dword 0x0083c310 to GVALS[i] when
  // diff_hook resolves that vector's argument (before the state snapshot), so the twelve vectors of one key run the two
  // arms from twelve different globals. The first thread is frozen and the dword is put back at the flush.
  fx.c3dr_blit = function () {
    __freeze.thread();
    __freeze.save(ptr('0x0083c310'), 4);
    const out = {};
    GVALS.forEach(function (v, i) {
      Object.defineProperty(out, 'v' + i, { enumerable: true, get: function () { ptr('0x0083c310').writeU32(v >>> 0); return 0; } });
    });
    return out;
  };
  globalKeys('c3dr_dirtyA_', 0x00822b98);
  globalKeys('c3dr_dirtyB_', 0x00822b9c);

  // wobble: seeds for the RNG 0x00822d9c chosen so that Random::range(0x65) returns the value r in the comment (the
  // generator step is seed * 0x41c64e6d + 0x3039, value (seed >> 16 & 0x7fff) * 2^-15 * 0x65 truncated; computed in
  // log/c3/alt/c3dr_wobble_seeds.py).
  const WOBBLE = [0xd6de9107, 0xae179107, 0xeb0a9107, 0xa61f9107, 0xf3029107, 0x9e279107, 0xfafa9107, 0x89fb9107,
                  0x0f269107, 0x65df9107, 0x47dd9107, 0x3d189107, 0x5c099107, 0x20f49107, 0x8cc89107, 0x64f09107,
                  0x48cc9107, 0x81149107, 0x180d9107];
  WOBBLE.forEach(function (seed, i) {
    fx['c3dr_wobble_' + i] = function () {
      __freeze.thread();
      __freeze.setU32(ptr('0x00822d9c'), seed);
      return {};
    };
  });

  // matchInputCode: the ring 0x0083aba4..0x0083abad (10 bytes) holds 'A'..'J'; the pointer 0x004e42ec is set to ring
  // index PI. Buffers: m<n> matches the n bytes ending at the ring pointer; x<n>_<j> flips the byte that is compared at
  // step j (the j-th from the end of the buffer).
  const RING = 0x0083aba4;
  const MATCH_PI = [9, 5, 0, 1];
  const MATCH_N = [1, 2, 3, 4, 5, 9, 10, 11, 12];
  const MATCH_X = [[3, 0], [3, 1], [3, 2], [10, 9], [12, 11], [12, 0], [5, 4], [2, 1]];
  MATCH_PI.forEach(function (pi) {
    fx['c3dr_match_' + pi] = function () {
      __freeze.thread();
      __freeze.save(ptr(RING), 0x10);
      __freeze.save(ptr('0x004e42ec'), 4);
      for (let i = 0; i < 10; i++) ptr(RING + i).writeU8(0x41 + i);
      ptr('0x004e42ec').writePointer(ptr(RING + pi));
      const out = {};
      function seq(n) {            // the bytes the loop reads from the ring: step k reads ring[(pi - k) mod 10]
        const s = [];
        for (let k = 0; k < n; k++) s.push(0x41 + (((pi - k) % 10) + 10) % 10);
        return s;
      }
      function buf(n, flipStep) {
        const b = __keepAlloc(Math.max(n, 1) + 4);
        const s = seq(n);
        for (let k = 0; k < n; k++) b.add(n - 1 - k).writeU8(k === flipStep ? s[k] ^ 0x20 : s[k]);
        return b;
      }
      out.m0 = __keepAlloc(8);
      MATCH_N.forEach(function (n) { out['m' + n] = buf(n, -1); });
      MATCH_X.forEach(function (x) { out['x' + x[0] + '_' + x[1]] = buf(x[0], x[1]); });
      return out;
    };
  });

  // zPush: the list 0x0083a2d8 holds `count` entries (a distinct fake window pointer each) and the count 0x0083ab94 is
  // set; windows are fixture blocks of 0xa8 bytes: wf (flag dword +0x9c = 0x2000000 | 0x11), wn (0x00000011), wz (0).
  const ZCOUNTS = [0, 1, 2, 5, 9, -1, -3];
  ZCOUNTS.forEach(function (c, ci) {
    fx['c3dr_zpush_' + ci] = function () {
      __freeze.thread();
      __freeze.save(ptr('0x0083a2d8'), 0x30);
      __freeze.save(ptr('0x0083ab94'), 4);
      for (let i = 0; i < 12; i++) ptr('0x0083a2d8').add(4 * i).writeU32(0x7e000000 + 0x100 * ci + i);
      ptr('0x0083ab94').writeS32(c);
      const out = {};
      const mk = function (flag) { const b = __keepAlloc(0xa8); b.writeByteArray(new Array(0xa8).fill(0)); b.add(0x9c).writeU32(flag); return b; };
      out.wf = mk(0x2000011); out.wn = mk(0x11); out.wz = mk(0); out.wf2 = mk(0x2000000);
      return out;
    };
  });

  // ---------------------------------------------------------------- Part 3: fixture-owned blocks
  function blocks(n, size, fill) {
    const out = {};
    for (let i = 0; i < n; i++) {
      const b = __keepAlloc(size);
      const f = fill(i);
      for (let j = 0; j < size; j += 4) b.add(j).writeU32(f(j) >>> 0);
      out['b' + i] = b;
    }
    return out;
  }
  const FILLS = [() => 0, () => 0xffffffff, () => 0xa5a5a5a5, () => 0x5a5a5a5a, () => 0xcdcdcdcd, () => 0xcccccccc,
                 (j) => 0x1000 + j, (j) => 0x7fff0000 - j, (j) => (j * 0x9e3779b1), (j) => 0x80000000 + j,
                 () => 0x00000001, () => 0xfffffffe];
  fx.c3dr_cache = function () { const o = blocks(12, 0x60, (i) => FILLS[i]); o.obj = o.b0; return o; };
  fx.c3dr_keytable = function () { const o = blocks(12, 0x120, (i) => FILLS[i]); o.obj = o.b0; return o; };
  fx.c3dr_flicbase = function () { const o = blocks(12, 0x60, (i) => FILLS[i]); o.obj = o.b0; return o; };
  fx.c3dr_flic = function () { const o = blocks(12, 0x90, (i) => FILLS[i]); o.obj = o.b0; return o; };

  // Decoder482ae0 / 482b20: blocks a0..a3 get the class vtable at +0 (the flag-0 path reads it), f0..f3 are filled; the
  // placement-heap global 0x00839650 is 0 (g0) or a value (g1) under the frozen thread.
  function decoder(vt, g) {
    return function () {
      __freeze.thread();
      __freeze.save(ptr('0x00839650'), 4);
      ptr('0x00839650').writeU32(g);
      const out = blocks(8, 0x100, (i) => FILLS[i]);
      const d = ptr(vt + 4).readU32();
      if (d >= 0xf0) throw new Error('c3dr: unexpected vtable displacement ' + d);
      // a0..a3 = b0..b3 carry the class vtable at +0; f0..f3 = b4..b7 stay filled
      for (let i = 0; i < 4; i++) { out['a' + i] = out['b' + i]; out['a' + i].writeU32(vt); }
      for (let i = 0; i < 4; i++) out['f' + i] = out['b' + (4 + i)];
      out.obj = out.b0;
      return out;
    };
  }
  fx.c3dr_dec_ae0_g0 = decoder(0x004ba28c, 0);
  fx.c3dr_dec_ae0_g1 = decoder(0x004ba28c, 0x5a5a1234);
  fx.c3dr_dec_b20_g0 = decoder(0x004ba2b8, 0);
  fx.c3dr_dec_b20_g1 = decoder(0x004ba2b8, 0x5a5a1234);

  // Window::releaseScrollbars: windows w0..w15 (0x200 bytes) whose +0x150 / +0x154 / +0x158 hold children chosen by mask
  // (bit 0 +0x150, bit 1 +0x154, bit 2 +0x158); w<i> uses mask i % 8. A child is 0x20 bytes whose dword 0 is a vtable with
  // slot 0 = a recording stub (thiscall, one stack argument, `ret 4`): $rec+0 = byte cursor, entries (tag, ecx, arg).
  function codeArena() {
    const size = Process.pageSize;
    const base = __keepAlloc(size);
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      return p;
    };
  }
  function stubBytes(rec, tag) {
    const r = U32(rec.toUInt32());
    const b = [];
    b.push(0xA1, ...r);                                    // mov eax, [REC]
    b.push(0x05, ...r);                                    // add eax, REC
    b.push(0xC7, 0x00, ...U32(tag));                       // mov dword [eax], tag
    b.push(0x89, 0x48, 0x04);                              // mov [eax+4], ecx
    b.push(0x8B, 0x54, 0x24, 4, 0x89, 0x50, 8);            // mov edx, [esp+4]; mov [eax+8], edx
    b.push(0x81, 0x05, ...r, ...U32(12));                  // add dword [REC], 12
    b.push(0xA1, ...r);                                    // mov eax, [REC]
    b.push(0x35, ...U32(tag));                             // xor eax, tag
    b.push(0xC2, 4, 0x00);                                 // ret 4
    return b;
  }
  fx.c3dr_scroll = function () {
    const out = {};
    const emit = codeArena();
    const rec = __keepAlloc(0x400);
    rec.writeByteArray(new Array(0x400).fill(0));
    rec.writeU32(0x10);
    out.rec = rec; out.obj = rec;
    const vt = __keepAlloc(0x40);
    vt.writePointer(emit(stubBytes(rec, 0xD15C0000)));
    for (let i = 4; i < 0x40; i += 4) vt.add(i).writeU32(0);
    for (let w = 0; w < 16; w++) {
      const win = __keepAlloc(0x200);
      win.writeByteArray(new Array(0x200).fill(0x5a));
      const mask = w % 8;
      for (let k = 0; k < 3; k++) {
        let c = ptr(0);
        if ((mask >> k) & 1) {
          c = __keepAlloc(0x20);
          c.writePointer(vt);
          c.add(4).writeU32(0x0c000000 + 0x100 * w + k);
        }
        win.add(0x150 + 4 * k).writePointer(c);
      }
      out['w' + w] = win;
    }
    return out;
  };

  // Reader::scalarDtor: eight Reader objects (0x170 bytes, mask m = index): bit 0 a file handle block at +0x158 (the CRT
  // fclose 0x004a609f only records it), bit 1 a block at +0x15c and bit 2 a block at +0x160 (free 0x004a5007 only records
  // them; operator delete 0x004a4ffc tail-calls it). The order is c3cq's: flush queued, stand-in installed, then the freeze.
  fx.c3dr_reader = function () {
    __freeze.begin();
    __freeze.schedule();
    const h = __heapStandIn({ module: 'golf_clean.exe', free: [0xa5007, 0xa609f] });
    __freeze.thread();
    const out = { freelog: h.log };
    for (let m = 0; m < 8; m++) {
      const o = __keepAlloc(0x170);
      for (let j = 0; j < 0x170; j += 4) o.add(j).writeU32(0x5a5a0000 + m * 0x100 + (j >> 2));
      o.add(0x158).writePointer((m & 1) ? __keepAlloc(0x40) : ptr(0));
      o.add(0x15c).writePointer((m & 2) ? __keepAlloc(0x40) : ptr(0));
      o.add(0x160).writePointer((m & 4) ? __keepAlloc(0x40) : ptr(0));
      out['o' + m] = o;
    }
    out.obj = out.o0;
    return out;
  };

  Object.assign(globalThis.DIFF_FIXTURES, fx);
})();
