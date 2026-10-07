// Fixtures for C3 batch c3p (Link482::data, C4837f0::ctor, Cache483060::detach, Surface_pixelPtr,
// Palette::decodeChunk, worldToScreen, screenToTile). Only our own synthetic values (no game text). Names are prefixed
// c3p_. Allocations go through __keepAlloc (diff_hook rewrites Memory.alloc) so blocks referenced only from a field
// are not freed mid-run; NativeCallbacks are pushed onto __diffKeepAlive by hand.
Object.assign(globalThis.DIFF_FIXTURES, {
  // Link482::data 0x00482e10 returns this+8 when the dword at this+4 is non-zero, else NULL. Twelve 0x10-byte objects
  // o0..o11 in one block (obj = the block, so a pointer return reads obj+0x10*i+8); +4 alternates zero / non-zero
  // values, including the sign bit and 1.
  c3p_link() {
    const b = Memory.alloc(0xc0);
    const f4 = [0, 1, 0, -1, 0x12345678, 0, 0x80000000 | 0, 0, 2, 0, 0x100, 0];
    const fx = { obj: b };
    for (let i = 0; i < 12; i++) {
      const o = b.add(i * 0x10);
      o.writeU32(0xa0a0a000 + i); o.add(4).writeS32(f4[i]); o.add(8).writeU32(0xb0b0b000 + i); o.add(12).writeU32(0);
      fx['o' + i] = o;
    }
    return fx;
  },

  // C4837f0::ctor 0x004837f0 writes 0 at +4 and the vtable 0x004ba468 at +0 and returns this. Twelve 0x10-byte slots
  // o0..o11 pre-filled with distinct garbage bytes (the whole 0xc0-byte block is the state region).
  c3p_ctor() {
    const b = Memory.alloc(0xc0);
    for (let i = 0; i < 0xc0; i++) b.add(i).writeU8((i * 37 + 11) & 0xff);
    const fx = { obj: b };
    for (let i = 0; i < 12; i++) fx['o' + i] = b.add(i * 0x10);
    return fx;
  },

  // Cache483060::detach 0x00483060 calls the virtual at vtable +0xc of the object at this+4 when it is non-null.
  // Twelve cache objects o0..o11 (0x10 bytes each, in $obj): +4 is null for o0, o2, o5, o8, o11 and one of four child
  // objects ch1..ch4 otherwise. Each child is {+0 vtbl, +4 id 1..4}; ONE fake vtable whose +0xc slot is a NativeCallback
  // (thiscall, no stack arguments) that bumps a call counter at rec+0, stores the child's id at rec+4 and folds it
  // into rec+8 (= rec+8 * 31 + id). rec (0x10 bytes) is the state region.
  c3p_detach() {
    const rec = Memory.alloc(0x10);
    for (let i = 0; i < 0x10; i += 4) rec.add(i).writeU32(0);
    const vt = Memory.alloc(0x20);
    for (let i = 0; i < 0x20; i += 4) vt.add(i).writeU32(0);
    const cb = new NativeCallback(function (self) {
      const id = self.add(4).readU32();
      rec.writeU32(rec.readU32() + 1);
      rec.add(4).writeU32(id);
      rec.add(8).writeU32((Math.imul(rec.add(8).readU32(), 31) + id) >>> 0);
      return 0x77;
    }, 'int', ['pointer'], 'thiscall');
    globalThis.__diffKeepAlive.push(cb);
    vt.add(0xc).writePointer(cb);
    const ch = [null];
    for (let k = 1; k <= 4; k++) {
      const c = Memory.alloc(8);
      c.writePointer(vt); c.add(4).writeU32(k);
      ch.push(c);
    }
    const which = [0, 1, 0, 2, 3, 0, 4, 1, 0, 2, 3, 0];
    const b = Memory.alloc(0xc0);
    const fx = { obj: b, rec: rec };
    for (let i = 0; i < 12; i++) {
      const o = b.add(i * 0x10);
      o.writeU32(0xc0c0c000 + i);
      o.add(4).writePointer(which[i] ? ch[which[i]] : ptr(0));
      o.add(8).writeU32(0); o.add(12).writeU32(0);
      fx['o' + i] = o;
    }
    return fx;
  },

  // Surface_pixelPtr 0x004796a0 reads the jgld.dll Surface at this+4 through its vtable. Each synthetic surface is a
  // 0x4d0-byte block whose +0 is the REAL Surface vtable (jgld.dll RVA 0x11d0b0, re/analysis/systems/jgld_2.md), so the
  // getters it calls are jgld's own: width = +0x5c - +0x54 and height = +0x60 - +0x58 (slots +0xd8 / +0xdc), stride =
  // +0x40 (slot +0xe0), depth pointer = this+0x24 (slot +0xe4), base = +0x4c0 copied to +0x4cc with the counter at
  // +0x4c8 incremented (slot +0x10, 0x10008730). No fake vtable. Every surface's base is `obj` (a 16-byte block, never
  // dereferenced), so pointer returns read obj+offset. Bounds {10,5,110,55} (100 x 50) except s16b {0,0,64,40}.
  // Depths 8/16/24/32 take the four case blocks; 12 and 20 hit the default slot of the index table; 7 and 40 fail the
  // `depth - 8 <= 0x18` test. snobits has base 0. Wrappers w<k> = {+0 0, +4 s<k>}; wnull has +4 = 0.
  c3p_surface() {
    const vt = Process.getModuleByName('jgld.dll').base.add(0x11d0b0);
    const bits = Memory.alloc(16);
    for (let i = 0; i < 16; i += 4) bits.add(i).writeU32(0);
    const fx = { obj: bits };
    const mk = (key, depth, stride, rect, base) => {
      const s = Memory.alloc(0x4d0);
      for (let i = 0; i < 0x4d0; i += 4) s.add(i).writeU32(0);
      s.writePointer(vt);
      s.add(0x24).writeS32(depth);
      s.add(0x40).writeS32(stride);
      for (let i = 0; i < 4; i++) s.add(0x54 + i * 4).writeS32(rect[i]);
      s.add(0x4c0).writePointer(base);
      const w = Memory.alloc(8);
      w.writeU32(0); w.add(4).writePointer(s);
      fx['s' + key] = s; fx['w' + key] = w;
    };
    const R = [10, 5, 110, 55];
    mk('8', 8, 0x80, R, bits); mk('16', 16, 0x50, R, bits); mk('24', 24, 0x40, R, bits); mk('32', 32, 0x30, R, bits);
    mk('16b', 16, 0x44, [0, 0, 64, 40], bits);
    mk('12', 12, 0x20, R, bits); mk('20', 20, 0x20, R, bits); mk('7', 7, 0x20, R, bits); mk('40', 40, 0x20, R, bits);
    mk('nobits', 8, 0x80, R, ptr(0));
    const wn = Memory.alloc(8);
    wn.writeU32(0); wn.add(4).writePointer(ptr(0));
    fx.wnull = wn;
    return fx;
  },

  // Palette::decodeChunk 0x004826f0. this+0x74 = P; P = {+0 vtblP, +4 palette}. TWO fake vtables:
  //   vtblP +0 (create, thiscall P): stores pal2 at P+4, bumps rec+8.
  //   vtblPal +0x10 (get, thiscall (pal, buf, start, count)): fills buf[0..0x300) from the fixed source pattern
  //     (i*5 + 1) & 0xff, bumps rec+0, stores the palette's id at rec+0x31c.
  //   vtblPal +0x14 (set, same arguments): copies buf[0..0x300) to rec+0x10, bumps rec+4, stores start/count at
  //     rec+0x310 / +0x314 and the palette's id at rec+0x318.
  // pal (id 1) and pal2 (id 2) are {+0 vtblPal, +4 id}. t: P has pal; tn: Pn has +4 = 0 (the create path); tz: this+0x74
  // = 0. State: rec (0x320) and the +4 fields of P / Pn. Chunks (6-byte header with type word 4 at +4, packet count
  // word at +6, packets from +8 as {skip, count, count * RGB}):
  //   c_empty 0 packets; c_one {0,1}; c_skip {10,3}; c_wrap {250,10} (index wraps past 255); c_multi {2,2} {5,1} {0,4};
  //   c_full {0,0} = 256 entries; c_full2 {0,0} then {7,2}; c_zero_skip {0,255} then {1,1} (index 255 + 1 wraps to 0).
  // tn is only given full chunks: without a palette the original never fills its local array first, so every entry
  // must be written for the set call to see defined bytes.
  c3p_pal() {
    const rec = Memory.alloc(0x320);
    for (let i = 0; i < 0x320; i += 4) rec.add(i).writeU32(0);
    const vtP = Memory.alloc(0x20), vtPal = Memory.alloc(0x20);
    for (let i = 0; i < 0x20; i += 4) { vtP.add(i).writeU32(0); vtPal.add(i).writeU32(0); }
    const pal = Memory.alloc(8), pal2 = Memory.alloc(8);
    pal.writePointer(vtPal); pal.add(4).writeU32(1);
    pal2.writePointer(vtPal); pal2.add(4).writeU32(2);
    const inc = (off) => rec.add(off).writeU32(rec.add(off).readU32() + 1);
    const getCb = new NativeCallback(function (self, buf, start, count) {
      for (let i = 0; i < 0x300; i++) buf.add(i).writeU8((i * 5 + 1) & 0xff);
      inc(0);
      rec.add(0x31c).writeU32(self.add(4).readU32());
      return 0;
    }, 'int', ['pointer', 'pointer', 'int', 'int'], 'thiscall');
    const setCb = new NativeCallback(function (self, buf, start, count) {
      rec.add(0x10).writeByteArray(buf.readByteArray(0x300));
      inc(4);
      rec.add(0x310).writeS32(start); rec.add(0x314).writeS32(count);
      rec.add(0x318).writeU32(self.add(4).readU32());
      return 0;
    }, 'int', ['pointer', 'pointer', 'int', 'int'], 'thiscall');
    const createCb = new NativeCallback(function (self) {
      self.add(4).writePointer(pal2);
      inc(8);
      return 0;
    }, 'int', ['pointer'], 'thiscall');
    globalThis.__diffKeepAlive.push(getCb, setCb, createCb);
    vtPal.add(0x10).writePointer(getCb); vtPal.add(0x14).writePointer(setCb);
    vtP.writePointer(createCb);
    const P = Memory.alloc(8), Pn = Memory.alloc(8);
    P.writePointer(vtP); P.add(4).writePointer(pal);
    Pn.writePointer(vtP); Pn.add(4).writePointer(ptr(0));
    const mkThis = (p) => { const t = Memory.alloc(0x78); for (let i = 0; i < 0x78; i += 4) t.add(i).writeU32(0);
                            t.add(0x74).writePointer(p); return t; };
    const chunk = (packets) => {
      let body = [];
      for (const [skip, count] of packets) {
        body.push(skip, count);
        const n = count === 0 ? 256 : count;
        for (let j = 0; j < n * 3; j++) body.push((skip * 7 + count * 3 + j * 11 + 0x40) & 0xff);
      }
      const c = Memory.alloc(8 + body.length + 4);
      c.writeU32(0); c.add(4).writeU16(4); c.add(6).writeU16(packets.length);
      for (let i = 0; i < body.length; i++) c.add(8 + i).writeU8(body[i]);
      return c;
    };
    return {
      obj: rec, rec: rec, P: P, Pn: Pn, t: mkThis(P), tn: mkThis(Pn), tz: mkThis(ptr(0)),
      c_empty: chunk([]), c_one: chunk([[0, 1]]), c_skip: chunk([[10, 3]]), c_wrap: chunk([[250, 10]]),
      c_multi: chunk([[2, 2], [5, 1], [0, 4]]), c_full: chunk([[0, 0]]), c_full2: chunk([[0, 0], [7, 2]]),
      c_zero_skip: chunk([[0, 255], [1, 1]]),
    };
  },

  // worldToScreen 0x0042fb90 / screenToTile 0x00430020 view. terrain_slopes (tile types (7x + 3y) & 0x1f, type flags
  // dwords 0x0057837c + t*0x30 = [8, 2, 4, 0, 0, 1][t % 6], corner bytes 0x0051b770 + cell*8 + k = (3x + 5y + 3k) % 7
  // + 1), then corner bytes k = 1, 3, 5, 7 of cell (x, y) rewritten by p = (3x + y) % 4 with b = 1 + x % 5: p 0 all b;
  // p 1 c5 = c7 = b, c1 = b + 1, c3 = b + 2; p 2 c5 = c7 = c1 = b, c3 = b + 1; p 3 unchanged (all four differ). Height
  // bytes 0x00543018 + cell = (cell*7) % 13 - 4. kind 'stt' also sets the flags of types t % 6 == 1 to 8 (no
  // cornerRange heights, so every projection of the screenToTile climb is a function of these tables only).
  // Globals: camera tile 0x004c2ba0 / 0x004c2ba4 = 25 / 25, zoom 0x004c2844 = 4, divisor 0x004c2840 = 16, view
  // 0x00822c8c = W and 0x00822c90 = 600, rotation 0x005685f4, flag 0x005a9cc0 = dbl, height scale 0x004c2e00 = 5.
  _c3p_view(rot, W, dbl, kind) {
    globalThis.DIFF_FIXTURES.terrain_slopes();
    const c = ptr('0x0051b770');
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++) {
        const p = (x * 3 + y) % 4, b = 1 + x % 5, o = c.add((x * 50 + y) * 8);
        if (p === 0) { o.add(1).writeS8(b); o.add(3).writeS8(b); o.add(5).writeS8(b); o.add(7).writeS8(b); }
        if (p === 1) { o.add(1).writeS8(b + 1); o.add(3).writeS8(b + 2); o.add(5).writeS8(b); o.add(7).writeS8(b); }
        if (p === 2) { o.add(1).writeS8(b); o.add(3).writeS8(b + 1); o.add(5).writeS8(b); o.add(7).writeS8(b); }
      }
    for (let i = 0; i < 2500; i++) ptr('0x00543018').add(i).writeS8((i * 7) % 13 - 4);
    if (kind === 'stt')
      for (let t = 0; t < 32; t++) if (t % 6 === 1) ptr('0x0057837c').add(t * 0x30).writeU32(8);
    ptr('0x004c2ba0').writeS32(25); ptr('0x004c2ba4').writeS32(25);
    ptr('0x004c2844').writeS32(4); ptr('0x004c2840').writeS32(16);
    ptr('0x00822c8c').writeS32(W); ptr('0x00822c90').writeS32(600);
    ptr('0x005685f4').writeS32(rot); ptr('0x005a9cc0').writeS32(dbl); ptr('0x004c2e00').writeS32(5);
  },
  _c3p_w2s(rot, W, dbl) {
    globalThis.DIFF_FIXTURES._c3p_view(rot, W, dbl, 'w2s');
    const cell = (v) => { const p = Memory.alloc(4); p.writeS32(v); return p; };
    if (rot === 1) {
      // no projection block runs: the bounds tests read the caller's *sx / *sy
      const pre = { in: [100, 100], xl: [-500, 100], xh: [2000, 100], yl: [100, -500], yh: [100, 2000] };
      const fx = {};
      for (const k in pre) { fx['sx_' + k] = cell(pre[k][0]); fx['sy_' + k] = cell(pre[k][1]); }
      fx.obj = fx.sx_in;
      return fx;
    }
    const sx = cell(0), sy = cell(0);
    return { obj: sx, sx: sx, sy: sy };
  },
  c3p_w2s_r0() { return globalThis.DIFF_FIXTURES._c3p_w2s(0, 800, 0); },
  c3p_w2s_r2() { return globalThis.DIFF_FIXTURES._c3p_w2s(2, 800, 0); },
  c3p_w2s_r4() { return globalThis.DIFF_FIXTURES._c3p_w2s(4, 800, 0); },
  c3p_w2s_r6() { return globalThis.DIFF_FIXTURES._c3p_w2s(6, 800, 0); },
  c3p_w2s_r0_w400() { return globalThis.DIFF_FIXTURES._c3p_w2s(0, 0x400, 0); },
  c3p_w2s_r0_w500() { return globalThis.DIFF_FIXTURES._c3p_w2s(0, 0x500, 0); },
  c3p_w2s_r0_dbl() { return globalThis.DIFF_FIXTURES._c3p_w2s(0, 800, 1); },
  c3p_w2s_r1() { return globalThis.DIFF_FIXTURES._c3p_w2s(1, 800, 0); },

  // screenToTile: the view above (kind 'stt'), the tile caches g_tileSX 0x0055eb40 / g_tileSY 0x0055fec8 zeroed (every
  // tile uncached, so tileToScreen 0x0042f940 projects through worldToScreen), and the two out cells.
  _c3p_stt(rot, dbl) {
    globalThis.DIFF_FIXTURES._c3p_view(rot, 800, dbl, 'stt');
    for (let i = 0; i < 5000; i += 4) { ptr('0x0055eb40').add(i).writeU32(0); ptr('0x0055fec8').add(i).writeU32(0); }
    const tx = Memory.alloc(4), ty = Memory.alloc(4);
    tx.writeS32(0); ty.writeS32(0);
    return { obj: tx, tx: tx, ty: ty };
  },
  c3p_stt_r0() { return globalThis.DIFF_FIXTURES._c3p_stt(0, 0); },
  c3p_stt_r2() { return globalThis.DIFF_FIXTURES._c3p_stt(2, 0); },
  c3p_stt_r4() { return globalThis.DIFF_FIXTURES._c3p_stt(4, 0); },
  c3p_stt_r6() { return globalThis.DIFF_FIXTURES._c3p_stt(6, 0); },
  c3p_stt_r0_dbl() { return globalThis.DIFF_FIXTURES._c3p_stt(0, 1); },
});
