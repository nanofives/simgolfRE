// Batch c3ap fixtures: jgld.dll's math objects (Vector3 {float v[3]}, Quat {float v[3]; float w},
// Matrix {vptr; float m[16]}, layouts from re/match/jgld_math.cpp). One allocation holds everything so a single
// state region ("$blk", 0, 0x700) covers every object a vector can touch; the game never sees these buffers.
//
// The values are chosen so that the x87 rounding is visible: products and sums of them are not exact in float
// (0.1, 1/3, 1e-7, 3e5, 1+2^-23, 16777215, 0.30000001192092896), the magnitudes span 1e-7..1e20 so that a sum
// loses low bits, and signs are mixed so that cross products cancel.
Object.assign(globalThis.DIFF_FIXTURES, {
  c3ap_math() {
    const blk = Memory.alloc(0x700);
    const wf = (p, a) => a.forEach((x, i) => p.add(4 * i).writeFloat(x));

    // 12 Vector3 sources at 0x000 + i*0x10 (read-only operands) and the same values again at 0x100 + i*0x10
    // (the mutable `this` of the writers, so each vector starts from a different inexact value).
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
    const out = { blk };
    V.forEach((v, i) => {
      wf(blk.add(0x000 + i * 0x10), v);
      wf(blk.add(0x100 + i * 0x10), v);
      out['s' + i] = blk.add(0x000 + i * 0x10);
      out['m' + i] = blk.add(0x100 + i * 0x10);
      // return buffers for the three functions that return a Vector3 by value, pre-filled with a pattern so a
      // component the original leaves alone would show up
      wf(blk.add(0x200 + i * 0x10), [-7.5, -7.5, -7.5]);
      out['r' + i] = blk.add(0x200 + i * 0x10);
    });

    // 12 Quats at 0x300 + i*0x10 (read-only, x,y,z,w) and the same values at 0x400 + i*0x10 (mutable).
    const Q = [
      [0.1, 0.3333333333, 1e-7, 0.9],
      [0.70710678, 0.0, 0.70710678, 1e-7],
      [1.0000001, 0.9999999, 0.3, 0.7],
      [0.57735026, 0.57735026, 0.57735026, 0.33333334],
      [1e-7, 1e7, 0.1, 1e-7],
      [0.2, 0.30000001192092896, 0.7, 0.11],
      [16777215.0, 1.0, 0.5, 3.0],
      [-0.1, -0.3333333333, -1e-7, -0.9],
      [0.0077, -33000.0, 1e-7, 1.0000001],
      [0.8, 0.6, 0.9, 0.5],
      [1e-20, 1e20, 0.123456789, 0.987654321],
      [123.456, -78.9, 0.001, 0.333],
    ];
    Q.forEach((q, i) => {
      wf(blk.add(0x300 + i * 0x10), q);
      wf(blk.add(0x400 + i * 0x10), q);
      out['q' + i] = blk.add(0x300 + i * 0x10);
      out['p' + i] = blk.add(0x400 + i * 0x10);
    });

    // 4 Matrices at 0x500 + i*0x50 (vptr + 16 floats = 0x44 bytes, stride 0x50). Matrix::setRotation writes 9 of
    // the 16 slots, so the four fills differ in the 7 it leaves alone as well as in the 9 it overwrites: zeros,
    // identity, a ramp, and large/inexact values. The vtable pointer stays null (no virtual call is made).
    const fills = [
      new Array(16).fill(0),
      [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1],
      [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6],
      [1e7, -1e-7, 16777215, 0.3333333333, -2.5e-5, 1.5e6, 123.456, 0.0077,
       -33000, 1e-7, 0.9999999, 1.0000001, 3e5, 0.7, 1234.567, -7.5],
    ];
    fills.forEach((f, i) => {
      const M = blk.add(0x500 + i * 0x50);
      M.writePointer(ptr(0));
      wf(M.add(4), f);
      out['M' + i] = M;
    });
    return out;
  },
});
