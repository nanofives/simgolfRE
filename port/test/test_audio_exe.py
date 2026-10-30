"""The exe's sound layer on the port's sound.dll engine (step S4.10, log/roadmap/S4/10_notes.md): module audio_exe.

What is judged:
  (a) every C3 / C4 backend-audio row of golf_clean.exe (re/port/scope.tsv) and the C2 forwarders and helpers with no
      registry key (the `s410:` keys of port/test/audio_stream.py) against the EMULATED ORIGINAL (decision G1): the fixture
      VM replays each key's fixture, the original exe runs in Unicorn and the port runs as a second arm on the same memory;
      return value, state regions and the exe's own writes must be equal, and the registry keys are tied to their path-1
      CSVs on the address-independent fields (sound_census's tiers EXACT / EQUAL_PI). Default runs replay the recorded
      bundles (port/corpus/audio_exe, local) through the script host and check the golden digests pinned from the oracle
      (port/test/golden_audio_exe.json); SG_EMU_ORACLE=1 runs the live oracle on a sample of keys.
  (b) the segments' AudioSink command stream: test_segment_stream runs two accepted segments with the module attached and
      checks the digest stays the baseline one and the stream holds the exe's sound requests (the 205-segment gate is
      `python -m port.test.audio_stream stream`, log/roadmap/S4/10_streams_x64.tsv).
  (c) the device-present path: on a segment start image with the sound globals cleared, initSound / loadSoundTable / every
      playable slot's playSound / 40 multimedia timer ticks / shutdownSoundSystem run through the port's sound.dll engine
      into the DeviceSink with no host and no fallback, and the rendered DirectSound buffers hold sound.

    ~/bin/py -3.12 -m pytest port/test/test_audio_exe.py
    SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_exe.py -k oracle

One test file per process (the whole port/test suite in one process exhausts memory).
"""
from __future__ import annotations

import csv
import json
import os
import pathlib
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from port.test import audio_stream as AS  # noqa: E402
from port.test import lib as L  # noqa: E402

pytestmark = pytest.mark.port
ORACLE = os.environ.get("SG_EMU_ORACLE") == "1"
GOLD = AS.load_golden()
KEYS = sorted(k for k in GOLD if not k.startswith("$"))   # "$device": gate (c)'s pinned stream
BASELINE = ROOT / "log" / "roadmap" / "S0" / "1_segs_x64.tsv"

# negative controls that the recorded vectors cannot see, with the reason (every other port function reads RED in mode 1)
NEGCTL_BLIND: dict[str, str] = {}


@pytest.fixture(scope="module")
def lib():
    lb = AS.load_lib()
    if "playSound" not in lb.functions() or "audio_exe.deviceStep" not in lb.functions():
        pytest.skip("the DLL has no audio_exe module")
    yield lb
    lb.negctl_reset()


def _functions() -> dict[str, list[str]]:
    out: dict[str, list[str]] = {}
    for k in KEYS:
        out.setdefault(GOLD[k]["name"], []).append(k)
    return out


# ---- static -----------------------------------------------------------------------------------------------------------
def test_every_audio_row_registered(lib):
    """every C3 / C4 backend-audio row of the exe that this step owns is a registered port function"""
    fns = lib.functions()
    rows = [r for r in AS.audio_rows() if r["level"] in ("C3", "C4") and r["name"] not in AS.OTHER_OWNERS]
    assert rows
    missing = sorted(r["name"] for r in rows if r["name"] not in fns)
    assert not missing, missing


def test_every_owned_key_pinned():
    """every registry key of those rows and every custom key has a golden entry (the oracle read it GREEN)"""
    want = set(AS.exe_keys()) | set(AS.CUSTOM)
    assert want, "no keys"
    assert not sorted(want - set(KEYS)), sorted(want - set(KEYS))


def test_golden_tiers():
    """registry keys tie to their live CSV (EXACT / EQUAL_PI); custom keys have no CSV (EMU_ONLY)"""
    bad = {k: g["tier"] for k, g in GOLD.items() if k in KEYS
           if g["tier"] not in (("EMU_ONLY",) if k in AS.CUSTOM else ("EXACT", "EQUAL_PI"))}
    assert not bad, bad
    pending = {k: g["fallback_names"] for k, g in GOLD.items() if k in KEYS and g["exe_fallbacks"]}
    assert not {k: n for k, n in pending.items() if set(n.split()) - PENDING_CALLEES}, pending


# exe callees another module owns, served by the replay host until it registers them (log/roadmap/S4/10_merge.md): the
# constant default of the window-class tables (a slot of the Snd tables) and the engine-glue NodeList destructors the three
# device-holder exit routines tail-jump into
PENDING_CALLEES = {"return0_479820", "NodeList::dtor", "NodeListB::dtor", "NodeListC::dtor"}


# ---- (a) replay of the pinned keys --------------------------------------------------------------------------------------
@pytest.mark.parametrize("key", KEYS)
def test_replay(lib, key):
    if AS.load_bundle(key) is None:
        pytest.skip("no bundle (port/corpus/audio_exe is local: run `python -m port.test.audio_stream oracle --update`)")
    r = AS.replay_key(lib, key)
    assert r.vectors == GOLD[key]["vectors"] and r.passed == r.vectors, r.first


@pytest.mark.parametrize("fn", sorted(_functions()))
def test_negctl_red(lib, fn):
    """mode 1 of the function's negative control changes at least one pinned digest"""
    if fn in NEGCTL_BLIND:
        pytest.skip(NEGCTL_BLIND[fn])
    keys = [k for k in _functions()[fn] if AS.load_bundle(k) is not None]
    if not keys:
        pytest.skip("no bundle")
    red = 0
    for k in keys:
        r = AS.replay_key(lib, k, (fn, 1))
        red += r.passed < r.vectors
        lib.negctl_reset()
    assert red, f"{fn}: mode 1 reads GREEN on {len(keys)} keys"


# ---- (a) the live oracle (opt-in) ---------------------------------------------------------------------------------------
ORACLE_SAMPLE = ["playSound", "loadSoundTable", "initSound", "Snd::setVolume", "Snd::play", "shutdownSound",
                 "s410:Snd::fwdDev49"]


@pytest.mark.skipif(not ORACLE, reason="SG_EMU_ORACLE=1 runs the emulated original")
@pytest.mark.parametrize("key", [k for k in ORACLE_SAMPLE if k in GOLD])
def test_oracle(lib, key):
    from port.test import sound_emu as E
    with E.FixVM() as vm:
        v, _, _ = AS.oracle_key(vm, lib, key)
    assert v.verdict.startswith("GREEN"), (v.verdict, v.first)
    assert v.golden == GOLD[key]["digests"]


# ---- (b) the segments' command stream -----------------------------------------------------------------------------------
@pytest.mark.parametrize("seg", ["championship/01", "course_season/01"])
def test_segment_stream(tmp_path, monkeypatch, seg):
    monkeypatch.setattr(AS, "STREAM_DIR", tmp_path)
    from port.test import integrator as I
    spec = I.LibSpec(dll=str(AS.mirror_dll()))
    row = AS._stream_one(spec, seg)
    assert row["status"] == "PASS", row.get("detail") or row.get("run_detail")
    base = {r["name"]: r["digest"] for r in csv.DictReader(open(BASELINE), delimiter="\t")} if BASELINE.exists() else {}
    if seg in base:
        assert row["digest"] == base[seg]
    assert row["requests"] > 0 and row["unmatched_frames"] == 0
    lines = (tmp_path / (seg.replace("/", "__") + ".txt")).read_text().splitlines()
    assert lines[0].startswith("frame ") and any(ln.startswith("req playSound ") for ln in lines)


# ---- (c) the device-present path ----------------------------------------------------------------------------------------
def _device_run(lib, plays=300, ticks=40, neg=None):
    return AS.device_run(lib, plays, ticks, neg)


def _unlocks(lines):
    out = []
    for ln in lines:
        if "; unlock " in ln:
            out.append(int(ln.split(" bytes, ")[1].split(" not silent")[0]))
    return out


def test_device_present_end_to_end(lib):
    out, rep, lines = _device_run(lib)
    assert out[0] == 0 and out[1] == 0 and out[4] == 0, out           # initSound, loadSoundTable, shutdownSoundSystem
    assert out[3] == 40
    assert rep["fallbacks"] == "0" and rep["replay"] == "0", rep
    plays = [ln for ln in lines if ln.startswith("import golf_clean.exe!playSound ")]
    assert len(plays) == 178                                            # the slots whose file the install holds (U-1499)
    assert any(ln.startswith("import DSOUND.dll!") for ln in lines)          # the DirectSound object
    assert any(ln.startswith("import WINMM.dll!timeSetEvent ") for ln in lines)
    assert any(ln.startswith("import WINMM.dll!mmioOpenA ") for ln in lines)
    loud = [n for n in _unlocks(lines) if n > 0]
    assert len(loud) >= 10, _unlocks(lines)
    assert sum(1 for ln in lines if ln.startswith("tick ")) == 40
    assert not any(ln.startswith(("fault ", "missing ")) for ln in lines)
    g = json.loads(AS.GOLDEN.read_text()).get("$device", {}) if AS.GOLDEN.exists() else {}
    if g:
        assert AS.device_sha(lines) == g["stream_sha256"]


@pytest.mark.parametrize("fn", ["playSound", "Snd::applyMode484e30", "loadSoundTable", "shutdownSoundSystem", "soundVersionCheck",
                                "snd.Wave_Device::get_rate"])
def test_device_negctl_red(lib, fn):
    """a wrong body changes the device stream (the pinned stream digest), or stops the chain"""
    g = json.loads(AS.GOLDEN.read_text()).get("$device", {}) if AS.GOLDEN.exists() else {}
    if not g:
        pytest.skip("no pinned device stream")
    try:
        _, _, lines = _device_run(lib, neg=(fn, 1))
    except L.SgError:
        return
    assert AS.device_sha(lines) != g["stream_sha256"]
