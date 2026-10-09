// Batch c3ch fixtures (jgld.dll): the top of the zlib 1.0.2 inflate chain and libpng 1.0.5's chunk reader.
//   huft_build 0x1009e7a0, inflate_blocks 0x1009ce80, inflate 0x1009c0f0,
//   png_read_data 0x10078c40, png_crc_read 0x100794b0, png_crc_error 0x10079620,
//   png_crc_finish 0x10079510.
//
// WHAT MAKES THE ZLIB THREE A/B-ABLE. zlib reaches memory only through the z_stream's zalloc at +0x20 and
// zfree at +0x24 with the opaque at +0x28, and the z_stream is the CALLER's. Every z_stream here is ours
// and carries c3cb's two hand-assembled stubs: a SLOT allocator that returns ctl->slots[ctl->i++] (so the
// fixture decides exactly which allocation of a call returns NULL, which is how every Z_MEM_ERROR arm is
// reached) and a RECORDING deallocator that appends the pointer to ctl->freeLog and releases nothing (so
// the ORDER of the frees is part of the compared state). Nothing reaches the process heap.
//
// WHAT MAKES THE PNG FOUR A/B-ABLE. libpng reads bytes only through the png_struct's read_data_fn at
// +0x50, which png_set_read_fn 0x10078cb0 (C3, c3bq) installs. Every scratch png_struct here carries a
// 35-byte __cdecl reader of ours that copies `length` bytes out of a buffer of ours, advances a cursor and
// counts the call; the cursor and the counter are compared state, so what each arm asked for and in which
// order is evidence. Each png_struct also carries a one-byte `ret` in the warning slot at +0x44 (c3bq's
// trick), which is what png_warning 0x10079040 calls instead of writing to a C stream.
//
// ALL THE COMPRESSED BYTES ARE REAL DEFLATE DATA, built by log/c3/c3ch_gen.py with Python's zlib or
// written bit by bit so that one chosen arm runs (an oversubscribed code-length tree, a repeat code with
// no previous length, HLIT 31, block type 3, a stored block whose complement is wrong). Nothing comes from
// the game's own files. The table below is that script's output.
const C3CH_DATA = {
  stored: '011000efff30313233343536373839414243444546',
  fixed: '4b4c848124200000',
  dyn: '0dce870183200000c1551c41e98c23d291aa82327df20bdc3f7e4ac5cc2b493af4b6aaa2f926179be532653eb8bb56cfec72f3d32cf1355c2d3261b63d10be77398b5e6e8a3bb894256be81eaf87e2cbd307864a37a4e4bd80eb1b028e60b14c972b73621dc0e4cd6dbcade3cb5e9f3ee71d9ff5dafdcadee57144423eff193465540904dba5d917ffbf142a7dbe8e17598b2142b7f6a65714a848f4f54cd05b57fa869119233dc73273a47808d1508d038c57679351ec09ed6868fb45746e5d4f3875084e78e8d1f09c3bb11beab698444d17294f65ab9303d29538e62911e0dcfde991e5f2a19423c36589ed300470a1c043486c8030440aad3c02b7da091e5536956eeb5f5f1dcc66e290c346665fe378f42b3a87407c9d5e44d96c746dc7d9dc56a76764c18f6ef2dc5413baeaacaa857be76b73072d365479c6fba6e902e46c43d463178447820a301b6693dc29f7e8e6906e57750b527fe42ad701cfd0c05edfce9f72dc699c7bff346a2416b2ff153f8f29ee3b6beabf07fbcb1f64238e9245642d0ef79a0f702958c175878a304d15aaaa969421fe14ba8ea87caf3f',
  minDyn: '05c081000000000010ffd508',
  overLit: '05200024ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff03',
  badBits: '05009204',
  incBits: '05e0dbb66ddbb66ddb02',
  badRepeat: '05000224',
  tooMany: 'fd0000',
  badType: '07',
  badLens: '0110000000',
  twoBlocks: '000000ffffaba8acaaaa0400',
  zOk: '78da4b4c84812420000027e10553',
  zDyn: '78da0d908711842000c0567104a5338e48ef584099fe3f0b2497c74fa9987925c987de565535dfe4628b5ca62c0777d7ea995d6e1ecd925ec3d5223366db03e17bd758f57253dcc1a52c5943f7783d145f9e3e3054fa444ade0bb8be21e00816cb7cb93a27d6014c7eba8d9febf88ad7d197b2e3d8aeddafec5d1e4724e4f38f4153269541b05d9a7df1ff4aa1f2e7db7891b51822746b6f7a43818a4c5fcf04bd75a36f188531d24baab3248a8710276a6980f1ea620a4a3da31d0d6dbf84e2d6f5845387e084871e0dcfb913bba16e4b59b47c91fa34b63a39205d89639e1201e2eea34796cb87528e0c97359d8721800b051e42d2090843a4d2c61370ab9de0517553f9b6fef5cdc162260e256c64f6358d47bfa27308c4d7e945942d46b7f388ff3f6d7a4616fce853c64d9d42375d54b370ef7c3ddd41ab0d4dc674df345f80c4738876ec82f044500566c36c923b979edc1cd2edaa6d41ea8f5cf53a600c27d8dbdbf9538f3b8fb8f74fa393a44af6bfc5cf638afb2e9afaefc1fef207d988a36411458bc3bde6035c0a5671dba1224c53859a6a3517883f85ae2329dfdb0f2f2ad70f',
  zBadMethod: '77da4b4c84812420000027e10553',
  zBadWindow: '88da4b4c84812420000027e10553',
  zBadCheck: '78db4b4c84812420000027e10553',
  zPresetDict: '78204b4c84812420000027e10553',
  zBadAdler: '78da4b4c84812420000027e105ac',
  btreeEntry: '40000000000088ff6a04',
  tableEntry: '0038100000000000e2bf1a01',
  btreeBadTree: '4902',
  tableTooMany: '1f00',
  tableHdist31: 'e003',
  tableOverrun: '0038100000000000f2ffff0f',
  fixedNotLast: '7204040400fbff5758595a',
  btreeTable: 0x3800,
  // png_crc_finish vectors: [skip, zbuf_size, initial png_ptr->crc, the bytes the reader serves].
  // The four bytes after the skipped ones are the big-endian crc32 of the skipped bytes folded into the
  // initial value (zlib's crc32 0x1009ca70, the one png_calculate_crc 0x100787d0 calls), xored with
  // 0xa5a5a5a5 where the vector is meant to disagree.
  crcFinish: [
    [0, 8, 0x00000000, '0000000000000000000000000000000000000000'],
    [0, 8, 0x12345678, 'b791f3dd00000000000000000000000000000000'],
    [1, 8, 0x00000000, '4a440b470300000000000000000000000000000000'],
    [7, 8, 0xdeadbeef, '6f7a85909ba6b1a004ed8100000000000000000000000000000000'],
    [8, 8, 0x00000001, '949faab5c0cbd6e190baf11500000000000000000000000000000000'],
    [9, 8, 0xcafebabe, 'b9c4cfdae5f0fb0611d55913f500000000000000000000000000000000'],
    [16, 8, 0x00000000, 'dee9f4ff0a15202b36414c57626d7883a1c2e4c400000000000000000000000000000000'],
    [17, 8, 0x5a5a5a5a, '030e19242f3a45505b66717c87929da8b375c153e000000000000000000000000000000000'],
    [23, 4, 0x11111111, '28333e49545f6a75808b96a1acb7c2cdd8e3eef9040f1ac2c2971e00000000000000000000000000000000'],
    [24, 4, 0x22222222, '4d58636e79848f9aa5b0bbc6d1dce7f2fd08131e29343f4ab0bdee6c00000000000000000000000000000000'],
    [3, 32, 0x33333333, '727d88bc50365900000000000000000000000000000000'],
    [64, 16, 0x44444444, '97a2adb8c3ced9e4effa05101b26313c47525d68737e89949faab5c0cbd6e1ecf7020d18232e39444f5a65707b86919ca7b2bdc8d3dee9f4ff0a15202b36414c3217277200000000000000000000000000000000'],
    [5, 8, 0x55555555, 'bcc7d2dde8e8fbab3500000000000000000000000000000000'],
    [0, 1, 0x66666666, '6666666600000000000000000000000000000000'],
    [2, 2, 0x77777777, '0611d32ce95200000000000000000000000000000000'],
    [100, 7, 0x88888888, '2b36414c57626d78838e99a4afbac5d0dbe6f1fc07121d28333e49545f6a75808b96a1acb7c2cdd8e3eef9040f1a25303b46515c67727d88939ea9b4bfcad5e0ebf6010c17222d38434e59646f7a85909ba6b1bcc7d2dde8f3fe09141f2a35404b56616c1073f44200000000000000000000000000000000'],
  ],
};

const C3CH = {
  JGLD: null,
  va(a) {
    if (!C3CH.JGLD) C3CH.JGLD = Process.getModuleByName('jgld.dll').base;
    return C3CH.JGLD.add(a - 0x10000000);
  },

  // A per-offset word pattern, so a store one field too wide or a field left unwritten changes the hash.
  fill(p, n, seed) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); },

  // A buffer holding `hex`, with `slack` zero bytes after it.
  buf(hex, slack) {
    const n = hex.length / 2;
    const p = Memory.alloc(n + (slack || 16));
    const a = new Uint8Array(n);
    for (let i = 0; i < n; i++) a[i] = parseInt(hex.substr(i * 2, 2), 16);
    if (n) p.writeByteArray(a.buffer);
    return { p: p, n: n };
  },

  // c3cb's two __cdecl stubs, assembled by hand. Both leave esp as they found it, which is what the
  // original's `cmp esi,esp; call __chkesp` after every indirect call checks.
  stubs: null,
  mkStubs() {
    if (C3CH.stubs) return C3CH.stubs;
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
    // A one-byte `ret` for the png_struct's warning slot at +0x44 (c3bq).
    code.add(0x40).writeByteArray([0xC3]);
    C3CH.stubs = { alloc: code, free: code.add(0x20), ret: code.add(0x40) };
    return C3CH.stubs;
  },

  // One __cdecl png read function per control block, with the block's address baked in:
  //   void reader(png_structp, png_bytep data, png_size_t length)
  //   { memcpy(data, ctl->src + ctl->cursor, length); ctl->cursor += length;
  //     ctl->calls++; ctl->lastLen = length; }
  // ctl: cursor +0, src +4, calls +8, lastLen +0xc. After the three pushes the arguments sit at
  // [esp+0x10] (png_ptr), [esp+0x14] (data) and [esp+0x18] (length).
  readers: null,
  mkReader(ctl) {
    if (!C3CH.readers) {
      const page = Memory.alloc(Process.pageSize);
      Memory.protect(page, Process.pageSize, 'rwx');
      C3CH.readers = { page: page, next: 0 };
    }
    const p = C3CH.readers.page.add(C3CH.readers.next * 0x30);
    C3CH.readers.next++;
    const c = ctl.toUInt32();
    p.writeByteArray([
      0x53,                                // push ebx
      0x56,                                // push esi
      0x57,                                // push edi
      0x8B, 0x7C, 0x24, 0x14,              // mov edi, [esp+0x14]       data
      0x8B, 0x4C, 0x24, 0x18,              // mov ecx, [esp+0x18]       length
      0xBB, c & 0xff, (c >>> 8) & 0xff, (c >>> 16) & 0xff, (c >>> 24) & 0xff,   // mov ebx, ctl
      0x8B, 0x73, 0x04,                    // mov esi, [ebx+4]          src
      0x03, 0x33,                          // add esi, [ebx]            + cursor
      0x01, 0x0B,                          // add [ebx], ecx            cursor += length
      0xFF, 0x43, 0x08,                    // inc dword [ebx+8]         calls++
      0x89, 0x4B, 0x0C,                    // mov [ebx+0xc], ecx        lastLen = length
      0xF3, 0xA4,                          // rep movsb
      0x5F, 0x5E, 0x5B,                    // pop edi / esi / ebx
      0xC3,                                // ret
    ]);
    return p;
  },

  // One arena for a key: n z_streams at a 0x40 stride and n ctls at 0x100 (allocation index and free log).
  arena(n, seed) {
    const a = { n: n, zBlk: Memory.alloc(n * 0x40), ctlBlk: Memory.alloc(n * 0x100) };
    C3CH.fill(a.zBlk, n * 0x40, seed);
    C3CH.fill(a.ctlBlk, n * 0x100, seed + 1);
    a.z = (i) => a.zBlk.add(i * 0x40);
    a.ctl = (i) => a.ctlBlk.add(i * 0x100);
    a.setZ = (i, slots) => {
      const z = a.z(i), c = a.ctl(i);
      c.writeU32(0);
      for (let k = 0; k < 32; k++) c.add(4 + k * 4).writePointer((slots && slots[k]) || ptr(0));
      c.add(0x84).writeU32(0);
      for (let k = 0; k < 24; k++) c.add(0x88 + k * 4).writeU32(0);
      z.add(0x20).writePointer(C3CH.mkStubs().alloc);
      z.add(0x24).writePointer(C3CH.mkStubs().free);
      z.add(0x28).writePointer(c);
      return z;
    };
    return a;
  },

  // An inflate_blocks_state (0x3c) at p. Offsets from shim/src/re/c3ch.cpp.
  blocks(p, o) {
    p.writeU32(o.mode);                                     // +0    mode
    p.add(0x04).writeU32(o.sub4 === undefined ? 0x31310000 : o.sub4);      // +4  left / table / tl
    p.add(0x08).writeU32(o.sub8 === undefined ? 0x32320000 : o.sub8);      // +8  index / td
    p.add(0x0c).writeU32(o.subC === undefined ? 0x33330000 : o.subC);      // +0xc blens / codes
    p.add(0x10).writeU32(o.bb === undefined ? 0x34340000 : o.bb);          // +0x10 trees.bb
    p.add(0x14).writeU32(o.tb === undefined ? 0 : o.tb);                   // +0x14 trees.tb
    p.add(0x18).writeU32(o.last || 0);                      // +0x18 last
    p.add(0x1c).writeU32(o.bitk || 0);                      // +0x1c bitk
    p.add(0x20).writeU32(o.bitb || 0);                      // +0x20 bitb
    p.add(0x24).writePointer(o.window);                     // +0x24 window
    p.add(0x28).writePointer(o.end);                        // +0x28 end
    p.add(0x2c).writePointer(o.read);                       // +0x2c read
    p.add(0x30).writePointer(o.write);                      // +0x30 write
    p.add(0x34).writePointer(o.checkfn || ptr(0));          // +0x34 checkfn
    p.add(0x38).writeU32(o.check === undefined ? 1 : o.check);             // +0x38 check
    return p;
  },

  // The module's fixed Huffman tables, built once through the module's own inflate_trees_fixed
  // 0x1009f030. The build is deterministic and writes only jgld.dll's own fixed_mem 0x10128af0 and the
  // five words around it; c3cb measured 0 entries into this code from the running game at the menu
  // (log/c3/alt/c3cb_probe.py), and after this call fixed_built 0x10129b88 is 1 for both arms.
  fixedTrees() {
    if (C3CH.fixed) return C3CH.fixed;
    const out = Memory.alloc(0x10);
    const f = new NativeFunction(C3CH.va(0x1009f030), 'int',
                                 ['pointer', 'pointer', 'pointer', 'pointer'], 'mscdecl');
    f(out, out.add(4), out.add(8), out.add(0xc));
    C3CH.fixed = { bl: out.readU32(), bd: out.add(4).readU32(),
                   tl: out.add(8).readPointer(), td: out.add(0xc).readPointer() };
    return C3CH.fixed;
  },

  // A code-length array for huft_build: `pairs` is a list of [index, value] over n zeroed entries.
  lens(p, n, pairs) {
    for (let i = 0; i < n; i++) p.add(i * 4).writeU32(0);
    if (pairs) pairs.forEach((kv) => p.add(kv[0] * 4).writeU32(kv[1]));
    return p;
  },
};

Object.assign(globalThis.DIFF_FIXTURES, {

  // ============================================================= huft_build 0x1009e7a0 (17 vectors)
  //
  // Seventeen code-length sets, each with its own z_stream, allocation control block, out words (*m and
  // *t) and table pool. The pool is handed out by the slot allocator in 0x120-byte pieces, which is one
  // byte more than the biggest table any of these sets needs (2^5 + 1 entries of eight bytes); the first
  // four pieces of each vector are compared state and the rest are not, so a build that needs more than
  // four tables still runs but only its first four are hashed.
  //
  // The sets, and the arm of the body each one is for:
  //   0  nineteen zero lengths          the c[0] == n shortcut at 0x1009e923: *t = NULL, *m = 0, Z_OK
  //   1  sixteen 4-bit codes            complete; *m 7 is clamped DOWN to the longest code (0x1009e9b2)
  //   2  four 2-bit codes, *m = 1       complete; *m is raised UP to the shortest code (0x1009e978)
  //   3  one 1-bit and two 2-bit codes  complete, mixed
  //   4  three 1-bit codes              oversubscribed at the final test 0x1009ea35 -> Z_DATA_ERROR
  //   5  three 1-bit and one 2-bit      oversubscribed INSIDE the loop 0x1009ea0d -> Z_DATA_ERROR
  //   6  nineteen 5-bit codes           incomplete: thirteen dummy codes take the `p >= v + n` arm at
  //                                     0x1009ed8f and the return is Z_BUF_ERROR (0x1009eece)
  //   7  one 1-bit code over n = 1      incomplete too, but g == 1, so 0x1009eecc returns Z_OK instead
  //   8  four 2-bit codes, no slot      the first ZALLOC fails with h == 0 (0x1009eccf not taken)
  //   9  1,2,3,3 with *m = 1, one slot  the SECOND ZALLOC fails with h != 0, so u[0] is freed
  //  10  1,2,3,3 with *m = 1            the same build with every slot: three table levels, so
  //                                     0x1009ed25 runs both ways and the backup loop 0x1009ee99 runs
  //  11  literal/length set, s = 257    values 0 and 1 take `*p < 256`, 256 takes `32 + 64`, 257 takes
  //                                     the cplens/cplext arm at 0x1009edd9
  //  12  thirty distance codes, s = 0   every value takes the cpdist/cpdext arm
  //  13  eight 3-bit codes, *m = 0      *m below the shortest code
  //  14  1,3,3,4,4,4,4 with *m = 2      a sparse multi-level set
  //  15  literal/length set, *m = 9     *m above the longest code
  //  16  1,2,3,5,6,6,6,6,6,7,7, *m = 4  the only set here whose smaller-table search at 0x1009ec79
  //                                     finds enough codes and BREAKS instead of deducting
  c3ch_huft() {
    const out = {};
    const N = 17, SLOT = 0x120, HOT = 4, COLD = 6;
    const a = C3CH.arena(N, 0x7101);
    const lensBlk = Memory.alloc(N * 0x480);              // b[], never written by the body
    const outBlk = Memory.alloc(N * 0x10);                // *m at +0, *t at +4
    const hot = Memory.alloc(N * HOT * SLOT);             // the first four tables of each vector
    const cold = Memory.alloc(N * COLD * SLOT);           // the rest, not compared
    C3CH.fill(lensBlk, N * 0x480, 0x7102);
    C3CH.fill(outBlk, N * 0x10, 0x7103);
    C3CH.fill(hot, N * HOT * SLOT, 0x7104);
    C3CH.fill(cold, N * COLD * SLOT, 0x7105);
    out.hbZ = a.zBlk; out.hbCtl = a.ctlBlk; out.hbOut = outBlk; out.hbHot = hot;

    const rep = (lo, hi, v) => { const r = []; for (let k = lo; k <= hi; k++) r.push([k, v]); return r; };
    const CPLENS = C3CH.va(0x101272bc), CPLEXT = C3CH.va(0x10127338);
    const CPDIST = C3CH.va(0x101273b4), CPDEXT = C3CH.va(0x1012742c);
    //      n    s    d/e            lengths                                            m   slots ok
    const V = [
      [19, 19, null, [], 7, 99],
      [19, 19, null, rep(0, 15, 4), 7, 99],
      [19, 19, null, rep(0, 3, 2), 1, 99],
      [19, 19, null, [[0, 1], [1, 2], [2, 2]], 7, 99],
      [19, 19, null, rep(0, 2, 1), 7, 99],
      [19, 19, null, [[0, 1], [1, 1], [2, 1], [3, 2]], 7, 99],
      [19, 19, null, rep(0, 18, 5), 7, 99],
      [1, 19, null, [[0, 1]], 7, 99],
      [19, 19, null, rep(0, 3, 2), 7, 0],
      [19, 19, null, [[0, 1], [1, 2], [2, 3], [3, 3]], 1, 1],
      [19, 19, null, [[0, 1], [1, 2], [2, 3], [3, 3]], 1, 99],
      [288, 257, 'lit', [[0, 2], [1, 2], [256, 2], [257, 2]], 7, 99],
      [30, 0, 'dist', rep(0, 1, 4).concat(rep(2, 29, 5)), 5, 99],
      [19, 19, null, rep(0, 7, 3), 0, 99],
      [19, 19, null, [[0, 1], [1, 3], [2, 3], [3, 4], [4, 4], [5, 4], [6, 4]], 2, 99],
      [288, 257, 'lit', [[0, 1], [257, 2], [258, 3], [259, 3]], 9, 99],
      [19, 19, null, [[0, 1], [1, 2], [2, 3], [3, 5], [4, 6], [5, 6], [6, 6], [7, 6], [8, 6],
                      [9, 7], [10, 7]], 4, 99],
    ];
    V.forEach((v, i) => {
      const slots = [];
      for (let k = 0; k < HOT + COLD; k++) {
        const base = k < HOT ? hot.add((i * HOT + k) * SLOT) : cold.add((i * COLD + k - HOT) * SLOT);
        slots.push(k < v[5] ? base : null);
      }
      a.setZ(i, slots);
      const b = lensBlk.add(i * 0x480);
      C3CH.lens(b, v[0], v[3]);
      const o = outBlk.add(i * 0x10);
      o.writeU32(v[4]);                                   // *m
      o.add(4).writeU32(0x4b4b0000 + i);                  // *t, overwritten by the first table
      out['hbB' + i] = b;
      out['hbN' + i] = v[0];
      out['hbS' + i] = v[1];
      out['hbD' + i] = v[2] === 'lit' ? CPLENS : (v[2] === 'dist' ? CPDIST : 0);
      out['hbE' + i] = v[2] === 'lit' ? CPLEXT : (v[2] === 'dist' ? CPDEXT : 0);
      out['hbT' + i] = o.add(4);
      out['hbM' + i] = o;
      out['hbZs' + i] = a.z(i);
    });
    return out;
  },

  // ========================================================= inflate_blocks 0x1009ce80 (17 vectors)
  //
  // Seventeen calls that enter in mode TYPE and run whole deflate blocks: a stored block, a fixed-Huffman
  // block, a 416-byte dynamic block, the minimal dynamic block, a block of type 3, a stored block that
  // is not the last one followed by a fixed one, a fixed block that is not the last one followed by a
  // stored one, and the allocation failures each of those can hit.
  //
  // Each vector owns a z_stream, a control block, an inflate_blocks_state, a 0x200-byte window and a
  // 0x80-byte output buffer; all four are compared state, and so is the free log. The pool the slot
  // allocator hands the blens array and the huft tables out of is NOT compared (it is huft_build's own
  // output, which that function's own key compares entry by entry); what proves the decode here is the
  // bytes in the output buffer and the window plus the stream's counters.
  //
  // The window is 0x200 bytes and the dynamic stream was compressed with a 512-byte window
  // (log/c3/c3ch_gen.py, wbits -9), so no back reference ever points outside the buffer.
  c3ch_blocks() {
    const out = {};
    const N = 17, BUF = 0x280, WIN = 0x200, OUTLEN = 0x80, SLOT = 0x600, SLOTS = 40;
    const a = C3CH.arena(N, 0x7201);
    const sBlk = Memory.alloc(N * 0x40);
    const bufBlk = Memory.alloc(N * BUF);
    const pool = Memory.alloc(N * SLOTS * SLOT);
    C3CH.fill(sBlk, N * 0x40, 0x7202);
    C3CH.fill(bufBlk, N * BUF, 0x7203);
    C3CH.fill(pool, N * SLOTS * SLOT, 0x7204);
    out.ibZ = a.zBlk; out.ibCtl = a.ctlBlk; out.ibS = sBlk; out.ibBuf = bufBlk;
    const adler = C3CH.va(0x1009e370);
    C3CH.fixedTrees();                                    // build the module's fixed tables up front

    //      stream         window  avail_out  checkfn  slots ok  r
    const V = [
      ['stored', WIN, OUTLEN, true, SLOTS, 0],
      ['fixed', WIN, OUTLEN, false, SLOTS, 0],
      ['dyn', WIN, OUTLEN, true, SLOTS, 0],
      ['minDyn', 0x80, 0x40, false, SLOTS, 0],
      ['badType', WIN, OUTLEN, false, SLOTS, 0],
      ['stored', WIN, OUTLEN, true, SLOTS, 0],            // 5: avail_in forced to 0 below
      ['twoBlocks', WIN, OUTLEN, false, SLOTS, 0],
      ['fixed', WIN, OUTLEN, false, 0, 0],                // inflate_codes_new starved
      ['stored', 8, 0, true, SLOTS, 0],                   // NEEDOUT: window full and nothing flushes
      ['badLens', WIN, OUTLEN, false, SLOTS, 0],
      ['tooMany', WIN, OUTLEN, false, SLOTS, 0],
      ['minDyn', 0x80, 0x40, false, 0, 0],                // the blens ZALLOC starved
      ['badBits', WIN, OUTLEN, false, SLOTS, 0],
      ['incBits', WIN, OUTLEN, false, SLOTS, 0],   // all 19 lengths nonzero, see c3ch_gen.py
      ['badRepeat', WIN, OUTLEN, false, SLOTS, 0],
      ['minDyn', 0x80, 0x40, false, 4, 0],                // the fourth ZALLOC (inflate_codes_new) fails
      ['fixedNotLast', WIN, OUTLEN, true, SLOTS, 0],      // CODES goes back to TYPE, not DRY
    ];
    V.forEach((v, i) => {
      const slots = [];
      for (let k = 0; k < SLOTS; k++)
        slots.push(k < v[4] ? pool.add((i * SLOTS + k) * SLOT) : null);
      const z = a.setZ(i, slots);
      const src = C3CH.buf(C3CH_DATA[v[0]], 64);
      const win = bufBlk.add(i * BUF);
      const dst = win.add(WIN);
      z.writePointer(src.p);                              // next_in
      z.add(0x04).writeU32(i === 5 ? 0 : src.n);          // avail_in
      z.add(0x08).writeU32(0);                            // total_in
      z.add(0x0c).writePointer(dst);                      // next_out
      z.add(0x10).writeU32(v[2]);                         // avail_out
      z.add(0x14).writeU32(0);                            // total_out
      z.add(0x18).writeU32(0);                            // msg
      z.add(0x1c).writeU32(0);                            // state
      z.add(0x30).writeU32(1);                            // adler
      C3CH.blocks(sBlk.add(i * 0x40), {
        mode: 0, window: win, end: win.add(v[1]), read: win, write: win,
        checkfn: v[3] ? adler : ptr(0), check: 1,
      });
      out['ibSt' + i] = sBlk.add(i * 0x40);
      out['ibZs' + i] = z;
      out['ibR' + i] = v[5];
    });
    return out;
  },

  // =================================================== inflate_blocks 0x1009ce80, entry modes (33)
  //
  // Thirty-three calls that enter in each of the other modes, so every slot of the ten-entry jump table at
  // 0x1009e258 and the default arm at 0x1009cf11 run, and so do the arms a whole-stream call never
  // reaches: the NEEDBYTE exits of LENS, TABLE, BTREE and DTREE, the two WRAP blocks of NEEDOUT with
  // their four window shapes, the `i + j` overrun of a repeat code, and the one-byte return at
  // 0x1009e030.
  //   LENS 1    a valid stored header, a header whose complement is wrong, a zero-length last block, and
  //             two bytes (the NEEDBYTE at 0x1009d1ca)
  //   STORED 2  a block with input left, one with avail_in 0 (0x1009d343), one whose length exceeds the
  //             input (0x1009d522), and the three wrapped-window shapes:
  //               read = window+0x11, write = window+0x10  the write cursor BELOW the read cursor, so
  //                                   the WAVAIL ternaries at 0x1009ced3 and 0x1009d436 take their other
  //                                   side and the two `q == end` tests fail
  //               read = window+0x20, write = end          the FIRST wrap runs (0x1009d3c1) and leaves
  //                                   room, so 0x1009d3fe takes its other side
  //               read = window+1, write = end, 0x20 out   the first wrap leaves no room, the flush
  //                                   moves the read cursor and the SECOND wrap then does
  //               read = window, write = end, 0x20 out     the first wrap is blocked by read == window
  //                                   and the flush then moves read off it, so only the SECOND wrap
  //                                   runs (0x1009d472 and the WAVAIL at 0x1009d486)
  //   TABLE 3   the minimal dynamic block from its HLIT field, HLIT 31, HDIST 31 (0x1009d677), one byte
  //             (0x1009d5b2), nine bytes (the repeat's NEEDBYTE at 0x1009db47) and three repeat codes of
  //             138 zeros over 258 lengths (the overrun at 0x1009dc4e)
  //   BTREE 4   the same block from its code-length field, an oversubscribed code-length tree, the same
  //             with no slot so inflate_trees_bits returns Z_MEM_ERROR (r != Z_DATA_ERROR), and one byte
  //             (the NEEDBYTE at 0x1009d7ef)
  //   DTREE 5   the loop already finished, so the body is inflate_trees_dynamic onwards: once with every
  //             slot, once with one slot (inflate_trees_dynamic returns Z_MEM_ERROR, 0x1009dda8), once
  //             with 257 literal lengths of 8 bits (an oversubscribed literal tree), once with the loop
  //             NOT finished and no input at all (0x1009d9fc), and once with a literal set whose
  //             end-of-block code is one bit while the table is nine (0x1009e030 keeps a whole byte)
  //   CODES 6   the state a fixed block leaves: the module's fixed trees in a codes state of ours
  //   DRY 7, DONE 8, BAD 9 and mode 10 (the default arm)
  // Vector 31 enters in mode TYPE with a fixed block that is not the last one, which is the only way
  // 0x1009e01c goes back to TYPE instead of falling into DRY.
  // The blens arrays and the codes state the DTREE and CODES vectors need are the caller's structures
  // here, in the compared parm slice, exactly as the allocator would have handed them over.
  c3ch_blocks_entry() {
    const out = {};
    const N = 33, BUF = 0x100, WIN = 0x80, OUTLEN = 0x80, SLOT = 0x1200, SLOTS = 24;
    const a = C3CH.arena(N, 0x7301);
    const sBlk = Memory.alloc(N * 0x40);
    const bufBlk = Memory.alloc(N * BUF);
    const parm = Memory.alloc(N * 0x500);                 // blens (0x408) and a codes state at +0x490
    const pool = Memory.alloc(N * SLOTS * SLOT);
    C3CH.fill(sBlk, N * 0x40, 0x7302);
    C3CH.fill(bufBlk, N * BUF, 0x7303);
    C3CH.fill(parm, N * 0x500, 0x7304);
    C3CH.fill(pool, N * SLOTS * SLOT, 0x7305);
    out.ieZ = a.zBlk; out.ieCtl = a.ctlBlk; out.ieS = sBlk; out.ieBuf = bufBlk; out.ieParm = parm;
    const adler = C3CH.va(0x1009e370);
    const fx = C3CH.fixedTrees();

    // the stored headers the LENS vectors read: LEN then its one's complement, little endian
    const lens8 = '0800f7ff' + '4142434445464748';
    const lens0last = '0000ffff';
    const stored8 = '5051525354555657';
    // a literal set whose end-of-block code is one bit while nine bits of lookup are asked for, so the
    // table is 2^9 wide and the code costs one: inflate_codes leaves fifteen bits in the buffer
    const WIDE = { 256: 1, 0: 2, 1: 3, 2: 4, 3: 5, 4: 6, 5: 7, 6: 8, 7: 9, 8: 9 };

    const V = [
      // [mode, stream hex, options]
      [1, lens8, { last: 0 }],                            // 0  LENS -> STORED -> TYPE
      [1, C3CH_DATA.badLens.slice(2), {}],                // 1  LENS with a wrong complement
      [1, lens0last, { last: 1 }],                        // 2  LENS, zero length, last -> DRY
      [2, stored8, { left: 8, last: 1 }],                 // 3  STORED with eight bytes to copy
      [2, '', { left: 8, last: 0, noInput: true }],       // 4  STORED with avail_in 0
      [3, C3CH_DATA.tableEntry, { last: 1 }],             // 5  TABLE, the minimal dynamic block
      [3, C3CH_DATA.tableTooMany, { last: 1 }],           // 6  TABLE, HLIT 31
      [4, C3CH_DATA.btreeEntry, { last: 1, table: C3CH_DATA.btreeTable }],               // 7
      [4, C3CH_DATA.btreeBadTree, { last: 1, table: 0 }],                                // 8
      [4, C3CH_DATA.btreeEntry, { last: 1, table: C3CH_DATA.btreeTable, slots: 0 }],     // 9
      [5, '', { last: 1, dtree: 'min', bitb: 2, bitk: 2 }],                              // 10
      [5, '', { last: 1, dtree: 'over' }],                                               // 11
      [6, C3CH_DATA.fixed.slice(2), { last: 1, codes: true,
                                      bitb: parseInt(C3CH_DATA.fixed.slice(0, 2), 16) >> 3,
                                      bitk: 5 }],                                        // 12
      [7, '', { last: 1, dry: 6 }],                       // 13 DRY: the flush empties the window
      [7, '', { last: 1, dry: 6, avail: 0 }],             // 14 DRY: nothing flushes, read != write
      [8, '', { last: 1 }],                               // 15 DONE
      [9, '', { last: 1 }],                               // 16 BAD
      [10, '', { last: 1 }],                              // 17 a mode the switch does not list
      [2, stored8, { left: 8, last: 1, read: 0x11, write: 0x10, avail: 0 }],             // 18
      [2, stored8, { left: 8, last: 1, read: 0x20, write: WIN, avail: 0 }],              // 19
      [2, stored8, { left: 8, last: 1, read: 1, write: WIN, avail: 0x20 }],              // 20
      [2, '5051', { left: 8, last: 1 }],                  // 21 STORED, the length exceeds the input
      [1, '0800', { last: 0 }],                           // 22 LENS with two bytes
      [3, C3CH_DATA.tableEntry.slice(0, 2), { last: 1 }], // 23 TABLE with one byte
      [3, C3CH_DATA.tableHdist31, { last: 1 }],           // 24 TABLE, HDIST 31
      [4, C3CH_DATA.btreeEntry.slice(0, 2), { last: 1, table: C3CH_DATA.btreeTable }],   // 25
      [3, C3CH_DATA.tableEntry.slice(0, 18), { last: 1 }],                               // 26
      [3, C3CH_DATA.tableOverrun, { last: 1 }],           // 27 three repeats of 138 over 258 lengths
      [5, '', { last: 1, dtree: 'min', bitb: 2, bitk: 2, slots: 1 }],                    // 28
      [5, '', { last: 1, dtree: 'none', bb: 1 }],                                        // 29
      [5, '00000000', { last: 1, dtree: 'wide' }],                                       // 30
      [0, C3CH_DATA.fixedNotLast, { last: 0 }],           // 31 CODES goes back to TYPE, not DRY
      [2, stored8, { left: 8, last: 1, read: 0, write: WIN, avail: 0x20 }],              // 32
    ];
    V.forEach((v, i) => {
      const o = v[2];
      const slots = [];
      const nslots = o.slots === undefined ? SLOTS : o.slots;
      for (let k = 0; k < SLOTS; k++)
        slots.push(k < nslots ? pool.add((i * SLOTS + k) * SLOT) : null);
      const z = a.setZ(i, slots);
      const src = C3CH.buf(v[1], 64);
      const win = bufBlk.add(i * BUF);
      const dst = win.add(WIN);
      const p = parm.add(i * 0x500);
      z.writePointer(src.p);
      z.add(0x04).writeU32(o.noInput ? 0 : src.n);
      z.add(0x08).writeU32(0);
      z.add(0x0c).writePointer(dst);
      z.add(0x10).writeU32(o.avail === undefined ? OUTLEN : o.avail);
      z.add(0x14).writeU32(0);
      z.add(0x18).writeU32(0);
      z.add(0x1c).writeU32(0);
      z.add(0x30).writeU32(1);

      const b = { mode: v[0], window: win, end: win.add(WIN), read: win, write: win,
                  checkfn: (i & 1) ? adler : ptr(0), check: 1, last: o.last,
                  bitb: o.bitb || 0, bitk: o.bitk || 0 };
      if (o.read !== undefined) b.read = win.add(o.read);
      if (o.write !== undefined) b.write = win.add(o.write);
      if (v[0] === 2) b.sub4 = o.left;
      if (v[0] === 3) b.sub4 = 0x32320000;
      if (v[0] === 4) { b.sub4 = o.table; b.sub8 = 0; b.subC = p.toUInt32(); b.tb = 0; }
      if (v[0] === 5) {
        // the DTREE loop is normally already past its end (index == 258 + 0 + 0), so the body starts at
        // the inflate_trees_free of tb and runs inflate_trees_dynamic over the lengths below; the
        // 'none' variant leaves the index at 0 with no input, so the loop's own NEEDBITS runs out
        // before tb is ever dereferenced
        b.sub4 = 0;
        b.sub8 = o.dtree === 'none' ? 0 : 258;
        b.subC = p.toUInt32();
        b.tb = 0;
        b.bb = o.bb === undefined ? 7 : o.bb;
        for (let k = 0; k < 258; k++) p.add(k * 4).writeU32(o.dtree === 'over' ? 8 : 0);
        if (o.dtree === 'min') {
          p.writeU32(1);                                   // literal 0x00, one bit
          p.add(256 * 4).writeU32(1);                      // end of block, one bit
          p.add(257 * 4).writeU32(1);                      // the single distance code
        } else if (o.dtree === 'wide') {
          Object.keys(WIDE).forEach((k) => p.add(k * 4).writeU32(WIDE[k]));
          p.add(257 * 4).writeU32(1);
        }
      }
      if (v[0] === 6) {
        // an inflate_codes_state (0x1c) of ours in mode START, holding the module's fixed trees
        const c = p.add(0x490);
        C3CH.fill(c, 0x20, 0x7306 + i);
        c.writeU32(0);                                     // mode START
        c.add(0x10).writeU8(fx.bl & 0xff);                 // lbits
        c.add(0x11).writeU8(fx.bd & 0xff);                 // dbits
        c.add(0x14).writePointer(fx.tl);
        c.add(0x18).writePointer(fx.td);
        b.sub4 = 0; b.sub8 = 0; b.subC = c.toUInt32();
      }
      if (v[0] === 7) {
        for (let k = 0; k < o.dry; k++) win.add(k).writeU8(0x60 + k);
        b.write = win.add(o.dry);
      }
      C3CH.blocks(sBlk.add(i * 0x40), b);
      out['ieSt' + i] = sBlk.add(i * 0x40);
      out['ieZs' + i] = z;
    });
    return out;
  },

  // ================================================================ inflate 0x1009c0f0 (34 vectors)
  //
  // Thirty-four calls: the four guards at the top (0x1009c10c, 0x1009c115, 0x1009c11d, 0x1009c123),
  // every one of the fourteen slots of the jump table at 0x1009c79d, and the default arm at 0x1009c152.
  // Each vector owns a z_stream, an internal_state (0x18 bytes), an inflate_blocks_state with its own
  // window and output buffer, and an allocation control block; all four are compared state. The three
  // BLOCKS vectors run a whole deflate block, so inflate_blocks and everything under it runs from here
  // as well; the other thirty touch only inflate's own bytes.
  c3ch_inflate() {
    const out = {};
    const N = 34, BUF = 0xc0, WIN = 0x80, OUTLEN = 0x40, SLOT = 0x600, SLOTS = 24;
    const a = C3CH.arena(N, 0x7401);
    const stBlk = Memory.alloc(N * 0x20);                 // internal_state, 0x18 bytes
    const sBlk = Memory.alloc(N * 0x40);                  // inflate_blocks_state
    const bufBlk = Memory.alloc(N * BUF);
    const pool = Memory.alloc(N * SLOTS * SLOT);
    C3CH.fill(stBlk, N * 0x20, 0x7402);
    C3CH.fill(sBlk, N * 0x40, 0x7403);
    C3CH.fill(bufBlk, N * BUF, 0x7404);
    C3CH.fill(pool, N * SLOTS * SLOT, 0x7405);
    out.ifZ = a.zBlk; out.ifCtl = a.ctlBlk; out.ifSt = stBlk; out.ifS = sBlk; out.ifBuf = bufBlk;
    const adler = C3CH.va(0x1009e370);
    C3CH.fixedTrees();

    // the deflate block of `fixed` followed by the adler32 its stream carries (the last four bytes of
    // zOk), so a vector entering in mode BLOCKS runs CHECK4..CHECK1 on a check that agrees
    const RAW_ADLER = C3CH_DATA.fixed + C3CH_DATA.zOk.slice(-8);

    //      [mode, stream, method/was, wbits, nowrap, f, flags]
    // flags: zNull, stNull, inNull
    const V = [
      [0, 'zOk', 0, 15, 0, 0, { zNull: true }],        // 0  z == NULL
      [0, 'zOk', 0, 15, 0, 0, { stNull: true }],       // 1  z->state == NULL
      [0, 'zOk', 0, 15, 0, 0, { inNull: true }],       // 2  z->next_in == NULL
      [0, 'zOk', 0, 15, 0, -1, {}],                    // 3  f < 0
      [0, 'zOk', 0, 15, 0, 0, {}],                     // 4  the whole zlib stream -> Z_STREAM_END
      [0, '', 0, 15, 0, 0, {}],                        // 5  avail_in 0 in METHOD
      [0, 'zBadMethod', 0, 15, 0, 0, {}],              // 6  method 7
      [0, 'zBadWindow', 0, 15, 0, 0, {}],              // 7  window 16 against wbits 15
      [1, C3CH_DATA.zBadCheck.slice(2), 0x78, 15, 0, 0, {}],   // 8  the header check fails
      [1, '', 0x78, 15, 0, 0, {}],                     // 9  avail_in 0 in FLAG
      [1, C3CH_DATA.zPresetDict.slice(2), 0x78, 15, 0, 0, {}], // 10 PRESET_DICT -> DICT4 -> Z_NEED_DICT
      [2, '01020304', 0, 15, 0, 0, {}],                // 11 DICT4 through DICT1
      [3, '020304', 0, 15, 0, 0, {}],                  // 12 DICT3
      [4, '0304', 0, 15, 0, 0, {}],                    // 13 DICT2
      [5, '04', 0, 15, 0, 0, {}],                      // 14 DICT1
      [5, '', 0, 15, 0, 0, {}],                        // 15 avail_in 0 in DICT1
      [6, '', 0, 15, 0, 0, {}],                        // 16 DICT0 -> Z_STREAM_ERROR
      [7, RAW_ADLER, 0, 15, 0, 0, {}],                 // 17 BLOCKS then CHECK4..CHECK1, check agrees
      [7, 'badType', 0, 15, 0, 0, {}],                 // 18 BLOCKS returns Z_DATA_ERROR
      [7, 'fixed', 0, 15, 1, 0, {}],                   // 19 nowrap: BLOCKS -> DONE
      [8, '00112233', 0x00112233, 15, 0, 0, {}],       // 20 CHECK4..CHECK1 with a check that agrees
      [8, '00112233', 0x55667788, 15, 0, 0, {}],       // 21 the same with one that does not
      [9, '', 0, 15, 0, 0, {}],                        // 22 avail_in 0 in CHECK3
      [10, '2233', 0x00002233, 15, 0, 0, {}],          // 23 CHECK2 -> CHECK1, check agrees
      [11, '', 0, 15, 0, 0, {}],                       // 24 avail_in 0 in CHECK1
      [12, '', 0, 15, 0, 0, {}],                       // 25 DONE -> Z_STREAM_END
      [13, '', 0, 15, 0, 0, {}],                       // 26 BAD -> Z_DATA_ERROR
      [14, '', 0, 15, 0, 0, {}],                       // 27 a mode past the fourteen the switch lists
      [2, '', 0, 15, 0, 0, {}],                        // 28 avail_in 0 in DICT4
      [3, '', 0, 15, 0, 0, {}],                        // 29 avail_in 0 in DICT3
      [4, '', 0, 15, 0, 0, {}],                        // 30 avail_in 0 in DICT2
      [8, '', 0, 15, 0, 0, {}],                        // 31 avail_in 0 in CHECK4
      [10, '', 0, 15, 0, 0, {}],                       // 32 avail_in 0 in CHECK2
      [7, C3CH_DATA.fixed.slice(0, 8), 0, 15, 0, 0, {}],   // 33 BLOCKS with half a block: Z_OK back
    ];
    V.forEach((v, i) => {
      const o = v[6];
      const slots = [];
      for (let k = 0; k < SLOTS; k++) slots.push(pool.add((i * SLOTS + k) * SLOT));
      const z = a.setZ(i, slots);
      const hex = C3CH_DATA[v[1]] !== undefined ? C3CH_DATA[v[1]] : v[1];
      const src = C3CH.buf(hex, 64);
      const win = bufBlk.add(i * BUF);
      const dst = win.add(WIN);
      const st = stBlk.add(i * 0x20);
      z.writePointer(o.inNull ? ptr(0) : src.p);          // next_in
      z.add(0x04).writeU32(src.n);                        // avail_in
      z.add(0x08).writeU32(0);                            // total_in
      z.add(0x0c).writePointer(dst);                      // next_out
      z.add(0x10).writeU32(OUTLEN);                       // avail_out
      z.add(0x14).writeU32(0);                            // total_out
      z.add(0x18).writeU32(0);                            // msg
      z.add(0x1c).writePointer(o.stNull ? ptr(0) : st);   // state
      z.add(0x30).writeU32(1);                            // adler
      C3CH.blocks(sBlk.add(i * 0x40), {
        mode: 0, window: win, end: win.add(WIN), read: win, write: win,
        checkfn: v[4] ? ptr(0) : adler, check: 1,
      });
      st.writeU32(v[0]);                                  // mode
      st.add(4).writeU32(v[2]);                           // sub.method, or sub.check.was in CHECK*
      const accum = (v[0] >= 3 && v[0] <= 5) || (v[0] >= 9 && v[0] <= 11);
      st.add(8).writeU32(accum ? 0 : 0x43430000 + i);      // sub.check.need
      st.add(0x0c).writeU32(v[4]);                        // nowrap
      st.add(0x10).writeU32(v[3]);                        // wbits
      st.add(0x14).writePointer(sBlk.add(i * 0x40));      // blocks
      out['ifZs' + i] = o.zNull ? 0 : z;
      out['ifF' + i] = v[5];
    });
    return out;
  },

  // ============================================================ png_read_data 0x10078c40 (12 vectors)
  //
  // Twelve scratch png_structs at a 0x200 stride, each with a reader of ours in the read_data_fn slot at
  // +0x50 and its own source buffer and control block. The vectors ask for different lengths from
  // different destinations, including zero; what is compared is the bytes that arrived, the reader's
  // cursor and its call counter. No vector leaves read_data_fn null: that arm calls png_error 0x10078fe0,
  // which never returns (log/c3/c3ch_purpose.md).
  c3ch_readdata() {
    const out = {};
    const N = 12, PNG = 0x200, DST = 0x40, CTL = 0x10, SRC = 0x100;
    const pngBlk = Memory.alloc(N * PNG);
    const dstBlk = Memory.alloc(N * DST);
    const ctlBlk = Memory.alloc(N * CTL);
    const srcBlk = Memory.alloc(N * SRC);
    C3CH.fill(pngBlk, N * PNG, 0x7501);
    C3CH.fill(dstBlk, N * DST, 0x7502);
    C3CH.fill(ctlBlk, N * CTL, 0x7503);
    for (let i = 0; i < N * SRC; i++) srcBlk.add(i).writeU8((i * 7 + 3) & 0xff);
    out.rdPng = pngBlk; out.rdDst = dstBlk; out.rdCtl = ctlBlk;
    const LEN = [0, 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 0x3f];
    for (let i = 0; i < N; i++) {
      const png = pngBlk.add(i * PNG);
      const ctl = ctlBlk.add(i * CTL);
      ctl.writeU32(0);                                    // cursor
      ctl.add(4).writePointer(srcBlk.add(i * SRC));       // src
      ctl.add(8).writeU32(0);                             // calls
      ctl.add(0xc).writeU32(0);                           // lastLen
      png.add(0x40).writePointer(C3CH.mkStubs().ret);     // error_fn, never reached
      png.add(0x44).writePointer(C3CH.mkStubs().ret);     // warning_fn
      png.add(0x50).writePointer(C3CH.mkReader(ctl));     // read_data_fn
      out['rdP' + i] = png;
      out['rdD' + i] = dstBlk.add(i * DST);
      out['rdL' + i] = LEN[i];
    }
    return out;
  },

  // ============================================================= png_crc_read 0x100794b0 (12 vectors)
  //
  // The same twelve png_structs with a crc at +0x110, a chunk name at +0x11c and the flags at +0x6c, so
  // png_calculate_crc 0x100787d0 runs both its arms: six vectors are ancillary chunks and six critical,
  // and the flag words switch the accumulation on and off. The compared state is the destination bytes,
  // the reader's cursor and counter, and the running crc the second call leaves behind.
  c3ch_crcread() {
    const out = {};
    const N = 12, PNG = 0x200, DST = 0x40, CTL = 0x10, SRC = 0x100;
    const pngBlk = Memory.alloc(N * PNG);
    const dstBlk = Memory.alloc(N * DST);
    const ctlBlk = Memory.alloc(N * CTL);
    const srcBlk = Memory.alloc(N * SRC);
    C3CH.fill(pngBlk, N * PNG, 0x7601);
    C3CH.fill(dstBlk, N * DST, 0x7602);
    C3CH.fill(ctlBlk, N * CTL, 0x7603);
    for (let i = 0; i < N * SRC; i++) srcBlk.add(i).writeU8((i * 13 + 5) & 0xff);
    out.crPng = pngBlk; out.crDst = dstBlk; out.crCtl = ctlBlk;
    //      chunk_name[0]  flags   length
    const V = [
      [0x62, 0x000, 4], [0x62, 0x300, 4], [0x62, 0x100, 8], [0x62, 0x200, 1],
      [0x49, 0x000, 4], [0x49, 0x800, 4], [0x49, 0x400, 16], [0x49, 0x300, 0],
      [0x74, 0x000, 32], [0x74, 0x300, 7], [0x50, 0x800, 2], [0x50, 0x000, 3],
    ];
    V.forEach((v, i) => {
      const png = pngBlk.add(i * PNG);
      const ctl = ctlBlk.add(i * CTL);
      ctl.writeU32(0);
      ctl.add(4).writePointer(srcBlk.add(i * SRC));
      ctl.add(8).writeU32(0);
      ctl.add(0xc).writeU32(0);
      png.add(0x40).writePointer(C3CH.mkStubs().ret);
      png.add(0x44).writePointer(C3CH.mkStubs().ret);
      png.add(0x50).writePointer(C3CH.mkReader(ctl));
      png.add(0x6c).writeU32(v[1]);                       // flags
      png.add(0x110).writeU32(0x1000 + i);                // crc
      png.add(0x11c).writeU32(v[0] | 0x48000000);         // chunk_name
      out['crP' + i] = png;
      out['crD' + i] = dstBlk.add(i * DST);
      out['crL' + i] = v[2];
    });
    return out;
  },

  // ============================================================ png_crc_error 0x10079620 (12 vectors)
  //
  // Twelve png_structs covering both sides of all four of its tests: ancillary and critical chunk names
  // (0x1007964f), the ancillary flag pair 0x300 complete and incomplete (0x10079661), the critical flag
  // 0x800 set and clear (0x1007967a), and, where need_crc survives, a stored crc that agrees with the
  // four bytes the reader serves and one that does not (0x100796b2). The four bytes are read on every
  // path, so the reader's cursor advances by four in every vector.
  c3ch_crcerror() {
    const out = {};
    const N = 12, PNG = 0x200, CTL = 0x10, SRC = 0x20;
    const pngBlk = Memory.alloc(N * PNG);
    const ctlBlk = Memory.alloc(N * CTL);
    const srcBlk = Memory.alloc(N * SRC);
    C3CH.fill(pngBlk, N * PNG, 0x7701);
    C3CH.fill(ctlBlk, N * CTL, 0x7702);
    out.cePng = pngBlk; out.ceCtl = ctlBlk;
    //      chunk_name[0]  flags   crc bytes (big endian)  stored crc
    const V = [
      [0x62, 0x000, 0x11223344, 0x11223344],   // ancillary, need_crc, equal
      [0x62, 0x000, 0x11223344, 0x11223345],   // ancillary, need_crc, different
      [0x62, 0x300, 0xaabbccdd, 0x00000000],   // ancillary, 0x300 complete: no comparison
      [0x62, 0x100, 0xaabbccdd, 0xaabbccdd],   // ancillary, only 0x100: equal
      [0x62, 0x200, 0xaabbccdd, 0x12345678],   // ancillary, only 0x200: different
      [0x49, 0x000, 0x00000000, 0x00000000],   // critical, need_crc, equal
      [0x49, 0x000, 0xffffffff, 0x00000000],   // critical, need_crc, different
      [0x49, 0x800, 0xdeadbeef, 0xcafebabe],   // critical, 0x800: no comparison
      [0x49, 0x400, 0xdeadbeef, 0xdeadbeef],   // critical, 0x400 only: equal
      [0x74, 0x300, 0x01020304, 0x01020304],   // ancillary, 0x300 complete, bytes equal anyway
      [0x50, 0x800, 0x05060708, 0x05060708],   // critical, 0x800, bytes equal anyway
      [0x50, 0x000, 0x80000000, 0x7fffffff],   // critical, need_crc, different in the top bit
    ];
    V.forEach((v, i) => {
      const png = pngBlk.add(i * PNG);
      const ctl = ctlBlk.add(i * CTL);
      const src = srcBlk.add(i * SRC);
      src.writeU8((v[2] >>> 24) & 0xff);
      src.add(1).writeU8((v[2] >>> 16) & 0xff);
      src.add(2).writeU8((v[2] >>> 8) & 0xff);
      src.add(3).writeU8(v[2] & 0xff);
      for (let k = 4; k < SRC; k++) src.add(k).writeU8(0);
      ctl.writeU32(0);
      ctl.add(4).writePointer(src);
      ctl.add(8).writeU32(0);
      ctl.add(0xc).writeU32(0);
      png.add(0x40).writePointer(C3CH.mkStubs().ret);
      png.add(0x44).writePointer(C3CH.mkStubs().ret);
      png.add(0x50).writePointer(C3CH.mkReader(ctl));
      png.add(0x6c).writeU32(v[1]);
      png.add(0x110).writeU32(v[3]);
      png.add(0x11c).writeU32(v[0] | 0x48000000);
      out['ceP' + i] = png;
    });
    return out;
  },

  // =========================================================== png_crc_finish 0x10079510 (16 vectors)
  //
  // Sixteen chunks whose skip counts and zbuf sizes between them take both sides of the loop at
  // 0x1007954b (skip above and below the zbuf size, and the exact multiple that the remainder read takes
  // alone) and of the remainder test at 0x10079573, and whose crc bytes - precomputed by
  // log/c3/c3ch_gen.py as the crc32 of exactly the bytes the reader will deliver, folded into the
  // vector's initial png_ptr->crc - take both sides of 0x1007959d. Every vector whose crc disagrees is
  // either an ancillary chunk without flag 0x200 or a critical chunk with flag 0x400, so all of them take
  // the png_chunk_warning arm at 0x100795eb, whose png_warning reaches the one-byte `ret` each
  // png_struct carries at +0x44. The png_chunk_error arm at 0x100795fe is not taken by any vector: it
  // ends in longjmp (log/c3/c3ch_purpose.md).
  c3ch_crcfinish() {
    const out = {};
    const N = 16, PNG = 0x200, ZBUF = 0x40, CTL = 0x10;
    const pngBlk = Memory.alloc(N * PNG);
    const zbufBlk = Memory.alloc(N * ZBUF);
    const ctlBlk = Memory.alloc(N * CTL);
    C3CH.fill(pngBlk, N * PNG, 0x7801);
    C3CH.fill(zbufBlk, N * ZBUF, 0x7802);
    C3CH.fill(ctlBlk, N * CTL, 0x7803);
    out.cfPng = pngBlk; out.cfZbuf = zbufBlk; out.cfCtl = ctlBlk;
    // chunk_name[0] and flags per vector: need_crc is 1 unless the ancillary pair 0x300 is complete or
    // the critical chunk carries 0x800, and every disagreeing vector must reach the warning, not the
    // error (ancillary without 0x200, or critical with 0x400).
    const NAME = [0x62, 0x62, 0x62, 0x49, 0x62, 0x49, 0x62, 0x49,
                  0x74, 0x49, 0x62, 0x49, 0x49, 0x62, 0x50, 0x62];
    const FLAG = [0x000, 0x000, 0x100, 0x000, 0x400, 0x000, 0x100, 0x400,
                  0x000, 0x400, 0x300, 0x800, 0x400, 0x000, 0x000, 0x000];
    for (let i = 0; i < N; i++) {
      const v = C3CH_DATA.crcFinish[i];
      const png = pngBlk.add(i * PNG);
      const ctl = ctlBlk.add(i * CTL);
      const src = C3CH.buf(v[3], 32);
      ctl.writeU32(0);
      ctl.add(4).writePointer(src.p);
      ctl.add(8).writeU32(0);
      ctl.add(0xc).writeU32(0);
      png.add(0x40).writePointer(C3CH.mkStubs().ret);
      png.add(0x44).writePointer(C3CH.mkStubs().ret);
      png.add(0x50).writePointer(C3CH.mkReader(ctl));
      png.add(0x6c).writeU32(FLAG[i]);
      png.add(0xac).writePointer(zbufBlk.add(i * ZBUF));   // zbuf
      png.add(0xb0).writeU32(v[1]);                        // zbuf_size
      png.add(0x110).writeU32(v[2]);                       // crc
      png.add(0x11c).writeU32(NAME[i] | 0x48000000);       // chunk_name
      out['cfP' + i] = png;
      out['cfSkip' + i] = v[0];
    }
    return out;
  },
});
