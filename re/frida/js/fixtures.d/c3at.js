// Batch c3at fixtures. Two allocations, each covered by a single state region, so every object a vector can
// touch is compared and restored. The game never sees these buffers: no live Display, Surface, Terrain or
// sound object is handed to an A/B here.
//
// c3at_math holds jgld.dll's math objects (layouts from re/match/jgld_math.cpp): Vector3 {float v[3]},
// Quat {float v[3]; float w}, Matrix {vptr; float m[16]} (m[0] at +4, 0x44 bytes) and
// Transform {vptr; Quat q at +4; Vector3 p at +0x14; unsigned flags at +0x20} (0x24 bytes), plus the 16-byte
// MappedFile and the Random object. Every object sits at a larger stride than its size and the whole block is
// pre-filled with a per-offset word pattern, so a reimplementation that writes one byte too many or leaves a
// field unwritten changes the block hash.
//
// The float values are the inexact ones c3ap pinned the x87 rounding with (0.1, 1/3, 1e-7, 3e5, 1+2^-23,
// 16777215, 0.30000001192092896); quaternion components are kept small so that the two chained products in
// Vector3::rotate stay inside float range. Two Transform sources (U6, U7) and two Matrix sources (N6, N7) are
// left as raw pattern words, which are not valid float values: the copies under test are integer moves in the
// original, so those words must come through unchanged.
Object.assign(globalThis.DIFF_FIXTURES, {
  c3at_math() {
    const blk = Memory.alloc(0x1600);
    const pat = (p, n) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32((0x5A5A0000 + i) >>> 0); };
    const wf = (p, a) => a.forEach((x, i) => p.add(4 * i).writeFloat(x));
    pat(blk, 0x1600);
    const out = { blk };

    // 12 Vector3 triples: sources at 0x000 (read-only operands) and the same values at 0x100 (the mutable
    // `this` of Vector3::rotate).
    const V = [
      [0.1, 0.3333333333, 1e-7],
      [3e5, 0.7, 1234.567],
      [1.0000001, 0.9999999, 123456.79],
      [0.1, 0.3, 7.0],
      [1e-7, 1e7, 0.1],
      [1e7, 1e-7, 0.1],
      [16777215.0, 1.0, 0.5],
      [1.0000001, 16777215.0, 3.0],
      [0.2, 0.30000001192092896, 0.7],
      [0.8, 0.6, 0.9],
      [-2.5e-5, 1.5e6, 123.456],
      [0.0077, -33000.0, 1e-7],
    ];
    // 12 Quats (x, y, z, w) with every component inside [-3.3, 3.3] so that q * (0,v) * conj(q) cannot
    // overflow a float, and every component inexact in binary.
    const Q = [
      [0.1, 0.3333333333, 1e-7, 0.9],
      [0.70710678, 0.0, 0.70710678, 1e-7],
      [1.0000001, 0.9999999, 0.3, 0.7],
      [0.57735026, 0.57735026, 0.57735026, 0.33333334],
      [1e-7, 2.5, 0.1, 1e-7],
      [0.2, 0.30000001192092896, 0.7, 0.11],
      [1.6777215, 1.0, 0.5, 3.0],
      [-0.1, -0.3333333333, -1e-7, -0.9],
      [0.0077, -3.3, 1e-7, 1.0000001],
      [0.8, 0.6, 0.9, 0.5],
      [1e-5, 2.0, 0.123456789, 0.987654321],
      [1.23456, -0.789, 0.001, 0.333],
    ];
    for (let i = 0; i < 12; i++) {
      wf(blk.add(0x000 + i * 0x10), V[i]); out['s' + i] = blk.add(0x000 + i * 0x10);
      wf(blk.add(0x100 + i * 0x10), V[i]); out['v' + i] = blk.add(0x100 + i * 0x10);
      wf(blk.add(0x200 + i * 0x10), Q[i]); out['q' + i] = blk.add(0x200 + i * 0x10);
      wf(blk.add(0x300 + i * 0x10), Q[i]); out['p' + i] = blk.add(0x300 + i * 0x10);
      // return buffers for Quat::operator*, pre-filled with a value no vector can produce
      wf(blk.add(0x400 + i * 0x10), [-7.5, -7.5, -7.5, -7.5]); out['b' + i] = blk.add(0x400 + i * 0x10);
    }

    // 16 Transforms at 0x500 (stride 0x30, 0x24 used): distinct vtable word, quaternion, position and flags.
    for (let i = 0; i < 16; i++) {
      const T = blk.add(0x500 + i * 0x30);
      T.writeU32((0xD0D00000 + i) >>> 0);
      wf(T.add(4), Q[i % 12]);
      wf(T.add(0x14), V[(i + 4) % 12]);
      T.add(0x20).writeU32(i * 0x11 + 3);
      out['T' + i] = T;
    }
    // 8 Transform copy sources at 0x800. U0..U5 carry float values; U6 and U7 keep the raw pattern words.
    for (let i = 0; i < 8; i++) {
      const U = blk.add(0x800 + i * 0x30);
      U.writeU32((0xE0E00000 + i) >>> 0);
      if (i < 6) {
        wf(U.add(4), Q[(i + 6) % 12]);
        wf(U.add(0x14), V[(i + 9) % 12]);
        U.add(0x20).writeU32(0x1000 + i * 7);
      }
      out['U' + i] = U;
    }

    // 12 Matrices at 0xA00 (stride 0x50, 0x44 used) and 8 copy sources at 0xE00; N6 and N7 keep the raw
    // pattern words. The vtable word starts at a value neither Matrix::copyCtor nor Matrix::assign can write,
    // so the one that sets it and the one that does not are told apart.
    const ramp = (k) => Array.from({ length: 16 }, (_, j) => (j + 1) * 0.1 + k * 1.0000001);
    for (let i = 0; i < 12; i++) {
      const M = blk.add(0xA00 + i * 0x50);
      M.writeU32((0xB0B00000 + i) >>> 0);
      wf(M.add(4), ramp(i));
      out['M' + i] = M;
    }
    for (let i = 0; i < 8; i++) {
      const N = blk.add(0xE00 + i * 0x50);
      N.writeU32((0xC0C00000 + i) >>> 0);
      if (i < 6) wf(N.add(4), ramp(i + 20).map((x, j) => (j & 1 ? -x : x * 1e-7)));
      out['N' + i] = N;
    }

    // 16 MappedFile objects at 0x1100 (stride 0x20, 0x10 used) and 16 Random objects at 0x1300 (stride 0x10,
    // 4 used): the pattern fill is the whole point, since both constructors must write a fixed number of
    // words and nothing beyond them.
    for (let i = 0; i < 16; i++) {
      out['F' + i] = blk.add(0x1100 + i * 0x20);
      out['R' + i] = blk.add(0x1300 + i * 0x10);
    }

    // 12 quaternion pairs built to separate the two candidate roundings of the scalar part. The original
    // computes w*q.w as a float (fstp dword [ebp-0x54] at 0x10004327) and subtracts Vector3::dot's result
    // while it is still the unrounded 53-bit value in st(0) (fsubr at 0x10004336). In each pair below the
    // scalar product w*q.w is within a few percent of the dot product, so the subtraction cancels almost
    // everything and the low bits of dot decide the answer: rounding dot to float first gives a different
    // float for all 12, in both operand orders. G holds the arguments (0x1400), H the `this` values (0x1500).
    const G = [
      [1.6439038515090942, 1.7570760250091553, 0.32891029119491577, 1.2180721759796143],
      [-0.052993837743997574, 1.8596062660217285, -1.7417508363723755, 0.43968796730041504],
      [-0.9216579794883728, 0.22573024034500122, 0.5785369277000427, -0.44706693291664124],
      [0.12064483016729355, -1.9228017330169678, 0.03240770474076271, -6.319899082183838],
      [0.35011401772499084, -1.3439871072769165, 0.2293209433555603, 0.3223837614059448],
      [1.335557222366333, -1.8357487916946411, -0.4552658796310425, -4.3060455322265625],
      [-0.11663950234651566, 1.2693760395050049, 1.5262703895568848, 1.3275412321090698],
      [-1.2575125694274902, -0.2557608187198639, -1.5221303701400757, 0.7540317177772522],
      [0.5641721487045288, -0.618604302406311, 0.8106358051300049, -0.16972872614860535],
      [1.3619166612625122, 0.8805119395256042, 1.174249529838562, -0.1648736298084259],
      [-1.8480216264724731, 0.02685936726629734, -1.6493288278579712, 1.1699079275131226],
      [-0.9702844619750977, -1.7080013751983643, -0.29478296637535095, -2.2186319828033447],
    ];
    const H = [
      [1.8658140897750854, -0.23706960678100586, -1.970034122467041, 1.507345199584961],
      [-1.2676373720169067, -1.5423481464385986, -1.941524863243103, 1.3116322755813599],
      [0.405853807926178, -1.644284725189209, 0.3160107433795929, 1.2215545177459717],
      [-1.0033915042877197, 1.7340619564056396, -0.18644791841506958, 0.5086703300476074],
      [-0.10869229584932327, -0.4906102418899536, -1.783299207687378, 0.716368556022644],
      [-1.3817179203033447, 1.8188745975494385, -1.3817944526672363, 1.0243884325027466],
      [1.2658522129058838, -0.09626036882400513, 1.1315611600875854, 1.15939462184906],
      [1.2589646577835083, -0.817284345626831, -1.5044933557510376, 1.294698715209961],
      [-0.0591953881084919, 1.2709360122680664, 0.6255664825439453, 1.714910864830017],
      [1.6319624185562134, -0.922678530216217, -1.3806140422821045, 1.1700060367584229],
      [-0.41902780532836914, -1.8090587854385376, -0.8554660081863403, 1.8992384672164917],
      [-0.7320559024810791, 1.7802042961120605, -1.7350895404815674, 0.8028451800346375],
    ];
    for (let i = 0; i < 12; i++) {
      wf(blk.add(0x1400 + i * 0x10), G[i]); out['g' + i] = blk.add(0x1400 + i * 0x10);
      wf(blk.add(0x1500 + i * 0x10), H[i]); out['h' + i] = blk.add(0x1500 + i * 0x10);
    }
    return out;
  },

  // libpng 1.0.5 png_info records for png_set_cHRM / png_set_gAMA. Only the valid-chunk word at +8, the gamma
  // float at +0x28 and the eight chromaticity floats at +0x80..+0x9c are touched; the rest of each 0x100-byte
  // record keeps the pattern so an over-wide store shows up. The png_struct argument is only tested against
  // null by both functions (0x1007db48, 0x1007dbe8) and never dereferenced, so "$png" is this block's own
  // first word, a non-null address that nothing reads.
  c3at_png() {
    const blk = Memory.alloc(0x1000);
    for (let i = 0; i < 0x1000; i += 4) blk.add(i).writeU32((0x37370000 + i) >>> 0);
    const out = { pblk: blk, png: blk };
    for (let i = 0; i < 12; i++) {
      const info = blk.add(0x100 + i * 0x100);
      // start each record with a different valid-chunk word, so the OR of bit 2 / bit 0 is visible and the
      // other bits must survive
      info.add(8).writeU32((0x1234 * (i + 1)) >>> 0);
      out['i' + i] = info;
    }
    return out;
  },
});
