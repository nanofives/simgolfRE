// mmio stand-in for one A/B key (batch c3fg, 2026-10-13). The winmm multimedia-file counterpart of filestandin.js.
// sound.dll reads its RIFF / WAVE and MIDI files through winmm's mmio layer (mmioOpenA, mmioDescend, mmioRead, mmioGetInfo /
// mmioAdvance direct buffer access ...). Those functions could not be A/B'd: the original arm would open the real disk (or
// nothing at all for a fixture name), an mmio handle and its buffer state live inside winmm where the second arm cannot see
// what the first arm moved, and the buffer winmm allocates has a different address in each arm. For the length of one key
// this serves FIXTURE FILES held in one memory block to the module's WINMM imports, through importstandin.js:
//   mmioOpenA mmioClose mmioRead mmioWrite mmioSeek mmioDescend mmioAscend mmioGetInfo mmioSetInfo mmioAdvance
//   mmioSetBuffer mmioStringToFOURCCA                                    (sound.dll imports all twelve, 0x100b8624-0x100b8670)
// Everything an mmio call changes lives in the block: each file's bytes and length, each handle's buffer state (lBufOffset,
// lDiskOffset, pchNext / pchEndRead / pchEndWrite as offsets, dwFlags), the buffers mmio allocates (fixed slots inside the
// block, so both arms see the same pchBuffer) and an operation log. Put the whole block in the key's state regions (and the
// import log the call returns): both arms then open the same files at the same positions, and every byte read or written,
// every position moved and the order of the calls are compared state.
//
//   const m = __mmioStandIn({ module: 'sound.dll',
//       files: { 'a.wav': bytes,                                   // bytes: Array / Uint8Array / ArrayBuffer / hex string
//                'out.wav': { bytes: null, capacity: 0x400 } },    // absent, but MMIO_CREATE creates it
//       imports: { 'KERNEL32.dll!Sleep': { args: 1 } },            // more imports in the same (single) import stand-in
//       bufSlots: 4, bufCap: 0x1000 });                            // mmio-allocated buffers: slot i belongs to handle i
//   fixture.mmblock = m.block; fixture.implog = m.implog;           // state: [('$mmblock', 0, m.size), ('$implog', 0, 0x400)]
//   const h = m.open('a.wav', 0x10000, 0x40);                       // a handle the fixture opens itself (logged like mmioOpenA)
//
// ONE CALL PER KEY (it makes the key's one __importStandIn call itself: pass every other import the key needs in `imports`).
// Only the thread that called __mmioStandIn (the rpc thread that runs both arms) is served; the import stubs forward every
// other thread to the real winmm natively. A call on the A/B thread with a handle that is not one of ours never reaches the
// real winmm: it is logged (op | 0x80) and fails with MMSYSERR_INVALHANDLE (or -1 / 0, the function's error value).
// Behaviour outside the model (listed below) is logged as op | 0x40, counted in the header and fails; a key whose block shows
// a non-zero unsupported count is not evidence for the arm that reached it.
//
// SEMANTICS. The Win32 documentation where it is explicit; where it is silent, the behaviour of the real winmm measured by
// log/c3/alt/c3fg_conform_real.py (ctypes on temp files, Windows 11, 2026-10-13) on the scripts in log/c3/c3fg_gen.py; the
// stand-in reproduces those traces (log/c3/alt/c3fg_conform.py compares them op by op). Measured rules:
//   mmioDescend: dwFlags = 0 first; then, per examined chunk, the 8-byte header is read INTO the MMCKINFO (a short read leaves
//     its partial bytes there and returns MMIOERR_CHUNKNOTFOUND 265, nothing else written); dwDataOffset = header + 8; with a
//     parent, a header outside [parent.dwDataOffset, parent.dwDataOffset + parent.cksize) returns 265 at once (fccType not
//     touched, cksize not clamped, the position left after the header); cksize is clamped to the file end (fileSize -
//     dwDataOffset); RIFF / LIST read their fccType (position dwDataOffset + 4), other chunks get fccType 0; a chunk that does
//     not match is skipped to dwDataOffset + cksize rounded up to even. The search ends at a short header read (end of
//     file) or at the parent bound; the MMCKINFO then holds the last chunk examined and the position stays where the search
//     stopped (it is not restored). MMIO_FINDCHUNK matches ckid, MMIO_FINDRIFF / MMIO_FINDLIST match 'RIFF' / 'LIST' and
//     fccType, no flag takes the chunk at the current position.
//   mmioAscend: seeks to dwDataOffset + cksize rounded up to even; the MMCKINFO is not changed. (MMIO_DIRTY in dwFlags, a chunk
//     made by mmioCreateChunk, is outside the model: sound.dll does not import mmioCreateChunk.)
//   Unbuffered files: the position is lBufOffset (a seek moves only lBufOffset; a read or write moves both it and
//     lDiskOffset); SEEK_CUR may go negative. Buffered files (MMIO_ALLOCBUF or mmioSetBuffer): the position is lBufOffset +
//     pchNext; mmioRead copies from the buffer and, while bytes remain, reads the remainder straight into the caller when it
//     is larger than cchBuffer (the buffer is then empty at the new position) or else refills the buffer at the position
//     (lBufOffset = position, pchEndRead = what was read, lDiskOffset = position + that) and stops when a refill gets 0
//     bytes. mmioSeek inside [lBufOffset, lBufOffset + pchEndRead] only moves pchNext; anywhere else it empties the buffer
//     at the target (lBufOffset = target, pchNext = pchEndRead = pchBuffer, lDiskOffset unchanged). SEEK_END gives fileSize -
//     lOffset for lOffset < 0 and fileSize otherwise, and sets lDiskOffset = fileSize (measured; no sound.dll caller uses
//     SEEK_END). mmioAdvance(MMIO_READ) refills at the CALLER's lBufOffset + (pchNext - pchBuffer) and writes pchNext,
//     pchEndRead, pchEndWrite, lBufOffset, lDiskOffset back into the caller's MMIOINFO; on an unbuffered file it returns
//     MMIOERR_UNBUFFERED 266. mmioSetInfo takes pchNext and pchEndRead (and MMIO_DIRTY) from the caller. mmioSetBuffer(NULL, n)
//     keeps the loaded bytes (pchEndRead and pchNext cut to n), sets MMIO_ALLOCBUF in dwFlags and makes an unbuffered file
//     buffered at its position; mmioSetBuffer(NULL, 0) makes it unbuffered at lBufOffset + pchNext. MMIO_ALLOCBUF without an
//     MMIOINFO (or with cchBuffer 0) gives 8192 bytes. mmioGetInfo: dwFlags = the open flags (+ MMIO_ALLOCBUF when mmio owns
//     the buffer), fccIOProc 'DOS ' / 'MEM ', the pointers absolute (NULL when unbuffered), adwInfo[0] the file handle for a
//     DOS file (here a fake handle 'FH' + slot) or the memory file's expansion increment, hmmio the handle.
//   Memory files (fccIOProc 'MEM '): the buffer is the file. pchBuffer NULL (with or without MMIO_ALLOCBUF) makes mmio
//     allocate cchBuffer bytes and adds MMIO_ALLOCBUF; MMIO_CREATE starts empty (pchEndRead = pchBuffer), otherwise the file
//     holds cchBuffer bytes. Reads stop at pchEndRead; seeks outside [0, pchEndRead - pchBuffer] return -1 (SEEK_END:
//     size - lOffset); mmioAdvance(MMIO_READ) returns MMIOERR_CANNOTREAD 261. The bytes of a buffer mmio allocates are
//     undefined in Windows (the measurement read old heap bytes); here they are 0.
//   mmioOpenA: a name not in `files` fails with wErrorRet MMIOERR_FILENOTFOUND 257 (measured); MMIO_CREATE needs the name
//     listed (an absent entry with a capacity) and truncates; wErrorRet = 0 on success when an MMIOINFO is given.
//   mmioStringToFOURCCA: the first four characters (upper-cased with MMIO_TOUPPER 0x10), padded with spaces (measured).
// Outside the model (logged | 0x40, they fail): buffered writes and mmioAdvance(MMIO_WRITE), writes to memory files, MMIO_DIRTY
// chunks, mmioSetBuffer with a caller buffer or on a memory file, custom I/O procedures, MMIO_PARSE / DELETE / EXIST / GETTEMP,
// reads at a negative position.
//
// BLOCK layout (m.size bytes, every byte state):
//   +0x000 header: 'MSI1', file count, operations served, op-log byte cursor, opens ok, opens failed, unsupported, foreign
//   +0x040 files   (16 x 0x20): exists, length, capacity, data offset, name crc32, opens, bytes read, bytes written
//   +0x240 handles  (8 x 0x60): see H_* below
//   +0x540 op log  (opLogSize, default 0x800): records {op, a, b, c}, 16 bytes each, op codes in OP below
//   then each file's data (capacity bytes, 16-aligned), then bufSlots buffers of bufCap bytes
(function () {
  const OP = {
    open: 1, close: 2, read: 3, write: 4, seek: 5, descend: 6, ascend: 7, getinfo: 8, setinfo: 9, advance: 0xa,
    setbuffer: 0xb, s2f: 0xc, fill: 0x10, direct: 0x11,
  };
  const FOREIGN = 0x80, UNSUP = 0x40;
  const NFILES = 16, NHANDLES = 8;
  const O_FILES = 0x40, O_HANDLES = 0x240, HSZ = 0x60, O_LOG = 0x540;
  // handle record fields
  const H_USE = 0, H_KIND = 4, H_FILE = 8, H_FLAGS = 0xc, H_CCH = 0x10, H_BUF = 0x14, H_OWN = 0x18, H_BOFF = 0x1c,
    H_DOFF = 0x20, H_NEXT = 0x24, H_ENDR = 0x28, H_ENDW = 0x2c, H_ADW0 = 0x30, H_ADW1 = 0x34, H_ADW2 = 0x38,
    H_DIRTY = 0x3c, H_SERIAL = 0x40, H_OPS = 0x44;
  const K_DOS = 1, K_MEM = 2;
  const HVAL = 0x4d4d0010;                   // handle value = HVAL + slot ('MM'..: never a real handle)
  const FAKE_OSH = 0x46480000;               // adwInfo[0] of a DOS file: 'FH' + slot
  const FCC_DOS = 0x20534f44, FCC_MEM = 0x204d454d, FCC_RIFF = 0x46464952, FCC_LIST = 0x5453494c;
  const MMIO_RWMODE = 3, MMIO_CREATE = 0x1000, MMIO_ALLOCBUF = 0x10000, MMIO_DIRTY = 0x10000000;
  const MMIO_UNSUP_OPEN = 0x100 | 0x200 | 0x4000 | 0x20000;   // PARSE, DELETE, EXIST, GETTEMP
  const MMIO_FINDCHUNK = 0x10, MMIO_FINDRIFF = 0x20, MMIO_FINDLIST = 0x40, MMIO_TOUPPER = 0x10;
  const E_INVALHANDLE = 5, E_INVALPARAM = 11, E_FILENOTFOUND = 257, E_OUTOFMEMORY = 258, E_CANNOTREAD = 261,
    E_CANNOTWRITE = 262, E_CANNOTSEEK = 263, E_CHUNKNOTFOUND = 265, E_UNBUFFERED = 266;
  const DEFAULTBUFFER = 8192;
  const ARGS = { mmioOpenA: 3, mmioClose: 2, mmioRead: 3, mmioWrite: 3, mmioSeek: 3, mmioDescend: 4, mmioAscend: 3,
    mmioGetInfo: 3, mmioSetInfo: 3, mmioAdvance: 3, mmioSetBuffer: 4, mmioStringToFOURCCA: 2 };

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
    if (typeof b === 'string') {
      const a = new Uint8Array(b.length >> 1);
      for (let i = 0; i < a.length; i++) a[i] = parseInt(b.substr(2 * i, 2), 16);
      return a;
    }
    if (b instanceof ArrayBuffer) return new Uint8Array(b);
    return Uint8Array.from(b);
  }

  globalThis.__mmioStandIn = function (opts) {
    const module = opts.module || 'sound.dll';
    const opLogSize = opts.opLogSize || 0x800;
    const bufSlots = opts.bufSlots === undefined ? 4 : opts.bufSlots;
    const bufCap = opts.bufCap || 0x1000;
    if (bufSlots > NHANDLES) throw new Error('mmio stand-in: at most ' + NHANDLES + ' buffer slots');
    const norm = function (s) {
      s = s.toLowerCase().replace(/\//g, '\\');
      if (opts.basename) s = s.slice(s.lastIndexOf('\\') + 1);
      return s;
    };
    // ---- the block
    const names = Object.keys(opts.files || {});
    if (names.length > NFILES) throw new Error('mmio stand-in: at most ' + NFILES + ' files per key');
    const specs = names.map(function (n) {
      const v = opts.files[n];
      const plain = v === null || typeof v === 'string' || v instanceof ArrayBuffer || Array.isArray(v) || v instanceof Uint8Array;
      const sp = plain ? { bytes: v } : v;
      const bytes = toBytes(sp.bytes);
      const cap = Math.max(sp.capacity || 0, bytes ? bytes.length : 0, 0x10);
      return { name: n, key: norm(n), bytes: bytes, cap: (cap + 15) & ~15 };
    });
    let off = (O_LOG + opLogSize + 15) & ~15;
    specs.forEach(function (sp) { sp.off = off; off += sp.cap; });
    const O_BUFS = off;
    off += bufSlots * ((bufCap + 15) & ~15);
    if (opts.size && opts.size < off) throw new Error('mmio stand-in: needs 0x' + off.toString(16) + ' bytes, size is 0x' + opts.size.toString(16));
    const size = Math.max(off, opts.size || 0);
    const blk = __keepAlloc(size);
    blk.writeByteArray(new Uint8Array(size).buffer);
    const U = function (o) { return blk.add(o).readU32(); };
    const S = function (o) { return blk.add(o).readS32(); };
    const W = function (o, v) { blk.add(o).writeU32(v >>> 0); };
    W(0, 0x3149534d);
    W(4, specs.length);
    W(0x20, bufSlots);
    W(0x24, bufCap);
    const fileE = function (i) { return O_FILES + 0x20 * i; };
    specs.forEach(function (sp, i) {
      const e = fileE(i);
      W(e, sp.bytes ? 1 : 0);
      W(e + 4, sp.bytes ? sp.bytes.length : 0);
      W(e + 8, sp.cap);
      W(e + 0xc, sp.off);
      W(e + 0x10, crc32(sp.key));
      if (sp.bytes && sp.bytes.length) blk.add(sp.off).writeByteArray(sp.bytes.buffer.slice(sp.bytes.byteOffset, sp.bytes.byteOffset + sp.bytes.length));
    });
    const flen = function (fi) { return U(fileE(fi) + 4); };
    const fcap = function (fi) { return U(fileE(fi) + 8); };
    const fdata = function (fi) { return blk.add(U(fileE(fi) + 0xc)); };
    const hE = function (i) { return O_HANDLES + HSZ * i; };
    const HG = function (i, f) { return U(hE(i) + f); };
    const HGS = function (i, f) { return S(hE(i) + f); };
    const HS = function (i, f, v) { W(hE(i) + f, v); };
    const slotBuf = function (i) { return blk.add(O_BUFS + i * ((bufCap + 15) & ~15)); };
    function log(op, a, b, c) {
      W(8, U(8) + 1);
      const cur = U(0xc);
      if (cur + 16 > opLogSize) return;
      const p = O_LOG + cur;
      W(p, op); W(p + 4, a | 0); W(p + 8, b | 0); W(p + 12, c | 0);
      W(0xc, cur + 16);
    }
    function unsupported(op, a, b) { W(0x18, U(0x18) + 1); log(op | UNSUP, a, b, 0); }
    function findFile(namePtr) {
      if (ptr(namePtr).isNull()) return -1;
      const k = norm(ptr(namePtr).readCString() || '');
      for (let i = 0; i < specs.length; i++) if (specs[i].key === k) return i;
      return -1;
    }
    function slotOf(h) {
      const i = (ptr(h).toUInt32() - HVAL) | 0;
      if (i < 0 || i >= NHANDLES || !HG(i, H_USE)) return -1;
      return i;
    }
    function foreign(op, h) { W(0x1c, U(0x1c) + 1); log(op | FOREIGN, ptr(h).toUInt32(), 0, 0); }
    const buffered = function (i) { return HG(i, H_BUF) !== 0; };
    const bufPtr = function (i) { return ptr(HG(i, H_BUF)); };
    // the size of the file behind a handle: a DOS file's length, a memory file's readable extent
    const hsize = function (i) { return HG(i, H_KIND) === K_MEM ? HG(i, H_ENDR) : flen(HG(i, H_FILE)); };
    const hpos = function (i) { return HG(i, H_KIND) === K_MEM ? HG(i, H_NEXT) : (HGS(i, H_BOFF) + (buffered(i) ? HG(i, H_NEXT) : 0)); };

    // refill the buffer of DOS handle i at file position p (lBufOffset = p, pchEndRead = bytes read, lDiskOffset = p + that)
    function fill(i, p) {
      const fi = HG(i, H_FILE), L = flen(fi), cch = HG(i, H_CCH);
      const n = p >= 0 && p < L ? Math.min(cch, L - p) : 0;
      if (n) Memory.copy(bufPtr(i), fdata(fi).add(p), n);
      HS(i, H_BOFF, p); HS(i, H_NEXT, 0); HS(i, H_ENDR, n); HS(i, H_DOFF, p + n);
      W(fileE(fi) + 0x18, U(fileE(fi) + 0x18) + n);
      log(OP.fill, i, p, n);
      return n;
    }
    // read up to n bytes of handle i to dst; returns the count (-1 outside the model)
    function readInto(i, dst, n) {
      if (n <= 0) return n < 0 ? -1 : 0;
      const kind = HG(i, H_KIND);
      if (kind === K_MEM) {
        const nx = HG(i, H_NEXT), c = Math.max(0, Math.min(n, HG(i, H_ENDR) - nx));
        if (c) Memory.copy(dst, bufPtr(i).add(nx), c);
        HS(i, H_NEXT, nx + c);
        return c;
      }
      const fi = HG(i, H_FILE), L = flen(fi);
      if (!buffered(i)) {
        const p = HGS(i, H_BOFF);
        if (p < 0) { unsupported(OP.read, i, p); return -1; }
        const c = Math.max(0, Math.min(n, L - p));
        if (c) Memory.copy(dst, fdata(fi).add(p), c);
        HS(i, H_BOFF, p + c); HS(i, H_DOFF, p + c);
        W(fileE(fi) + 0x18, U(fileE(fi) + 0x18) + c);
        return c;
      }
      const cch = HG(i, H_CCH);
      let total = 0;
      for (;;) {
        const nx = HG(i, H_NEXT), avail = HG(i, H_ENDR) - nx;
        const c = Math.max(0, Math.min(n, avail));
        if (c) { Memory.copy(dst.add(total), bufPtr(i).add(nx), c); HS(i, H_NEXT, nx + c); n -= c; total += c; }
        if (!n) break;
        const p = HGS(i, H_BOFF) + HG(i, H_NEXT);
        if (p < 0) { unsupported(OP.read, i, p); return -1; }
        if (n > cch) {                                    // the remainder straight into the caller, the buffer emptied there
          const c2 = Math.max(0, Math.min(n, L - p));
          if (c2) Memory.copy(dst.add(total), fdata(fi).add(p), c2);
          W(fileE(fi) + 0x18, U(fileE(fi) + 0x18) + c2);
          total += c2;
          HS(i, H_BOFF, p + c2); HS(i, H_NEXT, 0); HS(i, H_ENDR, 0); HS(i, H_DOFF, p + c2);
          log(OP.direct, i, p, c2);
          break;
        }
        if (!fill(i, p)) break;
      }
      return total;
    }
    // seek handle i; returns the new position or -1
    function seekTo(i, lOff, origin) {
      if (HG(i, H_KIND) === K_MEM) {
        const nx = HG(i, H_NEXT), sz = HG(i, H_ENDR);
        const np = origin === 0 ? lOff : origin === 1 ? nx + lOff : origin === 2 ? sz - lOff : NaN;
        if (!(np >= 0 && np <= sz)) return -1;
        HS(i, H_NEXT, np);
        return np;
      }
      let np;
      if (origin === 0) np = lOff;
      else if (origin === 1) np = hpos(i) + lOff;
      else if (origin === 2) { const L = flen(HG(i, H_FILE)); np = lOff < 0 ? L - lOff : L; HS(i, H_DOFF, L); }
      else return -1;
      if (!buffered(i)) { HS(i, H_BOFF, np); return np; }
      const bo = HGS(i, H_BOFF);
      if (np >= bo && np <= bo + HG(i, H_ENDR)) HS(i, H_NEXT, np - bo);
      else { HS(i, H_BOFF, np); HS(i, H_NEXT, 0); HS(i, H_ENDR, 0); }
      return np;
    }
    function allocBuf(i, cch) {
      if (i >= bufSlots || cch > bufCap) return null;
      const b = slotBuf(i);
      b.writeByteArray(new Uint8Array(bufCap).buffer);
      return b;
    }
    function newSlot() {
      for (let i = 0; i < NHANDLES; i++) if (!HG(i, H_USE)) return i;
      return -1;
    }
    // ---- the API
    function open(namePtr, info, flags) {
      const isMem = !info.isNull() && info.add(4).readU32() === FCC_MEM;
      const fail = function (err, fi) {
        if (!info.isNull()) info.add(0xc).writeU32(err);
        W(0x14, U(0x14) + 1);
        log(OP.open, fi, -1, flags);
        return ptr(0);
      };
      if (!info.isNull()) {
        const fcc = info.add(4).readU32(), proc = info.add(8).readU32();
        if (proc || (fcc && fcc !== FCC_MEM && fcc !== FCC_DOS)) { unsupported(OP.open, fcc, proc); return fail(E_FILENOTFOUND, -1); }
      }
      if (flags & MMIO_UNSUP_OPEN) { unsupported(OP.open, flags, 0); return fail(E_FILENOTFOUND, -1); }
      const i = newSlot();
      if (i < 0) return fail(E_OUTOFMEMORY, -1);
      const h = hE(i);
      blk.add(h).writeByteArray(new Uint8Array(HSZ).buffer);
      if (isMem) {
        const cch = info.add(0x14).readS32();
        let buf = info.add(0x18).readPointer(), own = 0, fl = flags;
        if (buf.isNull()) {
          buf = allocBuf(i, cch);
          if (!buf) return fail(E_OUTOFMEMORY, -1);
          own = 1; fl |= MMIO_ALLOCBUF;
        }
        HS(i, H_USE, 1); HS(i, H_KIND, K_MEM); HS(i, H_FILE, 0xffffffff); HS(i, H_FLAGS, fl); HS(i, H_CCH, cch);
        HS(i, H_BUF, buf.toUInt32()); HS(i, H_OWN, own);
        HS(i, H_ENDR, (flags & MMIO_CREATE) ? 0 : cch); HS(i, H_ENDW, cch);
        HS(i, H_ADW0, info.add(0x30).readU32()); HS(i, H_ADW1, info.add(0x34).readU32()); HS(i, H_ADW2, info.add(0x38).readU32());
        HS(i, H_SERIAL, U(8) + 1);
        info.add(0xc).writeU32(0);
        W(0x10, U(0x10) + 1);
        log(OP.open, 0x100 + i, i, flags);
        return ptr(HVAL + i);
      }
      const fi = findFile(namePtr);
      if (fi < 0) return fail(E_FILENOTFOUND, -1);
      if (flags & MMIO_CREATE) { W(fileE(fi), 1); W(fileE(fi) + 4, 0); }
      else if (!U(fileE(fi))) return fail(E_FILENOTFOUND, fi);
      let cch = 0, buf = ptr(0), own = 0;
      if (flags & MMIO_ALLOCBUF) {
        cch = (!info.isNull() && info.add(0x14).readS32() > 0) ? info.add(0x14).readS32() : DEFAULTBUFFER;
        buf = allocBuf(i, cch);
        if (!buf) return fail(E_OUTOFMEMORY, fi);
        own = 1;
      }
      HS(i, H_USE, 1); HS(i, H_KIND, K_DOS); HS(i, H_FILE, fi); HS(i, H_FLAGS, flags); HS(i, H_CCH, cch);
      HS(i, H_BUF, buf.toUInt32()); HS(i, H_OWN, own); HS(i, H_ENDW, cch); HS(i, H_ADW0, FAKE_OSH + i);
      HS(i, H_SERIAL, U(8) + 1);
      if (!info.isNull()) info.add(0xc).writeU32(0);
      W(fileE(fi) + 0x14, U(fileE(fi) + 0x14) + 1);
      W(0x10, U(0x10) + 1);
      log(OP.open, fi, i, flags);
      return ptr(HVAL + i);
    }
    function descend(i, ck, parent, flags) {
      ck.add(0x10).writeU32(0);
      const want = ck.readU32(), wantType = ck.add(8).readU32();
      const sz = hsize(i);
      for (;;) {
        const hdr = hpos(i);
        const n = readInto(i, ck, 8);
        if (n < 8) { log(OP.descend, i | (flags << 8), E_CHUNKNOTFOUND, hdr); return E_CHUNKNOTFOUND; }
        const dataOff = hdr + 8;
        ck.add(0xc).writeU32(dataOff);
        if (!parent.isNull()) {
          const po = parent.add(0xc).readU32(), pe = po + parent.add(4).readU32();
          if (hdr < po || hdr >= pe) { log(OP.descend, i | (flags << 8), E_CHUNKNOTFOUND, hdr); return E_CHUNKNOTFOUND; }
        }
        if (ck.add(4).readU32() > sz - dataOff) ck.add(4).writeU32(sz - dataOff);
        const id = ck.readU32();
        if (id === FCC_RIFF || id === FCC_LIST) {
          if (readInto(i, ck.add(8), 4) < 4) { log(OP.descend, i | (flags << 8), E_CHUNKNOTFOUND, hdr); return E_CHUNKNOTFOUND; }
        } else ck.add(8).writeU32(0);
        let hit;
        if (flags & MMIO_FINDRIFF) hit = id === FCC_RIFF && ck.add(8).readU32() === wantType;
        else if (flags & MMIO_FINDLIST) hit = id === FCC_LIST && ck.add(8).readU32() === wantType;
        else if (flags & MMIO_FINDCHUNK) hit = id === want;
        else hit = true;
        if (hit) {
          if (id !== FCC_RIFF && id !== FCC_LIST) seekTo(i, dataOff, 0);
          log(OP.descend, i | (flags << 8), 0, dataOff);
          return 0;
        }
        seekTo(i, dataOff + ((ck.add(4).readU32() + 1) & ~1), 0);
      }
    }
    function infoOut(i, info) {
      const b = HG(i, H_BUF);
      const rel = function (f) { return b ? (b + HG(i, f)) >>> 0 : 0; };
      const v = [HG(i, H_FLAGS) | (HG(i, H_DIRTY) ? MMIO_DIRTY : 0), HG(i, H_KIND) === K_MEM ? FCC_MEM : FCC_DOS, 0, 0, 0,
        HG(i, H_CCH), b, rel(H_NEXT), rel(H_ENDR), rel(H_ENDW), HG(i, H_BOFF), HG(i, H_DOFF),
        HG(i, H_ADW0), HG(i, H_ADW1), HG(i, H_ADW2), 0, 0, HVAL + i];
      v.forEach(function (x, k) { info.add(4 * k).writeU32(x >>> 0); });
    }
    const imps = {
      // mmioOpenA(name, info, flags): the name is recorded as its crc32 (the pointer differs between arms)
      'WINMM.dll!mmioOpenA': function (a, ctx) {
        ctx.record([a[0].isNull() ? 0 : crc32(norm(a[0].readCString() || '')), a[1].isNull() ? 0 : 1, a[2].toUInt32()]);
        return open(a[0], a[1], a[2].toUInt32());
      },
      'WINMM.dll!mmioClose': function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[1].toUInt32()]);
        const i = slotOf(a[0]);
        if (i < 0) { foreign(OP.close, a[0]); return E_INVALHANDLE; }
        if (HG(i, H_DIRTY)) { unsupported(OP.close, i, 0); }
        HS(i, H_USE, 0);
        log(OP.close, i, HG(i, H_FILE), 0);
        return 0;
      },
      // mmioRead(h, buf, n): the buffer pointer is not recorded (a stack address in most callers)
      'WINMM.dll!mmioRead': function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[2].toUInt32()]);
        const i = slotOf(a[0]);
        if (i < 0) { foreign(OP.read, a[0]); return -1; }
        const p = hpos(i), r = readInto(i, a[1], a[2].toInt32());
        HS(i, H_OPS, HG(i, H_OPS) + 1);
        log(OP.read, i, p, r);
        return r;
      },
      'WINMM.dll!mmioWrite': function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[2].toUInt32()]);
        const i = slotOf(a[0]);
        if (i < 0) { foreign(OP.write, a[0]); return -1; }
        const n = a[2].toInt32();
        if (HG(i, H_KIND) !== K_DOS || buffered(i) || !(HG(i, H_FLAGS) & MMIO_RWMODE) || n < 0) { unsupported(OP.write, i, n); return -1; }
        const fi = HG(i, H_FILE), p = HGS(i, H_BOFF);
        if (p < 0) { unsupported(OP.write, i, p); return -1; }
        const c = Math.max(0, Math.min(n, fcap(fi) - p));
        if (c) Memory.copy(fdata(fi).add(p), a[1], c);
        if (p + c > flen(fi)) W(fileE(fi) + 4, p + c);
        W(fileE(fi) + 0x1c, U(fileE(fi) + 0x1c) + c);
        HS(i, H_BOFF, p + c); HS(i, H_DOFF, p + c);
        log(OP.write, i, p, c);
        return c < n ? -1 : c;
      },
      'WINMM.dll!mmioSeek': function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[1].toUInt32(), a[2].toUInt32()]);
        const i = slotOf(a[0]);
        if (i < 0) { foreign(OP.seek, a[0]); return -1; }
        const r = seekTo(i, a[1].toInt32(), a[2].toInt32());
        log(OP.seek, i, a[1].toInt32(), r);
        return r;
      },
      // mmioDescend(h, ck, parent, flags): the MMCKINFO pointers are not recorded, their contents are the caller's state
      'WINMM.dll!mmioDescend': function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[2].isNull() ? 0 : 1, a[3].toUInt32()]);
        const i = slotOf(a[0]);
        if (i < 0) { foreign(OP.descend, a[0]); return E_INVALHANDLE; }
        return descend(i, a[1], a[2], a[3].toUInt32());
      },
      'WINMM.dll!mmioAscend': function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[2].toUInt32()]);
        const i = slotOf(a[0]);
        if (i < 0) { foreign(OP.ascend, a[0]); return E_INVALHANDLE; }
        const ck = a[1];
        if (ck.add(0x10).readU32() & MMIO_DIRTY) { unsupported(OP.ascend, i, 0); return E_CANNOTWRITE; }
        const r = seekTo(i, (ck.add(0xc).readU32() + ((ck.add(4).readU32() + 1) & ~1)) >>> 0, 0);
        log(OP.ascend, i, r < 0 ? E_CANNOTSEEK : 0, r);
        return r < 0 ? E_CANNOTSEEK : 0;
      },
      'WINMM.dll!mmioGetInfo': function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[2].toUInt32()]);
        const i = slotOf(a[0]);
        if (i < 0) { foreign(OP.getinfo, a[0]); return E_INVALHANDLE; }
        infoOut(i, a[1]);
        log(OP.getinfo, i, HG(i, H_NEXT), HG(i, H_ENDR));
        return 0;
      },
      'WINMM.dll!mmioSetInfo': function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[2].toUInt32()]);
        const i = slotOf(a[0]);
        if (i < 0) { foreign(OP.setinfo, a[0]); return E_INVALHANDLE; }
        const info = a[1];
        if (buffered(i)) {
          const b = HG(i, H_BUF), cch = HG(i, H_CCH);
          const pn = (info.add(0x1c).readU32() - b) | 0, pe = (info.add(0x20).readU32() - b) | 0;
          if (pn < 0 || pn > cch || pe < 0 || pe > cch) { log(OP.setinfo, i, pn, -1); return E_INVALPARAM; }
          HS(i, H_NEXT, pn); HS(i, H_ENDR, pe);
          if (info.readU32() & MMIO_DIRTY) HS(i, H_DIRTY, 1);
        }
        log(OP.setinfo, i, HG(i, H_NEXT), HG(i, H_ENDR));
        return 0;
      },
      'WINMM.dll!mmioAdvance': function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[1].isNull() ? 0 : 1, a[2].toUInt32()]);
        const i = slotOf(a[0]);
        if (i < 0) { foreign(OP.advance, a[0]); return E_INVALHANDLE; }
        const info = a[1], fl = a[2].toUInt32();
        if (!buffered(i)) { log(OP.advance, i, E_UNBUFFERED, 0); return E_UNBUFFERED; }
        if (HG(i, H_KIND) === K_MEM) {
          if (fl === 0) { log(OP.advance, i, E_CANNOTREAD, 0); return E_CANNOTREAD; }
          unsupported(OP.advance, i, fl); return E_CANNOTWRITE;
        }
        if (fl !== 0 || (!info.isNull() && (info.readU32() & MMIO_DIRTY))) { unsupported(OP.advance, i, fl); return E_CANNOTWRITE; }
        const p = info.isNull() ? hpos(i)
          : (info.add(0x28).readS32() + ((info.add(0x1c).readU32() - info.add(0x18).readU32()) | 0));
        fill(i, p);
        if (!info.isNull()) {
          const b = HG(i, H_BUF);
          info.add(0x1c).writeU32(b);
          info.add(0x20).writeU32((b + HG(i, H_ENDR)) >>> 0);
          info.add(0x24).writeU32((b + HG(i, H_ENDW)) >>> 0);
          info.add(0x28).writeU32(HG(i, H_BOFF));
          info.add(0x2c).writeU32(HG(i, H_DOFF));
        }
        log(OP.advance, i, 0, p);
        return 0;
      },
      'WINMM.dll!mmioSetBuffer': function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[1].isNull() ? 0 : 1, a[2].toUInt32(), a[3].toUInt32()]);
        const i = slotOf(a[0]);
        if (i < 0) { foreign(OP.setbuffer, a[0]); return E_INVALHANDLE; }
        const cch = a[2].toInt32();
        if (HG(i, H_KIND) !== K_DOS || !a[1].isNull() || cch < 0 || HG(i, H_DIRTY)) { unsupported(OP.setbuffer, i, cch); return E_CANNOTWRITE; }
        if (cch === 0) {
          const p = hpos(i);
          HS(i, H_BUF, 0); HS(i, H_OWN, 0); HS(i, H_CCH, 0); HS(i, H_NEXT, 0); HS(i, H_ENDR, 0); HS(i, H_ENDW, 0);
          HS(i, H_BOFF, p); HS(i, H_FLAGS, HG(i, H_FLAGS) & ~MMIO_ALLOCBUF);
          log(OP.setbuffer, i, 0, 0);
          return 0;
        }
        if (i >= bufSlots || cch > bufCap) { log(OP.setbuffer, i, cch, E_OUTOFMEMORY); return E_OUTOFMEMORY; }
        const b = slotBuf(i);
        if (buffered(i)) {
          // the loaded bytes stay (the slot is the same memory); pchEndRead and pchNext are cut to the new size
          if (HG(i, H_ENDR) > cch) HS(i, H_ENDR, cch);
          if (HG(i, H_NEXT) > cch) HS(i, H_NEXT, cch);
          if (cch > HG(i, H_CCH)) b.add(HG(i, H_ENDR)).writeByteArray(new Uint8Array(cch - HG(i, H_ENDR)).buffer);
        } else {
          b.writeByteArray(new Uint8Array(bufCap).buffer);
          HS(i, H_NEXT, 0); HS(i, H_ENDR, 0);
        }
        HS(i, H_BUF, b.toUInt32()); HS(i, H_OWN, 1); HS(i, H_CCH, cch); HS(i, H_ENDW, cch);
        HS(i, H_FLAGS, HG(i, H_FLAGS) | MMIO_ALLOCBUF);
        log(OP.setbuffer, i, cch, 0);
        return 0;
      },
      // mmioStringToFOURCCA(sz, flags): the result and the flags are recorded, not the string pointer
      'WINMM.dll!mmioStringToFOURCCA': function (a, ctx) {
        const s = a[0].isNull() ? '' : (a[0].readCString() || '');
        let v = 0;
        for (let k = 0; k < 4; k++) {
          let c = k < s.length ? s.charCodeAt(k) & 0xff : 0x20;
          if (k < s.length && (a[1].toUInt32() & MMIO_TOUPPER) && c >= 0x61 && c <= 0x7a) c -= 0x20;
          v |= c << (8 * k);
        }
        ctx.record([v >>> 0, a[1].toUInt32()]);
        log(OP.s2f, v, a[1].toUInt32(), 0);
        return v >>> 0;
      },
    };
    const wanted = opts.mmio || Object.keys(ARGS);
    const imports = {};
    wanted.forEach(function (n) {
      const k = 'WINMM.dll!' + n;
      imports[k] = { args: ARGS[n], impl: imps[k] };
    });
    Object.keys(opts.imports || {}).forEach(function (k) { imports[k] = opts.imports[k]; });
    const imp = __importStandIn({ module: module, imports: imports, logSize: opts.implogSize || 0x400 });
    return {
      block: blk, size: size, implog: imp.log, implogSize: opts.implogSize || 0x400, imp: imp, OP: OP,
      // a handle the fixture opens itself (a function that takes an open HMMIO): the same path as the stood-in mmioOpenA
      open: function (name, flags, cch) {
        const info = __keepAlloc(0x48);
        info.writeByteArray(new Uint8Array(0x48).buffer);
        if (cch) info.add(0x14).writeU32(cch);
        return open(Memory.allocUtf8String(name), info, flags || 0);
      },
      // a memory file the fixture opens itself (cch bytes; pchBuffer NULL lets the stand-in allocate it in the block)
      openMem: function (cch, flags, pchBuffer, expand) {
        const info = __keepAlloc(0x48);
        info.writeByteArray(new Uint8Array(0x48).buffer);
        info.add(4).writeU32(FCC_MEM);
        info.add(0x14).writeU32(cch);
        if (pchBuffer) info.add(0x18).writePointer(pchBuffer);
        if (expand) info.add(0x30).writeU32(expand);
        return open(ptr(0), info, flags || 0);
      },
      data: function (n) { return fdata(names.indexOf(n)); },
      length: function (n) { return flen(names.indexOf(n)); },
      handle: function (h) { const i = slotOf(h); return i < 0 ? null : blk.add(hE(i)); },
      buffer: function (slot) { return slotBuf(slot); },
      unsupported: function () { return U(0x18); },
      foreign: function () { return U(0x1c); },
    };
  };
  globalThis.__mmioStandIn.OP = OP;
})();
