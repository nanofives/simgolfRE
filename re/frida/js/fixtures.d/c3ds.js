// Fixtures for batch c3ds (round 28, jgld.dll). Reimplementations: shim/src/re/c3ds.cpp. Registry:
// re/frida/registry.d/c3ds.py. Purposes and branch tables: log/c3/c3ds_purpose.md.
//
// THE LONGJMP CATCHER. libpng 1.0.5 reports a fatal error with png_error 0x10078fe0, which never returns: it calls the
// caller's error function at png_ptr + 0x40 and then png_default_error 0x100792b0, which longjmps through the jmp_buf
// at png_ptr + 0 (longjmp 0x10082f90, value 1, at 0x100792e4). The png_structs here are the fixture's, so their jmp_buf
// is too: for the key's function, an Interceptor listener on BOTH the hook's trampoline (the original arm) and its
// detour (the reimplementation arm) fills the jmp_buf of the png_struct passed as the first argument at the moment
// the A/B harness enters the function, with exactly what VC6's setjmp would have saved had the harness called setjmp
// there: ebp, ebx, edi, esi, the entry esp, the return address, fs:[0], trylevel -1, the cookie 'VC20' and a NULL
// unwind function. jgld's longjmp (0x10082f90..0x10083005) then compares the saved registration with fs:[0] (equal:
// no unwind, 0x10082fa0), finds the cookie with a NULL unwind function (0x10082fcc), restores ebx/edi/esi/ebp, sets esp
// to the saved esp + 4 and jumps to the return address with eax = 1 (`cmp eax, 1; adc eax, 0`): to the harness that
// is the function returning 1. Only calls whose return address lies outside jgld.dll and winmm.dll (the shim) fill
// the jmp_buf, so a nested call of the same function never moves the catch point, and only png_structs of the key's
// own fixture are touched. The jmp_buf (png_ptr + 0 .. + 0x3f) is never compared state: the registers and the
// return address are the harness's. The listeners detach from setTimeout(0), after the key.
//
// THE ERROR AND WARNING FUNCTIONS are the fixture's: a recorder of the message TEXT (the bytes up to the terminator,
// so a stack buffer's garbage after it never enters the log) into $hlog, then either a longjmp through png_ptr's
// jmp_buf (mode 'jump': a caller's error function that never returns, as libpng requires), or a return (mode 'ret':
// png_default_error then runs), or no function at all (mode 'null'). The default arms print to jgld's stderr
// 0x101233f0: as c3dn found, that stream is buffered and gets its buffer on the first fprintf, so it is primed once
// here, its FILE and buffer are compared state, and WriteFile is stood in.

(function () {
  const jgld = () => Process.getModuleByName('jgld.dll').base;
  const va = (a) => jgld().add(a - 0x10000000);
  const zero = (p, n) => p.writeByteArray(new Array(n).fill(0));
  const fill = (p, n, seed) => { for (let i = 0; i + 4 <= n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };
  const u32 = (v) => (typeof v === 'number' ? v : v.toUInt32()) >>> 0;
  const le = (v) => { const a = []; for (let i = 0; i < 4; i++) a.push((v >>> (8 * i)) & 0xff); return a; };
  const addrBytes = (p) => le(u32(p));

  // freeze the game's first thread at most once per key (the negative-control flag below may need it too)
  let frozenKey = false;
  function freezeOnce() { if (!frozenKey) { __freeze.thread(); frozenKey = true; } }

  // one rwx page per stub
  function code(bytes) {
    const page = Process.pageSize;
    const p = __keepAlloc(page);
    Memory.protect(p, page, 'rwx');
    p.writeByteArray(bytes);
    return p;
  }
  function cblock(s, size) {
    const p = __keepAlloc(size || 0x80);
    zero(p, size || 0x80);
    p.writeUtf8String(s);
    return p;
  }
  function bytes(p, arr) { if (arr.length) p.writeByteArray(arr); return arr.length; }

  // C string -> dwords, zero after the terminator (c3dk's recorder)
  function strWords(p, max) {
    const out = [];
    if (p.isNull()) return [0];
    const u = new Uint8Array(p.readByteArray(4 * max));
    let end = false;
    for (let i = 0; i < max && !end; i++) {
      let w = 0;
      for (let k = 0; k < 4; k++) {
        const c = end ? 0 : u[4 * i + k];
        if (c === 0) end = true;
        w |= c << (8 * k);
      }
      out.push(w >>> 0);
    }
    return out;
  }

  // zlib's crc32 (jgld's 0x1009ca70 is zlib 1.0.2's), for the chunk CRCs the readers check
  const CRC = (function () {
    const t = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
      let c = n;
      for (let k = 0; k < 8; k++) c = c & 1 ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
      t[n] = c >>> 0;
    }
    return t;
  })();
  function crc32(crc, arr) {
    let c = (crc ^ 0xffffffff) >>> 0;
    for (let i = 0; i < arr.length; i++) c = (CRC[(c ^ arr[i]) & 0xff] ^ (c >>> 8)) >>> 0;
    return (c ^ 0xffffffff) >>> 0;
  }
  const be32 = (v) => [(v >>> 24) & 0xff, (v >>> 16) & 0xff, (v >>> 8) & 0xff, v & 0xff];
  const be16 = (v) => [(v >>> 8) & 0xff, v & 0xff];

  // ------------------------------------------------------------------------------------------- the shim's hooks
  function hookOf(rva) {
    const shim = Process.getModuleByName('winmm.dll');
    const pd = Memory.alloc(4), po = Memory.alloc(4), pi = Memory.alloc(4);
    const f = new NativeFunction(shim.getExportByName('SimGolfShim_FindHookM'), 'int',
      ['pointer', 'uint32', 'pointer', 'pointer', 'pointer'], 'stdcall');
    if (!f(Memory.allocUtf8String('jgld.dll'), rva, pd, po, pi)) throw new Error('c3ds: no hook at jgld.dll+' + rva.toString(16));
    // a hook switched off (SIMGOLF_HOOKS_OFF) has no trampoline: the function itself is then the original
    if (pi.readS32() !== 1) return { detour: null, original: jgld().add(rva) };
    return { detour: pd.readPointer(), original: po.readPointer() };
  }

  let fs0fn = null;
  function fs0() {
    if (!fs0fn) fs0fn = new NativeFunction(code([0x64, 0xA1, 0, 0, 0, 0, 0xC3]), 'uint32', []);
    return fs0fn();
  }

  // The catcher (see the header). `rvas`: the key's function (and any other entry the harness calls); `pngs`: the
  // fixture's png_structs (one contiguous block). A NATIVE entry stub, not an Interceptor listener: the first version
  // used Interceptor.attach and the game died now and then on vectors that longjmp past it (log/c3/c3ds_notes.txt);
  // this stub does no Gum invocation bookkeeping, so nothing of Frida's is left behind when a longjmp skips the frame.
  // For each target (the hook's trampoline, its detour, or the function itself when its hook is off) the first five
  // bytes become a jmp to a stub that, when the first argument lies in the fixture's png block and the return address
  // lies outside jgld.dll and winmm.dll, writes the jmp_buf (ebp, ebx, edi, esi, entry esp, return address, fs:[0],
  // -1, 'VC20', 0, zeros) and then runs the target's displaced instructions (X86Relocator) and jumps back. The five
  // bytes are restored from setTimeout(0); an install first restores a leftover patch of the same target.
  const catchLive = {};
  let catchToken = 0;
  function unpatch(k) {
    const e = catchLive[k];
    if (!e) return;
    delete catchLive[k];
    Memory.patchCode(e.at, e.saved.byteLength, function (code) { code.writeByteArray(e.saved); });
  }
  function modRange(name) { const m = Process.getModuleByName(name); return [u32(m.base), u32(m.base.add(m.size))]; }
  function catchStub(target, lo, hi) {
    const page = __keepAlloc(Process.pageSize);
    Memory.protect(page, Process.pageSize, 'rwx');
    const w = new X86Writer(page, { pc: page });
    const jg = modRange('jgld.dll'), wm = modRange('winmm.dll');
    w.putPushReg('eax'); w.putPushReg('ecx'); w.putPushReg('edx');
    w.putMovRegRegOffsetPtr('eax', 'esp', 0x10);                       // png_ptr
    w.putCmpRegI32('eax', lo); w.putJccNearLabel('jb', 'skip', 'no-hint');
    w.putCmpRegI32('eax', hi); w.putJccNearLabel('jae', 'skip', 'no-hint');
    w.putMovRegRegOffsetPtr('ecx', 'esp', 0x0c);                       // return address
    w.putCmpRegI32('ecx', jg[0]); w.putJccNearLabel('jb', 'notj', 'no-hint');
    w.putCmpRegI32('ecx', jg[1]); w.putJccNearLabel('jb', 'skip', 'no-hint');
    w.putLabel('notj');
    w.putCmpRegI32('ecx', wm[0]); w.putJccNearLabel('jb', 'fill', 'no-hint');
    w.putCmpRegI32('ecx', wm[1]); w.putJccNearLabel('jb', 'skip', 'no-hint');
    w.putLabel('fill');
    w.putMovRegOffsetPtrReg('eax', 0, 'ebp');
    w.putMovRegOffsetPtrReg('eax', 4, 'ebx');
    w.putMovRegOffsetPtrReg('eax', 8, 'edi');
    w.putMovRegOffsetPtrReg('eax', 0xc, 'esi');
    w.putLeaRegRegOffset('edx', 'esp', 0xc);                           // the esp the function was entered with
    w.putMovRegOffsetPtrReg('eax', 0x10, 'edx');
    w.putMovRegOffsetPtrReg('eax', 0x14, 'ecx');
    w.putBytes([0x64, 0x8B, 0x15, 0, 0, 0, 0]);                        // mov edx, fs:[0]
    w.putMovRegOffsetPtrReg('eax', 0x18, 'edx');
    w.putMovRegOffsetPtrU32('eax', 0x1c, 0xffffffff);
    w.putMovRegOffsetPtrU32('eax', 0x20, 0x56433230);                 // 'VC20'
    for (let o = 0x24; o < 0x40; o += 4) w.putMovRegOffsetPtrU32('eax', o, 0);
    w.putLabel('skip');
    w.putPopReg('edx'); w.putPopReg('ecx'); w.putPopReg('eax');
    const rl = new X86Relocator(target, w);
    let n = 0;
    while (n < 5) {
      const m = rl.readOne();
      if (m === 0) throw new Error('c3ds catcher: cannot relocate ' + target);
      n = m;
    }
    rl.writeAll();
    w.putJmpAddress(target.add(n));
    w.flush();
    return page;
  }
  function catcher(rvas, pngs) {
    if (!pngs.length) return;
    const lo = u32(pngs[0]), hi = u32(pngs[pngs.length - 1].add(0x220));
    const token = ++catchToken;
    const mine = [];
    rvas.forEach(function (rva) {
      const h = hookOf(rva);
      [h.original, h.detour].forEach(function (at) {
        if (!at) return;
        const k = at.toString();
        unpatch(k);
        const stub = catchStub(at, lo, hi);
        const saved = at.readByteArray(5);
        Memory.patchCode(at, 5, function (code) {
          const pw = new X86Writer(code, { pc: at });
          pw.putJmpAddress(stub);
          pw.flush();
        });
        catchLive[k] = { at: at, saved: saved, token: token };
        mine.push(k);
      });
    });
    setTimeout(function () { mine.forEach(function (k) { if (catchLive[k] && catchLive[k].token === token) unpatch(k); }); }, 0);
  }

  // ------------------------------------------------------------------------------- error / warning recorders
  // $hlog: +0 count, then records of 0x48 bytes: kind (1 error function, 2 warning function), png index, the text
  // as 16 dwords. A record past the eighth only counts.
  function recorders(hlog, pngs) {
    zero(hlog, 0x260);
    const idx = (png) => { for (let i = 0; i < pngs.length; i++) if (pngs[i].equals(png)) return i; return 0xff; };
    const rec = (kind) => new NativeCallback(function (png, msg) {
      const n = hlog.readU32();
      if (n < 8) {
        const r = hlog.add(4 + n * 0x48);
        r.writeU32(kind);
        r.add(4).writeU32(idx(png));
        strWords(msg, 16).forEach((w, i) => r.add(8 + 4 * i).writeU32(w));
      }
      hlog.writeU32(n + 1);
    }, 'void', ['pointer', 'pointer'], 'mscdecl');
    const recErr = rec(1), recWarn = rec(2);
    __diffKeepAlive.push(recErr, recWarn);
    const longjmp = va(0x10082f90);
    // error function, mode 'jump': record, then longjmp (png_ptr->jmpbuf, 1)
    const jump = code([
      0x8B, 0x44, 0x24, 0x08, 0x50,                 // mov eax, [esp+8]; push eax        message
      0x8B, 0x44, 0x24, 0x08, 0x50,                 // mov eax, [esp+8]; push eax        png_ptr
      0xB8].concat(addrBytes(recErr), [0xFF, 0xD0,  // mov eax, recorder; call eax
      0x83, 0xC4, 0x08,                             // add esp, 8
      0x6A, 0x01,                                   // push 1
      0xFF, 0x74, 0x24, 0x08,                       // push dword [esp+8]                png_ptr = &jmpbuf
      0xB8], addrBytes(longjmp), [0xFF, 0xD0,       // mov eax, longjmp; call eax
      0xCC]));
    // error function, mode 'ret': record and return (png_default_error runs next)
    const ret = code([
      0x8B, 0x44, 0x24, 0x08, 0x50, 0x8B, 0x44, 0x24, 0x08, 0x50,
      0xB8].concat(addrBytes(recErr), [0xFF, 0xD0, 0x83, 0xC4, 0x08, 0xC3]));
    return { jump: jump, ret: ret, warn: recWarn };
  }

  // jgld's stderr, primed (c3dn): returns {giob, gbuf}; WriteFile must be stood in by the caller.
  function primeStderr() {
    const giob = va(0x101233f0);
    if (giob.add(8).readPointer().isNull()) {
      const fpf = new NativeFunction(va(0x10083010), 'int', ['pointer', 'pointer'], 'mscdecl');
      fpf(giob, cblock('\n', 8));
    }
    if (giob.add(8).readPointer().isNull() || giob.add(0x18).readU32() !== 0x1000)
      throw new Error('c3ds: stderr stream not buffered as c3dn probed');
    return { giob: giob, gbuf: giob.add(8).readPointer() };
  }
  function writeFileStandIn() {
    return __importStandIn({ module: 'jgld.dll', logSize: 0x1000, imports: {
      'KERNEL32.dll!WriteFile': { args: 5, impl: (a, ctx) => {
        const n = u32(a[2]);
        ctx.record([u32(a[0]), n]);
        if (!a[3].isNull()) a[3].writeU32(n);
        return 1;
      } } } });
  }

  // ------------------------------------------------------------------------------------------ the png world
  // One 0xc000-byte block, patterned, fixed layout (re/frida/registry.d/c3ds.py `_pngs` mirrors it):
  //   +0x0000  up to 24 png_structs of 0x220 bytes (libpng 1.0.5's is 0x21c; offsets in log/c3/c3ds_png_offsets.txt)
  //   +0x3400  up to 24 png_infos of 0xc0 (0xb8 used)
  //   +0x4600  reader control blocks, 0x10 each (cursor, source, calls, last length)
  //   +0x4800  zbufs, 0x40 each
  //   +0x5000  reader sources, 0x400 each (not compared: nothing writes them)
  //   +0xb000  extras a fixture lays out (palettes, text arrays)
  // Every png_struct: the error function per mode at +0x40, the warning recorder at +0x44, c3ch's reader at +0x50
  // over its own source, zbuf +0xac / zbuf_size +0xb0, crc +0x110 = 0 (the CRC bytes a source carries assume it).
  // jgld's stderr is primed and WriteFile stood in for every key (gAMA / sRGB / cHRM print through fprintf).
  const PNG = 0x220, INFO = 0xc0;
  const LAY = { I: 0x3400, C: 0x4600, Z: 0x4800, S: 0x5000, SRC: 0x400, X: 0xb000, END: 0xc000 };
  function reader(ctl) {
    const c = u32(ctl);
    return code([
      0x53, 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x14, 0x8B, 0x4C, 0x24, 0x18,
      0xBB, c & 0xff, (c >>> 8) & 0xff, (c >>> 16) & 0xff, (c >>> 24) & 0xff,
      0x8B, 0x73, 0x04, 0x03, 0x33, 0x01, 0x0B, 0xFF, 0x43, 0x08, 0x89, 0x4B, 0x0C,
      0xF3, 0xA4, 0x5F, 0x5E, 0x5B, 0xC3,
    ]);
  }
  function world(mode, n, seed) {
    if (n > 24) throw new Error('c3ds: at most 24 png_structs per key');
    const s = primeStderr();
    const blk = __keepAlloc(LAY.END);
    fill(blk, LAY.END, seed);
    const out = { obj: blk, blk: blk, n: n, png: [], info: [], ctl: [], src: [], x: blk.add(LAY.X) };
    for (let i = 0; i < n; i++) {
      out.png.push(blk.add(i * PNG));
      out.info.push(blk.add(LAY.I + i * INFO));
      out.ctl.push(blk.add(LAY.C + i * 0x10));
      out.src.push(blk.add(LAY.S + i * LAY.SRC));
    }
    out.hlog = __keepAlloc(0x260);
    const r = recorders(out.hlog, out.png);
    out.ejump = r.jump; out.eret = r.ret; out.ewarn = r.warn;
    out.png.forEach(function (p, i) {
      zero(p, 0x40);
      p.add(0x40).writePointer(mode === 'jump' ? r.jump : mode === 'ret' ? r.ret : ptr(0));
      p.add(0x44).writePointer(r.warn);
      const ctl = out.ctl[i];
      ctl.writeU32(0); ctl.add(4).writePointer(out.src[i]); ctl.add(8).writeU32(0); ctl.add(0xc).writeU32(0);
      p.add(0x50).writePointer(reader(ctl));
      p.add(0xac).writePointer(blk.add(LAY.Z + i * 0x40));
      p.add(0xb0).writeU32(0x40);
      p.add(0x110).writeU32(0);
      out['P' + i] = p;
      out['I' + i] = out.info[i];
    });
    out.giob = s.giob; out.gbuf = s.gbuf;
    out.implog = writeFileStandIn().log;
    return out;
  }

  // A key of chunk-reader vectors. specs[i]: { mode (default 1), flags, ct (colour type +0x126), valid (info +8),
  // data: payload bytes (the vector's length is data.length unless `len`), bad: CRC xored with 0xa5a5a5a5, zsize (zbuf
  // size, <= 0x40), noInfo: info_ptr NULL, set(p, info, out): extra fields }. The source is payload, CRC, then 32
  // patterned bytes (what a png_crc_finish past the chunk reads). heap: options for __heapStandIn (jgld's malloc
  // 0x1007fb40 and free 0x100806d0, the allocator png_malloc 0x10078e60 / png_free 0x10078ed0 reach).
  function chunkFx(rva, name, specs, seed, heap, rows) {
    const out = world('jump', specs.length, seed);
    out.rows = rows;
    specs.forEach(function (s, i) {
      const p = out.png[i], info = out.info[i];
      p.add(0x68).writeU32(s.mode === undefined ? 1 : s.mode);
      p.add(0x6c).writeU32(s.flags || 0);
      p.add(0x126).writeU8(s.ct || 0);
      for (let k = 0; k < 4; k++) p.add(0x11c + k).writeU8(name.charCodeAt(k));
      p.add(0x120).writeU8(0);
      p.add(0xb0).writeU32(s.zsize || 0x40);
      info.add(8).writeU32(s.valid || 0);
      const data = s.data || [];
      let crc = crc32(0, data);
      if (s.bad) crc = (crc ^ 0xa5a5a5a5) >>> 0;
      const src = s.raw ? data.slice() : data.concat(be32(crc));
      for (let k = 0; k < 32; k++) src.push((k * 29 + i * 7 + 3) & 0xff);
      if (src.length > LAY.SRC) throw new Error('c3ds: source too long');
      bytes(out.src[i], src);
      out['L' + i] = s.len === undefined ? data.length : s.len;
      if (s.noInfo) out['I' + i] = ptr(0);
      if (s.set) s.set(p, info, out, i);
    });
    if (heap) {
      const h = __heapStandIn(Object.assign({ module: 'jgld.dll', alloc: [0x7fb40], free: [0x806d0], arenaSize: 0x1000 },
                                            heap));
      out.arena = h.arena; out.freelog = h.log;
    }
    specs.forEach(function (s, i) { if (s.post) s.post(out.png[i], out.info[i], out, i); });
    catcher([rva], out.png);
    return out;
  }
  const str = (s) => { const a = []; for (let i = 0; i < s.length; i++) a.push(s.charCodeAt(i) & 0xff); return a; };
  const pat = (n, k) => { const a = []; for (let i = 0; i < n; i++) a.push((i * 41 + k * 13 + 5) & 0xff); return a; };
  const u32s = (vs) => [].concat.apply([], vs.map(be32));
  const NAME = (s) => [s.charCodeAt(0), s.charCodeAt(1), s.charCodeAt(2), s.charCodeAt(3)];

  // ================================================================== group E: png_error, png_chunk_error, check name
  // Eight png_structs with chunk names of letters, digits and at most three non-letter bytes (png_format_buffer's
  // 80-byte limit, c3dn_notes); twelve messages in zeroed 0x80-byte blocks (png_format_buffer copies 64 bytes).
  const ENAMES = ['IHDR', 'tEXt', 'zTXt', 'gAMA', 'a1B2', 'Q\u0000Zz', 'ÿ\u0001Ab', '[`{@'];
  const EMSGS = ['Missing IHDR before gAMA', '', 'x', 'CRC error', 'invalid chunk type',
                 'a message of exactly forty bytes ......', 'Unknown zTXt compression type', 'Q',
                 'Duplicate', 'error %d', 'tab\tand\nnewline', 'sixty-three characters long message .........................end'];
  function errWorld(mode) {
    const out = world(mode, 8, 0x3d50 + (mode === 'jump' ? 0 : mode === 'ret' ? 1 : 2));
    out.png.forEach(function (p, i) {
      const nm = ENAMES[i];
      for (let k = 0; k < 4; k++) p.add(0x11c + k).writeU8(nm.charCodeAt(k) & 0xff);
      p.add(0x120).writeU8(0);
    });
    EMSGS.forEach(function (m, i) { out['m' + i] = cblock(m, 0x80); });
    // chunk names for png_check_chunk_name: each byte position takes every boundary (0x28/0x29, 0x5a/0x5b, 0x60/0x61,
    // 0x7a/0x7b, 0x00, 0xff)
    const CN = [];
    const bounds = [0x28, 0x29, 0x5a, 0x5b, 0x60, 0x61, 0x7a, 0x7b, 0x00, 0xff, 0x41];
    for (let pos = 0; pos < 4; pos++) bounds.forEach(function (b) { const a = [0x41, 0x62, 0x43, 0x64]; a[pos] = b; CN.push(a); });
    CN.push([0x29, 0x5a, 0x61, 0x7a]);
    CN.push([0x7a, 0x61, 0x5a, 0x29]);
    const nb = __keepAlloc(0x200);
    zero(nb, 0x200);
    CN.forEach(function (a, i) { nb.add(8 * i).writeByteArray(a); out['n' + i] = nb.add(8 * i); });
    out.ncount = CN.length;
    catcher([0x78fe0, 0x790a0, 0x7c1f0], out.png);
    return out;
  }

  // ================================================================== group C: chunk readers (vector specs)
  // Mode bits: IHDR 1, PLTE 2, IDAT 4. Flags: ANCILLARY_USE 0x100, ANCILLARY_NOWARN 0x200 (a bad CRC on an ancillary
  // chunk is then png_chunk_error), CRITICAL_USE 0x400 (a bad CRC on a critical chunk is then a warning). Info valid:
  // gAMA 1, cHRM 4, PLTE 8, tRNS 0x10, bKGD 0x20, hIST 0x40, sRGB 0x800.
  const fl = (p, off, v) => p.add(off).writeFloat(v);
  const G4 = (v) => be32(v);
  const GAMA = [
    { mode: 0, data: G4(45455) },                         // no IHDR: png_error
    { mode: 5, data: G4(45455) },                         // after IDAT: warning, skip
    { mode: 3, data: G4(45455) },                         // after PLTE: warning, then stored
    { valid: 1, data: G4(45455) },                        // duplicate
    { valid: 0x801, data: G4(45455) },                    // gAMA and sRGB valid: not a duplicate, |0| <= 500
    { valid: 0x800, data: G4(50000) },                    // |4545| > 500: warning, fprintf
    { valid: 0x800, data: G4(45955) },                    // |500| not > 500: stored
    { valid: 0x800, data: G4(44954) },                    // |-501| > 500
    { data: [1, 2, 3] },                                  // length 3
    { data: [], noInfo: true },                           // length 0, info NULL
    { data: G4(45455), bad: true },                       // bad CRC: chunk warning
    { data: G4(45455), bad: true, flags: 0x200 },         // bad CRC, NOWARN: png_chunk_error
    { data: G4(0) },                                      // zero gamma
    { data: G4(100000) }, { data: G4(1) }, { data: G4(0xffffffff) }, { data: G4(22727) }, { data: G4(33333) },
    { data: G4(45455), bad: true, flags: 0x300 },         // CRC not checked
    { data: pat(10, 1), zsize: 4 },                       // length 10, png_crc_finish in 4-byte pieces
    { valid: 0x800, data: G4(0xffffffff) },
    { data: G4(0), noInfo: true },                        // zero gamma returns before info is read
  ];
  const REF = [0.3127, 0.329, 0.64, 0.33, 0.3, 0.6, 0.15, 0.06];
  const chrmInfo = (vals) => (p, info) => vals.forEach((v, k) => fl(info, 0x80 + 4 * k, v));
  const off = (k, d) => REF.map((v, j) => (j === k ? v + d : v));
  const gam = (g) => (p) => fl(p, 0x15c, g);
  const both = (g, vals) => (p, info) => { gam(g)(p); chrmInfo(vals)(p, info); };
  const SRGB = [
    { mode: 0, data: [0] }, { mode: 5, data: [0] }, { mode: 3, data: [1] }, { valid: 0x800, data: [0] },
    { data: [0, 1] }, { data: [], noInfo: true }, { data: [0], bad: true }, { data: [0], bad: true, flags: 0x200 },
    { data: [4] }, { data: [0xff], noInfo: true }, { data: [0] }, { data: [2] }, { data: [3] },
    { valid: 1, data: [0], set: gam(0.45455) }, { valid: 1, data: [1], set: gam(0.5) },
    { valid: 1, data: [2], set: gam(0.4596) }, { valid: 1, data: [3], set: gam(0.4495) },
    { valid: 4, data: [0], set: chrmInfo(REF) },
  ].concat([0, 1, 2, 3, 4, 5, 6, 7].map((k) => ({ valid: 4, data: [k & 3], set: chrmInfo(off(k, (k & 1) ? 0.0015 : -0.0015)) })))
   .concat([{ valid: 5, data: [1], set: both(0.5, off(3, 0.01)) }, { valid: 4, data: [2], set: chrmInfo(off(5, 0.0005)) }]);
  const RV = [31270, 32900, 64000, 33000, 30000, 60000, 15000, 6000];
  const rv = (k, v) => RV.map((x, j) => (j === k ? v : x));
  const CHRM = [
    { mode: 0, data: u32s(RV) }, { mode: 5, data: u32s(RV) }, { mode: 3, data: u32s(RV) },
    { valid: 4, data: u32s(RV) },                         // duplicate
    { valid: 0x804, data: u32s(RV) },                     // sRGB valid: the reference points, no warning, not stored
    { valid: 0x800, data: u32s(rv(2, 64200)) },           // sRGB valid, red x off: warning, two fprintf
    { valid: 0x800, data: u32s(rv(7, 5800)) },
    { data: pat(31, 2) }, { data: [], noInfo: true },
    { data: u32s(rv(0, 80000)) },                         // white x 0.8f > 0.8 (double)
    { data: u32s(rv(1, 80001)) }, { data: u32s([50001, 50000].concat(RV.slice(2))) },
    { data: u32s(rv(2, 80000)) }, { data: u32s(rv(3, 80001)) }, { data: u32s([31270, 32900, 50000, 50001].concat(RV.slice(4))) },
    { data: u32s(rv(4, 80001)) }, { data: u32s(rv(5, 80000)) }, { data: u32s(RV.slice(0, 4).concat([60000, 40001], RV.slice(6))) },
    { data: u32s(rv(6, 80000)) },                         // blue x 0.8f not > 0.8f: accepted
    { data: u32s(rv(6, 80001)) }, { data: u32s(rv(7, 80001)) },
    { data: u32s(RV.slice(0, 6).concat([40000, 60000])) },   // blue 0.4f + 0.6f > 1.0f in double
    { data: u32s(RV), bad: true }, { data: u32s(RV), bad: true, flags: 0x200 },
  ];
  const CHRM2 = [
    { data: u32s(RV) }, { data: u32s([1, 2, 3, 4, 5, 6, 7, 8]) }, { data: u32s([33333, 33333, 70000, 29999, 0, 0, 12345, 67890]) },
    { data: u32s([0, 80000, 79999, 20001, 10000, 10000, 80000, 0]) }, { valid: 2, data: u32s(rv(0, 31271)) },
    { data: u32s(RV), zsize: 8 },
    { valid: 0x800, data: u32s(rv(0, 31400)) }, { valid: 0x800, data: u32s(rv(1, 32700)) },
    { valid: 0x800, data: u32s(rv(3, 33200)) }, { valid: 0x800, data: u32s(rv(4, 30200)) },
    { valid: 0x800, data: u32s(rv(5, 59800)) }, { valid: 0x800, data: u32s(rv(6, 15200)) },
  ];
  const pal = (n, k) => pat(3 * n, k);
  const ntr = (v) => (p) => p.add(0x11a).writeU16(v);
  const npal = (v) => (p) => p.add(0x118).writeU16(v);
  const PLTE = [
    { mode: 0, ct: 3, data: pal(2, 1) }, { mode: 5, ct: 3, data: pal(2, 2) }, { mode: 3, ct: 3, data: pal(2, 3) },
    { ct: 2, data: pal(2, 4).slice(0, 5) },               // length % 3, not paletted: warning
    { ct: 3, data: [1, 2, 3, 4] },                        // length % 3, paletted: png_error
    { ct: 3, data: pal(1, 5) },
    { ct: 3, valid: 0x10, data: pal(4, 6), set: ntr(6) }, // num_trans 6 > 4: truncated
    { ct: 3, valid: 0x10, data: pal(4, 7), set: ntr(4) },
    { ct: 3, valid: 0x10, data: pal(16, 8), set: ntr(3) },
    { ct: 3, data: pal(4, 9), set: ntr(9) },              // tRNS not valid
    { ct: 2, valid: 0x10, data: pal(5, 10), set: ntr(9) },
    { ct: 3, data: pal(3, 11), noInfo: true },
    { ct: 3, data: [] },
    { ct: 3, data: pal(2, 12), bad: true, flags: 0x400 }, // critical, CRITICAL_USE: warning
    { ct: 3, data: pal(2, 13), bad: true },               // critical: png_chunk_error
    { ct: 6, data: pal(256, 14) },
    { ct: 0, data: pal(3, 15), zsize: 4 },
  ];
  const TRNS = [
    { mode: 0, ct: 3, data: [1, 2] }, { mode: 5, ct: 3, data: [1, 2] }, { mode: 3, ct: 3, valid: 0x10, data: [1] },
    { ct: 3, data: [9, 8, 7] },                           // no PLTE: warning, then read
    { mode: 3, ct: 3, data: [1, 2, 3], set: npal(2) },    // 3 > 2
    { mode: 3, ct: 3, data: [1, 2, 3], set: npal(4) }, { mode: 3, ct: 3, data: [5, 6, 7], set: npal(3) },
    { mode: 3, ct: 3, data: [], set: npal(4) }, { ct: 3, data: [] },
    { ct: 2, data: [0, 1, 0, 2, 0x80, 3] }, { ct: 2, data: [1, 2, 3, 4, 5] },
    { ct: 0, data: [0xab, 0xcd] }, { ct: 0, data: [1, 2, 3] }, { ct: 4, data: [1, 2] }, { ct: 6, data: [1, 2] },
    { ct: 0, data: [0, 7], noInfo: true },
    { mode: 3, ct: 3, data: [1, 2], bad: true, set: npal(4) },
    { ct: 2, data: pat(6, 3), bad: true, flags: 0x200 },
    { mode: 3, ct: 3, data: pat(16, 4), set: npal(16) },
  ];
  const bkPal = (n) => (p, info, out) => {
    const pl = out.x.add(0x100);
    for (let k = 0; k < 0x60; k++) pl.add(k).writeU8((k * 7 + 1) & 0xff);
    p.add(0x114).writePointer(pl);
    info.add(0x14).writeU16(n);
  };
  const BKGD = [
    { mode: 0, ct: 2, data: pat(6, 1) }, { mode: 5, ct: 2, data: pat(6, 2) }, { ct: 3, data: [1] },
    { mode: 3, ct: 2, valid: 0x20, data: pat(6, 3) }, { mode: 3, ct: 3, data: [1, 2], set: bkPal(4) },
    { mode: 3, ct: 3, data: [2], set: bkPal(0) }, { mode: 3, ct: 3, data: [2], set: bkPal(4) },
    { mode: 3, ct: 3, data: [4], set: bkPal(4) },        // index == num_palette: read
    { mode: 3, ct: 3, data: [5], set: bkPal(4) },        // index > num_palette: warning
    { mode: 3, ct: 3, data: [0], set: bkPal(16) }, { mode: 3, ct: 3, data: [0x1f], set: bkPal(0x1f) },
    { ct: 0, data: [0x12, 0x34] }, { ct: 4, data: [0xfe, 0xdc] }, { ct: 2, data: pat(6, 4) }, { ct: 6, data: pat(6, 5) },
    { ct: 0, data: pat(6, 6) }, { ct: 2, data: [1, 2] }, { ct: 0, data: [0, 9], noInfo: true },
    { ct: 2, data: pat(6, 7), bad: true }, { ct: 2, data: pat(6, 8), bad: true, flags: 0x200 },
  ];
  const HIST = [
    { mode: 0, data: [0, 1] }, { mode: 7, data: [0, 1] }, { mode: 1, data: [0, 1] },
    { mode: 3, valid: 0x40, data: [0, 1], set: npal(1) }, { mode: 3, data: [0, 1], set: npal(2) },
    { mode: 3, data: [0x12, 0x34], set: npal(1) }, { mode: 3, data: pat(8, 1), set: npal(4) },
    { mode: 3, data: [], set: npal(0) }, { mode: 3, data: pat(6, 2), noInfo: true, set: npal(3) },
    { mode: 3, data: pat(4, 3), bad: true, set: npal(2) }, { mode: 3, data: pat(4, 4), bad: true, flags: 0x200, set: npal(2) },
    { mode: 3, data: pat(32, 5), set: npal(16) },
  ];
  const txtInfo = (num, max) => (p, info, out, i) => {
    const arr = out.x.add(0x400 + i * 0x40);
    for (let k = 0; k < 0x40; k += 4) arr.add(k).writeU32((0x7e000000 + i * 0x100 + k) >>> 0);
    info.add(0x30).writeS32(num); info.add(0x34).writeS32(max); info.add(0x38).writePointer(arr);
  };
  const noText = (p, info) => { info.add(0x30).writeS32(0); info.add(0x34).writeS32(0); info.add(0x38).writePointer(ptr(0)); };
  const TEXT = [
    { mode: 0, data: str('k\0v'), set: noText }, { mode: 5, data: str('Title\0Hello'), set: noText },
    { data: str('Author\0Me'), set: noText }, { data: str('keyonly'), set: noText }, { data: [], set: noText },
    { data: str('k\0'), set: noText }, { data: str('\0\0x'), set: noText }, { data: str('a\0b\0c'), set: noText },
    { data: str('Comment\0x'), bad: true, set: noText }, { data: str('Comment\0y'), bad: true, flags: 0x200, set: noText },
    { data: str('k\0v'), noInfo: true }, { data: str('Software\0abc'), set: txtInfo(2, 2) },
    { data: str('Source\0q'), set: txtInfo(1, 9) }, { mode: 13, data: str('Z\0after'), set: txtInfo(3, 3) },
  ];

  // zTXt: payloads from log/c3/c3ds_gen.py (deflate with a 1 KB window). `zinit` (post, i.e. with the heap stand-in
  // installed, so every zlib allocation is an arena block and compared state): the png_struct's z_stream +0x74 gets
  // zalloc +0x94 = png_zalloc 0x100786b0, zfree +0x98 = png_zfree 0x10078750, opaque +0x9c = the png_struct, and
  // jgld's inflateInit2_ 0x1009bf10 (zstream, 10, "1.0.2", 0x38) runs. Without it the z_stream's state +0x90 is
  // NULL, so inflate 0x1009c0f0 returns Z_STREAM_ERROR at once with msg +0x8c as the fixture left it.
  const ZTXT_DATA = {
    short: [40, 207, 243, 72, 205, 201, 201, 87, 168, 10, 137, 40, 1, 0, 19, 253, 3, 175],
    long: [40, 207, 75, 76, 203, 46, 40, 173, 74, 205, 202, 47, 169, 76, 201, 204, 43, 174, 72, 206, 200, 45, 42, 79, 74, 207, 41, 44, 75, 28, 82, 50, 0, 147, 61, 85, 135],
    exact64: [40, 207, 115, 112, 116, 114, 118, 113, 117, 115, 247, 240, 244, 242, 246, 241, 245, 243, 15, 8, 12, 10, 14, 9, 13, 11, 143, 136, 140, 138, 142, 137, 141, 139, 79, 72, 76, 74, 78, 73, 77, 75, 207, 200, 204, 202, 206, 201, 205, 203, 47, 40, 44, 42, 46, 41, 45, 43, 175, 168, 172, 170, 174, 169, 173, 171, 7, 0, 178, 254, 23, 225],
    garbage: [40, 21, 255, 255, 255, 1, 2],
    badhdr: [120, 0, 1, 2, 3],
    storedbad: [40, 21, 0, 100, 0, 155, 255, 3, 10, 17, 24, 31, 38, 45, 52, 59, 66, 73, 80, 87, 94, 101, 108, 115, 122, 129, 136, 143, 150, 157, 164, 171, 178, 185, 192, 199, 206, 213, 220, 227, 234, 241, 248, 255, 6, 13, 20, 27, 34, 41, 48, 55, 62, 69, 76, 83, 90, 97, 104, 111, 118, 125, 132, 139, 146, 153, 160, 167, 174, 181, 188, 195, 202, 209, 216, 223, 230, 237, 244, 251, 2, 9, 16, 23, 30, 37, 44, 51, 58, 65, 72, 79, 86, 93, 100, 107, 114, 121, 128, 135, 142, 149, 156, 163, 170, 177, 184, 1, 5, 0, 0, 0, 120, 120, 120, 120, 120],
    trunc: [40, 21, 1, 100, 0, 155, 255, 5, 16, 27, 38, 49, 60, 71, 82, 93, 104, 115, 126, 137, 148, 159, 170, 181, 192, 203, 214, 225, 236, 247, 2, 13, 24, 35, 46, 57, 68, 79, 90, 101, 112, 123, 134, 145, 156, 167, 178, 189, 200, 211, 222, 233, 244, 255, 10, 21, 32, 43, 54, 65, 76, 87, 98, 109, 120, 131, 142, 153, 164, 175, 186, 197, 208, 219, 230, 241, 252, 7, 18, 29, 40, 51, 62, 73, 84, 95, 106],
    level0: [40, 21, 1, 32, 0, 223, 255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 21, 112, 1, 241],
    empty: [40, 207, 3, 0, 0, 0, 0, 1],
    one: [40, 207, 171, 0, 0, 0, 121, 0, 121],
    two: [40, 207, 171, 168, 4, 0, 1, 107, 0, 242],
  };
  const ROW_DATA = {
    rgb3_none: [40, 207, 99, 16, 50, 143, 105, 92, 118, 250, 131, 168, 21, 0, 17, 140, 3, 215],
    rgb3_sub: [40, 207, 99, 148, 117, 74, 239, 217, 120, 237, 183, 130, 43, 0, 19, 133, 4, 59],
    rgb3_up: [40, 207, 99, 210, 240, 45, 154, 190, 231, 33, 155, 118, 0, 0, 18, 126, 3, 159],
    rgb3_avg: [40, 207, 99, 54, 142, 168, 93, 116, 252, 141, 160, 89, 52, 0, 20, 119, 4, 3],
    rgb3_paeth: [40, 207, 99, 177, 75, 238, 88, 123, 233, 187, 140, 99, 26, 0, 22, 112, 4, 103],
    rgb3_two: [40, 207, 99, 240, 204, 155, 188, 227, 46, 147, 186, 79, 33, 83, 72, 229, 188, 195, 47, 120, 141, 194, 107, 0, 80, 35, 7, 240],
    rgb3_short: [40, 207, 99, 136, 111, 89, 121, 14, 0, 5, 45, 2, 91],
    gray1_8: [40, 207, 99, 200, 2, 0, 0, 108, 0, 107],
    gray16_2: [40, 207, 99, 40, 157, 181, 255, 9, 0, 6, 9, 2, 179],
    gray8_1: [40, 207, 99, 104, 0, 0, 0, 130, 0, 129],
    gray8_2: [40, 207, 99, 232, 222, 0, 0, 1, 201, 1, 60],
    gray8_8: [40, 207, 99, 152, 182, 251, 1, 171, 150, 127, 201, 76, 0, 18, 69, 3, 189],
    rgba8_2: [40, 207, 99, 92, 120, 236, 181, 128, 105, 84, 253, 18, 0, 19, 218, 4, 22],
  };
  const zt = (k, ct, payload) => str(k).concat([0, ct & 0xff], payload);
  const zinit = (p) => {
    const zs = p.add(0x74);
    zero(zs, 0x38);
    zs.add(0x20).writePointer(va(0x100786b0));
    zs.add(0x24).writePointer(va(0x10078750));
    zs.add(0x28).writePointer(p);
    const init2 = new NativeFunction(va(0x1009bf10), 'int', ['pointer', 'int', 'pointer', 'int'], 'mscdecl');
    const r = init2(zs, 10, cblock('1.0.2', 8), 0x38);
    if (r !== 0) throw new Error('c3ds: inflateInit2_ returned ' + r);
  };
  const zstate = (msg) => (p, info) => { noText(p, info); p.add(0x90).writeU32(0); p.add(0x8c).writePointer(msg ? cblock('fixture zlib message', 0x20) : ptr(0)); };
  const Z = ZTXT_DATA;
  const ZTXT = [
    { mode: 0, data: zt('k', 0, Z.short) },
    { mode: 5, data: zt('Comment', 0, Z.short), post: zinit },
    { data: zt('Title', 0, Z.short), post: zinit },
    { data: zt('T', 0, Z.long), post: zinit },                  // several zbuf fills: the append path
    { data: zt('Exact', 0, Z.exact64), post: zinit },
    { data: zt('G', 0, Z.garbage), post: zinit },               // invalid block type: error, no text yet
    { data: zt('H', 0, Z.badhdr), post: zinit },                // header rejected
    { data: zt('S', 0, Z.storedbad), post: zinit },             // error after a full zbuf: text exists
    { data: zt('Tr', 0, Z.trunc), post: zinit },                // input runs out with text
    { data: zt('L0', 0, Z.level0), zsize: 8, post: zinit },     // stored, 8-byte zbuf
    { data: zt('N', 0, Z.short), set: zstate(false) },          // no inflate state, msg NULL
    { data: zt('M', 0, Z.short), set: zstate(true) },           // no inflate state, msg set
    { data: zt('k', 1, [1, 2, 3, 4]) },                         // unknown type 1
    { data: zt('key', 0xff, str('abcdefghijklmnopqrstuvwxyz0123456789')) },   // type -1, copy capped at 0x1a
    { data: str('keyonly') },                                   // nothing after the key
    { data: zt('k', 0, Z.short), bad: true },
    { data: zt('k', 0, Z.short), bad: true, flags: 0x200 },
    { data: zt('n', 0, Z.short), noInfo: true, post: zinit },
  ];
  ZTXT.forEach((s) => { if (!s.set) s.set = noText; });

  // pCAL: purpose, NUL, X0, X1 (big-endian), equation type, parameter count, units, NUL, parameters separated by NULs
  const pc = (purpose, x0, x1, type, n, units, params) => {
    let a = str(purpose).concat([0], be32(x0), be32(x1), [type, n], str(units), [0]);
    params.forEach((q, i) => { a = a.concat(str(q)); if (i < params.length - 1) a.push(0); });
    return a;
  };
  const PCAL = [
    { mode: 0, data: pc('cal', 0, 255, 0, 2, 'm', ['1', '2']) },
    { mode: 5, data: pc('cal', 0, 255, 0, 2, 'm', ['1', '2']) },
    { valid: 0x400, data: pc('cal', 0, 255, 0, 2, 'm', ['1', '2']) },
    { data: pc('linear', 0, 255, 0, 2, 'km', ['1.0', '2.5']) },
    { data: pc('base-e', 0xffffffff, 7, 1, 3, 'K', ['0.5', '1e3', '-2']) },
    { data: pc('arb', 3, 0x80000000, 2, 3, '', ['a', 'bb', 'ccc']) },
    { data: pc('hyper', 1, 2, 3, 4, 'Pa', ['1', '2', '3', '4']) },
    { data: pc('bad0', 0, 1, 0, 3, 'u', ['1', '2', '3']) },                // linear needs 2
    { data: pc('bad1', 0, 1, 1, 2, 'u', ['1', '2']) },
    { data: pc('bad2', 0, 1, 2, 4, 'u', ['1', '2', '3', '4']) },
    { data: pc('bad3', 0, 1, 3, 3, 'u', ['1', '2', '3']) },
    { data: pc('new', 0, 1, 4, 1, 'u', ['9']) },                           // unrecognised type: warning, kept
    { data: pc('none', 0, 1, 9, 0, 'u', []) },                             // no parameters
    { data: str('short\0').concat([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]) },   // endptr == buf + 12: too short
    { data: str('twelve\0').concat([0, 0, 0, 1, 0, 0, 0, 2, 4, 0, 0, 0]) },   // endptr == buf + 13: long enough
    { data: pc('runout', 0, 1, 1, 3, 'u', ['1', '2']) },                   // a parameter missing
    { data: pc('cal', 0, 255, 0, 2, 'm', ['1', '2']), bad: true },
    { data: pc('cal', 0, 255, 0, 2, 'm', ['1', '2']), bad: true, flags: 0x200 },
    { data: pc('noinfo', 5, 6, 0, 2, 'm', ['1', '2']), noInfo: true },
  ];

  // ================================================================== group R: chunk loops (whole streams)
  // `raw` specs: the source is the stream itself (signature and chunks, each chunk length + name + data + the crc32 of
  // name and data, which is what png_reset_crc 0x10078790 then png_crc_read of the name and payload accumulate).
  const SIG = [137, 80, 78, 71, 13, 10, 26, 10];
  const ck = (name, data, bad) => {
    const nd = str(name).concat(data || []);
    let c = crc32(0, nd);
    if (bad) c = (c ^ 0x5a5a5a5a) >>> 0;
    return be32((data || []).length).concat(nd, be32(c));
  };
  const ihdr = (w, h, d, ct, il) => ck('IHDR', be32(w).concat(be32(h), [d, ct, 0, 0, il || 0]));
  const cat = function () { return [].concat.apply([], Array.prototype.slice.call(arguments)); };
  const IDAT0 = ck('IDAT', []);
  const sig = (n) => (p) => p.add(0x12c).writeU8(n);
  const both2 = (f, g) => (p, info, out, i) => { f(p, info, out, i); g(p, info, out, i); };
  const RINFO = [
    { mode: 0, raw: true, data: cat(SIG, ihdr(4, 2, 8, 2), IDAT0), set: both2(sig(0), noText) },
    { mode: 0, raw: true, data: cat(ihdr(3, 3, 8, 0), ck('gAMA', be32(45455)), ck('sRGB', [0]), IDAT0), set: both2(sig(8), noText) },
    { mode: 0, raw: true, data: cat([137, 80, 78, 71, 13, 10, 26, 11], ihdr(1, 1, 8, 0), IDAT0), set: both2(sig(0), noText) },   // corrupted
    { mode: 0, raw: true, data: cat(str('GIF89a..'), ihdr(1, 1, 8, 0)), set: both2(sig(0), noText) },        // not a PNG
    { mode: 0, raw: true, data: cat([13, 10, 26, 12], ihdr(1, 1, 8, 0)), set: both2(sig(4), noText) },          // checked 4
    { mode: 0, raw: true, data: cat([1, 2, 13, 10, 26, 10], ihdr(1, 1, 8, 0)), set: both2(sig(2), noText) },    // checked 2, bad
    { mode: 0, raw: true, data: cat([78, 71, 13, 10, 26, 10], ihdr(2, 2, 8, 0), IDAT0), set: both2(sig(2), noText) },   // checked 2, good
    { mode: 0, raw: true, data: cat(SIG, ihdr(4, 4, 8, 3), ck('PLTE', pat(12, 1)), ck('tRNS', [0, 0x80, 0xff]),
                                    ck('bKGD', [2]), ck('hIST', pat(8, 2)), ck('IDAT', pat(5, 3))), set: both2(sig(0), noText) },
    { mode: 0, raw: true, data: cat(SIG, ihdr(4, 4, 8, 3), IDAT0), set: both2(sig(0), noText) },          // missing PLTE
    { mode: 0, raw: true, data: cat(SIG, IDAT0), set: both2(sig(0), noText) },                              // missing IHDR
    { mode: 0, raw: true, data: cat(SIG, ihdr(5, 5, 16, 6), ck('tEXt', str('Title\0x')), ck('zTXt', str('c\0\u0001zz')),
                                    ck('tIME', [7, 0xd2, 1, 2, 3, 4, 5]), ck('pHYs', be32(2835).concat(be32(2835), [1])),
                                    ck('oFFs', be32(3).concat(be32(4), [0])), ck('sBIT', [8, 8, 8, 8]),
                                    ck('cHRM', u32s(RV)), ck('pCAL', pc('p', 0, 1, 0, 2, 'u', ['1', '2'])),
                                    ck('abCd', [1, 2, 3]), ck('IDAT', [9])), set: both2(sig(0), noText) },
    { mode: 0, raw: true, data: cat(SIG, ihdr(1, 1, 1, 0), ck('IEND', [])), set: both2(sig(0), noText) },   // IEND before IDAT
    { mode: 0, raw: true, data: cat(SIG, ihdr(1, 1, 1, 0), ck('ABCD', [1])), set: both2(sig(0), noText) },  // unknown critical
    { mode: 0, raw: true, data: cat(SIG, ihdr(1, 1, 1, 0), ck('gAMA', be32(1), true), IDAT0), set: both2(sig(0), noText) },
    { mode: 0, flags: 0x200, raw: true, data: cat(SIG, ihdr(1, 1, 1, 0), ck('gAMA', be32(1), true), IDAT0), set: both2(sig(0), noText) },
  ];
  // png_read_end: the png_struct is past IDAT (mode IHDR | IDAT, chunk name IDAT, crc 0x13572468); the stream starts
  // with the four CRC bytes png_crc_finish (0) compares with it.
  const endState = (mode) => (p, info) => {
    noText(p, info);
    p.add(0x68).writeU32(mode);
    for (let k = 0; k < 4; k++) p.add(0x11c + k).writeU8('IDAT'.charCodeAt(k));
    p.add(0x110).writeU32(0x13572468);
  };
  const OK = be32(0x13572468), IEND0 = ck('IEND', []);
  const REND = [
    { raw: true, data: cat(OK, IEND0), set: endState(5) },
    { raw: true, data: cat(OK, ck('tEXt', str('A\0b')), IEND0), set: endState(5) },
    { raw: true, data: cat(OK, IDAT0, IEND0), set: endState(5) },
    { raw: true, data: cat(OK, ck('IDAT', [1, 2, 3, 4, 5]), IEND0), set: endState(5) },        // too many IDATs
    { raw: true, data: cat(OK, IDAT0, IEND0), set: endState(13) },                             // after IDAT: error
    { raw: true, data: cat(OK, ck('gAMA', be32(50000)), ck('tIME', [7, 0xd0, 12, 31, 23, 59, 59]),
                           ck('zTXt', str('k\0\u0002q')), ck('xyZw', [5]), ck('pCAL', pc('q', 2, 3, 1, 3, 'v', ['a', 'b', 'c'])),
                           IEND0), set: endState(5) },
    { raw: true, data: cat(be32(0x13572469), IEND0), set: endState(5) },                      // IDAT CRC mismatch: chunk error
    { raw: true, data: cat(OK, ihdr(1, 1, 8, 0), IEND0), set: endState(5) },                   // IHDR again: error
    { raw: true, data: cat(OK, ck('PLTE', pat(6, 4)), IEND0), set: endState(7) },              // PLTE after IDAT: warning
    { raw: true, data: cat(OK, ck('IEND', [1, 2, 3, 4])), set: endState(5) },                  // IEND with a length
    { raw: true, data: cat(OK, ck('hIST', pat(4, 5)), ck('tRNS', [0, 1]), ck('bKGD', [0, 2]), ck('sRGB', [1]),
                           ck('cHRM', u32s(RV)), ck('sBIT', [3]), ck('pHYs', pat(9, 6)), ck('oFFs', pat(9, 7)), IEND0),
      set: endState(7) },
    { raw: true, data: cat(OK, ck('IEND', [], true)), set: endState(5) },                      // IEND bad CRC: critical
    { raw: true, flags: 0x400, data: cat(OK, ck('IEND', [], true)), set: endState(5) },        // CRITICAL_USE: warning
  ];

  // png_read_finish_row: fields per spec (row_number +0xe4, num_rows +0xd0, interlaced +0x123, pass +0x124, width
  // +0xc8, height +0xcc, pixel_depth +0x129, rowbytes +0xd8, transformations +0x70, flags +0x6c, z_stream avail_in
  // +0x78 / next_in +0x74, idat_size +0x10c); prev_row +0xe8 is a 0x40-byte block in the extras area. The source
  // starts with the CRC of the IDAT chunk in progress (png->crc = 0x2468ace0) for png_crc_finish's first call.
  const PREV = be32(0x2468ace0);
  const frow = (o) => (p, info, out, i) => {
    noText(p, info);
    const f = Object.assign({ row: 0, rows: 1, il: 0, pass: 0, w: 8, h: 8, pd: 8, rb: 8, tr: 0, flags: 0x20,
                              avail: 0, idat: 0, zsize: 0x40, state0: false }, o);
    p.add(0xe4).writeU32(f.row); p.add(0xd0).writeU32(f.rows); p.add(0x123).writeU8(f.il); p.add(0x124).writeU8(f.pass);
    p.add(0xc8).writeU32(f.w); p.add(0xcc).writeU32(f.h); p.add(0x129).writeU8(f.pd); p.add(0xd8).writeU32(f.rb);
    p.add(0x70).writeU32(f.tr); p.add(0x6c).writeU32(f.flags); p.add(0x10c).writeU32(f.idat);
    p.add(0xb0).writeU32(f.zsize);
    p.add(0xe8).writePointer(out.x.add(0x800 + i * 0x40));
    p.add(0x110).writeU32(0x2468ace0);
    p.add(0x78).writeU32(f.avail);
    p.add(0x74).writePointer(p.add(0xac).readPointer());
    if (f.zbuf) bytes(p.add(0xac).readPointer(), f.zbuf);
    if (f.state0) { p.add(0x90).writeU32(0); p.add(0x8c).writeU32(0); }
  };
  const E = ZTXT_DATA.empty, ONE = ZTXT_DATA.one;
  const FROW = [
    { raw: true, data: [], set: frow({ row: 0, rows: 5 }) },
    { raw: true, data: [], set: frow({ row: 4, rows: 3, state0: true }) },
    { raw: true, data: [], set: frow({ row: 0, rows: 1, idat: 5, state0: true }) },
    { raw: true, data: [], set: frow({ row: 0, rows: 1, avail: 3, state0: true }) },
    { raw: true, data: [], set: frow({ il: 1, pass: 0 }) },
    { raw: true, data: [], set: frow({ il: 1, pass: 0, w: 4, h: 4 }) },
    { raw: true, data: [], set: frow({ il: 1, pass: 0, tr: 2, w: 13, pd: 24 }) },
    { raw: true, data: [], set: frow({ il: 1, pass: 6, state0: true }) },
    { raw: true, data: [], set: frow({ il: 1, pass: 0, w: 1, h: 1 }) },
    { raw: true, data: [], set: frow({ il: 1, pass: 2, w: 1, h: 1, pd: 1 }) },
    { raw: true, data: [], set: frow({ il: 1, pass: 5, w: 1, h: 1, pd: 48 }) },
    { raw: true, data: [], set: frow({ il: 1, pass: 0, w: 0, h: 0, state0: true }) },
    { raw: true, data: cat(PREV, ck('IDAT', E)), set: frow({ flags: 0 }), post: zinit },
    { raw: true, data: cat(PREV, ck('IDAT', ONE)), set: frow({ flags: 0 }), post: zinit },          // extra data
    { raw: true, data: cat(PREV, ck('IEND', [])), set: frow({ flags: 0 }), post: zinit },          // not enough data
    { raw: true, data: cat(PREV, ck('IDAT', E)), set: frow({ flags: 0, zsize: 4 }), post: zinit },
    { raw: true, data: E.concat(be32(0)), set: frow({ flags: 0, idat: 8 }), post: zinit },
    { raw: true, data: cat(PREV, ck('IDAT', E.slice(0, 5)), ck('IDAT', E.slice(5))), set: frow({ flags: 0 }), post: zinit },
    { raw: true, data: cat(PREV, ck('IDAT', [40, 207, 0xff, 0xff, 1, 2])), set: frow({ flags: 0 }), post: zinit },
    { raw: true, data: [], set: frow({ flags: 0, avail: 2, zbuf: [1, 2], state0: true }) },      // no inflate state
    { raw: true, data: cat(PREV, ck('IDAT', E.concat([7, 7]))), set: frow({ flags: 0 }), post: zinit },   // trailing bytes
    { raw: true, data: cat(be32(0x2468ace1), ck('IDAT', E)), set: frow({ flags: 0 }), post: zinit },      // CRC error
    { raw: true, data: cat(PREV, ck('IDAT', []), ck('IDAT', E)), set: frow({ flags: 0 }), post: zinit },  // empty IDAT first
    { raw: true, data: E, set: frow({ flags: 0, avail: 0, idat: 3 }), post: zinit },                      // needs more input
  ];
  const FROW2 = [
    { raw: true, data: cat(PREV, ck('IDAT', ZTXT_DATA.two)), set: frow({ flags: 0 }), post: zinit },      // Z_OK, no room
    { raw: true, data: cat(PREV, ck('IDAT', ZTXT_DATA.one)), set: frow({ flags: 0, zsize: 3 }), post: zinit },
    { raw: true, data: cat(PREV, ck('IDAT', E.concat([7, 7]))), set: frow({ flags: 0, zsize: E.length }), post: zinit },
  ];

  // png_read_row: the png_struct is mid-image (flags PNG_FLAG_ROW_INIT 0x40 unless `noinit`, mode IHDR | IDAT), its
  // z_stream initialised (wbits 10), idat_size 0 and avail_in 0 so the first refill reads a chunk header after the
  // CRC of the IDAT in progress. Row geometry per spec: w (width +0xc8), iw (iwidth +0xe0, default w), rb (rowbytes
  // +0xd8), irb (irowbytes +0xdc, default rb + 1), pd / bd / ch / ct, il / pass / rn / nr, tr. row_buf +0xec and
  // prev_row +0xe8 are 0x80-byte extras blocks (prev_row patterned); the vector's row / dsp_row are 0x80-byte blocks of
  // the extras area (+0x1000 + i * 0x100) or NULL. read_row_fn +0x198: `cb` installs a recorder of (row_number, pass)
  // into extras + 0xf00.
  let rowCb = null;
  const rowLog = (out) => {
    const log = out.x.add(0xf00);
    const cb = new NativeCallback(function (png, rn, pass) {
      const n = log.readU32();
      if (n < 30) { log.add(4 + n * 8).writeU32(rn); log.add(8 + n * 8).writeU32(pass); }
      log.writeU32(n + 1);
    }, 'void', ['pointer', 'uint32', 'int'], 'mscdecl');
    __diffKeepAlive.push(cb);
    return cb;
  };
  const rowfx = (o) => (p, info, out, i) => {
    noText(p, info);
    const f = Object.assign({ w: 3, rb: 9, pd: 24, bd: 8, ch: 3, ct: 2, il: 0, pass: 0, rn: 0, nr: 1, h: 1, tr: 0,
                              mode: 5, flags: 0, row: true, dsp: true, cb: false, idat: 0, zsize: 0x40 }, o);
    p.add(0xc8).writeU32(f.w); p.add(0xe0).writeU32(f.iw === undefined ? f.w : f.iw);
    p.add(0xd8).writeU32(f.rb); p.add(0xdc).writeU32(f.irb === undefined ? f.rb + 1 : f.irb);
    p.add(0x129).writeU8(f.pd); p.add(0x127).writeU8(f.bd); p.add(0x12a).writeU8(f.ch); p.add(0x126).writeU8(f.ct);
    p.add(0x123).writeU8(f.il); p.add(0x124).writeU8(f.pass); p.add(0xe4).writeU32(f.rn); p.add(0xd0).writeU32(f.nr);
    p.add(0xcc).writeU32(f.h); p.add(0x70).writeU32(f.tr); p.add(0x68).writeU32(f.mode);
    p.add(0x6c).writeU32((f.noinit ? 0 : 0x40) | f.flags);
    p.add(0x10c).writeU32(f.idat); p.add(0x78).writeU32(0); p.add(0xb0).writeU32(f.zsize);
    p.add(0x110).writeU32(0x2468ace0);
    const rb = out.x.add(0x100 + i * 0x80), pr = out.x.add(0x900 + i * 0x40);   // i < 14: below the callback log at 0xf00
    p.add(0xec).writePointer(rb); p.add(0xe8).writePointer(pr);
    for (let k = 0; k < 0x40; k++) pr.add(k).writeU8((k * 23 + i) & 0xff);
    out['R' + i] = f.row ? out.rows.add(i * 0x100) : ptr(0);
    out['D' + i] = f.dsp ? out.rows.add(i * 0x100 + 0x80) : ptr(0);
    p.add(0x198).writePointer(f.cb ? (rowCb = rowCb || rowLog(out)) : ptr(0));
  };
  const RD = ROW_DATA;
  const idat = (z) => cat(PREV, ck('IDAT', z));
  const G8 = (o) => rowfx(Object.assign({ il: 1, tr: 2, w: 8, rb: 8, pd: 8, ch: 1, ct: 0, nr: 8, h: 8 }, o));
  const ROWSPEC = [
    { raw: true, data: idat(RD.rgb3_none), set: rowfx({}), post: zinit },
    { raw: true, data: idat(RD.rgb3_sub), set: rowfx({ dsp: false }), post: zinit },
    { raw: true, data: idat(RD.rgb3_up), set: rowfx({ row: false }), post: zinit },
    { raw: true, data: idat(RD.rgb3_avg), set: rowfx({ row: false, dsp: false, cb: true }), post: zinit },
    { raw: true, data: idat(RD.rgb3_paeth), set: rowfx({ cb: true }), post: zinit },
    { raw: true, data: idat(RD.rgb3_two), set: rowfx({ nr: 2, h: 2 }), post: zinit },          // Z_OK: one of two rows
    { raw: true, data: idat(RD.rgb3_short), set: rowfx({}), post: zinit },                   // stream ends short
    { raw: true, data: idat([40, 207, 0xff, 0xff]), set: rowfx({}), post: zinit },          // inflate error
    { raw: true, data: cat(PREV, ck('IEND', [])), set: rowfx({}), post: zinit },            // not enough data
    { raw: true, data: idat(RD.rgb3_none), set: rowfx({ mode: 1 }), post: zinit },          // no IDAT yet
    { raw: true, data: idat(RD.gray1_8), set: rowfx({ w: 8, rb: 1, pd: 1, bd: 1, ch: 1, ct: 0, tr: 0x20 }), post: zinit },
    { raw: true, data: idat(RD.gray16_2), set: rowfx({ w: 2, rb: 4, pd: 16, bd: 16, ch: 1, ct: 0, tr: 0x10 }), post: zinit },
    { raw: true, data: idat(RD.rgba8_2), set: rowfx({ w: 2, rb: 8, pd: 32, bd: 8, ch: 4, ct: 6, tr: 0x1 }), post: zinit },
    { raw: true, data: idat(RD.rgb3_none), set: rowfx({ zsize: 5 }), post: zinit },         // several refills
    { raw: true, data: idat(RD.gray8_8), set: G8({ pass: 7 }), post: zinit },                // pass 7: switch default
    { raw: true, data: [], set: rowfx({}), post: (q) => {                                    // input already in the zbuf
      zinit(q);                                      // (inflateInit2_ clears the z_stream: the input goes in after it)
      bytes(q.add(0xac).readPointer(), RD.rgb3_none);
      q.add(0x74).writePointer(q.add(0xac).readPointer());
      q.add(0x78).writeU32(RD.rgb3_none.length);
    } },
    { raw: true, data: idat(RD.rgb3_none.concat([7, 7])), set: rowfx({}), post: zinit },     // avail_in left at the end
    { raw: true, data: idat(RD.rgb3_none.concat([7, 7])), set: rowfx({ zsize: RD.rgb3_none.length }), post: zinit },
  ];
  const nostate = (o) => (p, info, out, i) => { rowfx(o)(p, info, out, i); p.add(0x90).writeU32(0); p.add(0x8c).writeU32(0); };
  const ROWSPEC3 = [
    { raw: true, data: idat(RD.rgb3_none), set: rowfx({ noinit: true }), post: zinit },          // png_read_start_row first
    { raw: true, data: idat(RD.gray8_8), set: rowfx({ noinit: true, w: 8, rb: 8, pd: 8, ch: 1, ct: 0 }), post: zinit },
    { raw: true, data: idat(RD.rgb3_none), set: nostate({}) },                                   // Z_STREAM_ERROR, msg NULL
  ];
  // interlaced with PNG_INTERLACE: rows that are only combined and advanced, then rows that are read (8 x 8 gray;
  // iwidth per pass {1, 1, 2, 2, 4, 4, 8} by png_pass_inc / png_pass_start)
  const ROWSKIP = [
    { raw: true, data: [], set: G8({ pass: 0, rn: 3 }) },
    { raw: true, data: [], set: G8({ w: 4, rb: 4, pass: 1, rn: 0 }) },
    { raw: true, data: [], set: G8({ pass: 2, rn: 5 }) },
    { raw: true, data: [], set: G8({ pass: 2, rn: 1 }) },
    { raw: true, data: [], set: G8({ w: 2, rb: 2, pass: 3, rn: 0 }) },
    { raw: true, data: [], set: G8({ pass: 4, rn: 3 }) },
    { raw: true, data: [], set: G8({ pass: 4, rn: 1 }) },
    { raw: true, data: [], set: G8({ w: 1, rb: 1, pass: 5, rn: 2 }) },
    { raw: true, data: [], set: G8({ pass: 6, rn: 2 }) },
    { raw: true, data: [], set: G8({ pass: 0, rn: 1, dsp: false }) },
    { raw: true, data: [], set: G8({ pass: 1, rn: 3 }) }, { raw: true, data: [], set: G8({ pass: 1, rn: 3, dsp: false }) },
    { raw: true, data: [], set: G8({ pass: 2, rn: 5, dsp: false }) },
    { raw: true, data: [], set: G8({ pass: 3, rn: 1 }) }, { raw: true, data: [], set: G8({ pass: 3, rn: 1, dsp: false }) },
    { raw: true, data: [], set: G8({ pass: 4, rn: 3, dsp: false }) },
    { raw: true, data: [], set: G8({ pass: 5, rn: 1 }) }, { raw: true, data: [], set: G8({ pass: 5, rn: 1, dsp: false }) },
    { raw: true, data: cat(PREV, ck('IDAT', []), ck('IDAT', RD.rgb3_none)), set: rowfx({}), post: zinit },
  ];
  const IW = [1, 1, 2, 2, 4, 4, 8];
  const RDN = ['gray8_1', 'gray8_1', 'gray8_2', 'gray8_2', 'gray8_8', 'gray8_8', 'gray8_8'];
  const ROWREAD = [0, 1, 2, 3, 4, 5, 6].map((pass) => {
    const iw = IW[pass] === 4 ? 8 : IW[pass];        // passes 4 / 5 read 8-byte rows here (iwidth 8, irowbytes 9)
    return { raw: true, data: idat(RD[RDN[pass]]),
             set: G8({ pass: pass, iw: iw, irb: iw + 1, rn: [0, 0, 4, 0, 2, 0, 1][pass], cb: pass === 3 }), post: zinit };
  }).concat([
    { raw: true, data: idat(RD.gray8_8), set: G8({ tr: 0, pass: 0 }), post: zinit },             // interlaced, no PNG_INTERLACE
    { raw: true, data: idat(RD.gray8_2), set: G8({ pass: 2, iw: 2, irb: 3, rn: 4, row: false, dsp: false }), post: zinit },
  ]);
  const rowsKey = (specs, seed) => {
    rowCb = null;
    const rows = __keepAlloc(specs.length * 0x100);
    fill(rows, specs.length * 0x100, seed + 0x100);
    const o = chunkFx(0x6c3f0, 'IDAT', specs, seed, { arenaSize: 0x40000 }, rows);
    return o;
  };

  // png_read_image: image = an array of up to four row pointers (extras + 0xe00) into the key's rows block; the
  // png_struct is set up as for png_read_row with the first row and pass 0 pending.
  const imgfx = (o) => (p, info, out, i) => {
    rowfx(o)(p, info, out, i);
    const arr = out.x.add(0xe00 + i * 0x10);
    for (let k = 0; k < 4; k++) arr.add(4 * k).writePointer(out.rows.add(i * 0x100 + k * 0x40));
    out['A' + i] = arr;
  };
  const IMGSPEC = [
    { raw: true, data: idat(RD.rgb3_none), set: imgfx({ h: 1, nr: 0 }), post: zinit },
    { raw: true, data: idat(RD.rgb3_two), set: imgfx({ h: 2, nr: 0 }), post: zinit },
    { raw: true, data: idat(RD.gray8_1), set: imgfx({ il: 1, w: 1, rb: 1, iw: 1, irb: 2, pd: 8, ch: 1, ct: 0, h: 1 }), post: zinit },
    { raw: true, data: [], set: imgfx({ h: 0, state0: true }) },
    { raw: true, data: cat(idat(RD.rgb3_none), ck('IEND', [])), set: imgfx({ h: 2 }), post: zinit },   // runs out of data
    { raw: true, data: idat(RD.gray16_2), set: imgfx({ w: 2, rb: 4, pd: 16, bd: 16, ch: 1, ct: 0, tr: 0x10, h: 1 }), post: zinit },
  ];

  // png_create_read_struct: no png_struct of ours; the heap stand-in carves the one png_create_struct 0x10078da0
  // allocates (allocation call 0), the 0x2000-byte zbuf (call 1) and inflateInit_'s state (calls 2..5), so failMask
  // reaches each NULL arm. Version strings, the fixture's error functions ($ejump: record and longjmp to the jmp_buf
  // png_create_read_struct's own setjmp filled; NULL: png_default_error, fprintf then longjmp) and warning recorder.
  function createFx(failMask, seed) {
    const out = world('jump', 0, seed);
    const h = __heapStandIn({ module: 'jgld.dll', alloc: [0x7fb40], free: [0x806d0], arenaSize: 0x10000, failMask: failMask });
    out.arena = h.arena; out.freelog = h.log;
    ['1.0.5', '1.2.3', '1', '0.96', '0.89', '2.0', '', '1x'].forEach((v, i) => { out['v' + i] = cblock(v, 0x10); });
    out.eptr = cblock('error ptr', 0x10);
    return out;
  }

  // ================================================================== group S: Surface blend rectangles
  // c3ce's surface set (copied from c3dn's ceSurfaces, re/frida/js/fixtures.d/c3dn.js): sixteen Surfaces on jgld's
  // Surface vtable 0x1011d0b0 (depths 8 / 16 in layouts 565, 555 and 2 / 12 / 24 / 32 / 48 / 4, clip inside or past the
  // bounds), Palettes on jgld's Palette vtable 0x1011db10 with 565 / 555 tables, pixel buffers, argument rectangles.
  function ceSurfaces() {
    const base = jgld();
    const vtSurface = base.add(0x11d0b0);
    const vtPalette = base.add(0x11db10);
    const NSURF = 16, OSIZE = 0x820, PSTRIDE = 0x400;
    const OFF_SURF = 0x0000, OFF_PAL = 0x8200, OFF_PIX = 0x9400, PIXLEN = 0x4800;
    const OFF_OUT = 0xDC00, OFF_RECT = 0xDC80, OFF_LUT = 0xDD80, OFF_DC = 0xDE80, OFF_PAL2 = 0xE000;
    const TOTAL = 0x10200;
    const blk = __keepAlloc(TOTAL);
    zero(blk, TOTAL);
    const fx = { obj: blk, blk: blk, pix: blk.add(OFF_PIX), out: blk.add(OFF_OUT), lut: blk.add(OFF_LUT) };
    for (let i = 0; i < PIXLEN; i++) blk.add(OFF_PIX + i).writeU8((i * 37 + 11) & 0xff);
    const mkPal = (p, a565, b565, a555, b555) => {
      p.writePointer(vtPalette);
      for (let i = 0; i < 256; i++) {
        p.add(0x40c + i * 2).writeU16((i * a565 + b565) & 0xffff);
        p.add(0x60c + i * 2).writeU16((i * a555 + b555) & 0xffff);
      }
      return p;
    };
    const pal = mkPal(blk.add(OFF_PAL), 0x0105, 0x1234, 0x0203, 0x5678);
    const pal2 = mkPal(blk.add(OFF_PAL + OSIZE), 0x0307, 0x9abc, 0x0409, 0xdef0);
    fx.pal = pal; fx.pal2 = pal2;
    [0x11, 0x22, 0x22, 0].forEach(function (f, k) {
      const p = mkPal(blk.add(OFF_PAL2 + k * OSIZE), 0x0101, k, 0x0202, k);
      for (let i = 0; i < 0x400; i += 4) p.add(0xc + i).writeU32((0x00102030 * (k + 1) + i * 0x01030507) >>> 0);
      p.add(0x810).writeU32(f);
      fx['pal' + (k + 3)] = p;
    });
    const SPECS = [
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal, true, 0, 0, 0x11111111],
      [8, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true, pal, false, 0, 0, 0x22222222],
      [8, 1, 19, [0, 0, 19, 11], [0, 0, 19, 11], true, pal2, true, 3, 2, 0x00000000],
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal, false, 0, 0, -1],
      [16, 1, 16, [0, 0, 16, 10], [0, 0, 16, 10], true, pal2, true, 1, 1, 0x7FFFFFFF],
      [16, 2, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal, false, 0, 0, 5],
      [12, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal, false, 0, 0, 6],
      [24, 0, 10, [0, 0, 10, 8], [0, 0, 10, 8], true, pal, false, 0, 0, 7],
      [32, 0, 8, [0, 0, 8, 8], [0, 0, 8, 8], true, pal, false, 0, 0, 8],
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false, 0, false, 0, 0, 9],
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 64, 40], true, pal, false, 0, 0, 10],
      [16, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true, pal2, true, 5, 4, 11],
      [8, 0, 24, [2, 1, 26, 13], [2, 1, 26, 13], true, pal, false, 0, 0, 12],
      [8, 0, 32, [0, 0, 0, 0], [0, 0, 0, 0], true, 0, false, 0, 7, 13],
      [48, 0, 16, [0, 0, 16, 8], [0, 0, 16, 8], true, pal, false, 0, 0, 14],
      [4, 0, 16, [0, 0, 16, 8], [0, 0, 16, 8], true, pal, false, 0, 0, 15],
    ];
    SPECS.forEach((sp, i) => {
      const [depth, fmt, pitch, b, k, hasBits, p, hasDC, dcN, bitsN, f4d0] = sp;
      const s = blk.add(OFF_SURF + i * OSIZE);
      s.writePointer(vtSurface);
      s.add(0x24).writeS32(depth);
      s.add(0x28).writeS32(fmt);
      s.add(0x40).writeS32(pitch);
      for (let j = 0; j < 4; j++) s.add(0x44 + j * 4).writeS32(k[j]);
      for (let j = 0; j < 4; j++) s.add(0x54 + j * 4).writeS32(b[j]);
      s.add(0x78).writeU32(i === 0 || i === 3 ? 0x22 : 0);
      s.add(0x7c).writePointer(p ? p : ptr(0));
      const dc = hasDC ? blk.add(OFF_DC + i * 0x10) : ptr(0);
      const bits = hasBits ? blk.add(OFF_PIX + i * PSTRIDE) : ptr(0);
      s.add(0x4b8).writePointer(dcN > 0 ? dc : ptr(0));
      s.add(0x4bc).writePointer(dc);
      s.add(0x4c0).writePointer(bits);
      s.add(0x4c4).writeS32(dcN);
      s.add(0x4c8).writeS32(bitsN);
      s.add(0x4cc).writePointer(bitsN > 0 ? bits : ptr(0));
      s.add(0x4d0).writeS32(f4d0);
      fx['s' + i] = s;
    });
    const RECTS = {
      r_full: [0, 0, 32, 16], r_in: [4, 2, 20, 12], r_odd: [3, 1, 14, 10], r_1x1: [5, 5, 6, 6],
      r_5x3: [1, 1, 6, 4], r_7x5: [2, 3, 9, 8], r_w32h15: [0, 0, 32, 15], r_s19: [0, 0, 19, 11],
      r_s16: [0, 0, 16, 10], r_s24: [0, 0, 10, 8], r_s32: [0, 0, 8, 8], r_s12: [2, 1, 26, 13],
      r_out: [100, 100, 110, 110], r_empty: [10, 10, 10, 10], r_inv: [20, 20, 5, 5], r_neg: [-10, -5, 8, 6],
    };
    Object.keys(RECTS).forEach((name, i) => {
      const r = blk.add(OFF_RECT + i * 0x10);
      RECTS[name].forEach((v, j) => r.add(j * 4).writeS32(v));
      fx[name] = r;
    });
    for (let i = 0; i < 0x100; i++) blk.add(OFF_LUT + i).writeU8((i * 7 + 3) & 0xff);
    return fx;
  }
  // The module Display 0x10128420 (redOf / greenOf / blueOf / makeColor ask its slot +0xb4 for the pixel layout) and
  // the palette client 0x1012873c (blend bodies read its +0x7c when the surface has no palette) are swapped with the
  // first thread frozen (__freeze restores both before the resume); the client's palette is at +4 (0x1000e024). The display is ours: slot +0xb4 counts its calls
  // at $dcount and returns `fmt` (1: 565 channel shifts, else 555); every other slot is a trap that counts at its own
  // word. `client`: a 0x80-byte object whose +4 is the fixture palette pal2, or NULL.
  function blendFx(fmt, client, surfPal) {
    const fx = ceSurfaces();
    if (surfPal === null) { fx.s3.add(0x7c).writePointer(ptr(0)); fx.s4.add(0x7c).writePointer(ptr(0)); }
    const dcount = __keepAlloc(0x10);
    zero(dcount, 0x10);
    fx.dcount = dcount;
    const trap = code([0xFF, 0x05].concat(addrBytes(dcount.add(4)), [0xB8], le(0x0badc0de), [0xC3]));
    const fmtSlot = code([0xFF, 0x05].concat(addrBytes(dcount), [0xB8], le(fmt), [0xC3]));
    const vt = __keepAlloc(0x200);
    for (let o = 0; o < 0x200; o += 4) vt.add(o).writePointer(trap);
    vt.add(0xb4).writePointer(fmtSlot);
    const disp = __keepAlloc(0x40);
    zero(disp, 0x40);
    disp.writePointer(vt);
    const cl = __keepAlloc(0x80);
    fill(cl, 0x80, 0x4c11);
    cl.add(4).writePointer(fx.pal2);
    fx.client = cl;
    freezeOnce();
    const gd = va(0x10128420), gc = va(0x1012873c);
    __freeze.save(gd, 4); __freeze.save(gc, 4);
    gd.writePointer(disp);
    gc.writePointer(client ? cl : ptr(0));
    return fx;
  }

  // Surface::setClip: c3ce's surfaces; every GDI call stood in (DeleteObject / SelectClipRgn record their arguments,
  // CreateRectRgnIndirect records the rectangle and returns `rgn`), IntersectRect / EqualRect (reached through intersect
  // 0x10008590 / equal 0x100085f0) run for real, unrecorded (pure; their outputs land in the surface, which is state).
  // Surfaces s0 / s3 / s11 / s13 start with a clip region handle at +0x4ac; s9 has no DC (acquireDC returns 0).
  function clipFx(rgn) {
    const fx = ceSurfaces();
    ['s0', 's3', 's11', 's13'].forEach((k, i) => fx[k].add(0x4ac).writeU32(0x0e0000a0 + i));
    const s = __importStandIn({ module: 'jgld.dll', logSize: 0x800, imports: {
      'GDI32.dll!DeleteObject': { args: 1, ret: 1 },
      'GDI32.dll!SelectClipRgn': { args: 2, ret: 2 },
      'GDI32.dll!CreateRectRgnIndirect': { args: 1, impl: function (a, ctx) {
        ctx.record([0, 4, 8, 12].map(function (o) { return a[0].add(o).readU32(); }));
        return rgn;
      } },
      'USER32.dll!IntersectRect': { args: 3, forward: true, record: false },
      'USER32.dll!EqualRect': { args: 2, forward: true, record: false },
    } });
    fx.implog = s.log;
    return fx;
  }

  // ================================================================== group M: module-global methods
  // c3dk's __thiscall recorder with n stack arguments: appends {ecx, arg0..arg(n-1)} at log+4+count*4*(n+1), increments
  // the count at log+0, returns `ret`, pops 4n bytes.
  function recThis(log, n, ret) {
    const stride = 4 * (n + 1);
    let b = [0xA1].concat(addrBytes(log), [0x69, 0xC0], le(stride), [0x05], addrBytes(log.add(4)), [0x89, 0x08]);
    for (let i = 0; i < n; i++) b = b.concat([0x8B, 0x54, 0x24, 4 + 4 * i, 0x89, 0x50, 4 + 4 * i]);
    b = b.concat([0xFF, 0x05], addrBytes(log), [0xB8], le(ret));
    b = b.concat(n ? [0xC2, (4 * n) & 0xff, 0x00] : [0xC3]);
    return code(b);
  }

  // Palette::update: twelve palettes of our own (c3ck's layout: jgld's Palette vtable 0x1011db10 at +0, 256 patterned
  // entries at +0xc, tables +0x40c / +0x60c, id +0x810, applied id +0x814, counter +0x818). QueryPerformanceCounter is
  // stood in: it writes a per-palette value (table below, chosen by which palette's +0x818 it is handed) and returns 1.
  // Seed 0 and 0x80000000 make the first Random::range (0xffff) draw 0 and seed 225070530 the first two (found by
  // running jgld's LCG, Random::next 0x10007530: seed * 0x41c64e6d + 0x3039, (seed >> 16) & 0x7fff over 32768.0), so
  // the `while (id == 0)` loop runs one, two and three times. Odd palettes start with +0x814 equal to the id their
  // seed produces, so Palette::apply 0x1006a4a0 returns at once; even ones rebuild both tables.
  const PAL_SEEDS = [0, 225070530, 1, 0x12345678, 0xffffffff, 0x80000000, 26579, 0x0badc0de, 7, 99, 0x7fffffff, 1234567];
  function lcgId(seed) {
    let s = seed >>> 0;
    for (;;) {
      s = (Math.imul(s, 0x41c64e6d) + 0x3039) >>> 0;
      const id = Math.trunc(((s >>> 16) & 0x7fff) / 32768 * 65535) & 0xffff;
      if (id) return id;
    }
  }
  function paletteFx() {
    const N = PAL_SEEDS.length, STRIDE = 0x820;
    const blk = __keepAlloc(N * STRIDE);
    fill(blk, N * STRIDE, 0x9a1e);
    const vt = va(0x1011db10);
    const out = { obj: blk, blk: blk };
    const byCounter = {};
    PAL_SEEDS.forEach(function (seed, i) {
      const p = blk.add(i * STRIDE);
      p.writePointer(vt);
      for (let k = 0; k < 0x400; k++) p.add(0xc + k).writeU8((k * 13 + i * 31) & 0xff);
      p.add(0x810).writeU32(0x5555 + i);
      p.add(0x814).writeU32(i & 1 ? lcgId(seed) : 0x7f000000 + i);
      byCounter[p.add(0x818).toString()] = seed;
      out['Q' + i] = p;
    });
    const s = __importStandIn({ module: 'jgld.dll', logSize: 0x400, imports: {
      'KERNEL32.dll!QueryPerformanceCounter': { args: 1, impl: function (a, ctx) {
        const seed = byCounter[a[0].toString()];
        a[0].writeU32(seed === undefined ? 0xdeadbeef : seed);
        a[0].add(4).writeU32(0x11);
        return 1;
      } } } });
    out.implog = s.log;
    return out;
  }

  // Display::setModeIndex: four Displays of our own (0x200 bytes, patterned), each with an object at +0x148 whose
  // vtable slot 0 records (this, width, height, bits, 1) into $modelog and returns 0x77. g_devmodes 0x101286f8 is
  // swapped, first thread frozen, for an array of four 0x94-byte DEVMODE records (buffer +4, count +0xc = 4) placed
  // two records into a block, so the negative indexes -1 / -2 read the fixture's own patterned records.
  // EnumDisplaySettingsA writes width `curW` (dmPelsWidth +0x6c), 480, 16 into the DEVMODE it is handed (this + 4);
  // ChangeDisplaySettingsA records dmSize, dmFields, bits, width, height and the flags and returns `cds`;
  // MessageBoxA records its four arguments. The 640 key swaps the module Display 0x10128420 too, for one whose slot
  // +0x20 returns the window handle 0x00c0ffee.
  function modeFx(curW, cds) {
    const blk = __keepAlloc(0x1000);
    fill(blk, 0x1000, 0x30de + curW + cds);
    const out = { obj: blk, blk: blk };
    const modelog = __keepAlloc(0x200);
    zero(modelog, 0x200);
    out.modelog = modelog;
    const vt = __keepAlloc(0x10);
    vt.writePointer(recThis(modelog, 4, 0x77));
    for (let i = 0; i < 4; i++) {
      const d = blk.add(i * 0x200);
      const o = blk.add(0x800 + i * 0x10);
      o.writePointer(vt);
      d.add(0x148).writePointer(o);
      out['D' + i] = d;
    }
    const arr = __keepAlloc(8 * 0x94);
    fill(arr, 8 * 0x94, 0xde40);
    for (let k = 0; k < 8; k++) {
      const r = arr.add(k * 0x94);
      r.add(0x68).writeU32([8, 16, 24, 32, 15, 16, 32, 8][k]);
      r.add(0x6c).writeU32([320, 640, 800, 1024, 1280, 1600, 512, 400][k]);
      r.add(0x70).writeU32([200, 480, 600, 768, 1024, 1200, 384, 300][k]);
    }
    out.arr = arr;
    const s = __importStandIn({ module: 'jgld.dll', logSize: 0x800, imports: {
      'USER32.dll!EnumDisplaySettingsA': { args: 3, record: [0, 1], impl: function (a) {
        a[2].add(0x6c).writeU32(curW); a[2].add(0x70).writeU32(480); a[2].add(0x68).writeU32(16);
        return 1;
      } },
      'USER32.dll!ChangeDisplaySettingsA': { args: 2, impl: function (a, ctx) {
        const d = a[0];
        ctx.record([d.add(0x24).readU16(), d.add(0x28).readU32(), d.add(0x68).readU32(), d.add(0x6c).readU32(),
                    d.add(0x70).readU32(), d.readU32(), a[1].toUInt32()]);
        return cds;
      } },
      'USER32.dll!MessageBoxA': { args: 4, ret: 1 },
    } });
    out.implog = s.log;
    const disp = __keepAlloc(0x40);
    const dvt = __keepAlloc(0x200);
    const trap = code([0xB8].concat(le(0x0badc0de), [0xC3]));
    for (let o = 0; o < 0x200; o += 4) dvt.add(o).writePointer(trap);
    dvt.add(0x20).writePointer(code([0xB8].concat(le(0x00c0ffee), [0xC3])));
    disp.writePointer(dvt);
    freezeOnce();
    const g = va(0x101286f8);
    __freeze.save(g, 0x10);
    g.add(4).writePointer(arr.add(2 * 0x94));
    g.add(0xc).writeU32(4);
    if (curW === 640) {
      __freeze.save(va(0x10128420), 4);
      va(0x10128420).writePointer(disp);
    }
    return out;
  }

  // Font::create: four Fonts of our own (0x40 bytes, patterned) on a vtable whose slot +0xc (reset) records (this) into
  // $fontlog; +4 points at a 0x20-byte metrics block of each; +0x14 flags (bit 0 set on F1 / F3), +0x18 a font
  // handle (F1 and F2). Every GDI call stood in: DeleteObject, CreateFontIndirectA (records LOGFONTA's first 0x1c
  // bytes and the face name as packed words, returns 0x0f000000 | (height & 0xffff), or 0 in the fail key),
  // SelectObject (records; remembers the object selected), GetTextMetricsA (writes TEXTMETRICA fields derived from
  // the remembered font handle: height h, ascent h - 3, descent 3, internal 2 + (h & 1), external h & 3) and
  // GetStockObject (returns 0x05705700 + its argument). The font DC 0x10128724 is only read (its value is recorded).
  function fontFx(fail) {
    const blk = __keepAlloc(0x400);
    fill(blk, 0x400, 0xf0c5);
    const out = { obj: blk, blk: blk };
    const fontlog = __keepAlloc(0x80);
    zero(fontlog, 0x80);
    out.fontlog = fontlog;
    const vt = __keepAlloc(0x20);
    const trap = code([0xB8].concat(le(0x0badc0de), [0xC3]));
    for (let o = 0; o < 0x20; o += 4) vt.add(o).writePointer(trap);
    vt.add(0xc).writePointer(recThis(fontlog, 0, 0x66));
    for (let i = 0; i < 4; i++) {
      const f = blk.add(i * 0x40);
      f.writePointer(vt);
      f.add(4).writePointer(blk.add(0x200 + i * 0x20));
      f.add(0x14).writeU32(i & 1 ? 0x101 : 0x100);
      f.add(0x18).writeU32(i === 1 || i === 2 ? 0x0f0f0000 + i : 0);
      out['F' + i] = f;
    }
    ['Arial', '', 'Times New Roman', 'abcdefghijklmnopqrstuvwxyz01234', 'x'].forEach(function (s, i) {
      out['f' + i] = blk.add(0x300 + i * 0x20);
      out['f' + i].writeUtf8String(s);
    });
    let selected = 0;
    const s = __importStandIn({ module: 'jgld.dll', logSize: 0x800, imports: {
      'GDI32.dll!DeleteObject': { args: 1, ret: 1 },
      'GDI32.dll!CreateFontIndirectA': { args: 1, impl: function (a, ctx) {
        const lf = a[0];
        const w = [];
        for (let o = 0; o < 0x1c; o += 4) w.push(lf.add(o).readU32());
        ctx.record(w.concat(strWords(lf.add(0x1c), 8)));
        return fail ? 0 : ((0x0f000000 | (lf.readU32() & 0xffff)) >>> 0);
      } },
      'GDI32.dll!SelectObject': { args: 2, impl: function (a) { selected = a[1].toUInt32(); return 0x0b100000; } },
      'GDI32.dll!GetTextMetricsA': { args: 2, record: [0], impl: function (a) {
        const h = (selected & 0x3f) + 8;
        const tm = a[1];
        tm.writeS32(h); tm.add(4).writeS32(h - 3); tm.add(8).writeS32(3);
        tm.add(0xc).writeS32(2 + (h & 1)); tm.add(0x10).writeS32(h & 3);
        return 1;
      } },
      'GDI32.dll!GetStockObject': { args: 1, impl: function (a) { return (0x05705700 + a[0].toUInt32()) >>> 0; } },
    } });
    out.implog = s.log;
    return out;
  }

  // strcpy: a 0x400-byte destination block (patterned 0xee, compared) and source strings at every alignment
  function strcpyFx() {
    const blk = __keepAlloc(0x800);
    for (let i = 0; i < 0x400; i++) blk.add(i).writeU8(0xee);
    const src = ['', 'a', 'ab', 'abc', 'abcd', 'abcde', 'Hello, world', 'x'.repeat(31), 'tab\tand\u0001ctl', '0123456789abcdef'];
    const out = { obj: blk, blk: blk };
    src.forEach(function (s, i) {
      const p = blk.add(0x400 + i * 0x40 + (i & 3));
      zero(blk.add(0x400 + i * 0x40), 0x40);
      p.writeUtf8String(s);
      out['s' + i] = p;
      out['d' + i] = blk.add(i * 0x40 + ((i * 3) & 3));
    });
    return out;
  }

  // ================================================================== group I: static initialisation stubs
  // jgld's _atexit 0x1007f080 is replaced for the key (Interceptor.replace, reverted from setTimeout(0) by a token,
  // the heap stand-in's pattern) by a recorder: on the A/B thread it appends the function pointer to $alog (count at
  // +0) and returns 0; any other thread is forwarded to the real _atexit. The module globals the constructors write
  // (the tracked list 0x10128428, 0x1c bytes; g_modes 0x101286c8, g_arrayE0 0x101286e0, g_devmodes 0x101286f8, all in
  // 0x101286c0..0x1012870f) are saved and filled with a per-key pattern with the first thread frozen, and written back
  // before it resumes; both regions are compared state.
  let atexitLive = null, atexitToken = 0;
  function atexitRecorder(alog) {
    const at = va(0x1007f080);
    if (atexitLive) { Interceptor.revert(at); atexitLive = null; }
    const tid = Process.getCurrentThreadId();
    const orig = new NativeFunction(at, 'int', ['pointer'], 'mscdecl');
    const cb = new NativeCallback(function (fn) {
      if (Process.getCurrentThreadId() !== tid) return orig(fn);
      const n = alog.readU32();
      if (n < 8) alog.add(4 + 4 * n).writePointer(fn);
      alog.writeU32(n + 1);
      return 0;
    }, 'int', ['pointer'], 'mscdecl');
    __diffKeepAlive.push(cb, orig);
    Interceptor.replace(at, cb);
    Interceptor.flush();
    const token = ++atexitToken;
    atexitLive = token;
    setTimeout(function () { if (atexitLive === token) { Interceptor.revert(at); Interceptor.flush(); atexitLive = null; } }, 0);
  }
  function initFx(seed) {
    const alog = __keepAlloc(0x40);
    zero(alog, 0x40);
    const out = { alog: alog, gt: va(0x10128428), ga: va(0x101286c0) };
    out.obj = alog;
    freezeOnce();
    __freeze.save(out.gt, 0x20);
    __freeze.save(out.ga, 0x50);
    fill(out.gt, 0x20, seed);
    fill(out.ga, 0x50, seed + 1);
    atexitRecorder(alog);
    return out;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3ds_gama() { return chunkFx(0x79cc0, 'gAMA', GAMA, 0x6701); },
    c3ds_srgb() { return chunkFx(0x7a7d0, 'sRGB', SRGB.slice(0, 17), 0x6702); },
    c3ds_srgb_chrm() { return chunkFx(0x7a7d0, 'sRGB', SRGB.slice(17), 0x670e); },
    c3ds_chrm() { return chunkFx(0x7a0d0, 'cHRM', CHRM, 0x6703); },
    c3ds_chrm2() { return chunkFx(0x7a0d0, 'cHRM', CHRM2, 0x6704); },
    c3ds_plte() { return chunkFx(0x799e0, 'PLTE', PLTE, 0x6705, {}); },
    c3ds_plte_oom() { return chunkFx(0x799e0, 'PLTE', PLTE.slice(5, 9), 0x6706, { failMask: 0x5 }); },
    c3ds_trns() { return chunkFx(0x7ab40, 'tRNS', TRNS, 0x6707, {}); },
    c3ds_trns_oom() { return chunkFx(0x7ab40, 'tRNS', TRNS.slice(3, 7), 0x6708, { failMask: 0x3 }); },
    c3ds_bkgd() { return chunkFx(0x7ae70, 'bKGD', BKGD, 0x6709); },
    c3ds_hist() { return chunkFx(0x7b1a0, 'hIST', HIST, 0x670a, {}); },
    c3ds_hist_oom() { return chunkFx(0x7b1a0, 'hIST', HIST.slice(5, 7).concat(HIST.slice(11)), 0x670b, { failMask: 0x5 }); },
    c3ds_text() { return chunkFx(0x7baf0, 'tEXt', TEXT, 0x670c, {}); },
    c3ds_text_oom() { return chunkFx(0x7baf0, 'tEXt', TEXT.slice(2, 5).concat(TEXT.slice(11, 13)), 0x670d, { failMask: 0x2a }); },
    c3ds_ztxt() { return chunkFx(0x7bc40, 'zTXt', ZTXT, 0x670f, { arenaSize: 0x40000 }); },
    // four inflate-initialised png_structs: their inflateInit2_ makes allocation calls 0..15 at fixture time, so each
    // vector's own calls start at 16. oom_all fails every call from 16 on (the key itself); oom_late lets the key
    // through and fails everything after it (inflate's code tables, the text blocks).
    c3ds_ztxt_oom_all() { return chunkFx(0x7bc40, 'zTXt', [ZTXT[2], ZTXT[3], ZTXT[7], ZTXT[5]], 0x6710,
      { arenaSize: 0x10000, failMask: 0xffff0000 }); },
    c3ds_ztxt_oom_late() { return chunkFx(0x7bc40, 'zTXt', [ZTXT[2], ZTXT[3], ZTXT[7], ZTXT[5]], 0x6711,
      { arenaSize: 0x10000, failMask: 0xfffe0000 }); },
    c3ds_read_info() { return chunkFx(0x6be20, 'IHDR', RINFO, 0x6712, { arenaSize: 0x2000 }); },
    c3ds_read_end() { return chunkFx(0x6cd00, 'IDAT', REND, 0x6713, { arenaSize: 0x2000 }); },
    c3ds_finish_row() { return chunkFx(0x7d280, 'IDAT', FROW, 0x6714, { arenaSize: 0x40000 }); },
    c3ds_read_row() { return rowsKey(ROWSPEC, 0x6715); },
    c3ds_read_row_skip() { return rowsKey(ROWSKIP, 0x6716); },
    c3ds_read_row_il() { return rowsKey(ROWREAD, 0x6717); },
    c3ds_read_image() { rowCb = null; const rows = __keepAlloc(6 * 0x100); fill(rows, 0x600, 0x6818);
                        return chunkFx(0x6cc50, 'IDAT', IMGSPEC, 0x6718, { arenaSize: 0x20000 }, rows); },
    c3ds_create() { return createFx(0, 0x6719); },
    c3ds_create_f0() { return createFx(0x1, 0x671a); },
    c3ds_create_f1() { return createFx(0x2, 0x671b); },
    c3ds_create_f2() { return createFx(0x4, 0x671c); },
    c3ds_create_f3() { return createFx(0x8, 0x671d); },
    c3ds_create_f5() { return createFx(0x20, 0x671e); },
    c3ds_blend_565() { return blendFx(1, true); },
    c3ds_blend_555() { return blendFx(0, true); },
    c3ds_blend_client() { return blendFx(1, true, null); },
    c3ds_blend_noclient() { return blendFx(0, false, null); },
    c3ds_clip_ok() { return clipFx(0x0e0f0001); },
    c3ds_clip_fail() { return clipFx(0); },
    c3ds_palette() { return paletteFx(); },
    c3ds_mode_ok() { return modeFx(800, 0); },
    c3ds_mode_fail() { return modeFx(1024, 1); },
    c3ds_mode_640() { return modeFx(640, 0); },
    c3ds_font_ok() { return fontFx(false); },
    c3ds_font_fail() { return fontFx(true); },
    c3ds_strcpy() { return strcpyFx(); },
    c3ds_init_a() { return initFx(0x1a11); },
    c3ds_init_b() { return initFx(0x2b22); },
    c3ds_init_c() { return initFx(0x3c33); },
    c3ds_pcal() { return chunkFx(0x7b620, 'pCAL', PCAL, 0x671f, { arenaSize: 0x2000 }); },
    c3ds_pcal_oom() { return chunkFx(0x7b620, 'pCAL', [PCAL[3], PCAL[4], PCAL[6], PCAL[12]], 0x6720,
                                     { arenaSize: 0x2000, failMask: 0x2a }); },
    c3ds_finish_row2() { return chunkFx(0x7d280, 'IDAT', FROW2, 0x6721, { arenaSize: 0x10000 }); },
    c3ds_read_row3() { return rowsKey(ROWSPEC3, 0x6722); },
    c3ds_err_jump() { return errWorld('jump'); },
    c3ds_err_ret() { return errWorld('ret'); },
    c3ds_err_null() { return errWorld('null'); },
  });

  // Negative controls (log/c3/alt/c3ds_negctl_gen.py): the negctl build exports the switch C3dsNegCtl (magic
  // 0x5147454e at +4) and keeps its wrong details dormant until the flag is 1. Every c3ds fixture ends by freezing the
  // game's first thread (once per key) and only then setting the flag (restored to 0 at the flush, before the resume),
  // so the game's own calls of live functions (setClip, Palette::update, Font::create, strcpy) never see a wrong body.
  // In the normal build there is no such export and nothing happens.
  function negOn() {
    if (__freeze.negCtlWords().length === 0) return;
    freezeOnce();
    __freeze.negOn();
  }
  Object.keys(globalThis.DIFF_FIXTURES).filter((k) => k.startsWith('c3ds_')).forEach(function (k) {
    const f = globalThis.DIFF_FIXTURES[k];
    globalThis.DIFF_FIXTURES[k] = function () {
      frozenKey = false;
      const out = f.apply(this, arguments);
      negOn();
      return out;
    };
  });

  globalThis.__c3ds = { crc32: crc32, be32: be32, be16: be16, world: world, catcher: catcher, NAME: NAME, bytes: bytes,
                        cblock: cblock, zero: zero, fill: fill, code: code, va: va, strWords: strWords };
})();
