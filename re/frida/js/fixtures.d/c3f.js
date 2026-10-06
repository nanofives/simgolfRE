// Fixtures for C3 batch c3f. Each seeds the memory the function (and its original callees) read so every branch is
// reachable; only synthetic values are written (no game text). Names are prefixed c3f_. Allocations go through
// Memory.alloc (rewritten to __keepAlloc by diff_hook so they are never GC'd mid-run).
Object.assign(globalThis.DIFF_FIXTURES, {
  // Random::range 0x0045c1e0 calls Random::next (0x0045c1a0), which reads and rewrites the 4-byte seed at [this].
  // One private object with a fixed non-zero seed; the seed is the state region, restored before every vector, so
  // range(n) returns floor(next()*(n & 0xffff)) for the same starting seed and varies only with n.
  c3f_random() {
    const obj = Memory.alloc(8);
    obj.writeU32(0x12345678);          // seed
    return { obj: obj };
  },

  // Snd484 object for the field setters. Zero-filled, so the device pointer at +0x40 is NULL (the virtual device
  // notify is never taken) and the flag word +0x44 / mode +0x54 / fields +8, +0x38, +0x5c start at 0 (writes are
  // visible against 0). The same layout is produced fresh for each function (the fixture runs once per hook).
  c3f_snd() {
    const obj = Memory.alloc(0x100);
    for (let i = 0; i < 0x100; i += 4) obj.add(i).writeU32(0);
    return { obj: obj };
  },

  // Snd::flags 0x00484ff0 reads the pointer at +0x30 (tested, never dereferenced) and the low byte at +0x58, and
  // composes a status word from six bits of that byte plus bit 1 from +0x30. 16 objects whose +0x58 bytes together
  // light every bit (0x01,0x02,0x04,0x08,0x10,0x20) and whose +0x30 alternates null / non-null, so the returned
  // word varies across objects. +0x40 is NULL so the device virtual +0x70 is skipped.
  c3f_snd_flags() {
    const n = 16;
    const bits = [0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x3f, 0x15, 0x2a, 0x03, 0x0c, 0x30, 0x05, 0x28, 0x1f];
    const arena = Memory.alloc(0x80 * n);
    const out = { obj: arena };
    for (let i = 0; i < n; i++) {
      const o = arena.add(i * 0x80);
      for (let j = 0; j < 0x80; j += 4) o.add(j).writeU32(0);
      o.add(0x30).writePointer(i & 1 ? ptr(1) : ptr(0));   // tested for non-null only
      o.add(0x58).writeU8(bits[i]);
      out['s' + i] = o;
    }
    return out;
  },

  // MappedFile::ctor 0x00492d80 writes vtable 0x4bba78 at +0, 0 at +4, -1 at +8, 0 at +0xc. The 0x10-byte object is
  // filled with 0xaa first so each of the four writes is a visible state change (an omitted field would stay 0xaa).
  c3f_mappedfile() {
    const obj = Memory.alloc(0x10);
    for (let i = 0; i < 0x10; i++) obj.add(i).writeU8(0xaa);
    return { obj: obj };
  },

  // clearBuffers 0x0045b880 zeroes the 0x1002-byte store at 0x56fcb0 and fills the 0x100-byte table at 0x59d81c with
  // 0xff. Pre-fill both with other bytes so the memset is a visible change; the two regions are the state regions.
  c3f_clearbuffers() {
    const store = ptr('0x0056fcb0'), tbl = ptr('0x0059d81c');
    for (let i = 0; i < 0x1002; i++) store.add(i).writeU8(0x5a);
    for (let i = 0; i < 0x100; i++) tbl.add(i).writeU8(0x12);
    return { obj: store };
  },

  // appendToBuffer45b8b0 0x0045b8b0 manages the text store 0x56fcb0 with the offset table 0x59d81c (128 shorts, -1
  // means empty) and the length table 0x5a46b8 (128 shorts), appending the current text 0x51a068. Seed an empty
  // store, two live entries at slots 0x21/0x22 pointing at two NUL-terminated strings, and a short text to append.
  // Vectors (id = -1 / empty id / a live id) exercise the fresh-slot, explicit-slot and remove-then-append paths.
  c3f_appendbuffer() {
    const store = ptr('0x0056fcb0'), off = ptr('0x0059d81c'), len = ptr('0x005a46b8'), text = ptr('0x0051a068');
    for (let i = 0; i < 0x1002; i++) store.add(i).writeU8(0);
    for (let i = 0; i < 128; i++) { off.add(i * 2).writeS16(-1); len.add(i * 2).writeS16(0); }
    // store contents: "AAAA\0BB\0"
    const s = [0x41, 0x41, 0x41, 0x41, 0x00, 0x42, 0x42, 0x00];
    for (let i = 0; i < s.length; i++) store.add(i).writeU8(s[i]);
    off.add(0x21 * 2).writeS16(0); len.add(0x21 * 2).writeS16(5);   // "AAAA"
    off.add(0x22 * 2).writeS16(5); len.add(0x22 * 2).writeS16(3);   // "BB"
    // text to append: "HELLO\0"
    const t = [0x48, 0x45, 0x4c, 0x4c, 0x4f, 0x00];
    for (let i = 0; i < t.length; i++) text.add(i).writeU8(t[i]);
    return { obj: store };
  },
});
