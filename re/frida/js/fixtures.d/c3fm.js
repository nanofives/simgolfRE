// Fixtures for batch c3fm (round 34): golf_clean.exe static CRT, the sound-library open routines, Window::passLocalRect /
// blitToTarget with DC_call90 / DC_calla4, jgld.dll's debug CRT, jgld's dashed lines.
// Reimplementations: shim/src/re/c3fm.cpp. Registry: re/frida/registry.d/c3fm.py. Purposes and per-jcc tables:
// log/c3/c3fm_purpose.md. Coverage model over the same string table: log/c3/alt/c3fm_model.py (reads STR below).
//
// Every key freezes the game's first thread (re/frida/js/freeze.js) and arms the negative-control switch after it
// (__freeze.negOn(): only the negctl build exports one). Callees that are not the subject are RECORDING stand-ins
// (framemask.js FM.stand: a native thread-id gate, so game threads reach the real code); a frame pointer handed to one
// is logged as a layout-independent token. Objects a body dispatches through carry recording vtables (one
// NativeCallback per slot; every call is appended to $vlog, a state region). Every stand-in answers only from its
// arguments and from memory that is compared state.
(function () {
  const F = {};
  const u32 = (v) => (typeof v === 'number' ? v : v.toUInt32()) >>> 0;
  const fill = (p, n, b) => p.writeByteArray(new Uint8Array(n).fill(b).buffer);
  function alloc(n) { const p = __keepAlloc(n); fill(p, n, 0); return p; }
  const jgld = () => Process.getModuleByName('jgld.dll').base;
  const jva = (a) => jgld().add(a - 0x10000000);

  // The negctl build's mutants also test the calling thread: word 2 of every <Batch>NegCtl export gets the A/B thread's
  // id (this fixture's thread), restored by the flush like word 0. A normal build exports no switch: nothing happens.
  let frozen = false;
  function freezeNow() {
    if (frozen) return;
    __freeze.thread();
    frozen = true;
    __freeze.atFlush(function () { frozen = false; });
    __freeze.negOn();
    const tid = Process.getCurrentThreadId();
    __freeze.negCtlWords().forEach(function (a) { __freeze.setU32(a.add(8), tid); });
  }

  // ------------------------------------------------------------------------------------------------- recorders
  // $vlog: +0 records, +4 bytes used, +8 FNV-1a over every record, +0xc records not stored, records {tag, n, words...}
  // from +0x10.
  const VLOG = 0x1000;
  function fnv(h, w) { for (let k = 0; k < 4; k++) { h ^= (w >>> (8 * k)) & 0xff; h = Math.imul(h, 0x01000193) >>> 0; } return h >>> 0; }
  function recorder(FM) {
    const log = alloc(VLOG);
    log.add(8).writeU32(0x811c9dc5);
    const map = (v) => (FM ? FM.map(u32(v)) : u32(v));
    function rec(words) {
      let h = log.add(8).readU32();
      words.forEach(function (w) { h = fnv(h, w >>> 0); });
      log.add(8).writeU32(h);
      log.writeU32(log.readU32() + 1);
      const used = log.add(4).readU32(), need = 8 + 4 * (words.length - 1);
      if (0x10 + used + need > VLOG) { log.add(0xc).writeU32(log.add(0xc).readU32() + 1); return; }
      const e = log.add(0x10 + used);
      e.writeU32(words[0] >>> 0);
      e.add(4).writeU32(words.length - 1);
      for (let i = 1; i < words.length; i++) e.add(4 + 4 * i).writeU32(words[i] >>> 0);
      log.add(4).writeU32(used + need);
    }
    // a vtable of 64 thiscall slots; `slots` {offset: [nargs, body(self, args) -> ret]}; every other slot records
    // 0xdead0000 | offset and returns 0x0badc0de
    function vtable(tag, slots) {
      const vt = alloc(4 * 64);
      for (let i = 0; i < 64; i++) {
        const off = 4 * i, s = slots[off];
        const nargs = s ? s[0] : 0;
        const cb = new NativeCallback(function () {
          const a = Array.prototype.slice.call(arguments).map(u32);
          const self = a[0], args = a.slice(1);
          if (!s) { rec([(0xdead0000 | off) >>> 0, map(self)]); return 0x0badc0de; }
          const r = s[1](self, args);
          rec([(tag << 16 | off) >>> 0, map(self)].concat(args.map(map)));
          return (r === undefined ? 0 : r) >>> 0;
        }, 'uint32', ['uint32'].concat(Array(nargs).fill('uint32')), 'thiscall');
        __diffKeepAlive.push(cb);
        vt.add(off).writePointer(cb);
      }
      return vt;
    }
    return { log: log, rec: rec, vtable: vtable, map: map };
  }
  const rd = (p, o) => ptr(p).add(o).readU32();
  const rds = (p, o) => ptr(p).add(o).readS32();

  // --------------------------------------------------------------------------------------------- the string table
  // [name, alignment offset (0..3), text or byte array]; each string sits in its own 0x40 slot of the block at
  // 16-aligned slot + offset, the slot prefilled with 0xEE so bytes after a terminator are never 0. ALIAS: name -> [base
  // string, byte offset]. log/c3/alt/c3fm_model.py parses the JSON between the markers.
  /*STR-BEGIN*/
  const STR = [
    ["A16", 0, "abcdefghijklmnop"], ["E", 0, ""], ["E1", 1, ""], ["E3", 3, ""],
    ["U1N", 1, "q"], ["U2", 2, "xy"], ["U3", 3, "pqrstuvw"],
    ["FPC", 0, [97, 98, 99, 248, 101, 102, 120, 0]], ["FPZ", 0, [97, 98, 99, 128, 122, 0]],
    ["Z0", 0, "abcd"], ["Z1", 0, "abcde"], ["Z2", 0, "abcdef"], ["Z3", 0, "abcdefg"],
    ["ZB0", 0, [0, 113, 97, 97]], ["ZB1", 0, [97, 0, 113, 97]], ["ZB2", 0, [97, 98, 0, 113]], ["ZB3", 0, [97, 98, 99, 0]],
    ["HB", 0, [97, 128, 98, 255, 99, 200, 0]], ["H80", 2, [128, 128, 128, 128, 128, 0]],
    ["HABCABCABD", 0, "abcabcabd"], ["NA", 0, "a"], ["NAB", 1, "ab"], ["HXX", 0, "xx"], ["HAXBAB", 2, "axbab"],
    ["NABC", 0, "abc"], ["NABD", 3, "abd"], ["NZZZ", 0, "zzz"], ["HFOX", 0, "the quick brown fox jumps over the lazy dog"],
    ["NLAZY", 0, "lazy dog"], ["NDOG", 0, "dog"], ["HABCDE", 0, "abcde"], ["NABCDE", 2, "abcde"], ["HABCDX", 0, "abcdx"],
    ["NABCD", 0, "abcd"], ["HABCD", 1, "abcd"], ["HAAAB", 0, "aaab"], ["NAAB", 0, "aab"], ["HABCABD", 0, "abcabd"],
    ["NABCABD", 0, "abcabd"], ["NC", 0, "c"], ["HABCXE", 0, "abcxe"], ["NABCDEF", 0, "abcdef"], ["NBC", 0, "bc"],
    ["L20", 0, "abcdefghijklmnopqrst"], ["L7", 1, "abcdefg"], ["L5", 2, "abcde"], ["L6", 3, "abcdef"],
    ["L80", 0, [128, 128, 128, 128, 97, 0]], ["LFF", 1, [255, 255, 255, 1, 2, 3, 4, 5, 0]],
    ["T0", 0, "  hello, world;; foo "], ["T1", 0, ",,,"], ["T3", 1, "abc"], ["T4", 0, [97, 128, 98, 255, 99, 0]],
    ["T5", 0, "x"], ["T6", 2, ";;tok1;tok2"], ["T7", 0, "::"], ["T8", 0, "a,b"],
    ["D0", 0, " ,;"], ["DE", 0, ""], ["D2", 0, [128, 255]], ["D3", 0, ";"], ["D4", 0, ","], ["D5", 0, "abcdefghijklmnopqrstuvwxyz"]
  ];
  const ALIAS = { "A16_1": ["A16", 1], "A16_2": ["A16", 2], "A16_3": ["A16", 3], "A16_5": ["A16", 5],
                  "HFOX_3": ["HFOX", 3], "L20_1": ["L20", 1], "L20_2": ["L20", 2], "L20_3": ["L20", 3],
                  "T0_4": ["T0", 4] };
  /*STR-END*/
  function bytesOf(v) {
    if (typeof v === 'string') { const a = []; for (let i = 0; i < v.length; i++) a.push(v.charCodeAt(i) & 0xff); return a; }
    return v.slice();
  }
  const STRBLK = 0x40 * 128;
  function strings(out) {
    const blk = alloc(STRBLK);
    fill(blk, STRBLK, 0xee);
    STR.forEach(function (e, i) {
      const b = bytesOf(e[2]);
      if (b.length === 0 || b[b.length - 1] !== 0) b.push(0);
      const p = blk.add(0x40 * i + e[1]);
      p.writeByteArray(b);
      out[e[0]] = p;
    });
    Object.keys(ALIAS).forEach(function (k) { out[k] = out[ALIAS[k][0]].add(ALIAS[k][1]); });
    out.strblk = blk;
    return blk;
  }

  // ========================================================================= group X: exe string scans (pure, frozen)
  // _strchr / _strstr: pointer results are reported relative to the block (fx.obj).
  F.c3fm_str = function () {
    const out = {};
    const blk = strings(out);
    out.obj = blk;
    freezeNow();
    return out;
  };

  // __itoa / _xtoa: eight 0x40-byte buffers prefilled with 0xEE in $iblk (compared), results relative to it.
  F.c3fm_itoa = function () {
    const blk = alloc(0x200);
    fill(blk, 0x200, 0xee);
    const out = { iblk: blk, obj: blk };
    for (let i = 0; i < 8; i++) out['B' + i] = blk.add(0x40 * i + (i & 3));
    freezeNow();
    return out;
  };

  // _strtok: the A/B thread's data block from __getptd 0x004a8e53 (allocated on first use, so called BEFORE the
  // freeze); its +0x18 is the saved position, a state region ($ptd, 0x18, 4). `pos` is the preset position.
  function tokFx(pos) {
    return function () {
      const out = {};
      const blk = strings(out);
      out.obj = blk;
      const getptd = new NativeFunction(ptr(0x004a8e53), 'pointer', [], 'mscdecl');
      out.ptd = getptd();
      freezeNow();
      __freeze.save(out.ptd.add(0x18), 4);
      out.ptd.add(0x18).writePointer(pos(out));
      return out;
    };
  }
  F.c3fm_tok = tokFx(function (o) { return o.T6; });                  // ";;tok1;tok2"
  F.c3fm_tok_end = tokFx(function (o) { return o.T7.add(2); });       // the terminator of "::"
  F.c3fm_tok_mid = tokFx(function (o) { return o.T8.add(1); });       // ",b" of "a,b"

  // _fopen: __fsopen 0x004a611c stood in (records name / mode strings and the share flag; answers from them).
  const FMLOG = 0x1000;
  function strHash(p) {
    let h = 0x1505;
    if (p === 0) return 0x77;
    for (let i = 0; i < 64; i++) { const c = ptr(p).add(i).readU8(); if (c === 0) break; h = (Math.imul(h, 33) + c) >>> 0; }
    return h >>> 0;
  }
  F.c3fm_fopen = function () {
    const FM = __frameMask.begin({ size: FMLOG });
    const out = {};
    strings(out);
    out.fmlog = FM.block();
    FM.stand({ addr: 0x004a611c, abi: 'mscdecl', nargs: 3, str: { 0: 4, 1: 2 },
               ret: function (s, a) { return (0x0f000000 | ((strHash(a[0]) ^ Math.imul(strHash(a[1]), 7) ^ a[2]) & 0xffff)) >>> 0; } });
    freezeNow();
    return out;
  };

  // _fclose / _fgets: FILE records (0x20 bytes) in $fblk. +0 _ptr, +4 _cnt, +0xc _flag; the stood-in __filbuf reads
  // its refill source from the record's spare words: +0x14 next source byte, +0x18 bytes left, +0x1c chunk size.
  // __lock_file / __unlock_file stood in (recorded); __fclose_lk stood in: clears the flags, answers 0 for a stream
  // with any of 0x83 set, else -1 (recorded with the record's first four words).
  const FBLK = 0x1000;
  // [initial bytes, _cnt, flag, refill bytes, chunk]
  const FILES = [
    ["line one\nline two\n", 18, 0x01, null, 0],
    ["abc", 3, 0x01, "defgh\nxyz", 4],
    ["", 0, 0x01, null, 0],
    ["", 0, 0x01, "hello", 2],
    [[0x41, 0xff, 0x42, 0x0a, 0x43], 5, 0x01, null, 0],
    ["ab", 2, 0x01, null, 0],
    ["", 0, 0x09, "q", 1],
    ["x\n", 2, 0x01, "never", 3],
  ];
  const FLAGS = [0x40, 0x41, 0x01, 0x02, 0x80, 0x83, 0x00, 0xc0, 0x140, 0x4000, 0x10, 0xffffffbf];
  function filesFx() {
    const FM = __frameMask.begin({ size: FMLOG });
    const blk = alloc(FBLK);
    fill(blk.add(0x800), 0x800, 0xee);
    const out = { fblk: blk, obj: blk, fmlog: FM.block() };
    let data = 0x300;
    FILES.forEach(function (d, i) {
      const f = blk.add(0x20 * i);
      const b = bytesOf(d[0]);
      const p = blk.add(data);
      if (b.length) p.writeByteArray(b);
      data += 0x20;
      let r = 0, rn = 0;
      if (d[3] !== null) { const rb = bytesOf(d[3]); r = blk.add(data); r.writeByteArray(rb); rn = rb.length; data += 0x20; }
      f.writePointer(p); f.add(4).writeS32(d[1]); f.add(0xc).writeU32(d[2]);
      f.add(0x14).writePointer(r === 0 ? ptr(0) : r); f.add(0x18).writeU32(rn); f.add(0x1c).writeU32(d[4]);
      out['F' + i] = f;
    });
    FLAGS.forEach(function (fl, i) {
      const g = blk.add(0x100 + 0x14 * i);
      g.add(0xc).writeU32(fl);
      g.writeU32(0x6000 + i);
      g.add(4).writeU32(0x60000 + i);
      out['G' + i] = g;
    });
    for (let i = 0; i < 4; i++) out['B' + i] = blk.add(0x800 + 0x100 * i + i);
    FM.stand({ addr: 0x004a9874, abi: 'mscdecl', nargs: 1, ret: 0 });             // __lock_file
    FM.stand({ addr: 0x004a98c6, abi: 'mscdecl', nargs: 1, ret: 0 });             // __unlock_file
    FM.stand({ addr: 0x004a60d0, abi: 'mscdecl', nargs: 1, deref: { 0: 4 },        // __fclose_lk
               ret: function (s, a) {
                 const fl = rd(a[0], 0xc);
                 ptr(a[0]).add(0xc).writeU32(0);
                 return (fl & 0x83) !== 0 ? 0 : 0xffffffff;
               } });
    FM.stand({ addr: 0x004aa9f5, abi: 'mscdecl', nargs: 1, deref: { 0: 8 },        // __filbuf
               write: function (s, a) {
                 const f = ptr(a[0]), left = f.add(0x18).readU32();
                 if (left === 0) { f.add(4).writeS32(0); f.add(0xc).writeU32(f.add(0xc).readU32() | 0x10); return; }
               },
               ret: function (s, a) {
                 const f = ptr(a[0]), left = f.add(0x18).readU32();
                 if (left === 0) return 0xffffffff;
                 const src = f.add(0x14).readPointer(), chunk = Math.max(1, f.add(0x1c).readU32());
                 const k = Math.min(chunk, left), c = src.readU8();
                 f.writePointer(src.add(1)); f.add(4).writeS32(k - 1);
                 f.add(0x14).writePointer(src.add(k)); f.add(0x18).writeU32(left - k);
                 return c;
               } });
    freezeNow();
    return out;
  }
  F.c3fm_files = filesFx;

  // ======================================================================================== group O: openHook A/B/C
  // Device-node objects in $oblk: +0x14 the device pointer. The library entry point global (0x0083af7c / 0x0083af84 /
  // 0x0083af8c) is replaced under the freeze by a recording __cdecl (out, arg) callback (records out and arg, stores
  // 0xd0000000 | arg low 16 bits into *out unless arg is 7, answers arg * 3 + 1, or 0 for arg 7), or by NULL.
  function openFx(slot, withFn) {
    return function () {
      const R = recorder(null);
      const blk = alloc(0x200);
      const out = { oblk: blk, vlog: R.log };
      [0, 0x12345678, 0, 0xffffffff].forEach(function (v, i) {
        const o = blk.add(0x40 * i);
        o.writeU32(0x0b000000 + i); o.add(0x14).writeU32(v); o.add(0x18).writeU32(0x0e000000 + i);
        out['O' + i] = o;
      });
      const cb = new NativeCallback(function (outp, arg) {
        const a = u32(arg), o = u32(outp);
        R.rec([0xc4ea7e00, o, a]);
        if (a !== 7) ptr(o).writeU32((0xd0000000 | (a & 0xffff)) >>> 0);
        return a === 7 ? 0 : (Math.imul(a, 3) + 1) >>> 0;
      }, 'uint32', ['uint32', 'uint32'], 'mscdecl');
      __diffKeepAlive.push(cb);
      freezeNow();
      __freeze.save(ptr(slot), 4);
      ptr(slot).writePointer(withFn ? cb : ptr(0));
      return out;
    };
  }
  F.c3fm_open_a = openFx(0x0083af7c, true);
  F.c3fm_open_a_null = openFx(0x0083af7c, false);
  F.c3fm_open_b = openFx(0x0083af84, true);
  F.c3fm_open_b_null = openFx(0x0083af84, false);
  F.c3fm_open_c = openFx(0x0083af8c, true);
  F.c3fm_open_c_null = openFx(0x0083af8c, false);

  // ===================================================================================== group W: passLocalRect
  // 0x00487b00 stood in: records this and the four words of the rect it receives (the rect pointer is a frame pointer:
  // logged as a token), answers r[2] * 0x10001 ^ r[3] ^ the low 16 bits of this.
  const RECTS = [[0, 0, 0, 0], [10, 20, 110, 220], [-5, -6, 5, 6], [100, 50, 40, 30], [0x7fffffff, 0, -0x80000000, 1],
                 [3, 4, 3, 4], [-100, -200, -300, -400], [1, 2, 3, 0x7fffffff], [640, 480, 800, 600]];
  F.c3fm_plr = function () {
    const FM = __frameMask.begin({ size: FMLOG });
    const blk = alloc(0x200 * RECTS.length);
    const out = { wblk: blk, fmlog: FM.block() };
    RECTS.forEach(function (r, i) {
      const w = blk.add(0x200 * i);
      for (let k = 0; k < 0x200; k += 4) w.add(k).writeU32((0x57000000 | (i << 12) | k) >>> 0);
      r.forEach(function (v, k) { w.add(0x1ac + 4 * k).writeS32(v); });
      out['P' + i] = w;
    });
    FM.stand({ addr: 0x00487b00, abi: 'thiscall', nargs: 1, deref: { 0: 4 },
               ret: function (s, a) { return ((Math.imul(rd(a[0], 8), 0x10001) ^ rd(a[0], 0xc) ^ (s & 0xffff)) >>> 0); } });
    freezeNow();
    return out;
  };

  // ========================================================================================= group W: DC_call90 / a4
  // DC objects: D0 / D2 with a recording surface at +4 (slots +0x90 / +0xa4: four arguments, answer from them), D1 with
  // +4 = NULL. Targets: T0 (+4 = 0xa0000001), T1 (+4 = 0), T2 (+4 = the surface S0 itself).
  F.c3fm_dc = function () {
    const R = recorder(null);
    const blk = alloc(0x400);
    const out = { dblk: blk, vlog: R.log };
    const ans = function (tag) { return function (s, a) { return ((tag + Math.imul(a[1], 3) + Math.imul(a[2], 5) + Math.imul(a[3], 7)) ^ a[0]) >>> 0; }; };
    const svt = R.vtable(0x5f, { 0x90: [4, ans(0x90000000)], 0xa4: [4, ans(0xa4000000)] });
    const s0 = blk.add(0x300);
    s0.writePointer(svt);
    const mk = function (k, off, v) { const o = blk.add(off); o.writeU32(0x0dc00000 + off); o.add(4).writeU32(u32(v)); out[k] = o; };
    mk('D0', 0x000, s0); mk('D1', 0x040, 0); mk('D2', 0x080, s0);
    mk('T0', 0x100, 0xa0000001); mk('T1', 0x140, 0); mk('T2', 0x180, s0);
    out.S0 = s0;
    freezeNow();
    return out;
  };

  // ==================================================================================== group W: Window::blitToTarget
  // Stood in (each records its this, arguments and the words its pointer arguments point at; frame pointers become
  // tokens): Widget_fillRectR 0x00475b00, View::toParent 0x0047b170 (adds this-derived offsets to *x / *y), DC_call84
  // 0x00475b60, DC_call90 0x00475ba0, DC_calla4 0x00475be0, DC_calla0 0x00475c20, Surface_blitThunk 0x00475c60. The
  // display 0x0083ad50 is a recording display (slot +0x9c) guarded by recdisplay.js, or NULL; the redirect target
  // 0x0083aadc and its offsets 0x0083aae4 / 0x0083aae8, and the screen widget's surface pointer 0x0083a7b8 + 4, are set
  // under the freeze. Target surfaces carry a recording slot +0xc4.
  // window fields: [flags +0x9c, +0xa4, +0xa8, +0xac, rect +0x1ac (4), window rect +0x1dc (4)]
  const WINS = [
    [0x01000000, 0, 0, 0, [0, 0, 64, 32], [10, 20, 74, 52]],
    [0x01000000, 0x11, 0, 0, [5, 6, 50, 60], [-3, 4, 100, 200]],
    [0x01000000, 0, 0x22, 0, [1, 1, 2, 2], [0, 0, 1, 1]],
    [0x01000000, 0, 0, 0x33, [0, 0, 8, 8], [300, 200, 340, 260]],
    [0x01000000, 0, 0x44, 0x55, [2, 3, 4, 5], [7, 8, 9, 10]],
    [0x01000000, 0x66, 0x77, 0x88, [9, 9, 9, 9], [1, 2, 3, 4]],
    [0x00000000, 0x99, 0, 0, [10, 20, 110, 220], [50, 60, 70, 80]],
    [0xfeffffff, 0, 0, 0, [-10, -20, 30, 40], [-1, -2, -3, -4]],
    [0x01000001, 0, 0, 0, [0x7fffffff, 0, -0x80000000, 5], [100, 100, 100, 100]],
  ];
  function bttFx(opt) {
    return function () {
      const FM = __frameMask.begin({ size: FMLOG });
      const R = recorder(FM);
      const blk = alloc(0x400 * WINS.length + 0x400);
      const out = { wblk: blk, vlog: R.log, fmlog: FM.block() };
      WINS.forEach(function (d, i) {
        const w = blk.add(0x400 * i);
        for (let k = 0; k < 0x300; k += 4) w.add(k).writeU32((0x5b000000 | (i << 12) | k) >>> 0);
        w.add(0x9c).writeU32(d[0] >>> 0); w.add(0xa4).writeU32(d[1]); w.add(0xa8).writeU32(d[2]); w.add(0xac).writeU32(d[3]);
        d[4].forEach(function (v, k) { w.add(0x1ac + 4 * k).writeS32(v); });
        d[5].forEach(function (v, k) { w.add(0x1dc + 4 * k).writeS32(v); });
        out['W' + i] = w;
      });
      const misc = blk.add(0x400 * WINS.length);
      const tvt = R.vtable(0x7a, { 0xc4: [0, function (s) { return (0xc4000000 | (s & 0xffff)) >>> 0; }] });
      const ts = misc.add(0x100); ts.writePointer(tvt);                    // a target surface (+0xc4 recorder)
      const tgt = misc.add(0x200); tgt.writeU32(0x7a600000); tgt.add(4).writePointer(opt.ts ? ts : ptr(0));
      const dvt = R.vtable(0xd1, { 0x9c: [3, function (s, a) { return a[0] + a[1] + a[2]; }] });
      const disp = misc.add(0x300); disp.writePointer(dvt);
      const ans = function (tag, mul) {
        return function (s, a) { return (tag + Math.imul(a[1] | 0, mul) + Math.imul(a[2] | 0, 3) + (a[3] | 0) + (a[a.length - 1] & 0xff)) >>> 0; };
      };
      FM.stand({ addr: 0x00475b00, abi: 'thiscall', nargs: 1, deref: { 0: 4 }, ret: 0x1e00 });
      FM.stand({ addr: 0x0047b170, abi: 'thiscall', nargs: 2, deref: { 0: 1, 1: 1 },
                 write: function (s, a) {
                   ptr(a[0]).writeS32(ptr(a[0]).readS32() + (s & 0xff) + 3);
                   ptr(a[1]).writeS32(ptr(a[1]).readS32() - ((s >>> 8) & 0x3f) - 7);
                 }, ret: 0x2e00 });
      FM.stand({ addr: 0x00475b60, abi: 'thiscall', nargs: 3, ret: ans(0x84000000, 11) });
      FM.stand({ addr: 0x00475ba0, abi: 'thiscall', nargs: 4, ret: ans(0x90000000, 13) });
      FM.stand({ addr: 0x00475be0, abi: 'thiscall', nargs: 4, ret: ans(0xa4000000, 17) });
      FM.stand({ addr: 0x00475c20, abi: 'thiscall', nargs: 4, ret: ans(0xa0000000, 19) });
      FM.stand({ addr: 0x00475c60, abi: 'thiscall', nargs: 7, ret: ans(0xb0000000, 23) });
      if (opt.disp) __recDisplay.guard(disp);
      freezeNow();
      const set = function (a, v) { __freeze.save(ptr(a), 4); ptr(a).writeU32(u32(v)); };
      set(0x0083ad50, opt.disp ? disp : 0);
      if (opt.redirect) { set(0x0083aadc, tgt); set(0x0083aae4, 13); set(0x0083aae8, -7); }
      else { set(0x0083aadc, 0); set(0x0083a7b8 + 4, opt.ts ? ts : 0); }
      return out;
    };
  }
  F.c3fm_btt_redirect = bttFx({ disp: true, redirect: true, ts: true });
  F.c3fm_btt_redirect_ts0 = bttFx({ disp: true, redirect: true, ts: false });
  F.c3fm_btt_screen = bttFx({ disp: true, redirect: false, ts: true });
  F.c3fm_btt_screen_ts0 = bttFx({ disp: true, redirect: false, ts: false });
  F.c3fm_btt_nodisp = bttFx({ disp: false, redirect: true, ts: true });

  // ======================================================================================== group J: jgld debug CRT
  // fopen: __fsopen 0x1007f9a0 stood in, as for the exe.
  F.c3fm_jfopen = function () {
    const FM = __frameMask.begin({ size: FMLOG });
    const out = {};
    strings(out);
    out.fmlog = FM.block();
    FM.stand({ addr: u32(jva(0x1007f9a0)), abi: 'mscdecl', nargs: 3, str: { 0: 4, 1: 2 },
               ret: function (s, a) { return (0x0f100000 | ((strHash(a[0]) ^ Math.imul(strHash(a[1]), 5) ^ a[2]) & 0xffff)) >>> 0; } });
    freezeNow();
    return out;
  };

  // fclose: __lock_file 0x10083230, __fclose_lk 0x1007f8e0 (clears the flag dword, answers 0 when any of 0xf is set,
  // else -1), __unlock_file 0x100832a0 stood in. G0..G11 records with the flag dwords FLAGS.
  F.c3fm_jfile = function () {
    const FM = __frameMask.begin({ size: FMLOG });
    const blk = alloc(0x400);
    const out = { fblk: blk, fmlog: FM.block() };
    FLAGS.forEach(function (fl, i) {
      const g = blk.add(0x20 * i);
      g.add(0xc).writeU32(fl >>> 0); g.writeU32(0x6100 + i); g.add(4).writeU32(0x61000 + i);
      out['G' + i] = g;
    });
    FM.stand({ addr: u32(jva(0x10083230)), abi: 'mscdecl', nargs: 1, ret: 0 });
    FM.stand({ addr: u32(jva(0x1007f8e0)), abi: 'mscdecl', nargs: 1, deref: { 0: 4 },
               ret: function (s, a) {
                 const fl = rd(a[0], 0xc);
                 ptr(a[0]).add(0xc).writeU32(0);
                 return (fl & 0xf) !== 0 ? 0 : 0xffffffff;
               } });
    FM.stand({ addr: u32(jva(0x100832a0)), abi: 'mscdecl', nargs: 1, ret: 0 });
    freezeNow();
    return out;
  };

  // abs: nothing to seed; the key only freezes (so the negative-control switch arms like every other key's)
  F.c3fm_plain = function () { freezeNow(); return {}; };

  // operator_new: __nh_malloc 0x1007fba0 stood in: records (size, nhFlag), answers 0x0a000000 + size * 16 + nhFlag.
  F.c3fm_jnew = function () {
    const FM = __frameMask.begin({ size: FMLOG });
    const out = { fmlog: FM.block() };
    FM.stand({ addr: u32(jva(0x1007fba0)), abi: 'mscdecl', nargs: 2,
               ret: function (s, a) { return (0x0a000000 + Math.imul(a[0], 16) + a[1]) >>> 0; } });
    freezeNow();
    return out;
  };

  // strlen / strcat: the string table; strcat destinations: eight 0x40-byte slots of $cblk (0xEE-filled) preloaded
  // with C0..C7, at alignment offsets 0..3.
  // C8 "abc" at offset 0: the terminator is byte 3 of the first dword; C9 at offset 1: bytes 1..3 by the byte loop, then
  // the dword a b c 0x80 (a candidate with no zero byte), then a, 0.
  const CDST = ["", "ab", "abc", "abcdefg", "x", "", "hello wo", "abcdefghijklmnopqrstuvwxyz0123", "abc",
                [97, 98, 99, 97, 98, 99, 128, 97]];
  F.c3fm_jstr = function () {
    const out = {};
    const sblk = strings(out);
    const cblk = alloc(0x280);
    fill(cblk, 0x280, 0xee);
    CDST.forEach(function (s, i) {
      const p = cblk.add(0x40 * i + (i & 3));
      p.writeByteArray(bytesOf(s).concat([0]));
      out['C' + i] = p;
    });
    out.cblk = cblk;
    out.obj = cblk;
    freezeNow();
    return out;
  };

  // memcpy: $mblk 0x400 bytes, byte k = (k * 7 + (k >> 8) * 0x35 + 3) & 0xff (no period of 0x100, so a copy between
  // offsets 0x100 apart changes bytes); vectors give dst / src as offsets M<k>.
  F.c3fm_jmem = function () {
    const blk = alloc(0x400);
    const b = new Uint8Array(0x400);
    for (let k = 0; k < 0x400; k++) b[k] = (k * 7 + (k >> 8) * 0x35 + 3) & 0xff;
    blk.writeByteArray(b.buffer);
    const out = { mblk: blk, obj: blk };
    [0, 1, 2, 3, 4, 5, 8, 0x10, 0x11, 0x13, 0x20, 0x22, 0x40, 0x41, 0x80, 0x100, 0x101, 0x102, 0x103, 0x180, 0x200,
     0x201, 0x203, 0x280, 0x300].forEach(function (k) { out['M' + k.toString(16)] = blk.add(k); });
    freezeNow();
    return out;
  };

  // _setjmp3: eight 0x40-byte jmp_buf slots in $jblk (0xEE-filled, compared whole); R0 / R1 are records whose +0xc the
  // count-1 arm reads ([third argument + 0xc]).
  function sj3Fx(noHead) {
    return function () {
      const blk = alloc(0x300);
      fill(blk, 0x200, 0xee);
      const out = { jblk: blk };
      for (let i = 0; i < 8; i++) out['J' + i] = blk.add(0x40 * i);
      const r = blk.add(0x200);
      r.add(0xc).writeU32(0x1357);
      r.add(0x2c).writeU32(0x2468ace0);
      out.R0 = r; out.R1 = r.add(0x20);
      let getHead = null, setHead = null;
      if (noHead) {
        // two tiny routines on the calling thread: `mov eax, fs:[0]; ret` and `mov eax, [esp + 4]; mov fs:[0], eax; ret`.
        // The fixture runs on the A/B thread, so fs:[0] here is the head every vector of this key sees.
        const code = __keepAlloc(Process.pageSize);
        Memory.patchCode(code, 32, function (c) {
          c.writeByteArray([0x64, 0xa1, 0, 0, 0, 0, 0xc3]);
          c.add(16).writeByteArray([0x8b, 0x44, 0x24, 0x04, 0x64, 0xa3, 0, 0, 0, 0, 0xc3]);
        });
        getHead = new NativeFunction(code, 'uint32', [], 'mscdecl');
        setHead = new NativeFunction(code.add(16), 'void', ['uint32'], 'mscdecl');
      }
      freezeNow();
      if (noHead) {
        // the restore runs from the flush (setTimeout on the script's JS thread, the thread the rpc and this fixture
        // run on); it refuses to write another thread's fs:[0]
        const old = getHead(), tid = Process.getCurrentThreadId();
        setHead(0xffffffff);
        __freeze.atFlush(function () {
          if (Process.getCurrentThreadId() !== tid) { console.error('c3fm_sj3_nohead: flush on another thread, fs:[0] left -1'); return; }
          setHead(old);
        });
      }
      return out;
    };
  }
  F.c3fm_sj3 = sj3Fx(false);
  F.c3fm_sj3_nohead = sj3Fx(true);

  // ========================================================================================= group D: dashedLine16
  // Surfaces in $sblk, 0x100 bytes each, on a recording vtable whose slots answer from the object's own fields (so
  // log/c3/alt/c3fm_cov.py models them exactly): +0x10 lock -> [+0x60]; +0xc8 getClipRect(RECT *) copies +0x50..+0x5c
  // into the caller's RECT (a frame pointer: logged as a framemask token); +0x20 base -> [+0x40]; +0xe0 pitch (pixels)
  // -> [+0x44]; +0xe4 -> this + 0x64 (the format word the original reads at +4 is +0x68); +0x24 unlock(1) -> 0x24.
  // +0x7c = the palette. Palettes: slot +0x18 -> [+0x40], +0x1c -> [+0x44] (two 256-entry word tables). Pixel buffers
  // 48 x 32 pixels (pitch 48), 0xEEEE-filled, one per surface, all in $sblk. dashedVLine16 / dashedHLine16 are reached
  // through the thunks 0x100012b2 / 0x100016b3: recording stand-ins (thiscall, 8 arguments).
  //   S0 palette P0, format 0, clip {4, 3, 40, 28}    S1 as S0, format 1    S2 as S0, format 2 (returns at once)
  //   S3 no palette (+0x7c = 0: the module global 0x1012873c decides, per key)    S4 lockBits 0    S5 P0, format 0,
  //   clip {0, 0, 48, 32}
  // The global 0x1012873c is set under the freeze for the _g* keys: G (+4 = P1) for _gpal, G0 (+4 = 0) for _gnullpal,
  // 0 for _gnone; the main key leaves it alone (no vector of it uses S3).
  const DL_W = 48, DL_H = 32;
  function dl16Fx(gmode) {
    return function () {
      const FM = __frameMask.begin({ size: FMLOG });
      const R = recorder(FM);
      const NS = 6, PIX = DL_W * DL_H * 2;
      const blk = alloc(0x100 * 8 + 0x400 * 2 + PIX * NS);
      const out = { sblk: blk, vlog: R.log, fmlog: FM.block(), obj: blk };
      const svt = R.vtable(0x16, {
        0x10: [0, function (s) { return rd(s, 0x60); }],
        0xc8: [1, function (s, a) { for (let k = 0; k < 4; k++) ptr(a[0]).add(4 * k).writeU32(rd(s, 0x50 + 4 * k)); return 0; }],
        0x20: [0, function (s) { return rd(s, 0x40); }],
        0xe0: [0, function (s) { return rd(s, 0x44); }],
        0xe4: [0, function (s) { return (s + 0x64) >>> 0; }],
        0x24: [1, function () { return 0x24; }],
      });
      const pvt = R.vtable(0x17, {
        0x18: [0, function (s) { return rd(s, 0x40); }],
        0x1c: [0, function (s) { return rd(s, 0x44); }],
      });
      // two palettes and their tables: P0 t1[i] = 0x8000 | i * 0x41, t2[i] = 0x0400 | i * 0x83; P1 t1 = i * 0x105,
      // t2 = 0xf000 ^ i * 0x11
      const tabs = blk.add(0x800);
      const tw = function (t, f) { for (let i = 0; i < 256; i++) t.add(2 * i).writeU16(f(i) & 0xffff); };
      tw(tabs, function (i) { return 0x8000 | Math.imul(i, 0x41); });
      tw(tabs.add(0x200), function (i) { return 0x0400 | Math.imul(i, 0x83); });
      tw(tabs.add(0x400), function (i) { return Math.imul(i, 0x105); });
      tw(tabs.add(0x600), function (i) { return 0xf000 ^ Math.imul(i, 0x11); });
      const p0 = blk.add(0x600), p1 = blk.add(0x680), g = blk.add(0x700), g0 = blk.add(0x740);
      p0.writePointer(pvt); p0.add(0x40).writePointer(tabs); p0.add(0x44).writePointer(tabs.add(0x200));
      p1.writePointer(pvt); p1.add(0x40).writePointer(tabs.add(0x400)); p1.add(0x44).writePointer(tabs.add(0x600));
      g.writeU32(0x6a000000); g.add(4).writePointer(p1);
      g0.writeU32(0x6a000001); g0.add(4).writeU32(0);
      const SURF = [  // [palette, format, lock, clip]
        [p0, 0, 1, [4, 3, 40, 28]], [p0, 1, 1, [4, 3, 40, 28]], [p0, 2, 1, [4, 3, 40, 28]],
        [null, 0, 1, [4, 3, 40, 28]], [p0, 0, 0, [4, 3, 40, 28]], [p0, 0, 1, [0, 0, DL_W, DL_H]],
      ];
      const pix0 = blk.add(0x100 * 8 + 0x400 * 2);
      SURF.forEach(function (d, i) {
        const s = blk.add(0x100 * i);
        for (let k = 4; k < 0x100; k += 4) s.add(k).writeU32((0x5c000000 | (i << 12) | k) >>> 0);
        s.writePointer(svt);
        const pix = pix0.add(PIX * i);
        fill(pix, PIX, 0xee);
        s.add(0x40).writePointer(pix); s.add(0x44).writeU32(DL_W);
        d[3].forEach(function (v, k) { s.add(0x50 + 4 * k).writeS32(v); });
        s.add(0x60).writeU32(d[2]); s.add(0x68).writeU32(d[1]);
        s.add(0x7c).writePointer(d[0] === null ? ptr(0) : d[0]);
        out['S' + i] = s;
      });
      FM.stand({ addr: u32(jva(0x100012b2)), abi: 'thiscall', nargs: 8, ret: 0x7e000000 });   // -> dashedVLine16
      FM.stand({ addr: u32(jva(0x100016b3)), abi: 'thiscall', nargs: 8, ret: 0x7d000000 });   // -> dashedHLine16
      freezeNow();
      if (gmode) {
        const ga = jva(0x1012873c);
        __freeze.save(ga, 4);
        ga.writePointer(gmode === 'pal' ? g : gmode === 'nullpal' ? g0 : ptr(0));
      }
      return out;
    };
  }
  F.c3fm_dl16 = dl16Fx(null);
  F.c3fm_dl16_gpal = dl16Fx('pal');
  F.c3fm_dl16_gnullpal = dl16Fx('nullpal');
  F.c3fm_dl16_gnone = dl16Fx('none');

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
