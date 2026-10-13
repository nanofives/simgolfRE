// Fixtures for batch c3ev (round 32, 2026-10-13): golf_clean.exe thunks, adjustors, deleting destructors, root-window
// input handlers, static initialisers and small live-table walkers. Every fixture is built on re/frida/js/uitree.js
// (heap stand-in arena = $heap, recording display, import stand-in = $implog, the game's first thread frozen, window-tree
// globals neutral, function stand-ins recording into $rec on the A/B thread only). Live globals a body writes are saved
// with __freeze.save / fill before they are patterned and restored before the thread resumes.
// Only synthetic values: no game text, no live game object handed to a function under test.
//
// Reimplementations: shim/src/re/c3ev.cpp. Registry: re/frida/registry.d/c3ev.py. Purposes: log/c3/c3ev_purpose.md.
(function () {
  const F = {};

  // A function stand-in's result depends only on its arguments (round-31 lesson): a hash of the stand-in's address, its
  // `this` and every stack argument.
  function mix(addr) {
    return function (self, a) {
      let h = (addr ^ (self ? ptr(self).toUInt32() : 0)) >>> 0;
      for (let i = 0; i < a.length; i++) h = (Math.imul(h ^ (Number(a[i]) >>> 0), 0x01000193) + i) >>> 0;
      return h >>> 0;
    };
  }
  // Every call of a stand-in is also appended, with ALL its stack arguments, to the key's argument log ALOG (an arena block,
  // so part of the compared $heap): uitree.js's recorder keeps only `this` and the first two arguments, and a callee whose
  // result the caller discards (logLine, drawWindow, the eh vector constructor) would otherwise hide its later arguments.
  // ALOG: word 0 = words written, then {addr, this, args...} per call. It lives outside the arena (the registry's UI_STATE
  // arena sizes stay those of the fixtures) and is the `$alog` state region of every key (registry UI_STATE).
  const ALOG = { p: null };
  function uiTree(opts) {
    const p = Memory.alloc(0x1000);                // before the freeze (no allocation while the first thread is down)
    for (let k = 0; k < 0x1000; k += 4) p.add(k).writeU32(0);
    const T = __uiTree(opts);
    ALOG.p = p;
    __freeze.atFlush(function () { ALOG.p = null; });
    const fx0 = T.fx;
    T.fx = function (named) { const o = fx0(named); o.alog = p; return o; };
    return T;
  }
  function alog(addr, self, a) {
    const p = ALOG.p;
    if (p === null) return;
    const words = [addr, self ? ptr(self).toUInt32() : 0].concat(a.map(function (x) { return Number(x) >>> 0; }));
    const n = p.readU32();
    for (let i = 0; i < words.length && n + 1 + i < 0x3ff; i++) p.add(4 * (n + 1 + i)).writeU32(words[i] >>> 0);
    p.writeU32(Math.min(n + words.length, 0x3fe));
  }
  function stand(addr, nargs, abi, ret) {
    return { addr: addr, nargs: nargs, abi: abi || 'thiscall', ret: function (self, a) {
      alog(addr, self, a);
      if (ret === undefined) return mix(addr)(self, a);
      return typeof ret === 'function' ? ret(self, a) : ret;
    } };
  }

  // A vtordisp-thunk argument with displacement d: a scratch block s whose first dword is d; the thunk's `this` is s + 4,
  // so `sub ecx, [ecx - 4]` gives s + 4 - d. Targets are stood in or are real objects reached through dispTo.
  function dispPtr(T, d) { const s = T.alloc(0x10); s.writeS32(d | 0); return s.add(4); }
  function dispTo(T, target) {
    const s = T.alloc(0x10);
    s.writeS32((s.toUInt32() + 4 - ptr(target).toUInt32()) | 0);
    return s.add(4);
  }
  const DISPS = [0, 4, 8, 0x1c, 0x24, 0x8c, 0xe0, 0x1fc, -4, -0x100, 0x7fffffff, -0x80000000];

  // ==================================================================================================================
  // c3ev_thunks: every vtordisp thunk / adjustor target stood in (thiscall, stack argument counts from each body's
  // `ret N`): ListBox::onScroll 0x0048c420 (ret 8), ListBox::build 0x0048a5d0, panelBuildRow 0x0049d6d0, panelCreate4a2400
  // 0x004a2400, panelBuild4a3880 0x004a3880 (ret 0x10), the deleting destructors 0x004a3050, 0x004a4980, 0x0049bf70,
  // 0x0043cbd0, 0x00488630, 0x004916f0, 0x0049c800 (ret 4). pN = dispPtr blocks with the displacements DISPS.
  // ==================================================================================================================
  const THUNK_STANDS = [stand(0x0048c420, 2), stand(0x0048a5d0, 4), stand(0x0049d6d0, 4), stand(0x004a2400, 4),
                        stand(0x004a3880, 4), stand(0x004a3050, 1), stand(0x004a4980, 1), stand(0x0049bf70, 1),
                        stand(0x0043cbd0, 1), stand(0x00488630, 1), stand(0x004916f0, 1), stand(0x0049c800, 1)];
  F.c3ev_thunks = function () {
    const T = uiTree({ arenaSize: 0x2000, stand: THUNK_STANDS });
    const named = { obj: T.arena };
    DISPS.forEach(function (d, i) { named['p' + i] = dispPtr(T, d); });
    return T.fx(named);
  };

  // ==================================================================================================================
  // c3ev_sdtor: scalar deleting destructors with their destructor stood in (thiscall, no stack argument: Button::dtor
  // 0x00488650, Dialog::dtor 0x00491710, globalObjDtor49c780 0x0049c780, EditBox::dtor 0x0043cb50, C404200_dtor
  // 0x00404200 end `ret`; screenObj_deleteDtor's 0x0049bfc0 is `jmp 0x004853d0`). oN: arena blocks of 0x80 bytes whose
  // +0x60 (screenObj_deleteDtor's saved word) is P60[N]; operator delete 0x004a4ffc and free 0x004a5007 are recorded in
  // $freelog by the heap stand-in.
  // ==================================================================================================================
  // (second tranche) also Snd::dtor 0x004841e0, SndMusic::dtor 0x004848a0, Snd::setVtbl4852d0 0x004852d0 and Screen::dtor
  // 0x004853d0 (all end `ret`) for the Snd / Screen scalar deleting destructors.
  // 0x0049bfc0 is `jmp 0x004853d0`: Frida's Interceptor follows a leading jmp, so standing in 0x004853d0 is what covers
  // screenObj_deleteDtor's call (listing both raises `already replaced this function`).
  const SDTOR_STANDS = [stand(0x00488650, 0), stand(0x00491710, 0), stand(0x0049c780, 0), stand(0x0043cb50, 0),
                        stand(0x00404200, 0), stand(0x004841e0, 0), stand(0x004848a0, 0), stand(0x004852d0, 0),
                        stand(0x004853d0, 0)];
  const P60 = [0, 1, 0, 0x5000, 0, -1, 0, 7, 0, 0x80000000, 0, 2];
  F.c3ev_sdtor = function () {
    const T = uiTree({ arenaSize: 0x2000, stand: SDTOR_STANDS });
    const named = { obj: T.arena };
    P60.forEach(function (v, i) {
      const o = T.alloc(0x80);
      for (let k = 0; k < 0x80; k += 4) o.add(k).writeU32((0x0b0e0000 + (i << 8) + k) >>> 0);
      o.add(0x60).writeS32(v);
      named['o' + i] = o;
    });
    T.globals({ cookie: 0x0c00c1e4 });
    return T.fx(named);
  };

  // ==================================================================================================================
  // c3ev_real: real objects, no stand-in. bN Buttons (Button::ctor 0x00488500), eN EditBoxes (0x00486070); bN_s / eN_s =
  // the object + 0x274 (the adjustors' `this`). Rows: none (the destructors' own keys in c3ef cover decorated trees).
  // ==================================================================================================================
  F.c3ev_real = function () {
    const T = uiTree({ arenaSize: 0x10000 });
    const named = { obj: T.arena };
    for (let i = 0; i < 4; i++) {
      const b = T.make('Button'); named['b' + i] = b; named['b' + i + '_s'] = b.add(0x274);
      const e = T.make('EditBox'); named['e' + i] = e; named['e' + i + '_s'] = e.add(0x274);
    }
    T.globals({ cookie: 0x0c00c1e4 });
    return T.fx(named);
  };

  // ==================================================================================================================
  // c3ev_parts: stand-alone parts (build = 1) for the panel deleting destructors. faN Field4a2250 (Window base +0x8c,
  // ListModel base +0x604), psN PanelSub (Window base +0xe0, ListModel base +0x658); _w = the Window base (the `this` the
  // destructor takes), _t = a vtordisp-thunk argument whose displacement leads to _w. ROWS[N] rows are appended to the
  // part's ListModel base (ListModel::append 0x00489890), so ListModel::dtor 0x00489370 frees nodes in some vectors.
  // ==================================================================================================================
  const ROWS = [0, 1, 3, 0, 2, 5];
  F.c3ev_parts = function () {
    const T = uiTree({ arenaSize: 0x20000 });
    const named = { obj: T.arena };
    [['fa', 'Field4a2250', 0x8c, 0x604], ['ps', 'PanelSub', 0xe0, 0x658]].forEach(function (pp) {
      for (let i = 0; i < ROWS.length; i++) {
        const p = T.make(pp[1]);
        const w = p.add(pp[2]);
        named[pp[0] + i] = p; named[pp[0] + i + '_w'] = w; named[pp[0] + i + '_t'] = dispTo(T, w);
        for (let r = 0; r < ROWS[i]; r++) {
          const rc = T.append(p.add(pp[3]), T.str(pp[0] + i + 'r' + r), 0x40 + 0x10 * i + r);
          if (rc !== 0) throw new Error('c3ev: ListModel::append returned ' + rc);
        }
      }
    });
    T.globals({ cookie: 0x0c00c1e4 });
    return T.fx(named);
  };

  // ==================================================================================================================
  // Root-window input handlers. Globals they write (saved and patterned under the freeze): 0x00822d58 .. 0x00822d78
  // (press positions and flags), 0x00822d8c, 0x0083821c, 0x00822b98, 0x00822b9c, 0x00838208, the byte 0x0083ab9c; read:
  // the screen width 0x00822c8c (scaleX 0x00404970: (w * 2 + 0xa0) / 320 must stay >= 1, so w >= 80) and the key-timer
  // 0x005aa774 compared with timeGetTime (import stand-in, constant per variant).
  // ==================================================================================================================
  const IN_GLOBALS = [0x00822d58, 0x00822d5c, 0x00822d60, 0x00822d64, 0x00822d68, 0x00822d6c, 0x00822d70, 0x00822d74,
                      0x00822d78, 0x00822d8c, 0x0083821c, 0x00822b98, 0x00822b9c, 0x00838208];
  function inputEnv(v) {
    const T = uiTree({ arenaSize: 0x1000, imports: { 'WINMM.dll!timeGetTime': { ret: v.time >>> 0 } } });
    IN_GLOBALS.forEach(function (g, i) { __freeze.setU32(ptr(g), (0x5a5a0000 + 0x100 * v.seed + i) >>> 0); });
    __freeze.save(ptr(0x0083ab9c), 1); ptr(0x0083ab9c).writeU8(0xa5);
    __freeze.setU32(ptr(0x00822c8c), v.width >>> 0);
    __freeze.setU32(ptr(0x005aa774), v.timer >>> 0);
    return T.fx({ obj: T.arena });
  }
  // a: timer 0 (byte cleared); b: timer below the clock (cleared); c: timer above (kept); d: equal (kept, `jbe`);
  // e: timer 0xffffffff (kept); widths 800 / 640 / 1024 / 80 / 2000 (divisors 5 / 4 / 6 / 1 / 13).
  const IN_VARIANTS = {
    a: { seed: 1, width: 800, timer: 0, time: 1000 },
    b: { seed: 2, width: 640, timer: 999, time: 1000 },
    c: { seed: 3, width: 1024, timer: 1001, time: 1000 },
    d: { seed: 4, width: 80, timer: 1000, time: 1000 },
    e: { seed: 5, width: 2000, timer: 0xffffffff, time: 0x7fffffff },
  };
  Object.keys(IN_VARIANTS).forEach(function (k) { F['c3ev_input_' + k] = function () { return inputEnv(IN_VARIANTS[k]); }; });

  // ==================================================================================================================
  // tickAnimals 0x00401c00: the 100 records of 0x3c bytes at 0x0056d1d8 (live table, saved under the freeze); the first
  // word of record i is 0xffff unless MASK(i) is set; placeAnimal 0x004017d0 (cdecl, one argument) is stood in, so each
  // call is one $rec entry with its index.
  // ==================================================================================================================
  function animalsEnv(mask) {
    const T = uiTree({ arenaSize: 0x1000, stand: [stand(0x004017d0, 1, 'mscdecl')] });
    const base = ptr(0x0056d1d8);
    __freeze.save(base, 100 * 0x3c);
    for (let i = 0; i < 100; i++) {
      const r = base.add(i * 0x3c);
      for (let k = 0; k < 0x3c; k += 4) r.add(k).writeU32((0x41000000 + (i << 8) + k) >>> 0);
      r.writeU16(mask(i) ? (i & 0x7fff) : 0xffff);
    }
    return T.fx({ obj: T.arena });
  }
  F.c3ev_animals_none = function () { return animalsEnv(function () { return false; }); };
  F.c3ev_animals_all = function () { return animalsEnv(function () { return true; }); };
  F.c3ev_animals_odd = function () { return animalsEnv(function (i) { return (i & 1) === 1; }); };
  F.c3ev_animals_ends = function () { return animalsEnv(function (i) { return i === 0 || i === 99 || i === 37; }); };
  F.c3ev_animals_fffe = function () {                     // first word 0xfffe / 0x7fff / 0: not 0xffff, so called
    const fx = animalsEnv(function (i) { return i % 7 === 0; });
    ptr(0x0056d1d8).add(3 * 0x3c).writeU16(0xfffe);
    ptr(0x0056d1d8).add(4 * 0x3c).writeU16(0x0000);
    return fx;
  };

  // ==================================================================================================================
  // resetPair 0x004041c0: Stream482dd0::dtor 0x00482dd0 and Flic::reset 0x00481ba0 (thiscall, no stack argument) stood
  // in: the vectors' indexes only reach the recorders as `this` (0x0050fc20 + i * 0x44, 0x004f65c8 + i * 0x84).
  // ==================================================================================================================
  F.c3ev_pair = function () {
    const T = uiTree({ arenaSize: 0x1000, stand: [stand(0x00482dd0, 0), stand(0x00481ba0, 0)] });
    return T.fx({ obj: T.arena });
  };

  // ==================================================================================================================
  // initSinTable 0x00491c10: g_sinTable 0x0083b9f4 (256 dwords) and the dword after it (read by sinScaled at index 255
  // + 1) saved and filled with the variant's byte under the freeze; sinScaled / cosScaled run real (C3 leaves).
  // ==================================================================================================================
  function sinEnv(byte) {
    const T = uiTree({ arenaSize: 0x1000 });
    __freeze.fill(ptr(0x0083b9f4), 0x404, byte);
    return T.fx({ obj: T.arena });
  }
  F.c3ev_sin_00 = function () { return sinEnv(0x00); };
  F.c3ev_sin_a5 = function () { return sinEnv(0xa5); };
  F.c3ev_sin_7f = function () { return sinEnv(0x7f); };

  // ==================================================================================================================
  // staticInit_404360 0x00404360 on arena blocks vN (0x580 bytes, View4804a0::ctor 0x004804a0 real); staticInit_00404320
  // on the live root window 0x00519a60: the region 0x00519a50 .. +0x598 (c3co's dyn_00404310 region) saved and filled
  // with the variant's byte under the freeze, the recording display in 0x0083ad50 (uitree.js).
  // ==================================================================================================================
  F.c3ev_static = function () {
    const T = uiTree({ arenaSize: 0x4000 });
    const named = { obj: T.arena };
    for (let i = 0; i < 6; i++) {
      const v = T.alloc(0x580);
      for (let k = 0; k < 0x580; k += 4) v.add(k).writeU32((0x76000000 + (i << 12) + k) >>> 0);
      named['v' + i] = v;
    }
    return T.fx(named);
  };
  function rootEnv(byte) {
    const T = uiTree({ arenaSize: 0x1000 });
    __freeze.fill(ptr(0x00519a50), 0x598, byte);
    return T.fx({ obj: T.arena });
  }
  F.c3ev_root_00 = function () { return rootEnv(0x00); };
  F.c3ev_root_cd = function () { return rootEnv(0xcd); };
  F.c3ev_root_5a = function () { return rootEnv(0x5a); };

  // ==================================================================================================================
  // Link482::resolve 0x00482e20: lN arena blocks whose +4 is NULL (even N) or a 0x40-byte block (odd N); Image::step
  // 0x00482420 (thiscall, `ret 8`) stood in.
  // ==================================================================================================================
  F.c3ev_link = function () {
    const T = uiTree({ arenaSize: 0x2000, stand: [stand(0x00482420, 2)] });
    const named = { obj: T.arena };
    for (let i = 0; i < 12; i++) {
      const l = T.alloc(0x20);
      for (let k = 0; k < 0x20; k += 4) l.add(k).writeU32((0x4c000000 + (i << 8) + k) >>> 0);
      l.add(4).writePointer(i & 1 ? T.alloc(0x40) : ptr(0));
      named['l' + i] = l;
    }
    return T.fx(named);
  };

  // ==================================================================================================================
  // Second tranche.
  // ==================================================================================================================
  // c3ev_cdtor: C404200_dtor 0x00404200 / Dialog::subDtor 0x004914d0 with every callee stood in (Stream482dd0::dtor
  // 0x00482dd0, Surface_free 0x00473ae0, Dialog::viewDtor 0x00491500, ListModel::dtor 0x00489370, Base4805a0::dtor
  // 0x004805a0; thiscall, no stack argument). oN arena blocks of 0x40 bytes (C404200_dtor writes +0 and +8).
  F.c3ev_cdtor = function () {
    const T = uiTree({ arenaSize: 0x1000, stand: [stand(0x00482dd0, 0), stand(0x00473ae0, 0), stand(0x00491500, 0),
                                                     stand(0x00489370, 0), stand(0x004805a0, 0)] });
    const named = { obj: T.arena };
    for (let i = 0; i < 12; i++) {
      const o = T.alloc(0x40);
      for (let k = 0; k < 0x40; k += 4) o.add(k).writeU32((0xc4040000 + (i << 8) + k) >>> 0);
      named['o' + i] = o;
    }
    return T.fx(named);
  };

  // c3ev_scroll_<v>: scrollbarCreate 0x00496330 (thiscall, 7 stack arguments, `ret 0x1c`) stood in; the globals
  // 0x0083ff10 (read) and 0x0083ff60 (written by the wrappers) saved and patterned under the freeze.
  function scrollEnv(g10, g60) {
    const T = uiTree({ arenaSize: 0x1000, stand: [stand(0x00496330, 7)] });
    __freeze.setU32(ptr(0x0083ff10), g10 >>> 0);
    __freeze.setU32(ptr(0x0083ff60), g60 >>> 0);
    return T.fx({ obj: T.arena });
  }
  F.c3ev_scroll_a = function () { return scrollEnv(0x5c10a000, 0); };
  F.c3ev_scroll_b = function () { return scrollEnv(0x7fff0001, 0x5c60b000); };

  // c3ev_link2: lN blocks whose +4 is NULL (even N) or a block (odd N); Image::seek 0x00482490 (thiscall, `ret 0x10`)
  // stood in, for Link482b::send.
  F.c3ev_link2 = function () {
    const T = uiTree({ arenaSize: 0x2000, stand: [stand(0x00482490, 4)] });
    const named = { obj: T.arena };
    for (let i = 0; i < 12; i++) {
      const l = T.alloc(0x20);
      for (let k = 0; k < 0x20; k += 4) l.add(k).writeU32((0x4b000000 + (i << 8) + k) >>> 0);
      l.add(4).writePointer(i & 1 ? T.alloc(0x40) : ptr(0));
      named['l' + i] = l;
    }
    return T.fx(named);
  };

  // c3ev_image: Image objects for Image::step / Image::seek; decodeImageChunks 0x00482570 (thiscall, `ret 0xc`) stood in.
  //   chunk buffer C: 0x40 chunks of 0x10 bytes, each starting with its length 0x10 (the advance at 0x00482468 /
  //   0x0048247f / 0x00482544 adds the dword the chunk pointer points at); the frame table (32 entries) points at chunk
  //   (3 * k) % 0x30.
  //   imN: +0x48 the table, +0x58 the header; header words +0x60 (slot modulus) / +0x62 (steps per frame) and the mask
  //   +0x70 per IMG[N]; im3 has +0x58 = NULL.
  //   lkN: links (+0x38 chunk pointer into C, +0x3c counter, +0x40 slot) per LNK[N]; counters stay below every +0x62
  //   (the seek loop only ends when the counter reaches its target).
  const IMG = [[4, 3, 0], [7, 5, 0x0000000b], [1, 8, 0xffffffff], null, [32, 2, 0x80000001]];
  const LNK = [[0, 0, 0], [5, 1, 1], [9, 1, 2], [0x20, 0, 7], [0x11, 1, 0]];
  F.c3ev_image = function () {
    const T = uiTree({ arenaSize: 0x4000, stand: [stand(0x00482570, 3)] });
    const named = { obj: T.arena };
    const C = T.alloc(0x400);
    for (let k = 0; k < 0x400; k += 4) C.add(k).writeU32(k % 0x10 === 0 ? 0x10 : (0xc0000000 + k) >>> 0);
    const tab = T.alloc(0x80);
    for (let k = 0; k < 32; k++) tab.add(4 * k).writePointer(C.add(0x10 * ((3 * k) % 0x30)));
    IMG.forEach(function (spec, i) {
      const im = T.alloc(0x80);
      for (let k = 0; k < 0x80; k += 4) im.add(k).writeU32((0x1a000000 + (i << 8) + k) >>> 0);
      im.add(0x48).writePointer(tab);
      if (spec === null) { im.add(0x58).writePointer(ptr(0)); } else {
        const h = T.alloc(0x80);
        for (let k = 0; k < 0x80; k += 4) h.add(k).writeU32(0);
        h.add(0x60).writeU16(spec[0]); h.add(0x62).writeU16(spec[1]); h.add(0x70).writeU32(spec[2] >>> 0);
        im.add(0x58).writePointer(h);
      }
      named['im' + i] = im;
    });
    LNK.forEach(function (spec, i) {
      const l = T.alloc(0x48);
      for (let k = 0; k < 0x48; k += 4) l.add(k).writeU32((0x1b000000 + (i << 8) + k) >>> 0);
      l.add(0x38).writePointer(C.add(0x10 * spec[0])); l.add(0x3c).writeU32(spec[1]); l.add(0x40).writeU32(spec[2]);
      named['lk' + i] = l;
    });
    return T.fx(named);
  };

  // c3ev_free_<v>: freeTable's three live tables (16 entries each, saved under the freeze): base 0x005a9370, width
  // 0x0053f3e8, height 0x005a5a2c; freeBlock 0x0043d520 (cdecl, 2) and Surface_free 0x00473ae0 (thiscall, 0) stood in.
  const FREE_ROWS = [[-1, 3, 3], [0, 0, 5], [2, 1, 1], [7, 2, 3], [0x100, 4, 1], [5, -2, 3], [9, 3, -1], [-1, 0, 0],
                     [1, 1, 4], [0x30, 2, 2], [-2, 1, 1], [0, 5, 1], [0xdd00, 1, 3], [3, 0, 0], [11, 1, 2], [-1, 9, 9]];
  function freeEnv(rot) {
    const T = uiTree({ arenaSize: 0x1000, stand: [stand(0x0043d520, 2, 'mscdecl'), stand(0x00473ae0, 0)] });
    [0x005a9370, 0x0053f3e8, 0x005a5a2c].forEach(function (a) { __freeze.save(ptr(a), 0x40); });
    for (let i = 0; i < 16; i++) {
      const r = FREE_ROWS[(i + rot) % 16];
      ptr(0x005a9370).add(4 * i).writeS32(r[0]);
      ptr(0x0053f3e8).add(4 * i).writeS32(r[1]);
      ptr(0x005a5a2c).add(4 * i).writeS32(r[2]);
    }
    return T.fx({ obj: T.arena });
  }
  F.c3ev_free_a = function () { return freeEnv(0); };
  F.c3ev_free_b = function () { return freeEnv(5); };

  // c3ev_tick_<v>: tickMessage's globals saved under the freeze and set per variant; drawWindow 0x0040d320 (cdecl, 4)
  // stood in; the source text 0x005a6d40 is our own string, the message buffer 0x0051a068 (0x100 bytes) is patterned.
  // [delay 0x005694a4, mode byte 0x00569498, counter 0x005a9ccc, arg 0x005a34ec, bit 15 of 0x0059e7b8, g_date
  // 0x00834170, left 0x005a7144]
  const TICK = {
    a: [3, 1, 0, 0x11, 0, 7, 5],             // delay running: decrement only
    b: [0, 1, 1, 0x12, 1, 7, 5],             // mode 1, count up 1 -> 2, flag bit set: stop after the draw
    c: [0, 1, 3, 0x13, 0, 9, 5],             // mode 1, count at 3: draw -1; date % 3 == 0: stop
    d: [0, 0, 0, 0x14, 0, 7, 1],             // other mode, count 0: draw -1; left 1 -> 0: mode cleared
    e: [0, 0, 5, 0x15, 0, 8, 5],             // other mode, count 5 -> 4, draw 5; left 5 -> 4: kept
    f: [0, 2, -2, 0x16, 0, -7, 0],           // mode 2, count -2 -> -3, draw -2; date -7 % 3 = -1; left 0 -> -1: cleared
    g: [0, 1, -5, 0x17, 0, 10, 2],           // mode 1, count -5 -> -4 (signed < 3)
    h: [-1, 0, 0, 0x18, 1, 7, 5],            // delay -1: decrement to -2 (non-zero)
  };
  function tickEnv(v) {
    const T = uiTree({ arenaSize: 0x1000, stand: [stand(0x0040d320, 4, 'mscdecl')] });
    __freeze.setU32(ptr(0x005694a4), v[0] >>> 0);
    __freeze.save(ptr(0x00569498), 1); ptr(0x00569498).writeU8(v[1]);
    __freeze.setU32(ptr(0x005a9ccc), v[2] >>> 0);
    __freeze.setU32(ptr(0x005a34ec), v[3] >>> 0);
    const f = ptr(0x0059e7b8).readU32();
    __freeze.setU32(ptr(0x0059e7b8), (v[4] ? (f | 0x8000) : (f & ~0x8000)) >>> 0);
    __freeze.setU32(ptr(0x00834170), v[5] >>> 0);
    __freeze.setU32(ptr(0x005a7144), v[6] >>> 0);
    __freeze.save(ptr(0x005a6d40), 0x40);
    ptr(0x005a6d40).writeUtf8String('c3ev tick ' + v.join(','));
    __freeze.fill(ptr(0x0051a068), 0x100, 0x6d);
    return T.fx({ obj: T.arena });
  }
  Object.keys(TICK).forEach(function (k) { F['c3ev_tick_' + k] = function () { return tickEnv(TICK[k]); }; });

  // c3ev_arr_<v>: arrayStaticInit_43d460 with the eh vector constructor 0x004a55d2 (stdcall, 5) stood in, Surface_ctor
  // 0x00473ab0 real on 0x005aa748 (0x2c bytes saved and filled); _atexit 0x004a56d2 (cdecl, 1) stood in with result 0
  // ('ok') or -1 ('fail') for dynInit_arrayStaticInit_43d460 (its registerExit_43d490 returns it).
  function arrEnv(byte, atexitRet) {
    const T = uiTree({ arenaSize: 0x1000, stand: [stand(0x004a55d2, 5, 'stdcall'), stand(0x004a56d2, 1, 'mscdecl', atexitRet)] });
    __freeze.fill(ptr(0x005aa748), 0x2c, byte);
    return T.fx({ obj: T.arena });
  }
  F.c3ev_arr_ok = function () { return arrEnv(0x3c, 0); };
  F.c3ev_arr_fail = function () { return arrEnv(0xc3, 0xffffffff); };

  // c3ev_log_<v>: initLogFile on the logger 0x00840930 (0x10 bytes saved and patterned): resolveAndOpenFile 0x004a00f0
  // (cdecl, 2) stood in with a non-zero handle ('ok') or NULL ('nofile'); fclose 0x004a609f (cdecl, 1) stood in; malloc
  // through the heap stand-in, the first allocation failing in 'nomem'; _atexit stood in (0 / -1) for dynInit_initLogFile.
  function logEnv(openRet, failFirst, atexitRet) {
    const T = uiTree({ arenaSize: 0x1000, stand: [stand(0x004a00f0, 2, 'mscdecl', openRet), stand(0x004a609f, 1, 'mscdecl'),
                                                     stand(0x004a56d2, 1, 'mscdecl', atexitRet)] });
    __freeze.save(ptr(0x00840930), 0x10);
    for (let k = 0; k < 0x10; k += 4) ptr(0x00840930).add(k).writeU32((0x10600000 + k) >>> 0);
    if (failFirst) T.failAfter(0);
    return T.fx({ obj: T.arena });
  }
  F.c3ev_log_ok = function () { return logEnv(0x5f00f0f0, false, 0); };
  F.c3ev_log_nofile = function () { return logEnv(0, false, 0xffffffff); };
  F.c3ev_log_nomem = function () { return logEnv(0x5f00f0f1, true, 0); };

  // ==================================================================================================================
  // Third tranche.
  // ==================================================================================================================
  // Recording vtables built at load time (no NativeCallback is created while the first thread is frozen): every slot
  // (thiscall, no stack argument) logs {tag | slot, this} through the current key's uitree recorder and answers a hash of
  // its `this` and slot.
  const VTLOG = { log: null };
  function loadVtable(tag) {
    const vt = Memory.alloc(0x100);
    for (let s = 0; s < 0x100; s += 4) {
      const slot = s;
      const cb = new NativeCallback(function (self) {
        if (VTLOG.log) VTLOG.log((tag | slot) >>> 0, self, 0, 0);
        return mix(tag | slot)(self, []);
      }, 'uint32', ['pointer'], 'thiscall');
      globalThis.__diffKeepAlive.push(cb);
      vt.add(s).writePointer(cb);
    }
    return vt;
  }
  const VT_SND = loadVtable(0x5d000000);
  const VT_SUB = loadVtable(0x5e000000);

  // c3ev_sndstop: sN blocks of 0x80 bytes, [sN] = VT_SND (slot +0x80 records), +0x40 = a block on VT_SUB (slot +0x14
  // records and answers) or NULL, +0x58 byte flags, +0x44 patterned (its bit 0 set in some).
  const STOP = [[1, 0, 0x5a5a5a5b], [0, 0, 0x11], [1, 2, 0xffffffff], [0, 2, 0], [1, 1, 0x80000001], [0, 3, 0x2],
                [1, 0xfd, 0x7], [0, 0xff, 0xfffffffe], [1, 6, 0x101], [0, 0x80, 0x55]];
  F.c3ev_sndstop = function () {
    const T = uiTree({ arenaSize: 0x2000 });
    VTLOG.log = T.log;
    __freeze.atFlush(function () { VTLOG.log = null; });
    const named = { obj: T.arena };
    STOP.forEach(function (v, i) {
      const s = T.alloc(0x80);
      for (let k = 0; k < 0x80; k += 4) s.add(k).writeU32((0x5d0d0000 + (i << 8) + k) >>> 0);
      s.writePointer(VT_SND);
      if (v[0]) { const o = T.alloc(0x20); o.writePointer(VT_SUB); s.add(0x40).writePointer(o); } else s.add(0x40).writePointer(ptr(0));
      s.add(0x58).writeU8(v[1]);
      s.add(0x44).writeU32(v[2] >>> 0);
      named['s' + i] = s;
    });
    return T.fx(named);
  };

  // c3ev_snddtor: dN blocks (0x80 bytes) whose +0x50 (a block or NULL) and +0x40 (a block or NULL) follow DTOR[N];
  // releaseSoundObj 0x00484110 (cdecl, 1) and Snd::unlinkFromList 0x004844e0 (thiscall, 0) recorders; operator delete
  // recorded in $freelog; Snd::setVtbl4852d0 real (it stores the vtable 0x004badec at [this]).
  const DTOR = [[0, 0], [1, 0], [0, 1], [1, 1], [1, 1], [0, 0], [1, 0], [0, 1], [1, 1], [0, 0]];
  F.c3ev_snddtor = function () {
    const T = uiTree({ arenaSize: 0x2000, stand: [stand(0x00484110, 1, 'mscdecl'), stand(0x004844e0, 0)] });
    const named = { obj: T.arena };
    DTOR.forEach(function (v, i) {
      const d = T.alloc(0x80);
      for (let k = 0; k < 0x80; k += 4) d.add(k).writeU32((0x5dd00000 + (i << 8) + k) >>> 0);
      d.add(0x50).writePointer(v[0] ? T.alloc(0x10) : ptr(0));
      d.add(0x40).writePointer(v[1] ? T.alloc(0x10) : ptr(0));
      named['d' + i] = d;
    });
    return T.fx(named);
  };

  // c3ev_movie: movie objects mN (0x400 bytes, +0xb8 flags). moviePlayFrame 0x0049ccc0 and movieStepReturn1 0x0049cb80
  // are stood in with answers read from the object itself: play answers byte [m + 0x200 + k] for its k-th call (counter
  // dword at m + 0x1fc), step byte [m + 0x300 + k] (counter at m + 0x2fc); past the listed bytes they answer 0, so every
  // loop ends. The answers depend only on compared memory (the arena). logLine 0x004a0320 (cdecl, 5) and movieReadFrame
  // 0x0049cb90 (thiscall, 0) are recorders; timeGetTime answers 0x00c3e000.
  function seqStand(addr, nargs, ctrOff, seqOff) {
    return { addr: addr, nargs: nargs, abi: 'thiscall', ret: function (self, a) {
      alog(addr, self, a);
      const m = ptr(self);
      const k = m.add(ctrOff).readU32();
      m.add(ctrOff).writeU32(k + 1);
      return k < 0xfc ? m.add(seqOff + k).readU8() : 0;
    } };
  }
  // [flags +0xb8, play answers, step answers]
  const MOVIE = [
    [0x0000, [1], [1]],                    // bit 15 clear: serviceMovieFrame returns the flags
    [0x8010, [1, 1, 0], [1, 1]],           // bit 4: straight to movieReadProcess, which skips its loop
    [0x8000, [5], [1]],                    // play answers 5: serviceMovieFrame stops there
    [0x8000, [0, 0], [0]],                 // play 0, decodeStart's step 0: readProcess with play 0 (no loop)
    [0x8000, [0, 0, 1, 1, 0], [1, 1, 1]],  // readProcess loops: step 1 / play 1 twice, ends on play 0
    [0x0010, [1], [1]],                    // bit 4 without bit 15
    [0x8000, [0, 3], [7]],                 // decodeStart returns step's 7
    [0x8000, [0, 0, 2, 2, 2, 2], [0]],     // decodeStart's step answers 0, then readProcess: play 2, its step 0 ends the loop
    [0x8000, [0, 9, 4, 4, 4, 4, 0], [0, 1, 1, 1, 1]],
    [0x9fff, [0, 0, 0], [0, 0]],           // every flag bit but 0x6000: bit 4 set
    [0x80ef, [1, 0], [1]],                 // bit 4 clear, play 1
    [0xffff7fff, [1], [1]],                // bit 15 clear with the rest set
    [0x8000, [3, 3], [0]],                 // readProcess called directly: play 3, its first step 0 ends the loop
    [0x8000, [2, 2, 0], [1, 1]],           // readProcess called directly: one more pass, ends on play 0
  ];
  F.c3ev_movie = function () {
    const T = uiTree({ arenaSize: 0x6000, imports: { 'WINMM.dll!timeGetTime': { ret: 0x00c3e000 } },
                         stand: [seqStand(0x0049ccc0, 0, 0x1fc, 0x200), seqStand(0x0049cb80, 1, 0x2fc, 0x300),
                                 stand(0x004a0320, 5, 'mscdecl'), stand(0x0049cb90, 0)] });
    const named = { obj: T.arena };
    MOVIE.forEach(function (v, i) {
      const m = T.alloc(0x400);
      for (let k = 0; k < 0x400; k += 4) m.add(k).writeU32(0);
      for (let k = 0; k < 0x1f0; k += 4) m.add(k).writeU32((0x30d00000 + (i << 12) + k) >>> 0);
      m.add(0xb8).writeU32(v[0] >>> 0);
      v[1].forEach(function (b, k) { m.add(0x200 + k).writeU8(b); });
      v[2].forEach(function (b, k) { m.add(0x300 + k).writeU8(b); });
      named['m' + i] = m;
    });
    return T.fx(named);
  };

  // ==================================================================================================================
  // Fourth tranche.
  // ==================================================================================================================
  // c3ev_keys: KeyTable objects kN (0x120 bytes): +4 the table name, +0x104 the entry count, +0x110 the packed entries
  // {dword value; char name[]}; nN / yN named strings for the name and key arguments (fixture comment in the registry).
  // Every key the vectors look up in a table whose count is too small for it is absent from that table's entries too, so
  // the scan never runs past them except in k3 (count 0, the key is entry 2: the scan stops on the match).
  const KTAB = [
    ['golfers.txt', 5, [['Alpha', 0x11], ['beta', 0x22], ['#gamma', 0x33], ['Delta', 0x7fffffff], ['', 0x55]]],
    ['COURSE.TXT', 1, [['one', 1], ['two', 2]]],
    ['x.dat', 3, [['a', -1], ['b', 0], ['c', 0x80000000]]],
    ['zero.txt', 0, [['p', 0x70], ['q', 0x71], ['r', 0x72]]],
  ];
  const KNAMES = ['golfers', 'GOLFERS.TXT', 'golfers.dat', 'golfer', 'course', 'x.dat', 'x', 'zero', 'Golfers.Txt', ''];
  const KKEYS = ['alpha', '#Beta', 'DELTA', '', 'missing', '##gamma', 'gamma', 'two', 'one', 'c', 'A', 'r', 'b'];
  F.c3ev_keys = function () {
    const T = uiTree({ arenaSize: 0x2000 });
    const named = { obj: T.arena };
    KTAB.forEach(function (t, i) {
      const k = T.alloc(0x120);
      for (let o = 0; o < 0x120; o += 4) k.add(o).writeU32((0x4b000000 + (i << 12) + o) >>> 0);
      k.add(4).writeUtf8String(t[0]);
      k.add(0x104).writeS32(t[1]);
      const e = T.alloc(0x100);
      for (let o = 0; o < 0x100; o += 4) e.add(o).writeU32(0);
      let c = 0;
      t[2].forEach(function (en) { e.add(c).writeS32(en[1] | 0); e.add(c + 4).writeUtf8String(en[0]); c += 4 + en[0].length + 1; });
      k.add(0x110).writePointer(e);
      named['k' + i] = k;
    });
    KNAMES.forEach(function (s, i) { named['n' + i] = T.str(s); });
    KKEYS.forEach(function (s, i) { named['y' + i] = T.str(s); });
    return T.fx(named);
  };

  // c3ev_jinit: cinfo blocks cN (0x200 bytes) with err +0 (error_exit = the JCB recorder at slot 0, msg_code at +0x14),
  // mem +4 (realize_virt_arrays at +0x18) and marker +0x154 (write_file_header at +4) on arena blocks, the flag fields
  // per JINIT[N]; the ten jinit_* callees are recorders (cdecl).
  // [raw_data_in +0xa8, arith_code +0xac, progressive_mode +0xdc, num_scans +0xa0, optimize_coding +0xb0]
  const JINIT = [[0, 0, 0, 1, 0], [1, 0, 0, 1, 0], [0, 1, 0, 1, 0], [0, 0, 1, 1, 0], [0, 0, 0, 2, 0], [0, 0, 0, 1, 1],
                 [1, 1, 1, 3, 1], [0, 0, 1, 0, 0], [0, 0, 0, -1, 0], [0, 1, 1, 1, 1], [2, 0, 0, 0x80000000, 0],
                 [0, 0, 5, 1, 7]];
  function loadJcb(tag) {                                   // load time: no NativeCallback while the first thread is down
    const cb = new NativeCallback(function (cinfo) {
      alog(tag, ptr(0), [cinfo.toUInt32()]);
      return mix(tag)(ptr(0), [cinfo.toUInt32()]);
    }, 'uint32', ['pointer'], 'mscdecl');
    globalThis.__diffKeepAlive.push(cb);
    return cb;
  }
  const JCB = { exit: loadJcb(0x6a000000), realize: loadJcb(0x6a000018), header: loadJcb(0x6a000154) };
  const JSTANDS = [stand(0x004b51e0, 2, 'mscdecl'), stand(0x004b4a60, 1, 'mscdecl'), stand(0x004b4040, 1, 'mscdecl'),
                   stand(0x004b3a80, 2, 'mscdecl'), stand(0x004b34b0, 1, 'mscdecl'), stand(0x004b1f30, 1, 'mscdecl'),
                   stand(0x004b12b0, 1, 'mscdecl'), stand(0x004b06f0, 2, 'mscdecl'), stand(0x004b0560, 2, 'mscdecl'),
                   stand(0x004afaf0, 1, 'mscdecl')];
  F.c3ev_jinit = function () {
    const T = uiTree({ arenaSize: 0x4000, stand: JSTANDS });
    const named = { obj: T.arena };
    JINIT.forEach(function (v, i) {
      const c = T.alloc(0x200);
      for (let o = 0; o < 0x200; o += 4) c.add(o).writeU32((0x6c000000 + (i << 12) + o) >>> 0);
      const err = T.alloc(0x40), mem = T.alloc(0x40), mk = T.alloc(0x40);
      [err, mem, mk].forEach(function (b, j) { for (let o = 0; o < 0x40; o += 4) b.add(o).writeU32((0x6e000000 + (i << 12) + (j << 8) + o) >>> 0); });
      err.writePointer(JCB.exit); mem.add(0x18).writePointer(JCB.realize); mk.add(4).writePointer(JCB.header);
      c.writePointer(err); c.add(4).writePointer(mem); c.add(0x154).writePointer(mk);
      c.add(0xa8).writeU32(v[0] >>> 0); c.add(0xac).writeU32(v[1] >>> 0); c.add(0xdc).writeU32(v[2] >>> 0);
      c.add(0xa0).writeU32(v[3] >>> 0); c.add(0xb0).writeU32(v[4] >>> 0);
      named['c' + i] = c;
    });
    return T.fx(named);
  };

  // c3ev_graphsy: loadGraphsyObject's imports stood in. gN path blocks (0x40 bytes: the path text at +0, LoadLibraryA's
  // answer at +0x20); GetProcAddress writes its handle argument into the arena dword FH and answers the GFAC export
  // recorder when bit 4 of the handle is set, NULL otherwise; GFAC answers NULL when bit 5 of [FH] is set, else display
  // copy D[([FH] >> 8) & 3] (arena copies of the recording display: their DISP_VT slots forward every other thread's call
  // to the real display, so the winmm timer 0x00486f90 can follow [0x0083ad50] to any of them). FreeLibrary answers 1.
  // [0x0084092c] is saved and patterned under the freeze.
  const GR = { fh: null, d: null };
  const GFAC = new NativeCallback(function () {
    alog(0x6f000000, ptr(0), []);
    if (GR.fh === null) return ptr(0);
    const v = GR.fh.readU32();
    return (v & 0x20) ? ptr(0) : GR.d[(v >>> 8) & 3];
  }, 'pointer', [], 'mscdecl');
  globalThis.__diffKeepAlive.push(GFAC);
  const GHANDLE = [0x10, 0x00, 0x30, 0x110, 0x210, 0x310, 0x7fff0010, 0xffffffef, 0x0, 0x80000010, 0x1010, 0x20];
  F.c3ev_graphsy = function () {
    const T = uiTree({ arenaSize: 0x2000, imports: {
      'KERNEL32.dll!LoadLibraryA': { args: 1, impl: function (a) { return a[0].add(0x20).readU32(); } },
      'KERNEL32.dll!GetProcAddress': { args: 2, impl: function (a) {
        GR.fh.writeU32(a[0].toUInt32());
        return (a[0].toUInt32() & 0x10) ? GFAC : ptr(0);
      } },
      'KERNEL32.dll!FreeLibrary': { args: 1, ret: 1 },
    } });
    const named = { obj: T.arena };
    GR.fh = T.alloc(4); GR.fh.writeU32(0);
    GR.d = [];
    for (let j = 0; j < 4; j++) {
      const d = T.alloc(0x40);
      for (let o = 0; o < 0x40; o += 4) d.add(o).writeU32(T.disp.add(o).readU32());
      GR.d.push(d);
    }
    __freeze.atFlush(function () { GR.fh = null; GR.d = null; });
    __freeze.setU32(ptr(0x0084092c), 0x09c0092c);
    GHANDLE.forEach(function (h, i) {
      const g = T.alloc(0x40);
      for (let o = 0; o < 0x40; o += 4) g.add(o).writeU32(0);
      g.writeUtf8String('c3ev_graphsy_' + i + '.dll');
      g.add(0x20).writeU32(h >>> 0);
      named['g' + i] = g;
    });
    return T.fx(named);
  };

  // c3ev_sprites_<v>: renderSpriteQueue over the live sprite list (head 0x0082c160, next table 0x008361f4, 64 dwords
  // saved and patterned under the freeze); drawQueuedSprite 0x00462be0 (cdecl, 1) a recorder, so the drawn order is $rec.
  const SPRQ = {
    empty: [-1, []],
    one: [5, [[5, -1]]],
    chain: [0, [[0, 3], [3, 1], [1, 63], [63, 2], [2, -1]]],
    long: [10, Array.from({ length: 40 }, function (_, k) { return [10 + k, k === 39 ? -1 : 11 + k]; })],
    zero: [0, [[0, -1]]],
  };
  function spritesEnv(v) {
    const T = uiTree({ arenaSize: 0x1000, stand: [stand(0x00462be0, 1, 'mscdecl')] });
    __freeze.save(ptr(0x008361f4), 0x100);
    for (let k = 0; k < 64; k++) ptr(0x008361f4).add(4 * k).writeU32((0x5e000000 + k) >>> 0);
    v[1].forEach(function (p) { ptr(0x008361f4).add(4 * p[0]).writeS32(p[1]); });
    __freeze.setU32(ptr(0x0082c160), v[0] >>> 0);
    return T.fx({ obj: T.arena });
  }
  Object.keys(SPRQ).forEach(function (k) { F['c3ev_sprites_' + k] = function () { return spritesEnv(SPRQ[k]); }; });

  // c3ev_prow: panelBuildRow on panels oN (0x800 bytes; `this` = oN + 0x59c): [oN] = a vbtable {0, o4, o8} (Window base
  // at +o4, the virtual base the row fields live in at +o8); panelLayoutText 0x0049d770 (thiscall, no stack argument) a
  // recorder answering [oN + 0x10]; Window::create 0x004806c0 (thiscall, 9) a recorder. wN owners (+0x9c flag words).
  // [answer, o4, o8]
  const PROW = [[0, 0x59c, 0x600], [1, 0x59c, 0x600], [0, 0, 0x100], [0xffffffff, 0x10, 0], [0, 0x200, 0x790],
                [0, 0x59c, 0x5a0]];
  const POWN = [0, 0x1000, 0xffffffff, 0xefff];
  F.c3ev_prow = function () {
    const T = uiTree({ arenaSize: 0x8000, stand: [stand(0x0049d770, 0, 'thiscall', function (self) { return ptr(self).add(0x10).readU32(); }),
                                                    stand(0x004806c0, 9)] });
    const named = { obj: T.arena };
    PROW.forEach(function (v, i) {
      const o = T.alloc(0x800);
      for (let k = 0; k < 0x800; k += 4) o.add(k).writeU32((0x70000000 + (i << 12) + k) >>> 0);
      const vbt = T.alloc(0x10);
      vbt.writeU32(0); vbt.add(4).writeU32(v[1]); vbt.add(8).writeU32(v[2]);
      o.writePointer(vbt);
      o.add(0x10).writeU32(v[0] >>> 0);
      named['o' + i] = o.add(0x59c);
    });
    POWN.forEach(function (f, i) {
      const w = T.alloc(0x100);
      for (let k = 0; k < 0x100; k += 4) w.add(k).writeU32((0x77000000 + (i << 8) + k) >>> 0);
      w.add(0x9c).writeU32(f >>> 0);
      named['w' + i] = w;
    });
    return T.fx(named);
  };

  // A stand-in that writes an output argument which may be a FRAME pointer (movieReadFrame's local buffer pointer, handed
  // to listPush): it is installed outside uitree.js because uitree's recorder logs the first argument as a value, and the
  // original's frame and the reimplementation's place that local differently. Here a pointer inside the A/B thread's stack
  // (TEB +4 / +0xe0c, framemask.js's helper) is logged as the token 0xF5000000 (one frame object), anything else as it is.
  // The written value is what the caller reads back, so a body that passed another slot reads a wrong buffer.
  // (re/frida/js/framemask.js does this generally; diff_hook.py does not load it yet.)
  const TEBCODE = Memory.alloc(Process.pageSize);
  Memory.protect(TEBCODE, Process.pageSize, 'rwx');
  TEBCODE.writeByteArray([0x64, 0xa1, 0x04, 0x00, 0x00, 0x00, 0xc3, 0x90, 0x64, 0xa1, 0x0c, 0x0e, 0x00, 0x00, 0xc3, 0x90]);
  const stackHi = new NativeFunction(TEBCODE, 'uint32', []);
  const stackLo = new NativeFunction(TEBCODE.add(8), 'uint32', []);
  globalThis.__diffKeepAlive.push(TEBCODE, stackHi, stackLo);
  function maskedStand(addr, nargs, write, ret, raw) {
    const at = ptr(addr);
    const tid = Process.getCurrentThreadId();
    const hi = raw ? 0 : stackHi() >>> 0, lo = raw ? 0 : stackLo() >>> 0;      // raw: the mask-off control
    const types = ['pointer'].concat(Array(nargs).fill('uint32'));
    const orig = new NativeFunction(at, 'uint32', types, 'thiscall');
    const cb = new NativeCallback(function () {
      const a = Array.prototype.slice.call(arguments);
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, a);
      const self = a.shift();
      const vals = a.map(function (x) { const v = Number(x) >>> 0; return (v >= lo && v < hi) ? 0xF5000000 : v; });
      alog(addr, self, vals);
      write(self, a);
      return ret(self, a) >>> 0;
    }, 'uint32', types, 'thiscall');
    globalThis.__diffKeepAlive.push(orig, cb);
    Interceptor.replace(at, cb);
    Interceptor.flush();
    setTimeout(function () { try { Interceptor.revert(at); } catch (e) { /* gone */ } Interceptor.flush(); }, 0);
  }

  // c3ev_mread: movie objects mN (0x1000 bytes): +0xd8 frame size, +0xb8 flags, +0xbc = an mmio answer block A (A[0] call
  // counter, A[4 + 4k] the k-th mmioRead result, 0 past the list), +0x940 / +0x674 patterned; listPush (masked stand-in
  // above) writes [mN + 0xa00] (a 0x100-byte arena buffer or NULL) through its first argument and answers [mN + 0xa04].
  // mmioRead (import stand-in) answers A's next value and writes the call number + 1 into the first byte of its buffer
  // when the answer is non-zero; logLine 0x004a0320 (cdecl, 5) and movieAdvanceFrame 0x0049c940 (thiscall, 0) recorders;
  // timeGetTime answers 0x00c3e100.
  // [size, listPush result, buffer?, flags, mmio answers]
  const MREAD = [
    [0, 0, 1, 0x8000, []], [0, 5, 1, 0x8000, []], [0x20, 0x12, 1, 0x8000, []], [0x20, 0, 0, 0x8000, []],
    [0x20, 0, 1, 0x8000, [0x20]], [0x20, 0, 1, 0x8000, [0x10, 8, 8]], [0x20, 0, 1, 0xffffffff, [0x10, 0]],
    [0x10010, 0, 1, 0x8000, [0x10, 0x10000]], [0x20, 0, 1, 0x8000, [0]], [0x20, 7, 1, 0x18000, [0x20]],
    [0x20, 0, 1, 0xc000, [0xffffffff, 0]], [3, 0, 1, 0x8001, [1, 1, 1]],
  ];
  function mreadEnv(raw) {
    maskedStand(0x004a4db0, 4,                             // before uiTree: the freeze follows there
                function (self, a) { ptr(a[0]).writeU32(ptr(self).sub(0x668).add(0xa00).readU32()); },
                function (self) { return ptr(self).sub(0x668).add(0xa04).readU32(); }, raw);
    const T = uiTree({ arenaSize: 0x10000, imports: {
      'WINMM.dll!timeGetTime': { ret: 0x00c3e100 },
      'WINMM.dll!mmioRead': { args: 3, impl: function (a) {
        const A = a[0];
        const k = A.readU32();
        A.writeU32(k + 1);
        const v = k < 30 ? A.add(4 + 4 * k).readU32() : 0;
        if (v !== 0 && !a[1].isNull()) a[1].writeU8((k + 1) & 0xff);
        return v >>> 0;
      } },
    }, stand: [stand(0x004a0320, 5, 'mscdecl'), stand(0x0049c940, 0)] });
    const named = { obj: T.arena };
    MREAD.forEach(function (v, i) {
      const m = T.alloc(0x1000);
      for (let k = 0; k < 0x1000; k += 4) m.add(k).writeU32((0x3e000000 + (i << 12) + k) >>> 0);
      m.add(0xd8).writeU32(v[0] >>> 0);
      m.add(0xa04).writeU32(v[1] >>> 0);
      if (v[2]) { const b = T.alloc(0x100); for (let k = 0; k < 0x100; k += 4) b.add(k).writeU32(0); m.add(0xa00).writePointer(b); }
      else m.add(0xa00).writePointer(ptr(0));
      m.add(0xb8).writeU32(v[3] >>> 0);
      const A = T.alloc(0x80);
      for (let k = 0; k < 0x80; k += 4) A.add(k).writeU32(0);
      v[4].forEach(function (x, k) { A.add(4 + 4 * k).writeU32(x >>> 0); });
      m.add(0xbc).writePointer(A);
      named['m' + i] = m;
    });
    return T.fx(named);
  }
  F.c3ev_mread = function () { return mreadEnv(false); };

  // c3ev_walls: applyTileWalls on tiles tN (0x240 bytes, the wall bytes +0x234 / +0x236 / +0x238 / +0x23a per TWALL[N]);
  // wallHeight 0x00449330 (cdecl, 3) a recorder whose answer depends only on (x, y, side): bit side / 2 of x ^ y set ->
  // 0x10 + side + (x & 0xff), else 0; Terrain::setWall's import thunk 0x004a4f40 (thiscall, 4) a recorder (its `this` is
  // the live Terrain pointer [0x00820ed0], only read).
  const TWALL = [[0, 0, 0, 0], [1, 1, 1, 1], [0, 1, 0, 1], [1, 0, 0, 0xff], [0x80, 0, 2, 0]];
  F.c3ev_walls = function () {
    const T = uiTree({ arenaSize: 0x2000, stand: [
      stand(0x00449330, 3, 'mscdecl', function (self, a) {
        const x = Number(a[0]) >>> 0, y = Number(a[1]) >>> 0, s = Number(a[2]) >>> 0;
        return ((x ^ y) >>> (s >>> 1)) & 1 ? (0x10 + s + (x & 0xff)) >>> 0 : 0;
      }),
      stand(0x004a4f40, 4)] });
    const named = { obj: T.arena };
    TWALL.forEach(function (w, i) {
      const t = T.alloc(0x240);
      for (let k = 0; k < 0x240; k += 4) t.add(k).writeU32((0x7a000000 + (i << 12) + k) >>> 0);
      w.forEach(function (b, k) { t.add(0x234 + 2 * k).writeU8(b); });
      named['t' + i] = t;
    });
    return T.fx(named);
  };

  // c3ev_newcourse_<v>: startNewCourse with its three screens stood in (cdecl): pickAProScreen 0x0043b610 (2) answers
  // v.pick and overwrites [0x00822c88] (so the restore is visible), loadGameBrowserScreen 0x0043a8c0 (0) answers v.browse,
  // loadGolfPro 0x0046c970 (0) writes 5 into [0x00571fd4] (so the second store of 1000 is visible). The four globals the
  // body writes are saved and patterned under the freeze.
  const NEWC = { a: { pick: 0, browse: 1 }, b: { pick: 1, browse: 0 }, c: { pick: 7, browse: 1 }, d: { pick: -1, browse: 0x80000000 } };
  function newCourseEnv(v) {
    const T = uiTree({ arenaSize: 0x1000, stand: [
      stand(0x0043b610, 2, 'mscdecl', function () { ptr(0x00822c88).writeU32(0x7777abcd); return v.pick >>> 0; }),
      stand(0x0043a8c0, 0, 'mscdecl', v.browse >>> 0),
      stand(0x0046c970, 0, 'mscdecl', function () { ptr(0x00571fd4).writeU32(5); return 0x46c970; })] });
    __freeze.setU32(ptr(0x00822c88), (0x22c80000 + (v.pick & 0xffff)) >>> 0);
    __freeze.setU32(ptr(0x00571fd4), 0x571f0001);
    __freeze.setU32(ptr(0x00834170), 0x00834171);
    __freeze.setU32(ptr(0x0059e7b8), (ptr(0x0059e7b8).readU32() ^ 0x00100000) >>> 0);
    return T.fx({ obj: T.arena });
  }
  Object.keys(NEWC).forEach(function (k) { F['c3ev_newcourse_' + k] = function () { return newCourseEnv(NEWC[k]); }; });

  // c3ev_play: moviePlayFrame on movie objects pN (0x1000 bytes; words 0..0x1f0 patterned, the rest zero, then the fields
  // of PLAY[N]). Every callee is a recorder whose answer comes from the object (compared memory):
  //   clock slot +0x94 on pN + 4 ([pN + 4] = VT_CLK, built at load time): answers [pN + 0x14];
  //   movieStepReturn1 0x0049cb80 (thiscall, 1): byte k of pN + 0x300 for its k-th call (counter pN + 0x2fc), 0 past the
  //     listed bytes, so every catch-up loop ends (the real body always answers 1: see the notes);
  //   movieReadFrame 0x0049cb90 (thiscall, 0): adds 1 to [pN + 0x944] and [pN + 0x940], answers 1;
  //   movieAdvanceFrame 0x0049c940 (thiscall, 0): adds 1 to [pN + 0x940];
  //   select 0x0049cf50 (thiscall, 1): answers [pN + 0x400];
  //   listTrim 0x004a4d30 (thiscall, 1), listRemoveHead 0x004a4ed0 (thiscall, 0), logLine 0x004a0320 (cdecl, 5): recorders;
  //   listPeek 0x004a4ea0 (thiscall, 2; its first argument is the caller's local, masked stand-in): writes dword k of
  //     pN + 0x500 (counter pN + 0x4fc), 0x7fffffff past the list;
  //   timeGetTime: 0x00c3e200.
  const PT = 0x00c3e200;
  // [flags, clock, start, rate, div, cur, ahead, count, f674, select, steps, peeks]
  const PLAY = [
    [0xc010, 0, 0, 0, 1, 5, 5, 6, 0, 0, [], []],         // (bit 14 set wherever the bit-15 clear runs: the mask is exact)
    [0, 3, 0, 0, 1, 5, 5, 100, 0, 0, [], []],
    [0, 5, 0, 0, 1, 5, 5, 100, 0, 0, [], []],
    [0x1000, 5, 0, 0, 1, 5, 5, 100, 0, 0, [], []],
    [0xc000, 6, 0, 0, 1, 5, 5, 7, 0, 0, [], []],
    [0, 6, 0, 0, 1, 5, 5, 100, 1, 0, [], []],
    [0, 10, 0, 0, 1, 5, 20, 100, 0, -1, [1, 1, 0, 0, 1, 0], []],
    [1, 0, PT - 4000, 3, 1, 5, 5, 100, 0, 20, [1, 0, 1, 0, 0, 1], []],
    [1, 0, PT - 3000, 7, 3, 5, 6, 100, 0, 8, [], []],
    [1, 0, PT - 3000, 7, 3, 5, 20, 100, 0, 8, [], [3, 5, 9]],
    [1, 0, PT + 1000, 1, 1, 4294960, 4294969, 100, 0, 4294970, [], []],
    [0, 9, 0, 0, 1, 5, 5, 100, 0, 100, [0, 1, 1, 1, 0, 1], []],
    [0, 8, 0, 0, 1, 5, 5, 100, 0, 50, [0, 0, 1], []],
    [0x1001, 0, PT - 0x10000, 0x10001, 7, 9, 9, 100, 0, 0, [], []],
    [0, 7, 0, 0, 1, 5, 7, 100, 0, -1, [1, 0], [-5, 0]],
    [0xffffffee, 6, 0, 0, 1, 5, 5, 7, 0, 0, [], []],     // every flag bit but 0 and 4: only bits 12 and 15 may change
    [0, 9, 0, 0, 1, 5, 20, 100, 0, 13, [], [3, 20]],     // t - c == s - t (4 == 4): `jge` takes the second branch
    [1, 0, PT - 3000, 7, 3, 5, 9, 100, 0, 8, [], [8]],   // s == [0x940] - 1 (8): `jle` takes the peek arm, k == s at once
  ];
  const VT_CLK = (function () {
    const vt = Memory.alloc(0x100);
    const cb = new NativeCallback(function (self) {
      alog(0x6c000094, self, []);
      return self.add(0x10).readU32();
    }, 'uint32', ['pointer'], 'thiscall');
    globalThis.__diffKeepAlive.push(vt, cb);
    for (let s = 0; s < 0x100; s += 4) vt.add(s).writePointer(cb);
    return vt;
  })();
  function bump(p, off) { p.add(off).writeU32((p.add(off).readU32() + 1) >>> 0); }
  F.c3ev_play = function () {
    maskedStand(0x004a4ea0, 2,                             // before uiTree: the freeze follows there
                function (self, a) {
                  const m = ptr(self).sub(0x668);
                  const k = m.add(0x4fc).readU32();
                  m.add(0x4fc).writeU32(k + 1);
                  ptr(a[0]).writeU32(k < 0x40 ? m.add(0x500 + 4 * k).readU32() : 0x7fffffff);
                },
                function () { return 0; });
    const T = uiTree({ arenaSize: 0x14000, imports: { 'WINMM.dll!timeGetTime': { ret: PT } },
                       stand: [seqStand(0x0049cb80, 1, 0x2fc, 0x300),
                               stand(0x0049cb90, 0, 'thiscall', function (self) { bump(ptr(self), 0x944); bump(ptr(self), 0x940); return 1; }),
                               stand(0x0049c940, 0, 'thiscall', function (self) { bump(ptr(self), 0x940); return 0; }),
                               stand(0x0049cf50, 1, 'thiscall', function (self) { return ptr(self).add(0x400).readU32(); }),
                               stand(0x004a4d30, 1), stand(0x004a4ed0, 0), stand(0x004a0320, 5, 'mscdecl')] });
    const named = { obj: T.arena };
    PLAY.forEach(function (v, i) {
      const m = T.alloc(0x1000);
      for (let k = 0; k < 0x1000; k += 4) m.add(k).writeU32(k < 0x1f0 ? (0x91a00000 + (i << 12) + k) >>> 0 : 0);
      m.add(0xb8).writeU32(v[0] >>> 0);
      m.add(4).writePointer(VT_CLK); m.add(0x14).writeU32(v[1] >>> 0);
      m.add(0x93c).writeU32(v[2] >>> 0); m.add(0x13c).writeU32(v[3] >>> 0); m.add(0x138).writeU32(v[4] >>> 0);
      m.add(0x944).writeU32(v[5] >>> 0); m.add(0x940).writeU32(v[6] >>> 0); m.add(0x938).writeU32(v[7] >>> 0);
      m.add(0x674).writeU32(v[8] >>> 0); m.add(0x400).writeU32(v[9] >>> 0);
      v[10].forEach(function (b, k) { m.add(0x300 + k).writeU8(b); });
      v[11].forEach(function (x, k) { m.add(0x500 + 4 * k).writeU32(x >>> 0); });
      if (v[11].length === 0) m.add(0x500).writeU32(0x7fffffff);
      for (let k = Math.max(1, v[11].length); k < 0x40; k++) m.add(0x500 + 4 * k).writeU32(0x7fffffff);
      named['p' + i] = m;
    });
    return T.fx(named);
  };
  // mask-off control, run once as a temporary key into log/c3/alt/c3ev_masktest (no registry key now): GREEN as well, i.e.
  // both arms passed listPush the same stack address, so on this build the mask changes nothing for this key
  F.c3ev_mread_raw = function () { return mreadEnv(true); };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
