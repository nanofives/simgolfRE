// Fixtures for C3 batch c3cg (round 20, golf_clean.exe): the IJG libjpeg 6a method pointers
// write_any_marker 0x004afb60, write_file_header 0x004afc40, write_frame_header 0x004afdc0,
// write_tables_only 0x004b0320, fullsize_downsample 0x004b43f0, rgb_gray_convert 0x004b4eb0,
// grayscale_convert 0x004b50d0, request_virt_sarray 0x004af2d0 and request_virt_barray 0x004af340, plus the
// two vtable methods Window::hitTestBorder 0x0047ed30 and Snd::flags 0x00484ff0.
// Only our own synthetic values (no game text, no live game object). Names are prefixed c3cg_.
//
// Allocations go through Memory.alloc (diff_hook rewrites it to __keepAlloc so blocks referenced only from a
// field survive the run); NativeCallbacks are pushed onto __diffKeepAlive by hand.
//
// No global of the running game is written by any fixture here: every object is freshly allocated, and the
// two vtable methods run on synthetic objects carrying OUR vtable, never the live display 0x0083ad50, the
// live window list or a sound device.
//
// Every callback is a pure function of its arguments or of memory diff_hook snapshots and restores between
// the two arms, so the second arm sees exactly the world the first one saw. The libjpeg error_exit records
// and RETURNS, so the instructions after an ERREXIT run in both arms (round 19's lesson).
//
// libjpeg structures used below: cinfo+0 err (error_exit +0, emit_message +4, msg_code +0x14,
// msg_parm.i[0] +0x18), cinfo+4 mem, cinfo+0x14 dest (next_output_byte +0, free_in_buffer +4,
// empty_output_buffer +0xc; our spare buffer is stashed at dest+0x18), image_width +0x18,
// input_components +0x20, data_precision +0x30, num_components +0x34, jpeg_color_space +0x38,
// comp_info +0x3c, quant_tbl_ptrs +0x40, dc_huff_tbl_ptrs +0x50, ac_huff_tbl_ptrs +0x60, arith_code +0xac,
// write_JFIF_header +0xc8, density_unit +0xcc, X_density +0xce, Y_density +0xd0, write_Adobe_marker +0xd4,
// progressive_mode +0xdc, max_v_samp_factor +0xe4, cconvert +0x158. A jpeg_component_info is 0x54 bytes
// (component_id +0, h_samp_factor +8, v_samp_factor +0xc, quant_tbl_no +0x10, dc_tbl_no +0x14,
// ac_tbl_no +0x18, width_in_blocks +0x1c). A JQUANT_TBL is 64 u16 plus sent_table at +0x80; a JHUFF_TBL is
// bits[1..16] at +1, huffval at +0x11 and sent_table at +0x114.
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function zero(p, n) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(0); }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v); }
  function alloc(n, v) { const p = Memory.alloc(n); if (v === undefined) zero(p, n); else fill(p, n, v); return p; }

  // A rolling hash recorder: rec+0 the hash, rec+4 the number of calls, rec+8 the last value pushed. It makes
  // the ORDER and the arguments of every dispatched callback part of the compared state.
  function mkRec() { return alloc(0x40); }
  function push(rec, v) {
    let h = rec.readU32();
    h = (Math.imul(h, 33) + (v >>> 0)) >>> 0;
    rec.writeU32(h);
    rec.add(4).writeU32((rec.add(4).readU32() + 1) >>> 0);
    rec.add(8).writeU32(v >>> 0);
  }

  // ---- the shared libjpeg scaffolding -----------------------------------------------------------------------
  // error_exit(cinfo): records msg_code and msg_parm.i[0] in the error manager's tail (+0x30..+0x3b, which
  // none of these functions writes) and RETURNS. emit_message(cinfo, level) records the same plus the level.
  const ERR_REC = keep(new NativeCallback(function (cinfo) {
    const e = cinfo.readPointer();
    let h = e.add(0x30).readU32();
    h = (Math.imul(h, 33) + e.add(0x14).readU32()) >>> 0;
    h = (Math.imul(h, 33) + e.add(0x18).readU32()) >>> 0;
    e.add(0x30).writeU32(h);
    e.add(0x34).writeU32((e.add(0x34).readU32() + 1) >>> 0);
    e.add(0x38).writeU32(e.add(0x14).readU32());
  }, 'void', ['pointer']));
  const MSG_REC = keep(new NativeCallback(function (cinfo, level) {
    const e = cinfo.readPointer();
    let h = e.add(0x3c).readU32();
    h = (Math.imul(h, 33) + e.add(0x14).readU32()) >>> 0;
    h = (Math.imul(h, 33) + (level >>> 0)) >>> 0;
    e.add(0x3c).writeU32(h);
    e.add(0x40).writeU32((e.add(0x40).readU32() + 1) >>> 0);
    e.add(0x44).writeU32(e.add(0x14).readU32());
  }, 'void', ['pointer', 'int']));

  // A 0x84-byte jpeg_error_mgr pre-filled 0x22 with our two handlers and zeroed recorder slots.
  function mkErr() {
    const e = alloc(0x84, 0x22222222);
    e.writePointer(ERR_REC);
    e.add(4).writePointer(MSG_REC);
    zero(e.add(0x30), 0x20);
    return e;
  }

  // empty_output_buffer(cinfo) = TRUE: reset the destination manager to the spare buffer stashed at dest+0x18
  // with `free` bytes available, and record the refill. Deterministic, and every byte it touches is compared.
  function mkEmpty(rec, free) {
    return keep(new NativeCallback(function (cinfo) {
      const d = cinfo.add(0x14).readPointer();
      d.writePointer(d.add(0x18).readPointer());
      d.add(4).writeU32(free);
      push(rec, 0xe0000000 + free);
      return 1;
    }, 'int', ['pointer']));
  }

  // A destination manager over a 0x400-byte buffer, pre-filled 0x5b so every emitted byte shows.
  function mkDest(rec, free) {
    const buf = alloc(0x400, 0x5b5b5b5b);
    const dest = alloc(0x20);
    dest.writePointer(buf);
    dest.add(4).writeU32(free);
    dest.add(0xc).writePointer(mkEmpty(rec, free));
    dest.add(0x18).writePointer(buf);
    return { dest: dest, buf: buf };
  }

  function quantTbl(big, sent) {          // 64 u16 quantval, sent_table at +0x80
    const q = alloc(0x84);
    for (let i = 0; i < 64; i++) q.add(i * 2).writeU16(big ? (300 + i * 7) : (1 + (i * 3) % 200));
    if (sent) q.add(0x80).writeU32(1);
    return q;
  }
  function huffTbl(bits) {                // bits[1..16] at +1, huffval at +0x11, sent_table at +0x114
    const h = alloc(0x118);
    let count = 0;
    for (let i = 0; i < 16; i++) { h.add(1 + i).writeU8(bits[i]); count += bits[i]; }
    for (let i = 0; i < count; i++) h.add(0x11 + i).writeU8((i * 23 + 7) & 0xff);
    return h;
  }
  // rows = [component_id, h, v, quant_tbl_no, dc_tbl_no, ac_tbl_no, width_in_blocks]
  function mkComps(rows) {
    const k = alloc(8 * 0x54, 0x99999999);
    rows.forEach(function (r, i) {
      const c = k.add(i * 0x54);
      c.writeS32(r[0]);
      c.add(8).writeS32(r[1]);
      c.add(0xc).writeS32(r[2]);
      c.add(0x10).writeS32(r[3]);
      c.add(0x14).writeS32(r[4]);
      c.add(0x18).writeS32(r[5]);
      c.add(0x1c).writeS32(r[6]);
    });
    return k;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ---- write_any_marker 0x004afb60 -------------------------------------------------------------------------
    // One compress object with one error manager, one destination manager and one output buffer shared by every
    // vector (diff_hook restores all of them between the arms and between the vectors). The payloads are seven
    // blocks of our own bytes; the vectors pick the marker, the payload and the length, including four lengths
    // above 0xfffd that take the `ja` at 0x004afb6c and emit nothing. `free` sets free_in_buffer: a roomy buffer
    // never reaches emit_byte's refill at 0x004afbcc, a buffer with three free bytes reaches it repeatedly and
    // the refill is recorded.
    _c3cgWam(free) {
      const rec = mkRec();
      const d = mkDest(rec, free);
      const err = mkErr();
      const c = alloc(0x180);
      c.writePointer(err);
      c.add(0x14).writePointer(d.dest);
      const fx = { obj: c, c: c, err: err, dest: d.dest, buf: d.buf, rec: rec };
      [1, 2, 5, 16, 64, 200, 256].forEach(function (n, i) {
        const p = alloc((n + 4) & ~3);
        for (let j = 0; j < n; j++) p.add(j).writeU8((j * 37 + i * 11 + 3) & 0xff);
        fx['p' + i] = p;
      });
      return fx;
    },
    c3cg_wam() { return this._c3cgWam(0x400); },
    c3cg_wam_full() { return this._c3cgWam(3); },

    // ---- write_file_header 0x004afc40 ------------------------------------------------------------------------
    // Twelve compress objects over one shared error manager, destination manager and buffer. They cover all four
    // (write_JFIF_header +0xc8, write_Adobe_marker +0xd4) combinations; the JFIF ones carry different
    // density_unit (+0xcc), X_density (+0xce) and Y_density (+0xd0) so emit_jfif_app0 0x004afc80 emits different
    // bytes, and the Adobe ones different jpeg_color_space (+0x38) values so emit_adobe_app14 0x004afd20 takes
    // each of its arms.
    c3cg_wfh() {
      const rec = mkRec();
      const d = mkDest(rec, 0x400);
      const err = mkErr();
      const fx = { obj: err, err: err, dest: d.dest, buf: d.buf, rec: rec };
      //      jfif adobe unit  xd    yd    colour space
      const rows = [
        [0, 0, 0, 0, 0, 0],
        [1, 0, 0, 1, 1, 0],
        [0, 1, 0, 0, 0, 1],
        [1, 1, 1, 72, 72, 3],
        [1, 0, 2, 300, 150, 0],
        [0, 1, 0, 0, 0, 2],
        [1, 1, 0, 0x1234, 0x5678, 4],
        [0, 1, 0, 0, 0, 5],
        [1, 0, 1, 1, 0xffff, 0],
        [0, 1, 0, 0, 0, 0],
        [1, 1, 2, 96, 1, 1],
        [0, 0, 1, 10, 10, 3]];
      rows.forEach(function (r, i) {
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(0x14).writePointer(d.dest);
        c.add(0x38).writeS32(r[5]);
        c.add(0xc8).writeS32(r[0]);
        c.add(0xcc).writeU8(r[2]);
        c.add(0xce).writeU16(r[3]);
        c.add(0xd0).writeU16(r[4]);
        c.add(0xd4).writeS32(r[1]);
        fx['c' + i] = c;
      });
      return fx;
    },

    // ---- write_frame_header 0x004afdc0 -----------------------------------------------------------------------
    // Twelve compress objects over one shared error manager, destination manager and buffer, and one quantisation
    // table per (object, slot) because emit_dqt 0x004afec0 sets sent_table at +0x80 (all of them are compared).
    // q8 is an 8-bit table (every entry <= 0xff, so emit_dqt returns 0), q16 has entries above 0xff (returns 1),
    // which is what drives the prec test at 0x004afe31 and the TRACEMS at 0x004afe3f.
    c3cg_wfrh() {
      const rec = mkRec();
      const d = mkDest(rec, 0x400);
      const err = mkErr();
      const fx = { obj: err, err: err, dest: d.dest, buf: d.buf, rec: rec };
      // nc, precision, arith, progressive, [[id,h,v,q,dc,ac,wib]...], [16-bit quant slots]
      const rows = [
        { nc: 3, prec: 8, ar: 0, pr: 0, comps: [[1, 2, 2, 0, 0, 0, 4], [2, 1, 1, 1, 1, 1, 2], [3, 1, 1, 1, 0, 1, 2]], big: [] },
        { nc: 3, prec: 8, ar: 0, pr: 0, comps: [[1, 2, 2, 0, 0, 0, 4], [2, 1, 1, 1, 1, 1, 2], [3, 1, 1, 1, 0, 1, 2]], big: [1] },
        { nc: 1, prec: 8, ar: 0, pr: 0, comps: [[7, 1, 1, 0, 2, 0, 3]], big: [] },
        { nc: 1, prec: 8, ar: 0, pr: 0, comps: [[7, 1, 1, 0, 0, 3, 3]], big: [] },
        { nc: 2, prec: 12, ar: 0, pr: 0, comps: [[1, 1, 1, 0, 0, 0, 5], [2, 1, 1, 1, 1, 1, 5]], big: [] },
        { nc: 2, prec: 8, ar: 0, pr: 1, comps: [[1, 1, 1, 0, 0, 0, 5], [2, 1, 1, 1, 1, 1, 5]], big: [] },
        { nc: 2, prec: 8, ar: 1, pr: 0, comps: [[1, 1, 1, 0, 0, 0, 5], [2, 1, 1, 1, 1, 1, 5]], big: [] },
        { nc: 0, prec: 8, ar: 0, pr: 0, comps: [], big: [] },
        { nc: 1, prec: 8, ar: 1, pr: 1, comps: [[1, 1, 1, 0, 0, 0, 1]], big: [] },
        { nc: 2, prec: 8, ar: 0, pr: 1, comps: [[1, 1, 1, 0, 0, 0, 2], [2, 1, 1, 1, 0, 0, 2]], big: [0, 1] },
        { nc: 4, prec: 8, ar: 0, pr: 0, comps: [[1, 1, 1, 0, 1, 1, 1], [2, 1, 1, 1, 1, 1, 1], [3, 2, 2, 2, 0, 1, 1], [4, 1, 2, 3, 1, 0, 1]], big: [] },
        { nc: 2, prec: 8, ar: 0, pr: 0, comps: [[1, 1, 1, 0, 2, 0, 6], [2, 1, 1, 1, 0, 0, 6]], big: [1] }];
      rows.forEach(function (r, i) {
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(0x14).writePointer(d.dest);
        c.add(0x18).writeS32(32 + i * 8);           // image_width  (emit_sof)
        c.add(0x1c).writeS32(16 + i * 4);           // image_height (emit_sof)
        c.add(0x30).writeS32(r.prec);
        c.add(0x34).writeS32(r.nc);
        c.add(0x3c).writePointer(mkComps(r.comps));
        c.add(0xac).writeS32(r.ar);
        c.add(0xdc).writeS32(r.pr);
        for (let s = 0; s < 4; s++) {
          const q = quantTbl(r.big.indexOf(s) >= 0, false);
          c.add(0x40 + s * 4).writePointer(q);
          fx['q' + i + '_' + s] = q;
        }
        fx['c' + i] = c;
      });
      return fx;
    },

    // ---- write_tables_only 0x004b0320 ------------------------------------------------------------------------
    // Eight compress objects over one shared error manager, destination manager and buffer. `q`, `dc` and `ac`
    // list which of the four slots at cinfo+0x40, +0x50 and +0x60 hold a table; the rest are NULL, which is the
    // not-taken side of the tests at 0x004b033d, 0x004b0366 and 0x004b0377. arith_code 1 skips the whole Huffman
    // half (0x004b035a). Every table is a state region: emit_dqt 0x004afec0 writes sent_table at +0x80 and
    // emit_dht 0x004b0120 at +0x114.
    c3cg_wto() {
      const rec = mkRec();
      const d = mkDest(rec, 0x400);
      const err = mkErr();
      const fx = { obj: err, err: err, dest: d.dest, buf: d.buf, rec: rec };
      const BITS = [
        [2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        [0, 2, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        [1, 2, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        [0, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]];
      const rows = [
        { ar: 0, q: [0, 1, 2, 3], dc: [0, 1, 2, 3], ac: [0, 1, 2, 3] },
        { ar: 0, q: [], dc: [], ac: [] },
        { ar: 0, q: [0, 2], dc: [1], ac: [3] },
        { ar: 1, q: [0, 1, 2, 3], dc: [0, 1, 2, 3], ac: [0, 1, 2, 3] },
        { ar: 0, q: [3], dc: [0, 1, 2, 3], ac: [] },
        { ar: 0, q: [], dc: [], ac: [0, 1, 2, 3] },
        { ar: 0, q: [1], dc: [2], ac: [2] },
        { ar: 1, q: [], dc: [], ac: [] }];
      rows.forEach(function (r, i) {
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(0x14).writePointer(d.dest);
        c.add(0xac).writeS32(r.ar);
        r.q.forEach(function (s) {
          const q = quantTbl(s === 2, false);
          c.add(0x40 + s * 4).writePointer(q);
          fx['q' + i + '_' + s] = q;
        });
        r.dc.forEach(function (s) {
          const h = huffTbl(BITS[s]);
          c.add(0x50 + s * 4).writePointer(h);
          fx['d' + i + '_' + s] = h;
        });
        r.ac.forEach(function (s) {
          const h = huffTbl(BITS[3 - s]);
          c.add(0x60 + s * 4).writePointer(h);
          fx['a' + i + '_' + s] = h;
        });
        fx['c' + i] = c;
      });
      return fx;
    },

    // ---- fullsize_downsample 0x004b43f0 ----------------------------------------------------------------------
    // Ten (cinfo, compptr, input, output) sets. Each cinfo carries max_v_samp_factor (+0xe4) and image_width
    // (+0x18); each compptr a width_in_blocks (+0x1c), so the padded width is width_in_blocks * 8 (0x004b4424).
    // The input rows hold our own bytes; the output rows are pre-filled 0x6d over 0x80 bytes so both the copy
    // (jcopy_sample_rows 0x004b04f0) and the right-edge padding (expand_right_edge 0x004b4390) show. Rows beyond
    // max_v_samp_factor stay untouched, which is part of the evidence. image_width is never 0 while the padded
    // width is larger, because expand_right_edge reads the sample before the pad column.
    c3cg_fds() {
      const fx = {};
      //      rows, width, width_in_blocks
      const rows = [
        [1, 8, 1], [2, 8, 1], [2, 5, 1], [4, 16, 2], [1, 1, 1],
        [3, 24, 3], [2, 9, 2], [1, 31, 4], [4, 8, 2], [2, 17, 3]];
      rows.forEach(function (r, i) {
        const c = alloc(0x180);
        c.add(0x18).writeU32(r[1]);
        c.add(0xe4).writeS32(r[0]);
        const comp = alloc(0x54, 0x99999999);
        comp.add(0x1c).writeS32(r[2]);
        const inRows = alloc(8 * 4), outRows = alloc(8 * 4);
        const inBuf = alloc(8 * 0x80), outBuf = alloc(8 * 0x80, 0x6d6d6d6d);
        for (let row = 0; row < 8; row++) {
          const ip = inBuf.add(row * 0x80);
          for (let col = 0; col < 0x80; col++) ip.add(col).writeU8((col * 13 + row * 41 + i * 7) & 0xff);
          inRows.add(row * 4).writePointer(ip);
          outRows.add(row * 4).writePointer(outBuf.add(row * 0x80));
        }
        fx['c' + i] = c; fx['k' + i] = comp; fx['i' + i] = inRows; fx['o' + i] = outRows;
        fx['ib' + i] = inBuf; fx['ob' + i] = outBuf;
        if (i === 0) fx.obj = c;
      });
      return fx;
    },

    // ---- rgb_gray_convert 0x004b4eb0 -------------------------------------------------------------------------
    // Four compress objects with different image_width (+0x18) and one colour-converter object each whose +8 is
    // our own 3 x 256 table of ints (the three groups have different scales, so r, g and b contribute
    // differently and the arithmetic shift at 0x004b4f3a is visible). The input rows hold three bytes per pixel;
    // the output image is one component, output_buf[0] being an array of eight row pointers into a buffer
    // pre-filled 0x3c.
    _c3cgConvert(gray) {
      const fx = {};
      const tab = alloc(3 * 256 * 4);
      for (let i = 0; i < 256; i++) {
        tab.add(i * 4).writeS32(i * 19595);
        tab.add(0x400 + i * 4).writeS32(i * 38470);
        tab.add(0x800 + i * 4).writeS32(i * 7471 + 32768);
      }
      fx.tab = tab;
      [0, 1, 5, 8].forEach(function (w, i) {
        const cc = alloc(0x20);
        cc.add(8).writePointer(tab);
        const c = alloc(0x180);
        c.add(0x18).writeU32(w);
        c.add(0x20).writeS32(gray ? (i + 1) : 3);     // input_components (grayscale_convert's stride)
        c.add(0x158).writePointer(cc);
        fx['c' + i] = c;
        fx['cc' + i] = cc;
        if (i === 0) fx.obj = c;
      });
      // Eight input rows of 0x40 bytes and one output component of eight rows of 0x40 bytes.
      const inRows = alloc(8 * 4), inBuf = alloc(8 * 0x40);
      for (let row = 0; row < 8; row++) {
        const p = inBuf.add(row * 0x40);
        for (let col = 0; col < 0x40; col++) p.add(col).writeU8((col * 29 + row * 53 + 11) & 0xff);
        inRows.add(row * 4).writePointer(p);
      }
      const outRows = alloc(8 * 4), outBuf = alloc(8 * 0x40, 0x3c3c3c3c);
      for (let row = 0; row < 8; row++) outRows.add(row * 4).writePointer(outBuf.add(row * 0x40));
      const image = alloc(4 * 4);
      image.writePointer(outRows);
      fx.inRows = inRows; fx.inBuf = inBuf; fx.outRows = outRows; fx.outBuf = outBuf; fx.image = image;
      return fx;
    },
    c3cg_rgb() { return this._c3cgConvert(false); },
    c3cg_gray() { return this._c3cgConvert(true); },

    // ---- request_virt_sarray 0x004af2d0 / request_virt_barray 0x004af340 --------------------------------------
    // One compress object whose memory manager is our own my_memory_mgr: small_list[0] at mem+0x30 and
    // small_list[1] at mem+0x34 each point at a pool header (next +0, bytes_used +4, bytes_left +8) with room
    // for several 0x80-byte control blocks, so alloc_small 0x004aef70 carves from the pool at
    // header + 0x10 + bytes_used (0x004af087) and never calls jpeg_get_large: both arms get the same pointer.
    // virt_sarray_list (mem+0x40) and virt_barray_list (mem+0x44) start NULL and are compared, as are the two
    // pools, the memory manager and the error manager. pool_id 1 is the accepted one; pool_id 0 takes the
    // ERREXIT at 0x004af2e1 / 0x004af351 and, with the recording handler that returns, allocates from
    // small_list[0] (alloc_small accepts 0 at 0x004aefb5, so no second message is raised).
    c3cg_rva() {
      const err = mkErr();
      const mem = alloc(0x60, 0x77777777);
      zero(mem.add(0x30), 0x30);
      const pool0 = alloc(0x400, 0xcccccccc), pool1 = alloc(0x400, 0xcccccccc);
      [pool0, pool1].forEach(function (p) { p.writePointer(ptr(0)); p.add(4).writeU32(0); p.add(8).writeU32(0x300); });
      mem.add(0x30).writePointer(pool0);
      mem.add(0x34).writePointer(pool1);
      const c = alloc(0x180);
      c.writePointer(err);
      c.add(4).writePointer(mem);
      return { obj: pool1, c: c, err: err, mem: mem, pool0: pool0, pool1: pool1 };
    },

    // ---- Window::hitTestBorder 0x0047ed30 ---------------------------------------------------------------------
    // Seven synthetic windows of 0x200 bytes pre-filled 0x44 (so an unread field cannot be mistaken for a read
    // one), carrying only the eight fields the function reads: the resize flag byte at +0xa0 (bit 1), the flag
    // dword at +0x9c, the border width at +0x1f0, the rectangle at +0x1bc (left), +0x1c0 (top), +0x1c4 (right)
    // and +0x1c8 (bottom), and the two y limits at +0x180 and +0x1ec. No vtable is needed: the body makes no
    // call. These objects are never linked into the game's window or draw lists.
    c3cg_hit() {
      const fx = {};
      //      resize, width, l,  t,  r,   b,   flags9c,     y180, y1ec
      const rows = [
        [1, 8, 0, 0, 100, 60, 0, 0, 0],
        [1, 4, 10, 20, 110, 80, 1, 0, 0],
        [0, 8, 0, 0, 100, 60, 0x10, 30, 0],
        [0, 8, 0, 0, 100, 60, 0x4000000, 0, 25],
        [0, 8, 0, 0, 100, 60, 0, 0, 0],
        [1, 0, 0, 0, 10, 10, 0x4000000, 0, 5],
        [1, 50, 0, 0, 20, 20, 0x11, 7, 9]];
      rows.forEach(function (r, i) {
        const w = alloc(0x200, 0x44444444);
        w.add(0x9c).writeS32(r[6]);
        w.add(0xa0).writeU8(r[0] ? 2 : 0);
        w.add(0x180).writeS32(r[7]);
        w.add(0x1bc).writeS32(r[2]);
        w.add(0x1c0).writeS32(r[3]);
        w.add(0x1c4).writeS32(r[4]);
        w.add(0x1c8).writeS32(r[5]);
        w.add(0x1ec).writeS32(r[8]);
        w.add(0x1f0).writeS32(r[1]);
        fx['w' + i] = w;
        if (i === 0) fx.obj = w;
      });
      return fx;
    },

    // ---- setFocusItem 0x0047ae80 -------------------------------------------------------------------------------
    // Synthetic views of 0x180 bytes pre-filled 0x3b, carrying only the four fields the body reads and writes:
    // the child-list head at +0x138, the cursor at +0x13c, the child count at +0x140 and the index at +0x144.
    // The list nodes are 0x20-byte blocks with the item at +4 and the next link at +0xc; the items are
    // 0x20-byte objects on OUR OWN vtable whose slot +0xf0 is the thiscall focus notification. None of these
    // is a view of the game's live window or draw list.
    // `mode` picks what the LOSE-focus notification does, which is the only way the two re-reads at
    // 0x0047af3b and 0x0047af48 can ever fail: 0 = nothing, 1 = clear the view's list head at +0x138
    // (the item keeps a back pointer to its view at +8), 2 = clear the cursor node's item at +4.
    _c3cgFocus(mode) {
      const rec = mkRec();
      const NOTIFY = keep(new NativeCallback(function (self, on) {
        push(rec, (self.add(4).readS32() << 4) | (on & 0xf));
        if (on === 0 && mode !== 0) {
          const view = self.add(8).readPointer();
          if (!view.isNull()) {
            if (mode === 1) view.add(0x138).writePointer(ptr(0));
            else view.add(0x13c).readPointer().add(4).writePointer(ptr(0));
          }
        }
      }, 'void', ['pointer', 'int'], 'thiscall'));
      const vt = alloc(0x100);
      vt.add(0xf0).writePointer(NOTIFY);
      const fx = { obj: rec, rec: rec, vt: vt };

      // Each view gets its own four items and a four-node list; `cursor` is the node index +0x13c starts at
      // (-1 means a null head), `count` what +0x140 holds, `noitem` the node whose +4 is left null.
      function mkView(i, count, cursor, noitem, nullHead) {
        const v = alloc(0x180, 0x3b3b3b3b);
        const items = [], nodes = [];
        for (let k = 0; k < 4; k++) {
          const it = alloc(0x20);
          it.writePointer(vt);
          it.add(4).writeS32(i * 10 + k);
          it.add(8).writePointer(v);
          items.push(it);
          fx['it' + i + '_' + k] = it;
        }
        for (let k = 0; k < 4; k++) {
          const nd = alloc(0x20);
          nd.add(4).writePointer(k === noitem ? ptr(0) : items[k]);
          nodes.push(nd);
          fx['nd' + i + '_' + k] = nd;
        }
        for (let k = 0; k < 4; k++) nodes[k].add(0xc).writePointer(nodes[(k + 1) % 4]);
        v.add(0x138).writePointer(nullHead ? ptr(0) : nodes[0]);
        v.add(0x13c).writePointer(cursor < 0 ? ptr(0) : nodes[cursor]);
        v.add(0x140).writeS32(count);
        v.add(0x144).writeS32(0x77);
        fx['v' + i] = v;
      }
      //      count, cursor node, node with a null item, null head
      mkView(0, 4, 0, -1, false);
      mkView(1, 4, 2, -1, false);
      mkView(2, 1, 2, -1, false);         // child count 1 -> the jle at 0x0047af27, no lose-focus call
      mkView(3, 4, 0, 0, false);          // the cursor node's item is null -> no lose-focus notification
      mkView(4, 0, 0, -1, false);         // child count 0 -> immediate return
      mkView(5, -1, 0, -1, false);        // negative count: passes 0x0047ae9a, fails 0x0047aeed
      mkView(6, 4, 0, -1, true);          // null head -> the return at 0x0047aecd
      mkView(7, 2, 1, -1, false);
      return fx;
    },
    c3cg_focus() { return this._c3cgFocus(0); },
    c3cg_focus_cb_head() { return this._c3cgFocus(1); },
    c3cg_focus_cb_item() { return this._c3cgFocus(2); },

    // ---- Snd::flags 0x00484ff0 ---------------------------------------------------------------------------------
    // Fifteen synthetic sound objects of 0x80 bytes pre-filled 0x6b, carrying the sub-object pointer at +0x40,
    // the second pointer at +0x30 and the flag dword at +0x58. Four of them have a sub-object built on OUR OWN
    // vtable whose slot +0x70 is a thiscall callback returning a value taken from the sub-object's id field
    // (with bits above the low word set, which is how the run shows that the ORs at 0x00485008 onwards only
    // touch AL and AH). No live sound device or DirectSound object is involved.
    c3cg_snd() {
      const rec = mkRec();
      const BASE = keep(new NativeCallback(function (self) {
        const v = self.add(4).readS32();
        push(rec, v);
        return v;
      }, 'int', ['pointer'], 'thiscall'));
      const vt = alloc(0x80);
      vt.add(0x70).writePointer(BASE);
      function mkSub(v) { const s = alloc(0x20); s.writePointer(vt); s.add(4).writeS32(v); return s; }
      const subs = [mkSub(0x12340000), mkSub(-1), mkSub(-0x80000000), mkSub(0x00ff0000)];
      const other = alloc(0x10, 0x1a1a1a1a);
      const fx = { obj: rec, rec: rec, vt: vt };
      subs.forEach(function (s, i) { fx['s' + i] = s; });
      //      sub index (-1 = NULL), +0x30 non-null, +0x58
      const rows = [
        [-1, 0, 0], [-1, 1, 0], [-1, 0, 0x3f], [0, 0, 0], [0, 1, 0x3f],
        [-1, 0, 1], [-1, 0, 2], [-1, 0, 4], [-1, 0, 8], [-1, 0, 0x10],
        [-1, 0, 0x20], [-1, 0, 0xffffff00], [1, 1, 0x15], [2, 0, 0x2a], [3, 1, 0x3e]];
      rows.forEach(function (r, i) {
        const o = alloc(0x80, 0x6b6b6b6b);
        o.add(0x30).writePointer(r[1] ? other : ptr(0));
        o.add(0x40).writePointer(r[0] < 0 ? ptr(0) : subs[r[0]]);
        o.add(0x58).writeU32(r[2]);
        fx['o' + i] = o;
      });
      return fx;
    },
  });
})();
