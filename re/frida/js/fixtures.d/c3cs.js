// Fixtures for C3 batch c3cs (round 22, golf_clean.exe): IJG libjpeg 6a compress-side routines (the sequential
// Huffman encoder, the scan-header marker writers, the memory manager's virtual-array access layer, jzero_far)
// and the UI list accessor listGetAt 0x004a48f0. Only our own synthetic values (no game text, no live game
// object). Names are prefixed c3cs_.
//
// Allocations go through Memory.alloc (diff_hook rewrites it to __keepAlloc so blocks referenced only from a
// field survive the run); NativeCallbacks are pushed onto __diffKeepAlive by hand. No global of the running game
// is written: every object is freshly allocated.
//
// Every callback is a pure function of its arguments or of memory diff_hook snapshots and restores between the
// two arms, so the second arm sees exactly the world the first one saw. The libjpeg error_exit records and
// RETURNS, so the instructions after an ERREXIT run in both arms (round 19's lesson).
//
// libjpeg structures (see shim/src/re/c3cs.cpp for the full offset list): cinfo+0 err (error_exit +0,
// msg_code +0x14, msg_parm.i[0] +0x18), cinfo+4 mem (alloc_small +0; our pool pointer at mem+0x10),
// cinfo+0x14 dest (next_output_byte +0, free_in_buffer +4, empty_output_buffer +0xc; our spare buffer at
// dest+0x18), dc/ac_huff_tbl_ptrs +0x50 / +0x60, arith_code +0xac, restart_interval +0xc0,
// progressive_mode +0xdc, comps_in_scan +0xec, cur_comp_info +0xf0, blocks_in_MCU +0x108, MCU_membership +0x10c,
// Ss +0x134, Se +0x138, Ah +0x13c, Al +0x140, entropy +0x164. We keep a per-cinfo recorder pointer at
// cinfo+0x170, past the end of jpeg_compress_struct (0x168).
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function zero(p, n) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(0); }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v); }
  function alloc(n, v) { const p = Memory.alloc(n); if (v === undefined) zero(p, n); else fill(p, n, v); return p; }

  // jpeg_natural_order[k] as the exe holds it (dword table at 0x004bd0ac; the encoders walk k = 1..63 from
  // 0x004bd0b0). Read-only use: blocks are filled by zig-zag index through it.
  const NAT = ptr('0x004bd0ac');
  function setZ(block, k, v) { block.add(NAT.add(k * 4).readS32() * 2).writeS16(v); }
  // A 64-coefficient block from a list of [k, value] pairs (zig-zag index k), all other coefficients 0.
  function mkBlock(pairs) {
    const b = alloc(0x80);
    pairs.forEach(function (kv) { setZ(b, kv[0], kv[1]); });
    return b;
  }

  // A rolling hash recorder: rec+0 the hash, rec+4 the number of pushes, rec+8 the last value pushed.
  function mkRec() { return alloc(0x40); }
  function push(rec, v) {
    let h = rec.readU32();
    h = (Math.imul(h, 33) + (v >>> 0)) >>> 0;
    rec.writeU32(h);
    rec.add(4).writeU32((rec.add(4).readU32() + 1) >>> 0);
    rec.add(8).writeU32(v >>> 0);
  }

  // error_exit(cinfo): records msg_code and msg_parm.i[0] in the error manager's tail (+0x30..+0x3b, which none of
  // these functions writes) and RETURNS.
  const ERR_REC = keep(new NativeCallback(function (cinfo) {
    const e = cinfo.readPointer();
    let h = e.add(0x30).readU32();
    h = (Math.imul(h, 33) + e.add(0x14).readU32()) >>> 0;
    h = (Math.imul(h, 33) + e.add(0x18).readU32()) >>> 0;
    e.add(0x30).writeU32(h);
    e.add(0x34).writeU32((e.add(0x34).readU32() + 1) >>> 0);
    e.add(0x38).writeU32(e.add(0x14).readU32());
  }, 'void', ['pointer']));
  function mkErr() {
    const e = alloc(0x84, 0x22222222);
    e.writePointer(ERR_REC);
    zero(e.add(0x30), 0x20);
    return e;
  }
  // error_exit for start_pass_huff's missing-table vectors: records like ERR_REC, then for message 0x31 with a table
  // number 0..3 installs the spare JHUFF_TBL kept at err+0x40 in whichever of cinfo+0x50 / +0x60 slot n is NULL.
  // start_pass_huff re-reads the slot after the ERREXIT (0x004b140e / 0x004b141e), so the derived-table path then
  // runs on a real table in both arms instead of dereferencing NULL.
  const ERR_FIX = keep(new NativeCallback(function (cinfo) {
    const e = cinfo.readPointer();
    let h = e.add(0x30).readU32();
    h = (Math.imul(h, 33) + e.add(0x14).readU32()) >>> 0;
    h = (Math.imul(h, 33) + e.add(0x18).readU32()) >>> 0;
    e.add(0x30).writeU32(h);
    e.add(0x34).writeU32((e.add(0x34).readU32() + 1) >>> 0);
    e.add(0x38).writeU32(e.add(0x14).readU32());
    const n = e.add(0x18).readS32();
    if (e.add(0x14).readU32() === 0x31 && n >= 0 && n < 4) {
      [0x50, 0x60].forEach(function (o) {
        if (cinfo.add(o + n * 4).readPointer().isNull()) cinfo.add(o + n * 4).writePointer(e.add(0x40).readPointer());
      });
    }
  }, 'void', ['pointer']));

  // alloc_small(cinfo, pool_id, size): a bump allocator over the pool at mem+0x10. pool+0 is the cursor (bytes
  // used past pool+0x10), pool+4 a hash of the (pool_id, size) requests; both are compared state, so both arms
  // get the same addresses in the same order. Blocks are pre-filled 0xcc by the fixture.
  const ALLOC_SMALL = keep(new NativeCallback(function (cinfo, pool, size) {
    const p = cinfo.add(4).readPointer().add(0x10).readPointer();
    const cur = p.readU32();
    p.writeU32((cur + ((size + 15) & ~15)) >>> 0);
    p.add(4).writeU32((Math.imul(p.add(4).readU32(), 33) + pool * 0x10000 + size) >>> 0);
    return p.add(0x10 + cur);
  }, 'pointer', ['pointer', 'int', 'uint32']));
  function mkMem(poolSize) {
    const mem = alloc(0x20);
    const pool = alloc(0x10 + poolSize, 0xcccccccc);
    zero(pool, 0x10);
    mem.writePointer(ALLOC_SMALL);
    mem.add(0x10).writePointer(pool);
    return { mem: mem, pool: pool };
  }

  // empty_output_buffer(cinfo): reset the destination to the spare buffer at dest+0x18 with `free` bytes and
  // record the refill; `ok` 0 makes it a suspending manager (records, returns FALSE, leaves dest alone).
  function mkEmpty(rec, free, ok) {
    return keep(new NativeCallback(function (cinfo) {
      const d = cinfo.add(0x14).readPointer();
      push(rec, (ok ? 0xe0000000 : 0xd0000000) + free);
      if (!ok) return 0;
      d.writePointer(d.add(0x18).readPointer());
      d.add(4).writeU32(free);
      return 1;
    }, 'int', ['pointer']));
  }
  // A destination manager over a `size`-byte buffer pre-filled 0x5b and a 0x40-byte spare pre-filled 0x6c.
  function mkDest(rec, free, refill, ok, size) {
    const buf = alloc(size || 0x400, 0x5b5b5b5b);
    const spare = alloc(0x40, 0x6c6c6c6c);
    const dest = alloc(0x20);
    dest.writePointer(buf);
    dest.add(4).writeU32(free);
    dest.add(0xc).writePointer(mkEmpty(rec, refill === undefined ? free : refill, ok === undefined ? 1 : ok));
    dest.add(0x18).writePointer(spare);
    return { dest: dest, buf: buf, spare: spare };
  }

  // A JHUFF_TBL: bits[1..16] at +1, huffval at +0x11 (our own permutation), sent_table at +0x114.
  function huffTbl(bits, sent, seed) {
    const h = alloc(0x118);
    let count = 0;
    for (let i = 0; i < 16; i++) { h.add(1 + i).writeU8(bits[i]); count += bits[i]; }
    for (let i = 0; i < count; i++) h.add(0x11 + i).writeU8((i * 23 + (seed || 7)) & 0xff);
    if (sent) h.add(0x114).writeU32(1);
    return h;
  }

  // A synthetic c_derived_tbl (ehufco dwords at +0, ehufsi bytes at +0x400): sizes 1..16 by formula, codes chosen
  // so that some symbols are all ones (exercising the 0xff byte stuffing). `zeros` lists symbols given size 0.
  function derivedTbl(seed, zeros) {
    const t = alloc(0x500);
    for (let s = 0; s < 256; s++) {
      const size = 1 + ((s * 7 + seed) % 16);
      const code = (s % 5 === 0) ? 0xffff : ((s * 0x9e37 + seed * 0x101) & 0xffff);
      t.add(s * 4).writeU32(code);
      t.add(0x400 + s).writeU8(size);
    }
    (zeros || []).forEach(function (s) { t.add(0x400 + s).writeU8(0); });
    return t;
  }

  // jpeg_component_info entries (0x54 bytes, pre-filled 0x99): rows = [component_id, dc_tbl_no, ac_tbl_no].
  function mkComps(rows) {
    const k = alloc(8 * 0x54, 0x99999999);
    rows.forEach(function (r, i) {
      const c = k.add(i * 0x54);
      c.writeS32(r[0]);
      c.add(0x14).writeS32(r[1]);
      c.add(0x18).writeS32(r[2]);
    });
    return k;
  }
  function setScanComps(c, comps, n) {
    c.add(0xec).writeS32(n);
    for (let i = 0; i < 4; i++) c.add(0xf0 + i * 4).writePointer(comps.add(i * 0x54));
  }

  // The fourteen sample blocks shared by the encoder fixtures (read-only).
  function stdBlocks() {
    const all = [];
    for (let k = 1; k < 64; k++) all.push([k, ((k * 37) % 61) * ((k & 1) ? 1 : -1) || 3]);
    return [
      mkBlock([]),                                                       // 0: all zero -> EOB only
      mkBlock([[0, 100], [1, 5], [2, -3], [63, 1]]),                     // 1: ends on a coefficient, no EOB
      mkBlock([[0, -2048], [20, 7], [40, -1]]),                          // 2: runs of 18 and 19 (one ZRL each)
      mkBlock([[0, 32767]].concat(all)),                                 // 3: every AC non-zero
      mkBlock([[0, -32768], [50, -32768]]),                              // 4: run 49 (three ZRLs), nbits 16
      mkBlock([[0, 1], [33, 1], [63, 2]]),                               // 5: run 32 (two ZRLs), then 29
      mkBlock([[0, -1], [16, 1], [33, -255]]),                           // 6: run 15 (no ZRL), then 16 (one)
      mkBlock([[0, 0xff], [1, 0xff], [2, -0x100], [3, 0x7f]]),           // 7: 0xff-heavy value bits
      mkBlock([[0, 7], [63, -1]]),                                       // 8: run 62 (three ZRLs) at k = 63
      mkBlock([[0, 0x400], [5, 0x3ff], [6, -0x3ff], [7, 1], [8, -1]]),   // 9: mixed magnitudes
      mkBlock([[0, -100], [1, 1], [17, 1], [34, 1], [51, 1]]),           // 10: runs 15, 16, 16
      mkBlock([[0, 12345], [2, -2], [4, 4], [8, -8], [16, 16]]),         // 11
      mkBlock([[0, 0], [62, 1]]),                                        // 12: run 61 then EOB
      mkBlock([[0, -32767], [1, 32767]]),                                // 13
    ];
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ---- jzero_far 0x004b0540 ----------------------------------------------------------------------------------
    // One 0x110-byte buffer pre-filled 0xa5 (restored between vectors) and three misaligned starts into it.
    c3cs_zero() {
      const buf = alloc(0x110, 0xa5a5a5a5);
      return { obj: buf, buf: buf, b1: buf.add(1), b3: buf.add(3), b6: buf.add(6) };
    },

    // ---- listGetAt 0x004a48f0 ----------------------------------------------------------------------------------
    // List objects of 0x90 bytes: head +0x80, cursor +0x84, count +0x88, cursor index +0x8c. Nodes (0x14 bytes)
    // carry a value at +4 and are linked circularly forward at +0xc and backward at +0x10, so walks past either end
    // wrap. o0: 5 nodes, count 5. o1: 3 nodes but count 5 (walks wrap). o2: no head, count 0, cursor on a lone
    // node. o3: 1 node, count 1. Each object's cursor starts on a node that is not the head and its index at 0x77.
    c3cs_list() {
      function mkList(nNodes, count, base) {
        const o = alloc(0x90, 0x44444444);
        const nodes = [];
        for (let i = 0; i < nNodes; i++) {
          const n = alloc(0x14, 0x33333333);
          n.add(4).writeU32(base + i * 0x11);
          nodes.push(n);
        }
        for (let i = 0; i < nNodes; i++) {
          nodes[i].add(0xc).writePointer(nodes[(i + 1) % nNodes]);
          nodes[i].add(0x10).writePointer(nodes[(i + nNodes - 1) % nNodes]);
        }
        const lone = alloc(0x14, 0x33333333);
        lone.add(4).writeU32(base + 0xff);
        o.add(0x80).writePointer(nNodes ? nodes[0] : ptr(0));
        o.add(0x84).writePointer(nNodes > 1 ? nodes[nNodes - 1] : lone);
        o.add(0x88).writeS32(count);
        o.add(0x8c).writeS32(0x77);
        return o;
      }
      const o0 = mkList(5, 5, 0x1000), o1 = mkList(3, 5, 0x2000), o2 = mkList(0, 0, 0x3000), o3 = mkList(1, 1, 0x4000);
      return { obj: o0, o0: o0, o1: o1, o2: o2, o3: o3 };
    },

    // ---- htest_one_block 0x004b1d80 ----------------------------------------------------------------------------
    // The fourteen sample blocks plus a DC count array and an AC count array (257 dwords each, pre-filled with our
    // own small counts so increments show), both restored between vectors.
    c3cs_htest() {
      const fx = {};
      stdBlocks().forEach(function (b, i) { fx['k' + i] = b; });
      const dc = alloc(0x404), ac = alloc(0x404);
      for (let i = 0; i < 0x101; i++) { dc.add(i * 4).writeU32(i * 3); ac.add(i * 4).writeU32(0x100 + i); }
      fx.obj = dc; fx.dc = dc; fx.ac = ac;
      return fx;
    },

    // ---- jpeg_make_c_derived_tbl 0x004b0f60 --------------------------------------------------------------------
    // A compress object whose alloc_small hands out pool blocks, six JHUFF_TBLs and two *pdtbl slots: `snull` is
    // NULL (the function allocates, 0x004b0f74), `spre` points at an existing 0x500-byte table pre-filled 0x77
    // whose ehufco entries for unused symbols must survive.
    c3cs_derived() {
      const m = mkMem(0x600);
      const c = alloc(0x180);
      c.writePointer(mkErr());
      c.add(4).writePointer(m.mem);
      const pre = alloc(0x500, 0x77777777);
      const snull = alloc(4), spre = alloc(4);
      spre.writePointer(pre);
      const fx = { obj: c, c: c, pool: m.pool, pre: pre, snull: snull, spre: spre };
      const BITS = [
        [0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0],                // 12 codes, starting at length 2
        [0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125],               // 162 codes up to length 16
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],                 // empty: no code at all
        [0, 0, 0, 0, 0, 0, 0, 255, 1, 0, 0, 0, 0, 0, 0, 0],               // 256 codes (huffval repeats symbols)
        [1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3],                 // gaps between lengths
        [2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],                 // two 1-bit codes
      ];
      BITS.forEach(function (b, i) { fx['h' + i] = huffTbl(b, false, 7 + i * 40); });
      return fx;
    },

    // ---- jpeg_gen_optimal_table 0x004b1080 ---------------------------------------------------------------------
    // Twelve frequency arrays (257 dwords; the function writes freq[256] and merges the counts in place), each with
    // its own JHUFF_TBL pre-filled 0x5c, over one compress object with the recording error manager. f4 / f5 / f6 /
    // f7 make maximum code lengths 20, 30, 32 and 31, folded down to 16 (0x004b11d1). No set makes a length above
    // 32: after a returning ERREXIT 0x26 (0x004b11b1) the original's fold never terminates (model and argument in
    // log/c3/c3cs_genopt.py), so that side of 0x004b11ac is left to the real, non-returning error_exit.
    c3cs_genopt() {
      const err = mkErr();
      const c = alloc(0x180);
      c.writePointer(err);
      const fx = { obj: c, c: c, err: err };
      function fib(n) { const o = []; let a = 1, b = 2; for (let i = 0; i < n; i++) { o.push(a); const t = a + b; a = b; b = t; } return o; }
      let seed = 12345;
      function rnd(n) { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; }
      const sets = [];
      sets.push([[5, 10]]);                                                            // f0: one symbol
      sets.push([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11].map(function (s) { return [s, 120 - s * 9]; }));   // f1
      const uni = []; for (let s = 0; s < 256; s++) uni.push([s, 1]); sets.push(uni);  // f2: all ties
      const r3 = []; for (let s = 0; s < 256; s++) r3.push([s, 1 + rnd(1000)]); sets.push(r3);           // f3
      sets.push(fib(20).map(function (v, i) { return [i * 3, v]; }));                  // f4: lengths up to 20
      sets.push(fib(30).map(function (v, i) { return [255 - i * 2, v]; }));            // f5: lengths up to 30
      sets.push(fib(32).map(function (v, i) { return [i, v]; }));                      // f6: length 32
      sets.push(fib(31).map(function (v, i) { return [i * 7, v]; }));                  // f7: length 31
      sets.push([[1, 2000000000], [2, 5], [3, 5], [4, 1000000001], [9, 7]]);           // f8: counts above 1e9
      sets.push([[0, 3], [255, 3]]);                                                   // f9: first and last
      const r10 = []; for (let i = 0; i < 40; i++) r10.push([rnd(256), 1 + rnd(50)]); sets.push(r10);   // f10
      sets.push([[17, 1], [18, 1], [19, 1], [20, 2], [21, 2]]);                         // f11: ties with freq[256]
      sets.forEach(function (pairs, i) {
        const f = alloc(0x404);
        pairs.forEach(function (p) { f.add(p[0] * 4).writeU32(p[1]); });
        f.add(0x400).writeU32(0x5555);                                                 // overwritten with 1
        fx['f' + i] = f;
        fx['h' + i] = alloc(0x118, 0x5c5c5c5c);
      });
      return fx;
    },

    // ---- emit_dri 0x004b01f0 -----------------------------------------------------------------------------------
    // Eight compress objects with restart_interval 0, 1, 4, 0x1234, 0xffff, 0x10000, -1 and 0x7fffffff over one
    // shared roomy destination (emit_2bytes keeps only the low 16 bits).
    c3cs_dri() {
      const rec = mkRec();
      const d = mkDest(rec, 0x400);
      const err = mkErr();
      const fx = { obj: err, err: err, dest: d.dest, buf: d.buf, spare: d.spare, rec: rec };
      [0, 1, 4, 0x1234, 0xffff, 0x10000, -1, 0x7fffffff].forEach(function (ri, i) {
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(0x14).writePointer(d.dest);
        c.add(0xc0).writeS32(ri);
        fx['c' + i] = c;
      });
      return fx;
    },
    // The same objects over a destination with 1 free byte that refills 1 at a time, so emit_byte's
    // empty_output_buffer (0x004afbcc) runs for every byte.
    c3cs_dri_tight() {
      const rec = mkRec();
      const d = mkDest(rec, 1, 1);
      const err = mkErr();
      const fx = { obj: err, err: err, dest: d.dest, buf: d.buf, spare: d.spare, rec: rec };
      [0x1234, 0xffff, 7, 0x10203].forEach(function (ri, i) {
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(0x14).writePointer(d.dest);
        c.add(0xc0).writeS32(ri);
        fx['c' + i] = c;
      });
      return fx;
    },

    // ---- emit_sos 0x004b0220 -----------------------------------------------------------------------------------
    // Twelve compress objects over one shared destination. Four components (id, dc_tbl_no, ac_tbl_no) =
    // (1,0,0) (2,1,1) (3,1,0) (0x7f,3,2). rows = [comps_in_scan, progressive, Ss, Se, Ah, Al, arith_code].
    c3cs_sos() {
      const rec = mkRec();
      const d = mkDest(rec, 0x400);
      const err = mkErr();
      const comps = mkComps([[1, 0, 0], [2, 1, 1], [3, 1, 0], [0x7f, 3, 2]]);
      const fx = { obj: err, err: err, dest: d.dest, buf: d.buf, spare: d.spare, rec: rec };
      const rows = [
        [1, 0, 0, 63, 0, 0, 0],      // sequential, one component
        [3, 0, 0, 63, 0, 0, 0],
        [4, 0, 0, 63, 0, 0, 0],
        [0, 0, 0, 63, 0, 0, 0],      // no component: the loop at 0x004b025d is skipped
        [3, 1, 0, 0, 0, 0, 0],       // progressive first DC scan: ta = 0
        [3, 1, 0, 0, 1, 0, 0],       // DC refinement without arithmetic coding: td = ta = 0
        [3, 1, 0, 0, 1, 0, 1],       // DC refinement with arith_code: td kept
        [1, 1, 1, 5, 0, 2, 0],       // AC scan: td = 0
        [1, 1, 6, 63, 2, 1, 0],
        [2, 0, 0, 63, 3, 15, 0],     // (Ah << 4) + Al = 0x3f
        [4, 1, 0, 0, 0, 13, 1],      // first DC scan with arith_code
        [2, 0, 3, 0x1ff, 0x11, 0x22, 0],   // values above a byte: emit_byte keeps the low 8 bits
      ];
      rows.forEach(function (r, i) {
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(0x14).writePointer(d.dest);
        setScanComps(c, comps, r[0]);
        c.add(0xdc).writeS32(r[1]);
        c.add(0x134).writeS32(r[2]);
        c.add(0x138).writeS32(r[3]);
        c.add(0x13c).writeS32(r[4]);
        c.add(0x140).writeS32(r[5]);
        c.add(0xac).writeS32(r[6]);
        fx['c' + i] = c;
      });
      return fx;
    },

    // ---- write_scan_header 0x004b0060 --------------------------------------------------------------------------
    // Ten compress objects over one shared destination; each owns all eight Huffman tables (emit_dht 0x004b0120
    // sets sent_table at +0x114, so they are state). rows = [arith, progressive, Ss, Ah, comps_in_scan,
    // restart_interval, [dc slots already sent], [ac slots already sent]]. Components as in c3cs_sos; table
    // numbers repeat across components, so the second emit_dht of a table finds it sent.
    c3cs_wsh() {
      const rec = mkRec();
      const d = mkDest(rec, 0x1000, 0x40, 1, 0x1000);
      const err = mkErr();
      const comps = mkComps([[1, 0, 0], [2, 1, 1], [3, 1, 0], [0x7f, 3, 2]]);
      const fx = { obj: err, err: err, dest: d.dest, buf: d.buf, spare: d.spare, rec: rec };
      const BITS = [
        [0, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        [1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        [0, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        [0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1]];
      const rows = [
        [1, 0, 0, 0, 3, 0, [], []],          // arith_code: only the empty method, then SOS
        [1, 0, 0, 0, 3, 5, [], []],          // arith_code with a restart interval: DRI too
        [0, 0, 0, 0, 1, 0, [], []],          // sequential, one component
        [0, 0, 0, 0, 3, 0x40, [], []],
        [0, 1, 0, 0, 3, 0, [], []],          // progressive first DC scan: DC tables only
        [0, 1, 0, 1, 3, 0, [], []],          // DC refinement: no table
        [0, 1, 5, 0, 1, 2, [], []],          // AC scan: the AC table only
        [0, 0, 0, 0, 2, 0, [0], [1]],        // some tables already sent
        [0, 0, 0, 0, 0, 0, [], []],          // no component
        [0, 1, 0, 0, 4, 0x10001, [3], []],   // four components, one table sent
      ];
      rows.forEach(function (r, i) {
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(0x14).writePointer(d.dest);
        c.add(0xac).writeS32(r[0]);
        c.add(0xdc).writeS32(r[1]);
        c.add(0x134).writeS32(r[2]);
        c.add(0x138).writeS32(r[2] ? 63 : 0);
        c.add(0x13c).writeS32(r[3]);
        c.add(0x140).writeS32(1);
        setScanComps(c, comps, r[4]);
        c.add(0xc0).writeS32(r[5]);
        for (let s = 0; s < 4; s++) {
          const dh = huffTbl(BITS[s], r[6].indexOf(s) >= 0, 3 + s);
          const ah = huffTbl(BITS[3 - s], r[7].indexOf(s) >= 0, 90 + s);
          c.add(0x50 + s * 4).writePointer(dh);
          c.add(0x60 + s * 4).writePointer(ah);
          fx['d' + i + '_' + s] = dh;
          fx['a' + i + '_' + s] = ah;
        }
        fx['c' + i] = c;
      });
      return fx;
    },

    // ---- encode_one_block 0x004b15c0 / emit_restart 0x004b1ae0 -------------------------------------------------
    // Per vector: a working_state (next_output_byte +0, free_in_buffer +4, put_buffer +8, put_bits +0xc,
    // last_dc_val[4] +0x10, cinfo +0x20) over its own 0x200-byte output buffer, a compress object with the
    // recording error manager, a destination manager whose empty_output_buffer refills (or suspends) and a recorder.
    // Shared: the fourteen sample blocks and two derived tables (dc: seed 3, ac: seed 11).
    // r = { blk, dc (last_dc_val), pb (put_buffer), bits (put_bits), free, refill, ok, comps, num,
    //       dz / az (symbols of the dc / ac table given size 0) }
    _c3csWs(rows) {
      const fx = {};
      const blocks = stdBlocks();
      blocks.forEach(function (b, i) { fx['k' + i] = b; });
      const shared = { dc: derivedTbl(3), ac: derivedTbl(11) };
      fx.dt = shared.dc; fx.at = shared.ac;
      rows.forEach(function (r, i) {
        const rec = mkRec();
        const free = r.free === undefined ? 0x200 : r.free;
        const d = mkDest(rec, free, r.refill === undefined ? 0x40 : r.refill, r.ok === undefined ? 1 : r.ok, 0x200);
        const err = mkErr();
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(0x14).writePointer(d.dest);
        c.add(0xec).writeS32(r.comps === undefined ? 3 : r.comps);
        const ws = alloc(0x24);
        ws.writePointer(d.buf);
        ws.add(4).writeU32(free);
        ws.add(8).writeU32(r.pb === undefined ? 0 : r.pb);
        ws.add(0xc).writeS32(r.bits === undefined ? 0 : r.bits);
        for (let j = 0; j < 4; j++) ws.add(0x10 + j * 4).writeS32(0x1110 * (j + 1));
        ws.add(0x20).writePointer(c);
        fx['s' + i] = ws;
        fx['q' + i] = blocks[r.blk || 0];
        fx['b' + i] = d.buf;
        fx['p' + i] = d.spare;
        fx['d' + i] = d.dest;
        fx['e' + i] = err;
        fx['r' + i] = rec;
        fx['dt' + i] = r.dz ? derivedTbl(3, r.dz) : shared.dc;
        fx['at' + i] = r.az ? derivedTbl(11, r.az) : shared.ac;
        if (i === 0) fx.obj = ws;
      });
      return fx;
    },
    // Roomy buffer: every block shape, positive / negative / large DC differences, and leftover bits in the
    // bit buffer on entry.
    c3cs_eob() {
      return this._c3csWs(C3CS_EOB_ROWS);
    },
    // A buffer with a few free bytes that refills 2 at a time: dump_buffer 0x004b19e0 runs at each inlined
    // emit_bits copy (and inside emit_bits 0x004b1a10 for the value and EOB codes).
    c3cs_eob_tight() {
      return this._c3csWs(C3CS_EOB_TIGHT_ROWS);
    },
    // A suspending destination: free bytes 1..n before the first refill, which returns FALSE, so each vector
    // aborts at a different emit (every `je 0x004b19c4`).
    c3cs_eob_suspend() {
      return this._c3csWs(C3CS_EOB_SUSPEND_ROWS);
    },
    // Size-0 table entries: ERREXIT 0x27 from the DC symbol copy (0x004b1612), the ZRL copy (0x004b17b0), the AC
    // symbol copy (0x004b18c2), and emit_bits' own check for the EOB symbol 0.
    c3cs_eob_zero() {
      return this._c3csWs(C3CS_EOB_ZERO_ROWS);
    },
    // emit_restart: put_bits 0..7 on entry (flush_bits 0x004b1b70 pads with ones), restart numbers 0..7 plus
    // values outside, comps_in_scan 0..4 (last_dc_val cleared per component).
    c3cs_rst() {
      return this._c3csWs(C3CS_RST_ROWS);
    },
    // emit_restart with a suspending destination: flush_bits fails, the 0xff byte fails (0x004b1b12) or the
    // marker byte fails (0x004b1b3a); and with a refilling one free byte at a time.
    c3cs_rst_tight() {
      return this._c3csWs(C3CS_RST_TIGHT_ROWS);
    },

    // ---- encode_mcu_huff 0x004b1480 / encode_mcu_gather 0x004b1ca0 ---------------------------------------------
    // Per vector: a compress object with its own destination (roomy, or suspending after `free` bytes), its own
    // huff_entropy_encoder (0x6c bytes; saved put_buffer +0xc, put_bits +0x10, last_dc_val +0x14,
    // restarts_to_go +0x24, next_restart_num +0x28, derived tables +0x2c / +0x3c, count arrays +0x4c / +0x5c),
    // four components (dc, ac table numbers (0,0) (1,1) (1,0) (3,2)), MCU_membership and an MCU_data array of
    // block pointers taken from the sample blocks. Count arrays (gather) are 257 dwords pre-filled with our own
    // counts.
    // r = { blocks: [[component, sample block]...], ri, rtg, nrn, pb, bits, free, ok, comps }
    _c3csMcu(rows) {
      const fx = {};
      const blocks = stdBlocks();
      const comps = mkComps([[1, 0, 0], [2, 1, 1], [3, 1, 0], [0x7f, 3, 2]]);
      const dts = [derivedTbl(3), derivedTbl(5), derivedTbl(9), derivedTbl(13)];
      const ats = [derivedTbl(11), derivedTbl(2), derivedTbl(6), derivedTbl(15)];
      rows.forEach(function (r, i) {
        const rec = mkRec();
        const free = r.free === undefined ? 0x400 : r.free;
        const d = mkDest(rec, free, 0x40, r.ok === undefined ? 1 : r.ok, 0x400);
        const err = mkErr();
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(0x14).writePointer(d.dest);
        setScanComps(c, comps, r.comps === undefined ? 4 : r.comps);
        c.add(0xc0).writeS32(r.ri || 0);
        c.add(0x108).writeS32(r.blocks.length);
        const md = alloc(0x28);
        r.blocks.forEach(function (bl, j) {
          c.add(0x10c + j * 4).writeS32(bl[0]);
          md.add(j * 4).writePointer(blocks[bl[1]]);
        });
        const ent = alloc(0x6c, 0x55555555);
        ent.add(0xc).writeU32(r.pb || 0);
        ent.add(0x10).writeS32(r.bits || 0);
        for (let j = 0; j < 4; j++) ent.add(0x14 + j * 4).writeS32(0x101 * (j + 1) - 0x200);
        ent.add(0x24).writeS32(r.rtg || 0);
        ent.add(0x28).writeS32(r.nrn || 0);
        const counts = [];
        for (let t = 0; t < 4; t++) {
          ent.add(0x2c + t * 4).writePointer(dts[t]);
          ent.add(0x3c + t * 4).writePointer(ats[t]);
          const dcn = alloc(0x404), acn = alloc(0x404);
          for (let s = 0; s < 0x101; s++) { dcn.add(s * 4).writeU32(t * 0x1000 + s); acn.add(s * 4).writeU32(t * 0x2000 + s * 2); }
          ent.add(0x4c + t * 4).writePointer(dcn);
          ent.add(0x5c + t * 4).writePointer(acn);
          counts.push(dcn, acn);
        }
        c.add(0x164).writePointer(ent);
        fx['c' + i] = c;
        fx['m' + i] = md;
        fx['n' + i] = ent;
        fx['b' + i] = d.buf;
        fx['p' + i] = d.spare;
        fx['d' + i] = d.dest;
        fx['e' + i] = err;
        fx['r' + i] = rec;
        counts.forEach(function (p, j) { fx['t' + i + '_' + j] = p; });
        if (i === 0) fx.obj = c;
      });
      return fx;
    },
    c3cs_mcu() { return this._c3csMcu(C3CS_MCU_ROWS); },
    c3cs_mcu_suspend() { return this._c3csMcu(C3CS_MCU_SUSPEND_ROWS); },
    c3cs_gather() { return this._c3csMcu(C3CS_GATHER_ROWS); },

    // ---- finish_pass_gather 0x004b1e20 -------------------------------------------------------------------------
    // Per vector: a compress object whose alloc_small hands out pool blocks (jpeg_alloc_huff_table 0x004afad0 asks
    // for 0x118 bytes), the four components, and an entropy coder whose eight count arrays hold our own frequency
    // sets (jpeg_gen_optimal_table 0x004b1080 rewrites them). rows = { comps, dcSlots, acSlots (slots that already
    // hold a table, pre-filled 0x3a), tbls: [[dc, ac]...] per component }.
    c3cs_fpg() {
      const fx = {};
      let seed = 777;
      function rnd(n) { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; }
      C3CS_FPG_ROWS.forEach(function (r, i) {
        const m = mkMem(0x8 * 0x120);
        const err = mkErr();
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(4).writePointer(m.mem);
        const comps = mkComps(r.tbls.map(function (t, j) { return [j + 1, t[0], t[1]]; }).concat([[9, 0, 0], [9, 0, 0], [9, 0, 0], [9, 0, 0]]).slice(0, 4));
        setScanComps(c, comps, r.tbls.length);
        for (let s = 0; s < 4; s++) {
          if (r.dcSlots.indexOf(s) >= 0) { const h = alloc(0x118, 0x3a3a3a3a); c.add(0x50 + s * 4).writePointer(h); fx['hd' + i + '_' + s] = h; }
          if (r.acSlots.indexOf(s) >= 0) { const h = alloc(0x118, 0x3a3a3a3a); c.add(0x60 + s * 4).writePointer(h); fx['ha' + i + '_' + s] = h; }
        }
        const ent = alloc(0x6c, 0x55555555);
        for (let t = 0; t < 8; t++) {
          const f = alloc(0x404);
          const n = 2 + rnd(40);
          for (let k = 0; k < n; k++) f.add(rnd(256) * 4).writeU32(1 + rnd(3000));
          ent.add(0x4c + t * 4).writePointer(f);
          fx['f' + i + '_' + t] = f;
        }
        c.add(0x164).writePointer(ent);
        fx['c' + i] = c;
        fx['pool' + i] = m.pool;
        fx['e' + i] = err;
        if (i === 0) fx.obj = c;
      });
      return fx;
    },

    // ---- start_pass_huff 0x004b12f0 ----------------------------------------------------------------------------
    // Per vector: a compress object with a pool allocator, its own entropy coder (pre-filled 0x55; count-array and
    // derived-table slots NULL or pre-set to blocks pre-filled 0x66) and Huffman tables in the slots `dc` / `ac`
    // list. rows = { gather, ri, tbls: [[dc, ac]...], dc, ac, preCount, preDerived }. Out-of-range table numbers
    // (4, -1) raise ERREXIT1 0x31 and then index the neighbouring slots exactly as the original does: in the
    // derived mode they are only used where the neighbouring cinfo slot holds a valid table.
    c3cs_sph() {
      const fx = {};
      const BITS = [
        [0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0],
        [0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 20],
        [1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0],
        [0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]];
      C3CS_SPH_ROWS.forEach(function (r, i) {
        const m = mkMem(0x2900);
        const err = mkErr();
        if (r.fix) {
          err.writePointer(ERR_FIX);
          const spare = huffTbl(BITS[2], false, 33);
          err.add(0x40).writePointer(spare);
          fx['spare' + i] = spare;
        }
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(4).writePointer(m.mem);
        c.add(0xc0).writeS32(r.ri || 0);
        const comps = mkComps(r.tbls.map(function (t, j) { return [j + 1, t[0], t[1]]; }).concat([[9, 0, 0], [9, 0, 0], [9, 0, 0], [9, 0, 0]]).slice(0, 4));
        setScanComps(c, comps, r.tbls.length);
        for (let s = 0; s < 4; s++) {
          if (r.dc.indexOf(s) >= 0) c.add(0x50 + s * 4).writePointer(huffTbl(BITS[s], false, 5 + s));
          if (r.ac.indexOf(s) >= 0) c.add(0x60 + s * 4).writePointer(huffTbl(BITS[3 - s], false, 60 + s));
        }
        // 0x80 bytes: an AC table number of 4 makes the gather branch use the dword at +0x6c, one past
        // ac_count_ptrs[3]; `slack` presets it to a block of our own.
        const ent = alloc(0x80, 0x55555555);
        if (r.slack) { const b = alloc(0x404, 0x66666666); ent.add(0x6c).writePointer(b); fx['x' + i + '_6c'] = b; }
        for (let t = 0; t < 4; t++) {
          ['2c', '3c', '4c', '5c'].forEach(function (o, j) {
            const pre = (j < 2) ? (r.preDerived || []) : (r.preCount || []);
            if (pre.indexOf(t) >= 0) {
              const b = alloc(j < 2 ? 0x500 : 0x404, 0x66666666);
              ent.add(parseInt(o, 16) + t * 4).writePointer(b);
              fx['x' + i + '_' + o + '_' + t] = b;
            } else {
              ent.add(parseInt(o, 16) + t * 4).writePointer(ptr(0));
            }
          });
        }
        c.add(0x164).writePointer(ent);
        fx['c' + i] = c;
        fx['n' + i] = ent;
        fx['pool' + i] = m.pool;
        fx['e' + i] = err;
        if (i === 0) fx.obj = c;
      });
      return fx;
    },

    // ---- expand_bottom_edge 0x004b3d00 -------------------------------------------------------------------------
    // An image of eight 16-byte rows (row i byte j = i * 0x10 + j + 1) behind a row-pointer array.
    c3cs_ebe() {
      const rows = alloc(8 * 0x10);
      const arr = alloc(8 * 4);
      for (let i = 0; i < 8; i++) {
        for (let j = 0; j < 0x10; j++) rows.add(i * 0x10 + j).writeU8(i * 0x10 + j + 1);
        arr.add(i * 4).writePointer(rows.add(i * 0x10));
      }
      return { obj: arr, img: arr, rows: rows };
    },

    // ---- do_sarray_io 0x004af690 / do_barray_io 0x004af880 / access_virt_* 0x004af540 / 0x004af730 ------------
    // Per vector: a 0x40-byte virtual-array control block (mem_buffer +0, rows_in_array +4, samplesperrow /
    // blocksperrow +8, maxaccess +0xc, rows_in_mem +0x10, rowsperchunk +0x14, cur_start_row +0x18,
    // first_undef_row +0x1c, pre_zero +0x20, dirty +0x24, b_s_open +0x28, read_backing_store +0x30,
    // write_backing_store +0x34), a mem_buffer of 16 row pointers (rows past rows_in_mem point at real rows too, so
    // a window that ERREXIT let through stays inside our memory), the row data pre-filled per row, and a compress
    // object whose cinfo+0x170 holds the recorder. The backing-store callbacks hash (writing, file_offset,
    // byte_count, row index of the buffer) and, for a write, the bytes written; a read fills the buffer with
    // ((file_offset + j) * 7 + 3) & 0xff.
    // r = [rows_in_array, perrow, maxaccess, rows_in_mem, rowsperchunk, cur_start_row, first_undef_row, pre_zero,
    //      dirty, b_s_open]
    _c3csVirt(rows, rowBytes) {
      const fx = {};
      rows.forEach(function (r, i) {
        const rec = mkRec();
        const err = mkErr();
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(0x170).writePointer(rec);
        const bytes = r[1] * rowBytes;
        const data = alloc(16 * bytes);
        for (let k = 0; k < 16 * bytes; k++) data.add(k).writeU8((k / bytes | 0) * 0x11 + 1);
        const mb = alloc(16 * 4);
        for (let k = 0; k < 16; k++) mb.add(k * 4).writePointer(data.add(k * bytes));
        const p = alloc(0x40, 0x77777777);
        p.writePointer(r[10] === 'null' ? ptr(0) : mb);
        [r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9]].forEach(function (v, j) { p.add(4 + j * 4).writeS32(v); });
        p.add(0x30).writePointer(C3CS_READ_BS(mb));
        p.add(0x34).writePointer(C3CS_WRITE_BS(mb));
        fx['c' + i] = c;
        fx['v' + i] = p;
        fx['mb' + i] = mb;
        fx['data' + i] = data;
        fx['r' + i] = rec;
        fx['e' + i] = err;
        if (i === 0) fx.obj = mb;
      });
      return fx;
    },
    c3cs_sio() { return this._c3csVirt(C3CS_SIO_ROWS, 1); },
    c3cs_bio() { return this._c3csVirt(C3CS_BIO_ROWS, 128); },
    c3cs_avs() { return this._c3csVirt(C3CS_AV_ROWS, 1); },
    c3cs_avb() { return this._c3csVirt(C3CS_AVB_ROWS, 128); },
  });

  // Backing-store callbacks: (cinfo, info, buffer, file_offset, byte_count). The row index of the buffer is found
  // by its position in the vector's own mem_buffer array (`mb`), so the record is address-free.
  function rowIndex(mb, buf) {
    for (let k = 0; k < 16; k++) if (mb.add(k * 4).readPointer().equals(buf)) return k;
    return 0xff;
  }
  function C3CS_READ_BS(mb) {
    return keep(new NativeCallback(function (cinfo, info, buf, off, count) {
      const rec = cinfo.add(0x170).readPointer();
      push(rec, 0x10000000 + rowIndex(mb, buf));
      push(rec, off);
      push(rec, count);
      for (let j = 0; j < count; j++) buf.add(j).writeU8(((off + j) * 7 + 3) & 0xff);
    }, 'void', ['pointer', 'pointer', 'pointer', 'int', 'int']));
  }
  function C3CS_WRITE_BS(mb) {
    return keep(new NativeCallback(function (cinfo, info, buf, off, count) {
      const rec = cinfo.add(0x170).readPointer();
      push(rec, 0x20000000 + rowIndex(mb, buf));
      push(rec, off);
      push(rec, count);
      for (let j = 0; j < count; j++) push(rec, buf.add(j).readU8());
    }, 'void', ['pointer', 'pointer', 'pointer', 'int', 'int']));
  }

  // ---- vector tables (kept here so the registry can mirror the counts) ---------------------------------------
  const C3CS_EOB_ROWS = [
    { blk: 0 }, { blk: 1, dc: 0 }, { blk: 2, dc: 100 }, { blk: 3, dc: -100 }, { blk: 4, dc: 32767 },
    { blk: 5, bits: 3, pb: 0x00a00000 }, { blk: 6, bits: 7, pb: 0x00fe0000 }, { blk: 7, dc: 0xff },
    { blk: 8, dc: -0x10000 }, { blk: 9, bits: 5, pb: 0x00f80000 }, { blk: 10, dc: 0x40000000 },
    { blk: 11, dc: -0x40000000 }, { blk: 12, bits: 1, pb: 0x00800000 }, { blk: 13, dc: 1 },
  ];
  const C3CS_EOB_TIGHT_ROWS = [
    { blk: 1, free: 1, refill: 2 }, { blk: 3, free: 2, refill: 2 }, { blk: 4, free: 3, refill: 2 },
    { blk: 7, free: 1, refill: 2, bits: 6, pb: 0x00fc0000 }, { blk: 8, free: 2, refill: 3 },
    { blk: 9, free: 1, refill: 1 }, { blk: 2, free: 4, refill: 2 }, { blk: 5, free: 1, refill: 2 },
  ];
  const C3CS_EOB_SUSPEND_ROWS = [];
  [1, 2, 3, 4, 5, 6, 8, 10, 13, 17].forEach(function (f) { C3CS_EOB_SUSPEND_ROWS.push({ blk: 3, free: f, ok: 0 }); });
  [1, 2, 3, 5].forEach(function (f) { C3CS_EOB_SUSPEND_ROWS.push({ blk: 7, free: f, ok: 0 }); });
  C3CS_EOB_SUSPEND_ROWS.push({ blk: 4, free: 3, ok: 0 }, { blk: 8, free: 2, ok: 0 }, { blk: 12, free: 2, ok: 0 });
  // The empty block: the DC symbol (4 bits) emits nothing, so the end-of-block code through emit_bits 0x004b1a10 is
  // the first to flush a byte and the one that suspends (0x004b19c2).
  C3CS_EOB_SUSPEND_ROWS.push({ blk: 0, free: 1, ok: 0 }, { blk: 0, free: 2, ok: 0 });
  // dz / az: symbols whose size is 0. DC nbits for blk 1 with dc 0 is 7 (100); ZRL is 0xf0; blk 2's AC symbols
  // are 0x33 (run 19 -> ZRL + 3, value 7) and 0x31 (run 19 -> ZRL + 3, value -1); blk 6's are 0xf1 (run 15,
  // value 1) and 0x08 (run 16 -> ZRL + 0, value -255); EOB is symbol 0.
  const C3CS_EOB_ZERO_ROWS = [
    { blk: 1, dz: [7] }, { blk: 2, az: [0xf0] }, { blk: 2, az: [0x33] }, { blk: 0, az: [0] },
    { blk: 5, az: [0xf0, 0] }, { blk: 0, dz: [0], az: [0] }, { blk: 6, az: [0xf1, 0xf0] },
    { blk: 4, dc: 0, dz: [16], az: [0xf0, 0x20, 0] },
  ];
  const C3CS_RST_ROWS = [];
  for (let n = 0; n < 8; n++) C3CS_RST_ROWS.push({ num: n, bits: n, pb: (0x00ff0000 << (8 - n)) & 0x00ff0000, comps: n % 5 });
  C3CS_RST_ROWS.push({ num: 8, bits: 0, comps: 4 }, { num: -1, bits: 2, pb: 0x00400000, comps: 1 },
                     { num: 0x130, bits: 7, pb: 0x00fe0000, comps: 0 }, { num: 3, bits: 1, pb: 0x00800000, comps: 2 });
  const C3CS_RST_TIGHT_ROWS = [
    { num: 1, bits: 3, pb: 0x00e00000, free: 1, ok: 0 },     // flush_bits suspends
    { num: 2, bits: 0, free: 1, ok: 0 },                     // the 0xff byte suspends (0x004b1b12)
    { num: 3, bits: 0, free: 2, ok: 0 },                     // the marker byte suspends (0x004b1b3a)
    { num: 4, bits: 5, pb: 0x00f80000, free: 2, ok: 0 },     // the 0xff byte suspends after a flushed byte
    { num: 5, bits: 4, pb: 0x00a00000, free: 3, ok: 0 },
    { num: 6, bits: 0, free: 1, refill: 1, comps: 4 },       // refills after each byte
    { num: 7, bits: 6, pb: 0x00fc0000, free: 1, refill: 1, comps: 2 },
    { num: 0, bits: 7, pb: 0x00fe0000, free: 2, refill: 1, comps: 3 },
  ];
  const C3CS_MCU_ROWS = [
    { blocks: [[0, 1], [0, 3], [1, 2], [2, 4]] },
    { blocks: [[0, 0]] },
    { blocks: [] },                                                       // blocks_in_MCU 0
    { blocks: [[0, 5], [0, 6], [0, 7], [0, 8], [1, 9], [3, 10]], pb: 0x00c00000, bits: 2 },
    { blocks: [[2, 11], [2, 12], [2, 13]], ri: 3, rtg: 0, nrn: 0 },      // restart marker first
    { blocks: [[0, 1], [1, 2]], ri: 3, rtg: 0, nrn: 7 },                  // next_restart_num wraps to 0
    { blocks: [[0, 3]], ri: 3, rtg: 2, nrn: 4 },                          // only the counter moves
    { blocks: [[3, 4], [3, 5]], ri: 1, rtg: 0, nrn: 2, bits: 5, pb: 0x00a80000 },
    { blocks: [[0, 9], [1, 9], [2, 9], [3, 9]], ri: 0x10, rtg: 1, nrn: 6 },
    { blocks: [[1, 13], [0, 0]], ri: 2, rtg: 0, nrn: 1, comps: 2 },
  ];
  const C3CS_MCU_SUSPEND_ROWS = [
    { blocks: [[0, 1], [1, 3]], ri: 3, rtg: 0, nrn: 5, free: 1, ok: 0 },  // emit_restart suspends
    { blocks: [[0, 1], [1, 3]], free: 3, ok: 0 },                          // the first block suspends
    { blocks: [[0, 0], [1, 3], [2, 4]], free: 12, ok: 0 },                 // a later block suspends
    { blocks: [[0, 5], [0, 6]], ri: 2, rtg: 0, nrn: 3, free: 9, ok: 0 },
    { blocks: [[3, 7], [2, 8], [1, 9]], free: 40, ok: 0 },
  ];
  const C3CS_GATHER_ROWS = [
    { blocks: [[0, 1], [0, 3], [1, 2], [2, 4]] },
    { blocks: [[0, 0]] },
    { blocks: [] },
    { blocks: [[0, 5], [0, 6], [0, 7], [0, 8], [1, 9], [3, 10]] },
    { blocks: [[2, 11], [2, 12], [2, 13]], ri: 3, rtg: 0 },               // last_dc_val cleared, counter reloaded
    { blocks: [[0, 1], [1, 2]], ri: 3, rtg: 2 },                          // only the counter moves
    { blocks: [[3, 4], [3, 5]], ri: 1, rtg: 0, comps: 2 },
    { blocks: [[1, 13], [0, 0]], ri: 0x10, rtg: 0, comps: 0 },             // reload with no component cleared
    { blocks: [[0, 9], [1, 9], [2, 9], [3, 9]], ri: 0, rtg: 0 },
    { blocks: [[3, 3], [2, 3], [1, 3], [0, 3]], ri: 5, rtg: 1 },
  ];
  const C3CS_FPG_ROWS = [
    { tbls: [[0, 0]], dcSlots: [], acSlots: [] },
    { tbls: [[0, 0], [1, 1], [1, 1]], dcSlots: [], acSlots: [] },        // table 1 built once
    { tbls: [[0, 0], [1, 1], [2, 3]], dcSlots: [0, 1, 2, 3], acSlots: [0, 1, 2, 3] },   // no allocation
    { tbls: [[3, 2], [3, 2], [0, 1], [2, 0]], dcSlots: [3], acSlots: [1] },
    { tbls: [], dcSlots: [], acSlots: [] },                               // no component
    { tbls: [[2, 2], [2, 3]], dcSlots: [0], acSlots: [3] },
    { tbls: [[1, 0], [0, 1], [3, 3], [1, 0]], dcSlots: [], acSlots: [0] },
    { tbls: [[0, 3]], dcSlots: [0], acSlots: [] },
  ];
  const C3CS_SPH_ROWS = [
    { gather: 0, tbls: [[0, 0]], dc: [0], ac: [0] },
    { gather: 0, tbls: [[0, 0], [1, 1], [1, 0]], dc: [0, 1], ac: [0, 1], ri: 7 },
    { gather: 0, tbls: [[0, 0], [1, 1]], dc: [0, 1], ac: [0, 1], preDerived: [0, 1] },   // tables reused
    { gather: 1, tbls: [[0, 0], [1, 1]], dc: [], ac: [] },                 // count arrays allocated
    { gather: 1, tbls: [[2, 3], [3, 2]], dc: [], ac: [], preCount: [2, 3], ri: 1 },     // count arrays reused
    { gather: 1, tbls: [[0, 0], [0, 0], [0, 0], [0, 0]], dc: [], ac: [], preCount: [0] },
    { gather: 0, tbls: [], dc: [], ac: [], ri: 0x20 },                     // no component
    { gather: 1, tbls: [[4, 0], [0, -1]], dc: [], ac: [], preCount: [0, 1, 2, 3] },     // out of range numbers
    { gather: 1, tbls: [[1, 2]], dc: [1], ac: [2], preCount: [] },
    { gather: 0, tbls: [[4, 0]], dc: [0, 1, 2, 3], ac: [0, 1, 2, 3] },    // dc 4 reads cinfo+0x60 (ac slot 0)
    { gather: 0, tbls: [[3, -1]], dc: [0, 1, 2, 3], ac: [0, 1, 2, 3] },   // ac -1 reads cinfo+0x5c (dc slot 3)
    { gather: 1, tbls: [[3, 3]], dc: [3], ac: [], preCount: [3] },        // AC table missing: allowed when gathering
    { gather: 1, tbls: [[-1, 4]], dc: [], ac: [], slack: 1 },             // dc -1 (+0x48 = ac_derived[3]), ac 4 (+0x6c)
    { gather: 0, tbls: [[2, 0]], dc: [0], ac: [0], fix: 1 },               // DC slot 2 NULL, not gathering (0x004b136b)
    { gather: 0, tbls: [[0, 3], [1, 1]], dc: [0, 1], ac: [0], fix: 1 },    // AC slot 3 then AC slot 1 NULL (0x004b139a)
  ];
  // [rows_in_array, perrow, maxaccess, rows_in_mem, rowsperchunk, cur_start_row, first_undef_row, pre_zero,
  //  dirty, b_s_open]
  const C3CS_SIO_ROWS = [
    [16, 8, 4, 4, 4, 0, 16, 0, 0, 1],        // one chunk of four rows
    [16, 8, 4, 4, 1, 2, 16, 0, 0, 1],        // four chunks of one row
    [16, 8, 4, 6, 4, 0, 16, 0, 0, 1],        // chunks of 4 then 2
    [16, 8, 4, 6, 4, 4, 7, 0, 0, 1],         // first_undef_row limits the first chunk (3 rows)
    [10, 8, 4, 6, 3, 6, 16, 0, 0, 1],        // rows_in_array limits (4 rows: 3 then 1)
    [16, 8, 4, 4, 4, 5, 5, 0, 0, 1],         // nothing defined: the first chunk has no row
    [16, 5, 4, 0, 4, 0, 16, 0, 0, 1],        // rows_in_mem 0: no call at all
    [16, 3, 4, 5, 2, 1, 4, 0, 0, 1],         // chunk of 2, then 1 row (first_undef_row 4)
    [16, 7, 4, 8, 3, 0, 16, 0, 0, 1],
    [6, 1, 4, 8, 8, 2, 16, 0, 0, 1],         // rows_in_array - cur_start_row 4 of 8
  ];
  const C3CS_BIO_ROWS = [
    [16, 1, 4, 4, 4, 0, 16, 0, 0, 1],
    [16, 1, 4, 4, 1, 2, 16, 0, 0, 1],
    [16, 2, 4, 6, 4, 0, 16, 0, 0, 1],
    [16, 1, 4, 6, 4, 4, 7, 0, 0, 1],
    [10, 1, 4, 6, 3, 6, 16, 0, 0, 1],
    [16, 1, 4, 4, 4, 5, 5, 0, 0, 1],
    [16, 2, 4, 0, 4, 0, 16, 0, 0, 1],
    [16, 1, 4, 5, 2, 1, 4, 0, 0, 1],
  ];
  // access_virt_*: rows_in_mem 4, rowsperchunk 2, maxaccess 3 unless noted.
  const C3CS_AV_ROWS = [
    [16, 8, 3, 4, 2, 4, 16, 0, 0, 1],        // 0 in window, defined
    [16, 8, 3, 4, 2, 4, 6, 1, 0, 1],         // 1 reader past first_undef_row with pre_zero
    [16, 8, 3, 4, 2, 4, 6, 0, 0, 1],         // 2 reader past it without pre_zero: ERREXIT 0x14 and early return
    [16, 8, 3, 4, 2, 4, 5, 1, 0, 1],         // 3 writer that skips undefined rows: ERREXIT 0x14
    [16, 8, 3, 4, 2, 4, 16, 0, 1, 1],        // 4 window moves forward, dirty written back first
    [16, 8, 3, 4, 2, 6, 16, 0, 1, 1],        // 5 window moves backward: cur_start_row = max(end - 4, 0)
    [16, 8, 3, 4, 2, 6, 16, 0, 0, 0],        // 6 window move without backing store: ERREXIT 0x44
    [16, 8, 3, 4, 2, 0, 16, 0, 0, 1],        // 7 (bad access vectors use this one)
    [16, 8, 3, 4, 2, 0, 2, 1, 0, 1],         // 8 writer extends first_undef_row with pre_zero
    [16, 8, 3, 4, 2, 0, 16, 0, 0, 1, 'null'],   // 9 NULL mem_buffer: ERREXIT 0x14, in window, no row touched
    [16, 8, 3, 4, 2, 2, 3, 0, 0, 1],         // 10 writer past first_undef_row without pre_zero
    [16, 8, 3, 4, 2, 9, 16, 0, 1, 1],        // 11 backward move clamped to 0
    [16, 8, 3, 4, 2, 6, 3, 1, 0, 1],         // 12 num_rows 6 > maxaccess: the window lands above start_row, so the
                                             //    zero range undef_row - cur_start_row wraps and is empty
  ];
  // The block twin: the same cases over rows of blocksperrow 1 or 2 (0x80 or 0x100 bytes).
  const C3CS_AVB_ROWS = C3CS_AV_ROWS.map(function (r, i) { const c = r.slice(); c[1] = 1 + (i % 2); return c; });
})();
