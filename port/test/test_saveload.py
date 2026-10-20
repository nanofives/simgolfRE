"""Phase 3.8 stream 38b: saveGame, loadGame, loadCourse (port/src/sim/saveload.cpp).

Evidence:
  * a FORMAT check independent of the C3 sources the port was written from: the user's real saved games
    (original/Saved Games/*.sve) load into exactly the byte ranges re/model/sve_fields.tsv (the G2 table, 116 saves
    round-trip byte-identical through it) names, and saving the loaded state writes the body back byte for byte;
  * the emulator oracle (opt-in, SG_EMU_ORACLE=1; port/test/emu38b.py): the ORIGINAL functions run under Unicorn with the
    CRT's low-level file layer stood in by a dictionary, their drawing / terrain callees stubbed and logged (the port's
    recorded through commands.recordPeer); the port must equal eax, the data image, the file the call wrote and the
    callee sequence with its arguments. The default suite checks pinned digests of those results
    (port/test/golden_38b.json, SG_EMU_ORACLE=update after the oracle is GREEN) and every negative control against them.
"""
from __future__ import annotations

import csv
import ctypes as C
import hashlib
import os
import pathlib
import random
import struct

import pytest

from port.test import emu38b as E
from port.test import lib as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
INSTALL = ROOT / "original"
SAVES = INSTALL / "Saved Games"
PEER_LOG_VA, PEER_LOG_CAP = 0x32000000, 64
BLOCK_VA = 0x30000000
pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
need_saves = pytest.mark.skipif(not (SAVES / "mood5.sve").exists(), reason="the install's saved games are absent")
emu_oracle = pytest.mark.skipif(not E.have_oracle(), reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn")
ENTRY = {"saveGame": 0x0040B4A0, "loadGame": 0x0040B9B0, "loadCourse": 0x0040B840}
HEADER = 0x64
CHAMP = "Themes" + chr(92) + "Championship" + chr(92)   # the original's directory spelling


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


def first_diff(lib, a: bytes, b: bytes) -> str:
    if a == b:
        return ""
    i = next(i for i in range(len(a)) if a[i] != b[i])
    for n, (off, sz) in objs(lib).items():
        if off <= i < off + sz:
            return f"{n}+{i - off:#x}: port {a[i]:#04x} original {b[i]:#04x}"
    return f"byte {i:#x}"


def real_saves() -> dict:
    return {p.name: p.read_bytes() for p in sorted(SAVES.glob("*.sve"))} if SAVES.exists() else {}


def save_files(extra=None) -> E.Files:
    f = E.Files()
    for k, v in real_saves().items():
        f.put("Saved Games/" + k, v)
    for k, v in (extra or {}).items():
        f.put(k, v)
    return f


def port_run(lib, name, args, image: bytes, files: E.Files, root=None, record=True, text_at=None, unrecorded=()):
    """(eax or None, image, error code, recorded peer calls)"""
    logbuf = (C.c_uint32 * (1 + 8 * PEER_LOG_CAP))()
    prev_env = os.environ.get("SG_FILES_WRITE_THROUGH")
    if root is not None:
        os.environ["SG_FILES_WRITE_THROUGH"] = "1"
    try:
        return _port_run(lib, name, args, image, files, root, record, text_at, logbuf, unrecorded)
    finally:
        if prev_env is None:
            os.environ.pop("SG_FILES_WRITE_THROUGH", None)
        else:
            os.environ["SG_FILES_WRITE_THROUGH"] = prev_env


def _port_run(lib, name, args, image, files, root, record, text_at, logbuf, unrecorded=()):
    with lib.state() as st:
        st.set_data_base(E.DATA_VA)
        st.load_image(image)
        if root is not None:
            st.files_root(root)     # the tests that write read the file back from disk: write-through (gamefiles.h)
        else:
            for k, v in files.data.items():
                st.files_put(files.spell[k], v)
        blocks = []
        if text_at:
            blocks.append(text_at)
        keep = []
        for base, b in blocks:
            buf = (C.c_uint8 * len(b)).from_buffer_copy(bytes(b))
            keep.append(buf)
            st.map(base, buf, readonly=True)
        if record:
            st.map(PEER_LOG_VA, logbuf)
            for _, nm, _, _ in E.SAVE_PEERS:
                if nm in unrecorded:
                    continue
                st.call("commands.recordPeer", [E.fnv1a(nm), PEER_LOG_VA, PEER_LOG_CAP])
        try:
            eax, code = st.call(name, list(args)).eax, 0
        except L.SgError as e:
            eax, code = None, e.code
        if record:
            st.call("commands.recordClear", [])
        n = min(logbuf[0], PEER_LOG_CAP)
        calls = [tuple(logbuf[1 + 8 * k:1 + 8 * k + 8]) for k in range(n)]
        return eax, st.image(), code, calls


def name_block(s: str):
    b = s.encode("latin-1") + b"\0"
    return (BLOCK_VA, b)


# ------------------------------------------------------------------------------------------------ the format, from real saves
def sve_rows():
    rows = []
    for r in csv.DictReader(open(ROOT / "re" / "model" / "sve_fields.tsv"), delimiter="\t"):
        rows.append(r)
    return rows


def expected_ranges(file_bytes: bytes):
    """[(va, size, file bytes)] of the body fields of a save, from the G2 table (flag-gated tails included by the flags
    the file itself holds)"""
    rows = [r for r in sve_rows() if r["index"].isdigit()]
    out, pos = [], HEADER
    flags = None
    for r in rows:
        va, size = int(r["va"], 16), int(r["size"], 16)
        data = file_bytes[pos:pos + size]
        out.append((va, size, data))
        if va == 0x0059E7B8:
            flags = struct.unpack("<I", data)[0]
        pos += size
    return out, flags, pos


@need_exe
@need_saves
@pytest.mark.parametrize("name", sorted(real_saves()))
def test_real_saves_load_into_the_table_ranges(portlib, name):
    """loadGame puts every body field of a real save where the G2 table says (independent of the port's own table)"""
    files = save_files()
    img = bytearray(base_image(portlib))
    eax, out, code, calls = port_run(portlib, "loadGame", [BLOCK_VA, 0, 0, 0], bytes(img), files,
                                     text_at=name_block(name))
    assert code == 0
    raw = files.get("Saved Games/" + name)
    ranges, flags, end = expected_ranges(raw)
    state_off, _ = objs(portlib)["g_courseId"]
    base = state_off - (0x00822C78 - 0x004C1000)      # GameState offset of the image's first byte
    for va, size, data in ranges:
        if va == 0x0059E7B8:    # the flag word is rewritten after the read
            continue
        got = out[base + (va - 0x004C1000):base + (va - 0x004C1000) + size]
        assert bytes(got) == data, f"{name}: field at {va:#x}"
    # the flags: bits 2 and 3 cleared, bit 2 from the entry value (0 here), bit 30 set
    foff, _ = objs(portlib)["g_flags"]
    assert struct.unpack("<I", out[foff:foff + 4])[0] == ((flags & ~0xC) | 0x40000000) & 0xFFFFFFFF


@need_exe
@need_saves
@pytest.mark.parametrize("name", sorted(real_saves()))
def test_save_writes_the_loaded_body_back_byte_for_byte(portlib, name, tmp_path):
    """load a real save, restore its flag word, save it: the body of the new file equals the old one's"""
    root = tmp_path / "install"
    for k, v in real_saves().items():
        (root / "Saved Games").mkdir(parents=True, exist_ok=True)
        (root / "Saved Games" / k).write_bytes(v)
    raw = real_saves()[name]
    _, flags, end = expected_ranges(raw)
    img = bytearray(base_image(portlib))
    eax, loaded, code, _ = port_run(portlib, "loadGame", [BLOCK_VA, 0, 0, 0], bytes(img), E.Files(), root=root,
                                    text_at=name_block(name))
    assert code == 0
    img2 = bytearray(loaded)
    put(portlib, img2, "g_flags", struct.pack("<I", flags))
    eax, _, code, _ = port_run(portlib, "saveGame", [BLOCK_VA], bytes(img2), E.Files(), root=root,
                               text_at=name_block("roundtrip"))
    assert code == 0 and eax == 0
    new = (root / "Saved Games" / "roundtrip.sve").read_bytes()
    assert len(new) == len(raw) and new[HEADER:] == raw[HEADER:]
    assert new[:HEADER].split(b"\0")[0] != b""     # a header line was written


# ------------------------------------------------------------------------------------------------ oracle cases
def _bytes_of_cse() -> bytes:
    p = INSTALL / "Themes" / "Championship" / "Ocean's Edge MC.cse"
    return p.read_bytes() if p.exists() else b""


def case_image(lib, seed, course_type=None, flags=None, date=None) -> bytes:
    rng = random.Random(seed)
    img = bytearray(base_image(lib))
    put(lib, img, "g_courseType", struct.pack("<b", rng.choice([-1, 0, 1, 2, 3, 4]) if course_type is None else course_type))
    put(lib, img, "g_flags", struct.pack("<I", rng.getrandbits(32) & 0x0F0FFFFF if flags is None else flags))
    put(lib, img, "g_date", struct.pack("<i", rng.randrange(0, 0x30000) if date is None else date))
    put(lib, img, "g_holesPlusOne", struct.pack("<i", rng.randrange(1, 20)))
    put(lib, img, "g_textBuffer", bytes(rng.randrange(1, 256) for _ in range(300)))
    put(lib, img, "g_saveFd", struct.pack("<i", rng.choice([-1, 0, 3, 4, 9])))
    put(lib, img, "g_panelListMode", struct.pack("<i", rng.randrange(0, 5)))
    put(lib, img, "f_w_4c2848", struct.pack("<iiii", *[rng.randrange(0, 9) for _ in range(4)]))
    return bytes(img)


def _cases():
    """(id, op, args as (name or None, ints...), files, image kwargs, text path for loadCourse)"""
    saves = real_saves()
    c = []
    trunc = {"Saved Games/trunc.sve": saves.get("mood5.sve", b"")[:5000]} if saves else {}
    shortf = {"Saved Games/tiny.sve": b"abc"}
    cse = {"Themes/Championship/Ocean's Edge MC.cse": _bytes_of_cse()}
    c.append(("load_mood5", "loadGame", ("mood5.sve", 0, 0, 0), {}, {"seed": 1}, None))
    c.append(("load_mood5_finish", "loadGame", ("mood5.sve", 1, 0, 0), {}, {"seed": 2}, None))
    c.append(("load_stories9_finish_type_same", "loadGame", ("stories9.sve", 1, 0, 0), {}, {"seed": 3, "course_type": 0}, None))
    c.append(("load_autosave_header_only", "loadGame", ("&AutoSave1.sve", 0, 1, 0), {}, {"seed": 4}, None))
    c.append(("load_browsing_finish", "loadGame", ("While_Browsing.sve", 1, 0, 0),
              {"Saved Games/While_Browsing.sve": saves.get("stories5.sve", b"")}, {"seed": 5}, None))
    c.append(("load_missing", "loadGame", ("nosuch.sve", 1, 0, 0), {}, {"seed": 6}, None))
    c.append(("load_missing_header_only", "loadGame", ("nosuch.sve", 0, 1, 0), {}, {"seed": 7}, None))
    c.append(("load_truncated", "loadGame", ("trunc.sve", 1, 0, 0), trunc, {"seed": 8}, None))
    c.append(("load_tiny", "loadGame", ("tiny.sve", 0, 0, 0), shortf, {"seed": 9}, None))
    c.append(("load_flags_bit2", "loadGame", ("mood5.sve", 1, 0, 0), {}, {"seed": 10, "flags": 0x4 | 0x400000}, None))
    c.append(("load_flags_all", "loadGame", ("stories5.sve", 1, 0, 0), {}, {"seed": 11, "flags": 0xFFFFFFFF}, None))
    c.append(("load_type_neg", "loadGame", ("stories5.sve", 1, 0, 0), {}, {"seed": 12, "course_type": -1}, None))
    c.append(("load_champ_header2", "loadGame", ("Ocean's Edge MC.cse", 0, 0, 1), cse, {"seed": 13}, None))
    for mode in (0, 1, 2, 3):
        c.append((f"course_mode{mode}", "loadCourse", (mode,), cse, {"seed": 20 + mode},
                  CHAMP + "Ocean's Edge MC.cse"))
    c.append(("course_missing", "loadCourse", (0,), cse, {"seed": 30}, CHAMP + "nope.cse"))
    c.append(("course_truncated", "loadCourse", (1,), {"Themes/Championship/short.cse": _bytes_of_cse()[:3000]},
              {"seed": 31}, CHAMP + "short.cse"))
    for i, date in enumerate([0, 1, 1023, 1024, 0x2ABCD, 5000, 0x7FFFFC00, -1, -5000, 0x800003FF - (1 << 32), 8191]):
        c.append((f"save_date{i}", "saveGame", ("slot",), {}, {"seed": 40 + i, "date": date}, None))
    c.append(("save_named_dir", "saveGame", ("My Course 2",), {}, {"seed": 60}, None))
    return c


def _setup(lib, case):
    cid, op, args, extra, kw, text = case
    files = save_files(extra)
    img = bytearray(case_image(lib, **kw))
    blocks = None
    iargs = []
    if op == "loadCourse":
        s = text.encode("latin-1") + b"\0"
        put(lib, img, "g_textBuffer", s)
        iargs = list(args)
    else:
        blocks = name_block(args[0])
        iargs = [BLOCK_VA] + [a & 0xFFFFFFFF for a in args[1:]]
    return files, bytes(img), iargs, blocks


def run_oracle(lib, case):
    cid, op, args, extra, kw, text = case
    files, img, iargs, blocks = _setup(lib, case)
    io = E.LowIo(files)
    emu = E.make_save_emu(io)
    r = emu.run(ENTRY[op], iargs, img, blocks=[(blocks[0], blocks[1])] if blocks else ())
    return r, io


def run_port(lib, case, mode=0, tmp=None):
    cid, op, args, extra, kw, text = case
    files, img, iargs, blocks = _setup(lib, case)
    lib.negctl(op, mode)
    try:
        if op == "saveGame":
            import tempfile
            with tempfile.TemporaryDirectory(prefix="sg38b_save_") as d:
                root = pathlib.Path(d)
                eax, out, code, calls = port_run(lib, op, iargs, img, files, root=root, text_at=blocks)
                written = {E.norm(p.relative_to(root).as_posix()): p.read_bytes() for p in root.rglob("*") if p.is_file()}
        else:
            eax, out, code, calls = port_run(lib, op, iargs, img, files, text_at=blocks)
            written = {}
    finally:
        lib.negctl_reset()
    return eax, out, code, calls, written


def compare(lib, case, mode=0) -> str:
    r, io = run_oracle(lib, case)
    eax, out, code, calls, written = run_port(lib, case, mode)
    if r.err:
        return f"original: {r.err}"
    if code != 0:
        return f"port code {code}"
    if eax != r.eax:
        return f"eax port {eax} original {r.eax}"
    if out != r.image:
        return first_diff(lib, out, r.image)
    if calls != io.peer_calls:
        return f"callee calls differ: port {[(hex(c[0]), c[1:1 + c[7]]) for c in calls]} original {[(hex(c[0]), c[1:1 + c[7]]) for c in io.peer_calls]}"
    if case[1] == "saveGame":
        if set(written) != set(io.written):
            return f"files written: port {sorted(written)} original {sorted(io.written)}"
        for k in written:
            if written[k] != io.written[k]:
                n = next(i for i in range(min(len(written[k]), len(io.written[k]))) if written[k][i] != io.written[k][i]) \
                    if written[k][:min(len(written[k]), len(io.written[k]))] != io.written[k][:min(len(written[k]), len(io.written[k]))] else min(len(written[k]), len(io.written[k]))
                return f"file {k} differs at byte {n} (sizes {len(written[k])} / {len(io.written[k])})"
    return ""


CASES = _cases() if SAVES.exists() else []


@need_exe
@need_saves
@emu_oracle
@pytest.mark.parametrize("case", CASES, ids=[c[0] for c in CASES])
def test_oracle_equals_original(portlib, case):
    msg = compare(portlib, case)
    assert msg == "", msg


# ------------------------------------------------------------------------------------------------ golden digests, controls
def _digest(eax, code, image, calls, written) -> str:
    h = hashlib.sha256()
    h.update(b"err%d" % code if eax is None else struct.pack("<I", eax & 0xFFFFFFFF))
    h.update(image)
    h.update(repr(calls).encode())
    for k in sorted(written):
        h.update(k.encode())
        h.update(written[k])
    return h.hexdigest()[:24]


def _digests(lib, mode_for=None) -> dict:
    out = {}
    for c in CASES:
        mode = mode_for(c[1]) if mode_for else 0
        eax, img, code, calls, written = run_port(lib, c, mode)
        out[c[0]] = _digest(eax, code, img, calls, written)
    return out


def _files_sha() -> str:
    h = hashlib.sha256()
    for k, v in sorted(real_saves().items()):
        h.update(k.encode())
        h.update(hashlib.sha256(v).digest())
    h.update(hashlib.sha256(_bytes_of_cse()).digest())
    return h.hexdigest()[:16]


@need_exe
@need_saves
def test_golden_digests(portlib):
    """the port reproduces the pinned results of the emulated original; SG_EMU_ORACLE=update checks every case against the
    oracle first, then pins"""
    if os.environ.get("SG_EMU_ORACLE") == "update":
        bad = [f"{c[0]}: {m}" for c in CASES for m in [compare(portlib, c)] if m]
        assert not bad, f"oracle RED: not pinned: {bad[:4]}"
        E.golden_update("saveload", {"files": _files_sha(), "digests": _digests(portlib)})
    g = E.golden_load().get("saveload")
    if not g:
        pytest.skip("saveload: no pinned digests (SG_EMU_ORACLE=update)")
    if g["files"] != _files_sha():
        pytest.skip("saveload: the install's saved games differ from the pinned run")
    got = _digests(portlib)
    bad = [k for k in got if got[k] != g["digests"].get(k)]
    assert not bad, f"{len(bad)} cases differ from the pinned digests: {bad[:6]}"


NEGCTL = [
    ("saveGame", 1, "the date is not appended"), ("saveGame", 2, "no .sve extension"), ("saveGame", 3, "the header is 0x63 bytes"),
    ("loadGame", 1, "bit 2 of the entry flags not kept"), ("loadGame", 2, "bit 30 not set"),
    ("loadGame", 3, "header-only still reads the body"), ("loadGame", 4, "the browsing save shows the loading message"),
    ("loadGame", 5, "world assets reload on every finish"), ("loadGame", 7, "a championship file reads one header"),
    ("loadGame", 8, "the body is read with g_saveIoRead 0"), ("loadGame", 9, "the third body field is skipped"),
    ("loadCourse", 1, "mode 3 reads the body"), ("loadCourse", 2, "mode 0 does not rebuild the heightfield"),
    ("loadCourse", 3, "the two slots keep their value"),
]


@need_exe
@need_saves
@pytest.mark.parametrize("fname,mode,what", NEGCTL)
def test_negctl_reads_red(portlib, fname, mode, what):
    """every wrong body changes at least one pinned digest"""
    g = E.golden_load().get("saveload")
    if not g or g["files"] != _files_sha():
        pytest.skip("saveload: no pinned digests for these files")
    got = _digests(portlib, lambda op: mode if op == fname else 0)
    changed = [k for k in got if got[k] != g["digests"][k]]
    assert changed, f"{fname} mode {mode} ({what}) left every pinned digest unchanged"
    assert all(k.startswith(("load_", "course_", "save_")) for k in changed)


# ------------------------------------------------------------------------------------------------ refreshTerrain
A_RESET_TERRAIN = 0x004A4F34     # Terrain::resetTerrain's thunk (thiscall, no argument), a Terrain.dll call


def refresh_image(lib, seed):
    rng = random.Random(seed)
    img = bytearray(base_image(lib))
    put(lib, img, "g_terrain820f2a", bytes(rng.choice([0, 1, 2, 7]) for _ in range(3)))
    put(lib, img, "gTerrain", struct.pack("<I", 0x31000000 + rng.randrange(0x100)))
    return bytes(img)


def compare_refresh(lib, seed, mode=0):
    io = E.LowIo(E.Files())
    emu = E.add_save_stubs(E.CE.Emu(), io, [p for p in E.SAVE_PEERS if p[1] != "refreshTerrain"])   # the function under test runs
    emu.stub_py(A_RESET_TERRAIN, 0, lambda uc, a, ecx: io.peer_calls.append((E.fnv1a("resetTerrain"), ecx, 0, 0, 0, 0, 0, 1)) or 0,
                name="resetTerrain")
    img = refresh_image(lib, seed)
    r = emu.run(0x00449520, [], img)
    lib.negctl("refreshTerrain", mode)
    try:
        eax, out, code, calls = port_run(lib, "refreshTerrain", [], img, E.Files(), unrecorded=("refreshTerrain",))
    finally:
        lib.negctl_reset()
    if r.err or code != 0:
        return f"original {r.err!r}, port code {code}"
    if eax != r.eax or out != r.image:
        return f"eax {eax} / {r.eax}" if eax != r.eax else first_diff(lib, out, r.image)
    # the original also calls Terrain::resetTerrain first (a Terrain.dll call, not modelled by the port): the rest must match
    rest = [c for c in io.peer_calls if c[0] != E.fnv1a("resetTerrain")]
    return "" if calls == rest else f"calls port {calls} original {rest}"


@need_exe
@emu_oracle
@pytest.mark.parametrize("seed", range(6))
def test_refresh_terrain_oracle(portlib, seed):
    msg = compare_refresh(portlib, seed)
    assert msg == "", msg


def _refresh_flag(lib, seed, mode):
    """the byte g_terrain820f2a[0] after refreshTerrain (rebuildTerrainGrid recorded)"""
    logbuf = (C.c_uint32 * 40)()
    with lib.state() as st:
        st.set_data_base(E.DATA_VA)
        st.load_image(refresh_image(lib, seed))
        st.map(PEER_LOG_VA, logbuf)
        st.call("commands.recordPeer", [E.fnv1a("rebuildTerrainGrid"), PEER_LOG_VA, 4])
        lib.negctl("refreshTerrain", mode)
        try:
            st.call("refreshTerrain", [])
        finally:
            lib.negctl_reset()
            st.call("commands.recordClear", [])
        off, _ = objs(lib)["g_terrain820f2a"]
        return st.read(off, 1)[0], refresh_image(lib, seed)[off]


@need_exe
def test_refresh_terrain_control_reads_red(portlib):
    """mode 1 leaves the repaint flag byte as it was; the correct body leaves 1 (and the oracle test pins the rest)"""
    for seed in range(6):
        assert _refresh_flag(portlib, seed, 0)[0] == 1
        flag, was = _refresh_flag(portlib, seed, 1)
        assert flag == was
    assert any(_refresh_flag(portlib, seed, 1)[1] != 1 for seed in range(6))     # some seed starts with another value
