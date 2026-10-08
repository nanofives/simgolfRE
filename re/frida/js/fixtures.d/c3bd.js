// Fixtures for C3 batch c3bd (sound.dll: seven speech-codec leaves and two MIDI track walkers). Every block is
// allocated here: no live sound object, device, list, DirectSound buffer or module global is ever handed to an
// arm, and nothing is allocated or freed by the functions under test. Layouts come from the disassembly cited
// in shim/src/re/c3bd.cpp. All values are synthetic; the MIDI byte streams are written here byte by byte and
// are not taken from any game file.
Object.assign(globalThis.DIFF_FIXTURES, {

  // --- 0x1003e520 decodeTableSymbol: twenty packed-symbol cells, twenty value slots and twenty counters.
  // The symbol's low byte decides the parity and its low seven bits index the table at 0x10064a80, so the two
  // are chosen independently. The twenty values cover all four (table bit 4, parity) combinations:
  //   bit 4 set, parity 1 (the counter is incremented once):     14, 25, 37, 127, 0x180
  //   bit 4 set, parity 0 (nothing happens):                     0, 23, 43, 113, 0x100, -1
  //   bit 4 clear, parity 1 (one increment):                     1, 7, 11, 13
  //   bit 4 clear, parity 0 (two increments and the value -1):   3, 5, 6, 9, 15
  // 0x100, 0x180 and -1 also prove that the parity uses the low BYTE while the index uses the low seven bits.
  c3bd_symbols() {
    const syms = [14, 25, 37, 127, 0x180,
                  0, 23, 43, 113, 0x100, -1,
                  1, 7, 11, 13,
                  3, 5, 6, 9, 15];
    const out = {};
    syms.forEach((v, i) => {
      const s = __keepAlloc(4); s.writeS32(v); out['s' + i] = s;
      const val = __keepAlloc(4); val.writeS32(0x5a5a0000 + i); out['v' + i] = val;
      const cnt = __keepAlloc(4); cnt.writeS32(i * 7 - 3); out['c' + i] = cnt;
    });
    return out;
  },

  // --- 0x1003e600 substituteIfBelowThreshold: count cells, replacement buffers and scanned buffers. The
  // scanned buffer is both the input of the magnitude test and the destination of the copy, so it is the state
  // region. Each buffer holds 18 floats (the function indexes from 1 after decrementing by one float).
  // sc0 stays under the 0.99f limit everywhere, sc1 breaks it at the first element, sc2 at the last, sc3 with
  // a NEGATIVE value (the fchs arm), sc4 exactly at the limit (which continues, the compare is strict), sc5
  // just above it, sc6 is all zeros and sc7 holds large magnitudes.
  c3bd_substitute() {
    const buf = (vals) => {
      const p = __keepAlloc(0x48);
      for (let i = 0; i < 18; ++i) p.add(i * 4).writeFloat(i < vals.length ? vals[i] : 0);
      return p;
    };
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const third = 1 / 3;
    return {
      sc0: buf([0.1, -0.2, 0.3, -0.4, 0.5, -0.6, 0.7, -0.8, 0.9]),
      sc1: buf([1.5, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1]),
      sc2: buf([0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 2.25]),
      sc3: buf([0.2, -0.3, -1.75, 0.4, -0.5, 0.6, -0.7, 0.8, -0.9]),
      sc4: buf([0.99, -0.99, 0.99, -0.99, 0.99, -0.99, 0.99, -0.99, 0.99]),
      sc5: buf([0.99, 0.99, 0.9900001, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5]),
      sc6: buf([0, -0, 0, -0, 0, -0, 0, -0, 0]),
      sc7: buf([3e5, -3e5, 1e-7, -1e-7, third, -third, 1, -1, 0.5]),
      rp0: buf([11, 12, 13, 14, 15, 16, 17, 18, 19]),
      rp1: buf([-1 / 3, 2 / 3, -4 / 3, 8 / 3, -16 / 3, 32 / 3, -64 / 3, 1 / 3, -2 / 3]),
      rp2: buf([1e-7, -1e-7, 3e5, -3e5, 0.1, -0.1, 0, -0, 1]),
      rp3: buf([0, 0, 0, 0, 0, 0, 0, 0, 0]),
      n0: cell(0), n1: cell(1), n2: cell(2), n4: cell(4), n8: cell(8), n9: cell(9), nm1: cell(-1),
    };
  },

  // --- 0x1003d0a0 applyIir2: sample buffers, count cells and codec contexts. A context is 0xc00 bytes with
  // five float state slots at +0xbec..+0xbfc; everything else is zeroed so both arms start equal.
  // ic0 has a zero state, ic1 a small state, ic2 a state that is not exact in binary, ic3 a large one, ic4 a
  // negative one and ic5 a mixed one.
  c3bd_iir2() {
    const buf = (vals) => {
      const p = __keepAlloc(0x48);
      for (let i = 0; i < 18; ++i) p.add(i * 4).writeFloat(i < vals.length ? vals[i] : 0);
      return p;
    };
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const ctx = (s) => {
      const p = __keepAlloc(0xc00);
      for (let o = 0; o < 0xc00; o += 4) p.add(o).writeU32(0);
      for (let i = 0; i < 5; ++i) p.add(0xbec + i * 4).writeFloat(s[i]);
      return p;
    };
    const third = 1 / 3;
    return {
      is0: buf([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]),
      is1: buf([third, -third, 2 * third, -2 * third, 4 * third, -4 * third, third, -third, third]),
      is2: buf([3e5, -3e5, 1.5e5, 7e4, -2.5e5, 3e5, 1, -1, 0]),
      is3: buf([1e-7, -1e-7, 2e-7, 5e-8, 1e-7, 3e-7, -4e-7, 1e-7, 0]),
      is4: buf([1, 1, 1, 1, 1, 1, 1, 1, 1]),
      is5: buf([0, -0, 1e-45, -1e-45, 1.0000001, 0.9999999, 1e7, -1e7, 0.1]),
      ic0: ctx([0, 0, 0, 0, 0]),
      ic1: ctx([0.25, -0.125, 0.5, -0.0625, 0.03125]),
      ic2: ctx([third, -third, 0.1, -0.1, 1e-7]),
      ic3: ctx([3e5, -2e5, 1e5, -5e4, 2.5e4]),
      ic4: ctx([-1, -2, -4, -8, -16]),
      ic5: ctx([1e-7, 3e5, -1 / 3, 0.9999999, -1e-45]),
      in0: cell(0), in1: cell(1), in2: cell(2), in4: cell(4), in8: cell(8), inm1: cell(-1),
    };
  },

  // --- 0x10040e30 codecBiquadFilter: sample buffers, index cells and four-float state blocks.
  c3bd_biquad() {
    const buf = (vals) => {
      const p = __keepAlloc(0x48);
      for (let i = 0; i < 18; ++i) p.add(i * 4).writeFloat(i < vals.length ? vals[i] : 0);
      return p;
    };
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const st = (s) => {
      const p = __keepAlloc(0x10);
      for (let i = 0; i < 4; ++i) p.add(i * 4).writeFloat(s[i]);
      return p;
    };
    const third = 1 / 3;
    return {
      bs0: buf([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2]),
      bs1: buf([third, -third, 2 * third, -2 * third, 4 * third, -4 * third, third, -third, third, 1, -1, 0]),
      bs2: buf([3e5, -3e5, 1e-7, -1e-7, 1, -1, 2.5, -2.5, 0, 7e4, -7e4, 0.5]),
      bs3: buf([1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]),
      bs4: buf([0, -0, 1e-45, -1e-45, 1.0000001, 0.9999999, 1e7, -1e7, 0.1, -0.1, 0, 0]),
      bt0: st([0, 0, 0, 0]),
      bt1: st([0.5, -0.25, 0.125, -0.0625]),
      bt2: st([third, -third, 0.1, -0.1]),
      bt3: st([3e5, -2e5, 1e-7, -1e-7]),
      bt4: st([-1, -2, -4, -8]),
      bf1: cell(1), bf3: cell(3), bf5: cell(5),
      bl0: cell(0), bl1: cell(1), bl4: cell(4), bl8: cell(8), bl12: cell(12),
    };
  },

  // --- 0x10040330 codecCorrelationLags: input buffers of 32 floats, output buffers of 32 floats, last-index
  // cells and window cells. With last = 24 and window = 12 the first inner loop runs j = 12+4k..24 in steps of
  // two and the output loop j = 13..24, so in[j-8] never reads before the buffer.
  // cl0..cl2 give a first accumulator above the 1e-10 floor (the solve runs), cl3 is all zeros and cl4 is of
  // the order of 1e-7 so the squared sums stay under the floor (the solve is skipped).
  c3bd_corrlags() {
    const buf = (fn) => {
      const p = __keepAlloc(0x80);
      for (let i = 0; i < 32; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const zeros8 = () => { const p = __keepAlloc(8); p.writeU32(0); p.add(4).writeU32(0); return p; };
    const third = 1 / 3;
    return {
      cl0: buf((i) => Math.sin(i * 0.37) * 0.8),
      cl1: buf((i) => (i % 2 === 0 ? 1 : -1) * (0.1 + i * 0.05)),
      cl2: buf((i) => (i === 0 ? 3e5 : third * (i % 5) - 0.4)),
      cl3: buf(() => 0),
      cl4: buf((i) => (i % 3 === 0 ? 1e-7 : -1e-7)),
      co0: buf(() => 0), co1: buf(() => 0), co2: buf(() => 0), co3: buf(() => 0), co4: buf(() => 0),
      co5: buf(() => 0),
      ck0: zeros8(), ck1: zeros8(), ck2: zeros8(), ck3: zeros8(), ck4: zeros8(), ck5: zeros8(),
      clast24: cell(24), clast16: cell(16),
      cwin12: cell(12), cwin8: cell(8), cwin4: cell(4), cwin0: cell(0),
    };
  },

  // --- 0x100404d0 codecSymmetricFir: input buffers of 64 floats, output buffers of 64 floats, last-index and
  // window cells. With last = 48 and window = 16 the loop runs j = 33..48 and in[j-30] never reads before the
  // buffer. window 0 leaves the loop unentered.
  c3bd_fir() {
    const buf = (fn) => {
      const p = __keepAlloc(0x100);
      for (let i = 0; i < 64; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const third = 1 / 3;
    return {
      fi0: buf((i) => Math.sin(i * 0.21) * 0.9),
      fi1: buf((i) => (i % 2 === 0 ? third : -third) * (1 + (i % 7))),
      fi2: buf((i) => (i === 10 ? 3e5 : (i % 4 === 0 ? 1e-7 : 0.25))),
      fi3: buf(() => 1),
      fi4: buf(() => 0),
      fo0: buf(() => 0), fo1: buf(() => 0), fo2: buf(() => 0), fo3: buf(() => 0), fo4: buf(() => 0),
      flast48: cell(48), flast40: cell(40),
      fwin16: cell(16), fwin8: cell(8), fwin1: cell(1), fwin0: cell(0),
    };
  },

  // --- 0x1003e9f0 codecAutocorrelation: order, first and last cells, 32-float sample buffers, 32-float
  // matrices and 16-float lag vectors. With order 4, first 1 and last 12 every index the four loops form stays
  // inside the buffers (the lowest read is x[1], the highest matrix slot is 15).
  // ao1 (order 1) leaves loop C unentered and loop D's bound at 0; af5 with last 4 puts the start past the
  // last index so loops A and B do not run.
  c3bd_autocorr() {
    const buf = (fn) => {
      const p = __keepAlloc(0x80);
      for (let i = 0; i < 32; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };
    const zeros = (n) => {
      const p = __keepAlloc(n);
      for (let o = 0; o < n; o += 4) p.add(o).writeU32(0);
      return p;
    };
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const third = 1 / 3;
    return {
      ax0: buf((i) => Math.sin(i * 0.41) * 0.7),
      ax1: buf((i) => (i % 2 === 0 ? 1 : -1) * (0.2 + i * 0.03)),
      ax2: buf((i) => (i === 3 ? 3e5 : third * ((i % 4) - 1.5))),
      ax3: buf(() => 0),
      ax4: buf((i) => (i % 3 === 0 ? 1e-7 : -1e-7)),
      am0: zeros(0x80), am1: zeros(0x80), am2: zeros(0x80), am3: zeros(0x80), am4: zeros(0x80),
      am5: zeros(0x80), am6: zeros(0x80), am7: zeros(0x80),
      al0: zeros(0x40), al1: zeros(0x40), al2: zeros(0x40), al3: zeros(0x40), al4: zeros(0x40),
      al5: zeros(0x40), al6: zeros(0x40), al7: zeros(0x40),
      ao1: cell(1), ao2: cell(2), ao4: cell(4),
      af1: cell(1), af3: cell(3), af5: cell(5),
      an12: cell(12), an8: cell(8), an4: cell(4),
    };
  },

  // --- 0x1001ef80 Midi_scanTrack: fourteen reader objects, each 0x60 bytes with a synthetic MIDI track at
  // +0x3c and every other field zero. The tracks below are written byte by byte here.
  // End of track is the meta event ff 2f 00; `vlq(n)` writes a variable-length quantity.
  c3bd_scantracks() {
    const vlq = (n) => {
      const out = [n & 0x7f];
      n >>>= 7;
      while (n > 0) { out.unshift((n & 0x7f) | 0x80); n >>>= 7; }
      return out;
    };
    const END = [0xff, 0x2f, 0x00];
    const mk = (bytes) => {
      const data = __keepAlloc(bytes.length + 16);
      bytes.forEach((b, i) => data.add(i).writeU8(b & 0xff));
      for (let i = bytes.length; i < bytes.length + 16; ++i) data.add(i).writeU8(0);
      const obj = __keepAlloc(0x60);
      for (let o = 0; o < 0x60; o += 4) obj.add(o).writeU32(0);
      obj.add(0x3c).writePointer(data);
      return obj;
    };
    return {
      // one note on, one note off (two data bytes each, table case 0), then end of track
      k0: mk([].concat(vlq(0), [0x90, 60, 100], vlq(96), [0x80, 60, 0], vlq(0), END)),
      // running status: the second event has no status byte of its own
      k1: mk([].concat(vlq(10), [0x90, 60, 100], vlq(20), [62, 90], vlq(30), [64, 80], vlq(0), END)),
      // program change (0xc0) and pitch bend (0xe0): the table gives both ONE data byte
      k2: mk([].concat(vlq(5), [0xc0, 7], vlq(5), [0xe0, 64], vlq(0), END)),
      // control change (0xb0), two data bytes
      k3: mk([].concat(vlq(1), [0xb0, 7, 110], vlq(1), [0xb0, 10, 64], vlq(0), END)),
      // multi-byte delta times (128, 16384 and 2097151)
      k4: mk([].concat(vlq(128), [0x90, 60, 1], vlq(16384), [0x80, 60, 0], vlq(2097151), END)),
      // two meta events that are not end of track: a five-byte payload and a 200-byte payload, whose length
      // needs TWO variable-length bytes (0x81 0x48), then end of track
      k5: mk([].concat(vlq(3), [0xff, 0x51, 5, 1, 2, 3, 4, 5],
                       vlq(4), [0xff, 0x7f], vlq(200), Array.from({ length: 200 }, (_, i) => i & 0xff),
                       vlq(4), END)),
      // status 0xa0 is an error in the table: the walk returns 0 and writes nothing
      k6: mk([].concat(vlq(0), [0xa0, 60, 100], vlq(0), END)),
      // status 0xd0 is an error as well
      k7: mk([].concat(vlq(0), [0xd0, 60], vlq(0), END)),
      // the first event has no status byte, so the running status is still 0 and the walk returns 0
      k8: mk([].concat(vlq(7), [60, 100], vlq(0), END)),
      // sysex 0xf0 keeps its whole byte as the status and is an error in the table
      k9: mk([].concat(vlq(0), [0xf0, 3, 1, 2, 3], vlq(0), END)),
      // end of track as the very first event
      k10: mk([].concat(vlq(0), END)),
      // channel 15 on every status, mixing all three data-byte cases
      k11: mk([].concat(vlq(2), [0x9f, 60, 1], vlq(2), [0xcf, 3], vlq(2), [0xbf, 7, 7], vlq(2), END)),
      // a long run of running-status events
      k12: mk([].concat(vlq(0), [0x90, 60, 100], vlq(1), [61, 100], vlq(2), [62, 100], vlq(3), [63, 100],
                        vlq(4), [64, 100], vlq(5), END)),
      // a meta event with an EMPTY payload followed by end of track
      k13: mk([].concat(vlq(0), [0xff, 0x01, 0], vlq(0), [0xff, 0x03, 2, 65, 66], vlq(0), END)),
    };
  },

  // --- 0x10024920 Midi_seekTrackToTick: one sequencer object of 0x800 bytes (the controller-7 store lands at
  // +0x254 + channel*0x4c, so channel 15 reaches +0x6c8), sixteen track records of 0x60 bytes each with a
  // synthetic MIDI track at +0x3c, and tick limits.
  c3bd_seektracks() {
    const vlq = (n) => {
      const out = [n & 0x7f];
      n >>>= 7;
      while (n > 0) { out.unshift((n & 0x7f) | 0x80); n >>>= 7; }
      return out;
    };
    const END = [0xff, 0x2f, 0x00];
    const seq = () => {
      const p = __keepAlloc(0x800);
      for (let o = 0; o < 0x800; o += 4) p.add(o).writeU32(0);
      return p;
    };
    const mk = (bytes) => {
      const data = __keepAlloc(bytes.length + 16);
      bytes.forEach((b, i) => data.add(i).writeU8(b & 0xff));
      for (let i = bytes.length; i < bytes.length + 16; ++i) data.add(i).writeU8(0);
      const rec = __keepAlloc(0x60);
      for (let o = 0; o < 0x60; o += 4) rec.add(o).writeU32(0);
      rec.add(0x3c).writePointer(data);
      return rec;
    };
    const notes = [].concat(vlq(0), [0x90, 60, 100], vlq(100), [0x80, 60, 0], vlq(100), [0x90, 62, 90],
                            vlq(100), END);
    const ctrl7 = [].concat(vlq(0), [0xb0, 7, 110], vlq(50), [0xb3, 7, 64], vlq(50), [0xbf, 7, 1],
                            vlq(50), END);
    const ctrlOther = [].concat(vlq(0), [0xb0, 10, 64], vlq(50), [0xb0, 11, 32], vlq(50), END);
    const metaMix = [].concat(vlq(10), [0xff, 0x51, 3, 7, 0xa1, 0x20], vlq(10), [0xff, 0x01, 0],
                              vlq(10), [0x90, 60, 1], vlq(10), END);
    const oneByte = [].concat(vlq(4), [0xc0, 5], vlq(4), [0xe0, 64], vlq(4), END);
    const running = [].concat(vlq(1), [0x90, 60, 100], vlq(1), [61, 100], vlq(1), [62, 100], vlq(1), END);
    const bad = [].concat(vlq(1), [0xa0, 60, 100], vlq(1), END);
    const noStatus = [].concat(vlq(1), [60, 100], vlq(1), END);
    const longDeltas = [].concat(vlq(1000), [0x90, 60, 1], vlq(1000), [0x80, 60, 0], vlq(1000), END);
    const endFirst = [].concat(vlq(0), END);
    return {
      q: seq(),
      t_notes: mk(notes), t_notes2: mk(notes), t_notes3: mk(notes),
      t_ctrl7: mk(ctrl7), t_ctrl7b: mk(ctrl7),
      t_ctrlOther: mk(ctrlOther),
      t_meta: mk(metaMix), t_meta2: mk(metaMix),
      t_one: mk(oneByte),
      t_run: mk(running), t_run2: mk(running),
      t_bad: mk(bad),
      t_nostatus: mk(noStatus),
      t_long: mk(longDeltas), t_long2: mk(longDeltas),
      t_endfirst: mk(endFirst),
    };
  },
});
