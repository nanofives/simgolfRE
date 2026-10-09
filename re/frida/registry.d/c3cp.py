# Batch c3cp (round 22, 2026-10-09): golf_clean.exe C++ static-initialization families, high half
# (shim/src/re/c3cp.cpp, fixtures in re/frida/js/fixtures.d/c3cp.js, purposes in log/c3/c3cp_purpose.md).
#
# Every key runs with the game's FIRST thread suspended (it owns the window and is the only thread that touches these
# globals; the fixture resumes it from setTimeout(0), after the rpc call that ran the key, once the globals hold the
# game's bytes again) and with _atexit 0x004a56d2 replaced, for the A/B thread only, by a recording stand-in: each
# routine passed to it is appended to $atexitlog (count at +0) and the stand-in returns the key's value (0 or -1), so
# nothing is ever registered for real and the dynInit's return value is the stand-in's.
#
#   plain     dynInit whose constructor writes only its object: variants _live (the game's own bytes, _atexit -> 0),
#             _pat (a dword pattern seeded over the object, _atexit -> -1), _zero (zeroed object, _atexit -> 0)
#   disp      same, for the Image / View constructors that reach 0x00483da0 (`or [0x0083ad4c], 0x3f`, the display's
#             slot +0x150 when 0x0083ad50 is not NULL at 0x00483db7, then 0x0083ad4c = 0): _live and _pat run with
#             0x0083ad50 = $sink (a recording display), _zero with 0x0083ad50 = NULL
#   heap_new / heap_malloc   allocTileBuffer (operator new 0x004a57f1) / initGlobalReader (malloc 0x004a504f): the
#             allocator is heapstandin.js's arena for the A/B thread
#   atexitdtor  the exit routine on an object built by the game's own constructor stub and then seeded: _full (every
#             Surface member has a display handle at +4, a buffer at +8 and the first three a cache at +0xc), _mixed
#             (one field per member, by member index mod 4), _nodisplay (_full with 0x0083ad50 = NULL: Surface_init
#             0x00473ae0 returns at 0x00473aee), _empty (as constructed); _free 0x004a5007 and operator delete
#             0x004a4ffc only record into $freelog, display slots +0x90 / +0x94 record into $sink, and the caches'
#             entry objects' deleting destructors (slot 0) record into $caches
#   cachedtor atexitDtor_83aca8 on the object staticInit_g83ac88 0x00482f00 builds, guard byte 0x0083ac28 = 0x00 /
#             0x01 / 0xfe / 0xff (_clear / _guard / _high / _guardhigh)
#   register  the three atexitRegister_* (_ok: the stand-in returns 0, _fail: -1)
#   store     storeUiEventTable, ten keys, each over a different prior value of 0x0083a4f8 (a leaf: 10 vectors)
#
# allocTileBuffer's state leaves out the byte 0x00820f18: it is byte 3 of the CALLER's ecx (`push ecx` 0x00449210,
# `mov al, [esp+3]` 0x00449211), which differs between the two arms; the fixture still saves and restores it.

_TABLE = [
    (0x004058e0, 'dynInit_staticInit_004058f0', 'disp', [(0x005a7148, 0x2b4), (0x0083ad4c, 0x4)]),
    (0x00405900, 'atexitRegister_00405900', 'register', []),
    (0x004061a0, 'dynInit_staticInit_004061b0', 'disp', [(0x00568d10, 0x2b4), (0x0083ad4c, 0x4)]),
    (0x004061c0, 'atexitRegister_004061c0', 'register', []),
    (0x004061e0, 'dynInit_staticInit_004061f0', 'disp', [(0x00587ae8, 0x2b4), (0x0083ad4c, 0x4)]),
    (0x00406200, 'atexitRegister_00406200', 'register', []),
    (0x004378d0, 'dynInit_staticCtor_4378e0', 'plain', [(0x005aa560, 0x58)]),
    (0x00438590, 'dynInit_staticCtor_4385a0', 'disp', [(0x005a9f78, 0x5dc), (0x0083ad4c, 0x4)]),
    (0x0043cc00, 'dynInit_arrayStaticInit_43cc10', 'plain', [(0x0081ca10, 0x4150)]),
    (0x0043cc60, 'dynInit_staticCtor_43cc70', 'plain', [(0x005aa6f0, 0x58)]),
    (0x0043cca0, 'dynInit_staticCtor_43ccb0', 'plain', [(0x005aa6d0, 0x1c)]),
    (0x0043d4d0, 'dynInit_staticCtor_43d4e0', 'plain', [(0x00820e90, 0x28)]),
    (0x0043d630, 'dynInit_staticCtor_43d640', 'disp', [(0x005aa778, 0x2b4), (0x0083ad4c, 0x4)]),
    (0x00448150, 'dynInit_arrayStaticInit_448160', 'plain', [(0x0080d840, 0x7e90)]),
    (0x004491b0, 'dynInit_staticCtor_4491c0', 'plain', [(0x00820ed8, 0x28)]),
    (0x00449200, 'dynInit_allocTileBuffer', 'heap_new', [(0x00820f1c, 0x8)]),
    (0x0044ac50, 'dynInit_initPanelState', 'plain', [(0x00821020, 0x8), (0x00821ec8, 0x68)]),
    (0x0044ad10, 'dynInit_arrayStaticInit_44ad20', 'plain', [(0x00820f40, 0xd8)]),
    (0x0044ad70, 'dynInit_staticCtor_44ad80', 'plain', [(0x00821040, 0x28)]),
    (0x0044adc0, 'dynInit_staticCtor_44add0', 'plain', [(0x008210c8, 0x54)]),
    (0x0044adf0, 'atexitDtor_8210c8', 'atexitdtor', [(0x008210c8, 0x58)]),
    (0x0044aeb0, 'dynInit_staticCtor_44aec0', 'plain', [(0x00821c60, 0x54)]),
    (0x0044aee0, 'atexitDtor_821c60', 'atexitdtor', [(0x00821c60, 0x58)]),
    (0x0044aef0, 'dynInit_staticCtor_44af00', 'plain', [(0x00821bf8, 0x54)]),
    (0x0044af20, 'atexitDtor_821bf8', 'atexitdtor', [(0x00821bf8, 0x58)]),
    (0x0044af30, 'dynInit_staticCtor_44af40', 'plain', [(0x00821070, 0x54)]),
    (0x0044af60, 'atexitDtor_821070', 'atexitdtor', [(0x00821070, 0x58)]),
    (0x0044af70, 'dynInit_staticCtor_44af80', 'plain', [(0x00821200, 0x104)]),
    (0x0044afa0, 'atexitDtor_821200', 'atexitdtor', [(0x00821200, 0x108)]),
    (0x0044b0f0, 'dynInit_staticCtor_44b100', 'plain', [(0x008223c0, 0x104)]),
    (0x0044b120, 'atexitDtor_8223c0', 'atexitdtor', [(0x008223c0, 0x108)]),
    (0x0044b130, 'dynInit_staticCtor_44b140', 'plain', [(0x008222b8, 0x104)]),
    (0x0044b160, 'atexitDtor_8222b8', 'atexitdtor', [(0x008222b8, 0x108)]),
    (0x0044b170, 'dynInit_staticCtor_44b180', 'plain', [(0x00821308, 0x8ec)]),
    (0x0044b1a0, 'atexitDtor_821308', 'atexitdtor', [(0x00821308, 0x8f0)]),
    (0x0044b3a0, 'dynInit_staticCtor_44b3b0', 'plain', [(0x00821f48, 0x36c)]),
    (0x0044b3d0, 'atexitDtor_821f48', 'atexitdtor', [(0x00821f48, 0x370)]),
    (0x0044b4b0, 'dynInit_staticCtor_44b4c0', 'plain', [(0x008224c8, 0x6b0)]),
    (0x0044b4e0, 'atexitDtor_8224c8', 'atexitdtor', [(0x008224c8, 0x6b4)]),
    (0x0044b650, 'dynInit_staticCtor_44b660', 'plain', [(0x00821120, 0xd8)]),
    (0x0044b680, 'atexitDtor_821120', 'atexitdtor', [(0x00821120, 0xdc)]),
    (0x0044b750, 'dynInit_staticCtor_44b760', 'plain', [(0x00821cb8, 0x20c)]),
    (0x0044b780, 'atexitDtor_821cb8', 'atexitdtor', [(0x00821cb8, 0x210)]),
    (0x0045ba50, 'dynInit_staticCtor_45ba60', 'plain', [(0x00822cc8, 0x14)]),
    (0x0045baa0, 'dynInit_staticCtor_45bab0', 'plain', [(0x00822c98, 0x28)]),
    (0x0046de30, 'dynInit_gImageCtor_838f98', 'disp', [(0x00838f98, 0x2b4), (0x0083ad4c, 0x4)]),
    (0x00473a60, 'dynInit_initGlobalImage', 'disp', [(0x00839348, 0x2b4), (0x0083ad4c, 0x4)]),
    (0x00474a80, 'dynInit_gSurfaceCtor_839a68', 'plain', [(0x00839a68, 0x28)]),
    (0x004799d0, 'storeUiEventTable', 'store', [(0x0083a4f8, 0x4)]),
    (0x004799e0, 'dynInit_gImageCtor_83a500', 'disp', [(0x0083a500, 0x2b4), (0x0083ad4c, 0x4)]),
    (0x00479a30, 'dynInit_gImageCtor_83a7b8', 'disp', [(0x0083a7b8, 0x2b4), (0x0083ad4c, 0x4)]),
    (0x00482e70, 'dynInit_staticInit_g83acb0', 'plain', [(0x0083acb0, 0x58)]),
    (0x00482eb0, 'dynInit_staticInit_g83ac30', 'plain', [(0x0083ac30, 0x58)]),
    (0x00482ef0, 'dynInit_staticInit_g83ac88', 'plain', [(0x00839650, 0x4), (0x0083ac88, 0x28)]),
    (0x00482f20, 'atexitDtor_83aca8', 'cachedtor', [(0x0083ac28, 0x1), (0x0083ac88, 0x28), (0x00839650, 0x4)]),
    (0x004837a0, 'dynInit_screenMgrCtor4837b0', 'plain', [(0x0083ad20, 0x8)]),
    (0x00483dd0, 'dynInit_staticInit_g83ad80', 'plain', [(0x0083ad80, 0x1e4)]),
    (0x00483e10, 'dynInit_staticInit_g83ad58', 'plain', [(0x0083ad58, 0x20)]),
    (0x00483e50, 'dynInit_staticInit_g83af98', 'plain', [(0x0083af98, 0x20)]),
    (0x00487d70, 'dynInit_initGlobalReader', 'heap_malloc', [(0x0083b000, 0x164)]),
    (0x00487e00, 'dynInit_initKeyTableArray', 'plain', [(0x0083b170, 0x480)]),
    (0x0049bfe0, 'dynInit_initGlobalObj_49c020', 'disp', [(0x0083ad4c, 0x4), (0x008400c8, 0x5c8)]),
    (0x0049d000, 'dynInit_initPairArrayGlobal', 'plain', [(0x008406e8, 0x1c)]),
    (0x0049d150, 'dynInit_initGfxGlobal_840710', 'plain', [(0x00840710, 0x28)]),
    (0x0049d290, 'dynInit_initGfxGlobal_8407c0', 'plain', [(0x008407c0, 0x28)]),
    (0x0049d2f0, 'dynInit_initGfxGlobal_840790', 'plain', [(0x00840790, 0x28)]),
    (0x0049d350, 'dynInit_initGfxGlobal_8407f0', 'plain', [(0x008407f0, 0x28)]),
    (0x0049d480, 'dynInit_initGfxGlobal_840830', 'plain', [(0x00840830, 0x28)]),
    (0x0049d4e0, 'dynInit_initGfxGlobal_840890', 'plain', [(0x00840890, 0x28)]),
    (0x0049d540, 'dynInit_initGfxGlobal_840860', 'plain', [(0x00840860, 0x28)]),
    (0x0049eaf0, 'dynInit_initGfxGlobal_8408c8', 'plain', [(0x008408c8, 0x28)]),
    (0x0049eb50, 'dynInit_initGfxGlobal_8408f8', 'plain', [(0x008408f8, 0x28)]),
]

_ARENA = 0x1000
_EXTRA = {
    "plain": [("$atexitlog", 0, 0x44)],
    "disp": [("$atexitlog", 0, 0x44), ("$sink", 0, 0x800)],
    "heap_new": [("$atexitlog", 0, 0x44), ("$arena", 0, 0x10 + _ARENA), ("$freelog", 0, 0x404)],
    "heap_malloc": [("$atexitlog", 0, 0x44), ("$arena", 0, 0x10 + _ARENA), ("$freelog", 0, 0x404)],
    "atexitdtor": [("$sink", 0, 0x800), ("$caches", 0, 0x800), ("$freelog", 0, 0x404)],
    "cachedtor": [],
    "register": [("$atexitlog", 0, 0x44)],
    "store": [],
}
_VARIANTS = {
    "plain": ["live", "pat", "zero"], "disp": ["live", "pat", "zero"],
    "heap_new": ["live", "pat", "zero"], "heap_malloc": ["live", "pat", "zero"],
    "atexitdtor": ["full", "mixed", "nodisplay", "empty"],
    "cachedtor": ["clear", "guard", "high", "guardhigh"],
    "register": ["ok", "fail"],
    "store": ["s%d" % i for i in range(10)],
}
# the dynInit and atexitRegister functions return _atexit's int; exit routines and storeUiEventTable return nothing
_RET = {"plain": "int", "disp": "int", "heap_new": "int", "heap_malloc": "int", "register": "int",
         "atexitdtor": "void", "cachedtor": "void", "store": "void"}

HOOKS = {}
for _addr, _name, _kind, _regs in _TABLE:
    for _v in _VARIANTS[_kind]:
        HOOKS[f"{_name}_{_v}"] = dict(
            module="golf_clean.exe", addr=_addr, abi="default", ret=_RET[_kind], args=[],
            fixture=f"c3cp_{_addr:08x}_{_v}", state=[(_a, 0, _n) for _a, _n in _regs] + _EXTRA[_kind],
            vectors=[[]])
