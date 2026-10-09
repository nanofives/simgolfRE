// Fixtures for batch c3cj (jgld.dll Display / DisplayBase vtable methods).
// Reimplementations: shim/src/re/c3cj.cpp. Registry: re/frida/registry.d/c3cj.py. Branch tables and the
// proposed Purposes: log/c3/c3cj_purpose.md. What was refused and why: log/c3/c3cj_notes.txt.
//
// jgld.dll is relocated at run time, so every module address below is built from
// Process.getModuleByName('jgld.dll').base; a hard-coded 0x10xxxxxx would read another module.
//
// Nothing live is handed to these bodies. The module's Display pointer 0x10128420 and the Lib pointer
// 0x1012872c are never read or written by this batch's fixtures: every Display, Mode, Array<ModeInfo>,
// mode table and destroyable object is allocated here.
//
// The Display objects carry a SYNTHETIC vtable of recording stubs, not jgld's own 0x1011d640. The four
// slots the mode methods dispatch (+0x58 pickMode, +0x60 setMode, +0x64 setModeIndex, +0x68 enumModes,
// read out of the binary by log/c3/alt/c3cj_vt.py) hold bodies that call EnumDisplaySettingsA,
// ChangeDisplaySettingsA and MessageBoxA and refill the module arrays g_modes 0x101286c8 / g_devmodes
// 0x101286f8, so the real table could not be used without changing the display mode of the machine the
// test runs on. What each slot receives, in which order, is the evidence these stubs record.
//
// The two scale fixtures swap MODULE GLOBALS the running game owns:
//   0x10122dc0 / 0x10122dc4 / 0x10122dc8   the three scale words Display::setScale writes and
//                                          Display::getScale reads; about forty live sprite blitters read
//                                          them to pick their body and their mirror quadrant
//                                          (re/names/jgld_1.tsv; c3bm, c3bp, c3bu, c3by).
// Those keys therefore SUSPEND the process's first thread (the one c3ce's and c3cd's probes found every
// jgld entry on) before touching the globals, and write the game's values back and resume it when the key
// ends (c3av's and c3cd's pattern: the resume runs from setTimeout(0), i.e. after diff_hook's run() has
// returned all of the key's vectors). Everything is allocated before the suspend; with the thread down the
// vectors only run jgld code over the fixture's own memory. If the suspend cannot be taken the fixture
// throws, so the A/B is recorded as ERROR instead of running against the live scale words.

(function () {
  // Shared frozen-thread helper (re/frida/js/freeze.js): suspends the process's first thread at a safe point;
  // `undo` runs at the flush, before the thread is resumed.
  function freezeFirstThread(undo) {
    __freeze.thread();
    __freeze.atFlush(undo);
  }

  const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

  // A __thiscall recording stub for one vtable slot. It appends {tag, this, arg1..argN} to the record area
  // the cursor dword points at, advances the cursor, and returns the dword at [this + retOff].
  //   A1 <&cursor>          mov eax, [cursor]
  //   C7 00 <tag>           mov dword [eax], tag
  //   89 48 04              mov [eax+4], ecx            ; `this`
  //   per argument i:  8B 54 24 (4i)  mov edx,[esp+4i]  /  89 50 (4+4i)  mov [eax+4+4i], edx
  //   83 C0 (8+4n)          add eax, 8+4*n
  //   A3 <&cursor>          mov [cursor], eax
  //   8B 81 <retOff>        mov eax, [ecx+retOff]       ; the object's canned return value
  //   C2 (4n) 00 | C3       ret 4n                      ; __thiscall: the callee pops its stack arguments,
  //                                                     ;   which the caller's `cmp esi,esp` + __chkesp
  //                                                     ;   0x1007e780 checks
  // retOff 'arg' returns the FIRST STACK ARGUMENT instead (8B 44 24 04, mov eax,[esp+4]): the variant the
  // _idx keys use, which puts the index pickMode / pickModeFreq chose into the A/B's return column.
  function recStub(tag, n, retOff) {
    const b = [0xA1, 0, 0, 0, 0];
    const patches = [1];
    b.push(0xC7, 0x00, tag & 0xff, (tag >>> 8) & 0xff, (tag >>> 16) & 0xff, (tag >>> 24) & 0xff);
    b.push(0x89, 0x48, 0x04);
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 + 4 * i);
    b.push(0x83, 0xC0, 8 + 4 * n);
    patches.push(b.length + 1);
    b.push(0xA3, 0, 0, 0, 0);
    if (retOff === 'arg') b.push(0x8B, 0x44, 0x24, 0x04);
    else b.push(0x8B, 0x81, retOff & 0xff, (retOff >>> 8) & 0xff, (retOff >>> 16) & 0xff, (retOff >>> 24) & 0xff);
    if (n === 0) b.push(0xC3); else b.push(0xC2, (4 * n) & 0xff, ((4 * n) >>> 8) & 0xff);
    return { bytes: b, patches: patches };
  }

  // The sixteen mode lists. Each row is one ModeInfo {w, h, w2, bpp, freq} as the three searchers read it:
  // +0x00 w, +0x04 h, +0x0c bpp, +0x10 freq (+0x08 is never read by any of them).
  const TABLES = [
    [],                                                                   // a0  empty list
    [[640, 480, 640, 8, 60]],                                             // a1  one exact match
    [[800, 600, 800, 16, 60], [800, 600, 800, 16, 75],
     [800, 600, 800, 16, 70]],                                            // a2  75 is exactly 0x4b
    [[640, 480, 640, 8, 100], [640, 480, 640, 8, 120]],                   // a3  both above 0x4b
    [[1024, 768, 1024, 32, 85], [1024, 768, 1024, 32, 85]],               // a4  equal frequencies
    [[640, 480, 640, 8, -5], [640, 480, 640, 8, 0]],                      // a5  negative frequency
    [[320, 200, 320, 8, 60], [640, 481, 640, 8, 70], [641, 480, 640, 8, 70],
     [640, 480, 640, 16, 70], [640, 480, 640, 8, 65]],                    // a6  one mismatch per field
    [[800, 600, 800, 16, 50], [800, 600, 800, 16, 60],
     [800, 600, 800, 16, 72], [800, 600, 800, 16, 40]],                   // a7  72 for the equality break
    [[640, 480, 640, 8, 76]],                                             // a8  above 0x4b, no candidate yet
    [[640, 480, 640, 8, 76], [640, 480, 640, 8, 80]],                     // a9  above 0x4b twice
    [[0, 0, 0, 0, 0]],                                                    // a10 all zero
    [[-1, -1, -1, -1, -1]],                                               // a11 all -1
    [[0x7fffffff, 0x7fffffff, 0, 0x7fffffff, 0x7fffffff]],                // a12 extremes
    [[640, 480, 640, 8, 0x7fffffff], [640, 480, 640, 8, -0x80000000]],    // a13 INT_MAX then INT_MIN
    [[640, 480, 640, 8, 10], [320, 240, 320, 8, 20], [640, 480, 640, 8, 30],
     [640, 480, 640, 32, 40], [640, 480, 640, 8, 25], [640, 480, 640, 8, 90],
     [640, 480, 640, 8, 95], [640, 480, 640, 8, 5]],                      // a14 eight entries
    [[1280, 1024, 1280, 32, 60], [1280, 1024, 1280, 32, 60],
     [1280, 1024, 1280, 32, 100], [1280, 1024, 1280, 32, 90]],            // a15 equal then above 0x4b
  ];

  // The sixteen Display objects: [m_width +0x12c, m_height +0x130, m_bpp +0x134, +0x138, +0x140, +0x144,
  // mode index (+0x148), canned slot return (+0x184)]. Every column is different across the sixteen, so a
  // getter that read the wrong field would change its result.
  const DISPLAYS = [
    [640, 480, 8, 0x11110000, 0x22220000, 0x33330000, 0, 0x5000],
    [800, 600, 16, 0x11110001, 0x22220001, 0x33330001, 1, 0x5001],
    [1024, 768, 32, 0x11110002, 0x22220002, 0x33330002, 2, 0x5002],
    [0, 0, 0, 0x00000000, 0x00000000, 0x00000000, 3, 0x00000000],
    [-1, -1, -1, 0xffffffff, 0xfffffffe, 0xfffffffd, 0, 0xffffffff],
    [0x7fffffff, 0x7ffffffe, 24, 0x12345678, 0x9abcdef0, 0x0fedcba9, 1, 0x7fffffff],
    [-0x80000000, 320, 4, 0xcccccccc, 0xcccccccd, 0xcccccccb, 2, 0x80000000],
    [320, 200, 8, 0xdeadbeef, 0xfeedface, 0x0badf00d, 3, 7],
    [1280, 1024, 32, 0x13570000, 0x24680000, 0x98760000, 0, 0x5008],
    [640, 400, 15, 0x13570001, 0x24680001, 0x98760001, 1, 0x5009],
    [1, 2, 3, 0x00000004, 0x00000005, 0x00000006, 2, 0x5010],
    [-2, -3, -4, 0xfffffff0, 0xfffffff1, 0xfffffff2, 3, 0x5011],
    [1600, 1200, 16, 0x7fffffff, 0x80000000, 0x40000000, 0, 0x5012],
    [200, 100, 1, 0x000000ff, 0x0000ff00, 0x00ff0000, 1, 0x5013],
    [8, 9, 10, 0x11111111, 0x22222222, 0x33333333, 2, 0x5014],
    [2048, 1536, 32, 0xabcdef01, 0xbcdef012, 0xcdef0123, 3, 0x5015],
  ];

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ----------------------------------------------------------- the Display objects and their lists
    //
    // One 0x8000 block, which is the whole compared state of every key that uses this fixture, so a store
    // one field too wide, a record written in the wrong order or a getter that touched the object would
    // change its hash. Layout:
    //   +0x0000  16 Display objects,  stride 0x200 ($d0..$d15; vtable +0, fields +0x12c..+0x148,
    //                                 the list pointer the +0x68 stub returns at +0x180, the canned slot
    //                                 return at +0x184)
    //   +0x2000   4 Mode objects,     stride 0x200 ($mo0..$mo3; vtable +0, canned return +0x184)
    //   +0x2800  16 Array<ModeInfo>,  stride 0x20   ($a0..$a15; m_data +4, m_count +0xc)
    //   +0x2a00  the sixteen mode tables, 0x14 bytes per entry
    //   +0x3400  the record cursor, then the record area to +0x3800
    //   +0x3800  24 out-parameter slots, stride 0x20 ($p0..$p23)
    //   +0x3c00  16 destroyable objects, stride 0x40 ($z0..$z15; vtable +0, canned return +0x20)
    c3cj_displays() { return buildDisplays(false); },

    // The same, with the slot +0x64 stub returning its ARGUMENT instead of the object's canned dword, so
    // that the index pickMode / pickModeFreq chose - the one fact of theirs that otherwise lives only
    // inside the hashed record area - appears in the A/B's return column. Used by the _idx keys.
    c3cj_displays_idx() { return buildDisplays(true); },

    // ------------------------------------------------- the three scale words, first value triple
    //
    // The globals are set to three DIFFERENT values so that a getter reading the wrong one, or writing the
    // out parameters in the wrong order, changes the result. The game's values are written back and its
    // first thread resumed when the key ends.
    c3cj_scale_a() { return scaleFixture(0x11111111, 0x22222222, 0x33333333, 0x5ca0); },

    // ------------------------------------------------ the three scale words, second value triple
    //
    // The same with a negative, a zero and INT_MAX, so no key's result can be a constant of the fixture's.
    c3cj_scale_b() { return scaleFixture(0xffffffff, 0x00000000, 0x7fffffff, 0x5cb0); },
  });

  function buildDisplays(idxArg) {
      const out = {};
      const blk = Memory.alloc(0x8000);
      fill(blk, 0x8000, 0x3c0a);
      out.blk = blk;
      out.obj = blk;

      const cursor = blk.add(0x3400);
      cursor.writePointer(blk.add(0x3404));

      const page = Process.pageSize;
      const code = Memory.alloc(page);
      Memory.protect(code, page, 'rwx');
      const nullStub = code;
      code.writeByteArray([0x33, 0xC0, 0xC3]);          // xor eax,eax; ret  (slots this batch never calls)

      // [offset in the code page, tag, stack argument count, return field]
      const STUBS = {
        enumModes: [0x40, 0xB0000000, 0, 0x180],        // slot +0x68, no argument, returns the list
        setModeIndex: [0x80, 0xB1000000, 1, idxArg ? 'arg' : 0x184],   // slot +0x64
        pickMode: [0xc0, 0xB2000000, 3, 0x184],         // slot +0x58
        setMode: [0x100, 0xB3000000, 4, 0x184],         // slot +0x60
        modeSet: [0x140, 0xB4000000, 4, 0x184],         // Mode slot +0x00
        deletingDtor: [0x180, 0xB5000000, 1, 0x20],     // slot 0 of a destroyable object
      };
      const at = {};
      for (const k of Object.keys(STUBS)) {
        const [off, tag, n, retOff] = STUBS[k];
        const p = code.add(off);
        const st = recStub(tag, n, retOff);
        p.writeByteArray(st.bytes);
        for (const q of st.patches) p.add(q).writePointer(cursor);
        at[k] = p;
      }

      // the synthetic Display vtable: the four mode slots recorded, everything else a harmless stub
      const vtD = Memory.alloc(0x100);
      for (let i = 0; i < 0x40; i++) vtD.add(i * 4).writePointer(nullStub);
      vtD.add(0x58).writePointer(at.pickMode);
      vtD.add(0x60).writePointer(at.setMode);
      vtD.add(0x64).writePointer(at.setModeIndex);
      vtD.add(0x68).writePointer(at.enumModes);
      // the Mode vtable (slot 0 only) and the destroyable-object vtable (slot 0 only)
      const vtM = Memory.alloc(0x40);
      for (let i = 0; i < 0x10; i++) vtM.add(i * 4).writePointer(nullStub);
      vtM.writePointer(at.modeSet);
      const vtZ = Memory.alloc(0x40);
      for (let i = 0; i < 0x10; i++) vtZ.add(i * 4).writePointer(nullStub);
      vtZ.writePointer(at.deletingDtor);

      // the mode tables and their Array<ModeInfo> headers
      let tbl = 0x2a00;
      for (let i = 0; i < 16; i++) {
        const arr = blk.add(0x2800 + i * 0x20);
        const rows = TABLES[i];
        const data = blk.add(tbl);
        for (let k = 0; k < rows.length; k++) {
          const e = data.add(k * 0x14);
          e.add(0x00).writeS32(rows[k][0]);             // w
          e.add(0x04).writeS32(rows[k][1]);             // h
          e.add(0x08).writeS32(rows[k][2]);             // never read by the three searchers
          e.add(0x0c).writeS32(rows[k][3]);             // bpp
          e.add(0x10).writeS32(rows[k][4]);             // frequency
        }
        tbl += rows.length * 0x14;
        arr.add(0x04).writePointer(data);               // m_data
        arr.add(0x0c).writeS32(rows.length);            // m_count, what Array::count 0x10066a80 returns
        out['a' + i] = arr;
      }

      for (let j = 0; j < 4; j++) {
        const mo = blk.add(0x2000 + j * 0x200);
        mo.writePointer(vtM);
        mo.add(0x184).writeU32((0x6000 + j) >>> 0);     // what the Mode slot returns (never read by setMode)
        out['mo' + j] = mo;
      }

      for (let i = 0; i < 16; i++) {
        const d = blk.add(i * 0x200);
        const c = DISPLAYS[i];
        d.writePointer(vtD);                            // +0x000 vtable
        d.add(0x12c).writeS32(c[0]);                    // m_width
        d.add(0x130).writeS32(c[1]);                    // m_height
        d.add(0x134).writeS32(c[2]);                    // m_bpp
        d.add(0x138).writeU32(c[3] >>> 0);              // the device context getField138 returns
        d.add(0x140).writeU32(c[4] >>> 0);              // the module handle getInstance returns
        d.add(0x144).writeU32(c[5] >>> 0);              // the window handle getHwnd returns
        d.add(0x148).writePointer(blk.add(0x2000 + c[6] * 0x200));   // the Mode object setMode calls
        d.add(0x180).writePointer(blk.add(0x2800 + i * 0x20));       // the list the +0x68 stub returns
        d.add(0x184).writeU32(c[7] >>> 0);              // the canned return of the three recording slots
        out['d' + i] = d;
      }

      for (let p = 0; p < 24; p++) out['p' + p] = blk.add(0x3800 + p * 0x20);
      for (let z = 0; z < 16; z++) {
        const o = blk.add(0x3c00 + z * 0x40);
        o.writePointer(vtZ);
        o.add(0x20).writeU32((0x7000 + z) >>> 0);
        out['z' + z] = o;
      }
      return out;
  }

  function scaleFixture(a, b, c, seed) {
    const out = {};
    const blk = Memory.alloc(0x1000);
    fill(blk, 0x1000, seed);
    out.blk = blk;
    out.obj = blk;
    // 48 out-parameter slots, far enough apart that a store one dword too wide shows
    for (let i = 0; i < 48; i++) out['q' + i] = blk.add(0x100 + i * 0x20);

    const g = Process.getModuleByName('jgld.dll').base.add(0x122dc0);
    out.g = g;
    const saved = [g.readU32(), g.add(4).readU32(), g.add(8).readU32()];
    freezeFirstThread(function () {
      g.writeU32(saved[0]); g.add(4).writeU32(saved[1]); g.add(8).writeU32(saved[2]);
    });
    g.writeU32(a >>> 0); g.add(4).writeU32(b >>> 0); g.add(8).writeU32(c >>> 0);
    return out;
  }
})();
