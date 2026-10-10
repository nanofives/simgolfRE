// Fixtures for batch c3df (round 25, golf_clean.exe text measurement and aligned text drawing). Reimplementations:
// shim/src/re/c3df.cpp. Registry: re/frida/registry.d/c3df.py. Purposes and per-jcc branch coverage:
// log/c3/c3df_purpose.md.
//
// Every object is our own allocation; nothing is linked into a window list, draw list or focus chain. The only code
// that runs inside a vector is machine code (no JavaScript, no allocation), so the keys that freeze the game's first
// thread cannot deadlock on a lock that thread holds:
//   * the font measurer: slot +0x10 of the measurer's vtable (thiscall (text, count), called by measureTextWidth
//     0x00483930) returns the sum over the count characters of (W + (char & 3)), W = measurer +4 (3, 5, 7, 11), so a
//     wrong substring, count or start flows into the result (bytes copied from c3ct's MEASURE);
//   * the text-draw callback at object +0xc (cdecl (text, x, y, len), called first by 0x004767a0): it appends
//     {tag, 0xC0C0C0C0, text, x, y, len, watch} to the recorder and returns (cursor ^ tag), always non-zero, so
//     drawRichText 0x004775b0 returns at once and every call's arguments are compared through $rec.
// The markup tokens measureMarkupRun / drawMarkupRun compare with (0x4e4254 / 0x4e4248 / 0x4e423c) are copied at run
// time from the game image, never stored here.
//
// Object layout (0x300 zeroed bytes; offsets read from the instructions, see shim/src/re/c3df.cpp): +0xc draw
// callback, +0x14 first-line indent (wrapText), +0x30 / +0x34 / +0x38 the three style fields, +0x3c link state,
// +0x40 markup on, +0x48 / +0x4c / +0x50 the three dropdown fields, +0x54 arrow width selector, +0x5c.. fonts.
// Font object (0x20 bytes): +4 measurer (vtable at +0, W at +4), +8 / +0xc / +0x10 read by Widget_value / Widget_get.
(function () {
  const G_ACC = 0x00839aa8;

  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v >>> 0); }
  function alloc(n) { const p = __keepAlloc(n); fill(p, n, 0); return p; }

  function codeArena() {
    const size = Process.pageSize * 2;
    const base = __keepAlloc(size);
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3df: code arena full');
      return p;
    };
  }

  // A recording stub (c3ct's builder): cdecl, n stack arguments, returns (cursor ^ tag).
  function stubBytes(rec, watch, tag, n) {
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
    b.push(0xC3);                                                    // ret
    return b;
  }

  // push esi; push edi; mov esi,[esp+0xc]; mov edi,[esp+0x10]; xor eax,eax; test edi,edi; jle done
  // loop: movzx edx,byte[esi]; and edx,3; add edx,[ecx+4]; add eax,edx; inc esi; dec edi; jnz loop
  // done: pop edi; pop esi; ret 8
  const MEASURE = [0x56, 0x57, 0x8B, 0x74, 0x24, 0x0C, 0x8B, 0x7C, 0x24, 0x10, 0x33, 0xC0, 0x85, 0xFF, 0x7E, 0x0F,
                   0x0F, 0xB6, 0x16, 0x83, 0xE2, 0x03, 0x03, 0x51, 0x04, 0x03, 0xC2, 0x46, 0x4F, 0x75, 0xF1,
                   0x5F, 0x5E, 0xC2, 0x08, 0x00];

  const REC_SIZE = 0x1000;

  // Strings (ASCII plus the three image tokens). NAMES keep their order: the registry mirrors the names.
  function strings(tok) {
    const b = (s) => Array.from(s, (c) => c.charCodeAt(0));
    return {
      s_plain: b('aaa bbb ccc ddd eee'),
      s_one: b('abcdefgh'),
      s_braces: b('ab {cd ef} gh ij'),
      s_brackets: b('xx [yy zz] ww vv'),
      s_mixed: b('a {b [c] d} e f'),
      s_double: b('{{a}} b [[c]] d e'),
      s_close: b('abc} def ghi'),
      s_closeb: b('abc] def ghi'),
      s_dollar: b('a$bc def ghi'),
      s_link: [...b('ab '), ...tok.link, ...b('uu=vv> cd ef gh')],
      s_linknogt: [...b('ab '), ...tok.link, ...b('uu=vvvv wx yz')],
      s_dd: [...b('ab '), ...tok.dd, ...b('1(xy^zw ab cd')],
      s_ddend: [...b('q '), ...tok.dd, ...b('1(xyz')],
      s_dl: [...b('ab '), ...tok.dl, ...b('1(xy^zz ab cd')],
      s_ddbrace: b('ab {123456789cd^ef gh ij'),
      s_caret: b('ab^cd=ef>g hi jk'),
      s_eq: b('ab=cd>ef gh ij'),
      s_empty: [],
      s_long: b('the quick brown fox jumps over the lazy dog again and again'),
      s_spaces: b('  lead  and trail  '),
      s_brace2: b('{a b c d e f g h i j k l}'),
      s_open1: b('ab {cd ef gh'),                                  // style 1 never closed
      s_open2: b('xx [yy zz'),                                      // style 2 never closed
      s_open3: b('{cd ef gh ij'),                                   // starts in style 1, never closed
      s_dl2: [...b('ab '), ...tok.dl, ...b('1(xy zz ab')],          // style 3 (dropdown link) to the end
    };
  }

  // The shared builder. opts.freeze: freeze the game's first thread and save the accumulator 0x00839aa8 (keys whose
  // functions reach wrapTextToWidth); opts.acc: the value seeded into it.
  function build(opts) {
    opts = opts || {};
    const emit = codeArena();
    const rec = alloc(REC_SIZE);
    rec.writeU32(0x10);
    const cb = emit(stubBytes(rec, G_ACC, 0xC3DF0001, 4));
    const nul = emit([0x33, 0xC0, 0xC3]);
    const meas = emit(MEASURE);
    const mvt = alloc(0x200);
    for (let i = 0; i < 0x80; i++) mvt.add(i * 4).writePointer(nul);
    mvt.add(0x10).writePointer(meas);

    const font = (w, h, p8, pc, nomeas) => {
      const m = alloc(0x10);
      m.writePointer(mvt); m.add(4).writeS32(w);
      const f = alloc(0x20);
      if (!nomeas) f.add(4).writePointer(m);
      f.add(8).writeS32(p8 || 0); f.add(0xc).writeS32(pc || 0); f.add(0x10).writeS32(h);
      return f;
    };
    const F = [font(3, 12), font(5, 14), font(7, 16), font(11, 18)];
    const Fneg = font(3, 12, -1, 21);           // Widget_value: +8 < 0 returns +0xc (21)
    const Fnomeas = font(3, 12, 0, 0, true);    // +4 NULL: measureTextWidth returns 0, the draw wrappers return 3

    const tobj = (o) => {
      const t = alloc(0x300);
      if (o.cb !== false) t.add(0xc).writePointer(cb);
      t.add(0x14).writeS32(o.indent || 0);
      const st = o.style || 0;
      t.add(0x30).writeS32(0); t.add(0x34).writeS32(st); t.add(0x38).writeS32(0);
      t.add(0x40).writeS32(o.markup === undefined ? 1 : o.markup);
      t.add(0x48).writeS32(o.drop || 0);
      t.add(0x54).writeS32(o.narrow || 0);
      const fonts = o.fonts || F;
      for (let i = 0; i < 4; i++) if (fonts[i]) t.add(0x5c + 4 * i).writePointer(fonts[i]);
      return t;
    };
    const fx = {
      rec,
      tm: tobj({}),                                         // markup on, all four fonts
      tpart: tobj({ fonts: [F[0], null, F[2], null] }),     // styles 1 and 3 fall back to the base font
      tplain: tobj({ markup: 0, fonts: [F[1]] }),           // markup off
      tnofont: tobj({ markup: 0, fonts: [] }),              // markup off, base font 0 (filled from 0x0083ad44)
      tnofont1: tobj({ markup: 1, fonts: [] }),             // markup on, base font 0
      tdrop: tobj({ drop: 1 }),                             // measure dropdown state 1 (scanToken)
      tlink: tobj({ style: 3 }),                            // measure style 3 (skipToken)
      tpend: tobj({ drop: 2 }),                             // measure dropdown state 2 (becomes 1 at entry)
      tind: tobj({ indent: 20 }),                           // wrapText first-line indent
      tneg: tobj({ fonts: [Fneg, F[1], F[2], F[3]] }),      // base font with +8 < 0
      tnomeas: tobj({ fonts: [Fnomeas] }),                  // font whose +4 is NULL
      tnocb: tobj({ markup: 0, fonts: [F[1]], cb: false }), // no draw callback and +4 NULL: 0x004767a0 returns x
      tnocbm: tobj({ markup: 1, fonts: [F[0], F[1], F[2], F[3]], cb: false }),
    };

    // Strings, each NUL-terminated and followed by 16 zero bytes in one buffer.
    const rd = (va, n) => Array.from(new Uint8Array(ptr(va).readByteArray(n)));
    const S = strings({ link: rd('0x004e4254', 6), dd: rd('0x004e4248', 9), dl: rd('0x004e423c', 9) });
    let size = 0;
    for (const k in S) size += S[k].length + 1 + 16;
    const buf = alloc((size + 3) & ~3);
    let off = 0;
    for (const k in S) {
      if (S[k].length) buf.add(off).writeByteArray(S[k]);
      fx[k] = buf.add(off);
      fx['len_' + k] = S[k].length;
      fx['cut_' + k] = Math.max(S[k].length - 4, 0);        // a run that stops inside the string
      fx['over_' + k] = S[k].length + 6;                    // a run that goes on into the zero fill
      off += S[k].length + 1 + 16;
    }
    fx.obj = buf;                                            // char* returns are reported as obj + offset
    fx.buf = buf;

    // Width cells (measureMarkupRun's in-out pixel budget): one per budget so each vector starts from the same value.
    const cell = (v) => { const c = alloc(4); c.writeS32(v); return c; };
    fx.b1 = cell(1); fx.b6 = cell(6); fx.b18 = cell(18); fx.b40 = cell(40); fx.b90 = cell(90); fx.b1e5 = cell(100000);
    // wrapText's line table and count cell
    fx.lines = alloc(0x80); fx.cnt = alloc(4);
    // rectangles for drawRight2: left, top, right, bottom
    const rect = (l, t, r, b) => { const p = alloc(0x10); [l, t, r, b].forEach((v, i) => p.add(4 * i).writeS32(v)); return p; };
    fx.r0 = rect(0, 0, 100, 40);
    fx.r1 = rect(10, 20, 60, 25);        // (b - t) - height = -7 for height 12: truncation toward zero differs from floor
    fx.r2 = rect(-5, -8, 30, 50);
    fx.r3 = rect(0, 0, 0, 0);
    fx.r4 = rect(7, 3, 200, 160);

    if (opts.freeze) {
      __freeze.thread();
      __freeze.save(ptr(G_ACC), 4);
      ptr(G_ACC).writeS32(opts.acc || 0);
    }
    return fx;
  }

  // ---- seedCourseTable 0x0046f2b0: a seed for the RNG at 0x00822d9c, the flags bit 0x1000000 of 0x0059e7b8 and a junk
  // pre-fill of the course records (0x00571fd8, 0x30e bytes cleared by the function) with a margin after them. The
  // thread is frozen and every touched global is saved and restored at the flush.
  const SEEDS = [0x00000001, 0x12345678, 0x9e3779b9, 0xdeadbeef, 0x0badf00d, 0x31415926, 0x27182818, 0x7fffffff,
                 0x80000000, 0xffffffff, 0x00001234, 0xcafebabe, 0x13579bdf, 0x2468ace0, 0xfedcba98, 0x0f0f0f0f,
                 0x00c0ffee, 0x4242424a, 0x1badb002, 0x55aa55aa, 0xa5a5a5a5, 0x600df00d, 0x0eadbeef, 0x3c6ef372];
  function seedFixture(i) {
    return function () {
      const fx = { obj: ptr('0x00571fd8') };
      __freeze.thread();
      __freeze.save(ptr('0x00822d9c'), 4);
      ptr('0x00822d9c').writeU32(SEEDS[i] >>> 0);
      const fl = ptr('0x0059e7b8');
      __freeze.save(fl, 4);
      const v = fl.readU32();
      fl.writeU32(((i & 1) ? (v | 0x1000000) : (v & ~0x1000000)) >>> 0);
      __freeze.save(ptr('0x00571fd8'), 0x320);
      const junk = (i & 2) ? 0xa5 : 0x00;                  // half of the keys start from a non-zero record area
      ptr('0x00571fd8').writeByteArray(new Array(0x320).fill(junk));
      return fx;
    };
  }

  // ---- enqueueSprite 0x00462a30: the draw queue arrays are saved, filled with a junk pattern and rebuilt. Shapes:
  //   count n, list order (node indices in link order from the head) and each node's depth; pivot = [0x00822c8c];
  //   k = [0x004c2844].
  const QARR = [0x00830164, 0x00831164, 0x0082915c, 0x0082c164, 0x0082815c, 0x0082a15c, 0x0082b160, 0x0082415c,
                0x008361f4, 0x00834174];
  function queueFixture(o) {
    return function () {
      const fx = { obj: ptr('0x00838200') };
      __freeze.thread();
      QARR.forEach(function (a, ai) {
        __freeze.save(ptr(a), 0x1000);
        for (let i = 0; i < 0x400; i++) ptr(a).add(4 * i).writeU32((0x13579bdf + ai * 0x1111 + i) >>> 0);
      });
      [0x00838200, 0x0082c160, 0x00822c8c, 0x004c2844].forEach(function (a) { __freeze.save(ptr(a), 4); });
      ptr('0x00838200').writeS32(o.count);
      ptr('0x00822c8c').writeS32(o.pivot);
      ptr('0x004c2844').writeS32(o.k);
      const order = o.order || [];
      ptr('0x0082c160').writeS32(order.length ? order[0] : -1);
      order.forEach(function (node, pos) {
        ptr('0x008361f4').add(4 * node).writeS32(pos + 1 < order.length ? order[pos + 1] : -1);
        ptr('0x0082415c').add(4 * node).writeS32(o.depth[pos]);
      });
      return fx;
    };
  }

  // ---- Surface_drawScaled 0x00473cb0 / Image_draw 0x00473e60 (the file-name arm, this+8 != 0, is not run): fixture
  // Surface records (0x30 bytes: +4 handle, +8 file name = 0, +0x20 / +0x24 hot spot) whose handles are objects with a
  // RECORDING vtable (machine code, as in c3da: entry = tag, ecx, each stack argument; $rec+0 is the byte cursor),
  // and a fixture DISPLAY (+0 vtable, +0x10 / +0x14 / +0x18 the scale triple) swapped into g_graphicsDevice
  // 0x0083ad50 with the first thread frozen: slot +0x9c (nx, ny, den) records and stores the triple, slot +0xa0
  // (&t0, &t1, &t2) records and writes the stored triple through the pointers. Handle slot +0x44 (surf handle, x, y,
  // mask handle) records and returns cursor ^ tag.
  function dispStub(rec, tag, args, ret, act, skipRecord) {
    const r = u32(rec.toUInt32());
    const b = [0x53];                                                  // push ebx
    b.push(0x8B, 0x1D, ...r);                                          // mov ebx, [REC]
    b.push(0x81, 0xC3, ...r);                                          // add ebx, REC
    b.push(0xC7, 0x03, ...u32(tag));                                   // mov dword [ebx], tag
    b.push(0x89, 0x4B, 0x04);                                          // mov [ebx+4], ecx
    let off = 8;
    for (let i = 0; !skipRecord && i < args; i++) {                    // pointer arguments are stack addresses: skipped
      b.push(0x8B, 0x44, 0x24, 8 + 4 * i, 0x89, 0x43, off); off += 4;  // mov eax,[esp+8+4i]; mov [ebx+off],eax
    }
    b.push(0x81, 0x05, ...r, ...u32(off));                             // add dword [REC], off
    if (act === 'store3') {
      for (let j = 0; j < 3; j++) b.push(0x8B, 0x44, 0x24, 8 + 4 * j, 0x89, 0x41, 0x10 + 4 * j);   // [ecx+0x10+4j] = arg j
    } else if (act === 'load3') {
      for (let j = 0; j < 3; j++) b.push(0x8B, 0x54, 0x24, 8 + 4 * j, 0x8B, 0x41, 0x10 + 4 * j, 0x89, 0x02);   // *arg j = [ecx+..]
    }
    b.push(0xA1, ...r, 0x35, ...u32(tag));                             // mov eax, [REC]; xor eax, tag
    b.push(0x5B);                                                      // pop ebx
    if (args > 0) b.push(0xC2, 4 * args, 0x00); else b.push(0xC3);
    return b;
  }
  function dispFixture(triple) {
    return function () {
      const fx = {};
      const emit = codeArena();
      const rec = alloc(0x2000);
      rec.writeU32(0x10);
      fx.rec = rec;
      const nul = emit([0x33, 0xC0, 0xC3]);
      const hvt = alloc(0x400);
      for (let i = 0; i < 0x100; i++) hvt.add(i * 4).writePointer(nul);
      hvt.add(0x44).writePointer(emit(dispStub(rec, 0x4ac30044, 4, 'cur', null)));
      const dvt = alloc(0x400);
      for (let i = 0; i < 0x100; i++) dvt.add(i * 4).writePointer(nul);
      dvt.add(0x9c).writePointer(emit(dispStub(rec, 0x0d15009c, 3, 'cur', 'store3')));
      dvt.add(0xa0).writePointer(emit(dispStub(rec, 0x0d1500a0, 3, 'cur', 'load3', true)));
      const handle = (i) => { const h = alloc(0x20); h.writePointer(hvt); h.add(4).writeU32(0x4a000000 + i); return h; };
      // [handle index or -1, m_20, m_24]: s0..s2 drawable (s1's hot spot overflows when scaled), s3 has no handle
      const D = [[0, 5, -3], [1, 0x7ffffff0, 17], [2, -40, 100], [-1, 1, 2], [4, 6, 7], [5, 0, 0], [6, -9, 13], [7, 123, -77]];
      const STRIDE = 0x30;
      const surfs = alloc(D.length * STRIDE);
      D.forEach(function (d, i) {
        const s = surfs.add(i * STRIDE);
        s.writeU32(0x53000000 + i);
        if (d[0] >= 0) s.add(4).writePointer(handle(d[0]));
        s.add(0x20).writeS32(d[1]);
        s.add(0x24).writeS32(d[2]);
        fx['s' + i] = s;
      });
      fx.surfs = surfs;
      fx.obj = surfs;
      const disp = alloc(0x40);
      disp.writePointer(dvt);
      disp.add(4).writeU32(0xd15d15d1);
      disp.add(0x10).writeS32(triple[0]); disp.add(0x14).writeS32(triple[1]); disp.add(0x18).writeS32(triple[2]);
      fx.disp = disp;
      __freeze.thread();
      __freeze.save(ptr('0x0083ad50'), 4);
      ptr('0x0083ad50').writePointer(disp);
      return fx;
    };
  }

  const F = {
    c3df_text_acc0() { return build({ freeze: true, acc: 0 }); },
    c3df_text_accN() { return build({ freeze: true, acc: 100 }); },
    c3df_draw() { return build({}); },
  };
  SEEDS.forEach(function (_, i) { F['c3df_seed_' + i] = seedFixture(i); });
  const TRIPLES = { s1: [1, 1, 1], s2: [3, 5, 2], s3: [-7, 11, 3], s4: [2, 3, -4] };
  Object.keys(TRIPLES).forEach(function (k) { F['c3df_disp_' + k] = dispFixture(TRIPLES[k]); });
  // order = node indices in link order; depth[pos] = depth of order[pos] (ascending along the list)
  const THREE = { order: [1, 0, 2], depth: [5, 10, 20] };
  F.c3df_q_empty = queueFixture({ count: 0, pivot: 500, k: 6 });
  F.c3df_q_one = queueFixture({ count: 1, pivot: 500, k: 5, order: [0], depth: [10] });
  [500, 1000, 1001, 1199, 1200, 3000].forEach(function (pv, i) {
    F['c3df_q_three_' + i] = queueFixture({ count: 3, pivot: pv, k: [4, 6, 7, 5, 9, 4][i], order: THREE.order,
                                            depth: THREE.depth });
  });
  F.c3df_q_last = queueFixture({ count: 0x3fe, pivot: 700, k: 6, order: [3, 0], depth: [-4, 8] });
  F.c3df_q_full = queueFixture({ count: 0x3ff, pivot: 700, k: 6, order: [3, 0], depth: [-4, 8] });
  F.c3df_q_over = queueFixture({ count: 0x500, pivot: 700, k: 6, order: [3, 0], depth: [-4, 8] });
  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
