"""Phase 3.7b commands module (port/src/sim/commands*.cpp, portsim/sim/commands.h, log/p3/p37b_notes.txt).

The 62 UI-module straddlers have no usable corpus vectors (re/port/vectors_manifest.tsv: every key excluded), so the
evidence is the EMULATOR ORACLE of port/test/cmdemu.py: the ORIGINAL function runs under Unicorn from the user's
golf_clean.exe over constructed GameState images, with the callees the port drops (drawing, display, window tree) stubbed
and the modal pumps fed by the same scripted raw commands the port's InputSource gets; the whole data image, eax and the
consumed input must equal the port's. That oracle is opt-in (SG_EMU_ORACLE=1: it runs original code); the default suite
checks GOLDEN digests of the emulated original's results for fixed cases, computed from the PORT (regenerate with
SG_EMU_ORACLE=update after the oracle is GREEN), plus the negative controls against those digests.
"""
from __future__ import annotations

import hashlib
import json
import os
import pathlib
import random
import struct

import pytest

from port.test import lib as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "port" / "test" / "golden_commands.json"

pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")


# ------------------------------------------------------------------------------------------------ helpers
_BASE = {}


def objs(lib) -> dict[str, tuple[int, int]]:
    if "objs" not in _BASE:
        _BASE["objs"] = {n: (off, sz) for n, off, sz in lib.objects()}
    return _BASE["objs"]


def base_image(lib) -> bytes:
    """the file's initialised .data (the loader's image)"""
    if "img" not in _BASE:
        assert lib.exe_load() == ""
        with lib.state() as st:
            st.fill_from_exe(whole_image=True)
            _BASE["img"] = st.image()
    return _BASE["img"]


def put(lib, img: bytearray, name: str, data: bytes, at: int = 0):
    off, size = objs(lib)[name]
    assert at + len(data) <= size, (name, at, len(data), size)
    img[off + at:off + at + len(data)] = data


def put32(lib, img, name, v, at=0):
    put(lib, img, name, struct.pack("<I", v & 0xFFFFFFFF), at)


def rand_obj(lib, img, name, rng, at=0, n=None):
    off, size = objs(lib)[name]
    n = size - at if n is None else n
    img[off + at:off + at + n] = rng.randbytes(n)


INPUT_VA = 0x30000000
DATA_VA = 0x004C1000


def port_call(lib, name, args, image, inputs=()):
    """(eax or None on a port error, final image, error code or 0, inputs consumed); `inputs` are cmdemu.Raw commands
    served to the function's modal waits through the context's InputSource"""
    import ctypes as C
    with lib.state() as st:
        st.set_data_base(DATA_VA)   # the original's data address: pointer-holding constants (string tables) stay valid
        st.load_image(image)
        words = [w for c in inputs for w in c.args()]
        if inputs:
            buf = (C.c_uint32 * len(words))(*words)
            st.map(INPUT_VA, buf, readonly=True)
            st.call("commands.setInput", [INPUT_VA, len(inputs)])
        try:
            eax, code = st.call(name, list(args)).eax, 0
        except L.SgError as e:
            eax, code = None, e.code
        used = st.call("commands.inputConsumed", []).eax
        st.call("commands.clearInput", [])
        return eax, st.image(), code, used


def digest(eax, image: bytes) -> str:
    h = hashlib.sha256()
    h.update(b"none" if eax is None else struct.pack("<I", eax & 0xFFFFFFFF))
    h.update(image)
    return h.hexdigest()[:24]


def first_diff(a: bytes, b: bytes, lib) -> str:
    if a == b:
        return ""
    i = next(i for i in range(len(a)) if a[i] != b[i])
    for n, (off, sz) in objs(lib).items():
        if off <= i < off + sz:
            return f"{n}+{i - off:#x}: port {a[i]:#04x} original {b[i]:#04x}"
    return f"byte {i:#x}"


# ------------------------------------------------------------------------------------------------ root handlers
def _rh_cases(lib, n=400, seed=5):
    from port.test import cmdemu as CE
    rng = random.Random(seed)
    base = base_image(lib)
    out = []
    for _ in range(n):
        img = bytearray(base)
        put32(lib, img, "g_screenWidth", rng.choice([640, 800, 1024, 1280]))
        put32(lib, img, "g_audio5aa774", rng.choice([0, 0, 5000, 0xFFFFFFFF]))
        for nm in ("g_rootInput", "gDirtyFlagA", "gDirtyFlagB", "g_char83821c", "g_key838208", "g_ui83ab9c"):
            rand_obj(lib, img, nm, rng)
        h = rng.randrange(1, 12)
        c = CE.Raw(h, rng.randrange(-50, 900),
                   rng.choice([rng.randrange(-50, 700), 0x1000D, 0x60, 0x61, 0x65, 0x69, 0x6A, 0x5F]),
                   rng.getrandbits(16), rng.getrandbits(16), rng.choice([0, 4000, 5000, 6000, 0xFFFFFFF0]))
        out.append((bytes(img), c))
    return out


def _rh_port(lib, img, c):
    with lib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(img)
        eax = st.call("commands.rootHandler", c.args()).eax
        return eax, st.image()


@need_exe
@emu_oracle
def test_root_handlers_equal_emulated_original(portlib):
    """every root-window handler, run by the oracle's dispatch trampoline (the original code), equals the port's
    apply_command(RootHandler): the whole data image"""
    from port.test import cmdemu as CE
    emu = CE.Emu()
    bad = []
    for k, (img, c) in enumerate(_rh_cases(portlib)):
        r = emu.run(CE.FRAME_TICK, [], img, inputs=[c])
        assert not r.err, (k, c, r.err)
        _, pimg = _rh_port(portlib, img, c)
        d = first_diff(pimg, r.image, portlib)
        if d:
            bad.append((k, c, d))
    assert not bad, bad[:5]


@need_exe
@pytest.mark.parametrize("mode", [1, 2, 3, 4])
def test_root_handler_negctl_reads_red(portlib, mode):
    if not os.environ.get("SG_EMU_ORACLE"):
        pytest.skip("judged against the emulated original (SG_EMU_ORACLE=1)")
    from port.test import cmdemu as CE
    emu = CE.Emu()
    portlib.negctl("commands.rootHandler", mode)
    try:
        red = 0
        for img, c in _rh_cases(portlib, n=200):
            r = emu.run(CE.FRAME_TICK, [], img, inputs=[c])
            _, pimg = _rh_port(portlib, img, c)
            red += pimg != r.image
    finally:
        portlib.negctl_reset()
    assert red > 0


# ------------------------------------------------------------------------------------------------ panel click handlers
# dropped callees of the original (address, stack arguments, pops them, eax): drawing, display, window tree, audio
DROPPED = [
    (0x00473E60, 4, True, 0),    # Image_draw (thiscall)
    (0x0040D320, 4, False, 0),   # drawWindow (its thought-bubble composeGolferSprite is tested apart)
    (0x00480C80, 1, True, 0),    # Window::refreshFull (thiscall)
    (0x00483C70, 0, False, 0),   # redrawAll
    (0x00483C30, 0, False, 0),   # pumpInput483c30 (no dispatch in the port model)
    (0x00483D30, 0, False, 0),   # enterModal483d30
    (0x004481B0, 5, False, 0),   # playSound (audio backend; no simulation effect while unregistered)
    (0x00483BD0, 0, False, 0),   # pumpInput483bd0 (pumpLight's jgld pump: no dispatch in the port model)
]
# peers the port calls by name that are not ported yet: the oracle stubs them too and the port must report
# SG_E_NO_RESOURCE on exactly those paths (documented gaps)
GAP_PEERS = {0x004385D0: ("showCharacterEditor", 2), 0x0045B2C0: ("textInputBox", 4)}

ENTRY = {"clickGolferPanel": 0x00436060, "clickEmployeeView": 0x00435680, "handleEmployeeAction": 0x00436C00,
         "clickBuildTool433d30": 0x00433D30, "pickBuilding": 0x00434140, "clickAmenityPanel": 0x00434AC0,
         "clickSlotPanel": 0x00433040, "hitGolferPanel435f00": 0x00435F00, "hitEmployeeSlot435570": 0x00435570,
         "hitEmployee436b00": 0x00436B00, "hitTestSlot432f90": 0x00432F90, "pickAmenity434980": 0x00434980,
         "hitBuildTool433c60": 0x00433C60, "hitBuildingTool": 0x004340A0,
         "pumpAndRedraw": 0x0045C0C0, "panelListSelect": 0x0040D890}
# hotspot centres of the panels (the clicks are drawn around them)
SPOTS = [(0x111, 0x22f), (0x139, 0x207), (0x139, 0x224), (0x139, 0x244), (0x19e, 0x1f9), (0x1ed, 0x1f9), (0x23c, 0x1f9),
         (0x28b, 0x1f9), (0x2da, 0x1f9), (0x11e, 0x1ea), (0xe7, 0x21c), (0x11d, 0x1ec), (0x100, 0x1fe), (0xe8, 0x21a),
         (0x14c, 0x250), (0x304, 0x250), (0x136 + 60, 0x1f2 + 10), (0x136 + 300, 0x1f2 + 50), (0xed, 0x20d),
         (0x105, 0x242), (0x10d, 500), (0x12e, 0x233), (0x14d, 0x20e), (0x16b, 0x233), (0x1d6, 0x23f), (0x24f, 0x23f),
         (0x28c, 0x23f), (0x2ca, 0x23f), (0xf8, 0x237), (0x213, 0x244), (0x10e, 0x1f3), (0x160, 0x223), (0x1d3, 0x223),
         (0x24b, 0x223), (0x2cb, 0x223), (0x10b, 0x231), (0xec, 0x20e), (0x115, 0x237), (0x17f, 0x213), (0x1cf, 0x213),
         (0x21f, 0x1eb), (0x200, 0x1ff)]


def _panel_image(lib, rng) -> bytes:
    img = bytearray(base_image(lib))

    def p32(n, v, at=0):
        put32(lib, img, n, v, at)

    p32("g_screenWidth", 800)
    for n in ("g_ui_561254", "g_buildPanelSlot", "g_buildPanelPick", "g_ui_59ca54"):
        p32(n, rng.choice([0, 0, 1]))
    p32("g_panelListMode", rng.randrange(5))
    p32("g_flags", rng.getrandbits(32) & rng.choice([0xFFFFFFFF, 0x00406002, 0x04200000, 0x2000]))
    p32("g_holesPlusOne", rng.choice([0, 1, 2, 5, 12, 19]))
    p32("g_playerGolfer", rng.choice([-1, -1, 3]))
    p32("g_proChallenger", rng.choice([-1, 2]))
    p32("g_msgState53df54", rng.choice([0, 0, 5, -1]))
    p32("g_employeePanelMode", rng.choice([-1, 0, 1, 7]))
    p32("g_ui_5aa558", rng.choice([0, 2, 4]))
    p32("g_cash_div100", rng.randrange(-1000, 100000))
    put(lib, img, "g_year", struct.pack("<h", rng.randrange(-5, 300)))
    p32("g_buildingUnlockLevel", rng.randrange(0, 16))
    for k in range(20):
        p32("g_objectTypeLevel", rng.choice([0, 0, 1, 2, 3]), 4 * k)
    for k in range(5):
        p32("g_ui_5aa6b8", rng.choice([0, 0x10, 3, 8, 20]), 4 * k)
    for k in range(7):
        p32("g_panelState", rng.choice([-1, 0, 1, 2, 5]), 4 * k)
    for k in range(3):
        p32("g_ui_56d1a8", rng.choice([0, 1, -1]), 4 * k)
    for k in range(4):
        p32("f_w_4c2848", rng.choice([-1, 0, 3, 6]), 4 * k)
    for k in range(5):
        p32("f_w_4c2e00", rng.choice([-1, 0, 4, -0x16]), 4 * k)
    for k in range(152):
        p32("g_employeeViewList", rng.choice([-1, rng.randrange(152)]), 4 * k)
    for k in range(64):
        p32("g_employeeDetailList", rng.randrange(64), 4 * k)
    rand_obj(lib, img, "g_golfers", rng)
    rand_obj(lib, img, "g_walkers", rng)
    rand_obj(lib, img, "g_financeYears", rng)
    rand_obj(lib, img, "g_rng", rng)
    for k in range(24):   # the palette sprites' height field (hitTestSlot432f90)
        p32("g_sprites570cb8", rng.choice([0x30, 0x40, 0x50, rng.randrange(0x100)]), 0x1c + 0xb0 * k)
    rand_obj(lib, img, "g_rootInput", rng)
    p32("g_redrawPending", rng.choice([0, 1]))
    p32("gDirtyFlagA", rng.choice([0, 0, 0x41]))
    p32("gDirtyFlagB", rng.choice([0, 0, 0x61]))
    p32("g_flag_83afcc", rng.choice([0, 0, 0, 1]))
    p32("g_audio5aa774", 0)
    return bytes(img)


def _panel_inputs(rng):
    from port.test import cmdemu as CE
    out = []
    for _ in range(rng.choice([0, 1, 1, 2, 3])):
        k = rng.random()
        if k < 0.6:
            out += CE.click(rng.randrange(800), rng.randrange(600), rng.getrandbits(16), rng.getrandbits(16))
        elif k < 0.8:
            out += CE.key(rng.choice([0x1B, 0x41, 0x50, 0x0D]), rng.choice([0, 0x61, 0x1B]))
        else:
            out.append(CE.Raw(rng.choice([2, 3, 9]), rng.randrange(800), rng.randrange(600)))
    return out


def _cells_building():
    """the nine hitBuildingTool cells (centres from its running offset)"""
    out, sx, sy = [], 0, 0
    for i in range(9):
        out.append((0x17F + sx, (0x426 + sy) // 2))
        if i == 4:
            sy += 0x50
            sx -= 0x118
        else:
            sx += 0x50
    return out


SPOT_SETS = {
    "golfer": SPOTS[0:11],
    "view": [(0x11d, 0x1ec), (0x100, 0x1fe), (0xe8, 0x21a), (0x14c, 0x250), (0x304, 0x250)]
            + [(0x136 + 121 * c + 60, 0x1f2 + 21 * r + 10) for c in range(4) for r in range(4)],
    "slot": [(0xed, 0x20d), (0x105, 0x242)],
    "amenity": SPOTS[20:30],
    "tool": SPOTS[30:36],
    "building": [(0xec, 0x20e), (0x115, 0x237)] + _cells_building(),
    "list": [(0, 0)],   # panelListSelect takes (item, direct), not a point
}


def _employee_spots(lib):
    off, _ = objs(lib)["f_tab_point16_4c7af0"]
    img = base_image(lib)
    out = [(0x11e, 0x1ec), (0x100, 0x1fe)]
    for i in range(16):   # the walk runs past the 8-entry model object to the x == -1 entry (16 entries)
        px, py = struct.unpack_from("<hh", img, off + 4 * i)
        out.append((px + 8, py + 0x1e) if i in (2, 3) else (px + (0x10 if i < 8 else 0x20), py + 0x10))
    return out


FAMILY = {"hitGolferPanel435f00": "golfer", "clickGolferPanel": "golfer", "hitEmployeeSlot435570": "view",
          "clickEmployeeView": "view", "hitEmployee436b00": "employee", "handleEmployeeAction": "employee",
          "hitTestSlot432f90": "slot", "clickSlotPanel": "slot", "pickAmenity434980": "amenity",
          "clickAmenityPanel": "amenity", "hitBuildTool433c60": "tool", "clickBuildTool433d30": "tool",
          "hitBuildingTool": "building", "pickBuilding": "building", "pumpAndRedraw": "golfer",
          "panelListSelect": "list"}


def _slot_spots(lib, img):
    """hitTestSlot432f90's slot boxes over this image's sprite heights (the walk ends at the 0xff byte)"""
    o = objs(lib)
    tx, _ = o["f_tab_point16_4c79b4"]
    tb, _ = o["f_tab_bytes_4c79a0"]
    hs, _ = o["g_sprites570cb8"]
    out = []
    for i in range(20):
        xmin, yy = struct.unpack_from("<hh", img, tx + 4 * i)
        top = struct.unpack_from("<i", img, hs + 0x1c + 0xb0 * i)[0] - 0xa + yy
        out.append((xmin + 0x1f, top - 0x16))
        if img[tb + 1 + i] == 0xFF:
            break
    return out


def panel_cases(lib, name, n, seed):
    rng = random.Random(f"{name}/{seed}")
    fam = FAMILY[name]
    out = []
    for _ in range(n):
        img = bytearray(_panel_image(lib, rng))
        # the forwarding flags mostly clear, so each handler's own body runs
        for flag in ("g_ui_561254", "g_buildPanelSlot", "g_buildPanelPick"):
            put32(lib, img, flag, 1 if rng.random() < 0.12 else 0)
        if fam == "amenity":
            put32(lib, img, "f_w_4c2848", rng.choice([4, 2, 1, 0x10, 0x13, 0, 3, -1]), 12)
            put32(lib, img, "g_panelState", rng.choice([-1, -1, 0, 3]))
        if fam == "employee":
            put32(lib, img, "g_ui_59ca54", rng.choice([0, 1, 1]))
            put32(lib, img, "g_employeePanelMode", rng.choice([-1, 0, 1, 7, 40]))
        if fam == "view":
            put32(lib, img, "g_msgState53df54", rng.choice([0, 0, 5, -1]))
            for g in range(152):   # the record word +8 is -1 for some golfers
                if rng.random() < 0.4:
                    put32(lib, img, "g_golfers", -1, 0x100 * g + 8)
        if fam == "golfer":
            put32(lib, img, "g_panelListMode", rng.choice([3, 3, 0, 4]))
            put32(lib, img, "g_playerGolfer", rng.choice([-1, -1, -1, 3]))
            put32(lib, img, "g_flags", rng.choice([0, 0x2000, 0x400000, 0x402000, 0x4200000, rng.getrandbits(32)]))
            put32(lib, img, "g_holesPlusOne", rng.choice([0, 1, 2, 5]))
            put32(lib, img, "g_proChallenger", rng.choice([-1, 2, 2]))
            put32(lib, img, "g_golferTypes", rng.choice([0, 0, 5, 0x13]), 0x2c)
        if fam == "building":
            put32(lib, img, "g_holesPlusOne", rng.choice([0, 1, 2, 3, 5, 12, 19]))
        spots = _employee_spots(lib) if fam == "employee" else _slot_spots(lib, img) + SPOT_SETS["slot"]             if fam == "slot" else SPOT_SETS[fam]
        x, y = rng.choice(spots)
        j = rng.choice([0, 0, 3, 8, 15, 30])
        x, y = x + rng.randrange(-j, j + 1), y + rng.randrange(-j, j + 1)
        if rng.random() < 0.08:
            x, y = rng.randrange(-20, 820), rng.randrange(-20, 620)
        if fam == "list":   # panelListSelect (item, direct): the list, the mode and the tool tables it reads
            put32(lib, img, "g_panelListMode", rng.choice([0, 1, 1, 2, 2, 3, 3, 4, 5]))
            count = rng.choice([0, 1, 5, 20, 0x99])
            put32(lib, img, "g_panelListCount", count)
            for k in range(0x264 // 4):
                put32(lib, img, "g_panelListIds", rng.choice([rng.randrange(0x98), rng.randrange(-2, 20)]), 4 * k)
            for k in range(8):
                put32(lib, img, "g_themeSlots59bfc4", rng.randrange(0x17), 4 * k)
            toff, tsize = objs(lib)["g_tileTypeAttrs"]
            for k in range(tsize // 0x30):
                img[toff + 0x30 * k + 0x26] = rng.choice([0xD, 0xD, 0, 3, 0xC])
            put32(lib, img, "g_difficulty", rng.choice([0, 2, 3, 4, 5, -1]))
            put32(lib, img, "g_amenityUnlockMask", rng.choice([0, 0, 1, 0x3FFF]))
            put32(lib, img, "g_panelSelectPending", rng.choice([-1, 0, 3]))
            for g in range(152):   # golfers whose record word +8 is -1
                if rng.random() < 0.3:
                    put32(lib, img, "g_golfers", -1, 0x100 * g + 8)
            item = rng.choice([rng.randrange(-1, 10), rng.randrange(0, 0x99), count, count - 1, 3, 7, 8])
            out.append((bytes(img), [item, rng.choice([0, 0, 1])], []))
            continue
        if name == "pumpAndRedraw":   # (limit 0): a wait that always has a script (one empty run waits forever)
            out.append((bytes(img), [0], _panel_inputs(rng) or _panel_inputs(rng)))
            continue
        out.append((bytes(img), [x, y], _panel_inputs(rng)))
    return out


def emu_panel(name, img, args, inputs):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    for a, n, pops, eax in DROPPED:
        emu.stub(a, n, eax, pops)
    for a, (nm, n) in GAP_PEERS.items():
        emu.stub(a, n, 0, False, nm)
    return emu.run(ENTRY[name], args, img, inputs=inputs)


def panel_compare(lib, name, img, args, inputs) -> str:
    """'' when the port equals the emulated original. A path where the original entered an unported peer must be a port
    SG_E_NO_RESOURCE (documented gap); both waiting for input past the script counts as equal."""
    r = emu_panel(name, img, args, inputs)
    pe, pimg, code, used = port_call(lib, name, args, img, inputs)
    gap = [nm for nm, _ in r.log if nm in ("showCharacterEditor", "textInputBox")]
    if gap:
        return "" if code == L.SG_E_NO_RESOURCE else f"gap {gap[0]}: port code {code}"
    if r.err.startswith("NeedInput"):
        return "" if code == L.SG_E_NO_RESOURCE else f"original waits for input, port {code} {pe}"
    if r.err:
        return "" if pe is None and code in (L.SG_E_FAULT, L.SG_E_DIVIDE) else f"original error {r.err}, port {pe} {code}"
    if pe is None:
        return f"port error {code}, original eax {r.eax:#x}"
    if pe != r.eax:
        return f"eax port {pe:#x} original {r.eax:#x}"
    if used != r.consumed:
        return f"input consumed port {used} original {r.consumed}"
    return first_diff(pimg, r.image, lib)


PANELS = ["hitGolferPanel435f00", "hitEmployeeSlot435570", "hitEmployee436b00", "hitTestSlot432f90", "pickAmenity434980",
          "hitBuildTool433c60", "hitBuildingTool", "handleEmployeeAction", "clickEmployeeView", "clickGolferPanel",
          "clickBuildTool433d30", "pickBuilding", "clickAmenityPanel", "clickSlotPanel", "pumpAndRedraw",
          "panelListSelect"]


@need_exe
@emu_oracle
@pytest.mark.parametrize("name", PANELS)
def test_panel_equals_emulated_original(portlib, name):
    bad = []
    for k, (img, args, inputs) in enumerate(panel_cases(portlib, name, 500, 1)):
        d = panel_compare(portlib, name, img, args, inputs)
        if d:
            bad.append((k, args, len(inputs), d))
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


# ------------------------------------------------------------------------------------------------ layout effects
LAYOUT_ENTRY = {"drawAmenitiesPanel": 0x00434CF0, "showInfoCursorPanel": 0x00433190, "drawEmployeeDetailPanel": 0x00436E50,
                "drawEmployeeViewPanel": 0x00435760, "drawGolferActionPanel": 0x004362F0, "tickMessage": 0x0040D6A0}
LAYOUT = list(LAYOUT_ENTRY)
# dropped callees of the panel-drawing functions (stubbed with their own convention, eax 0)
LAYOUT_DROPPED = [0x00473F60, 0x00473E60, 0x00473CB0, 0x004762D0, 0x004049D0, 0x00404B70, 0x004493D0, 0x00407C60,
                  0x0043D6F0, 0x004628D0, 0x00480B00, 0x00432620, 0x00461110, 0x004676E0, 0x0040D320, 0x004481B0,
                  0x00480C80]
# what the port deliberately does not reproduce (object, offset, length): drawing scratch and hover counters
LAYOUT_MASK = [("g_textBuffer", 0, None), ("g_panelState", 4, 4), ("g_ui_5aa554", 0, None), ("g_ui_5aa6b8", 0xC, 4),
               ("g_value587ea0", 0, None), ("g_infoPanelNumBuf", 0, None),
               ("g_panelNumberText", 0, None)]
HIRE = 0x0040AA80   # hireEmployee: phase 3.7a's; stubbed (a gap) while no module registers it


def _masked(lib, img: bytes) -> bytes:
    b = bytearray(img)
    o = objs(lib)
    for n, at, ln in LAYOUT_MASK:
        if n not in o:
            continue
        off, size = o[n]
        ln = size - at if ln is None else ln
        b[off + at:off + at + ln] = bytes(ln)
    return bytes(b)


def _layout_image(lib, rng, name) -> bytes:
    img = bytearray(_panel_image(lib, rng))

    def p32(n, v, at=0):
        put32(lib, img, n, v, at)

    p32("g_ui_561254", 1 if rng.random() < (0.7 if name in ("drawEmployeeDetailPanel", "drawGolferActionPanel") else 0.2) else 0)
    p32("g_buildPanelSlot", 1 if rng.random() < 0.6 else 0)
    p32("f_w_4c2848", rng.choice([1, 4, 0x13, 2, 0x10, 0, 3, -1]), 12)
    p32("g_amenityUnlockMask", rng.choice([0, 0x3FFF, rng.getrandbits(14), 1]))
    p32("g_employeePanelMode", rng.choice([-1, -1, 0, 1, 3, 7]))
    p32("g_ui_5aa558", rng.choice([0, 0, 2, 4, 6]))
    p32("g_ui_5aa6b8", rng.choice([0, 3, 7, 8, 10, 20]), 8)
    p32("g_ui_5aa6b8", rng.choice([0, 0, 0x10, 0x20, 0x60]), 0x10)
    p32("g_ui_59ca54", rng.choice([0, 1, 1]))
    p32("g_zoomScale", rng.choice([1, 2, 4, 4]))
    p32("g_lastAllocatedGolfer", rng.randrange(0x98))
    p32("g_panelListMode", rng.choice([0, 3, 4]))
    off, _ = objs(lib)["g_walkers"]
    for k in range(64):   # employee records: type byte +0x13 (negative = employee), flags +0x12, animation words +0x1c / +0x1e
        r = off + 0x4C * k
        img[r + 0x13] = rng.choice([0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0x00, 0x05]) & 0xFF
        img[r + 0x12] = rng.choice([0, 1, 8, 9])
        struct.pack_into("<hh", img, r + 0x1C, rng.randrange(-20, 60), rng.choice([0, 5, 0xB, 0xC, 0xC]))
    soff, _ = objs(lib)["g_sheetCols"]
    cols = [rng.choice([1, 2, 4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 0]) for _ in range(0x938 // 4)]
    struct.pack_into(f"<{len(cols)}i", img, soff, *cols)
    goff, _ = objs(lib)["g_golfers"]
    for g in range(152):
        img[goff + 0x100 * g + 0x21] = rng.choice([0, 0, 1, 5, 0x13, 0xFF])
        struct.pack_into("<h", img, goff + 0x100 * g + 0xA2, rng.choice([-1, -1, rng.randrange(152)]))
    # tickMessage
    p32("g_ui_5694a0", rng.choice([0, 0, 0, 2]), 4)
    p32("unk_005a9ccc", rng.choice([0, 1, 2, 3, 4]))
    put(lib, img, "g_ui_569498", bytes([rng.choice([0, 1, 1, 2])]))
    p32("g_messageTimer", rng.choice([0, 1, 2, 30]))
    p32("g_date", rng.randrange(0, 50000))
    p32("f_w_4c2e00", rng.choice([-1, -1, -0x16, -2]), 8)   # no golfer selected: the thought bubble is tested apart
    put(lib, img, "unk_005a6d40", b"msg\0")
    return bytes(img)


def layout_cases(lib, name, n, seed):
    rng = random.Random(f"{name}/{seed}")
    out = []
    for _ in range(n):
        img = _layout_image(lib, rng, name)
        spots = SPOT_SETS["amenity"] + _employee_spots(lib) + SPOT_SETS["view"]
        x, y = rng.choice(spots)
        j = rng.choice([0, 3, 8, 20])
        out.append((img, [x + rng.randrange(-j, j + 1), y + rng.randrange(-j, j + 1)] if name != "tickMessage" else [], []))
    return out


def emu_layout(lib, name, img, args):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    for a in LAYOUT_DROPPED:
        emu.drop(a)
    if not lib.has("hireEmployee"):
        emu.stub(HIRE, 0, 0, False, "hireEmployee")
    return emu.run(LAYOUT_ENTRY[name], args, img)


def layout_compare(lib, name, img, args, _inputs=()) -> str:
    r = emu_layout(lib, name, img, args)
    pe, pimg, code, used = port_call(lib, name, args, img)
    if any(n == "hireEmployee" for n, _ in r.log):
        return "" if code == L.SG_E_NO_RESOURCE else f"gap hireEmployee: port code {code}"
    if r.err:
        return "" if pe is None and code in (L.SG_E_FAULT, L.SG_E_DIVIDE) else f"original error {r.err}, port {pe} {code}"
    if pe is None:
        return f"port error {code}, original eax {r.eax:#x}"
    if name == "tickMessage" and pe != r.eax:
        return f"eax port {pe:#x} original {r.eax:#x}"
    return first_diff(_masked(lib, pimg), _masked(lib, r.image), lib)


@need_exe
@emu_oracle
@pytest.mark.parametrize("name", LAYOUT)
def test_layout_effect_equals_emulated_original(portlib, name):
    bad = []
    for k, (img, args, _) in enumerate(layout_cases(portlib, name, 200, 1)):
        d = layout_compare(portlib, name, img, args)
        if d:
            bad.append((k, args, d))
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


def test_draw_window_effect_composes_the_selected_golfer(portlib):
    """drawWindow's only simulation write is drawThoughtBubble's composeGolferSprite on f_w_4c2e00[2] (c3ei.cpp drawWindow
    0x0040d4bb, drawThoughtBubble 0x0045c264): tickMessage with a selected golfer equals composeGolferSprite(g) followed
    by tickMessage without one"""
    rng = random.Random(3)
    for _ in range(20):
        img = bytearray(_layout_image(portlib, rng, "tickMessage"))
        put32(portlib, img, "g_ui_5694a0", 0, 4)
        g = rng.randrange(0x98)
        put32(portlib, img, "f_w_4c2e00", g, 8)
        put(portlib, img, "g_golfers", struct.pack("<h", rng.randrange(20)), 0x100 * g + 0xB6)   # a valid golfer type
        a, aimg, _, _ = port_call(portlib, "tickMessage", [], bytes(img))
        _, cimg, _, _ = port_call(portlib, "composeGolferSprite", [g], bytes(img))
        with portlib.state() as st:
            st.load_image(cimg)
            put_back = bytearray(cimg)
        put32(portlib, put_back, "f_w_4c2e00", -2, 8)
        b, bimg, _, _ = port_call(portlib, "tickMessage", [], bytes(put_back))
        bimg = bytearray(bimg)
        put32(portlib, bimg, "f_w_4c2e00", g, 8)
        assert a is not None and a == b and aimg == bytes(bimg)


# ------------------------------------------------------------------------------------------------ modal screens
SCREEN_ENTRY = {"showBestScoresScreen": 0x00455A30, "showHistographScreen": 0x00455ED0, "showFinancialReport": 0x0044F6B0,
                "showPlayerCommentsReport": 0x004546B0, "showHireEmployeeDialog": 0x00459400,
                "selectDifficultyScreen": 0x0043A400, "pumpLight": 0x0045C150, "showLoadingScreen": 0x00406250,
                "drawScoreSummary": 0x00473470}
SCREENS = list(SCREEN_ENTRY)
SCREEN_ARGS = {"showLoadingScreen": [50]}
SCREEN_DROPPED = [0x004049D0, 0x00404B70, 0x00432620, 0x0043D6F0, 0x004628D0, 0x00473AB0, 0x00473AE0, 0x00473BF0,
                  0x00473C60, 0x00473DF0, 0x00473E60, 0x00473F60, 0x00474440, 0x00474AE0, 0x00474C40, 0x00474CB0,
                  0x00474DD0, 0x00475840, 0x00475B20, 0x00475C60, 0x004762D0, 0x00476310, 0x00476650, 0x00476700,
                  0x00476750, 0x00477C30, 0x00478B80, 0x00480B00, 0x00480C80, 0x00483D30, 0x00483C30, 0x00483C70,
                  0x00483BD0, 0x0045B0D0,   # ... drawMessageBuffer
                  0x00477250, 0x00475C90]   # Text_put (drawScoreSummary measures the selected name only to draw), Surface_fillRegion
EH_VEC = {0x004A55D2: 5, 0x004A5713: 4}   # the MSVC array constructor / destructor helpers (stdcall: they pop)
VIEW_TO_LOCAL = 0x0047AB50                # View_toLocal(this, &x, &y), ret 8: the cursor
FAKE_DISPLAY = 0x31000000                 # a display object whose every vtable slot is one stub (slot(this), ret)
# peers a screen calls whose own port is another child's: both sides record the calls instead of running them (the
# original's callee stubbed, the port through commands.recordPeer) and the call sequences are compared
SCREEN_RECORD = {"showPlayerCommentsReport": {"buildGolferComment": (0x00469B00, 4)}}
PEER_LOG_VA, PEER_LOG_CAP = 0x32000000, 64


def fnv1a(s: str) -> int:
    h = 2166136261
    for ch in s.encode():
        h = ((h ^ ch) * 16777619) & 0xFFFFFFFF
    return h


SCREEN_MASK = LAYOUT_MASK + [("g_ui_569628", 0, None), ("g_ui_821c50", 0, None)]   # g_textBuf compared: the port formats like the original


def _screen_image(lib, rng) -> bytes:
    img = bytearray(_panel_image(lib, rng))
    put(lib, img, "g_year", struct.pack("<h", rng.choice([-3, 0, 1, 5, 40, 99, 120])))
    rand_obj(lib, img, "g_holes", rng)
    hoff, _ = objs(lib)["g_holes"]
    for h in range(1, 19):   # the comment arguments (hole record +0x16c + 2 kind) in the golfer-index range: random words
        for k in range(0x40):  # outside it make 3.5's buildGolferComment fault where the original does not (U-0945)
            struct.pack_into("<H", img, hoff + 0x208 * h + 0x16C + 2 * k, rng.randrange(0x98))
    rand_obj(lib, img, "g_financeYears", rng)
    soff, _ = objs(lib)["g_sheetCols"]
    cols = [rng.choice([1, 2, 4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 0]) for _ in range(0x938 // 4)]
    struct.pack_into(f"<{len(cols)}i", img, soff, *cols)
    o = objs(lib)
    if "g_widget519cd4" in o:
        pass
    struct.pack_into("<I", img, 0x00519CD8 - 0x004C1000, 0)      # the histograph's surface slot call: none
    struct.pack_into("<I", img, 0x0083AD50 - 0x004C1000, FAKE_DISPLAY)
    return bytes(img)


def screen_cases(lib, name, n, seed):
    rng = random.Random(f"{name}/{seed}")
    out = []
    for _ in range(n):
        img = _screen_image(lib, rng)
        inputs = _panel_inputs(rng) or _panel_inputs(rng) or _panel_inputs(rng)
        cur = (rng.randrange(-10, 810), rng.choice([rng.randrange(-10, 610), 0xab, 0xbf, 0xd2, 0x106, 0x14d, 0x194, 0x1a8,
                                                   0x6b, 0xda, 0x16f, 0x1ef, 0x22d]))
        if name == "drawScoreSummary":   # the ten g_topScores rows, some empty (name byte 0)
            img = bytearray(img)
            toff, _ = objs(lib)["g_topScores"]
            for i in range(10):
                r = toff + 0x9C * i
                img[r:r + 0x9C] = bytes(0x9C)
                if rng.random() < 0.7:
                    img[r:r + 8] = bytes(rng.choice(b"ABCdefg") for _ in range(7)) + b"\0"
                    img[r + 0x40:r + 0x48] = b"textrow\0"
                    struct.pack_into("<iii", img, r + 0x80, rng.randrange(-50, 500), rng.randrange(-900, 9000),
                                     rng.randrange(-1000, 10 ** 7))
                    struct.pack_into("<h", img, r + 0x96, rng.randrange(-2, 6))
            img = bytes(img)
        if name == "selectDifficultyScreen" and rng.random() < 0.5:
            cur = rng.choice([(0x15c, 0x6b), (0x13f, 0xda), (0x140, 0x16f), (0x16b, 0x1ef), (0x2ff, 0x22d)])
        out.append((img, [], inputs, cur))
    return out


def emu_screen(name, img, inputs, cur):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    for a in SCREEN_DROPPED:
        if a in (0x00483D30, 0x00483C30, 0x00483C70, 0x00483BD0):   # tail-jumping helpers without arguments (no `ret` of their own)
            emu.stub(a, 0, 0, False)
        else:
            emu.drop(a)
    for a, n in EH_VEC.items():
        emu.stub(a, n, 0, True)

    emu.stub(VIEW_TO_LOCAL, 2, ("cursor", cur), True)
    for nm, (a, n) in SCREEN_RECORD.get(name, {}).items():
        emu.stub(a, n, 0, False, nm)
    emu.stub(FAKE_DISPLAY + 0x800, 0, 0, False, "display slot")   # slot(this): no stack argument
    vt = struct.pack("<I", FAKE_DISPLAY + 0x100) + bytes(0xFC) + struct.pack("<I", FAKE_DISPLAY + 0x800) * 0x100
    block = (FAKE_DISPLAY, vt.ljust(0x900, b"\xC3"))
    return emu.run(SCREEN_ENTRY[name], SCREEN_ARGS.get(name, []), img, inputs=inputs, blocks=[block])


def port_screen(lib, name, img, inputs, cur):
    import ctypes as C
    with lib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(img)
        words = [w for c in inputs for w in c.args()]
        if inputs:
            buf = (C.c_uint32 * len(words))(*words)
            st.map(INPUT_VA, buf, readonly=True)
        st.call("commands.setInput", [INPUT_VA, len(inputs)])
        st.call("commands.setCursor", [cur[0] & 0xFFFFFFFF, cur[1] & 0xFFFFFFFF])
        logbuf = (C.c_uint32 * (1 + 8 * PEER_LOG_CAP))()
        st.map(PEER_LOG_VA, logbuf)
        for nm in SCREEN_RECORD.get(name, {}):
            st.call("commands.recordPeer", [fnv1a(nm), PEER_LOG_VA, PEER_LOG_CAP])
        try:
            eax, code = st.call(name, SCREEN_ARGS.get(name, [])).eax, 0
        except L.SgError as e:
            eax, code = None, e.code
        used = st.call("commands.inputConsumed", []).eax
        st.call("commands.clearInput", [])
        st.call("commands.recordClear", [])
        n = min(logbuf[0], PEER_LOG_CAP)
        calls = [tuple(logbuf[1 + 8 * k: 1 + 8 * k + 8]) for k in range(n)]
        port_screen.calls = calls
        return eax, st.image(), code, used


def screen_compare(lib, name, img, inputs, cur) -> str:
    r = emu_screen(name, img, inputs, cur)
    pe, pimg, code, used = port_screen(lib, name, img, inputs, cur)
    if r.err.startswith("NeedInput"):
        return "" if code == L.SG_E_NO_RESOURCE else f"original waits for input, port {code} {pe}"
    if r.err:
        return "" if pe is None and code in (L.SG_E_FAULT, L.SG_E_DIVIDE) else f"original error {r.err}, port {pe} {code}"
    if pe is None:
        return f"port error {code}, original eax {r.eax:#x}"
    if name in ("showHireEmployeeDialog", "selectDifficultyScreen") and pe != r.eax:
        return f"eax port {pe:#x} original {r.eax:#x}"
    if used != r.consumed:
        return f"input consumed port {used} original {r.consumed}"
    rec = SCREEN_RECORD.get(name, {})
    if rec:
        orig = [(fnv1a(n), *[v & 0xFFFFFFFF for v in a]) for n, a in r.log if n in rec]
        port = [(c[0], *c[1:1 + c[7]]) for c in port_screen.calls]
        if orig != port:
            return f"peer calls differ: port {port[:3]} original {orig[:3]}"
    b = bytearray(pimg), bytearray(r.image)
    for img2 in b:
        for n, at, ln in SCREEN_MASK:
            if n in objs(lib):
                off, size = objs(lib)[n]
                ln2 = size - at if ln is None else ln
                img2[off + at:off + at + ln2] = bytes(ln2)
    return first_diff(bytes(b[0]), bytes(b[1]), lib)


@need_exe
@emu_oracle
@pytest.mark.parametrize("name", SCREENS)
def test_screen_equals_emulated_original(portlib, name):
    bad = []
    for k, (img, _, inputs, cur) in enumerate(screen_cases(portlib, name, 120, 1)):
        d = screen_compare(portlib, name, img, inputs, cur)
        if d:
            bad.append((k, len(inputs), cur, d))
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


# ------------------------------------------------------------------------------------------------ text widgets
# showPopup and textInputBox measure with Text_put (font metrics: a GDI service, U-0944). Both sides measure with the same
# scripted width rule: the original's Text_put is stubbed by cmdemu's ("width", base, mask) and the port gets the same
# rule through commands.setTextWidth (a TextMetrics provider). Everything the widths feed is judged.
WIDGET_ENTRY = {"showPopup": 0x0046D6E0, "textInputBox": 0x0045B2C0, "nearestMenuSpot": 0x004326A0,
                "showSystemMenu": 0x00432720, "showPreferencesMenu": 0x00432560}
# showSystemMenu's peers the port does not have: the oracle stubs them and the port must give SG_E_NO_RESOURCE there
WIDGET_GAP_PEERS = {0x00437910: ("promptAndSaveFile", 3)}
# peers recorded on both sides instead of run (their drawing text in g_textBuffer is masked by their own family)
WIDGET_RECORD = {"showSystemMenu": {"showBestScoresScreen": (0x00455A30, 0), "drawScoreSummary": (0x00473470, 0)}}
WIDGETS = list(WIDGET_ENTRY)
TEXT_PUT = 0x00477250                 # Text_put(this, text), ret 4
WIDGET_DROPPED = [0x004762D0, 0x00479830, 0x004798C0, 0x00479950, 0x0040CC00, 0x00480B00, 0x00473F60, 0x004049D0,
                  0x00404AD0, 0x00404BC0, 0x00473CB0, 0x00480C80, 0x00478AF0, 0x00478B80, 0x004481B0]
TITLE_VA = 0x004C1000 + 0x100        # any readable bytes: the title is only drawn


def _width(rule, s: bytes) -> int:
    return sum(rule[0] + (c & rule[1]) for c in s)


def _popup_text(rng):
    lines = []
    for _ in range(rng.choice([1, 2, 3, 3, 4, 5, 7])):
        k = rng.random()
        word = bytes(rng.choice(b"abcdefghijklmnopqrstuvwxyzABCDEFGH 0123456789") for _ in range(rng.randrange(0, 14)))
        lines.append(b" " + word if k < 0.6 else word)
    if rng.random() < 0.85 and not any(ln.startswith(b" ") for ln in lines):
        lines.append(b" option")
    if rng.random() < 0.04:   # an empty buffer: no line, no option (the uninitialised first-option y)
        return b""
    return b"\n".join(lines) + (b"\n" if rng.random() < 0.8 else b"")


def _popup_layout(text: bytes, x, y, rule):
    """the box and the option rows the original computes from the widths (for aiming the scripted clicks)"""
    parts = text.split(b"\n")
    widest = max(_width(rule, p) for p in parts)
    lines = text.count(b"\n")
    inner = (widest - 1) | 0xF
    w, h = inner + 0x31, (lines * 3 + 3) * 8
    half = int(w / 2)
    ly, first, fy = y + 0xC, -1, 0
    for i, p in enumerate(parts[:lines]):
        if p.startswith(b" ") and first == -1:
            first, fy = i, ly
        ly += (6 if first != -1 else 0) + 0x12
    count = lines - first if first != -1 else lines + 1
    return x - half, x + half - 0x30, fy, count, (x + half - 0x14, y + h - 0x14)


def widget_cases(lib, name, n, seed):
    from port.test import cmdemu as CE
    rng = random.Random(f"{name}/{seed}")
    out = []
    toff, tsize = objs(lib)["g_textBuffer"]
    for _ in range(n):
        img = bytearray(_screen_image(lib, rng))
        for nm in ("gDirtyFlagA", "gDirtyFlagB", "g_redrawPending"):
            put32(lib, img, nm, 0)
        rule = (rng.choice([5, 6, 7, 8, 9]), rng.choice([0, 1, 3, 7]))
        inputs = []
        if name in ("nearestMenuSpot", "showSystemMenu"):   # the menu-bar spots of f_tab_point16_4c7930 and around
            o, _ = objs(lib)["f_tab_point16_4c7930"]
            pts = [struct.unpack_from("<hh", img, o + 4 * i) for i in range(16)]
            pts = [p for p in pts[:next((i for i, p in enumerate(pts) if p[0] == -1), 16)]]
            k = rng.random() if name == "showSystemMenu" else 1.0
            px, py = pts[5] if k < 0.4 else pts[3] if k < 0.6 else rng.choice(pts)   # 5 System, 3 Information
            j = rng.choice([0, 4, 12, 30, 60])
            args = [px + rng.randrange(-j, j + 1), py + rng.randrange(-j, j + 1)]
            put32(lib, img, "g_panelListMode", rng.choice([0, 1, 2, 5, 3]))
            put32(lib, img, "g_rootInput", rng.choice([0, 1, 2, 2]), 0x10)
            put32(lib, img, "g_rootInput", 0 if rng.random() < 0.85 else 1, 0x1c)   # the click flag: the first pump clears the button
            text = b"x"
            spots = [(rng.randrange(150, 650), rng.randrange(100, 500)) for _ in range(4)]
            for _ in range(rng.choice([1, 2, 3])):   # popup answers (option k: k + 1 Downs, Enter), then rename keys
                downs = rng.choice([0, 5, 5, 5, 4] + list(range(14)))   # 5 Downs = answer 4 (rename), 0 = none (-1)
                inputs += [r for _ in range(downs) for r in CE.key(0x28)[::2]] + CE.key(0x0D)   # key down / up: an arrow sends no WM_CHAR
                for _ in range(rng.choice([0, 1, 1, 3])):
                    inputs += CE.key(0x41, rng.choice(b"ab z?*")) if rng.random() < 0.7 else CE.key(8, 8)
                if rng.random() < 0.5:
                    inputs += CE.key(0x0D, 0x0D) if rng.random() < 0.7 else CE.key(0x1B, 0x1B)
                if rng.random() < 0.2:
                    inputs += CE.click(*rng.choice(spots))
            cx, cy = (0, 0) if rng.random() < 0.8 else rng.choice(spots)
        elif name == "showPreferencesMenu":
            args, text = [], b"x"
            put32(lib, img, "g_preferences", rng.getrandbits(6))
            o = objs(lib)
            cstr = lambda va: bytes(img[va - DATA_VA:img.index(b"\0", va - DATA_VA)])
            pool, ptrs = o["f_strpool_4c7c30"][0] + DATA_VA, o["f_ptrtab_4c2ca8"][0]
            ptext = cstr(pool + 4) + b"".join(cstr(struct.unpack_from("<I", img, ptrs + 4 * k)[0]) + cstr(pool)
                                              for k in range(6))
            left, right, fy, count, close = _popup_layout(ptext, 0x190, 0xC8, rule)
            spots = [((left + right) // 2, fy + 0x18 * k + rng.randrange(0x18)) for k in range(max(count, 1))]
            spots += [close, close, (close[0] + rng.randrange(-0x16, 0x17), close[1] + rng.randrange(-0x16, 0x17))]
            for _ in range(rng.choice([1, 2, 4, 6])):
                inputs += CE.click(*rng.choice(spots)) if rng.random() < 0.8 else CE.key(0x1B)
            cx, cy = rng.choice(spots)
        elif name == "showPopup":
            text = _popup_text(rng)
            x, y = rng.randrange(150, 650), rng.choice([rng.randrange(40, 300), 0x64, 0x8C, 0xA0, 0x96, 0x65])
            flag4 = rng.choice([0, 1, 1])
            mask, disabled = rng.getrandbits(6), rng.choice([0, 0, 1, 2, 5, rng.getrandbits(6)])
            args = [x, y, mask, flag4, disabled]
            left, right, fy, count, close = _popup_layout(text, x, y, rule)
            spots = [((left + right) // 2, fy + 0x18 * k + rng.randrange(0x18)) for k in range(max(count, 1))]
            spots += [close, (close[0] + rng.randrange(-0x16, 0x17), close[1] + rng.randrange(-0x16, 0x17)),
                      ((left + right) // 2, fy - rng.randrange(1, 40)),                  # above the first option
                      ((left + right) // 2, fy + 0x18 * max(count, 0) + rng.randrange(0, 30))]   # below the last
            for _ in range(rng.choice([1, 2, 3, 4, 6])):
                k = rng.random()
                if k < 0.5:
                    inputs += CE.click(*rng.choice(spots))
                elif k < 0.6:
                    inputs += CE.click(rng.randrange(800), rng.randrange(600))
                else:
                    inputs += CE.key(rng.choice([0x0D, 0x20, 0x1B, 0x26, 0x28, 0x28, 0x26, 0x41]))
                    if rng.random() < 0.3:   # walk down to the last option and past it
                        inputs += [r for _ in range(count + 1) for r in CE.key(0x28)] + CE.key(0x26)
            above = ((left + right) // 2, fy - rng.randrange(1, 40))   # inside the band, above the first option
            cx, cy = rng.choice(spots + [above, (rng.randrange(800), rng.randrange(600))])
        else:
            max_len = rng.choice([2, 4, 8, 12, 20, 30])
            text = bytes(rng.choice(b"abcdefghijklmnopqrstuvwxyz ") for _ in range(rng.randrange(0, max_len + 4)))
            args = [TITLE_VA, rng.randrange(100, 400), rng.randrange(50, 400), max_len]
            for _ in range(rng.choice([1, 2, 4, 8, 12])):
                k = rng.random()
                if k < 0.45:
                    inputs += CE.key(0x41, rng.choice(b"abcxyzABC019 ~"))
                elif k < 0.6:
                    inputs += CE.key(8, 8)
                elif k < 0.8:
                    inputs += CE.key(rng.choice([0x25, 0x27, 0x23, 0x24, 0x2E]), 0)
                elif k < 0.9:
                    inputs += CE.key(rng.choice([0x0D, 0x1B]), rng.choice([0x0D, 0x1B]))
                else:
                    inputs += CE.click(rng.randrange(800), rng.randrange(600))
            cx, cy = 0, 0
        img[toff:toff + tsize] = bytes(tsize)
        img[toff:toff + len(text)] = text
        out.append((bytes(img), args, inputs, (cx, cy), rule))
    return out


def emu_widget(name, img, args, inputs, cur, rule, cover=None):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    for a in WIDGET_DROPPED:
        emu.drop(a)
    for a in (0x00483D30, 0x00483C30, 0x00483C70, 0x00483BD0):   # enterModal483d30, pumpInput483c30, redrawAll, pumpInput483bd0
        emu.stub(a, 0, 0, False)
    emu.stub(TEXT_PUT, 1, ("width",) + tuple(rule), True)
    emu.stub(VIEW_TO_LOCAL, 2, ("cursor", cur), True)
    for a, (nm, n) in WIDGET_GAP_PEERS.items():
        emu.stub(a, n, 0, False, nm)
    for nm, (a, n) in WIDGET_RECORD.get(name, {}).items():
        emu.stub(a, n, 0, False, nm)
    for a in SCREEN_DROPPED:   # showSystemMenu reaches the best-scores screen and the score summary
        if a not in WIDGET_DROPPED and a not in (TEXT_PUT, 0x00483D30, 0x00483C30, 0x00483C70, 0x00483BD0):
            emu.drop(a)
    for a in (0x00475C90, 0x00475840):   # drawScoreSummary's Surface_fillRegion / loadImage
        emu.drop(a)
    for a, n in EH_VEC.items():
        emu.stub(a, n, 0, True)
    return emu.run(WIDGET_ENTRY[name], args, img, inputs=inputs, cover=cover)


def port_widget(lib, name, img, args, inputs, cur, rule):
    import ctypes as C
    with lib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(img)
        words = [w for c in inputs for w in c.args()]
        if inputs:
            buf = (C.c_uint32 * len(words))(*words)
            st.map(INPUT_VA, buf, readonly=True)
        st.call("commands.setInput", [INPUT_VA, len(inputs)])
        st.call("commands.setCursor", [cur[0] & 0xFFFFFFFF, cur[1] & 0xFFFFFFFF])
        if rule is not None:
            st.call("commands.setTextWidth", [rule[0], rule[1]])
        logbuf = (C.c_uint32 * (1 + 8 * PEER_LOG_CAP))()
        st.map(PEER_LOG_VA, logbuf)
        for nm in WIDGET_RECORD.get(name, {}):
            st.call("commands.recordPeer", [fnv1a(nm), PEER_LOG_VA, PEER_LOG_CAP])
        try:
            eax, code = st.call(name, [a & 0xFFFFFFFF for a in args]).eax, 0
        except L.SgError as e:
            eax, code = None, e.code
        used = st.call("commands.inputConsumed", []).eax
        st.call("commands.clearInput", [])
        st.call("commands.clearTextWidth", [])
        st.call("commands.recordClear", [])
        n = min(logbuf[0], PEER_LOG_CAP)
        port_widget.calls = [tuple(logbuf[1 + 8 * k: 1 + 8 * k + 8]) for k in range(n)]
        return eax, st.image(), code, used


def widget_compare(lib, name, img, args, inputs, cur, rule) -> str:
    r = emu_widget(name, img, args, inputs, cur, rule)
    pe, pimg, code, used = port_widget(lib, name, img, args, inputs, cur, rule)
    gap = [nm for nm, _ in r.log if nm in ("promptAndSaveFile",)]
    if gap:
        return "" if code == L.SG_E_NO_RESOURCE else f"gap {gap[0]}: port code {code}"
    if r.err.startswith("NeedInput"):
        return "" if code == L.SG_E_NO_RESOURCE else f"original waits for input, port {code} {pe}"
    if r.err == "instruction budget":
        return "" if code == L.SG_E_NO_RETURN else f"original loops, port {code} {pe}"
    if r.err:
        return "" if pe is None and code in (L.SG_E_FAULT, L.SG_E_DIVIDE) else f"original error {r.err}, port {pe} {code}"
    if pe is None:
        return f"port error {code}, original eax {r.eax:#x}"
    if (pe & 0xFFFFFFFF) != r.eax:
        return f"eax port {pe & 0xFFFFFFFF:#x} original {r.eax:#x}"
    if used != r.consumed:
        return f"input consumed port {used} original {r.consumed}"
    rec = WIDGET_RECORD.get(name, {})
    if rec:
        orig = [(fnv1a(n), *[v & 0xFFFFFFFF for v in a]) for n, a in r.log if n in rec]
        port = [(c[0], *c[1:1 + c[7]]) for c in port_widget.calls]
        if orig != port:
            return f"peer calls differ: port {port[:3]} original {orig[:3]}"
    return first_diff(pimg, r.image, lib)


@need_exe
@emu_oracle
@pytest.mark.parametrize("name", WIDGETS)
def test_widget_equals_emulated_original(portlib, name):
    bad = []
    for k, (img, args, inputs, cur, rule) in enumerate(widget_cases(portlib, name, 300, 1)):
        d = widget_compare(portlib, name, img, args, inputs, cur, rule)
        if d:
            bad.append((k, args, len(inputs), d))
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


@need_exe
@pytest.mark.parametrize("name", ["showPopup", "textInputBox"])   # the menus write their text before the popup measures
def test_widget_without_text_metrics_is_a_missing_resource(portlib, name):
    """no TextMetrics attached: SG_E_NO_RESOURCE before any write (the first width is measured before the first store)"""
    for img, args, inputs, cur, _ in widget_cases(portlib, name, 10, "nometrics"):
        pe, pimg, code, used = port_widget(portlib, name, img, args, inputs, cur, None)
        assert (pe, code, used) == (None, L.SG_E_NO_RESOURCE, 0) and pimg == img


def test_isprint_reads_the_crt_ctype_table(portlib):
    """textInputBox's _isprint: the C-locale ctype word & 0x157 (printable 0x20..0x7e), from the data image"""
    from port.test import cmdemu as CE
    img = bytearray(base_image(portlib))
    put(portlib, img, "g_textBuffer", b"\0")
    off, _ = objs(portlib)["g_textBuffer"]
    for c, printable in ((0x20, True), (0x41, True), (0x7E, True), (0x1F, False), (0x7F, False), (0x09, False)):
        inputs = CE.key(0x41, c) + CE.key(0x0D, 0x0D)
        _, pimg, code, _ = port_widget(portlib, "textInputBox", bytes(img), [TITLE_VA, 0, 0, 8], inputs, (0, 0), (8, 0))
        assert code == 0 and (pimg[off] == c) == printable and (pimg[off] == 0) != printable, hex(c)


# ------------------------------------------------------------------------------------------------ golden digests
# Fixed cases per function (seed "golden"); the digests are the emulated ORIGINAL's results, pinned from the port once the
# oracle is GREEN on them (SG_EMU_ORACLE=update). The default suite recomputes them from the port.
GOLDEN_N = 60
# more cases where a control's path is rare in 60 (the fire / shot-style arms, the fourth wide column)
GOLDEN_N_FOR = {"handleEmployeeAction": 200, "clickGolferPanel": 200, "drawEmployeeViewPanel": 200, "showPopup": 200,
                "textInputBox": 200, "panelListSelect": 200}
NEGCTL_RED = [
    ("pumpAndRedraw", 1, "pumpAndRedraw", "the click flag does not keep the button word"),
    ("pumpAndRedraw", 2, "pumpAndRedraw", "the key words are not copied after the wait"),
    ("handleEmployeeAction", 1, "handleEmployeeAction", "firing costs 0x18"),
    ("clickEmployeeView", 1, "clickEmployeeView", "camera x >> 9"),
    ("clickGolferPanel", 1, "clickGolferPanel", "shot style 7 -> 4"),
    ("clickBuildTool433d30", 1, "clickBuildTool433d30", "a tool sets flag bit 2"),
    ("pickBuilding", 1, "pickBuilding", "uniqueness refusal at level 3"),
    ("clickAmenityPanel", 1, "clickAmenityPanel", "case 5 draws Random::range(5)"),
    ("clickSlotPanel", 1, "clickSlotPanel", "slot table one entry later"),
    ("clickSlotPanel", 2, "clickSlotPanel", "no Random::range draw"),
    ("hitGolferPanel435f00", 1, "hitGolferPanel435f00", "hotspot 10 returns 9"),
    ("hitEmployeeSlot435570", 1, "hitEmployeeSlot435570", "row height 20"),
    ("hitEmployee436b00", 1, "hitEmployee436b00", "wide entries return idx + 1"),
    ("hitTestSlot432f90", 1, "hitTestSlot432f90", "box width 0x3f"),
    ("pickAmenity434980", 1, "pickAmenity434980", "hotspot 9 returns 7"),
    ("hitBuildTool433c60", 1, "hitBuildTool433c60", "the -3 anchor returns -2"),
    ("hitBuildingTool", 1, "hitBuildingTool", "cell 8 returns 7"),
    ("drawAmenitiesPanel", 1, "drawAmenitiesPanel", "tool 2's row has 6 items"),
    ("drawEmployeeDetailPanel", 1, "drawEmployeeDetailPanel", "the walker animation word becomes 0xa"),
    ("drawEmployeeViewPanel", 1, "drawEmployeeViewPanel", "the wide layout's column limit 0x2af"),
    ("tickMessage", 1, "tickMessage", "the open counter counts up to 4"),
    ("showBestScoresScreen", 1, "showBestScoresScreen", "the mode byte is left 1"),
    ("showFinancialReport", 2, "showFinancialReport", "the year total + 1"),
    ("showHireEmployeeDialog", 1, "showHireEmployeeDialog", "the row mapping without + 1"),
    ("selectDifficultyScreen", 1, "selectDifficultyScreen", "the back button kept as 4"),
    ("showLoadingScreen", 3, "showLoadingScreen", "the picture drawn with Random::range(11)"),
    ("showPopup", 1, "showPopup", "a check-list click ORs the option bit"),
    ("showPopup", 2, "showPopup", "Up passes the first option"),
    ("textInputBox", 1, "textInputBox", "the width cut at width - 0x18"),
    ("textInputBox", 2, "textInputBox", "an inserted character is stored + 1"),
    ("panelListSelect", 1, "panelListSelect", "item 7 sets shot style 3"),
    ("panelListSelect", 2, "panelListSelect", "the difficulty refusal from difficulty 3"),
    ("nearestMenuSpot", 1, "nearestMenuSpot", "the distance limit 31"),
    ("showPreferencesMenu", 2, "showPreferencesMenu", "the result stored ^ 1"),
    ("showSystemMenu", 4, "showSystemMenu", "spot 4 returns 0x51"),
    ("drawScoreSummary", 3, "drawScoreSummary", "the multiplier formatted without + 1"),
]


def _family(name):
    """(cases(lib, name, n, seed) -> [(img, args, inputs[, cursor])], port(lib, name, case) -> (eax, img, code, used),
    compare(lib, name, case) -> str) of a function's test family"""
    if name in WIDGET_ENTRY:
        return (widget_cases, lambda lib, n, c: port_widget(lib, n, *c),
                lambda lib, n, c: widget_compare(lib, n, *c))
    if name in SCREEN_ENTRY:
        return (screen_cases, lambda lib, n, c: port_screen(lib, n, c[0], c[2], c[3]),
                lambda lib, n, c: screen_compare(lib, n, c[0], c[2], c[3]))
    if name in LAYOUT_ENTRY:
        return (layout_cases, lambda lib, n, c: port_call(lib, n, c[1], c[0]),
                lambda lib, n, c: layout_compare(lib, n, c[0], c[1]))
    return (panel_cases, lambda lib, n, c: port_call(lib, n, c[1], c[0], c[2]),
            lambda lib, n, c: panel_compare(lib, n, c[0], c[1], c[2]))


def _golden_cases(lib, name):
    key = ("golden", name)
    if key not in _BASE:
        _BASE[key] = _family(name)[0](lib, name, GOLDEN_N_FOR.get(name, GOLDEN_N), "golden")
    return _BASE[key]


def _golden_port(lib, name):
    port = _family(name)[1]
    out = []
    for c in _golden_cases(lib, name):
        pe, pimg, code, used = port(lib, name, c)
        out.append(digest(pe, pimg) + f":{code}:{used}")
    return out


def _golden_load():
    return json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}


GOLDEN_FUNCS = PANELS + LAYOUT + SCREENS + WIDGETS


@need_exe
@pytest.mark.parametrize("name", GOLDEN_FUNCS)
def test_golden_digests(portlib, name):
    """the port reproduces the emulated original's pinned results (eax, error, consumed input, whole data image)"""
    if os.environ.get("SG_EMU_ORACLE") == "update":
        cmp = _family(name)[2]
        bad = [k for k, c in enumerate(_golden_cases(portlib, name)) if cmp(portlib, name, c)]
        assert not bad, f"oracle RED on golden cases {bad[:5]}: not pinned"
        g = _golden_load()
        g[name] = _golden_port(portlib, name)
        GOLDEN.write_text(json.dumps(g, indent=0, sort_keys=True))
    g = _golden_load()
    if name not in g:
        pytest.skip(f"{name}: no pinned digests (SG_EMU_ORACLE=update)")
    got = _golden_port(portlib, name)
    bad = [k for k, (a, b) in enumerate(zip(got, g[name])) if a != b]
    assert not bad and len(got) == len(g[name]), f"{len(bad)} golden cases differ: {bad[:6]}"


@need_exe
@pytest.mark.parametrize("negname,mode,judge,what", NEGCTL_RED)
def test_negctl_reads_red(portlib, negname, mode, judge, what):
    """every wrong body changes at least one pinned digest (and, with the oracle, reads RED against the original)"""
    g = _golden_load()
    if judge not in g:
        pytest.skip(f"{judge}: no pinned digests")
    portlib.negctl(negname, mode)
    try:
        got = _golden_port(portlib, judge)
        if os.environ.get("SG_EMU_ORACLE") == "1":
            cmp = _family(judge)[2]
            red = sum(bool(cmp(portlib, judge, c)) for c in _golden_cases(portlib, judge))
            assert red > 0, f"{negname} mode {mode} ({what}) GREEN against the original"
    finally:
        portlib.negctl_reset()
    assert got != g[judge], f"{negname} mode {mode} ({what}) is blind on the golden cases"


def test_every_command_function_has_a_negctl(portlib):
    mine = {n for n, f in portlib.functions().items() if f.get("module") == "commands" and not n.startswith("commands.")}
    covered = {n for n, *_ in NEGCTL_RED} | {"flushRedraw"} | SHARED_NEGCTL
    assert mine <= covered | set(BLIND_OR_STATE), sorted(mine - covered - set(BLIND_OR_STATE))


# functions that share another function's negative-control variable (the mode is read in a shared body)
SHARED_NEGCTL = {"showInfoCursorPanel", "drawGolferActionPanel", "showHistographScreen", "showPlayerCommentsReport",
                 "pumpLight"}
# functions whose negative control is judged by a state test instead (name -> test)
BLIND_OR_STATE = {"flushRedraw": "test_flush_redraw_state"}


def test_flush_redraw_state(portlib):
    """flushRedraw: a pending redraw clears both key words; a key word left sets the pending flag and returns 1"""
    for pending, a, b, ret, after in ((1, 0x41, 0x61, 0, (0, 0, 0)), (0, 0x41, 0, 1, (1, 0x41, 0)),
                                     (0, 0, 0x61, 1, (1, 0, 0x61)), (0, 0, 0, 0, (0, 0, 0))):
        img = bytearray(base_image(portlib))
        put32(portlib, img, "g_redrawPending", pending)
        put32(portlib, img, "gDirtyFlagA", a)
        put32(portlib, img, "gDirtyFlagB", b)
        pe, pimg, code, _ = port_call(portlib, "flushRedraw", [], bytes(img))
        o = objs(portlib)
        got = tuple(struct.unpack_from("<I", pimg, o[n][0])[0] for n in ("g_redrawPending", "gDirtyFlagA", "gDirtyFlagB"))
        assert (pe, got) == (ret, after)


# ------------------------------------------------------------------------------------------------ semantic commands
SEMANTIC = {"clickGolferPanel": (0x100, "hitGolferPanel435f00", "g_ui_561254"),
            "handleEmployeeAction": (0x101, "hitEmployee436b00", None),
            "clickEmployeeView": (0x102, "hitEmployeeSlot435570", "g_ui_561254"),
            "clickBuildTool433d30": (0x103, "hitBuildTool433c60", None),
            "pickBuilding": (0x104, "hitBuildingTool", "g_buildPanelPick"),
            "clickAmenityPanel": (0x105, "pickAmenity434980", None),
            "clickSlotPanel": (0x106, "hitTestSlot432f90", "g_buildPanelSlot")}


@need_exe
@pytest.mark.parametrize("name", list(SEMANTIC))
def test_semantic_command_equals_the_click(portlib, name):
    """apply_command(<kind>, the hit test's result) does exactly what the click handler does at that point (the forwarding
    flag clear): the same eax, consumed input and data image"""
    kind, hit, fwd = SEMANTIC[name]
    n = 0
    for img, args, inputs in panel_cases(portlib, name, 80, "semantic"):
        img = bytearray(img)
        if fwd:
            put32(portlib, img, fwd, 0)
        img = bytes(img)
        h, _, _, _ = port_call(portlib, hit, args, img)
        a = port_call(portlib, name, args, img, inputs)
        b = port_call(portlib, "commands.semantic", [kind, h & 0xFFFFFFFF], img, inputs)
        assert (a[0], a[2], a[3]) == (b[0], b[2], b[3]) and a[1] == b[1], (name, args)
        n += 1
    assert n == 80


@need_exe
def test_root_handler_golden_digests(portlib):
    """the 11 root handlers on 60 fixed states: pinned from the port after the oracle was GREEN on them; every
    commands.rootHandler negative control changes a digest"""
    from port.test import cmdemu as CE
    cases = _rh_cases(portlib, n=60, seed=11)

    def digests():
        return [digest(*_rh_port(portlib, img, c)) for img, c in cases]
    if os.environ.get("SG_EMU_ORACLE") == "update":
        emu = CE.Emu()
        for img, c in cases:
            assert emu.run(CE.FRAME_TICK, [], img, inputs=[c]).image == _rh_port(portlib, img, c)[1]
        g = _golden_load()
        g["commands.rootHandler"] = digests()
        GOLDEN.write_text(json.dumps(g, indent=0, sort_keys=True))
    g = _golden_load()
    if "commands.rootHandler" not in g:
        pytest.skip("no pinned digests (SG_EMU_ORACLE=update)")
    assert digests() == g["commands.rootHandler"]
    for mode in (1, 2, 3, 4):
        portlib.negctl("commands.rootHandler", mode)
        try:
            assert digests() != g["commands.rootHandler"], f"rootHandler mode {mode} blind"
        finally:
            portlib.negctl_reset()
