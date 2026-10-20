"""Phase 3.7a tick: the frame step against the TIME-FORWARD segments (the gate), the negative controls of the tick module
(segments, vectors, or a state test each), and state tests of the straddler leaves no TIME-FORWARD frame reaches.
Notes: log/p3/p37a_notes.txt."""
from __future__ import annotations

import struct

import pytest

from port.test import lib as L
from port.test import segments as S

pytestmark = pytest.mark.port
need_segments = pytest.mark.skipif(not S.SEG_DIR.exists(), reason="segment corpus not present (port/corpus/segments, local)")
need_install = pytest.mark.skipif(not S.INSTALL.is_dir(), reason="the user's install (original/) is not present")

TIME_FORWARD = [r for r in S.manifest_rows() if r["status"] == "accepted" and r["class"] == "TIME-FORWARD"]
TF_IDS = [f"{r['scenario']}/{int(r['segment']):02d}" for r in TIME_FORWARD]
COMMAND = [r for r in S.manifest_rows() if r["status"] == "accepted" and r["class"] == "COMMAND"]
CMD_IDS = [f"{r['scenario']}/{int(r['segment']):02d}" for r in COMMAND]

# COMMAND segments (the trace-driven input, tick.cpp TraceInput): the ones that do not pass yet, each with its measured cause
# (log/p3/p37a_notes.txt section 9). A run that stops with SG_E_NO_RESOURCE names a callee no module registers yet (the UI
# straddlers phase 3.7b lists as gaps, re/port/commands_split.tsv) or a provider the run has none of (Text_put: a TextMetrics,
# U-0944); OPEN_DIVERGENT are hash divergences whose cause lies outside this module.
OPEN_MISSING = {
    "Text_put": "BLOCKED by U-0947: showPopup / textInputBox / the browsers measure text with the host GDI rasterizer "
                "(U-0944) and the traces record no width (re/model/trace_schema.md has no such record): no provider",
    "drawGolferInfoPanel": "3.7b gap (6.3 KB)",
    "showCourseReport": "3.7b gap",
    "shotStyleButtonCode": "not registered (ui, 46 bytes)",
    "loadGameBrowserScreen": "3.7b gap (file browser)",
    "showHoleStatsScreen": "3.7b gap",
    "showEndOfYearScreen": "3.7b gap",
    "showBulletinBoard": "the board itself (modal, skillPointDialog / scrollTextWindow): 3.7b gap; tick's fallback stops there",
    "courseActionMenu": "3.7b gap (switchToCourse's menu)",
    "showCharacterIntro": "3.7b gap (customizeCharacter's modal intro; needs skillPointDialog U-0865); U-0931",
}
OPEN_DIVERGENT = {}
# a fast subset for the default run (the full gate is marked slow: about five minutes in one process)
CMD_FAST = ["course_season/03", "cov_a_keys/02", "cov_b_inject/00", "cov_k2_simfoto/00", "cov_k3_camera2/02",
            "cov_k1_washers/00", "ui_tour/03"]


def _seg(name: str) -> S.Segment:
    sc, k = name.split("/")
    return S.load_segment(S.SEG_DIR / sc / k)


# ------------------------------------------------------------------------------------------------ the gate
@need_segments
@need_install
@pytest.mark.parametrize("row", TIME_FORWARD, ids=TF_IDS)
def test_time_forward_segment_passes(srunner, row):
    """Every frame record (T and the 203 sim hashes) and the end checkpoint's sim hashes equal the original's."""
    seg = S.load_segment(S.segment_dir(row))
    r = srunner.run(seg)
    assert r.status == "PASS", f"{r.name}: {r.status} frames {r.frames_run}/{r.frames_expected} {r.detail}"


def _command_verdict(lib, srunner, row) -> str:
    """'' when the COMMAND segment passes or stops for its documented open cause, else what went wrong."""
    seg = S.load_segment(S.segment_dir(row))
    r = srunner.run(seg)
    if r.status == "PASS":
        return ""
    if seg.name in OPEN_DIVERGENT:
        return ""
    m = r.run_detail.split(";")[0]
    if m.startswith("missing: ") and m[len("missing: "):] in OPEN_MISSING:
        return ""
    return f"{seg.name}: {r.status} frames {r.frames_run}/{r.frames_expected} {r.detail} | {r.run_detail[:300]}"


@need_segments
@need_install
@pytest.mark.parametrize("name", CMD_FAST)
def test_command_segment_passes(srunner, name):
    """The trace's dispatches, cursor and key reads and harness writes reach the pumps that took them in the original."""
    r = srunner.run(_seg(name))
    assert r.status == "PASS", f"{r.name}: {r.status} frames {r.frames_run}/{r.frames_expected} {r.detail} | {r.run_detail}"


@pytest.mark.slow
@need_segments
@need_install
@pytest.mark.parametrize("row", COMMAND, ids=CMD_IDS)
def test_command_segment_passes_or_stops_for_its_open_cause(portlib, srunner, row):
    why = _command_verdict(portlib, srunner, row)
    assert why == "", why


# ------------------------------------------------------------------------------------------------ non-causal objects
def test_noncausal_list_is_documented():
    """segments.NON_CAUSAL holds only g_textBuf (its evidence: log/p3/p37a_noncausal.py, 81 of 81 references are __itoa
    destinations); a difference there is reported, never a failure, and every other sim object still fails a segment."""
    assert set(S.NON_CAUSAL) == {"g_textBuf"}
    syms = ["g_rng", "g_textBuf", "g_date"]
    exp = [(0, 0, [1, 2, 3]), (10, 1, [1, 2, 3]), (20, 2, [1, 2, 3])]
    got_nc = [(0, 0, [1, 2, 3]), (10, 1, [1, 9, 3]), (20, 2, [1, 9, 3])]
    assert S.first_divergent_record(exp, got_nc, syms, ignore=S.NON_CAUSAL) is None
    assert S.first_divergent_record(exp, got_nc, syms)["objects"] == ["g_textBuf"]
    assert S.noncausal_differences(exp, got_nc, syms) == "g_textBuf@1"
    got_c = [(0, 0, [1, 2, 3]), (10, 1, [1, 2, 3]), (20, 2, [1, 2, 4])]
    assert S.first_divergent_record(exp, got_c, syms, ignore=S.NON_CAUSAL)["objects"] == ["g_date"]


@need_install
def test_noncausal_textbuf_evidence():
    import importlib.util
    path = S.ROOT / "log" / "p3" / "p37a_noncausal.py"
    if not path.exists():
        pytest.skip("log/p3/p37a_noncausal.py not present")
    spec = importlib.util.spec_from_file_location("p37a_noncausal", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    assert mod.main() == 0


# ------------------------------------------------------------------------------------------------ negative controls
# (function, mode, what, a TIME-FORWARD segment it reads RED on)
NEGCTL_RED_SEGMENTS = [
    ("tick.frame", 1, "one extra RNG draw per frame", "sandbox_basic/00"),
    ("cellAt", 1, "cell index + 1", "sandbox_basic/00"),
    ("screenToTile", 1, "no neighbour descent", "cov_a_career/05"),
    ("drawQueuedSprite", 1, "composeGolferSprite skipped", "cov_a_flags/05"),
    ("drawQueuedSprite", 2, "enqueued entries not linked", "cov_a_flags/05"),
    ("buildSceneSprites", 1, "nothing runs", "cov_l_bridge/00"),
    ("drawWorldObjects", 1, "no spawn roll for flyer kind 0", "cov_a_build/05"),
    ("drawFlyingBall", 1, "nothing runs", "cov_a_build/05"),
    ("updateGolfers", 1, "no golfer ticks", "cov_a_build/05"),
    ("View_toLocal", 1, "cursor x / y swapped", "sandbox_basic/00"),
    ("View::fromParent", 1, "x off by one", "cov_k3_build/05"),
    ("matchUpdate", 1, "nothing runs", "cov_a_flags/02"),
]
# (function, mode, what): wrong bodies the vector corpus reads RED
NEGCTL_RED_VECTORS = [
    ("cellAt", 1, "cell index + 1"),
    ("logTick", 1, "the next month's slot"),
    ("updateGolfers", 1, "no golfer ticks"),
    ("matchUpdate", 1, "nothing runs"),
    ("addScore", 1, "level + 2 multiplier"),
    ("membershipReport", 1, "nothing runs"),
    ("rankValue", 1, "q 1..2 ranks 2"),
]
# (function, mode, what, a COMMAND segment it reads RED on): the input wiring's controls (tick.frame 3..5) and the functions
# the COMMAND segments needed (log/p3/p37a_negctl_cmd.py found the shortest PASSing segment each)
NEGCTL_RED_COMMAND = [
    ("tick.frame", 3, "the frame body's dispatches dropped", "cov_k1_washers/00"),
    ("tick.frame", 4, "every GetAsyncKeyState read answers 0", "cov_b_holes18/02"),
    ("tick.frame", 5, "the harness's memory writes skipped", "cov_k3_camera2/02"),
    ("playHabitatSound", 1, "no playSoundAt (its on-screen RNG draw)", "golfer_stories/00"),
]
# wrong bodies no segment or vector reaches, each with the state test that reads it RED (below) or the reason it is blind
NEGCTL_STATE = {
    ("startMessage", 1): "test_start_message",
    ("appendCents", 1): "test_append_cents",
    ("drawLabels", 1): "test_draw_labels_counts_down",
    ("drawMoneyLabels", 1): "test_draw_money_labels_counts_down",
    ("flushCourseInfo", 1): "test_flush_course_info",
    ("setDirtyFlagA", 1): "test_take_and_clear_readers",
    ("setDirtyFlagB", 1): "test_take_and_clear_readers",
    ("saveGame", 1): "test_save_game_refreshes_the_year",
    ("drawThoughtBubble", 1): "test_thought_bubble_composes_the_portrait",
    ("drawWindow", 1): "test_thought_bubble_composes_the_portrait",
    ("tileToScreen", 1): "test_tile_to_screen_caches",
    ("canAfford", 1): "test_can_afford_boundary",
}
NEGCTL_BLIND = {
    ("snapshotTables", 1): "writes render-class arrays only (the sprite snapshot): no simulation object can see it",
    ("tick.frame", 2): "the bulletin-board fallback's counter: no TIME-FORWARD start image has a pending photo",
    ("hireEmployee", 1): "needs the hire dialog's choice (showHireEmployeeDialog, phase 3.7b): no segment or vector",
    ("drawRecords", 1): "its own writes are render-class objects (g_recordBanks, g_animalCell, the sprite queue: "
                        "gamestate_meta.inc); blind on every PASSing COMMAND segment and cov_k1_animals/00's 18 frames",
}


@need_segments
@need_install
@pytest.mark.parametrize("function,mode,what,segment", NEGCTL_RED_SEGMENTS)
def test_negctl_reads_red_on_segments(portlib, srunner, function, mode, what, segment):
    seg = _seg(segment)
    assert srunner.run(seg).status == "PASS"
    portlib.negctl(function, mode)
    r = srunner.run(seg)
    portlib.negctl_reset()
    assert r.status != "PASS", f"{function} mode {mode} ({what}) is not seen by {segment}"


@need_segments
@need_install
@pytest.mark.parametrize("function,mode,what,segment", NEGCTL_RED_COMMAND)
def test_negctl_reads_red_on_command_segments(portlib, srunner, function, mode, what, segment):
    seg = _seg(segment)
    assert srunner.run(seg).status == "PASS"
    portlib.negctl(function, mode)
    try:
        r = srunner.run(seg)
    finally:
        portlib.negctl_reset()
    assert r.status != "PASS", f"{function} mode {mode} ({what}) is not seen by {segment}"


@pytest.mark.parametrize("function,mode,what", NEGCTL_RED_VECTORS)
def test_negctl_reads_red_on_vectors(portlib, vrunner, function, mode, what):
    if not vrunner.run_function(function).vec_file:
        pytest.skip("vector corpus not present")
    portlib.negctl(function, mode)
    r = vrunner.run_function(function)
    portlib.negctl_reset()
    assert r.verdict == "RED" and r.failed > 0, f"{function} mode {mode} ({what}) read {r.summary()}"


def test_every_tick_function_has_a_control(portlib):
    mine = {n for n, f in portlib.functions().items() if f.get("module") == "tick"}
    covered = ({f for f, *_ in NEGCTL_RED_SEGMENTS} | {f for f, *_ in NEGCTL_RED_VECTORS} | {f for f, *_ in NEGCTL_RED_COMMAND}
               | {f for f, _ in NEGCTL_STATE} | {f for f, _ in NEGCTL_BLIND})
    # the sprite queue's entry points share drawQueuedSprite's switch; View_toLocal's cursor read is fromParent's caller
    shared = {"resetSpriteQueue", "enqueueSprite", "addSprite", "renderSpriteQueue"}
    assert mine - covered - shared == set(), sorted(mine - covered - shared)
    for f in mine:
        assert portlib.negctl(f, 0) == 0, f


# ------------------------------------------------------------------------------------------------ state tests
def _off(portlib, name: str) -> int:
    for n, off, size in portlib.objects(None):
        if n == name:
            return off
    raise KeyError(name)


def _w32(st, off, v):
    st.write(off, struct.pack("<i", v))


def _r32(st, off):
    return struct.unpack("<i", st.read(off, 4))[0]


def _cstr(st, off, n=64):
    b = st.read(off, n)
    return b[:b.index(0)]


def test_start_message(portlib):
    """startMessage: shown when no message runs (copy of the text, timer strlen / 3 + 16, arg / sound, two RNG draws
    range(600) then range(200) + 200 on g_rng), refused while one shows with prio <= 0."""
    def run(mode):
        with portlib.state() as st:
            st.write(_off(portlib, "g_textBuffer"), b"hello world\0")
            _w32(st, _off(portlib, "g_rng"), 0x1234567)
            portlib.negctl("startMessage", mode)
            r = st.call("startMessage", [7, 0, 9]).eax
            portlib.negctl_reset()
            ui = _off(portlib, "g_ui_56d1a8")
            return (r, _cstr(st, _off(portlib, "unk_005a6d40")), _r32(st, _off(portlib, "g_messageTimer")),
                    _r32(st, ui), _r32(st, ui + 4), st.read(_off(portlib, "g_ui_569498"), 1))
    with portlib.state() as st:
        _w32(st, _off(portlib, "g_rng"), 0x1234567)
        a = st.call("Random::range", [st.data_base + _off(portlib, "g_rng"), 600]).eax
        b = st.call("Random::range", [st.data_base + _off(portlib, "g_rng"), 200]).eax
    got = run(0)
    assert got == (1, b"hello world", 11 // 3 + 16, a, b + 200, b"\x01")
    assert run(1) != got
    with portlib.state() as st:
        st.write(_off(portlib, "g_ui_569498"), b"\x01")
        assert st.call("startMessage", [7, 0, 9]).eax == 0
        assert st.call("startMessage", [7, 1, 9]).eax == 1


def test_append_cents(portlib):
    def run(n, mode=0):
        with portlib.state() as st:
            st.write(_off(portlib, "g_textBuffer"), b"$+\0")
            portlib.negctl("appendCents", mode)
            st.call("appendCents", [n & 0xFFFFFFFF])
            portlib.negctl_reset()
            return _cstr(st, _off(portlib, "g_textBuffer"))
    assert run(1205) == b"$+12.05"
    assert run(-1250) == b"$-12.50"
    assert run(1205, 1) != b"$+12.05"


def test_draw_labels_counts_down(portlib):
    for fn, timer in (("drawLabels", "g_labelQueueB"), ("drawMoneyLabels", "g_moneyLabelTime")):
        for mode, want in ((0, 4), (1, 5)):
            with portlib.state() as st:
                _w32(st, _off(portlib, timer) + 8, 5)
                portlib.negctl(fn, mode)
                st.call(fn, [])
                portlib.negctl_reset()
                assert _r32(st, _off(portlib, timer) + 8) == want, (fn, mode)


test_draw_money_labels_counts_down = test_draw_labels_counts_down


def test_can_afford_boundary(portlib):
    """canAfford: a cost equal to the cash is affordable (the vectors all return 1 at cost 0: zero spread)."""
    for mode, want in ((0, 1), (1, None)):
        with portlib.state() as st:
            _w32(st, _off(portlib, "g_cash_div100"), 500)
            _w32(st, _off(portlib, "g_holesPlusOne"), 9)
            portlib.negctl("canAfford", mode)
            try:
                got = st.call("canAfford", [500]).eax
            except L.SgError:
                got = None   # the refusal path waits for input (pumpAndRedraw): no input attached
            portlib.negctl_reset()
            assert got == want, mode


def test_take_and_clear_readers(portlib):
    for fn, sym in (("setDirtyFlagA", "gDirtyFlagA"), ("setDirtyFlagB", "gDirtyFlagB")):
        for mode, left in ((0, 0), (1, 0x41)):
            with portlib.state() as st:
                _w32(st, _off(portlib, sym), 0x41)
                portlib.negctl(fn, mode)
                assert st.call(fn, []).eax == 0x41
                portlib.negctl_reset()
                assert _r32(st, _off(portlib, sym)) == left, (fn, mode)


def test_flush_course_info(portlib):
    """A pending course-info path gets the info-file extension, then the two image names, and is consumed (byte 0 = 0)."""
    for mode, first in ((0, 0), (1, ord("c"))):
        with portlib.state() as st:
            off = _off(portlib, "g_pendingCourseInfo")
            st.write(off, b"course.sve\0")
            portlib.negctl("flushCourseInfo", mode)
            st.call("flushCourseInfo", [])
            portlib.negctl_reset()
            assert st.read(off, 1)[0] == first
            assert st.read(off + 1, 5) == b"ourse"


def _from_segment(st, name="sandbox_basic/00"):
    """A real frame-head state (a segment's start image at the original data base) as the base of a state test."""
    st.set_data_base(S.DATA_VA)
    st.files_root(S.INSTALL)
    st.load_image(_seg(name).start)


@need_segments
def test_save_game_refreshes_the_year(portlib):
    """saveGame records the save descriptor (4 when none was recorded) and refreshes g_year through appendDate."""
    def run(mode):
        with portlib.state() as st:
            _from_segment(st)
            _w32(st, _off(portlib, "g_saveFd"), -1)
            _w32(st, _off(portlib, "g_date"), 3 * 8192 + 5 * 1024)
            st.write(_off(portlib, "g_year"), b"\0\0")
            portlib.negctl("saveGame", mode)
            st.call("saveGame", [st.data_base + _off(portlib, "f_strpool_4c63c8") + 100])
            portlib.negctl_reset()
            return _r32(st, _off(portlib, "g_saveFd")), st.read(_off(portlib, "g_year"), 2)
    fd, year = run(0)
    assert fd == 4 and year != b"\0\0"
    assert run(1)[1] == b"\0\0"


def test_thought_bubble_composes_the_portrait(portlib):
    """drawWindow with a portrait selection runs drawThoughtBubble's composeGolferSprite (look indices of the type)."""
    def run(fn, mode):
        with portlib.state() as st:
            g = 3
            rec = _off(portlib, "g_golfers") + 0x100 * g
            st.write(rec + 0xb6, struct.pack("<h", 7))             # the golfer's type
            _w32(st, _off(portlib, "f_w_4c2e00") + 8, g)
            before = st.read(_off(portlib, "g_golferTypes"), 0x230 * 8)
            portlib.negctl(fn, mode)
            st.call("drawWindow", [0xf0, -8 & 0xFFFFFFFF, 0, 0])
            portlib.negctl_reset()
            return st.read(_off(portlib, "g_golferTypes"), 0x230 * 8) != before
    assert run("drawWindow", 0)
    assert not run("drawWindow", 1)
    assert not run("drawThoughtBubble", 1)


test_draw_thought_bubble = test_thought_bubble_composes_the_portrait


@need_segments
def test_tile_to_screen_caches(portlib):
    """tileToScreen stores an on-screen tile's point in g_tileScreenX / Y (cleared cache entry recomputed)."""
    def run(mode):
        with portlib.state() as st:
            _from_segment(st)
            out = st.map(0x30000000, bytearray(0x1000))
            xs = _off(portlib, "g_tileScreenX")
            hit = None
            for cell in range(2500):
                if struct.unpack("<h", st.read(xs + 2 * cell, 2))[0] not in (0, -99):
                    hit = cell
                    break
            assert hit is not None
            st.write(xs + 2 * hit, b"\0\0")
            portlib.negctl("tileToScreen", mode)
            r = st.call("tileToScreen", [hit // 50, hit % 50, 0x30000000, 0x30000004]).eax
            portlib.negctl_reset()
            return r, st.read(xs + 2 * hit, 2)
    r, cached = run(0)
    assert r == 1 and cached != b"\0\0"
    assert run(1)[1] == b"\0\0"


# ------------------------------------------------------------------------------------------------ the C ABI
@need_segments
def test_run_frames_reports_why_it_stopped(portlib):
    """sg_run_frames' detail names a callee that is neither ported nor a drop (MissingResource) instead of skipping it."""
    import ctypes as C
    portlib.dll.sg_run_frames_detail.restype = C.c_char_p
    portlib.dll.sg_run_frames_detail.argtypes = [C.c_void_p]
    seg = _seg("sandbox_basic/00")
    with portlib.state() as st:
        st.set_data_base(S.DATA_VA)
        st.load_image(seg.start)
        code, n, ts, hs = st.run_frames(seg.trace, seg.queued, 3)
        assert code == L.SG_OK and n == 3 and ts == [r[0] for r in seg.records[1:4]]
        # the input summary of a run with no input: nothing delivered, left or read out of order
        assert portlib.dll.sg_run_frames_detail(st.ctx) == (
            b"input: 0 dispatches delivered, 0 left, 0 at another cursor count; cursor reads over/under 0/0; "
            b"async-key reads over/under 0/0")
