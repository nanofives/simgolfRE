"""Phase 3.8 stream 38b (round 2): saveCourse (0x0040b680), saveGameDialog (0x00405b10), promptAndSaveFile (0x00437910),
port/src/sim/saveload.cpp. Emulator oracle in the manner of test_saveload.py: the ORIGINAL runs under Unicorn with its file
layer (low-level and stdio) stood in by a dictionary, Windows' FindFirstFileA behind fileExists, drawing dropped and the dialog /
message callees recorded; the port must equal eax, the whole data image, the recorded calls and the files written. Default
suite: pinned digests (port/test/golden_38b.json) and every negative control against them.
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

pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(not E.have_oracle(), reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn")

ENTRY = {"saveCourse": 0x0040B680, "saveGameDialog": 0x00405B10, "promptAndSaveFile": 0x00437910}
DROPS = {
    "saveCourse": [0x004762D0],
    "saveGameDialog": [0x0040CC00, 0x004762D0, 0x00404BC0],
    "promptAndSaveFile": [0x00474AE0, 0x00474DD0, 0x00478AF0, 0x00473CB0, 0x004789F0, 0x00485AA0, 0x00474C40, 0x00483030],
}
FAKE_VA = 0x31100000          # a sprite object whose vtable slot 5 (the virtual call promptAndSaveFile makes) is a stub


def fake_block():
    vt = struct.pack("<I", FAKE_VA + 0x100) + bytes(0xFC) + struct.pack("<I", FAKE_VA + 0x800) * 0x40
    return (FAKE_VA, vt.ljust(0x900, b"\xC3"))


def case_image(lib, seed, **kw) -> bytes:
    rng = random.Random(seed)
    img = bytearray(TS.case_image(lib, seed=seed, course_type=kw.get("course_type", 1), flags=kw.get("flags", 0),
                                  date=kw.get("date")))
    P = lambda n, d, at=0: TS.put(lib, img, n, d, at)
    P("g_bestRounds", struct.pack("<i", kw.get("best", rng.randrange(-5, 400))))
    P("g_holesPlusOne", struct.pack("<i", rng.randrange(1, 20)))
    P("g_theme", kw.get("theme", b"Standard") + b"\0")
    P("g_screenWidth", struct.pack("<i", 1024))
    # a golfer type's name field is what the file name and the headers use
    off, _ = TS.objs(lib)["g_golferTypes"]
    for t in range(84):
        name = kw.get("names", {}).get(t, ("Golfer%02d" % t).encode())
        img[off + 0x230 * t + 0x10:off + 0x230 * t + 0x10 + len(name) + 1] = name + b"\0"
        img[off + 0x230 * t + 0x23] = rng.randrange(256)
    P("g_golferPhrases", bytes(rng.randrange(256) for _ in range(0x4E2 * 3)))
    P("g_playerSkills", bytes(rng.randrange(1, 20) for _ in range(16)))
    for g in (0, 1, 2, 3, 9):
        P("g_golfers", bytes(rng.randrange(1, 20) for _ in range(16)), 0x100 * g + 0xF0)
    P("g_ui_4c7b34", struct.pack("<i", kw.get("ui_flag", 0)))
    P("g_namePoolOffset", struct.pack("<h", kw.get("title_offset", -1)))
    if "title" in kw:
        P("g_namePool", kw["title"] + b"\0")
    if "text" in kw:
        P("g_textBuffer", kw["text"] + b"\0")
    # promptAndSaveFile's virtual calls read the sprite object pointers at two arrays (0x84 apart): point them at the fake
    for base in (0x562918, 0x563368):
        for idx in range(0, 80):
            va = base + 0x84 * idx
            o = va - 0x4C1000
            if o + 4 <= len(img):
                struct.pack_into("<I", img, o, FAKE_VA)
    struct.pack_into("<I", img, 0x5AA564 - 0x4C1000, 0)
    return bytes(img)


SCEN = []   # (id, entry name, args, image kwargs, extra files)


def _add(cid, fn, args=(), files=None, **kw):
    SCEN.append((cid, fn, tuple(args), kw, files or {}))


SV = "Saved Games/"
_add("course_save", "saveCourse", text=b"Courses\\mycourse.cse", seed=1)
_add("course_save_record0", "saveCourse", text=b"Saved Games\\a.cse", seed=2, best=0, title_offset=0, title=b"Custom title")
_add("course_save_negative_record", "saveCourse", text=b"x.cse", seed=3, best=-7)
_add("course_save_flags", "saveCourse", text=b"y.cse", seed=4, flags=0xFFFFFFFF)
_add("dialog_default_name", "saveGameDialog", seed=5, date=0x1234)
_add("dialog_title_custom", "saveGameDialog", seed=6, date=0x2400, title_offset=0, title=b"My Links")
_add("dialog_title_trailing_spaces", "saveGameDialog", seed=7, date=5, title_offset=0, title=b"Padded   ")
_add("dialog_illegal_name", "saveGameDialog", seed=8, date=7, title_offset=0, title=b"Bad*Name")
_add("dialog_slash_name", "saveGameDialog", seed=9, date=7, title_offset=0, title=b"A/B")
_add("dialog_date0", "saveGameDialog", seed=10, date=0)
_add("dialog_negative_date", "saveGameDialog", seed=11, date=-3000)
_add("dialog_big_date", "saveGameDialog", seed=12, date=0x7FFFFFFF)
_add("dialog_existing_file", "saveGameDialog", seed=5, date=0x1234,
     files={SV + "Ocean's Edge 17 July 2001.sve": b"old save"})
_add("dialog_existing_other_date", "saveGameDialog", seed=6, date=0x2400, title_offset=0, title=b"My Links",
     files={SV + "My Links 1 April 2002.sve": b"old", SV + "other.sve": b"x"})
_add("prompt_chr_index0", "promptAndSaveFile", (0, -1, 0), seed=13)
_add("prompt_pro_mode0", "promptAndSaveFile", (3, 2, 0), seed=14)
_add("prompt_pro_mode9", "promptAndSaveFile", (9, 9, 0), seed=15)
_add("prompt_default_name", "promptAndSaveFile", (5, -2, 0), seed=16)
_add("prompt_championship", "promptAndSaveFile", (1, 1, 1), seed=17)
_add("prompt_index0_skills", "promptAndSaveFile", (0, 0, 0), seed=18)
_add("prompt_ui_flag", "promptAndSaveFile", (2, -1, 0), seed=19, ui_flag=1)
_add("prompt_bad_name", "promptAndSaveFile", (4, -1, 0), seed=20, names={4: b"A?B"})
_add("prompt_empty_name", "promptAndSaveFile", (4, -1, 0), seed=21, names={4: b""})
_add("prompt_spaces_name", "promptAndSaveFile", (4, 1, 0), seed=22, names={4: b"Name   "})
_add("prompt_existing_file", "promptAndSaveFile", (6, -1, 0), seed=23, files={"Themes/Standard/Golfer06.chr": b"old"})
_add("prompt_existing_default", "promptAndSaveFile", (6, -2, 0), seed=24, files={"Themes/Default.pro": b"old"})


def make_oracle(fn, files):
    f = E.Files(files)
    io = E.LowIo(f)
    crt = E.CrtModel(f, None, live=True)
    emu = CE.Emu()
    E.make_emu(crt, emu=emu)
    E.add_save_stubs(emu, io, [p for p in E.SAVE_PEERS if p[1] != "Window::refreshFull"])
    for a in DROPS[fn]:
        emu.drop(a)
    emu.stub(FAKE_VA + 0x800, 0, 0, False, "fake slot")
    return emu, io, crt, f


def run_oracle(lib, scen):
    cid, fn, args, kw, files = scen
    emu, io, crt, f = make_oracle(fn, files)
    r = emu.run(ENTRY[fn], list(args), case_image(lib, **kw), blocks=[fake_block()])
    return r, io, crt, f


def snapshot_files(f: E.Files) -> dict:
    return {k: hashlib.sha256(v).hexdigest()[:16] for k, v in sorted(f.data.items())}


def run_port(lib, scen, mode=0):
    cid, fn, args, kw, files = scen
    logbuf = (C.c_uint32 * (1 + 8 * TS.PEER_LOG_CAP))()
    fake = (C.c_uint8 * 0x900).from_buffer_copy(fake_block()[1])
    with tempfile.TemporaryDirectory(prefix="sg38b_dlg_") as d:
        root = pathlib.Path(d)
        for k, v in files.items():
            p = root / k
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(v)
        prev = os.environ.get("SG_FILES_WRITE_THROUGH")
        os.environ["SG_FILES_WRITE_THROUGH"] = "1"
        lib.negctl(fn, mode)
        try:
            with lib.state() as st:
                st.set_data_base(E.DATA_VA)
                st.load_image(case_image(lib, **kw))
                st.files_root(root)
                st.map(FAKE_VA, fake)
                st.map(TS.PEER_LOG_VA, logbuf)
                for _, nm, _, _ in E.SAVE_PEERS:
                    st.call("commands.recordPeer", [E.fnv1a(nm), TS.PEER_LOG_VA, TS.PEER_LOG_CAP])
                try:
                    eax, code = st.call(fn, list(args)).eax, 0
                except L.SgError as e:
                    eax, code = None, e.code
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
    r, io, crt, f = run_oracle(lib, scen)
    eax, out, code, calls, after = run_port(lib, scen, mode)
    if r.err:
        return f"original: {r.err}"
    if code != 0:
        return f"port code {code}"
    if eax != r.eax:
        return f"eax port {eax} original {r.eax}"
    if out != r.image:
        # promptAndSaveFile: the original also writes a render field (0x5aa564, a fake slot's eax = 0): equal by construction
        return TS.first_diff(lib, out, r.image)
    if calls != io.peer_calls:
        fmt = lambda cs: [(hex(c[0])[-4:], c[1:1 + c[7]]) for c in cs]
        return f"callee calls differ: port {fmt(calls)} original {fmt(io.peer_calls)}"
    want = snapshot_files(f)
    if after != want:
        only_p, only_o = sorted(set(after) - set(want)), sorted(set(want) - set(after))
        diff = [k for k in after if k in want and after[k] != want[k]]
        return f"files differ: only port {only_p} only original {only_o} changed {diff}"
    return ""


@need_exe
@emu_oracle
@pytest.mark.parametrize("scen", SCEN, ids=[s[0] for s in SCEN])
def test_oracle_equals_original(portlib, scen):
    msg = compare(portlib, scen)
    assert msg == "", msg


def _digest(eax, code, image, calls, after) -> str:
    h = hashlib.sha256()
    h.update(b"err%d" % code if eax is None else struct.pack("<I", eax & 0xFFFFFFFF))
    h.update(image)
    h.update(repr(calls).encode())
    h.update(repr(sorted(after.items())).encode())
    return h.hexdigest()[:24]


def _digests(lib, fn=None, mode=0) -> dict:
    out = {}
    for sc in SCEN:
        eax, img, code, calls, after = run_port(lib, sc, mode if sc[1] == fn else 0)
        out[sc[0]] = _digest(eax, code, img, calls, after)
    return out


@need_exe
def test_golden_digests(portlib):
    if os.environ.get("SG_EMU_ORACLE") == "update":
        bad = [f"{sc[0]}: {m}" for sc in SCEN for m in [compare(portlib, sc)] if m]
        assert not bad, f"oracle RED: not pinned: {bad[:4]}"
        E.golden_update("saveDialogs", {"digests": _digests(portlib)})
    g = E.golden_load().get("saveDialogs")
    if not g:
        pytest.skip("saveDialogs: no pinned digests (SG_EMU_ORACLE=update)")
    got = _digests(portlib)
    bad = [k for k in got if got[k] != g["digests"].get(k)]
    assert not bad, f"{len(bad)} scenarios differ from the pinned digests: {bad[:6]}"


NEGCTL = [("saveCourse", 1, "no course body"), ("saveCourse", 2, "a 4-byte marker"),
          ("saveGameDialog", 1, "a refused name goes on"), ("saveGameDialog", 2, "an existing file is not asked about"),
          ("promptAndSaveFile", 1, "the skill record's first byte flipped")]


@need_exe
@pytest.mark.parametrize("fn,mode,what", NEGCTL)
def test_negctl_reads_red(portlib, fn, mode, what):
    g = E.golden_load().get("saveDialogs")
    if not g:
        pytest.skip("saveDialogs: no pinned digests")
    got = _digests(portlib, fn, mode)
    changed = [k for k in got if got[k] != g["digests"][k]]
    assert changed, f"{fn} mode {mode} ({what}) left every pinned digest unchanged"
