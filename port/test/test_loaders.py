"""Phase 3.8 stream 38b: listMatchingFiles and loadGolferRoster (port/src/sim/loaders.cpp).

Evidence (README "judging"):
  * the listing is checked against WINDOWS' OWN FindFirstFileA on a real directory (test_listing_equals_windows: order,
    wildcard rules, '.' / '..', case), through the port function with a Memory and a Dir GameFiles;
  * the emulator oracle (opt-in, SG_EMU_ORACLE=1; port/test/emu38b.py) runs the ORIGINAL functions under Unicorn with the
    file layer served from the same files and Windows' real listing, and the port must equal eax, the error state and
    the whole data image; the default suite checks the port against pinned digests of that oracle's results
    (port/test/golden_38b.json, SG_EMU_ORACLE=update after the oracle is GREEN) and every negative control against them.
The files are the user's own install (original/Themes) plus synthetic ones; without the install the tests skip.
"""
from __future__ import annotations

import ctypes as C
import hashlib
import json
import os
import pathlib
import struct
import sys

import pytest

from port.test import emu38b as E
from port.test import lib as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "port" / "test" / "golden_38b.json"
INSTALL = ROOT / "original"
BLOCK_VA = 0x30000000
pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
need_themes = pytest.mark.skipif(not (INSTALL / "Themes" / "Standard").exists(), reason="the install's Themes are absent")
need_windows = pytest.mark.skipif(sys.platform != "win32", reason="the listing oracle is Windows' FindFirstFileA")
emu_oracle = pytest.mark.skipif(not E.have_oracle(), reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn")

ENTRY = {"listMatchingFiles": 0x0043D2A0, "loadGolferRoster": 0x004659A0}
# callees the oracle drops (drawing / decode): Image::loadPcx, Surface_blit
DROPS = (0x00485790, 0x00473BF0)
SCROLL = 0x0046DE70   # scrollTextWindow: recorded in both arms (the original's stub, the port's commands.recordPeer)
PEER_LOG_VA, PEER_LOG_CAP = 0x32000000, 16


# ------------------------------------------------------------------------------------------------ helpers
_B = {}


def objs(lib):
    if "o" not in _B:
        _B["o"] = {n: (off, sz) for n, off, sz in lib.objects()}
    return _B["o"]


def base_image(lib) -> bytes:
    if "img" not in _B:
        assert lib.exe_load() == ""
        with lib.state() as st:
            st.fill_from_exe(whole_image=True)
            _B["img"] = st.image()
    return _B["img"]


def put(lib, img: bytearray, name: str, data: bytes, at=0):
    off, size = objs(lib)[name]
    assert at + len(data) <= size, (name, at, len(data), size)
    img[off + at:off + at + len(data)] = data


def get(lib, img, name: str, at=0, n=4) -> bytes:
    off, _ = objs(lib)[name]
    return bytes(img[off + at:off + at + n])


def themes(files_extra: dict | None = None) -> E.Files:
    f = E.load_install_themes(INSTALL)
    for k, v in (files_extra or {}).items():
        if v is None:
            f.data.pop(E.norm(k), None)
        else:
            f.put(k, v)
    return f


def first_diff(lib, a: bytes, b: bytes) -> str:
    if a == b:
        return ""
    i = next(i for i in range(len(a)) if a[i] != b[i])
    for n, (off, sz) in objs(lib).items():
        if off <= i < off + sz:
            return f"{n}+{i - off:#x}: port {a[i]:#04x} original {b[i]:#04x}"
    return f"byte {i:#x}"


def port_run(lib, name, args, image: bytes, files: E.Files, blocks=(), files_root=None, record=(), calls_out=None):
    """(eax or None, image, error code); `record` names the peers whose calls are logged instead of run (the same
    commands.recordPeer seam test_saveload.py uses) and `calls_out` receives the log records (hash, 6 args, count)"""
    keep = []
    logbuf = (C.c_uint32 * (1 + 8 * PEER_LOG_CAP))()
    with lib.state() as st:
        st.set_data_base(E.DATA_VA)
        st.load_image(image)
        if files_root is not None:
            st.files_root(files_root)
        else:
            for k, v in files.data.items():
                st.files_put(files.spell[k], v)
        for base, b in blocks:
            buf = (C.c_uint8 * len(b)).from_buffer_copy(bytes(b))
            keep.append(buf)
            st.map(base, buf, readonly=True)
        if record:
            st.map(PEER_LOG_VA, logbuf)
            for nm in record:
                st.call("commands.recordPeer", [E.fnv1a(nm), PEER_LOG_VA, PEER_LOG_CAP])
        try:
            eax, code = st.call(name, list(args)).eax, 0
        except L.SgError as e:
            eax, code = None, e.code
        if record:
            st.call("commands.recordClear", [])
            if calls_out is not None:
                n = min(logbuf[0], PEER_LOG_CAP)
                calls_out.extend(tuple(logbuf[1 + 8 * k:1 + 8 * k + 8]) for k in range(n))
        return eax, st.image(), code


def digest(eax, code, image: bytes, calls=()) -> str:
    h = hashlib.sha256()
    h.update(b"err%d" % code if eax is None else struct.pack("<I", eax & 0xFFFFFFFF))
    h.update(image)
    for c in calls:
        h.update(struct.pack("<8I", *c))
    return h.hexdigest()[:24]


# ------------------------------------------------------------------------------------------------ listMatchingFiles
def synth_saves(n_extra=0) -> dict:
    d = {}
    for nm in ["&AutoSave1.sve", "&AutoSave2.sve", "mood5.sve", "stories5.sve", "Zed.SVE", "a b.sve", "_x9.sve",
               "While_Browsing.sve", "ShadowRun.sve", "my_shadow.sve", "notes.txt", "noext", "UPPER.sve", "z.sve",
               "Sub/inner.sve", "9lives.sve", "Mood10.sve", "mood2.sve"]:
        d["Saved Games/" + nm] = b"x" + nm.encode()
    for i in range(n_extra):
        d["Many/f%03d.sve" % i] = b"m"
    return d


def list_cases() -> list[tuple]:
    """(case id, extra files, pattern, require, exclude)"""
    c = []
    S = "Saved Games\\"
    for i, (pat, req, exc) in enumerate([
            (S + "*.sve", "", ""), (S + "*.sve", "", "While_Browsing"), (S + "*.sve", "mood", ""),
            (S + "*.sve", "AutoSave", "2"), (S + "*", "", ""), (S + "?.sve", "", ""), (S + "*.*", "", "txt"),
            (S + "*.SVE", "e", "ZED"), ("Saved Games\\Sub\\*", "", ""), (S + "*.chr", "", ""),
            ("Themes\\Firaxis\\*.chr", "", ""), ("Themes\\Firaxis\\*.pro", "", ""), ("Themes\\Standard\\*.txt", "", ""),
            ("Themes\\Standard\\*.txt", "Open", ""), ("Themes\\Nowhere\\*.txt", "", ""), ("Nowhere\\*", "", ""),
            ("Themes\\*", "", ""), ("Themes\\Championship\\*.cse", "", ""), ("Themes\\More_Stories\\*.txt", "", "Hot")]):
        c.append((f"saves_{i}", synth_saves(), pat, req, exc))
    c.append(("many_105", synth_saves(105), "Many\\*.sve", "", ""))     # past the 100-slot table
    c.append(("many_130", synth_saves(130), "Many\\*.sve", "", "f05"))
    return c


def list_image(lib, case, files):
    img = bytearray(base_image(lib))
    put(lib, img, "g_textBuffer", bytes((7 * i + 3) % 251 for i in range(64)))   # old text the appends go after
    return bytes(img)


def list_blocks(pattern, req, exc):
    """the three strings in one block: pattern at +0, require at +0x200, exclude at +0x300"""
    b = bytearray(0x400)
    for off, s in ((0, pattern), (0x200, req), (0x300, exc)):
        b[off:off + len(s)] = s.encode("latin-1")
    return [(BLOCK_VA, bytes(b))], [BLOCK_VA, BLOCK_VA + 0x200, BLOCK_VA + 0x300]


def list_files(extra) -> E.Files:
    f = themes(extra)
    return f


def run_list_port(lib, case, mode=0):
    _, extra, pat, req, exc = case
    files = list_files(extra)
    blocks, args = list_blocks(pat, req, exc)
    lib.negctl("listMatchingFiles", mode)
    try:
        return port_run(lib, "listMatchingFiles", args, list_image(lib, case, files), files, blocks), files
    finally:
        lib.negctl_reset()


def run_list_oracle(lib, case):
    _, extra, pat, req, exc = case
    files = list_files(extra)
    listing = E.WinListing(files)
    try:
        model = E.CrtModel(files, listing)
        emu = E.make_emu(model)
        blocks, args = list_blocks(pat, req, exc)
        r = emu.run(ENTRY["listMatchingFiles"], args, list_image(lib, case, files), blocks=blocks)
    finally:
        listing.close()
    return r, model


@need_exe
@need_themes
@need_windows
def test_listing_equals_windows(portlib, tmp_path):
    """the port's listing (Memory and Dir) is Windows' FindFirstFileA's: same names, same order, '.' and '..'"""
    files = themes(synth_saves(7))
    listing = E.WinListing(files)
    try:
        for pat in ["Saved Games\\*.sve", "Saved Games\\*", "Saved Games\\?.sve", "Saved Games\\*.*", "Saved Games\\Sub\\*",
                    "Themes\\Firaxis\\*.chr", "Themes\\*", "Themes\\Standard\\*.txt", "Themes\\Nowhere\\*", "Many\\*.sve",
                    "Saved Games\\*.cse", "Saved Games\\m*5.sve", "Saved Games\\*s*"]:
            want = [n for n, _ in listing.find(pat)]
            # the unfiltered names through the port function (no filter strings; the fixed Shadow filters apply)
            expect = [n for n in want if "Shadow" not in n and "shadow" not in n]
            for mode in ("memory", "dir"):
                blocks, args = list_blocks(pat, "", "")
                img = bytearray(base_image(portlib))
                if mode == "dir":
                    root = tmp_path / "install"
                    for k, v in files.data.items():
                        p = root / files.spell[k]
                        p.parent.mkdir(parents=True, exist_ok=True)
                        p.write_bytes(v)
                    eax, out, code = port_run(portlib, "listMatchingFiles", args, bytes(img), files, blocks, files_root=root)
                else:
                    eax, out, code = port_run(portlib, "listMatchingFiles", args, bytes(img), files, blocks)
                assert code == 0 and eax == len(expect), (pat, mode, eax, expect)
                off, _ = objs(portlib)["g_fileNames"]
                got = [bytes(out[off + 0x64 * i:off + 0x64 * i + 0x64]).split(b"\0")[0].decode("latin-1")
                       for i in range(min(len(expect), 100))]
                assert got == expect[:100], (pat, mode)
    finally:
        listing.close()


@need_exe
@need_themes
@need_windows
@emu_oracle
@pytest.mark.parametrize("case", list_cases(), ids=[c[0] for c in list_cases()])
def test_list_oracle_equals_original(portlib, case):
    r, model = run_list_oracle(portlib, case)
    assert not r.err, r.err
    (eax, img, code), _ = run_list_port(portlib, case)
    assert code == 0 and eax == r.eax, (eax, r.eax)
    assert img == r.image, first_diff(portlib, img, r.image)


# ------------------------------------------------------------------------------------------------ loadGolferRoster
def _pro_lines(n, kinds="digits"):
    """n valid pro-golfer lines (name, body, skin, hat, shirt, pants, ten skill characters)"""
    out = []
    for i in range(n):
        skills = {"digits": "%010d" % (i * 7919 % 10 ** 10), "upper": "ABCDEFGHIJ"[::-1],
                  "lower": "klmnopqrst", "mixed": "9aZ!x0Q#b~", "short": "123"}[kinds]
        out.append("Pro%02d,%d,%d,%d,%d,%d,%s" % (i, i % 9, i % 5, i % 11, i % 10, (i * 3) % 12, skills))
    return out


def _celeb_lines(n):
    return ["Celeb%02d,%s,%d,%d,%d,%d" % (i, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"[i % 26], i % 5, i % 10, (i * 7) % 11, i % 10)
            for i in range(n)]


def _txt(lines, eol="\r\n", tail=b""):
    return (eol.join(lines) + eol).encode("latin-1") + tail


ROSTER_CASES = []   # (id, theme, extra files, flags, text seed)


def _add(case_id, theme="Standard", extra=None, flags=0x0, seed=1):
    ROSTER_CASES.append((case_id, theme, extra or {}, flags, seed))


def _pro_file(k: int = 0) -> bytes:
    """a .pro golfer file (the install's Golfmonkey.pro) with non-zero skill bytes (the shipped ones are all 0): variant k"""
    if not (INSTALL / "Themes").exists():
        return b""
    b = bytearray((INSTALL / "Themes" / "Firaxis" / "Golfmonkey.pro").read_bytes())
    for i in range(16):
        b[0x712 + i] = (i * 3 + k * 5) % 19 + 1
    return bytes(b)


_add("standard")
_add("standard_flags", flags=0x10000000 | 0x40, seed=2)
_add("firaxis", theme="Firaxis", extra={"Themes/Firaxis/Golfmonkey.pro": _pro_file(8)})                      # no celebrities / progolfers of its own: the standard theme's; one .pro
_add("more_stories", theme="More_Stories", seed=3)
_add("the_sims", theme="The_Sims", seed=4)
_add("championship", theme="Championship")                 # no story list: the fallback list and its flag bit; one .pro
_add("missing_theme", theme="NoSuchTheme", seed=5)
_add("no_lists", theme="Championship", extra={"Themes/Standard/GxxxxxxxOpeningDay.txt": None}, seed=6)
_add("celeb_variants", extra={"Themes/Standard/celebrities.dta": _txt(
    ["* header", "", "short", "Cx,A,1,1,1,1", "Cy,Z,3,9,9,9", "Cz,a,0,5,0,0", "Cw,L,7,0,0,0", "Ck,@,2,x,y,z"]
    + _celeb_lines(8) + ["*Cx,A,1,1,1,1"] + _celeb_lines(3))}, seed=7)
_add("celeb_130", extra={"Themes/Standard/celebrities.dta": _txt(_celeb_lines(130))}, seed=8)
_add("celeb_ctrlz", extra={"Themes/Standard/celebrities.dta": _txt(_celeb_lines(5), tail=b"\x1a" + _txt(_celeb_lines(3)))}, seed=9)
_add("celeb_lf", extra={"Themes/Standard/celebrities.dta": _txt(_celeb_lines(12), eol="\n")}, seed=10)
_add("celeb_no_eol", extra={"Themes/Standard/celebrities.dta": ("\r\n".join(_celeb_lines(6))).encode()}, seed=11)
_add("celeb_missing_field", extra={"Themes/Standard/celebrities.dta": _txt(["Cx,A,1,1,1"])}, seed=12)   # strtok NULL: fault
_add("pro_digits", extra={"Themes/Standard/progolfers.dta": _txt(_pro_lines(20, "digits"))}, seed=13)
_add("pro_upper", extra={"Themes/Standard/progolfers.dta": _txt(_pro_lines(9, "upper"))}, seed=14)
_add("pro_lower", extra={"Themes/Standard/progolfers.dta": _txt(_pro_lines(9, "lower"))}, seed=15)
_add("pro_mixed", extra={"Themes/Standard/progolfers.dta": _txt(_pro_lines(9, "mixed"))}, seed=16)
_add("pro_short_skills", extra={"Themes/Standard/progolfers.dta": _txt(_pro_lines(6, "short"))}, seed=17)
_add("pro_130", extra={"Themes/Standard/progolfers.dta": _txt(_pro_lines(130))}, seed=18)
_add("pro_99_and_pro_files", extra={"Themes/Standard/progolfers.dta": _txt(_pro_lines(99)),
                                    "Themes/Standard/A.pro": _pro_file(1), "Themes/Standard/B.pro": _pro_file(2)}, seed=19)
_add("pro_100_and_pro_files", extra={"Themes/Standard/progolfers.dta": _txt(_pro_lines(100)),
                                     "Themes/Standard/A.pro": _pro_file(3)}, seed=20)
_add("pro_missing_field", extra={"Themes/Standard/progolfers.dta": _txt(["Pa,1,2,3,4,5"])}, seed=21)
_add("three_pro_files", extra={"Themes/Standard/Z.pro": _pro_file(4), "Themes/Standard/Y.pro": _pro_file(5),
                               "Themes/Standard/X.pro": _pro_file(6)}, seed=22)
def _pro_negative() -> bytes:
    b = bytearray(_pro_file(7))
    b[0x23] = 0xF5     # GolferType look_23: high nibble as a SIGNED byte shifts to -1
    return bytes(b)


_add("pro_negative_look", extra={"Themes/Standard/Neg.pro": _pro_negative()}, seed=26)
_add("two_opening_days", extra={"Themes/Standard/FooxxxxxOpeningDay.txt": b"x\r\n",
                                "Themes/Standard/ZzzOpeningDay2.txt": b"y\r\n"}, seed=27)
_add("celeb_both_missing", extra={"Themes/Standard/celebrities.dta": None}, seed=23)
_add("pro_both_missing", extra={"Themes/Standard/progolfers.dta": None}, seed=24)
_add("firaxis_pro_files_flags", theme="Firaxis", flags=0xFFFFFFFF, seed=25)


def roster_image(lib, case) -> bytes:
    import random
    cid, theme, extra, flags, seed = case
    rng = random.Random(seed)
    img = bytearray(base_image(lib))
    put(lib, img, "g_theme", theme.encode() + b"\0" + bytes(rng.randrange(256) for _ in range(40)))
    put(lib, img, "g_flags", struct.pack("<I", (flags ^ (rng.getrandbits(32) & 0x0F0F0000)) & 0xFFFFFFFF))
    put(lib, img, "g_textBuffer", bytes(rng.randrange(1, 256) for _ in range(300)))
    put(lib, img, "g_namedStoryIndex", struct.pack("<i", rng.randrange(-5, 50)))
    put(lib, img, "g_loopCounter59b76c", struct.pack("<ii", rng.randrange(0, 0x48), rng.randrange(0, 0x48)))
    return bytes(img)


def run_roster_port(lib, case, mode=0):
    """((eax, image, code), recorded calls): scrollTextWindow is recorded, not run (module dialogs registers the real
    modal, which has its own tests)"""
    files = themes(case[2])
    calls: list = []
    lib.negctl("loadGolferRoster", mode)
    try:
        r = port_run(lib, "loadGolferRoster", [], roster_image(lib, case), files, record=("scrollTextWindow",),
                     calls_out=calls)
        return r, calls
    finally:
        lib.negctl_reset()


def run_roster_oracle(lib, case):
    files = themes(case[2])
    listing = E.WinListing(files)
    snaps = []
    try:
        model = E.CrtModel(files, listing)
        emu = E.make_emu(model, drops=DROPS)

        def scroll(uc, a, ecx):
            snaps.append((tuple(a), bytes(uc.mem_read(E.DATA_VA, E.VL.DATA_SIZE))))
            return 0
        emu.stub_py(SCROLL, 3, scroll, pops=False, name="scrollTextWindow")   # cdecl, three arguments (E.SAVE_PEERS)
        r = emu.run(ENTRY["loadGolferRoster"], [], roster_image(lib, case))
    finally:
        listing.close()
    return r, model, snaps


def roster_compare(lib, case, mode=0) -> str:
    """'' when the port equals the original: the same scrollTextWindow calls (arguments), then the same eax and data image
    (both arms' scrollTextWindow returns 0 and writes nothing; the image the original's stub saw at its first call is
    covered by the image after it, which the later steps rewrite only in the text buffer)"""
    r, model, snaps = run_roster_oracle(lib, case)
    (eax, img, code), calls = run_roster_port(lib, case, mode)
    if code != 0 or r.err:
        if code != 0 and r.err and not snaps:
            return ""    # both faulted (the original's unchecked strtok result)
        return f"port code {code}, original error {r.err!r}"
    want = [(E.fnv1a("scrollTextWindow"),) + tuple(a) + (0,) * (6 - len(a)) + (len(a),) for a, _ in snaps]
    if [tuple(c) for c in calls] != want:
        return f"scrollTextWindow calls: port {[tuple(hex(v) for v in c) for c in calls]} original {want}"
    if eax != r.eax:
        return f"eax port {eax} original {r.eax}"
    return first_diff(lib, img, r.image)


@need_exe
@need_themes
@need_windows
@emu_oracle
@pytest.mark.parametrize("case", ROSTER_CASES, ids=[c[0] for c in ROSTER_CASES])
def test_roster_oracle_equals_original(portlib, case):
    msg = roster_compare(portlib, case)
    assert msg == "", msg


# ------------------------------------------------------------------------------------------------ golden digests, controls
def _list_digests(lib, mode=0) -> dict:
    out = {}
    for c in list_cases():
        (eax, img, code), _ = run_list_port(lib, c, mode)
        out[c[0]] = digest(eax, code, img)
    return out


def _roster_digests(lib, mode=0) -> dict:
    out = {}
    for c in ROSTER_CASES:
        (eax, img, code), calls = run_roster_port(lib, c, mode)
        out[c[0]] = digest(eax, code, img, calls)
    return out


def _pinned(section):
    g = E.golden_load().get(section)
    if not g:
        pytest.skip(f"{section}: no pinned digests (SG_EMU_ORACLE=update)")
    if g["files"] != E.files_sha(themes(synth_saves())):
        pytest.skip(f"{section}: the install's Themes differ from the pinned run")
    return g


@need_exe
@need_themes
@need_windows
@pytest.mark.parametrize("section,fn,ids,compare", [
    ("listMatchingFiles", _list_digests, [c[0] for c in list_cases()],
     lambda lib, c: _list_cmp(lib, c)),
    ("loadGolferRoster", _roster_digests, [c[0] for c in ROSTER_CASES],
     lambda lib, c: roster_compare(lib, c)),
])
def test_golden_digests(portlib, section, fn, ids, compare):
    """the port reproduces the pinned results of the emulated original; SG_EMU_ORACLE=update checks every case against the
    oracle first, then pins"""
    if os.environ.get("SG_EMU_ORACLE") == "update":
        cases = list_cases() if section == "listMatchingFiles" else ROSTER_CASES
        bad = [f"{c[0]}: {m}" for c in cases for m in [compare(portlib, c)] if m]
        assert not bad, f"oracle RED: not pinned: {bad[:4]}"
        d = fn(portlib)
        E.golden_update(section, {"files": E.files_sha(themes(synth_saves())), "digests": d})
    g = _pinned(section)
    got = fn(portlib)
    bad = [k for k in ids if got[k] != g["digests"].get(k)]
    assert not bad, f"{len(bad)} cases differ from the pinned digests: {bad[:6]}"


def _list_cmp(lib, case) -> str:
    r, model = run_list_oracle(lib, case)
    if r.err:
        return r.err
    (eax, img, code), _ = run_list_port(lib, case)
    if code != 0 or eax != r.eax:
        return f"eax port {eax} (code {code}) original {r.eax}"
    return first_diff(lib, img, r.image)


NEGCTL = [
    ("listMatchingFiles", 1, "the exclude filter ignored"), ("listMatchingFiles", 2, "the Shadow filter ignored"),
    ("listMatchingFiles", 3, "no separator in the text buffer"), ("listMatchingFiles", 4, "require tested for absence"),
    ("loadGolferRoster", 1, "celebrity category % 10"), ("loadGolferRoster", 2, "lower-case skill letters from 'A'"),
    ("loadGolferRoster", 3, "the fallback list does not set the flag bit"),
    ("loadGolferRoster", 4, ".pro skills stored in the same record"),
    ("loadGolferRoster", 5, "story index of the last match"), ("loadGolferRoster", 6, "look_hi = b / 16"),
    ("loadGolferRoster", 7, "celebrity look_a accepts '4'"),
]


@need_exe
@need_themes
@need_windows
@pytest.mark.parametrize("fname,mode,what", NEGCTL)
def test_negctl_reads_red(portlib, fname, mode, what):
    """every wrong body changes at least one pinned digest"""
    g = _pinned(fname)
    got = (_list_digests if fname == "listMatchingFiles" else _roster_digests)(portlib, mode)
    changed = [k for k in got if got[k] != g["digests"][k]]
    assert changed, f"{fname} mode {mode} ({what}) left every pinned digest unchanged"
