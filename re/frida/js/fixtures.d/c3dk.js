// Fixtures for batch c3dk (round 26, jgld.dll). Reimplementations: shim/src/re/c3dk.cpp. Registry:
// re/frida/registry.d/c3dk.py. Purposes and branch tables: log/c3/c3dk_purpose.md.
//
// Everything a vector touches is the fixture's own memory, built on jgld's own vtables where a body or a callee
// dispatches through one. Module globals the bodies read or write are only written with the game's first thread
// frozen (re/frida/js/freeze.js), which writes the game's values back before it resumes the thread.
// jgld.dll is relocated at run time, so every module address is built from the running base.

(function () {
  const jgld = () => Process.getModuleByName('jgld.dll').base;
  const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

  function standIn(size, extra) {
    const o = { module: 'jgld.dll',
                alloc: [0x7f0f0, 0x7fb40, { rva: 0x7ffa0, args: 2, sizeArgs: 2, fill: 0 }],
                free: [0x7e6d0, 0x806d0], arenaSize: size || 0x400, fill: 0xcd };
    if (extra) Object.assign(o, extra);
    return __heapStandIn(o);
  }

  // c3cw's settle guard (copied from c3dc): wait until the tracked list's count (+0xc) has not moved for 0.5 s.
  function settle() {
    const glist = jgld().add(0x128428);
    let last = glist.add(0xc).readS32(), still = 0;
    for (let t = 0; still < 5; t++) {
      if (t >= 600) throw new Error('c3dk: tracked list still changing after 60 s; not freezing');
      Thread.sleep(0.1);
      const c = glist.add(0xc).readS32();
      still = c === last ? still + 1 : 0;
      last = c;
    }
  }

  // Executable stubs. Each gets its own page-sized allocation, kept alive.
  function code(bytes) {
    const page = Process.pageSize;
    const p = __keepAlloc(page);
    Memory.protect(p, page, 'rwx');
    p.writeByteArray(bytes);
    return p;
  }
  const le = (v) => { const a = []; for (let i = 0; i < 4; i++) a.push((v >>> (8 * i)) & 0xff); return a; };
  const addrBytes = (p) => le(p.toUInt32());

  // ============================================================================ group D: Display construction
  // $blk (0x1400 bytes, patterned) holds the objects: `this` pointers at 0x10, 0x170, 0x2d0... (0x160 apart,
  // Display is 0x14c bytes). The module's command-handler pointer 0x10128728, which DisplayMember::ctor zeroes,
  // is written only with the first thread frozen: it is set to a sentinel so the zero store is visible, and the
  // freeze helper puts the game's value back before the resume.
  function displayFx() {
    const base = jgld();
    const blk = __keepAlloc(0x1400);
    fill(blk, 0x1400, 0x44d1);
    const out = { obj: blk, blk: blk, gcmd: base.add(0x128728) };
    for (let i = 0; i < 12; i++) out['D' + i] = blk.add(0x10 + i * 0x160).add(i & 3);
    settle();
    __freeze.thread();
    __freeze.setU32(out.gcmd, 0x5ac0de01);
    return out;
  }

  // ============================================================================ the Sprite draw dispatchers
  // Copied from c3dc (re/frida/js/fixtures.d/c3dc.js spriteObjects / spriteFreeze), itself c3ci's object set: fourteen surfaces on jgld's Surface
  // vtable 0x1011d0b0, two Palettes on 0x1011db10, Sprites on 0x1011d380 with plain 8-bit payloads in the middle
  // of 0x4000-byte slices, and the lookup tables the blitters are handed. Added here:
  //   - run-length row tables at +0x1c for the busy sprites (bit 0 of +0x18), the form the drawRle16* blitters
  //     read (re/match/jgld_blitY6..12.cpp: struct Row { BYTE start; BYTE len; WORD off; } per row, `off` a byte
  //     offset into the payload); four extra rows with len 0 follow each table, because the stepped body indexes
  //     the table with a 16.16 row counter;
  //   - a fifth payload slice of 16-bit words drawn from {0, 1, 2, 3, 0x00ff} for the 16-bit sprites: draw16t
  //     0x1001b060 indexes its table with (source << 16) | destination (c3bm), so only small words stay inside it.
  function spriteObjects() {
    if (globalThis.__c3dkSpr) return globalThis.__c3dkSpr;
    const base = jgld();
    const vtSurface = base.add(0x11d0b0), vtPalette = base.add(0x11db10), vtSprite = base.add(0x11d380);
    const mkPal = (a565, b565, a555, b555) => {
      const p = __keepAlloc(0x820);
      for (let o = 0; o < 0x820; o += 4) p.add(o).writeU32(0);
      p.writePointer(vtPalette);
      for (let i = 0; i < 256; i++) {
        p.add(0x40c + i * 2).writeU16((i * a565 + b565) & 0x1fff);
        p.add(0x60c + i * 2).writeU16((i * a555 + b555) & 0x1fff);
      }
      return p;
    };
    const pal = mkPal(0x0105, 0x0234, 0x0203, 0x0678);
    const pal2 = mkPal(0x0307, 0x0abc, 0x0409, 0x0ef0);
    const NSURF = 14, SLICE = 0x1000;
    const pix = __keepAlloc(NSURF * SLICE);
    //      depth fmt pitch bounds            clip              base
    const SPECS = [
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  0 d0
      [16, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true],   //  1 d1  clip inside
      [16, 1, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  2 d2  format 1
      [16, 2, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  3 d3  format 2
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false],  //  4 d4  no pixel base
      [16, 0, 24, [0, 0, 24, 12], [0, 0, 24, 12], true],   //  5 d5  stride 24
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  6 d6
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],    //  7 e0  8-bit
      [8, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true],    //  8 e1  8-bit, clip inside
      [8, 0, 24, [0, 0, 24, 12], [0, 0, 24, 12], true],    //  9 e2
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false],   // 10 e3  8-bit, no pixel base
      [24, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   // 11 x0  depth 24
      [12, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   // 12 x1  depth 12
      [0, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],    // 13 x2  depth 0
    ];
    const SNAMES = ['d0', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'e0', 'e1', 'e2', 'e3', 'x0', 'x1', 'x2'];
    for (let i = 0; i < NSURF; i++) {
      const b0 = i * SLICE;
      if (SPECS[i][0] === 16) {
        for (let k = 0; k < SLICE / 2; k++) pix.add(b0 + k * 2).writeU16((k * 4099 + i * 311 + 7) & 0x1fff);
      } else {
        for (let j = 0; j < SLICE; j++) pix.add(b0 + j).writeU8((j * 37 + i * 13 + 5) & 0xff);
      }
    }
    const fx = { obj: pix, pix: pix, pal: pal, pal2: pal2 };
    const surf = __keepAlloc(NSURF * 0x820);
    SPECS.forEach((sp, i) => {
      const s = surf.add(i * 0x820);
      for (let o = 0; o < 0x820; o += 4) s.add(o).writeU32(0);
      s.writePointer(vtSurface);
      s.add(0x24).writeS32(sp[0]);
      s.add(0x28).writeS32(sp[1]);
      s.add(0x40).writeS32(sp[2]);
      for (let j = 0; j < 4; j++) s.add(0x44 + j * 4).writeS32(sp[4][j]);
      for (let j = 0; j < 4; j++) s.add(0x54 + j * 4).writeS32(sp[3][j]);
      s.add(0x4c0).writePointer(sp[5] ? pix.add(i * SLICE) : ptr(0));
      fx[SNAMES[i]] = s;
      fx['l' + i] = s.add(0x4c0);
    });
    // payload slices 0..3: c3ci's alphabet (0xf8..0xff drive the blitters' special arms); slice 4: 16-bit words
    const NPAY = 6, PAY = 0x4000;
    const pay = __keepAlloc(NPAY * PAY);
    const ALPH = [0x00, 0x01, 0x05, 0x0f, 0x10, 0x3c, 0x7f, 0xa5, 0xf8, 0xf9, 0xfd, 0xfe, 0xff];
    const STEP = [5, 3, 7, 11];
    for (let i = 0; i < 4; i++) {
      for (let j = 0; j < PAY; j++) pay.add(i * PAY + j).writeU8(ALPH[(j * STEP[i] + i * 4) % 13]);
    }
    const W16 = [0, 1, 2, 3, 0xff, 2, 1];
    for (let k = 0; k < PAY / 2; k++) pay.add(4 * PAY + k * 2).writeU16(W16[(k * 3) % 7]);
    // slice 5: only 0xf8..0xff. Sprite::draw_1002ab90 (drawM's 8 bpp arm) indexes its table with
    // ((byte - 0xf8) & 0xff) << 15 | destination for EVERY byte below 0xfe (0x1002ae36..0x1002ae46 in
    // re/match/jgld_blit16d.cpp), so any lower byte reads megabytes past the table.
    for (let j = 0; j < PAY; j++) pay.add(5 * PAY + j).writeU8(0xf8 + ((j * 5) % 8));
    // run-length row tables
    const rows = __keepAlloc(0x400);
    const mkRows = (off, w, h, pitch) => {
      const t = rows.add(off);
      for (let r = 0; r < h + 4; r++) {
        const e = t.add(r * 4);
        if (r < h) {
          const st = (r * 3) % 4, ln = w - st - ((r * 5) % 3);
          e.writeU8(st); e.add(1).writeU8(ln); e.add(2).writeU16(r * pitch + st);
        } else {
          e.writeU32(0);
        }
      }
      return t;
    };
    //   palette, payload slice (-1 none), payload offset, m_18, m_20, pitch, width, height, rows (-1 none)
    const BSP = [
      [pal, 0, 0x1000, 0, 8, 16, 16, 8, -1],      //  0 bA     the main sprite
      [pal2, 1, 0x1000, 0, 8, 20, 20, 10, -1],    //  1 bB     wider, the other Palette
      [0, 2, 0x1000, 0, 8, 16, 16, 8, -1],        //  2 bNP    no Palette of its own: the client's
      [pal, 0, 0x1040, 0, 8, 4, 1, 1, -1],        //  3 bD     1 x 1
      [pal, -1, 0, 0, 8, 16, 16, 8, -1],          //  4 bNB    no payload: slot +0x20 returns 0
      [pal, 3, 0x1000, 0, 8, 16, 16, 8, -1],      //  5 bW
      [pal, 1, 0x1000, 1, 8, 16, 16, 8, 0x000],   //  6 bBsy   busy (run-length), 16 x 8
      [pal, 2, 0x1000, 0, 0x10, 16, 16, 8, -1],   //  7 b16    16 bpp, 8-bit alphabet slice
      [pal, 3, 0x1000, 0, 24, 16, 16, 8, -1],     //  8 bX     24 bpp: the outer default
      [pal2, 3, 0x1000, 3, 8, 20, 20, 10, 0x100], //  9 bBsy2  busy, 20 x 10, the other Palette, bit 1 too
      [pal, 4, 0x1000, 0, 0x10, 16, 16, 8, -1],   // 10 b16w   16 bpp, 16-bit word payload
      [pal, 4, 0x1000, 1, 0x10, 16, 16, 8, 0x200],// 11 b16Bsy 16 bpp, busy
      [0, 1, 0x1000, 1, 8, 16, 16, 8, 0x300],     // 12 bBnp   busy with no Palette: the client's
      [pal, 5, 0x1000, 0, 8, 16, 16, 8, -1],      // 13 bP     8 bpp, payload 0xf8..0xff only (drawM)
      [pal2, 5, 0x1000, 0, 8, 20, 20, 10, -1],    // 14 bP2    the same, 20 x 10, the other Palette
    ];
    const BSPN = ['bA', 'bB', 'bNP', 'bD', 'bNB', 'bW', 'bBsy', 'b16', 'bX', 'bBsy2', 'b16w', 'b16Bsy', 'bBnp', 'bP', 'bP2'];
    const spr = __keepAlloc(BSP.length * 0x80);
    BSP.forEach((s, i) => {
      const p = spr.add(i * 0x80);
      for (let o = 0; o < 0x80; o += 4) p.add(o).writeU32(0);
      p.writePointer(vtSprite);
      p.add(0x10).writePointer(s[0] ? s[0] : ptr(0));
      p.add(0x14).writePointer(s[1] < 0 ? ptr(0) : pay.add(s[1] * PAY + s[2]));
      p.add(0x18).writeS32(s[3]);
      p.add(0x1c).writePointer(s[8] < 0 ? ptr(0) : mkRows(s[8], s[6], s[7], s[5]));
      p.add(0x20).writeS32(s[4]);
      p.add(0x24).writeS32(0x40);
      p.add(0x28).writeS32(i === 10 || i === 11 ? 0xff : 0);
      p.add(0x2c).writeS32(s[5]);
      p.add(0x30).writeS32(s[6]);
      p.add(0x34).writeS32(s[7]);
      fx[BSPN[i]] = p;
    });
    fx.spr = spr;
    fx.spix = pay;
    fx.rows = rows;
    // lookup tables (c3ci): $t 16-bit entries masked to 0x1fff (0xe4100 bytes), $m a 256-byte index remap,
    // $b a 64 KB byte table, each with a second pointer 0x80 in
    const t = __keepAlloc(0xe4100);
    for (let i = 0; i < 0x72080; i++) t.add(i * 2).writeU16((i * 7919 + 13) & 0x1fff);
    const m = __keepAlloc(0x200);
    for (let i = 0; i < 0x200; i++) m.add(i).writeU8((i * 37 + 11) & 0xff);
    const b = __keepAlloc(0x10100);
    for (let i = 0; i < 0x10100; i++) b.add(i).writeU8((i * 97 + 29) & 0xff);
    fx.t = t; fx.t2 = t.add(0x80); fx.m = m; fx.m2 = m.add(0x80); fx.b = b; fx.b2 = b.add(0x80);
    const c8 = __keepAlloc(0x100);
    for (let i = 0; i < 0x100; i++) c8.add(i).writeU8((i * 151 + 71) & 0xff);
    fx.c8 = c8;
    const client = __keepAlloc(8);
    client.writeU32(0);
    client.add(4).writePointer(pal2);
    fx.client = client;
    // the module globals that are state regions (c3ci's list plus the run-length block)
    fx.gmask = base.add(0x128530); fx.gvstep = base.add(0x128540); fx.gtbl = base.add(0x128548);
    fx.gpos = base.add(0x128554); fx.glevel = base.add(0x128568); fx.gcols = base.add(0x128570);
    fx.gxstep = base.add(0x128580);
    fx.grle = base.add(0x1285a8);       // 0x101285a8..0x10128603, the drawRle16* block
    fx.g8vstep = base.add(0x128624); fx.g8tbl = base.add(0x12862c); fx.g8pos = base.add(0x12863c);
    fx.g8cols = base.add(0x128660); fx.g8colour = base.add(0x128664); fx.g8pix = base.add(0x12866c);
    fx.g8step = base.add(0x12867c); fx.g8xstep = base.add(0x128688);
    const rowPtr = base.add(0x122e20).readPointer();
    fx.growpair = rowPtr.isNull() ? base.add(0x12854c) : rowPtr.sub(4);
    const rowPtr8 = base.add(0x122e58).readPointer();
    fx.g8rowpair = rowPtr8.isNull() ? base.add(0x128634) : rowPtr8.sub(4);
    globalThis.__c3dkSpr = fx;
    return fx;
  }

  // c3ci's freeze wrapper on the shared helper: suspends the first thread, saves 0x101284d0..0x10128698 (every
  // loop global of the blitters reached here, the run-length block included), the scale globals 0x10122dc0..c8,
  // the palette client 0x1012873c, the colour-table slot 0x10128658, the two row-step pairs and jgld's
  // _CrtDbgMode[_CRT_WARN] (RVA 0x123378), then writes the scale, our client and 0 into the debug mode (c3ci: with
  // mode 2 the report's OutputDebugStringA raises DBG_PRINTEXCEPTION_C and Frida fails the rpc; with 0 the report
  // takes none of its output arms and returns 0, as in mode 2).
  function spriteFreeze(scale) {
    settle();
    const fx = spriteObjects();
    const base = jgld();
    const RANGES = [[base.add(0x1284d0), 0x1c8], [base.add(0x122dc0), 0xc], [base.add(0x12873c), 4],
                    [fx.growpair, 8], [fx.g8rowpair, 8], [base.add(0x123378), 4]];
    __freeze.thread();
    RANGES.forEach((r) => __freeze.save(r[0], r[1]));
    base.add(0x122dc0).writeS32(scale[0]);
    base.add(0x122dc4).writeS32(scale[1]);
    base.add(0x122dc8).writeS32(scale[2]);
    base.add(0x12873c).writePointer(fx.client);
    base.add(0x128658).writePointer(fx.c8);
    base.add(0x123378).writeS32(0);
    return fx;
  }

  // ============================================================================ shared: recording stubs
  const words = (p, vals) => vals.forEach(function (v, i) { p.add(4 * i).writeU32(v >>> 0); });
  const cstr = (s) => { const p = __keepAlloc(s.length + 1); p.writeUtf8String(s); return p; };
  // A vtable of n slots all pointing at `def`, with `slots` {offset: stub} overriding.
  function vtable(n, def, slots) {
    const vt = __keepAlloc(4 * n);
    for (let i = 0; i < n; i++) vt.add(4 * i).writePointer(def);
    Object.keys(slots || {}).forEach(function (o) { vt.add(Number(o)).writePointer(slots[o]); });
    return vt;
  }
  // c3dg's trap: an unexpected slot returns 0x0badc0de and counts the call at its own word (int3 would kill the game).
  function trapStub() {
    const ctr = __keepAlloc(4); ctr.writeU32(0);
    return code([0xFF, 0x05].concat(addrBytes(ctr), [0xB8], le(0x0badc0de), [0xC3]));
  }
  // __thiscall recorder with n stack arguments: appends {ecx, arg0..arg(n-1)} at log+4+count*4*(n+1), increments
  // the count at log+0, returns `ret` and pops 4n bytes. Hand-encoded:
  //   mov eax,[log]; imul eax,eax,stride; add eax,log+4; mov [eax],ecx;
  //   (mov edx,[esp+4+4i]; mov [eax+4+4i],edx) * n; inc dword [log]; mov eax,ret; ret 4n
  function recThis(log, n, ret) {
    const stride = 4 * (n + 1);
    let b = [0xA1].concat(addrBytes(log), [0x69, 0xC0], le(stride), [0x05], addrBytes(log.add(4)), [0x89, 0x08]);
    for (let i = 0; i < n; i++) b = b.concat([0x8B, 0x54, 0x24, 4 + 4 * i, 0x89, 0x50, 4 + 4 * i]);
    b = b.concat([0xFF, 0x05], addrBytes(log), [0xB8], le(ret));
    b = b.concat(n ? [0xC2, (4 * n) & 0xff, 0x00] : [0xC3]);
    return code(b);
  }
  // the bytes of a C string, packed four to a dword (for an import log), at most `max` dwords
  function strWords(p, max) {
    const out = [];
    if (p.isNull()) return [0];
    const s = p.readByteArray(4 * max);
    const u = new Uint8Array(s);
    // bytes after the terminator are zeroed: in a stack buffer they are whatever the frame held, which differs
    // between the two arms
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

  // ============================================================================ group W: Display::createWindow
  // Six Display-sized objects in $blk (0x160 apart), two titles, two mode objects whose vtable slot 0 is a recorder
  // of (this, w, h, bpp, flag) into $modelog. Every import is stood in (one call per key); RegisterClassA records the
  // ten WNDCLASSA words (the struct is a stack local: its address is not recorded) and returns `reg`;
  // CreateWindowExA records its twelve arguments and returns `hwnd`. The listener store into the module's event sink
  // 0x1012872c happens with the first thread frozen; the sink is a state region and the game's value goes back.
  function windowFx(reg, hwnd) {
    settle();
    const base = jgld();
    const blk = __keepAlloc(0x1000);
    fill(blk, 0x1000, 0x3c1d);
    const out = { obj: blk, blk: blk };
    for (let i = 0; i < 6; i++) out['W' + i] = blk.add(0x10 + i * 0x160);
    out.t0 = cstr('fixture title');
    out.t1 = cstr('T');
    const modelog = __keepAlloc(0x100);
    modelog.writeByteArray(new Array(0x100).fill(0));
    out.modelog = modelog;
    const trap = trapStub();
    const vtM = vtable(4, trap, { 0: recThis(modelog, 4, 0x77) });
    for (let i = 0; i < 2; i++) {
      const m = blk.add(0xe00 + i * 0x40);
      m.writePointer(vtM);
      out['M' + i] = m;
    }
    const s = __importStandIn({ module: 'jgld.dll', imports: {
      'KERNEL32.dll!GetModuleHandleA': { args: 1, ret: 0x00510000 },
      'USER32.dll!LoadIconA': { args: 2, ret: 0x00010021 },
      'GDI32.dll!GetStockObject': { args: 1, ret: 0x01900015 },
      'USER32.dll!RegisterClassA': { args: 1, impl: function (a, ctx) {
        const w = [];
        for (let i = 0; i < 10; i++) w.push(a[0].add(4 * i).readU32());
        ctx.record(w);
        return reg;
      } },
      'USER32.dll!GetSystemMetrics': { impl: function (a) {
        const i = a[0].toUInt32();
        return i === 0 ? 1024 : i === 1 ? 768 : 0x5a;
      } },
      'USER32.dll!CreateWindowExA': { args: 12, ret: hwnd },
      'USER32.dll!GetDC': { args: 1, ret: 0x0dc00001 },
      'GDI32.dll!SetBkMode': { args: 2, ret: 1 },
    } });
    out.implog = s.log;
    __freeze.thread();
    out.gsink = base.add(0x12872c);
    __freeze.save(out.gsink, 4);
    return out;
  }

  // ============================================================================ group A: allocDrawBuffer
  // Surfaces of our own on jgld's Surface vtable 0x1011d0b0 (0x820 bytes each, patterned; c3dc's layout), so the
  // body's slot +0x8 (Surface::release 0x10007cc0) and slot +0x34 (Surface::setClip 0x100083c0, which goes through
  // slots +0x28 / +0x2c and intersect / equal) are jgld's own. Every import the tree reaches is stood in:
  // CreateCompatibleDC (ret `dc`), CreateDIBSection (records the BITMAPINFOHEADER words the body wrote, stores our
  // pixel block through its ppvBits argument, ret `dib`), SelectObject / DeleteDC / DeleteObject / SetBkMode /
  // SetTextAlign / CreateRectRgnIndirect (records the rect) / SelectClipRgn, GetLastError, FormatMessageA (stack
  // buffer not recorded), MessageBoxA; IntersectRect and EqualRect run for real (pure) and are not recorded (their
  // outputs land in the surface, which is state). The module Display 0x10128420 is swapped, first thread frozen,
  // for one of ours whose slot +0xb0 records the call and returns `depth`. CreateDIBSection always succeeds here:
  // its failure arm ends in jgld's CRT _exit 0x1007f1a0, and a stand-in that returns cannot be used, because the
  // original never pops the `push 4` (0x10008006, no `add esp` after the call at 0x10008008), so its epilogue's
  // __chkesp (0x10008216) would report (a first try hung the game in that report, log/c3/c3dk_notes.txt).
  //   S0 32x16 depth 16, DC/DIB/old bitmap/clip region all set     S1 the same with all four handles 0
  //   S2 7x3 depth 8, DC set, region 0                              S3 0x0 depth 0, all handles set
  function drawBufFx(dc, dib, depth) {
    settle();
    const base = jgld();
    const vtSurface = base.add(0x11d0b0);
    const pix = __keepAlloc(0x400);
    fill(pix, 0x400, 0x0b17);
    const surf = __keepAlloc(4 * 0x820);
    const out = { obj: surf, surf: surf, bits: pix };
    const SPEC = [[32, 16, 16, 1], [32, 16, 16, 0], [7, 3, 8, 2], [0, 0, 0, 1]];
    SPEC.forEach(function (sp, i) {
      const s = surf.add(i * 0x820);
      fill(s, 0x820, 0x5a00 + i);
      s.writePointer(vtSurface);
      s.add(0x24).writeS32(sp[2]);
      s.add(0x38).writeS32(sp[0]);
      s.add(0x3c).writeS32(sp[1]);
      const hset = sp[3] === 1, dcOnly = sp[3] === 2;
      s.add(0x4ac).writeU32(hset ? 0x0e0000a0 + i : 0);
      s.add(0x4b0).writeU32(hset ? 0x0b0000b0 + i : 0);
      s.add(0x4b4).writeU32(hset ? 0x0d1b00c0 + i : 0);
      s.add(0x4bc).writeU32(hset || dcOnly ? 0x0dc000d0 + i : 0);
      s.add(0x4c4).writeU32(0); s.add(0x4c8).writeU32(0);
      out['S' + i] = s;
    });
    const displog = __keepAlloc(0x40);
    displog.writeByteArray(new Array(0x40).fill(0));
    out.displog = displog;
    const disp = __keepAlloc(0x40);
    disp.writePointer(vtable(0x40, trapStub(), { 0xb0: recThis(displog, 0, depth) }));
    const s = __importStandIn({ module: 'jgld.dll', logSize: 0x800, imports: {
      'GDI32.dll!CreateCompatibleDC': { args: 1, ret: dc },
      'GDI32.dll!CreateDIBSection': { args: 6, impl: function (a, ctx) {
        const h = a[1];
        ctx.record([a[0].toUInt32(), h.add(4).readU32(), h.add(8).readU32(), h.add(0xe).readU16(), h.add(0x20).readU32(),
                    a[2].toUInt32(), a[4].toUInt32(), a[5].toUInt32()]);
        if (dib) a[3].writePointer(pix);
        return dib;
      } },
      'GDI32.dll!SelectObject': { args: 2, impl: function (a) { return (0x0b100000 | (a[1].toUInt32() & 0xfff)) >>> 0; } },
      'GDI32.dll!DeleteDC': { args: 1, ret: 1 },
      'GDI32.dll!DeleteObject': { args: 1, ret: 1 },
      'GDI32.dll!SetBkMode': { args: 2, ret: 1 },
      'GDI32.dll!SetTextAlign': { args: 2, ret: 0 },
      'GDI32.dll!CreateRectRgnIndirect': { args: 1, impl: function (a, ctx) {
        ctx.record([0, 4, 8, 12].map(function (o) { return a[0].add(o).readU32(); }));
        return 0x0e0f0001;
      } },
      'GDI32.dll!SelectClipRgn': { args: 2, ret: 2 },
      'USER32.dll!IntersectRect': { args: 3, forward: true, record: false },
      'USER32.dll!EqualRect': { args: 2, forward: true, record: false },
      'KERNEL32.dll!GetLastError': { ret: 8 },
      'KERNEL32.dll!FormatMessageA': { args: 7, record: [0, 1, 2, 3, 5, 6], ret: 0 },
      'USER32.dll!MessageBoxA': { args: 4, ret: 1 },
    } });
    out.implog = s.log;
    __freeze.thread();
    const gd = base.add(0x128420);
    __freeze.save(gd, 4);
    gd.writePointer(disp);
    return out;
  }

  // ============================================================================ group F: Font::loadFile
  // Fonts of our own (0x40 bytes) on a vtable whose slot +0xc records (this) and slot +0x8 records (this, name, a, b)
  // and returns 0x55, both into $fontlog. jalloc / jfree end in jgld's malloc 0x1007fb40 / free 0x100806d0, served by
  // the heap stand-in (blocks from the compared arena, frees logged; failMask bit 0 makes the first allocation fail).
  // GetCurrentDirectoryA writes our own directory string into its stack buffer; CreateScalableFontResourceA and
  // AddFontResourceA record the string bytes they receive (the path buffer is a stack local: only its contents are
  // recorded) and return `csfr` / `add`; GetLastError returns `gle`. No module global is written: no freeze.
  function fontFx(csfr, gle, add, failMask) {
    const h = standIn(0x400, failMask ? { failMask: failMask } : null);
    const blk = __keepAlloc(0x400);
    fill(blk, 0x400, 0xf0f0);
    const out = { obj: blk, blk: blk, arena: h.arena, freelog: h.log };
    const fontlog = __keepAlloc(0x100);
    fontlog.writeByteArray(new Array(0x100).fill(0));
    out.fontlog = fontlog;
    const vt = vtable(8, trapStub(), { 0xc: recThis(fontlog.add(0x80), 0, 0), 0x8: recThis(fontlog, 3, 0x55) });
    for (let i = 0; i < 4; i++) {
      const f = blk.add(0x10 + i * 0x40);
      f.writePointer(vt);
      f.add(0x14).writeU32(0x100 * i + 2 * i);
      out['F' + i] = f;
    }
    const names = ['alpha.ttf', 'bb.fon', 'a-much-longer-font-file-name.ttf', 'abcd', 'x.ttf'];
    names.forEach(function (n, i) { out['f' + i] = blk.add(0x200 + i * 0x30); out['f' + i].writeUtf8String(n); });
    out.n0 = blk.add(0x320); out.n0.writeUtf8String('face');
    out.n1 = blk.add(0x330); out.n1.writeUtf8String('');
    const s = __importStandIn({ module: 'jgld.dll', logSize: 0x800, imports: {
      'KERNEL32.dll!GetCurrentDirectoryA': { args: 2, record: [0], impl: function (a) {
        a[1].writeUtf8String('C:\\fx\\dir');
        return 9;
      } },
      'GDI32.dll!CreateScalableFontResourceA': { args: 4, impl: function (a, ctx) {
        ctx.record([a[0].toUInt32(), a[1].toUInt32(), a[3].toUInt32()].concat(strWords(a[1], 16), strWords(a[2], 24)));
        return csfr;
      } },
      'GDI32.dll!AddFontResourceA': { args: 1, impl: function (a, ctx) {
        ctx.record([a[0].toUInt32()].concat(strWords(a[0], 16)));
        return add;
      } },
      'KERNEL32.dll!GetLastError': { ret: gle },
      'USER32.dll!PostMessageA': { args: 4 },
    } });
    out.implog = s.log;
    return out;
  }

  // ============================================================================ group P: png_set_pCAL
  // A png_struct of ours ($png: only tested for NULL and handed to png_malloc, which only tests it), four info
  // structs I0..I3 (0x100 bytes, patterned, so every field the body does not write stays visible), our own ASCII
  // strings and parameter arrays A0..A5 (n entries each). png_malloc ends in jgld's malloc 0x1007fb40, served by the
  // heap stand-in: every block comes from the compared arena. No module global is written: no freeze.
  function pcalFx() {
    const h = standIn(0x800);
    const blk = __keepAlloc(0x1000);
    fill(blk, 0x1000, 0x7ca1);
    const out = { obj: blk, blk: blk, arena: h.arena, freelog: h.log, png: blk };
    for (let i = 0; i < 4; i++) out['I' + i] = blk.add(0x200 + i * 0x100);
    const str = (off, s) => { blk.add(off).writeUtf8String(s); return blk.add(off); };
    out.sP = str(0x700, 'purpose-a'); out.sP2 = str(0x710, 'p'); out.sE = str(0x720, '');
    out.sU = str(0x730, 'units'); out.sU2 = str(0x740, 'a longer units string');
    const params = [str(0x760, 'one'), str(0x770, '2.5'), str(0x780, ''), str(0x790, '-17'), str(0x7a0, 'x')];
    for (let n = 0; n <= 5; n++) {
      const a = blk.add(0x800 + n * 0x20);
      for (let k = 0; k < n; k++) a.add(4 * k).writePointer(params[(k + n) % 5]);
      out['A' + n] = a;
    }
    return out;
  }

  // ============================================================================ group P: png_read_destroy
  // Six png_structs P0..P5 of ours (0x220 apart, patterned) whose pointer fields hold fake block addresses
  // 0x0f0000xx: png_free hands them to jgld's free 0x100806d0, which the heap stand-in only RECORDS, so the order
  // and identity of every release is compared state and nothing is dereferenced. Fields set per struct: the eight
  // single blocks, the flags word +0x6c, the gamma tables +0x170 / +0x174 / +0x178 (arrays of ours, eight entries) with
  // the shift +0x158 (6, 8, 9, 7, 5: counts 4, 1, none (1 << 31 is negative), 2, 8), +0x210, +0x1b0, the jmp_buf / error words left patterned, and the z_stream at +0x74 with state (+0x90)
  // NULL so inflateEnd 0x1009be70 returns Z_STREAM_ERROR without freeing. Info structs J0..J3 (0x100 bytes) with
  // a text array (num_text +0x30, text +0x38), pCAL purpose / units (+0xa0 / +0xac) and a pCAL parameter array
  // (+0xb0, count byte +0xb5) of fake blocks, for png_info_destroy 0x10078980.
  function readDestroyFx() {
    const h = standIn(0x100);
    const blk = __keepAlloc(0x2000);
    fill(blk, 0x2000, 0x2dd0);
    const out = { obj: blk, blk: blk, arena: h.arena, freelog: h.log };
    let fake = 0x0f000001;
    const nf = () => (fake++) >>> 0;
    const tbl = (off, n) => { const t = blk.add(off); for (let i = 0; i < n; i++) t.add(4 * i).writeU32(nf()); return t; };
    //       flags   shift tables(0x170,0x174,0x178)  singles
    const SPEC = [
      [0x7000, 6, [1, 1, 1], true],
      [0x0000, 6, [0, 0, 0], true],
      [0x1000, 8, [1, 0, 0], true],
      [0x6000, 9, [0, 1, 1], false],
      [0xffff, 7, [1, 1, 0], false],
      [0x0000, 5, [0, 0, 1], true],
    ];
    SPEC.forEach(function (sp, i) {
      const p = blk.add(i * 0x220);
      const single = [0xac, 0xec, 0xe8, 0x1ec, 0x1f0, 0x164, 0x168, 0x16c, 0x114, 0x188, 0x1f4, 0x210, 0x1b0];
      single.forEach(function (o) { p.add(o).writeU32(sp[3] ? nf() : 0); });
      p.add(0x6c).writeU32(sp[0]);
      p.add(0x158).writeU32(sp[1]);
      [0x170, 0x174, 0x178].forEach(function (o, k) {
        p.add(o).writePointer(sp[2][k] ? tbl(0x1400 + i * 0x80 + k * 0x28, 8) : ptr(0));
      });
      p.add(0x74 + 0x1c).writeU32(0);                // z_stream state NULL
      out['P' + i] = p;
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
      info.add(0xa0).writeU32(j ? nf() : 0);
      info.add(0xac).writeU32(j > 1 ? nf() : 0);
      if (j >= 2) {
        info.add(0xb0).writePointer(tbl(0x1900 + j * 0x20, 3));
        info.add(0xb5).writeU8(3);
      }
      out['J' + j] = info;
    }
    return out;
  }

  // ============================================================================ group P: png_read_start_row
  // Twenty-seven png_structs R0..R26 of ours (0x220 apart, patterned) with the fields the body and
  // png_init_read_transformations 0x100702a0 read. The transformation words use only INTERLACE 2, PACK 4, EXPAND
  // 0x1000, GRAY_TO_RGB 0x4000, FILLER 0x8000 and USER_TRANSFORM 0x100000: with none of BACKGROUND_EXPAND 0x100,
  // BACKGROUND 0x80, GAMMA 0x2000, RGB_TO_GRAY 0x600000 or SHIFT 8, png_init_read_transformations only copies the
  // background colour (pngrtran.c; its other arms test those bits). Both blocks come from png_malloc through the
  // heap stand-in (compared arena). No module global is written: no freeze.
  //   [interlaced, transformations, width, height, pass, pixel_depth, rowbytes, bit_depth, color_type, num_trans,
  //    user depth, user channels]
  const START_ROWS = [
    [0, 0x000000, 16, 8, 0, 8, 16, 8, 0, 0, 0, 0],
    [1, 0x000000, 17, 9, 0, 8, 17, 8, 0, 0, 0, 0],
    [1, 0x000002, 17, 9, 3, 24, 51, 8, 2, 0, 0, 0],
    [1, 0x000000, 33, 5, 6, 1, 5, 1, 0, 0, 0, 0],
    [1, 0x000000, 1, 1, 1, 4, 1, 4, 3, 0, 0, 0],
    [0, 0x000004, 20, 3, 0, 2, 5, 2, 0, 0, 0, 0],
    [0, 0x000004, 20, 3, 0, 8, 20, 8, 0, 0, 0, 0],
    [0, 0x001000, 10, 2, 0, 8, 10, 8, 3, 2, 0, 0],
    [0, 0x001000, 10, 2, 0, 4, 5, 4, 3, 0, 0, 0],
    [0, 0x001000, 10, 2, 0, 2, 3, 2, 0, 1, 0, 0],
    [0, 0x001000, 10, 2, 0, 16, 20, 16, 0, 0, 0, 0],
    [0, 0x001000, 10, 2, 0, 24, 30, 8, 2, 1, 0, 0],
    [0, 0x009000, 10, 2, 0, 24, 30, 8, 2, 0, 0, 0],
    [0, 0x008000, 10, 2, 0, 48, 60, 16, 2, 0, 0, 0],
    [0, 0x008000, 10, 2, 0, 8, 10, 8, 0, 0, 0, 0],
    [0, 0x008000, 10, 2, 0, 16, 20, 16, 0, 0, 0, 0],
    [0, 0x008000, 10, 2, 0, 8, 10, 8, 3, 0, 0, 0],
    [0, 0x005000, 10, 2, 0, 8, 10, 8, 0, 1, 0, 0],
    [0, 0x004000, 10, 2, 0, 16, 40, 8, 4, 0, 0, 0],
    [0, 0x104000, 7, 2, 0, 16, 14, 16, 0, 0, 16, 5],
    [0, 0x00c000, 10, 2, 0, 8, 10, 8, 0, 0, 0, 0],
    [0, 0x004000, 10, 2, 0, 16, 20, 16, 0, 0, 0, 0],
    [0, 0x004000, 10, 2, 0, 32, 40, 16, 4, 0, 0, 0],
    [0, 0x100000, 9, 2, 0, 24, 27, 8, 2, 0, 2, 3],
    [0, 0x001000, 10, 2, 0, 16, 20, 8, 4, 0, 0, 0],
    [0, 0x008000, 10, 2, 0, 16, 20, 8, 4, 0, 0, 0],
    [0, 0x004000, 10, 2, 0, 8, 10, 8, 0, 1, 0, 0],
  ];
  function startRowFx(extra) {
    const h = standIn(0x2000);
    const specs = START_ROWS.concat(extra || []);
    const blk = __keepAlloc(0x220 * specs.length);
    fill(blk, 0x220 * specs.length, 0x5717);
    const out = { obj: blk, blk: blk, arena: h.arena, freelog: h.log };
    specs.forEach(function (sp, i) {
      const p = blk.add(i * 0x220);
      p.add(0x123).writeU8(sp[0]);
      p.add(0x70).writeU32(sp[1]);
      p.add(0xc8).writeU32(sp[2]);
      p.add(0xcc).writeU32(sp[3]);
      p.add(0x124).writeU8(sp[4]);
      p.add(0x129).writeU8(sp[5]);
      p.add(0xd8).writeU32(sp[6]);
      p.add(0x127).writeU8(sp[7]);
      p.add(0x126).writeU8(sp[8]);
      p.add(0x11a).writeU16(sp[9]);
      p.add(0x64).writeU8(sp[10]);
      p.add(0x65).writeU8(sp[11]);
      out['R' + i] = p;
    });
    return out;
  }

  // ============================================================================ group P: png_build_gamma_table
  // Sixteen png_structs G0..G15 of ours (0x220 apart, patterned): gamma +0x15c and screen_gamma +0x160 (floats, a
  // NaN written as 0x7fc00000), transformations +0x70, color_type +0x126, bit_depth +0x127, sig_bit +0x17c..+0x17f.
  // Every table comes from png_malloc through the heap stand-in, so the tables are compared arena content. No module
  // global is written: no freeze. The fixture also reads the x87 control word of the A/B thread (__c3dkCw).
  //   [gamma, screen, transformations, color_type, bit_depth, sig r, g, b, gray]
  const GAMMAS = [
    [0.45455, 2.2, 0x000000, 2, 8, 0, 0, 0, 0],
    [0.5, 0.0, 0x000080, 0, 8, 0, 0, 0, 0],
    [1.0, 1.0, 0x200000, 0, 4, 0, 0, 0, 0],
    [0.0, 2.2, 0x000080, 0, 8, 0, 0, 0, 0],
    [NaN, 2.2, 0x000080, 0, 8, 0, 0, 0, 0],
    [2.2, 1e-7, 0x400000, 3, 1, 0, 0, 0, 0],
    [0.45455, 2.2, 0x000000, 2, 16, 8, 10, 6, 0],
    [0.7, 1.3, 0x000400, 0, 16, 0, 0, 0, 12],
    [0.3, 2.5, 0x000080, 0, 16, 0, 0, 0, 9],
    [1.0 / 3.0, 3.0, 0x000000, 6, 16, 16, 3, 5, 0],
    [1.7, 0.6, 0x600000, 0, 16, 0, 0, 0, 3],
    [0.9, 0.0, 0x000480, 0, 16, 0, 0, 0, 0],
    [0.45455, 2.2, 0x000400, 2, 16, 0, 0, 0, 0],
    [0.8, 1.9, 0x000000, 2, 16, 4, 5, 7, 0],
    [0.6, 1.1, 0x000400, 0, 16, 0, 0, 0, 10],
    [0.4, 2.1, 0x000000, 0, 16, 0, 0, 0, 20],
  ];
  function gammaFx() {
    const h = standIn(0x30000);
    const blk = __keepAlloc(0x220 * GAMMAS.length);
    fill(blk, 0x220 * GAMMAS.length, 0x6a77);
    const out = { obj: blk, blk: blk, arena: h.arena, freelog: h.log };
    GAMMAS.forEach(function (sp, i) {
      const p = blk.add(i * 0x220);
      if (sp[0] !== sp[0]) p.add(0x15c).writeU32(0x7fc00000); else p.add(0x15c).writeFloat(sp[0]);
      p.add(0x160).writeFloat(sp[1]);
      p.add(0x70).writeU32(sp[2]);
      p.add(0x126).writeU8(sp[3]);
      p.add(0x127).writeU8(sp[4]);
      [5, 6, 7, 8].forEach(function (k, j) { p.add(0x17c + j).writeU8(sp[k]); });
      out['G' + i] = p;
    });
    // the A/B thread's x87 control word (sub esp, 4; fnstcw [esp]; movzx eax, word [esp]; add esp, 4; ret), kept
    // in $cwbox (not a state region) for log/c3/alt/c3dk_cw.py
    const cwStub = code([0x83, 0xEC, 0x04, 0xD9, 0x3C, 0x24, 0x0F, 0xB7, 0x04, 0x24, 0x83, 0xC4, 0x04, 0xC3]);
    out.cwbox = __keepAlloc(4);
    out.cwbox.writeU32(new NativeFunction(cwStub, 'uint32', [])());
    globalThis.__c3dkCw = out.cwbox.readU32();
    return out;
  }

  // drawAlpha's NaN argument: a JS number in the fixture (vectors are JSON, which has no NaN)
  function sprFx(scale) {
    const fx = spriteFreeze(scale);
    fx.nan = NaN;
    return fx;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dk_display() { return displayFx(); },
    c3dk_spr_s111() { return sprFx([1, 1, 1]); },
    c3dk_spr_s223() { return sprFx([2, 2, 3]); },
    c3dk_win_ok() { return windowFx(0xc0de, 0x00ab0001); },
    c3dk_win_regfail() { return windowFx(0, 0x00ab0001); },
    c3dk_win_atom0() { return windowFx(0x10000, 0x00ab0001); },
    c3dk_win_cwfail() { return windowFx(0xc0de, 0); },
    c3dk_adb_d16() { return drawBufFx(0x0dc0f001, 0x0d1bf001, 16); },
    c3dk_adb_d0() { return drawBufFx(0x0dc0f002, 0x0d1bf002, 0); },
    c3dk_adb_dcfail() { return drawBufFx(0, 0x0d1bf003, 16); },
    c3dk_font_ok() { return fontFx(1, 0, 1, 0); },
    c3dk_font_csfr50() { return fontFx(0, 0x50, 1, 0); },
    c3dk_font_csfr0() { return fontFx(0, 0, 1, 0); },
    c3dk_font_csfr5() { return fontFx(0, 5, 1, 0); },
    c3dk_font_addfail() { return fontFx(1, 2, 0, 0); },
    c3dk_font_allocfail() { return fontFx(1, 0, 1, 1); },
    c3dk_pcal() { return pcalFx(); },
    c3dk_read_destroy() { return readDestroyFx(); },
    c3dk_start_row() { return startRowFx(); },
    c3dk_gamma() { return gammaFx(); },
  });
})();
