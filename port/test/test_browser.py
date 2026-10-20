"""Phase 3.8 stream 38b: loadGameBrowserScreen (0x0043b610, port/src/sim/saveload.cpp).

The modal browser of saved games and championship courses, simulation effects only. Evidence: the emulator oracle
(opt-in, SG_EMU_ORACLE=1; port/test/emu38b.py): the ORIGINAL runs under Unicorn with its drawing callees dropped, its
pumps fed from a scripted input (cmdemu's modal pump: one batch per Blocking pump, ending at a press or a key), its
cursor read from a script, the CRT's file layer stood in by a dictionary, Windows' own FindFirstFileA listing the files,
and the terrain / window callees of loadGame recorded. The port must equal eax, the whole data image, the recorded call
sequence and the files written or deleted. Default suite: pinned digests of those results (port/test/golden_38b.json) and
every negative control against them. The segments that reach the browser (cov_a_flags/01, 03, 04) are run by
log/p3/p37a_cmdsegs.py with the port's DLL (log/p38/38b_notes.md).
"""
from __future__ import annotations

import ctypes as C
import hashlib
import os
import pathlib
import struct
import tempfile

import pytest

from port.test import cmdemu as CE
from port.test import emu38b as E
from port.test import lib as L
from port.test import test_saveload as TS

ROOT = pathlib.Path(__file__).resolve().parents[2]
INSTALL = ROOT / "original"
pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
need_saves = pytest.mark.skipif(not (INSTALL / "Saved Games" / "mood5.sve").exists(), reason="the install's saves are absent")
emu_oracle = pytest.mark.skipif(not E.have_oracle(), reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn")

ENTRY = 0x0043B610
VIEW_TO_LOCAL = 0x0047AB50
WAIT_TICKS = 0x0045BF80
DISPLAY_PTR_VA = 0x31000000
# drawing callees of the browser, dropped by their own `ret N`
BROWSER_DROPS = [0x004049D0, 0x00404B70, 0x00404BC0, 0x0040CA10, 0x00473BF0, 0x00473E60, 0x00473F60, 0x00474AE0, 0x00474C40,
                 0x00475840, 0x00475C60, 0x004762D0, 0x00480B00, 0x00480C80, WAIT_TICKS,
                 0x00485790]      # Image::loadPcx (loadThemeFile decodes a portrait)
TAIL_HELPERS = [0x00483C30, 0x00483C70, 0x00483BD0]      # pumpInput helpers: tail jumps, no argument (cmdemu)
EH_VEC = {0x004A55D2: 5, 0x004A5713: 4}                   # the MSVC array constructor / destructor helpers (stdcall)
BTN = {"load": (0x279, 0x22B), "back": (0x301, 0x22C), "up": (0x310, 0x78), "down": (0x310, 0x190), "delete": (0x2C0, 0x22B)}
LIST_X, LIST_Y0 = 0x190, 0x74


def row_y(r: int) -> int:
    return LIST_Y0 + 16 * r + 3


class Step:
    def __init__(self, raws, cursor):
        self.raws, self.cursor = raws, cursor


def click(x, y, cursor=None):
    return Step([CE.Raw(2, x, y), CE.Raw(1, x, y)], cursor or (x, y))


def click_row(r, scroll_rows=0):
    return click(LIST_X, row_y(r))


def click_btn(name):
    x, y = BTN[name]
    return click(x, y)


def key_steps(code, cursor=(1, 1)):
    """a key as the root handlers see it: onChar (WM_KEYDOWN) then onKey (WM_CHAR), each ends a Blocking batch"""
    return [Step([CE.Raw(8, code)], cursor), Step([CE.Raw(7, code, code)], cursor)]


def flatten(steps):
    return [r for s in steps for r in s.raws], [s.cursor for s in steps]


# ------------------------------------------------------------------------------------------------ scenarios
def saves_dir_files(extra_copies=0, autosave_copies=0) -> dict:
    """the install's saved games plus copies under other names: `extra_copies` plain names, `autosave_copies` names that
    start with '&' (their title is read from the file)"""
    s = TS.real_saves()
    out = {"Saved Games/" + k: v for k, v in s.items()}
    base = s.get("mood5.sve", b"")
    for i in range(extra_copies):
        out["Saved Games/course%02d.sve" % i] = base if i % 2 == 0 else s.get("stories9.sve", base)
    for i in range(autosave_copies):
        out["Saved Games/&Auto%02d.sve" % i] = s.get("stories5.sve", base)
    return out


def champ_files() -> dict:
    p = INSTALL / "Themes" / "Championship"
    out = {}
    cse = TS._bytes_of_cse()
    if cse:
        out["Themes/Championship/Ocean's Edge MC.cse"] = cse
        out["Themes/Championship/Second Course.cse"] = cse[:20000]
    return out


SCEN = []   # (id, files dict, resume, champ, steps, image kwargs)


def _add(cid, files, resume, champ, steps, **kw):
    SCEN.append((cid, files, resume, champ, steps, kw))


_base = saves_dir_files() if (INSTALL / "Saved Games").exists() else {}
_many = saves_dir_files(extra_copies=22, autosave_copies=3) if _base else {}
_add("load_row", _base, 0, 0, [click_row(2), click_btn("load")], seed=1)
_with_themes = dict(_base, **{k: v for k, v in E.load_install_themes(INSTALL).data.items()}) if _base else {}
_add("load_row_other_theme", _with_themes, 0, 0, [click_row(1), click_btn("load")], seed=2, theme=b"Firaxis")
_add("load_row_resume", _base, 1, 0, [click_row(3), click_btn("load")], seed=3)
_add("cancel", _base, 0, 0, [click_row(2), click_btn("back")], seed=4)
_add("cancel_resume", _base, 1, 0, [click_btn("back")], seed=5)
_add("load_no_selection", _base, 0, 0, [click_btn("load")], seed=6)
_add("click_below_list", _base, 0, 0, [click(LIST_X, row_y(9)), click_btn("load")], seed=7)
_add("click_left_of_list", _base, 0, 0, [click(0x100, row_y(1)), click_btn("back")], seed=8)
_add("click_above_list", _base, 0, 0, [click(LIST_X, 0x50), click_btn("back")], seed=9)
_add("select_then_deselect", _base, 0, 0, [click_row(1), click_row(7), click_btn("load")], seed=10)
_add("esc_key_back", _base, 0, 0, [click_row(2)] + key_steps(0x1B) + [click_btn("back")], seed=11)
_add("other_key_acts_at_pointer", _base, 0, 0, [click_row(0)] + key_steps(0x41) + [click_btn("back")], seed=12)
_add("many_scroll_down_up", _many, 0, 0, [click_btn("down"), click_btn("down"), click_btn("up"), click_btn("back")], seed=13)
_add("many_below_list_scrolls", _many, 0, 0, [click(LIST_X, row_y(17)), click(LIST_X, row_y(17)), click_row(3), click_btn("load")], seed=14)
_add("many_row_autosave_titles", _many, 0, 0, [click_btn("down"), click_row(5), click_btn("back")], seed=15)
_add("many_scroll_past_end", _many, 0, 0, [click_btn("down")] * 6 + [click_row(4), click_btn("load")], seed=16)
_add("delete_confirmed", _base, 0, 0, [click_row(2), click_btn("delete"), click_btn("back")], seed=17)
_add("delete_without_selection", _base, 0, 0, [click_btn("delete"), click_btn("back")], seed=18)
_champ = champ_files()
_add("champ_load_row", _champ, 0, 1, [click_row(0), click_btn("load")], seed=19)
_add("champ_second_row", _champ, 0, 1, [click_row(1), click_btn("load")], seed=20)
_add("champ_cancel", _champ, 0, 1, [click_row(0), click_btn("back")], seed=21)
_add("champ_no_selection", _champ, 0, 1, [click_btn("load")], seed=22)
_add("champ_delete", _champ, 0, 1, [click_row(1), click_btn("delete"), click_btn("back")], seed=23)
_add("empty_directory", {}, 0, 0, [click_row(0), click_btn("load")], seed=24)
_add("load_button_edge", _base, 0, 0, [click(0x279 + 25, 0x22B), click_btn("back")], seed=26)
_add("needs_input", _base, 0, 0, [click_row(1)], seed=25)               # the script runs out: both sides wait for input


def scen_image(lib, kw) -> bytes:
    import random
    rng = random.Random(kw.get("seed", 0))
    img = bytearray(TS.case_image(lib, seed=kw.get("seed", 0), course_type=kw.get("course_type"), flags=kw.get("flags")))
    TS.put(lib, img, "g_theme", kw.get("theme", b"Standard") + b"\0" + bytes(rng.randrange(256) for _ in range(30)))
    TS.put(lib, img, "f_stateptr_4c1570", struct.pack("<I", DISPLAY_PTR_VA))
    TS.put(lib, img, "g_screenWidth", struct.pack("<i", 1024))   # the root handlers scale the click by it (a zero width divides by zero)
    TS.put(lib, img, "g_screenHeight", struct.pack("<i", 768))
    return bytes(img)


def make_emu(io, crt, steps):
    emu = CE.Emu()
    E.make_emu(crt, emu=emu)
    peers = [p for p in E.SAVE_PEERS if p[1] != "Window::refreshFull"]
    E.add_save_stubs(emu, io, peers)
    for a in BROWSER_DROPS:
        emu.drop(a)
    for a in TAIL_HELPERS:
        emu.stub(a, 0, 0, False)
    for a, n in EH_VEC.items():
        emu.stub(a, n, 0, True)
    cursors = [s.cursor for s in steps]
    count = [0]

    def view_to_local(uc, a, ecx):
        x, y = cursors[min(count[0], len(cursors) - 1)]
        count[0] += 1
        uc.mem_write(a[0], struct.pack("<i", x))
        uc.mem_write(a[1], struct.pack("<i", y))
        return 0
    emu.stub_py(VIEW_TO_LOCAL, 2, view_to_local, pops=True, name="View_toLocal")
    return emu


def run_oracle(lib, scen):
    cid, files_extra, resume, champ, steps, kw = scen
    files = E.Files(files_extra)
    io = E.LowIo(files)
    crt = E.CrtModel(files, None, live=True)
    emu = make_emu(io, crt, steps)
    raws, _ = flatten(steps)
    r = emu.run(ENTRY, [resume, champ], scen_image(lib, kw), inputs=raws)
    return r, io, files


def snapshot_dir(root: pathlib.Path) -> dict:
    return {E.norm(p.relative_to(root).as_posix()): hashlib.sha256(p.read_bytes()).hexdigest()[:16]
            for p in sorted(root.rglob("*")) if p.is_file()}


def snapshot_files(files: E.Files) -> dict:
    return {k: hashlib.sha256(v).hexdigest()[:16] for k, v in sorted(files.data.items())}


def run_port(lib, scen, mode=0):
    cid, files_extra, resume, champ, steps, kw = scen
    raws, cursors = flatten(steps)
    words = [len(raws), len(cursors)]
    for c in raws:
        words += [c.handler, c.a0 & 0xFFFFFFFF, c.a1 & 0xFFFFFFFF, c.async_ctrl, c.async_shift, c.t]
    for x, y in cursors:
        words += [x & 0xFFFFFFFF, y & 0xFFFFFFFF]
    script = (C.c_uint32 * len(words))(*words)
    peer_names = [p[1] for p in E.SAVE_PEERS if p[1] != "Window::refreshFull"]
    logbuf = (C.c_uint32 * (1 + 8 * TS.PEER_LOG_CAP))()
    with tempfile.TemporaryDirectory(prefix="sg38b_browser_") as d:
        root = pathlib.Path(d)
        for k, v in files_extra.items():
            p = root / k
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(v)
        prev = os.environ.get("SG_FILES_WRITE_THROUGH")
        os.environ["SG_FILES_WRITE_THROUGH"] = "1"
        lib.negctl("loadGameBrowserScreen", mode)
        try:
            with lib.state() as st:
                st.set_data_base(E.DATA_VA)
                st.load_image(scen_image(lib, kw))
                st.files_root(root)
                st.map(0x30000000, script, readonly=True)
                st.map(TS.PEER_LOG_VA, logbuf)
                for nm in peer_names:
                    st.call("commands.recordPeer", [E.fnv1a(nm), TS.PEER_LOG_VA, TS.PEER_LOG_CAP])
                st.call("saveload.setScript", [0x30000000])
                try:
                    eax, code = st.call("loadGameBrowserScreen", [resume, champ]).eax, 0
                except L.SgError as e:
                    eax, code = None, e.code
                st.call("saveload.clearScript", [])
                st.call("commands.recordClear", [])
                out = st.image()
            n = min(logbuf[0], TS.PEER_LOG_CAP)
            calls = [tuple(logbuf[1 + 8 * k:1 + 8 * k + 8]) for k in range(n)]
            after = snapshot_dir(root)
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
    if calls != io.peer_calls:
        f = lambda cs: [(hex(c[0])[-4:], c[1:1 + c[7]]) for c in cs]
        return f"callee calls differ: port {f(calls)} original {f(io.peer_calls)}"
    want = snapshot_files(files)
    # the files: the original's final dictionary against the port's final directory
    if after != want:
        return f"files differ: only port {sorted(set(after) - set(want))} only original {sorted(set(want) - set(after))} " \
               f"changed {[k for k in after if k in want and after[k] != want[k]]}"
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
        for k in sorted(sc[1]):
            h.update(k.encode())
            h.update(hashlib.sha256(sc[1][k]).digest())
    return h.hexdigest()[:16]


@need_exe
@need_saves
def test_golden_digests(portlib):
    """the port reproduces the pinned results of the emulated original; SG_EMU_ORACLE=update checks every scenario against
    the oracle first, then pins"""
    if os.environ.get("SG_EMU_ORACLE") == "update":
        bad = [f"{sc[0]}: {m}" for sc in SCEN for m in [compare(portlib, sc)] if m]
        assert not bad, f"oracle RED: not pinned: {bad[:4]}"
        E.golden_update("browser", {"files": _files_sha(), "digests": _digests(portlib)})
    g = E.golden_load().get("browser")
    if not g:
        pytest.skip("browser: no pinned digests (SG_EMU_ORACLE=update)")
    if g["files"] != _files_sha():
        pytest.skip("browser: the install's files differ from the pinned run")
    got = _digests(portlib)
    bad = [k for k in got if got[k] != g["digests"].get(k)]
    assert not bad, f"{len(bad)} scenarios differ from the pinned digests: {bad[:6]}"


NEGCTL = [(1, "scroll down by 5"), (2, "list rows 17 pixels high"), (3, "load takes the row after the selection"),
          (4, "back does not restore the flags"), (5, "the browsing save is not reloaded"),
          (6, "loadThemes never runs after a load"), (7, "delete removes the next row's file"), (8, "load button radius 0x19")]


@need_exe
@need_saves
@pytest.mark.parametrize("mode,what", NEGCTL)
def test_negctl_reads_red(portlib, mode, what):
    """every wrong body changes at least one pinned digest"""
    g = E.golden_load().get("browser")
    if not g or g["files"] != _files_sha():
        pytest.skip("browser: no pinned digests for these files")
    got = _digests(portlib, mode)
    changed = [k for k in got if got[k] != g["digests"][k]]
    assert changed, f"mode {mode} ({what}) left every pinned digest unchanged"
