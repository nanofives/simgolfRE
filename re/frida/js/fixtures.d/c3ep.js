// Fixtures for batch c3ep (round 31, 2026-10-13): the file layer and the theme / save readers.
// Reimplementations: shim/src/re/c3ep.cpp. Registry: re/frida/registry.d/c3ep.py. Purposes and per-jcc branch tables:
// log/c3/c3ep_purpose.md.
//
// Every key freezes the game's first thread (re/frida/js/freeze.js) before it touches a global, because the functions
// under test build their paths in the shared text buffer 0x0051a068 and write the tables at 0x0080b130, 0x004d6088,
// 0x00543d10 and 0x005795a8, which the running menu also uses.
//
// Three stand-ins, all state:
//   * DIRECTORY (dirOn): the exe's FindFirstFileA / FindNextFileA / FindClose IAT slots 0x004ba090 / 0x004ba098 /
//     0x004ba094 through re/frida/js/importstandin.js. The enumeration serves a fixture list of names, writes each into
//     the caller's WIN32_FIND_DATAA (name at +0x2c) and keeps its cursor in the block below, so both arms enumerate the
//     same directory in the same order. Names that must contain one of the exe's own filter strings are built at
//     fixture time from the running image (0x004c8a34, 0x004c8a2c, 0x004c3c5c, 0x004c5314): no binary text in the repo.
//   * LOW-LEVEL FILES (lowIo): the static CRT's __open 0x004a5d48, __read 0x004a583a, __write 0x004a5b58,
//     __close 0x004a5a78 and __tell 0x004a6057, which the save / load family uses (filestandin.js covers the FILE
//     layer only). One block holds every file's bytes, length and capacity, the open descriptors with their cursors
//     and an operation log; a body that reads one byte too few, skips a close or writes a wrong byte is RED on state.
//     Only the A/B thread is served; any other thread is forwarded to the real routine.
//   * FILE layer (filesOn -> re/frida/js/filestandin.js) for loadThemeFile's fopen / fread / fclose.
// Game callees that would write the live terrain, window tree or display are replaced by RECORDING stubs (stand(),
// c3ea's calleeStand method): their calls, arguments and order are compared through the record buffer `rec`.
(function () {
  const F = {};
  const EXE = 'golf_clean.exe';
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  function zalloc(n) { const p = keep(Memory.alloc(n)); p.writeByteArray(new Uint8Array(n).buffer); return p; }
  function crc32(s) {
    let c = 0xffffffff;
    for (let i = 0; i < s.length; i++) {
      c ^= s.charCodeAt(i) & 0xff;
      for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1));
    }
    return (c ^ 0xffffffff) >>> 0;
  }
  function norm(s) { return s.toLowerCase().replace(/\//g, '\\'); }

  // ---- freeze (once per key)
  let frozen = false;
  function freezeOnce() {
    if (frozen) return;
    __freeze.thread();
    frozen = true;
    __freeze.atFlush(function () { frozen = false; });
    __freeze.negOn();                       // a negctl build's exported switch; nothing in a normal build
  }
  function saveRange(a, n) { __freeze.save(ptr(a), n); }
  function setU32(a, v) { __freeze.setU32(ptr(a), v >>> 0); }
  function seedText(s) { saveRange(G_TEXT, 0x600); ptr(G_TEXT).writeByteArray(new Uint8Array(0x600).buffer); if (s) ptr(G_TEXT).writeUtf8String(s); }

  // ---- globals the bodies read or write (addresses from the instructions, see shim/src/re/c3ep.cpp)
  const G_TEXT = 0x0051a068, G_DIRTAB = 0x0080b130, G_THEMES = 0x0056e928, G_THEME = 0x00567328;
  const G_FD = 0x00568d08, G_DATE = 0x00834170, G_FLAGS = 0x0059e7b8, G_ITOA = 0x0058a528;
  const G_TYPE = 0x005a34e0, G_LOADFLAG = 0x004c2e08, G_567AFC = 0x00567afc, G_C2854 = 0x004c2854, G_C2848 = 0x004c2848;
  const G_THEMEREC = 0x004d6088, G_THEMEREC2 = 0x00543d10, G_SLOTS = 0x005795a8, G_SLOT0 = 0x005a5a04, G_USED = 0x0059b76c;
  const G_MEMBER = 0x00584a0e;

  // strings of the running image the fixtures build names from (never stored in the repository)
  let STR = null;
  function strs() {
    if (STR === null) {
      STR = { sh1: ptr(0x004c8a34).readCString(), sh2: ptr(0x004c8a2c).readCString(),
              champ: ptr(0x004c3c5c).readCString(), dot: ptr(0x004c4944).readCString(),
              browse: ptr(0x004c5314).readCString() };
    }
    return STR;
  }

  // ---- record buffer for the stood-in callees: {tag, argc, args...}, 0x20 bytes each
  const REC = 0x1000;
  function newRec() { return zalloc(REC); }
  function record(rec, tag, args) {
    const n = rec.readU32(), cur = rec.add(4).readU32();
    rec.writeU32(n + 1);
    if (0x10 + cur + 0x20 > REC) return;
    const p = rec.add(0x10 + cur);
    p.writeU32(tag >>> 0);
    p.add(4).writeU32(args.length);
    for (let i = 0; i < args.length && i < 6; i++) {
      const v = args[i];
      p.add(8 + 4 * i).writeU32(typeof v === 'number' ? (v >>> 0) : ptr(v).toUInt32());
    }
    rec.add(4).writeU32(cur + 0x20);
  }

  // ---- callee stand-in on the A/B thread only (c3ee's pattern, one revert token per install)
  // `mask` is a list of argument indices recorded as 0xf0000000 | index instead of their value: a stub whose caller
  // passes a pointer into ITS OWN FRAME (promptAndSaveFile's local Image) would otherwise record a different address
  // in each arm, because the two frames have different sizes.
  const standLive = {};
  function stand(fx, va, argc, fn, abi, mask) {
    const tid = Process.getCurrentThreadId();
    const at = ptr(va), k = at.toString();
    if (standLive[k]) { try { Interceptor.revert(at); } catch (e) { /* gone */ } delete standLive[k]; Interceptor.flush(); }
    const types = Array(argc + (abi === 'thiscall' ? 1 : 0)).fill('uint32');
    const orig = keep(new NativeFunction(at, 'uint32', types, abi || 'default'));
    const cb = keep(new NativeCallback(function () {
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, arguments);
      const a = Array.prototype.slice.call(arguments);
      const rec = a.slice();
      if (mask) mask.forEach(function (i) { rec[i] = (0xf0000000 | i) >>> 0; });
      record(fx.rec, va, rec);
      return (fn ? fn(a) : 0) >>> 0;
    }, 'uint32', types, abi || 'default'));
    Interceptor.replace(at, cb);
    Interceptor.flush();
    const tok = {};
    standLive[k] = tok;
    setTimeout(function () {
      if (standLive[k] !== tok) return;
      Interceptor.revert(at); delete standLive[k]; Interceptor.flush();
    }, 0);
  }

  // ---------------------------------------------------------------------------------------------------- directory
  // Block (shared with lowIo, see below): +0xd0 cursor, +0xd4 handle open, +0xd8 list index, +0xdc calls.
  const D_CUR = 0xd0, D_OPEN = 0xd4, D_LIST = 0xd8, D_CALLS = 0xdc;
  const FIND_HANDLE = 0x46440001;
  function writeFind(p, name) {
    p.writeByteArray(new Uint8Array(0x140).buffer);
    p.writeU32(name.indexOf('.') < 0 ? 0x10 : 0x20);          // FILE_ATTRIBUTE_DIRECTORY / _ARCHIVE (never read)
    p.add(0x2c).writeUtf8String(name);                        // WIN32_FIND_DATAA.cFileName
  }
  // lists: an array of arrays; byPattern maps a normalised pattern to a list index (or is a function of the
  // normalised pattern); def is the fallback index
  function dirSpec(blk, lists, byPattern, def) {
    const pick = function (pat) {
      const k = norm(pat);
      if (typeof byPattern === 'function') { const v = byPattern(k); return v === undefined ? def : v; }
      return (byPattern && k in byPattern) ? byPattern[k] : def;
    };
    return {
      'KERNEL32.dll!FindFirstFileA': { args: 2, record: false, impl: function (a, ctx) {
        const pat = a[0].readCString();
        const li = pick(pat);
        const list = lists[li] || [];
        blk.add(D_LIST).writeU32(li);
        blk.add(D_CALLS).writeU32(blk.add(D_CALLS).readU32() + 1);
        ctx.record([crc32(norm(pat)), li, list.length]);
        if (list.length === 0) { blk.add(D_CUR).writeU32(0); blk.add(D_OPEN).writeU32(0); return 0xffffffff; }
        writeFind(a[1], list[0]);
        blk.add(D_CUR).writeU32(1);
        blk.add(D_OPEN).writeU32(1);
        return FIND_HANDLE;
      } },
      'KERNEL32.dll!FindNextFileA': { args: 2, record: false, impl: function (a, ctx) {
        const h = a[0].toUInt32();
        const list = lists[blk.add(D_LIST).readU32()] || [];
        const i = blk.add(D_CUR).readU32();
        ctx.record([h, i]);
        if (h !== FIND_HANDLE || blk.add(D_OPEN).readU32() === 0 || i >= list.length) return 0;
        writeFind(a[1], list[i]);
        blk.add(D_CUR).writeU32(i + 1);
        return 1;
      } },
      'KERNEL32.dll!FindClose': { args: 1, record: false, impl: function (a, ctx) {
        ctx.record([a[0].toUInt32()]);
        blk.add(D_OPEN).writeU32(0);
        return 1;
      } },
    };
  }

  // ---------------------------------------------------------------------------------------------------- low-level files
  // Block layout (LIO bytes, every byte state):
  //   +0x000 'LIO1'  +0x004 operations  +0x008 log byte cursor  +0x00c descriptors handed out
  //   +0x010 files   (8 x 0x20): exists, length, capacity, data offset, name crc32, opens, read, written
  //   +0x0d0 the directory stand-in's cursor / handle / list / calls (above)
  //   +0x110 descriptors (8 x 0x10): in use, file index, cursor, flags
  //   +0x190 operation log (0x600): {op, a, b, c} of 16 bytes; op | 0x80 = a descriptor that is not ours
  //   +0x790 the files' bytes (capacity each, 16-aligned)
  const LIO = 0x3000, L_FILES = 0x10, L_FDS = 0x110, L_LOG = 0x190, L_LOGSZ = 0x600, L_DATA = 0x790;
  const NFILE = 8, NFD = 8, FD0 = 3;
  const OP = { open: 1, read: 2, write: 3, close: 4, tell: 5 };
  const crtLive = {};
  function lowIo(fx, files, o) {
    o = o || {};
    const blk = fx.lio;
    const tid = Process.getCurrentThreadId();
    const names = Object.keys(files);
    if (names.length > NFILE) throw new Error('c3ep: at most ' + NFILE + ' low-level files');
    const idxOf = {};
    let off = L_DATA;
    names.forEach(function (n, i) {
      const spec = files[n];
      const bytes = spec.bytes === null || spec.bytes === undefined ? null : Uint8Array.from(spec.bytes);
      const cap = spec.capacity || (bytes ? bytes.length : 0x100);
      if (off + cap > LIO) throw new Error('c3ep: low-level file block full at ' + n);
      const e = blk.add(L_FILES + 0x20 * i);
      e.writeU32(bytes ? 1 : 0);
      e.add(4).writeU32(bytes ? bytes.length : 0);
      e.add(8).writeU32(cap);
      e.add(0xc).writeU32(off);
      e.add(0x10).writeU32(crc32(norm(n)));
      if (bytes) blk.add(off).writeByteArray(bytes.buffer);
      idxOf[norm(n)] = i;
      off = (off + cap + 15) & ~15;
    });
    blk.writeU32(0x3149494c);
    const find = function (p) {
      const s = p.isNull() ? '' : p.readCString();
      const k = norm(s);
      const key = o.basename ? k.slice(k.lastIndexOf('\\') + 1) : k;
      let hit = -1;
      Object.keys(idxOf).forEach(function (n) {
        const nn = o.basename ? n.slice(n.lastIndexOf('\\') + 1) : n;
        if (nn === key) hit = idxOf[n];
      });
      return { idx: hit, crc: crc32(key) };
    };
    const log = function (op, a, b, c) {
      const n = blk.add(4).readU32(), cur = blk.add(8).readU32();
      blk.add(4).writeU32(n + 1);
      if (cur + 0x10 <= L_LOGSZ) {
        const p = blk.add(L_LOG + cur);
        p.writeU32(op >>> 0); p.add(4).writeU32(a >>> 0); p.add(8).writeU32(b >>> 0); p.add(0xc).writeU32(c >>> 0);
        blk.add(8).writeU32(cur + 0x10);
      }
    };
    const fdSlot = function (fd) {
      const s = (fd | 0) - FD0;
      if (s < 0 || s >= NFD) return null;
      const e = blk.add(L_FDS + 0x10 * s);
      return e.readU32() === 0 ? null : e;
    };
    const fileOf = function (e) { return blk.add(L_FILES + 0x20 * e.add(4).readU32()); };
    const impl = {
      // __open(path, flags[, pmode]): _sopen reads the third argument only for _O_CREAT (0x004a5d48), so it is not
      // recorded; loadCourse / loadGame pass two arguments (0x0040b876, 0x0040ba78).
      0xa5d48: [3, function (a) {
        const f = find(ptr(a[0])), flags = a[1] | 0;
        let idx = f.idx;
        if (idx >= 0) {
          const e = blk.add(L_FILES + 0x20 * idx);
          if (e.readU32() === 0) {
            if ((flags & 0x100) === 0) idx = -1;                       // _O_CREAT: create an absent declared name
            else { e.writeU32(1); e.add(4).writeU32(0); }
          } else if ((flags & 0x200) !== 0) e.add(4).writeU32(0);      // _O_TRUNC
        }
        if (idx < 0 || (o.failCrc && o.failCrc.indexOf(f.crc) >= 0)) { log(OP.open | 0x80, f.crc, flags, 0xffffffff); return 0xffffffff; }
        let slot = -1;
        for (let i = 0; i < NFD; i++) if (blk.add(L_FDS + 0x10 * i).readU32() === 0) { slot = i; break; }
        if (slot < 0) { log(OP.open | 0x80, f.crc, flags, 0xfffffffe); return 0xffffffff; }
        const e = blk.add(L_FDS + 0x10 * slot);
        const file = blk.add(L_FILES + 0x20 * idx);
        e.writeU32(1); e.add(4).writeU32(idx);
        e.add(8).writeU32((flags & 8) !== 0 ? file.add(4).readU32() : 0);     // _O_APPEND
        e.add(0xc).writeU32(flags);
        file.add(0x14).writeU32(file.add(0x14).readU32() + 1);
        blk.add(0xc).writeU32(blk.add(0xc).readU32() + 1);
        log(OP.open, f.crc, flags, FD0 + slot);
        return FD0 + slot;
      }],
      0xa583a: [3, function (a) {                                      // __read(fd, buf, count)
        const e = fdSlot(a[0]);
        if (e === null) { log(OP.read | 0x80, a[0], a[2], 0); return 0xffffffff; }
        const file = fileOf(e), cur = e.add(8).readU32(), len = file.add(4).readU32();
        const got = Math.max(0, Math.min(a[2] | 0, len - cur));
        if (got > 0) Memory.copy(ptr(a[1]), blk.add(file.add(0xc).readU32() + cur), got);
        e.add(8).writeU32(cur + got);
        file.add(0x18).writeU32(file.add(0x18).readU32() + got);
        log(OP.read, a[0], a[2], got);
        return got;
      }],
      0xa5b58: [3, function (a) {                                      // __write(fd, buf, count)
        const e = fdSlot(a[0]);
        if (e === null) { log(OP.write | 0x80, a[0], a[2], 0); return 0xffffffff; }
        const file = fileOf(e), cur = e.add(8).readU32(), cap = file.add(8).readU32();
        const put = Math.max(0, Math.min(a[2] | 0, cap - cur));
        if (put > 0) Memory.copy(blk.add(file.add(0xc).readU32() + cur), ptr(a[1]), put);
        e.add(8).writeU32(cur + put);
        if (cur + put > file.add(4).readU32()) file.add(4).writeU32(cur + put);
        file.add(0x1c).writeU32(file.add(0x1c).readU32() + put);
        log(OP.write, a[0], a[2], put);
        return put;
      }],
      0xa5a78: [1, function (a) {                                      // __close(fd)
        const e = fdSlot(a[0]);
        if (e === null) { log(OP.close | 0x80, a[0], 0, 0); return 0xffffffff; }
        log(OP.close, a[0], e.add(4).readU32(), e.add(8).readU32());
        e.writeU32(0);
        return 0;
      }],
      0xa6057: [1, function (a) {                                      // __tell(fd)
        const e = fdSlot(a[0]);
        if (e === null) { log(OP.tell | 0x80, a[0], 0, 0); return 0xffffffff; }
        log(OP.tell, a[0], e.add(8).readU32(), 0);
        return e.add(8).readU32();
      }],
    };
    const base = Process.getModuleByName(EXE).base;
    Object.keys(impl).forEach(function (rvaKey) {
      const rva = parseInt(rvaKey, 10);
      const at = base.add(rva), k = at.toString();
      if (crtLive[k]) { try { Interceptor.revert(at); } catch (e) { /* gone */ } delete crtLive[k]; Interceptor.flush(); }
      const argc = impl[rvaKey][0], fn = impl[rvaKey][1];
      const types = Array(argc).fill('uint32');
      const orig = keep(new NativeFunction(at, 'uint32', types));
      const cb = keep(new NativeCallback(function () {
        if (Process.getCurrentThreadId() !== tid) return orig.apply(null, arguments);
        return fn(Array.prototype.slice.call(arguments)) >>> 0;
      }, 'uint32', types));
      Interceptor.replace(at, cb);
      const tok = {};
      crtLive[k] = tok;
      setTimeout(function () {
        if (crtLive[k] !== tok) return;
        Interceptor.revert(at); delete crtLive[k]; Interceptor.flush();
      }, 0);
    });
    Interceptor.flush();
    return blk;
  }

  // ---- the file stand-in (FILE layer) for loadThemeFile
  const FSB = 0x4000;
  function filesOn(fx, files, o) {
    o = o || {};
    const fs = __fileStandIn({ module: EXE, kernel32: false, crt: o.crt || ['fopen', 'fread', 'fclose'],
                               files: files, size: FSB, basename: true, imports: o.imports });
    fx.fsblock = fs.block;
    fx.implog = fs.implog || zalloc(0x400);
    return fs;
  }

  // ---- the base of every key: the record buffer, the shared block and the freeze
  function env() {
    const fx = { rec: newRec(), lio: zalloc(LIO) };
    freezeOnce();
    return fx;
  }
  function strp(fx, name, text) { fx[name] = keep(Memory.allocUtf8String(text)); }

  // ==================================================================================================== listMatchingFiles
  // Four directories, one per pattern argument: p0 a mixed listing, p1 an empty one (FindFirstFileA fails), p2 one
  // entry, p3 twelve entries. The filter strings are vectors, so one fixture covers every arm of the four tests.
  function dirLists() {
    const s = strs();
    return [
      ['aa1.chr', 'bb2.chr', 'cc', 'dd.txt', s.sh1 + 'x', 'y' + s.sh2 + 'z', 'needle1', 'xneedlex', 'last'],
      [],
      ['only.chr'],
      ['f00', 'f01.chr', 'f02', 'f03.chr', 'f04', 'f05.chr', 'f06', 'f07.chr', 'f08', 'f09.chr', 'f10', 'f11.chr'],
      ['t1', 't2', s.champ, 't3.dat', 't4'],
      ['solo'],
      // the inline comparison at 0x00472513..0x00472531 reads two bytes per iteration: a name that differs in the
      // first byte, one that differs in the second, one that differs in the third, and the name itself twice
      ['pad', s.champ, s.champ[0] + 'Z', s.champ.slice(0, 2) + 'Z', s.champ, 'tail'],
      ['d1.chr', 'd2.chr', 'd3.chr'],
    ];
  }
  function dirKey(o) {
    o = o || {};
    const fx = env();
    const lists = dirLists();
    strp(fx, 'p0', 'dir0\\*.*'); strp(fx, 'p1', 'dir1\\*.*'); strp(fx, 'p2', 'dir2\\*.*'); strp(fx, 'p3', 'dir3\\*.*');
    const byPattern = { 'dir0\\*.*': 0, 'dir1\\*.*': 1, 'dir2\\*.*': 2, 'dir3\\*.*': 3 };
    ['', 'needle', '.chr', 'n1', '.txt', 'zz'].forEach(function (t, i) { strp(fx, 'q' + i, t); });
    seedText(o.text === undefined ? 'seed>' : o.text);
    // the table is seeded with a pattern so the clear at 0x0043d2a7 is observable (a body that skips it leaves the
    // pattern in every slot the enumeration does not fill)
    saveRange(G_DIRTAB, 0x2710);
    for (let i = 0; i < 0x2710; i += 4) ptr(G_DIRTAB).add(i).writeU32((0xc3e90000 + i) >>> 0);
    const s = __importStandIn({ module: EXE, imports: dirSpec(fx.lio, lists, byPattern, o.def === undefined ? 0 : o.def) });
    fx.implog = s.log;
    return fx;
  }
  F.c3ep_dir = function () { return dirKey({}); };
  F.c3ep_dir_empty = function () { return dirKey({ text: '' }); };

  // ==================================================================================================== listThemes
  // One call per key: each variant serves a different directory to the listMatchingFiles call at 0x004724dc.
  function themesKey(li) {
    const fx = env();
    const lists = dirLists();
    seedText('');
    saveRange(G_DIRTAB, 0x2710);
    saveRange(G_THEMES, 0x32 * 16);
    saveRange(G_THEME, 0x40);
    const s = __importStandIn({ module: EXE, imports: dirSpec(fx.lio, lists, null, li) });
    fx.implog = s.log;
    return fx;
  }
  F.c3ep_themes_mix = function () { return themesKey(4); };       // names with and without a dot, and the skipped name
  F.c3ep_themes_one = function () { return themesKey(5); };       // one entry: it lands in slot 0, which is skipped
  F.c3ep_themes_skip = function () { return themesKey(6); };      // the only entry is the skipped name
  F.c3ep_themes_many = function () { return themesKey(3); };      // twelve entries, half of them with a dot
  F.c3ep_themes_empty = function () { return themesKey(1); };     // no entry at all
  F.c3ep_themes_chr = function () { return themesKey(7); };       // three entries, every one with a dot

  // ==================================================================================================== loadThemes
  // loadGolferRoster 0x004659a0 and loadThemeFile 0x00437fa0 are recording stubs: the key compares the path the body
  // builds, the enumeration, the membership flags at 0x00584a0e + 0x2c * i and the stub's call list.
  function loadThemesKey(li) {
    const fx = env();
    const lists = dirLists();
    seedText('');
    saveRange(G_DIRTAB, 0x2710);
    saveRange(G_THEME, 0x40);
    ptr(G_THEME).writeUtf8String('T1');
    saveRange(G_MEMBER - 2, 0x2c * 14);
    const s = __importStandIn({ module: EXE, imports: dirSpec(fx.lio, lists, null, li) });
    fx.implog = s.log;
    stand(fx, 0x004659a0, 0, function () { return 0x5900; });
    stand(fx, 0x00437fa0, 3, function (a) { return 0x3700 + (a[1] & 0xff); });
    return fx;
  }
  F.c3ep_lt_mix = function () { return loadThemesKey(0); };
  F.c3ep_lt_one = function () { return loadThemesKey(2); };
  F.c3ep_lt_empty = function () { return loadThemesKey(1); };
  F.c3ep_lt_many = function () { return loadThemesKey(3); };

  // ==================================================================================================== loadThemeFile
  // Theme files are generated here: 0x230 bytes of record, 0x4e2 bytes of a second record, 0x10 bytes copied to
  // 0x005795a8 + (slot << 8) and 8 trailing bytes. The body reads the record's bytes +0x21 and +0x22 (0x004d60a9 /
  // 0x004d60aa against the table base 0x004d6088, 0x0043818f / 0x00438195): +0x21 picks the counter at 0x0059b76c
  // through its bit 7, +0x22 is the value compared with 0x14 as a signed byte.
  function themeFile(a, c, opt) {
    opt = opt || {};
    const n = opt.len === undefined ? 0x230 + 0x4e2 + 0x10 + 8 : opt.len;
    const b = new Uint8Array(n);
    for (let i = 0; i < n; i++) b[i] = (i * 7 + 0x11) & 0xff;
    if (n > 0x22) { b[0x21] = a & 0xff; b[0x22] = c & 0xff; }
    const r = 0x230 + 0x4e2;                                      // the 0x10-byte record's offset in the file
    for (let i = 0; i < 0x10 && r + i < n; i++) b[r + i] = (opt.rec === undefined ? 0x40 : opt.rec) + i;
    return b;
  }
  function themeFileKey(o) {
    o = o || {};
    const fx = env();
    seedText('');
    saveRange(G_THEME, 0x40);
    ptr(G_THEME).writeUtf8String('T1');
    saveRange(G_THEMEREC, 0x230 * 4);                             // index 0..3
    saveRange(G_THEMEREC + 0x4d * 0x230, 0x230);                  // index 0x4d (the other path arm)
    saveRange(G_THEMEREC2, 0x4e2 * 4);
    saveRange(G_THEMEREC2 + 0x4d * 0x4e2, 0x4e2);
    saveRange(G_SLOTS, 0x400);                                    // slot 0..3
    saveRange(G_SLOT0, 0x14);
    saveRange(G_USED, 8);
    saveRange(G_FLAGS, 4);
    ptr(G_USED).writeU32(o.used0 === undefined ? 0 : o.used0);
    ptr(G_USED).add(4).writeU32(o.used1 === undefined ? 0 : o.used1);
    ptr(G_FLAGS).writeU32(o.flags === undefined ? 0 : o.flags);
    ['t1.chr', 't2.chr', 't3.chr', 't4.chr', 't5.chr', 't6.chr'].forEach(function (n, i) { strp(fx, 'n' + i, n); });
    strp(fx, 'nmiss', 'gone.chr');
    filesOn(fx, { 't1.chr': { bytes: Array.from(themeFile(0x00, 0x20)), capacity: 0x800 },
                  't2.chr': { bytes: Array.from(themeFile(0x80, 0x20, { rec: 0x90 })), capacity: 0x800 },
                  't3.chr': { bytes: Array.from(themeFile(0x00, 0x10)), capacity: 0x800 },
                  't4.chr': { bytes: Array.from(themeFile(0xff, 0x7f, { rec: 0xc0 })), capacity: 0x800 },
                  // t5 ends right after the 0x10-byte record: the 8-byte read at 0x0043817c is short and leaves that
                  // record as the third read wrote it. t6 ends inside the second read, so the record slot keeps the
                  // caller's stack bytes: it is used only with slot -1, which never copies them (0x0043811a).
                  't5.chr': { bytes: Array.from(themeFile(0x00, 0x20, { len: 0x230 + 0x4e2 + 0x10 })), capacity: 0x800 },
                  't6.chr': { bytes: Array.from(themeFile(0x00, 0x20, { len: 0x300 })), capacity: 0x800 } });
    stand(fx, 0x00485790, 1, function () { return 0x8579; }, 'thiscall');
    stand(fx, 0x00473bf0, 7, function () { return 0x73b0; }, 'thiscall');
    return fx;
  }
  F.c3ep_tf = function () { return themeFileKey({}); };
  F.c3ep_tf_flag = function () { return themeFileKey({ flags: 0x4000000 }); };
  F.c3ep_tf_full = function () { return themeFileKey({ used0: 0x48, used1: 0x48 }); };
  F.c3ep_tf_mid = function () { return themeFileKey({ used0: 0x47, used1: 3, flags: 0x4000000 }); };

  // ==================================================================================================== saveGame
  // The low-level layer serves `saved games\<name>.sve`; serializeCourse 0x0040afa0 is a recording stub (it would write
  // the live game state); appendCourseTitle 0x0040daa0 and appendDate 0x0040d7b0 run for real (both only append to the
  // text buffer, which is state).
  function saveKey(o) {
    o = o || {};
    const fx = env();
    seedText('');
    saveRange(G_FD, 4);
    saveRange(G_ITOA, 0x20);
    saveRange(G_DATE, 4);
    ptr(G_DATE).writeU32(o.date >>> 0);
    ['a', 'bb', 'long_save_name_0123456789', 'x.y'].forEach(function (n, i) { strp(fx, 'n' + i, n); });
    strp(fx, 'nbad', 'nosuch');
    const files = {};
    ['a.sve', 'bb.sve', 'long_save_name_0123456789.sve', 'x.y.sve'].forEach(function (n) {
      files[n] = { bytes: null, capacity: 0x200 };
    });
    lowIo(fx, files, { basename: true });
    stand(fx, 0x0040afa0, 1, function () { return 0x0afa; });
    return fx;
  }
  F.c3ep_save = function () { return saveKey({ date: 0 }); };
  F.c3ep_save_d1 = function () { return saveKey({ date: 8192 * 3 + 1024 * 5 + 700 }); };
  F.c3ep_save_d2 = function () { return saveKey({ date: 1023 }); };
  F.c3ep_save_neg = function () { return saveKey({ date: 0x80000000 | 700 }); };          // the `jns` at 0x0040b5ca

  // ==================================================================================================== loadCourse
  // The path stands in the text buffer at entry (the callers leave it there). Every game callee is a recording stub.
  function loadCourseKey(o) {
    o = o || {};
    const fx = env();
    seedText(o.path === undefined ? 'course1.sve' : o.path);
    saveRange(G_FD, 4);
    saveRange(G_LOADFLAG, 4); saveRange(G_567AFC, 4); saveRange(G_C2854, 4); saveRange(G_C2848, 4);
    const body = [];
    for (let i = 0; i < 0x180; i++) body.push((i * 13 + 5) & 0xff);
    lowIo(fx, { 'course1.sve': { bytes: body, capacity: 0x200 },
                'short.sve': { bytes: [1, 2, 3, 4], capacity: 0x100 } }, { basename: true });
    stand(fx, 0x0040d320, 4, function () { return 0x0d32; });
    stand(fx, 0x00480c80, 1, function () { return 0x80c8; }, 'thiscall');
    stand(fx, 0x00442180, 1, function () { return 0x4218; });
    stand(fx, 0x004315e0, 0, function () { return 0x315e; });
    stand(fx, 0x0040bbf0, 1, function () { return 0x0bbf; });
    stand(fx, 0x0042f7a0, 0, function () { return 0x2f7a; });
    return fx;
  }
  F.c3ep_lc = function () { return loadCourseKey({}); };
  F.c3ep_lc_short = function () { return loadCourseKey({ path: 'short.sve' }); };
  F.c3ep_lc_miss = function () { return loadCourseKey({ path: 'gone.sve' }); };

  // ==================================================================================================== loadGame
  function loadGameKey(o) {
    o = o || {};
    const fx = env();
    seedText('');
    saveRange(G_FD, 4);
    saveRange(G_FLAGS, 4); saveRange(G_567AFC, 4); saveRange(G_C2854, 4); saveRange(G_C2848, 4);
    saveRange(G_LOADFLAG, 4); saveRange(G_TYPE, 4);
    ptr(G_FLAGS).writeU32((o.flags === undefined ? 0x0000000e : o.flags) >>> 0);
    ptr(G_TYPE).writeU32((o.type === undefined ? 2 : o.type) >>> 0);
    ['g1.sve', 'g2.sve'].forEach(function (n, i) { strp(fx, 'n' + i, n); });
    strp(fx, 'nbrowse', strs().browse);
    // the inline comparison at 0x0040bb40..0x0040bb62 reads two bytes per iteration: a name equal in the first byte
    // only, and one equal in the first two (neither has a file, which the comparison is reached past)
    strp(fx, 'nb1', strs().browse[0] + 'x.sve');
    strp(fx, 'nb2', strs().browse.slice(0, 2) + 'x.sve');
    strp(fx, 'nmiss', 'gone.sve');
    const body = [];
    for (let i = 0; i < 0x200; i++) body.push((i * 29 + 7) & 0xff);
    const files = { 'g1.sve': { bytes: body, capacity: 0x300 }, 'g2.sve': { bytes: body.slice(0, 0x40), capacity: 0x300 } };
    files[strs().browse] = { bytes: body, capacity: 0x300 };
    lowIo(fx, files, { basename: true });
    stand(fx, 0x0040afa0, 1, function () { return 0x0afa; });
    stand(fx, 0x0042f7a0, 0, function () { return 0x2f7a; });
    stand(fx, 0x0042f340, 0, function () { return 0x2f34; });
    stand(fx, 0x0042f2c0, 0, function () { return 0x2f2c; });
    stand(fx, 0x0040d320, 4, function () { return 0x0d32; });
    stand(fx, 0x00480c80, 1, function () { return 0x80c8; }, 'thiscall');
    stand(fx, 0x00449400, 1, function () {
      if (o.change) ptr(G_TYPE).writeU32((o.change) >>> 0);        // applyCourseType changing 0x005a34e0
      return 0x4940;
    });
    stand(fx, 0x0043dbe0, 0, function () { return 0x3dbe; });
    stand(fx, 0x00449520, 0, function () { return 0x4952; });
    return fx;
  }
  F.c3ep_lg = function () { return loadGameKey({}); };
  F.c3ep_lg_changed = function () { return loadGameKey({ change: 9 }); };
  F.c3ep_lg_flags = function () { return loadGameKey({ flags: 0xf0f0f0f4, type: -1 }); };

  // ==================================================================================================== EditBox::create
  // Real objects from re/frida/js/uitree.js: an EditBox built by its own constructor 0x00486070 and a View as the parent
  // (its word +0x9c carries the style bit 12 the body reads at 0x0048613f).
  // The environment is c3ef's createKey (log/c3/alt/c3ef_fixture_src*.js): the recording display answers the screen
  // size (+0xa8 / +0xac) and hands out Surfaces (+0x7c), scrollbarCreateA / B 0x00496710 / 0x00496740 and
  // buildControlStrip47d610 are stood in, and the palette global 0x0083ad10 carries a palette object. The parents are
  // real windows: Window::create 0x004806c0 is run on them before the key, with its own hook off (T.orig).
  const UI_ARENA = 0x30000;
  const EB_STAND = [{ addr: 0x00496710, abi: 'thiscall', nargs: 5, ret: 0 },
                    { addr: 0x00496740, abi: 'thiscall', nargs: 5, ret: 0 },
                    { addr: 0x0047d610, abi: 'thiscall', nargs: 0, ret: 0 }];
  function editKey(o) {
    o = o || {};
    let T = null;
    T = __uiTree({ arenaSize: UI_ARENA, stand: EB_STAND,
                   dispRet: { 0xa8: 800, 0xac: 600, 0x7c: function () { return T.surface().toUInt32(); } },
                   surfRet: { 4: 0 } });
    const create0 = T.orig(0x004806c0, 'uint32',
      ['pointer', 'int', 'int', 'int', 'int', 'pointer', 'uint32', 'pointer', 'pointer', 'pointer']);
    const p0 = T.make('View'), p1 = T.make('View');
    [p0, p1].forEach(function (p, i) {
      const r = create0(p, 40 + 10 * i, 30, 400, 300, T.str('parent'), 0x1, ptr(0), ptr(0), ptr(0));
      if (r !== 0) throw new Error('c3ep: parent Window::create returned ' + r);
    });
    p0.add(0x9c).writeU32((p0.add(0x9c).readU32() | 0x1000) >>> 0);   // bit 12 set: the body picks 0x2020
    p1.add(0x9c).writeU32((p1.add(0x9c).readU32() & ~0x1000) >>> 0);  // bit 12 clear: 0x102020 (0x0048613f)
    const pal = T.alloc(0x10);
    pal.writeU32(0x0a1e0000); pal.add(4).writeU32(0x0a1e77e5);
    __freeze.setU32(ptr(0x0083ad10), pal.toUInt32());                 // Widget_applyPalette's palette (0x00480846)
    // The chain under test reaches currentFocusOwner 0x0047f2f0, which walks the modal top [0x0083ab54] and the focus
    // globals. uitree.js leaves the modal top at the sentinel 0x0bad0054, which is not a window: the first key of a
    // boot faulted at 0x0bad0210 reading its +0x1bc. The modal top is one of the real parents here, and the three
    // globals currentFocusOwner picks between (plus the one it writes) are pinned and compared.
    T.globals({ modal: p0 });
    [0x0083b990, 0x0083ab38, 0x0083ab60, 0x0083ab3c].forEach(function (a) { __freeze.setU32(ptr(a), 0); });
    const e0 = T.make('EditBox'), e1 = T.make('EditBox'), e2 = T.make('EditBox');
    const t0 = T.str('abcd'), t1 = T.str('');
    e1.add(0x574).writePointer(t0);                                   // the text the body measures at 0x004861ac
    e2.add(0x574).writePointer(t1);
    if (o.failSize !== undefined) T.failSize(o.failSize);
    return T.fx({ e0: e0, e1: e1, e2: e2, p0: p0, p1: p1, t0: t0, t1: t1 });
  }
  F.c3ep_eb = function () { return editKey({}); };
  F.c3ep_eb_fail = function () { return editKey({ failSize: 0x2b8 }); };   // Window::create's +0x568 Image fails

  // ==================================================================================================== promptAndSaveFile
  // 0x00437910: builds a path, asks before overwriting, writes two theme records plus a 0x10-byte record and an
  // 8-byte tag, then draws three tiles into a local Image and saves it as a PCX through the same stream.
  //   * fileExists 0x004378a0 runs for real: it calls FindFirstFileA through the SAME IAT slot 0x004ba090 the
  //     directory stand-in patches (0x004378b0), so the fixture answers "exists" per path (and the original never
  //     calls FindClose, which the stand-in makes harmless);
  //   * sanitizeFileName 0x00405ac0 runs for real (it only edits the record name, which is state);
  //   * fopen / fwrite / fclose go through re/frida/js/filestandin.js;
  //   * every other callee is a recording stub, with the local Image pointer masked out of the record.
  // The two Surface records the body reads a jgld handle from (0x00562918 + 0x84 * idx and 0x00563368 + 0x84 * idx)
  // are seeded with a synthetic handle whose vtable slot +0x14 records its call and returns a constant; the body only
  // reads those slots, so they are saved and restored by the freeze and are not state regions.
  const PSF_STAND = [
    // addr, stack args, abi, masked argument indices (0 = `this` for a thiscall stub)
    [0x00483030, 0, 'thiscall', null, 0x4830],                       // Cache483030::attach (0x00437939)
    [0x0046de70, 3, 'default', null, 0x6de7],                        // scrollTextWindow (0x00437bf8 / 0x00437c53)
    [0x00474ae0, 0, 'thiscall', [0], 0x74ae],                        // Image_ctor (0x00437d2b)
    [0x00474dd0, 6, 'thiscall', [0], 0x74dd],                        // Surface_createFull (0x00437d51)
    [0x00478af0, 5, 'thiscall', [0], 0x78af],                        // Win_fillRect (0x00437d6d)
    [0x004789f0, 1, 'thiscall', [0], 0x7890],                        // Widget_applyPalette (0x00437db8 / 0x00437ea7)
    [0x00473cb0, 7, 'thiscall', [1], 0x73cb],                        // Surface_drawScaled (its `this` is a global table)
    [0x00485aa0, 1, 'thiscall', [0], 0x85aa],                        // Image::savePcx (0x00437f5f)
    [0x00474c40, 0, 'thiscall', [0], 0x74c4],                        // Image_dtor (0x00437f7c)
  ];
  const PSF_NAMES = ['SAVE1', 'SAVE2', 'BAD?NAME'];                  // record 2's name carries one of 0x004c3ee0
  const PSF_SIGN = [0x80, 0x00, 0x80];                               // record byte +0x21: bit 7 set picks counter 0
  const PSF_COUNT = [0x05, 0x20, 0x03];                              // record byte +0x22: < 0x14 or not
  // paths FindFirstFileA answers for (normalised, full): everything else does not exist
  const PSF_FOUND = ['themes\\default.pro', 'themes\\t1\\save1.pro', 'themes\\championship\\save1.pro'];
  function psfKey(o) {
    o = o || {};
    const fx = env();
    seedText('');
    saveRange(G_THEME, 0x40);
    ptr(G_THEME).writeUtf8String('T1');
    saveRange(G_THEMEREC, 0x230 * 3);
    saveRange(G_THEMEREC2, 0x4e2 * 3);
    saveRange(G_SLOTS, 0x400);
    saveRange(G_SLOT0, 0x14);
    saveRange(0x004c7b34, 4);
    saveRange(0x005aa564, 4);
    ptr(0x004c7b34).writeU32(o.cache ? 1 : 0);                       // 0x0043791e
    ptr(0x005aa564).writeU32(0);
    for (let i = 0; i < 3; i++) {
      const r = ptr(G_THEMEREC).add(0x230 * i);
      for (let k = 0; k < 0x230; k += 4) r.add(k).writeU32((0x51000000 + i * 0x10000 + k) >>> 0);
      r.add(0x10).writeUtf8String(PSF_NAMES[i]);                     // the record name at +0x10 (0x004d6098)
      r.add(0x21).writeU8(PSF_SIGN[i]);
      r.add(0x22).writeU8(PSF_COUNT[i]);
      const r2 = ptr(G_THEMEREC2).add(0x4e2 * i);
      for (let k = 0; k + 4 <= 0x4e2; k += 4) r2.add(k).writeU32((0x52000000 + i * 0x10000 + k) >>> 0);
    }
    for (let s = 0; s < 4; s++) {
      const p = ptr(G_SLOTS).add(s << 8);
      for (let k = 0; k < 0x10; k += 4) p.add(k).writeU32((0x53000000 + s * 0x100 + k) >>> 0);
    }
    for (let k = 0; k < 0x14; k += 4) ptr(G_SLOT0).add(k).writeU32((0x54000000 + k) >>> 0);
    // the two jgld handles the body calls slot +0x14 on: index 0 takes the low arm (counter 5, counter word 0),
    // index 1 the high arm (counter 0x20 - 0x14 = 12, counter word 1, so 12 + 72)
    const handle = function (tag) {
      const vt = zalloc(0x40);
      const cb = keep(new NativeCallback(function () { record(fx.rec, 0x14000000 | tag, [tag]); return tag >>> 0; },
                                         'uint32', ['uint32'], 'thiscall'));
      for (let k = 0; k < 0x40; k += 4) vt.add(k).writePointer(cb);
      const obj = zalloc(0x20);
      obj.writePointer(vt);
      return obj;
    };
    const low = 0x00562918 + 0x84 * 5, high = 0x00563368 + 0x84 * (12 + 72);
    saveRange(low, 4); ptr(low).writePointer(handle(0x101));
    saveRange(high, 4); ptr(high).writePointer(handle(0x202));
    PSF_STAND.forEach(function (s) {
      stand(fx, s[0], s[1], function () { return s[4]; }, s[2], s[3]);
    });
    stand(fx, 0x0046d6e0, 5, function () { return o.cancel ? 1 : 0; }, 'default');              // showPopup
    const found = {};
    PSF_FOUND.forEach(function (p) { found[p] = 0; });               // list index 0 (one entry): the file exists
    const lists = [['x'], []];
    // the body writes 0x230 + 0x4e2 + 0x10 + 8 = 0x72a bytes per file (the PCX goes to a stood-in savePcx)
    filesOn(fx, { 'save1.chr': { bytes: null, capacity: 0x800 }, 'save1.pro': { bytes: null, capacity: 0x800 },
                  'save2.chr': { bytes: null, capacity: 0x800 }, 'save2.pro': { bytes: null, capacity: 0x800 } },
            { crt: ['fopen', 'fwrite', 'fclose'],
              imports: dirSpec(fx.lio, lists, function (p) { return p in found ? 0 : 1; }, 1) });
    return fx;
  }
  F.c3ep_psf = function () { return psfKey({}); };
  F.c3ep_psf_cancel = function () { return psfKey({ cancel: 1 }); };
  F.c3ep_psf_cache = function () { return psfKey({ cache: 1 }); };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
