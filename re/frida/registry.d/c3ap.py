# Batch c3ap: jgld.dll's x87 math library (Vector3, Quat, Matrix). Addresses are RVAs.
# All keys share the fixture c3ap_math (one 0x700 allocation: 12 Vector3 sources $s0..$s11, 12 mutable copies
# $m0..$m11, 12 return buffers $r0..$r11, 12 Quats $q0..$q11, 12 mutable Quats $p0..$p11, 4 Matrices $M0..$M3)
# and the single state region that covers it, so a function that returns a Vector3 by value is compared through
# its return buffer and the writers through the object they change.
#
# Every one of these functions is a leaf except Quat::setAxisAngle (which calls Vector3::operator*= 0x00003a40,
# hooked in this same batch, and jgld.dll's own cos/sin), so each key carries >= 12 vectors.
# The operands are deliberately inexact in float (0.1, 1/3, 1e-7, 3e5, 1+2^-23, 16777215, 0.30000001192092896):
# computing these bodies in float instead of at the measured 53-bit x87 precision returns a different float.

_BLK = [("$blk", 0, 0x700)]

# pairs of source vectors (both orders, so a swapped operand would show)
_PAIRS = [(0, 1), (1, 0), (2, 3), (3, 2), (4, 5), (5, 4), (6, 7), (7, 6), (8, 9), (9, 8), (10, 11), (11, 10)]
# Vector3::dot: 14 different pairs (the sum is symmetric, so both orders of one pair return the same float).
# Computing the two additions in float instead of at 53 bits changes the result of 3 of these 14.
_DOT = [(0, 1), (2, 3), (4, 5), (6, 7), (8, 9), (10, 11), (0, 0), (2, 2), (0, 2), (1, 3), (4, 6), (5, 7),
        (8, 10), (9, 11)]
# Vector3::cross: pairs chosen so that the float model differs from the 53-bit one in 2 of the 3 components of
# 12 of these 14 vectors; each pair appears in both orders, which a swapped operand would turn into a sign flip.
_CROSS = [(0, 3), (3, 0), (0, 7), (7, 0), (1, 2), (2, 1), (1, 8), (8, 1), (4, 7), (7, 4), (3, 9), (9, 3),
          (10, 11), (11, 10)]
# scalars: three exact (0, 1, -1) and nine whose product with the operands is not exact in float
_SCALARS = [0.1, 0.3333333333, 1e-7, 3e5, -7.7, 1.0000001, 0.0, 1.0, -1.0, 16777215.0, 1e-20, 123.456]
# the same 12 triples the fixture stores, reused as arguments (rotated so a vector never writes what it holds)
_TRIPLES = [(0.1, 0.3333333333, 1e-7), (3e5, 0.7, 1234.567), (1.0000001, 0.9999999, 123456.79),
            (0.1, 0.3, 7.0), (1e-7, 1e7, 0.1), (1e7, 1e-7, 0.1), (16777215.0, 1.0, 0.5),
            (1.0000001, 16777215.0, 3.0), (0.2, 0.30000001192092896, 0.7), (0.8, 0.6, 0.9),
            (-2.5e-5, 1.5e6, 123.456), (0.0077, -33000.0, 1e-7)]
_ANGLES = [0.0, 1.0, 90.0, 0.1, 33.333333, -45.0, 1e-7, 3e5, 359.9999, 180.0, 1e7, -1e-7]

HOOKS = {
    # --- no arithmetic: integer copies and sign flips (the control group: these must match under any precision)
    "Vector3::set": dict(module="jgld.dll", addr=0x000036E0, abi="thiscall", ret="void",
                         args=["pointer", "float", "float", "float"], fixture="c3ap_math", state=_BLK,
                         vectors=[(f"$m{i}", *_TRIPLES[(i + 1) % 12]) for i in range(12)]),
    "Vector3::negate": dict(module="jgld.dll", addr=0x00003DF0, abi="thiscall", ret="void",
                            args=["pointer"], fixture="c3ap_math", state=_BLK,
                            vectors=[(f"$m{i}",) for i in range(12)]),
    "Quat::conjugate": dict(module="jgld.dll", addr=0x00004530, abi="thiscall", ret="void",
                            args=["pointer"], fixture="c3ap_math", state=_BLK,
                            vectors=[(f"$p{i}",) for i in range(12)]),

    # --- one float operation per stored component
    "Vector3::addXYZ": dict(module="jgld.dll", addr=0x00004020, abi="thiscall", ret="void",
                            args=["pointer", "float", "float", "float"], fixture="c3ap_math", state=_BLK,
                            vectors=[(f"$m{i}", *_TRIPLES[(i + 5) % 12]) for i in range(12)]),
    "Vector3::addAssignRet": dict(module="jgld.dll", addr=0x000038B0, abi="thiscall", ret="pointer",
                                  args=["pointer", "pointer"], fixture="c3ap_math", state=_BLK,
                                  vectors=[(f"$m{i}", f"$s{j}") for i, j in _PAIRS]),
    "Vector3::subAssignRet": dict(module="jgld.dll", addr=0x00003930, abi="thiscall", ret="pointer",
                                  args=["pointer", "pointer"], fixture="c3ap_math", state=_BLK,
                                  vectors=[(f"$m{i}", f"$s{j}") for i, j in _PAIRS]),
    "Vector3::add": dict(module="jgld.dll", addr=0x00003790, abi="thiscall", ret="pointer",
                         args=["pointer", "pointer", "pointer"], fixture="c3ap_math", state=_BLK,
                         vectors=[(f"$s{i}", f"$r{i}", f"$s{j}") for i, j in _PAIRS]),
    "Vector3::mulScalar": dict(module="jgld.dll", addr=0x000039B0, abi="thiscall", ret="pointer",
                               args=["pointer", "pointer", "float"], fixture="c3ap_math", state=_BLK,
                               vectors=[(f"$s{i}", f"$r{i}", _SCALARS[i]) for i in range(12)]),
    "Vector3::mulAssignScalar": dict(module="jgld.dll", addr=0x00003A40, abi="thiscall", ret="pointer",
                                     args=["pointer", "float"], fixture="c3ap_math", state=_BLK,
                                     vectors=[(f"$m{i}", _SCALARS[i]) for i in range(12)]),

    # --- several chained operations: these are the keys that pin the x87 precision down
    "Vector3::dot": dict(module="jgld.dll", addr=0x00003C90, abi="thiscall", ret="float",
                         args=["pointer", "pointer"], fixture="c3ap_math", state=_BLK,
                         vectors=[(f"$s{i}", f"$s{j}") for i, j in _DOT]),
    "Vector3::cross": dict(module="jgld.dll", addr=0x00003E50, abi="thiscall", ret="pointer",
                           args=["pointer", "pointer", "pointer"], fixture="c3ap_math", state=_BLK,
                           vectors=[(f"$s{i}", f"$r{i}", f"$s{j}") for i, j in _CROSS]),
    "Matrix::setRotation": dict(module="jgld.dll", addr=0x00004750, abi="thiscall", ret="void",
                                args=["pointer", "pointer"], fixture="c3ap_math", state=_BLK,
                                vectors=[(f"$M{i % 4}", f"$q{i}") for i in range(12)]),
    "Quat::setAxisAngle": dict(module="jgld.dll", addr=0x00004230, abi="thiscall", ret="void",
                               args=["pointer", "float", "pointer"], fixture="c3ap_math", state=_BLK,
                               vectors=[(f"$p{i}", _ANGLES[i], f"$s{i}") for i in range(12)]),
}
