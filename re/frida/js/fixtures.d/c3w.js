// Fixtures for C3 batch c3w (jpeg_quality_scaling and directionOf need none; findByteInRange, swapTableEntry,
// sanitizeFileName, jcopy_sample_rows, expand_right_edge, jpeg_suppress_tables). Only our own synthetic values, never
// game text. Names are prefixed c3w_. diff_hook rewrites Memory.alloc to __keepAlloc so blocks referenced only through
// a pointer field are not freed mid-run.
(function () {
  function fillPattern(p, size, seed) {
    for (let i = 0; i < size; i++) p.add(i).writeU8((i * 13 + seed) & 0xff);
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // findByteInRange 0x004935f0: ten NUL-terminated strings; it returns the first byte in ['0'..'9'] (globals
    // 0x004bba88 / 0x004bba89, read-only). s0..s4 contain no digit (reach the NUL branch), s5..s10 reach a digit via
    // the jl (byte < '0': ' ', '/') and jg (byte > '9': letters, ':') continue arms. s2 is the empty string.
    c3w_bytes() {
      const texts = ['abc', 'XYZ', '', '   ', '/.-', '5', 'ab3cd', '  7', 'A9', ':5', '0'];
      const fx = {};
      for (let i = 0; i < texts.length; i++) fx['s' + i] = Memory.allocUtf8String(texts[i]);
      fx.obj = fx.s0;
      return fx;
    },

    // swapTableEntry 0x0045de30: seed six 0x100-byte golfer records at base 0x005794b8 and the 0x100-byte scratch at
    // 0x00582cb8 with distinct per-record patterns so a swap of any two records is observable (both are snapshotted
    // and restored by diff_hook).
    c3w_golfers() {
      const base = ptr('0x005794b8');
      for (let r = 0; r < 6; r++) {
        const rec = base.add(r * 0x100);
        for (let i = 0; i < 0x100; i++) rec.add(i).writeU8((r * 37 + i * 13) & 0xff);
      }
      const scratch = ptr('0x00582cb8');
      for (let i = 0; i < 0x100; i++) scratch.add(i).writeU8(0xaa);
      return { obj: base };
    },

    // sanitizeFileName 0x00405ac0: ten strings in one 0x200 buffer (restored by diff_hook). n0 "file" (valid, no
    // trailing space -> true unchanged), n1 "file   " (trailing spaces -> true trimmed), n2 "   " (all spaces ->
    // false trimmed to empty), n3 "" (empty -> false), n4 "bad:name" (':' -> false), n5 "a*b" ('*' -> false),
    // n6 "ok name" (inner space, no trailing -> true unchanged), n7 "end " (one trailing space -> true trimmed),
    // n8 "x" (true), n9 "no/slash" ('/' -> false).
    c3w_names() {
      const buf = Memory.alloc(0x200);
      for (let i = 0; i < 0x200; i += 4) buf.add(i).writeU32(0);
      const texts = ['file', 'file   ', '   ', '', 'bad:name', 'a*b', 'ok name', 'end ', 'x', 'no/slash'];
      const fx = { obj: buf, buf: buf };
      let off = 0;
      for (let i = 0; i < texts.length; i++) {
        buf.add(off).writeUtf8String(texts[i]);
        fx['n' + i] = buf.add(off);
        off += 0x20;  // 32-byte slot per string, all inside the 0x200 buffer
      }
      return fx;
    },

    // jcopy_sample_rows 0x004b04f0: a src row-pointer array (4 entries) over four pattern-filled 64-byte buffers, and
    // a dst row-pointer array (4 entries) over one contiguous 4*64 block `dbufs` (the snapshotted/restored state,
    // zeroed so a copy changes it). The function reads src+src_row*4 and dst+dst_row*4 row pointers.
    c3w_rows() {
      const sbufs = Memory.alloc(4 * 64);
      fillPattern(sbufs, 4 * 64, 101);
      const dbufs = Memory.alloc(4 * 64);
      for (let i = 0; i < 4 * 64; i += 4) dbufs.add(i).writeU32(0);
      const src = Memory.alloc(4 * 4);
      const dst = Memory.alloc(4 * 4);
      for (let i = 0; i < 4; i++) {
        src.add(i * 4).writePointer(sbufs.add(i * 64));
        dst.add(i * 4).writePointer(dbufs.add(i * 64));
      }
      return { obj: dbufs, src: src, dst: dst, dbufs: dbufs };
    },

    // expand_right_edge 0x004b4390: an image_data row-pointer array (4 entries) over one contiguous 4*64 block `bufs`
    // (the snapshotted/restored state), pattern-filled so row[input_cols-1] is defined and the pad is observable.
    c3w_edge() {
      const bufs = Memory.alloc(4 * 64);
      fillPattern(bufs, 4 * 64, 200);
      const img = Memory.alloc(4 * 4);
      for (let i = 0; i < 4; i++) img.add(i * 4).writePointer(bufs.add(i * 64));
      return { obj: bufs, img: img, bufs: bufs };
    },

    // jpeg_suppress_tables 0x004ae220: a cinfo with quant-table pointers at +0x40..+0x4c, DC huff at +0x50..+0x5c and
    // AC huff at +0x60..+0x6c. Six 0x120-byte tables in one block `tbls` (the snapshotted state); quant[0,1],
    // dc[0,1], ac[0,1] point at tbls 0..5, the other two of each four are NULL (exercising both the non-NULL write
    // and the skip arm). The flags written are quant +0x80 and huff +0x114, inside each 0x120 slot. Tables are
    // pattern-filled so the suppress write stands out.
    c3w_jpeg() {
      const cinfo = Memory.alloc(0x80);
      for (let i = 0; i < 0x80; i += 4) cinfo.add(i).writeU32(0);
      const tbls = Memory.alloc(6 * 0x120);
      fillPattern(tbls, 6 * 0x120, 50);
      const t = (i) => tbls.add(i * 0x120);
      cinfo.add(0x40).writePointer(t(0)); cinfo.add(0x44).writePointer(t(1));  // quant[0,1]; [2,3] NULL
      cinfo.add(0x50).writePointer(t(2)); cinfo.add(0x54).writePointer(t(3));  // dc[0,1]; [2,3] NULL
      cinfo.add(0x60).writePointer(t(4)); cinfo.add(0x64).writePointer(t(5));  // ac[0,1]; [2,3] NULL
      return { obj: tbls, cinfo: cinfo, tbls: tbls };
    },
  });
})();
