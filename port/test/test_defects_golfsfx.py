"""S7.12: the `fixed`-profile switch sound.golfSfxAlias (port/src/sim/audio_exe.cpp golf_sfx_name; Mariano's G15, U-1499) and
the app's defect profile (port/app/main.cpp --profile, session.json, port/test/app_replay.py).

The exe's sound table opens 18 course effects as `sounds\\Golf sfx\\<file>` (0x004d1da0 .. 0x004d2020, one more as `golf sfx`
at 0x004d104c); the install holds `sounds\\Golf_Sfx`, so the original's mmioOpenA fails on every one. At the default 1 the port's
device sink fails the same open; at 0 it opens the install's Golf_Sfx file when the exe's name is absent.
(a) the name lookup (audio_exe.golfSfxName) at 1 and 0, case-insensitive, an exe name that exists left alone; control 1 RED.
(b) audio_stream's device run: switch 1 = golden $device (4b6ed61a, 13,020 lines), switch 0 = DEVICE0 below, line for line the
    renamed-install run (S7.11's 11_sfx_install.py) but the alias notes; x64 and Win32; control 2 (the slot check ignores the
    alias) RED.
(c) the app: --profile-list equals re/port/defect_profiles.tsv; the tropical study trace runs under `fixed` (the default), faults
    under `original` and under the control --negctl profile-ignored; session.json records the profile; app_replay replays the
    recording under its recorded switches (PASS) and RED with the switches at 1.
(d) the app's shadow audio (--audio model) over course_season: the course effects open through the alias under `fixed`, none
    under `original`, trace.trace and framehash.bin identical.
Binaries: SG_PORTSIM_DLL (x64), port/build/Win32 through lib32, SG_APP_EXE or the S7.12 mirror's simgolf_port.exe.
"""
from __future__ import annotations

import hashlib
import json
import os
import pathlib
import shutil
import subprocess

import pytest

from port.test import audio_stream as AS
from port.test import profiles as P

ROOT = pathlib.Path(__file__).resolve().parents[2]
INSTALL = ROOT / "original"
SWITCH = "sound.golfSfxAlias"
BS = chr(92)
# the exe's 18 names (pefile over original/golf_clean.exe; the first is the lower-case one)
SFX = ["Cart Away", "Ball Drop Sand", "Ball Drop Fairway", "Ball Tree Leaves", "Ball Tree2", "Ball Tree", "Ball Water",
       "Ball In Hole", "Putt", "Chip", "Iron", "Drive With Ball", "Golf Swing Perfect FM", "Golf Swing NoImag M",
       "Golf Swing NoAcc", "Golf Swing Perfect M", "Tiger Swing 1", "Tiger Bounce 3"]
# S7.13 (decision G16): the six theme music tracks the device run asks for under the exe's spaced directory names
MUSIC = [("Desert Music", "Desert_Music", "Spanish Guitar 1"), ("Desert Music", "Desert_Music", "Spanish Guitar 3"),
         ("Links Music", "Links_Music", "Bagpipe2"), ("Links Music", "Links_Music", "Harpiscord 1"),
         ("Tropical Music", "Tropical_Music", "Steel Drum 1a"), ("Tropical Music", "Tropical_Music", "Steel Drums 3")]
# audio_stream's device run at switch 0 (x64 = Win32; log/roadmap/S7/13_device_x64.json): 235 mmioOpenA, 24 through the alias
# (S7.12's 18 course effects, 229 opens, 14,309 lines; S7.13 adds the six music tracks)
DEVICE0 = {"lines": 14833, "stream_sha256": "07f83f3fce538b032224583239c1b55773728396ff334d2cfb1844811918c812", "mmio": 235}
DEVICE1 = {"lines": 13020, "stream_sha256": "4b6ed61a9306793c298044d20fb3c91e32c2fa9a4e75b11220220c43b8f0eddb", "mmio": 211}
HAWAII = ROOT / "log" / "p5" / "terrain" / "s32t_cov_b_s34_hawaii" / "trace.trace"

pytestmark = pytest.mark.port


@pytest.fixture
def switch0(portlib):
    prev = portlib.defect(SWITCH, False)
    yield portlib
    portlib.defect(SWITCH, bool(prev))


def sfx_name(lib, name: str, neg: int = 0) -> tuple[str, int]:
    """audio_exe.golfSfxName over the install: (the name the device sink opens, 1 when that file exists)"""
    if neg:
        lib.negctl("audio_exe.golfSfxName", neg)
    try:
        with lib.state() as st:
            st.files_root(str(INSTALL))
            st.files_put("$audio_exe/sfxname", name.encode())
            r = st.call("audio_exe.golfSfxName", []).eax
            return st.files_read("$audio_exe/sfxname.out").decode(), r
    finally:
        if neg:
            lib.negctl_reset()


# ---- (a) the name lookup ------------------------------------------------------------------------------------------------
def test_switch_registered_default_one_and_in_fixed(portlib):
    d = portlib.defects()
    assert SWITCH in d and d[SWITCH]["on"] == 1 and d[SWITCH]["module"] == "audio_exe"
    assert P.members("fixed").get(SWITCH) == 0
    assert all((INSTALL / "sounds" / "Golf_Sfx" / f"{n}.wav").exists() for n in SFX)
    assert not (INSTALL / "sounds" / "Golf sfx").exists(), "the install keeps the stock directory name (never renamed)"


@pytest.mark.parametrize("n", SFX)
def test_original_keeps_the_failed_open(portlib, n):
    d = "golf sfx" if n == "Cart Away" else "Golf sfx"
    assert sfx_name(portlib, f"sounds{BS}{d}{BS}{n}.wav") == (f"sounds/{d}/{n}.wav", 0)


@pytest.mark.parametrize("n", SFX)
def test_fixed_opens_the_install_name(switch0, n):
    d = "golf sfx" if n == "Cart Away" else "Golf sfx"
    assert sfx_name(switch0, f"sounds{BS}{d}{BS}{n}.wav") == (f"sounds/Golf_Sfx/{n}.wav", 1)


def test_fixed_leaves_other_names_alone(switch0):
    for n in ("sounds/Applause.wav", "sounds/Golf sfx/NoSuchSound.wav", "sounds/Golf_Sfx/Putt.wav", "Golf sfx/Putt.wav",
              "sounds/Golf sfx/", "music/Golf sfx/Putt.wav"):
        got = sfx_name(switch0, n)[0]
        assert got == n, (n, got)


def test_fixed_keeps_an_exe_name_that_exists(switch0):
    with switch0.state() as st:  # a file under the exe's own spelling (the overlay; the install is not written) wins
        st.files_root(str(INSTALL))
        st.files_put("sounds/Golf sfx/Putt.wav", b"RIFF")
        st.files_put("$audio_exe/sfxname", b"sounds/Golf sfx/Putt.wav")
        assert st.call("audio_exe.golfSfxName", []).eax == 1
        assert st.files_read("$audio_exe/sfxname.out") == b"sounds/Golf sfx/Putt.wav"


def test_control_case_sensitive_directory_red(switch0):
    """control 1: the directory compared case-sensitively: the lower-case `golf sfx` name (Cart Away) is not found"""
    assert sfx_name(switch0, f"sounds{BS}golf sfx{BS}Cart Away.wav") == ("sounds/Golf_Sfx/Cart Away.wav", 1)
    assert sfx_name(switch0, f"sounds{BS}golf sfx{BS}Cart Away.wav", neg=1) == ("sounds/golf sfx/Cart Away.wav", 0)


@pytest.mark.parametrize("exe_dir,inst_dir,n", MUSIC)
def test_music_original_keeps_the_failed_open(portlib, exe_dir, inst_dir, n):
    assert sfx_name(portlib, f"sounds{BS}music{BS}{exe_dir}{BS}{n}.wav") == (f"sounds/music/{exe_dir}/{n}.wav", 0)


@pytest.mark.parametrize("exe_dir,inst_dir,n", MUSIC)
def test_music_fixed_opens_the_install_name(switch0, exe_dir, inst_dir, n):
    assert (INSTALL / "sounds" / "music" / inst_dir / f"{n}.wav").exists()
    assert sfx_name(switch0, f"sounds{BS}music{BS}{exe_dir}{BS}{n}.wav") == (f"sounds/music/{inst_dir}/{n}.wav", 1)


def test_music_fixed_leaves_the_cd_names_alone(switch0):
    """the names wrong on the CD itself (not an unshield rename) stay absent at 0 (G16)"""
    for n in ("sounds/Celebs/Marshal Male.wav", "sounds/effects/Alligator.wav", "sounds/emotion/PLS Sid mix.wav",
              "sounds/music/Misc Music/Jazz.wav", "sounds/Desert Music/Spanish Guitar 1.wav"):
        assert sfx_name(switch0, n) == (n, 0), n


def test_control_music_aliases_ignored_red(switch0):
    """control 3: the music aliases ignored: the tracks stay absent at 0 (the course effects still alias)"""
    exe_dir, inst_dir, n = MUSIC[0]
    name = f"sounds{BS}music{BS}{exe_dir}{BS}{n}.wav"
    assert sfx_name(switch0, name) == (f"sounds/music/{inst_dir}/{n}.wav", 1)
    assert sfx_name(switch0, name, neg=3) == (f"sounds/music/{exe_dir}/{n}.wav", 0)
    assert sfx_name(switch0, f"sounds{BS}Golf sfx{BS}Putt.wav", neg=3) == ("sounds/Golf_Sfx/Putt.wav", 1)


# ---- (b) the device run ---------------------------------------------------------------------------------------------------
def device(lib, on: bool, neg=None) -> tuple[list, list[str]]:
    prev = lib.defect(SWITCH, on)
    try:
        out, _rep, lines = AS.device_run(lib, neg=neg)
    finally:
        lib.defect(SWITCH, bool(prev))
    return out, lines


def check_device(out, lines, want):
    assert out == [0, 0, 0, 40, 0]
    assert (len(lines), AS.device_sha(lines)) == (want["lines"], want["stream_sha256"])
    assert sum("mmioOpen" in x for x in lines) == want["mmio"]


@pytest.mark.parametrize("backend", ["x64", "win32"])
def test_device_run_switch_one_is_the_golden_stream(backend, request):
    lib = request.getfixturevalue("portlib" if backend == "x64" else "lib32")
    out, lines = device(lib, True)
    check_device(out, lines, DEVICE1)
    gold = json.loads(AS.GOLDEN.read_text())["$device"]
    assert AS.device_sha(lines) == gold["stream_sha256"] and len(lines) == gold["lines"]
    assert not any(" (alias " in x for x in lines)


@pytest.mark.parametrize("backend", ["x64", "win32"])
def test_device_run_switch_zero_opens_the_course_effects(backend, request):
    lib = request.getfixturevalue("portlib" if backend == "x64" else "lib32")
    out, lines = device(lib, False)
    check_device(out, lines, DEVICE0)
    al = [x for x in lines if " (alias " in x]
    sfx = [x for x in al if " (alias sounds/Golf_Sfx/" in x]
    mus = [x for x in al if " (alias sounds/music/" in x]
    assert len(sfx) == len(SFX) and len(mus) == len(MUSIC) and len(sfx) + len(mus) == len(al)
    assert not any("(absent)" in x for x in lines)
    assert sorted(x.split(" (alias sounds/Golf_Sfx/")[1].rstrip(")").removesuffix(".wav") for x in sfx) == sorted(SFX)
    assert sorted(x.split(" (alias sounds/music/")[1].rstrip(")").removesuffix(".wav") for x in mus) ==         sorted(f"{i}/{n}" for _, i, n in MUSIC)


def test_device_run_switch_zero_equals_a_renamed_install(portlib, tmp_path, monkeypatch):
    """the alias opens exactly what an install with the exe's directory name opens (S7.11's layout: junctions and hard links into
    original/, nothing written there): every line equal once the alias notes are removed"""
    import importlib.util
    import re
    from port.test import segments as S
    spec = importlib.util.spec_from_file_location("sfx", ROOT / "log" / "roadmap" / "S7" / "11_sfx_install.py")
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    try:
        m.build(tmp_path / "inst")
        _, l0 = device(portlib, False)
        monkeypatch.setattr(S, "INSTALL", tmp_path / "inst")
        _, lr = device(portlib, True)
    finally:  # junctions and hard links only: rmdir /s does not follow a junction, a deleted hard link leaves the file
        subprocess.run(["cmd", "/c", "rmdir", "/s", "/q", str(tmp_path / "inst")], capture_output=True)
    assert [re.sub(r" \(alias [^)]*\)", "", x) for x in l0] == lr


def test_control_slot_check_ignores_the_alias_red(portlib):
    """control 2: deviceStep 3's slot check ignores the alias (the opens still alias): the course-effect slots are not played"""
    out, lines = device(portlib, False, neg=("audio_exe.golfSfxName", 2))
    assert AS.device_sha(lines) not in (DEVICE0["stream_sha256"], DEVICE1["stream_sha256"])
    assert sum("mmioOpen" in x for x in lines) < DEVICE0["mmio"]


# ---- (c) the app's profile ------------------------------------------------------------------------------------------------
def app_exe() -> pathlib.Path:
    env = os.environ.get("SG_APP_EXE")
    cands = ([pathlib.Path(env)] if env else []) + [
        ROOT / "port" / "build" / "S7_12_root" / "port" / "build" / "x64" / "app" / "Release" / "simgolf_port.exe",
        ROOT / "port" / "build" / "x64" / "app" / "Release" / "simgolf_port.exe"]  # the integrator's / inside the mirror
    exe = next((c for c in cands if c.exists()), None)
    if exe is None:
        pytest.skip("no simgolf_port.exe")
    return exe


def app(*args: str, timeout=1200) -> subprocess.CompletedProcess:
    return subprocess.run([str(app_exe()), *args], cwd=str(ROOT), capture_output=True, text=True, timeout=timeout)


def live_trace(tmp: pathlib.Path, trace: pathlib.Path, tag: str, *extra: str) -> tuple[pathlib.Path, dict]:
    rec = tmp / f"rec_{tag}"
    app("--install", str(INSTALL), "--data", str(tmp / f"data_{tag}"), "--record-dir", str(rec), "--live-trace", str(trace),
        "--terrain", "headless", *extra)
    assert (rec / "session.json").exists(), tag
    return rec, json.loads((rec / "session.json").read_text())


@pytest.mark.parametrize("profile", ["original", "fixed"])
def test_app_profile_list_equals_the_table(portlib, profile):
    r = app("--profile-list", profile, timeout=60)
    assert r.returncode == 0, r.stderr
    got = dict((ln.split("\t")[0], int(ln.split("\t")[1])) for ln in r.stdout.splitlines() if ln)
    want = {n: 1 for n in portlib.defects()}
    want.update(P.members(profile))
    assert got == want


def test_app_refuses_an_unknown_profile():
    assert app("--profile", "bogus", timeout=60).returncode == 2


def test_app_tropical_trace_runs_under_fixed_and_faults_under_original(tmp_path):
    """the study trace of a tropical course (recorded with S3.4's Tree alias copies in the instance): under `fixed`, the default,
    the run completes; under `original` and under the control --negctl profile-ignored (the profile recorded, no switch set) it
    stops with the stock game's fault (Image_draw reads 0x8 of loadFlic's NULL stream, U-1330)"""
    if not HAWAII.exists():
        pytest.skip("no tropical study trace")
    rec, s = live_trace(tmp_path, HAWAII, "default", "--audio", "off")
    assert (s["profile"], s["code"], s["stop"]) == ("fixed", 0, "host") and s["records"] > 0
    assert s["defects"][SWITCH] == 0 and s["defects"]["flic.treeAliasNames"] == 0
    assert {k: v for k, v in s["defects"].items() if v != 1} == P.members("fixed")
    for tag, extra in (("original", ("--profile", "original")), ("ignored", ("--negctl", "profile-ignored"))):
        _, so = live_trace(tmp_path, HAWAII, tag, "--audio", "off", *extra)
        assert so["code"] == -3 and so["detail"].startswith("fault at va 8"), (tag, so["code"], so["detail"][:120])
        assert so["profile"] == ("original" if tag == "original" else "fixed")
        assert all(v == 1 for v in so["defects"].values())
    # the recording replays under the switches it recorded; with every switch at 1 it faults (control)
    from port.test import app_replay as AR
    from port.test import lib as L
    lib = L.load(os.environ.get("SG_PORTSIM_DLL"))
    args = dict(trace=(rec / "trace.trace").read_bytes(), framehash=(rec / "framehash.bin").read_bytes(), session=s)
    assert AR.gate_one(lib, "tropical", **args)["status"] == "PASS"
    bad = AR.gate_one(lib, "tropical", **args, switches={})
    assert bad["status"] == "FAIL" and bad["code"] == -3
    assert lib.defects()[SWITCH]["on"] == 1  # every switch back at 1 after a replay


# ---- (d) the app's shadow audio under the two profiles --------------------------------------------------------------------
def test_app_course_effects_sound_under_fixed_and_the_sim_is_unchanged(tmp_path):
    """course_season's corpus trace through the app with the shadow audio in Model mode (S4's device sink, a device present):
    under `fixed` the course effects open through the alias (13 opens in this run, log/roadmap/S7/12_app_course_season.log),
    under `original` none do and more opens fail (the engine retries a failed sound); trace.trace and framehash.bin equal"""
    tr = ROOT / "port" / "corpus" / "traces" / "course_season.trace"
    if not tr.exists():
        pytest.skip("no corpus trace course_season")
    got = {}
    for p in ("fixed", "original"):
        rec, s = live_trace(tmp_path, tr, p, "--audio", "model", "--profile", p)
        a = json.loads((rec / "audio.json").read_text())
        sha = [hashlib.sha256((rec / f).read_bytes()).hexdigest() for f in ("trace.trace", "framehash.bin")]
        got[p] = (s, a, sha)
    (sf, af, hf), (so, ao, ho) = got["fixed"], got["original"]
    assert sf["code"] == so["code"] == 0 and hf == ho
    assert af["failed"] == ao["failed"] == 0 and af["sound_ready"] == ao["sound_ready"] == 1
    assert ao["files_aliased"] == 0 and af["files_aliased"] > 0
    assert af["files_present"] == ao["files_present"] + af["files_aliased"] and af["files_absent"] < ao["files_absent"]
