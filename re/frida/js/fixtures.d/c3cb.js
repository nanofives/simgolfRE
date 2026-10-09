// Batch c3cb fixtures (jgld.dll): the zlib 1.0.2 inflate chain.
//   inflate_trees_fixed 0x1009f030, falloc 0x1009f210, inflate_trees_bits 0x1009e710,
//   inflate_trees_dynamic 0x1009ef00, inflate_codes_new 0x1009f2f0, inflate_codes_free 0x100a00e0,
//   inflate_blocks_reset 0x1009cc70, inflate_blocks_new 0x1009cd90, inflate_blocks_free 0x1009e290,
//   inflateReset 0x1009bdd0, inflateEnd 0x1009be70, inflateInit2_ 0x1009bf10, inflateInit_ 0x1009c0b0.
//
// THE ONE IDEA THAT MAKES THIS FAMILY A/B-ABLE. zlib never calls the heap: every allocation goes through
// the z_stream's zalloc at +0x20 and every free through its zfree at +0x24, both with the opaque at
// +0x28, and the z_stream belongs to the CALLER. Every z_stream here is ours and carries
//   * a SLOT allocator: `void* alloc(ctl, items, size)` returns ctl->slots[ctl->i++] and ignores the
//     size, so the fixture decides exactly which allocation of a call succeeds and which returns NULL
//     (that is how every Z_MEM_ERROR arm is reached without starving the real heap), and
//   * a RECORDING deallocator: `void free(ctl, p)` appends p to ctl->freeLog and frees nothing.
// Nothing reaches operator new or operator delete, the two arms get the same addresses in the same order
// (diff_hook restores the state regions between the arms, so ctl->i is back at 0 for the second arm) and
// the ORDER of the allocations and frees is part of the compared state.
//
// ctl layout, 0x100 bytes, one per vector: i +0, slots[32] +4..+0x83, freeIdx +0x84,
// freeLog[24] +0x88..+0xe7. Six frees is the most any one call here makes (inflateEnd of a stream whose
// block state is in mode CODES), so the log cannot run into the next ctl.
//
// WHAT IS IN THE COMPARED STATE AND WHAT IS NOT. Each key's regions are the z_streams, the ctls (the
// allocation index and the free log) and a per-vector PARM slice holding everything a body under test
// writes: the block states, the internal_states, the code-length arrays and the bl/bd/tl/td out-dwords.
// The POOL the slot allocator hands out for huft tables and for sliding windows is deliberately NOT a
// region: no body in this batch writes a window byte (they move the window/read/write/end POINTERS, which
// live in the block state), and the huft tables are huft_build's own output, identical in both arms and
// never read back by it - including megabytes of them would cost time and add no evidence. What the
// tables prove here is carried by the pointers the parm slice records and by the free log.
//
// Nothing live in the running jgld.dll is handed to any of these bodies. The one exception is deliberate
// and measured: inflate_trees_fixed writes the module storage at 0x10128ae8..0x10129b8b (fixed_td,
// fixed_tl, the 530-entry fixed_mem, fixed_bd, fixed_bl, fixed_built), which it has to, and a 20 s probe
// at the menu entered inflate_trees_fixed, inflate_blocks, inflate, inflateInit_, huft_build and
// png_read_row 0 times each with fixed_built sampled as 0 throughout (log/c3/alt/c3cb_probe.py), so the
// game is not in this code while the A/B runs.
const C3CB = {
  JGLD: null,
  va(a) {
    if (!C3CB.JGLD) C3CB.JGLD = Process.getModuleByName('jgld.dll').base;
    return C3CB.JGLD.add(a - 0x10000000);
  },

  // A per-offset word pattern, so a store one field too wide or a field left unwritten changes the hash.
  fill(p, n, seed) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); },

  // The two __cdecl stubs, assembled by hand. Both leave esp as they found it, which is what the
  // original's `cmp esi,esp; call __chkesp` after every indirect call checks.
  stubs: null,
  mkStubs() {
    if (C3CB.stubs) return C3CB.stubs;
    const code = Memory.alloc(Process.pageSize);
    Memory.protect(code, Process.pageSize, 'rwx');
    // alloc(ctl, items, size) -> ctl->slots[ctl->i++]; the size is ignored (the fixture sizes the slot)
    code.writeByteArray([
      0x8B, 0x4C, 0x24, 0x04,              // mov ecx, [esp+4]           ctl (the opaque)
      0x8B, 0x01,                          // mov eax, [ecx]             i
      0x8B, 0x54, 0x81, 0x04,              // mov edx, [ecx+eax*4+4]     slots[i]
      0x40,                                // inc eax
      0x89, 0x01,                          // mov [ecx], eax             i + 1
      0x8B, 0xC2,                          // mov eax, edx
      0xC3,                                // ret
    ]);
    // free(ctl, p) -> ctl->freeLog[ctl->freeIdx++] = p; nothing is released
    code.add(0x20).writeByteArray([
      0x8B, 0x4C, 0x24, 0x04,                          // mov ecx, [esp+4]            ctl
      0x8B, 0x44, 0x24, 0x08,                          // mov eax, [esp+8]            p
      0x8B, 0x91, 0x84, 0x00, 0x00, 0x00,              // mov edx, [ecx+0x84]         freeIdx
      0x89, 0x84, 0x91, 0x88, 0x00, 0x00, 0x00,        // mov [ecx+edx*4+0x88], eax
      0x42,                                            // inc edx
      0x89, 0x91, 0x84, 0x00, 0x00, 0x00,              // mov [ecx+0x84], edx
      0xC3,                                            // ret
    ]);
    C3CB.stubs = { alloc: code, free: code.add(0x20) };
    return C3CB.stubs;
  },

  // One arena for a key: n z_streams at a 0x40 stride, n ctls at 0x100, n parm slices at `parm` (all
  // three compared) and n pool slices at `pool` (not compared; `pool` may be 0).
  arena(n, parm, pool, seed) {
    const a = {
      n: n,
      zBlk: Memory.alloc(n * 0x40),
      ctlBlk: Memory.alloc(n * 0x100),
      parmBlk: Memory.alloc(n * parm),
      poolBlk: pool ? Memory.alloc(n * pool) : null,
    };
    C3CB.fill(a.zBlk, n * 0x40, seed);
    C3CB.fill(a.ctlBlk, n * 0x100, seed + 1);
    C3CB.fill(a.parmBlk, n * parm, seed + 2);
    a.z = (i) => a.zBlk.add(i * 0x40);
    a.ctl = (i) => a.ctlBlk.add(i * 0x100);
    a.parm = (i) => a.parmBlk.add(i * parm);
    a.pool = (i) => a.poolBlk.add(i * pool);
    // slots: an array of NativePointer or null (null = that allocation returns NULL)
    a.setCtl = (i, slots) => {
      const c = a.ctl(i);
      c.writeU32(0);
      for (let k = 0; k < 32; k++) c.add(4 + k * 4).writePointer((slots && slots[k]) || ptr(0));
      c.add(0x84).writeU32(0);
      for (let k = 0; k < 24; k++) c.add(0x88 + k * 4).writeU32(0);
      return c;
    };
    // opts: noAlloc / noFree leave that pointer null (the guards inflateEnd and inflateInit2_ test)
    a.setZ = (i, slots, opts) => {
      const o = opts || {};
      const z = a.z(i);
      const ctl = a.setCtl(i, slots);
      z.add(0x20).writePointer(o.noAlloc ? ptr(0) : C3CB.mkStubs().alloc);
      z.add(0x24).writePointer(o.noFree ? ptr(0) : C3CB.mkStubs().free);
      z.add(0x28).writePointer(ctl);
      return z;
    };
    return a;
  },

  // An inflate_blocks_state (0x3c) laid out at p. Offsets from shim/src/re/c3cb.cpp.
  blocks(p, o) {
    p.writeU32(o.mode);                                     // +0    mode
    p.add(0x04).writePointer(o.tl || ptr(0));               // +4    sub.decode.tl
    p.add(0x08).writePointer(o.td || ptr(0));               // +8    sub.decode.td
    p.add(0x0c).writePointer(o.codes || ptr(0));            // +0xc  sub.decode.codes / sub.trees.blens
    p.add(0x10).writeU32(0x31310000);
    p.add(0x14).writeU32(0x32320000);
    p.add(0x18).writeU32(0x33330000);                       // +0x18 last
    p.add(0x1c).writeU32(0x34340000);                       // +0x1c bitk, cleared by reset
    p.add(0x20).writeU32(0x35350000);                       // +0x20 bitb, cleared by reset
    p.add(0x24).writePointer(o.window || ptr(0));           // +0x24 window
    p.add(0x28).writePointer(o.end || ptr(0));              // +0x28 end
    p.add(0x2c).writeU32(0x36360000);                       // +0x2c read, overwritten by reset
    p.add(0x30).writeU32(0x37370000);                       // +0x30 write, overwritten by reset
    p.add(0x34).writePointer(o.checkfn || ptr(0));          // +0x34 checkfn
    p.add(0x38).writeU32(o.check === undefined ? 0x38380000 : o.check);   // +0x38 check
    return p;
  },

  // An internal_state (0x18): mode, sub, nowrap, wbits, blocks.
  istate(p, o) {
    p.writeU32(o.mode === undefined ? 0x41410000 : o.mode);
    p.add(0x04).writeU32(0x42420000);
    p.add(0x08).writeU32(0x43430000);
    p.add(0x0c).writeU32(o.nowrap || 0);
    p.add(0x10).writeU32(o.wbits === undefined ? 0x44440000 : o.wbits);
    p.add(0x14).writePointer(o.blocks || ptr(0));
    return p;
  },

  // A chain of huft tables the way huft_build links them, so inflate_trees_free has something to walk:
  // each table is preceded by a dummy entry whose `next` at t-4 points at the following table.
  huftChain(base, k, tag) {
    const BLK = 0x40;
    for (let j = 0; j < k; j++) {
      const b = base.add(j * BLK);
      b.writeU32((tag + j) >>> 0);
      b.add(4).writePointer(j + 1 < k ? base.add((j + 1) * BLK + 8) : ptr(0));
      for (let e = 8; e < BLK; e += 4) b.add(e).writeU32((tag + 0x1000 + j * 0x10 + e) >>> 0);
    }
    return k > 0 ? base.add(8) : ptr(0);
  },

  // A code-length array for huft_build: `pairs` is a list of [index, value] over n zeroed entries.
  lens(p, n, pairs) {
    for (let i = 0; i < n; i++) p.add(i * 4).writeU32(0);
    if (pairs) pairs.forEach((kv) => p.add(kv[0] * 4).writeU32(kv[1]));
    return p;
  },
};

Object.assign(globalThis.DIFF_FIXTURES, {

  // --------------------------------------------------------------- falloc 0x1009f210 (12 vectors)
  // falloc only reads and writes the counter the caller points it at and returns fixed_mem + counter*8.
  // The counters here are ours, not inflate_trees_fixed's stack slot, so nothing of the module's is
  // touched: the returned pointer is never dereferenced, by the function or by the A/B.
  c3cb_falloc() {
    const out = {};
    const blk = Memory.alloc(12 * 0x10);
    C3CB.fill(blk, 12 * 0x10, 0x6101);
    out.faBlk = blk;
    // counter values: the real sweep starts at 530 and comes down, plus 0 and one smaller than n
    const COUNT = [530, 530, 401, 273, 130, 33, 9, 1, 0, 0x212, 64, 7];
    COUNT.forEach((v, i) => {
      const p = blk.add(i * 0x10);
      p.writeU32(v);
      out['fa' + i] = p;
    });
    return out;
  },

  // ------------------------------------------------- inflate_trees_fixed 0x1009f030, UNBUILT (10)
  // fixed_built 0x10129b88 is set to 0 and the whole module block 0x10128ae8..0x10129b8b is given a
  // pattern, so every vector runs the full build: two huft_build calls handing 530 entries out of
  // fixed_mem through falloc. Each vector writes its four results into a different 0x20 slice of our own
  // out block, which is what makes the vectors distinct - the module block itself comes out identical
  // every time, because the build is deterministic.
  c3cb_fixed_unbuilt() {
    const out = {};
    const mod = C3CB.va(0x10128ae8);
    out.fixedMod = mod;
    C3CB.fill(mod, 0x10a4, 0x6202);                 // fixed_td, fixed_tl, fixed_mem, fixed_bd, fixed_bl
    mod.add(0x10129b88 - 0x10128ae8).writeU32(0);   // fixed_built = 0: every vector rebuilds
    const blk = Memory.alloc(10 * 0x20);
    C3CB.fill(blk, 10 * 0x20, 0x6203);
    out.fixedOut = blk;
    for (let i = 0; i < 10; i++) {
      const p = blk.add(i * 0x20);
      out['fx' + i + 'bl'] = p;
      out['fx' + i + 'bd'] = p.add(4);
      out['fx' + i + 'tl'] = p.add(8);
      out['fx' + i + 'td'] = p.add(0xc);
    }
    return out;
  },

  // --------------------------------------------- inflate_trees_fixed 0x1009f030, ALREADY BUILT (10)
  // fixed_built is 1 and the four module results carry markers of ours, so the guard at 0x1009f04e skips
  // the build and the body is only the four copies at 0x1009f1cd-0x1009f1ed.
  c3cb_fixed_built() {
    const out = {};
    const mod = C3CB.va(0x10128ae8);
    out.fixedMod = mod;
    C3CB.fill(mod, 0x10a4, 0x6302);
    const at = (va) => mod.add(va - 0x10128ae8);
    at(0x10128ae8).writeU32(0x6a6a0001);         // fixed_td
    at(0x10128aec).writeU32(0x6a6a0002);         // fixed_tl
    at(0x10129b80).writeU32(0x0000000d);         // fixed_bd, a value the build would never leave
    at(0x10129b84).writeU32(0x0000000b);         // fixed_bl
    at(0x10129b88).writeU32(1);                  // fixed_built = 1
    const blk = Memory.alloc(10 * 0x20);
    C3CB.fill(blk, 10 * 0x20, 0x6303);
    out.fixedOut = blk;
    for (let i = 0; i < 10; i++) {
      const p = blk.add(i * 0x20);
      out['fb' + i + 'bl'] = p;
      out['fb' + i + 'bd'] = p.add(4);
      out['fb' + i + 'tl'] = p.add(8);
      out['fb' + i + 'td'] = p.add(0xc);
    }
    return out;
  },

  // ------------------------------------------------------- inflate_trees_bits 0x1009e710 (11 vectors)
  // Eleven 19-entry code-length arrays chosen so huft_build returns each of its four codes:
  //   Z_OK          all lengths zero (its c[0] == n shortcut, which leaves *tb NULL and *bb 0), a
  //                 complete 4-bit set, a complete 2-bit set, a complete 3-bit set and two complete
  //                 mixed sets
  //   Z_DATA_ERROR  three and four codes of one bit (oversubscribed) -> z->msg = 0x10120978
  //   Z_BUF_ERROR   nineteen 5-bit codes, and sixteen 5-bit plus three 4-bit codes (incomplete, g != 1)
  //                 -> the arm at 0x1009e75d, which frees *tb through our stub and rewrites r
  //   Z_MEM_ERROR   the first slot is NULL, so huft_build's ZALLOC fails and the value falls past both
  //                 compares untouched
  //
  // WHY EVERY INCOMPLETE SET HERE HAS ALL NINETEEN LENGTHS NONZERO. When the set is incomplete,
  // huft_build adds y dummy codes (`c[i] += y`) and then walks more codes than it put values in v[]. Its
  // only guard is `if (p >= v + n)`, which counts ALL n lengths rather than the nonzero ones, so a set
  // with zero lengths makes it read v[] entries it never wrote - 0xcccccccc in this /GZ debug build -
  // and index the caller's base and extra lists with it. For inflate_trees_bits those lists are NULL, so
  // the ORIGINAL faults: a first run of this batch with the incomplete sets [[0,1],[5,3]] and
  // [[2,2],[9,4]] died at `e[*p - s]` with e = 0 and *p = 0xcccccccc, address
  // (0xcccccccc - 0x13) * 4 = 0x333332e4, exactly what diff_hook reported. Keeping every length nonzero
  // makes the dummy codes reach `p >= v + n` and take the invalid-code arm instead, which is the path a
  // real caller with 19 code-length codes takes.
  //
  // bb stays at 4 or 5, so huft_build never needs more than one table and it fits in a 0x200 slot.
  c3cb_trees_bits() {
    const out = {};
    const N = 11, SLOTS = 6, SLOT = 0x200;
    const a = C3CB.arena(N, 0x80, SLOTS * SLOT, 0x6401);
    out.tbZ = a.zBlk; out.tbCtl = a.ctlBlk; out.tbParm = a.parmBlk;
    const C4 = []; for (let k = 0; k < 16; k++) C4.push([k, 4]);
    const C3 = []; for (let k = 0; k < 8; k++) C3.push([k, 3]);
    const ALL5 = []; for (let k = 0; k < 19; k++) ALL5.push([k, 5]);
    const MIX5 = []; for (let k = 0; k < 19; k++) MIX5.push([k, k < 3 ? 4 : 5]);
    //      lengths                                           bb  allocations work
    const V = [
      [[], 5, true],                                       // all zero -> Z_OK, *tb = NULL, *bb = 0
      [C4, 5, true],                                       // complete 4-bit set -> Z_OK
      [[[0, 2], [1, 2], [2, 2], [3, 2]], 5, true],         // complete 2-bit set -> Z_OK
      [[[0, 1], [1, 2], [2, 2]], 5, true],                 // complete mixed set -> Z_OK
      [[[0, 1], [1, 1], [2, 1], [3, 1]], 5, true],         // oversubscribed -> Z_DATA_ERROR
      [[[0, 1], [1, 1], [2, 1]], 5, true],                 // oversubscribed -> Z_DATA_ERROR
      [ALL5, 5, true],                                     // incomplete (y = 13) -> Z_BUF_ERROR
      [MIX5, 5, true],                                     // incomplete (y = 10) -> Z_BUF_ERROR
      [[[0, 2], [1, 2], [2, 2], [3, 2]], 5, false],        // no slot -> Z_MEM_ERROR
      [C3, 4, true],                                       // complete 3-bit set -> Z_OK
      [[[0, 1], [1, 2], [2, 3], [3, 4], [4, 4]], 5, true], // complete mixed set -> Z_OK
    ];
    V.forEach((v, i) => {
      const p = a.parm(i), pool = a.pool(i);
      const slots = [];
      for (let k = 0; k < SLOTS; k++) slots.push(v[2] ? pool.add(k * SLOT) : null);
      a.setZ(i, slots);
      C3CB.lens(p, 19, v[0]);                      // c[19] at +0..+0x4b
      p.add(0x4c).writeU32(v[1]);                  // bb
      p.add(0x50).writeU32(0x4b4b0000 + i);        // tb, overwritten by huft_build
      out['tbC' + i] = p;
      out['tbB' + i] = p.add(0x4c);
      out['tbT' + i] = p.add(0x50);
      out['tbS' + i] = a.z(i);
    });
    return out;
  },

  // --------------------------------------------------- inflate_trees_dynamic 0x1009ef00 (11 vectors)
  // The same huft_build outcomes, now for each of the two builds, so every arm of both error blocks and
  // the shared tail at 0x1009f001 runs:
  //   literal Z_OK and distance Z_OK                    -> returns 0 at 0x1009f018
  //   literal Z_DATA_ERROR / Z_BUF_ERROR / Z_MEM_ERROR  -> returns at 0x1009ef87 WITHOUT freeing *tl
  //   distance Z_DATA_ERROR / Z_BUF_ERROR / Z_MEM_ERROR -> the tail at 0x1009f001 frees *tl as well
  // bl and bd stay at 4 or 5 so each build needs one table; the slot counts are what force the two
  // Z_MEM_ERROR arms (0 slots fails the literal build, 1 slot fails the distance one).
  //
  // As in c3cb_trees_bits, every INCOMPLETE set here gives all nl (or all nd) lengths a nonzero value,
  // because huft_build's `p >= v + n` guard counts all n lengths and not the nonzero ones: with zero
  // lengths present it walks past the values it stored and indexes the base and extra lists with the
  // /GZ stack fill 0xcccccccc. The first run of this batch died in the ORIGINAL arm that way, at
  // `e[*p - s]` = cplext + (0xcccccccc - 0x101) * 4 (reported address 0x36dda264 with jgld.dll at
  // 0x03980000). With every length nonzero the dummy codes take the invalid-code arm instead.
  c3cb_trees_dyn() {
    const out = {};
    const N = 11, SLOTS = 6, SLOT = 0x200;
    const a = C3CB.arena(N, 0x100, SLOTS * SLOT, 0x6501);
    out.tdZ = a.zBlk; out.tdCtl = a.ctlBlk; out.tdParm = a.parmBlk;
    const C4 = []; for (let k = 0; k < 16; k++) C4.push([k, 4]);
    const C3L = []; for (let k = 0; k < 8; k++) C3L.push([k, 3]);
    const C2 = [[0, 2], [1, 2], [2, 2], [3, 2]];
    const OVER = [[0, 1], [1, 1], [2, 1], [3, 1]];
    const INC16 = []; for (let k = 0; k < 16; k++) INC16.push([k, 5]);   // 16 of 5 bits: y = 16
    const INC4 = []; for (let k = 0; k < 4; k++) INC4.push([k, 3]);      // 4 of 3 bits: y = 4
    const MIX8 = [[0, 2], [1, 2], [2, 3], [3, 3], [4, 4], [5, 4], [6, 4], [7, 4]];   // complete
    //      nl  nd  literal lengths  distance lengths  bl bd  slots that work
    const V = [
      [16, 4, C4, C2, 5, 5, SLOTS],        // both Z_OK
      [16, 4, OVER, C2, 5, 5, SLOTS],      // literal Z_DATA_ERROR
      [16, 4, INC16, C2, 5, 5, SLOTS],     // literal Z_BUF_ERROR, frees *tl at 0x1009ef6e
      [16, 4, C4, OVER, 5, 5, SLOTS],      // distance Z_DATA_ERROR, tail frees *tl
      [16, 4, C4, INC4, 5, 5, SLOTS],      // distance Z_BUF_ERROR, frees *td then *tl
      [16, 4, C4, C2, 5, 5, 0],            // literal Z_MEM_ERROR, no free
      [16, 4, C4, C2, 5, 5, 1],            // distance Z_MEM_ERROR, tail frees *tl
      [8, 8, C3L, C3L, 4, 4, SLOTS],       // both Z_OK, a different shape
      [4, 4, C2, C2, 5, 5, SLOTS],         // both Z_OK, the smallest pair
      [16, 8, C4, C3L, 5, 4, SLOTS],       // both Z_OK, different depths
      [8, 4, MIX8, C2, 5, 5, SLOTS],       // both Z_OK, a mixed literal tree
    ];
    V.forEach((v, i) => {
      const p = a.parm(i), pool = a.pool(i);
      const slots = [];
      for (let k = 0; k < SLOTS; k++) slots.push(k < v[6] ? pool.add(k * SLOT) : null);
      a.setZ(i, slots);
      const nl = v[0], nd = v[1];
      for (let k = 0; k < nl + nd; k++) p.add(k * 4).writeU32(0);      // c[nl + nd] at +0
      v[2].forEach((kv) => p.add(kv[0] * 4).writeU32(kv[1]));
      v[3].forEach((kv) => p.add((nl + kv[0]) * 4).writeU32(kv[1]));
      const tail = p.add(0xc0);
      tail.writeU32(v[4]);                           // bl
      tail.add(4).writeU32(v[5]);                    // bd
      tail.add(8).writeU32(0x4c4c0000 + i);          // tl
      tail.add(0xc).writeU32(0x4d4d0000 + i);        // td
      out['tdNl' + i] = nl;
      out['tdNd' + i] = nd;
      out['tdC' + i] = p;
      out['tdBl' + i] = tail;
      out['tdBd' + i] = tail.add(4);
      out['tdTl' + i] = tail.add(8);
      out['tdTd' + i] = tail.add(0xc);
      out['tdS' + i] = a.z(i);
    });
    return out;
  },

  // ---------------------------------------------------- inflate_codes_new 0x1009f2f0 (12 vectors)
  // Twelve allocations, three of which fail (slot NULL), so the guard at 0x1009f328 is taken both ways.
  // bl and bd are passed values above 255 in four vectors, which is how the byte stores at 0x1009f33d
  // and 0x1009f346 show that only the low byte reaches the block.
  c3cb_codes_new() {
    const out = {};
    const N = 12;
    const a = C3CB.arena(N, 0x100, 0, 0x6601);
    out.cnZ = a.zBlk; out.cnCtl = a.ctlBlk; out.cnParm = a.parmBlk;
    //      bl          bd          allocation succeeds
    const V = [
      [7, 5, true], [9, 6, true], [0, 0, true], [255, 255, true],
      [0x100, 0x1ff, true], [0x3ff, 0x107, true], [1, 2, false],
      [8, 4, false], [0x12345678, 0x9abcdef0, true], [3, 3, true],
      [6, 0, false], [0xff00, 0x00ff, true],
    ];
    V.forEach((v, i) => {
      const p = a.parm(i);
      a.setZ(i, [v[2] ? p.add(0x80) : null]);        // the 0x1c block lands at parm+0x80
      out['cnBl' + i] = v[0];
      out['cnBd' + i] = v[1];
      out['cnTl' + i] = p.add(0x10 + i * 4);         // a pointer value, never dereferenced
      out['cnTd' + i] = p.add(0x50 + i * 4);
      out['cnS' + i] = a.z(i);
    });
    return out;
  },

  // --------------------------------------------------- inflate_codes_free 0x100a00e0 (12 vectors)
  // The whole body is one ZFREE with no null test, so the twelve vectors are twelve different pointers
  // (one of them NULL) handed to our recording deallocator; the freeLog entry is the compared state.
  c3cb_codes_free() {
    const out = {};
    const N = 12;
    const a = C3CB.arena(N, 0x80, 0, 0x6701);
    out.cfZ = a.zBlk; out.cfCtl = a.ctlBlk; out.cfParm = a.parmBlk;
    for (let i = 0; i < N; i++) {
      a.setZ(i, []);
      out['cfP' + i] = (i === 5) ? 0 : a.parm(i).add(0x10 + i * 4);
      out['cfS' + i] = a.z(i);
    }
    return out;
  },

  // -------------------------------------------------- inflate_blocks_reset 0x1009cc70 (12 vectors)
  // Twelve block states covering every arm: checkfn null and the module's own adler32 0x1009e370 (the
  // guards at 0x1009cc8b and 0x1009cd45), mode BTREE 4 and DTREE 5 (the ZFREE of sub.trees.blens at
  // 0x1009ccbf), mode CODES 6 (inflate_codes_free then two inflate_trees_free, in that order) and modes
  // that take neither. The huft chains the CODES vectors hand over are linked the way huft_build links
  // its tables, so inflate_trees_free has a real chain to walk and record.
  c3cb_blocks_reset() {
    const out = {};
    const N = 12, PARM = 0x400;
    const a = C3CB.arena(N, PARM, 0, 0x6801);
    out.brZ = a.zBlk; out.brCtl = a.ctlBlk; out.brParm = a.parmBlk;
    const adler = C3CB.va(0x1009e370);
    //      mode  checkfn  blens/codes  tl chain  td chain
    const V = [
      [0, false, false, 0, 0], [0, true, false, 0, 0],
      [4, false, true, 0, 0], [5, true, true, 0, 0],
      [6, false, true, 2, 1], [6, true, true, 3, 2],
      [3, false, false, 0, 0], [9, true, false, 0, 0],
      [4, false, false, 0, 0], [6, true, false, 0, 0],
      [5, false, true, 0, 0], [0, true, true, 0, 0],
    ];
    V.forEach((v, i) => {
      const p = a.parm(i);
      a.setZ(i, []);
      const window = p.add(0x80);
      C3CB.blocks(p, {
        mode: v[0],
        checkfn: v[1] ? adler : ptr(0),
        codes: v[2] ? p.add(0x300) : ptr(0),
        tl: C3CB.huftChain(p.add(0x100), v[3], 0x7a7a0000 + i * 0x100),
        td: C3CB.huftChain(p.add(0x200), v[4], 0x7b7b0000 + i * 0x100),
        window: window, end: window.add(0x40),
        check: 0x39390000 + i,
      });
      out['brS' + i] = p;
      out['brZs' + i] = a.z(i);
      out['brC' + i] = p.add(0x390);               // the uLongf* the first arm writes
    });
    return out;
  },

  // ---------------------------------------------------- inflate_blocks_new 0x1009cd90 (12 vectors)
  // Two allocations per call: the 0x3c block state (from the compared parm slice) and the window (from
  // the pool). Three vectors fail the first - the function returns the NULL it got - and three fail the
  // second, where it hands the block state back through ZFREE and returns 0 rather than s. The rest
  // succeed with different window sizes and with and without adler32 as the check function, which is
  // what makes the inflate_blocks_reset call at 0x1009ce55 run its own check arm from here.
  c3cb_blocks_new() {
    const out = {};
    const N = 12, PARM = 0x100, POOL = 0x500;
    const a = C3CB.arena(N, PARM, POOL, 0x6901);
    out.bnZ = a.zBlk; out.bnCtl = a.ctlBlk; out.bnParm = a.parmBlk;
    const adler = C3CB.va(0x1009e370);
    //      w     checkfn  state slot ok  window slot ok
    const V = [
      [8, false, true, true], [16, true, true, true], [32, false, true, true],
      [64, true, true, true], [256, false, true, true], [1024, true, true, true],
      [8, false, false, true], [64, true, false, true], [256, false, false, true],
      [8, true, true, false], [128, false, true, false], [512, true, true, false],
    ];
    V.forEach((v, i) => {
      const p = a.parm(i), pool = a.pool(i);
      a.setZ(i, [v[2] ? p.add(0x40) : null, v[3] ? pool : null]);
      out['bnW' + i] = v[0];
      out['bnF' + i] = v[1] ? adler : 0;
      out['bnS' + i] = a.z(i);
    });
    return out;
  },

  // --------------------------------------------------- inflate_blocks_free 0x1009e290 (12 vectors)
  // No branch of its own: inflate_blocks_reset, then the window and the block state through ZFREE. The
  // twelve block states repeat the reset fixture's shapes, so the recorded free ORDER (whatever the reset
  // released first, then the window, then the state itself) differs from vector to vector.
  c3cb_blocks_free() {
    const out = {};
    const N = 12, PARM = 0x400;
    const a = C3CB.arena(N, PARM, 0, 0x6a01);
    out.bfZ = a.zBlk; out.bfCtl = a.ctlBlk; out.bfParm = a.parmBlk;
    const adler = C3CB.va(0x1009e370);
    const V = [
      [0, false, false, 0, 0], [0, true, false, 0, 0],
      [4, false, true, 0, 0], [5, true, true, 0, 0],
      [6, false, true, 2, 1], [6, true, true, 1, 3],
      [2, false, false, 0, 0], [8, true, false, 0, 0],
      [6, false, true, 4, 0], [6, true, false, 0, 2],
      [4, true, true, 0, 0], [0, false, false, 0, 0],
    ];
    V.forEach((v, i) => {
      const p = a.parm(i);
      a.setZ(i, []);
      const window = p.add(0x80);
      C3CB.blocks(p, {
        mode: v[0],
        checkfn: v[1] ? adler : ptr(0),
        codes: v[2] ? p.add(0x300) : ptr(0),
        tl: C3CB.huftChain(p.add(0x100), v[3], 0x7c7c0000 + i * 0x100),
        td: C3CB.huftChain(p.add(0x200), v[4], 0x7d7d0000 + i * 0x100),
        window: window, end: window.add(0x40),
        check: 0x3a3a0000 + i,
      });
      out['bfS' + i] = p;
      out['bfZs' + i] = a.z(i);
      out['bfC' + i] = p.add(0x390);
    });
    return out;
  },

  // ------------------------------------------------------------ inflateReset 0x1009bdd0 (12 vectors)
  // A null stream and a stream with a null state take the shared Z_STREAM_ERROR arm; the rest differ in
  // nowrap (which picks mode BLOCKS 7 or METHOD 0 without a branch, 0x1009be25-0x1009be32) and in the
  // block state the reset reaches, so the compared state carries both the stream fields it clears and
  // everything inflate_blocks_reset did underneath.
  c3cb_inflate_reset() {
    const out = {};
    const N = 12, PARM = 0x400;
    const a = C3CB.arena(N, PARM, 0, 0x6b01);
    out.irZ = a.zBlk; out.irCtl = a.ctlBlk; out.irParm = a.parmBlk;
    const adler = C3CB.va(0x1009e370);
    //      z null  state null  nowrap  block mode  checkfn
    const V = [
      [true, false, 0, 0, false], [false, true, 0, 0, false],
      [false, false, 0, 0, false], [false, false, 1, 0, false],
      [false, false, 0, 0, true], [false, false, 1, 0, true],
      [false, false, 0, 4, false], [false, false, 1, 6, true],
      [false, false, 7, 5, false], [false, false, 0, 2, true],
      [false, false, 1, 0, false], [false, true, 0, 0, false],
    ];
    V.forEach((v, i) => {
      const p = a.parm(i);
      const z = a.setZ(i, []);
      const window = p.add(0x80);
      const blocks = C3CB.blocks(p.add(0x100), {
        mode: v[3],
        checkfn: v[4] ? adler : ptr(0),
        codes: p.add(0x300),
        tl: ptr(0), td: ptr(0),
        window: window, end: window.add(0x40),
        check: 0x3b3b0000 + i,
      });
      const st = C3CB.istate(p.add(0x200), { nowrap: v[2], blocks: blocks });
      z.add(0x1c).writePointer(v[1] ? ptr(0) : st);
      out['irS' + i] = v[0] ? 0 : z;
    });
    return out;
  },

  // -------------------------------------------------------------- inflateEnd 0x1009be70 (12 vectors)
  // The three guards get a vector each (null stream, null state, null zfree) and the blocks test at
  // 0x1009bead is taken both ways, so the compared free order is either just the stream state or the
  // whole chain: whatever the block state owns, then its window, then the block state, then the stream
  // state. The block state is reached from the internal_state, which the function then frees last.
  c3cb_inflate_end() {
    const out = {};
    const N = 12, PARM = 0x400;
    const a = C3CB.arena(N, PARM, 0, 0x6c01);
    out.ieZ = a.zBlk; out.ieCtl = a.ctlBlk; out.ieParm = a.parmBlk;
    const adler = C3CB.va(0x1009e370);
    //      z null  state null  zfree null  has blocks  block mode  checkfn
    const V = [
      [true, false, false, true, 0, false], [false, true, false, true, 0, false],
      [false, false, true, true, 0, false], [false, false, false, false, 0, false],
      [false, false, false, true, 0, false], [false, false, false, true, 0, true],
      [false, false, false, true, 4, false], [false, false, false, true, 6, true],
      [false, false, false, false, 0, true], [false, false, false, true, 5, false],
      [false, false, false, true, 6, false], [false, false, true, true, 6, true],
    ];
    V.forEach((v, i) => {
      const p = a.parm(i);
      const z = a.setZ(i, [], { noFree: v[2] });
      const window = p.add(0x80);
      const blocks = C3CB.blocks(p.add(0x100), {
        mode: v[4],
        checkfn: v[5] ? adler : ptr(0),
        codes: p.add(0x300),
        tl: C3CB.huftChain(p.add(0x340), v[4] === 6 ? 1 : 0, 0x7e7e0000 + i * 0x100),
        td: ptr(0),
        window: window, end: window.add(0x40),
        check: 0x3c3c0000 + i,
      });
      const st = C3CB.istate(p.add(0x200), { nowrap: 0, blocks: v[3] ? blocks : ptr(0) });
      z.add(0x1c).writePointer(v[1] ? ptr(0) : st);
      out['ieS' + i] = v[0] ? 0 : z;
    });
    return out;
  },

  // ------------------------------------------------------------ inflateInit2_ 0x1009bf10 (16 vectors)
  // Every guard of the body except the zalloc one, which has its own key (c3cb_init2_zcalloc, below):
  // a null version, a wrong first version byte, a wrong stream_size, a null stream, a null zfree (which
  // installs zcfree 0x1009e6d0 and then SUCCEEDS, so nothing is ever freed through the real heap), a
  // failing state allocation, a negative w (nowrap), w below 8, w above 15, a negative w whose magnitude
  // is above 15, and both ways inflate_blocks_new can fail - which is what makes the inflateEnd call at
  // 0x1009c075 run and free the state the function had just allocated.
  // Slot 0 (the 0x18 internal_state) and slot 1 (the 0x3c block state) come from the compared parm
  // slice; slot 2 (the window, up to 1 << 15) comes from the pool.
  c3cb_init2() {
    const out = {};
    const N = 16, PARM = 0x100, POOL = 0x8000;
    const a = C3CB.arena(N, PARM, POOL, 0x6d01);
    out.i2Z = a.zBlk; out.i2Ctl = a.ctlBlk; out.i2Parm = a.parmBlk;
    const VERSION = Memory.alloc(16);
    VERSION.writeUtf8String('1.0.2');
    const BADVER = Memory.alloc(16);
    BADVER.writeUtf8String('0.9.9');
    out.i2Ver = VERSION;
    out.i2Bad = BADVER;
    //      w   version  size   z null  zfree null  state ok  blocks ok  window ok
    const V = [
      [10, 'null', 0x38, false, false, true, true, true],
      [10, 'bad', 0x38, false, false, true, true, true],
      [10, 'ok', 0x30, false, false, true, true, true],
      [10, 'ok', 0x38, true, false, true, true, true],
      [10, 'ok', 0x38, false, true, true, true, true],
      [10, 'ok', 0x38, false, false, false, true, true],
      [-10, 'ok', 0x38, false, false, true, true, true],
      [7, 'ok', 0x38, false, false, true, true, true],
      [16, 'ok', 0x38, false, false, true, true, true],
      [-20, 'ok', 0x38, false, false, true, true, true],
      [8, 'ok', 0x38, false, false, true, true, true],
      [15, 'ok', 0x38, false, false, true, true, true],
      [12, 'ok', 0x38, false, false, true, true, false],
      [12, 'ok', 0x38, false, false, true, false, true],
      [9, 'ok', 0x38, false, false, true, true, true],
      [-8, 'ok', 0x38, false, false, true, true, true],
    ];
    V.forEach((v, i) => {
      const p = a.parm(i), pool = a.pool(i);
      a.setZ(i, [v[5] ? p.add(0x40) : null, v[6] ? p.add(0x80) : null, v[7] ? pool : null],
             { noFree: v[4] });
      out['i2W' + i] = v[0];
      out['i2V' + i] = v[1] === 'null' ? 0 : (v[1] === 'bad' ? BADVER : VERSION);
      out['i2Sz' + i] = v[2];
      out['i2S' + i] = v[3] ? 0 : a.z(i);
    });
    return out;
  },

  // ------------------------------------------- inflateInit2_ 0x1009bf10, the zalloc guard (2 vectors)
  // The arm at 0x1009bf6c installs zcalloc 0x1009e680 in a stream that has no allocator, and everything
  // below it then allocates through the C runtime. Two blocks of 0x18 and 0x3c bytes and a 256-byte
  // window (w = 8) are therefore really allocated, once per arm, and those pointers differ between the
  // arms by construction - which is why this key's state regions cover the whole z_stream EXCEPT the
  // state pointer at +0x1c. What is compared is the zalloc store, the cleared opaque, the zfree store
  // (this stream has no deallocator either, so zcfree 0x1009e6d0 is installed as well), msg, total_in,
  // total_out and adler, plus the return value. The CONTENT of the two structures is compared by every
  // other vector of c3cb_init2, which allocates them from our own slots. Nothing is freed on this path,
  // so the ~0x350 bytes each arm takes are left behind on purpose rather than handed to zcfree, whose
  // argument would otherwise be one of our pool pointers.
  c3cb_init2_zcalloc() {
    const out = {};
    const blk = Memory.alloc(2 * 0x40);
    C3CB.fill(blk, 2 * 0x40, 0x6e01);
    out.zcBlk = blk;
    const VERSION = Memory.alloc(16);
    VERSION.writeUtf8String('1.0.2');
    out.zcVer = VERSION;
    for (let i = 0; i < 2; i++) {
      const z = blk.add(i * 0x40);
      z.add(0x20).writePointer(ptr(0));         // zalloc NULL: the arm under test
      z.add(0x24).writePointer(ptr(0));         // zfree NULL too, so zcfree is installed as well
      z.add(0x28).writeU32(0x5c5c0000 + i);     // opaque, cleared by the same arm
      out['zcS' + i] = z;
    }
    return out;
  },

  // ------------------------------------------------------------- inflateInit_ 0x1009c0b0 (12 vectors)
  // inflateInit_ has no conditional jump of its own: it is the fixed window size 0xf plus a forward. The
  // twelve vectors are therefore chosen for what comes BACK through inflateInit2_ - each of its return
  // codes - and four of them run the whole 32 KiB-window success path.
  c3cb_init() {
    const out = {};
    const N = 12, PARM = 0x100, POOL = 0x8000;
    const a = C3CB.arena(N, PARM, POOL, 0x6f01);
    out.i1Z = a.zBlk; out.i1Ctl = a.ctlBlk; out.i1Parm = a.parmBlk;
    const VERSION = Memory.alloc(16);
    VERSION.writeUtf8String('1.0.2');
    const BADVER = Memory.alloc(16);
    BADVER.writeUtf8String('0.9.9');
    out.i1Ver = VERSION;
    out.i1Bad = BADVER;
    //      version  size   z null  zfree null  state ok  blocks ok  window ok
    const V = [
      ['null', 0x38, false, false, true, true, true],
      ['bad', 0x38, false, false, true, true, true],
      ['ok', 0x00, false, false, true, true, true],
      ['ok', 0x38, true, false, true, true, true],
      ['ok', 0x38, false, false, false, true, true],
      ['ok', 0x38, false, false, true, false, true],
      ['ok', 0x38, false, false, true, true, false],
      ['ok', 0x38, false, false, true, true, true],
      ['ok', 0x38, false, true, true, true, true],
      ['ok', 0x39, false, false, true, true, true],
      ['ok', 0x38, false, false, false, true, true],
      ['ok', 0x38, false, false, true, true, true],
    ];
    V.forEach((v, i) => {
      const p = a.parm(i), pool = a.pool(i);
      a.setZ(i, [v[4] ? p.add(0x40) : null, v[5] ? p.add(0x80) : null, v[6] ? pool : null],
             { noFree: v[3] });
      out['i1V' + i] = v[0] === 'null' ? 0 : (v[0] === 'bad' ? BADVER : VERSION);
      out['i1Sz' + i] = v[1];
      out['i1S' + i] = v[2] ? 0 : a.z(i);
    });
    return out;
  },
});
