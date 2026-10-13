// Fixtures for batch c3eq (round 31, 2026-10-13): golf_clean.exe round-30 leftovers and the Bink video player.
// Reimplementations: shim/src/re/c3eq.cpp. Registry: re/frida/registry.d/c3eq.py. Purposes and per-jcc branch tables:
// log/c3/c3eq_purpose.md. Notes: log/c3/c3eq_notes.txt.
//
// Every key runs under __uiTree (re/frida/js/uitree.js): heap stand-in arena, import stand-in, frozen first thread,
// recording display at 0x0083ad50, neutral window-tree globals. The binkw32.dll imports are served by the import
// stand-in, so nothing reaches the real video player, and the call log is compared state.
(function () {
  const F = globalThis.DIFF_FIXTURES;

  function junk(p, n, tag) { for (let k = 0; k < n; k += 4) p.add(k).writeU32((tag + k * 0x01000193) >>> 0); }

  // ====================================================================================================================
  // BinkPlayer::play 0x00487090 and playAudioFile 0x0043cce0
  //
  // Players (0x18 bytes: +4 the Bink handle the function stores, +8 the buffer it stores, +0xc the target window,
  // +0x10 / +0x14 the two offsets). The path argument is a block whose first dword is the Bink handle _BinkOpen answers
  // for it (0 = the open fails) and whose bytes from +8 are a name, so that stub is a pure function of its argument;
  // _BinkBufferOpen is keyed by its width argument (handle +0), which is unique per row.
  // A Bink handle is 0x40 bytes: +0 width, +4 height, +8 frame count and +0xc current frame (BinkPlayer::frame 0x00487180
  // compares them), +0x1c the error word 0x004870de reads, +0x34 the rectangle _BinkBufferBlit gets. Three fields are the
  // fixture's own script and no code reads them: +0x24 the bit pattern _BinkWait answers (bit k at call k), +0x28 the
  // call cursor, +0x2c the cursor value at which _BinkWait clears the byte 0x0083ab9c (-1 = never). They live in the
  // arena, which is compared state, so both arms run the same sequence.
  // ====================================================================================================================
  // [target != 0, openOk, err1c, bufferOk, oddBuffer, waitPattern, clearAt, frames left, offX, offY]
  const PLAY = [
    [0, 1, 0, 1, 1, 0, -1, 2, 0, 0],                 //  0 no target window: returns 7
    [1, 0, 0, 1, 1, 0, -1, 2, 0, 0],                 //  1 _BinkOpen answers 0: returns 1
    [1, 1, 1, 1, 1, 0, -1, 2, 0, 0],                 //  2 handle +0x1c set: returns 1
    [1, 1, 0, 0, 1, 0, -1, 2, 0, 0],                 //  3 _BinkBufferOpen answers 0: returns 1
    [1, 1, 0, 1, 1, 0, -1, 0, 0, 0],                 //  4 the movie is already at its last frame: one frame call
    [1, 1, 0, 1, 1, 0, -1, 3, 12, 34],               //  5 three frames, offsets 12 / 34
    [1, 1, 0, 1, 0, 0, -1, 2, -1, -2],               //  6 even buffer: _BinkBufferLock answers 0 inside frame
    [1, 1, 0, 1, 1, 0x5, -1, 4, 7, 0],               //  7 waits at calls 0 and 2: the pump path twice
    [1, 1, 0, 1, 1, 0xff, -1, 1, 0, 0],              //  8 eight waits in a row, then one frame
    [1, 1, 0, 1, 1, 0, 3, 9, 0, 0],                  //  9 the byte 0x0083ab9c is cleared at call 3: the flag exit
    [1, 1, 0, 1, 0, 0x2, 4, 9, 1, 1],                // 10 one wait, then the flag exit
    [1, 1, 0, 1, 1, 0, 1, 9, 0x7fffffff, -0x80000000],   // 11 the flag exit on the first call
    [1, 1, 1, 1, 1, 0, -1, 2, 5, 6],                 // 12 handle +0x1c set, with offsets: still returns 1
    [1, 1, 0, 1, 1, 0x9, -1, 2, 0, 0],               // 13 waits at calls 0 and 3
  ];

  // The four binkw32 entry points BinkPlayer::play itself calls, plus the seven of BinkPlayer::frame 0x00487180 and the
  // two of BinkPlayer::close 0x00487060 (both run real under these keys).
  function binkImports(bufFor, binkFor) {
    return {
      'binkw32.dll!_BinkSetSoundSystem@8': { args: 2, ret: 1 },
      'binkw32.dll!_BinkSetIOSize@4': { args: 1, ret: 0 },
      'binkw32.dll!_BinkOpen@8': { args: 2, impl: function (a) { return binkFor(a[0]); } },
      'binkw32.dll!_BinkBufferOpen@16': { args: 4, impl: function (a) { return bufFor(a[1]); } },
      'binkw32.dll!_BinkBufferSetOffset@12': { args: 3, ret: 0 },
      'binkw32.dll!_BinkWait@4': { args: 1, impl: function (a) {
        const h = a[0];
        const cur = h.add(0x28).readU32();
        const bit = (h.add(0x24).readU32() >>> (cur & 31)) & 1;
        h.add(0x28).writeU32(cur + 1);
        if (h.add(0x2c).readU32() === cur + 1) ptr(0x0083ab9c).writeU8(0);
        return bit;
      } },
      // BinkPlayer::frame 0x00487180 (c3dl's hook runs in both arms)
      'binkw32.dll!_BinkDoFrame@4': { args: 1, ret: 0 },
      'binkw32.dll!_BinkBufferLock@4': { args: 1, impl: function (a) { return a[0].toUInt32() & 1; } },
      'binkw32.dll!_BinkCopyToBuffer@28': { args: 7, ret: 0 },
      'binkw32.dll!_BinkBufferUnlock@4': { args: 1, ret: 0 },
      'binkw32.dll!_BinkGetRects@8': { args: 2, impl: function (a) { return (a[1].toUInt32() + 0x77) >>> 0; } },
      'binkw32.dll!_BinkBufferBlit@12': { args: 3, ret: 0x1234 },
      'binkw32.dll!_BinkNextFrame@4': { args: 1, impl: function (a) {
        a[0].add(0xc).writeU32(a[0].add(0xc).readU32() + 1);      // one frame on: the movie ends after `frames` calls
        return 0x5678;
      } },
      // BinkPlayer::close 0x00487060 (c3dl)
      'binkw32.dll!_BinkClose@4': { args: 1, ret: 1 },
      'binkw32.dll!_BinkBufferClose@4': { args: 1,
        impl: function (a) { return (Math.imul(a[0].toUInt32(), 3) + 1) >>> 0; } },
      'USER32.dll!ShowCursor': { args: 1, ret: 0 },
      'WINMM.dll!timeGetTime': { args: 0, ret: 0x20000 },
    };
  }

  // Builds the players, their handles, buffers and paths, and the two argument-to-result maps.
  function scene(rows, opts) {
    opts = opts || {};
    const bufs = {}, binks = {};
    const bufFor = function (w) { const v = bufs[w.toUInt32()]; return v === undefined ? 0 : v; };
    const binkFor = function (p) { const v = binks[p.toUInt32()]; return v === undefined ? 0 : v; };
    const T = __uiTree({ arenaSize: opts.arenaSize || 0x8000, imports: binkImports(bufFor, binkFor),
                         stand: opts.stand || [] });
    const named = {};
    rows.forEach(function (r, i) {
      const p = T.alloc(0x18);
      const bk = T.alloc(0x40), bf = T.alloc(0x40), path = T.alloc(0x20);
      junk(p, 0x18, 0x7b000000 + i * 0x100);
      junk(bk, 0x40, 0x7c000000 + i * 0x100);
      junk(bf, 0x40, 0x7d000000 + i * 0x100);
      junk(path, 0x20, 0x7e000000 + i * 0x100);
      const width = 0x100 + i;
      bk.writeU32(width);                           // width: the second _BinkBufferOpen argument
      bk.add(4).writeU32(0x80 + i);                 // height
      bk.add(8).writeU32(0x20 + i);                 // frame count
      bk.add(0xc).writeU32(0x20 + i - r[7]);        // current frame: r[7] frames left
      bk.add(0x1c).writeU32(r[2] ? 0x5a0000 + i : 0);
      bk.add(0x24).writeU32(r[5] >>> 0);
      bk.add(0x28).writeU32(0);
      bk.add(0x2c).writeU32(r[6] >>> 0);
      bf.add(4).writeU32(0x500 + i); bf.add(0x10).writeU32(0x600 + i);
      bf.add(0x14).writeU32(0x300 + i); bf.add(0x18).writeU32(0x400 + i);
      path.writePointer(r[1] ? bk : ptr(0));
      path.add(8).writeUtf8String('mv' + i);
      p.add(4).writeU32(0); p.add(8).writeU32(0);
      p.add(0xc).writeU32(r[0] ? 0x7e000000 + i * 0x40 : 0);
      p.add(0x10).writeU32(0); p.add(0x14).writeU32(0);
      if (r[3]) bufs[width] = bf.add(r[4] ? 1 : 0).toUInt32();   // an odd buffer: _BinkBufferLock reports success
      binks[path.toUInt32()] = r[1] ? bk.toUInt32() : 0;
      named['p' + i] = p;
      named['path' + i] = path;
      named['bk' + i] = bk;
    });
    __freeze.save(ptr(0x0083ab9c), 4);
    ptr(0x0083ab9c).writeU32(0);
    return { T: T, named: named, binks: binks, bufs: bufs };
  }

  F.c3eq_play = function () {
    const s = scene(PLAY, {});
    return s.T.fx(s.named);
  };

  // ====================================================================================================================
  // playAudioFile 0x0043cce0
  //
  // The global BinkPlayer 0x005aa6d0 and the deadline 0x005aa774 are the live objects the function uses; both are state
  // regions, so diff_hook restores them after every arm, and the first thread is frozen for the key. fileExists
  // 0x004378a0, pumpInput483bd0 0x00483bd0, Window::paint 0x004808c0 (it would draw into the live root window 0x00519a60)
  // and resourceQuery 0x00487630 are stand-ins that record their arguments. BinkPlayer::setTarget 0x00487050,
  // BinkPlayer::play 0x00487090 (this batch) and BinkPlayer::close 0x00487060 run real: the window the recording
  // display's slot +0x20 answers (0xbeef) is what setTarget stores at 0x005aa6d4 + 8, so the player reaches _BinkOpen
  // with the path of the vector and _BinkBufferOpen with that handle's width.
  // ====================================================================================================================
  const AUDIO = [
    [1, 1, 0, 1, 1, 0, -1, 2, 0, 0],                 // 0 the movie plays two frames
    [1, 1, 0, 1, 1, 0, -1, 0, 0, 0],                 // 1 the movie is at its last frame
    [1, 0, 0, 1, 1, 0, -1, 2, 0, 0],                 // 2 _BinkOpen answers 0
    [1, 1, 1, 1, 1, 0, -1, 2, 0, 0],                 // 3 the handle's error word is set
    [1, 1, 0, 0, 1, 0, -1, 2, 0, 0],                 // 4 _BinkBufferOpen answers 0
    [1, 1, 0, 1, 0, 0x3, -1, 1, 0, 0],               // 5 two waits, even buffer
    [1, 1, 0, 1, 1, 0, 2, 9, 0, 0],                  // 6 the byte 0x0083ab9c is cleared at call 2
    [1, 1, 0, 1, 1, 0, -1, 4, 0, 0],                 // 7 four frames
  ];

  F.c3eq_audio = function () {
    const exists = {};
    const stand = [
      { addr: 0x004378a0, abi: 'mscdecl', nargs: 1,
        ret: function (self, a) { const v = exists[a[0].toUInt32()]; return v === undefined ? 0 : v; } },
      { addr: 0x00483bd0, abi: 'mscdecl', nargs: 0, ret: 0 },
      { addr: 0x004808c0, abi: 'thiscall', nargs: 1, ret: 0 },
      { addr: 0x00487630, abi: 'thiscall', nargs: 0, ret: 0x9911 },
    ];
    const s = scene(AUDIO, { stand: stand });
    Object.keys(s.binks).forEach(function (k) { exists[k] = 1; });     // every row's path exists
    const miss = s.T.alloc(0x20);
    junk(miss, 0x20, 0x7f000000);
    miss.writePointer(ptr(0));
    miss.add(8).writeUtf8String('gone');
    s.named.missing = miss;                                            // fileExists answers 0 for it
    const pl = ptr(0x005aa6d0);
    __freeze.save(pl, 0x18);
    for (let k = 4; k < 0x18; k += 4) pl.add(k).writeU32(0);           // the player as BinkPlayer::close leaves it
    __freeze.save(ptr(0x005aa774), 4);
    ptr(0x005aa774).writeU32(0x0a0a0a0a);
    return s.T.fx(s.named);
  };

  // ====================================================================================================================
  // TextView::render 0x0048fe60
  //
  // Real TextViews (TextView::ctor 0x0048ce00 through __uiTree) with their own copy of the class vtable 0x004bb3f8, in
  // which only the two slots the function calls (+0x18c at 0x0049071c, +0x180 at 0x00490920) are recording callbacks:
  // in the original both slots hold the empty body 0x0047e5e0, so without the copy neither call could be observed.
  // Every callee that is not under test is a __uiTree stand-in that logs `this` and all its arguments into a block in
  // the arena (compared as part of $heap) and answers a value computed from them, except: panelStop 0x004a11c0 answers
  // the word at this - 0x1460 + 0x2100 (an unused field of the TextView), so a row decides whether the early return at
  // 0x00490712 is taken, and Text_put 0x00477250 answers the dword its text argument points at, so a row decides the
  // width it reports. The row list at +0x1418 / +0x141c / +0x1420 / +0x1424 is a circular list of nodes {+8 record,
  // +0xc next} whose records are {+4 text, +8 kind}.
  // ====================================================================================================================
  const RENDER_STAND = [
    [0x00476310, 4], [0x00476340, 4], [0x00476370, 4], [0x004763a0, 4], [0x004762d0, 4],   // the quad / draw-target family
    [0x0048e120, 1], [0x0048e0b0, 1], [0x0048e190, 2],                                     // setFontSize / setLineSpacing / setSize
    [0x004898d0, 1], [0x00489ed0, 1], [0x00489f50, 2], [0x004a29a0, 1],                    // the switch's selectors
    [0x00477580, 0], [0x00476e20, 4], [0x004785e0, 2], [0x00478610, 1], [0x00478700, 0],   // the measuring family
  ];
  // [f590, flags 0x1f68, f137c, list rows, kinds, record, selector 0x1654, id 0x5c4, f164, f145c, num, den,
  //  owner is self, panelStop answer]
  const RENDER = [
    [0, 0x0000, 2, 3, [1, 2, 3], 1, -1, -1, 0, -1, 3, 2, 0, 0],          // 0 switch index 0, no early return
    [1, 0x0042, 1, 2, [0, 1], 1, 1, 7, 0, 4, 5, 3, 0, 0],                // 1 [+0x590] set, bit 1 set: the second panelStop runs
    [1, 0x0042, 3, 4, [2, 3, 0, 1], 1, 2, -1, 0, 0, 2, 7, 0, 0x5aa5],    // 2 the same, panelStop answers non-zero: early return
    [0, 0x0400, 4, 1, [2], 1, 2, 9, 0, 11, 1, 1, 0, 0],                  // 3 flag 0x400: the display is not asked
    [0, 0x0000, 0, 0, [], 0, 4, 3, 0, -1, 4, 5, 0, 0],                   // 4 no record, no rows, +0x1f70 ends 1
    [0, 0x0100, 1, 5, [3, 0, 2, 1, 0], 1, 8, -1, 0, 2, 6, 4, 0, 0],      // 5 switch index 9
    [1, 0x0100, 2, 2, [1, 1], 1, 0, 5, 0, -1, 1, 3, 0, 0],               // 6 switch default (index 1), id set
    [0, 0x10000, 2, 3, [0, 0, 0], 1, 3, -1, 0, 6, 2, 2, 0, 0],           // 7 switch default (index 4), id -1
    [0, 0x0040, 1, 2, [2, 0], 1, 20, 1, 0x4fa0, -1, 3, 4, 0, 0],         // 8 switch default above 9, [+0x164] set
    [0, 0x0000, 3, 4, [3, 2, 1, 0], 0, -5, -1, 0x4fb0, 3, 5, 2, 1, 0],   // 9 owner == this, [+0x164] set, no record
    [0, 0x0000, 2, 0, [], 1, 4, -1, 0, 7, 3, 2, 0, 0],                   // 10 a record but no rows; switch 5 with id -1
    [1, 0x0142, -1, 3, [1, 2, 0], 1, -1, 5, 0, -1, 2, 3, 0, 0],          // 11 [+0x1f70] ends 0, [+0x590] set, selector -1
  ];

  function renderScene(opts) {
    const lg = { b: null };
    const log = function (tag, self, a) {
      const b = lg.b;
      const n = b.readU32();
      b.writeU32(n + 1);
      let h = b.add(0x1f8).readU32() ^ 0x811c9dc5;
      h = Math.imul(h ^ (tag >>> 0), 0x01000193) >>> 0;
      h = Math.imul(h ^ (self.toUInt32 ? self.toUInt32() : (self >>> 0)), 0x01000193) >>> 0;
      for (let i = 0; i < a.length; i++) h = Math.imul(h ^ (a[i] >>> 0), 0x01000193) >>> 0;
      b.add(0x1f8).writeU32(Math.imul(h ^ n, 0x01000193) >>> 0);
      if (n >= 10) return;
      const e = b.add(4 + n * 32);
      e.writeU32(tag >>> 0);
      e.add(4).writeU32(self.toUInt32 ? self.toUInt32() : (self >>> 0));
      for (let i = 0; i < a.length && i < 6; i++) e.add(8 + 4 * i).writeU32(a[i] >>> 0);
    };
    const mix = function (self, a, tag) {
      let h = (Math.imul(self.toUInt32 ? self.toUInt32() : self, 0x9E3779B1) ^ tag) >>> 0;
      a.forEach(function (x, i) { h = (Math.imul(h ^ (x >>> 0), 0x01000193) + i * 0x35) >>> 0; });
      return h;
    };
    const stand = RENDER_STAND.map(function (r) {
      return { addr: r[0], abi: 'thiscall', nargs: r[1],
               ret: function (self, a) { log(r[0], self, a); return mix(self, a, r[0]); } };
    });
    stand.push({ addr: 0x004a11c0, abi: 'thiscall', nargs: 0, ret: function (self, a) {     // panelStop
      log(0x004a11c0, self, a);
      return self.sub(0x1460).add(0x2100).readU32();
    } });
    stand.push({ addr: 0x00477250, abi: 'thiscall', nargs: 1, ret: function (self, a) {     // Text_put
      log(0x00477250, self, a);
      return a[0] === 0 ? 0 : ptr(a[0]).readU32();
    } });
    stand.push({ addr: 0x004941e0, abi: 'thiscall', nargs: 0, ret: function (self, a) {     // listCurrentIndex
      log(0x004941e0, self, a);
      return opts.cur === undefined ? 0 : opts.cur();
    } });
    const T = __uiTree({ arenaSize: 0x60000, stand: stand, display: opts.display,
                         dispRet: { 0xa8: opts.dispWidth } });
    lg.b = T.alloc(0x200);
    for (let k = 0; k < 0x200; k += 4) lg.b.add(k).writeU32(0);
    globalThis.__c3eqLog = log;                       // the two recording vtable slots log here
    __freeze.atFlush(function () { globalThis.__c3eqLog = null; });
    const named = { lg: lg.b };
    // a copy of the TextView vtable 0x004bb3f8 whose slots +0x180 and +0x18c record
    const vt = T.alloc(0x400);
    Memory.copy(vt, ptr(0x004bb3f8), 0x400);
    [0x180, 0x18c].forEach(function (slot) {
      vt.add(slot).writePointer(globalThis.__c3eqSlot[slot]);
    });
    const text = function (w, name) {
      const p = T.alloc(0x20);
      junk(p, 0x20, 0x81000000 + w);
      p.writeU32(w >>> 0);
      p.add(4).writeUtf8String(name);
      return p;
    };
    if (opts.cur !== undefined) {
      const cmb = T.alloc(0x20);
      junk(cmb, 0x20, 0x82000000);
      named.combo = cmb;
      __freeze.setU32(ptr(0x0083fe08), cmb.toUInt32());
      opts.curText = text(opts.curWidth, 'combo');
      named.curText = opts.curText;
    } else {
      __freeze.setU32(ptr(0x0083fe08), 0);
    }
    RENDER.forEach(function (r, i) {
      const tv = T.make('TextView');
      tv.writePointer(vt);
      const vb = tv.add(0x1464).readPointer().add(8).readS32();
      const put = function (off, v) { tv.add(off).writeS32(v | 0); };
      put(0x590, r[0]);
      tv.add(0x1f68).writeU32(r[1] >>> 0);
      put(0x137c, r[2]);
      put(0x1654, r[6]);
      put(0x5c4, r[7]);
      put(0x164, r[8]);
      put(0x145c, r[9]);
      put(0x1fc4, r[10]);
      put(0x1fc8, r[11]);
      tv.add(0x5bc).writePointer(r[12] ? tv : T.alloc(0x300));
      tv.add(0x2100).writeU32(r[13] >>> 0);
      // the geometry inputs, distinct per object so a dropped or swapped operand shows in a stand-in's log
      [[0x1450, 7 + i], [0x1454, 11 + i], [0x1448, 3 + i], [0x144c, 5 + i], [0x1440, 13 + i], [0x1444, 17 + i],
       [0x1458, 19 + i], [0x59c, 400 + i * 3], [0x5a0, 300 + i * 5], [0x594, 23 + i], [0x598, 29 + i],
       [0x2020, 31 + i], [0x2024, 37 + i], [0x2028, 41 + i], [0x1f74, 43 + i], [0x1f78, 47 + i],
       [0x1f90, 53 + i]].forEach(function (p) { put(p[0], p[1]); });
      for (let k = 0; k < 4; k++) {
        put(0x1fd0 + 4 * k, 0x1000 + i * 0x10 + k);
        put(0x1fe0 + 4 * k, 0x2000 + i * 0x10 + k);
        put(0x1ff0 + 4 * k, 0x3000 + i * 0x10 + k);
        put(0x2000 + 4 * k, 0x4000 + i * 0x10 + k);
        put(0x2010 + 4 * k, 0x5000 + i * 0x10 + k);
      }
      put(vb + 0x14c4, i % 3 === 0 ? 0 : 60 + i * 7);
      put(vb + 0x1494, 90 + i * 11);
      put(vb + 0x14c8, i % 3 === 2 ? 0 : 70 + i * 5);
      put(vb + 0x1498, 110 + i * 3);
      put(vb + 0x1554, 0x5150 + i);
      tv.add(vb + 0x1488).writeU32(0);
      tv.add(0x5c8).writePointer(text(i === 9 ? 5 : 120 + i * 30, 'title' + i));   // row 9: Text_put below +0x1f7c
      // the record at +0x13d0 and the one at +0x202c
      const sub = T.alloc(0x40);
      junk(sub, 0x40, 0x83000000 + i * 0x100);
      sub.add(0x10).writeS32(60 + i * 4); sub.add(0x14).writeS32(80 + i * 6);
      tv.add(0x202c).writePointer(sub);
      if (r[5]) {
        const rc = T.alloc(0x40);
        junk(rc, 0x40, 0x84000000 + i * 0x100);
        rc.add(0x10).writeS32(100 + i * 9); rc.add(0x14).writeS32(140 + i * 13);
        tv.add(0x13d0).writePointer(rc);
      } else {
        tv.add(0x13d0).writeU32(0);
      }
      // the circular row list
      const n = r[3];
      put(0x1420, n);
      put(0x1424, 0);
      if (n === 0) { tv.add(0x1418).writeU32(0); tv.add(0x141c).writeU32(0); } else {
        const nodes = [];
        for (let k = 0; k < n; k++) {
          const node = T.alloc(0x10), recd = T.alloc(0x10);
          junk(node, 0x10, 0x85000000 + i * 0x100 + k);
          junk(recd, 0x10, 0x86000000 + i * 0x100 + k);
          recd.add(4).writePointer(text(150 + i * 20 + k * 9, 'row' + i + '.' + k));
          recd.add(8).writeS32(r[4][k]);
          node.add(8).writePointer(recd);
          nodes.push(node);
        }
        nodes.forEach(function (nd, k) { nd.add(0xc).writePointer(nodes[(k + 1) % n]); });
        tv.add(0x1418).writePointer(nodes[0]);
        tv.add(0x141c).writePointer(nodes[0]);
      }
      named['tv' + i] = tv;
    });
    __freeze.save(ptr(0x0083b8d8), 8);
    ptr(0x0083b8d8).writeU32(0x0b0b0b0b);
    ptr(0x0083b8dc).writeU32(0x0c0c0c0c);
    return T.fx(named);
  }

  // the two recording vtable slots, built once at load time (no NativeCallback while the first thread is frozen)
  (function () {
    const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
    globalThis.__c3eqSlot = {};
    [0x180, 0x18c].forEach(function (slot) {
      globalThis.__c3eqSlot[slot] = keep(new NativeCallback(function (self) {
        if (globalThis.__c3eqLog) globalThis.__c3eqLog(0x0c3e0000 | slot, self, []);
        return 0;
      }, 'uint32', ['pointer'], 'thiscall'));
    });
  })();

  F.c3eq_render = function () { return renderScene({ dispWidth: 0x100 }); };
  F.c3eq_render_big = function () { return renderScene({ dispWidth: 0x400 }); };
  // no display at 0x0083ad50: the NULL side of the test at 0x0048ff9f
  F.c3eq_render_nodisp = function () { return renderScene({ dispWidth: 0x400, display: false }); };
  F.c3eq_render_cs = function () {
    const o = { dispWidth: 0x100, curWidth: 150 };
    o.cur = function () { return o.curText.toUInt32(); };
    return renderScene(o);
  };
  F.c3eq_render_cl = function () {
    const o = { dispWidth: 0x100, curWidth: 300 };
    o.cur = function () { return o.curText.toUInt32(); };
    return renderScene(o);
  };
  F.c3eq_render_c0 = function () {
    const o = { dispWidth: 0x100, curWidth: 150 };
    o.cur = function () { return 0; };
    return renderScene(o);
  };
})();
