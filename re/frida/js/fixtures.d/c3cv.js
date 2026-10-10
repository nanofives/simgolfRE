// Fixtures for C3 batch c3cv (round 23, golf_clean.exe): the rest of IJG libjpeg 6a's compress side (main,
// coefficient and preprocessing controllers, progressive Huffman encoder). Only our own synthetic values (no game
// text, no live game object). Names are prefixed c3cv_.
//
// Every key builds its objects in ONE arena (a single allocation, zero-filled, carved with `take`) and the
// registry's state for the key is that whole arena (re/frida/registry.d/c3cv.py, C3CV_ARENA): everything the
// function, its callees and our callbacks write lives in it, so diff_hook snapshots, compares and restores all of
// it per vector. Each vector gets its own compress object(s) inside the arena (fixture names c0, c1, ...).
//
// Callbacks (NativeCallbacks, outside the arena) are pure functions of their arguments and of arena memory; each
// records into the recorder found at cinfo+0x170 (past the end of jpeg_compress_struct, 0x168):
//   rec+0 rolling hash, +4 pushes, +8 last value; +0x10 call counter A, +0x14 fail mask A, +0x18 call counter B,
//   +0x1c step nibbles B.
// error_exit records and RETURNS (round 19), so the instructions after every ERREXIT run in both arms.
//
// libjpeg offsets used (see shim/src/re/c3cv.cpp for the full list): cinfo err +0, mem +4 (alloc_small +0,
// access_virt_barray +0x20; our pool pointer at mem+0x30), dest +0x14 (next_output_byte +0, free_in_buffer +4,
// empty_output_buffer +0xc; our refill count +0x14, spare buffer +0x18, ok flag +0x1c), image_width +0x18,
// image_height +0x1c, num_components +0x34, comp_info +0x3c, dc/ac_huff_tbl_ptrs +0x50 / +0x60, raw_data_in +0xa8,
// restart_interval +0xc0, max_v_samp_factor +0xe4, total_iMCU_rows +0xe8, comps_in_scan +0xec, cur_comp_info +0xf0,
// MCUs_per_row +0x100, blocks_in_MCU +0x108, MCU_membership +0x10c, Ss/Se/Ah/Al +0x134/+0x138/+0x13c/+0x140,
// main +0x148, prep +0x14c, coef +0x150, cconvert +0x158, downsample +0x15c, fdct +0x160, entropy +0x164.
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function fill(p, n, v) { for (let i = 0; i + 4 <= n; i += 4) p.add(i).writeU32(v >>> 0); }

  // Sizes must equal C3CV_ARENA in re/frida/registry.d/c3cv.py.
  const ARENA = {
    spm: 0x4000, pdsm: 0x4000, spc: 0x4000, cd: 0xa000, cfp: 0x1c000, co: 0x18000, jpe: 0x8000, spp: 0x2a000,
    dcf: 0x18000, dcr: 0x12000, acf: 0x18000, acr: 0x18000, rst: 0x10000, fpg: 0x14000, ppd: 0x8000, ppc: 0x8000,
  };
  function arena(key) {
    const size = ARENA[key];
    const base = Memory.alloc(size);
    fill(base, size, 0);
    let cur = 0;
    return {
      base: base,
      take(n, v) {
        n = (n + 15) & ~15;
        if (cur + n > size) throw new Error('c3cv arena ' + key + ' overflow');
        const p = base.add(cur);
        cur += n;
        fill(p, n, v === undefined ? 0 : v);
        return p;
      },
    };
  }

  // ---- recorder ------------------------------------------------------------------------------------------------
  function push(rec, v) {
    v = (typeof v === 'number') ? (v >>> 0) : v.toUInt32();
    rec.writeU32((Math.imul(rec.readU32(), 33) + v) >>> 0);
    rec.add(4).writeU32((rec.add(4).readU32() + 1) >>> 0);
    rec.add(8).writeU32(v);
  }
  function recOf(cinfo) { return cinfo.add(0x170).readPointer(); }
  function nextIdx(rec, off) { const n = rec.add(off).readU32(); rec.add(off).writeU32(n + 1); return n; }

  // ---- error manager: records msg_code / msg_parm.i[0] in err+0x30..0x3b and RETURNS ---------------------------
  const ERR_REC = keep(new NativeCallback(function (cinfo) {
    const e = cinfo.readPointer();
    let h = e.add(0x30).readU32();
    h = (Math.imul(h, 33) + e.add(0x14).readU32()) >>> 0;
    h = (Math.imul(h, 33) + e.add(0x18).readU32()) >>> 0;
    e.add(0x30).writeU32(h);
    e.add(0x34).writeU32((e.add(0x34).readU32() + 1) >>> 0);
    e.add(0x38).writeU32(e.add(0x14).readU32());
  }, 'void', ['pointer']));
  // start_pass_phuff's missing-table vectors: records like ERR_REC, then for message 0x31 with a number n (any
  // value) installs the spare JHUFF_TBL kept at err+0x40 in the dword the original re-reads after the ERREXIT
  // (cinfo+0x50+4n for a DC band, cinfo+0x60+4n for an AC band; err+0x44 holds 1 for DC) when it is NULL, so
  // jpeg_make_c_derived_tbl then runs on a real table in both arms instead of dereferencing NULL.
  const ERR_FIX = keep(new NativeCallback(function (cinfo) {
    const e = cinfo.readPointer();
    let h = e.add(0x30).readU32();
    h = (Math.imul(h, 33) + e.add(0x14).readU32()) >>> 0;
    h = (Math.imul(h, 33) + e.add(0x18).readU32()) >>> 0;
    e.add(0x30).writeU32(h);
    e.add(0x34).writeU32((e.add(0x34).readU32() + 1) >>> 0);
    e.add(0x38).writeU32(e.add(0x14).readU32());
    const n = e.add(0x18).readS32();
    if (e.add(0x14).readU32() === 0x31 && n >= -1 && n < 8) {
      const slot = cinfo.add((e.add(0x44).readU32() ? 0x50 : 0x60) + n * 4);
      if (slot.readPointer().isNull()) slot.writePointer(e.add(0x40).readPointer());
    }
  }, 'void', ['pointer']));
  // The encoders' gather-flag re-reads: records like ERR_REC, then for message 0x27 (a size-0 Huffman code) sets the
  // encoder's gather_statistics (cinfo->entropy+0xc), so the instructions after the ERREXIT that re-read the flag
  // take the gathering side in both arms.
  const ERR_GATHER = keep(new NativeCallback(function (cinfo) {
    const e = cinfo.readPointer();
    let h = e.add(0x30).readU32();
    h = (Math.imul(h, 33) + e.add(0x14).readU32()) >>> 0;
    e.add(0x30).writeU32(h);
    e.add(0x34).writeU32((e.add(0x34).readU32() + 1) >>> 0);
    e.add(0x38).writeU32(e.add(0x14).readU32());
    if (e.add(0x14).readU32() === 0x27) cinfo.add(0x164).readPointer().add(0xc).writeU32(1);
  }, 'void', ['pointer']));
  function mkErr(A, kind) {
    const e = A.take(0x84, 0x22222222);
    e.writePointer(kind === 'gather' ? ERR_GATHER : (kind ? ERR_FIX : ERR_REC));
    fill(e.add(0x30), 0x10, 0);
    return e;
  }

  // ---- memory manager: alloc_small bump allocator over the pool at mem+0x30 (pool+0 cursor, pool+4 hash of the
  // (pool_id, size) requests); blocks pre-filled 0xcc. access_virt_barray at mem+0x20 (see ACCESS).
  const ALLOC_SMALL = keep(new NativeCallback(function (cinfo, pool, size) {
    const p = cinfo.add(4).readPointer().add(0x30).readPointer();
    const cur = p.readU32();
    p.writeU32((cur + ((size + 15) & ~15)) >>> 0);
    p.add(4).writeU32((Math.imul(p.add(4).readU32(), 33) + pool * 0x10000 + size) >>> 0);
    return p.add(0x10 + cur);
  }, 'pointer', ['pointer', 'int', 'uint32']));
  // access_virt_barray(cinfo, ptr, start_row, num_rows, writable): our "virtual array" holds its row-pointer array
  // at ptr+0; returns &rows[start_row] and records the request.
  const ACCESS = keep(new NativeCallback(function (cinfo, va, start, num, writable) {
    const rec = recOf(cinfo);
    push(rec, 0xacce55); push(rec, va); push(rec, start); push(rec, num); push(rec, writable);
    return va.readPointer().add(start * 4);
  }, 'pointer', ['pointer', 'pointer', 'uint32', 'uint32', 'int']));
  function mkMem(A, poolSize) {
    const mem = A.take(0x40);
    const pool = A.take(0x10 + poolSize, 0xcccccccc);
    fill(pool, 0x10, 0);
    mem.writePointer(ALLOC_SMALL);
    mem.add(0x20).writePointer(ACCESS);
    mem.add(0x30).writePointer(pool);
    return mem;
  }

  // ---- destination: empty_output_buffer resets next/free to the spare buffer (dest+0x18) with dest+0x14 bytes, or
  // with ok (dest+0x1c) 0 records and returns FALSE leaving dest alone (jcphuff's dump_buffer then raises ERREXIT).
  const EMPTY = keep(new NativeCallback(function (cinfo) {
    const d = cinfo.add(0x14).readPointer();
    const ok = d.add(0x1c).readU32();
    const free = d.add(0x14).readU32();
    push(recOf(cinfo), (ok ? 0xe0000000 : 0xd0000000) + free);
    if (!ok) return 0;
    d.writePointer(d.add(0x18).readPointer());
    d.add(4).writeU32(free);
    return 1;
  }, 'int', ['pointer']));
  function mkDest(A, free, refill, ok) {
    const buf = A.take(0x200, 0x5b5b5b5b);
    const spare = A.take(0x40, 0x6c6c6c6c);
    const d = A.take(0x20);
    d.writePointer(buf);
    d.add(4).writeU32(free);
    d.add(0xc).writePointer(EMPTY);
    d.add(0x14).writeU32(refill);
    d.add(0x18).writePointer(spare);
    d.add(0x1c).writeU32(ok === undefined ? 1 : ok);
    return d;
  }

  // ---- method callbacks ----------------------------------------------------------------------------------------
  // forward_DCT(cinfo, compptr, sample_data, coef_blocks, start_row, start_col, num_blocks): records the call and
  // writes coefficients 0, 1 and 63 of each of the num_blocks consecutive blocks from values of the call.
  const FDCT = keep(new NativeCallback(function (cinfo, comp, sample, blocks, row, col, num) {
    const rec = recOf(cinfo);
    const n = nextIdx(rec, 0x18);
    push(rec, 0xfdc7); push(rec, comp); push(rec, sample); push(rec, blocks); push(rec, row); push(rec, col);
    push(rec, num);
    for (let i = 0; i < num; i++) {
      const b = blocks.add(i * 0x80);
      b.writeS16(((n * 37 + row * 11 + col * 3 + i * 5) % 4000) - 2000);
      b.add(2).writeS16(n + i + 1);
      b.add(126).writeS16(-(i + 1));
    }
  }, 'void', ['pointer', 'pointer', 'pointer', 'pointer', 'uint32', 'uint32', 'uint32']));
  // encode_mcu(cinfo, MCU_data): records the call number, each of the blocks_in_MCU (cinfo+0x108, at most 10)
  // block pointers and their coefficients 0, 1 and 63; returns 0 when bit n of rec+0x14 is set.
  const ENCODE = keep(new NativeCallback(function (cinfo, md) {
    const rec = recOf(cinfo);
    const n = nextIdx(rec, 0x10);
    push(rec, 0xe9c0de00 + n);
    const nb = Math.min(cinfo.add(0x108).readS32(), 10);
    for (let b = 0; b < nb; b++) {
      const p = md.add(b * 4).readPointer();
      push(rec, p); push(rec, p.readU16()); push(rec, p.add(2).readU16()); push(rec, p.add(126).readU16());
    }
    return ((rec.add(0x14).readU32() >>> n) & 1) ? 0 : 1;
  }, 'int', ['pointer', 'pointer']));
  // pre_process_data as process_data_simple_main sees it: records its arguments, then advances *in_row_ctr by one
  // (while below in_rows_avail) and *out_row_group_ctr by nibble n of rec+0x1c, capped at out_row_groups_avail.
  const PREP = keep(new NativeCallback(function (cinfo, inb, inCtr, inAvail, outb, outCtr, outAvail) {
    const rec = recOf(cinfo);
    const n = nextIdx(rec, 0x18);
    const i0 = inCtr.readU32(), o0 = outCtr.readU32();
    push(rec, 0x9e9e0000 + n); push(rec, inb); push(rec, i0); push(rec, inAvail); push(rec, outb); push(rec, o0);
    push(rec, outAvail);
    if (i0 < inAvail) inCtr.writeU32(i0 + 1);
    const step = (rec.add(0x1c).readU32() >>> (4 * (n & 7))) & 0xf;
    const room = o0 < outAvail ? outAvail - o0 : 0;
    outCtr.writeU32(o0 + Math.min(step, room));
  }, 'void', ['pointer', 'pointer', 'pointer', 'uint32', 'pointer', 'pointer', 'uint32']));
  // compress_data as process_data_simple_main sees it: records, returns 0 when bit n of rec+0x14 is set.
  const COMPRESS = keep(new NativeCallback(function (cinfo, inb) {
    const rec = recOf(cinfo);
    const n = nextIdx(rec, 0x10);
    push(rec, 0xc0c0000 + n); push(rec, inb);
    return ((rec.add(0x14).readU32() >>> n) & 1) ? 0 : 1;
  }, 'int', ['pointer', 'pointer']));
  // color_convert(cinfo, input_buf, output_buf, output_row, num_rows): records, then for every component writes
  // rows output_row .. +num_rows-1 of output_buf[ci] (image_width bytes) from input rows input_buf[r].
  const CCONVERT = keep(new NativeCallback(function (cinfo, inb, outb, orow, num) {
    const rec = recOf(cinfo);
    const n = nextIdx(rec, 0x18);
    push(rec, 0xcc0000 + n); push(rec, inb); push(rec, orow); push(rec, num);
    const comps = cinfo.add(0x34).readS32();
    const w = cinfo.add(0x18).readU32();
    for (let ci = 0; ci < comps; ci++) {
      const rows = outb.add(ci * 4).readPointer();
      for (let r = 0; r < num; r++) {
        const src = inb.add(r * 4).readPointer();
        const dst = rows.add((orow + r) * 4).readPointer();
        for (let x = 0; x < w; x++) dst.add(x).writeU8((src.add(x).readU8() + ci * 40 + n) & 0xff);
      }
    }
  }, 'void', ['pointer', 'pointer', 'pointer', 'uint32', 'int']));
  // downsample(cinfo, input_buf, in_row_index, output_buf, out_row_group_index): records, then for every component
  // writes rows out_row_group_index * v .. + v - 1 of output_buf[ci] (width_in_blocks * 8 bytes) from color rows
  // in_row_index + j.
  const DOWNSAMPLE = keep(new NativeCallback(function (cinfo, inb, inIdx, outb, outIdx) {
    const rec = recOf(cinfo);
    push(rec, 0xd0e5); push(rec, inb); push(rec, inIdx); push(rec, outb); push(rec, outIdx);
    const comps = cinfo.add(0x34).readS32();
    const w = cinfo.add(0x18).readU32();
    const ci0 = cinfo.add(0x3c).readPointer();
    for (let ci = 0; ci < comps; ci++) {
      const comp = ci0.add(ci * 0x54);
      const v = comp.add(0xc).readU32();
      const ow = comp.add(0x1c).readU32() * 8;
      const inRows = inb.add(ci * 4).readPointer();
      const outRows = outb.add(ci * 4).readPointer();
      for (let j = 0; j < v; j++) {
        const src = inRows.add((inIdx + j) * 4).readPointer();
        const dst = outRows.add((outIdx * v + j) * 4).readPointer();
        for (let x = 0; x < ow; x++) dst.add(x).writeU8((src.add(x % w).readU8() ^ (j * 16 + ci)) & 0xff);
      }
    }
  }, 'void', ['pointer', 'pointer', 'uint32', 'pointer', 'uint32']));
  function methodObj(A, fn) { const o = A.take(0x10); o.add(4).writePointer(fn); return o; }

  // ---- shared pieces ---------------------------------------------------------------------------------------------
  // A compress object (0x180 bytes: the struct is 0x168, our recorder pointer sits at +0x170).
  function mkCinfo(A, fix) {
    const c = A.take(0x180);
    c.writePointer(mkErr(A, fix));
    const rec = A.take(0x40);
    c.add(0x170).writePointer(rec);
    return { c: c, rec: rec };
  }
  // jpeg_component_info entries (0x54 bytes each, pre-filled 0x99) from objects with the fields we set.
  function mkComps(A, list) {
    const k = A.take(Math.max(list.length, 4) * 0x54, 0x99999999);
    list.forEach(function (o, i) {
      const p = k.add(i * 0x54);
      const set = function (off, v) { if (v !== undefined) p.add(off).writeS32(v); };
      set(0, o.id === undefined ? i + 1 : o.id);
      set(4, o.idx === undefined ? i : o.idx);
      set(8, o.h); set(0xc, o.v); set(0x14, o.dc); set(0x18, o.ac); set(0x1c, o.wib); set(0x20, o.hib);
      set(0x34, o.w); set(0x38, o.hh); set(0x40, o.sw); set(0x44, o.lcw); set(0x48, o.lrh);
    });
    return k;
  }
  function setScan(c, comps, n) {
    c.add(0xec).writeS32(n);
    for (let i = 0; i < 4; i++) c.add(0xf0 + i * 4).writePointer(comps.add(i * 0x54));
  }
  // A JHUFF_TBL: bits[1..16] at +1, our own huffval permutation at +0x11.
  const BITS = [
    [0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0],
    [0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 20],
    [1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]];
  function huffTbl(A, bits, seed) {
    const h = A.take(0x118);
    let count = 0;
    for (let i = 0; i < 16; i++) { h.add(1 + i).writeU8(bits[i]); count += bits[i]; }
    for (let i = 0; i < count; i++) h.add(0x11 + i).writeU8((i * 23 + seed) & 0xff);
    return h;
  }
  // A synthetic c_derived_tbl (ehufco dwords at +0, ehufsi bytes at +0x400): sizes 1..16 by formula, every fifth
  // symbol's code all ones (0xff bytes, so the stuffed zero runs); `zeros` lists symbols given size 0.
  function derivedTbl(A, seed, zeros) {
    const t = A.take(0x500);
    for (let s = 0; s < 256; s++) {
      t.add(s * 4).writeU32((s % 5 === 0) ? 0xffff : ((s * 0x9e37 + seed * 0x101) & 0xffff));
      t.add(0x400 + s).writeU8(1 + ((s * 7 + seed) % 16));
    }
    (zeros || []).forEach(function (s) { t.add(0x400 + s).writeU8(0); });
    return t;
  }
  // 8x8 blocks (0x80 bytes, coefficient k of the zig-zag order written through jpeg_natural_order 0x004bd0ac).
  const NAT = ptr('0x004bd0ac');
  function mkBlock(A, pairs) {
    const b = A.take(0x80);
    pairs.forEach(function (kv) { b.add(NAT.add(kv[0] * 4).readS32() * 2).writeS16(kv[1]); });
    return b;
  }
  function seqBlock(A, f) { const pairs = []; for (let k = 0; k < 64; k++) { const v = f(k); if (v) pairs.push([k, v]); } return mkBlock(A, pairs); }

  // ================================================================================================================
  // jcmainct.c
  // ================================================================================================================
  // start_pass_main 0x004b05f0: [raw_data_in, main pre-fill]; the pass mode is the vector's second argument.
  const SPM_ROWS = [[0, 0x77777777], [1, 0x77777777], [0, 0x77777777], [0, 0x77777777], [0, 0x77777777],
    [1, 0x77777777], [0, 0x77777777], [0, 0x77777777], [0, 0x11111111], [2, 0x77777777], [0, 0], [0, 0x77777777]];

  // process_data_simple_main 0x004b0640: {total, cur, rg (rowgroup_ctr), susp, inCtr, steps (nibbles), fail}.
  const PDSM_ROWS = [
    { total: 2, cur: 0, rg: 0, susp: 0, inCtr: 0, steps: 0x88, fail: 0 },
    { total: 1, cur: 1, rg: 0, susp: 0, inCtr: 3, steps: 0x8, fail: 0 },
    { total: 3, cur: 0, rg: 0, susp: 0, inCtr: 0, steps: 0x4, fail: 0 },
    { total: 2, cur: 0, rg: 8, susp: 0, inCtr: 2, steps: 0x8, fail: 0 },
    { total: 2, cur: 0, rg: 0, susp: 0, inCtr: 5, steps: 0x8, fail: 1 },
    { total: 2, cur: 0, rg: 0, susp: 1, inCtr: 5, steps: 0x8, fail: 1 },
    { total: 3, cur: 0, rg: 0, susp: 1, inCtr: 4, steps: 0x888, fail: 0 },
    { total: 3, cur: 0, rg: 9, susp: 0, inCtr: 1, steps: 0x8, fail: 0 },
    { total: 4, cur: 1, rg: 0, susp: 0, inCtr: 0, steps: 0x38, fail: 0 },
    { total: 3, cur: 0, rg: 0, susp: 0, inCtr: 0, steps: 0x888, fail: 2 },
    { total: 0, cur: 0, rg: 0, susp: 0, inCtr: 0, steps: 0x8, fail: 0 },
    { total: 2, cur: 0, rg: 5, susp: 0, inCtr: 7, steps: 0x83, fail: 0 },
    { total: 3, cur: 0, rg: 8, susp: 1, inCtr: 6, steps: 0x8, fail: 1 },
    { total: 5, cur: 2, rg: 0, susp: 1, inCtr: 9, steps: 0x0828, fail: 0 },
  ];

  // ================================================================================================================
  // jccoefct.c
  // ================================================================================================================
  // start_pass_coef 0x004b07b0: [mode (vector), whole_image[0] set, comps_in_scan, total_iMCU_rows, v, lrh].
  const SPC_ROWS = [[0, 0, 1, 3, 2, 1], [0, 1, 1, 3, 2, 1], [2, 1, 2, 1, 2, 1], [2, 0, 1, 1, 2, 1], [3, 1, 1, 1, 3, 2],
    [3, 0, 1, 2, 1, 1], [1, 1, 1, 2, 2, 1], [4, 0, 3, 2, 2, 2], [-1, 1, 1, 1, 1, 1], [0, 0, 2, 5, 2, 1],
    [3, 1, 1, 1, 1, 1], [2, 1, 1, 4, 2, 1]];

  // compress_data 0x004b08b0: {mcus, total, imcu, ctr, voff, rows, fail, comps: [{w, hh, lcw, lrh, sw, v}]}.
  const CD_ROWS = [
    { mcus: 3, total: 4, imcu: 0, ctr: 0, voff: 0, rows: 1, fail: 0,
      comps: [{ w: 2, hh: 1, lcw: 1, lrh: 1, sw: 16, v: 1 }, { w: 1, hh: 1, lcw: 1, lrh: 1, sw: 8, v: 1 }] },
    { mcus: 2, total: 4, imcu: 3, ctr: 0, voff: 0, rows: 1, fail: 0,
      comps: [{ w: 1, hh: 2, lcw: 1, lrh: 1, sw: 8, v: 2 }, { w: 2, hh: 2, lcw: 1, lrh: 1, sw: 16, v: 2 }] },
    { mcus: 4, total: 4, imcu: 1, ctr: 0, voff: 0, rows: 1, fail: 2,
      comps: [{ w: 2, hh: 2, lcw: 2, lrh: 2, sw: 16, v: 2 }, { w: 1, hh: 1, lcw: 1, lrh: 1, sw: 8, v: 1 }] },
    { mcus: 3, total: 4, imcu: 0, ctr: 5, voff: 0, rows: 2, fail: 0,
      comps: [{ w: 1, hh: 1, lcw: 1, lrh: 1, sw: 8, v: 1 }] },
    { mcus: 3, total: 4, imcu: 0, ctr: 0, voff: 1, rows: 1, fail: 0,
      comps: [{ w: 1, hh: 1, lcw: 1, lrh: 1, sw: 8, v: 1 }] },
    { mcus: 2, total: 2, imcu: 0, ctr: 0, voff: 0, rows: 1, fail: 0, nscan: 0,
      comps: [{ w: 1, hh: 1, lcw: 1, lrh: 1, sw: 8, v: 1 }] },
    { mcus: 2, total: 3, imcu: 0, ctr: 0, voff: 0, rows: 1, fail: 0,
      comps: [{ w: 1, hh: 0, lcw: 1, lrh: 1, sw: 8, v: 1 }, { w: 2, hh: 1, lcw: 2, lrh: 1, sw: 16, v: 1 }] },
    { mcus: 2, total: 2, imcu: 1, ctr: 0, voff: 0, rows: 1, fail: 0,
      comps: [{ w: 1, hh: 1, lcw: 1, lrh: 1, sw: 8, v: 1 }, { w: 0, hh: 2, lcw: 0, lrh: 1, sw: 0, v: 2 }] },
    { mcus: 2, total: 2, imcu: 1, ctr: 0, voff: 0, rows: 2, fail: 0,
      comps: [{ w: 1, hh: 1, lcw: 1, lrh: 2, sw: 8, v: 2 }] },
    { mcus: 3, total: 3, imcu: 0, ctr: 1, voff: 0, rows: 1, fail: 1,
      comps: [{ w: 2, hh: 1, lcw: 1, lrh: 1, sw: 16, v: 1 }] },
    { mcus: 1, total: 3, imcu: 0, ctr: 0, voff: 0, rows: 1, fail: 0,
      comps: [{ w: 3, hh: 1, lcw: 1, lrh: 1, sw: 24, v: 1 }, { w: 1, hh: 1, lcw: 1, lrh: 1, sw: 8, v: 1 }] },
    { mcus: 2, total: 3, imcu: 0, ctr: 0, voff: 0, rows: 2, fail: 4,
      comps: [{ w: 1, hh: 1, lcw: 1, lrh: 1, sw: 8, v: 2 }] },
  ];

  // compress_first_pass 0x004b0b20 / compress_output 0x004b0d70 share one builder. Row: {total, imcu, ctr, voff,
  // rows (MCU_rows_per_iMCU_row), mcus (MCUs_per_row), fail, ncomp (num_components, default comps.length), nscan
  // (comps_in_scan, default comps.length), comps: [{h, v, wib, hib, w (MCU_width, default h), hh (MCU_height,
  // default v)}]}. Each component gets a virtual array of (imcu + 1) * v + 1 rows of `len` blocks (enough for
  // the padded width and MCUs_per_row * MCU_width) pre-filled 0x3c3c; access_virt_barray returns its row array.
  const CFP_ROWS = [
    { total: 3, imcu: 0, rows: 1, mcus: 3, fail: 0, comps: [{ h: 1, v: 1, wib: 3, hib: 3 }] },
    { total: 2, imcu: 1, rows: 1, mcus: 2, fail: 0, comps: [{ h: 2, v: 2, wib: 3, hib: 3 }] },
    { total: 2, imcu: 1, rows: 1, mcus: 2, fail: 0, comps: [{ h: 2, v: 2, wib: 4, hib: 4 }, { h: 1, v: 1, wib: 2, hib: 2 }] },
    { total: 2, imcu: 1, rows: 1, mcus: 2, fail: 1, comps: [{ h: 2, v: 2, wib: 4, hib: 4 }, { h: 1, v: 1, wib: 2, hib: 2 }] },
    { total: 2, imcu: 0, rows: 1, mcus: 1, fail: 0, ncomp: 0, nscan: 0, comps: [{ h: 1, v: 1, wib: 1, hib: 1 }] },
    { total: 1, imcu: 0, rows: 1, mcus: 2, fail: 0, comps: [{ h: 3, v: 1, wib: 4, hib: 1 }] },
    { total: 2, imcu: 1, rows: 1, mcus: 2, fail: 0, comps: [{ h: 1, v: 3, wib: 2, hib: 4 }] },
    { total: 1, imcu: 0, rows: 1, mcus: 0, fail: 0, comps: [{ h: 2, v: 2, wib: 0, hib: 1 }] },
    { total: 3, imcu: 2, rows: 1, mcus: 3, fail: 0, comps: [{ h: 2, v: 2, wib: 5, hib: 6 }] },
    { total: 2, imcu: 0, rows: 1, mcus: 2, fail: 0, comps: [{ h: 1, v: 2, wib: 2, hib: 3 }, { h: 1, v: 1, wib: 2, hib: 2 }] },
    { total: 2, imcu: 1, rows: 1, mcus: 2, fail: 2, comps: [{ h: 2, v: 2, wib: 3, hib: 4 }, { h: 1, v: 1, wib: 2, hib: 2 }] },
    { total: 3, imcu: 0, rows: 1, mcus: 1, fail: 0, nscan: 0, comps: [{ h: 1, v: -1, wib: 2, hib: 2 }] },
    // h_samp_factor 0xfe000000: ndummy = h - wib (0x004b0bd8), width + ndummy = h, so the bottom row's jzero_far size
    // h << 7 wraps to 0 (0x004b0cbb), one MCU across, and the per-block DC loop's h <= 0 test (0x004b0cdd) is taken.
    { total: 1, imcu: 0, rows: 1, mcus: 1, fail: 0, nscan: 0, comps: [{ h: -0x2000000, v: 2, wib: 2, hib: 1, w: 1, hh: 1 }] },
  ];
  const CO_ROWS = [
    { total: 3, imcu: 0, ctr: 0, voff: 0, rows: 1, mcus: 2, fail: 0, comps: [{ h: 2, v: 2, wib: 4, hib: 4 }, { h: 1, v: 1, wib: 2, hib: 2 }] },
    { total: 3, imcu: 1, ctr: 1, voff: 0, rows: 1, mcus: 2, fail: 0, comps: [{ h: 2, v: 2, wib: 4, hib: 4 }, { h: 1, v: 1, wib: 2, hib: 2 }] },
    { total: 3, imcu: 0, ctr: 0, voff: 1, rows: 1, mcus: 2, fail: 0, comps: [{ h: 1, v: 1, wib: 2, hib: 2 }] },
    { total: 3, imcu: 0, ctr: 0, voff: 0, rows: 1, mcus: 0, fail: 0, comps: [{ h: 1, v: 1, wib: 2, hib: 2 }] },
    { total: 3, imcu: 0, ctr: 0, voff: 0, rows: 1, mcus: 2, fail: 0, nscan: 0, comps: [{ h: 1, v: 1, wib: 2, hib: 2 }] },
    { total: 3, imcu: 0, ctr: 0, voff: 0, rows: 1, mcus: 2, fail: 0, comps: [{ h: 1, v: 1, wib: 2, hib: 2, hh: 0 }, { h: 1, v: 1, wib: 2, hib: 2 }] },
    { total: 3, imcu: 0, ctr: 0, voff: 0, rows: 1, mcus: 2, fail: 0, comps: [{ h: 1, v: 1, wib: 2, hib: 2, w: 0 }, { h: 1, v: 1, wib: 2, hib: 2 }] },
    { total: 3, imcu: 2, ctr: 0, voff: 0, rows: 1, mcus: 2, fail: 1, comps: [{ h: 2, v: 2, wib: 4, hib: 6 }, { h: 1, v: 1, wib: 2, hib: 3 }] },
    { total: 3, imcu: 0, ctr: 0, voff: 0, rows: 2, mcus: 2, fail: 4, comps: [{ h: 1, v: 2, wib: 2, hib: 4, w: 1, hh: 1 }] },
    { total: 3, imcu: 1, ctr: 2, voff: 1, rows: 2, mcus: 3, fail: 0, comps: [{ h: 1, v: 2, wib: 3, hib: 4, w: 1, hh: 1 }] },
    { total: 2, imcu: 0, ctr: 0, voff: 0, rows: 1, mcus: 2, fail: 0,
      comps: [{ h: 1, v: 1, wib: 2, hib: 1 }, { h: 2, v: 1, wib: 4, hib: 1 }, { h: 1, v: 2, wib: 2, hib: 2 }] },
  ];

  // ================================================================================================================
  // jcphuff.c
  // ================================================================================================================
  // start_pass_phuff 0x004b1f70: {Ss, Ah, gather (vector), tbls [[dc, ac]...], dc / ac (slots holding a JHUFF_TBL),
  // preDerived / preCount (slots already holding a block), bitbuf (bit_buffer preset), ri, fix (ERR_FIX with the
  // spare table and band flag), x74 / x60 (a JHUFF_TBL at cinfo+0x74 / +0x60 for out-of-range numbers)}.
  const SPP_ROWS = [
    { Ss: 0, Ah: 0, g: 0, tbls: [[0, 0], [1, 1]], dc: [0, 1], ac: [] },
    { Ss: 0, Ah: 0, g: 1, tbls: [[0, 0], [1, 1], [0, 1]], dc: [], ac: [] },
    { Ss: 1, Ah: 0, g: 0, tbls: [[0, 2]], dc: [], ac: [2], preDerived: [2] },
    { Ss: 1, Ah: 0, g: 1, tbls: [[0, 3]], dc: [], ac: [], preCount: [3] },
    { Ss: 0, Ah: 1, g: 0, tbls: [[0, 0], [1, 1]], dc: [], ac: [] },
    { Ss: 1, Ah: 1, g: 0, tbls: [[0, 1]], dc: [], ac: [1] },
    { Ss: 1, Ah: 1, g: 1, tbls: [[0, 0]], dc: [], ac: [], bitbuf: 1 },
    { Ss: 0, Ah: 0, g: 0, tbls: [], dc: [], ac: [], ri: 5 },
    { Ss: 0, Ah: 0, g: 0, tbls: [[2, 0]], dc: [], ac: [], fix: 1 },
    { Ss: 1, Ah: 0, g: 0, tbls: [[0, 1]], dc: [], ac: [], fix: 1 },
    { Ss: 0, Ah: 0, g: 1, tbls: [[-1, 0]], dc: [], ac: [] },
    { Ss: 1, Ah: 0, g: 1, tbls: [[0, 4]], dc: [], ac: [] },
    { Ss: 1, Ah: 0, g: 0, tbls: [[0, 5]], dc: [], ac: [], x74: 1 },
    { Ss: 0, Ah: 0, g: 0, tbls: [[4, 0]], dc: [], ac: [0] },
    { Ss: 1, Ah: 2, g: 0, tbls: [[0, 2]], dc: [], ac: [2], bitbuf: 1, preDerived: [2], ri: 0x10 },
    { Ss: 0, Ah: 0, g: 1, tbls: [[0, 0]], dc: [], ac: [], preCount: [0], ri: 3 },
    { Ss: 1, Ah: 0, g: 1, tbls: [[0, -1]], dc: [], ac: [] },
  ];

  // The encoders' common builder. Row fields (all optional except blocks): gather, Al, Ss, Se, Ah, ri, togo, nrn,
  // pb / bits (put_buffer / put_bits), last (last_dc_val[4]), act (ac_tbl_no), eobrun, be, comps (dc tables of the
  // scan's components), memb (MCU_membership), blocks (list of [k, value] lists or {dc: v}), free / refill / ok
  // (destination), tbl (derived table set: 0 plain, 1 sizes 0 at DC symbols 0..2 and ZRL 0xf0, 2 sizes 0 at AC
  // symbols 0x01, 0x02, 0x11, 0x12, 0x21), bitfill (bit_buffer pattern seed).
  function encRow(A, r, i, fx) {
    const x = mkCinfo(A, r.errg ? 'gather' : undefined);
    const c = x.c;
    const d = mkDest(A, r.free === undefined ? 0x1f0 : r.free, r.refill === undefined ? 0x40 : r.refill, r.ok);
    c.add(0x14).writePointer(d);
    const comps = mkComps(A, (r.comps || [0, 1, 2, 3]).map(function (t) { return { dc: t, ac: t }; }));
    setScan(c, comps, r.nscan === undefined ? (r.comps || [0, 1, 2, 3]).length : r.nscan);
    c.add(0xc0).writeU32(r.ri || 0);
    c.add(0x134).writeS32(r.Ss || 0);
    c.add(0x138).writeS32(r.Se === undefined ? 63 : r.Se);
    c.add(0x13c).writeS32(r.Ah || 0);
    c.add(0x140).writeS32(r.Al || 0);
    const md = A.take(0x28);
    const blocks = r.blocks || [];
    c.add(0x108).writeS32(blocks.length);
    blocks.forEach(function (b, j) {
      c.add(0x10c + j * 4).writeS32((r.memb || [])[j] || 0);
      md.add(j * 4).writePointer(Array.isArray(b) ? mkBlock(A, b) : (typeof b === 'function' ? seqBlock(A, b) : mkBlock(A, [[0, b.dc]])));
    });
    const e = A.take(0x80, 0x55555555);
    e.add(0xc).writeU32(r.gather || 0);
    e.add(0x18).writeU32(r.pb || 0);
    e.add(0x1c).writeS32(r.bits || 0);
    e.add(0x10).writePointer(d.readPointer());
    e.add(0x14).writeU32(d.add(4).readU32());
    e.add(0x20).writePointer(c);
    const last = r.last || [0, 0, 0, 0];
    for (let j = 0; j < 4; j++) e.add(0x24 + j * 4).writeS32(last[j]);
    e.add(0x34).writeS32(r.act || 0);
    e.add(0x38).writeU32(r.eobrun || 0);
    e.add(0x3c).writeU32(r.be || 0);
    const bb = A.take(0x3f0);
    for (let k = 0; k < 1000; k++) bb.add(k).writeU8(((k * 7 + (r.bitfill || 3)) >> 2) & 1);
    e.add(0x40).writePointer(bb);
    e.add(0x44).writeU32(r.togo || 0);
    e.add(0x48).writeS32(r.nrn || 0);
    const set = fx._tabs[r.tbl || 0];
    for (let t = 0; t < 4; t++) {
      e.add(0x4c + t * 4).writePointer(set[t]);
      if (r.gather || r.counts) {
        const cnt = A.take(0x404);
        for (let s = 0; s < 0x101; s++) cnt.add(s * 4).writeU32(t * 0x1000 + s);
        e.add(0x5c + t * 4).writePointer(cnt);
      } else {
        e.add(0x5c + t * 4).writePointer(ptr(0));
      }
    }
    c.add(0x164).writePointer(e);
    fx['c' + i] = c;
    fx['n' + i] = e;
    fx['m' + i] = md;
    if (i === 0) fx.obj = c;
  }
  function encFixture(key, rows) {
    const A = arena(key);
    const fx = { arena: A.base };
    const plain = [derivedTbl(A, 3), derivedTbl(A, 5), derivedTbl(A, 9), derivedTbl(A, 13)];
    const z1 = derivedTbl(A, 7, [0, 1, 2, 0xf0]);
    const z2 = derivedTbl(A, 11, [0x01, 0x02, 0x11, 0x12, 0x21]);
    fx._tabs = [plain, [z1, z1, z1, z1], [z2, z2, z2, z2]];
    rows.forEach(function (r, i) { encRow(A, r, i, fx); });
    delete fx._tabs;
    return fx;
  }

  // Coefficient patterns (zig-zag index k, value).
  const B_SHORT = [[0, 100], [1, 5], [2, -3], [5, 9], [63, 1]];
  const B_RUNS = [[0, -20], [1, 2], [20, 7], [40, -1], [41, 300]];
  const B_RUN49 = [[0, 1], [50, -32768]];
  const B_FF = [[0, 0xff], [1, 0xff], [2, -0x100], [3, 0x7f], [4, -1]];
  const allNonzero = function (k) { return k === 0 ? 0 : (((k * 37) % 61) * ((k & 1) ? 1 : -1) || 3); };

  // encode_mcu_DC_first 0x004b2140.
  const DCF_ROWS = [
    { comps: [0, 1], memb: [0, 0, 1], blocks: [{ dc: 100 }, { dc: -50 }, { dc: 0 }] },
    { comps: [0, 1], memb: [0, 1, 1], Al: 1, last: [5, -7, 0, 0], blocks: [{ dc: -32768 }, { dc: 32767 }, { dc: 1 }] },
    { comps: [0, 1, 2, 3], memb: [0, 1, 2, 3], Al: 3, blocks: [{ dc: 0x7fff }, { dc: -1 }, { dc: -2 }, { dc: 255 }] },
    { gather: 1, comps: [0, 1], memb: [0, 1, 0], blocks: [{ dc: 7 }, { dc: -900 }, { dc: 7 }] },
    { comps: [0], memb: [0, 0], ri: 4, togo: 0, nrn: 3, pb: 0x12345600, bits: 5, blocks: [{ dc: 20 }, { dc: -20 }] },
    { comps: [0], memb: [0], ri: 2, togo: 1, nrn: 6, blocks: [{ dc: 3 }] },
    { comps: [0, 1], memb: [1, 0], ri: 1, togo: 0, nrn: 7, last: [9, 9, 9, 9], blocks: [{ dc: 4 }, { dc: -4 }] },
    { gather: 1, comps: [0, 1], memb: [0, 1], ri: 3, togo: 0, nrn: 1, blocks: [{ dc: 12 }, { dc: 13 }] },
    { comps: [0, 1], memb: [0, 0, 0, 0, 0, 0], free: 1, refill: 1, bits: 7, pb: 0x00fe0000,
      blocks: [{ dc: 0 }, { dc: 16 }, { dc: 528 }, { dc: -12 }, { dc: 243 }, { dc: 1266 }] },
    { comps: [0], memb: [0, 0, 0], tbl: 1, last: [0, 0, 0, 0], blocks: [{ dc: 0 }, { dc: 1 }, { dc: 4 }] },
    { comps: [0], memb: [0, 0], free: 2, refill: 0x10, ok: 0, blocks: [{ dc: 300 }, { dc: -300 }] },
    { comps: [0, 1], memb: [0, 1], Al: 0x10021, blocks: [{ dc: 0x4000 }, { dc: -0x4001 }] },
    { comps: [0, 1], memb: [], blocks: [], ri: 2, togo: 2 },
    { comps: [0], memb: [0], bits: 5, pb: 0x00a80000, last: [0x20000000, 0, 0, 0], blocks: [{ dc: -5 }] },
    { errg: 1, counts: 1, comps: [0], memb: [0, 0, 0], tbl: 1, blocks: [{ dc: 0 }, { dc: 1 }, { dc: 5 }] },
  ];
  // encode_mcu_DC_refine 0x004b2d30.
  const DCR_ROWS = [
    { comps: [0], memb: [0, 0, 0], Al: 0, blocks: [{ dc: 1 }, { dc: 2 }, { dc: 3 }] },
    { comps: [0], memb: [0, 0, 0, 0], Al: 1, blocks: [{ dc: 2 }, { dc: -2 }, { dc: 5 }, { dc: -7 }] },
    { comps: [0], memb: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], Al: 2, bits: 3, pb: 0x00e00000,
      blocks: [{ dc: 4 }, { dc: 4 }, { dc: 4 }, { dc: 4 }, { dc: 4 }, { dc: 4 }, { dc: 4 }, { dc: 4 }, { dc: 4 }, { dc: 4 }] },
    { gather: 1, comps: [0], memb: [0, 0], Al: 1, blocks: [{ dc: 3 }, { dc: 1 }] },
    { comps: [0], memb: [0, 0], ri: 3, togo: 0, nrn: 2, bits: 2, pb: 0x00c00000, blocks: [{ dc: 1 }, { dc: 0 }] },
    { comps: [0], memb: [0], ri: 5, togo: 4, nrn: 7, blocks: [{ dc: -1 }] },
    { comps: [0], memb: [0, 0, 0, 0, 0, 0, 0, 0, 0], free: 1, refill: 1, bits: 7, pb: 0x00fe0000, Al: 0,
      blocks: [{ dc: 1 }, { dc: 1 }, { dc: 1 }, { dc: 1 }, { dc: 1 }, { dc: 1 }, { dc: 1 }, { dc: 1 }, { dc: 1 }] },
    { comps: [0], memb: [], blocks: [] },
    { comps: [0], memb: [0, 0, 0, 0, 0, 0, 0, 0], Al: 0x10023, blocks: [{ dc: 8 }, { dc: 16 }, { dc: -8 }, { dc: 9 }, { dc: 0x7fff }, { dc: -0x8000 }, { dc: 0 }, { dc: 24 }] },
    { comps: [0], memb: [0, 0, 0, 0, 0, 0, 0, 0, 0], free: 1, refill: 0x20, ok: 0, bits: 7, pb: 0x00fe0000,
      blocks: [{ dc: 1 }, { dc: 1 }, { dc: 1 }, { dc: 1 }, { dc: 1 }, { dc: 1 }, { dc: 1 }, { dc: 1 }, { dc: 1 }] },
    { gather: 1, comps: [0, 1], memb: [0, 1], ri: 1, togo: 0, nrn: 4, blocks: [{ dc: 1 }, { dc: 0 }] },
    { comps: [0], memb: [0, 0, 0], bits: 15, pb: 0x00fffe00, blocks: [{ dc: 1 }, { dc: 1 }, { dc: 0 }] },
  ];
  // encode_mcu_AC_first 0x004b28c0 (MCU_data[0] only; act = ac_tbl_no).
  const ACF_ROWS = [
    { comps: [0], Ss: 1, Se: 63, act: 0, blocks: [B_SHORT] },
    { comps: [1], Ss: 1, Se: 63, act: 1, eobrun: 3, be: 2, blocks: [B_RUNS] },
    { comps: [0], Ss: 1, Se: 63, act: 2, eobrun: 5, blocks: [[[0, 9]]] },
    { comps: [0], Ss: 1, Se: 63, act: 3, eobrun: 0x7ffe, be: 5, blocks: [[[0, 9]]] },
    { comps: [0], Ss: 1, Se: 63, Al: 2, act: 0, blocks: [[[1, 3], [2, -3], [3, 4], [4, -4], [5, 1], [9, -9], [12, 2]]] },
    { gather: 1, comps: [0], Ss: 1, Se: 63, act: 2, eobrun: 2, blocks: [B_RUNS] },
    { comps: [0], Ss: 5, Se: 3, act: 0, eobrun: 1, blocks: [B_SHORT] },
    { comps: [0], Ss: 1, Se: 1, act: 1, blocks: [B_SHORT] },
    { comps: [0], Ss: 1, Se: 63, act: 0, free: 1, refill: 1, bits: 7, pb: 0x00fe0000, blocks: [B_FF] },
    { comps: [0], Ss: 1, Se: 63, act: 0, tbl: 1, blocks: [B_RUNS] },
    { comps: [0], Ss: 1, Se: 63, act: 0, tbl: 2, blocks: [[[1, 1], [2, 2], [4, -1], [6, 1]]] },
    { comps: [0], Ss: 1, Se: 63, act: 3, blocks: [B_RUN49] },
    { comps: [0], Ss: 1, Se: 63, act: 1, ri: 2, togo: 0, nrn: 5, eobrun: 2, be: 3, blocks: [B_SHORT] },
    { comps: [0], Ss: 1, Se: 63, act: 0, free: 2, refill: 8, ok: 0, blocks: [allNonzero] },
    { comps: [0], Ss: 1, Se: 63, act: 0, ri: 3, togo: 2, nrn: 1, blocks: [B_SHORT] },
    { comps: [0], Ss: 1, Se: 63, act: 0, free: 1, refill: 1, bits: 7, pb: 0x00fe0000, blocks: [[[50, -32768], [51, 31]]] },
    { comps: [0], Ss: 1, Se: 63, act: 0, free: 1, refill: 1, bits: 7, pb: 0x00fe0000, blocks: [[[1, 31], [2, 31], [3, -31], [4, 31], [5, 31]]] },
    { errg: 1, counts: 1, comps: [0], Ss: 1, Se: 63, act: 0, tbl: 1, blocks: [[[20, 7], [22, 1]]] },
    { errg: 1, counts: 1, comps: [0], Ss: 1, Se: 63, act: 0, tbl: 2, blocks: [[[1, 1], [3, 5]]] },
  ];
  // encode_mcu_AC_refine 0x004b2eb0.
  const ACR_ROWS = [
    { comps: [0], Ss: 1, Se: 63, Al: 1, act: 0, blocks: [[[1, 2], [2, 3], [3, 4], [4, -5], [5, 1], [6, -1], [8, -2], [10, 9]]] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 1, be: 4, blocks: [[[1, 3], [20, 1], [21, 5], [45, 2], [62, -6]]] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 2, be: 0x3a8, eobrun: 1, blocks: [[[1, 2], [2, 3], [3, 6], [40, 7], [41, 9], [50, 3]]] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 3, eobrun: 0x7ffe, blocks: [[[0, 4]]] },
    { gather: 1, comps: [0], Ss: 1, Se: 63, Al: 0, act: 2, eobrun: 2, be: 2, blocks: [[[1, 3], [20, 1], [21, 5], [45, 2], [62, -6]]] },
    { comps: [0], Ss: 9, Se: 4, act: 0, blocks: [B_SHORT] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 0, blocks: [function (k) { return k === 0 ? 0 : ((k & 1) ? 1 : -1); }] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 1, eobrun: 7, blocks: [[[0, 5]]] },
    { comps: [0], Ss: 1, Se: 30, Al: 1, act: 2, eobrun: 1, be: 10, blocks: [[[1, 4], [2, 6], [3, -8], [30, 12]]] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 0, free: 1, refill: 1, bits: 7, pb: 0x00fe0000, eobrun: 2, be: 3, blocks: [[[1, 1], [2, 2], [3, -1], [20, 1], [60, 3]]] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 0, tbl: 1, blocks: [[[1, 2], [25, 1], [26, 3]]] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 0, tbl: 2, blocks: [[[1, 1], [2, 4], [3, 1], [5, -1], [7, 1]]] },
    { comps: [0], Ss: 2, Se: 40, Al: 1, act: 3, ri: 1, togo: 0, nrn: 6, eobrun: 4, be: 6, blocks: [[[2, 2], [3, -3], [30, 6], [39, 2]]] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 1, blocks: [[[1, -1], [2, 1], [3, -1], [17, 1], [35, -1], [52, 7]]] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 0, ri: 3, togo: 2, nrn: 2, blocks: [[[1, 1], [2, 3]]] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 0, free: 1, refill: 1, bits: 7, pb: 0x00fe0000, blocks: [[[40, 1], [41, 2]]] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 0, free: 1, refill: 1, bits: 7, pb: 0x00fe0000,
      blocks: [function (k) { return (k % 5 === 1) ? 1 : 0; }] },
    { errg: 1, counts: 1, comps: [0], Ss: 1, Se: 63, Al: 0, act: 0, tbl: 1, blocks: [[[30, 1], [31, 1]]] },
    { errg: 1, counts: 1, comps: [0], Ss: 1, Se: 63, Al: 0, act: 0, tbl: 2, blocks: [[[1, 1], [2, 3]]] },
    { comps: [0], Ss: 1, Se: 3, Al: 0, act: 0, blocks: [[[1, 1], [2, 2], [3, 3]]] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 0, bits: 7, pb: 0x00fe0000, blocks: [function (k) { return (k % 5 === 1) ? 1 : 0; }] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 0, bits: 4, pb: 0x00f00000, blocks: [[[5, 1], [10, 1]]] },
    { comps: [0], Ss: 1, Se: 63, Al: 0, act: 0, free: 1, refill: 1, bits: 4, pb: 0x00f00000, blocks: [[[5, 1], [10, 1]]] },
  ];
  // emit_restart_4b2460 0x004b2460: entropy-level vectors (the entropy pointer and restart_num). nscan overrides
  // comps_in_scan (cinfo+0xec, read through entropy->cinfo).
  const RST_ROWS = [
    { Ss: 0, comps: [0, 1], bits: 3, pb: 0x00e00000, last: [5, 6, 7, 8] },
    { Ss: 0, comps: [0, 1, 2, 3], bits: 0, last: [1, 2, 3, 4] },
    { Ss: 1, act: 0, eobrun: 3, be: 4, bits: 6, pb: 0x00fc0000 },
    { Ss: 1, act: 1, eobrun: 0, be: 7 },
    { gather: 1, Ss: 0, comps: [0, 1, 2], last: [9, 9, 9, 9] },
    { gather: 1, Ss: 2, act: 2, eobrun: 5, be: 1 },
    { Ss: 0, nscan: 0, last: [3, 3, 3, 3] },
    { Ss: 0, comps: [0], free: 2, refill: 1, bits: 1, pb: 0x00800000, last: [1, 1, 1, 1] },
    { Ss: 0, comps: [0], free: 3, refill: 1, bits: 0, last: [2, 2, 2, 2] },
    { Ss: 1, act: 3, eobrun: 0x40, be: 0, free: 1, refill: 0x20, bits: 7, pb: 0x00fe0000 },
    { Ss: 0, comps: [0, 1], free: 2, refill: 4, ok: 0, last: [4, 4, 4, 4] },
    { Ss: 0, comps: [0, 1], bits: 4, pb: 0x00f00000, last: [7, 7, 7, 7] },
  ];
  const RST_NUMS = [0, 1, 2, 3, 4, 5, 6, 7, -1, 0x130, 3, 6];

  // finish_pass_gather_phuff 0x004b33c0: {Ss, Ah, tbls [[dc, ac]...], dc / ac (slots holding a JHUFF_TBL), act,
  // eobrun}. Count arrays hold at most 28 non-zero counts so every code length stays at or below 32.
  const FPG_ROWS = [
    { Ss: 0, Ah: 0, tbls: [[0, 0], [1, 1]], dc: [], ac: [] },
    { Ss: 0, Ah: 1, tbls: [[0, 0], [1, 1]], dc: [], ac: [] },
    { Ss: 1, Ah: 0, tbls: [[0, 2]], dc: [], ac: [2], act: 2, eobrun: 5 },
    { Ss: 0, Ah: 0, tbls: [], dc: [], ac: [] },
    { Ss: 0, Ah: 0, tbls: [[0, 0], [0, 1], [1, 1]], dc: [1], ac: [] },
    { Ss: 2, Ah: 0, tbls: [[0, 3]], dc: [], ac: [], act: 3 },
    { Ss: 0, Ah: 0, tbls: [[0, 0], [1, 0], [2, 0], [3, 0]], dc: [0, 2], ac: [] },
    { Ss: 1, Ah: 1, tbls: [[0, 1]], dc: [], ac: [1], act: 1, eobrun: 0x40 },
  ];

  // ================================================================================================================
  // jcprepct.c
  // ================================================================================================================
  // pre_process_data 0x004b3b80: {maxv, width, comps [{v, wib}], rtg (rows_to_go), nbr (next_buf_row), inCtr,
  // inAvail, outCtr, outAvail, ncomp (num_components, default comps.length)}.
  const PPD_ROWS = [
    { maxv: 2, width: 8, comps: [{ v: 2, wib: 1 }], rtg: 10, nbr: 0, inCtr: 0, inAvail: 4, outCtr: 0, outAvail: 1 },
    { maxv: 2, width: 8, comps: [{ v: 2, wib: 1 }], rtg: 10, nbr: 0, inCtr: 4, inAvail: 4, outCtr: 0, outAvail: 1 },
    { maxv: 2, width: 8, comps: [{ v: 2, wib: 1 }], rtg: 10, nbr: 0, inCtr: 0, inAvail: 2, outCtr: 1, outAvail: 1 },
    { maxv: 2, width: 8, comps: [{ v: 2, wib: 1 }], rtg: 1, nbr: 0, inCtr: 0, inAvail: 1, outCtr: 0, outAvail: 2 },
    { maxv: 2, width: 8, comps: [{ v: 2, wib: 1 }], rtg: 10, nbr: 1, inCtr: 0, inAvail: 5, outCtr: 0, outAvail: 2 },
    { maxv: 1, width: 6, comps: [{ v: 1, wib: 1 }], rtg: 100, nbr: 0, inCtr: 0, inAvail: 3, outCtr: 0, outAvail: 3 },
    { maxv: 2, width: 10, comps: [{ v: 2, wib: 2 }, { v: 1, wib: 1 }, { v: 1, wib: 1 }], rtg: 3, nbr: 0, inCtr: 1, inAvail: 4, outCtr: 0, outAvail: 3 },
    { maxv: 2, width: 8, comps: [{ v: 2, wib: 1 }], rtg: 2, nbr: 0, inCtr: 0, inAvail: 2, outCtr: 0, outAvail: 3 },
    { maxv: 2, width: 8, comps: [{ v: 2, wib: 1 }], ncomp: 0, rtg: 1, nbr: 0, inCtr: 0, inAvail: 1, outCtr: 0, outAvail: 2 },
    { maxv: 2, width: 8, comps: [{ v: 2, wib: 1 }], rtg: 1, nbr: 1, inCtr: 0, inAvail: 3, outCtr: 0, outAvail: 1 },
    { maxv: 3, width: 7, comps: [{ v: 3, wib: 1 }, { v: 1, wib: 1 }], rtg: 20, nbr: 2, inCtr: 2, inAvail: 9, outCtr: 1, outAvail: 3 },
    { maxv: 4, width: 8, comps: [{ v: 4, wib: 1 }], rtg: 50, nbr: 0, inCtr: 0, inAvail: 2, outCtr: 0, outAvail: 2 },
  ];
  // pre_process_context 0x004b3d40: as above plus height (image_height), trg (this_row_group), stop
  // (next_buf_stop). color_buf rows run from -maxv to 3 * maxv - 1.
  const PPC_ROWS = [
    { maxv: 1, width: 8, height: 10, comps: [{ v: 1, wib: 1 }], rtg: 10, nbr: 0, trg: 0, stop: 2, inCtr: 0, inAvail: 3, outCtr: 0, outAvail: 2 },
    { maxv: 1, width: 8, height: 10, comps: [{ v: 1, wib: 1 }], rtg: 10, nbr: 0, trg: 0, stop: 2, inCtr: 0, inAvail: 3, outCtr: 2, outAvail: 2 },
    { maxv: 1, width: 8, height: 10, comps: [{ v: 1, wib: 1 }], rtg: 4, nbr: 1, trg: 0, stop: 2, inCtr: 3, inAvail: 3, outCtr: 0, outAvail: 2 },
    { maxv: 1, width: 8, height: 10, comps: [{ v: 1, wib: 1 }], rtg: 0, nbr: 1, trg: 1, stop: 3, inCtr: 3, inAvail: 3, outCtr: 0, outAvail: 2 },
    { maxv: 1, width: 8, height: 10, comps: [{ v: 1, wib: 1 }], rtg: 0, nbr: 2, trg: 1, stop: 2, inCtr: 3, inAvail: 3, outCtr: 0, outAvail: 1 },
    { maxv: 2, width: 6, height: 7, comps: [{ v: 2, wib: 1 }, { v: 1, wib: 1 }], rtg: 7, nbr: 0, trg: 0, stop: 4, inCtr: 0, inAvail: 7, outCtr: 0, outAvail: 2 },
    { maxv: 2, width: 6, height: 9, comps: [{ v: 2, wib: 1 }], rtg: 5, nbr: 4, trg: 4, stop: 6, inCtr: 0, inAvail: 4, outCtr: 0, outAvail: 2 },
    { maxv: 2, width: 6, height: 9, comps: [{ v: 2, wib: 1 }], rtg: 5, nbr: 1, trg: 0, stop: 4, inCtr: 2, inAvail: 3, outCtr: 0, outAvail: 2 },
    { maxv: 1, width: 5, height: 4, comps: [{ v: 1, wib: 1 }], ncomp: 0, rtg: 4, nbr: 0, trg: 0, stop: 2, inCtr: 0, inAvail: 2, outCtr: 0, outAvail: 1 },
    { maxv: 0, width: 5, height: 4, comps: [{ v: 0, wib: 1 }], rtg: 4, nbr: 0, trg: 0, stop: 0, inCtr: 0, inAvail: 2, outCtr: 0, outAvail: 2 },
    { maxv: 1, width: 7, height: 6, comps: [{ v: 1, wib: 1 }, { v: 1, wib: 1 }, { v: 1, wib: 1 }], rtg: 0, nbr: 0, trg: 2, stop: 1, inCtr: 5, inAvail: 5, outCtr: 0, outAvail: 3 },
    { maxv: 2, width: 6, height: 9, comps: [{ v: 2, wib: 1 }], rtg: 6, nbr: 2, trg: 0, stop: 4, inCtr: 0, inAvail: 1, outCtr: 0, outAvail: 2 },
    { maxv: 1, width: 6, height: 5, comps: [{ v: 1, wib: 1 }], ncomp: 0, rtg: 0, nbr: 0, trg: 0, stop: 1, inCtr: 2, inAvail: 2, outCtr: 0, outAvail: 1 },
  ];

  // Sample-row machinery for the prep fixtures: a row-pointer array of `n` rows of `w` bytes each starting `before`
  // rows before the returned pointer, filled from `seed`.
  function rows(A, n, w, seed, before) {
    before = before || 0;
    const arr = A.take((n + before) * 4);
    for (let r = 0; r < n + before; r++) {
      const row = A.take(Math.max(w, 4));
      for (let x = 0; x < w; x++) row.add(x).writeU8((seed * 31 + r * 13 + x * 7) & 0xff);
      arr.add(r * 4).writePointer(row);
    }
    return arr.add(before * 4);
  }
  function prepFixture(key, rowsList, context) {
    const A = arena(key);
    const fx = { arena: A.base };
    rowsList.forEach(function (r, i) {
      const x = mkCinfo(A);
      const c = x.c;
      const ncomp = r.ncomp === undefined ? r.comps.length : r.ncomp;
      c.add(0x18).writeU32(r.width);
      c.add(0x1c).writeU32(r.height || 0);
      c.add(0x34).writeS32(ncomp);
      c.add(0x3c).writePointer(mkComps(A, r.comps.map(function (o) { return { v: o.v, wib: o.wib }; })));
      c.add(0xe4).writeS32(r.maxv);
      const prep = A.take(0x40, 0x44444444);
      const crows = context ? 3 * r.maxv : r.maxv;
      for (let ci = 0; ci < 10; ci++) {
        prep.add(8 + ci * 4).writePointer(ci < r.comps.length ? rows(A, Math.max(crows, 1), r.width, 40 + ci, context ? r.maxv : 0) : ptr(0));
      }
      prep.add(0x30).writeS32(r.rtg);
      prep.add(0x34).writeS32(r.nbr);
      if (context) { prep.add(0x38).writeS32(r.trg); prep.add(0x3c).writeS32(r.stop); }
      c.add(0x14c).writePointer(prep);
      c.add(0x158).writePointer(methodObj(A, CCONVERT));
      c.add(0x15c).writePointer(methodObj(A, DOWNSAMPLE));
      const inb = rows(A, Math.max(r.inAvail, 1), r.width, 7 + i);
      const outb = A.take(0x28);
      r.comps.forEach(function (o, ci) {
        outb.add(ci * 4).writePointer(rows(A, Math.max(r.outAvail * o.v, 1), Math.max(o.wib * 8, 1), 90 + ci, 1));
      });
      const ctr = A.take(0x10);
      ctr.writeU32(r.inCtr);
      ctr.add(4).writeU32(r.outCtr);
      fx['c' + i] = c;
      fx['i' + i] = inb;
      fx['o' + i] = outb;
      fx['ri' + i] = ctr;
      fx['ro' + i] = ctr.add(4);
      if (i === 0) fx.obj = c;
    });
    return fx;
  }

  // compress_first_pass / compress_output builder.
  function coefFixture(key, rowsList) {
    const A = arena(key);
    const fx = { arena: A.base };
    rowsList.forEach(function (r, i) {
      const x = mkCinfo(A);
      const c = x.c;
      const list = r.comps.map(function (o) {
        return { h: o.h, v: o.v, wib: o.wib, hib: o.hib, w: o.w === undefined ? o.h : o.w,
                 hh: o.hh === undefined ? o.v : o.hh, lcw: 1, lrh: 1, sw: 8 * o.h };
      });
      const comps = mkComps(A, list);
      const ncomp = r.ncomp === undefined ? list.length : r.ncomp;
      const nscan = r.nscan === undefined ? list.length : r.nscan;
      c.add(4).writePointer(mkMem(A, 0x10));
      c.add(0x34).writeS32(ncomp);
      c.add(0x3c).writePointer(comps);
      c.add(0xe8).writeU32(r.total);
      setScan(c, comps, nscan);
      c.add(0x100).writeU32(r.mcus);
      let nb = 0;
      list.slice(0, nscan).forEach(function (o) { nb += o.w * o.hh; });
      c.add(0x108).writeS32(nb);
      c.add(0x160).writePointer(methodObj(A, FDCT));
      c.add(0x164).writePointer(methodObj(A, ENCODE));
      x.rec.add(0x14).writeU32(r.fail);
      const coef = A.take(0x80, 0x66666666);
      coef.add(8).writeU32(r.imcu);
      coef.add(0xc).writeU32(r.ctr || 0);
      coef.add(0x10).writeS32(r.voff || 0);
      coef.add(0x14).writeS32(r.rows);
      list.forEach(function (o, ci) {
        const pad = o.h > 0 ? Math.ceil(o.wib / o.h) * o.h : o.wib;
        const len = Math.max(pad, r.mcus * o.w, 1) + 1;
        const nrows = (r.imcu + 1) * o.v + 1;
        const va = A.take(0x10);
        const arr = A.take(nrows * 4);
        for (let k = 0; k < nrows; k++) {
          const row = A.take(len * 0x80, 0x3c3c3c3c);
          for (let b = 0; b < len; b++) row.add(b * 0x80).writeS16(ci * 1000 + k * 100 + b);
          arr.add(k * 4).writePointer(row);
        }
        va.writePointer(arr);
        coef.add(0x40 + ci * 4).writePointer(va);
      });
      c.add(0x150).writePointer(coef);
      const inb = A.take(0x10);
      for (let k = 0; k < 4; k++) inb.add(k * 4).writePointer(A.base.add(0x40 + k * 0x10));
      fx['c' + i] = c;
      fx['i' + i] = inb;
      if (i === 0) fx.obj = c;
    });
    return fx;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ---- start_pass_main 0x004b05f0 ------------------------------------------------------------------------------
    c3cv_spm() {
      const A = arena('spm');
      const fx = { arena: A.base };
      SPM_ROWS.forEach(function (r, i) {
        const x = mkCinfo(A);
        x.c.add(0xa8).writeS32(r[0]);
        x.c.add(0x148).writePointer(A.take(0x40, r[1]));
        fx['c' + i] = x.c;
        if (i === 0) fx.obj = x.c;
      });
      return fx;
    },

    // ---- process_data_simple_main 0x004b0640 ----------------------------------------------------------------------
    // main controller (0x40 bytes, pre-filled 0x33 so the buffer pointer main+0x18 and the untouched fields show),
    // prep / coef method objects, in_row_ctr in its own dword.
    c3cv_pdsm() {
      const A = arena('pdsm');
      const fx = { arena: A.base, ib: A.base.add(0x20) };
      PDSM_ROWS.forEach(function (r, i) {
        const x = mkCinfo(A);
        const c = x.c;
        c.add(0xe8).writeU32(r.total);
        const main = A.take(0x40, 0x33333333);
        main.add(8).writeU32(r.cur);
        main.add(0xc).writeU32(r.rg);
        main.add(0x10).writeS32(r.susp);
        c.add(0x148).writePointer(main);
        c.add(0x14c).writePointer(methodObj(A, PREP));
        c.add(0x150).writePointer(methodObj(A, COMPRESS));
        x.rec.add(0x14).writeU32(r.fail);
        x.rec.add(0x1c).writeU32(r.steps);
        const ir = A.take(0x10);
        ir.writeU32(r.inCtr);
        fx['c' + i] = c;
        fx['ir' + i] = ir;
        if (i === 0) fx.obj = c;
      });
      return fx;
    },

    // ---- start_pass_coef 0x004b07b0 -------------------------------------------------------------------------------
    c3cv_spc() {
      const A = arena('spc');
      const fx = { arena: A.base };
      SPC_ROWS.forEach(function (r, i) {
        const x = mkCinfo(A);
        const c = x.c;
        const coef = A.take(0x80, 0x66666666);
        coef.add(0x40).writePointer(r[1] ? A.base.add(0x10) : ptr(0));
        c.add(0x150).writePointer(coef);
        const comps = mkComps(A, [{ v: r[4], lrh: r[5] }, { v: 3, lrh: 2 }, { v: 1, lrh: 1 }]);
        setScan(c, comps, r[2]);
        c.add(0xe8).writeU32(r[3]);
        fx['c' + i] = c;
        if (i === 0) fx.obj = c;
      });
      return fx;
    },

    // ---- compress_data 0x004b08b0 ---------------------------------------------------------------------------------
    // Per vector: coef with MCU_buffer[0..9] pointing at ten consecutive 0x80-byte blocks pre-filled 0x5a5a, the
    // scan's components, forward_DCT and encode_mcu recorders.
    c3cv_cd() {
      const A = arena('cd');
      const fx = { arena: A.base };
      CD_ROWS.forEach(function (r, i) {
        const x = mkCinfo(A);
        const c = x.c;
        const comps = mkComps(A, r.comps.map(function (o) { return { w: o.w, hh: o.hh, lcw: o.lcw, lrh: o.lrh, sw: o.sw, v: o.v }; }));
        setScan(c, comps, r.nscan === undefined ? r.comps.length : r.nscan);
        c.add(0xe8).writeU32(r.total);
        c.add(0x100).writeU32(r.mcus);
        let nb = 0;
        r.comps.forEach(function (o) { nb += o.w * o.hh; });
        c.add(0x108).writeS32(nb);
        c.add(0x160).writePointer(methodObj(A, FDCT));
        c.add(0x164).writePointer(methodObj(A, ENCODE));
        x.rec.add(0x14).writeU32(r.fail);
        const coef = A.take(0x80, 0x66666666);
        coef.add(8).writeU32(r.imcu);
        coef.add(0xc).writeU32(r.ctr);
        coef.add(0x10).writeS32(r.voff);
        coef.add(0x14).writeS32(r.rows);
        const ws = A.take(10 * 0x80, 0x5a5a5a5a);
        for (let b = 0; b < 10; b++) coef.add(0x18 + b * 4).writePointer(ws.add(b * 0x80));
        c.add(0x150).writePointer(coef);
        const inb = A.take(0x10);
        for (let k = 0; k < 4; k++) inb.add(k * 4).writePointer(A.base.add(0x40 + k * 0x10));
        fx['c' + i] = c;
        fx['i' + i] = inb;
        if (i === 0) fx.obj = c;
      });
      return fx;
    },

    // ---- compress_first_pass 0x004b0b20 / compress_output 0x004b0d70 ----------------------------------------------
    c3cv_cfp() { return coefFixture('cfp', CFP_ROWS); },
    c3cv_co() { return coefFixture('co', CO_ROWS); },

    // ---- jinit_phuff_encoder 0x004b1f30 ---------------------------------------------------------------------------
    // Ten compress objects whose pools are pre-filled with different words and whose cursors start at different
    // offsets, so the cleared fields and the stored pointer differ per vector; cinfo+0x164 pre-set 0x99999999.
    c3cv_jpe() {
      const A = arena('jpe');
      const fx = { arena: A.base };
      for (let i = 0; i < 10; i++) {
        const x = mkCinfo(A);
        const mem = mkMem(A, 0x200);
        const pool = mem.add(0x30).readPointer();
        fill(pool.add(0x10), 0x200, (0x11111111 * ((i % 15) + 1)) >>> 0);
        pool.writeU32(i * 0x10);
        x.c.add(4).writePointer(mem);
        x.c.add(0x164).writeU32(0x99999999);
        fx['c' + i] = x.c;
        if (i === 0) fx.obj = x.c;
      }
      return fx;
    },

    // ---- start_pass_phuff 0x004b1f70 ------------------------------------------------------------------------------
    c3cv_spp() {
      const A = arena('spp');
      const fx = { arena: A.base };
      SPP_ROWS.forEach(function (r, i) {
        const x = mkCinfo(A, r.fix);
        const c = x.c;
        const err = c.readPointer();
        if (r.fix) {
          err.add(0x40).writePointer(huffTbl(A, BITS[2], 33));
          err.add(0x44).writeU32(r.Ss === 0 ? 1 : 0);
        }
        c.add(4).writePointer(mkMem(A, 0x1800));
        c.add(0xc0).writeU32(r.ri || 0);
        c.add(0x134).writeS32(r.Ss);
        c.add(0x13c).writeS32(r.Ah);
        const comps = mkComps(A, r.tbls.map(function (t) { return { dc: t[0], ac: t[1] }; }));
        setScan(c, comps, r.tbls.length);
        for (let s = 0; s < 4; s++) {
          if (r.dc.indexOf(s) >= 0) c.add(0x50 + s * 4).writePointer(huffTbl(A, BITS[s], 5 + s));
          if (r.ac.indexOf(s) >= 0) c.add(0x60 + s * 4).writePointer(huffTbl(A, BITS[3 - s], 60 + s));
        }
        if (r.x74) c.add(0x74).writePointer(huffTbl(A, BITS[1], 77));
        // 0x80 bytes: an AC number of 4 makes the gather branch use the dword at +0x6c, one past count_ptrs[3].
        const e = A.take(0x80, 0x55555555);
        e.add(0x6c).writePointer(ptr(0));
        e.add(0x40).writePointer(r.bitbuf ? A.take(0x3f0, 0x01000100) : ptr(0));
        for (let t = 0; t < 4; t++) {
          const pd = (r.preDerived || []).indexOf(t) >= 0, pc = (r.preCount || []).indexOf(t) >= 0;
          e.add(0x4c + t * 4).writePointer(pd ? A.take(0x500, 0x77777777) : ptr(0));
          e.add(0x5c + t * 4).writePointer(pc ? A.take(0x404, 0x77777777) : ptr(0));
        }
        c.add(0x164).writePointer(e);
        fx['c' + i] = c;
        if (i === 0) fx.obj = c;
      });
      return fx;
    },

    // ---- the encoders ---------------------------------------------------------------------------------------------
    c3cv_dcf() { return encFixture('dcf', DCF_ROWS); },
    c3cv_dcr() { return encFixture('dcr', DCR_ROWS); },
    c3cv_acf() { return encFixture('acf', ACF_ROWS); },
    c3cv_acr() { return encFixture('acr', ACR_ROWS); },
    c3cv_rst() { return encFixture('rst', RST_ROWS); },

    // ---- finish_pass_gather_phuff 0x004b33c0 ----------------------------------------------------------------------
    c3cv_fpg() {
      const A = arena('fpg');
      const fx = { arena: A.base };
      let seed = 4242;
      function rnd(n) { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; }
      FPG_ROWS.forEach(function (r, i) {
        const x = mkCinfo(A);
        const c = x.c;
        c.add(4).writePointer(mkMem(A, 4 * 0x120));
        c.add(0x134).writeS32(r.Ss);
        c.add(0x13c).writeS32(r.Ah);
        const comps = mkComps(A, r.tbls.map(function (t) { return { dc: t[0], ac: t[1] }; }));
        setScan(c, comps, r.tbls.length);
        for (let s = 0; s < 4; s++) {
          if (r.dc.indexOf(s) >= 0) c.add(0x50 + s * 4).writePointer(A.take(0x118, 0x3a3a3a3a));
          if (r.ac.indexOf(s) >= 0) c.add(0x60 + s * 4).writePointer(A.take(0x118, 0x3a3a3a3a));
        }
        const e = A.take(0x80, 0x55555555);
        e.add(0xc).writeU32(1);
        e.add(0x20).writePointer(c);
        e.add(0x34).writeS32(r.act || 0);
        e.add(0x38).writeU32(r.eobrun || 0);
        e.add(0x3c).writeU32(0);
        e.add(0x40).writePointer(A.take(0x10));
        for (let t = 0; t < 4; t++) {
          const f = A.take(0x404);
          const n = 2 + rnd(26);
          for (let k = 0; k < n; k++) f.add(rnd(256) * 4).writeU32(1 + rnd(3000));
          e.add(0x5c + t * 4).writePointer(f);
          e.add(0x4c + t * 4).writePointer(ptr(0));
        }
        c.add(0x164).writePointer(e);
        fx['c' + i] = c;
        if (i === 0) fx.obj = c;
      });
      return fx;
    },

    // ---- pre_process_data 0x004b3b80 / pre_process_context 0x004b3d40 ---------------------------------------------
    c3cv_ppd() { return prepFixture('ppd', PPD_ROWS, false); },
    c3cv_ppc() { return prepFixture('ppc', PPC_ROWS, true); },
  });
})();
