"""Phase 3.5: events, news and golfer stories (module `events`, port/src/sim/events*.cpp).

Evidence per function (see log/p3/p35_notes.txt):
  * the vector corpus (test_vectors.py runs every function with a .vec file),
  * the LIVE observations of the raw captures (port/test/liveraw.py) for the functions the emulation could not run or ran
    thinly (story file reader, story chain, hole / building / patron builders),
  * unit tests below for what neither reaches (replaceInText, appendOpinion's ranges, loadStoryStrings on a synthetic file,
    announceBuilding's thresholds, runBulletinBoardWithSound),
  * a negative control (wrong body) per function: every mode must read RED under one of the three, or is recorded in
    NEGCTL_BLIND with the reason.
"""
from __future__ import annotations

import ctypes as C
import pathlib
import struct
import sys

import pytest

from port.test import lib as L
from port.test import liveraw as LR
from port.test import vectors as V

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

need_corpus = pytest.mark.skipif(not VL.VEC_DIR.exists(), reason="vector corpus not present (local)")
need_raw = pytest.mark.skipif(not LR.RAW_DIR.exists(), reason="raw vector captures not present (local)")
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")

# objects the stand-ins of a capture (startMessage, appendCents, playSound / playSoundAt, the RNG they would have drawn from)
# replaced: a live comparison of a key whose fixture still stood these in leaves them out
STAND_INS = ("f_w_4c2e00", "g_ui_5a34e4", "g_ui_5694a0", "g_ui_56d1a8", "g_msgState53df54", "unk_005a6d40", "g_messageTimer",
             "g_ui_569498", "g_rng", "g_ui_569628", "g_textBuf", "g_infoPanelNumBuf")

# function -> [(raw key, ignored objects, minimum vectors that must pass)]. The raw keys named `+unstub` revert the stand-ins
# of the selected callees; a key that still stands startMessage in answers its scripted return, so a vector whose control flow
# depends on that return can differ (emitGolferThought, pairGolfersWithStory): the minimum is what the port reproduces.
LIVE = {
    "loadStoryStrings": [("loadStoryStrings+unstub", (), 40)],
    "computeStoryReply": [("computeStoryReply+unstub", (), 40), ("computeStoryReply_chain+unstub", (), 40)],
    "storyText": [("storyText+unstub", (), 19)],
    "advanceStory": [("advanceStory+unstub", (), 61)],
    "announceBuilding": [(k, (), 1) for k in (
        "announceBuilding", "announceBuilding_bi0", "announceBuilding_bi15", "announceBuilding_bi4",
        "announceBuilding_flag60short", "announceBuilding_h1", "announceBuilding_h10", "announceBuilding_h18",
        "announceBuilding_h6", "announceBuilding_h9", "announceBuilding_p2", "announceBuilding_p3long",
        "announceBuilding_p3short", "announceBuilding_p4long", "announceBuilding_p4short", "announceBuilding_p5",
        "announceBuilding_p5long", "announceBuilding_p5short", "announceBuilding_rej_pin", "announceBuilding_rej_tee",
        "announceBuilding_rej_third", "announceBuilding_skipbi")],
    "holeMagazineEvent": [(k, (), 1) for k in (
        "holeMagazineEvent", "holeMagazineEvent_busy", "holeMagazineEvent_diff2", "holeMagazineEvent_force",
        "holeMagazineEvent_mag2")],
    "announceLandmark": [("LandmarkAvailableNotice", (), 22), ("LandmarkAvailableNotice_busy", (), 22)],
    "patronEvent": [(k, (), 18) for k in (
        "patronEvent", "patronEvent_busy", "patronEvent_diff2", "patronEvent_lm0", "patronEvent_lm1", "patronEvent_lm2",
        "patronEvent_lm3")],
    "emitGolferThought": [("emitGolferThought+unstub", STAND_INS, 124), ("emitGolferThought_partner+unstub", STAND_INS, 12)],
    "pairGolfersWithStory": [("announceVipGolfer+unstub", STAND_INS + ("g_textBuffer",), 284)],
}

# (function, mode, what, oracle): oracle "vec" = the vector corpus, "live" = the raw captures above, "unit" = a unit test below
NEGCTL = [
    ("appendUpgradeText", 1, "closing clauses swapped", "vec"), ("appendUpgradeText", 2, "strcpy for the first piece", "vec"),
    ("replaceInText", 1, "saved tail skips key length - 1", "unit"),
    ("thoughtBalance", 1, "0x8000 counts as -1", "vec"), ("thoughtFlag", 1, "bit 14 set gives 1", "vec"),
    ("appendOpinion", 1, "rating 6 twist 1 takes the second phrase", "vec"), ("appendOpinion", 2, "rating 0x80 default phrase", "unit"),
    ("postEvent", 1, "already-posted test skipped", "vec"),
    ("runBulletinBoardWithSound", 1, "the tick is not cleared", "unit"),
    ("loadStoryStrings", 1, "cache key ignores sel", "live"), ("loadStoryStrings", 2, "header count starts at 0", "unit"),
    ("loadStoryStrings", 3, "line cap 201", "unit"),
    ("computeStoryReply", 1, "0xc000 outcome drops v once", "live"), ("computeStoryReply", 2, "run of 0x13 words not counted", "live"),
    ("storyText", 1, "owner flag test uses bit 0x10000", "live"),
    ("substituteStoryNames", 1, "second name is the golfer's own", "vec"),
    ("advanceStory", 2, "counter drop c / 4", "live"),
    ("emitGolferThought", 1, "kind 0x13 test d > 2", "vec"), ("emitGolferThought", 2, "mood clamp -9", "live"),
    ("emitGolferThought", 3, "delta / 2", "live"),
    ("buildGolferComment", 1, "t3 mirrored", "vec"), ("buildGolferComment", 2, "own and other names swapped", "vec"),
    ("buildGolferComment", 3, "g == 0x98 clears nothing", "vec"),
    ("announceLandmark", 1, "'A' maps to 0x16b", "vec"),
    ("patronEvent", 1, "CEO amount 4000", "live"),
    ("announceHoleType", 1, "kind 3 posts event 1", "vec"),
    ("announceBuilding", 1, "yards <= 0x31 gives par 2", "unit"), ("announceBuilding", 2, "facing rounds down", "live"),
    ("pairGolfersWithStory", 1, "compat counts 4 bits", "live"),
]
# wrong bodies no oracle reaches (recorded on purpose: the segments must cover them once sg_run_frames exists)
NEGCTL_BLIND = [
    ("advanceStory", 1, "live", "a partner one stage behind plays the other sound: the audio call has no state effect"),
    ("announceLandmark", 2, "live", "Random::range(2) for the neighbour step: only the retry path, no captured vector retries"),
    ("holeMagazineEvent", 1, "live", "notable threshold 0x31: no captured hole has a skill delta of exactly 0x31"),
    ("pairGolfersWithStory", 2, "live", "reset threshold roll > 7: no captured roll equals 7"),
]


# ------------------------------------------------------------------------------------------------ helpers
def _off(lib, name):
    for n, o, s in lib.objects():
        if n == name:
            return o
    raise KeyError(name)


def _new_state(lib, exe=True):
    st = lib.state()
    st.set_data_base(VL.DATA_VA)
    if exe:
        assert lib.exe_load() == "", "the user's exe does not verify"
        st.fill_from_exe(whole_image=True)
    for name, va, data in V.exe_sections():
        if va < VL.DATA_VA + VL.DATA_SIZE and VL.DATA_VA < va + len(data):
            continue
        st.map(va, data, readonly=True)
    return st


def _text(st, lib, n=0x200):
    b = st.read(_off(lib, "g_textBuffer"), n)
    return b[:b.index(0)] if 0 in b else b


def _set_text(st, lib, s: bytes):
    st.write(_off(lib, "g_textBuffer"), s + b"\0")


# ------------------------------------------------------------------------------------------------ live oracle
@pytest.mark.port
@need_raw
@pytest.mark.parametrize("function", sorted(LIVE))
def test_live_captures(portlib, function):
    """The port against every vector of the raw captures of the function (the live return value and live byte changes)."""
    runner = LR.LiveRunner(portlib)
    total = passed = 0
    for key, ignore, minimum in LIVE[function]:
        r = runner.run_key(function, key, ignore=ignore)
        if r.run == 0:
            pytest.skip(f"{key}: {r.skipped}")
        # a vector that reaches a function of another stage-B child that is not registered yet is not judged
        # (a read of live heap memory the capture did not keep is a fault here: the original read it, the port cannot)
        pending = [f for f in r.failures if "missing exe constants / game file" in f[1] or "fault (unmapped address)" in f[1]]
        hard = [f for f in r.failures if f not in pending]
        total += r.run
        passed += r.passed
        assert r.passed + len(pending) >= minimum or not hard, f"{r.summary()}"
        if not ignore:
            assert not hard, r.summary()
    print(f"{function}: {passed}/{total} live vectors passed")


# ------------------------------------------------------------------------------------------------ negative controls
def _detect(lib, function, mode, oracle):
    lib.negctl(function, mode)
    try:
        if oracle == "vec":
            r = V.VectorRunner(lib).run_function(function)
            return r.failed > 0
        if oracle == "live":
            runner = LR.LiveRunner(lib)
            base_fail = 0
            red = False
            for key, ignore, minimum in LIVE[function]:
                lib.negctl_reset()
                b = runner.run_key(function, key, ignore=ignore)
                lib.negctl(function, mode)
                r = runner.run_key(function, key, ignore=ignore)
                base_fail = len(b.failures)
                red = red or len(r.failures) > base_fail
            return red
        raise AssertionError(oracle)
    finally:
        lib.negctl_reset()


@pytest.mark.port
@need_corpus
@need_raw
@pytest.mark.parametrize("function,mode,what,oracle", [n for n in NEGCTL if n[3] in ("vec", "live")])
def test_negctl_reads_red(portlib, function, mode, what, oracle):
    assert _detect(portlib, function, mode, oracle), f"{function} mode {mode} ({what}) is not seen by the {oracle} oracle"


@pytest.mark.port
@need_corpus
@need_raw
@pytest.mark.parametrize("function,mode,oracle,why", NEGCTL_BLIND)
def test_negctl_blind_spots_are_known(portlib, function, mode, oracle, why):
    """A blind spot that starts reading RED means the corpus grew: move it to NEGCTL and update log/p3/p35_notes.txt."""
    assert not _detect(portlib, function, mode, oracle), f"{function} mode {mode} is no longer blind ({why})"


@pytest.mark.port
def test_every_function_has_a_negctl(portlib):
    funcs = portlib.functions()
    mine = {n[0] for n in NEGCTL}
    for f in sorted(mine):
        assert f in funcs, f
        assert portlib.negctl(f, 0) == 0, f


MINE = ["appendUpgradeText", "replaceInText", "thoughtBalance", "thoughtFlag", "appendOpinion", "postEvent",
        "runBulletinBoardWithSound", "loadStoryStrings", "computeStoryReply", "storyText", "substituteStoryNames",
        "advanceStory", "emitGolferThought", "buildGolferComment", "announceLandmark", "patronEvent", "holeMagazineEvent",
        "announceHoleType", "announceBuilding", "pairGolfersWithStory"]


@pytest.mark.port
def test_the_module_registers_its_functions(portlib):
    funcs = portlib.functions()
    for f in MINE:
        assert f in funcs, f
    # the straddlers of phase 3.7 and the audio backend are reached through the registry, never registered here
    for f in ("startMessage", "appendCents", "playSound", "playSoundAt", "logTick"):
        assert f not in MINE


# ------------------------------------------------------------------------------------------------ unit tests
@pytest.mark.port
@need_exe
@pytest.mark.parametrize("mode", [0, 1])
def test_replace_in_text(portlib, mode):
    lib = portlib
    lib.negctl("replaceInText", mode)
    cases = [(b"Hello [N], dear [N].", b"[N]", b"Bo"), (b"[N] first", b"[N]", b"A longer replacement"),
             (b"no token here", b"[N]", b"X"), (b"end [N]", b"[N]", b""), (b"abcabc", b"abc", b"z"),
             (b"x" * 40 + b"[KEY]" + b"y" * 20, b"[KEY]", b"R")]
    expect = []
    for text, key, repl in cases:
        i = text.find(key)
        expect.append(text if i < 0 else text[:i] + repl + text[i + len(key):])
    ok = True
    with _new_state(lib) as st:
        kb = (C.c_uint8 * 64)()
        rb = (C.c_uint8 * 64)()
        st.map(0x40000000, kb)
        st.map(0x40010000, rb)
        for (text, key, repl), want in zip(cases, expect):
            C.memmove(kb, key.ljust(64, b"\0"), 64)
            C.memmove(rb, repl.ljust(64, b"\0"), 64)
            _set_text(st, lib, text)
            st.call("replaceInText", [0x40000000, 0x40010000])
            ok &= _text(st, lib) == want
    assert ok == (mode == 0)


# the opinion phrases: (pool offset) per rating and twist as the hook source describes them, text taken from the user's exe
_OPINION_FIXED = [404, 372, 340, 308, 272, 244]


def _opinion_offset(rating, twist):
    if 0 <= rating <= 5:
        return _OPINION_FIXED[rating]
    if rating == 6:
        return 212 if twist <= 1 else 176
    if rating == 7:
        return 148 if twist < 1 else 120
    if rating < 0 or rating > 0x7F:
        return 4
    return 80 if twist < 1 else 40


@pytest.mark.port
@need_exe
@pytest.mark.parametrize("mode", [0, 2])
def test_append_opinion_ranges(portlib, mode):
    lib = portlib
    lib.negctl("appendOpinion", mode)
    pool = lib.exe_const("f_strpool_4e28e0")
    ok = True
    with _new_state(lib) as st:
        for rating in list(range(-3, 12)) + [0x7E, 0x7F, 0x80, 0x81, 1000, -1000]:
            for twist in (-1, 0, 1, 2):
                _set_text(st, lib, b">")
                st.call("appendOpinion", [rating, twist])
                off = _opinion_offset(rating, twist)
                want = b">" + pool[off:pool.index(0, off)]
                ok &= _text(st, lib, 0x400) == want
    assert ok == (mode == 0)


@pytest.mark.port
def test_run_bulletin_board_with_sound(portlib):
    lib = portlib
    for mode in (0, 1):
        lib.negctl("runBulletinBoardWithSound", mode)
        with lib.state() as st:
            st.set_data_base(VL.DATA_VA)
            tick, words = _off(lib, "g_bulletinAnimTick"), _off(lib, "f_w_4e3db8")
            st.write(tick, struct.pack("<i", 7))
            st.call("runBulletinBoardWithSound", [])
            got_tick = struct.unpack("<i", st.read(tick, 4))[0]
            got_word = struct.unpack("<i", st.read(words, 4))[0]
        assert got_word == -1
        assert got_tick == (0 if mode == 0 else 0x14)
    lib.negctl_reset()


def _story_file(lines):
    return ("\r\n".join(lines) + "\r\n").encode()


def _prep_story(st, lib, theme=b"T1", slot_name=b"story.txt", slot=3):
    st.write(_off(lib, "g_theme"), theme + b"\0")
    st.write(_off(lib, "g_storyDefs") + 50 * slot, slot_name + b"\0")
    st.write(_off(lib, "g_flags"), struct.pack("<I", 0))


@pytest.mark.port
@need_exe
@pytest.mark.parametrize("mode", [0, 2, 3])
def test_load_story_strings_on_a_synthetic_file(portlib, mode):
    """Header lines count from 1; a header equal to the key's countdown is appended with a newline; the cap is 200 lines."""
    lib = portlib
    lib.negctl("loadStoryStrings", mode)
    long_file = _story_file([f"H{i}" for i in range(1, 301)])
    results = []
    with _new_state(lib) as st:
        _prep_story(st, lib)
        st.files_put("Themes/T1/story.txt", long_file)
        for key in (1, 5, 201, 202):
            _set_text(st, lib, b"")
            st.write(_off(lib, "g_story838220"), struct.pack("<h", 0))
            r = st.call("loadStoryStrings", [3, key, 0, -1])
            results.append((key, _text(st, lib, 0x100), r.eax))
    want = [(1, b"H1\n", 1), (5, b"H5\n", 1), (201, b"H201\n", 0), (202, b"", 0)]
    # the 201st line is processed and the read then stops with 0 and no cache entry; key 202 is never reached
    assert (results == want) == (mode == 0)


@pytest.mark.port
@need_exe
def test_load_story_strings_cache_and_missing_file(portlib):
    lib = portlib
    with _new_state(lib) as st:
        _prep_story(st, lib)
        # a file that does not exist: nothing appended, 0 returned, the slot is still claimed
        _set_text(st, lib, b"x")
        assert st.call("loadStoryStrings", [3, 2, 0, -1]).eax == 0
        assert _text(st, lib) == b"x"
        st.files_put("Themes/T1/story.txt", _story_file(["HA", " one", " two", "HB", " three"]))
        for sel, want in ((0, b"HA\n one\n"), (1, b"HA\n two\n")):
            pass
        # key 1, sel 0: header 1 plus its first indented line; the second call is served from the cache with the same text
        _set_text(st, lib, b"")
        first = st.call("loadStoryStrings", [3, 1, 0, -1]).eax
        t1 = _text(st, lib, 0x100)
        _set_text(st, lib, b"")
        st.files_clear()                       # the cache must answer now
        st.call("loadStoryStrings", [3, 1, 0, -1])
        t2 = _text(st, lib, 0x100)
        assert t1 == t2 and t1.startswith(b"HA")
        assert first == 1                      # the slot word advances to 1 after the first stored entry


@pytest.mark.port
@need_exe
@pytest.mark.parametrize("mode", [0, 1])
def test_announce_building_par_thresholds(portlib, mode):
    """A hole of exactly 50 yards is a par 2 (yards <= 0x32)."""
    lib = portlib
    lib.negctl("announceBuilding", mode)
    with _new_state(lib) as st:
        holes = _off(lib, "g_holes")
        st.write(_off(lib, "g_holesPlusOne"), struct.pack("<i", 1))
        rec = holes + 0x208
        st.write(rec + 8, struct.pack("<i", 5))        # tee x
        st.write(rec + 24, struct.pack("<i", 9))       # pin x
        st.write(rec + 4, struct.pack("<h", 0x32))     # yards
        st.call("announceBuilding", [])
        par = st.read(rec, 1)[0]
    assert par == (2 if mode == 0 else 3)
    lib.negctl_reset()
