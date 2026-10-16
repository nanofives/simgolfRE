// C3 batch c3fz (round 37, golf_clean.exe): fixtures for shim/src/re/c3fz.cpp (registry re/frida/registry.d/c3fz.py).
// Synthetic values and records only.
//
// EditBox::onKey: the EditBox objects are c3ct's (re/frida/js/fixtures.d/c3ct.js, buildEditBoxes: a copy, not a
// dependency): 0x700 bytes, pre-filled 0x5a5a5a5a, the EditBox vtable recording slots +0x10 and +0x120 as machine code
// stubs (no JavaScript inside a vector), the text widget at +0x274, an optional child at +0x278 whose slot +0xd8 returns
// its width, fonts with a measurer, margins +0x598 / +0x59c, text +0x574, caret +0x5a4. Differences from c3ct: every object
// owns its text buffer (0x80 bytes, the key handler edits it in place) and its caret, and MessageBeep (USER32, IAT slot
// 0x004ba1bc) runs under the import stand-in, so each beep is a compared record in $implog. The first thread is frozen:
// wrapToLines -> wrapText writes 0x00839aa8.
//
// __ArrayUnwind: $D (0x210 bytes): +0 destructor calls, +8 the call index that raises (-1: none), +0x10 the `this` of each
// destructor call (64 slots). $dtor (thiscall) logs `this`, inverts the element's first dword, and raises exception
// 0xE0C3F00F through KERNEL32!RaiseException when its call index equals +8. Every arm runs through C3fzSehCall4 (the arm
// is inside __try / __except in the shim, so the raise unwinds the arm's own frames and is caught there; the code goes to
// $blk +0x30).
(function () {
  const G_DRAG = 0x0083ab30, G_WRAP = 0x00839aa8;

  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v >>> 0); }
  function alloc(n, v) { const p = Memory.alloc(n); fill(p, n, v === undefined ? 0 : v); return p; }

  function armNeg() {
    __freeze.negOn();
    const tid = Process.getCurrentThreadId();
    __freeze.negCtlWords().forEach(function (a) { __freeze.setU32(a.add(8), tid); });
  }

  function codeArena() {
    const size = Process.pageSize * 4;
    const base = Memory.alloc(size);
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3fz: code arena full');
      return p;
    };
  }

  // A recording stub (c3ct's): mov eax,[REC]; add eax,REC; mov [eax],tag; mov [eax+4],ecx; each stack argument;
  // mov edx,[WATCH]; mov [eax+8+4n],edx; add [REC],4(n+3); mov eax,[REC]; xor eax,tag (or mov eax,[ecx+retOff]); ret 4n.
  function stubBytes(rec, watch, tag, n, thiscall, retOff) {
    const r = u32(rec.toUInt32());
    const b = [];
    b.push(0xA1, ...r);
    b.push(0x05, ...r);
    b.push(0xC7, 0x00, ...u32(tag));
    if (thiscall) b.push(0x89, 0x48, 0x04);
    else b.push(0xC7, 0x40, 0x04, ...u32(0xC0C0C0C0));
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 + 4 * i);
    b.push(0x8B, 0x15, ...u32(watch));
    b.push(0x89, 0x50, 8 + 4 * n);
    b.push(0x81, 0x05, ...r, ...u32(4 * (n + 3)));
    if (retOff === undefined) {
      b.push(0xA1, ...r);
      b.push(0x35, ...u32(tag));
    } else {
      b.push(0x8B, 0x81, ...u32(retOff));
    }
    if (thiscall && n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }
  function newRec(size) { const rec = alloc(size); rec.writeU32(0x10); return rec; }
  function vtable(emit, rec, watch, spec) {
    const nul = emit([0x33, 0xC0, 0xC3]);
    const vt = alloc(0x200);
    for (let i = 0; i < 0x80; i++) vt.add(i * 4).writePointer(nul);
    for (const [slot, tag, n, retOff] of spec) vt.add(slot).writePointer(emit(stubBytes(rec, watch, tag, n, true, retOff)));
    return vt;
  }
  // The font measurer slot +0x10 (thiscall (text, count)): the width of `count` characters is the sum over them of
  // W + (char & 3), W = the measurer's +4.
  const MEASURE = [0x56, 0x57, 0x8B, 0x74, 0x24, 0x0C, 0x8B, 0x7C, 0x24, 0x10, 0x33, 0xC0, 0x85, 0xFF, 0x7E, 0x0F,
                   0x0F, 0xB6, 0x16, 0x83, 0xE2, 0x03, 0x03, 0x51, 0x04, 0x03, 0xC2, 0x46, 0x4F, 0x75, 0xF1,
                   0x5F, 0x5E, 0xC2, 0x08, 0x00];

  // ---- EditBox layouts (c3ct's EB rows): child width, margins, markup, font widths per style, top line, indent, text
  const EB = [[120, [2, 3], 0, [5, 5, 5, 5], 0, 0, 'alpha beta gamma delta epsilon zeta eta theta'],
              [null, [1, 1], 0, [7, 7, 7, 7], 0, 0, 'one two three four'],
              [90, [0, 4], 1, [5, 7, 3, 5], 1, 4, 'ab {cd ef} gh [ij kl] mn op qr st'],
              [200, [3, 0], 0, [3, 3, 3, 3], -3, 0, 'x'],
              [64, [0, 0], 0, [3, 3, 3, 3], 2, 0, ''],
              [300, [5, 5], 1, [7, 3, 5, 7], 0, 6, 'pre {bold run} and [other run] tail words wrap {x} end']];
  // The second variant changes widths and margins so the same texts wrap differently.
  const EB_WIDTH_B = [70, null, 150, 40, 30, 120];
  const EB_PAD_B = [[0, 0], [4, 4], [1, 1], [0, 2], [3, 3], [2, 2]];

  const VK = { BACK: 0x08, END: 0x23, HOME: 0x24, LEFT: 0x25, UP: 0x26, RIGHT: 0x27, DOWN: 0x28, DEL: 0x2e };
  // Objects: [layout row, caret ('e' = the text length), keys in order]. One object's keys run in this order, so the
  // caret and the text a key sees are what the earlier keys left.
  const OBJS = [
    [0, 0, [VK.LEFT, VK.BACK, VK.UP, VK.RIGHT, VK.DOWN]],
    [0, 'e', [VK.RIGHT, VK.DOWN, VK.END, VK.DEL, VK.LEFT]],
    [0, 17, [VK.UP, VK.UP, VK.DOWN, VK.DOWN]],
    [0, 30, [VK.DOWN, VK.UP, VK.HOME, VK.END]],
    [0, 5, [VK.BACK, VK.BACK, VK.DEL, VK.DEL, VK.RIGHT]],
    [0, 44, [VK.DEL, VK.DEL, VK.BACK, VK.END]],
    [1, 0, [VK.UP, VK.DOWN, VK.DOWN, VK.UP]],
    [1, 'e', [VK.RIGHT, VK.DOWN, VK.UP, VK.UP]],
    [1, 9, [VK.DOWN, VK.DOWN, VK.UP, VK.LEFT, VK.RIGHT]],
    [2, 5, [VK.DOWN, VK.UP, VK.RIGHT, VK.LEFT]],
    [2, 12, [VK.DOWN, VK.DOWN, VK.UP]],
    [2, 'e', [VK.UP, VK.UP, VK.DOWN, VK.BACK]],
    [3, 0, [VK.DOWN, VK.UP, VK.RIGHT, VK.RIGHT, VK.DEL]],
    [3, 1, [VK.UP, VK.DOWN, VK.BACK, VK.BACK]],
    [4, 0, [VK.LEFT, VK.RIGHT, VK.HOME, VK.END, VK.DEL, VK.BACK, VK.UP, VK.DOWN]],
    [5, 0, [VK.DOWN, VK.DOWN, VK.DOWN, VK.UP, VK.UP]],
    [5, 14, [VK.DOWN, VK.UP, VK.LEFT, VK.RIGHT]],
    [5, 'e', [VK.UP, VK.DOWN, VK.DEL, VK.BACK]],
    // keys that are not handled (index byte 8 of the table 0x00486abc, k above 0x26, k below 0) and high bits of the key
    // word, which `and eax, 0xffff` (0x004867ba) drops
    [0, 3, [0x41, 0x07, 0x29, 0x2f, 0x1b, 0x0d, 0x09, 0xffff, 0x00, 0x2a, 0x21, 0x22, 0x30000,
            0x10025, 0x00ab0008, 0xffff0027, 0x7fff0023, 0x80000024]],
  ];

  function buildEditBoxes(fx, emit, rec, variant) {
    const vtE = vtable(emit, rec, G_DRAG, [[0x10, 0xE0100000, 0], [0x120, 0xE1200000, 0]]);
    const vtC = vtable(emit, rec, G_DRAG, [[0xd8, 0xC0D80000, 0, 0x40], [0xdc, 0xC0DC0000, 0]]);
    const mvt = alloc(0x40);
    mvt.add(0x10).writePointer(emit(MEASURE));
    const fonts = {};
    function font(w, i) {
      const key = w + ':' + (i % 2);
      if (fonts[key]) return fonts[key];
      const m = alloc(0x10); m.writePointer(mvt); m.add(4).writeS32(w);
      const f = alloc(0x20, 0x0f0f0f0f);
      f.add(4).writePointer(m);
      if (i % 2 === 0) { f.add(8).writeS32(2); f.add(0x10).writeS32(10); f.add(0xc).writeS32(77); }
      else { f.add(8).writeS32(-1); f.add(0xc).writeS32(9); f.add(0x10).writeS32(55); }
      fonts[key] = f;
      return f;
    }
    OBJS.forEach(function (o, i) {
      const [row, caretSpec, keys] = o;
      const [cw0, pad0, markup, styleW, top, indent, text] = EB[row];
      const cw = variant === 'b' ? EB_WIDTH_B[row] : cw0;
      const pad = variant === 'b' ? EB_PAD_B[row] : pad0;
      const caret = caretSpec === 'e' ? text.length : caretSpec;
      const t = alloc(0x80);
      t.writeByteArray(Array.from(text, (c) => c.charCodeAt(0)).concat([0]));
      fx['txt' + i] = t;
      const eb = alloc(0x700, 0x5a5a5a5a);
      fill(eb.add(0x274), 0x8c, 0);
      eb.writePointer(vtE);
      eb.add(0x288).writeS32(indent);
      eb.add(0x2b4).writeS32(markup);
      for (let s = 0; s < 4; s++) eb.add(0x2d0 + 4 * s).writePointer(font(styleW[s], row));
      if (cw === null) {
        eb.add(0x278).writePointer(ptr(0));
      } else {
        const ch = alloc(0x80, 0x2c2c2c2c);
        ch.writePointer(vtC);
        ch.add(0x40).writeS32(cw);
        eb.add(0x278).writePointer(ch);
        fx['ebch' + i] = ch;
      }
      eb.add(0x574).writePointer(t);
      eb.add(0x57c).writeS32(top);
      eb.add(0x598).writeS32(pad[0]);
      eb.add(0x59c).writeS32(pad[1]);
      eb.add(0x5a4).writeS32(caret);
      eb.add(0x5a8).writeS32(0x33);
      const tm = eb.add(0x5ac);
      fill(tm, 0x30, 0);
      tm.add(8).writeU32(1);
      fx['eb' + i] = eb;
    });
  }

  function editFixture(variant) {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x1000);
    fx.rec = rec; fx.obj = rec;
    buildEditBoxes(fx, emit, rec, variant);
    __freeze.thread();
    __freeze.save(ptr(G_WRAP), 4);
    const s = __importStandIn({ module: 'golf_clean.exe', imports: { 'USER32.dll!MessageBeep': { ret: 1 } } });
    fx.implog = s.log;
    armNeg();
    return fx;
  }

  // ---- __ArrayUnwind --------------------------------------------------------------------------------------------------
  function sehArms(blk) {
    const target = Process.getModuleByName('winmm.dll').getExportByName('C3fzSehCall4');
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
    c3fz_edit() { return editFixture('a'); },
    c3fz_edit_b() { return editFixture('b'); },
    c3fz_unwind_none: unwindFixture(-1),
    c3fz_unwind_t0: unwindFixture(0),
    c3fz_unwind_t2: unwindFixture(2),
  });
})();
