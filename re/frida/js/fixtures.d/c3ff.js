// Fixtures for batch c3ff (round 33, jgld.dll): saveImage / writePcx8, createDisplay, drawTo / drawToZ, loadPng.
// Reimplementations: shim/src/re/c3ff.cpp. Registry: re/frida/registry.d/c3ff.py. Purposes and per-jcc tables:
// log/c3/c3ff_purpose.md.
//
// Every key freezes the game's first thread (re/frida/js/freeze.js) and arms the negative-control switch after it
// (__freeze.negOn(): only the negctl build exports one). Objects the bodies dispatch through are the fixture's own:
// Surfaces, palettes, the palette client and loadPng's buffer object carry RECORDING vtables (one NativeCallback per
// slot; every call appends {slot, this, arguments} to $vlog, a state region, and answers from the object's own
// fields). A frame pointer handed to a recorder is logged through framemask.js (FM.map), so the two arms, whose frames
// differ, log the same token. Shared helpers used: framemask.js (drawTo / drawToZ's frame Sprite), filestandin.js
// (MappedFile's KERNEL32 imports for writePcx8, jgld's fopen / fread / fclose for loadPng), heapstandin.js
// (createDisplay's operator new, libpng's malloc / free), importstandin.js (OutputDebugStringA, WriteFile).
//
// jgld.dll is relocated at run time, so every module address is built from the running base.
(function () {
  const F = {};
  const u32 = (v) => (typeof v === 'number' ? v : v.toUInt32()) >>> 0;
  const zero = (p, n) => p.writeByteArray(new Uint8Array(n).buffer);
  function alloc(n) { const p = __keepAlloc(n); zero(p, n); return p; }
  const jgld = () => Process.getModuleByName('jgld.dll').base;
  const jva = (a) => jgld().add(a - 0x10000000);
  function cstr(s) { const p = alloc(s.length + 1); p.writeUtf8String(s); return p; }

  const SBLK = 0x8000, VLOG = 0x2000, FMLOG = 0x1000;    // c3ff.py

  let frozen = false;
  function freezeNow() {
    if (frozen) return;
    __freeze.thread();
    frozen = true;
    __freeze.atFlush(function () { frozen = false; });
    __freeze.negOn();             // every exported <Batch>NegCtl switch (only the negative-control build has one)
  }

  const CRCT = (function () {
    const t = [];
    for (let n = 0; n < 256; n++) {
      let c = n;
      for (let k = 0; k < 8; k++) c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
      t.push(c >>> 0);
    }
    return t;
  })();
  function crc32(arr) { let c = 0xffffffff; for (let i = 0; i < arr.length; i++) c = CRCT[(c ^ arr[i]) & 0xff] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; }
  function crcStr(s) { const a = []; for (let i = 0; i < s.length; i++) a.push(s.charCodeAt(i) & 0xff); return crc32(a); }

  // ---------------------------------------------------------------------------------------------------- recorders
  // $vlog: +0 records, +4 bytes used, +8 FNV-1a over every record (so an overflow is still compared), +0xc records not
  // stored, records {slot, n, words...} from +0x10.
  function fnv(h, w) { for (let k = 0; k < 4; k++) { h ^= (w >>> (8 * k)) & 0xff; h = Math.imul(h, 0x01000193) >>> 0; } return h >>> 0; }
  function recorder(FM) {
    const log = alloc(VLOG);
    log.add(8).writeU32(0x811c9dc5);
    const map = (v) => FM.map(u32(v));
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
    // a vtable of 64 thiscall slots; `slots` {offset: [nargs, body(self, args) -> ret | {r, extra}]}; every other slot
    // records 0xdead0000 | offset and returns 0x0badc0de
    function vtable(tag, slots) {
      const vt = alloc(4 * 64);
      for (let i = 0; i < 64; i++) {
        const off = 4 * i, s = slots[off];
        const nargs = s ? s[0] : 0;
        const cb = new NativeCallback(function () {
          const a = Array.prototype.slice.call(arguments).map(u32);
          const self = a[0], args = a.slice(1);
          if (!s) { rec([(0xdead0000 | off) >>> 0, map(self)]); return 0x0badc0de; }
          const out = s[1](self, args);
          const r = (typeof out === 'object' && out !== null) ? out.r : out;
          const extra = (typeof out === 'object' && out !== null && out.extra) ? out.extra : [];
          rec([(tag << 16 | off) >>> 0, map(self)].concat(args.map(map), extra.map(map)));
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

  // Synthetic Surface fields (block of 0x600 bytes each): +0x24 depth, +0x40, +0x7c palette, +0x80 buffer object,
  // +0x4d0; the recording slots answer from +0x500 fwdVfn18 pixels, +0x504 acquireDC2, +0x508 width, +0x50c height,
  // +0x510 getField40, +0x514 the format pair getDepthPtr returns the address of, +0x518 / +0x51c pixelAddr's base and
  // stride. Offsets: Surface vtable 0x1011d0b0 (re/analysis/systems/jgld_2.md).
  const SURF = 0x600;
  function surfaceVtable(R) {
    return R.vtable(0x5f, {
      0x0c: [2, function (s, a) { return (rd(s, 0x518) + Math.imul(a[1], rd(s, 0x51c)) + a[0]) >>> 0; }],   // pixelAddr
      0x10: [0, function (s) { return rd(s, 0x504); }],                     // acquireDC2
      0x18: [0, function (s) { return rd(s, 0x500); }],                     // fwdVfn18
      0x24: [1, function () { return 0; }],                                  // releaseDC2(flag)
      0x44: [2, function () { return 0; }],                                  // fillRectDispatch
      0xd8: [0, function (s) { return rd(s, 0x508); }],                     // width
      0xdc: [0, function (s) { return rd(s, 0x50c); }],                     // height
      0xe0: [0, function (s) { return rd(s, 0x510); }],                     // getField40
      0xe4: [0, function (s) { return (s + 0x514) >>> 0; }],                // getDepthPtr
      0xe8: [0, function (s) { return rd(s, 0x7c); }],                      // getField7c
      0xec: [1, function () { return 0; }],                                  // setPalette
    });
  }
  function surface(blk, vt, i, f) {
    const s = blk.add(SURF * i);
    s.writePointer(vt);
    const put = (o, v) => s.add(o).writeU32(u32(v));
    put(0x24, f.depth === undefined ? 8 : f.depth); put(0x40, f.f40 || 0); put(0x7c, f.pal || 0); put(0x80, f.buf || 0);
    put(0x4d0, f.f4d0 || 0); put(0x500, f.pix || 0); put(0x504, f.dc2 || 0); put(0x508, f.w || 0); put(0x50c, f.h || 0);
    put(0x510, f.pitch || 0); put(0x514, f.fmt === undefined ? 8 : f.fmt); put(0x518, f.base || 0);
    put(0x51c, f.stride || 0);
    return s;
  }

  // ======================================================================================= group F: writePcx8 / saveImage
  // Pixel rows (row-major, `pitch` bytes each) and the expected walk (writePcx8 0x10012140): runs of equal bytes up to
  // 0x3f long, never reaching the row's last byte (x + run < pitch - 1), single bytes below and from 0x40.
  const PIX = {
    S0: [5, 5, 5, 0x40, 0x41, 0x41, 7, 7, 0x80, 1, 2, 3, 3, 3, 3, 0xff, 9, 9, 9, 9, 9, 9, 9, 9],
    S1: Array(70).fill(0x22).concat(Array(70).fill(0xc5)),
    S2: [0, 1, 2, 3, 4, 0xe0, 0xe0, 0x3f, 0x40, 0x40, 0x40, 0x40, 0x40, 1, 1, 2, 6, 6, 6, 6, 6, 6, 6, 0x41,
         0x10, 0x20, 0x10, 0x20, 0x10, 0x20, 0x10, 0x20],
    S4: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],
    S6: Array(16).fill(0x10).concat(Array(16).fill(0x20), Array(16).fill(0x30), Array(15).fill(0x40), [0x41]),
    S7: [0x41, 0x41, 0x41],
    S8: [0x99],
    S9: [1, 2, 3, 4, 5, 6, 7, 8],
    S12: [0x7f, 0x7f, 0x00, 0x00, 0x00, 0xc0, 0x3e, 0x3e, 0x3e, 0x3e],
  };
  // [w, h, pitch, pixels, palette, depth]
  const SDEF = {
    S0: [8, 3, 8, 'S0', 'P0', 8], S1: [70, 2, 70, 'S1', 'P1', 8], S2: [5, 4, 8, 'S2', null, 8],
    S3: [4, 0, 4, 'S0', 'P0', 8], S4: [6, 2, 0, 'S4', null, 8], S5: [4, 4, 4, null, 'P0', 8],
    S6: [16, 4, 16, 'S6', 'P2', 8], S7: [3, 1, 3, 'S7', 'P0', 8], S8: [1, 1, 1, 'S8', 'P1', 8],
    S9: [4, 2, 4, 'S9', 'P0', 16], S10: [4, 2, 4, 'S9', 'P0', 0], S11: [4, 2, 4, 'S9', 'P0', 24],
    S12: [5, 2, 5, 'S12', 'P2', 8],
  };
  const NAMES = ['img1.bmp', 'a.b.c', 'noext', '.x', 'dir\\pic.raw', 'trail.', 'missing.bmp', ''];
  // writePcx8's name rule (0x100121bd..0x1001220f): cut at the last '.' found walking back from the terminator while
  // above the first character, and append the extension; a name with no such '.' is kept whole
  function pcxName(n, ext) {
    let p = n.length;
    while (p > 0 && n.charAt(p) !== '.') p--;
    return n.charAt(p) === '.' ? n.slice(0, p) + ext : n;
  }
  // palettes: +4 the slot +0x10 result, +8 the seed of the 768 bytes it writes
  function palVtable(R) {
    return R.vtable(0x9a, {
      0x10: [3, function (s, a) {
        const seed = rd(s, 8), buf = ptr(a[0]), n = Math.min(a[2], 0x100);
        const bytes = new Uint8Array(3 * n);
        for (let i = 0; i < 3 * n; i++) bytes[i] = (seed + i * 7 + (i >> 3)) & 0xff;
        if (n > 0) buf.writeByteArray(bytes.buffer);
        return rd(s, 4);
      }],
    });
  }

  function fileFx(client) {
    return function () {
      const FM = __frameMask.begin({ size: FMLOG });
      const R = recorder(FM);
      const blk = alloc(SBLK);
      const svt = surfaceVtable(R), pvt = palVtable(R);
      const out = { sblk: blk, vlog: R.log, fmlog: FM.block() };
      const pals = {};
      [['P0', 0, 0x11], ['P1', 1, 0x37], ['P2', 0, 0x5a], ['P3', 0, 0xa3]].forEach(function (d, i) {
        const p = blk.add(0x6000 + 0x40 * i);
        p.writePointer(pvt); p.add(4).writeU32(d[1]); p.add(8).writeU32(d[2]);
        pals[d[0]] = p;
      });
      const cl = blk.add(0x6100);                       // the palette client: its +4 is the palette writePcx8 reads
      cl.writeU32(0xc11e0000); cl.add(4).writePointer(client ? pals.P3 : ptr(0));
      let pixOff = 0x6400;
      Object.keys(SDEF).forEach(function (k, i) {
        const d = SDEF[k];
        let pix = 0;
        if (d[3]) {
          pix = blk.add(pixOff);
          pix.writeByteArray(PIX[d[3]]);
          pixOff = (pixOff + PIX[d[3]].length + 15) & ~15;
        }
        out[k] = surface(blk, svt, i, { w: d[0], h: d[1], pitch: d[2], pix: pix, pal: d[4] ? pals[d[4]] : 0,
                                        depth: d[5], fmt: d[5] });
      });
      const ext = jva(0x1011d354).readCString();
      const files = {};
      NAMES.forEach(function (n, i) {
        out['N' + i] = cstr(n);
        if (n !== '' && n !== 'missing.bmp') files[pcxName(n, ext)] = { bytes: null, capacity: 0x800 };
      });
      freezeNow();
      const gcl = jva(0x1012873c);
      __freeze.save(gcl, 4);
      gcl.writePointer(cl);
      let fs = null;
      // SetEndOfFile: the file's length becomes the handle's cursor (filestandin.js block layout: handles at +0x640,
      // 0x10 each {kind, file index, cursor, open}; files at +0x40, 0x20 each, length at +4)
      const setEnd = { args: 1, impl: function (a, ctx) {
        const b = fs.block, i = (u32(a[0]) - 0x46480000) | 0;
        ctx.record([u32(a[0])]);
        if (i < 0 || i >= 8 || !b.add(0x640 + 0x10 * i + 0xc).readU32()) return 0;
        const fi = b.add(0x640 + 0x10 * i + 4).readU32();
        b.add(0x40 + 0x20 * fi + 4).writeU32(b.add(0x640 + 0x10 * i + 8).readU32());
        return 1;
      } };
      fs = __fileStandIn({ module: 'jgld.dll', kernel32: true, files: files, size: 0x9000,
                           k32Imports: ['CreateFileA', 'GetFileSize', 'CreateFileMappingA', 'MapViewOfFile',
                                        'UnmapViewOfFile', 'CloseHandle', 'GetLastError', 'SetFilePointer'],
                           imports: { 'KERNEL32.dll!SetEndOfFile': setEnd } });
      out.fsblock = fs.block;
      out.implog = fs.implog;
      return out;
    };
  }
  F.c3ff_file_client = fileFx(true);
  F.c3ff_file_clientnull = fileFx(false);

  // ======================================================================================= group D: createDisplay
  // operator new 0x1007f0f0 / delete 0x1007e6d0 through the heap stand-in (arena 0x800). Vector rN's getter sets the
  // arena cursor and fail mask before the snapshot; the three globals are left at the game's values (saved under the
  // freeze; each arm's writes are undone by diff_hook's restore and again by the freeze flush).
  const DCUR = [0, 0x10, 0x150, 0x40, 0, 0x300, 0x600, 0x20];
  const DFAIL = [0, 0, 0, 1, 1, 0, 0, 1];
  F.c3ff_display = function () {
    freezeNow();
    const h = __heapStandIn({ module: 'jgld.dll', alloc: [0x7f0f0], free: [0x7e6d0], arenaSize: 0x800, fill: 0xcd });
    const out = { arena: h.arena, freelog: h.log, obj: h.arena, g70c: jva(0x1012870c), g420: jva(0x10128420),
                  g728: jva(0x10128728) };
    __freeze.save(out.g70c, 4); __freeze.save(out.g420, 4); __freeze.save(out.g728, 4);
    for (let i = 0; i < DCUR.length; i++) {
      Object.defineProperty(out, 'r' + i, { get: (function (k) {
        return function () { h.arena.writeU32(DCUR[k]); h.arena.add(8).writeU32(DFAIL[k]); return k; };
      })(i) });
    }
    return out;
  };

  // ======================================================================================= group S: drawTo / drawToZ
  // framemask.js stands in Sprite::ctorN 0x10014cd0 (declares the frame Sprite, 0x50 bytes, and fills it with object
  // tokens), Sprite::drawB 0x10015480 / Sprite::drawG 0x100165a0 and Sprite::dtor 0x10014de0 (each logs the Sprite's 20
  // words). setField28 0x1000aeb0 runs for real. The draw stand-ins answer from their arguments and call index.
  F.c3ff_draw = function () {
    const FM = __frameMask.begin({ size: FMLOG });
    const R = recorder(FM);
    const blk = alloc(SBLK);
    const svt = surfaceVtable(R);
    const va = (a) => u32(jva(a));
    FM.stand({ addr: va(0x10014cd0), abi: 'thiscall', nargs: 1, region: { arg: 'this', size: 0x50 },
               fill: { arg: 'this', words: 0x14 }, ret: 'this' });
    const drawRet = function (self, a, n) { return (Math.imul(a[1], 3) + a[2] + Math.imul(n, 0x100) + (a[a.length - 1] & 0xff0)) >>> 0; };
    FM.stand({ addr: va(0x10015480), abi: 'thiscall', nargs: 4, deref: { 'this': 0x14 }, ret: drawRet });
    FM.stand({ addr: va(0x100165a0), abi: 'thiscall', nargs: 5, deref: { 'this': 0x14 }, ret: drawRet });
    FM.stand({ addr: va(0x10014de0), abi: 'thiscall', nargs: 0, deref: { 'this': 0x14 }, retire: 'this', ret: 0 });
    const out = { sblk: blk, vlog: R.log, fmlog: FM.block() };
    // [depth +0x24, +0x40, palette +0x7c, +0x4d0, acquireDC2, width, height]
    const T = [[8, 0x40, 0x9a1e0000, 0x4d000000, 0x00d20000, 64, 48], [16, 0x80, 0, 0x4d000001, 0x00d20001, 1, 1],
               [8, 0, 0x9a1e0002, 0, 0, 0, 0], [24, 0x7fffffff, 0x9a1e0003, 0x4d000003, 0x00d20003, 800, 600],
               [0, 0x10, 0, 0x4d000004, 0x00d20004, -1, -2], [16, 0x2c, 0x9a1e0005, 0xffffffff, 0x00d20005, 3, 300]];
    T.forEach(function (t, i) {
      out['T' + i] = surface(blk, svt, i, { depth: t[0], f40: t[1], pal: t[2], f4d0: t[3], dc2: t[4], w: t[5], h: t[6] });
    });
    freezeNow();
    return out;
  };

  // ======================================================================================= group P: loadPng
  function be32(v) { return [(v >>> 24) & 0xff, (v >>> 16) & 0xff, (v >>> 8) & 0xff, v & 0xff]; }
  function chunk(type, data, bad) {
    const t = Array.from(type, function (c) { return c.charCodeAt(0); });
    const c = (crc32(t.concat(data)) ^ (bad ? 1 : 0)) >>> 0;
    return be32(data.length).concat(t, data, be32(c));
  }
  function adler(arr) { let a = 1, b = 0; for (let i = 0; i < arr.length; i++) { a = (a + arr[i]) % 65521; b = (b + a) % 65521; } return ((b << 16) | a) >>> 0; }
  // a zlib stream of stored deflate blocks (header 0x78 0x01, BFINAL on the last block, then the Adler-32)
  function zstored(raw, badHeader) {
    const out = [0x78, badHeader ? 0x02 : 0x01];
    let i = 0;
    do {
      const n = Math.min(0xffff, raw.length - i), fin = i + n >= raw.length ? 1 : 0;
      out.push(fin, n & 0xff, n >>> 8, (~n) & 0xff, ((~n) >>> 8) & 0xff);
      for (let k = 0; k < n; k++) out.push(raw[i + k]);
      i += n;
    } while (i < raw.length);
    return out.concat(be32(adler(raw)));
  }
  const ADAM7 = [[0, 0, 8, 8], [4, 0, 8, 8], [0, 4, 4, 8], [2, 0, 4, 4], [0, 2, 2, 4], [1, 0, 2, 2], [0, 1, 1, 2]];
  const CHANNELS = { 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 };
  // the filtered scanlines of a w x h image (filter byte row % 5, then arbitrary bytes), Adam7 sub-images if interlaced
  function scanlines(w, h, ctype, depth, ilace, seed) {
    const bpp = CHANNELS[ctype] * depth / 8, out = [];
    let r = 0;
    (ilace ? ADAM7 : [[0, 0, 1, 1]]).forEach(function (p) {
      const pw = w > p[0] ? Math.ceil((w - p[0]) / p[2]) : 0, ph = h > p[1] ? Math.ceil((h - p[1]) / p[3]) : 0;
      if (!pw || !ph) return;
      for (let y = 0; y < ph; y++, r++) {
        out.push(r % 5);
        for (let k = 0; k < pw * bpp; k++) out.push((seed * 31 + r * 17 + k * 7 + (k >> 2)) & 0xff);
      }
    });
    return out;
  }
  const SIG = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];
  function pngFile(o) {
    const ihdr = be32(o.w).concat(be32(o.h), [o.depth, o.ctype, 0, 0, o.ilace ? 1 : 0]);
    let f = (o.sig || SIG).concat(chunk('IHDR', ihdr));
    (o.pre || []).forEach(function (c) { f = f.concat(chunk(c[0], c[1], c[2])); });
    const z = zstored(scanlines(o.w, o.h, o.ctype, o.depth, o.ilace, o.seed), o.badz);
    f = f.concat(chunk('IDAT', z, o.badcrc), chunk('IEND', []));
    if (o.cut) f = f.slice(0, o.cut(f.length, z.length));
    return f;
  }
  const PNGS = [
    ['rgb8.png', { w: 4, h: 3, depth: 8, ctype: 2, seed: 1 }],
    ['rgb16.png', { w: 3, h: 2, depth: 16, ctype: 2, seed: 2 }],
    ['ilace.png', { w: 5, h: 5, depth: 8, ctype: 2, ilace: true, seed: 3 }],
    ['gray.png', { w: 4, h: 2, depth: 8, ctype: 0, seed: 4 }],
    ['rgba.png', { w: 2, h: 2, depth: 8, ctype: 6, seed: 5 }],
    ['pal.png', { w: 2, h: 2, depth: 8, ctype: 3, seed: 6, pre: [['PLTE', [1, 2, 3, 4, 5, 6, 7, 8, 9]]] }],
    ['badsig.png', { w: 2, h: 2, depth: 8, ctype: 2, seed: 7, sig: [0x88, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a] }],
    ['trunc.png', { w: 4, h: 3, depth: 8, ctype: 2, seed: 8, cut: function (n, zl) { return n - 12 - 4 - Math.floor(zl / 2); } }],
    ['badz.png', { w: 2, h: 2, depth: 8, ctype: 2, seed: 9, badz: true }],
    ['badcrc.png', { w: 2, h: 2, depth: 8, ctype: 2, seed: 10, badcrc: true }],
    ['warn.png', { w: 3, h: 2, depth: 8, ctype: 2, seed: 11, pre: [['abCd', [1, 2, 3, 4], true]] }],
    ['crit.png', { w: 2, h: 2, depth: 8, ctype: 2, seed: 12, pre: [['ABCD', [9, 9]]] }],
  ];
  // jgld's stderr 0x101233f0 is buffered and gets its 0x1000-byte buffer on the first fprintf (c3dn, c3ey): primed once
  function primeStderr() {
    const giob = jva(0x101233f0);
    if (giob.add(8).readPointer().isNull()) {
      const fpf = new NativeFunction(jva(0x10083010), 'int', ['pointer', 'pointer'], 'mscdecl');
      fpf(giob, cstr('\n'));
    }
    if (giob.add(8).readPointer().isNull() || giob.add(0x18).readU32() !== 0x1000)
      throw new Error('c3ff: jgld stderr not buffered as c3dn probed');
    return { giob: giob, gbuf: giob.add(8).readPointer() };
  }
  const WRITEFILE = { args: 5, impl: function (a, ctx) {
    const n = u32(a[2]);
    ctx.record([u32(a[0]), n]);
    if (!a[3].isNull()) a[3].writeU32(n);
    return 1;
  } };
  // trace 0x10014550's OutputDebugStringA: the message is logged as its crc32 and length (chunk messages are built in
  // libpng's frame buffers, whose addresses differ between the arms)
  const ODS = { args: 1, impl: function (a, ctx) {
    const s = a[0].isNull() ? '' : (a[0].readCString() || '');
    ctx.record([crcStr(s), s.length]);
    return 0;
  } };
  const ARENA = 0x20000;
  F.c3ff_png = function () {
    const FM = __frameMask.begin({ size: FMLOG });
    const R = recorder(FM);
    const blk = alloc(SBLK);
    const svt = surfaceVtable(R);
    const se = primeStderr();
    const out = { sblk: blk, vlog: R.log, fmlog: FM.block(), giob: se.giob, gbuf: se.gbuf };
    // buffer objects at +0x80 (slot +4 (w, h, depth)) and the palette argument (slot +0x14 (p, a3, a4), p's first three
    // words logged: the palette arm passes the address of its loop counter)
    const bvt = R.vtable(0xbf, { 0x04: [3, function (s, a) { return (0x4b1d0000 + a[2]) >>> 0; }] });
    const pvt = R.vtable(0x9b, { 0x14: [3, function (s, a) {
      return { r: 0, extra: [0, 4, 8].map(function (o) { return ptr(a[0]).add(o).readU32(); }) };
    }] });
    const bufs = [blk.add(0x6000), blk.add(0x6040)];
    bufs.forEach(function (b, i) { b.writePointer(bvt); b.add(4).writeU32(0xb0f00000 + i); });
    out.P = blk.add(0x6080); out.P.writePointer(pvt);
    [[0x6400, 0], [0x6c00, 1], [0x7400, 0]].forEach(function (u, i) {
      out['U' + i] = surface(blk, svt, i, { base: blk.add(u[0]), stride: 0x60, buf: bufs[u[1]], depth: 24 });
    });
    const files = {};
    PNGS.forEach(function (p, i) { files[p[0]] = pngFile(p[1]); out['F' + i] = cstr(p[0]); });
    out.F12 = cstr('missing.png');
    freezeNow();
    const h = __heapStandIn({ module: 'jgld.dll', alloc: [0x7f0f0, 0x7fb40, { rva: 0x7ffa0, args: 2, sizeArgs: 2, fill: 0 }],
                              free: [0x7e6d0, 0x806d0], arenaSize: ARENA, fill: 0xcd });
    out.arena = h.arena; out.freelog = h.log;
    const tbl = __fileStandIn.CRT['jgld.dll'] || (__fileStandIn.CRT['jgld.dll'] = {});
    tbl.fopen = u32(jva(0x1007faa0)); tbl.fread = u32(jva(0x10082d80)); tbl.fclose = u32(jva(0x1007f850));
    const fs = __fileStandIn({ module: 'jgld.dll', crt: ['fopen', 'fread', 'fclose'], files: files, size: 0x4000,
                               imports: { 'KERNEL32.dll!WriteFile': WRITEFILE, 'KERNEL32.dll!OutputDebugStringA': ODS },
                               implogSize: 0x800 });
    out.fsblock = fs.block;
    out.implog = fs.implog;
    for (let k = 0; k < 12; k++) {
      Object.defineProperty(out, 'f' + k, { get: (function (kk) {
        return function () { h.arena.add(8).writeU32((1 << kk) >>> 0); return kk; };
      })(k) });
    }
    return out;
  };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
