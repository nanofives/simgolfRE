// Fixtures for C3 batch c3dv (round 28, 2026-10-11): golf_clean.exe drawing rows (text widgets, window tree, panel layouts).
// Reimplementations: shim/src/re/c3dv.cpp. Registry: re/frida/registry.d/c3dv.py. Purposes and per-jcc coverage:
// log/c3/c3dv_purpose.md.
//
// The helpers up to buildText are c3dq's (fixtures.d/c3dq.js): everything a vector touches is OURS (one arena per fixture
// call, the state regions of the key); machine-code stubs in a private rwx page record into $rec, so no JavaScript runs
// inside a vector. Keys that read live globals freeze the game's first thread (re/frida/js/freeze.js), point the globals at
// our objects and let the flush restore them.
(function () {
  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function alloc(n) {
    const p = __keepAlloc(n);
    for (let i = 0; i < n; i += 4) p.add(i).writeU32(0);
    return p;
  }
  // The negative-control build (log/c3/alt/c3dv_negctl_gen.py) keeps its wrong bodies dormant until its exported NegCtl switch is set;
  // in the normal build there is no such export and nothing is written.
  function negOn() {
    __freeze.negOn();   // every exported <Batch>NegCtl switch (re/frida/js/freeze.js)
  }
  // Freeze the game's first thread, then (only then) switch the negative-control flag on: the game calls several of these
  // functions on its own thread, so a wrong body must not be live while that thread still runs.
  function freezeNow() {
    __freeze.thread();
    negOn();
  }
  function codeArena() {
    const size = Process.pageSize * 16;
    const base = __keepAlloc(size);
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3dv: code arena full');
      return p;
    };
  }
  // n value arguments are recorded; ret: undefined -> cursor ^ tag, a number -> that constant
  function stubBytes(rec, tag, n, ret, body, skip) {
    const r = u32(rec.toUInt32());
    const b = [0x53];                                                  // push ebx
    b.push(0x8B, 0x1D, ...r);                                          // mov ebx, [REC]
    b.push(0x81, 0xC3, ...r);                                          // add ebx, REC
    b.push(0xC7, 0x03, ...u32(tag));                                   // mov dword [ebx], tag
    b.push(0x89, 0x4B, 0x04);                                          // mov [ebx+4], ecx
    let off = 8;
    for (let i = 0; !skip && i < n; i++) {
      b.push(0x8B, 0x44, 0x24, 8 + 4 * i, 0x89, 0x43, off); off += 4;  // mov eax,[esp+8+4i]; mov [ebx+off],eax
    }
    b.push(0x81, 0x05, ...r, ...u32(off));                             // add dword [REC], off
    if (body) b.push(...body);                                         // extra effect (argument at [esp+8+4i])
    if (ret === undefined) b.push(0xA1, ...r, 0x35, ...u32(tag));      // mov eax, [REC]; xor eax, tag
    else b.push(0xB8, ...u32(ret));                                    // mov eax, ret
    b.push(0x5B);                                                      // pop ebx
    if (n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }

  // A recording display: 0x100 slots; the slot offsets in ARGS take that many stack arguments (callee pops).
  const DISP_ARGS = { 0x88: 1, 0x94: 1 };
  function makeDisplay(emit, rec, retOverride, bodies, argsOverride, skipMap) {
    const dvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) {
      const off = i * 4;
      const ret = retOverride && retOverride[off] !== undefined ? retOverride[off] : undefined;
      const body = bodies && bodies[off] ? bodies[off] : undefined;
      const nargs = argsOverride && argsOverride[off] !== undefined ? argsOverride[off] : (DISP_ARGS[off] || 0);
      dvt.add(off).writePointer(emit(stubBytes(rec, 0xD15A0000 + off, nargs, ret, body, skipMap ? !!skipMap[off] : off === 0x38)));   // 0x38: stack pointer
    }
    const disp = alloc(0x40);
    disp.writePointer(dvt);
    disp.add(4).writeU32(0xD15D15D1);
    return disp;
  }

  const REC_SIZE = 0x2000;
  function newRec(size) {
    const rec = alloc(size || REC_SIZE);
    rec.writeU32(0x10);
    return rec;
  }
  // ---- recording stub with argument kinds (c3da's builder): 'v' = value, 'r' = the four dwords behind a pointer, 's' =
  // skipped. ret: undefined -> cursor ^ tag, a number -> that constant. thiscall: ecx is recorded.
  function stubK(rec, tag, kinds, ret, noEcx, cdecl) {
    const r = u32(rec.toUInt32());
    const b = [0x53];                                                  // push ebx
    b.push(0x8B, 0x1D, ...r);                                          // mov ebx, [REC]
    b.push(0x81, 0xC3, ...r);                                          // add ebx, REC
    b.push(0xC7, 0x03, ...u32(tag));                                   // mov dword [ebx], tag
    if (noEcx) b.push(0xC7, 0x43, 0x04, 0, 0, 0, 0);                   // stdcall slot: ecx is not an argument
    else b.push(0x89, 0x4B, 0x04);                                     // mov [ebx+4], ecx
    let off = 8;
    kinds.split('').forEach(function (k, i) {
      const sp = 8 + 4 * i;
      if (k === 'v') {
        b.push(0x8B, 0x44, 0x24, sp, 0x89, 0x43, off); off += 4;
      } else if (k === 'r') {
        b.push(0x8B, 0x54, 0x24, sp);                                  // mov edx,[esp+sp]
        for (let j = 0; j < 4; j++) { b.push(0x8B, 0x42, 4 * j, 0x89, 0x43, off); off += 4; }
      } else if (k === 'n') {                                          // a RECT* that may be NULL: nothing is recorded then
        b.push(0x8B, 0x54, 0x24, sp, 0x85, 0xD2, 0x74, 0x18);          // mov edx,[esp+sp]; test edx,edx; jz +24
        for (let j = 0; j < 4; j++) { b.push(0x8B, 0x42, 4 * j, 0x89, 0x43, off); off += 4; }
      }
    });
    if (off > 0x7f) throw new Error('c3dv: entry too long');
    b.push(0x81, 0x05, ...r, ...u32(off));                             // add dword [REC], off
    if (ret === undefined) b.push(0xA1, ...r, 0x35, ...u32(tag));      // mov eax, [REC]; xor eax, tag
    else b.push(0xB8, ...u32(ret));                                    // mov eax, ret
    b.push(0x5B);                                                      // pop ebx
    const n = kinds.length;
    if (n > 0 && !cdecl) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }
  // ---- text objects (c3df's model: +0xc draw callback, +0x5c.. fonts, +4 handle) ------------------------------
  // The draw callback at object +0xc (cdecl (text, x, y, len), called first by drawRichText 0x004775b0 through
  // 0x004767a0) appends {tag, 0xC0C0C0C0, text, x, y, len, [watch]} to the recorder and returns (cursor ^ tag), always
  // non-zero, so drawRichText returns at once and every call's arguments are compared through $rec. The font measurer
  // (slot +0x10 of its vtable) returns the sum over the count characters of (W + (char & 3)), W = measurer + 4.
  function cbBytes(rec, watch, tag, n) {
    const r = u32(rec.toUInt32());
    const b = [];
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x05, ...r);                                              // add eax, REC
    b.push(0xC7, 0x00, ...u32(tag));                                 // mov dword [eax], tag
    b.push(0xC7, 0x40, 0x04, ...u32(0xC0C0C0C0));                    // mov dword [eax+4], 0xC0C0C0C0
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 + 4 * i);
    b.push(0x8B, 0x15, ...u32(watch));                               // mov edx, [WATCH]
    b.push(0x89, 0x50, 8 + 4 * n);                                   // mov [eax+8+4n], edx
    b.push(0x81, 0x05, ...r, ...u32(4 * (n + 3)));                   // add dword [REC], 4(n+3)
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x35, ...u32(tag));                                       // xor eax, tag
    b.push(0xC3);
    return b;
  }
  const MEASURE = [0x56, 0x57, 0x8B, 0x74, 0x24, 0x0C, 0x8B, 0x7C, 0x24, 0x10, 0x33, 0xC0, 0x85, 0xFF, 0x7E, 0x0F,
                   0x0F, 0xB6, 0x16, 0x83, 0xE2, 0x03, 0x03, 0x51, 0x04, 0x03, 0xC2, 0x46, 0x4F, 0x75, 0xF1,
                   0x5F, 0x5E, 0xC2, 0x08, 0x00];
  function bytesOf(s) { return Array.from(s, function (c) { return c.charCodeAt(0); }); }
  const TEXTS = {
    s_plain: 'aaa bbb ccc ddd eee', s_one: 'abcdefgh', s_braces: 'ab {cd ef} gh ij', s_long:
      'the quick brown fox jumps over the lazy dog again and again', s_spaces: '  lead  and trail  ', s_empty: '',
    s_short: 'xy',
  };

  // opts.global: swap the tgt object into [0x004c1570] (freezing the first thread); opts.h: its handle's slot +0xdc
  // result (undefined: no handle); opts.fonts: false -> no fonts; opts.scale: value for [0x00822c8c].
  function buildText(opts) {
    opts = opts || {};
    const emit = codeArena();
    const rec = newRec();
    const watch = alloc(16);
    const fx = { rec: rec, watch: watch };
    const cb = emit(cbBytes(rec, watch.toUInt32(), 0xC3D90001, 4));
    const nul = emit([0x33, 0xC0, 0xC3]);
    const meas = emit(MEASURE);
    const mvt = alloc(0x200);
    for (let i = 0; i < 0x80; i++) mvt.add(i * 4).writePointer(nul);
    mvt.add(0x10).writePointer(meas);
    const font = function (w, h, p8, pc, nomeas) {
      const m = alloc(0x10);
      m.writePointer(mvt); m.add(4).writeS32(w);
      const f = alloc(0x20);
      if (!nomeas) f.add(4).writePointer(m);
      f.add(8).writeS32(p8 || 0); f.add(0xc).writeS32(pc || 0); f.add(0x10).writeS32(h);
      return f;
    };
    const F = [font(3, 12), font(5, 14), font(7, 16), font(11, 18)];
    fx.f0 = F[0]; fx.f1 = F[1]; fx.f2 = F[2]; fx.f3 = F[3];
    fx.fneg = font(3, 12, -1, 21);           // Widget_value: +8 < 0 returns +0xc (21)
    fx.fnomeas = font(3, 12, 0, 0, true);    // measurer NULL: the font swap is refused, the draw wrappers return 3
    // handle objects: slot +0xdc returns H
    const handle = function (H) {
      const vt = alloc(0x400);
      for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
      vt.add(0xdc).writePointer(emit([0xB8, ...u32(H), 0xC3]));
      const h = alloc(0x20);
      h.writePointer(vt);
      return h;
    };
    const tobj = function (o) {
      const t = alloc(0x300);
      t.writeU32(0x54580000 + (o.id || 0));
      if (o.h !== undefined) t.add(4).writePointer(handle(o.h));
      if (o.cb !== false) t.add(0xc).writePointer(cb);
      t.add(0x14).writeS32(o.indent || 0);
      t.add(0x34).writeS32(o.style || 0);
      t.add(0x40).writeS32(o.markup === undefined ? 1 : o.markup);
      t.add(0x48).writeS32(o.drop || 0);
      t.add(0x54).writeS32(o.narrow || 0);
      const fonts = o.fonts === false ? [] : (o.fontlist || F);
      for (let i = 0; i < 4; i++) if (fonts[i]) t.add(0x5c + 4 * i).writePointer(fonts[i]);
      return t;
    };
    fx.tm = tobj({ id: 1, h: 100 });                                   // markup on, all four fonts, handle height 100
    fx.th0 = tobj({ id: 2, h: 0 });                                    // handle whose height is 0
    fx.tnoh = tobj({ id: 3 });                                         // no handle
    fx.tnofont = tobj({ id: 4, h: 100, fonts: false, markup: 0 });     // no fonts: the wrappers return 3
    fx.tplain = tobj({ id: 5, h: 100, markup: 0, fontlist: [F[1]] });
    fx.tneg = tobj({ id: 6, h: 100, fontlist: [fx.fneg, F[1], F[2], F[3]] });
    fx.tnomeas = tobj({ id: 7, h: 100, fontlist: [fx.fnomeas] });
    fx.tind = tobj({ id: 8, h: 100, indent: 20 });                     // first-line indent 20
    fx.tind.add(0x18).writeS32(20);                                    // pending indent (measureText4 reads it without resetting)
    fx.tind2 = tobj({ id: 9, h: 100, indent: 3, fontlist: [F[1]], markup: 0 });
    fx.tbig = tobj({ id: 10, h: 100, fontlist: [font(5, 30)] });          // Widget_value 30: drawValueWidget's H = 26
    fx.tmid = tobj({ id: 11, h: 100, fontlist: [font(4, 16)], markup: 0 }); // Widget_value 16: H = 12
    fx.tmcb = tobj({ id: 12, h: 100 });                                      // callback at +0x10: cdecl (id, kind), recorded
    fx.tmcb.add(0x10).writePointer(emit(stubK(rec, 0xCB100010, 'vv', undefined, true, true)));
    fx.tsmall = tobj({ id: 13, h: 30 });                                     // handle height 30: drawMenuList stops early
    fx.lines = alloc(0x80); fx.cnt = alloc(4); fx.cnt.writeS32(0x7777);
    fx.tgt = opts.tgt ? fx[opts.tgt] : fx.tm;
    // strings: NUL-terminated, 16 zero bytes after each
    let size = 0;
    Object.keys(TEXTS).forEach(function (k) { size += TEXTS[k].length + 1 + 16; });
    const buf = alloc((size + 3) & ~3);
    let off = 0;
    Object.keys(TEXTS).forEach(function (k) {
      if (TEXTS[k].length) buf.add(off).writeByteArray(bytesOf(TEXTS[k]));
      fx[k] = buf.add(off);
      off += TEXTS[k].length + 1 + 16;
    });
    fx.buf = buf;
    fx.obj = buf;
    const rect = function (l, t, r, b) { const p = alloc(0x10); [l, t, r, b].forEach(function (v, i) { p.add(4 * i).writeS32(v); }); return p; };
    fx.r0 = rect(0, 0, 100, 40); fx.r1 = rect(10, 20, 60, 25); fx.r2 = rect(-5, -8, 30, 50); fx.r3 = rect(0, 0, 0, 0);
    fx.r4 = rect(7, 3, 200, 160); fx.r5 = rect(10, 4, 11, 5);
    if (!opts.glob && !opts.global) freezeNow();
    if (opts.glob) {
      // text layout globals: 0x00839a9c (x cursor), 0x00839aa0 (y accumulator), 0x00839aa4 (limit), 0x00839aa8 (accumulator)
      // and 0x00839a5c (last draw result)
      freezeNow();
      __freeze.save(ptr('0x00839a9c'), 0x10);
      __freeze.save(ptr('0x00839a5c'), 4);
      ptr('0x00839a9c').writeS32(opts.glob[0]);
      ptr('0x00839aa0').writeS32(opts.glob[1]);
      ptr('0x00839aa4').writeS32(opts.glob[2]);
      ptr('0x00839aa8').writeS32(opts.glob[3]);
      ptr('0x00839a5c').writeS32(0x5c5c);
    }
    if (opts.global) {
      freezeNow();
      __freeze.setU32(ptr('0x004c1570'), fx.tgt.toUInt32());
      if (opts.scale !== undefined) __freeze.setU32(ptr('0x00822c8c'), opts.scale);
    }
    return fx;
  }


  // ---- shared helpers of this batch -----------------------------------------------------------------------------
  function zero(p, n) { p.writeByteArray(new Array(n).fill(0)); }
  // save a global region, clear it and write a string into it
  function setStr(addr, s, cap) {
    __freeze.save(ptr(addr), cap);
    zero(ptr(addr), cap);
    if (s && s.length) ptr(addr).writeByteArray(bytesOf(s));
  }
  function setWords(addr, arr) {          // save the region and write 32-bit values
    __freeze.save(ptr(addr), 4 * arr.length);
    arr.forEach(function (v, i) { ptr(addr).add(4 * i).writeU32(v >>> 0); });
  }
  const F = {};

  // ---- drawTextBlock / TextView_beginDrawRun ----------------------------------------------------------------------
  // glob: [x cursor, y, width limit, accumulated width] (0x00839a9c..0x00839aa8); buf: the pending line buffer 0x00839658
  // (0x404 bytes, saved and cleared). The draw callback of the text objects records every drawText call into $rec.
  function buildTB(o) {
    const fx = buildText({ glob: o.glob });
    __freeze.save(ptr('0x00839658'), 0x404);
    zero(ptr('0x00839658'), 0x404);
    if (o.buf) ptr('0x00839658').writeByteArray(bytesOf(o.buf));
    return fx;
  }
  F.c3dv_tb_base = function () { return buildTB({ glob: [11, 7, 60, 0] }); };
  F.c3dv_tb_pre = function () { return buildTB({ glob: [11, 7, 60, 0], buf: 'ab' }); };
  F.c3dv_tb_lim0 = function () { return buildTB({ glob: [11, 7, 0, 0] }); };
  F.c3dv_tb_acc = function () { return buildTB({ glob: [11, 7, 60, 50] }); };
  F.c3dv_tb_accfull = function () { return buildTB({ glob: [11, 7, 60, 60] }); };
  F.c3dv_tb_wide = function () { return buildTB({ glob: [11, 7, 400, 5] }); };
  F.c3dv_tb_pre_acc = function () { return buildTB({ glob: [20, 9, 70, 30], buf: 'xyz' }); };
  F.c3dv_tb_neg = function () { return buildTB({ glob: [-4, 100, 45, 0] }); };

  // ---- the shared message buffer 0x0051a068 as input (renderTextWidget45af30, drawValueWidget) ---------------------
  // The current text object [0x004c1570] is swapped for opts.tgt (default tm, handle height 100); Text_put measures with the
  // fixture font measurer (width = font width + (char & 3) per character). The buffer region is 0x200 bytes.
  function buildMsg(o) {
    const fx = buildText({ global: true, tgt: o.tgt || 'tm', scale: 800 });
    setStr('0x0051a068', o.text, 0x200);
    return fx;
  }
  const MSGS = {
    plain: 'one two three four five six seven',
    caret: 'ab^cd ef^gh',
    nl: 'ab\ncd ef\n',
    under: 'under_score and_more words_here_now',
    nospace: 'abcdefghijklmnopqrstuvwxyz',
    empty: '',
    tail: 'x^',
    sp: ' a  b   c ',
    long: 'the quick brown fox jumps over the lazy dog again and again and again and yet again and once more',
  };
  Object.keys(MSGS).forEach(function (k) {
    F['c3dv_rt_' + k] = function () { return buildMsg({ text: MSGS[k] }); };
    F['c3dv_vw_' + k] = function () { return buildMsg({ text: MSGS[k] }); };
    F['c3dv_vwb_' + k] = function () { return buildMsg({ text: MSGS[k], tgt: 'tbig' }); };
  });

  // ---- floating labels: drawLabels 0x0040c7a0 and drawMoneyLabels 0x0040c910 ---------------------------------------
  // The text object tm is [0x004c1570]; the object at 0x0051b360 (setDrawTarget's `surf` argument) is overwritten with a
  // font built from the fixture's own measurer (or with +4 = 0, which setDrawTarget refuses to store). worldToScreen reads the
  // camera: tile (4, 4) at 0x004c2ba0 / 0x004c2ba4, zoom 128 at 0x004c2844, view 800 x 600 at 0x00822c8c / 0x00822c90, rotation
  // 0x005685f4 = 0, doubling flag 0x005a9cc0 = 0. With these values world (4608 - d, 4608 - d) projects to
  // sx = 1424 - 2d, sy = 300, so d = 600 lies on screen and -200000 off it.
  function labEnv(o) {
    const fx = buildText({ global: true, tgt: 'tm', scale: 800 });
    [[0x00822c90, 600], [0x004c2844, 128], [0x004c2ba0, 4], [0x004c2ba4, 4], [0x005685f4, 0], [0x005a9cc0, 0]].forEach(function (e) {
      __freeze.setU32(ptr(e[0]), e[1] >>> 0);
    });
    // on-screen world points under these camera globals, found with the game's own worldToScreen 0x0042fb90 (margin 0)
    // worldToScreen adds a tile-height term (0x0042fe11..0x0042ffa6) unless the tile type's flags (0x0057837c + type * 0x30) have bit 8
    // set: tiles (2..5, 2..5) get type 0 (0x005722e8 + tx * 50 + ty) and the type-0 flags are 8, so the height term is skipped
    for (let tx = 2; tx <= 5; tx++) {
      for (let ty = 2; ty <= 5; ty++) {
        const a = ptr('0x005722e8').add(tx * 50 + ty);
        __freeze.save(a, 1);
        a.writeU8(0);
      }
    }
    __freeze.save(ptr('0x0057837c'), 4);
    ptr('0x0057837c').writeU32(8);
    const w2s = new NativeFunction(ptr('0x0042fb90'), 'int', ['int', 'int', 'pointer', 'pointer', 'int']);
    const px = alloc(16);
    fx.vis = [];
    for (let wx = 3400; wx < 4800 && fx.vis.length < 6; wx += 160) {
      for (let wy = 3400; wy < 4800 && fx.vis.length < 6; wy += 230) {
        if (w2s(wx, wy, px, px.add(4), 0) !== 0) fx.vis.push([wx, wy]);
      }
    }
    if (fx.vis.length < 6) {
      const dbg = [];
      [[4008, 4008], [4608, 4608], [3500, 3500]].forEach(function (pp) {
        px.writeS32(0x7777); px.add(4).writeS32(0x7778);
        const r = w2s(pp[0], pp[1], px, px.add(4), 0);
        dbg.push(pp[0] + ',' + pp[1] + ' -> ' + r + ' (' + px.readS32() + ',' + px.add(4).readS32() + ')');
      });
      const g = ['0x00822c8c', '0x00822c90', '0x004c2844', '0x004c2ba0', '0x004c2ba4', '0x005685f4', '0x004c2e00', '0x005a9cc0'].map(function (a) { return a + '=' + ptr(a).readS32(); });
      throw new Error('c3dv: worldToScreen found only ' + fx.vis.length + ' visible points; ' + dbg.join(' | ') + ' ; ' + g.join(' '));
    }
    __freeze.save(ptr('0x0051b360'), 0x20);
    zero(ptr('0x0051b360'), 0x20);
    if (!o.nofont) {
      ptr('0x0051b360').add(4).writePointer(fx.f1.add(4).readPointer());
      ptr('0x0051b360').add(0x10).writeS32(14);
    }
    setStr('0x0051a068', '', 0x200);
    __freeze.save(ptr('0x0058a528'), 0x20);
    return fx;
  }
  const LABELS = ['north', 'a', 'b long label', 'dd', 'e', 'five five', 'six', 'sevenths'];
  const FAR = [-200000, 4008];
  // positions per slot: V[i] are the on-screen points labEnv found, FAR is off screen, [0, 0] belongs to a slot whose counter is 0
  const POSA = function (V) { return [V[0], [0, 0], FAR, V[1], [0, 0], V[2], V[3], FAR]; };      // last slot off screen
  const POSB = function (V) { return [V[0], [0, 0], FAR, V[1], [0, 0], V[2], V[3], V[4]]; };     // last slot on screen
  const POSV = function (V) { return [V[0], V[1], V[2], V[3], V[4], V[5], V[0], V[1]]; };        // every slot on screen
  function buildLabels(o) {
    const fx = labEnv(o);
    setWords('0x0056a924', o.cnt);
    setWords('0x0056a794', [0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x8888]);
    __freeze.save(ptr('0x0056c570'), 0x244);
    zero(ptr('0x0056c570'), 0x244);
    LABELS.forEach(function (s, k) { ptr('0x0056c570').add(0x40 * k).writeByteArray(bytesOf(s)); });
    o.pos(fx.vis).forEach(function (p, k) {
      ptr('0x0056c770').add(4 * k).writeS32(p[0]);
      ptr('0x0056c794').add(4 * k).writeS32(p[1]);
    });
    return fx;
  }
  F.c3dv_lab_a = function () { return buildLabels({ cnt: [3, 0, 2, 1, 0, 5, 1, 1], pos: POSA }); };
  F.c3dv_lab_b = function () { return buildLabels({ cnt: [3, 0, 2, 1, 0, 5, 1, 1], pos: POSB }); };
  F.c3dv_lab_c = function () { return buildLabels({ cnt: [3, 0, 2, 1, 0, 5, 1, 0], pos: POSA }); };
  F.c3dv_lab_nofont = function () { return buildLabels({ cnt: [1, 0, 0, 1, 0, 1, 1, 1], pos: POSB, nofont: true }); };
  F.c3dv_lab_none = function () { return buildLabels({ cnt: [0, 0, 0, 0, 0, 0, 0, 0], pos: POSA }); };
  F.c3dv_lab_neg = function () { return buildLabels({ cnt: [1, 1, 1, 1, 1, 1, 1, 1], pos: POSV }); };

  function buildMoney(o) {
    const fx = labEnv(o);
    const pos = o.pos(fx.vis);
    setWords('0x00542f00', o.cnt);
    setWords('0x00542dd8', o.amt);
    setWords('0x00542fd8', pos.map(function (p) { return p[0]; }));
    setWords('0x00542ff8', pos.map(function (p) { return p[1]; }));
    __freeze.save(ptr('0x0059e7b8'), 4);
    ptr('0x0059e7b8').writeU32(o.flag >>> 0);
    return fx;
  }
  const AMT = [5, -3, 0, 12, -1, 7, 100, -250];
  const MPOSA = function (V) { return [V[0], V[1], FAR, V[2], V[3], V[0], V[1], FAR]; };
  const MPOSB = function (V) { return [V[0], V[1], FAR, V[2], V[3], V[0], V[1], V[4]]; };
  F.c3dv_mon_a = function () { return buildMoney({ cnt: [2, 0, 1, 3, 1, 0, 2, 1], amt: AMT, pos: MPOSA, flag: 0 }); };
  F.c3dv_mon_b = function () { return buildMoney({ cnt: [2, 0, 1, 3, 1, 0, 2, 1], amt: AMT, pos: MPOSB, flag: 0 }); };
  F.c3dv_mon_c = function () { return buildMoney({ cnt: [2, 0, 1, 3, 1, 0, 2, 0], amt: AMT, pos: MPOSA, flag: 0 }); };
  F.c3dv_mon_flag = function () { return buildMoney({ cnt: [2, 0, 1, 3, 1, 0, 2, 1], amt: AMT, pos: MPOSA, flag: 0x1000000 }); };
  F.c3dv_mon_flag2 = function () { return buildMoney({ cnt: [2, 0, 1, 3, 1, 0, 2, 1], amt: AMT, pos: MPOSA, flag: 0xfeffffff }); };
  F.c3dv_mon_nofont = function () { return buildMoney({ cnt: [1, 1, 1, 1, 1, 1, 1, 1], amt: AMT, pos: POSV, flag: 0, nofont: true }); };
  F.c3dv_mon_pos = function () { return buildMoney({ cnt: [1, 1, 1, 1, 1, 1, 1, 1], amt: [1, 2, 3, 4, 5, 6, 7, 8], pos: POSV, flag: 0 }); };
  F.c3dv_mon_nonpos = function () { return buildMoney({ cnt: [1, 1, 1, 1, 1, 1, 1, 1], amt: [0, -1, -2, -3, 0, -100, -5, 0], pos: POSV, flag: 0 }); };

  // ---- window refresh family: Window::moveTo, redrawWindow, redrawRegion, Window::refreshFull ----------------------------
  // Six windows (0x300 bytes each, $wins). Fields read by the chain (from the instructions of the functions and of the
  // original callees relayoutClip 0x0047e140, Window::refreshRect 0x00480c20, redrawDrawList 0x0047f8e0, View::toParent
  // 0x0047b170, Window::visible 0x004801f0): +0x9c flags, +0xa0 flags (bit 0 visible, bit 1 gets slot +0x13c), +0xb0 owner,
  // +0x130 parent, +0x1ac / +0x1bc RECTs, +0x1dc clip RECT, +0x224 / +0x22c child array and count, +0x278 the surface handle
  // whose slot +0xd4 returns the window's size RECT. Every window shares one vtable: slot +0x11c returns 1, slot +0x13c
  // (RECT* or NULL, int) and slot +0x138 (RECT* or NULL) record into $rec and pop their arguments.
  const RWN = 6, WSIZE = 0x300;
  //        f9c     fa0 owner parent A (+0x1ac)           B (+0x1bc)          children  size RECT
  const RWSPEC = [
    [0x0000, 3, 0, -1, [10, 20, 310, 220], [5, 6, 305, 206], [1, 2, 4], [3, 5, 303, 205]],
    [0x0020, 1, 0, 0, [20, 30, 120, 130], [3, 4, 103, 104], [5], [1, 2, 101, 82]],
    [0x0000, 0, 2, 0, [40, 50, 140, 150], [1, 2, 101, 102], [3], [0, 0, 60, 60]],
    [0x0000, 1, 3, 2, [60, 70, 160, 170], [7, 8, 107, 108], [], [2, 1, 72, 51]],
    [0x8020, 3, 1, 0, [80, 90, 180, 190], [9, 1, 109, 101], [], [4, 6, 54, 46]],
    [0x8020, 1, 5, 1, [11, 12, 51, 62], [2, 3, 52, 63], [], [0, 0, 20, 10]],
  ];
  // o.list: window indices of the draw list 0x00839ac0 (count at 0x0083ab90); o.focus: window index for [0x0083aae0] or -1;
  // The block 0x0083aaa0..0x0083aad0 (cursor image pointer, tooltip callback 0x0083aabc, tooltip text 0x0083aac4) is zeroed.
  function buildRW(o) {
    const emit = codeArena();
    const rec = newRec(0x4000);
    const fx = { rec: rec };
    const nul = emit([0x33, 0xC0, 0xC3]);
    const vt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
    vt.add(0x11c).writePointer(emit([0xB8, 1, 0, 0, 0, 0xC3]));
    vt.add(0x13c).writePointer(emit(stubK(rec, 0x13C0013C, 'nv')));
    vt.add(0x138).writePointer(emit(stubK(rec, 0x13800138, 'n')));
    const surfVt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) surfVt.add(i * 4).writePointer(nul);
    surfVt.add(0x40).writePointer(emit(stubK(rec, 0x4ac30040, 'vrr')));
    const mkSurfH = function (id) { const h = alloc(0x20); h.writePointer(surfVt); h.add(4).writeU32(0x48000000 + id); return h; };
    const wins = alloc(RWN * WSIZE);
    fx.wins = wins;
    for (let k = 0; k < RWN; k++) {
      const sp = RWSPEC[k];
      const w = wins.add(k * WSIZE);
      w.writePointer(vt);
      w.add(4).writeU32(0x57490000 + k);
      w.add(0x9c).writeU32(sp[0]); w.add(0xa0).writeU32(sp[1]);
      fx['w' + k] = w;
    }
    const mkRectH = function (r) {
      const rr = alloc(0x10);
      r.forEach(function (v, i) { rr.add(4 * i).writeS32(v); });
      const hv = alloc(0x400);
      for (let i = 0; i < 0x100; i++) hv.add(i * 4).writePointer(nul);
      hv.add(0xd4).writePointer(emit([0xB8, ...u32(rr.toUInt32()), 0xC3]));
      const h = alloc(0x20);
      h.writePointer(hv);
      return h;
    };
    for (let k = 0; k < RWN; k++) {
      const sp = RWSPEC[k];
      const w = fx['w' + k];
      w.add(0xb0).writePointer(fx['w' + sp[2]]);
      if (sp[3] >= 0) w.add(0x130).writePointer(fx['w' + sp[3]]);
      sp[4].forEach(function (v, i) { w.add(0x1ac + 4 * i).writeS32(v); });
      sp[5].forEach(function (v, i) { w.add(0x1bc + 4 * i).writeS32(v); });
      for (let j = 0; j < 4; j++) w.add(0x1dc + 4 * j).writeS32(0x7000 + j);
      const arr = alloc(0x40);
      sp[6].forEach(function (c, i) { arr.add(4 * i).writePointer(fx['w' + c]); });
      w.add(0x224).writePointer(arr);
      w.add(0x22c).writeS32(sp[6].length);
      w.add(0x278).writePointer(mkRectH(sp[7]));
    }
    const rect = function (l, t, r, b) { const p = alloc(0x10); [l, t, r, b].forEach(function (v, i) { p.add(4 * i).writeS32(v); }); return p; };
    fx.r0 = rect(2, 3, 40, 30); fx.r1 = rect(-5, -6, 10, 12); fx.r2 = rect(0, 0, 0, 0); fx.r3 = rect(100, 100, 400, 300);
    // the object at [0x0083a7bc] (slot +0xd4 returns the RECT {1, 2, 300, 200}, relayoutClip's clip source)
    const rr2 = alloc(0x10);
    [1, 2, 300, 200].forEach(function (v, i) { rr2.add(4 * i).writeS32(v); });
    const rvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) rvt.add(i * 4).writePointer(nul);
    rvt.add(0xd4).writePointer(emit([0xB8, ...u32(rr2.toUInt32()), 0xC3]));
    const robj = alloc(0x20);
    robj.writePointer(rvt);
    fx.robj = robj;
    fx.obj = wins;
    freezeNow();
    __freeze.save(ptr('0x0083a7bc'), 4);
    ptr('0x0083a7bc').writePointer(robj);
    __freeze.save(ptr('0x00839ac0'), 0x80);
    const list = o.list || [];
    for (let i = 0; i < 0x80; i += 4) ptr('0x00839ac0').add(i).writeU32(0xA5A50000 + i);
    list.forEach(function (c, i) { ptr('0x00839ac0').add(4 * i).writePointer(fx['w' + c]); });
    __freeze.save(ptr('0x0083ab90'), 4);
    ptr('0x0083ab90').writeS32(list.length);
    __freeze.save(ptr('0x0083aae0'), 4);
    ptr('0x0083aae0').writePointer(o.focus === undefined || o.focus < 0 ? ptr(0) : fx['w' + o.focus]);
    __freeze.save(ptr('0x0083aaa0'), 0x30);
    zero(ptr('0x0083aaa0'), 0x30);
    // drawTooltip 0x0047cdb0 ends with a call of the live display's slot +0xa4 (RECT*) at 0x0047cfb9 (0x0083ad50): swap in a
    // recording display. [0x0083aaa0] (a pointer to the cursor image) stays 0, so the cursor blits and the tooltip body are skipped.
    const dvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) dvt.add(i * 4).writePointer(nul);
    dvt.add(0xa4).writePointer(emit(stubK(rec, 0xD15A00A4, 'n')));
    const disp = alloc(0x40);
    disp.writePointer(dvt);
    __freeze.setU32(ptr('0x0083ad50'), disp.toUInt32());
    return fx;
  }
  F.c3dv_rw_a = function () { return buildRW({ list: [0, 1, 2, 4] }); };
  F.c3dv_rw_focus = function () { return buildRW({ list: [0, 1, 2, 4], focus: 0 }); };
  F.c3dv_rw_focus4 = function () { return buildRW({ list: [0, 1, 2, 4], focus: 4 }); };
  F.c3dv_rw_empty = function () { return buildRW({ list: [] }); };

  // ---- listSetItems 0x00494020 ------------------------------------------------------------------------------------------
  // The list object (0x3600 bytes): +0x23b4 the surface handle (slots +0xd8 / +0xdc return W / H) or 0, +0x26c4 the sub-window
  // (a Window whose vtable slot +0x120 records; flags byte +0xa0, no children, parent none), +0x3474 receives the three item
  // pointers. Window::moveTo runs ORIGINAL (c3dv hook in the main build) and relayoutClip 0x0047e140 reads [0x0083a7bc].
  function buildLS(o) {
    const emit = codeArena();
    const rec = newRec(0x2000);
    const fx = { rec: rec };
    const nul = emit([0x33, 0xC0, 0xC3]);
    const wvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) wvt.add(i * 4).writePointer(nul);
    wvt.add(0x120).writePointer(emit(stubK(rec, 0x15700120, '')));
    const obj = alloc(0x3600);
    fx.obj = obj;
    if (o.W !== undefined) {
      const hv = alloc(0x400);
      for (let i = 0; i < 0x100; i++) hv.add(i * 4).writePointer(nul);
      hv.add(0xd8).writePointer(emit([0xB8, ...u32(o.W >>> 0), 0xC3]));
      hv.add(0xdc).writePointer(emit([0xB8, ...u32(o.H >>> 0), 0xC3]));
      const h = alloc(0x20);
      h.writePointer(hv);
      obj.add(0x23b4).writePointer(h);
    }
    const win = obj.add(0x26c4);
    win.writePointer(wvt);
    win.add(0xa0).writeU32(o.flags === undefined ? 1 : o.flags);
    [10, 20, 110, 90].forEach(function (v, i) { win.add(0x1ac + 4 * i).writeS32(v); });
    [5, 6, 105, 86].forEach(function (v, i) { win.add(0x1bc + 4 * i).writeS32(v); });
    const arr = alloc(0x10);
    win.add(0x224).writePointer(arr);
    win.add(0x22c).writeS32(0);
    const img = function (w, h) { const p = alloc(0x40); p.add(0x18).writeS32(w); p.add(0x1c).writeS32(h); return p; };
    const triple = function (a, b, c) { const t = alloc(0x10); t.writePointer(a); t.add(4).writePointer(b); t.add(8).writePointer(c); return t; };
    fx.i0 = triple(img(30, 20), img(11, 12), img(13, 14));
    fx.i1 = triple(img(7, 33), img(21, 22), img(23, 24));
    fx.i2 = triple(img(0, 0), img(1, 2), img(3, 4));
    fx.i3 = triple(img(64, 101), img(5, 6), img(7, 8));
    fx.i4 = triple(img(-6, -15), img(5, 6), img(7, 8));
    const rr2 = alloc(0x10);
    [1, 2, 300, 200].forEach(function (v, i) { rr2.add(4 * i).writeS32(v); });
    const rvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) rvt.add(i * 4).writePointer(nul);
    rvt.add(0xd4).writePointer(emit([0xB8, ...u32(rr2.toUInt32()), 0xC3]));
    const robj = alloc(0x20);
    robj.writePointer(rvt);
    freezeNow();
    __freeze.save(ptr('0x0083a7bc'), 4);
    ptr('0x0083a7bc').writePointer(robj);
    return fx;
  }
  F.c3dv_ls_a = function () { return buildLS({ W: 200, H: 100 }); };
  F.c3dv_ls_odd = function () { return buildLS({ W: 201, H: 101 }); };
  F.c3dv_ls_noh = function () { return buildLS({}); };
  F.c3dv_ls_hidden = function () { return buildLS({ W: 200, H: 100, flags: 2 }); };

  // ---- Button::endPress 0x00488eb0 ----------------------------------------------------------------------------------------
  // cdecl one-argument recorder (appends {tag, 0, arg}, returns 7, pops nothing): the callback at button+0x628
  function cdeclStub1(rec, tag) {
    const r = u32(rec.toUInt32());
    const b = [0x53];
    b.push(0x8B, 0x1D, ...r, 0x81, 0xC3, ...r);                        // ebx = REC + [REC]
    b.push(0xC7, 0x03, ...u32(tag), 0xC7, 0x43, 0x04, 0, 0, 0, 0);
    b.push(0x8B, 0x44, 0x24, 0x08, 0x89, 0x43, 0x08);                  // [ebx+8] = arg
    b.push(0x81, 0x05, ...r, ...u32(12));
    b.push(0xB8, ...u32(7), 0x5B, 0xC3);
    return b;
  }
  // Six buttons (0x700 bytes): +0x5e8 id, +0x614 mode, +0x620 radio group, +0x624 skip flag, +0x628 callback, +0x130 parent.
  // The parent (0x200 bytes) has slot +0xd4(id) recording and, at +0x80, an object with slot +0x1c recording (parent A) or 0
  // (parent B). The group is a zeroed 0x200-byte block with +0x88 = 0x1234 (radioSelect 0x004a0350 runs ORIGINAL on it, count
  // +0x84 = 0).
  function buildBT(o) {
    const emit = codeArena();
    const rec = newRec(0x2000);
    const fx = { rec: rec };
    const nul = emit([0x33, 0xC0, 0xC3]);
    const mkvt = function (slots) {
      const vt = alloc(0x400);
      for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
      slots.forEach(function (s) { vt.add(s[0]).writePointer(emit(stubK(rec, s[1], s[2]))); });
      return vt;
    };
    const bvt = mkvt([[0x10, 0xB7100010, ''], [0x120, 0xB7100120, '']]);
    const pvt = mkvt([[0xd4, 0xB71000D4, 'v']]);
    const cvt = mkvt([[0x1c, 0xB710001C, '']]);
    const child = alloc(0x40); child.writePointer(cvt);
    const parentA = alloc(0x200); parentA.writePointer(pvt); parentA.add(0x80).writePointer(child);
    const parentB = alloc(0x200); parentB.writePointer(pvt);
    const grp = alloc(0x200); grp.add(0x88).writeU32(0x1234);
    fx.grp = grp;
    const cb = emit(cdeclStub1(rec, 0xCB620628));
    const objs = alloc(6 * 0x700);
    fx.objs = objs;
    const SP = [
      // id, f624, group, cb, f614, parent
      [5, 0, grp, cb, 1, parentA], [-1, 0, null, null, 0, parentA], [-2, 0, null, cb, 1, parentB],
      [7, 1, grp, cb, 0, parentA], [-1, 0, grp, null, 2, parentA], [9, 0, null, null, 1, parentB],
    ];
    SP.forEach(function (s, k) {
      const b = objs.add(k * 0x700);
      b.writePointer(bvt);
      b.add(4).writeU32(0xB7000000 + k);
      b.add(0x5e8).writeS32(s[0]); b.add(0x624).writeU32(s[1]);
      if (s[2]) b.add(0x620).writePointer(s[2]);
      if (s[3]) b.add(0x628).writePointer(s[3]);
      b.add(0x614).writeU32(s[4]);
      b.add(0x130).writePointer(s[5]);
      fx['b' + k] = b;
    });
    fx.obj = objs;
    const gcb = emit(stubK(rec, 0x6C0B000C, '', undefined, true));
    const gcb2 = emit(stubK(rec, 0x6C0B0010, '', undefined, true));
    freezeNow();
    __freeze.save(ptr('0x0083ab2c'), 12);
    ptr('0x0083ab2c').writeU32(0x2c2c2c2c);
    ptr('0x0083ab30').writePointer(fx[o.cur30]);
    ptr('0x0083ab34').writePointer(fx[o.cur34]);
    __freeze.save(ptr('0x0083ab0c'), 8);
    if (o.cbs) { ptr('0x0083ab0c').writePointer(gcb); ptr('0x0083ab10').writePointer(gcb2); }
    else { ptr('0x0083ab0c').writeU32(0); ptr('0x0083ab10').writeU32(0); }
    return fx;
  }
  F.c3dv_bt_a = function () { return buildBT({ cur30: 'b0', cur34: 'b1', cbs: true }); };
  F.c3dv_bt_b = function () { return buildBT({ cur30: 'b3', cur34: 'b3', cbs: true }); };
  F.c3dv_bt_nocb = function () { return buildBT({ cur30: 'b2', cur34: 'b5', cbs: false }); };
  F.c3dv_bt_none = function () { return buildBT({ cur30: 'b4', cur34: 'b4', cbs: true }); };

  // ---- panel layout builders: panelBuildText 0x0049d770, optionDialogDraw 0x0049f050, panelBuildText4a3be0 0x004a3be0 -------
  // One fixture per function builds twelve objects (E = 0x200 bytes, the `this`; BK = 0x200 bytes, the base record b; V = a
  // 0x10-byte vtable whose dwords [+4] / [+8] are the offsets from E to the text object and to b). All objects share the
  // text object tm (its fonts measure with the fixture measurer). b fields are written from the spec's `b` map (offset ->
  // value); lists are circular node rings (+8 item, +0xc next). Divisors (b+0x50, this+0x74, b+0x6c...) are never 0 in a spec.
  function mkStr(s) { const p = alloc((s.length + 4 + 3) & ~3); if (s.length) p.writeByteArray(bytesOf(s)); return p; }
  function mkImg(w, h) { const p = alloc(0x40); p.add(0x18).writeS32(w); p.add(0x1c).writeS32(h); return p; }
  function mkRing(its) {
    const nodes = its.map(function (it) { const n = alloc(0x20); n.add(8).writePointer(it); return n; });
    nodes.forEach(function (n, i) { n.add(0xc).writePointer(nodes[(i + 1) % nodes.length]); });
    return { head: nodes.length ? nodes[0] : ptr(0), cursor: nodes.length ? nodes[nodes.length - 1] : ptr(0), count: nodes.length };
  }
  // the item records come from one arena ($IT) so that the rows count panelBuildText stores at item+8 is part of the state
  let ITA = null, ITO = 0;
  function takeItem() { const p = ITA.add(ITO); ITO += 0x20; if (ITO > 0x2000) throw new Error('c3dv: item arena full'); return p; }
  function textItems(strs) { return strs.map(function (s) { const it = takeItem(); it.add(4).writePointer(mkStr(s)); return it; }); }
  function imgItems(dims) { return dims.map(function (d) { const it = takeItem(); it.add(4).writePointer(mkImg(d[0], d[1])); return it; }); }
  const NP = 16;
  function buildPanelSet(kind, specs) {
    const fx = buildText({});
    const E = alloc(NP * 0x200), BK = alloc(NP * 0x200), V = alloc(NP * 0x10);
    ITA = alloc(0x2000); ITO = 0;
    fx.E = E; fx.BK = BK; fx.V = V; fx.IT = ITA; fx.obj = E;
    const surfA = fx.f2;                     // a real font record: setDrawTarget stores it as the text object's base font
    const surfB = alloc(0x40);
    specs.forEach(function (sp, k) {
      const e = E.add(k * 0x200), b = BK.add(k * 0x200), v = V.add(k * 0x10);
      v.add(4).writeS32(fx.tm.sub(e).sub(0x274).toInt32());
      v.add(8).writeS32(b.sub(e).toInt32());
      e.writePointer(v);
      fx['o' + k] = e;
      const d = sp.b || {};
      Object.keys(d).forEach(function (key) { b.add(parseInt(key, 10)).writeS32(d[key]); });
      if (kind !== 'pbt') {
        // setDrawTarget's surface b+0x74: 'A' is a font record (non-zero +4: stored), 'B' has a zero +4, 'N' is NULL (refused)
        const s = sp.surf === 'A' ? surfA : (sp.surf === 'B' ? surfB : ptr(0));
        b.add(0x74).writePointer(s);
      }
      const ring = mkRing(textItems(sp.items || []));
      b.add(0xc8).writePointer(ring.head); b.add(0xcc).writePointer(ring.cursor);
      b.add(0xd0).writeS32(sp.count !== undefined ? sp.count : ring.count);
      const img = function (dm) { return dm ? mkImg(dm[0], dm[1]) : ptr(0); };
      if (kind === 'pbt') {
        (sp.imgs || []).forEach(function (dm, i) { e.add(4 + 4 * i).writePointer(img(dm)); });
      } else if (kind === 'odd') {
        (sp.imgs || []).forEach(function (dm, i) { e.add(0x10 + 4 * i).writePointer(img(dm)); });
      } else {
        const r2 = mkRing(imgItems(sp.imgl || []));
        e.add(0x70).writeS32(sp.num); e.add(0x74).writeS32(sp.den);
        e.add(0x80).writePointer(r2.head); e.add(0x84).writePointer(r2.cursor); e.add(0x88).writeS32(sp.n1 !== undefined ? sp.n1 : r2.count);
      }
    });
    return fx;
  }
  // b maps: 0x24 flags, 0x30, 0x34, 0x38, 0x44, 0x48, 0x4c, 0x50, 0x54 as read by the three functions
  function Bm(f24, f30, f34, f38, f44, f48, f4c, f50, f54) {
    return { 0x24: f24, 0x30: f30, 0x34: f34, 0x38: f38, 0x44: f44, 0x48: f48, 0x4c: f4c, 0x50: f50, 0x54: f54,
             0x80: 11, 0x84: 12, 0x88: 13, 0x8c: 21, 0x90: 22, 0x94: 23, 0x98: 31, 0x9c: 32, 0xa0: 33, 0xa4: 41, 0xa8: 42, 0xac: 43,
             0x78: 5, 0x7c: 6 };
  }
  const T5 = ['alpha', 'bb', 'a longer entry text', 'x', 'mid entry'];
  const PBT = [
    { b: Bm(3, 200, 150, 0, 2, 3, -1, 1, 3), imgs: [[30, 20], [26, 25], [40, 18]], items: T5 },
    { b: Bm(0, 0, 0, 7, 0, 0, 12, 1, 10), imgs: [null, [1, 1], [2, 2]], items: ['one', 'two'] },
    { b: Bm(1, 0, 100, 0, 1, 2, -1, 2, 2), imgs: [[10, 10], [20, 12], [35, 9]], items: ['p', 'qq', 'rrr', 'ssss'] },
    { b: Bm(2, 90, 0, 0, 0, 0, -1, 2, 4), imgs: [[50, 5], [10, 4], [20, 3]], items: ['aa', 'bb', 'cc'] },
    { b: Bm(0, 0, 0, 0, 0, 0, -1, 1, 3), imgs: [[1, 1], [1, 1], [1, 1]], items: [] },
    { b: Bm(3, 200, 150, 0, 2, 3, -1, 1, 1), imgs: [[30, 20], [26, 25], [40, 18]], items: T5 },
    { b: Bm(3, 0, 0, 0, 1, 1, 5, 1, 3), imgs: [[1, 1], [2, 2], [3, 3]], items: ['a', 'b', 'c', 'd', 'e', 'f'] },
    { b: Bm(3, 500, 300, 0, 0, 0, -1, 3, 2), imgs: [[10, 30], [12, 4], [8, 6]], items: ['1', '22', '333', '4444', '55555', '666666', '7777777'] },
    { b: Bm(0, 0, 0, 0, 2, 2, -1, 1, 5), imgs: [[5, 5], [60, 70], [7, 8]], items: ['u', 'v', 'w'] },
    { b: Bm(0, 0, 0, 0, 0, 0, -1, 1, 8), imgs: [[5, 5], [6, 6], null], items: ['k', 'l'] },
    { b: Bm(2, 77, 0, 0, 3, 1, 3, 2, 4), imgs: [[9, 9], [9, 9], [9, 9]], items: ['m', 'n', 'o', 'p', 'q', 'r'] },
    { b: Bm(1, 0, 33, 4, 0, 5, 8, 2, 3), imgs: [[1, 1], [1, 1], [1, 1]], items: ['s', 't', 'u', 'v'] },
    { b: Bm(0, 0, 0, 0, 1, 1, -1, 1, 4), imgs: [null, [5, 5], [6, 6]], items: ['k1', 'k2'] },
    { b: Bm(0, 0, 0, 0, 1, 1, -1, 1, 4), imgs: [[5, 5], null, [6, 6]], items: ['k3', 'k4'] },
    { b: Bm(0, 0, 0, 0, 1, 1, -1, 1, 4), imgs: [[10, 10], [20, 20], [30, 40]], items: ['k5', 'k6', 'k7'] },
    { b: Bm(0, 0, 0, 0, 1, 1, -1, 2, 4), imgs: [[10, 10], [20, 20], [30, 40]], items: ['k8'], count: -2 },
  ];
  F.c3dv_pb_pbt = function () { return buildPanelSet('pbt', PBT); };
  const ODD = [
    { b: Bm(3, 200, 150, 0, 2, 3, -1, 1, 0), imgs: [[30, 20], [26, 25]], items: ['aa', 'b', 'cccc', 'dd'], surf: 'A' },
    { b: Bm(0, 0, 0, 9, 0, 0, 7, 2, 0), imgs: [null, [1, 1]], items: ['one', 'two'], surf: 'N' },
    { b: Bm(1, 0, 80, 0, 0, 0, -1, 1, 0), imgs: [[10, 10], [22, 30]], items: ['x', 'yy', 'zzz'], surf: 'B' },
    { b: Bm(2, 120, 0, 0, 1, 0, -1, 3, 0), imgs: [[44, 3], [12, 4]], items: ['only'], surf: 'A' },
    { b: Bm(0, 0, 0, 0, 0, 0, -1, 1, 0), imgs: [[1, 1], [1, 1]], items: [], surf: 'A' },
    { b: Bm(3, 90, 60, 0, 4, 1, -1, 2, 0), imgs: [[1, 1], [2, 2]], items: ['a', 'bb', 'ccc', 'dddd', 'eeeee'], surf: 'A' },
    { b: Bm(0, 0, 0, 0, 0, 0, -1, 1, 0), imgs: [[3, 3], null], items: ['q', 'r'], surf: 'A' },
    { b: Bm(0, 1000, 0, 5, 1, 1, 4, 1, 0), imgs: [[2, 2], [2, 2]], items: ['s', 't'], surf: 'B' },
    { b: Bm(0, 0, 0, 5, 1, 1, 4, 1, 0), imgs: [[2, 2], [2, 2]], items: ['s', 't'], surf: 'A' },
    { b: Bm(3, 250, 0, 0, 0, 2, -1, 2, 0), imgs: [[20, 50], [60, 8]], items: ['aaa', 'bbbbbb'], surf: 'N' },
    { b: Bm(1, 0, 17, 3, 2, 2, 90, 4, 0), imgs: [[8, 8], [9, 9]], items: ['i', 'j', 'k'], surf: 'A' },
    { b: Bm(2, 31, 0, 3, 0, 0, 2, 5, 0), imgs: [null, null], items: ['l'], surf: 'B' },
    { b: Bm(0, 0, 0, 0, 1, 1, -1, 1, 0), imgs: [null, [5, 5]], items: ['m1', 'm2'], surf: 'A' },
    { b: Bm(0, 0, 0, 0, 1, 1, -1, 2, 0), imgs: [[7, 7], [8, 8]], items: ['n'], surf: 'B', count: -3 },
    { b: Bm(0, 0, 0, 0, 1, 1, -1, 1, 0), imgs: [[3, 3], [4, 4]], items: [], surf: 'N', count: 3 },
  ];
  F.c3dv_pb_odd = function () { return buildPanelSet('odd', ODD); };
  const PB2 = [
    { b: Bm(3, 200, 150, 0, 2, 3, 0, 2, 2), num: 3, den: 2, imgl: [[20, 10], [50, 30], [8, 40]], items: T5, surf: 'A' },
    { b: Bm(0, 0, 0, 0, 0, 0, 0, 1, 10), num: 1, den: 1, imgl: [[5, 5]], items: ['solo'], surf: 'B' },
    { b: Bm(1, 0, 100, 0, 1, 2, 0, 1, 3), num: 2, den: 1, imgl: [[7, 7], [9, 9]], items: ['p', 'qq'], surf: 'N' },
    { b: Bm(2, 90, 0, 0, 0, 0, 0, 1, 3), num: 1, den: 3, imgl: [[30, 60], [12, 6], [5, 5], [44, 22]], items: T5.slice(0, 4), surf: 'A' },
    { b: Bm(0, 0, 0, 0, 0, 0, 0, 1, 3), num: 1, den: 1, imgl: [], items: ['z'], surf: 'A' },
    { b: Bm(0, 0, 0, 0, 3, 1, 0, 1, 1), num: 5, den: 3, imgl: [[7, 9], [13, 11]], items: ['m', 'nn'], surf: 'A' },
    { b: Bm(3, 0, 0, 0, 0, 0, 0, 1, 4), num: 1, den: 1, imgl: [[6, 6], [6, 6], [6, 6]], items: ['a', 'b', 'c'], surf: 'B' },
    { b: Bm(0, 900, 0, 0, 1, 2, 0, 1, 2), num: 1, den: 1, imgl: [[20, 30], [21, 31], [22, 32]], items: ['u', 'vv', 'www'], surf: 'A' },
    { b: Bm(0, 0, 0, 0, 5, 1, 0, 1, 7), num: 2, den: 5, imgl: [[100, 100], [3, 3]], items: ['aaaaaaaaaa', 'b'], surf: 'A' },
    { b: Bm(1, 0, 40, 0, 0, 0, 0, 1, 2), num: 1, den: 1, imgl: [[10, 3], [10, 3], [10, 3], [10, 3], [10, 3]], items: ['1', '2', '3', '4', '5'], surf: 'N' },
    { b: Bm(2, 33, 0, 0, 2, 2, 0, 1, 5), num: -3, den: 2, imgl: [[10, 20], [-4, 6]], items: ['neg', 'tive'], surf: 'B' },
    { b: Bm(3, 70, 80, 0, 0, 1, 0, 1, 3), num: 1, den: 1, imgl: [[9, 99], [9, 98], [9, 97]], items: ['c', 'd', 'e', 'f'], surf: 'A' },
    { b: Bm(0, 0, 0, 0, 1, 1, 0, 1, 2), num: 1, den: 1, imgl: [[5, 5], [6, 6]], items: ['g', 'h'], surf: 'B', n1: -2 },
  ];
  F.c3dv_pb_pb2 = function () { return buildPanelSet('pb2', PB2); };

  // ---- ListBox::onScroll 0x0048c420: c3ct's sixteen ListBoxes (fixture c3ct_list; c3ct.cpp is linked through c3dv_batches.txt) ----
  // [0x0083ff18] (the key mode written by scrollbarScrollBy / scrollbarOnKey) is set per key: 0 skips ListBox::setSelection, 1 runs it.
  function listFixtureKey(mode) {
    const fx = globalThis.DIFF_FIXTURES.c3ct_list();
    negOn();                                  // the negative-control build keeps its wrong details dormant until a fixture sets the flag
    __freeze.setU32(ptr('0x0083ff18'), mode);
    return fx;
  }
  F.c3dv_os_a = function () { return listFixtureKey(0); };
  F.c3dv_os_key = function () { return listFixtureKey(1); };

  // ---- comboLayoutItems 0x00493f50 -------------------------------------------------------------------------------------------
  // Combo objects (0x3600 bytes each, one block $cbs): +0x23b0 the drawing Surface record whose handle (+0x23b4; slots +0xd8
  // width, +0xc8 clip RECT out, +0x34 clip RECT in) is ours, +0x3468 / +0x346c / +0x3470 the left / middle / right image records
  // (+4 handle with slot +0x44 recording, +0x18 / +0x1c size, +0x20 / +0x24 hot spot). The display (slots +0x9c store a triple,
  // +0xa0 reads it back) is ours. Image_draw 0x00473e60 and blitRight 0x00476140 run ORIGINAL (hooked in the main build).
  function rectFillCL(l, t, r, b) {
    return [0x8B, 0x44, 0x24, 0x08, 0xC7, 0x00, ...u32(l), 0xC7, 0x40, 0x04, ...u32(t), 0xC7, 0x40, 0x08, ...u32(r),
            0xC7, 0x40, 0x0C, ...u32(b)];
  }
  const CL = [
    // W    left [w,h]   mid [w,h]   right [w,h]  hot spots (left, mid, right)
    { W: 100, L: [8, 6], M: [5, 7], R: [9, 6] },
    { W: 17, L: [8, 6], M: [5, 7], R: [9, 6] },          // 17 = 8 + 9: the middle is skipped (not wider)
    { W: 18, L: [8, 6], M: [5, 7], R: [9, 6] },          // one pixel wider: the middle is drawn
    { W: 40, L: [10, 12], M: [0, 4], R: [6, 3] },       // a zero-width middle record
    { W: 64, L: [4, 4], M: [16, 8], R: [4, 4], hs: [[1, 2], [0, 0], [-3, 4]] },
    { W: 0, L: [8, 6], M: [5, 7], R: [9, 6], noh: true },  // no handle: width 0, Image_draw refuses (returns 7)
  ];
  function buildCL() {
    const emit = codeArena();
    const rec = newRec(0x8000);
    const fx = { rec: rec };
    const nul = emit([0x33, 0xC0, 0xC3]);
    const st = {};
    st[0x9c] = [0, 1, 2].reduce(function (a, j) { return a.concat([0x8B, 0x44, 0x24, 8 + 4 * j, 0x89, 0x41, 0x10 + 4 * j]); }, []);
    st[0xa0] = [0, 1, 2].reduce(function (a, j) { return a.concat([0x8B, 0x54, 0x24, 8 + 4 * j, 0x8B, 0x41, 0x10 + 4 * j, 0x89, 0x02]); }, []);
    const dargs = {}; dargs[0x9c] = 3; dargs[0xa0] = 3;
    const dskip = {}; dskip[0xa0] = true;
    const disp = makeDisplay(emit, rec, undefined, st, dargs, dskip);
    disp.add(0x10).writeS32(2); disp.add(0x14).writeS32(3); disp.add(0x18).writeS32(2);
    fx.disp = disp;
    const cbs = alloc(CL.length * 0x3600);
    fx.cbs = cbs; fx.obj = cbs;
    const mkImg3 = function (id, dm, hs) {
      const im = alloc(0x30);
      im.writeU32(0x53000000 + id);
      const vt = alloc(0x400);
      for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
      vt.add(0x44).writePointer(emit(stubK(rec, 0x4ac30044 + id * 0x10000, 'vvvv')));
      const h = alloc(0x20); h.writePointer(vt); h.add(4).writeU32(0x48000000 + id);
      im.add(4).writePointer(h);
      im.add(0x18).writeS32(dm[0]); im.add(0x1c).writeS32(dm[1]);
      im.add(0x20).writeS32(hs[0]); im.add(0x24).writeS32(hs[1]);
      return im;
    };
    CL.forEach(function (sp, k) {
      const c = cbs.add(k * 0x3600);
      const hs = sp.hs || [[0, 0], [0, 0], [0, 0]];
      c.add(0x3468).writePointer(mkImg3(k * 3 + 1, sp.L, hs[0]));
      c.add(0x346c).writePointer(mkImg3(k * 3 + 2, sp.M, hs[1]));
      c.add(0x3470).writePointer(mkImg3(k * 3 + 3, sp.R, hs[2]));
      const D = c.add(0x23b0);
      D.writeU32(0x53440000 + k);
      if (!sp.noh) {
        const vt = alloc(0x400);
        for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
        vt.add(0xd8).writePointer(emit([0xB8, ...u32(sp.W >>> 0), 0xC3]));
        vt.add(0xc8).writePointer(emit(stubBytes(rec, 0x4ac300c8, 1, 0, rectFillCL(0, 0, 320, 240), true)));
        vt.add(0x34).writePointer(emit(stubK(rec, 0x4ac30034, 'r')));
        const h = alloc(0x20); h.writePointer(vt); h.add(4).writeU32(0x48440000 + k);
        D.add(4).writePointer(h);
      }
      fx['c' + k] = c;
    });
    freezeNow();
    __freeze.setU32(ptr('0x0083ad50'), disp.toUInt32());
    return fx;
  }
  F.c3dv_cl_a = function () { return buildCL(); };

  // ---- drawMenuList 0x004763d0 --------------------------------------------------------------------------------------------
  // Models live in one arena ($MA): a model is {+8 head, +0xc cursor, +0x10 count, +0x14 index}, a node {+4 id, +8 item, +0xc
  // next}, an item {+4 text, +8 kind}. The text objects are the fixture's (tm has no callback, tmcb the cdecl callback at +0x10,
  // tsmall a handle height of 30). The globals of the text layout (0x00839a9c..0x00839aa8, 0x00839a5c) and the pending line
  // buffer 0x00839658 are saved, as for drawTextBlock.
  const MENU = [
    { k: [0, 0, 0], t: ['one two three', 'four', 'five six'] },
    { k: [3, 0, 2, 0], t: ['t1', 'a b c d e f g h', 'x y z', 'w'] },
    { k: [2, 3, 0, 0, 1], t: ['aa bb', 'cc', 'dd ee ff', 'gg', 'hh ii jj kk'] },
    { k: [], t: [] },
    { k: [0], t: ['solo item here'] },
    { k: [0, 0, 0, 0, 0, 0, 0, 0], t: ['aaa bbb ccc', 'ddd eee', 'fff ggg hhh iii', 'jjj', 'kkk lll', 'mmm nnn', 'ooo', 'ppp qqq'] },
    { k: [1, 0, 3], t: ['first', 'second line', 'third'] },
    { k: [2, 2], t: ['para one', 'para two is longer than one'] },
    { k: [0, 0], t: ['p', 'q'] },
  ];
  function buildMenu() {
    const fx = buildText({ glob: [11, 7, 60, 0] });
    __freeze.save(ptr('0x00839658'), 0x404);
    zero(ptr('0x00839658'), 0x404);
    const MA = alloc(0x4000);
    fx.MA = MA; fx.obj = MA;
    let off = 0;
    const take = function (n) { const p = MA.add(off); off += (n + 0x1f) & ~0x1f; if (off > 0x4000) throw new Error('c3dv: menu arena full'); return p; };
    MENU.forEach(function (sp, m) {
      const model = take(0x20);
      const n = sp.k.length;
      const nodes = [];
      for (let i = 0; i < n; i++) {
        const it = take(0x20);
        it.add(4).writePointer(mkStr(sp.t[i]));
        it.add(8).writeS32(sp.k[i]);
        const nd = take(0x20);
        nd.add(4).writeS32(100 * m + i + 1);
        nd.add(8).writePointer(it);
        nodes.push(nd);
      }
      nodes.forEach(function (nd, i) { nd.add(0xc).writePointer(nodes[(i + 1) % n]); });
      if (n) { model.add(8).writePointer(nodes[0]); model.add(0xc).writePointer(nodes[n - 1]); }
      model.add(0x10).writeS32(n);
      fx['m' + m] = model;
    });
    return fx;
  }
  F.c3dv_mn_a = function () { return buildMenu(); };

  // ---- TextView::drawPanelWithImage 0x00491270 ----------------------------------------------------------------------------------
  // Panel objects (0x1500 bytes, block $pns): +0xa0 = 0 so drawPanel4810d0 (run ORIGINAL) leaves at 0x004810d9..0x004810e0,
  // +0x11c the Surface record (its handle +4 fills a RECT through slot +0xd0), +0x13cc the image (handle slot +0x44 records),
  // +0x194 the margin. The display is ours (slots +0x9c store a triple, +0xa0 reads it back: Image_draw 0x00473e60).
  const DP = [
    { surf: true, img: true, rc: [10, 20, 110, 80], m: 3, hs: [0, 0] },
    { surf: true, img: true, rc: [0, 0, 50, 40], m: 0, hs: [1, 2] },
    { surf: true, img: true, rc: [-5, -6, 10, 12], m: -2, hs: [4, -3] },
    { surf: false, img: true, rc: [1, 2, 3, 4], m: 1, hs: [0, 0] },
    { surf: true, img: false, rc: [1, 2, 3, 4], m: 1, hs: [0, 0] },
    { surf: true, img: true, rc: [300, 200, 400, 300], m: 7, hs: [-2, 5] },
  ];
  function buildDP() {
    const emit = codeArena();
    const rec = newRec(0x4000);
    const fx = { rec: rec };
    const nul = emit([0x33, 0xC0, 0xC3]);
    const st = {};
    st[0x9c] = [0, 1, 2].reduce(function (a, j) { return a.concat([0x8B, 0x44, 0x24, 8 + 4 * j, 0x89, 0x41, 0x10 + 4 * j]); }, []);
    st[0xa0] = [0, 1, 2].reduce(function (a, j) { return a.concat([0x8B, 0x54, 0x24, 8 + 4 * j, 0x8B, 0x41, 0x10 + 4 * j, 0x89, 0x02]); }, []);
    const dargs = {}; dargs[0x9c] = 3; dargs[0xa0] = 3;
    const dskip = {}; dskip[0xa0] = true;
    const disp = makeDisplay(emit, rec, undefined, st, dargs, dskip);
    disp.add(0x10).writeS32(2); disp.add(0x14).writeS32(3); disp.add(0x18).writeS32(2);
    fx.disp = disp;
    const pns = alloc(DP.length * 0x1500);
    fx.pns = pns; fx.obj = pns;
    DP.forEach(function (sp, k) {
      const p = pns.add(k * 0x1500);
      p.add(0xa0).writeU32(0);
      p.add(0x194).writeS32(sp.m);
      if (sp.surf) {
        const vt = alloc(0x400);
        for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
        vt.add(0xd0).writePointer(emit(stubBytes(rec, 0x4ac300d0, 1, 0, rectFillCL(sp.rc[0], sp.rc[1], sp.rc[2], sp.rc[3]), true)));
        const h = alloc(0x20); h.writePointer(vt); h.add(4).writeU32(0x48000000 + k);
        const sf = alloc(0x30); sf.writeU32(0x53000000 + k); sf.add(4).writePointer(h);
        p.add(0x11c).writePointer(sf);
        fx['sf' + k] = sf;
      }
      if (sp.img) {
        const vt = alloc(0x400);
        for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
        vt.add(0x44).writePointer(emit(stubK(rec, 0x4ac30044 + k * 0x10000, 'vvvv')));
        const h = alloc(0x20); h.writePointer(vt); h.add(4).writeU32(0x48100000 + k);
        const im = alloc(0x30); im.writeU32(0x53100000 + k); im.add(4).writePointer(h);
        im.add(0x20).writeS32(sp.hs[0]); im.add(0x24).writeS32(sp.hs[1]);
        p.add(0x13cc).writePointer(im);
      }
      fx['p' + k] = p;
    });
    freezeNow();
    __freeze.setU32(ptr('0x0083ad50'), disp.toUInt32());
    return fx;
  }
  F.c3dv_dp_a = function () { return buildDP(); };

  // ---- panelSub objects: panelDraw4a3f10 0x004a3f10 and the handlers around it ------------------------------------------------
  // One 0x2000-byte block per object ($PD), all of it state. Q = block + 0x100 is panelDraw's `this` (and P = Q + 0xe0 the
  // `this` of panelOnPick / panelOnKey / panelSubDrawContents). Q: +0 vtable (dwords [+4] = 0x700 -> the window base W =
  // block + 0x800, [+8] = 0xb00 -> the record b = block + 0xc00), +4 / +8 flag and colour, +0x70 / +0x74 scale, +0x80 head,
  // +0x84 cursor, +0x88 count, +0x8c index of the item list, +0xa8, +0xb8 callback; the HotList of panelOnPick sits at Q + 0x10
  // (entries pointer +0x50, count +0x58). W: +0x130 parent, +0x274 a copy of the fixture text object whose handle is ours
  // (slot +0x64 drawLine records, +0xdc height). b: +0x28 / +0x2c / +0x48 / +0x5c / +0x68 / +0x6c, the setQuad words +0x80..+0xac
  // and +0x74..+0x7c, the second list +0xc8 head / +0xcc cursor / +0xd0 count / +0xd4 index, +0xf0 the selected index. The
  // parent block has the Surface sub-object at +0x274 whose handle slot +0x40 (src handle, RECT*, RECT*) records.
  const PDN = 11, PDSZ = 0x2000;
  function buildPD(specs, opts) {
    opts = opts || {};
    const fx = buildText({});
    const rec = fx.rec;
    const emit = codeArena();
    const nul = emit([0x33, 0xC0, 0xC3]);
    const st = {};
    st[0x9c] = [0, 1, 2].reduce(function (a, j) { return a.concat([0x8B, 0x44, 0x24, 8 + 4 * j, 0x89, 0x41, 0x10 + 4 * j]); }, []);
    st[0xa0] = [0, 1, 2].reduce(function (a, j) { return a.concat([0x8B, 0x54, 0x24, 8 + 4 * j, 0x8B, 0x41, 0x10 + 4 * j, 0x89, 0x02]); }, []);
    const dargs = {}; dargs[0x9c] = 3; dargs[0xa0] = 3;
    const dskip = {}; dskip[0xa0] = true;
    const disp = makeDisplay(emit, rec, undefined, st, dargs, dskip);
    disp.add(0x10).writeS32(2); disp.add(0x14).writeS32(3); disp.add(0x18).writeS32(2);
    fx.disp = disp;
    const PD = alloc(PDN * PDSZ);
    fx.PD = PD; fx.obj = PD;
    const cbStub = emit(stubK(rec, 0xCB5A0000, 'vvvvv', undefined, true, true));
    const tvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) tvt.add(i * 4).writePointer(nul);
    tvt.add(0x64).writePointer(emit(stubK(rec, 0x4ac30064, 'vvvvvv')));
    tvt.add(0xdc).writePointer(emit([0xB8, ...u32(100), 0xC3]));
    tvt.add(0xd8).writePointer(emit([0xB8, ...u32(80), 0xC3]));
    const thnd = alloc(0x20); thnd.writePointer(tvt); thnd.add(4).writeU32(0x48090000);
    const pvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) pvt.add(i * 4).writePointer(nul);
    pvt.add(0x40).writePointer(emit(stubK(rec, 0x4ac30040, 'vrr')));
    const phnd = alloc(0x20); phnd.writePointer(pvt); phnd.add(4).writeU32(0x48080000);
    const vtWp = alloc(0x400);
    for (let i = 0; i < 0x100; i++) vtWp.add(i * 4).writePointer(nul);
    vtWp.add(0x124).writePointer(emit(stubK(rec, 0xA1240000, '')));
    const valueCb = emit(stubK(rec, 0xCB5B0000, 'v', undefined, true, true));
    const parent = alloc(0x400);
    parent.add(0x274).writeU32(0x53090000); parent.add(0x274 + 4).writePointer(phnd);
    fx.parent = parent;
    specs.forEach(function (sp, k) {
      const blk = PD.add(k * PDSZ);
      const Q = blk.add(0x100), W = blk.add(0x800), b = blk.add(0xc00);
      const vt = alloc(0x10);
      vt.add(4).writeS32(0x700); vt.add(8).writeS32(0xb00);
      Q.writePointer(vt);
      Q.add(4).writeU32(sp.f4); Q.add(8).writeU32(sp.f8);
      Q.add(0x70).writeS32(sp.sc[0]); Q.add(0x74).writeS32(sp.sc[1]);
      Q.add(0xa8).writeS32(sp.a8);
      if (sp.cb) Q.add(0xb8).writePointer(cbStub);
      W.writePointer(vtWp);
      if (sp.par) W.add(0x130).writePointer(parent);
      // panelOnPick: the HotList at Q+0x10 (entries pointer +0x50, count +0x58), the callback at P-0x24 = Q+0xbc, and the vtordisp
      // displacement [T-4] = 0x40 for T = P + 0x40
      Q.add(0x11c).writeS32(0x40);
      const ent = alloc(4 * 0x20);
      [[0, 0, 10, 10], [20, 20, 40, 40], [50, 50, 90, 90], [60, 60, 100, 100]].forEach(function (rc, i) {
        rc.forEach(function (v, j) { ent.add(i * 0x20 + 4 + 4 * j).writeS32(v); });
        ent.add(i * 0x20 + 0x14).writeS32(0x2000 + i);
        ent.add(i * 0x20 + 0x18).writeS32(i < 3 ? 1000 * k + i + 1 : 7777777);
      });
      Q.add(0x10 + 0x50).writePointer(ent); Q.add(0x10 + 0x58).writeS32(4);
      if (sp.pcb) Q.add(0xbc).writePointer(valueCb);
      W.add(0x274).writeByteArray(fx.tm.readByteArray(0x300));
      W.add(0x274 + 4).writePointer(sp.noth ? ptr(0) : thnd);
      const bset = function (off, v) { b.add(off).writeS32(v); };
      bset(0x58, sp.m58 || 0);
      bset(0x28, sp.o28); bset(0x2c, sp.o2c); bset(0x48, sp.g48); bset(0x5c, sp.cols); bset(0x68, sp.w68); bset(0x6c, sp.h6c);
      bset(0xf0, sp.sel);
      [[0x80, 11], [0x84, 12], [0x88, 13], [0x8c, 21], [0x90, 22], [0x94, 23], [0x98, 31], [0x9c, 32], [0xa0, 33], [0xa4, 41],
       [0xa8, 42], [0xac, 43], [0x78, 6], [0x7c, 7]].forEach(function (e) { bset(e[0], e[1] + k); });
      // setDrawTarget's surface b+0x74: sd 1 = a font record (stored as the base font), 2 = a record with +4 == 0, else NULL (refused)
      b.add(0x74).writePointer(sp.sd === 1 ? fx.f2 : (sp.sd === 2 ? alloc(0x40) : ptr(0)));
      // list 1: nodes (+4 id, +8 item, +0xc next, +0x10 prev) over image records
      const n1 = sp.im.length;
      const nodes = sp.im.map(function (d, i) {
        const im = alloc(0x30);
        const vt2 = alloc(0x400);
        for (let j = 0; j < 0x100; j++) vt2.add(j * 4).writePointer(nul);
        vt2.add(0x44).writePointer(emit(stubK(rec, 0x4ac30044 + (k * 16 + i) * 0x10000, 'vvvv')));
        const h = alloc(0x20); h.writePointer(vt2); h.add(4).writeU32(0x48200000 + k * 16 + i);
        im.writeU32(0x53200000 + k * 16 + i); im.add(4).writePointer(h);
        im.add(0x18).writeS32(d[0]); im.add(0x1c).writeS32(d[1]);
        im.add(0x20).writeS32(i - 1); im.add(0x24).writeS32(2 * i);
        const it = alloc(0x20); it.add(4).writePointer(im);
        const nd = alloc(0x20); nd.add(4).writeS32(1000 * k + i + 1); nd.add(8).writePointer(it);
        return nd;
      });
      nodes.forEach(function (nd, i) {
        nd.add(0xc).writePointer(nodes[(i + 1) % n1]); nd.add(0x10).writePointer(nodes[(i + n1 - 1) % n1]);
      });
      if (n1) { Q.add(0x80).writePointer(nodes[0]); Q.add(0x84).writePointer(nodes[n1 - 1]); }
      Q.add(0x88).writeS32(n1);
      // list 2: texts
      const n2 = sp.tx.length;
      const nodes2 = sp.tx.map(function (s) {
        const it = alloc(0x20); it.add(4).writePointer(s === null ? ptr(0) : mkStr(s));
        const nd = alloc(0x20); nd.add(8).writePointer(it);
        return nd;
      });
      nodes2.forEach(function (nd, i) {
        nd.add(0xc).writePointer(nodes2[(i + 1) % n2]); nd.add(0x10).writePointer(nodes2[(i + n2 - 1) % n2]);
      });
      if (n2) { b.add(0xc8).writePointer(nodes2[0]); b.add(0xcc).writePointer(nodes2[n2 - 1]); }
      b.add(0xd0).writeS32(sp.n2 !== undefined ? sp.n2 : n2);
      fx['q' + k] = Q; fx['p' + k] = Q.add(0xe0); fx['t' + k] = Q.add(0xe0 + 0x40); fx['w' + k] = W; fx['b' + k] = b;
    });
    __freeze.setU32(ptr('0x0083ad50'), opts.nodisp ? 0 : disp.toUInt32());      // buildText has frozen the first thread
    return fx;
  }
  const IMS = [[20, 12], [30, 14], [16, 10], [24, 18], [8, 40], [50, 6]];
  const TXS = ['one', 'two', 'three', 'four', 'five', 'six'];
  //  im      tx                  f4 f8      sc      a8 cb sel cols w68 h6c g48 o28 o2c par
  const PDS = [
    { im: IMS.slice(0, 4), tx: TXS.slice(0, 4), f4: 1, f8: 0x1111, sc: [3, 2], a8: 30, cb: 0, pcb: 1, m58: 3, sel: 2, cols: 2, w68: 40, h6c: 20, g48: 6, o28: 4, o2c: 5, par: 1 },
    { im: IMS.slice(0, 4), tx: TXS.slice(0, 4), f4: 0, f8: 0x2222, sc: [1, 1], a8: 12, cb: 1, sd: 1, m58: 1, sel: -1, cols: 2, w68: 33, h6c: 17, g48: 3, o28: -4, o2c: 9, par: 1 },
    { im: IMS.slice(0, 4), tx: TXS.slice(0, 4), f4: 1, f8: 0x3333, sc: [3, 2], a8: 30, cb: 0, m58: 2, sel: 1, cols: 2, w68: 40, h6c: 20, g48: 6, o28: 4, o2c: 5, par: 0 },
    { im: IMS.slice(0, 3), tx: TXS.slice(0, 3), f4: 1, f8: 0x4444, sc: [1, 1], a8: 20, cb: 0, pcb: 1, m58: 2, sel: 0, cols: 1, w68: 25, h6c: 30, g48: 0, o28: 0, o2c: 0, par: 1 },
    { im: IMS.slice(0, 5), tx: TXS.slice(0, 2), f4: 1, f8: 0x5555, sc: [2, 3], a8: 25, cb: 0, m58: 4, sel: 3, cols: 3, w68: 22, h6c: 22, g48: 2, o28: 1, o2c: 1, par: 1 },
    { im: IMS.slice(0, 6), tx: TXS.slice(0, 6), f4: 1, f8: 0x6666, sc: [5, 4], a8: 40, cb: 1, sd: 2, m58: 5, sel: 4, cols: 3, w68: 45, h6c: 28, g48: 5, o28: 10, o2c: 20, par: 1 },
    { im: IMS.slice(0, 1), tx: TXS.slice(0, 1), f4: 1, f8: 0x7777, sc: [7, 8], a8: 9, cb: 0, m58: 3, sel: 0, cols: 4, w68: 10, h6c: 10, g48: 1, o28: 0, o2c: 0, par: 1 },
    { im: [], tx: [], f4: 1, f8: 0x8888, sc: [1, 1], a8: 5, cb: 0, m58: 2, sel: 0, cols: 2, w68: 10, h6c: 10, g48: 0, o28: 0, o2c: 0, par: 1 },
    { im: IMS.slice(0, 3), tx: TXS.slice(0, 3), f4: 1, f8: 0x9999, sc: [1, 1], a8: 20, cb: 0, noth: 1, m58: 2, sel: 1, cols: 1, w68: 25, h6c: 30, g48: 0, o28: 0, o2c: 0, par: 1 },
    { im: IMS.slice(0, 3), tx: [null, 'bb', 'cc'], f4: 0, f8: 0xaaaa, sc: [2, 1], a8: 18, cb: 0, m58: 3, sel: 2, cols: 2, w68: 30, h6c: 24, g48: 2, o28: 3, o2c: 3, par: 1 },
    { im: IMS.slice(0, 3), tx: [], n2: 2, f4: 1, f8: 0xbbbb, sc: [1, 1], a8: 18, cb: 0, m58: 3, sel: 0, cols: 2, w68: 30, h6c: 24, g48: 2, o28: 3, o2c: 3, par: 1 },
  ];
  F.c3dv_pd_a = function () { return buildPD(PDS); };
  F.c3dv_pd_nodisp = function () { return buildPD(PDS, { nodisp: true }); };

  // ---- panelRow: panelDrawRow 0x004a2d50 and its two vtordisp thunks -----------------------------------------------------------
  // One 0x3000-byte block per object ($PR). P2 (`this`) = block + 0x200, Q2 = P2 - 0x8c holds the vtable pointer (dwords [+4] =
  // 0x400 -> wb = P2 + 0x400, [+8] = 0x1000 -> bb = P2 + 0x1000). wb: +0xa4 parent, +0x1e8 a copy of the fixture text object whose handle
  // (+0x1ec) is ours (slots +0xd8 / +0xdc). bb holds the grid fields (negative offsets from bb) and the cell list: +0x3c head, +0x40 cursor,
  // +0x44 count, +0x48 index; nodes (+8 item, +0xc next, +0x10 prev), items (+4 text). The vtordisp thunks get T1 = P2 + 0x10 ([T1-4] =
  // 0x10) and T2 = P2 + 0x588 ([T2-4] = 0x10, then minus 0x578).
  const PRN = 11, PRSZ = 0x3000;
  function buildPR(specs) {
    const fx = buildText({});
    const rec = fx.rec;
    const emit = codeArena();
    const nul = emit([0x33, 0xC0, 0xC3]);
    const PR = alloc(PRN * PRSZ);
    fx.PR = PR; fx.obj = PR;
    const tvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) tvt.add(i * 4).writePointer(nul);
    tvt.add(0xdc).writePointer(emit([0xB8, ...u32(90), 0xC3]));
    tvt.add(0xd8).writePointer(emit([0xB8, ...u32(70), 0xC3]));
    const thnd = alloc(0x20); thnd.writePointer(tvt); thnd.add(4).writeU32(0x480A0000);
    const pvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) pvt.add(i * 4).writePointer(nul);
    pvt.add(0x40).writePointer(emit(stubK(rec, 0x4ac30040, 'vrr')));
    const phnd = alloc(0x20); phnd.writePointer(pvt); phnd.add(4).writeU32(0x480B0000);
    const parent = alloc(0x400);
    parent.add(0x274).writeU32(0x530A0000); parent.add(0x274 + 4).writePointer(phnd);
    specs.forEach(function (sp, k) {
      const blk = PR.add(k * PRSZ);
      const P2 = blk.add(0x200), wb = P2.add(0x400), bb = P2.add(0x1000);
      const vt = alloc(0x10);
      vt.add(4).writeS32(0x400); vt.add(8).writeS32(0x1000);
      P2.sub(0x8c).writePointer(vt);
      P2.add(0xc).writeS32(0x10); P2.add(0x584).writeS32(0x10);
      if (sp.par) wb.add(0xa4).writePointer(parent);
      wb.add(0x1e8).writeByteArray(fx.tm.readByteArray(0x300));
      wb.add(0x1e8 + 4).writePointer(sp.noth ? ptr(0) : thnd);
      const bs = function (off, v) { bb.add(off).writeS32(v); };
      bs(-0x64, sp.dx); bs(-0x60, sp.dy); bs(-0x54, sp.m54); bs(-0x48, sp.m48); bs(-0x44, sp.m44);
      bs(-0x34, sp.rows); bs(-0x30, sp.cols); bs(-0x24, sp.cw); bs(-0x20, sp.ch);
      const n = sp.tx.length;
      const nodes = sp.tx.map(function (s) {
        const it = alloc(0x20); it.add(4).writePointer(s === null ? ptr(0) : mkStr(s));
        const nd = alloc(0x20); nd.add(8).writePointer(it);
        return nd;
      });
      nodes.forEach(function (nd, i) {
        nd.add(0xc).writePointer(nodes[(i + 1) % n]); nd.add(0x10).writePointer(nodes[(i + n - 1) % n]);
      });
      if (n) { bb.add(0x3c).writePointer(nodes[0]); bb.add(0x40).writePointer(nodes[n - 1]); }
      bs(0x44, sp.count === undefined ? n : sp.count); bs(0x48, 0x77);
      fx['r' + k] = P2; fx['u' + k] = P2.add(0x10); fx['v' + k] = P2.add(0x588);
    });
    __freeze.setU32(ptr('0x0083ab2c'), 0x2c2c2c2c);
    return fx;
  }
  //          dx dy  m54 m48 m44 rows cols cw  ch  par tx
  const PRS = [
    { dx: 3, dy: 4, m54: 5, m48: 2, m44: 10, rows: 2, cols: 3, cw: 50, ch: 14, par: 1, tx: ['a1', 'b22', 'c333', 'd4444', 'e55555', 'f666666'] },
    { dx: 0, dy: 0, m54: 0, m48: 0, m44: 7, rows: 1, cols: 1, cw: 30, ch: 9, par: 1, tx: ['solo'] },
    { dx: 1, dy: 1, m54: 1, m48: 1, m44: 1, rows: 0, cols: 3, cw: 20, ch: 8, par: 1, tx: ['x', 'y'] },
    { dx: 2, dy: 2, m54: 2, m48: 2, m44: 4, rows: 2, cols: 0, cw: 20, ch: 8, par: 1, tx: ['x', 'y'] },
    { dx: 3, dy: 3, m54: 3, m48: 3, m44: 6, rows: 2, cols: 2, cw: 25, ch: 11, par: 0, tx: ['p', 'q', 'r', 's'] },
    { dx: 4, dy: 5, m54: 6, m48: 7, m44: 9, rows: 2, cols: 2, cw: 33, ch: 12, par: 1, tx: [], count: 0 },
    { dx: 5, dy: 6, m54: 8, m48: 9, m44: 11, rows: 3, cols: 3, cw: 40, ch: 10, par: 1, tx: ['u', 'vv'] },
    { dx: -6, dy: 7, m54: -3, m48: -4, m44: -9, rows: 2, cols: 2, cw: -5, ch: -2, par: 1, tx: ['m', 'n', 'o'] },
    { dx: 1, dy: 2, m54: 3, m48: 4, m44: 8, rows: 2, cols: 2, cw: 30, ch: 10, par: 1, noth: 1, tx: ['h1', 'h2', 'h3'] },
    { dx: 2, dy: 3, m54: 4, m48: 5, m44: 6, rows: 1, cols: 3, cw: 28, ch: 9, par: 1, tx: [null, 'two', null] },
    { dx: 3, dy: 4, m54: 5, m48: 6, m44: 12, rows: 2, cols: 2, cw: 35, ch: 13, par: 1, tx: ['n1', 'n2', 'n3'], count: -3 },
  ];
  F.c3dv_pr_a = function () { return buildPR(PRS); };

  // ---- drawRoutingPoly 0x00475df0: c3db's polygon fixture (fixtures.d/c3db.js, c3db.cpp is linked through c3dv_batches.txt) -------
  F.c3dv_rp_a = function () {
    const fx = globalThis.DIFF_FIXTURES.c3db_a();
    negOn();
    return fx;
  };

  // @@NEXT_FAMILY@@
  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
