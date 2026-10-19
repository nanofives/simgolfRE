"""Phase 3.1b course module: negative controls of every ported function and state tests for the functions the vector
corpus does not cover (static initialisers, file readers over GameFiles, the heap list eraser, the course switch).
The vectors themselves run in test_vectors.py (test_function_green_on_vectors) like every registered function."""
from __future__ import annotations

import pathlib
import struct
import sys

import pytest

from port.test import lib as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

pytestmark = pytest.mark.port
need_corpus = pytest.mark.skipif(not VL.VEC_DIR.exists(), reason="vector corpus not present (port/corpus/vectors, local)")
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
SEGMENTS = ROOT / "port" / "corpus" / "segments"
BLOCK = 0x10000000          # where the tests map their own blocks (outside the data section)

# (function, mode, what): wrong bodies the vectors must see (RED)
NEGCTL_RED = [
    ("freeRecordAtPos", 1, "frees only the first match"), ("freeRecordAtPos", 2, "y compared with x"),
    ("initRecords", 1, "99 animal slots"), ("initRecords", 2, "placeRecord type 0"),
    ("clearMatching", 2, "stores -1"),
    ("appendString", 1, "offset -1 not tested"),
    ("landmarkName", 1, "long / short swapped"), ("landmarkName", 2, "types 17 / 18 swapped"),
    ("buildGolferName", 1, "no title"), ("buildGolferName", 2, "class 0x80 parity inverted"),
    ("buildGolferName", 3, "class 0x40 suffix from the sub-kind"),
    ("buildStatusText", 1, "equal priority returns early"), ("buildStatusText", 2, "saved text not appended"),
    ("appendHoleName", 1, "par 3 / par 4 tables swapped"),
    ("appendCourseTitle", 1, "full == 1 takes the short suffix"),
    ("nearestHole", 1, "ties take the later hole"), ("nearestHole", 2, "pin point ignored"),
    ("holeQuadrants", 1, "threshold / 2"),
    ("seedCourseTable", 1, "records 12..15 never redraw"), ("seedCourseTable", 2, "setting 2 subtracts 10"),
    ("buildScenarioIntro", 1, "g_messageTimer not set"), ("buildScenarioIntro", 2, "cash / 100"),
]
# sceneryLabel calls 3.1a's decorationName through the registry: its vectors run only when that is registered
NEGCTL_RED_SCENERY = [("sceneryLabel", 1, "text == 0 match returns 0"), ("sceneryLabel", 2, "no separator")]
# wrong bodies the vectors CANNOT see (recorded blind spots: state tests below or the segments must cover them)
NEGCTL_BLIND = [
    ("clearMatching", 1, "b not compared: every seed's g_labelQueueB is 0, so a wrong clear writes 0 over 0"),
    ("buildScenarioIntro", 3, "startMessage priority 0: no vector has a message pending"),
    ("listEraseRange", 1, "size not decremented: the 2 reproduced vectors erase an empty range (state test reads it)"),
    ("listEraseRange", 2, "*out = first: the 2 reproduced vectors have first == last (state test reads it)"),
]
COURSE = ["freeRecordAtPos", "initRecords", "clearMatching", "staticInit_820f04", "setPendingCourseInfo",
          "clearBuffers", "appendString", "landmarkName", "buildGolferName", "buildStatusText", "appendHoleName",
          "appendCourseTitle", "nearestHole", "holeQuadrants", "sceneryLabel", "fileExists", "lookupThemeText",
          "listEraseRange", "loadThemeFile", "loadThemes", "switchToCourse", "seedCourseTable", "buildScenarioIntro"]


def _objs(lib):
    return {n: (o, s) for n, o, s in lib.objects()}


def _va(st, objs, name, off=0):
    return st.data_base + objs[name][0] + off


@pytest.mark.parametrize("function,mode,what", NEGCTL_RED)
@need_corpus
def test_negctl_reads_red(portlib, vrunner, function, mode, what):
    assert portlib.negctl(function, mode) == 0
    r = vrunner.run_function(function)
    assert r.verdict == "RED" and r.failed > 0, f"{function} mode {mode} ({what}) read {r.summary()}"
    portlib.negctl_reset()
    assert vrunner.run_function(function).verdict == "GREEN"


@pytest.mark.parametrize("function,mode,what", NEGCTL_RED_SCENERY)
@need_corpus
def test_negctl_scenery_reads_red(portlib, vrunner, function, mode, what):
    if not portlib.has("decorationName"):
        pytest.skip("decorationName (3.1a) is not registered in this build")
    portlib.negctl(function, mode)
    r = vrunner.run_function(function)
    assert r.verdict == "RED" and r.failed > 0, f"{function} mode {mode} ({what}) read {r.summary()}"


@pytest.mark.parametrize("function,mode,what", NEGCTL_BLIND)
@need_corpus
def test_negctl_blind_spots_are_known(portlib, vrunner, function, mode, what):
    """A blind spot that starts reading RED means the corpus grew: move it to NEGCTL_RED."""
    portlib.negctl(function, mode)
    assert vrunner.run_function(function).verdict == "GREEN", f"{function} mode {mode} ({what}) is no longer blind"


def test_every_course_function_has_a_negctl(portlib):
    funcs = portlib.functions()
    for f in COURSE:
        assert f in funcs, f
        assert portlib.negctl(f, 0) == 0, f


# ------------------------------------------------------------------------------------------- clearMatching
@pytest.mark.parametrize("mode", [0, 1])
def test_clear_matching_compares_b(portlib, mode):
    """The blind spot of the vectors: with non-zero g_labelQueueB only slots matching a AND b are cleared."""
    objs = _objs(portlib)
    portlib.negctl("clearMatching", mode)
    with portlib.state() as st:
        st.write(objs["g_labelQueueC"][0], struct.pack("<8i", 0, 1, 2, 0, 1, 2, 0, 1))
        st.write(objs["g_floatLabelY"][0], struct.pack("<8i", 0, 1, 0, 1, 0, 1, 0, 1))
        st.write(objs["g_labelQueueB"][0], struct.pack("<9i", *range(10, 19)))
        st.call("clearMatching", [0, 1])
        got = struct.unpack("<9i", st.read(objs["g_labelQueueB"][0], 36))
    want = (10, 11, 12, 0, 14, 15, 16, 17, 18) if mode == 0 else (0, 11, 12, 0, 14, 15, 0, 17, 18)
    assert got == want


# ------------------------------------------------------------------------------------------- static initialiser
@need_exe
@pytest.mark.parametrize("mode", [0, 1])
def test_static_init_820f04(portlib, mode):
    """No vector (a static initialiser): static_820f04 = __ftol(fabs(f_k_dbl_4ba620)); the original's value is in every
    segment start image; nothing else changes."""
    assert portlib.exe_load() == ""
    k = struct.unpack("<d", portlib.exe_const("f_k_dbl_4ba620"))[0]
    want = int(abs(k)) if mode == 0 else int(k)
    off, _ = _objs(portlib)["static_820f04"]
    imgs = sorted(SEGMENTS.glob("*/*/start.data.bin"))
    for p in imgs:
        with open(p, "rb") as f:
            f.seek(off)
            assert struct.unpack("<i", f.read(4))[0] == int(abs(k)), p
    portlib.negctl("staticInit_820f04", mode)
    with portlib.state() as st:
        before = bytearray(st.image())
        st.call("staticInit_820f04", [])
        after = bytearray(st.image())
    before[off:off + 4] = struct.pack("<i", want)
    assert after == before
    assert (want == int(abs(k))) == (mode == 0)


# ------------------------------------------------------------------------------------------- setPendingCourseInfo
@pytest.mark.parametrize("mode", [0, 1])
def test_set_pending_course_info(portlib, mode):
    objs = _objs(portlib)
    portlib.negctl("setPendingCourseInfo", mode)
    text = b"Pebble Bay 18 holes\0"
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.map(BLOCK, bytearray(text + b"\0" * 12))
        off, size = objs["g_pendingCourseInfo"]
        st.write(off, b"old\0")
        before = bytearray(st.image())
        r = st.call("setPendingCourseInfo", [BLOCK])
        after = bytearray(st.image())
    before[off:off + len(text)] = text
    assert (after == before and r.eax == len(text)) == (mode == 0)


# ------------------------------------------------------------------------------------------- clearBuffers
@pytest.mark.parametrize("mode", [0, 1])
def test_clear_buffers(portlib, mode):
    """clearBuffers (C2, held at C3: no vector): g_namePool zeroed, g_namePoolOffset all 0xff; nothing else."""
    objs = _objs(portlib)
    portlib.negctl("clearBuffers", mode)
    with portlib.state() as st:
        po, ps = objs["g_namePool"]
        oo, os_ = objs["g_namePoolOffset"]
        st.write(po, bytes((i * 7 + 1) & 0xFF for i in range(ps)))
        st.write(oo, bytes(range(os_)))
        before = bytearray(st.image())
        st.call("clearBuffers", [])
        after = bytearray(st.image())
    before[po:po + ps] = b"\0" * ps
    before[oo:oo + os_] = b"\xff" * os_
    assert (after == before) == (mode == 0)


# ------------------------------------------------------------------------------------------- listEraseRange
@pytest.mark.parametrize("mode", [0, 1, 2])
def test_list_erase_range(portlib, mode):
    """A std::list of head + 5 nodes in a block; erase(n1, n4) unlinks n1..n3, size 5 -> 2, *out = n4, returns out."""
    portlib.negctl("listEraseRange", mode)
    node = lambda i: BLOCK + 0x100 + 0x10 * i       # noqa: E731 - node i (0 = head)
    blk = bytearray(0x200)
    for i in range(6):
        struct.pack_into("<II", blk, 0x100 + 0x10 * i, node((i + 1) % 6), node((i - 1) % 6))
    struct.pack_into("<III", blk, 0, 0, node(0), 5)          # list object: allocator, head, size
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        buf = st.map(BLOCK, blk)
        r = st.call("listEraseRange", [BLOCK, BLOCK + 0x40, node(1), node(4)])
        out = bytes(buf)
    nxt = lambda i: struct.unpack_from("<I", out, 0x100 + 0x10 * i)[0]     # noqa: E731
    prv = lambda i: struct.unpack_from("<I", out, 0x104 + 0x10 * i)[0]     # noqa: E731
    ok = (r.eax == BLOCK + 0x40 and struct.unpack_from("<I", out, 0x40)[0] == node(4)
          and struct.unpack_from("<I", out, 8)[0] == 2 and nxt(0) == node(4) and prv(4) == node(0))
    assert ok == (mode == 0)


# ------------------------------------------------------------------------------------------- game files
def _str_block(st, *strings):
    """Maps the strings one after another at BLOCK; returns their addresses."""
    data, addrs = bytearray(), []
    for s in strings:
        addrs.append(BLOCK + len(data))
        data += s + b"\0"
    st.map(BLOCK, data + b"\0" * 16)
    return addrs


@pytest.mark.parametrize("mode", [0, 1])
def test_file_exists(portlib, mode):
    portlib.negctl("fileExists", mode)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        a, b = _str_block(st, b"Saves\\Course1.sve", b"Saves\\nothing.sve")
        st.files_put("saves/course1.SVE", b"x")
        assert st.call("fileExists", [a]).eax == (1 if mode == 0 else 0)
        assert st.call("fileExists", [b]).eax == (0 if mode == 0 else 1)


def _theme_oracle(raw: bytes, key: bytes):
    """lookupThemeText from its specification (re/analysis/course/0045b660), independently of the port: CRT text mode,
    fgets(0xfa), last character cut, _stricmp. Returns (result, appended text)."""
    data = bytearray()
    i = 0
    while i < len(raw):
        c = raw[i]
        if c == 0x1A:
            break
        if c == 0x0D and i + 1 < len(raw) and raw[i + 1] == 0x0A:
            i += 1
            continue
        data.append(c)
        i += 1
    pos = 0
    buf = bytearray(0x100)

    def fgets():
        nonlocal pos
        if pos >= len(data):
            return
        k = 0
        while k < 0xF9 and pos < len(data):
            c = data[pos]
            pos += 1
            buf[k] = c
            k += 1
            if c == 0x0A:
                break
        buf[k] = 0

    def cut():
        n = buf.index(0)
        if n:
            buf[n - 1] = 0

    def cur():
        return bytes(buf[:buf.index(0)])
    lines = 0
    while True:
        fgets()
        cut()
        if cur().lower() == key.lower():
            break
        lines += 1
        if lines >= 100:
            return -1, b""
    out, lines, counter = b"", 0, 0
    fgets()
    cut()
    while True:
        out += cur() + b"\n"
        fgets()
        cut()
        lines += 1
        if buf[0] == 0x2A:
            break
        counter += 1
        if counter >= 100:
            break
    return lines, out


THEME = (b"[intro]\r\nignored\r\n[Story]\r\nFirst line\r\nSecond\rline\r\n" + b"X" * 300 + b"\r\n*end\r\n"
         b"[last]\r\nonly\r\nlines\r\n")
THEME_CASES = [(b"[story]", THEME), (b"[INTRO]", THEME), (b"[last]", THEME), (b"[none]", THEME),
               (b"", b"a\nb\n"), (b"[k]", b"[k]\nx\x1ay\n*\n"), (b"[k]", b"[k]\n" + b"l\n" * 150)]


@pytest.mark.parametrize("case", range(len(THEME_CASES)))
def test_lookup_theme_text(portlib, case):
    key, raw = THEME_CASES[case]
    objs = _objs(portlib)
    sep = None
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        sep_off = objs["f_strpool_4c3f70"][0]
        st.write(sep_off, b" \0")               # the one-byte separator (the exe's value: a space)
        path, k = _str_block(st, b"Themes\\Standard\\Intro.txt", key)
        st.files_put("themes/standard/intro.txt", raw)
        to, ts = objs["g_textBuffer"]
        st.write(to, b"pre:\0")
        r = st.call("lookupThemeText", [path, k])
        text = st.read(to, ts).split(b"\0")[0]
        sep = st.read(sep_off, 1)
    want, out = _theme_oracle(raw, key)
    assert struct.unpack("<i", struct.pack("<I", r.eax))[0] == want
    assert text == b"pre:" + out.replace(b"\n", sep)


@need_exe
def test_lookup_theme_text_separator_is_the_exe_space(portlib):
    assert portlib.exe_load() == ""
    assert portlib.exe_const("f_strpool_4c3f70")[:2] == b" \0"


@pytest.mark.parametrize("mode", [1, 2])
def test_lookup_theme_text_negctl(portlib, mode):
    portlib.negctl("lookupThemeText", mode)
    objs = _objs(portlib)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(objs["f_strpool_4c3f70"][0], b" \0")
        path, k = _str_block(st, b"t.txt", b"[story]")
        st.files_put("t.txt", THEME)
        st.call("lookupThemeText", [path, k])
        text = st.read(objs["g_textBuffer"][0], 0x1000).split(b"\0")[0]
    assert text != _theme_oracle(THEME, b"[story]")[1].replace(b"\n", b" ")


def test_lookup_theme_text_missing_file_faults(portlib):
    """The original never tests fopen's NULL: its first fgets faults."""
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        path, k = _str_block(st, b"none.txt", b"k")
        st.files_put("other.txt", b"")
        with pytest.raises(L.SgError) as e:
            st.call("lookupThemeText", [path, k])
        assert e.value.code == L.SG_E_FAULT


# ------------------------------------------------------------------------------------------- loadThemeFile
def _chr_file(demo: int, body: int, skills: bytes) -> bytes:
    rec = bytearray(0x230)
    for i in range(0x230):
        rec[i] = (i * 3 + 5) & 0xFF
    rec[0x21], rec[0x22] = demo, body
    phrases = bytes((i * 11 + 2) & 0xFF for i in range(0x4E2))
    return bytes(rec) + phrases + skills + b"\xee" * 8 + b"PCX image bytes"


@pytest.mark.parametrize("mode", [0, 1, 2])
@pytest.mark.parametrize("index,slot,flags,demo,body,used", [
    (3, 7, 0, 0x00, 0x14, 5),         # women's tile (bit 7 clear -> k = 1), body takes the counter
    (3, -1, 0, 0x80, 0x13, 5),        # body below 0x14: untouched; slot -1: no skill copy
    (0, 2, 0x4000000, 0x80, 0x20, 0x48),   # index 0 with the flag: skills + g_playerSkills; counter full
    (0, 2, 0, 0x80, 0x20, 1),         # index 0 without the flag: no skill copy
    (0x4D, 4, 0, 0x00, 0x15, 0),      # index > 0x4c: the Standard theme directory
])
def test_load_theme_file(portlib, mode, index, slot, flags, demo, body, used):
    objs = _objs(portlib)
    portlib.negctl("loadThemeFile", mode)
    skills = bytes(range(0x40, 0x50))
    data = _chr_file(demo, body, skills)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(objs["f_strpool_4c84e8"][0], b"Themes\\\0rb\0\0Themes\\Standard\\\0")   # +0, +8 mode, +0xc
        st.write(objs["f_strpool_4c84e4"][0], b"\\\0")
        st.write(objs["g_theme"][0], b"Gold\0")
        st.write(objs["g_flags"][0], struct.pack("<I", flags))
        st.write(objs["g_loopCounter59b76c"][0], struct.pack("<2i", used, used + 10))
        (name,) = _str_block(st, b"Pro.chr")
        st.files_put("Themes/Standard/Pro.chr" if index > 0x4C else "Themes/Gold/Pro.chr", data)
        before = bytearray(st.image())
        r = st.call("loadThemeFile", [name, index, slot])
        after = bytearray(st.image())
        missing = st.call("loadThemeFile", [BLOCK + 3, index, slot]).eax     # "chr": no such file
    want = before
    t = objs["g_golferTypes"][0] + 0x230 * index
    want[t:t + 0x230] = data[:0x230]
    p = objs["g_golferPhrases"][0] + 0x4E2 * index
    want[p:p + 0x4E2] = data[0x230:0x230 + 0x4E2]
    if slot != -1 and (index != 0 or flags & 0x4000000):
        g = objs["g_golfers"][0] + 0x100 * slot + 0xF0
        want[g:g + 16] = skills
        if index == 0:
            o = objs["g_playerSkills"][0]
            want[o:o + 16] = skills
    k = 0 if demo & 0x80 else 1
    cnt = used if k == 0 else used + 10
    if body >= 0x14 and cnt < 0x48:
        want[t + 0x22] = cnt & 0xFF
        c = objs["g_loopCounter59b76c"][0] + 4 * k
        want[c:c + 4] = struct.pack("<i", cnt + 1)
    tb = objs["g_textBuffer"][0]
    path = (b"Themes\\Standard\\" if index > 0x4C else b"Themes\\Gold\\") + b"Pro.chr\0"
    want[tb:tb + len(path)] = path
    assert r.eax == 1 and missing == 0
    differs = ((mode == 1 and body == 0x13 and cnt < 0x48)
               or (mode == 2 and index == 0 and slot != -1 and flags & 0x4000000))
    assert (after == want) == (not differs)


def test_load_themes_needs_its_peers(portlib):
    """loadThemes calls 3.3's loadGolferRoster and listMatchingFiles through the registry."""
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        if portlib.has("loadGolferRoster") and portlib.has("listMatchingFiles"):
            pytest.skip("both peers registered: covered by the segments")
        with pytest.raises(L.SgError) as e:
            st.call("loadThemes", [1])
        assert e.value.code == L.SG_E_NO_RESOURCE


# ------------------------------------------------------------------------------------------- switchToCourse
def test_switch_to_course_flag_gate(portlib):
    """g_flags bit 0x4000000 makes switchToCourse a no-op; otherwise it needs the ui's courseActionMenu (phase 3.7)."""
    objs = _objs(portlib)
    with portlib.state() as st:
        st.write(objs["g_flags"][0], struct.pack("<I", 0x4000000))
        before = st.image()
        st.call("switchToCourse", [0])
        assert st.image() == before
        st.write(objs["g_flags"][0], struct.pack("<I", 0))
        if not portlib.has("courseActionMenu"):
            with pytest.raises(L.SgError) as e:
                st.call("switchToCourse", [0])
            assert e.value.code == L.SG_E_NO_RESOURCE
