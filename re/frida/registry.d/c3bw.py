# Batch c3bw: jgld.dll non-render (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr).
# Reimplementations and the instruction citations: shim/src/re/c3bw.cpp; branch tables and the Purposes:
# log/c3/c3bw_purpose.md; fixtures: re/frida/js/fixtures.d/c3bw.js.
#
# Every quaternion, matrix, transform, list, list node, huft chain and z_stream the vectors touch is
# allocated by the fixtures. Nothing live in the running jgld.dll (the Display 0x10128420, the Lib
# 0x1012872c, the tracked-object list 0x10128428, a surface, a font or the game's heap blocks) is passed
# in. Every object sits at a larger stride than its own size inside one pre-filled block, and the whole
# block is the state region, so a store one field too wide or a field left unwritten changes the hash.
#
# Hooked callees to switch off when re-running (SIMGOLF_HOOKS_OFF):
#   Quat::ctor, Quat::setIdentity         jgld.dll:36e0          Vector3::set      (c3ap)
#   Matrix::ctor, Matrix::dtor            jgld.dll:45f0          Matrix::loadIdentity (c3bq)
#   Transform::dtor                       jgld.dll:5650          Transform::reset  (c3at)
#   Transform::ctor                       jgld.dll:5650,3050     Transform::reset (c3at) + Quat::ctor (this batch)
#   LinkedList::dtor                      jgld.dll:6df0          LinkedList::clear (this batch)
#   LinkedList::ctor, LinkedList::clear, LinkedList::removeCurrent, inflate_trees_free: no hooked callee.
#
# Leaf rule (>= 10 vectors for a function with no callee): LinkedList::ctor 0x6c80 has no callee at all
# (12 vectors) and inflate_trees_free 0x9f250 calls only through the caller's own zfree pointer
# (12 vectors). The other nine each have a named callee in hooks.csv.

_QUAT = [("$quatBlk", 0, 24 * 0x20)]
_MAT = [("$matBlk", 0, 24 * 0x60)]
_XF = [("$xfBlk", 0, 24 * 0x40)]
# the list objects, the nodes (the deleting-destructor marker writes node+0x14) and the marker's counter
_LIST = [("$llBlk", 0, 48 * 0x40), ("$nodeBlk", 0, 160 * 0x20), ("$llCtr", 0, 4)]
_LIST_OBJ = [("$llBlk", 0, 48 * 0x40)]
_ZLIB = [("$zBlk", 0, 12 * 0x200), ("$zStream", 0, 0x40), ("$zCtr", 0, 4)]

HOOKS = {
    # Quat::ctor 0x00003050: twelve quaternions, each pre-filled with a different pattern, so the three
    # zeros Vector3::set stores and the 1.0f at +0xc land over different bytes every time. No branch in
    # the body; returns `this`.
    "Quat::ctor": dict(module="jgld.dll", addr=0x00003050, abi="thiscall", ret="pointer",
                       args=["pointer"], fixture="c3bw_math", state=_QUAT,
                       vectors=[("$qc%d" % i,) for i in range(12)]),

    # Quat::setIdentity 0x00003170: the same body at another address, declared ret="void" because the
    # epilogue at 0x100031a5 does not reload `this` (the A/B compares memory only).
    "Quat::setIdentity": dict(module="jgld.dll", addr=0x00003170, abi="thiscall", ret="void",
                              args=["pointer"], fixture="c3bw_math", state=_QUAT,
                              vectors=[("$qi%d" % i,) for i in range(12)]),

    # Matrix::ctor 0x00004590: twelve matrices; the vtable dword at +0 and the sixteen cells at +4..+0x40
    # are rewritten, the eight bytes of stride above them are not. Returns `this`.
    "Matrix::ctor": dict(module="jgld.dll", addr=0x00004590, abi="thiscall", ret="pointer",
                         args=["pointer"], fixture="c3bw_math", state=_MAT,
                         vectors=[("$mc%d" % i,) for i in range(12)]),

    # Matrix::dtor 0x000032f0: the same two steps with no return value.
    "Matrix::dtor": dict(module="jgld.dll", addr=0x000032F0, abi="thiscall", ret="void",
                         args=["pointer"], fixture="c3bw_math", state=_MAT,
                         vectors=[("$md%d" % i,) for i in range(12)]),

    # Transform::ctor 0x00005450: twelve transforms. The quaternion at +4 is constructed, the vtable at +0
    # written and the flags word at +0x20 cleared; the position Vector3 at +0x14 is left alone, which the
    # region checks. Returns `this`.
    "Transform::ctor": dict(module="jgld.dll", addr=0x00005450, abi="thiscall", ret="pointer",
                            args=["pointer"], fixture="c3bw_math", state=_XF,
                            vectors=[("$xc%d" % i,) for i in range(12)]),

    # Transform::dtor 0x000034e0: vtable plus the flags word only; the quaternion is not destroyed.
    "Transform::dtor": dict(module="jgld.dll", addr=0x000034E0, abi="thiscall", ret="void",
                            args=["pointer"], fixture="c3bw_math", state=_XF,
                            vectors=[("$xd%d" % i,) for i in range(12)]),

    # LinkedList::ctor 0x00006c80: twelve list objects pre-filled with different patterns. It writes the
    # vtable, m_head, m_cur and m_count and leaves m_data, m_flag and m_ordered, which the region checks.
    # No callee: twelve vectors for the leaf rule. Returns `this`.
    "LinkedList::ctor": dict(module="jgld.dll", addr=0x00006C80, abi="thiscall", ret="pointer",
                             args=["pointer"], fixture="c3bw_list", state=_LIST_OBJ,
                             vectors=[("$lc%d" % i,) for i in range(12)]),

    # LinkedList::dtor 0x00006cf0: the vtable store plus LinkedList::clear. Lists of 0, 1, 2, 3, 4, 5, 6,
    # 8, 1 and 0 nodes, so clear's early return, its empty loop and its many-node loop all run under this
    # entry too.
    "LinkedList::dtor": dict(module="jgld.dll", addr=0x00006CF0, abi="thiscall", ret="void",
                             args=["pointer"], fixture="c3bw_list", state=_LIST,
                             vectors=[("$ld%d" % i,) for i in range(10)]),

    # LinkedList::clear 0x00006df0: twelve lists of 0, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2 and 0 nodes. Vector 0
    # and vector 11 take the m_head == 0 return at 0x10006e14; vector 1 and vector 9 (one node) enter the
    # loop test and leave it at once; the rest iterate 0, 1, 2, 3, 4, 5 and 6 times. The deleting
    # destructor of each node writes an increasing counter into node+0x14, so the ORDER of the deletions
    # is part of the compared state.
    "LinkedList::clear": dict(module="jgld.dll", addr=0x00006DF0, abi="thiscall", ret="void",
                              args=["pointer"], fixture="c3bw_list", state=_LIST,
                              vectors=[("$lk%d" % i,) for i in range(12)]),

    # LinkedList::removeCurrent 0x00007100: fourteen lists covering the empty return (vector 0), the
    # one-node arm at 0x1000713a (vectors 1 and 12), the m_head == m_cur arm at 0x100071a1 (vectors 3, 5,
    # 8, 10, 13) and the general arm with m_cur at the tail or in the middle (2, 4, 6, 7, 9, 11).
    "LinkedList::removeCurrent": dict(module="jgld.dll", addr=0x00007100, abi="thiscall", ret="void",
                                      args=["pointer"], fixture="c3bw_list", state=_LIST,
                                      vectors=[("$lr%d" % i,) for i in range(14)]),

    # inflate_trees_free 0x0009f250: twelve huft chains of 0, 1, 2, 3, 4, 5, 6, 7, 8, 1, 3 and 2 blocks.
    # Vector 0 passes a null table, which skips both loops; the rest reverse the chain in place and then
    # hand every block to the z_stream's zfree, which is a marker of ours. Twelve vectors for the leaf
    # rule (the only call it makes is through the caller's own function pointer).
    "inflate_trees_free": dict(module="jgld.dll", addr=0x0009F250, abi="default", ret="int",
                               args=["pointer", "pointer"], fixture="c3bw_zlib", state=_ZLIB,
                               vectors=[(0, "$zStream")] + [("$zT%d" % i, "$zStream") for i in range(1, 12)]),
}
