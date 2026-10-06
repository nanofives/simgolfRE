// Fixtures for C3 batch c3e. Each seeds the tables the function (and its original callees) read so every branch is
// reachable; only our own synthetic values are written (no game text). Names are prefixed c3e_.
Object.assign(globalThis.DIFF_FIXTURES, {
  // bilinearSample 0x004674c0 reads the signed-byte grid at 0x00838c1c (row stride 19, index iy + ix*19, corners
  // base+0/+1/+19/+20). ix,iy stay in 0..15, so base+20 <= 320; 352 bytes cover every read.
  c3e_bilinear() {
    const g = ptr('0x00838c1c');
    for (let i = 0; i < 352; i++) g.add(i).writeS8(((i * 37 + 11) % 61) - 30);
    return { obj: g };
  },

  // objectAt 0x0040df80 scans the 256 placed-object records at 0x0058bcb8 (stride 0x10; type +0, rx +2, ry +4). The
  // first 48 records carry types 0..7 at spread tile positions; the rest are empty (type -1). The footprint tables
  // 0x004c26c0 (size) and 0x005a8c38 (width) are left as the game's own constants (they feed the footprint, not a
  // guard this fixture needs to vary).
  c3e_placed() {
    const base = ptr('0x0058bcb8');
    for (let i = 0; i < 256; i++) {
      const r = base.add(i * 0x10);
      r.writeS16(i < 48 ? (i % 8) : -1);
      r.add(2).writeS16((i * 5) % 50);
      r.add(4).writeS16((i * 11) % 50);
    }
    return { obj: base };
  },

  // matchInputCode 0x0047eee0 reads the 10-byte circular keystroke buffer 0x0083aba4..0x0083abad through the cursor
  // pointer at 0x004e42ec. The ring is seeded 0x41..0x4a by address and the cursor is placed at the top (0x0083abad);
  // `code` is an exact ascending copy (full match at len 10 -> 0), `bad` is all 0xff (mismatch -> 1). Shorter lengths
  // over `code` mismatch on the first byte (-> 1).
  c3e_inputcode() {
    const ring = ptr('0x0083aba4');
    for (let k = 0; k < 10; k++) ring.add(k).writeU8(0x41 + k);
    const cur = ptr('0x004e42ec');
    Memory.protect(cur, 4, 'rw-');
    cur.writePointer(ptr('0x0083abad'));
    const code = Memory.alloc(16);
    for (let k = 0; k < 10; k++) code.add(k).writeU8(0x41 + k);
    const bad = Memory.alloc(16);
    for (let k = 0; k < 16; k++) bad.add(k).writeU8(0xff);
    return { obj: ring, code: code, bad: bad };
  },

  // resetRecordBank 0x00401000 fills 8 dwords at 0x004e6d20 + p*0x74 and 9 dwords at 0x004e6d70 + p*0x74 with -1.
  // Pre-fill the whole bank region (6 banks) with a non -1 pattern so the write to -1 is a visible state change.
  c3e_banks() {
    const b = ptr('0x004e6d20');
    for (let i = 0; i < 0x2c0 / 4; i++) b.add(i * 4).writeS32(0x1000 + i);
    return { obj: b };
  },

  // updatePairSnapshot 0x00409950: per pair p (stride 0x388) reads the id word (0x0059fc60) and counter word
  // (0x0059fc64), then the cached golfer attributes at 0x0057957c/80/8c/90/94 + id*0x100. Seed four pairs with
  // distinct ids and counters (counter 0 and counter != 0 exercise both attribute sources), zero the three output
  // arrays first, and seed attributes for ids 0..7.
  c3e_pairs() {
    const area = ptr('0x0059fc60');
    for (let i = 0; i < 0xbc0 / 4; i++) area.add(i * 4).writeS32(0);
    const cfg = [[0, 0, 2], [1, 3, 5], [2, 0, 7], [3, 5, 1]];  // [p, counter, id]
    for (const c of cfg) {
      const rec = c[0] * 0x388;
      area.add(rec + 0).writeS16(c[2]);   // 0x59fc60 id
      area.add(rec + 4).writeS16(c[1]);   // 0x59fc64 counter
    }
    for (let id = 0; id < 8; id++) {
      ptr('0x0057957c').add(id * 0x100).writeS32(0x1100 + id);
      ptr('0x00579580').add(id * 0x100).writeS32(0x2200 + id);
      ptr('0x0057958c').add(id * 0x100).writeS32(0x3300 + id);
      ptr('0x00579590').add(id * 0x100).writeS32(0x4400 + id);
      ptr('0x00579594').add(id * 0x100).writeS32(0x5500 + id);
    }
    return { obj: area };
  },

  // addGolferPair 0x004099f0: slot 0 is made free (-1) so the pair lands in slot 0; its write regions are zeroed.
  // Partner ids sit at 0x0057955a + g*0x100; the 0x40-dword golfer records at 0x005794b8 + id*0x100 are seeded with a
  // per-golfer pattern (which includes the attribute fields updatePairSnapshot reads, at +0xc4..+0xdc).
  c3e_addpair() {
    const sp = ptr('0x0059fc60');
    for (let i = 0; i < 0x390 / 4; i++) sp.add(i * 4).writeS32(0);
    sp.writeS16(-1);                      // slot 0 header free
    for (let id = 0; id < 8; id++) {
      const rec = ptr('0x005794b8').add(id * 0x100);
      for (let w = 0; w < 0x40; w++) rec.add(w * 4).writeS32((id << 16) | (w * 7 + 3));
      ptr('0x0057955a').add(id * 0x100).writeS16((id + 1) & 7);
    }
    return { obj: sp };
  },

  // updateRollingStats 0x00409bf0 sweeps the 256 records at 0x005736b0 (stride 0x24) using fields at base+8..+0x20.
  // Seed live and free records (liveness base+8, -1 on every 7th) with varied accumulators so the fold/abs/decay,
  // the reset-when-<=0 and the free-when-stale (global date 0x00834170) paths all run.
  c3e_rolling() {
    const base = ptr('0x005736b0');
    for (let i = 0; i < 256; i++) {
      const r = base.add(i * 0x24);
      r.add(8).writeS32((i % 7 === 0) ? -1 : (i * 13));  // base+8  = [esi-4] liveness
      r.add(0xc).writeS32((i * 5) - 300);                // base+0xc  = [esi]
      r.add(0x10).writeS32((i * 9) + 4);                 // base+0x10 = [esi+4] accumulator
      r.add(0x14).writeS32((i * 3) & 0xff);              // base+0x14 = [esi+8] fold/abs arg
      r.add(0x18).writeS32((i * 17) - 500);              // base+0x18 = [esi+0xc]
      r.add(0x1c).writeS32(((i % 5) * 40) - 80);         // base+0x1c = [esi+0x10] counter
      r.add(0x20).writeS32(i * 2);                       // base+0x20 = [esi+0x14] date stamp
    }
    ptr('0x00834170').writeS32(0x4000);                  // global date
    return { obj: base };
  },
});

// c3i fix-up (2026-10-06): wider fixtures for two leaves that were held back for too few vectors (the leaf gate needs
// >= 10). Both reach genuinely distinct final states per vector, so the extra vectors are evidence, not padding.
Object.assign(globalThis.DIFF_FIXTURES, {
  // resetRecordBank 0x00401000 writes bank p (8 dwords at 0x004e6d20 + p*0x74, then 9 dwords at 0x004e6d70 + p*0x74)
  // to -1. p = 0..9 land within 0x004e6d20..0x004e71a8 (bank 9's second run ends at +0x488), so a 0x490-byte region
  // covers every write. Pre-fill it with a non -1 pattern so each bank's two runs are a visible change; each p writes
  // a different sub-range, so the 10 vectors give 10 distinct final states.
  c3i_resetbank() {
    const b = ptr('0x004e6d20');
    for (let i = 0; i < 0x490 / 4; i++) b.add(i * 4).writeS32(0x1000 + i);
    return { obj: b };
  },

  // updatePairSnapshot 0x00409950 records one event for pair p (stride 0x388); g_storyPairs at 0x0059fc60 holds exactly
  // 10 pairs (end 0x005a34e0, 0x3880 bytes). Seed all 10 with distinct ids (id = p) and a counter that cycles 0/2/4,
  // so both the counter==0 and counter!=0 attribute sources run and the write slot rec+counter*4 varies; zero the whole
  // array first and seed the cached golfer attributes at 0x0057957c/80/8c/90/94 + id*0x100 for ids 0..9. Each p writes a
  // different pair record, so the 10 vectors give 10 distinct final states.
  c3i_pairs() {
    const area = ptr('0x0059fc60');
    for (let i = 0; i < 0x3880 / 4; i++) area.add(i * 4).writeS32(0);
    for (let p = 0; p < 10; p++) {
      const rec = p * 0x388;
      area.add(rec + 0).writeS16(p);              // 0x59fc60 golfer id
      area.add(rec + 4).writeS16((p % 3) * 2);    // 0x59fc64 counter: 0,2,4 cycling
    }
    for (let id = 0; id < 10; id++) {
      ptr('0x0057957c').add(id * 0x100).writeS32(0x1100 + id);
      ptr('0x00579580').add(id * 0x100).writeS32(0x2200 + id);
      ptr('0x0057958c').add(id * 0x100).writeS32(0x3300 + id);
      ptr('0x00579590').add(id * 0x100).writeS32(0x4400 + id);
      ptr('0x00579594').add(id * 0x100).writeS32(0x5500 + id);
    }
    return { obj: area };
  },
});
