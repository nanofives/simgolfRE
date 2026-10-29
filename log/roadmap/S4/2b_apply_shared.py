"""S4.2b: every shared-file edit of the resubmitted S4 wave (S4.1, S4.2, S4.2b, S4.3 .. S4.9b, S4.10, S4.11), deduplicated, in apply
order (2b_merge.md section 1 explains each).

  1. Wave 1 (S4.1 + S4.2) through I3's log/roadmap/merges/S4-3/apply_shared.py, unchanged: modules.inc `SG_MODULE(audio_core)`,
     the C ABI addition (2_cabi.h.txt / 2_cabi.cpp.txt), sg_state_free's `audio_release`, UNCERTAINTIES rows U-1400..U-1417, the
     CLAUDE.md S4 paragraph, port/CORE.md section 8. I3's script reads the `### 1.` .. `### 6.` sections of a wave-1 MERGE.md:
     log/roadmap/S4/wave1/ holds them (I8's log/roadmap/merges/I8/wave1_merge.py rebuilt them from what I3's script wrote into
     port/build/S4_2b_full; the resubmission's log/roadmap/S4/MERGE.md no longer carries them).
     port/src/sim/modules.inc keeps the checkout's line endings: both scripts edit it as LF and it is written back CRLF when it was
     CRLF (it is CRLF at HEAD 78ffb06d with core.autocrlf=true, LF at b3d61e41).
  2. modules.inc: the eleven module lines of the later steps after audio_core (S4.4 .. S4.9b, then S4.11's audio_crt and S4.10's
     audio_exe), then port/src/registry.cpp touched (MSBuild does not rebuild it when only modules.inc changes).
  3. NOT applied, replaced by S4.2b: 4_merge.md sections 2 / 3, 8_merge.md section 2. The C ABI and sg_state_free stay as wave 1
     left them. The edits of files the wave owns (audio_core.cpp's audio_crt_function_name / audio_exe_sound_name lookups,
     audio_wave.cpp's argument fixes, sound_replay.py's S4.11 install) are already in those files in the tree.
  4. port/src/sim/tick.cpp: S4.10's three kDrops lines removed (playSound, startSoundSlot, Snd::status).
  5. port/src/sim/tick_boot.inc: S4.10's boot calls (arrayStaticInit_448160 in BootCrt, loadSoundTable in BootGameMain, playSound
     in BootSetup) and the BootGameMain comment. Sections 4 and 5 go in together (10_merge.md section 2b).
  6. re/roadmap/S4_functions.tsv (the lead's table): S4.11's nine step changes `-` -> `S4.11` and its nineteen new rows.
  7. UNCERTAINTIES.md: the rows of 2b_, 3_ .. 9b_, 10_, 11_ and resub_uncertain.tsv in that order (U-1418 .. U-1545; U-1545 is the lead's row from the resubmission proof). The rules of I3's script
     hold (an address cell that is not a hex address moves to the front of the description); four-column files (9_, 9b_, 11_) get an
     empty path-to-resolution and status `open`; a `|` in a cell is written as a backslash-escaped bar. RESOLVED sets the status of
     rows the wave settles; WAVE1_STATUS rewrites the status of three wave-1 rows (U-1411, U-1415, U-1416, S4.10's texts); U-1421
     keeps `open` with S4.10's call-graph text added to its path-to-resolution.
  8. CLAUDE.md: MERGE.md's `## CLAUDE.md paragraph` (`S4 wave 2 ...`) right after the Section S4 block wave 1 inserts, before
     `Phase 7 defect decisions` (I8's apply.py did this by hand).

Usage: ~/bin/py -3.12 log/roadmap/S4/2b_apply_shared.py <root>          (apply)
       ~/bin/py -3.12 log/roadmap/S4/2b_apply_shared.py --prove         (apply to a scratch root of the tree's copies of the shared files
                                                                         (each unmodified against HEAD), then check with
                                                                         log/roadmap/S4/resub/edit_equiv.py that every edit equals the
                                                                         one S4 made on b3d61e41 (port/build/S4_2b_full), line for line)
"""
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
I3 = ROOT / "log" / "roadmap" / "merges" / "S4-3" / "apply_shared.py"
WAVE1 = HERE / "wave1"   # the wave-1 sections I3's script reads (section 1 of the docstring)
CRLF = "\r\n"
BAR, ESCAPED_BAR = "|", "\\|"
MODULES = ("audio_mixer", "audio_chan", "audio_ds", "audio_dev", "audio_codec", "audio_seq", "audio_sound", "audio_wave",
           "audio_soundfile", "audio_crt", "audio_exe")
UNCERTAIN = ("2b", "3", "4", "5", "6", "7", "8", "9", "9b", "10", "11", "resub")
S2B = "resolved (S4.2b, 2026-10-24)"
RESOLVED = {
    "U-1420": S2B + ": CodecFrame sits on the core's SndFrame at the original's /GZ offsets; S4.3 57 / 57 keys GREEN",
    "U-1425": S2B + ": sound_replay negctl --callee judges a callee's mode under its callers' keys",
    "U-1435": S2B + ": the frame block is the core's SndFrame (audio.h), renderer locals at the original's offsets",
    "U-1440": "partial (S4.2b, 2026-10-24): fcell is gone, every local is in the core's SndFrame; the cells are not at the "
              "original's offsets (no key compares their addresses)",
    "U-1444": S2B + ": the 0x0f7f keys run on the core's x87 unit on x64 too (S4.5 113 / 113 keys GREEN)",
    "U-1450": S2B + ": frame_cell is gone, the locals are SndFrame cells at the original's offsets",
    "U-1452": S2B + ": the 0x0f7f keys run on the core's x87 unit on x64 too",
    "U-1460": "partial (S4.2b, 2026-10-24): fcell is gone, every local is in the core's SndFrame; the cells are not at the "
              "original's offsets (no key compares their addresses)",
    "U-1462": "partial (S4.2b, 2026-10-24): the clock VM is sound_replay CLOCK_FUNCTIONS; syslibs.js still has no timeGetTime",
    "U-1470": S2B + ": SndLocals is the core's SndFrame; the dispatcher passes each callee its entry esp",
    "U-1472": S2B + ": the dispatcher routes an unclaimed `FF 25` import stub as snd_import of its slot",
    "U-1493": "resolved (S4.11, 2026-10-25): the four callees are GlblHeader::init 0x2e3c0, Sound::getField1b8 0x5f40, "
              "MixBuffer::deletingDtor 0x2cf50 and Sound::queueMidiCommand3 0x20480 (re/names/sound_19.tsv), ported in audio_crt",
    "U-1494": "resolved (S4.10, 2026-10-25): the production AudioSink serves mmioOpenA names from GameFiles (DeviceSink in "
              "audio_exe.cpp: the name as the engine passes it, backslashes turned to slashes and a leading `.\\` dropped, read with "
              "GameFiles::read_all; memory files per re/frida/js/mmiostandin.js). The device-present run opens the install's .wav "
              "files this way.",
}
WAVE1_STATUS = {
    "U-1411": "resolved (S4.10, 2026-10-25): sound.dll's heap entry points run as port code down to KERNEL32 (S4.11's audio_crt "
              "ports the static CRT allocator); its HeapAlloc / HeapReAlloc / HeapSize / HeapFree imports go to the AudioSink with no "
              "host, and the device-present run's DeviceSink serves them from an arena in the context's address space (16 MB chunks "
              "at 0xF4000000..0xFC000000, freed blocks reused by size). In the replay the fixture's heap stand-in still claims them.",
    "U-1415": "resolved (S4.10, 2026-10-25): survey log/roadmap/S4/10_thunk_survey.py, 10_thunk_refs.py, 10_thunk_survey.tsv: the "
              "table holds 1,207 thunks (rva 0x1005..0x2793). 660 targets are registered port functions; 65 are hooks.csv rows "
              "marked dead; 482 have no hooks.csv row: 156 of them nothing references, 316 are referenced only from data "
              "(class-table slots), 10 from code (0x2e460, 0x2e3c0, 0x9210, 0x2e530, 0x1b1f0, 0x1c060, 0x1e680, 0x2f8b0, 0x2c270, "
              "0x24260). No thunk target outside the registered set is reached by any S4.10 key or by the device-present run (0 "
              "host fallbacks); a call to one in production is MissingResource, never a silent skip.",
    "U-1416": "resolved (S4.10, 2026-10-25): the chosen addresses stay: sink objects 0xFEC00000 (64 KB), sink traps "
              "0xFED00000..0xFEDFFFFF, the core's frames 0xFEB00000 and box 0xFEAF0000; the exe sound layer's arena is 0xFE000000 "
              "(1 MB) and the DeviceSink's heap 0xF4000000..0xFC000000, both below them. The device-present run (stream with 178 "
              "sounds, 40 ticks) and all 205 segments run with all of them mapped and no overlap (Memory::map refuses an overlap).",
}
U1421_ADD = (" S4.10 call graph (log/roadmap/S4/10_fpcw_paths.py, 10_fpcw_paths.txt): no function on either path loads a control "
             "word. codecDecodeFrame <- openMmioSound <- Sound5c450::openMmio, which has no direct caller and sits in class-table "
             "slot 0x5c4dc, called through [reg + 0x8c] from FUN_10029c90, FUN_1002be90, FUN_1002d9c0, FUN_1002ec70, FUN_1002f1c0, "
             "FUN_1002f6e0. codecEncodeFrame <- loadWaveResource <- WaveInDevice_requeueBuffer <- soundWindowProc (<- "
             "soundThreadPump) and soundWindowSubclassProc (<- pumpWaveMessages <- loadSoundFile, Midi_Device::enableOutput, "
             "Seq::loadFile). The only fldcw sites outside the CRT are the six Voice_mix* leaves and streamRenderBlock / "
             "streamRenderTail, reached through Wave_Device::serviceTick (table slot 14 of 0x5b304, the timer thread), which leave "
             "0x0f7f. Which thread runs each codec path, and so the word it inherits, needs a game run: NEED_GAME.")
KDROPS = ('    {"playSound", "audio request"},\r\n',
          '    {"startSoundSlot", "audio request"},\r\n',
          '    {"Snd::status", "audio device status query; the frame discards it (ML+0xf3c)"},\r\n')
BOOT = (
    ('    cc("staticInit_820f04");\r\n',
     '    cc("staticInit_820f04");\r\n'
     '    cc("arrayStaticInit_448160");   // the CRT initializer dynInit_arrayStaticInit_448160: the 300 g_sounds records\r\n'),
    ('    cc("showTitleScreen");\r\n',
     '    cc("showTitleScreen");\r\n'
     '    cc("loadSoundTable");                                             // gameMain+0x42e\r\n'),
    ("    // ML+0x2f playSound(0x80, 0x64, 0, 0, 0): the audio backend's\r\n",
     '    cc("playSound", 0x80, 0x64, 0, 0, 0);                             // ML+0x2f\r\n'),
    ("loadSoundTable (gameMain+0x42e) and the colour cycle are not the boot's.", "the colour cycle is not the boot's."),
)
S411_STEP = {0x4249a: "operator_delete", 0x42690: "strstr", 0x4288e: "__ArrayUnwind", 0x42902: "ehVectorConstructorIterator",
             0x42b0b: "heap_alloc", 0x42c07: "free", 0x45252: "crtRealloc", 0x45581: "msize", 0x47771: "_unlock"}
S411_NEW = [("0x2e3c0", "GlblHeader::init"), ("0x05f40", "Sound::getField1b8"), ("0x2cf50", "MixBuffer::deletingDtor"),
            ("0x20480", "Sound::queueMidiCommand3"), ("0x244a0", "ChannelDesc_dtor"), ("0x29b80", "VoiceBase_initListSlot"),
            ("0x2c270", "Ch5bb90_set3DBuffer"), ("0x424b0", "_strrchr"), ("0x424d7", "operator_new"), ("0x42577", "_sprintf"),
            ("0x425c9", "__onexit"), ("0x42647", "_atexit"), ("0x4280e", "ehVectorDestructorIterator"), ("0x42acd", "_malloc"),
            ("0x42adf", "__nh_malloc"), ("0x45226", "_lockexit"), ("0x4522f", "_unlockexit"), ("0x45b80", "__callnewh"),
            ("0x47710", "__lock")]
SHARED = ("port/src/sim/modules.inc", "port/include/portsim_c.h", "port/src/portsim_c.cpp", "port/src/registry.cpp",
          "port/src/sim/tick.cpp", "port/src/sim/tick_boot.inc", "UNCERTAINTIES.md", "CLAUDE.md", "port/CORE.md")
UNTRACKED_SHARED = ("re/roadmap/S4_functions.tsv",)       # the lead's table, a wave file (not at HEAD)
PROVED = ("port/src/sim/modules.inc", "port/include/portsim_c.h", "port/src/portsim_c.cpp", "port/src/sim/tick.cpp",
          "port/src/sim/tick_boot.inc")


def rd(root, p):
    return (root / p).read_bytes().decode("utf-8")


def wr(root, p, s):
    (root / p).write_bytes(s.encode("utf-8"))
    print("edited", p)


def once(text, anchor, what):
    n = text.count(anchor)
    if n != 1:
        sys.exit(f"{what}: anchor occurs {n} times")


def rows_of(name):
    out = []
    text = (HERE / f"{name}_uncertain.tsv").read_bytes().decode("utf-8").replace(CRLF, "\n")
    for line in text.split("\n"):
        if not line.strip():
            continue
        c = [x.strip() for x in line.split("\t")]
        if len(c) == 4:
            c += ["", "open"]
        assert len(c) == 6, (name, c[0], len(c))
        if any(BAR in x for x in c):
            print("escaped a bar in", c[0])
            c = [x.replace(BAR, ESCAPED_BAR) for x in c]
        out.append(c)
    return out


I3_ASSERT = 'assert prev.startswith("Roadmap S0, baseline and frame census"), prev[:60]'
# HEAD b3d61e41 put the S2 / S3 paragraphs between S0's and "Phase 7" (S6), the anchor I3 inserts before; the S4 paragraph still
# goes there (section order S0, S2, S3, S4, S6). Only this check of I3's script is relaxed; the script is otherwise run as is.
I3_ASSERT_NOW = ('assert prev.startswith(("Roadmap S0, baseline and frame census", "Roadmap S2", "Roadmap S3")), prev[:60]')
# HEAD d66aa755 (S7) split portsim_c.cpp into two extern "C" blocks (sg_state_new .. sg_run_frames_detail, then sg_live_run ..
# sg_build_info), so `}  // extern "C"` occurs twice (I11's refusal). S4's C ABI block still goes where it went on 78ffb06d and
# b3d61e41: right after sg_build_info, before the file's last `}  // extern "C"`. The check now requires that exact tail once
# (sg_build_info's `#endif`, `}`, blank line, closing brace); the replacement below it (s[:-len(last)] + block + last) is I3's.
I3_CABI_ASSERT = 'assert s.endswith(last) and s.count(last) == 1'
I3_CABI_ASSERT_NOW = ('assert s.endswith(last) and s.count("#endif\\r\\n}\\r\\n\\r\\n" + last) == 1 '
                      'and s.endswith("#endif\\r\\n}\\r\\n\\r\\n" + last)')


def i3_script() -> pathlib.Path:
    t = I3.read_text(encoding="utf-8")
    assert t.count(I3_ASSERT) == 1, "I3's CLAUDE.md check changed"
    p = pathlib.Path(tempfile.mkdtemp(prefix="s42b_i3_")) / "apply_shared_i3.py"
    # the copy runs from a temp dir: its S4 path (parents[2] / "S4") must still be log/roadmap/S4
    t = t.replace("S4 = pathlib.Path(__file__).resolve().parents[2] / \"S4\"", f"S4 = pathlib.Path({str(HERE)!r})")
    assert f"S4 = pathlib.Path({str(HERE)!r})" in t, "I3's S4 path line changed"
    t = t.replace(f"S4 = pathlib.Path({str(HERE)!r})", f"S4 = pathlib.Path({str(WAVE1)!r})")
    assert t.count(I3_CABI_ASSERT) == 1, "I3's portsim_c.cpp check changed"
    t = t.replace(I3_CABI_ASSERT, I3_CABI_ASSERT_NOW)
    p.write_text(t.replace(I3_ASSERT, I3_ASSERT_NOW), encoding="utf-8")
    return p


def apply(root: pathlib.Path):
    for f in ("MERGE.md", "2_cabi.h.txt", "2_cabi.cpp.txt", "1_uncertain.tsv", "2_uncertain.tsv"):
        assert (WAVE1 / f).is_file(), f"log/roadmap/S4/wave1/{f} missing"
    mi = root / "port/src/sim/modules.inc"
    mi_crlf = b"\r\n" in mi.read_bytes()
    if mi_crlf:
        mi.write_bytes(mi.read_bytes().replace(b"\r\n", b"\n"))
        print("modules.inc: CRLF in this root, edited as LF and written back CRLF")
    r = subprocess.run([sys.executable, str(i3_script()), str(root)], capture_output=True, text=True)
    print(r.stdout, end="")
    if r.returncode:
        sys.exit("wave 1 (I3's apply_shared.py) failed: " + r.stdout[-600:] + r.stderr[-600:])

    # 2. modules.inc
    m = rd(root, "port/src/sim/modules.inc")
    assert m.endswith("SG_MODULE(audio_core)\n") and CRLF not in m, "modules.inc: wave 1's line is not the last"
    for mod in MODULES:
        if f"SG_MODULE({mod})" in m:
            sys.exit(f"modules.inc: {mod} already present")
    m += "".join(f"SG_MODULE({mod})\n" for mod in MODULES)
    wr(root, "port/src/sim/modules.inc", m.replace("\n", CRLF) if mi_crlf else m)
    now = time.time()
    os.utime(root / "port/src/registry.cpp", (now, now))
    print("touched port/src/registry.cpp")

    # 4. tick.cpp kDrops
    t = rd(root, "port/src/sim/tick.cpp")
    for line in KDROPS:
        once(t, line, "tick.cpp kDrops " + line.strip()[:30])
        t = t.replace(line, "", 1)
    wr(root, "port/src/sim/tick.cpp", t)

    # 5. tick_boot.inc
    b = rd(root, "port/src/sim/tick_boot.inc")
    if "arrayStaticInit_448160" in b:
        sys.exit("tick_boot.inc: S4.10's calls already present")
    for a, n in BOOT:
        once(b, a, "tick_boot.inc " + a.strip()[:40])
        b = b.replace(a, n, 1)
    wr(root, "port/src/sim/tick_boot.inc", b)

    # 6. S4_functions.tsv
    p = root / "re/roadmap/S4_functions.tsv"
    if p.exists():
        f = rd(root, "re/roadmap/S4_functions.tsv")
        nl = CRLF if CRLF in f else "\n"
        lines = f.split(nl)
        hdr = lines[0].split("\t")
        assert hdr[:4] == ["step", "family", "rva", "name"], hdr
        have = {(int(l.split("\t")[2], 16)): i for i, l in enumerate(lines[1:], 1) if l.strip()}
        for rva, name in S411_STEP.items():
            c = lines[have[rva]].split("\t")
            assert c[3] == name and c[0] == "-", (hex(rva), c[:4])
            c[0] = "S4.11"
            lines[have[rva]] = "\t".join(c)
        while lines and not lines[-1].strip():
            lines.pop()
        for rva, name in S411_NEW:
            assert int(rva, 16) not in have, rva
            lines.append("\t".join(["S4.11", "crt", rva, name, "C2", "transcribed", "0", "emu-only", ""]))
        wr(root, "re/roadmap/S4_functions.tsv", nl.join(lines) + nl)
    else:
        print("re/roadmap/S4_functions.tsv absent in this root: section 6 skipped")

    # 7. UNCERTAINTIES.md
    rows, seen = [], set()
    for name in UNCERTAIN:
        for uid, addr, typ, desc, path, status in rows_of(name):
            assert uid not in seen, uid
            seen.add(uid)
            if not re.fullmatch(r"0x[0-9a-fA-F]+", addr):
                mm = re.match(r"(S4\.\d+b?|S4 lead): ", desc)
                assert mm, (uid, desc[:30])
                desc = f"{mm.group(1)}, {addr}: " + desc[mm.end():]
                addr = ""
            status = RESOLVED.get(uid, status)
            if uid == "U-1421":
                path = path + U1421_ADD
            rows.append(f"| {uid} | {addr} | {typ} | {desc} | {path} | {status} |")
    assert set(RESOLVED) <= seen, sorted(set(RESOLVED) - seen)
    u = rd(root, "UNCERTAINTIES.md")
    if "| U-1418 |" in u:
        sys.exit("UNCERTAINTIES.md: S4.2b rows already present")
    assert u.endswith(CRLF)
    lines = u.split(CRLF)
    for uid, st in WAVE1_STATUS.items():
        idx = [i for i, l in enumerate(lines) if l.startswith(f"| {uid} |")]
        assert len(idx) == 1, uid
        cells = lines[idx[0]].split(" | ")
        assert cells[-1] == "open |", (uid, cells[-1])
        cells[-1] = st.replace(BAR, ESCAPED_BAR) + " |"
        lines[idx[0]] = " | ".join(cells)
    u = CRLF.join(lines)
    wr(root, "UNCERTAINTIES.md", u + CRLF.join(rows) + CRLF)
    print(f"UNCERTAINTIES rows {len(rows)} ({min(seen)} .. {max(seen)}), status set for {len(RESOLVED)} + {len(WAVE1_STATUS)} "
          f"wave-1 rows, U-1421 extended")

    # 8. CLAUDE.md: MERGE.md's wave-2 paragraph after the Section S4 block
    mm = (HERE / "MERGE.md").read_bytes().decode("utf-8").replace(CRLF, "\n")
    sec = mm.split("\n## CLAUDE.md paragraph", 1)[1].split("\n## ", 1)[0]
    blocks = re.findall(r"```[a-z]*\n(.*?)\n```", sec, re.S)
    assert len(blocks) == 1 and blocks[0].startswith("S4 wave 2 (") and "\n\n" not in blocks[0], blocks[:1]
    c = rd(root, "CLAUDE.md")
    if CRLF + "S4 wave 2 (" in c:
        sys.exit("CLAUDE.md: S4 wave 2 paragraph already present")
    anchor = CRLF + "Phase 7 defect decisions, section S6 of the roadmap orchestrator"
    once(c, anchor, "CLAUDE.md Phase 7 anchor")
    assert "Section S4, audio engine model" in c.split(anchor, 1)[0].rsplit(CRLF + "Roadmap ", 1)[-1], "S4 block not before Phase 7"
    wr(root, "CLAUDE.md", c.replace(anchor, CRLF + blocks[0].replace("\n", CRLF) + anchor, 1))


def prove():
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="s42b_apply_"))
    for p in SHARED + UNTRACKED_SHARED:
        if p in SHARED:
            # the working-tree copy (checkout line endings, which I3's anchors expect); it must be unmodified against HEAD
            st = subprocess.run(["git", "status", "--porcelain", "--", p], cwd=ROOT, capture_output=True, text=True, check=True)
            assert not st.stdout.strip(), f"{p} differs from HEAD"
        d = tmp / p
        d.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / p, d)
    apply(tmp)
    import importlib.util
    spec = importlib.util.spec_from_file_location("s4_edit_equiv", HERE / "resub" / "edit_equiv.py")
    ee = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(ee)
    ee.MIR = tmp
    bad = ee.main()
    mi = (tmp / "port/src/sim/modules.inc").read_bytes()
    head_crlf = b"\r\n" in (ROOT / "port/src/sim/modules.inc").read_bytes()
    if (b"\r\n" in mi) != head_crlf or (head_crlf and b"\n" in mi.replace(b"\r\n", b"")):
        bad += 1
        print("modules.inc: line endings differ from the checkout's")
    print("scratch root", tmp)
    if not bad:
        shutil.rmtree(tmp)
    return 1 if bad else 0


def shared_texts() -> dict:
    """{path: bytes} of the edited shared port files (and S4_functions.tsv) as the script leaves them: what 2b_mirror.py builds with."""
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="s42b_apply_"))
    try:
        for p in SHARED + UNTRACKED_SHARED:
            d = tmp / p
            d.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / p, d)
        import contextlib
        import io
        with contextlib.redirect_stdout(io.StringIO()):
            apply(tmp)
        return {p: (tmp / p).read_bytes() for p in PROVED + UNTRACKED_SHARED}
    finally:
        shutil.rmtree(tmp)


if __name__ == "__main__":
    if sys.argv[1:] == ["--prove"]:
        sys.exit(prove())
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    apply(pathlib.Path(sys.argv[1]).resolve())
