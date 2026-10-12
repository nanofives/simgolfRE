// File stand-in for one A/B key (batch c3ee, 2026-10-12). The file counterpart of heapstandin.js / importstandin.js.
// Functions that open, map, read or write a file could not be A/B'd: the original arm would read the real disk (game data
// that differs from install to install, or nothing at all for a fixture name), a write would land on disk where the second
// arm cannot see it, and the file cursor the first arm moved is not restored before the second. For the length of one key
// this serves FIXTURE FILES held in one memory block:
//   - the module's KERNEL32 imports (CreateFileA, GetFileSize, CreateFileMappingA, MapViewOfFile, UnmapViewOfFile,
//     CloseHandle, ReadFile, WriteFile, SetFilePointer, GetLastError) through importstandin.js (MappedFile, the exe's
//     loggers): a mapped view is a pointer INTO the block;
//   - the static CRT's FILE layer at the entry points listed in `crt` (fopen, _fsopen, fclose, fread, fwrite, fgets, fseek,
//     rewind, fflush, _filbuf, _flsbuf): a FILE * is a stream record inside the block, laid out like the VC6 FILE.
// Everything a file operation changes lives in the block: each file's bytes and length, each stream's cursor and EOF / error
// flags, each Win32 handle, the last error and an operation log. Put the whole block in the key's state regions and
// diff_hook restores it between the arms, so both arms open the same files at the same cursors, and every byte one arm
// writes, every cursor it moves and the order of the operations are compared state. A body that reads one byte too many,
// skips a close or writes one wrong byte reads RED on state alone (log/c3/alt/c3ee_negctl.py).
//
//   const fs = __fileStandIn({ module: 'golf_clean.exe', kernel32: true, crt: ['fopen', 'fread', 'fclose'],
//       files: { 'art\\a.pcx': bytes,                                  // bytes: Array / Uint8Array / ArrayBuffer / string
//                'out.pcx': { bytes: null, capacity: 0x2000 },         // absent, but fopen 'w' / CREATE_ALWAYS creates it
//                'hdr.pcx': { bytes: [...], mask: [[0x10, 0x30]] } },  // writes into file offsets 0x10..0x3f are stored as 0
//       imports: { 'USER32.dll!MessageBoxA': { args: 4, ret: 1 } } }); // more imports in the same (single) import stand-in
//   fixture.fsblock = fs.block;  fixture.implog = fs.implog;
//   // state: [('$fsblock', 0, FS_SIZE), ('$implog', 0, 0x400)]    (fs.size is the block size; fs.implogSize the log's)
//
// ONE CALL PER KEY, and it makes the key's one __importStandIn call itself (when kernel32 or imports are given): pass every
// other import the key needs in `imports`. A name that is not in `files` does not exist (CreateFileA returns -1 with
// GetLastError 2, fopen NULL); names are compared case-insensitively with '/' read as '\\' (opts.basename: only the part
// after the last '\\' is compared). Only the thread that called __fileStandIn (the rpc thread that runs both arms) is served;
// any other thread entering a replaced CRT routine is forwarded to the real one, and the import stubs forward other threads
// natively (importstandin.js). A call on the A/B thread with a FILE * or handle that is not one of ours never reaches the real
// CRT or KERNEL32: it is logged (op | 0x80) and fails (NULL / -1 / 0, the routine's error value).
//
// Freeze first: replacing a CRT routine puts a JavaScript callback in front of it, and a game thread that enters it while the
// stand-in is installed waits for the JS lock (c3dl's race). Fixtures call __freeze.thread() before __fileStandIn whenever
// `crt` is given (the c3ee fixtures do), so the game's first thread is held at a safe point for the key.
//
// The CRT layer follows the VC6 runtime's contracts, not its buffering: fread / fwrite move size * count bytes and return
// the number of COMPLETE items (a short read still copies the bytes it got, as the CRT does); text mode (no 'b' in the mode)
// turns CR LF into LF and stops at ^Z on input, and LF into CR LF on output; fgets copies up to n - 1 characters, stops after
// a newline, and returns NULL when it read nothing at end of file; fseek clears EOF; _filbuf / _flsbuf leave the FILE's
// _cnt at 0 so the getc / putc macros call them for every character. Writes past a file's capacity are short writes and set
// the stream's error flag. A Win32 view is the file's bytes themselves: what a body writes through a view lands in the file.
//
// BLOCK layout (fs.size bytes, every byte state):
//   +0x000 header: 'FSI1', file count, operations served, op-log byte cursor, last error, masked bytes, opens ok / failed
//   +0x040 files   (16 x 0x20): exists, length, capacity, data offset, name crc32, opens, bytes read, bytes written
//   +0x240 streams (16 x 0x40):  a VC6 FILE {_ptr, _cnt, _base, _flag, _file, _charbuf, _bufsiz, _tmpfname}, then
//                               in use, file index, cursor, mode bits (1 read, 2 write, 4 append, 8 binary), eof, error, serial
//   +0x640 handles (8 x 0x10):  kind (1 file, 2 mapping), file index, cursor, open
//   +0x6c0 op log  (opLogSize, default 0x800): records {op, a, b, c}, 16 bytes each, op codes in OP below
//   then each file's data (capacity bytes, 16-aligned)
(function () {
  // golf_clean.exe's static CRT (VC6), entry points from re/functions_ghidra.tsv (FID names; 0x004a614d is fopen: it pushes
  // _SH_DENYNO and calls _fsopen 0x004a611c, as c3dz found)
  const CRT = {
    'golf_clean.exe': {
      fopen: 0x004a614d, _fsopen: 0x004a611c, fclose: 0x004a609f, fread: 0x004a63a1, fwrite: 0x004a6268,
      fgets: 0x004a65ee, fseek: 0x004a7119, rewind: 0x004a70b5, fflush: 0x004a9943, _filbuf: 0x004aa9f5, _flsbuf: 0x004aa5a2,
    },
  };
  const OP = {
    CreateFileA: 1, GetFileSize: 2, CreateFileMappingA: 3, MapViewOfFile: 4, UnmapViewOfFile: 5, CloseHandle: 6,
    ReadFile: 7, WriteFile: 8, SetFilePointer: 9, GetLastError: 10,
    fopen: 0x11, _fsopen: 0x11, fclose: 0x12, fread: 0x13, fwrite: 0x14, fgets: 0x15, fseek: 0x16, rewind: 0x17,
    fflush: 0x18, _filbuf: 0x19, _flsbuf: 0x1a,
  };
  const FOREIGN = 0x80;
  const NFILES = 16, NSTREAMS = 16, NHANDLES = 8;
  const O_FILES = 0x40, O_STREAMS = 0x240, O_HANDLES = 0x640, O_LOG = 0x6c0;
  const H_FILE = 0x46480000, H_MAP = 0x4d480000;            // handle values: 'FH' / 'MH' + slot (never real handles)
  const crtLive = {};
  let crtToken = 1;

  function crc32(s) {
    let c = 0xffffffff;
    for (let i = 0; i < s.length; i++) {
      c ^= s.charCodeAt(i) & 0xff;
      for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1));
    }
    return (c ^ 0xffffffff) >>> 0;
  }
  function toBytes(b) {
    if (b === null || b === undefined) return null;
    if (typeof b === 'string') { const a = new Uint8Array(b.length); for (let i = 0; i < b.length; i++) a[i] = b.charCodeAt(i) & 0xff; return a; }
    if (b instanceof ArrayBuffer) return new Uint8Array(b);
    return Uint8Array.from(b);
  }

  globalThis.__fileStandIn = function (opts) {
    const module = opts.module || 'golf_clean.exe';
    const tid = Process.getCurrentThreadId();
    const opLogSize = opts.opLogSize || 0x800;
    const norm = function (s) {
      s = s.toLowerCase().replace(/\//g, '\\');
      if (opts.basename) s = s.slice(s.lastIndexOf('\\') + 1);
      return s;
    };
    // ---- the block
    const names = Object.keys(opts.files || {});
    if (names.length > NFILES) throw new Error('file stand-in: at most ' + NFILES + ' files per key');
    const specs = names.map(function (n) {
      const v = opts.files[n];
      const plain = v === null || typeof v === 'string' || v instanceof ArrayBuffer || Array.isArray(v) || v instanceof Uint8Array;
      const sp = plain ? { bytes: v } : v;
      const bytes = toBytes(sp.bytes);
      const cap = Math.max(sp.capacity || 0, bytes ? bytes.length : 0, 0x10);
      return { name: n, key: norm(n), bytes: bytes, cap: (cap + 15) & ~15, mask: sp.mask || [] };
    });
    let dataOff = (O_LOG + opLogSize + 15) & ~15;
    specs.forEach(function (sp) { sp.off = dataOff; dataOff += sp.cap; });
    // opts.size pads the block to a fixed size, so a registry entry can name one state region for every key of a family
    if (opts.size && opts.size < dataOff) throw new Error('file stand-in: files need 0x' + dataOff.toString(16) + ' bytes, size is 0x' + opts.size.toString(16));
    const size = Math.max(dataOff, opts.size || 0);
    const blk = __keepAlloc(size);
    blk.writeByteArray(new Uint8Array(size).buffer);
    blk.writeU32(0x31495346);
    blk.add(4).writeU32(specs.length);
    specs.forEach(function (sp, i) {
      const e = blk.add(O_FILES + 0x20 * i);
      e.writeU32(sp.bytes ? 1 : 0);
      e.add(4).writeU32(sp.bytes ? sp.bytes.length : 0);
      e.add(8).writeU32(sp.cap);
      e.add(0xc).writeU32(sp.off);
      e.add(0x10).writeU32(crc32(sp.key));
      if (sp.bytes && sp.bytes.length) blk.add(sp.off).writeByteArray(sp.bytes.buffer.slice(sp.bytes.byteOffset, sp.bytes.byteOffset + sp.bytes.length));
    });
    const U = function (o) { return blk.add(o).readU32(); };
    const W = function (o, v) { blk.add(o).writeU32(v >>> 0); };
    const fileE = function (i) { return O_FILES + 0x20 * i; };
    const streamE = function (i) { return O_STREAMS + 0x40 * i; };
    const handleE = function (i) { return O_HANDLES + 0x10 * i; };
    function log(op, a, b, c) {
      W(8, U(8) + 1);
      const cur = U(0xc);
      if (cur + 16 > opLogSize) return;
      const p = O_LOG + cur;
      W(p, op); W(p + 4, a | 0); W(p + 8, b | 0); W(p + 12, c | 0);
      W(0xc, cur + 16);
    }
    function findFile(namePtr) {
      if (ptr(namePtr).isNull()) return -1;
      const k = norm(ptr(namePtr).readCString() || '');
      for (let i = 0; i < specs.length; i++) if (specs[i].key === k) return i;
      return -1;
    }
    function nameCrc(namePtr) { return ptr(namePtr).isNull() ? 0 : crc32(norm(ptr(namePtr).readCString() || '')); }
    const len = function (fi) { return U(fileE(fi) + 4); };
    const cap = function (fi) { return U(fileE(fi) + 8); };
    const data = function (fi) { return blk.add(U(fileE(fi) + 0xc)); };
    function truncate(fi) { W(fileE(fi), 1); W(fileE(fi) + 4, 0); }
    // read up to n raw bytes at pos; text mode turns CR LF into LF and stops at ^Z. Returns {bytes, consumed}.
    function readAt(fi, pos, n, text, stopAtNl) {
      const L = len(fi), out = [];
      let p = pos;
      while (out.length < n && p < L) {
        const b = data(fi).add(p).readU8();
        if (text && b === 0x1a) break;
        if (text && b === 0x0d && p + 1 < L && data(fi).add(p + 1).readU8() === 0x0a) { p++; continue; }
        out.push(b); p++;
        if (stopAtNl && b === 0x0a) break;
      }
      W(fileE(fi) + 0x18, U(fileE(fi) + 0x18) + out.length);
      return { bytes: out, consumed: p - pos };
    }
    // write bytes at pos (append: at the end); text mode turns LF into CR LF. Returns {written source bytes, raw}.
    function writeAt(fi, pos, src, text) {
      const sp = specs[fi], C = cap(fi);
      let p = pos, n = 0;
      for (let i = 0; i < src.length; i++) {
        const seq = (text && src[i] === 0x0a) ? [0x0d, 0x0a] : [src[i]];
        if (p + seq.length > C) break;
        seq.forEach(function (b) {
          const masked = sp.mask.some(function (m) { return p >= m[0] && p < m[0] + m[1]; });
          if (masked) W(0x14, U(0x14) + 1);
          data(fi).add(p).writeU8(masked ? 0 : b);
          p++;
        });
        n++;
      }
      if (p > len(fi)) W(fileE(fi) + 4, p);
      W(fileE(fi) + 0x1c, U(fileE(fi) + 0x1c) + n);
      return { n: n, raw: p - pos };
    }

    // ---- Win32 layer
    function newHandle(kind, fi) {
      for (let i = 0; i < NHANDLES; i++) {
        const e = handleE(i);
        if (!U(e + 0xc)) { W(e, kind); W(e + 4, fi); W(e + 8, 0); W(e + 0xc, 1); return i; }
      }
      return -1;
    }
    function handleSlot(h, kind) {
      h = h >>> 0;
      const base = kind === 1 ? H_FILE : H_MAP;
      const i = h - base;
      if (i < 0 || i >= NHANDLES || !U(handleE(i) + 0xc) || U(handleE(i)) !== kind) return -1;
      return i;
    }
    const setErr = function (e) { W(0x10, e); };
    const k32 = {
      // CreateFileA(name, access, share, sa, disposition, flags, template)
      'KERNEL32.dll!CreateFileA': function (a, ctx) {
        const fi = findFile(a[0]), disp = a[4].toUInt32(), acc = a[1].toUInt32();
        ctx.record([nameCrc(a[0]), acc, a[2].toUInt32(), disp, a[5].toUInt32()]);
        let ok = fi >= 0 && (U(fileE(fi)) || disp === 1 || disp === 2 || disp === 4);   // CREATE_NEW / ALWAYS, OPEN_ALWAYS
        if (ok && disp === 1 && U(fileE(fi))) { ok = false; setErr(80); }              // ERROR_FILE_EXISTS
        const hi = ok ? newHandle(1, fi) : -1;
        if (hi < 0) {
          if (U(0x10) !== 80) setErr(2);                                                 // ERROR_FILE_NOT_FOUND
          W(0x1c, U(0x1c) + 1);
          log(OP.CreateFileA, fi, -1, disp);
          return 0xffffffff;
        }
        if (disp === 2 || disp === 5 || !U(fileE(fi))) truncate(fi);                    // CREATE_ALWAYS / TRUNCATE_EXISTING
        W(fileE(fi) + 0x14, U(fileE(fi) + 0x14) + 1);
        W(0x18, U(0x18) + 1);
        setErr(0);
        log(OP.CreateFileA, fi, hi, disp);
        return H_FILE + hi;
      },
      // GetFileSize(file, high)
      'KERNEL32.dll!GetFileSize': function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[1].isNull() ? 0 : 1]);
        const hi = handleSlot(a[0].toUInt32(), 1);
        if (hi < 0) { log(OP.GetFileSize | FOREIGN, a[0].toUInt32(), 0, 0); setErr(6); return 0xffffffff; }
        const fi = U(handleE(hi) + 4);
        if (!a[1].isNull()) a[1].writeU32(0);
        log(OP.GetFileSize, fi, len(fi), 0);
        return len(fi);
      },
      // CreateFileMappingA(file, sa, protect, sizeHigh, sizeLow, name): a 0-byte file cannot be mapped (Windows answers
      // ERROR_FILE_INVALID 1006)
      'KERNEL32.dll!CreateFileMappingA': function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[2].toUInt32(), a[3].toUInt32(), a[4].toUInt32()]);
        const hi = handleSlot(a[0].toUInt32(), 1);
        if (hi < 0) { log(OP.CreateFileMappingA | FOREIGN, a[0].toUInt32(), 0, 0); setErr(6); return 0; }
        const fi = U(handleE(hi) + 4);
        if (len(fi) === 0 && a[4].toUInt32() === 0) { setErr(1006); log(OP.CreateFileMappingA, fi, -1, 0); return 0; }
        const mi = newHandle(2, fi);
        if (mi < 0) { setErr(8); log(OP.CreateFileMappingA, fi, -1, 8); return 0; }
        log(OP.CreateFileMappingA, fi, mi, a[2].toUInt32());
        return H_MAP + mi;
      },
      // MapViewOfFile(mapping, access, offHigh, offLow, bytes): the view IS the file's data in the block
      'KERNEL32.dll!MapViewOfFile': function (a) {
        const mi = handleSlot(a[0].toUInt32(), 2);
        if (mi < 0) { log(OP.MapViewOfFile | FOREIGN, a[0].toUInt32(), 0, 0); setErr(6); return 0; }
        const fi = U(handleE(mi) + 4), off = a[3].toUInt32();
        if (off > len(fi)) { setErr(87); log(OP.MapViewOfFile, fi, -1, off); return 0; }
        W(handleE(mi) + 8, U(handleE(mi) + 8) + 1);                       // views made from this mapping
        log(OP.MapViewOfFile, fi, off, a[1].toUInt32());
        return data(fi).add(off);
      },
      'KERNEL32.dll!UnmapViewOfFile': function (a) {
        const p = a[0];
        for (let fi = 0; fi < specs.length; fi++) {
          const d = data(fi);
          if (p.compare(d) >= 0 && p.compare(d.add(cap(fi))) < 0) { log(OP.UnmapViewOfFile, fi, p.sub(d).toInt32(), 0); return 1; }
        }
        log(OP.UnmapViewOfFile | FOREIGN, 0, 0, 0);
        setErr(487);
        return 0;
      },
      'KERNEL32.dll!CloseHandle': function (a) {
        const h = a[0].toUInt32();
        let i = handleSlot(h, 1);
        if (i < 0) i = handleSlot(h, 2);
        if (i < 0) { log(OP.CloseHandle | FOREIGN, h, 0, 0); setErr(6); return 0; }
        W(handleE(i) + 0xc, 0);
        log(OP.CloseHandle, U(handleE(i) + 4), U(handleE(i)), 0);
        return 1;
      },
      // ReadFile(file, buf, n, &read, overlapped)
      'KERNEL32.dll!ReadFile': { args: 5, impl: function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[2].toUInt32()]);
        const hi = handleSlot(a[0].toUInt32(), 1);
        if (hi < 0) { log(OP.ReadFile | FOREIGN, a[0].toUInt32(), 0, 0); setErr(6); return 0; }
        const fi = U(handleE(hi) + 4), pos = U(handleE(hi) + 8);
        const r = readAt(fi, pos, a[2].toUInt32(), false, false);
        if (r.bytes.length) a[1].writeByteArray(r.bytes);
        W(handleE(hi) + 8, pos + r.consumed);
        if (!a[3].isNull()) a[3].writeU32(r.bytes.length);
        log(OP.ReadFile, fi, pos, r.bytes.length);
        return 1;
      } },
      // WriteFile(file, buf, n, &written, overlapped)
      'KERNEL32.dll!WriteFile': { args: 5, impl: function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[2].toUInt32()]);
        const hi = handleSlot(a[0].toUInt32(), 1);
        if (hi < 0) { log(OP.WriteFile | FOREIGN, a[0].toUInt32(), 0, 0); setErr(6); return 0; }
        const fi = U(handleE(hi) + 4), pos = U(handleE(hi) + 8), n = a[2].toUInt32();
        const src = n ? new Uint8Array(a[1].readByteArray(n)) : new Uint8Array(0);
        const w = writeAt(fi, pos, src, false);
        W(handleE(hi) + 8, pos + w.raw);
        if (!a[3].isNull()) a[3].writeU32(w.n);
        log(OP.WriteFile, fi, pos, w.n);
        if (w.n < n) { setErr(112); return 0; }                          // ERROR_DISK_FULL
        return 1;
      } },
      // SetFilePointer(file, distance, &high, method)
      'KERNEL32.dll!SetFilePointer': function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[1].toUInt32(), a[2].isNull() ? 0 : 1, a[3].toUInt32()]);
        const hi = handleSlot(a[0].toUInt32(), 1);
        if (hi < 0) { log(OP.SetFilePointer | FOREIGN, a[0].toUInt32(), 0, 0); setErr(6); return 0xffffffff; }
        const fi = U(handleE(hi) + 4), m = a[3].toUInt32();
        const base = m === 0 ? 0 : m === 1 ? U(handleE(hi) + 8) : len(fi);
        const np = base + a[1].toInt32();
        if (np < 0) { setErr(131); log(OP.SetFilePointer, fi, -1, m); return 0xffffffff; }
        if (!a[2].isNull()) a[2].writeU32(0);
        W(handleE(hi) + 8, np);
        log(OP.SetFilePointer, fi, np, m);
        return np;
      },
      'KERNEL32.dll!GetLastError': function () { log(OP.GetLastError, U(0x10), 0, 0); return U(0x10); },
    };

    // ---- CRT layer
    const IOREAD = 1, IOWRT = 2, IOEOF = 0x10, IOERR = 0x20, IORW = 0x80;
    function streamOf(f) {
      const p = ptr(f);
      const o = p.sub(blk).toInt32();
      if (p.compare(blk) < 0 || o < O_STREAMS || o >= O_STREAMS + 0x40 * NSTREAMS || (o - O_STREAMS) % 0x40) return -1;
      const i = (o - O_STREAMS) / 0x40;
      return U(streamE(i) + 0x20) ? i : -1;
    }
    function syncFlag(si) {
      const e = streamE(si), m = U(e + 0x2c);
      let fl = (m & 1 ? IOREAD : 0) | (m & 2 ? IOWRT : 0) | ((m & 3) === 3 ? IORW : 0);
      if (U(e + 0x30)) fl |= IOEOF;
      if (U(e + 0x34)) fl |= IOERR;
      W(e + 0xc, fl);
      W(e + 0, 0); W(e + 4, 0); W(e + 8, 0);                              // _ptr / _cnt / _base: the macros call us every time
    }
    function openStream(namePtr, modePtr) {
      const mode = ptr(modePtr).isNull() ? '' : (ptr(modePtr).readCString() || '');
      const fi = findFile(namePtr);
      const crc = nameCrc(namePtr);
      let bits = 0;
      const c0 = mode.charAt(0);
      if (c0 === 'r') bits = 1; else if (c0 === 'w') bits = 2; else if (c0 === 'a') bits = 2 | 4;
      if (mode.indexOf('+') >= 0) bits |= 3;
      if (mode.indexOf('b') >= 0) bits |= 8;
      let si = -1;
      if (fi >= 0 && bits && (c0 !== 'r' || U(fileE(fi)))) {
        for (let i = 0; i < NSTREAMS; i++) if (!U(streamE(i) + 0x20)) { si = i; break; }
      }
      if (si < 0) { W(0x1c, U(0x1c) + 1); log(OP.fopen, fi, -1, crc); return ptr(0); }
      if (c0 === 'w' || !U(fileE(fi))) truncate(fi);
      const e = streamE(si);
      blk.add(e).writeByteArray(new Uint8Array(0x40).buffer);
      W(e + 0x10, 0x50 + si);                                              // _file
      W(e + 0x20, 1); W(e + 0x24, fi); W(e + 0x28, c0 === 'a' ? len(fi) : 0); W(e + 0x2c, bits);
      W(e + 0x38, U(8) + 1);
      syncFlag(si);
      W(fileE(fi) + 0x14, U(fileE(fi) + 0x14) + 1);
      W(0x18, U(0x18) + 1);
      log(OP.fopen, fi, si, crc);
      return blk.add(e);
    }
    const impl = {
      fopen: [['pointer', 'pointer'], 'pointer', function (n, m) { return openStream(n, m); }],
      _fsopen: [['pointer', 'pointer', 'int'], 'pointer', function (n, m) { return openStream(n, m); }],
      fclose: [['pointer'], 'int', function (f) {
        const si = streamOf(f);
        if (si < 0) { log(OP.fclose | FOREIGN, 0, 0, 0); return -1; }
        const e = streamE(si), err = U(e + 0x34);
        log(OP.fclose, U(e + 0x24), si, U(e + 0x28));
        blk.add(e).writeByteArray(new Uint8Array(0x40).buffer);
        return err ? -1 : 0;
      }],
      fread: [['pointer', 'uint32', 'uint32', 'pointer'], 'uint32', function (buf, sz, cnt, f) {
        const si = streamOf(f);
        if (si < 0) { log(OP.fread | FOREIGN, sz, cnt, 0); return 0; }
        const e = streamE(si), fi = U(e + 0x24), total = (sz * cnt) >>> 0;
        if (!total) { log(OP.fread, fi, 0, 0); return 0; }
        if (!(U(e + 0x2c) & 1)) { W(e + 0x34, 1); syncFlag(si); log(OP.fread, fi, -1, total); return 0; }
        const pos = U(e + 0x28);
        const r = readAt(fi, pos, total, !(U(e + 0x2c) & 8), false);
        if (r.bytes.length) buf.writeByteArray(r.bytes);
        W(e + 0x28, pos + r.consumed);
        if (r.bytes.length < total) W(e + 0x30, 1);
        syncFlag(si);
        log(OP.fread, fi, total, r.bytes.length);
        return Math.floor(r.bytes.length / sz);
      }],
      fwrite: [['pointer', 'uint32', 'uint32', 'pointer'], 'uint32', function (buf, sz, cnt, f) {
        const si = streamOf(f);
        if (si < 0) { log(OP.fwrite | FOREIGN, sz, cnt, 0); return 0; }
        const e = streamE(si), fi = U(e + 0x24), total = (sz * cnt) >>> 0;
        if (!total) { log(OP.fwrite, fi, 0, 0); return 0; }
        if (!(U(e + 0x2c) & 2)) { W(e + 0x34, 1); syncFlag(si); log(OP.fwrite, fi, -1, total); return 0; }
        const pos = (U(e + 0x2c) & 4) ? len(fi) : U(e + 0x28);
        const w = writeAt(fi, pos, new Uint8Array(buf.readByteArray(total)), !(U(e + 0x2c) & 8));
        W(e + 0x28, pos + w.raw);
        if (w.n < total) W(e + 0x34, 1);
        syncFlag(si);
        log(OP.fwrite, fi, total, w.n);
        return Math.floor(w.n / sz);
      }],
      fgets: [['pointer', 'int', 'pointer'], 'pointer', function (buf, n, f) {
        const si = streamOf(f);
        if (si < 0) { log(OP.fgets | FOREIGN, n, 0, 0); return ptr(0); }
        const e = streamE(si), fi = U(e + 0x24);
        if (n <= 0 || !(U(e + 0x2c) & 1)) { log(OP.fgets, fi, n, -1); return ptr(0); }
        const pos = U(e + 0x28);
        const r = readAt(fi, pos, n - 1, !(U(e + 0x2c) & 8), true);
        W(e + 0x28, pos + r.consumed);
        if (!r.bytes.length) { W(e + 0x30, 1); syncFlag(si); log(OP.fgets, fi, n, 0); return ptr(0); }
        if (r.bytes.length < n - 1 && r.bytes[r.bytes.length - 1] !== 0x0a) W(e + 0x30, 1);
        buf.writeByteArray(r.bytes.concat([0]));
        syncFlag(si);
        log(OP.fgets, fi, n, r.bytes.length);
        return buf;
      }],
      fseek: [['pointer', 'int', 'int'], 'int', function (f, off, org) {
        const si = streamOf(f);
        if (si < 0) { log(OP.fseek | FOREIGN, off, org, 0); return -1; }
        const e = streamE(si), fi = U(e + 0x24);
        const base = org === 0 ? 0 : org === 1 ? U(e + 0x28) : org === 2 ? len(fi) : -1;
        const np = base + off;
        if (base < 0 || np < 0) { log(OP.fseek, fi, off, -1); return -1; }
        W(e + 0x28, np); W(e + 0x30, 0);
        syncFlag(si);
        log(OP.fseek, fi, off, org);
        return 0;
      }],
      rewind: [['pointer'], 'void', function (f) {
        const si = streamOf(f);
        if (si < 0) { log(OP.rewind | FOREIGN, 0, 0, 0); return; }
        const e = streamE(si);
        W(e + 0x28, 0); W(e + 0x30, 0); W(e + 0x34, 0);
        syncFlag(si);
        log(OP.rewind, U(e + 0x24), 0, 0);
      }],
      fflush: [['pointer'], 'int', function (f) {
        if (ptr(f).isNull()) { log(OP.fflush, -1, 0, 0); return 0; }
        const si = streamOf(f);
        if (si < 0) { log(OP.fflush | FOREIGN, 0, 0, 0); return -1; }
        const e = streamE(si);
        log(OP.fflush, U(e + 0x24), 0, 0);
        return U(e + 0x34) ? -1 : 0;
      }],
      _filbuf: [['pointer'], 'int', function (f) {
        const si = streamOf(f);
        if (si < 0) { log(OP._filbuf | FOREIGN, 0, 0, 0); return -1; }
        const e = streamE(si), fi = U(e + 0x24), pos = U(e + 0x28);
        const r = (U(e + 0x2c) & 1) ? readAt(fi, pos, 1, !(U(e + 0x2c) & 8), false) : { bytes: [], consumed: 0 };
        W(e + 0x28, pos + r.consumed);
        if (!r.bytes.length) W(e + 0x30, 1);
        syncFlag(si);
        log(OP._filbuf, fi, pos, r.bytes.length ? r.bytes[0] : -1);
        return r.bytes.length ? r.bytes[0] : -1;
      }],
      _flsbuf: [['int', 'pointer'], 'int', function (ch, f) {
        const si = streamOf(f);
        if (si < 0) { log(OP._flsbuf | FOREIGN, ch, 0, 0); return -1; }
        const e = streamE(si), fi = U(e + 0x24);
        if (!(U(e + 0x2c) & 2)) { W(e + 0x34, 1); syncFlag(si); log(OP._flsbuf, fi, ch, -1); return -1; }
        const pos = (U(e + 0x2c) & 4) ? len(fi) : U(e + 0x28);
        const w = writeAt(fi, pos, new Uint8Array([ch & 0xff]), !(U(e + 0x2c) & 8));
        W(e + 0x28, pos + w.raw);
        if (!w.n) W(e + 0x34, 1);
        syncFlag(si);
        log(OP._flsbuf, fi, ch & 0xff, w.n);
        return w.n ? (ch & 0xff) : -1;
      }],
    };

    // ---- install the CRT replacements (a token per install; a key's timer only reverts its own, heapstandin.js's pattern)
    const table = CRT[module];
    const base = Process.getModuleByName(module).base;
    const token = crtToken++;
    (opts.crt || []).forEach(function (c) {
      let name = c, va;
      if (typeof c === 'number') {
        va = c < base.toUInt32() ? base.add(c).toUInt32() : c;
        name = table && Object.keys(table).find(function (k) { return table[k] === va; });
        if (!name) throw new Error('file stand-in: no CRT routine of ' + module + ' at 0x' + va.toString(16));
      } else {
        if (!table || table[c] === undefined) throw new Error('file stand-in: unknown CRT routine ' + c + ' of ' + module);
        va = table[c];
      }
      const sig = impl[name];
      const at = ptr(va), k = at.toString();
      if (crtLive[k]) {
        try { Interceptor.revert(at); } catch (e) { /* already reverted */ }
        delete crtLive[k];
        Interceptor.flush();               // commit the revert before the new replace (c3dl: a replace in the same turn was lost)
      }
      const orig = new NativeFunction(at, sig[1], sig[0]);
      const cb = new NativeCallback(function () {
        if (Process.getCurrentThreadId() !== tid) return orig.apply(null, arguments);
        return sig[2].apply(null, arguments);
      }, sig[1], sig[0]);
      __diffKeepAlive.push(cb, orig);
      Interceptor.replace(at, cb);
      crtLive[k] = token;
    });
    if ((opts.crt || []).length) {
      Interceptor.flush();
      setTimeout(function () {
        Object.keys(crtLive).forEach(function (k) {
          if (crtLive[k] !== token) return;
          try { Interceptor.revert(ptr(k)); } catch (e) { /* gone */ }
          delete crtLive[k];
        });
        Interceptor.flush();
      }, 0);
    }

    // ---- the key's single import stand-in: KERNEL32 file imports plus the fixture's own
    let imp = null;
    const mods = opts.kernel32 === true ? [module] : (Array.isArray(opts.kernel32) ? opts.kernel32 : []);
    const imports = {};
    const want = opts.k32Imports || ['CreateFileA', 'GetFileSize', 'CreateFileMappingA', 'MapViewOfFile', 'UnmapViewOfFile',
                                     'CloseHandle', 'GetLastError'];
    mods.forEach(function (m) {
      want.forEach(function (api) {
        const key = 'KERNEL32.dll!' + api;
        imports[(m === module ? '' : m + ':') + key] = k32[key];
      });
    });
    Object.keys(opts.imports || {}).forEach(function (k) { imports[k] = opts.imports[k]; });
    if (Object.keys(imports).length) imp = __importStandIn({ module: module, imports: imports, logSize: opts.implogSize || 0x400 });

    return {
      block: blk, size: size, implog: imp ? imp.log : null, implogSize: imp ? (opts.implogSize || 0x400) : 0, imp: imp,
      // a FILE * opened by the fixture itself (for functions that take an open stream): the same path as the stood-in
      // fopen, logged like it, without going through the CRT
      fopen: function (name, mode) { return openStream(Memory.allocUtf8String(name), Memory.allocUtf8String(mode)); },
      data: function (n) { return data(names.indexOf(n)); },
      length: function (n) { return len(names.indexOf(n)); },
      file: function (n) { const i = names.indexOf(n); return { index: i, data: data(i), entry: blk.add(fileE(i)) }; },
      k32: k32,
    };
  };
  globalThis.__fileStandIn.CRT = CRT;
  globalThis.__fileStandIn.OP = OP;
})();
