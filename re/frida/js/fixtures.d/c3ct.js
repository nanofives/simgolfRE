// Fixtures for batch c3ct (round 23, golf_clean.exe UI: the EditBox caret family, scrollbarOnDrag and three ListBox
// handlers). Reimplementations: shim/src/re/c3ct.cpp. Registry: re/frida/registry.d/c3ct.py. Purposes and per-jcc
// branch coverage: log/c3/c3ct_purpose.md.
//
// Every object is our own allocation; none is linked into the game's window list, draw list or focus chain. Virtual
// calls land in RECORDING STUBS written as machine code (no JavaScript runs inside a vector):
//     mov eax, [REC]; add eax, REC            ; REC+0 is the record cursor (byte offset)
//     mov dword [eax], tag; mov [eax+4], ecx  ; (cdecl stubs store the constant 0xC0C0C0C0 instead of ecx)
//     mov edx, [esp+4i]; mov [eax+4+4i], edx  ; each stack argument
//     mov edx, [WATCH]; mov [eax+8+4n], edx   ; a global the function under test writes, as it is at the call
//     add dword [REC], 4(n+3)
//     mov eax, [REC]; xor eax, tag            ; a return value that differs per call
//       or  mov eax, [ecx+off]                ; (value stubs: a field of the called object)
//     ret 4n (thiscall) | ret (cdecl)
// The recorder block is part of every key's state, so the order, target, arguments and the watched global of each
// dispatched call are compared.
//
// Every key freezes the game's first thread with the shared helper re/frida/js/freeze.js: the functions write
// 0x0083ab30 / 0x0083ab34 (drag capture, read by hitTestScreen 0x0047f707 / 0x0047f70d on the input path),
// 0x0083ab2c (current widget), 0x0083b650 (ListBox drag row) and, through wrapText 0x00476e20 /
// wrapTextToWidth 0x00483980, 0x00839aa8. Those are saved and written back before the thread resumes, and they are
// state regions of the keys.
(function () {
  const G_DRAG = 0x0083ab30, G_DRAG2 = 0x0083ab34, G_CUR = 0x0083ab2c, G_ROW = 0x0083b650, G_MARKED = 0x0083b654;
  const G_WRAP = 0x00839aa8;

  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v >>> 0); }
  function alloc(n, v) { const p = Memory.alloc(n); fill(p, n, v === undefined ? 0 : v); return p; }

  function codeArena() {
    const size = Process.pageSize * 4;
    const base = Memory.alloc(size);
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3ct: code arena full');
      return p;
    };
  }

  // A recording stub (see the header). retOff === undefined: cursor ^ tag; otherwise [ecx + retOff].
  function stubBytes(rec, watch, tag, n, thiscall, retOff) {
    const r = u32(rec.toUInt32());
    const b = [];
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x05, ...r);                                              // add eax, REC
    b.push(0xC7, 0x00, ...u32(tag));                                 // mov dword [eax], tag
    if (thiscall) b.push(0x89, 0x48, 0x04);                          // mov [eax+4], ecx
    else b.push(0xC7, 0x40, 0x04, ...u32(0xC0C0C0C0));
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 + 4 * i);
    b.push(0x8B, 0x15, ...u32(watch));                               // mov edx, [WATCH]
    b.push(0x89, 0x50, 8 + 4 * n);                                   // mov [eax+8+4n], edx
    b.push(0x81, 0x05, ...r, ...u32(4 * (n + 3)));                   // add dword [REC], 4(n+3)
    if (retOff === undefined) {
      b.push(0xA1, ...r);                                            // mov eax, [REC]
      b.push(0x35, ...u32(tag));                                     // xor eax, tag
    } else {
      b.push(0x8B, 0x81, ...u32(retOff));                            // mov eax, [ecx+retOff]
    }
    if (thiscall && n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }

  function newRec(size) { const rec = alloc(size); rec.writeU32(0x10); return rec; }

  // A vtable of 0x80 slots: unused slots `xor eax, eax; ret`; spec [[slot offset, tag, n, retOff], ...] (thiscall).
  function vtable(emit, rec, watch, spec) {
    const nul = emit([0x33, 0xC0, 0xC3]);
    const vt = alloc(0x200);
    for (let i = 0; i < 0x80; i++) vt.add(i * 4).writePointer(nul);
    for (const [slot, tag, n, retOff] of spec) vt.add(slot).writePointer(emit(stubBytes(rec, watch, tag, n, true, retOff)));
    return vt;
  }

  // The font measurer slot +0x10 that measureTextWidth 0x00483930 calls as thiscall (text, count): the width of
  // `count` characters is the sum over them of W + (char & 3), W = the measurer's +4.
  //   push esi; push edi; mov esi,[esp+0xc]; mov edi,[esp+0x10]; xor eax,eax; test edi,edi; jle done
  //   loop: movzx edx,byte[esi]; and edx,3; add edx,[ecx+4]; add eax,edx; inc esi; dec edi; jnz loop
  //   done: pop edi; pop esi; ret 8
  const MEASURE = [0x56, 0x57, 0x8B, 0x74, 0x24, 0x0C, 0x8B, 0x7C, 0x24, 0x10, 0x33, 0xC0, 0x85, 0xFF, 0x7E, 0x0F,
                   0x0F, 0xB6, 0x16, 0x83, 0xE2, 0x03, 0x03, 0x51, 0x04, 0x03, 0xC2, 0x46, 0x4F, 0x75, 0xF1,
                   0x5F, 0x5E, 0xC2, 0x08, 0x00];

  // Freeze the first thread, save the globals the key writes, and optionally force Ctrl / Shift through the shim's
  // virtual-key overlay (GetKeyState's bit 15; 1 = forced up, 2 = forced down, 0 = released at the flush).
  function freezeAndSave(globals, vk) {
    let setVk = null;
    if (vk) {
      setVk = new NativeFunction(Process.getModuleByName('winmm.dll').getExportByName('SimGolfShim_SetVirtualKey'),
                                 'void', ['int', 'int'], { abi: 'stdcall', scheduling: 'exclusive' });
    }
    __freeze.thread();
    for (const g of globals) __freeze.save(ptr(g), 4);
    if (vk) {
      __freeze.atFlush(function () { setVk(0x11, 0); setVk(0x10, 0); });
      setVk(0x11, vk.ctrl ? 2 : 1);
      setVk(0x10, vk.shift ? 2 : 1);
    }
  }

  // ---- EditBox ---------------------------------------------------------------------------------------------------
  // EditBox objects (0x700 bytes, pre-filled 0x5a5a5a5a; the text widget's fields +0x274..+0x300 zeroed):
  //   +0      the EditBox vtable: slots +0x10 and +0x120 record (no argument)
  //   +0x274  the text widget: +0x14 (+0x288) first-line indent for wrapText, +0x34 (+0x2a8) / +0x38 (+0x2ac) style,
  //           +0x40 (+0x2b4) markup on, +0x5c..+0x68 (+0x2d0..+0x2dc) the font per style
  //   +0x278  child on the child vtable: slot +0xd8 records and returns the child's +0x40 (the width), slot +0xdc records
  //   +0x574  text; +0x57c first visible line; +0x598 / +0x59c margins; +0x5a4 caret; +0x5a8 caret flag
  //   +0x5ac  a Timer that is idle: +8 = 1, +0xc = 0, +0x10 = 0, +0x14 = 0, so Timer::startOnce returns 7 at 0x00486dd1
  //           and Timer::stop returns at 0x00486f04 without reaching an import
  // Fonts (0x20): +4 a measurer (vtable slot +0x10 = MEASURE, +4 = W); Widget_value 0x00477580 gives +0x10 + +8 when
  // +8 >= 0, else +0xc.
  //            child width, margins,  markup, fonts (W per style),   top, indent, text
  const EB = [[120, [2, 3], 0, [5, 5, 5, 5], 0, 0, 'alpha beta gamma delta epsilon zeta eta theta'],
              [null, [1, 1], 0, [7, 7, 7, 7], 0, 0, 'one two three four'],
              [90, [0, 4], 1, [5, 7, 3, 5], 1, 4, 'ab {cd ef} gh [ij kl] mn op qr st'],
              [200, [3, 0], 0, [3, 3, 3, 3], -3, 0, 'x'],
              [64, [0, 0], 0, [3, 3, 3, 3], 2, 0, ''],
              [300, [5, 5], 1, [7, 3, 5, 7], 0, 6, 'pre {bold run} and [other run] tail words wrap {x} end']];
  // The second variant changes widths and margins so the same texts wrap differently.
  const EB_WIDTH_B = [70, null, 150, 40, 30, 120];
  const EB_PAD_B = [[0, 0], [4, 4], [1, 1], [0, 2], [3, 3], [2, 2]];
  // Explicit line tables (lines, n) for objects 0, 2 and 5: start offsets into the text, the last entry its end.
  const EB_LINES = { 0: [0, 6, 17, 30, 45], 2: [0, 11, 22, 33], 5: [0, 8, 15, 26, 40, 56] };

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
      if (i % 2 === 0) { f.add(8).writeS32(2); f.add(0x10).writeS32(10); f.add(0xc).writeS32(77); }  // height 12
      else { f.add(8).writeS32(-1); f.add(0xc).writeS32(9); f.add(0x10).writeS32(55); }              // height 9
      fonts[key] = f;
      return f;
    }
    // all texts in one 0x200-byte buffer, each followed by 16 zero bytes
    const tbuf = alloc(0x200);
    fx.tbuf = tbuf;
    let off = 0;
    EB.forEach(function (r, i) {
      const [cw0, pad0, markup, styleW, top, indent, text] = r;
      const cw = variant === 'b' ? EB_WIDTH_B[i] : cw0;
      const pad = variant === 'b' ? EB_PAD_B[i] : pad0;
      const t = tbuf.add(off);
      t.writeByteArray(Array.from(text, (c) => c.charCodeAt(0)).concat([0]));
      off += text.length + 1 + 16;
      const eb = alloc(0x700, 0x5a5a5a5a);
      fill(eb.add(0x274), 0x8c, 0);
      eb.writePointer(vtE);
      eb.add(0x288).writeS32(indent);
      eb.add(0x2b4).writeS32(markup);
      for (let s = 0; s < 4; s++) eb.add(0x2d0 + 4 * s).writePointer(font(styleW[s], i));
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
      eb.add(0x5a4).writeS32(-7);
      eb.add(0x5a8).writeS32(0x33);
      const tm = eb.add(0x5ac);
      fill(tm, 0x30, 0);
      tm.add(8).writeU32(1);
      fx['eb' + i] = eb;
      if (EB_LINES[i]) {
        const L = alloc(0x40);
        EB_LINES[i].forEach(function (o, k) { L.add(4 * k).writePointer(t.add(Math.min(o, text.length))); });
        fx['lines' + i] = L;
      }
    });
    fx.lbuf = alloc(0x100, 0x3d3d3d3d);    // wrapToLines' caller-owned line array
    fx.cnt = alloc(0x10, 0x3e3e3e3e);      // and its count slot
  }

  // The drag-capture settings of the caret-drag keys: values of (0x0083ab30, 0x0083ab34).
  const DRAG = {
    none: (fx) => [0, 0],
    own: (fx) => [fx.eb0, 0],
    second: (fx) => [fx.eb1, fx.eb0],
    secondonly: (fx) => [0, fx.eb0],
    both: (fx) => [fx.eb0, fx.eb0],
  };

  function editFixture(variant, drag) {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x800);
    fx.rec = rec; fx.obj = rec;
    buildEditBoxes(fx, emit, rec, variant);
    freezeAndSave([G_DRAG, G_DRAG2, G_WRAP], null);
    const d = (DRAG[drag] || DRAG.none)(fx);
    ptr(G_DRAG).writePointer(ptr(d[0]));
    ptr(G_DRAG2).writePointer(ptr(d[1]));
    return fx;
  }

  // ---- scrollbars for scrollbarOnDrag -------------------------------------------------------------------------------
  const SB_DRAG = {
    none: (fx) => [0, 0],
    own: (fx) => [fx.sb0, 0],
    second: (fx) => [fx.sb1, fx.sb0],
    both: (fx) => [fx.sb0, fx.sb0],
    other: (fx) => [fx.sb2, fx.sb3],
  };
  function scrollFixture(drag) {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x400);
    fx.rec = rec; fx.obj = rec;
    const vtS = vtable(emit, rec, G_DRAG, [[0x10, 0x5B100000, 0], [0x120, 0x5B200000, 0]]);
    for (let i = 0; i < 4; i++) {
      const s = alloc(0x40, 0x0a0a0a0a + i);
      s.writePointer(vtS);
      fx['sb' + i] = s;
    }
    freezeAndSave([G_DRAG, G_DRAG2], null);
    const d = SB_DRAG[drag](fx);
    ptr(G_DRAG).writePointer(ptr(d[0]));
    ptr(G_DRAG2).writePointer(ptr(d[1]));
    return fx;
  }

  // ---- ListBox --------------------------------------------------------------------------------------------------------
  // One 0x800-byte block per ListBox (pre-filled 0x5a5a5a5a). P = block+0x40 holds the vbtable pointer; S = P-4 is
  // setSelection's this, T = P+0x58 onNavKey's / onDrag's this. vbtable [1] d1, [2] d2: W = P + d1, M = P + d2.
  //   S+0   vtable whose slot 0 (1 argument) records ("draw row")
  //   P+4   byte, bit 0: onDrag also selects (T-0x54)          P+8   multi-select flag (T-0x50)
  //   P+0xc force flag of setSelection (S+0x10)                 P+0x1c / P+0x20 the stored point (T-0x3c / T-0x38)
  //   P+0x24 top row (S+0x28, T-0x34)                           P+0x28 visible rows (S+0x2c, T-0x30)
  //   P+0x4c cdecl selection callback or 0 (S+0x50)
  //   W+0   vtable: slot +0x120 records; W-4 = d1 - 0x58 (the vtordisp displacement, W - that = T)
  //   W+0xa0 = 0: Window::visible 0x004801f0 returns 0, so Window::invalidate 0x00480ce0 returns at 0x00480ced
  //   W+0x130 parent (vtable slots +0x100 1 arg, +0xfc 2, +0xd4 1; +0x94 child with slot +0x1c) or 0
  //   W+0x26c / W+0x270 scrollbar objects (0x600 bytes; vtable slot +0x120; +0x130, +0x580 min, +0x584 max,
  //          +0x588 invert) or 0: fwdScroll26c / fwdScroll270 call the scrollbar setter 0x004967f0 on them
  //   M+0x24 byte, bit 2: onNavKey calls the parent's +0xd4      M+0x3c header height (R-0x1c)
  //   M+0x44 x origin (R-0x14)  M+0x48 spacing (R-0x10)  M+0x58 mode (R+0; 1 = rows with their own heights)
  //   M+0x5c rows per column / scroll step (R+4)  M+0x68 cell width (R+0x10)  M+0x6c cell height (R+0x14)
  //   M+0xc8 head, +0xcc cursor, +0xd0 count, +0xd4 index (R+0x70..0x7c)  M+0xf0 selected (R+0x98)  M+0xf4 hover (R+0x9c)
  // Nodes (0x20): +4 id, +8 item, +0xc next, +0x10 prev. Items (0x20): +8 selected flag, +0xc row height.
  //         d1     d2    mode rpc count vis top sel hov flg mul frc cb par sb26c sb270 nfy hdr  x0  sp  cw  ch
  const LB = [[0x100, 0x400, 1, 0, 6, 3, 2, 4, 3, 1, 1, 0, 1, 2, 1, 1, 1, 10, 0, 0, 0, 1],
              [0x140, 0x440, 3, 4, 12, 8, 5, 6, -1, 0, 1, 1, 0, 1, 1, 1, 0, 5, 3, 2, 30, 10],
              [0x180, 0x480, 0, 1, 2, 5, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 50, 9],
              [0x380, 0x100, 2, 3, 10, 2, 0, -1, 5, 1, 1, 0, 0, 2, 0, 1, 1, 4, -10, 0, 40, 12],
              [0x1c0, 0x4c0, 1, 0, 8, 4, 6, 7, 2, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0],
              [0x100, 0x400, 1, 0, 0, 3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
              [0x120, 0x420, 1, 0, 2, 5, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1],
              [0x160, 0x460, 5, 1, 9, 3, -2, 2, 8, 1, 1, 1, 1, 2, 1, 1, 1, -4, 5, 3, 20, 6],
              [0x100, 0x400, 1, 0, -1, 3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
              [0x100, 0x400, 1, 0, 3, 5, 1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1],
              [0x120, 0x420, 1, 0, 2, 4, 4, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
              [0x140, 0x440, 1, 0, 2, 4, -5, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1],
              [0x100, 0x400, 1, 0, 1, 3, 0, -1, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
              [0x120, 0x420, 1, 0, 2, 4, -5, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1],
              [0x100, 0x400, 0, 1, -1, 5, -10, -20, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 20, 10],
              [0x100, 0x400, 0, 1, -1, 5, -10, 0, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 20, 10]];
  // ListBoxes 8..11 reach sides the first eight cannot: 8 has a count of -1 (RowAt's `count <= 0` exit,
  // 0x0048c970 / 0x0048aad4), 9 has three rows but a NULL head and cursor (the passes skip the step, 0x0048c980 /
  // 0x0048aae4, and see no cursor, 0x0048c9a3 / 0x0048ab07), 10 has top 4 above its count of 2 (the row walk's seek
  // exits at its first test, 0x0048c921 / 0x0048aa85) and 11 has top -5 (|top-1| above the count, 0x0048c936 /
  // 0x0048aa9a; the clicked row is negative). 12 and 13 are multi-select: 12 has one row and no selection, so a
  // Shift click on row 0 places the cursor on row -2 (|-2| above the count, 0x0048ac5f); 13 has top -5, so the
  // clicked row is -5 or -4 and the walks to r-1 / r exit there too (0x0048ad15, 0x0048add7). 14 and 15 are grids
  // with a row count of -1 over a three-node ring and top -10: the clicked row (-10..-6) is below the count, so the
  // clear-all walk exits at once (0x0048aba3) and its loop is skipped (0x0048abd6); 14 has selection -20 (Shift arm
  // r > sel), 15 selection 0 (Shift arm r <= sel).
  // Row heights (item +0xc) and initially selected items (item +8) per ListBox.
  const LB_H = [[12, 8, 20, 6, 16, 10], null, null, null, [5, 5, 5, 5, 5, 5, 5, 5], null, [7, 9], null,
                null, null, null, null, [10], null, null, null];
  const LB_RING = { 14: 3, 15: 3 };   // ring size when it differs from the count
  const LB_SEL = [[0, 1, 0, 0, 1, 0], [1, 0, 0, 1], [1, 1], [0, 0, 1], [0, 1, 0, 1, 0, 1, 0, 1], null, [0, 0], [1],
                  null, null, null, null, [1], [0, 1], [1, 0, 1], [0, 1, 1]];

  function buildListBoxes(fx, emit, rec) {
    const vtS = vtable(emit, rec, G_ROW, [[0, 0x50000000, 1]]);
    const vtW = vtable(emit, rec, G_ROW, [[0x120, 0xA1200000, 0]]);
    const vtP = vtable(emit, rec, G_ROW, [[0x100, 0xB1000000, 1], [0xfc, 0xB0FC0000, 2], [0xd4, 0xB0D40000, 1]]);
    const vtC = vtable(emit, rec, G_ROW, [[0x1c, 0xC01C0000, 0]]);
    const vtB = vtable(emit, rec, G_CUR, [[0x120, 0x5B200000, 0]]);
    const cb = emit(stubBytes(rec, G_ROW, 0xD0000000, 1, false));
    LB.forEach(function (r, i) {
      const [d1, d2, mode, rpc, count, vis, top, sel, hov, flg, mul, frc, hasCb, par, sb26c, sb270, nfy,
             hdr, x0, sp, cw, ch] = r;
      const blk = alloc(0x800, 0x5a5a5a5a);
      const P = blk.add(0x40);
      const S = P.sub(4);
      const T = P.add(0x58);
      const vbt = alloc(0x10);
      vbt.writeU32(0); vbt.add(4).writeS32(d1); vbt.add(8).writeS32(d2);
      S.writePointer(vtS);
      P.writePointer(vbt);
      P.add(4).writeU32(flg);
      P.add(8).writeU32(mul);
      P.add(0xc).writeU32(frc);
      P.add(0x24).writeS32(top);
      P.add(0x28).writeS32(vis);
      P.add(0x4c).writePointer(hasCb ? cb : ptr(0));
      const aux = alloc(0x1000, 0x3c3c3c3c);   // parent +0, its child +0x200, scrollbars +0x300 / +0x900
      const lst = alloc(0x400, 0x1d1d1d1d);    // nodes +0x20*k, items +0x200+0x20*k
      fx['aux' + i] = aux; fx['lst' + i] = lst;
      const W = P.add(d1);
      W.writePointer(vtW);
      W.sub(4).writeS32(d1 - 0x58);
      W.add(0xa0).writeU32(0);
      if (par) {
        const p = aux;
        p.writePointer(vtP);
        p.add(0x94).writePointer(ptr(0));
        if (par === 2) {
          const c = aux.add(0x200);
          fill(c, 0x40, 0x2d2d2d2d);
          c.writePointer(vtC);
          p.add(0x94).writePointer(c);
          fx['lbch' + i] = c;
        }
        W.add(0x130).writePointer(p);
        fx['lbpar' + i] = p;
      } else {
        W.add(0x130).writePointer(ptr(0));
      }
      [[0x26c, sb26c, 0, 5, 0, 0x300], [0x270, sb270, 1, 3, 1, 0x900]].forEach(function ([off, on, mn, mx, inv, at]) {
        if (!on) { W.add(off).writePointer(ptr(0)); return; }
        const b = aux.add(at);
        fill(b, 0x600, 0x4e4e4e4e);
        b.writePointer(vtB);
        b.add(0x130).writeU32(0x13000000 + i * 0x100 + off);
        b.add(0x580).writeS32(mn); b.add(0x584).writeS32(mx); b.add(0x588).writeU32(inv);
        W.add(off).writePointer(b);
        fx['lbsb' + i + '_' + off.toString(16)] = b;
      });
      const M = P.add(d2);
      M.add(0x24).writeU32(nfy ? 4 : 0);
      M.add(0x3c).writeS32(hdr);
      M.add(0x44).writeS32(x0);
      M.add(0x48).writeS32(sp);
      M.add(0x58).writeS32(mode);
      M.add(0x5c).writeS32(rpc);
      M.add(0x68).writeS32(cw);
      M.add(0x6c).writeS32(ch);
      M.add(0xd0).writeS32(count);
      M.add(0xd4).writeS32(0x77);
      M.add(0xf0).writeS32(sel);
      M.add(0xf4).writeS32(hov);
      // The item list: a ring of `count` nodes (none for count 0). ListBox 6 is a two-node chain whose first node's
      // next is NULL (its prev is node 1): onDrag's row walk then lands on a NULL cursor in its last pass
      // (0x0048c9a3 / 0x0048ab07 taken) and stops, because the count ends the passes there.
      const items = [], nodes = [];
      const ring = LB_RING[i] || count;
      for (let k = 0; k < ring; k++) {
        const it = lst.add(0x200 + 0x20 * k);
        fill(it, 0x20, 0x1e1e1e1e);
        it.add(8).writeU32(LB_SEL[i] ? (LB_SEL[i][k % LB_SEL[i].length]) : 0);
        it.add(0xc).writeS32(LB_H[i] ? LB_H[i][k] : 10);
        const nd = lst.add(0x20 * k);
        nd.add(4).writeU32(0x100 * i + k);
        nd.add(8).writePointer(it);
        items.push(it); nodes.push(nd);
        fx['it' + i + '_' + k] = it;
        fx['nd' + i + '_' + k] = nd;
      }
      for (let k = 0; k < ring; k++) {
        nodes[k].add(0xc).writePointer(nodes[(k + 1) % ring]);
        nodes[k].add(0x10).writePointer(nodes[(k + ring - 1) % ring]);
      }
      if (i === 6) nodes[0].add(0xc).writePointer(ptr(0));
      const noList = ring <= 0 || i === 9;
      M.add(0xc8).writePointer(noList ? ptr(0) : nodes[0]);
      M.add(0xcc).writePointer(noList ? ptr(0) : nodes[1 % ring]);
      fx['blk' + i] = blk;
      fx['s' + i] = S;
      fx['t' + i] = T;
      fx['w' + i] = W;
    });
  }

  function listFixture(vk, marked) {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x2000);
    fx.rec = rec; fx.obj = rec;
    buildListBoxes(fx, emit, rec);
    freezeAndSave([G_CUR, G_ROW, G_MARKED], vk);
    ptr(G_ROW).writeS32(-0x2468);
    ptr(G_MARKED).writeS32(marked ? 1 : 0);
    return fx;
  }

  // ---- optionDialog grid (optionDialogOnMouse) -------------------------------------------------------------------------
  // One 0x400-byte block per object (pre-filled 0x5a5a5a5a). X = block+0x40; the vbtable pointer at X-0x1c; vbtable
  // [1] d1, [2] d2: the Window base W = X - 0x1c + d1 (vtable slot +0x120 records; W-4 = W - X for the thunk) and the
  // grid s = X + d2: +0x2c spacing, +0x4c cell width, +0x50 cell height, +0x40 rows per column, +0xd4 disabled mask,
  // +0xd8 hover index.
  //          d1     d2     sp cw  ch rows mask         hover
  const OD = [[0x100, 0x200, 2, 10, 8, 3, 0x00000000, 5],
              [0x120, 0x240, 0, 16, 16, 4, 0x0000f0f0, -1],
              [0x0c0, 0x180, 4, 20, 6, 1, 0xffffffff, 2],
              [0x100, 0x200, 1, 7, 9, 5, 0x80000001, -1],
              [0x140, 0x280, 3, 30, 12, 2, 0x12345678, 0]];
  // Also: W+0x130 a value the slot-38 body stores to 0x0083ab2c, X-0x10 a cdecl callback stub (objects 0, 2, 4) or 0.
  function buildOptions(fx, emit, rec) {
    const vtW = vtable(emit, rec, G_CUR, [[0x120, 0x0D120000, 0]]);
    const cb = emit(stubBytes(rec, G_CUR, 0xD2000000, 1, false));
    OD.forEach(function (r, i) {
      const [d1, d2, sp, cw, ch, rows, mask, hover] = r;
      const blk = alloc(0x400, 0x5a5a5a5a);
      const X = blk.add(0x40);
      const vbt = alloc(0x10);
      vbt.writeU32(0); vbt.add(4).writeS32(d1); vbt.add(8).writeS32(d2);
      X.sub(0x1c).writePointer(vbt);
      const W = X.sub(0x1c).add(d1);
      W.writePointer(vtW);
      W.sub(4).writeS32(d1 - 0x1c);
      const s = X.add(d2);
      s.add(0x2c).writeS32(sp); s.add(0x4c).writeS32(cw); s.add(0x50).writeS32(ch); s.add(0x40).writeS32(rows);
      s.add(0xd4).writeU32(mask); s.add(0xd8).writeS32(hover);
      W.add(0x130).writeU32(0x7b000000 + i);
      X.sub(0x10).writePointer(i % 2 === 0 ? cb : ptr(0));
      fx['odblk' + i] = blk; fx['od' + i] = X; fx['odw' + i] = W;
    });
  }
  function optionFixture(freeze) {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x400);
    fx.rec = rec; fx.obj = rec;
    buildOptions(fx, emit, rec);
    if (freeze) freezeAndSave([G_CUR], null);
    return fx;
  }

  // ---- listPanel selection (listSelectionChanged) ---------------------------------------------------------------------
  // One 0x400-byte block per object (pre-filled 0x5a5a5a5a). Y = block+0x80; the vbtable pointer at Y-0x24; vbtable
  // [1] d1, [2] d2. Y + d1 + 0x10c holds the value stored to 0x0083ab2c; Y-0x10 a cdecl callback stub or 0; the list
  // model M = Y - 0x24 + d2 (+0xc8 head, +0xcc cursor, +0xd0 count, +0xd4 index, +0xf0 selected index) walked by
  // ListModel::selectedId 0x00489950. The thunk's entry p = Y + 0x100 with [p-4] = 0x100.
  // For listPanelReadValue 0x0049e7d0 (reached by viewDispatchSlot27's tag 0x10): s = Y + d2 with +0x24, +0x38 (rows
  // per column), +0x44 (column width), +0x48, +0xd0 hover, +0xd4 previous hover (+0xcc is the selected index M+0xf0);
  // the child Y + d1 + 0x28 (vtable slot +0x1c records) or 0; the widget Y + d1 + 0x250 whose font +0x5c (Y + d1 +
  // 0x2ac) gives Widget_value 0x00477580 a line height of 11; each node's item (+8) has its line count at +8.
  //          d1    d2     cb count sel  s24 s38 s44 s48 d0  d4 child lines
  const LP = [[0x40, 0x200, 1, 4, 2, 2, 3, 10, 1, 1, 4, 1, [1, 2, 1, 3]],
              [0x60, 0x220, 0, 3, 0, 0, 1, 25, 5, -1, -1, 0, [2, 2, 2]],
              [0x40, 0x200, 1, 0, 0, 4, 2, 8, 0, 0, 3, 1, []],
              [0x80, 0x240, 1, 5, -2, 1, 4, 12, 2, 2, 2, 1, [0, 1, 4, 1, 2]],
              [0x40, 0x200, 1, 3, 7, 3, 1, 30, 3, -1, 0, 0, [1, 1, 1]]];
  // The Window base W = Y - 0x24 + d1 carries a vtable whose slot +0x120 records (W+0x130 is the stored value above).
  function buildPanels(fx, emit, rec) {
    const vtCh = vtable(emit, rec, G_CUR, [[0x1c, 0x0F1C0000, 0]]);
    const font = alloc(0x20, 0x0f0f0f0f);
    font.add(8).writeS32(2); font.add(0x10).writeS32(9);
    const cb = emit(stubBytes(rec, G_CUR, 0xD1000000, 1, false));
    const vtW = vtable(emit, rec, G_CUR, [[0x120, 0x0E120000, 0]]);
    LP.forEach(function (r, i) {
      const [d1, d2, hasCb, count, sel, s24, s38, s44, s48, d0, d4, hasChild, lines] = r;
      const blk = alloc(0x400, 0x5a5a5a5a);
      const Y = blk.add(0x80);
      const vbt = alloc(0x10);
      vbt.writeU32(0); vbt.add(4).writeS32(d1); vbt.add(8).writeS32(d2);
      Y.sub(0x24).writePointer(vbt);
      Y.add(d1 + 0x10c).writeU32(0x7a000000 + i);
      Y.sub(0x24).add(d1).writePointer(vtW);
      Y.sub(0x10).writePointer(hasCb ? cb : ptr(0));
      Y.add(0xfc).writeS32(0x100);
      const M = Y.sub(0x24).add(d2);
      const lst = alloc(0x200, 0x1d1d1d1d);   // nodes +0x20*k, items +0x100+0x10*k, child +0x1c0
      const nodes = [];
      for (let k = 0; k < count; k++) {
        const nd = lst.add(0x20 * k);
        const it = lst.add(0x100 + 0x10 * k);
        nd.add(4).writeU32(0x500 + 0x10 * i + k);
        nd.add(8).writePointer(it);
        it.add(8).writeS32(lines[k]);
        nodes.push(nd);
      }
      for (let k = 0; k < count; k++) {
        nodes[k].add(0xc).writePointer(nodes[(k + 1) % count]);
        nodes[k].add(0x10).writePointer(nodes[(k + count - 1) % count]);
      }
      M.add(0xc8).writePointer(count ? nodes[0] : ptr(0));
      M.add(0xcc).writePointer(count ? nodes[count - 1] : ptr(0));
      M.add(0xd0).writeS32(count);
      M.add(0xd4).writeS32(0x66);
      M.add(0xf0).writeS32(sel);
      const sP = Y.add(d2);
      sP.add(0x24).writeS32(s24); sP.add(0x38).writeS32(s38); sP.add(0x44).writeS32(s44); sP.add(0x48).writeS32(s48);
      sP.add(0xd0).writeS32(d0); sP.add(0xd4).writeS32(d4);
      if (hasChild) {
        const ch = lst.add(0x1c0);
        ch.writePointer(vtCh);
        Y.add(d1 + 0x28).writePointer(ch);
      } else {
        Y.add(d1 + 0x28).writePointer(ptr(0));
      }
      Y.add(d1 + 0x2ac).writePointer(font);
      fx['lpblk' + i] = blk; fx['lplst' + i] = lst; fx['lp' + i] = Y; fx['lpv' + i] = Y.add(0x100);
    });
  }
  function panelFixture() {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x400);
    fx.rec = rec; fx.obj = rec;
    buildPanels(fx, emit, rec);
    freezeAndSave([G_CUR], null);
    return fx;
  }

  // ---- views for viewDispatchSlot27 0x004a0ab0 ------------------------------------------------------------------------
  // The dispatcher reads a tag at V-8 and enters the sub-object for it: 1 -> optionDialogOnMouse on V-0x168, 2 ->
  // ListBox::onDrag on V-0x1a0, 8 -> 0x004a4340 (`ret 8`) on V-0x8c, 0x10 -> listPanelReadValue on V-0x180; any other
  // tag returns. Views are placed inside the objects above: $vo<i> = option X + 0x168, $vl<i> = ListBox T + 0x1a0
  // (not ListBox 3, whose list model covers that dword), $vp<i> = panel Y + 0x180, and $vx<k> in a separate block for
  // tag 8 and the tags with no case. Every view has [V+0x1c] = 0x20, so the thunk 0x00491980 is entered at V + 0x20.
  const VIEW_X_TAGS = [8, 0, 3, 7, 9, 15, 17, -1];
  function viewFixture() {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x2000);
    fx.rec = rec; fx.obj = rec;
    buildOptions(fx, emit, rec);
    buildPanels(fx, emit, rec);
    buildListBoxes(fx, emit, rec);
    function view(name, V, tag) {
      V.sub(8).writeS32(tag);
      V.add(0x1c).writeS32(0x20);
      fx[name] = V;
      fx[name + 'p'] = V.add(0x20);
    }
    for (let i = 0; i < OD.length; i++) view('vo' + i, fx['od' + i].add(0x168), 1);
    for (let i = 0; i < LB.length; i++) if (i !== 3) view('vl' + i, fx['t' + i].add(0x1a0), 2);
    for (let i = 0; i < LP.length; i++) view('vp' + i, fx['lp' + i].add(0x180), 0x10);
    const xb = alloc(0x400, 0x5a5a5a5a);
    fx.vxblk = xb;
    VIEW_X_TAGS.forEach(function (tag, k) { view('vx' + k, xb.add(0x40 + 0x60 * k), tag); });
    freezeAndSave([G_CUR, G_ROW, G_MARKED], null);
    ptr(G_ROW).writeS32(-0x1357);
    return fx;
  }

  // ---- Timers (Timer_fire) ---------------------------------------------------------------------------------------
  // Six 0x40-byte timers (pre-filled 0x6b6b6b6b): +8 flags, +0xc 0 (no Win32 timer: Timer::stop 0x00486ec0 skips
  // its imports at 0x00486ed1 and, with bit 0 set, returns at 0x00486f04), +0x10 cdecl callback (1 argument) or 0,
  // +0x14 cdecl callback (2 arguments) or 0, +0x18 / +0x1c their arguments, +0x24 cleared by Timer_fire.
  // Globals per variant: 0x0083ad4c (busy) and 0x0083aff0 (owner timer).
  //          flags cb1 cb2 v18  v1c
  const TM = [[0, 1, 1, 5, 7],
              [1, 1, 0, -1, 9],
              [3, 1, 1, 0, 0],
              [2, 0, 1, 11, -4],
              [0, 0, 0, 1, 2],
              [1, 0, 1, 3, 3]];
  function timerFixture(busy, owner) {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x400);
    fx.rec = rec; fx.obj = rec;
    const cb1 = emit(stubBytes(rec, 0x0083ad4c, 0x71000000, 1, false));
    const cb2 = emit(stubBytes(rec, 0x0083ad4c, 0x72000000, 2, false));
    TM.forEach(function (r, i) {
      const [flags, c1, c2, v18, v1c] = r;
      const t = alloc(0x40, 0x6b6b6b6b);
      t.add(8).writeU32(flags);
      t.add(0xc).writeU32(0);
      t.add(0x10).writePointer(c1 ? cb1 : ptr(0));
      t.add(0x14).writePointer(c2 ? cb2 : ptr(0));
      t.add(0x18).writeS32(v18);
      t.add(0x1c).writeS32(v1c);
      fx['tm' + i] = t;
    });
    freezeAndSave([0x0083ad4c, 0x0083aff0], null);
    ptr(0x0083ad4c).writeU32(busy);
    ptr(0x0083aff0).writePointer(owner === null ? ptr(0) : fx['tm' + owner]);
    return fx;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3ct_option() { return optionFixture(false); },
    c3ct_option_frozen() { return optionFixture(true); },
    c3ct_panel() { return panelFixture(); },
    c3ct_view() { return viewFixture(); },
    c3ct_timer() { return timerFixture(0, null); },
    c3ct_timer_busy() { return timerFixture(0x3f, null); },
    c3ct_timer_owner0() { return timerFixture(0, 0); },
    c3ct_timer_owner3() { return timerFixture(0, 3); },
    c3ct_edit() { return editFixture('a', 'none'); },
    c3ct_edit_b() { return editFixture('b', 'none'); },
    c3ct_edit_own() { return editFixture('a', 'own'); },
    c3ct_edit_second() { return editFixture('a', 'second'); },
    c3ct_edit_secondonly() { return editFixture('a', 'secondonly'); },
    c3ct_edit_both() { return editFixture('b', 'both'); },
    c3ct_scroll() { return scrollFixture('none'); },
    c3ct_scroll_own() { return scrollFixture('own'); },
    c3ct_scroll_second() { return scrollFixture('second'); },
    c3ct_scroll_both() { return scrollFixture('both'); },
    c3ct_scroll_other() { return scrollFixture('other'); },
    c3ct_list() { return listFixture({ ctrl: 0, shift: 0 }, false); },
    c3ct_list_marked() { return listFixture({ ctrl: 0, shift: 0 }, true); },
    c3ct_list_ctrl() { return listFixture({ ctrl: 1, shift: 0 }, false); },
    c3ct_list_shift() { return listFixture({ ctrl: 0, shift: 1 }, false); },
    c3ct_list_ctrlshift() { return listFixture({ ctrl: 1, shift: 1 }, true); },
  });
})();
