"""Phase 3.8 stream 38b: courseActionMenu (0x0046f550) and tierPrice (0x0046f1d0), port/src/sim/saveload.cpp.

tierPrice has corpus vectors (27, full coverage): the vector runner judges it. courseActionMenu is a modal screen with no
vectors: the evidence is the emulator oracle (opt-in, SG_EMU_ORACLE=1) in the manner of test_browser.py: the ORIGINAL runs under
Unicorn with its drawing callees dropped, the pumps fed from a scripted input (one batch per Blocking pump), the pointer read
from a script (two reads per frame: the hover test and the nearest-site search), the file layer and the sound / message /
text-input callees stood in and logged (the port's recorded). The port must equal eax, the whole data image, the recorded
call sequence and the files written. Default suite: pinned digests of those results (port/test/golden_38b.json) and every
negative control against them. The segment that reaches it (cov_a_keys/01, back button) is run by log/p3/p37a_cmdsegs.py.
"""
from __future__ import annotations

import ctypes as C
import hashlib
import os
import pathlib
import random
import struct
import tempfile

import pytest

from port.test import cmdemu as CE
from port.test import emu38b as E
from port.test import lib as L
from port.test import test_browser as TB
from port.test import test_saveload as TS

ROOT = pathlib.Path(__file__).resolve().parents[2]
INSTALL = ROOT / "original"
pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
need_saves = pytest.mark.skipif(not (INSTALL / "Saved Games" / "mood5.sve").exists(), reason="the install's saves are absent")
emu_oracle = pytest.mark.skipif(not E.have_oracle(), reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn")

ENTRY = 0x0046F550
# the menu's own drawing callees (the browser's are in TB.BROWSER_DROPS), dropped by their `ret N`
MENU_DROPS = [0x0040CC00, 0x0040CEF0, 0x00475C90, 0x00432620, 0x00482FD0]
CACHE_DTOR = 0x00483010          # Cache483010::dtor: no stack argument, no `ret` of its own (cmdemu's tail helpers)


# ------------------------------------------------------------------------------------------------ tierPrice (vectors)
@pytest.mark.port
def test_tier_price_vectors(vrunner):
    r = vrunner.run_function("tierPrice")
    assert r.verdict == "GREEN" and r.run >= 20, r.summary()


@pytest.mark.parametrize("mode,what", [(1, "an out-of-range tier costs 600"), (2, "tier 15 costs 9000")])
def test_tier_price_negctl_reads_red(portlib, vrunner, mode, what):
    assert portlib.negctl("tierPrice", mode) == 0
    try:
        r = vrunner.run_function("tierPrice")
    finally:
        portlib.negctl_reset()
    assert r.verdict == "RED" and r.failed > 0, f"mode {mode} ({what}) read {r.summary()}"


# ------------------------------------------------------------------------------------------------ scenarios
class MStep:
    """one pump batch; `n` pointer reads precede it (2 in the menu: hover test and nearest-site search; 1 in the browser)"""

    def __init__(self, raws, cursor, n=2):
        self.raws, self.cursor, self.n = raws, cursor, n


def press(x, y, n=2):
    return MStep([CE.Raw(2, x, y), CE.Raw(1, x, y)], (x, y), n)


def idle(cursor, n=1):
    """a key step: the menu ignores keys, so it is one iteration with no action (the pump needs a batch)"""
    return [MStep([CE.Raw(8, 0x41)], cursor) for _ in range(n)]


BACK = (0x300, 0x22D)
SEED_SAVE = (0x2F8, 0x1DC)
LOAD = (0x306, 0x1A9)


SOUND_OF = {1: 1, 3: 2, 5: 0, 7: 3, 9: 7}     # the open records and the sound byte (+4) each carries: every arm of the switch


def menu_image(lib, seed, date=0x4000, cash=900, flags=0, level=0, sites_open=tuple(SOUND_OF), sounds=(0, 1, 2, 3)) -> bytes:
    rng = random.Random(seed)
    img = bytearray(TS.case_image(lib, seed=seed, course_type=1, flags=flags, date=date))
    TS.put(lib, img, "g_cash_div100", struct.pack("<i", cash))
    TS.put(lib, img, "g_buildingUnlockLevel", struct.pack("<i", level))
    TS.put(lib, img, "g_amenityUnlockMask", struct.pack("<I", rng.getrandbits(32)))
    TS.put(lib, img, "g_screenWidth", struct.pack("<i", 1024))
    TS.put(lib, img, "g_screenHeight", struct.pack("<i", 768))
    sites = list(range(16))
    rng.shuffle(sites)
    off, _ = TS.objs(lib)["g_courseTable"]
    for c in range(16):
        r = off + 0x2E * c
        img[r:r + 0x1C] = bytes([65 + (c % 26)]) * 7 + bytes(0x15)                    # the name
        img[r + 0x1C] = sites[c]                                                      # site
        img[r + 0x1D] = rng.randrange(0, 20)                                          # byte +1 (tier)
        img[r + 0x20] = rng.choice(list(sounds) + [7])                                # byte +4: sound
        img[r + 0x22] = 0xFF if c in sites_open else rng.randrange(0, 5)              # byte +6: 0xff = for sale
        img[r + 0x23] = 0xFF if rng.random() < 0.4 else rng.randrange(0, 5)           # byte +7
        if c in SOUND_OF and c in sites_open:
            img[r + 0x20] = SOUND_OF[c]
    return bytes(img)


def spot_of(lib, img, k):
    """site k's map spot + 0x19 (the pointer position that makes it the nearest) from the image's tables"""
    toff, _ = TS.objs(lib)["f_tab_point16_4e3dc0"]
    x, y = struct.unpack_from("<hh", img, toff + 4 * k)
    return x + 0x19, y + 0x19


def site_of_record(lib, img, c):
    off, _ = TS.objs(lib)["g_courseTable"]
    return img[off + 0x2E * c + 0x1C]


def spot_of_record(lib, img, c):
    return spot_of(lib, img, site_of_record(lib, img, c))


SCEN = []   # (id, steps builder (lib, img) -> [MStep], image kwargs, files)


def _add(cid, build, files=None, **kw):
    SCEN.append((cid, build, kw, files))


_saves = TB.saves_dir_files() if (INSTALL / "Saved Games").exists() else {}
_saves_themes = dict(_saves, **E.load_install_themes(INSTALL).data) if _saves else {}   # a theme reload after a load needs the theme files
_add("back", lambda lib, img: [press(*BACK)], seed=1)
_add("back_date0", lambda lib, img: [press(*BACK)], seed=2, date=0)
_add("seed_courses", lambda lib, img: [press(*SEED_SAVE), press(*BACK)], seed=3, date=0)
_add("save_dialog", lambda lib, img: [press(*SEED_SAVE), press(*BACK)], files=_saves, seed=4)
_add("load_then_cancel", lambda lib, img: [press(*LOAD), press(*TB.BTN["back"], n=1), press(*BACK)], files=_saves, seed=5)
_add("load_a_game", lambda lib, img: [press(*LOAD), press(TB.LIST_X, TB.row_y(1), n=1), press(*TB.BTN["load"], n=1)],
     files=_saves_themes, seed=6)
_add("buy_record_1", lambda lib, img: [press(*spot_of_record(lib, img, 1)), press(*BACK)], seed=7, cash=5000)
_add("buy_record_3", lambda lib, img: [press(*spot_of_record(lib, img, 3)), press(*BACK)], seed=8, cash=9000)
_add("buy_record_5", lambda lib, img: [press(*spot_of_record(lib, img, 5)), press(*BACK)], seed=9, cash=9000)
_add("buy_record_7", lambda lib, img: [press(*spot_of_record(lib, img, 7)), press(*BACK)], seed=20, cash=9000)
_add("buy_record_9", lambda lib, img: [press(*spot_of_record(lib, img, 9)), press(*BACK)], seed=21, cash=9000)
_add("buy_without_cash", lambda lib, img: [press(*spot_of_record(lib, img, 1)), MStep([CE.Raw(8, 0x41)], BACK, 0),
                                              MStep([CE.Raw(7, 0x41, 0x41)], BACK, 0), press(*BACK)], seed=10, cash=1)   # the warning waits for a key
_add("buy_cheat", lambda lib, img: [press(*spot_of_record(lib, img, 3)), press(*BACK)], seed=11, cash=1, level=0x11)
_add("press_closed_site", lambda lib, img: [press(*spot_of_record(lib, img, 0)), press(*BACK)], seed=12, cash=9000)
_add("right_press_on_site", lambda lib, img: [MStep([CE.Raw(4, 0, 0)], spot_of_record(lib, img, 1)), press(*BACK)], seed=13)
_add("press_nowhere", lambda lib, img: [press(10, 10), press(*BACK)], seed=14)
_add("hover_tips", lambda lib, img: idle(BACK, 24) + idle(SEED_SAVE, 23) + idle(LOAD, 22) + [press(*BACK)], seed=15)
_add("hover_tips_date0", lambda lib, img: idle(SEED_SAVE, 25) + [press(*BACK)], seed=16, date=0)
_add("cash_flag_label", lambda lib, img: [press(*BACK)], seed=17, flags=0x1000000)
_add("site_text_variants", lambda lib, img: idle(spot_of_record(lib, img, 1), 2) + idle(spot_of_record(lib, img, 2), 2)
     + idle(spot_of_record(lib, img, 5), 2) + idle(spot_of_record(lib, img, 6), 2) + [press(*BACK)], seed=18, cash=300)
def _edge(lib, img, c, dx):
    x, y = spot_of_record(lib, img, c)
    return x + dx, y


_add("nearest_boundary_29", lambda lib, img: [press(*_edge(lib, img, 5, 29)), press(*BACK)], seed=22, cash=9000)
_add("nearest_boundary_30", lambda lib, img: [press(*_edge(lib, img, 5, 30)), press(*BACK)], seed=23, cash=9000)
_add("nearest_boundary_neg", lambda lib, img: [press(*_edge(lib, img, 7, -29)), press(*BACK)], seed=24, cash=9000)
_add("needs_input", lambda lib, img: idle(BACK, 2), seed=19)


def cursors_of(steps):
    out = []
    for s in steps:
        out += [s.cursor] * s.n
    return out


def build(lib, scen):
    cid, builder, kw, files_extra = scen
    img = menu_image(lib, **kw)
    steps = builder(lib, img)
    # the load flow continues in the browser, which reads the pointer once per frame
    return img, [s for s in steps if s is not None]


def run_oracle(lib, scen):
    cid, builder, kw, files_extra = scen
    img, steps = build(lib, scen)
    files = E.Files(files_extra or {})
    io = E.LowIo(files)
    crt = E.CrtModel(files, None, live=True)
    emu = CE.Emu()
    E.make_emu(crt, emu=emu)
    E.add_save_stubs(emu, io, [p for p in E.SAVE_PEERS if p[1] != "Window::refreshFull"])
    for a in TB.BROWSER_DROPS + MENU_DROPS:
        emu.drop(a)
    emu.stub(CACHE_DTOR, 0, 0, False)
    for a in TB.TAIL_HELPERS:
        emu.stub(a, 0, 0, False)
    for a, n in TB.EH_VEC.items():
        emu.stub(a, n, 0, True)
    curs = cursors_of(steps)
    count = [0]

    def view_to_local(uc, a, ecx):
        x, y = curs[min(count[0], len(curs) - 1)]
        count[0] += 1
        uc.mem_write(a[0], struct.pack("<i", x))
        uc.mem_write(a[1], struct.pack("<i", y))
        return 0
    emu.stub_py(TB.VIEW_TO_LOCAL, 2, view_to_local, pops=True, name="View_toLocal")
    raws = [r for s in steps for r in s.raws]
    r = emu.run(ENTRY, [0], img, inputs=raws)
    return r, io, files


def run_port(lib, scen, mode=0):
    cid, builder, kw, files_extra = scen
    img, steps = build(lib, scen)
    raws = [r for s in steps for r in s.raws]
    curs = cursors_of(steps)
    words = [len(raws), len(curs)]
    for c in raws:
        words += [c.handler, c.a0 & 0xFFFFFFFF, c.a1 & 0xFFFFFFFF, c.async_ctrl, c.async_shift, c.t]
    for x, y in curs:
        words += [x & 0xFFFFFFFF, y & 0xFFFFFFFF]
    script = (C.c_uint32 * len(words))(*words)
    peer_names = [p[1] for p in E.SAVE_PEERS if p[1] != "Window::refreshFull"]
    logbuf = (C.c_uint32 * (1 + 8 * TS.PEER_LOG_CAP))()
    with tempfile.TemporaryDirectory(prefix="sg38b_menu_") as d:
        root = pathlib.Path(d)
        for k, v in (files_extra or {}).items():
            p = root / k
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(v)
        prev = os.environ.get("SG_FILES_WRITE_THROUGH")
        os.environ["SG_FILES_WRITE_THROUGH"] = "1"
        lib.negctl("courseActionMenu", mode)
        try:
            with lib.state() as st:
                st.set_data_base(E.DATA_VA)
                st.load_image(img)
                st.files_root(root)
                st.map(0x30000000, script, readonly=True)
                st.map(TS.PEER_LOG_VA, logbuf)
                for nm in peer_names:
                    st.call("commands.recordPeer", [E.fnv1a(nm), TS.PEER_LOG_VA, TS.PEER_LOG_CAP])
                st.call("saveload.setScript", [0x30000000])
                try:
                    eax, code = st.call("courseActionMenu", [0]).eax, 0
                except L.SgError as e:
                    eax, code = None, e.code
                st.call("saveload.clearScript", [])
                st.call("commands.recordClear", [])
                out = st.image()
            n = min(logbuf[0], TS.PEER_LOG_CAP)
            calls = [tuple(logbuf[1 + 8 * k:1 + 8 * k + 8]) for k in range(n)]
            after = TB.snapshot_dir(root)
        finally:
            lib.negctl_reset()
            if prev is None:
                os.environ.pop("SG_FILES_WRITE_THROUGH", None)
            else:
                os.environ["SG_FILES_WRITE_THROUGH"] = prev
    return eax, out, code, calls, after


def compare(lib, scen, mode=0) -> str:
    r, io, files = run_oracle(lib, scen)
    eax, out, code, calls, after = run_port(lib, scen, mode)
    if r.err.startswith("NeedInput"):
        return "" if code == L.SG_E_NO_RESOURCE else f"the original waits for input, the port code {code}"
    if r.err:
        return f"original: {r.err}"
    if code != 0:
        return f"port code {code}"
    if eax != r.eax:
        return f"eax port {eax} original {r.eax}"
    if out != r.image:
        return TS.first_diff(lib, out, r.image)
    # enterModal483d30 is also called from inside the original's real pumpAndRedraw (the port's modal pump does not call it:
    # a drop, no simulation effect); the function's own call is its last: compare the rest and that last call
    modal = E.fnv1a("enterModal483d30")
    strip = lambda cs: [c for c in cs if c[0] != modal]
    if strip(calls) != strip(io.peer_calls) or not calls or calls[-1][0] != modal or io.peer_calls[-1][0] != modal:
        f = lambda cs: [(hex(c[0])[-4:], c[1:1 + c[7]]) for c in cs]
        return f"callee calls differ: port {f(calls)} original {f(io.peer_calls)}"
    want = TB.snapshot_files(files)
    if after != want:
        return f"files differ: only port {sorted(set(after) - set(want))} only original {sorted(set(want) - set(after))}"
    return ""


@need_exe
@need_saves
@emu_oracle
@pytest.mark.parametrize("scen", SCEN, ids=[s[0] for s in SCEN])
def test_oracle_equals_original(portlib, scen):
    msg = compare(portlib, scen)
    assert msg == "", msg


# ------------------------------------------------------------------------------------------------ golden digests, controls
def _digest(eax, code, image, calls, after) -> str:
    h = hashlib.sha256()
    h.update(b"err%d" % code if eax is None else struct.pack("<I", eax & 0xFFFFFFFF))
    h.update(image)
    h.update(repr(calls).encode())
    h.update(repr(sorted(after.items())).encode())
    return h.hexdigest()[:24]


def _digests(lib, mode=0) -> dict:
    out = {}
    for sc in SCEN:
        eax, img, code, calls, after = run_port(lib, sc, mode)
        out[sc[0]] = _digest(eax, code, img, calls, after)
    return out


def _files_sha() -> str:
    h = hashlib.sha256()
    for sc in SCEN:
        for k in sorted(sc[3] or {}):
            h.update(k.encode())
            h.update(hashlib.sha256(sc[3][k]).digest())
    return h.hexdigest()[:16]


@need_exe
@need_saves
def test_golden_digests(portlib):
    if os.environ.get("SG_EMU_ORACLE") == "update":
        bad = [f"{sc[0]}: {m}" for sc in SCEN for m in [compare(portlib, sc)] if m]
        assert not bad, f"oracle RED: not pinned: {bad[:4]}"
        E.golden_update("courseActionMenu", {"files": _files_sha(), "digests": _digests(portlib)})
    g = E.golden_load().get("courseActionMenu")
    if not g:
        pytest.skip("courseActionMenu: no pinned digests (SG_EMU_ORACLE=update)")
    if g["files"] != _files_sha():
        pytest.skip("courseActionMenu: the install's files differ from the pinned run")
    got = _digests(portlib)
    bad = [k for k in got if got[k] != g["digests"].get(k)]
    assert not bad, f"{len(bad)} scenarios differ from the pinned digests: {bad[:6]}"


NEGCTL = [(1, "the purchase takes the next site's record"), (2, "the unlock mask gets bit c + 1"), (4, "nearest-site radius 0x1d")]


@need_exe
@need_saves
@pytest.mark.parametrize("mode,what", NEGCTL)
def test_negctl_reads_red(portlib, mode, what):
    g = E.golden_load().get("courseActionMenu")
    if not g or g["files"] != _files_sha():
        pytest.skip("courseActionMenu: no pinned digests for these files")
    got = _digests(portlib, mode)
    changed = [k for k in got if got[k] != g["digests"][k]]
    assert changed, f"mode {mode} ({what}) left every pinned digest unchanged"
