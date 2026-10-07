// Fixtures for C3 batch c3x (flagIsSet, flagSet, flagToggle49ee90, Widget_setQuad70/74/78, polyEdgeStep,
// Palette::nearest). Only our own synthetic values (no game text). Names are prefixed c3x_. Allocations go through
// Memory.alloc (diff_hook rewrites it to __keepAlloc so field-only references are not freed); NativeCallbacks are
// pushed onto __diffKeepAlive by hand.
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function zero(p, size) { for (let i = 0; i < size; i += 4) p.add(i).writeU32(0); }

  // Flag object: [this] points at a vbtable whose dword at +8 is the offset of the data base from this; we set it to 0
  // so the data base is this itself. The flag list lives at this+0xc0 (m_head +0xc8, m_cur +0xcc, m_count +0xd0,
  // m_idx +0xd4) and the 32-bit mask at this+0xf0. A node is 0x10 bytes: id at +4, next pointer at +0xc.
  function flagObj(ids, count, mask, idx) {
    const obj = Memory.alloc(0x100);
    zero(obj, 0x100);
    const vbt = Memory.alloc(0x10);
    zero(vbt, 0x10);
    vbt.add(8).writeS32(0);                 // vbtable+8 = data base offset = 0
    obj.writePointer(vbt);                  // [this] = vbtable
    obj.add(0xd4).writeS32(idx);            // m_idx preset (used only when the list is empty)
    obj.add(0xf0).writeU32(mask >>> 0);     // mask
    if (ids && ids.length) {
      let head = null, prev = null;
      for (let k = 0; k < ids.length; k++) {
        const n = Memory.alloc(0x10);
        zero(n, 0x10);
        n.add(4).writeS32(ids[k]);          // node id
        if (prev) prev.add(0xc).writePointer(n); else head = n;
        prev = n;
      }
      obj.add(0xc8).writePointer(head);     // m_head
      obj.add(0xcc).writePointer(head);     // m_cur
    }
    obj.add(0xd0).writeS32(count);          // m_count
    return { obj: obj };
  }

  // Palette object: m_pal at +4 (points at an object whose vtable slot +0x10 is getEntries(out, first, count)); five
  // 0x10-byte range records at +8 (id +0, start byte +8, count byte +9). The getEntries callback fills count entries
  // of 3 bytes (r = i*5, g = i*7, b = i*11, all & 0xff) into out, so distances - and the nearest index - vary with the
  // query. ranges=true marks indices 0x10..0x2f as claimed by one active range (id 1); the other four are inactive
  // (id -1). nullPal=true leaves m_pal null (nearest returns 7).
  function palObj(ranges, nullPal) {
    const obj = Memory.alloc(0x60);
    zero(obj, 0x60);
    if (!nullPal) {
      const vt = Memory.alloc(0x20);
      zero(vt, 0x20);
      vt.add(0x10).writePointer(keep(new NativeCallback(function (self, out, first, count) {
        for (let i = 0; i < count; i++) {
          out.add(i * 3 + 0).writeU8((i * 5) & 0xff);
          out.add(i * 3 + 1).writeU8((i * 7) & 0xff);
          out.add(i * 3 + 2).writeU8((i * 11) & 0xff);
        }
      }, 'void', ['pointer', 'pointer', 'int', 'int'], 'thiscall')));
      const pal = Memory.alloc(8);
      pal.writePointer(vt);
      obj.add(4).writePointer(pal);
    }
    for (let i = 0; i < 5; i++) obj.add(8 + i * 0x10).writeS32(-1);  // every range inactive by default
    if (ranges) {
      obj.add(8).writeS32(1);               // range 0 active (id 1)
      obj.add(8 + 8).writeU8(0x10);         // start
      obj.add(8 + 9).writeU8(0x20);         // count -> marks 0x10..0x2f
    }
    return { obj: obj };
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // Populated list (ids 10, 20, 30 at indices 0, 1, 2; count 3), mask 0b1101 (bits 0, 2, 3 set, bit 1 clear).
    c3x_flags_pop() { return flagObj([10, 20, 30], 3, 0xd, 0); },
    // Empty list (m_head null): isSet/set leave m_idx at its preset (2) and read/update bit 2 of mask 0b100.
    c3x_flags_empty() { return flagObj(null, 0, 0x4, 2); },
    // Non-empty head but m_count 0: the loop is skipped after m_idx is reset to 0 and m_cur set to the head.
    c3x_flags_zcount() { return flagObj([10, 20, 30], 0, 0x1, 0); },

    // Widget quad setters: one object; each function writes four dwords starting at +0x70/+0x74/+0x78 (stride 0x10).
    c3x_widget() { const obj = Memory.alloc(0x100); zero(obj, 0x100); return { obj: obj }; },

    // polyEdgeStep: vertex table at 0x0083d358 (x, y dword pairs), vertex count 0x0083d378 = 6, end vertex index
    // 0x0083d350 = 3. Vertices v0 (10,0) v1 (30,5) v2 (25,5) v3 (5,20) v4 (40,20) v5 (8,2). Twelve 0x24-byte edge
    // records e0..e11 in one block; only edge+0 (dir) is seeded, the DDA fields are written by the function.
    c3x_poly() {
      const verts = Memory.alloc(6 * 8);
      const V = [[10, 0], [30, 5], [25, 5], [5, 20], [40, 20], [8, 2]];
      for (let i = 0; i < 6; i++) { verts.add(i * 8).writeS32(V[i][0]); verts.add(i * 8 + 4).writeS32(V[i][1]); }
      ptr('0x0083d358').writePointer(verts);
      ptr('0x0083d378').writeS32(6);
      ptr('0x0083d350').writeS32(3);
      const dirs = [1, 1, -1, 1, 1, 1, 1, -1, 1, 1, -1, 1];
      const block = Memory.alloc(dirs.length * 0x24);
      zero(block, dirs.length * 0x24);
      const fx = { obj: block, edges: block };
      for (let i = 0; i < dirs.length; i++) {
        const e = block.add(i * 0x24);
        e.writeS32(dirs[i]);
        fx['e' + i] = e;
      }
      return fx;
    },

    c3x_pal_r0() { return palObj(false, false); },
    c3x_pal_r1() { return palObj(true, false); },
    c3x_pal_null() { return palObj(false, true); },
  });
})();
