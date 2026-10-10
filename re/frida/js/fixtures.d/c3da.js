// Fixtures for batch c3da (round 24, golf_clean.exe Surface wrapper layer below 0x004781f0).
// Reimplementations: shim/src/re/c3da.cpp. Registry: re/frida/registry.d/c3da.py. Purposes: log/c3/c3da_purpose.md.
//
// Nothing here is handed to the game's own drawing objects. Every Surface record the functions receive is a fixture
// record whose +4 is a fixture HANDLE: a 0x20-byte object whose vtable slots are RECORDING STUBS (machine code in a
// private rwx page). A stub appends one entry to the call recorder $rec and returns:
//     entry = tag (slot offset | 0x4ac30000 for a handle slot, 0x0d150000 for a display slot), ecx (the object the
//             slot was called on), then each stack argument by value, or for a pointer argument ('r') the four dwords
//             it points to (the rects Surface_blit3 / Widget_fillArea pass live on the caller's stack, whose address
//             differs between the arms, so the CONTENTS are recorded).
//     $rec+0 = byte cursor (entries from $rec+0x10). eax = cursor ^ tag (a value that differs per call), except:
//       handle +0xe4 returns &handle[+8] (the format word Surface_blit compares with 8),
//       handle +0x04 returns handle[+0xc] (Surface_create / Surface_init2 test it),
//       display +0x80 returns display[+8] (the handle a create/copy installs).
// The display object (+0 its vtable, +0x10/+0x14/+0x18 the scale triple nx, ny, den):
//       +0x9c (nx, ny, den) records and stores the triple; +0xa0 (&nx, &ny, &den) records (no arguments: they are
//       stack addresses) and writes the stored triple through the three pointers; +0x94 (handle) records.
// Keys that reach the display pointer 0x0083ad50 or the game's static Surface tables freeze the game's first thread
// (re/frida/js/freeze.js) and swap them; every swapped global is saved and restored at the flush.
(function () {
  const REC_SIZE = 0x2000;
  const STRIDE = 0xc0;          // fixture Surface records (blitTakeDirty reads src+0xac)
  const NSURF = 16;

  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }

  function codeArena() {
    const size = Process.pageSize * 4;
    const base = Memory.alloc(size);
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3da: code arena full');
      return p;
    };
  }

  // A recording thiscall stub. args: one letter per stack argument: 'v' value, 'r' four dwords behind a pointer,
  // 's' skipped. ret: 'cur' | 'lea8' | 'f8' | 'fc'. act: null | 'store3' | 'load3'.
  function stubBytes(rec, tag, args, ret, act) {
    const r = u32(rec.toUInt32());
    const b = [0x53];                                                  // push ebx
    b.push(0x8B, 0x1D, ...r);                                          // mov ebx, [REC]
    b.push(0x81, 0xC3, ...r);                                          // add ebx, REC
    b.push(0xC7, 0x03, ...u32(tag));                                   // mov dword [ebx], tag
    b.push(0x89, 0x4B, 0x04);                                          // mov [ebx+4], ecx
    let off = 8;
    args.forEach(function (k, i) {
      const sp = 8 + 4 * i;                                            // [esp+sp] = argument i (after push ebx)
      if (k === 'v') {
        b.push(0x8B, 0x44, 0x24, sp, 0x89, 0x43, off); off += 4;       // mov eax,[esp+sp]; mov [ebx+off],eax
      } else if (k === 'r') {
        b.push(0x8B, 0x54, 0x24, sp);                                  // mov edx,[esp+sp]
        for (let j = 0; j < 4; j++) { b.push(0x8B, 0x42, 4 * j, 0x89, 0x43, off); off += 4; }
      }
    });
    if (off > 0x7f) throw new Error('c3da: entry too long');
    b.push(0x81, 0x05, ...r, ...u32(off));                             // add dword [REC], off
    if (act === 'store3') {
      for (let j = 0; j < 3; j++) b.push(0x8B, 0x44, 0x24, 8 + 4 * j, 0x89, 0x41, 0x10 + 4 * j);  // [ecx+0x10+4j] = arg j
    } else if (act === 'load3') {
      for (let j = 0; j < 3; j++) b.push(0x8B, 0x54, 0x24, 8 + 4 * j, 0x8B, 0x41, 0x10 + 4 * j, 0x89, 0x02);  // *arg j = [ecx+..]
    }
    if (ret === 'lea8') b.push(0x8D, 0x41, 0x08);                      // lea eax, [ecx+8]
    else if (ret === 'f8') b.push(0x8B, 0x41, 0x08);                   // mov eax, [ecx+8]
    else if (ret === 'fc') b.push(0x8B, 0x41, 0x0C);                   // mov eax, [ecx+0xc]
    else b.push(0xA1, ...r, 0x35, ...u32(tag));                        // mov eax, [REC]; xor eax, tag
    b.push(0x5B);                                                      // pop ebx
    const n = args.length;
    if (n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }

  // Handle slots (offset: argument kinds), from the call sites in c3da.cpp.
  const HANDLE_SLOTS = {
    0x04: ['vvvvvv', 'fc'],   // Surface_create 0x004745a1 / Surface_init2 0x0047461c
    0x0c: ['v', 'cur'],       // Surface_copyFrom 0x00474541
    0x18: ['v', 'cur'],       // 0x00474535 / 0x00474582 / 0x004745f2
    0x40: ['vrr', 'cur'],     // Surface_blit3 0x00475d7c
    0x44: ['rv', 'cur'],      // Widget_fillArea 0x00478b6b
    0x4c: ['vvvvvv', 'cur'],  // Sprite_drawScaled 0x00474338
    0x50: ['vvvvvv', 'cur'],  // Surface_blitMasked 0x00474012
    0x54: ['vvvvv', 'cur'],   // Surface_stretchBlit 0x00473fa5
    0x64: ['vvvvvv', 'cur'],  // Widget_fillRect 0x00478ba4
    0x6c: ['vvvvv', 'cur'],   // Surface_blit2 0x00474194
    0x78: ['vvvvv', 'cur'],   // Sprite_draw 0x00474240
    0x94: ['vvvvvv', 'cur'],  // Surface_drawSprite 0x004744a5
    0xe4: ['', 'lea8'],       // Surface_blit 0x00473c19
  };

  function build(opt) {
    opt = opt || {};
    const fx = {};
    const emit = codeArena();
    const rec = Memory.alloc(REC_SIZE);
    for (let i = 0; i < REC_SIZE; i += 4) rec.add(i).writeU32(0);
    rec.writeU32(0x10);
    fx.rec = rec;

    const nul = emit([0x33, 0xC0, 0xC3]);                              // xor eax, eax; ret
    const hvt = Memory.alloc(0x100 * 4);
    for (let i = 0; i < 0x100; i++) hvt.add(i * 4).writePointer(nul);
    Object.keys(HANDLE_SLOTS).forEach(function (k) {
      const off = Number(k), spec = HANDLE_SLOTS[k];
      hvt.add(off).writePointer(emit(stubBytes(rec, 0x4ac30000 + off, spec[0].split(''), spec[1], null)));
    });
    const dvt = Memory.alloc(0x100 * 4);
    for (let i = 0; i < 0x100; i++) dvt.add(i * 4).writePointer(nul);
    dvt.add(0x80).writePointer(emit(stubBytes(rec, 0x0d150080, ['v', 'v'], 'f8', null)));
    dvt.add(0x94).writePointer(emit(stubBytes(rec, 0x0d150094, ['v'], 'cur', null)));
    dvt.add(0x9c).writePointer(emit(stubBytes(rec, 0x0d15009c, ['v', 'v', 'v'], 'cur', 'store3')));
    dvt.add(0xa0).writePointer(emit(stubBytes(rec, 0x0d1500a0, ['s', 's', 's'], 'cur', 'load3')));

    // Handles: one block, 0x20 each. +4 id, +8 format word (16: Surface_blit's `cmp [eax], 8` fails), +0xc slot-4 result.
    const NH = 160;
    const hblk = Memory.alloc(NH * 0x20);
    function handle(i, fmt, r4) {
      const h = hblk.add(i * 0x20);
      h.writePointer(hvt);
      h.add(4).writeU32(0x4a000000 + i);
      h.add(8).writeU32(fmt === undefined ? 16 : fmt);
      h.add(0xc).writeU32(r4 === undefined ? 0 : r4);
      return h;
    }
    fx.hblk = hblk;

    // Fixture Surface records $s0..$s15 in one block ($surfs, the state region of every key that writes them).
    const surfs = Memory.alloc(NSURF * STRIDE);
    for (let i = 0; i < NSURF * STRIDE; i += 4) surfs.add(i).writeU32(0x5a5a0000 + i);
    fx.surfs = surfs;
    // [handle index or -1, m_8, m_c, hotspot x (+0x20), hotspot y (+0x24), +0x28, +0xac]
    const S = [
      [0, 0, 0, 5, -3, 0x28, 0x700],                  // s0  drawable
      [1, 0, 0, 0x7ffffff0, 17, 0x128, 0x701],        // s1  hotspot that overflows when scaled
      [2, 0, 0, -40, 100, 0x228, 0x702],              // s2
      [-1, 0, 0, 1, 2, 0x328, 0x703],                 // s3  null handle
      [4, 0x11, 0, 6, 7, 0x428, 0x704],               // s4  busy (m_8 != 0)
      [5, 0, 0, 3, 4, 0x528, 0x705],                  // s5  destination
      [6, 0, 0, -9, 13, 0x628, 0x706],                // s6  destination
      [7, 0, 0, 0, 0, 0x728, 0x707],                  // s7  mask
      [-1, 0, 0, 0, 0, 0x828, 0x708],                 // s8  mask with a null handle
      [9, 0, 0, 0, 0, 0x928, 0x709],                  // s9  extra
      [10, 0, 0, 11, 12, 0xa28, 0],                   // s10 lifetime target with a handle
      [-1, 0, 0, 13, 14, 0xb28, 0],                   // s11 lifetime target without a handle
      [-1, 0x55, 0, 15, 16, 0xc28, 0x70c],            // s12 copy source without a handle
      [13, 0x66, 0x77, 17, 18, 0xd28, 0x70d],         // s13 copy source with a handle
      [14, 0, 0, -1, -1, 0xe28, 0x70e],               // s14 blitTakeDirty source, format 16
      [15, 0, 0, 2, 2, 0xf28, -1],                    // s15 blitTakeDirty source, format 15
    ];
    S.forEach(function (d, i) {
      const s = surfs.add(i * STRIDE);
      s.writeU32(0x53000000 + i);
      s.add(4).writePointer(d[0] < 0 ? ptr(0) : handle(d[0], d[0] === 15 ? 15 : 16, 0));
      s.add(8).writeU32(d[1]);
      s.add(0xc).writeU32(d[2]);
      s.add(0x20).writeS32(d[3]);
      s.add(0x24).writeS32(d[4]);
      s.add(0x28).writeU32(d[5]);
      s.add(0xac).writeS32(d[6]);
      fx['s' + i] = s;
    });
    fx.obj = surfs;

    // The display: +8 the handle slot +0x80 hands out (handle 20; its slot-4 result is opt.r4).
    const disp = Memory.alloc(0x40);
    for (let i = 0; i < 0x40; i += 4) disp.add(i).writeU32(0);
    disp.writePointer(dvt);
    disp.add(4).writeU32(0xd15d15d1);
    disp.add(8).writePointer(handle(20, 16, opt.r4 === undefined ? 0 : opt.r4));
    const sc = opt.scale || [1, 1, 1];
    disp.add(0x10).writeS32(sc[0]);
    disp.add(0x14).writeS32(sc[1]);
    disp.add(0x18).writeS32(sc[2]);
    fx.disp = disp;

    // Static tables (filled only when opt.statics): fixture records written over the game's own, one handle each.
    let statics = [];
    if (opt.statics) {
      const A = [0x005a5554];
      for (let i = 0; i < 9; i++) A.push(0x00541580 + 0x2c * i);                        // blitPanel 0x0040caa9..
      [0x00561810, 0x0056183c, 0x00561944, 0x00561894, 0x005618ec, 0x00561868, 0x00561970, 0x00561918,
       0x0058d1e0, 0x0058d20c, 0x0058d314, 0x0058d264, 0x0058d2bc, 0x0058d238, 0x0058d340, 0x0058d2e8,
       0x005a53c8, 0x005a53f4, 0x005a54fc, 0x005a544c, 0x005a54a4, 0x005a5420, 0x005a5528, 0x005a54d0,
       0x0058b680, 0x0058b6ac, 0x0058b7b4, 0x0058b704, 0x0058b75c, 0x0058b6d8, 0x0058b7e0, 0x0058b788]
        .forEach(function (a) { A.push(a); });
      // drawFrameS2's tables for all four styles (0x00404e00 builds 0x20 records each = 4 styles x 8): the selected
      // style first, so its records get the same handles whatever the style, then the other three.
      const st = opt.style || 0;
      [0x00562394, 0x00562418, 0x00562368, 0x005623c0, 0x00562470, 0x00562444, 0x0056249c, 0x005623ec,
       0x0059d94c, 0x0059d9d0, 0x0059d920, 0x0059d978, 0x0059da28, 0x0059d9fc, 0x0059da54, 0x0059d9a4]
        .forEach(function (a) { for (let k = 0; k < 4; k++) A.push(a + 0x160 * ((st + k) & 3)); });
      const recs = Memory.alloc(A.length * 0x2c);
      A.forEach(function (a, k) {
        const r = recs.add(k * 0x2c);
        for (let j = 0; j < 0x2c; j += 4) r.add(j).writeU32(0x57000000 + k * 0x100 + j);
        r.add(4).writePointer(handle(30 + k, 16, 0));
        r.add(8).writeU32(opt.busyPanel === k ? 1 : 0);
        r.add(0xc).writeU32(0);
        r.add(0x20).writeS32((k * 3) % 7 - 2);
        r.add(0x24).writeS32((k * 5) % 9 - 3);
      });
      statics = A.map(function (a, k) { return [ptr(a), recs.add(k * 0x2c)]; });
    }
    // The target Surface *0x004c1570 and the window surface handle 0x00519a60+0x274+4 (fillWindowRect ->
    // Win_fillRect(this+0x274) -> Widget_fillArea reads [this+4], 0x00480b19 / 0x00478b50).
    const tgt = surfs.add(5 * STRIDE);
    const winHandle = handle(21, 16, 0);

    if (opt.freeze) {
      __freeze.thread();
      if (opt.display) {
        __freeze.save(ptr('0x0083ad50'), 4);
        ptr('0x0083ad50').writePointer(opt.display === 'null' ? ptr(0) : disp);
      }
      if (opt.statics || opt.target) {
        __freeze.save(ptr('0x004c1570'), 4);
        ptr('0x004c1570').writePointer(opt.target === 'nohandle' ? surfs.add(3 * STRIDE) : tgt);
      }
      if (opt.statics) {
        statics.forEach(function (p) { __freeze.save(p[0], 0x2c); p[0].writeByteArray(p[1].readByteArray(0x2c)); });
        __freeze.save(ptr('0x00519cd8'), 4);
        ptr('0x00519cd8').writePointer(winHandle);
        __freeze.save(ptr('0x005a34e0'), 1);
        ptr('0x005a34e0').writeS8(opt.style || 0);
      }
    }
    return fx;
  }

  const F = {
    c3da_plain() { return build({}); },
  };
  // Display keys: the scale triple the display starts with.
  const SCALES = { s1: [1, 1, 1], s2: [3, 5, 2], s3: [-7, 11, 3], s4: [2, 3, -4] };
  Object.keys(SCALES).forEach(function (k) {
    F['c3da_disp_' + k] = function () { return build({ freeze: true, display: 'fx', scale: SCALES[k] }); };
  });
  // Lifetime keys: the result of the created handle's slot +4.
  F.c3da_life_r0 = function () { return build({ freeze: true, display: 'fx', r4: 0 }); };
  F.c3da_life_r9 = function () { return build({ freeze: true, display: 'fx', r4: 9 }); };
  // Static-table keys (blitPanel, the frame drawers, drawRectScaled).
  F.c3da_static = function () { return build({ freeze: true, display: 'fx', statics: true, scale: [3, 5, 2] }); };
  F.c3da_static_busy = function () {
    return build({ freeze: true, display: 'fx', statics: true, scale: [1, 1, 1], busyPanel: 5 });
  };
  F.c3da_static_nodisp = function () { return build({ freeze: true, display: 'null', statics: true }); };
  F.c3da_static_st2 = function () { return build({ freeze: true, display: 'fx', statics: true, style: 2 }); };
  F.c3da_static_st3 = function () { return build({ freeze: true, display: 'fx', statics: true, style: 3 }); };
  F.c3da_target = function () { return build({ freeze: true, target: 'fx' }); };
  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
