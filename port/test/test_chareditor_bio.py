"""Roadmap S1.5 (U-1032): showCharacterEditor's bio entry and the EditBox family (port/src/sim/chareditor.cpp, notes
log/roadmap/S1/5_notes.md).

The evidence is the EMULATOR ORACLE (opt-in SG_EMU_ORACLE=1) on the ORIGINAL under Unicorn (port/test/cmdemu.py):

1. EDITBOX FUNCTIONS. EditBox::setText, resizeBuffer, insertChar, onKey (no Up / Down), resetFields, releaseState and create
   run over random EditBox objects whose text lives in the data image, against the port's registered functions. Stubbed
   on the original's side, as window-tree bookkeeping the port does not model (U-1191): the layout slot +0x120
   (Window::doLayout), Window::create, Widget_setQuad, EditBox_setCursorChar, Timer::initA / startMmPeriodic / stop /
   reset, Window::ctorInit; malloc hands out blocks inside the data image (the port's arena block is copied there with
   chareditor.peek before the images are compared, and the text pointer word is compared as "same block"), free does
   nothing; strncpy and memmove run for real; MessageBeep, GetKeyState, MapVirtualKeyA are import handlers.
2. THE BIO ENTRY in showCharacterEditor, with the KEY ROUTING of the original running for real: a scripted WM_KEYDOWN goes
   through uiKeyEvent(vk, 1) (onCommand, Window::dispatchAccel over the root's focus ring), a WM_CHAR through
   uiKeyEvent148 (Window::onKeyChar, Window::key), a WM_KEYUP through uiKeyEvent(vk, 0), at the editor's pumps
   (pumpMessages is replaced by its own semantics around a trampoline holding the scripted group), with drawing stubbed
   as in port/test/test_chareditor.py. Window::create is replaced by a stub that does what the live game showed
   (cov_r_bio probe4): the EditBox enters the root window's focus ring (head / cursor node, count 1) with the EditBox
   vtable 0x004ba4a8; Window::ctorInit takes it out again (the live root's count was 0 after the bio). The port gets what
   the trace recorder would have recorded: every root handler the original ran (identified by the writes it made, logged
   per scripted message between marker stores) as a RootHandler command, a key message no root handler took as command 12
   (WM_KEYDOWN) / 13 (WM_CHAR); tick.cpp parse_trace makes the same commands from a recording. The whole data image, eax,
   the consumed input and the cursor reads must be equal, with the routing's own ui bookkeeping masked (_route_mask:
   g_curWidget, the last-key word, the focus-owner cache, the key ring and its pointer, the stub-written EditBox words).

The default suite checks GOLDEN digests pinned from the port after the oracle was GREEN on the same cases
(SG_EMU_ORACLE=update) and the negative controls against them.
"""
from __future__ import annotations

import ctypes as C
import json
import os
import pathlib
import random
import struct

import pytest

from port.test import lib as L
from port.test import test_chareditor as TC
from port.test.test_commands import DATA_VA, base_image, digest, objs, put32

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "port" / "test" / "golden_chareditor.json"   # shared with test_chareditor.py (keys "bio", "EditBox::...")

pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")

# ---- addresses of the original (the oracle's side only; port code cites names)
EB_ENTRY = {"EditBox::setText": 0x00486200, "EditBox::resizeBuffer": 0x00486250, "EditBox::insertChar": 0x004866F0,
            "EditBox::onKey": 0x004867B0, "EditBox::resetFields": 0x00485FF0, "EditBox::releaseState": 0x004860D0,
            "EditBox::create": 0x00486110}
EB_VTABLE = 0x004BA4A8            # EditBox::ctor 0x004860a7
MALLOC, FREE = 0x004A504F, 0x004A5007
WINDOW_CREATE, WINDOW_CTORINIT = 0x004806C0, 0x00480610
UI_DROPS = [0x00480D70,           # Window::doLayout (the EditBox's slot +0x120)
            0x00476310,           # Widget_setQuad (the frame quad)
            0x0047AB00,           # EditBox_setCursorChar
            0x00486CF0, 0x00486DC0, 0x00486EC0, 0x00486F10,   # Timer::initA, startMmPeriodic, stop, reset
            0x0047B670, 0x0047B820,                         # Window::show, Window::detach
            0x0040CC00, 0x00479830, 0x004798C0, 0x00479950,   # drawPanelS2, pixelRed / Green / Blue
            0x00483BB0, 0x00483D80, 0x00480C80]               # showScreen, leaveModal483d80, Window::refreshFull
UI_KEY_EVENT, UI_KEY_EVENT148 = 0x00495910, 0x00495990
ROOT_ON_KEY, ROOT_ON_CHAR, ROOT_ON_CHAR0 = (0x00465750, 0x004657B0), (0x004657B0, 0x004657D0), (0x004657D0, 0x004657E0)
ROOT_WINDOW = 0x00519A60
EDITBOX = 0x005A9F78              # g_characterEditorEditBox
ROOT_VTABLE = 0x004BA2FC          # the root window's class (re/names/exe_13.tsv: rootWin_* are its slots 29..39)
G_DIRTY_A, G_DIRTY_B, G_CHAR = 0x00822B98, 0x00822B9C, 0x0083821C
PUMP_MESSAGES = 0x0045C030
BLOCK = 0x34000000                # node, marker, trampolines, heap (the oracle's own memory)
BLOCK_SIZE, HEAP_OFF = 0x200000, 0x1C0000
# chareditor.cpp's EditBox arena for the tests' data base 0x004c1000: (base + sizeof(GameState) 0x381174 + 0x2800000) rounded
# down to 64 KB, its first free placement (the oracle's malloc uses the same address and rule)
ARENA_VA, ARENA_SIZE = 0x03040000, 0x10000
NODE, MARK, TRAMP2 = BLOCK, BLOCK + 0x100, BLOCK + 0x1000
# US-layout MAPVK_VK_TO_CHAR (MapVirtualKeyA(vk, 2)) for the keys the cases press: dispatchAccel's needKey test
VK_CHAR = {0x08: 0x08, 0x09: 0x09, 0x0D: 0x0D, 0x1B: 0x1B, 0x20: 0x20}
VK_CHAR.update({v: v for v in range(0x30, 0x3A)})
VK_CHAR.update({v: v for v in range(0x41, 0x5B)})
EB_KEYS = (0x08, 0x23, 0x24, 0x25, 0x27, 0x2E)   # the keys EditBox::onKey handles (Up / Down 0x26 / 0x28 excluded: U-1190)


def _va(lib, name, at=0):
    return DATA_VA + objs(lib)[name][0] + at


def _eb(lib):
    return _va(lib, "g_characterEditorEditBox")


def _imports(emu):
    emu.import_handlers["MessageBeep"] = (1, lambda uc, a: 1)
    emu.import_handlers["GetKeyState"] = (1, lambda uc, a: 0)
    emu.import_handlers["MapVirtualKeyA"] = (2, lambda uc, a: VK_CHAR.get(a[0] & 0xFFFF, 0) if a[1] == 2 else 0)


class Heap:
    """the original's malloc / free: blocks of 16-byte multiples from `base`, the cursor back at `base` when no block is
    live. For the editor runs `base` is ARENA_VA with chareditor.cpp's own rule, so the text pointer words of both sides
    are equal; for the function cases it lies inside the data image (the result image holds the blocks)."""

    def __init__(self, base):
        self.base, self.cur, self.blocks, self.live = base, base, [], set()

    def malloc(self, uc, a, ecx):
        if not self.live:
            self.cur = self.base
        p = self.cur
        self.cur += (a[0] + 15) & ~15
        self.blocks.append((p, a[0]))
        self.live.add(p)
        return p

    def free(self, uc, a, ecx):
        self.live.discard(a[0])
        return 0


def _ui_stubs(emu, heap, ring=True):
    for a in UI_DROPS:
        try:
            emu.drop(a)
        except ValueError:   # a tail-jumping body (showScreen, leaveModal483d80: no `ret` of their own, no arguments)
            emu.stub(a, 0, 0, False)
    emu.stub_py(MALLOC, 1, heap.malloc, False, "malloc")
    emu.stub_py(FREE, 1, heap.free, False, "free")
    eb = EDITBOX

    def create(uc, a, ecx):   # Window::create(x, y, w, h, label, flags, parent, owner, border): ret 0x24
        uc.mem_write(ecx, struct.pack("<I", EB_VTABLE))
        if ring and ecx == eb:
            par = a[6]
            uc.mem_write(NODE, struct.pack("<IIIII", 0, ecx, 0, NODE, NODE))
            uc.mem_write(par + 0x138, struct.pack("<IIII", NODE, NODE, 1, 0))
            uc.mem_write(ecx + 0x130, struct.pack("<I", par))
        return 0

    def ctor_init(uc, a, ecx):
        if ring and ecx == eb:
            par = struct.unpack("<I", uc.mem_read(ecx + 0x130, 4))[0]
            if par:
                uc.mem_write(par + 0x138, struct.pack("<IIII", 0, 0, 0, 0))
        return 0
    emu.stub_py(WINDOW_CREATE, 9, create, True, "Window::create")
    emu.stub_py(WINDOW_CTORINIT, 0, ctor_init, False, "Window::ctorInit")
    _imports(emu)


def editor_oracle_stubs(emu):
    """for port/test/test_chareditor.py's oracle (raw root-handler input, no key routing): the bio entry's EditBox family
    over the stubs above, with malloc in this module's block; returns the block to map"""
    _ui_stubs(emu, Heap(ARENA_VA), ring=False)
    return [(BLOCK, bytes(BLOCK_SIZE)), (ARENA_VA, bytes(ARENA_SIZE))]


def editor_stub_words():
    """data-image offsets of the words only the original's Window::create stub writes (the EditBox vtable, which the game
    sets at startup in EditBox::ctor and the port never writes)"""
    return [(EDITBOX - DATA_VA, EDITBOX - DATA_VA + 4)]


# ================================================================================================ 1. the EditBox functions
def _eb_image(lib, rng):
    img = bytearray(base_image(lib))
    eo = objs(lib)["g_characterEditorEditBox"][0]
    img[eo:eo + 0x5DC] = rng.randbytes(0x5DC)
    struct.pack_into("<I", img, eo, EB_VTABLE)
    for nm in ("f_cfg_EditBox_4e43f0", "g_editBoxDefaults"):   # resetFields' sources, all distinct
        off, sz = objs(lib)[nm]
        img[off:off + sz] = rng.randbytes(sz)
    to = objs(lib)["g_textBuffer"][0]
    img[to:to + 0x1000] = bytes(0x1000)
    cap = rng.choice([0, 3, 8, 0x40, 0x40, 0x1F4])
    n = rng.choice([0, cap, rng.randrange(cap + 1), rng.randrange(cap + 1)]) if cap else 0
    txt = bytes(rng.choice(b"abcdefXYZ 019\x80\xe9") for _ in range(n))
    img[to:to + len(txt)] = txt
    if rng.random() < 0.3:   # non-zero bytes after the NUL (strncpy's padding and insertChar's end clear show)
        img[to + len(txt) + 1:to + cap + 1] = rng.randbytes(max(0, cap - len(txt)))
    struct.pack_into("<IIi", img, eo + 0x574, DATA_VA + to if rng.random() < 0.95 else 0, cap, 0)
    struct.pack_into("<I", img, eo + 0x5A4, rng.choice([0, n, rng.randrange(n + 1), rng.randrange(n + 1)]))
    src = to + 0x800
    stxt = bytes(rng.choice(b"Florence likes naps.\xe9 ") for _ in range(rng.choice([0, 1, 7, 0x3F, 0x40, 0x41, 0x200])))
    img[src:src + len(stxt) + 1] = stxt + b"\0"
    return img, DATA_VA + src


def eb_cases(lib, name, n, seed):
    rng = random.Random(f"{name}/{seed}")
    out = []
    for _ in range(n):
        img, src = _eb_image(lib, rng)
        eb = _eb(lib)
        if name == "EditBox::setText":
            args = [eb, rng.choice([src, src, 0])]
        elif name == "EditBox::resizeBuffer":
            cap = struct.unpack_from("<I", img, objs(lib)["g_characterEditorEditBox"][0] + 0x578)[0]
            args = [eb, rng.choice([cap, 0x40, 0x1F4, 3, 0, 9])]
        elif name == "EditBox::insertChar":
            args = [eb, rng.choice([rng.randrange(0x20, 0x7F), rng.randrange(0x20), rng.randrange(-128, 0), 0x100, 0x1FF,
                                    0x7FFFFFFF, -0x80000000, 0x41, 0x20]), rng.choice([0, 0x60, 0x65, 0x69, 0x41])]
        elif name == "EditBox::onKey":
            k = rng.choice(list(EB_KEYS) * 4 + [0x41, 0x0D, 0x09, 0x70, 0x2F, 0x07, 0x2E + 0x10000, 0x25 + 0x20000])
            args = [eb, k]
        elif name == "EditBox::create":
            args = [eb, 0x1E0, 0x70, 0x12C, 0xC8, rng.choice([ROOT_WINDOW, ROOT_WINDOW, 0]), rng.getrandbits(32)]
        else:
            args = [eb]
        out.append((bytes(img), args))
    return out


def eb_emu(lib, name, img, args):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    heap = Heap(_va(lib, "g_textBuffer", 0x400))
    _ui_stubs(emu, heap, ring=False)
    r = emu.run(EB_ENTRY[name], args[1:], img, ecx=args[0], budget=2_000_000)
    return r, heap


def eb_port(lib, name, img, args, heap_blocks):
    """the port, then its arena block (when the text pointer moved there) copied to where the original's malloc put it"""
    eo = objs(lib)["g_characterEditorEditBox"][0]
    with lib.state() as st:
        if st.lib.has("audio_exe.stubRequests"):  # S4.2b: the audio requests stubbed, as this file's oracle stubs them
            st.call("audio_exe.stubRequests", [1])
        st.set_data_base(DATA_VA)
        st.load_image(img)
        try:
            eax, code = st.call(name, [a & 0xFFFFFFFF for a in args]).eax, 0
        except L.SgError as e:
            eax, code = None, e.code
        out = bytearray(st.image())
        ptr = struct.unpack_from("<I", out, eo + 0x574)[0]
        if heap_blocks and ptr and not (DATA_VA <= ptr < DATA_VA + len(out)):
            dst, n = heap_blocks[-1]
            st.call("chareditor.peek", [ptr, dst, n])
            out = bytearray(st.image())
            struct.pack_into("<I", out, eo + 0x574, dst)
        return eax, bytes(out), code


def eb_compare(lib, name, case) -> str:
    img, args = case
    r, heap = eb_emu(lib, name, img, args)
    pe, pimg, code = eb_port(lib, name, img, args, heap.blocks)
    if r.err:
        return "" if pe is None and code in (L.SG_E_FAULT,) else f"original error {r.err}, port {pe} {code}"
    if pe is None:
        return f"port error {code}, original eax {r.eax:#x}"
    ret = name not in ("EditBox::setText", "EditBox::resizeBuffer")
    if ret and (pe & 0xFFFFFFFF) != r.eax:
        return f"eax port {pe & 0xFFFFFFFF:#x} original {r.eax:#x}"
    return TC.first_diff(pimg, r.image, lib)


EB_FUNCS = list(EB_ENTRY)
EB_N = {n: 300 for n in EB_FUNCS}


@need_exe
@emu_oracle
@pytest.mark.parametrize("name", EB_FUNCS)
def test_editbox_equals_emulated_original(portlib, name):
    bad = [(k, d) for k, c in enumerate(eb_cases(portlib, name, EB_N[name], 1)) if (d := eb_compare(portlib, name, c))]
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


@need_exe
def test_editbox_up_down_need_the_line_wrap(portlib):
    """Up / Down (0x26 / 0x28) place the caret through the font's line wrap: MissingResource (U-1190)"""
    img, _src = _eb_image(portlib, random.Random(2))
    with portlib.state() as st:
        if st.lib.has("audio_exe.stubRequests"):  # S4.2b: the audio requests stubbed, as this file's oracle stubs them
            st.call("audio_exe.stubRequests", [1])
        st.set_data_base(DATA_VA)
        st.load_image(bytes(img))
        for k in (0x26, 0x28):
            with pytest.raises(L.SgError) as e:
                st.call("EditBox::onKey", [_eb(portlib), k])
            assert e.value.code == L.SG_E_NO_RESOURCE


# ================================================================================================ 2. the bio entry
class Msg:
    """a scripted window message delivered through the original's key routing: kind 'kd' / 'ch' / 'ku', value"""

    def __init__(self, kind, v):
        self.kind, self.v = kind, v

    def __repr__(self):
        return f"{self.kind}:{self.v:#x}"


def _bio_image(lib, rng, character, golfer):
    img = bytearray(TC._editor_image(lib, rng, character, golfer, False))
    put32(lib, img, "g_graphicsDevice", TC.FAKE_DISPLAY)   # always a display: the cursor reads place the presses
    o = objs(lib)
    ro = o["g_rootWindow"][0]
    # the routing's reads: the root is visible, its own focus owner, no parent, no focus ring yet, no callbacks
    struct.pack_into("<I", img, ro, ROOT_VTABLE)
    img[ro + 0xA0] |= 1
    struct.pack_into("<I", img, ro + 0xB0, ROOT_WINDOW)
    for at in (0x130, 0x138, 0x13C, 0x140, 0x144, 0x148, 0x25C, 0x260, 0x26C, 0x270, 0x38, 0x3C):
        struct.pack_into("<I", img, ro + at, 0)
    struct.pack_into("<I", img, ro + 0x9C, struct.unpack_from("<I", img, ro + 0x9C)[0] & ~0x200000)
    img[ro + 0xA0] &= ~8 & 0xFF
    eo = o["g_characterEditorEditBox"][0]
    for at in (0x9C, 0xA0, 0x25C, 0x260, 0x26C, 0x270, 0x38, 0x3C, 0x140, 0x138, 0x13C):
        struct.pack_into("<I", img, eo + at, 0)
    for nm, v in (("g_modalTop", 0), ("g_focusOwnerState", 0), ("g_dragTarget", ROOT_WINDOW), ("g_ui_83ab88", 0)):
        put32(lib, img, nm, v)
    put32(lib, img, "f_stateptr_4e42ec", _va(lib, "g_codeBuf"))
    put32(lib, img, "g_audio5aa774", 0)   # no deadline: the handlers' deadline tail reads no clock
    for va in (0x0083AB00, 0x0083AAFC):   # uiKeyEvent's two optional callbacks (U-1192): none
        struct.pack_into("<I", img, va - DATA_VA, 0)
    return img


def _route_mask(lib):
    """byte ranges of the data image the routing's ui bookkeeping writes in the original and the port does not model"""
    o = objs(lib)
    m = []
    for nm in ("g_curWidget", "g_ui_83ab88", "g_codeBuf", "f_stateptr_4e42ec", "g_focusOwnerState", "g_focusedItem"):
        off, sz = o[nm]
        m.append((off, off + sz))
    eo = o["g_characterEditorEditBox"][0]
    for at in (0, 0x130):            # the stub-written vtable and parent words
        m.append((eo + at, eo + at + 4))
    return m


def _masked(img, mask):
    b = bytearray(img)
    for lo, hi in mask:
        b[lo:hi] = bytes(hi - lo)
    return bytes(b)


def predict(items, in_bio):
    """the commands the trace would give for the scripted items (in_bio[k]: item k is dispatched while the bio's EditBox
    has the focus): a consumed key message becomes 12 / 13, the rest the root handler it reaches"""
    from port.test import cmdemu as CE
    out, last_kd = [], 0
    for it, bio in zip(items, in_bio):
        if isinstance(it, CE.Raw):
            out.append([it])
        elif it.kind == "kd":   # needKey (no character): the root's onKey(0, vk) through Window::key, else its onChar
            last_kd = it.v
            out.append([CE.Raw(12, it.v)] if bio and it.v in EB_KEYS else
                       [CE.Raw(7, 0, it.v)] if VK_CHAR.get(it.v, 0) == 0 else [CE.Raw(8, it.v)])
        elif it.kind == "ch":
            c = it.v if it.v < 0x80 else it.v - 0x100
            out.append([CE.Raw(13, it.v)] if bio and not (0 <= c < 0x20) else [CE.Raw(7, c, last_kd)])
        else:
            out.append([CE.Raw(9)])
    return out


def item_batches(cmds_per_item):
    """the port's ScriptInput split (up to and including the first press or key) mapped back to item groups"""
    groups, cur = [], []
    for k, cmds in enumerate(cmds_per_item):
        cur.append(k)
        if any(c.handler in (1, 4, 7, 8, 10, 11) for c in cmds):
            groups.append(cur)
            cur = []
    if cur:
        groups.append(cur)
    return groups


def bio_emu(lib, img, args, items, groups, cursors, rule, salt):
    """the original editor (test_chareditor's oracle setup) with the bio's EditBox family and the key routing for real"""
    from unicorn import x86_const as X

    from port.test import cmdemu as CE
    emu = CE.Emu()
    for a in TC.DROPPED:
        emu.drop(a)
    for a in TC.NORET:
        emu.stub(a, 0, 0, False)
    emu.stub(TC.TEXT_PUT, 1, ("width",) + tuple(rule), True)
    seq = TC.CursorSeq(cursors)
    emu.stub(TC.CURSOR_STUB, 2, ("cursor", seq), True, "display cursor")
    emu.stub(TC.SLOT_STUB, 0, 0, False, "display slot")
    emu.stub(TC.SLOT9C_STUB, 3, 0, True, "display slot 0x9c")
    emu.stub(TC.SLOT38_STUB, 1, 0, True, "display slot 0x38")
    emu.stub(TC.COMP_STUB, 3, 0, True, "compositor slot")
    for a, n in TC.EH_VEC.items():
        emu.stub(a, n, 0, True)
    if salt is not None:
        for nm, (a, n) in TC.SCRIPTED.items():
            emu.stub(a, n, (lambda nm: lambda av, ecx: TC.script_value(nm, av, salt))(nm), False, nm)
    heap = Heap(ARENA_VA)
    _ui_stubs(emu, heap)
    st = {"g": 0, "off": 0}
    drains = dict(CE.DRAIN_CALLERS)
    drains.update(TC.RELEASE_DRAINS)
    root_input = _va(lib, "g_rootInput")

    def pump(uc, a, ecx):
        """pumpMessages (cdecl, no arguments): a clear click word clears the button word; then the dispatch (the scripted
        group of messages, through a trampoline run in place of doMouse's jgld pumps); then the click word is
        cleared. Its callers are told apart by its return address ([esp] on entry), the drains returning at once."""
        esp = uc.reg_read(X.UC_X86_REG_ESP)
        ret = struct.unpack("<I", uc.mem_read(esp, 4))[0]
        if struct.unpack("<I", uc.mem_read(root_input + 0x1C, 4))[0] == 0:
            uc.mem_write(root_input + 0x10, bytes(4))
        if ret in drains:
            uc.mem_write(root_input + 0x1C, bytes(4))
            return 0
        if st["g"] >= len(groups):
            ptr = struct.unpack("<I", uc.mem_read(EDITBOX + 0x574, 4))[0]
            if ARENA_VA <= ptr < ARENA_VA + ARENA_SIZE:   # stopped inside the bio: its live text
                st["live"] = (ptr, bytes(uc.mem_read(ptr, 0x1F5)))
            raise CE.NeedInput("modal wait with no input left")
        base = TRAMP2 + st["off"]
        code = bytearray()
        for k in groups[st["g"]]:
            it = items[k]
            code += bytes([0xC7, 0x05]) + struct.pack("<II", MARK, k + 1)          # mov dword [MARK], k + 1
            if isinstance(it, CE.Raw):
                code += bytes([0xB9]) + bytes(4) + bytes([0x68]) + struct.pack("<I", it.a1 & 0xFFFFFFFF)
                code += bytes([0x68]) + struct.pack("<I", it.a0 & 0xFFFFFFFF)
                tgt = CE.ROOT_HANDLERS[it.handler]
                at = base + len(code)
                code += bytes([0xE8]) + struct.pack("<i", tgt - (at + 5))
                if it.handler in (8, 9):
                    code += bytes([0x83, 0xC4, 0x04])                                 # add esp, 4
            else:
                if it.kind == "ch":
                    code += bytes([0x68]) + struct.pack("<I", 0xCCCCCC00 | (it.v & 0xFF))
                    tgt = UI_KEY_EVENT148
                else:
                    code += bytes([0x68]) + struct.pack("<I", 1 if it.kind == "kd" else 0)
                    code += bytes([0x68]) + struct.pack("<I", it.v)
                    tgt = UI_KEY_EVENT
                at = base + len(code)
                code += bytes([0xE8]) + struct.pack("<i", tgt - (at + 5))
        code += bytes([0xC7, 0x05]) + struct.pack("<II", MARK, 0)
        code += bytes([0xC7, 0x05]) + struct.pack("<II", root_input + 0x1C, 0)        # the click word cleared
        code += bytes([0x68]) + struct.pack("<I", ret) + bytes([0x31, 0xC0, 0xC3])   # push ret; xor eax, eax; ret
        st["off"] += (len(code) + 0x3F) & ~0x3F       # a fresh address per dispatch (Unicorn caches translations)
        assert st["off"] < HEAP_OFF - 0x1000, "too many dispatches for the trampoline block"
        uc.mem_write(base, bytes(code))
        uc.mem_write(esp, struct.pack("<I", base))
        st["g"] += 1
        return 0
    emu.stub_py(PUMP_MESSAGES, 0, pump, False, "pumpMessages")
    r = emu.run(TC.ENTRY["showCharacterEditor"], args, img,
                blocks=[TC._display_block(), (BLOCK, bytes(BLOCK_SIZE)), (ARENA_VA, bytes(ARENA_SIZE))],
                watch=(MARK, G_DIRTY_A, G_DIRTY_B, G_CHAR))
    r.reads = seq.reads
    r.groups_done = st["g"]
    r.live = st.get("live")
    return r


def derive(log, items):
    """the commands the trace recorder + parse_trace would make from the original's run: per item, the root handlers whose
    writes follow its marker"""
    from port.test import cmdemu as CE
    seen = {k: [] for k in range(len(items))}
    cur = None
    for e in log:
        if e[0] != "write":
            continue
        eip, addr, val = int(e[1], 16), int(e[2], 16), int(e[4], 16)
        if addr == MARK:
            cur = val - 1 if val else None
            continue
        if cur is None:
            continue
        for h, (lo, hi) in ((7, ROOT_ON_KEY), (8, ROOT_ON_CHAR), (9, ROOT_ON_CHAR0)):
            if lo <= eip < hi:
                seen[cur].append((h, addr, val))
    out = []
    for k, it in enumerate(items):
        if isinstance(it, CE.Raw):
            out.append([it])
            continue
        hs = {h for h, _a, _v in seen[k]}
        if it.kind == "kd":
            if 7 in hs:
                out.append([CE.Raw(7, 0, next(v for h, a, v in seen[k] if h == 7 and a == G_DIRTY_A))])
            else:
                out.append([CE.Raw(8, it.v)] if 8 in hs else [CE.Raw(12, it.v)])
        elif it.kind == "ch":
            if 7 in hs:
                code = next(v for h, a, v in seen[k] if h == 7 and a == G_DIRTY_A)
                c = it.v if it.v < 0x80 else it.v - 0x100
                out.append([CE.Raw(7, c, code)])
            else:
                out.append([CE.Raw(13, it.v)])
        else:
            out.append([CE.Raw(9)] if 9 in hs else [])
    return out


def _sig(cmds):
    return [[(c.handler, c.a0 & 0xFFFFFFFF, c.a1 & 0xFFFFFFFF) for c in cs] for cs in cmds]


def bio_compare(lib, case) -> str:
    img, args, items, in_bio, cursors, rule, salt = case
    pred = predict(items, in_bio)
    groups = item_batches(pred)
    r = bio_emu(lib, img, args, items, groups, cursors, rule, salt)
    der = derive(r.log, items)
    done = {k for g in groups[:r.groups_done] for k in g}
    if _sig([d for k, d in enumerate(der) if k in done]) != _sig([p for k, p in enumerate(pred) if k in done]):
        k = next(k for k in sorted(done) if _sig([der[k]]) != _sig([pred[k]]))
        return f"routing prediction wrong at item {k} {items[k]}: original {_sig([der[k]])} predicted {_sig([pred[k]])}"
    cmds = [c for cs in pred for c in cs]
    pe, pimg, code, used, reads, calls = TC.port_run(lib, "showCharacterEditor", img, args, cmds, cursors, rule, salt,
                                                     peek=(r.live[0], 0x1F5) if r.live else None)
    if r.err.startswith("NeedInput"):
        if code != L.SG_E_NO_RESOURCE:
            return f"original waits for input, port {code} {pe}"
        if r.live and TC.port_run.last_peek != r.live[1]:
            return "the live bio text differs (the run stopped inside the bio)"
    elif r.err:
        return "" if pe is None and code in (L.SG_E_FAULT, L.SG_E_DIVIDE) else f"original error {r.err}, port {pe} {code}"
    elif pe is None:
        return f"port error {code}, original eax {r.eax:#x}"
    elif (pe & 0xFFFFFFFF) != r.eax:
        return f"eax port {pe & 0xFFFFFFFF:#x} original {r.eax:#x}"
    used_orig = sum(len(pred[k]) for k in done)
    if used != used_orig:
        return f"input consumed port {used} original {used_orig}"
    if reads != r.reads:
        return f"cursor reads port {reads} original {r.reads}"
    if salt is not None:
        orig = [(TC.fnv1a(e[0]), *[v & 0xFFFFFFFF for v in e[1]]) for e in r.log if e[0] in TC.SCRIPTED]
        port = [(c[0], *c[1:1 + c[7]]) for c in calls]
        if orig != port:
            return f"peer calls differ ({len(port)} / {len(orig)})"
    mask = _route_mask(lib)
    return TC.first_diff(_masked(pimg, mask), _masked(r.image, mask), lib)


# ---- cases
def _bio_keys(rng, n):
    keys = []
    for _ in range(n):
        k = rng.random()
        if k < 0.55:
            keys.append(Msg("ch", rng.choice(b"abcdefghijklmnopqrstuvwxyz ABCXYZ019.,!'\x80\xe9\xff")))
        elif k < 0.85:
            vk = rng.choice(EB_KEYS)
            keys += [Msg("kd", vk), Msg("ku", vk)]
            if vk == 0x08 and rng.random() < 0.7:   # TranslateMessage's WM_CHAR 8 (root onKey)
                keys.insert(len(keys) - 1, Msg("ch", 0x08))
        elif k < 0.95:
            vk = rng.choice([0x41, 0x70, 0x10, 0x5A])    # keys the EditBox ignores (root onChar)
            keys += [Msg("kd", vk), Msg("ku", vk)]
        else:
            keys.append(Msg("ch", rng.choice([0x09, 0x1B, 0x01])))   # control characters: the root's onKey
    return keys


def bio_cases(lib, n, seed):
    from port.test import cmdemu as CE
    rng = random.Random(f"bio/{seed}")
    out = []
    nt = objs(lib)["g_golferTypes"][1] // 0x230
    for _ in range(n):
        rule = (rng.choice([5, 6, 7, 8, 9]), rng.choice([0, 1, 3, 7]))
        salt = rng.getrandbits(32)
        character = rng.choice([0, rng.randrange(nt), rng.randrange(nt)])
        golfer = rng.choice([rng.randrange(0x98), 0x99])
        img = _bio_image(lib, rng, character, golfer)
        to = objs(lib)["g_golferTypes"][0] + 0x230 * character
        blen = rng.choice([0, 5, 40, 0x1F0, 0x1F4, 0x1F5, 0x1FF])
        img[to + 0x30:to + 0x230] = (TC._text(rng, 4) * 200)[:blen].ljust(0x200, b"\0") if blen else bytes(0x200)
        spots = TC._spots(lib, img)
        items, in_bio, icur = [], [], []

        def add(its, bio=False, pt=None):
            items.extend(its)
            in_bio.extend([bio] * len(its))
            icur.extend([pt] * len(its))
        for _entry in range(rng.choice([1, 1, 2, 3])):
            if rng.random() < 0.3:   # a key before the bio (the root's handlers; no focus ring yet)
                add([Msg("kd", 0x41), Msg("ch", 0x61), Msg("ku", 0x41)], pt=spots[rng.randrange(len(spots))])
            add(CE.click(*spots[20])[:2], pt=spots[20])   # down, the press (the bio starts in this pass)
            keys = _bio_keys(rng, rng.choice([0, 1, 4, 12, 30]))
            if rng.random() < 0.1:
                keys += [Msg("ch", 0x41)] * 520           # past the 500-character capacity (the beep arm)
            add([CE.Raw(3)] + keys, True)                  # onUnpoint arrives in the bio loop
            if rng.random() < 0.6:
                add([Msg("ch", 0x0D)], True)               # Enter
            else:
                add(CE.click(rng.randrange(800), rng.randrange(0x102, 600))[:2], True)   # a press
                add([CE.Raw(3)])
        if rng.random() < 0.9:
            add(CE.click(*spots[19]), pt=spots[19])
        cursors = []
        for g in item_batches(predict(items, in_bio)):
            if not in_bio[g[0]]:
                pts = [icur[k] for k in g if icur[k] is not None]   # the pass's press (the group's last pointed item)
                cursors.append(pts[-1] if pts else cursors[-1] if cursors else spots[20])
        out.append((bytes(img), [character, golfer], items, in_bio, cursors or [(0, 0)], rule, salt))
    return out


BIO_N = 120


@need_exe
@emu_oracle
def test_bio_equals_emulated_original(portlib):
    bad = [(k, d) for k, c in enumerate(bio_cases(portlib, BIO_N, 1)) if (d := bio_compare(portlib, c))]
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


# ================================================================================================ golden digests
GOLDEN_N = {"bio": 60, **{n: 120 for n in EB_FUNCS}}
_CACHE = {}


def _golden_cases(lib, name):
    if name not in _CACHE:
        _CACHE[name] = bio_cases(lib, GOLDEN_N[name], "golden") if name == "bio" else eb_cases(lib, name, GOLDEN_N[name], "golden")
    return _CACHE[name]


def _port_digest(lib, name, case) -> str:
    import hashlib
    if name == "bio":
        img, args, items, in_bio, cursors, rule, salt = case
        cmds = [c for cs in predict(items, in_bio) for c in cs]
        pe, pimg, code, used, reads, calls = TC.port_run(lib, "showCharacterEditor", img, args, cmds, cursors, rule, salt)
        tail = json.dumps([code, used, reads, [list(c) for c in calls]])
        return digest(pe, pimg) + ":" + hashlib.sha256(tail.encode()).hexdigest()[:12]
    img, args = case
    with lib.state() as st:
        if st.lib.has("audio_exe.stubRequests"):  # S4.2b: the audio requests stubbed, as this file's oracle stubs them
            st.call("audio_exe.stubRequests", [1])
        st.set_data_base(DATA_VA)
        st.load_image(img)
        try:
            eax, code = st.call(name, [a & 0xFFFFFFFF for a in args]).eax, 0
        except L.SgError as e:
            eax, code = None, e.code
        out = bytearray(st.image())
        eo = objs(lib)["g_characterEditorEditBox"][0]
        ptr = struct.unpack_from("<I", out, eo + 0x574)[0]
        tail = b""
        if ptr and not (DATA_VA <= ptr < DATA_VA + len(out)):
            buf = (C.c_uint8 * 0x210)()
            st.map(0x35000000, buf)
            st.call("chareditor.peek", [ptr, 0x35000000, 0x200])
            tail = bytes(buf)
            struct.pack_into("<I", out, eo + 0x574, 0)
        return digest(eax, bytes(out) + tail) + f":{code}"


def _golden_load():
    return json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}


GOLDEN_NAMES = ["bio"] + EB_FUNCS


@need_exe
@pytest.mark.parametrize("name", GOLDEN_NAMES)
def test_golden_digests(portlib, name):
    got = [_port_digest(portlib, name, c) for c in _golden_cases(portlib, name)]
    if os.environ.get("SG_EMU_ORACLE") == "update":
        cmp = (lambda c: bio_compare(portlib, c)) if name == "bio" else (lambda c: eb_compare(portlib, name, c))
        bad = [(k, d) for k, c in enumerate(_golden_cases(portlib, name)) if (d := cmp(c))]
        assert not bad, f"oracle not GREEN on the golden cases: {bad[:4]}"
        g = _golden_load()
        g[name] = got
        GOLDEN.write_text(json.dumps(g, indent=0, sort_keys=True))
    want = _golden_load().get(name)
    assert want is not None, "no golden digests: run with SG_EMU_ORACLE=update once the oracle is GREEN"
    bad = [k for k, (a, b) in enumerate(zip(got, want)) if a != b]
    assert len(got) == len(want) and not bad, f"{len(bad)} cases differ, first {bad[:5]}"


# ================================================================================================ negative controls
NEGCTL_RED = [("EditBox::insertChar", 1, "EditBox::insertChar"), ("EditBox::onKey", 1, "EditBox::onKey"),
              ("EditBox::onKey", 2, "EditBox::onKey"), ("EditBox::setText", 1, "EditBox::setText"),
              ("EditBox::resizeBuffer", 1, "EditBox::resizeBuffer"), ("EditBox::resetFields", 1, "EditBox::resetFields"),
              ("EditBox::releaseState", 1, "EditBox::releaseState"), ("EditBox::create", 1, "EditBox::create"),
              ("showCharacterEditor", 10, "bio"), ("showCharacterEditor", 11, "bio"),
              ("EditBox::insertChar", 1, "bio"), ("EditBox::onKey", 1, "bio"), ("EditBox::setText", 1, "bio")]


@need_exe
@pytest.mark.parametrize("func,mode,cases", NEGCTL_RED)
def test_negctl_reads_red(portlib, func, mode, cases):
    want = _golden_load().get(cases)
    assert want is not None
    portlib.negctl(func, mode)
    try:
        got = [_port_digest(portlib, cases, c) for c in _golden_cases(portlib, cases)]
        red = [k for k, (a, b) in enumerate(zip(got, want)) if a != b]
        if red and os.environ.get("SG_EMU_ORACLE") == "1":
            c = _golden_cases(portlib, cases)[red[0]]
            d = bio_compare(portlib, c) if cases == "bio" else eb_compare(portlib, cases, c)
            assert d, "red against the digest, not the original"
    finally:
        portlib.negctl_reset()
    assert red, f"{func} negctl {mode} is blind on the {cases} golden cases"


# ================================================================================================ the pinned scenario
# cov_r_bio (re/tools/scenario_s1_5.py, pinned in re/analysis/data/scenario_refs.json): mood5.sve, a golfer's info panel, '?'
# opens the editor on that golfer (a non-member), two bio entries (typed text with Back / Left / Home / Delete / Right /
# End, ended by Enter; text appended, ended by a press), Exit. The boot run from process start (port/test/boot.py) compares
# every frame record's 203 sim hashes with the capture (log/p2/p24/cov_r_bio/c_a): the bio is in g_golferTypes (sim).
BOOT_NEGCTL = [("showCharacterEditor", 11, "the EditBox's text is not copied into the bio"),
               ("showCharacterEditor", 10, "the bio loop waits for 0x0a instead of Enter"),
               ("EditBox::insertChar", 1, "the caret is not advanced"),
               ("EditBox::onKey", 1, "Back removes the character at the caret"),
               ("tick.frame", 8, "parse_trace drops the commands of the keys the focus child took")]


def _need_bio_trace():
    from port.test import boot as B
    if not (B.TRACES / "cov_r_bio.trace").exists() or B.expected_records("cov_r_bio")[1] is None:
        pytest.skip("no trace or capture for cov_r_bio (port/corpus/traces, log/p2/p24)")
    return B


@need_exe
def test_cov_r_bio_boot_passes(portlib):
    B = _need_bio_trace()
    r = B.run("cov_r_bio", portlib)
    assert r.status == "PASS", f"{r.status} {r.records}/{r.expected} {r.detail}"


@need_exe
@pytest.mark.parametrize("fname,mode,what", BOOT_NEGCTL)
def test_cov_r_bio_negctl_reads_red(portlib, fname, mode, what):
    B = _need_bio_trace()
    portlib.negctl(fname, mode)
    try:
        r = B.run("cov_r_bio", portlib)
    finally:
        portlib.negctl_reset()
    assert r.status not in ("PASS", "KNOWN"), f"{fname} mode {mode} ({what}) read GREEN on cov_r_bio"
