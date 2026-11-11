"""S7.10 audio out: the app's shadow audio context (port/app/audio_out.h) and its sink (port/app/ds_sink.h).

(a) audio_stream.device_run's sequence through AudioOut in Model mode (simgolf_audio_check `device`) gives the S4 golden
    stream golden_audio_exe.json $device line for line, x64 and Win32.
(b) the shadow's start (the exe's initialised data + audio_exe.constructHolders + the CRT's arrayStaticInit_448160) equals a
    captured image after deviceFixture on every sound object once loadSoundTable ran, but the Snd +4 words startSoundSlot
    writes at run time (portsim_c.dll, SG_PORTSIM_DLL).
(c) a short scripted title-menu session in Capture mode, judged against the install's WAV files decoded here: each sound
    found in the capture by correlation, its level and channel balance, the fades; controls RED (unlock copy skipped, volume
    ignored), pan / frequency controls equivalent on these sounds (the engine sends no SetPan and only the buffers' own
    rate to SetFrequency).
S7.11: (d) no output device: the sink answers the engine's device probes as a PC without one (forced with --no-device /
    --audio-no-device, or this PC's own state), the shadow's initSound takes the original's failure path (eax 3, g_soundReady
    0), the thread ends and the requests are dropped, the sim unchanged; (e) shutdown leaves no thread or handle (counted over
    repeated start / stop cycles and in the app against an audio-free reference), controls RED; (f) --audio defaults to on.
Binaries: SG_AUDIO_CHECK_X64 / SG_AUDIO_CHECK_W32, else the S7.11 / S7.10 mirror's, else the integrator's port/build/<arch>.
"""
from __future__ import annotations

import hashlib
import json
import os
import pathlib
import subprocess
import wave

import numpy as np
import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
INSTALL = ROOT / "original"
EXE = INSTALL / "golf_clean.exe"
IMAGE = ROOT / "port" / "corpus" / "segments" / "sandbox_basic" / "00" / "start.data.bin"
GOLDEN = ROOT / "port" / "test" / "golden_audio_exe.json"
SCRIPT = ROOT / "log" / "roadmap" / "S7" / "10_title.script"
NL = chr(10)

pytestmark = pytest.mark.port


def checker(arch: str) -> pathlib.Path | None:
    env = os.environ.get("SG_AUDIO_CHECK_" + arch.upper())
    cands = [pathlib.Path(env)] if env else []
    sub = "x64" if arch == "x64" else "w32"
    # only the current build (2026-11-04): old step trees (S7_10_root / S7_11_root / S7_12_root) left on disk predate later fixes
    cands += [ROOT / "port" / "build" / d / "app" / "Release" / "simgolf_audio_check.exe"  # integrator / inside a mirror
              for d in (("x64",) if arch == "x64" else ("Win32", "w32"))]
    return next((c for c in cands if c.exists()), None)


def run_check(arch: str, *args: str) -> subprocess.CompletedProcess:
    exe = checker(arch)
    if exe is None:
        pytest.skip(f"no simgolf_audio_check for {arch}")
    if not EXE.exists():
        pytest.skip("no install")
    return subprocess.run([str(exe), *args, "--exe", str(EXE), "--install", str(INSTALL)], capture_output=True, text=True,
                          timeout=300)


# ---- (a) the device stream through the app's sink ---------------------------------------------------------------------
def device_lines(arch: str, tmp: pathlib.Path, *extra: str) -> tuple[list[str], dict]:
    if not IMAGE.exists():
        pytest.skip("no segment corpus")
    out, st = tmp / f"device_{arch}.txt", tmp / f"device_{arch}.json"
    r = run_check(arch, "device", "--image", str(IMAGE), "--out", str(out), "--stats", str(st), *extra)
    assert out.exists(), r.stdout + r.stderr
    return out.read_text().split(NL), json.loads(st.read_text())


@pytest.mark.parametrize("arch", ["x64", "w32"])
def test_device_stream_through_the_app_sink_equals_golden(arch, tmp_path):
    gold = json.loads(GOLDEN.read_text())["$device"]
    lines, st = device_lines(arch, tmp_path)
    assert st["failed"] == 0, st["first_error"]
    assert st["ticks"] == 40 and st["sink"]["buffers"] == 14
    assert len(lines) == gold["lines"]
    assert hashlib.sha256(NL.join(lines).encode()).hexdigest() == gold["stream_sha256"]


@pytest.mark.parametrize("control", ["1", "2"])
def test_device_stream_control_red(control, tmp_path):
    """controls of the shadow's plumbing (AudioOutOptions::control): 1 the timer client is called without its user word,
    2 a replayed request loses its last argument slot"""
    gold = json.loads(GOLDEN.read_text())["$device"]
    lines, _ = device_lines("x64", tmp_path, "--control", control)
    assert hashlib.sha256(NL.join(lines).encode()).hexdigest() != gold["stream_sha256"]


def test_device_stream_tick_clock_is_blind(tmp_path):
    """equivalent: an 11 ms model clock step per tick instead of deviceStep 4's 10 changes no line (no line carries a
    timeGetTime answer and the 40 ticks take no time-dependent branch)"""
    gold = json.loads(GOLDEN.read_text())["$device"]
    lines, _ = device_lines("x64", tmp_path, "--tick-ms", "11")
    assert hashlib.sha256(NL.join(lines).encode()).hexdigest() == gold["stream_sha256"]


# ---- (b) the shadow's start state ----------------------------------------------------------------------------------------
def _holders_diff(neg: int = 0) -> dict:
    from port.test import lib as L
    from port.test import segments as S
    dll = os.environ.get("SG_PORTSIM_DLL")
    if not dll:
        pytest.skip("SG_PORTSIM_DLL (a build with audio_exe.constructHolders)")
    lib = L.load(pathlib.Path(dll), rebuild=False)
    lib.exe_load()
    objs = {n: (o, s) for n, o, s in lib.objects()}
    seg = S.load_segment(S.SEG_DIR / "sandbox_basic" / "00")
    if neg:
        lib.negctl("audio_exe.constructHolders", neg)
    try:
        with lib.state() as a, lib.state() as b:
            for st in (a, b):
                st.set_data_base(0x004C1000)
                st.files_root(S.INSTALL)
            a.load_image(seg.start)
            a.call("audio_exe.deviceFixture", [0])
            b.fill_from_exe(True)
            b.call("arrayStaticInit_448160", [])
            b.call("audio_exe.constructHolders", [])
            b.call("audio_exe.useSink", [0])
            for st in (a, b):
                st.call("loadSoundTable", [])
            ia, ib = a.image(), b.image()
    finally:
        if neg:
            lib.negctl_reset()
    out = {}
    for n in ("g_sfxDevice", "g_musicDevice", "g_voiceDevice", "g_sounds", "g_soundHead", "g_soundTail", "g_soundFns",
              "g_soundReady", "g_soundDll"):
        o, s = objs[n]
        out[n] = [i for i in range(s) if ia[o + i] != ib[o + i]]
    return out


def test_shadow_start_equals_a_captured_image():
    d = _holders_diff()
    # one byte of the +4 word of records 0x2d, 0x33 and 0x80, which the recorded run wrote (Snd::applyField484750's field)
    assert {k: v for k, v in d.items() if v} == {"g_sounds": [0x2d * 0x6c + 4, 51 * 0x6c + 4, 0x80 * 0x6c + 4]}


def test_shadow_start_control_red():
    assert _holders_diff(neg=1)["g_musicDevice"]


# ---- (c) the capture of a scripted title-menu session, judged against the install's WAV files ----------------------------
def read_wav(path: pathlib.Path) -> tuple[np.ndarray, int]:
    with wave.open(str(path)) as w:
        assert w.getsampwidth() == 2
        a = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64)
        return a.reshape(-1, w.getnchannels()), w.getframerate()


def upsample(ref: np.ndarray, factor: int) -> np.ndarray:
    """the reference at the capture's rate: linear interpolation between neighbouring frames (U-1689)"""
    if factor == 1:
        return ref
    x = np.arange(len(ref) * factor) / factor
    return np.stack([np.interp(x, np.arange(len(ref)), ref[:, c]) for c in range(ref.shape[1])], axis=1)


def locate(cap: np.ndarray, ref: np.ndarray, t0: float, rate: int, n: int | None = None, win: float = 0.8) -> dict:
    """the reference's best-correlated position in the capture after t0 (s): lag, per-channel gain and correlation"""
    r = ref[:n] if n else ref
    rm = r.mean(axis=1)
    a = int(t0 * rate)
    seg = cap[a:a + int(win * rate) + len(rm)].mean(axis=1)
    if len(seg) < len(rm) or not np.any(seg):
        return {"t": None, "gain": [0.0, 0.0], "rho": [0.0, 0.0]}
    m = 1 << int(np.ceil(np.log2(len(seg) + len(rm))))  # the valid cross-correlation through the FFT
    xc = np.fft.irfft(np.fft.rfft(seg, m) * np.conj(np.fft.rfft(rm, m)), m)[: len(seg) - len(rm) + 1]
    k = int(np.argmax(xc))
    out = {"t": (a + k) / rate, "gain": [], "rho": []}
    for ch in range(2):
        rr, cc = r[:, min(ch, r.shape[1] - 1)], cap[a + k:a + k + len(r), ch]
        out["gain"].append(float(cc @ rr / (rr @ rr)))
        out["rho"].append(float(cc @ rr / (np.linalg.norm(cc) * np.linalg.norm(rr) + 1e-9)))
    return out


def envelope(cap: np.ndarray, ref: np.ndarray, t: float, rate: int, start: float, span: float, step: float = 0.1) -> list:
    """the capture's level over the located reference (RMS ratio) in `step` windows from start to start + span (s)"""
    out = []
    for k in range(int(span / step)):
        a = int((start + k * step) * rate)
        o = a - int(t * rate)
        c, r = cap[a:a + int(step * rate)], ref[o:o + int(step * rate)]
        out.append(float(np.sqrt((c ** 2).mean()) / (np.sqrt((r ** 2).mean()) + 1e-9)) if len(r) == len(c) else 0.0)
    return out


CLICKS = [(0.0, 100, 0), (0.9, 60, 0), (1.8, 100, -100), (2.7, 100, 100)]   # 10_title.script: request time (s), volume, pan
STATIC = [("sounds/buy2short.wav", 3.6, 5.4), ("sounds/Music Idea.wav", 6.9, 8.7)]  # start and fade (startSoundSlot) times


def judge(wav: pathlib.Path) -> tuple[list[str], dict]:
    """the capture against the install's files: every click found (correlation >= 0.999) with the gain volume / 127 (the exe's
    Snd::setVolume scale) and the pan's side; every static-buffer sound found at unity gain, then faded over several 100 ms
    windows to silence"""
    cap, rate = read_wav(wav)
    bad, facts = [], {"rate": rate, "frames": len(cap)}
    boing, br = read_wav(INSTALL / "sounds" / "effects" / "boing.wav")
    up = rate // br
    ref = upsample(boing, up)
    for i, (t0, vol, pan) in enumerate(CLICKS):
        f = locate(cap, ref, t0, rate)
        facts[f"click{i}"] = f
        if f["t"] is None or not 0.0 <= f["t"] - t0 <= 0.8:
            bad.append(f"click{i}: not found after {t0} s ({f['t']})")
            continue
        sides = [0, 1] if pan == 0 else ([0] if pan < 0 else [1])
        if min(f["rho"][c] for c in sides) < 0.999:
            bad.append(f"click{i}: correlation {f['rho']}")
        for c in sides:  # centred: volume / 127 on both sides; panned: the sounding side not below it, at most full scale
            g, want = f["gain"][c], vol / 127.0
            if (abs(g - want) > 0.01) if pan == 0 else not want - 0.01 <= g <= 1.01:
                bad.append(f"click{i}: channel {c} gain {g:.4f} (volume / 127 = {want:.4f}, pan {pan})")
        if pan != 0 and f["gain"][1 - sides[0]] > 0.01:
            bad.append(f"click{i}: pan {pan} leaves the other side at {f['gain'][1 - sides[0]]:.4f}")
    for name, t0, t_fade in STATIC:
        src, sr = read_wav(INSTALL / name)
        ref = upsample(src[: sr * 12], rate // sr)
        f = locate(cap, ref, t0, rate, n=rate)
        key = pathlib.Path(name).stem
        facts[key] = f
        if f["t"] is None or not 0.0 <= f["t"] - t0 <= 0.8 or min(f["rho"]) < 0.999 or max(abs(g - 1) for g in f["gain"]) > 0.01:
            bad.append(f"{key}: start {f}")
            continue
        env = envelope(cap, ref, f["t"], rate, t_fade, 1.6)
        facts[key + "_fade"] = [round(x, 3) for x in env]
        ramp = [x for x in env if 0.05 < x < 0.9]
        if env[-1] > 0.02 or len(ramp) < 3 or any(b > a + 0.03 for a, b in zip(env, env[1:])):
            bad.append(f"{key}: fade {facts[key + '_fade']}")
    return bad, facts


def capture(arch: str, tmp: pathlib.Path, defect: str = "none") -> tuple[pathlib.Path, dict]:
    wav, st = tmp / f"title_{arch}_{defect}.wav", tmp / f"title_{arch}_{defect}.json"
    r = run_check(arch, "script", "--script", str(SCRIPT), "--mode", "capture", "--wav", str(wav), "--stats", str(st),
                  "--defect", defect)
    assert wav.exists(), r.stdout + r.stderr
    return wav, json.loads(st.read_text())


@pytest.mark.parametrize("arch", ["x64", "w32"])
def test_title_capture_meets_the_install_files(arch, tmp_path):
    wav, st = capture(arch, tmp_path)
    assert st["failed"] == 0, st["first_error"]
    assert st["dropped"] == 0 and st["delivered"] >= 4   # the engine's completion messages reach its window procedure
    bad, facts = judge(wav)
    (tmp_path / "facts.json").write_text(json.dumps(facts, indent=1))
    assert not bad, bad


def test_title_capture_x64_equals_win32(tmp_path):
    a, _ = capture("x64", tmp_path)
    b, _ = capture("w32", tmp_path)
    assert a.read_bytes() == b.read_bytes()


@pytest.mark.parametrize("defect", ["unlock", "volume"])
def test_title_capture_controls_red(defect, tmp_path):
    wav, _ = capture("x64", tmp_path, defect)
    bad, _ = judge(wav)
    assert bad, f"control {defect} read GREEN"


@pytest.mark.parametrize("defect", ["pan", "frequency"])
def test_title_capture_equivalent_controls(defect, tmp_path):
    """equivalent mutants on this session: the engine sends no SetPan to a DS buffer (the clicks' pan is mixed by the engine)
    and SetFrequency only with the buffers' own 22050 Hz, so ignoring either changes no byte (blind; judged by a sound the
    engine pans or pitches at the device, which the scripted title session does not reach)"""
    clean, _ = capture("x64", tmp_path)
    wav, _ = capture("x64", tmp_path, defect)
    assert wav.read_bytes() == clean.read_bytes()


# ---- the app: the sim context with audio out attached --------------------------------------------------------------------
def app_exe() -> pathlib.Path:
    env = os.environ.get("SG_APP_EXE")
    # only the current build (2026-11-04): old step trees (S7_10_root / S7_11_root / S7_12_root) left on disk predate later fixes
    cands = ([pathlib.Path(env)] if env else []) + [
        ROOT / "port" / "build" / "x64" / "app" / "Release" / "simgolf_port.exe"]
    exe = next((c for c in cands if c.exists()), None)
    if exe is None:
        pytest.skip("no simgolf_port.exe")
    return exe


def live_trace(tmp: pathlib.Path, scenario: str, audio: str) -> tuple[str, str, dict]:
    rec = tmp / f"rec_{audio}"
    r = subprocess.run([str(app_exe()), "--install", str(INSTALL), "--data", str(tmp / f"data_{audio}"), "--record-dir", str(rec),
                        "--live-trace", str(ROOT / "port" / "corpus" / "traces" / f"{scenario}.trace"), "--terrain", "headless",
                        "--audio", audio], cwd=str(ROOT), capture_output=True, text=True, timeout=1200)
    assert (rec / "framehash.bin").exists(), r.stdout[-2000:] + r.stderr[-2000:]
    sha = lambda f: hashlib.sha256((rec / f).read_bytes()).hexdigest()  # noqa: E731
    audio_json = json.loads((rec / "audio.json").read_text()) if (rec / "audio.json").exists() else {}
    return sha("trace.trace"), sha("framehash.bin"), audio_json


@pytest.mark.parametrize("scenario", ["sandbox_basic"])
def test_audio_out_leaves_the_sim_unchanged(scenario, tmp_path):
    """--audio capture against --audio off over a corpus trace: the recording and every frame record (203 sim hashes) equal;
    the sim's requests reached the shadow and all ran there"""
    if not (ROOT / "port" / "corpus" / "traces" / f"{scenario}.trace").exists():
        pytest.skip("no corpus trace")
    off = live_trace(tmp_path, scenario, "off")
    cap = live_trace(tmp_path, scenario, "capture")
    assert off[:2] == cap[:2]
    a = cap[2]
    assert a.get("failed") == 0 and a.get("first_error") == "", a
    assert a["requests"] >= 3 and a["replayed"] == a["requests"] + 1   # + the shadow's own initSound
    (tmp_path / "audio.json").write_text(json.dumps(a))


def test_live_window_session_plays_the_title_music(tmp_path):
    """the app's window session with --audio capture for 8 s on the title menu, launched in the Normal priority class as
    app_drive.launch does (U-1395): the boot's requests (arrayStaticInit_448160, loadSoundTable, playSound 0x80) reach the
    shadow; its timer keeps its own rate, (ticks - 1 + skipped periods) over the span from its first tick to its last, within
    2 % of one per 30 ms (the boot's length under load is not judged: at least 100 ticks, 166 measured at 50 % load before
    the time-critical audio thread); the capture holds `Music Idea.wav` at unity gain"""
    import sys
    sys.path.insert(0, str(ROOT / "port" / "app" / "tools"))
    import app_drive
    rec = tmp_path / "rec"
    subprocess.run([str(app_exe()), "--install", str(INSTALL), "--data", str(tmp_path / "data"), "--record-dir", str(rec),
                    "--audio", "capture", "--exit-after", "8000", "--no-activate"], cwd=str(ROOT), capture_output=True,
                   text=True, timeout=120, creationflags=app_drive.priority_flags())
    a = json.loads((rec / "audio.json").read_text())
    (tmp_path / "audio.json").write_text(json.dumps(a))
    assert a["failed"] == 0 and a["requests"] >= 3, a
    assert a["ticks"] >= 100, a
    periods = a["ticks"] - 1 + a["skipped_periods"]
    period_ms = a["tick_span_ns"] / 1e6 / periods
    assert abs(period_ms - 30.0) <= 0.6, (period_ms, a)
    cap, rate = read_wav(rec / "audio.wav")
    src, sr = read_wav(INSTALL / "sounds" / "Music Idea.wav")
    f = locate(cap, upsample(src[: sr * 4], rate // sr), 0.0, rate, n=rate, win=3.0)
    assert f["t"] is not None and min(f["rho"]) > 0.999 and max(abs(g - 1) for g in f["gain"]) < 0.01, f


# ---- S7.11 (d) no output device -----------------------------------------------------------------------------------------
# The device probes the engine makes in initSound on a PC without an output device, in order (scenario_baseline.md section 1,
# measured on the original with log/c3/b2_soundprobe.js): timeGetDevCaps, DirectSoundEnumerateA (#2), CoInitialize,
# CoCreateInstance, IDirectSound::Initialize (com slot 10) DSERR_NODRIVER, its Release (com slot 2) 0.
NO_DEVICE_PROBES = ["WINMM.dll!timeGetDevCaps", "DSOUND.dll!#2", "ole32.dll!CoInitialize", "ole32.dll!CoCreateInstance",
                    "com 1.10", "com 1.2"]


def script_run(arch: str, tmp: pathlib.Path, tag: str, *extra: str) -> tuple[dict, list[str], subprocess.CompletedProcess]:
    st, lines = tmp / f"{tag}_{arch}.json", tmp / f"{tag}_{arch}.lines"
    r = run_check(arch, "script", "--script", str(SCRIPT), "--stats", str(st), "--lines", str(lines), *extra)
    assert st.exists(), r.stdout + r.stderr
    return json.loads(st.read_text()), lines.read_text().split(NL), r


def device_less_ok(s: dict, lines: list[str]) -> list[str]:
    """what the no-device gate requires of one shadow; the list of failed requirements (empty = GREEN)"""
    bad = []
    if s["init_eax"] != 3 or s["sound_ready"] != 0 or not s["device_less"]:
        bad.append("initSound did not take the device-less path")
    if not s["thread_ended"] or s["ticks"] != 0 or s["replayed"] != 1:
        bad.append("the shadow kept working after initSound")
    if s["dropped_requests"] != s["requests"] - 1 or s["failed"] != 0:
        bad.append("requests were not dropped")
    pos, k = [], 0
    for want in NO_DEVICE_PROBES:
        while k < len(lines) and want not in lines[k]:
            k += 1
        pos.append(k)
        k += 1
    if pos[-1] >= len(lines):
        bad.append("probe order differs from the measured device-less path")
    if any(ln.startswith("com 1.3 ") for ln in lines):
        bad.append("a sound buffer was created")
    c = s["counts"]
    if c["threads_added"] != 0 or c["handles_after"] != c["handles_before"]:
        bad.append("a thread or handle was left")
    return bad


@pytest.mark.parametrize("arch", ["x64", "w32"])
def test_no_device_takes_the_original_path(arch, tmp_path):
    """the capture device forced to fail (--no-device 1): the shadow's initSound returns 3 with g_soundReady 0 after exactly
    the measured probe sequence, makes no buffer, its thread ends and the later requests are dropped; nothing is left"""
    s, lines, r = script_run(arch, tmp_path, "nodev", "--mode", "capture", "--no-device", "1")
    assert r.returncode == 0, r.stdout + r.stderr
    assert device_less_ok(s, lines) == [], (s, lines[:40])
    assert s["no_device_answers"] == 2 and s["sink"]["buffers"] == 0, s


def test_no_device_control_keep_thread_red(tmp_path):
    """control 3 (the device-less shadow keeps its thread and replays every request) fails the gate"""
    s, lines, _ = script_run("x64", tmp_path, "nodev_c3", "--mode", "capture", "--no-device", "1", "--control", "3")
    assert s["device_less"] and "the shadow kept working after initSound" in device_less_ok(s, lines), s


def test_dsound_on_this_pc(tmp_path):
    """--mode dsound without forcing: on a PC without an output device the real DirectSoundCreate fails and the shadow takes
    the same device-less path; with a device the real buffers are made (the listening check is Mariano's, U-1691)"""
    s, lines, r = script_run("x64", tmp_path, "dsound", "--mode", "dsound")
    assert r.returncode == 0, r.stdout + r.stderr
    if s["sink"]["first_failure"].startswith("DirectSoundCreate"):
        bad = device_less_ok(s, lines)
        # the first DirectSoundCreate of a process loads the system's audio DLLs, which keep their handles (constant over
        # later cycles: test_shutdown_leaves_nothing); only that requirement is waived here
        assert bad in ([], ["a thread or handle was left"]), (bad, s)
    else:
        assert not s["device_less"] and s["sink"]["real_buffers"] > 0 and s["failed"] == 0, s


# ---- S7.11 (e) shutdown -------------------------------------------------------------------------------------------------
@pytest.mark.parametrize("arch", ["x64", "w32"])
@pytest.mark.parametrize("mode", ["capture", "dsound"])
def test_shutdown_leaves_nothing(arch, mode, tmp_path):
    """four start .. stop cycles in one process (the title script, shutdownSoundSystem, the device and buffers released, the
    thread joined): no thread id is left that was not there before the cycle (Toolhelp32; the count alone moves with the
    system's thread-pool workers) and the handle count stops growing after the first cycle (a first DirectSoundCreate loads
    the system's audio DLLs once)"""
    r = run_check(arch, "script", "--script", str(SCRIPT), "--mode", mode, "--cycles", "4")
    rows = [ln.split() for ln in r.stdout.split(NL) if ln.startswith("cycle ")]
    assert len(rows) == 4 and r.returncode == 0, r.stdout + r.stderr
    cnt = lambda row, key: [int(x) for x in row[row.index(key) + 1].split("->")]  # noqa: E731
    assert all(int(row[row.index("new") + 1]) == 0 for row in rows), r.stdout  # no thread id that was not there before
    hs = [cnt(row, "handles") for row in rows]
    assert all(h1 == h0 for h0, h1 in hs[1:]), r.stdout


def test_shutdown_control_leak_red(tmp_path):
    """control 4 (stop() leaves the thread running) leaves one thread more after the cycle"""
    r = run_check("x64", "script", "--script", str(SCRIPT), "--mode", "capture", "--control", "4")
    row = next(ln.split() for ln in r.stdout.split(NL) if ln.startswith("cycle "))
    assert int(row[row.index("new") + 1]) == 1, r.stdout


# ---- S7.11 the app: no device, the default, shutdown counts against an audio-free reference ------------------------------
def live_trace_args(tmp: pathlib.Path, scenario: str, tag: str, *args: str) -> tuple[str, str, dict]:
    rec = tmp / f"rec_{tag}"
    r = subprocess.run([str(app_exe()), "--install", str(INSTALL), "--data", str(tmp / f"data_{tag}"), "--record-dir", str(rec),
                        "--live-trace", str(ROOT / "port" / "corpus" / "traces" / f"{scenario}.trace"), "--terrain", "headless",
                        *args], cwd=str(ROOT), capture_output=True, text=True, timeout=1200)
    assert (rec / "framehash.bin").exists(), r.stdout[-2000:] + r.stderr[-2000:]
    sha = lambda f: hashlib.sha256((rec / f).read_bytes()).hexdigest()  # noqa: E731
    audio_json = json.loads((rec / "audio.json").read_text()) if (rec / "audio.json").exists() else {}
    return sha("trace.trace"), sha("framehash.bin"), audio_json


HANDLE_SLACK = 4  # handles one run may differ from the audio-free reference (measured -3 .. +3 under load, see below)


def test_no_device_app_leaves_the_sim_unchanged(tmp_path):
    """sandbox_basic's corpus trace with --audio on and the device forced to fail, with the default (no --audio: on, this
    PC's real device state) and with --audio capture: the recording and every frame record equal --audio off's; the forced
    run is device-less, its thread ended, and it leaves no thread and no handle beyond the audio-free reference's own
    (--audio off --negctl audio-count-only counts the same span)"""
    if not (ROOT / "port" / "corpus" / "traces" / "sandbox_basic.trace").exists():
        pytest.skip("no corpus trace")
    ref = live_trace_args(tmp_path, "sandbox_basic", "off", "--audio", "off", "--negctl", "audio-count-only")
    nod = live_trace_args(tmp_path, "sandbox_basic", "nodev", "--audio", "on", "--audio-no-device")
    dflt = live_trace_args(tmp_path, "sandbox_basic", "default")
    cap = live_trace_args(tmp_path, "sandbox_basic", "capture", "--audio", "capture")
    assert ref[:2] == nod[:2] == dflt[:2] == cap[:2]
    r, a = ref[2], nod[2]
    (tmp_path / "audio.json").write_text(json.dumps({"ref": r, "nodev": a, "default": dflt[2], "capture": cap[2]}))
    assert a["mode"] == "on" and a["init_eax"] == 3 and a["sound_ready"] == 0 and a["device_less"] and a["thread_ended"], a
    assert a["ticks"] == 0 and a["replayed"] == 1 and a["dropped_requests"] == a["requests"] - 1 and a["failed"] == 0, a
    assert dflt[2]["mode"] == "on" and dflt[2]["failed"] == 0, dflt[2]   # (f) the default is on
    for run in (a, cap[2]):
        assert run["threads_added"] == r["threads_added"] == 0, (run, r)
        # within HANDLE_SLACK of the reference's own growth: one process's handle count moves with the system's thread-pool
        # workers under load (lead, 2026-10-30, Win32 under two other heavy jobs: no-device 3 below the reference, capture 3
        # above it). The leak proofs are threads_added (above) and test_shutdown_leaves_nothing's constant count over cycles.
        assert run["handles_after"] - run["handles_before"] <= r["handles_after"] - r["handles_before"] + HANDLE_SLACK, (run, r)


def test_app_controls_red(tmp_path):
    """the app's two shutdown controls: audio-leak-thread leaves one thread; audio-keep-thread keeps the device-less shadow's
    thread replaying requests"""
    if not (ROOT / "port" / "corpus" / "traces" / "sandbox_basic.trace").exists():
        pytest.skip("no corpus trace")
    leak = live_trace_args(tmp_path, "sandbox_basic", "leak", "--audio", "capture", "--negctl", "audio-leak-thread")[2]
    keep = live_trace_args(tmp_path, "sandbox_basic", "keep", "--audio", "on", "--audio-no-device", "--negctl",
                           "audio-keep-thread")[2]
    assert leak["threads_added"] == 1, leak
    assert keep["device_less"] and not keep["thread_ended"] and keep["replayed"] == keep["requests"] + 1, keep


def window_close_run(tmp: pathlib.Path, tag: str, *extra: str) -> dict:
    import sys
    sys.path.insert(0, str(ROOT / "port" / "app" / "tools"))
    import app_drive
    rec = tmp / f"rec_{tag}"
    subprocess.run([str(app_exe()), "--install", str(INSTALL), "--data", str(tmp / f"data_{tag}"), "--record-dir", str(rec),
                    "--exit-after", "5000", "--no-activate", *extra], cwd=str(ROOT), capture_output=True, text=True,
                   timeout=120, creationflags=app_drive.priority_flags())
    return json.loads((rec / "audio.json").read_text())


def test_window_close_stops_the_audio_first(tmp_path):
    """the window session closed before the game quits (--exit-after: WM_CLOSE): the shadow is stopped inside WM_CLOSE,
    before DestroyWindow (gameMain's order: shutdownSoundSystem after mainLoop, before the window goes), the capture is
    still written and no thread is left; control audio-close-late (no WM_CLOSE hook) stops it after the window is gone"""
    a = window_close_run(tmp_path, "close", "--audio", "capture")
    assert a["closed_early"] and a["window_at_stop"] == 1 and a["threads_added"] == 0 and a["failed"] == 0, a
    assert (tmp_path / "rec_close" / "audio.wav").exists()
    late = window_close_run(tmp_path, "late", "--audio", "capture", "--negctl", "audio-close-late")
    assert not late["closed_early"] and late["window_at_stop"] == 0, late
