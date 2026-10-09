# Batch c3cn (2026-10-09): the heap stand-in pilot. Every key here runs under the fixture-installed
# __heapStandIn (re/frida/js/heapstandin.js, fixtures in re/frida/js/fixtures.d/c3cn.js), which for the
# length of the key carves jgld.dll's operator new 0x1007f0f0 out of a fixture arena and makes its
# operator delete 0x1007e6d0 record the pointer instead of freeing it. The arena and the free log are
# state regions, so both arms get the same block addresses and the frees are compared.
#
# The keys of the FIRST group re-test functions whose reimplementations already exist and whose heap
# branch no earlier key could reach (re/c3_hold.tsv, rounds 14 and 20):
#   Array_Rec14::add / Array_Rec94::add   shim/src/re/c3bb.cpp   (the operator-new growth branch)
#   the four scalar deleting destructors  shim/src/re/c3cd.cpp   (the operator-delete side, flags bit 0)
# Promote those with --file shim/src/re/c3bb.cpp / shim/src/re/c3cd.cpp, not with this batch's file.
#
# Hooked callees reached by these keys (all from earlier batches, all ON in the c3cn build):
#   Array<T>::add         none (its callees are the CRT memcpy 0x1007f3a0 and the stand-in's two)
#   Matrix::deletingDtor      Matrix::dtor jgld.dll:32f0 (c3bw) -> Matrix::loadIdentity jgld.dll:45f0 (c3bq)
#   Transform::deletingDtor   Transform::dtor jgld.dll:34e0 (c3bw) -> Transform::reset jgld.dll:5650 (c3at)
#   LinkedList::deletingDtor  LinkedList::dtor jgld.dll:6cf0, LinkedList::clear jgld.dll:6df0 (both c3bw)
#   ListNode::deletingDtor    ListNode::dtor jgld.dll:7450 (c3ax)

# The three state regions every key shares. Sizes must match re/frida/js/fixtures.d/c3cn.js:
# the arena block is arenaSize (0x8000) + the 0x10 header whose first dword is the cursor.
_ARENA = [("$arena", 0, 0x8010), ("$freelog", 0, 0x404)]
_ABLK = [("$ablk", 0, 0x6000)] + _ARENA
_OBJ = [("$blk", 0, 0x4000)] + _ARENA

# Five dwords for a Rec14 and 0x25 for a Rec94, passed by value on the stack behind `this` in ecx
# (`rep movsd` of 5 dwords from [ebp+8] at 0x100681e4, of 0x25 dwords at 0x10068447).
_V14 = [[0x61610000 + i, 0x72720000 + i * 5, -i - 3, 0x7FFFFFFF - i * 7, (i * 0x03030303) & 0xFFFFFFFF]
        for i in range(12)]
_V94 = [[(i * 0x400 + j * 0x11 + 0x29) & 0xFFFFFFFF for j in range(0x25)] for i in range(12)]

# flags of a scalar deleting destructor: only bit 0 decides (`and eax,1` at 0x10003628 and its twins).
# Seven vectors set it (the operator-delete side, new in this batch), five clear it, so one key covers
# both sides of that branch and the free log's count differs across the vectors of a single key.
_FLAGS = [0x00000001, 0x00000000, 0x00000003, 0x00000002, 0x00000005, 0x00000004,
          0x80000001, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE, 0x00000101, 0x7FFFFFFF]

HOOKS = {
    # ------------------------------------------------------------- Array<T>::add, the growth branch
    # Eight of the twelve headers have count >= capacity, so `jge 0x100681fa` (0x100681c9) takes the
    # growth path: the new buffer comes from the arena, the old one (null for one vector) is recorded
    # in the free log, and the new pointer is stored at +4 (0x10068295). Four keep count < capacity.
    "Array_Rec14::add_grow": dict(module="jgld.dll", addr=0x000681A0, abi="thiscall", ret="int",
                                  args=["pointer"] + ["int"] * 5, fixture="c3cn_arrays", state=_ABLK,
                                  vectors=[tuple([f"$a14_{i}"] + _V14[i]) for i in range(12)]),
    "Array_Rec94::add_grow": dict(module="jgld.dll", addr=0x00068400, abi="thiscall", ret="int",
                                  args=["pointer"] + ["int"] * 0x25, fixture="c3cn_arrays", state=_ABLK,
                                  vectors=[tuple([f"$a94_{i}"] + _V94[i]) for i in range(12)]),

    # ------------------------------------------- the four scalar deleting destructors, delete side on
    "Matrix::deletingDtor_free": dict(module="jgld.dll", addr=0x00003600, abi="thiscall", ret="pointer",
                                      args=["pointer", "uint32"], fixture="c3cn_objects", state=_OBJ,
                                      vectors=[(f"$m{i}", _FLAGS[i]) for i in range(12)]),
    "Transform::deletingDtor_free": dict(module="jgld.dll", addr=0x00003670, abi="thiscall", ret="pointer",
                                         args=["pointer", "uint32"], fixture="c3cn_objects", state=_OBJ,
                                         vectors=[(f"$x{i}", _FLAGS[i]) for i in range(12)]),
    "LinkedList::deletingDtor_free": dict(module="jgld.dll", addr=0x00006D80, abi="thiscall", ret="pointer",
                                          args=["pointer", "uint32"], fixture="c3cn_objects", state=_OBJ,
                                          vectors=[(f"$l{i}", _FLAGS[i]) for i in range(12)]),
    "ListNode::deletingDtor_free": dict(module="jgld.dll", addr=0x000073E0, abi="thiscall", ret="pointer",
                                        args=["pointer", "uint32"], fixture="c3cn_objects", state=_OBJ,
                                        vectors=[(f"$n{i}", _FLAGS[i]) for i in range(12)]),
}

# ---------------------------------------------------------------------------------------------------
# jgld.dll's allocation and release leaves (shim/src/re/c3cn.cpp), under the wider stand-in: operator
# new 0x1007f0f0, operator delete 0x1007e6d0, malloc 0x1007fb40, free 0x100806d0 and calloc 0x1007ffa0
# are all served from the fixture arena / free log, so every body above them is ordinary compared code.
#
# Hooked callees inside this batch (the verifier must re-run these five with the callee hooks off):
#   png_zalloc           -> png_malloc            jgld.dll:78e60
#   png_zfree            -> png_free              jgld.dll:78ed0
#   Array_e0::clearRet   -> Array_e0::clear       jgld.dll:68000
#   Array_Rec94::clearRet-> Array_Rec94::clear    jgld.dll:68360
#   Sprite::copyFrom     -> jalloc                jgld.dll:14a40
# No function of this group is called by an existing C3 function.
_HEAP = [("$hblk", 0, 0x6000), ("$arena", 0, 0x2010), ("$freelog", 0, 0x404)]
_HEAPBIG = [("$hblk", 0, 0x6000), ("$arena", 0, 0xA010), ("$freelog", 0, 0x404)]

_PTRS = [0] + [f"$p{i}" for i in range(11)]          # a NULL plus eleven fixture pointers

HOOKS.update({
    # jalloc 0x00014a40: `return malloc(size)` (0x10014a5c). Twelve sizes, including 0 (which carves
    # nothing from the arena) and the 16-byte boundaries either side of the arena's alignment.
    "jalloc": dict(module="jgld.dll", addr=0x00014A40, abi="default", ret="pointer", args=["uint32"],
                   fixture="c3cn_jgld_heap", state=_HEAP,
                   vectors=[(s,) for s in (0, 1, 7, 15, 16, 17, 0x40, 0x80, 0x100, 0x400, 0x1000, 0x1800)]),
    # jfree 0x00014a90: the null guard at 0x10014aa8 against eleven pointers that reach free 0x100806d0
    # at 0x10014ab6. The return is 0 for every vector; the free log is the evidence.
    "jfree": dict(module="jgld.dll", addr=0x00014A90, abi="default", ret="int", args=["pointer"],
                  fixture="c3cn_jgld_heap", state=_HEAP, vectors=[(p,) for p in _PTRS]),

    # png_malloc 0x00078e60: both sides of the two guards (png_ptr NULL at 0x10078e78, size 0 at
    # 0x10078e7e) and ten allocating sizes. The `p == NULL` arm at 0x10078e9b is NOT reachable here:
    # the arena never fails, and png_error 0x10078fe0 does not return.
    "png_malloc": dict(module="jgld.dll", addr=0x00078E60, abi="default", ret="pointer",
                       args=["pointer", "uint32"], fixture="c3cn_jgld_heap", state=_HEAP,
                       vectors=[(0, 0x10), ("$png", 0), (0, 0)]
                               + [("$png", s) for s in (1, 8, 0x10, 0x11, 0x40, 0x100, 0x400, 0x1000, 0x1800)]),
    # png_free 0x00078ed0: both sides of both guards (0x10078ee8, 0x10078eee) and nine real releases.
    "png_free": dict(module="jgld.dll", addr=0x00078ED0, abi="default", ret="void",
                     args=["pointer", "pointer"], fixture="c3cn_jgld_heap", state=_HEAP,
                     vectors=[(0, "$p0"), ("$png", 0), (0, 0)] + [("$png", f"$p{i}") for i in range(1, 10)]),
    # png_zalloc 0x000786b0: the `jbe 0x10078721` at 0x100786ec selects one memset or two. Four vectors
    # land on or below the 0x8000 boundary, four above it, and two have a zero product (png_malloc
    # returns NULL and the single memset writes nothing). The 0xa000-byte arena is for this key.
    "png_zalloc": dict(module="jgld.dll", addr=0x000786B0, abi="default", ret="pointer",
                       args=["pointer", "uint32", "uint32"], fixture="c3cn_jgld_heap_big", state=_HEAPBIG,
                       vectors=[("$png", 0, 0), ("$png", 0, 5), ("$png", 1, 1), ("$png", 4, 4),
                                ("$png", 3, 0x10), ("$png", 0x1000, 2), ("$png", 1, 0x8000),
                                ("$png", 2, 0x4000), ("$png", 0x100, 0x80), ("$png", 0x100, 0x81),
                                ("$png", 0x201, 0x40), ("$png", 0x81, 0x100)]),
    # png_zfree 0x00078750: forwards to png_free (0x10078770). Its own body has no branch, so the
    # vectors are the ones png_free's guards react to.
    "png_zfree": dict(module="jgld.dll", addr=0x00078750, abi="default", ret="void",
                      args=["pointer", "pointer"], fixture="c3cn_jgld_heap", state=_HEAP,
                      vectors=[(0, "$p0"), ("$png", 0), (0, 0)] + [("$png", f"$p{i}") for i in range(1, 10)]),
    # png_create_struct 0x00078da0: type 1 (0x21c bytes), type 2 (0xb8) and ten types that are neither,
    # which return NULL before allocating (0x10078dd6).
    "png_create_struct": dict(module="jgld.dll", addr=0x00078DA0, abi="default", ret="pointer",
                              args=["int"], fixture="c3cn_jgld_heap", state=_HEAP,
                              vectors=[(t,) for t in (0, 1, 2, 3, -1, 4, 5, 0x10, 1, 2,
                                                      0x7FFFFFFF, -0x80000000)]),
    # png_destroy_struct 0x00078e20: the null guard at 0x10078e38 and eleven releases.
    "png_destroy_struct": dict(module="jgld.dll", addr=0x00078E20, abi="default", ret="void",
                               args=["pointer"], fixture="c3cn_jgld_heap", state=_HEAP,
                               vectors=[(p,) for p in _PTRS]),

    # zcalloc 0x0009e680: both sides of the opaque test at 0x1009e698 (the branch adds size - size, so
    # it changes nothing) and twelve (items, size) pairs for calloc 0x1007ffa0.
    "zcalloc": dict(module="jgld.dll", addr=0x0009E680, abi="default", ret="pointer",
                    args=["pointer", "uint32", "uint32"], fixture="c3cn_jgld_heap", state=_HEAP,
                    vectors=[(0, 0, 0), (0, 1, 1), (0, 4, 4), (0, 8, 0x10), (0, 0x10, 0x20), (0, 0x100, 1),
                             ("$png", 0, 0), ("$png", 1, 1), ("$png", 4, 4), ("$png", 8, 0x10),
                             ("$png", 0x10, 0x20), ("$png", 0x100, 1)]),
    # zcfree 0x0009e6d0: free 0x100806d0 on the SECOND argument with no guard of its own, so a NULL
    # pointer reaches free and is recorded too.
    "zcfree": dict(module="jgld.dll", addr=0x0009E6D0, abi="default", ret="void",
                   args=["pointer", "pointer"], fixture="c3cn_jgld_heap", state=_HEAP,
                   vectors=[(0, 0), ("$png", 0)] + [("$png", f"$p{i}") for i in range(10)]),

    # Array<T>::clear, three byte-identical instances: the guard at 0x10068020 / 0x10068120 /
    # 0x10068380 releases the buffer through operator delete 0x1007e6d0, then four fields are written
    # (buffer 0, capacity 0, count 0, growth step 1). Six of the twelve headers have a buffer.
    "Array_e0::clear": dict(module="jgld.dll", addr=0x00068000, abi="thiscall", ret="void",
                            args=["pointer"], fixture="c3cn_jgld_heap", state=_HEAP,
                            vectors=[(f"$ar{i}",) for i in range(12)]),
    "Array_Rec14::clear": dict(module="jgld.dll", addr=0x00068100, abi="thiscall", ret="void",
                               args=["pointer"], fixture="c3cn_jgld_heap", state=_HEAP,
                               vectors=[(f"$ar{i}",) for i in range(12)]),
    "Array_Rec94::clear": dict(module="jgld.dll", addr=0x00068360, abi="thiscall", ret="void",
                               args=["pointer"], fixture="c3cn_jgld_heap", state=_HEAP,
                               vectors=[(f"$ar{i}",) for i in range(12)]),
    # The two clearRet wrappers return 0 whatever happens (0x10068595, 0x10068635), so their evidence
    # is the clear they run.
    "Array_e0::clearRet": dict(module="jgld.dll", addr=0x00068570, abi="thiscall", ret="int",
                               args=["pointer"], fixture="c3cn_jgld_heap", state=_HEAP,
                               vectors=[(f"$ar{i}",) for i in range(12)]),
    "Array_Rec94::clearRet": dict(module="jgld.dll", addr=0x00068610, abi="thiscall", ret="int",
                                  args=["pointer"], fixture="c3cn_jgld_heap", state=_HEAP,
                                  vectors=[(f"$ar{i}",) for i in range(12)]),

    # Sprite::copyFrom 0x0001a0e0: slot 2 of the destination's own vtable (the fixture's counting stub),
    # eight field copies, one jalloc + memcpy of the payload and, when the source's +0x1c is not null
    # (0x1001a1b0), a second jalloc + memcpy of the row table. Six sources have a row table, six do not,
    # and two have a zero-byte payload.
    "Sprite::copyFrom": dict(module="jgld.dll", addr=0x0001A0E0, abi="thiscall", ret="pointer",
                             args=["pointer", "pointer"], fixture="c3cn_jgld_heap", state=_HEAP,
                             vectors=[(f"$sd{i}", f"$ss{i}") for i in range(12)]),
})

# ---------------------------------------------------------------------------------------------------
# golf_clean.exe's release leaves (shim/src/re/c3cn.cpp part 3), under the stand-in over malloc
# 0x004a504f, operator new 0x004a57f1 and free 0x004a5007 (operator delete 0x004a4ffc tail-calls free).
#
# Hooked callees (the verifier must re-run these four with the callee hooks off):
#   Buf_releaseData   -> Buf_init 0x00474780          (c3n, already C3)
#   Buf_dtor          -> Buf_releaseData 0x004747e0   (this batch)
#   strFree           -> Buf_releaseData 0x004747e0   (this batch)
#   KeyTable::dtor    -> Buf_dtor 0x00474810          (this batch)
# No existing C3 function of the exe calls any address this group hooks.
_EXE = [("$eblk", 0, 0x5000), ("$arena", 0, 0x2010), ("$freelog", 0, 0x404)]

HOOKS.update({
    # Buf_releaseData 0x004747e0: six of the twelve Buf objects have a buffer at +8 (the free side of
    # the guard at 0x004747e8) and six do not; Buf_init 0x00474780 runs on all twelve.
    "Buf_releaseData": dict(module="golf_clean.exe", addr=0x004747E0, abi="thiscall", ret="void",
                            args=["pointer"], fixture="c3cn_exe_heap", state=_EXE,
                            vectors=[(f"$bf{i}",) for i in range(12)]),
    # Buf_dtor 0x00474810: the same twelve objects, plus the vtable store at +0 (0x00474810) that
    # separates it from Buf_releaseData.
    "Buf_dtor": dict(module="golf_clean.exe", addr=0x00474810, abi="thiscall", ret="void",
                     args=["pointer"], fixture="c3cn_exe_heap", state=_EXE,
                     vectors=[(f"$bf{i}",) for i in range(12)]),
    # strFree 0x0049d0e0: the same twelve, plus the +0x18 store (0x0049d0e8); +0 must stay untouched.
    "strFree": dict(module="golf_clean.exe", addr=0x0049D0E0, abi="thiscall", ret="int",
                    args=["pointer"], fixture="c3cn_exe_heap", state=_EXE,
                    vectors=[(f"$bf{i}",) for i in range(12)]),
    # KeyTable::dtor 0x004882f0: twelve KeyTables whose embedded Buf at +0x108 has a buffer in six.
    "KeyTable::dtor": dict(module="golf_clean.exe", addr=0x004882F0, abi="thiscall", ret="void",
                           args=["pointer"], fixture="c3cn_exe_heap", state=_EXE,
                           vectors=[(f"$kt{i}",) for i in range(12)]),
    # freeLoggerBuf 0x004a01a0: eight of the twelve have a buffer at +4, four do not; the vtable store
    # at +0 (0x004a01a6) happens either way.
    "freeLoggerBuf": dict(module="golf_clean.exe", addr=0x004A01A0, abi="thiscall", ret="void",
                          args=["pointer"], fixture="c3cn_exe_heap", state=_EXE,
                          vectors=[(f"$lg{i}",) for i in range(12)]),

    # jpeg_free_large 0x004b0470: the second argument reaches free with no guard, so a NULL is
    # recorded too; the first argument is never read, which the two null cinfo vectors show.
    "jpeg_free_large": dict(module="golf_clean.exe", addr=0x004B0470, abi="default", ret="void",
                            args=["pointer", "pointer"], fixture="c3cn_exe_heap", state=_EXE,
                            vectors=[(0, 0), (0, "$jp0"), ("$jp0", 0)]
                                    + [("$jp0", f"$jp{i}") for i in range(1, 10)]),
    # jpeg_get_large 0x004b0460: twelve sizes through malloc 0x004a504f, the first argument unread.
    "jpeg_get_large": dict(module="golf_clean.exe", addr=0x004B0460, abi="default", ret="pointer",
                           args=["pointer", "uint32"], fixture="c3cn_exe_heap", state=_EXE,
                           vectors=[(0, 0), ("$jp0", 1), ("$jp0", 15), ("$jp0", 16), ("$jp0", 17),
                                    ("$jp0", 0x40), ("$jp0", 0x100), ("$jp0", 0x400), ("$jp0", 0x800),
                                    ("$jp0", 0x1000), ("$jp0", 0x1800), (0, 0x20)]),

    # listRemoveHead 0x004a4ed0: three empty lists (the whole body skipped) and nine with a head whose
    # payload at +8 is null in half of them.
    "listRemoveHead": dict(module="golf_clean.exe", addr=0x004A4ED0, abi="thiscall", ret="void",
                           args=["pointer"], fixture="c3cn_exe_heap", state=_EXE,
                           vectors=[(f"$ls{i}",) for i in range(12)]),
    # listClear4a4d 0x004a4d50: the same twelve lists, 0 to 4 nodes each, so the empty exit
    # (0x004a4d9b) and one to four loop iterations are all taken.
    "listClear4a4d": dict(module="golf_clean.exe", addr=0x004A4D50, abi="thiscall", ret="void",
                          args=["pointer"], fixture="c3cn_exe_heap", state=_EXE,
                          vectors=[(f"$ls{i}",) for i in range(12)]),
    # bufferFree 0x004a05d0: the twelve (this, record) pairs cover the four combinations of a null or
    # non-null buffer at record+4 and a zero or non-zero guard at this+0x18.
    "bufferFree": dict(module="golf_clean.exe", addr=0x004A05D0, abi="thiscall", ret="void",
                       args=["pointer", "pointer"], fixture="c3cn_exe_heap", state=_EXE,
                       vectors=[(f"$bt{i}", f"$br{i}") for i in range(12)]),
    # NodeList::clear 0x00487310: three empty lists (only the +8 store runs) and nine with 1 to 4
    # nodes, so the peeled first iteration, the loop and both sides of the prev test are taken.
    "NodeList::clear": dict(module="golf_clean.exe", addr=0x00487310, abi="thiscall", ret="void",
                            args=["pointer"], fixture="c3cn_exe_heap", state=_EXE,
                            vectors=[(f"$nl{i}",) for i in range(12)]),
})

# ---------------------------------------------------------------------------------------------------
# sound.dll's linked-list helpers (shim/src/re/c3cn.cpp part 4), under the stand-in over operator new
# 0x100424d7, malloc 0x10042acd and operator delete 0x1004249a. Twenty-two addresses, four distinct
# bodies; log/c3/alt/c3cn_twins.txt is the instruction-by-instruction proof that the members of each
# family are identical. No hooked callee: their only callee is the allocator the stand-in serves, and
# no existing C3 function of sound.dll calls any of these addresses.
_SND = [("$sblk", 0, 0x2000), ("$arena", 0, 0x2010), ("$freelog", 0, 0x404)]

# ten append helpers: nine tail inserts and NodeList_pushFront, which anchors on the head instead
_SND_PUSH = {"appendNodeB": 0x00005E30, "appendNodeC": 0x0000E680, "appendNode": 0x000137F0,
             "PtrList_pushBack": 0x0001D150, "NodeList_pushBack": 0x00024DE0,
             "ResourceList_pushBack": 0x00031B60, "nodeListAppendValue": 0x00038430,
             "listAppendNodeA": 0x00038550, "listAppendNodeB": 0x000385D0,
             "NodeList_pushFront": 0x00024F90}
# nine "free every node" helpers
_SND_CLEAR = {"clearNodeListD": 0x00005A60, "clearNodeListE": 0x00005B60, "clearNodeListF": 0x00006C70,
              "clearNodeListG": 0x0000AE80, "clearNodeListC": 0x0000E5E0, "clearNodeListA": 0x00013620,
              "clearNodeListB": 0x00013750, "PtrList_freeNodesA": 0x0001F1C0,
              # hooks.csv names sound.dll 0x00024d40 "NodeList_clear" (the exe's unrelated
              # NodeList::clear 0x00487310 is a different name and a different module)
              "NodeList_clear": 0x00024D40}
# two "pop the head" helpers
_SND_POP = {"listPopFront": 0x00005C00, "nodeListPopFront": 0x00039620}

# Six of the twelve lists already hold a node and six are empty, so both arms of the tail/head test
# (0x10005e5e, 0x10024fbd) run inside every append key.
HOOKS.update({name: dict(module="sound.dll", addr=addr, abi="thiscall", ret="int",
                         args=["pointer", "pointer"], fixture="c3cn_sound_heap", state=_SND,
                         vectors=[(f"$pl{i}", f"$pv{i}") for i in range(12)])
              for name, addr in _SND_PUSH.items()})

# Three of the twelve lists are empty (the body returns at once) and the others hold 1 to 4 nodes, so
# the peeled first iteration, one to four loop passes and both sides of the +4 test are taken.
HOOKS.update({name: dict(module="sound.dll", addr=addr, abi="thiscall", ret="void",
                         args=["pointer"], fixture="c3cn_sound_heap", state=_SND,
                         vectors=[(f"$cl{i}",) for i in range(12)])
              for name, addr in _SND_CLEAR.items()})

# popFront over the same twelve lists: three empty (returning the null head), nine removing one node
# and returning its +8, which is null only for a one-node list.
HOOKS.update({name: dict(module="sound.dll", addr=addr, abi="thiscall", ret="pointer",
                         args=["pointer"], fixture="c3cn_sound_heap", state=_SND,
                         vectors=[(f"$cl{i}",) for i in range(12)])
              for name, addr in _SND_POP.items()})

HOOKS.update({
    # freeDynArray 0x00006ec0: six records have a buffer at +4 (the operator-delete side of the guard
    # at 0x10006ed1) and the flag word at +8 has bit 0 set in half of them, so the `and edx, ~1` is
    # shown both to clear a set bit and to leave a clear one alone.
    "freeDynArray": dict(module="sound.dll", addr=0x00006EC0, abi="thiscall", ret="void",
                         args=["pointer"], fixture="c3cn_sound_heap", state=_SND,
                         vectors=[(f"$da{i}",) for i in range(12)]),
})

# ---------------------------------------------------------------------------------------------------
# Terrain.dll's std::list<Tile*> allocation layer (shim/src/re/c3cn.cpp part 5), under the stand-in
# over operator new 0x10018c90 and operator delete 0x10018be0.
#
# Hooked callees (one boot each, because every set would otherwise switch off a function under test):
#   std::list<Tile*>::list       -> _Buynode b500, _Charalloc b910, std::_Allocate bae0  (this batch)
#   std::list<Tile*>::_Buynode   -> _Charalloc b910, std::_Allocate bae0 (this batch) and
#                                   _Acc::_Next b600 / _Acc::_Prev b630 (c3aw, already C3)
#   std::allocator<Tile*>::_Charalloc -> std::_Allocate bae0  (this batch)
#   std::list<Tile*>::_Freenode  -> std::allocator<Tile*>::deallocate b960  (this batch)
# Keys with `<`, `>` or `*` must be handed to diff_hook from Python, not through a shell.
_TERR = [("$tblk", 0, 0x1000), ("$arena", 0, 0x1010), ("$freelog", 0, 0x404)]

HOOKS.update({
    # std::_Allocate 0x0000bae0: twelve counts, four of them negative, so both sides of the signed
    # clamp at 0x1000bafc run and the arena cursor shows the clamped value reaching operator new.
    "std::_Allocate": dict(module="Terrain.dll", addr=0x0000BAE0, abi="default", ret="pointer",
                           args=["int"], fixture="c3cn_terrain_heap", state=_TERR,
                           vectors=[(n,) for n in (0, 1, 15, 16, 17, 0xC, 0x40, 0x100,
                                                   -1, -16, -0x80000000, 0x400)]),
    # std::allocator<Tile*>::deallocate 0x0000b960: the first argument reaches operator delete with no
    # guard, so a NULL is recorded too; the element count is shown unread by varying it.
    "std::allocator<Tile*>::deallocate": dict(
        module="Terrain.dll", addr=0x0000B960, abi="thiscall", ret="void",
        args=["pointer", "pointer", "uint32"], fixture="c3cn_terrain_heap", state=_TERR,
        vectors=[("$tl0", 0, 1), ("$tl0", 0, 0)] + [("$tl0", f"$tn{i}", i) for i in range(10)]),
    # std::allocator<Tile*>::_Charalloc 0x0000b910: twelve sizes through std::_Allocate 0x1000bae0.
    "std::allocator<Tile*>::_Charalloc": dict(
        module="Terrain.dll", addr=0x0000B910, abi="thiscall", ret="pointer",
        args=["pointer", "uint32"], fixture="c3cn_terrain_heap", state=_TERR,
        vectors=[("$tl0", s) for s in (0, 1, 0xC, 0x10, 0x11, 0x20, 0x40, 0x80, 0x100, 0x200, 7, 0x400)]),
    # std::list<Tile*>::_Freenode 0x0000b5b0: eleven scratch nodes and a NULL, each recorded once in
    # the free log by the deallocate below it.
    "std::list<Tile*>::_Freenode": dict(
        module="Terrain.dll", addr=0x0000B5B0, abi="thiscall", ret="void",
        args=["pointer", "pointer"], fixture="c3cn_terrain_heap", state=_TERR,
        vectors=[("$tl0", 0)] + [("$tl0", f"$tn{i}") for i in range(11)]),
    # std::list<Tile*>::_Buynode 0x0000b500: the four combinations of a null or non-null next and prev,
    # so both `?:` at 0x1000b52e and 0x1000b553 take each side; the node always comes from the arena.
    "std::list<Tile*>::_Buynode": dict(
        module="Terrain.dll", addr=0x0000B500, abi="thiscall", ret="pointer",
        args=["pointer", "pointer", "pointer"], fixture="c3cn_terrain_heap", state=_TERR,
        vectors=[("$tl0", 0, 0), ("$tl0", "$tn0", 0), ("$tl0", 0, "$tn1"), ("$tl0", "$tn2", "$tn3"),
                 ("$tl1", 0, 0), ("$tl1", "$tn4", 0), ("$tl1", 0, "$tn5"), ("$tl1", "$tn6", "$tn7"),
                 ("$tl2", 0, 0), ("$tl2", "$tn8", 0), ("$tl2", 0, "$tn9"), ("$tl2", "$tn10", "$tn0")]),
    # std::list<Tile*>::list 0x0000ae60: twelve lists built from twelve allocator objects whose single
    # byte differs, so the +0 copy is visible; each one takes a sentinel node from the arena.
    "std::list<Tile*>::list": dict(
        module="Terrain.dll", addr=0x0000AE60, abi="thiscall", ret="pointer",
        args=["pointer", "pointer"], fixture="c3cn_terrain_heap", state=_TERR,
        vectors=[(f"$tl{i}", f"$ta{i}") for i in range(12)]),
})
