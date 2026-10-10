# Batch c3cw (round 23, jgld.dll): destructor chains left at C2 for the heap or the live tracked-object list.
# Reimplementations and instruction citations: shim/src/re/c3cw.cpp; fixtures: re/frida/js/fixtures.d/c3cw.js;
# Purposes and per-jcc branch tables: log/c3/c3cw_purpose.md; what was not taken: log/c3/c3cw_notes.txt.
# addr is the RVA (VA = 0x10000000 + addr).
#
# Every key runs under re/frida/js/heapstandin.js over operator delete 0x1007e6d0 (and operator new
# 0x1007f0f0, which nothing here calls): the frees become a compared log. Group B keys also swap jgld's
# tracked-object list 0x10128428 for the fixture's own list with the game's first thread frozen
# (re/frida/js/freeze.js), the list header being a compared state region.
#
# Hooked callees from other batches (re-run with them off: SIMGOLF_HOOKS_OFF):
#   Array_e0::dtor            Array_e0::clear            jgld.dll:68000 (c3cn)
#   Array_Rec14::dtor         Array_Rec14::clear         jgld.dll:68100 (c3cn)
#   Array_Rec94::dtor         Array_Rec94::clear         jgld.dll:68360 (c3cn)
#   Tracked::dtor             LinkedList::find           jgld.dll:72b0  (c3dll)
#                             LinkedList::removeCurrent  jgld.dll:7100  (c3bw)
#   Palette::~Palette         Palette::reset             jgld.dll:6a380 (c3bq)
# and inside this batch: every deletingDtor -> its dtor; PaletteBase::dtor -> Tracked::dtor;
# Palette::~Palette -> PaletteBase::dtor.

_HEAP = [("$arena", 0, 0x110), ("$freelog", 0, 0x404)]
_A = [("$blk", 0, 0x2000)] + _HEAP
_B = [("$blk", 0, 0x6800), ("$glist", 0, 0x1c)] + _HEAP

# flags of a scalar deleting destructor: only bit 0 decides (`and eax,1` at 0x10066bf8 and its twins).
# Seven vectors set it, five clear it, so one key covers both sides and the free log differs across vectors.
_FLAGS = [0x00000001, 0x00000000, 0x00000003, 0x00000002, 0x00000005, 0x00000004,
          0x80000001, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE, 0x00000101, 0x7FFFFFFF]


def _dtor(addr, prefix, fixture, state):
    return dict(module="jgld.dll", addr=addr, abi="thiscall", ret="void", args=["pointer"], fixture=fixture,
                state=state, vectors=[(f"${prefix}{i}",) for i in range(12)])


def _ddtor(addr, prefix, fixture, state):
    return dict(module="jgld.dll", addr=addr, abi="thiscall", ret="pointer", args=["pointer", "uint32"],
                fixture=fixture, state=state, vectors=[(f"${prefix}{i}", _FLAGS[i]) for i in range(12)])


HOOKS = {
    # ------------------------------------------------------------------------------- group A
    # Twelve headers per Array type (vtable +0, buffer +4, capacity +8, count +0xc, growth +0x10); eight have
    # a buffer (operator delete gets it from the clear), four have none (the clear's null guard).
    "Array_e0::dtor": _dtor(0x000669E0, "e", "c3cw_arrays", _A),
    "Array_e0::deletingDtor": _ddtor(0x00066BD0, "e", "c3cw_arrays", _A),
    "Array_Rec14::dtor": _dtor(0x00066A30, "r", "c3cw_arrays", _A),
    "Array_Rec14::deletingDtor": _ddtor(0x00066C40, "r", "c3cw_arrays", _A),
    "Array_Rec94::dtor": _dtor(0x00066B00, "s", "c3cw_arrays", _A),
    "Array_Rec94::deletingDtor": _ddtor(0x00066CB0, "s", "c3cw_arrays", _A),
    # Twelve FontBase objects at different offsets of the block; the one store lands on a different dword each.
    "FontBase::~FontBase": _dtor(0x00069010, "f", "c3cw_arrays", _A),
    "FontBase::deletingDtor": _ddtor(0x00069140, "f", "c3cw_arrays", _A),
}

# ----------------------------------------------------------------------------------- group B
# Four shapes of the swapped tracked-object list, one fixture each; the twelve objects $o0..$o11 are the same
# in every shape (Palette-sized, +0x80c set on even indices):
#   empty     head 0: find returns 0 at 0x100072d0's test, removeCurrent returns at 0x10007124
#   one       one node holding $o0, current: $o0 is found; every other object is not, and removeCurrent
#             still deletes that node (the one-node arm, 0x1000713a)
#   five      nodes holding $o0..$o4, current = the third: $o0 found makes head == current (0x100071a1 taken),
#             $o1..$o4 found and every not-found object leave head != current
#   fivehead  the same nodes with current = head: a not-found object removes the HEAD node
for _shape in ("empty", "one", "five", "fivehead"):
    _fx = f"c3cw_tracked_{_shape}"
    HOOKS.update({
        f"Tracked::dtor_{_shape}": _dtor(0x00006B80, "o", _fx, _B),
        f"Tracked::deletingDtor_{_shape}": _ddtor(0x00006B10, "o", _fx, _B),
        f"PaletteBase::dtor_{_shape}": _dtor(0x0006AF00, "o", _fx, _B),
        f"PaletteBase::deletingDtor_{_shape}": _ddtor(0x0006AF50, "o", _fx, _B),
        f"Palette::~Palette_{_shape}": _dtor(0x0006A2C0, "o", _fx, _B),
        f"Palette::deletingDtor_{_shape}": _ddtor(0x0006A1F0, "o", _fx, _B),
    })
