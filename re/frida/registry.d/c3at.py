# Batch c3at: jgld.dll's quaternion product family, the Transform/Matrix copy-and-set helpers, two trivial
# constructors and libpng's png_set_cHRM / png_set_gAMA. Addresses are RVAs (VA = 0x10000000 + RVA).
#
# Every math key shares the fixture c3at_math and the single state region that covers its whole allocation, so
# a writer is compared through the object it changes AND through the bytes around it (each object sits at a
# larger stride than its size, and the block is pre-filled with a per-offset word pattern).
# The png keys share c3at_png the same way.
#
# Leaf rule: Transform::set, Transform::reset, Matrix::copyCtor, Matrix::assign, Transform::assignOp,
# Transform::copyCtor, Random::ctor, MappedFile::ctor, png_set_cHRM and png_set_gAMA have no callees, so each
# carries at least 12 vectors.

_BLK = [("$blk", 0, 0x1600)]
_PBLK = [("$pblk", 0, 0x1000)]

# pairs of quaternion indices, both orders (the quaternion product is not commutative, so a swapped operand
# would show in every vector), plus one aliased call (argument == this, which the original supports because
# every read happens before the first write)
_QPAIRS = [(0, 1), (1, 0), (2, 3), (3, 2), (4, 5), (5, 4), (6, 7), (7, 6), (8, 9), (9, 8), (10, 11), (11, 10),
           (0, 6), (3, 9)]

# png_set_cHRM takes eight doubles; 30 values rotated by 8 per vector give 12 tuples that between them cover
# every one of them. They include values that are not exactly representable in float (0.1, 1/3, 1+2^-23,
# 16777217, 0.30000001192092896), both zeros, negatives, and the ends of the float range that do not overflow.
_D = [0.3127, 0.3290, 0.64, 0.33, 0.30, 0.60, 0.15, 0.06, 0.3333333333333333, 0.1, 1.0000001,
      1e-30, 1e38, 1.0, 0.0, -0.0, -1.0, 2.0, -2.5e-5, 1.5e6, 123.456, 16777217.0, 16777215.0,
      0.30000001192092896, 1e7, 1e-7, 3e5, 1234.567, 0.0077, 0.9999999]
_CHRM = [tuple(_D[(i * 8 + k) % len(_D)] for k in range(8)) for i in range(12)]
_GAMMA = [0.45455, 1.0, 2.2, 0.1, 0.3333333333333333, 1e-30, 1e38, -0.0, 16777217.0,
          0.30000001192092896, 1e-7, 3e5, 0.9999999]

HOOKS = {
    # --- the quaternion product: w*q.w - v.dot(q.v) for the scalar part, cross + q.v*w + v*q.w for the vector
    # part. Callees Vector3::dot/cross/operator+/operator*(float) are hooked by batch c3ap, so these three keys
    # are re-run with SIMGOLF_HOOKS_OFF=jgld.dll:3790,jgld.dll:39b0,jgld.dll:3c90,jgld.dll:3e50 (and
    # jgld.dll:4530, jgld.dll:4410 for Vector3::rotate).
    "Quat::mulAssign": dict(module="jgld.dll", addr=0x000042F0, abi="thiscall", ret="pointer",
                            args=["pointer", "pointer"], fixture="c3at_math", state=_BLK,
                            vectors=[(f"$p{i}", f"$q{j}") for i, j in _QPAIRS] + [("$p11", "$p11")]
                                    + [(f"$h{i}", f"$g{i}") for i in range(12)]
                                    + [(f"$g{i}", f"$h{i}") for i in range(12)]),
    "Quat::mul": dict(module="jgld.dll", addr=0x00004410, abi="thiscall", ret="pointer",
                      args=["pointer", "pointer", "pointer"], fixture="c3at_math", state=_BLK,
                      vectors=[(f"$q{i}", f"$b{i}", f"$q{j}") for i, j in _QPAIRS]
                              + [(f"$h{i}", f"$b{i % 12}", f"$g{i}") for i in range(12)]
                              + [(f"$g{i}", f"$b{i % 12}", f"$h{i}") for i in range(12)]),
    # the null-quaternion guard at 0x10004133 is the only conditional branch in the batch's math half: the two
    # last vectors take it, the first twelve fall through
    "Vector3::rotate": dict(module="jgld.dll", addr=0x00004100, abi="thiscall", ret="void",
                            args=["pointer", "pointer"], fixture="c3at_math", state=_BLK,
                            vectors=[(f"$v{i}", f"$q{i}") for i in range(12)]
                                    + [(f"$v{i}", f"$g{i}") for i in range(12)]
                                    + [("$v0", 0), ("$v5", 0)]),

    # --- Matrix: the two compiler-generated copies (one writes the vtable pointer, the other does not) and the
    # compose helper that calls Matrix::setRotation (c3ap) and Matrix::setTranslation (c3al)
    "Matrix::copyCtor": dict(module="jgld.dll", addr=0x00006720, abi="thiscall", ret="pointer",
                             args=["pointer", "pointer"], fixture="c3at_math", state=_BLK,
                             vectors=[(f"$M{i}", f"$N{i % 8}") for i in range(12)]),
    "Matrix::assign": dict(module="jgld.dll", addr=0x00006790, abi="thiscall", ret="pointer",
                           args=["pointer", "pointer"], fixture="c3at_math", state=_BLK,
                           vectors=[(f"$M{i}", f"$N{(i + 3) % 8}") for i in range(12)]),
    "Matrix::setRotationTranslation": dict(module="jgld.dll", addr=0x00004690, abi="thiscall", ret="void",
                                           args=["pointer", "pointer", "pointer"], fixture="c3at_math",
                                           state=_BLK,
                                           vectors=[(f"$M{i}", f"$q{i}", f"$s{(i + 2) % 12}")
                                                    for i in range(12)]),

    # --- Transform: three setters (each stores a different flags constant), the two compiler-generated copies
    # and reset
    "Transform::setTranslation": dict(module="jgld.dll", addr=0x000054B0, abi="thiscall", ret="void",
                                      args=["pointer", "pointer"], fixture="c3at_math", state=_BLK,
                                      vectors=[(f"$T{i}", f"$s{i % 12}") for i in range(16)]),
    "Transform::setRotation": dict(module="jgld.dll", addr=0x00005530, abi="thiscall", ret="void",
                                   args=["pointer", "pointer"], fixture="c3at_math", state=_BLK,
                                   vectors=[(f"$T{i}", f"$q{i % 12}") for i in range(16)]),
    "Transform::set": dict(module="jgld.dll", addr=0x000055C0, abi="thiscall", ret="void",
                           args=["pointer", "pointer", "pointer"], fixture="c3at_math", state=_BLK,
                           vectors=[(f"$T{i}", f"$q{(i + 3) % 12}", f"$s{(i + 5) % 12}") for i in range(16)]),
    "Transform::reset": dict(module="jgld.dll", addr=0x00005650, abi="thiscall", ret="void",
                             args=["pointer"], fixture="c3at_math", state=_BLK,
                             vectors=[(f"$T{i}",) for i in range(16)]),
    "Transform::assignOp": dict(module="jgld.dll", addr=0x00006840, abi="thiscall", ret="pointer",
                                args=["pointer", "pointer"], fixture="c3at_math", state=_BLK,
                                vectors=[(f"$T{i}", f"$U{i % 8}") for i in range(16)]),
    "Transform::copyCtor": dict(module="jgld.dll", addr=0x000068D0, abi="thiscall", ret="pointer",
                                args=["pointer", "pointer"], fixture="c3at_math", state=_BLK,
                                vectors=[(f"$T{i}", f"$U{(i + 2) % 8}") for i in range(16)]),

    # --- two constructors whose whole content is a fixed number of stores: the pattern fill around each object
    # is what makes "and nothing else" testable
    # keys carry the jgld_ prefix c3ak introduced: golf_clean.exe statically links the same two classes and
    # c3f already registers an exe "MappedFile::ctor" (0x00492d80)
    "jgld_Random::ctor": dict(module="jgld.dll", addr=0x00007620, abi="thiscall", ret="pointer",
                              args=["pointer"], fixture="c3at_math", state=_BLK,
                              vectors=[(f"$R{i}",) for i in range(16)]),
    "jgld_MappedFile::ctor": dict(module="jgld.dll", addr=0x00002650, abi="thiscall", ret="pointer",
                                  args=["pointer"], fixture="c3at_math", state=_BLK,
                                  vectors=[(f"$F{i}",) for i in range(16)]),

    # --- libpng setters: __cdecl, double arguments, two null guards each. The last three vectors of each key
    # take the guards (png_ptr null, info_ptr null, both null); the rest reach the stores.
    "png_set_cHRM": dict(module="jgld.dll", addr=0x0007DB30, abi="default", ret="void",
                         args=["pointer", "pointer"] + ["double"] * 8, fixture="c3at_png", state=_PBLK,
                         vectors=[("$png", f"$i{i}") + _CHRM[i] for i in range(12)]
                                 + [(0, "$i0") + _CHRM[0], ("$png", 0) + _CHRM[1], (0, 0) + _CHRM[2]]),
    "png_set_gAMA": dict(module="jgld.dll", addr=0x0007DBD0, abi="default", ret="void",
                         args=["pointer", "pointer", "double"], fixture="c3at_png", state=_PBLK,
                         vectors=[("$png", f"$i{i % 12}", g) for i, g in enumerate(_GAMMA)]
                                 + [(0, "$i0", 2.2), ("$png", 0, 2.2), (0, 0, 2.2)]),
}
