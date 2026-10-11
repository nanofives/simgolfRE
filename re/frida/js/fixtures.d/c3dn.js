// Fixtures for batch c3dn (round 27, jgld.dll). Reimplementations: shim/src/re/c3dn.cpp. Registry:
// re/frida/registry.d/c3dn.py. Purposes and branch tables: log/c3/c3dn_purpose.md.
//
// Everything a vector touches is the fixture's own memory, built on jgld's own vtables where a body or a callee
// dispatches through one. Every Win32 import a body (or a callee) reaches is stood in for the key
// (re/frida/js/importstandin.js; the call log $implog is compared state). Module globals the bodies read or write
// are only written with the game's first thread frozen (re/frida/js/freeze.js), which writes the game's values back
// before it resumes the thread. jgld.dll is relocated at run time, so every module address is built from the
// running base.

(function () {
  const jgld = () => Process.getModuleByName('jgld.dll').base;
  const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };
  const zero = (p, n) => p.writeByteArray(new Array(n).fill(0));
  const u32 = (v) => (typeof v === 'number' ? v : v.toUInt32()) >>> 0;

  // c3cw's settle guard (as c3dc / c3dk): wait until the tracked list's count (+0xc) has not moved for 0.5 s.
  function settle() {
    const glist = jgld().add(0x128428);
    let last = glist.add(0xc).readS32(), still = 0;
    for (let t = 0; still < 5; t++) {
      if (t >= 600) throw new Error('c3dn: tracked list still changing after 60 s; not freezing');
      Thread.sleep(0.1);
      const c = glist.add(0xc).readS32();
      still = c === last ? still + 1 : 0;
      last = c;
    }
  }

  // Executable stubs (each on its own kept page).
  function code(bytes) {
    const page = Process.pageSize;
    const p = __keepAlloc(page);
    Memory.protect(p, page, 'rwx');
    p.writeByteArray(bytes);
    return p;
  }
  const le = (v) => { const a = []; for (let i = 0; i < 4; i++) a.push((v >>> (8 * i)) & 0xff); return a; };
  const addrBytes = (p) => le(p.toUInt32());
  const cstr = (s) => { const p = __keepAlloc(s.length + 1); p.writeUtf8String(s); return p; };
  function vtable(n, def, slots) {
    const vt = __keepAlloc(4 * n);
    for (let i = 0; i < n; i++) vt.add(4 * i).writePointer(def);
    Object.keys(slots || {}).forEach(function (o) { vt.add(Number(o)).writePointer(slots[o]); });
    return vt;
  }
  // c3dg's trap: an unexpected slot returns 0x0badc0de and counts the call at its own word.
  function trapStub() {
    const ctr = __keepAlloc(4); ctr.writeU32(0);
    return code([0xFF, 0x05].concat(addrBytes(ctr), [0xB8], le(0x0badc0de), [0xC3]));
  }
  // c3dk's __thiscall recorder with n stack arguments: appends {ecx, arg0..arg(n-1)} at log+4+count*4*(n+1),
  // increments the count at log+0, returns `ret`, pops 4n bytes.
  function recThis(log, n, ret) {
    const stride = 4 * (n + 1);
    let b = [0xA1].concat(addrBytes(log), [0x69, 0xC0], le(stride), [0x05], addrBytes(log.add(4)), [0x89, 0x08]);
    for (let i = 0; i < n; i++) b = b.concat([0x8B, 0x54, 0x24, 4 + 4 * i, 0x89, 0x50, 4 + 4 * i]);
    b = b.concat([0xFF, 0x05], addrBytes(log), [0xB8], le(ret));
    b = b.concat(n ? [0xC2, (4 * n) & 0xff, 0x00] : [0xC3]);
    return code(b);
  }
  // c3dk's string recorder: the bytes of a C string packed four to a dword, zeroed after the terminator.
  function strWords(p, max) {
    const out = [];
    if (p.isNull()) return [0];
    const u = new Uint8Array(p.readByteArray(4 * max));
    let end = false;
    for (let i = 0; i < max && !end; i++) {
      let w = 0;
      for (let k = 0; k < 4; k++) {
        const c = end ? 0 : u[4 * i + k];
        if (!c) end = true;
        w |= c << (8 * k);
      }
      out.push(w >>> 0);
    }
    return out;
  }
  // the n bytes at p, packed four to a dword
  function bytesWords(p, n) {
    const out = [];
    if (p.isNull()) return [0];
    const u = new Uint8Array(p.readByteArray(n));
    for (let i = 0; i < n; i += 4) {
      let w = 0;
      for (let k = 0; k < 4 && i + k < n; k++) w |= u[i + k] << (8 * k);
      out.push(w >>> 0);
    }
    return out;
  }
  const words = (p, n) => { const o = []; for (let i = 0; i < n; i++) o.push(p.add(4 * i).readU32()); return o; };
  // a small deterministic mix of argument values, for import results with a spread
  const mix = (a, b) => (((u32(a) * 0x9e3779b1) ^ (u32(b) * 0x85ebca6b)) >>> 0);

  // ============================================================================ group W: Display import wrappers
  // Twelve Display-sized objects in $blk (0x160 apart; Display is 0x14c bytes, re/names/jgld_3.tsv), each with a
  // window handle at +0x144 (D2 and D7: 0), an instance handle at +0x140, a released object at +0x148 (D0, D3, D6,
  // D9: objects whose vtable slot +8 records into $rellog) and a patterned DEVMODE at +0x98. Every Display uses one
  // vtable of our own: slot +0x58 records (this, cx, cy, flag) into $modelog and returns 0x58; every other slot is
  // the trap. RECTs $R0..$R2, the ints $P0..$P5 (getCursor's outputs) sit in $blk too.
  // cds: 'ok' (ChangeDisplaySettingsA returns DISP_CHANGE_SUCCESSFUL 0), 'fail' (DISP_CHANGE_FAILED -1) or 'mix'
  // (a value derived from the DEVMODE it is handed: 0, -1 or 1 across the vectors).
  const HWNDS = [0x00010010, 0x00020022, 0, 0x7fff0004, 0xffff1234, 0x00000001, 0x0abc0def, 0, 0x00300030,
                 0x80000000, 0x0000fffe, 0x12345678];
  const HINSTS = [0x10000000, 0, 0x00400000, 0x7ff00000, 1, 0x00510000, 0xffffffff, 0x10000000, 0, 0x00020000,
                  0x00400000, 0x12340000];
  function displayFx(cds) {
    const blk = __keepAlloc(0x1400);
    fill(blk, 0x1400, 0x3d11);
    const out = { obj: blk, blk: blk };
    const trap = trapStub();
    const modelog = __keepAlloc(0x100); zero(modelog, 0x100);
    const rellog = __keepAlloc(0x80); zero(rellog, 0x80);
    out.modelog = modelog; out.rellog = rellog;
    const vtD = vtable(0x40, trap, { 0x58: recThis(modelog, 3, 0x58) });
    const vtR = vtable(4, trap, { 8: recThis(rellog, 0, 0x5e1) });
    for (let i = 0; i < 12; i++) {
      const d = blk.add(0x10 + i * 0x160);
      d.writePointer(vtD);
      d.add(0x144).writeU32(HWNDS[i]);
      d.add(0x140).writeU32(HINSTS[i]);
      if (i % 3 === 0) {
        const r = blk.add(0x1100 + i * 0x10);
        r.writePointer(vtR);
        d.add(0x148).writePointer(r);
      } else {
        d.add(0x148).writeU32(0);
      }
      out['D' + i] = d;
    }
    for (let i = 0; i < 3; i++) out['R' + i] = blk.add(0x1200 + i * 0x20);
    for (let i = 0; i < 6; i++) out['P' + i] = blk.add(0x1300 + i * 8);
    const cdsResult = (dm) => {
      if (cds === 'ok') return 0;
      if (cds === 'fail') return 0xffffffff;
      const h = dm.isNull() ? 0 : (dm.readU32() ^ dm.add(0x6c).readU32());
      return [0, 0xffffffff, 1][h % 3];
    };
    const s = __importStandIn({ module: 'jgld.dll', logSize: 0x400, imports: {
      'USER32.dll!ShowWindow': { args: 2, impl: (a) => (mix(a[0], a[1]) & 0xffff) },
      'USER32.dll!GetWindowRect': { args: 2, impl: (a) => {
        const h = u32(a[0]);
        if (!a[1].isNull()) {
          a[1].writeS32(h & 0xff); a[1].add(4).writeS32((h >>> 8) & 0xff);
          a[1].add(8).writeS32(640 + (h & 0x3f)); a[1].add(12).writeS32(480 + ((h >>> 16) & 0x3f));
        }
        return h ? 1 : 0;
      } },
      'USER32.dll!InvalidateRect': { args: 3, impl: (a) => (mix(a[0], a[2]) >>> 31) },
      'USER32.dll!SetWindowPos': { args: 7, impl: (a) => ((u32(a[0]) ^ u32(a[2]) ^ u32(a[5])) & 1) },
      // getCursor's POINT is a stack local: its address is not recorded, the values ScreenToClient sees are
      'USER32.dll!GetCursorPos': { args: 1, record: false, impl: (a, ctx) => {
        a[0].writeS32(300 + 17 * ctx.call); a[0].add(4).writeS32(200 - 11 * ctx.call);
        return 1;
      } },
      'USER32.dll!ScreenToClient': { args: 2, impl: (a, ctx) => {
        const h = u32(a[0]);
        const x = a[1].readS32() - (h & 0xff), y = a[1].add(4).readS32() - ((h >>> 8) & 0xff);
        a[1].writeS32(x); a[1].add(4).writeS32(y);
        ctx.record([h, x >>> 0, y >>> 0]);
        return h ? 1 : 0;
      } },
      // setResolution hands a stack copy: record the flags and the 0x94 bytes, never the address
      'USER32.dll!ChangeDisplaySettingsA': { args: 2, impl: (a, ctx) => {
        ctx.record([u32(a[1])].concat(bytesWords(a[0], 0x94)));
        return cdsResult(a[0]);
      } },
      'USER32.dll!EnumDisplaySettingsA': { args: 3, impl: (a, ctx) => {
        for (let i = 0; i < 0x94; i += 4) a[2].add(i).writeU32((0x0e000000 + i * 0x01010101 + ctx.call) >>> 0);
        return 1;
      } },
      'USER32.dll!GetSystemMetrics': { args: 1, impl: (a) => [1024, 768][u32(a[0])] || 0x5a },
      'USER32.dll!DestroyWindow': { args: 1, impl: (a) => (u32(a[0]) & 0x10) ? 0 : 1 },
      'USER32.dll!UnregisterClassA': { args: 2, impl: (a, ctx) => {
        ctx.record(strWords(a[0], 8).concat([u32(a[1])]));
        return u32(a[1]) ? 1 : 0;
      } },
    } });
    out.implog = s.log;
    return out;
  }

  // ============================================================================ group M: module globals
  // One recorder fixture for the keys over the font DC 0x10128724, the module palette 0x10128738 and the event sink
  // 0x1012872c. The globals are set with the first thread frozen (the game's values go back before the resume) and
  // are state regions. Imports: DeleteDC, DeleteObject, AnimatePalette, OutputDebugStringA (records the message),
  // InitializeCriticalSection / DeleteCriticalSection, BeginPaint / EndPaint (BeginPaint fills the PAINTSTRUCT from
  // the window handle and returns NULL for a handle with bit 4 set), InvalidateRect, DefWindowProcA, and the four
  // realizePalette 0x100699c0 reaches (GetDC, SelectPalette, RealizePalette, ReleaseDC).
  // The sink's vtable: slot +4 (onPaint) records the RECT it is handed (four values, not the stack address), slot
  // +0x2c (onActivate) records its two arguments; both write into the sink's own record area (cursor at +4).
  //   g: { font, pal } values for the globals (undefined = leave the game's value, frozen anyway)
  //   r: { dc, obj } results of DeleteDC / DeleteObject
  function sinkObject() {
    const sink = __keepAlloc(0x200);
    fill(sink, 0x200, 0x5c0e);
    // slot +4: mov eax,[ecx+4]; add eax,ecx; mov dword [eax],0xB4000000; mov edx,[esp+4]; push esi;
    //          (mov esi,[edx+4k]; mov [eax+4+4k],esi) x4; pop esi; add dword [ecx+4],0x14; mov eax,0x44; ret 4
    let b = [0x8B, 0x41, 0x04, 0x03, 0xC1, 0xC7, 0x00].concat(le(0xB4000000), [0x8B, 0x54, 0x24, 0x04, 0x56]);
    for (let k = 0; k < 4; k++) b = b.concat([0x8B, 0x72, 4 * k, 0x89, 0x70, 4 + 4 * k]);
    b = b.concat([0x5E, 0x83, 0x41, 0x04, 0x14, 0xB8], le(0x44), [0xC2, 0x04, 0x00]);
    const s4 = code(b);
    // slot +0x2c: mov eax,[ecx+4]; add eax,ecx; mov dword [eax],0xB2C00000; (mov edx,[esp+4i]; mov [eax+4i],edx)
    //   i=1,2; add dword [ecx+4],0xc; mov eax,0x2c; ret 8
    let c = [0x8B, 0x41, 0x04, 0x03, 0xC1, 0xC7, 0x00].concat(le(0xB2C00000));
    for (let i = 1; i <= 2; i++) c = c.concat([0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 * i]);
    c = c.concat([0x83, 0x41, 0x04, 0x0C, 0xB8], le(0x2c), [0xC2, 0x08, 0x00]);
    const s2c = code(c);
    sink.writePointer(vtable(0x20, trapStub(), { 4: s4, 0x2c: s2c }));
    sink.add(4).writeU32(0x40);
    return sink;
  }
  function globalsFx(g, r) {
    settle();
    const base = jgld();
    const blk = __keepAlloc(0x1000);
    fill(blk, 0x1000, 0x6a7e);
    const out = { obj: blk, blk: blk };
    for (let i = 0; i < 12; i++) out['O' + i] = blk.add(0x20 + i * 0x80 + (i & 3));    // Palette-like objects
    const msgs = ['fixture message', '', 'x', 'libpng warning: bad', 'A rather longer message of forty-one ch',
                  '\t tab', 'end\n', '%s %d', 'zz', 'Q', 'twelve chars', 'last one'];
    msgs.forEach((m, i) => { out['m' + i] = cstr(m); });
    const sink = sinkObject();
    out.sink = sink;
    const s = __importStandIn({ module: 'jgld.dll', logSize: 0x400, imports: {
      'GDI32.dll!DeleteDC': { args: 1, ret: r.dc },
      'GDI32.dll!DeleteObject': { args: 1, ret: r.obj },
      'GDI32.dll!AnimatePalette': { args: 4, impl: (a) => (u32(a[3]) >>> 2) & 0xff },
      'KERNEL32.dll!OutputDebugStringA': { args: 1, impl: (a, ctx) => {
        const w = strWords(a[0], 12);
        ctx.record(w);
        return w.length * 4 + (w[0] & 0xff);
      } },
      'KERNEL32.dll!InitializeCriticalSection': { args: 1, ret: 0x1c },
      'KERNEL32.dll!DeleteCriticalSection': { args: 1, ret: 0xdc },
      // the PAINTSTRUCT is a stack local: record the window handle only
      'USER32.dll!BeginPaint': { args: 2, record: [0], impl: (a) => {
        const h = u32(a[0]);
        for (let i = 0; i < 0x40; i += 4) a[1].add(i).writeU32((h + i * 0x01000193) >>> 0);
        return (h & 0x10) ? 0 : (0x0dc00000 | (h & 0xffff));
      } },
      'USER32.dll!EndPaint': { args: 2, impl: (a, ctx) => {
        ctx.record([u32(a[0])].concat(words(a[1], 16)));
        return (u32(a[0]) & 3) + 1;
      } },
      'USER32.dll!InvalidateRect': { args: 3, ret: 1 },
      'USER32.dll!DefWindowProcA': { args: 4, impl: (a) => mix(mix(a[0], a[1]), mix(a[2], a[3])) },
      'USER32.dll!GetDC': { args: 1, impl: (a) => (0x0d000000 | (u32(a[0]) & 0xffff)) >>> 0 },
      'GDI32.dll!SelectPalette': { args: 3, ret: 0x08000001 },
      'GDI32.dll!RealizePalette': { args: 1, ret: 7 },
      'USER32.dll!ReleaseDC': { args: 2, ret: 1 },
    } });
    out.implog = s.log;
    out.gfont = base.add(0x128724);
    out.gpal = base.add(0x128738);
    out.gsink = base.add(0x12872c);
    __freeze.thread();
    __freeze.save(out.gfont, 4);
    __freeze.save(out.gpal, 4);
    __freeze.save(out.gsink, 4);
    if (g.font !== undefined) out.gfont.writeU32(g.font);
    if (g.pal !== undefined) out.gpal.writeU32(g.pal);
    out.gsink.writePointer(sink);
    return out;
  }

  // ============================================================================ group G: Surface GDI wrappers
  // Twelve surfaces of our own on jgld's Surface vtable 0x1011d0b0 (0x820 bytes, patterned), memory DC at +0x4bc
  // (S3 and S8: 0, so acquireDC hands a NULL DC), +0x4b8 / +0x4c4 cleared (S5: count 2 left over, so releaseDC(1)
  // keeps the DC). Fonts F0..F3 (+0x18 = HFONT) and pointers to them (pF0..pF3, the selectObject argument). The
  // font DC 0x10128724 is set to 0x0dc0f0e1 with the first thread frozen for Font::textWidth. Imports: SetTextColor,
  // SelectObject, GetStockObject, TextOutA (records the text it is handed, up to its length), GetTextExtentPoint32A
  // (cx from the text and length, cy 16; the SIZE is a stack local).
  function surfaceFx() {
    settle();
    const base = jgld();
    const vtSurface = base.add(0x11d0b0);
    const surf = __keepAlloc(12 * 0x820);
    const out = { obj: surf, surf: surf };
    for (let i = 0; i < 12; i++) {
      const s = surf.add(i * 0x820);
      fill(s, 0x820, 0x5f00 + i);
      s.writePointer(vtSurface);
      s.add(0x4bc).writeU32(i === 3 || i === 8 ? 0 : (0x0dc00100 + i * 0x11) >>> 0);
      s.add(0x4b8).writeU32(0);
      s.add(0x4c4).writeU32(i === 5 ? 2 : 0);
      out['S' + i] = s;
    }
    const fonts = __keepAlloc(0x100);
    fill(fonts, 0x100, 0x7f07);
    for (let i = 0; i < 4; i++) {
      const f = fonts.add(i * 0x30);
      f.add(0x18).writeU32((0x0f000010 + i * 0x101) >>> 0);
      out['F' + i] = f;
      const pf = fonts.add(0xc0 + i * 4);
      pf.writePointer(f);
      out['pF' + i] = pf;
    }
    out.fonts = fonts;
    ['Hello', '', 'SimGolf fixture text', 'ab', 'x\ty', 'longer line of text 0123456789'].forEach((t, i) => {
      out['t' + i] = cstr(t);
    });
    const s = __importStandIn({ module: 'jgld.dll', logSize: 0x400, imports: {
      'GDI32.dll!SetTextColor': { args: 2, impl: (a) => (u32(a[1]) ^ 0x00ffffff) >>> 0 },
      'GDI32.dll!SelectObject': { args: 2, impl: (a) => (u32(a[1]) + 1) >>> 0 },
      'GDI32.dll!GetStockObject': { args: 1, impl: (a) => (0x018a0000 | u32(a[0])) >>> 0 },
      'GDI32.dll!TextOutA': { args: 5, impl: (a, ctx) => {
        const n = u32(a[4]);
        const text = (a[3].isNull() || n === 0 || n > 64) ? [] : bytesWords(a[3], n);
        ctx.record([u32(a[0]), u32(a[1]), u32(a[2]), n].concat(text));
        return n & 1;
      } },
      'GDI32.dll!GetTextExtentPoint32A': { args: 4, record: [0, 1, 2], impl: (a) => {
        const n = u32(a[2]);
        const first = (a[1].isNull() || n === 0 || n > 64) ? 0 : a[1].readU8();
        a[3].writeS32(n * 7 + (first & 0xf)); a[3].add(4).writeS32(16);
        return 1;
      } },
    } });
    out.implog = s.log;
    out.gfont = base.add(0x128724);
    __freeze.thread();
    __freeze.setU32(out.gfont, 0x0dc0f0e1);
    return out;
  }

  // ============================================================================ groups C / H: tracked objects
  // c3dk's heap stand-in entries: jgld operator new 0x1007f0f0, malloc 0x1007fb40, calloc 0x1007ffa0 carve from the
  // compared arena; operator delete 0x1007e6d0 and free 0x100806d0 only record (log $freelog).
  function standIn(size, extra) {
    const o = { module: 'jgld.dll',
                alloc: [0x7f0f0, 0x7fb40, { rva: 0x7ffa0, args: 2, sizeArgs: 2, fill: 0 }],
                free: [0x7e6d0, 0x806d0], arenaSize: size || 0x400, fill: 0xcd };
    if (extra) Object.assign(o, extra);
    return __heapStandIn(o);
  }
  // c3cw / c3dc's swapped tracked list: the 0x1c-byte header of 0x10128428 replaced, first thread frozen, by a list
  // of our own whose nodes' vtable slot 0 is the counting "deleting destructor" stub (eax = ++*ctr; node[+0x14] =
  // eax; ret 4), so the node removeCurrent deletes is compared state. Ordered word 0: LinkedList::add never sends
  // its debug message.
  function nodeVtable(ctr) {
    ctr.writeU32(0);
    const c = code([0xA1].concat(addrBytes(ctr), [0x40, 0xA3], addrBytes(ctr), [0x89, 0x41, 0x14, 0xC2, 0x04, 0x00]));
    const vt = __keepAlloc(0x20);
    for (let i = 0; i < 8; i++) vt.add(i * 4).writePointer(c);
    return vt;
  }
  function buildList(list, nodes, datas, cur, vt) {
    const n = datas.length, ps = [];
    for (let k = 0; k < n; k++) {
      const p = nodes.add(k * 0x20);
      p.writePointer(vt);
      p.add(0xc).writePointer(ptr(datas[k]));
      p.add(0x10).writeU8(0x40 + k);
      ps.push(p);
    }
    for (let k = 0; k < n; k++) {
      ps[k].add(4).writePointer(ps[(k + n - 1) % n]);
      ps[k].add(8).writePointer(ps[(k + 1) % n]);
    }
    list.add(4).writePointer(n && cur >= 0 ? ptr(datas[cur]) : ptr(0x5a5a0001));
    list.add(8).writeU32(n && cur >= 0 ? (0x40 + cur) : 0x5a5a0002);
    list.add(0xc).writeS32(n);
    list.add(0x10).writePointer(n ? ps[0] : ptr(0));
    list.add(0x14).writePointer(n && cur >= 0 ? ps[cur] : ptr(0));
    list.add(0x18).writeU32(0);
  }
  // Twelve objects $o0..$o11 (0x600 apart in $blk, patterned) laid out as Surfaces on jgld's Surface vtable
  // 0x1011d0b0: owner +0x80 (i % 3 == 0: an owner block at $blk + 0x7000 + 0x10 i), handles by the bits of i:
  // memory DC +0x4bc (bit 0), previous bitmap +0x4b0 (bit 1), DIB section +0x4b4 (bit 2), clip region +0x4ac
  // (bit 3, and o7). Array words: +4 = an element block for odd i (Array_Rec14::clearRet frees it), else 0.
  // shape: 'empty' | 'five' (the list holds $o0..$o4, current node 2) | 'all' (all twelve, current = head).
  //   opts.failMask: scheduled allocation failures (the factories' NULL arm); opts.cs: InitializeCriticalSection
  //   is recorded either way.
  function objectsFx(shape, opts) {
    opts = opts || {};
    settle();
    const base = jgld();
    const h = standIn(0x1000, opts.failMask ? { failMask: opts.failMask } : undefined);
    const blk = __keepAlloc(0x8000);
    fill(blk, 0x8000, 0x0b1e);
    const vt = nodeVtable(blk.add(0x7ff0));
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk };
    for (let i = 0; i < 12; i++) {
      const o = blk.add(i * 0x600);
      o.writePointer(base.add(0x11d0b0));
      o.add(4).writePointer(i & 1 ? blk.add(0x7800 + i * 0x10) : ptr(0));
      o.add(0x80).writePointer(i % 3 === 0 ? blk.add(0x7000 + i * 0x10) : ptr(0));
      o.add(0x4bc).writeU32(i & 1 ? (0x0dc00000 + i) >>> 0 : 0);
      o.add(0x4b0).writeU32(i & 2 ? (0x0b000000 + i) >>> 0 : 0);
      o.add(0x4b4).writeU32(i & 4 ? (0x0d1b0000 + i) >>> 0 : 0);
      o.add(0x4ac).writeU32(i & 8 || i === 7 ? (0x0e000000 + i) >>> 0 : 0);
      out['o' + i] = o;
      out['w' + i] = blk.add(0x7400 + i * 0x10);       // owner arguments (opaque to the constructors)
    }
    const s = __importStandIn({ module: 'jgld.dll', imports: {
      'KERNEL32.dll!InitializeCriticalSection': { args: 1, ret: 0x1c },
      'KERNEL32.dll!DeleteCriticalSection': { args: 1, ret: 0xdc },
      'KERNEL32.dll!OutputDebugStringA': { args: 1, impl: (a, ctx) => { ctx.record(strWords(a[0], 8)); return 0; } },
      'GDI32.dll!SelectObject': { args: 2, impl: (a) => (u32(a[1]) ^ 0x5e1) >>> 0 },
      'GDI32.dll!DeleteDC': { args: 1, impl: (a) => u32(a[0]) & 1 },
      'GDI32.dll!DeleteObject': { args: 1, impl: (a) => (u32(a[0]) >>> 1) & 1 },
    } });
    out.implog = s.log;
    const glist = base.add(0x128428);
    out.glist = glist;
    const n = { empty: 0, five: 5, all: 12 }[shape];
    const datas = [];
    for (let k = 0; k < n; k++) datas.push(out['o' + k]);
    __freeze.thread();
    __freeze.save(glist, 0x1c);
    buildList(glist, blk.add(0x6c00), datas, shape === 'five' ? 2 : 0, vt);
    return out;
  }

  // c3dg's Sprite set (re/frida/js/fixtures.d/c3dg.js spriteFx) for Sprite::deletingDtor: Sprites on jgld's Sprite
  // vtable 0x1011d380 with the fields Sprite::release reads (payload +0x14, flags +0x18, span table +0x1c, span count
  // +0x24, +0x2c, width +0x30, height +0x34) and an owner word (+0xc) the destructor clears; the sprite counters at
  // 0x101284c8 / 0x101284cc are set under the freeze (they are state, the game's values go back).
  const SPR = [
    [1, 0x00, 0, 0x10, 16, 16, 8, 1], [1, 0x01, 0, 0x30, 16, 16, 8, 1], [1, 0x03, 1, 0x44, 20, 20, 10, 0],
    [0, 0x01, 1, 0x50, 8, 8, 4, 1], [0, 0x00, 0, 0x00, 8, 8, 4, 0], [1, 0x02, 1, 0x20, 4, 3, 7, 1],
    [1, 0x01, 0, 0x7fffffff, 9, 9, 9, 1], [0, 0xfe, 0, 0x12, 3, 4, 5, 0], [1, 0x01, 1, 0x13, 100, 640, 480, 1],
    [0, 0x00, 0, 0x00, 1, 1, 1, 1], [1, 0xffffffff, 1, 5, 1, 1, 1, 0], [1, 0x00, 1, 0x11, 7, 2, 5, 1],
  ];
  function spriteFx(shape) {
    settle();
    const base = jgld();
    const h = standIn(0x400);
    const blk = __keepAlloc(0x2000);
    fill(blk, 0x2000, 0x5b18);
    const vt = nodeVtable(blk.add(0x1ff0));
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk };
    SPR.forEach(function (s, i) {
      const p = blk.add(0x60 * i);
      const w = blk.add(0x600 + 4 * i);
      p.writePointer(base.add(0x11d380));
      p.add(0xc).writePointer(s[7] ? w : ptr(0));
      p.add(0x14).writePointer(s[0] ? blk.add(0x800 + i * 0x20) : ptr(0));
      p.add(0x18).writeU32(s[1] >>> 0);
      p.add(0x1c).writePointer(s[2] ? blk.add(0xc00 + i * 0x20) : ptr(0));
      p.add(0x24).writeU32(s[3] >>> 0);
      p.add(0x2c).writeS32(s[4]);
      p.add(0x30).writeS32(s[5]);
      p.add(0x34).writeS32(s[6]);
      out['p' + i] = p;
    });
    const s = __importStandIn({ module: 'jgld.dll', imports: {
      'KERNEL32.dll!InitializeCriticalSection': { args: 1 },
      'KERNEL32.dll!DeleteCriticalSection': { args: 1 },
      'KERNEL32.dll!OutputDebugStringA': { args: 1 } } });
    out.implog = s.log;
    const glist = base.add(0x128428), gcount = base.add(0x1284c8);
    out.glist = glist;
    out.gcount = gcount;
    const n = { empty: 0, five: 5, all: 12 }[shape];
    const datas = [];
    for (let k = 0; k < n; k++) datas.push(out['p' + k]);
    __freeze.thread();
    __freeze.save(glist, 0x1c);
    buildList(glist, blk.add(0x1000), datas, shape === 'five' ? 2 : 0, vt);
    __freeze.save(gcount, 8);
    gcount.writeU32(0x00123456);
    gcount.add(4).writeU32(0x00654321);
    return out;
  }

  // constructTrackedList over the live list header (first thread frozen; the header is saved and written back):
  // shape 'live' leaves the game's header in place, 'pat' writes a pattern first, 'five' a list of five.
  function listCtorFx(shape) {
    settle();
    const base = jgld();
    const blk = __keepAlloc(0x400);
    fill(blk, 0x400, 0x1c70);
    const out = { blk: blk, obj: blk };
    const glist = base.add(0x128428);
    out.glist = glist;
    const vt = nodeVtable(blk.add(0x3f0));
    __freeze.thread();
    __freeze.save(glist, 0x1c);
    if (shape === 'pat') fill(glist, 0x1c, 0x1c71);
    if (shape === 'five') buildList(glist, blk, [blk.add(0x200), blk.add(0x240), blk.add(0x280), blk.add(0x2c0),
                                                 blk.add(0x300)], 1, vt);
    return out;
  }

  // ============================================================================ group P: libpng helpers
  // png_structs P0..P5 (0x220 apart, patterned) laid out as c3dk's readDestroyFx (re/frida/js/fixtures.d/c3dk.js):
  // the blocks png_read_destroy 0x1006d250 frees are fake addresses 0x0f0000xx that png_free hands to jgld's free
  // 0x100806d0, which the heap stand-in only records. Info structs J0..J3 (0x100 bytes; text arrays, pCAL purpose /
  // units / parameter arrays of fake blocks). Added here: the chunk name at +0x11c (letters, digits, bytes outside
  // the letter ranges) and the warning function at +0x44: `hdl` (a cdecl recorder of the message TEXT into $hlog,
  // returning its length) or NULL (png_default_warning). Pointer-to-pointer slots for png_destroy_read_struct:
  // $qP<i> -> P<i>, $qJ<j> -> J<j>, $qN -> NULL.
  //   mode 'handler' | 'default' (the stderr stream of jgld's CRT 0x101233f0 is state, and WriteFile, the import
  //   jgld's CRT _write reaches, records the bytes it is handed and reports them written)
  function pngFx(mode) {
    // the default keys run without the heap stand-in: jgld's CRT allocates stderr's temporary buffer once, on the
    // first fprintf, and keeps it in a CRT global, so a block carved from the arena would outlive the key
    const h = mode === 'default' ? null : standIn(0x100);
    const blk = __keepAlloc(0x2000);
    fill(blk, 0x2000, 0x2dd1);
    const out = { obj: blk, blk: blk };
    if (h) { out.arena = h.arena; out.freelog = h.log; }
    let fake = 0x0f000001;
    const nf = () => (fake++) >>> 0;
    const tbl = (off, n) => { const t = blk.add(off); for (let i = 0; i < n; i++) t.add(4 * i).writeU32(nf()); return t; };
    const hlog = __keepAlloc(0x200); zero(hlog, 0x200);
    out.hlog = hlog;
    const hdl = new NativeCallback(function (png, msg) {
      const n = hlog.readU32();
      const w = strWords(msg, 24);
      if (n < 2) {
        const rec = hlog.add(4 + n * 0x68);
        rec.writeU32(png.toUInt32());
        w.forEach((v, i) => rec.add(4 + 4 * i).writeU32(v));
      }
      hlog.writeU32(n + 1);
      return w.length * 4;
    }, 'int', ['pointer', 'pointer'], 'mscdecl');
    out.hdl = hdl;
    const SPEC = [
      [0x7000, 6, [1, 1, 1], true], [0x0000, 6, [0, 0, 0], true], [0x1000, 8, [1, 0, 0], true],
      [0x6000, 9, [0, 1, 1], false], [0xffff, 7, [1, 1, 0], false], [0x0000, 5, [0, 0, 1], true],
    ];
    const NAMES = [[0x49, 0x48, 0x44, 0x52], [0x74, 0x45, 0x58, 0x74], [0x7a, 0x31, 0x5b, 0x00],
                   [0xff, 0x41, 0x40, 0x7b], [0x5a, 0x61, 0x60, 0x20], [0x00, 0x41, 0x42, 0x43]];
    // never four non-letter bytes: png_format_buffer then writes up to index 81 of png_chunk_warning's 80-byte
    // buffer at ebp-0x50 (msg[16+64] in libpng 1.0.5), over the saved ebp (log/c3/c3dn_notes.txt)
    SPEC.forEach(function (sp, i) {
      const p = blk.add(i * 0x220);
      const single = [0xac, 0xec, 0xe8, 0x1ec, 0x1f0, 0x164, 0x168, 0x16c, 0x114, 0x188, 0x1f4, 0x210, 0x1b0];
      single.forEach(function (o) { p.add(o).writeU32(sp[3] ? nf() : 0); });
      p.add(0x6c).writeU32(sp[0]);
      p.add(0x158).writeU32(sp[1]);
      [0x170, 0x174, 0x178].forEach(function (o, k) {
        p.add(o).writePointer(sp[2][k] ? tbl(0x1400 + i * 0x80 + k * 0x28, 8) : ptr(0));
      });
      p.add(0x74 + 0x1c).writeU32(0);                // z_stream state NULL: inflateEnd frees nothing
      p.add(0x44).writePointer(mode === 'handler' ? hdl : ptr(0));
      p.add(0x11c).writeByteArray(NAMES[i]);
      out['P' + i] = p;
      out['qP' + i] = blk.add(0x1e00 + 4 * i);
      out['qP' + i].writePointer(p);
    });
    for (let j = 0; j < 4; j++) {
      const info = blk.add(0xd00 + j * 0x100);
      for (let o = 0; o < 0x100; o += 4) info.add(o).writeU32(0);
      if (j & 1) {
        const text = blk.add(0x1800 + j * 0x40);
        for (let k = 0; k < 2; k++) { text.add(16 * k + 4).writeU32(nf()); text.add(16 * k + 8).writeU32(nf()); }
        info.add(0x30).writeS32(2);
        info.add(0x38).writePointer(text);
      }
      if (j === 2) info.add(0x30).writeS32(-1);        // a negative count with no text array: never read
      info.add(0xa0).writeU32(j ? nf() : 0);
      info.add(0xac).writeU32(j > 1 ? nf() : 0);
      if (j >= 2) {
        info.add(0xb0).writePointer(tbl(0x1900 + j * 0x20, 3));
        info.add(0xb5).writeU8(j === 3 ? 0 : 3);       // J3: an array with a zero count
      }
      out['J' + j] = info;
      out['qJ' + j] = blk.add(0x1e40 + 4 * j);
      out['qJ' + j].writePointer(info);
    }
    out.qN = blk.add(0x1e60); out.qN.writeU32(0);
    const msgs = ['bad chunk', '', 'Incorrect gAMA chunk length', 'x', 'CRC error',
                  'a message of exactly forty bytes ......', 'Unknown zTXt compression type', 'Q'];
    // each message sits in a zeroed 0x80-byte block: png_format_buffer copies 64 bytes from it whatever its length
    msgs.forEach((m, i) => {
      const p = __keepAlloc(0x80);
      zero(p, 0x80);
      p.writeUtf8String(m);
      out['m' + i] = p;
    });
    if (mode === 'default') {
      const s = __importStandIn({ module: 'jgld.dll', logSize: 0x1000, imports: {
        'KERNEL32.dll!WriteFile': { args: 5, impl: (a, ctx) => {
          const n = u32(a[2]);
          ctx.record([u32(a[0]), n].concat(n > 0 && n <= 0x80 ? bytesWords(a[1], n) : []));
          if (!a[3].isNull()) a[3].writeU32(n);
          return 1;
        } } } });
      out.implog = s.log;
      // jgld's stderr is a buffered stream (log/c3/alt/c3dn_stderr_probe.py: _flag 0xa, _bufsiz 0x1000, nothing
      // reaches WriteFile until the buffer fills). Its buffer is allocated by the first fprintf: if that happened
      // inside an arm, the two arms would get different buffers. So the stream is primed here with one newline
      // (before the stand-in is installed), and the FILE (0x20 bytes) and its buffer are the compared state.
      const giob = jgld().add(0x1233f0);
      if (giob.add(8).readPointer().isNull()) {
        const fpf = new NativeFunction(jgld().add(0x83010), 'int', ['pointer', 'pointer'], 'mscdecl');
        fpf(giob, cstr('\n'));
      }
      if (giob.add(8).readPointer().isNull() || giob.add(0x18).readU32() !== 0x1000)
        throw new Error('c3dn: stderr stream not buffered as probed');
      out.giob = giob;
      out.gbuf = giob.add(8).readPointer();
    }
    return out;
  }

  // ============================================================================ group D: Surface dispatchers
  // c3ce's surface set (re/frida/js/fixtures.d/c3ce.js c3ce_surfaces), copied: one block with sixteen Surfaces on
  // jgld's Surface vtable (depths 8, 16 in 565 / 555 / format 2, 12, 24, 32, 48, 4; clip inside / past the bounds; no
  // pixel base; empty bounds), two Palettes on jgld's Palette vtable with their 565 / 555 tables, pixel buffers,
  // argument rectangles. Added: four more Palettes pal3..pal6 whose PALETTEENTRY area (+0xc..+0x40b) is patterned and
  // whose +0x810 (Palette::getField810) is 0x11, 0x22, 0x22, 0; surfaces s0 / s3 start with +0x78 = 0x22 (equal to
  // pal4 / pal5's field) and the rest 0.
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
  // frozen: the first thread is held for the key and the module row-skip / pixel-count words the 8- and 16-bit
  // primitives park (0x1012847c fillRectC8, 0x10128458 fillRect16, 0x10128450 dashedHLine16) are saved and
  // written back before it resumes (c3av's recipe; they are not state regions). setPalette's SetDIBColorTable is
  // stood in and records the 256 quads it is handed.
  function ceFx(frozen) {
    const fx = ceSurfaces();
    const s = __importStandIn({ module: 'jgld.dll', logSize: 0x800, imports: {
      'GDI32.dll!SetDIBColorTable': { args: 4, impl: (a, ctx) => {
        ctx.record([u32(a[1]), u32(a[2])].concat(a[3].isNull() ? [] : words(a[3], 256)));
        return u32(a[2]) & 0xff;
      } } } });
    fx.implog = s.log;
    if (frozen) {
      settle();
      const base = jgld();
      __freeze.thread();
      [0x12847c, 0x128458, 0x128450].forEach((o) => __freeze.save(base.add(o), 4));
    }
    return fx;
  }

  const FX = {
    c3dn_png_handler() { return pngFx('handler'); },
    c3dn_png_default() { return pngFx('default'); },
    c3dn_ce() { return ceFx(false); },
    c3dn_ce_frozen() { return ceFx(true); },
    c3dn_obj_empty() { return objectsFx('empty'); },
    c3dn_obj_five() { return objectsFx('five'); },
    c3dn_obj_all() { return objectsFx('all'); },
    c3dn_obj_oom() { return objectsFx('five', { failMask: 1 }); },
    c3dn_spr_five() { return spriteFx('five'); },
    c3dn_spr_all() { return spriteFx('all'); },
    c3dn_list_live() { return listCtorFx('live'); },
    c3dn_list_pat() { return listCtorFx('pat'); },
    c3dn_list_five() { return listCtorFx('five'); },
    c3dn_disp_ok() { return displayFx('ok'); },
    c3dn_disp_fail() { return displayFx('fail'); },
    c3dn_disp_mix() { return displayFx('mix'); },
    c3dn_surf() { return surfaceFx(); },
    c3dn_glob() { return globalsFx({ font: 0x0dc0f0e1, pal: 0x08000123 }, { dc: 1, obj: 1 }); },
    c3dn_glob_zero() { return globalsFx({ font: 0, pal: 0 }, { dc: 1, obj: 1 }); },
  };
  // the argument-free teardowns: one key per (global value, import result) pair
  const GVALS = [0, 0x0dc0f0e1, 0x0dc00001, 0x7fffffff, 0x80000000, 0xffffffff, 0x00000010, 0x01234567,
                 0x0000fffe, 0x0abcdef0];
  GVALS.forEach(function (v, i) {
    const ret = [1, 0, 2][i % 3];
    FX['c3dn_font_' + i] = function () { return globalsFx({ font: v }, { dc: ret, obj: 1 }); };
    FX['c3dn_pal_' + i] = function () { return globalsFx({ pal: v }, { dc: 1, obj: ret }); };
  });
  Object.assign(globalThis.DIFF_FIXTURES, FX);
})();
