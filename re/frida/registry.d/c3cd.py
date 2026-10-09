# Batch c3cd: jgld.dll non-render (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr).
# Reimplementations and the instruction citations: shim/src/re/c3cd.cpp; branch tables and the Purposes:
# log/c3/c3cd_purpose.md; fixtures: re/frida/js/fixtures.d/c3cd.js; what was refused and why:
# log/c3/c3cd_notes.txt.
#
# Nothing live is handed to these bodies. The event sink 0x1012872c and the command handler 0x10128728
# are module globals the running game owns, so each key that touches them runs with the process's first
# thread (the only one that dispatches window messages) suspended, and the fixture writes the real value
# back and resumes it when the key ends.
#
# Hooked callees to switch off when re-running (SIMGOLF_HOOKS_OFF):
#   Lib::setCommandHandler    jgld.dll:67f60   storeCommandHandler (this batch)
#   Matrix::deletingDtor      jgld.dll:32f0,45f0   Matrix::dtor (c3bw), Matrix::loadIdentity (c3bq)
#   Transform::deletingDtor   jgld.dll:34e0,5650   Transform::dtor (c3bw), Transform::reset (c3at)
#   LinkedList::deletingDtor  jgld.dll:6cf0,6df0   LinkedList::dtor, LinkedList::clear (both c3bw)
#   ListNode::deletingDtor    jgld.dll:7450        ListNode::dtor (c3ax)
#   the nine on* forwarders and onCommand: no hooked callee (their one call is through a pointer the
#   fixture supplies, and the dispatch target is the event sink's vtable slot / the command handler).
#
# Leaf rule: every row of this batch has a named callee in hooks.csv (__chkesp 0x1007e780 for the
# forwarders, the class destructor for the deleting destructors, storeCommandHandler for
# Lib::setCommandHandler); each still carries twelve vectors, and onCommand twelve plus ten.

# the whole fixture block: a store one field too wide, or a node deleted out of order, changes the hash
_OBJ = [("$blk", 0, 0x4000)]
# the 0x100-byte event sink: vtable, record cursor at +4 and the records the slot stubs write
_SINK = [("$sink", 0, 0x100)]
# the command handler global 0x10128728 (4 bytes), reached through the fixture's own pointer to it
_GCMD = [("$gCmd", 0, 4)]
# the recording handler's two-dword buffer plus the global it is installed in
_CMDREC = [("$rec", 0, 0x40), ("$gCmd", 0, 4)]
# the global plus the scratch `this` the two writers are given (neither body writes it)
_GCMD_S = [("$gCmd", 0, 4), ("$scratch", 0, 0x40)]

# flags with BIT 0 CLEAR: the `and eax,1` / `je` of every scalar deleting destructor takes the
# no-delete side for all of them, and the other 31 bits are discarded (log/c3/c3cd_notes.txt explains
# why the delete side cannot be A/B'd)
_FLAGS = [0x00000000, 0x00000002, 0x00000004, 0x00000006, 0x00000008, 0x0000000A,
          0x00000010, 0x000000FE, 0x00001234, 0x7FFFFFFE, 0x80000000, 0xCCCCCCCC]

HOOKS = {
    # ------------------------------------------------------------ the nine window-message forwarders
    #
    # Each reads the event sink pointer from 0x1012872c twice, loads one vtable slot and makes a single
    # __thiscall on it. The fixture's sink records the slot's tag and every dword it was handed and
    # returns the first of them, so the compared state proves WHICH slot ran and with which arguments in
    # which order, and the return value proves the body passes the call's eax through untouched. The
    # first argument (the HWND) is 0 in every vector: no body reads it.

    # onChar 0x00069d50: __cdecl(hwnd, ch, repeat) -> slot +0x10 with 0xcccccc00 | (ch & 0xff). Twelve
    # low bytes, all different, plus upper bytes and a third argument that must make no difference.
    "onChar": dict(module="jgld.dll", addr=0x00069D50, abi="default", ret="int",
                   args=["pointer", "uint32", "uint32"], fixture="c3cd_sink", state=_SINK,
                   vectors=[(0, 0x00000000, 0), (0, 0x00000001, 1), (0, 0x00000020, 0xFFFFFFFF),
                            (0, 0x00000041, 7), (0, 0x00000061, 0x10000), (0, 0x0000007F, 3),
                            (0, 0x00000080, 0), (0, 0x00001102, 0x7FFFFFFF), (0, 0x000012CC, 2),
                            (0, 0xFFFFFF0D, 5), (0, 0x8000001B, 9), (0, 0xCCCCCC5A, 0x80000000)]),

    # onWmSize 0x00069db0: __cdecl(hwnd, wParam, cx, cy) -> slot +0x00 with (cx, cy).
    "onWmSize": dict(module="jgld.dll", addr=0x00069DB0, abi="default", ret="int",
                     args=["pointer", "uint32", "uint32", "uint32"], fixture="c3cd_sink", state=_SINK,
                     vectors=[(0, 0, 0, 0), (0, 1, 1, 2), (0, 2, 800, 600), (0, 0xFFFFFFFF, 640, 480),
                              (0, 3, 0xFFFFFFFF, 0), (0, 4, 0, 0xFFFFFFFF), (0, 5, 0x7FFFFFFF, 1),
                              (0, 6, 1, 0x80000000), (0, 7, 0x1234, 0x5678), (0, 8, 0xCCCCCCCC, 0x33333333),
                              (0, 9, 32, 24), (0, 0x80000000, 1024, 768)]),

    # onWmNcHitTest 0x00069e10: __cdecl(hwnd, x, y) -> slot +0x08 with (x, y).
    "onWmNcHitTest": dict(module="jgld.dll", addr=0x00069E10, abi="default", ret="int",
                          args=["pointer", "uint32", "uint32"], fixture="c3cd_sink", state=_SINK,
                          vectors=[(0, 0, 0), (0, 1, 2), (0, 0xFFFFFFFF, 0), (0, 0, 0xFFFFFFFF),
                                   (0, 0x7FFFFFFF, 1), (0, 1, 0x80000000), (0, 400, 300), (0, 0x1234, 0x5678),
                                   (0, 0xCCCCCCCC, 0x33333333), (0, 799, 599), (0, 0x8000, 0x8000), (0, 13, 7)]),

    # onWmMouseWheel 0x00069e70: __cdecl(hwnd, a, b, c, d) -> slot +0x18 with (a, b, c); the fifth
    # argument is never read.
    "onWmMouseWheel": dict(module="jgld.dll", addr=0x00069E70, abi="default", ret="int",
                           args=["pointer", "uint32", "uint32", "uint32", "uint32"],
                           fixture="c3cd_sink", state=_SINK,
                           vectors=[(0, 0, 0, 0, 0), (0, 1, 2, 3, 4), (0, 0xFFFFFFFF, 0, 1, 0),
                                    (0, 0, 0xFFFFFFFF, 2, 1), (0, 120, 400, 300, 0x7FFFFFFF),
                                    (0, 0xFFFFFF88, 10, 20, 0), (0, 0x7FFFFFFF, 1, 1, 2),
                                    (0, 1, 0x80000000, 3, 3), (0, 0x1234, 0x5678, 0x9ABC, 4),
                                    (0, 0xCCCCCCCC, 0x33333333, 0x55555555, 5), (0, 7, 8, 9, 6),
                                    (0, 0x8000, 0x8000, 0x8000, 7)]),

    # onWmMouseMove 0x00069ee0: __cdecl(hwnd, x, y, wParam) -> slot +0x14 with (x, y).
    "onWmMouseMove": dict(module="jgld.dll", addr=0x00069EE0, abi="default", ret="int",
                          args=["pointer", "uint32", "uint32", "uint32"], fixture="c3cd_sink", state=_SINK,
                          vectors=[(0, 0, 0, 0), (0, 1, 2, 1), (0, 0xFFFFFFFF, 0, 2), (0, 0, 0xFFFFFFFF, 3),
                                   (0, 0x7FFFFFFF, 1, 4), (0, 1, 0x80000000, 5), (0, 400, 300, 6),
                                   (0, 0x1234, 0x5678, 7), (0, 0xCCCCCCCC, 0x33333333, 8), (0, 799, 599, 9),
                                   (0, 0x8000, 0x8000, 0xFFFFFFFF), (0, 11, 13, 0x80000000)]),

    # onWmLButtonDown 0x00069f40: __cdecl(hwnd, dbl, x, y, wParam) -> slot +0x1c with (dbl, x, y). The
    # two call sites in windowProc pass dbl = 1 (0x1006965f, WM_LBUTTONDBLCLK) and dbl = 0 (0x1006968d,
    # WM_LBUTTONDOWN); the body does not test it, so both and other values go through unchanged.
    "onWmLButtonDown": dict(module="jgld.dll", addr=0x00069F40, abi="default", ret="int",
                            args=["pointer", "uint32", "uint32", "uint32", "uint32"],
                            fixture="c3cd_sink", state=_SINK,
                            vectors=[(0, 0, 0, 0, 0), (0, 1, 1, 2, 1), (0, 0, 0xFFFFFFFF, 0, 2),
                                     (0, 1, 0, 0xFFFFFFFF, 3), (0, 0, 0x7FFFFFFF, 1, 4),
                                     (0, 1, 1, 0x80000000, 5), (0, 2, 400, 300, 6), (0, 0, 0x1234, 0x5678, 7),
                                     (0, 1, 0xCCCCCCCC, 0x33333333, 8), (0, 0xFFFFFFFF, 799, 599, 9),
                                     (0, 0, 0x8000, 0x8000, 0xFFFFFFFF), (0, 1, 17, 19, 0x80000000)]),

    # onWmLButtonUp 0x00069fb0: __cdecl(hwnd, x, y, wParam) -> slot +0x20 with (x, y).
    "onWmLButtonUp": dict(module="jgld.dll", addr=0x00069FB0, abi="default", ret="int",
                          args=["pointer", "uint32", "uint32", "uint32"], fixture="c3cd_sink", state=_SINK,
                          vectors=[(0, 0, 0, 0), (0, 3, 4, 1), (0, 0xFFFFFFFF, 0, 2), (0, 0, 0xFFFFFFFF, 3),
                                   (0, 0x7FFFFFFF, 2, 4), (0, 2, 0x80000000, 5), (0, 401, 301, 6),
                                   (0, 0x2345, 0x6789, 7), (0, 0xCCCCCCCC, 0x33333333, 8), (0, 798, 598, 9),
                                   (0, 0x8001, 0x8001, 0xFFFFFFFF), (0, 23, 29, 0x80000000)]),

    # onWmRButtonDown 0x0006a010: __cdecl(hwnd, dbl, x, y, wParam) -> slot +0x24 with (dbl, x, y).
    "onWmRButtonDown": dict(module="jgld.dll", addr=0x0006A010, abi="default", ret="int",
                            args=["pointer", "uint32", "uint32", "uint32", "uint32"],
                            fixture="c3cd_sink", state=_SINK,
                            vectors=[(0, 0, 0, 0, 0), (0, 1, 5, 6, 1), (0, 0, 0xFFFFFFFF, 0, 2),
                                     (0, 1, 0, 0xFFFFFFFF, 3), (0, 0, 0x7FFFFFFF, 3, 4),
                                     (0, 1, 3, 0x80000000, 5), (0, 2, 402, 302, 6), (0, 0, 0x3456, 0x789A, 7),
                                     (0, 1, 0xCCCCCCCC, 0x33333333, 8), (0, 0xFFFFFFFF, 797, 597, 9),
                                     (0, 0, 0x8002, 0x8002, 0xFFFFFFFF), (0, 1, 31, 37, 0x80000000)]),

    # onWmRButtonUp 0x0006a080: __cdecl(hwnd, x, y, wParam) -> slot +0x28 with (x, y).
    "onWmRButtonUp": dict(module="jgld.dll", addr=0x0006A080, abi="default", ret="int",
                          args=["pointer", "uint32", "uint32", "uint32"], fixture="c3cd_sink", state=_SINK,
                          vectors=[(0, 0, 0, 0), (0, 7, 8, 1), (0, 0xFFFFFFFF, 0, 2), (0, 0, 0xFFFFFFFF, 3),
                                   (0, 0x7FFFFFFF, 4, 4), (0, 4, 0x80000000, 5), (0, 403, 303, 6),
                                   (0, 0x4567, 0x89AB, 7), (0, 0xCCCCCCCC, 0x33333333, 8), (0, 796, 596, 9),
                                   (0, 0x8003, 0x8003, 0xFFFFFFFF), (0, 41, 43, 0x80000000)]),

    # ----------------------------------------------------------------- the command handler 0x10128728
    #
    # onCommand 0x0006a0e0, handler INSTALLED (the `jne` side of 0x1006a0f8): the fixture's __cdecl
    # recording handler stores the two dwords it is handed and returns the first, so the compared state
    # is the order of those two arguments and the return value is the handler's, passed through
    # untouched. Twelve vectors, all with a different pair.
    "onCommand": dict(module="jgld.dll", addr=0x0006A0E0, abi="default", ret="int",
                      args=["pointer", "uint32", "uint32"], fixture="c3cd_cmd_handler", state=_CMDREC,
                      vectors=[(0, 0, 0), (0, 1, 2), (0, 0xFFFFFFFF, 0), (0, 0, 0xFFFFFFFF),
                               (0, 0x7FFFFFFF, 1), (0, 1, 0x80000000), (0, 0x1234, 0x5678),
                               (0, 0xCCCCCCCC, 0x33333333), (0, 100, 200), (0, 0x8000, 0x8000),
                               (0, 47, 53), (0, 0x80000000, 0x7FFFFFFF)]),

    # onCommand, handler NULL (the `je 0x1006a11d` side): every vector returns the 0 of `xor eax,eax`
    # and nothing is written, which is the point - this key exists for the branch, the spread comes from
    # the key above. Ten vectors.
    "onCommand_null": dict(module="jgld.dll", addr=0x0006A0E0, abi="default", ret="int",
                           args=["pointer", "uint32", "uint32"], fixture="c3cd_cmd_null", state=_GCMD,
                           vectors=[(0, 0, 0), (0, 1, 2), (0, 0xFFFFFFFF, 0), (0, 0, 0xFFFFFFFF),
                                    (0, 0x7FFFFFFF, 1), (0, 1, 0x80000000), (0, 0x1234, 0x5678),
                                    (0, 0xCCCCCCCC, 0x33333333), (0, 59, 61), (0, 0x8000, 0x8000)]),

    # storeCommandHandler 0x00067f60: __thiscall(handler), one store into 0x10128728 and nothing else.
    # `this` is a scratch buffer of the fixture's: the body spills it to [ebp-4] at 0x10067f7a and never
    # reads it. ret="void" because the epilogue does not reload a result (eax happens to hold the
    # argument, loaded at 0x10067f7d to address the store), so the A/B compares the global only. Twelve
    # `ret` stubs plus NULL, all different, so every vector changes the global to a different value.
    "storeCommandHandler": dict(module="jgld.dll", addr=0x00067F60, abi="thiscall", ret="void",
                                args=["pointer", "pointer"], fixture="c3cd_cmd_store", state=_GCMD_S,
                                vectors=[("$scratch", "$h%d" % i) for i in range(12)] + [("$scratch", 0)]),

    # Lib::setCommandHandler 0x00067f10: __thiscall(handler) -> storeCommandHandler on `this` + 4
    # (0x10067f34, 0x10067f37). The adjusted `this` is dead - storeCommandHandler never reads ecx - so
    # the only observable effect is the same store; the scratch buffer is in the state region to show
    # that neither `this` nor `this`+4 is written.
    "Lib::setCommandHandler": dict(module="jgld.dll", addr=0x00067F10, abi="thiscall", ret="void",
                                   args=["pointer", "pointer"], fixture="c3cd_cmd_store", state=_GCMD_S,
                                   vectors=[("$scratch", "$h%d" % i) for i in range(12)] + [("$scratch", 0)]),

    # ----------------------------------------------------- the four scalar deleting destructors
    #
    # `??_G`: run the class destructor on `this`, then `if (flags & 1) operator delete(this)`, then
    # return `this`. Every vector has bit 0 of flags CLEAR, so the delete is not taken (and the other 31
    # bits are shown to be discarded); log/c3/c3cd_notes.txt records why the delete side cannot be
    # A/B'd. Twelve objects each, carved out of one block that is the compared state in full, so what
    # the destructor writes - and what it leaves alone - is checked field by field.

    # Matrix::deletingDtor 0x00003600 -> Matrix::dtor 0x100032f0 (vtable dword, then the sixteen cells
    # at +4..+0x40 through Matrix::loadIdentity 0x100045f0). Returns `this`.
    "Matrix::deletingDtor": dict(module="jgld.dll", addr=0x00003600, abi="thiscall", ret="pointer",
                                 args=["pointer", "uint32"], fixture="c3cd_objects", state=_OBJ,
                                 vectors=[("$m%d" % i, _FLAGS[i]) for i in range(12)]),

    # Transform::deletingDtor 0x00003670 -> Transform::dtor 0x100034e0 (vtable dword, then the flags
    # word at +0x20 through Transform::reset 0x10005650; the quaternion at +4 and the position at +0x14
    # are left alone, which the block checks). Returns `this`.
    "Transform::deletingDtor": dict(module="jgld.dll", addr=0x00003670, abi="thiscall", ret="pointer",
                                    args=["pointer", "uint32"], fixture="c3cd_objects", state=_OBJ,
                                    vectors=[("$x%d" % i, _FLAGS[i]) for i in range(12)]),

    # LinkedList::deletingDtor 0x00006d80 -> LinkedList::dtor 0x10006cf0 (vtable dword, then
    # LinkedList::clear 0x10006df0). The twelve lists hold 0, 1, 2, 3, 5, 1, 2, 4, 0, 3, 6 and 1 nodes,
    # so both sides of clear's `m_head == 0` test and its ring walk are exercised and the marker counter
    # at the end of the block records how many nodes each one deleted - including clear's one-node leak
    # for every list of two or more (c3bw). Returns `this`.
    "LinkedList::deletingDtor": dict(module="jgld.dll", addr=0x00006D80, abi="thiscall", ret="pointer",
                                     args=["pointer", "uint32"], fixture="c3cd_objects", state=_OBJ,
                                     vectors=[("$l%d" % i, _FLAGS[i]) for i in range(12)]),

    # ListNode::deletingDtor 0x000073e0 -> ListNode::dtor 0x10007450 (the ListNode vtable VA 0x1011d098
    # at +0, next = 0 at +8 and prev = 0 at +4; the data pointer at +0xc and the flag byte at +0x10 are
    # left untouched, which the compared block checks).
    # Returns `this`.
    "ListNode::deletingDtor": dict(module="jgld.dll", addr=0x000073E0, abi="thiscall", ret="pointer",
                                   args=["pointer", "uint32"], fixture="c3cd_objects", state=_OBJ,
                                   vectors=[("$n%d" % i, _FLAGS[i]) for i in range(12)]),
}
