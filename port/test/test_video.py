"""Roadmap S8.3: the video module (port/src/sim/video.cpp; re/analysis/video/skip_path.md).

playAudioFile and the BinkPlayer g_bink take the original's own skip path when the MovieHost cannot open a movie (the default:
no decoder, decision D2), and drive the frame loop through a host that can (a scripted test host here). Evidence:

  * EMULATOR ORACLE (opt-in SG_EMU_ORACLE=1, development): the ORIGINAL playAudioFile / BinkPlayer::play / close / setTarget run
    under Unicorn (port/test/cmdemu.py) over the same states; binkw32's imports answer the scripted host's script, the display's
    window slot (+0x20) answers 1 (0 for the no-window case), fileExists answers the case's file presence, the drawing callees
    (Window::paint, showScreen, redrawAll, gfxFlushA) are stubbed, doMouse delivers the case's key on the chosen pass.
    The whole data image and eax must equal the port's (BinkPlayer::play alone: the handle / buffer words compared as zero /
    non-zero, the port stores tokens), and the host's frame counter the emulated BINK header's FrameNum.
  * PINNED DIGESTS (default): port/test/golden_video.json, written from the port with SG_EMU_ORACLE=update after the oracle was
    GREEN on every case; every negative control must change at least one.
  * BOOT RUNS: the intro-ON scenario cov_r_s8_play (re/tools/s8_boot_capture.py; capture log/p2/p24/cov_r_s8_play/c_a, trace
    port/corpus/traces/cov_r_s8_play.trace) passes the boot gate record by record and runs to the process exit with both movies
    through the module (a recording host sees both opens), writing the quit files the original wrote; S8.2's intro-ON trace
    (port/corpus/traces/s8_2/full_on_t1.trace, no frame head) runs to its end the same way.
"""
from __future__ import annotations

import hashlib
import json
import os
import pathlib
import struct

import pytest

from port.test import lib as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "port" / "test" / "golden_video.json"
pytestmark = pytest.mark.port
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")

DATA_VA = 0x004C1000
DATA_N = 0x381174
# golf_clean.exe functions and data (re/analysis/audio/0043cce0_playAudioFile.md, re/analysis/video/*)
PLAY_AUDIO_FILE, BINK_PLAY, BINK_CLOSE, BINK_SET_TARGET = 0x0043CCE0, 0x00487090, 0x00487060, 0x00487050
FILE_EXISTS, WINDOW_PAINT, SHOW_SCREEN, REDRAW_ALL, GFX_FLUSH_A = 0x004378A0, 0x004808C0, 0x00483BB0, 0x00483C70, 0x00497B00
G_BINK, G_DEADLINE, G_RUNFLAG, G_PHASE, G_DISPLAY = 0x005AA6D0, 0x005AA774, 0x0083AB9C, 0x0083AD4C, 0x0083AD50
DEADLINE_RA = 0x0043CD00          # the return address of playAudioFile's timeGetTime call
INTRO, CLOSING = b"./flics/SMSG_introfinal.bik", b"./flics/SMSG_closingfinal.bik"
# blocks of the tests' address space (both sides)
PATH_VA, DISPLAY_VA, HANDLE_VA, BUFFER_VA, SELF_VA = 0x31000000, 0x31001000, 0x31002000, 0x31003000, 0x31004000
WINDOW_SLOT_STUB = DISPLAY_VA + 0x200
T0 = 0x00100001                    # the clock value S8.2's trace holds at the deadline read

# A case: fn (pa = playAudioFile, play = BinkPlayer::play, close, target), args, host (None = the default skip host, else
# (frames, waits, flags)), file (fileExists' answer), init (va -> (size, value) written into the image before the call), the
# clock at the deadline read, the key (pass, clock) or None.
CASES = {
    "intro_skip":            dict(fn="pa", path=INTRO, imm=0),
    "closing_skip":          dict(fn="pa", path=CLOSING, imm=1),
    "intro_absent":          dict(fn="pa", path=INTRO, imm=0, file=0),
    "closing_absent":        dict(fn="pa", path=CLOSING, imm=1, file=0),
    "intro_skip_dirty":      dict(fn="pa", path=INTRO, imm=0, init={G_DEADLINE: (4, 0x1234), G_RUNFLAG: (1, 1), G_PHASE: (4, 5),
                                                                    G_BINK + 0xC: (4, 0x77), G_BINK + 0x10: (4, 9)}),
    "closing_skip_dirty":    dict(fn="pa", path=CLOSING, imm=1, init={G_DEADLINE: (4, 0xFFFF0000), G_RUNFLAG: (1, 1)}),
    "intro_imm7":            dict(fn="pa", path=INTRO, imm=7),
    "intro_errhandle":       dict(fn="pa", path=INTRO, imm=0, host=(5, 1, 8)),   # open answers a handle whose error word is set
    "intro_play3":           dict(fn="pa", path=INTRO, imm=0, host=(3, 2, 0)),
    "closing_play4":         dict(fn="pa", path=CLOSING, imm=1, host=(4, 0, 0)),
    "intro_nobuffer":        dict(fn="pa", path=INTRO, imm=0, host=(3, 1, 2)),
    "intro_nowindow":        dict(fn="pa", path=INTRO, imm=0, host=(3, 1, 4)),
    "intro_key_late":        dict(fn="pa", path=INTRO, imm=0, host=(6, 2, 0), key=(3, T0 + 0x1B59)),   # after the deadline: ends it
    "intro_key_early":       dict(fn="pa", path=INTRO, imm=0, host=(6, 2, 0), key=(3, T0 + 0x1B58)),   # not after it: plays on
    "closing_key":           dict(fn="pa", path=CLOSING, imm=1, host=(6, 2, 0), key=(2, 5)),            # deadline 0: any key ends it
    "play_skip_self":        dict(fn="play", self=SELF_VA, args=(0, 0, 0), init_self={0xC: 1}),
    "play_notarget":         dict(fn="play", self=SELF_VA, args=(0, 0, 0)),
    "play_offsets_sound":    dict(fn="play", self=G_BINK, args=(-3, 17, 5), host=(2, 1, 0), init={G_BINK + 0xC: (4, 0x55)}),
    "play_nobuffer":         dict(fn="play", self=G_BINK, args=(1, 2, 0), host=(2, 1, 2), init={G_BINK + 0xC: (4, 0x55)}),
    "close_both":            dict(fn="close", self=G_BINK, init={G_BINK + 4: (4, 1), G_BINK + 8: (4, 1)}),
    "close_handle_only":     dict(fn="close", self=G_BINK, init={G_BINK + 4: (4, 1)}),
    "close_none":            dict(fn="close", self=G_BINK, init={G_BINK + 0xC: (4, 3)}),
    "target":                dict(fn="target", self=G_BINK, target=0xABCD1234),
}
# words the port stores as host tokens where the original stores binkw32 pointers: compared as zero / non-zero
TOKEN_WORDS = (4, 8)


def _image(case) -> bytearray:
    img = bytearray(DATA_N)
    struct.pack_into("<I", img, G_DISPLAY - DATA_VA, DISPLAY_VA)   # the display object the oracle's slot +0x20 call reads
    for va, (n, v) in case.get("init", {}).items():
        img[va - DATA_VA:va - DATA_VA + n] = v.to_bytes(n, "little")
    return img


def _self_block(case) -> bytearray:
    b = bytearray(0x1000)
    for off, v in case.get("init_self", {}).items():
        struct.pack_into("<I", b, off, v)
    return b


def _path_block(case) -> bytearray:
    b = bytearray(0x1000)
    p = case.get("path", INTRO)
    b[:len(p)] = p
    return b


# ------------------------------------------------------------------------------------------------ the port side
def port_run(lib, case) -> dict:
    with lib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(bytes(_image(case)))
        st.map(PATH_VA, _path_block(case))
        selfb = st.map(SELF_VA, _self_block(case))
        if case.get("file", 1):
            p = case.get("path", INTRO).decode()
            st.files_put(p[2:], b"BIKi")       # fileExists: the logical name without "./"
        if case.get("host"):
            st.call("video.attachScripted", list(case["host"]))
        key = case.get("key") or (0, 0)
        st.call("video.setInput", [T0, key[0], key[1]])
        try:
            fn = case["fn"]
            if fn == "pa":
                eax = st.call("playAudioFile", [PATH_VA, case["imm"]]).eax
            elif fn == "play":
                eax = st.call("BinkPlayer::play", [case["self"], PATH_VA] + [a & 0xFFFFFFFF for a in case["args"]]).eax
            elif fn == "close":
                eax = st.call("BinkPlayer::close", [case["self"]]).eax
            else:
                eax = st.call("BinkPlayer::setTarget", [case["self"], case["target"]]).eax
            ss = st.call("video.scriptedState", []).eax
        finally:
            st.call("video.clearInput", [])
            st.call("video.detach", [])
        # the host's frame counter once a movie opened (1 after the open), None when none did
        fnum = None if ss == 0xFFFFFFFF or (ss & 0xFFFF) == 0 else ss & 0xFFFF
        return {"eax": eax, "image": st.image(), "self": bytes(selfb), "frame_num": fnum}


def digest(r) -> str:
    h = hashlib.sha256()
    h.update(struct.pack("<I", r["eax"] & 0xFFFFFFFF))
    h.update(r["image"])
    h.update(r["self"])
    h.update(struct.pack("<i", -1 if r["frame_num"] is None else r["frame_num"]))
    return h.hexdigest()


# ------------------------------------------------------------------------------------------------ the original (oracle)
def emu_run(case, cover=None) -> dict:
    from port.test import cmdemu as CE
    emu = CE.Emu()
    host = case.get("host")
    frames, waits, flags = host if host else (0, 0, 1)    # no host: the open fails (the default skip host)
    st = {"frame_num": None, "wait_left": waits, "ticks": 0}

    def u32(uc, va, v=None):
        if v is None:
            return struct.unpack("<I", uc.mem_read(va, 4))[0]
        uc.mem_write(va, struct.pack("<I", v & 0xFFFFFFFF))

    def bink_open(uc, a):
        if flags & 1:
            return 0
        err = 1 if flags & 8 else 0
        if err:                       # play answers 1 on the error word before the module's counter would start
            u32(uc, HANDLE_VA + 0x1C, err)
            return HANDLE_VA
        for i, v in enumerate((800, 600, frames, 1, 0, 15, 1, err)):   # Width Height Frames FrameNum LastFrameNum rate div ReadError
            u32(uc, HANDLE_VA + 4 * i, v)
        st["frame_num"] = 1
        return HANDLE_VA

    def bink_wait(uc, a):
        if st["wait_left"] == 0:
            st["wait_left"] = waits
            return 0
        st["wait_left"] -= 1
        return 1

    def next_frame(uc, a):
        u32(uc, HANDLE_VA + 0xC, u32(uc, HANDLE_VA + 0xC) + 1)
        st["frame_num"] += 1
        return 0

    imports = {"_BinkSetSoundSystem@8": (2, lambda uc, a: 1), "_BinkSetIOSize@4": (1, lambda uc, a: a[0]),
               "_BinkOpen@8": (2, bink_open), "_BinkBufferOpen@16": (4, lambda uc, a: 0 if flags & 2 else BUFFER_VA),
               "_BinkBufferSetOffset@12": (3, lambda uc, a: 1), "ShowCursor": (1, lambda uc, a: 0),
               "_BinkWait@4": (1, bink_wait), "_BinkDoFrame@4": (1, lambda uc, a: 0), "_BinkBufferLock@4": (1, lambda uc, a: 1),
               "_BinkCopyToBuffer@28": (7, lambda uc, a: 0), "_BinkBufferUnlock@4": (1, lambda uc, a: 1),
               "_BinkGetRects@8": (2, lambda uc, a: 0), "_BinkBufferBlit@12": (3, lambda uc, a: 1),
               "_BinkNextFrame@4": (1, next_frame), "_BinkClose@4": (1, lambda uc, a: 1),
               "_BinkBufferClose@4": (1, lambda uc, a: 1)}
    emu.import_handlers.update(imports)
    emu.drop(FILE_EXISTS, eax=case.get("file", 1), name="fileExists")
    for a, n in ((WINDOW_PAINT, "Window::paint"), (GFX_FLUSH_A, "gfxFlushA")):
        emu.drop(a, eax=0, name=n)
    # showScreen / redrawAll: no arguments, they end in a tail `jmp [eax + slot]` into the display (no `ret` of their own)
    emu.stub(SHOW_SCREEN, 0, 0, False, "showScreen")
    emu.stub(REDRAW_ALL, 0, 0, False, "redrawAll")
    emu.stub(WINDOW_SLOT_STUB, 0, 0 if flags & 4 else 1, False, "display slot +0x20")
    key = case.get("key")
    inputs = [CE.Raw(7, 0x1B, 0x1B, t=key[1])] if key else []

    def gate():
        st["ticks"] += 1
        return bool(key) and st["ticks"] == key[0]

    disp = bytearray(0x1000)
    struct.pack_into("<I", disp, 0, DISPLAY_VA + 0x100)                 # vptr
    struct.pack_into("<I", disp, 0x100 + 0x20, WINDOW_SLOT_STUB)        # slot +0x20
    blocks = [(PATH_VA, bytes(_path_block(case))), (DISPLAY_VA, bytes(disp)), (HANDLE_VA, bytes(0x1000)),
              (BUFFER_VA, bytes(0x1000)), (SELF_VA, bytes(_self_block(case)))]
    fn = case["fn"]
    if fn == "pa":
        entry, args, ecx = PLAY_AUDIO_FILE, [PATH_VA, case["imm"]], 0
    elif fn == "play":
        entry, args, ecx = BINK_PLAY, [PATH_VA] + [a & 0xFFFFFFFF for a in case["args"]], case["self"]
    elif fn == "close":
        entry, args, ecx = BINK_CLOSE, [], case["self"]
    else:
        entry, args, ecx = BINK_SET_TARGET, [case["target"]], case["self"]
    r = emu.run(entry, args, bytes(_image(case)), inputs=inputs, ecx=ecx, blocks=blocks, gate=gate,
                clock=lambda ra: T0 if ra == DEADLINE_RA else None, cover=cover)
    assert not r.err, (case, r.err, r.log[-6:])
    return {"eax": r.eax, "image": r.image, "frame_num": st["frame_num"],
            "log": r.log, "consumed": r.consumed}


def _self_va_words(case, r):
    """the BinkPlayer object of the case as the side holds it: (image or self block, offset)"""
    if case.get("self", G_BINK) == SELF_VA:
        return r["self"], 0
    return r["image"], G_BINK - DATA_VA


def compare(case, port, orig) -> list[str]:
    bad = []
    if port["eax"] != orig["eax"]:
        bad.append(f"eax port {port['eax']:#x} original {orig['eax']:#x}")
    pi, oi = bytearray(port["image"]), bytearray(orig["image"])
    if case["fn"] == "play" and case["self"] == G_BINK:
        for off in TOKEN_WORDS:
            a = G_BINK - DATA_VA + off
            pz, oz = pi[a:a + 4] == bytes(4), oi[a:a + 4] == bytes(4)
            if pz != oz:
                bad.append(f"g_bink+{off:#x} zero port {pz} original {oz}")
            pi[a:a + 4] = oi[a:a + 4] = bytes(4)
    if pi != oi:
        diff = [i for i in range(len(pi)) if pi[i] != oi[i]]
        bad.append(f"{len(diff)} image bytes differ, first at {DATA_VA + diff[0]:#010x}: port {pi[diff[0]]:#x} original {oi[diff[0]]:#x}")
    if port["frame_num"] != orig["frame_num"]:
        bad.append(f"frame counter port {port['frame_num']} original {orig['frame_num']}")
    return bad


@emu_oracle
@pytest.mark.parametrize("name", list(CASES))
def test_oracle_equals_original(portlib, name):
    case = CASES[name]
    orig = emu_run(case)
    port = port_run(portlib, case)
    bad = compare(case, port, orig)
    assert not bad, (name, bad)
    if os.environ.get("SG_EMU_ORACLE") == "update":
        g = json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}
        g[name] = {"digest": digest(port), "eax": port["eax"], "frame_num": port["frame_num"]}
        GOLDEN.write_text(json.dumps(g, indent=1, sort_keys=True) + "\n")


@emu_oracle
def test_oracle_branch_coverage():
    """over all cases, both sides of every conditional jump of playAudioFile, BinkPlayer::play and BinkPlayer::close run"""
    from port.test import cmdemu as CE
    cover = {"__functions__": [PLAY_AUDIO_FILE, BINK_PLAY, BINK_CLOSE]}
    for case in CASES.values():
        emu_run(case, cover=cover)
    sites, both, missing = CE.coverage_summary(cover)
    assert sites == 12 and both == sites, (sites, both, missing)


def _golden():
    if not GOLDEN.exists():
        pytest.skip("no golden_video.json (SG_EMU_ORACLE=update writes it after the oracle is GREEN)")
    return json.loads(GOLDEN.read_text())


@pytest.mark.parametrize("name", list(CASES))
def test_pinned_digest(portlib, name):
    g = _golden()
    assert name in g, f"{name} is not pinned"
    r = port_run(portlib, CASES[name])
    assert digest(r) == g[name]["digest"], (name, r["eax"], r["frame_num"], g[name])


def test_skip_path_effects(portlib):
    """the documented skip path (skip_path.md section 3): the intro leaves g_bink's target 1, the deadline 0, the input phase 0,
    the run flag untouched (0 stays 0, 1 stays 1), the handle / buffer 0, and answers 0"""
    for name, flag in (("intro_skip", 0), ("intro_skip_dirty", 1)):
        r = port_run(portlib, CASES[name])
        img = r["image"]
        w = lambda va: struct.unpack_from("<I", img, va - DATA_VA)[0]   # noqa: E731
        assert r["eax"] == 0, name
        assert w(G_DEADLINE) == 0 and w(G_PHASE) == 0 and img[G_RUNFLAG - DATA_VA] == flag, name
        assert (w(G_BINK + 4), w(G_BINK + 8), w(G_BINK + 0xC)) == (0, 0, 1), name


# (mode, what, cases it must change)
NEGCTL = [(1, "the deadline word is not cleared after the play", ["intro_skip", "intro_play3"]),
          (2, "the skip path sets the cursor flag g_ui83ab9c", ["intro_skip", "closing_skip"]),
          (3, "the skip path is not taken (the frame loop runs for an unavailable movie)", ["intro_skip", "closing_skip"]),
          (4, "the closing movie is dropped", ["closing_skip", "closing_play4"]),
          (5, "the intro's deadline one millisecond early", ["intro_key_early"])]


@pytest.mark.parametrize("mode,what,cases", NEGCTL)
def test_negctl_reads_red(portlib, mode, what, cases):
    g = _golden()
    assert portlib.negctl("playAudioFile", mode) == 0
    try:
        changed = [n for n in cases if digest(port_run(portlib, CASES[n])) != g[n]["digest"]]
    finally:
        portlib.negctl_reset()
    assert changed == cases, f"mode {mode} ({what}) left {sorted(set(cases) - set(changed))} unchanged"


# ------------------------------------------------------------------------------------------------ boot runs
from port.test import boot as B  # noqa: E402
from port.test import sve_refs  # noqa: E402

PLAY = "cov_r_s8_play"
S82_TRACE = ROOT / "port" / "corpus" / "traces" / "s8_2" / "full_on_t1.trace"
S82_RUN = ROOT / "log" / "c3" / "alt" / "refs" / "cov_r_s8_full" / "rec_on" / "summary.json"
OPENS = ["open ./flics/SMSG_introfinal.bik", "open ./flics/SMSG_closingfinal.bik"]


def _need(scen):
    if not (B.TRACES / f"{scen}.trace").exists() or not (B.CAPTURES / scen / "c_a" / "framehash.bin").exists():
        pytest.skip(f"no {scen} trace / capture (re/tools/s8_boot_capture.py install)")


def _recorded_boot(lib, scen, to_end=True):
    got = {}

    def before(st):
        st.call("video.attachRecorder", [])

    def after(st, res):
        st.call("video.log", [])
        got["log"] = (st.files_read("$video/log.txt") or b"").decode().split("\n")[:-1]
        got["files"] = {k: st.files_read(sve_refs.logical(k)) for k in got.get("want", {})}
        st.call("video.detach", [])
    return got, before, after


def test_boot_intro_on_scenario(portlib):
    """the boot gate's record-by-record comparison of the intro-ON scenario, run on to the process exit: both movies go
    through the video module and the quit writes the files the original wrote"""
    _need(PLAY)
    end, start = sve_refs.load(PLAY, "end"), sve_refs.load(PLAY, "run_a")
    if end is None or start is None:
        pytest.skip(f"no collected {PLAY} files (~/bin/py -3.12 -m port.test.sve_refs collect {PLAY})")
    want = {k: v for k, v in end.items() if start.get(k) != v or k == "top10.sve"}
    assert {"top10.sve", "Themes/Default.pro", "&QuitSave.sve"} <= set(want), sorted(want)
    got, before, after = _recorded_boot(portlib, PLAY)
    got["want"] = want
    r = B.run(PLAY, portlib, before=before, after=after, to_end=True)
    assert r.status == "PASS", f"{r.status} {r.detail} | {r.run_detail[:300]}"
    assert r.records == r.expected and r.expected > 0
    if r.run_detail:   # the Win32 runner reports no run detail (re/roadmap/BASELINE.md section 4)
        assert "mainLoop returned" in r.run_detail, r.run_detail[:300]
    assert got["log"] == OPENS, got["log"]
    bad = {k: ("missing" if got["files"].get(k) is None else "differs") for k, v in want.items()
           if got["files"].get(k) is None or sve_refs.compared_bytes(k, got["files"][k]) != sve_refs.compared_bytes(k, v)}
    assert not bad, bad


def test_boot_closing_dropped_reads_red(portlib):
    """negative control 4 on the boot run: the closing movie never reaches the host"""
    _need(PLAY)
    got, before, after = _recorded_boot(portlib, PLAY)
    portlib.negctl("playAudioFile", 4)
    try:
        B.run(PLAY, portlib, before=before, after=after, to_end=True)
    finally:
        portlib.negctl_reset()
    assert got["log"] == OPENS[:1], got["log"]


def _boot_trace(lib, trace: pathlib.Path, before=None, after=None):
    """boot.run's setup over a trace that has no capture (no frame head): the boot runs to the end of the trace"""
    from port.test import segment_files
    from port.test import segments as SG
    why = lib.exe_load()
    if why:
        pytest.skip(f"exe: {why}")
    with lib.state() as st:
        st.set_data_base(SG.DATA_VA)
        st.fill_from_exe(True)
        if SG.INSTALL.is_dir():
            st.files_root(SG.INSTALL)
        gdi = lib.has("gditext.attach")
        if gdi:
            st.call("gditext.attach", [])
        try:
            if before:
                before(st)
            code, done, _ts, _hs = st.run_boot(trace.read_bytes(), 1)
            detail = SG.SegmentRunner(lib)._detail(st)
            if after:
                after(st, None)
        finally:
            if gdi:
                st.call("gditext.detach", [])
    return code, done, detail


def test_boot_s8_2_full_trace(portlib):
    """S8.2's intro-ON recording (title menu only: the intro, the exit icon, the closing movie, the credits, Esc): the boot runs
    to mainLoop's return with both movies through the module and writes top10.sve as the original did"""
    if not S82_TRACE.exists() or not S82_RUN.exists():
        pytest.skip("no S8.2 trace / run (re/tools/s8_movie_runs.py)")
    got = {}

    def before(st):
        st.call("video.attachRecorder", [])

    def after(st, _res):
        st.call("video.log", [])
        got["log"] = (st.files_read("$video/log.txt") or b"").decode().split("\n")[:-1]
        got["top10"] = st.files_read("top10.sve")
        st.call("video.detach", [])
    code, done, detail = _boot_trace(portlib, S82_TRACE, before, after)
    assert code == L.SG_OK, (code, detail[:300])
    # tick.cpp's frameless boot branch keeps the boot step's FrameExit and reports the quit (3_merge.md item 4, merged by I4);
    # every recorded dispatch is taken, the closing movie (QUIT only) reached the host and top10.sve (QUIT, before the movie)
    # was written
    if detail:         # the Win32 runner reports no run detail
        assert "mainLoop returned (the title menu's quit)" in detail and " 0 left" in detail, detail[:300]
    assert got["log"] == OPENS, got["log"]
    want = json.loads(S82_RUN.read_text())["exit"]["files"]["top10.sve"]
    assert got["top10"] is not None and hashlib.sha256(got["top10"]).hexdigest() == want
