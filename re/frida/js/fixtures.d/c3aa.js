// Fixtures for C3 batch c3aa: IJG libjpeg 6a compress-side helpers (emit_bits, flush_bits_4b2510,
// emit_buffered_bits, jpeg_add_quant_table, emit_dqt, emit_sof, emit_dht, select_scan_parameters).
// Only our own synthetic values (no game text). Names are prefixed c3aa_. Output buffers are sized so the
// buffer-full "suspend" path (dump_buffer / empty_output_buffer) is never reached.
(function () {
  function zero(p, size) { for (let i = 0; i < size; i += 4) p.add(i).writeU32(0); }

  // --- jchuff working_state: +0 next_output_byte, +4 free_in_buffer, +8 put_buffer, +0xc put_bits, +0x20 cinfo.
  function jchuffState(put_buffer, put_bits) {
    const buf = Memory.alloc(512); zero(buf, 512);
    const st = Memory.alloc(0x24); zero(st, 0x24);
    st.writePointer(buf);                       // next_output_byte
    st.add(4).writeS32(512);                    // free_in_buffer (large)
    st.add(8).writeU32(put_buffer >>> 0);       // put_buffer
    st.add(0xc).writeS32(put_bits);             // put_bits
    return { state: st, buf: buf };
  }

  // --- jcphuff phuff_entropy_encoder: +0xc gather_statistics, +0x10 next_output_byte, +0x14 free_in_buffer,
  //     +0x18 put_buffer, +0x1c put_bits, +0x20 cinfo.
  function phuffEnt(gather, put_buffer, put_bits) {
    const buf = Memory.alloc(512); zero(buf, 512);
    const e = Memory.alloc(0x24); zero(e, 0x24);
    e.add(0xc).writeS32(gather);
    e.add(0x10).writePointer(buf);              // next_output_byte
    e.add(0x14).writeS32(512);                  // free_in_buffer (large)
    e.add(0x18).writeU32(put_buffer >>> 0);     // put_buffer
    e.add(0x1c).writeS32(put_bits);             // put_bits
    return { obj: e, buf: buf };
  }
  function corrBits(fn, n) { const b = Memory.alloc((n + 3) & ~3); for (let i = 0; i < n; i++) b.add(i).writeU8(fn(i) & 0xff); return b; }

  // --- jpeg_compress_struct (offsets from the decompiles): see shim/src/re/c3aa.cpp. dest at +0x14; a dest
  //     manager is {next_output_byte, free_in_buffer, _, empty_output_buffer}; buffer sized so it never fills.
  function cinfoDest() {
    const cinfo = Memory.alloc(0x160); zero(cinfo, 0x160);
    const buf = Memory.alloc(1024); zero(buf, 1024);
    const dest = Memory.alloc(0x10); zero(dest, 0x10);
    dest.writePointer(buf);                     // next_output_byte
    dest.add(4).writeS32(1024);                 // free_in_buffer (large)
    cinfo.add(0x14).writePointer(dest);         // cinfo->dest
    return { obj: cinfo, buf: buf, dest: dest };
  }
  function quantTbl(big, sent) {               // 64 u16 quantval, sent_table dword at +0x80
    const q = Memory.alloc(0x84); zero(q, 0x84);
    for (let i = 0; i < 64; i++) q.add(i * 2).writeU16(big ? (10 + i * 5) : (1 + (i * 3) % 200));
    if (sent) q.add(0x80).writeU32(1);
    return q;
  }
  function huffTbl(bits, sent) {               // bits[1..16] at +1, huffval at +0x11, sent_table at +0x114
    const h = Memory.alloc(0x118); zero(h, 0x118);
    let count = 0;
    for (let i = 0; i < 16; i++) { h.add(1 + i).writeU8(bits[i]); count += bits[i]; }
    for (let i = 0; i < count; i++) h.add(0x11 + i).writeU8((i * 17) & 0xff);
    if (sent) h.add(0x114).writeU32(1);
    return h;
  }
  // emit_dht: cinfo with dc_huff_tbl_ptrs[0..3] (+0x50) and ac_huff_tbl_ptrs[0..3] (+0x60), each table with a
  // distinct bits[1..16] (distinct byte count -> distinct emitted length and huffval bytes). Keys dc0..dc3/ac0..ac3.
  const _DHT_DC = [                           // counts 2, 5, 9, 12
    [2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 2, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [1, 2, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]];
  const _DHT_AC = [                           // counts 7, 15, 20, 4
    [0, 1, 2, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [2, 3, 2, 3, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]];
  function dhtCinfo(sent) {
    const f = cinfoDest();
    for (let i = 0; i < 4; i++) {
      const dc = huffTbl(_DHT_DC[i], sent), ac = huffTbl(_DHT_AC[i], sent);
      f.obj.add(0x50 + i * 4).writePointer(dc);   // dc_huff_tbl_ptrs[i]
      f.obj.add(0x60 + i * 4).writePointer(ac);   // ac_huff_tbl_ptrs[i]
      f['dc' + i] = dc; f['ac' + i] = ac;
    }
    return f;
  }

  // scan_info / master + comp_info for select_scan_parameters. One entry per behaviour variant (argument-free).
  function scanCinfo(opts) {
    const cinfo = Memory.alloc(0x160); zero(cinfo, 0x160);
    const comp = Memory.alloc(8 * 0x54); zero(comp, 8 * 0x54);
    cinfo.add(0x3c).writePointer(comp);         // comp_info
    cinfo.add(0x34).writeS32(opts.nc);          // num_components
    if (opts.scans) {
      const scans = Memory.alloc(opts.scans.length * 0x24); zero(scans, opts.scans.length * 0x24);
      opts.scans.forEach(function (s, si) {
        const sp = scans.add(si * 0x24);
        sp.writeS32(s.n);                       // comps_in_scan (scanptr[0])
        s.idx.forEach(function (ix, k) { sp.add(4 + k * 4).writeS32(ix); });  // component_index[k] (scanptr[1+k])
        sp.add(0x14).writeS32(s.Ss);            // scanptr[5]
        sp.add(0x18).writeS32(s.Se);            // scanptr[6]
        sp.add(0x1c).writeS32(s.Ah);            // scanptr[7]
        sp.add(0x20).writeS32(s.Al);            // scanptr[8]
      });
      cinfo.add(0xa4).writePointer(scans);      // scan_info
      const master = Memory.alloc(0x24); zero(master, 0x24);
      master.add(0x20).writeS32(opts.scan_number);  // master->scan_number
      cinfo.add(0x144).writePointer(master);
    }
    return { obj: cinfo };
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // emit_bits: empty bit buffer, 512-byte output. Vectors (code, size) drive the accumulate/flush/0xFF-stuff logic.
    c3aa_emit_bits() { return jchuffState(0, 0); },

    // flush_bits variants (argument-free): emit path (bits+7 >= 8), 0xFF-stuff path, gather-only reset, no-emit reset.
    c3aa_phuff_flush() { return phuffEnt(0, 0x00abc000, 3); },
    c3aa_phuff_flush_ff() { return phuffEnt(0, 0x00ff0000, 1); },
    c3aa_phuff_flush_g1() { return phuffEnt(1, 0x00123456, 5); },
    c3aa_phuff_flush_nz() { return phuffEnt(0, 0x00123456, 0); },
    c3aa_phuff_flush_multi() { return phuffEnt(0, 0x00abcdef, 12); },   // bits+7 = 19 -> 2 output bytes (loop-again)

    // emit_buffered_bits: empty bit buffer; two correction-bit sources (alternating, and all-ones to force 0xFF).
    c3aa_phuff_buffered() {
      const e = phuffEnt(0, 0, 0);
      e.bits_alt = corrBits(function (i) { return i & 1; }, 64);
      e.bits_ones = corrBits(function () { return 1; }, 64);
      return e;
    },
    c3aa_phuff_buffered_g1() {
      const e = phuffEnt(1, 0, 0);
      e.bits_ones = corrBits(function () { return 1; }, 64);
      return e;
    },

    // jpeg_add_quant_table: global_state = CSTATE_START (100); two pre-allocated quant tables so the NULL/alloc
    // branch is not taken; basic[] mixes values below and above 255.
    c3aa_add_quant() {
      const cinfo = Memory.alloc(0x160); zero(cinfo, 0x160);
      cinfo.add(0x10).writeS32(100);
      const q0 = quantTbl(false, false), q1 = quantTbl(false, false);
      cinfo.add(0x40).writePointer(q0);         // quant_tbl_ptrs[0]
      cinfo.add(0x44).writePointer(q1);         // quant_tbl_ptrs[1]
      const basic = Memory.alloc(64 * 4);
      for (let i = 0; i < 64; i++) basic.add(i * 4).writeS32(1 + (i * 7) % 400);
      return { obj: cinfo, q0: q0, q1: q1, basic: basic };
    },

    // emit_dqt: quant_tbl_ptrs[0] = 8-bit precision, [1] = 16-bit precision, neither sent.
    c3aa_dqt() {
      const f = cinfoDest();
      const q0 = quantTbl(false, false), q1 = quantTbl(true, false);
      f.obj.add(0x40).writePointer(q0);
      f.obj.add(0x44).writePointer(q1);
      f.q0 = q0; f.q1 = q1;
      return f;
    },
    // emit_dqt sent-table set on both tables: the body returns the precision without emitting.
    c3aa_dqt_sent() {
      const f = cinfoDest();
      const q0 = quantTbl(false, true), q1 = quantTbl(true, true);
      f.obj.add(0x40).writePointer(q0);
      f.obj.add(0x44).writePointer(q1);
      f.q0 = q0; f.q1 = q1;
      return f;
    },

    // emit_sof: three components, small image.
    c3aa_sof() {
      const f = cinfoDest();
      f.obj.add(0x30).writeS32(8);              // data_precision
      f.obj.add(0x1c).writeS32(64);             // image_height
      f.obj.add(0x18).writeS32(48);             // image_width
      f.obj.add(0x34).writeS32(3);              // num_components
      const comp = Memory.alloc(3 * 0x54); zero(comp, 3 * 0x54);
      for (let i = 0; i < 3; i++) {
        const c = comp.add(i * 0x54);
        c.writeS32(i + 1);                      // component_id
        c.add(8).writeS32(2);                   // h_samp_factor
        c.add(0xc).writeS32(1);                 // v_samp_factor
        c.add(0x10).writeS32(i % 2);            // quant_tbl_no
      }
      f.obj.add(0x3c).writePointer(comp);       // comp_info
      f.comp = comp;
      return f;
    },
    c3aa_sof_nc1() {
      const f = cinfoDest();
      f.obj.add(0x30).writeS32(8);
      f.obj.add(0x1c).writeS32(100);
      f.obj.add(0x18).writeS32(120);
      f.obj.add(0x34).writeS32(1);
      const comp = Memory.alloc(0x54); zero(comp, 0x54);
      comp.writeS32(7); comp.add(8).writeS32(1); comp.add(0xc).writeS32(1); comp.add(0x10).writeS32(0);
      f.obj.add(0x3c).writePointer(comp);
      f.comp = comp;
      return f;
    },

    // emit_dht: four DC and four AC tables (indices 0..3), each with a distinct bits[]/huffval length so the
    // emitted segment length and bytes differ per (index, is_ac). c3aa_dht leaves sent_table 0 (emit path);
    // c3aa_dht_sent sets it to 1 (skip path: a GREEN with no bytes proves the jcc, the output buffer is in state).
    c3aa_dht() { return dhtCinfo(false); },
    c3aa_dht_sent() { return dhtCinfo(true); },

    // select_scan_parameters: single-scan path (scan_info NULL), num_components 1..4.
    c3aa_scan_nc1() { return scanCinfo({ nc: 1 }); },
    c3aa_scan_nc2() { return scanCinfo({ nc: 2 }); },
    c3aa_scan_nc3() { return scanCinfo({ nc: 3 }); },
    c3aa_scan_nc4() { return scanCinfo({ nc: 4 }); },
    // scan-script path (scan_info != NULL): master->scan_number selects the script entry.
    c3aa_scan_s0c1() { return scanCinfo({ nc: 4, scan_number: 0, scans: [{ n: 1, idx: [0], Ss: 0, Se: 0, Ah: 0, Al: 0 }] }); },
    c3aa_scan_s0c2() { return scanCinfo({ nc: 4, scan_number: 0, scans: [{ n: 2, idx: [0, 1], Ss: 1, Se: 5, Ah: 0, Al: 1 }] }); },
    c3aa_scan_s0c3() { return scanCinfo({ nc: 4, scan_number: 0, scans: [{ n: 3, idx: [0, 1, 2], Ss: 2, Se: 10, Ah: 1, Al: 2 }] }); },
    c3aa_scan_s0c4() { return scanCinfo({ nc: 4, scan_number: 0, scans: [{ n: 4, idx: [0, 1, 2, 3], Ss: 0, Se: 0x3f, Ah: 0, Al: 0 }] }); },
    c3aa_scan_s1c1() { return scanCinfo({ nc: 4, scan_number: 1, scans: [{ n: 2, idx: [0, 1], Ss: 1, Se: 5, Ah: 0, Al: 1 }, { n: 1, idx: [2], Ss: 5, Se: 5, Ah: 0, Al: 0 }] }); },
    c3aa_scan_s1c2() { return scanCinfo({ nc: 4, scan_number: 1, scans: [{ n: 1, idx: [0], Ss: 0, Se: 0, Ah: 0, Al: 0 }, { n: 2, idx: [1, 3], Ss: 3, Se: 9, Ah: 2, Al: 3 }] }); },
  });
})();
