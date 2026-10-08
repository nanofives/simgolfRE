# Batch c3aw: Terrain.dll. addr is the RVA (VA = 0x10000000 + RVA).
#
# The allocation-free rest of the std::list<Tile*> family batch c3as left for a next batch, plus roundHalf.
# Fixture: re/frida/js/fixtures.d/c3aw.js (c3aw_lists). Nothing of the game's is handed to an arm: the list
# objects, nodes, iterators, return slots and construct destinations are all our own allocations, and none of
# these twelve bodies reads or writes a module global.
#
# Hooked callees (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses original
# callees):
#   std::list<Tile*>::empty                        -> size Terrain.dll:b430 (this batch)
#   std::list<Tile*>::const_iterator::operator*    -> _Acc::_Value Terrain.dll:b660 (c3as)
#   std::list<Tile*>::const_iterator::operator!=   -> operator== Terrain.dll:b380 (c3as)
#   std::list<Tile*>::const_iterator::operator++   -> _Acc::_Next Terrain.dll:b600 (c3as)
#   std::list<Tile*>::iterator::operator++         -> const_iterator::operator++ Terrain.dll:b7a0 (this batch)
#   std::list<Tile*>::end                          -> iterator::iterator Terrain.dll:b750 (this batch)
#   std::list<Tile*>::begin                        -> _Acc::_Next Terrain.dll:b600 (c3as),
#                                                     iterator::iterator Terrain.dll:b750 (this batch)
#   std::list<Tile*>::iterator::iterator           -> the one-dword ctor Terrain.dll:b9b0 (this batch)
#   std::allocator<Tile*>::construct               -> std::_Construct Terrain.dll:ba40 (c3aq), which itself
#                                                     calls the placement operator new Terrain.dll:bab0 (c3aq)
#
# NULL nodes: $i20 and $i21 hold NULL. operator* 0x1000b2d0 and operator== 0x1000b380 never dereference the
# node, so they take them; the operator++ keys do not, because 0x1000b7ce dereferences the node.

_BLK = [("$blk", 0, 0x400)]                       # the whole fixture block: nodes, lists, iterators, slots

# iterators whose node is not NULL (the operator++ family dereferences it at 0x1000b7ce)
_LIVE_ITERS = [i for i in range(24) if i not in (20, 21)]

# operator!=: equal pairs ($i16..$i19, $i22, $i23 repeat nodes 0..3, 6, 7; $i20 and $i21 are both NULL) and
# unequal pairs, so const_iterator::operator==' `sete al` at 0x1000b3a9 is read on both sides
_NE_PAIRS = [("$i0", "$i16"), ("$i1", "$i17"), ("$i2", "$i18"), ("$i3", "$i19"), ("$i6", "$i22"),
             ("$i7", "$i23"), ("$i5", "$i5"), ("$i20", "$i21"),
             ("$i0", "$i1"), ("$i4", "$i9"), ("$i15", "$i0"), ("$i7", "$i8"), ("$i10", "$i11"),
             ("$i12", "$i13"), ("$i6", "$i14"), ("$i0", "$i20"), ("$i20", "$i3"), ("$i23", "$i8"),
             ("$i16", "$i2"), ("$i19", "$i5")]

# roundHalf: 0.5f is the only bit pattern that takes the `jne 0x1000202f` fall-through to `mov eax, 2`;
# every other value goes through __ftol 0x100183ec. 3e9 and -3e9 show the low dword of a 64-bit truncation,
# 1e19 the invalid conversion, and the two neighbours of 0.5f show the compare is on the exact bits.
_ROUND = [0.5, 0.0, -0.0, 1.0, -1.0, 2.0, 2.5, -2.5, 0.25, 0.75,
          0.4999999701976776, 0.5000000596046448, 0.30000001192092896,
          123.456, -123.456, 1e7, -1e7, 1e-7, 3e5, 16777215.0, 3e9, -3e9, 1e19]

# Faces::build: (arg1, arg2, arg3) triples. C = arg3 * 3, B = arg2 * C * 3 and A = arg1 * 3 are signed
# 32-bit products (0x10002084-0x100020a0), so the extremes show the wrap both arms perform.
_FACES = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1), (1, 1, 1), (2, 3, 4), (5, 0, 7), (-1, 0, 0),
          (0, -1, 0), (0, 0, -1), (10, 10, 10), (0x7FFFFFFF, 1, 1), (1, 0x7FFFFFFF, 1),
          (1, 1, 0x7FFFFFFF), (-0x80000000, 2, 3), (100, 200, 300)]

HOOKS = {
    # ------------------------------------------------------------------ the list object
    # size: leaf (no callee), 16 lists whose +8 counts are 0, 1, 2, 3, 7, 16, 255, 1000, 0x7fffffff, -1,
    # -1000, -0x80000000, 0, 5, 0, 42. No conditional jump in the body.
    "std::list<Tile*>::size": dict(module="Terrain.dll", addr=0x0000B430, abi="thiscall", ret="int",
                                   args=["pointer"], fixture="c3aw_lists",
                                   vectors=[("$L%d" % i,) for i in range(16)]),
    # empty: the same 16 lists; $L0, $L12 and $L14 hold the count 0, so `neg; sbb eax, eax; inc eax`
    # (0x1000b065-0x1000b069) is read with the carry flag both set and clear
    "std::list<Tile*>::empty": dict(module="Terrain.dll", addr=0x0000B040, abi="thiscall", ret="int",
                                    args=["pointer"], fixture="c3aw_lists",
                                    vectors=[("$L%d" % i,) for i in range(16)]),
    # begin/end: 16 lists, writing the iterator into a fixture slot. end copies the head pointer at +4,
    # begin the head node's `next`; the heads walk the ring, so the 16 results differ.
    "std::list<Tile*>::end": dict(module="Terrain.dll", addr=0x0000AFE0, abi="thiscall", ret="pointer",
                                  args=["pointer", "pointer"], fixture="c3aw_lists", state=_BLK,
                                  vectors=[("$L%d" % i, "$r%d" % i) for i in range(16)]),
    "std::list<Tile*>::begin": dict(module="Terrain.dll", addr=0x0000AF70, abi="thiscall", ret="pointer",
                                    args=["pointer", "pointer"], fixture="c3aw_lists", state=_BLK,
                                    vectors=[("$L%d" % i, "$r%d" % ((i + 5) % 16)) for i in range(16)]),

    # ------------------------------------------------------------------ iterators
    # operator*: 24 iterators, including the two holding NULL ($i20, $i21); neither body dereferences the
    # node, so those return the constant 8. No conditional jump.
    "std::list<Tile*>::const_iterator::operator*": dict(
        module="Terrain.dll", addr=0x0000B2D0, abi="thiscall", ret="pointer", args=["pointer"],
        fixture="c3aw_lists", vectors=[("$i%d" % i,) for i in range(24)]),
    "std::list<Tile*>::const_iterator::operator!=": dict(
        module="Terrain.dll", addr=0x0000B3D0, abi="thiscall", ret="int", args=["pointer", "pointer"],
        fixture="c3aw_lists", vectors=_NE_PAIRS),
    # prefix operator++: 22 iterators with a non-NULL node; the return is `this` and the state region shows
    # the advanced node
    "std::list<Tile*>::const_iterator::operator++": dict(
        module="Terrain.dll", addr=0x0000B7A0, abi="thiscall", ret="pointer", args=["pointer"],
        fixture="c3aw_lists", state=_BLK, vectors=[("$i%d" % i,) for i in _LIVE_ITERS]),
    # postfix operator++: this, the address of the iterator returned by value, and the unread `int` the
    # postfix form carries ([ebp+0xc], never loaded; 7 and -1 are passed to show that)
    "std::list<Tile*>::iterator::operator++": dict(
        module="Terrain.dll", addr=0x0000B320, abi="thiscall", ret="pointer",
        args=["pointer", "pointer", "int"], fixture="c3aw_lists", state=_BLK,
        vectors=[("$i%d" % k, "$r%d" % (k % 16), (0 if k % 3 == 0 else 7 if k % 3 == 1 else -1))
                 for k in _LIVE_ITERS]),
    # iterator::iterator and the one-dword ctor it delegates to: 16 (destination, node) pairs plus a NULL
    # node twice and two repeats. `this` is never NULL (0x1000b9d3 stores through it).
    "std::list<Tile*>::iterator::iterator": dict(
        module="Terrain.dll", addr=0x0000B750, abi="thiscall", ret="pointer", args=["pointer", "pointer"],
        fixture="c3aw_lists", state=_BLK,
        vectors=[("$i%d" % i, "$n%d" % ((i * 5 + 3) % 16)) for i in range(16)]
                + [("$i0", 0), ("$i7", 0), ("$i15", "$n15"), ("$i3", "$n0")]),
    # leaf (no callee): 20 vectors
    "std::reverse_bidirectional_iterator<std::list<Tile*>::const_iterator>::ctor": dict(
        module="Terrain.dll", addr=0x0000B9B0, abi="thiscall", ret="pointer", args=["pointer", "pointer"],
        fixture="c3aw_lists", state=_BLK,
        vectors=[("$i%d" % i, "$n%d" % ((i * 7 + 2) % 16)) for i in range(16)]
                + [("$i1", 0), ("$i9", 0), ("$i14", "$n14"), ("$i2", "$n1")]),

    # ------------------------------------------------------------------ allocation-free construction
    # construct: `this` is stored at 0x1000b6aa and never read, so 0 is passed for it. ret is "void": the
    # body loads nothing into eax after the call and std::_Construct keeps its result in a dead frame slot,
    # so only the written memory is comparable. The three NULL-destination vectors take std::_Construct's
    # je 0x1000ba6d (no copy); a NULL source with a non-NULL destination is not passed, because
    # std::_Construct dereferences it at 0x1000ba75.
    "std::allocator<Tile*>::construct": dict(
        module="Terrain.dll", addr=0x0000B690, abi="thiscall", ret="void",
        args=["pointer", "pointer", "pointer"], fixture="c3aw_lists", state=_BLK,
        vectors=[(0, "$d%d" % i, "$s%d" % ((i * 5 + 3) % 16)) for i in range(16)]
                + [(0, 0, "$s0"), (0, 0, "$s9"), (0, 0, 0)]),

    # ------------------------------------------------------------------ roundHalf and its caller
    "roundHalf": dict(module="Terrain.dll", addr=0x00002010, abi="default", ret="int", args=["float"],
                      vectors=[(v,) for v in _ROUND]),
    # Faces::build: three int arguments and a 0x200-byte buffer of our own per vector. ret is "void": the
    # body loads nothing into eax after the last tail copy (0x100024ed), so only the written memory is
    # comparable. The two nested loops always run 2 x 2 times (`cmp ..., 2` at 0x100020be and 0x100020da),
    # which is why the argument triples - not a loop count - are what separates the vectors.
    "Faces::build": dict(module="Terrain.dll", addr=0x00002060, abi="thiscall", ret="void",
                         args=["pointer", "int", "int", "int"], fixture="c3aw_faces",
                         state=[("$blk", 0, 16 * 0x200)],
                         vectors=[("$F%d" % k, a, b, c) for k, (a, b, c) in enumerate(_FACES)]),
}
