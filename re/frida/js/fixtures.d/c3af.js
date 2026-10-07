// Fixtures for C3 batch c3af (revisit). Only our own synthetic values (no game text). Names are prefixed c3af_.
// Allocations go through Memory.alloc (diff_hook rewrites it to __keepAlloc so field-only references survive);
// NativeCallbacks are pushed onto __diffKeepAlive by hand.
//
// The libjpeg helpers read a fake compress/common struct: cinfo+0 = err, cinfo+4 = mem, cinfo+0x14 = dest. The
// memory manager's first slot mem+0 is alloc_small(cinfo, pool, size); we point it at one shared NativeCallback that
// returns a per-cinfo buffer stashed at mem+8, so both A/B arms get the SAME pointer (a fresh Memory.alloc per call
// would differ between arms and read RED). A destination manager is {next_output_byte+0, free_in_buffer+4,
// init_destination+8, empty_output_buffer+0xc, term_destination+0x10}.
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function zero(p, size) { for (let i = 0; i < size; i += 4) p.add(i).writeU32(0); }

  // Shared alloc_small: cinfo->mem->[+8] holds the buffer this cinfo hands out. __cdecl.
  const ALLOC_CB = keep(new NativeCallback(function (cinfo, pool, size) {
    return cinfo.add(4).readPointer().add(8).readPointer();
  }, 'pointer', ['pointer', 'int', 'int']));
  // Shared empty_output_buffer stub: reports success so emit_byte's free==0 path returns without ERREXIT. __cdecl.
  const EMPTY_CB = keep(new NativeCallback(function (cinfo) { return 1; }, 'int', ['pointer']));

  // One cinfo of `csize` bytes whose mem hands out `buf`. Optional method at cinfo+0xbc.
  function mkCinfo(csize, buf, method) {
    const c = Memory.alloc(csize); zero(c, csize);
    const mem = Memory.alloc(0x10); zero(mem, 0x10);
    mem.writePointer(ALLOC_CB);         // mem+0 = alloc_small
    mem.add(8).writePointer(buf);       // mem+8 = buffer to hand out
    c.add(4).writePointer(mem);         // cinfo+4 = mem
    if (method !== undefined) c.add(0xbc).writeS32(method);
    return c;
  }

  // A destination manager with a buffer; freeInBuf large (no callback) unless `full` sets it to 1.
  function mkDest(full) {
    const buf = Memory.alloc(0x40); zero(buf, 0x40);
    const dest = Memory.alloc(0x14); zero(dest, 0x14);
    dest.writePointer(buf);                                   // dest+0 = next_output_byte
    dest.add(4).writeU32(full ? 1 : 0x100);                  // dest+4 = free_in_buffer
    dest.add(0xc).writePointer(EMPTY_CB);                    // dest+0xc = empty_output_buffer
    const cinfo = Memory.alloc(0x20); zero(cinfo, 0x20);
    const err = Memory.alloc(0x20); zero(err, 0x20);
    cinfo.writePointer(err);                                 // cinfo+0 = err (ERREXIT target, not hit)
    cinfo.add(0x14).writePointer(dest);                     // cinfo+0x14 = dest
    return { obj: buf, buf: buf, dest: dest, cinfo: cinfo };
  }

  // N cinfos handing out N distinct buffers of bufsize; each buffer is pre-filled 0xAA (so clearing sent_table is an
  // observable state change) with a distinct marker dword at +0.
  function mkTables(n, bufsize, methodCycle) {
    const fx = {};
    for (let i = 0; i < n; i++) {
      const buf = Memory.alloc(bufsize);
      for (let j = 0; j < bufsize; j += 4) buf.add(j).writeU32(0xaaaaaaaa);
      buf.writeU32(0x1000 + i);                               // marker the function never overwrites (quant/huff)
      const method = methodCycle ? (i % 3) : undefined;
      const c = mkCinfo(0x180, buf, method);
      fx['buf' + i] = buf;
      fx['c' + i] = c;
      if (i === 0) fx.obj = buf;
    }
    return fx;
  }

  // jpeg_stdio_dest with a pre-existing (non-null) dest: the fn only rewrites dest fields, varying outfile at dest+0x14.
  function mkStdioPreset() {
    const dest = Memory.alloc(0x1c); zero(dest, 0x1c);
    const cinfo = Memory.alloc(0x20); zero(cinfo, 0x20);
    cinfo.add(0x14).writePointer(dest);                      // cinfo+0x14 = dest (non-null -> no alloc)
    return { obj: dest, dest: dest, cinfo: cinfo };
  }

  // jpeg_stdio_dest with dest == NULL: the fn allocs 0x1c via alloc_small, stores it at cinfo+0x14, writes fields.
  function mkStdioAlloc() {
    const buf = Memory.alloc(0x1c); zero(buf, 0x1c);
    const cinfo = mkCinfo(0x20, buf);                        // cinfo+0x14 stays 0 -> alloc branch
    return { obj: buf, buf: buf, cinfo: cinfo };
  }

  // spawnWalker: seed the walker table (base 0x00585850, stride 0x4c, occupied byte +0x12) and the globals it reads.
  // `occupied` = how many leading slots are already taken, so the first free slot (the return value) is `occupied`.
  function seedWalker(occupied) {
    const base = ptr('0x00585850');
    for (let i = 0; i < 0x130; i += 4) base.add(i).writeU32(0);   // clear slots 0..3
    for (let s = 0; s < occupied; s++) base.add(s * 0x4c + 0x12).writeU8(1);  // mark leading slots taken
    ptr('0x0058bcba').writeS16(5);   // placed-object-0 x read at rec+0
    ptr('0x0058bcbc').writeS16(7);   // placed-object-0 y read at rec+4
    ptr('0x00575cb9').writeU8(3);    // direction base byte
    return {};
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3af_dest() { return mkDest(false); },
    c3af_dest_full() { return mkDest(true); },
    c3af_quant() { return mkTables(10, 0x84, false); },
    c3af_huff() { return mkTables(10, 0x118, false); },
    c3af_fdct() { return mkTables(12, 0x30, true); },
    c3af_stdio_preset() { return mkStdioPreset(); },
    c3af_stdio_alloc() { return mkStdioAlloc(); },
    c3af_walker0() { return seedWalker(0); },
    c3af_walker3() { return seedWalker(3); },
  });
})();
