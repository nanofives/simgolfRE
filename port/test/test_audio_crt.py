"""sound.dll's static C runtime and the loaders' four helpers (step S4.11, log/roadmap/S4/11_notes.md): module audio_crt, every
function registered as `snd.<name>`, judged against the emulated original (decision G1: no hook source for most of them) by
port/test/sound_replay.py (`--step S4.11`) over the keys of port/test/sound_keys_crt.py and the fixtures of
port/test/sound_fixtures/s411_crt.js.

Default runs replay the bundles (port/corpus/audio, local) against port/test/golden_audio_crt.json on x64 and Win32 and run every
function's negative control (mode 1) against the goldens. The golden file pins 0 fallbacks: every call a function makes reaches
another port function, a fixture stand-in, an import stand-in or the host's fake KERNEL32. SG_EMU_ORACLE=1 adds the live oracle
per key.

    ~/bin/py -3.12 -m pytest port/test/test_audio_crt.py
    SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_crt.py -k oracle

One test file per process (the whole port/test suite in one process exhausts memory).
"""
from __future__ import annotations

import os
import pathlib
import re
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from port.test import sound_keys_crt as K  # noqa: E402
from port.test import sound_replay as R  # noqa: E402

pytestmark = pytest.mark.port
STEP = K.STEP
ORACLE = os.environ.get("SG_EMU_ORACLE") == "1"
K.install(R)
KEYS = R.keys_for(STEP)
SRC = ROOT / "port" / "src" / "sim" / "audio_crt.cpp"
NAMES = ROOT / "re" / "names" / "sound_19.tsv"
# the registry keys of the C3 batches c3fo / c3fz whose vectors make the stub raise (U-1540); everything else of the rows is a key
EXCLUDED = set(K.EXCLUDED)
# conditional jumps of the originals no key reaches on one side (11_notes.md section 4; every other jcc of the 28 functions has both
# sides in the emulated original's coverage of the keys): small-block heap arms past what the stubs reach and the three
# sites nothing can reach in this build
ONE_SIDED_NOTE = "11_notes.md section 4"


def registered() -> list[str]:
    t = SRC.read_text(encoding="utf-8")
    return re.findall(r'\{"(snd\.[^"]+)", "(?:default|stdcall|thiscall)"', t)


@pytest.fixture(scope="module")
def sndlib():
    lib = R.load_lib()
    if "snd.strstr" not in lib.functions():
        pytest.skip("the DLL has no audio_crt module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def sndlib32():
    from port.test import lib32 as L32
    try:
        lib = R.load_w32()
    except (L32.RunnerUnavailable, FileNotFoundError, OSError) as e:
        pytest.skip(f"Win32 backend unavailable: {e}")
    if "snd.strstr" not in lib.functions():
        pytest.skip("the Win32 DLL has no audio_crt module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def golden():
    g = R.load_golden(STEP)
    if not g:
        pytest.skip(f"no golden digests (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step {STEP} --update)")
    return g


def _need_bundles():
    for k in KEYS:
        if R.load_bundle(k) is None:
            pytest.skip(f"no bundle for {k} (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step {STEP} --update)")


# ================================================================================================ static
@pytest.mark.static
def test_every_function_is_registered_once_and_named():
    names = registered()
    assert len(names) == len(set(names)) == 28
    assert {n[len("snd."):] for n in names} == {n for _, n in K.FUNCTIONS}
    rows = {line.split("\t")[2] for line in NAMES.read_text(encoding="utf-8").splitlines()[1:] if line.startswith("sound.dll")}
    # the names this step introduced have a row in re/names/sound_19.tsv; the others are hooks.csv / earlier names files' rows
    for n in ("GlblHeader::init", "Sound::getField1b8", "MixBuffer::deletingDtor", "Sound::queueMidiCommand3", "ChannelDesc_dtor",
              "VoiceBase_initListSlot", "Ch5bb90_set3DBuffer", "_strrchr", "operator_new", "_malloc", "__nh_malloc", "__callnewh",
              "__lock", "_lockexit", "_unlockexit", "_atexit", "__onexit", "_sprintf", "ehVectorDestructorIterator"):
        assert n in rows, n


@pytest.mark.static
def test_the_step_owns_its_rows():
    rows = {r["name"]: r for r in R.step_rows(STEP)}
    assert set(rows) == {n for _, n in K.FUNCTIONS}


@pytest.mark.static
def test_goldens_cover_every_key_and_pin_zero_fallbacks():
    g = R.load_golden(STEP)
    if not g:
        pytest.skip("no golden file")
    assert len(KEYS) == 61
    for k in KEYS:
        assert k in g, k
        assert g[k]["vectors"] == len(R.spec_of(k)["vectors"]) == len(g[k]["digests"]) == len(g[k]["x87_exit"]), k
        assert not g[k].get("fallbacks"), (k, g[k].get("fallbacks"))
        assert not g[k].get("pending"), k
    assert sum(g[k]["vectors"] for k in KEYS) == 672


@pytest.mark.static
def test_excluded_keys_are_the_raising_ones():
    assert EXCLUDED == {"ehVectorConstructorIterator_snd_t0", "ehVectorConstructorIterator_snd_t3", "sound___ArrayUnwind_t0",
                        "sound___ArrayUnwind_t2"}
    for k in EXCLUDED:
        assert k not in KEYS


@pytest.mark.static
def test_no_original_address_literal_in_the_module():
    """test_static's rule: no 0x4xxxxx-0x8xxxxx immediate (golf_clean.exe addresses) in port code, comments included."""
    t = SRC.read_text(encoding="utf-8")
    assert not re.findall(r"0x00[4-8][0-9a-f]{5}\b", t)


# ================================================================================================ the module in the library
def test_module_registers_every_function(sndlib):
    have = set(sndlib.functions())
    assert set(registered()) <= have


@pytest.mark.parametrize("key", KEYS)
def test_key_equals_golden_x64(sndlib, golden, key):
    _need_bundles()
    r = R.replay_key(sndlib, key, golden)
    assert R.replay_ok(r), r


@pytest.mark.x87
def test_every_key_equals_golden_win32(sndlib32, golden):
    _need_bundles()
    bad = []
    for key in KEYS:
        r = R.replay_key(sndlib32, key, golden)
        if not R.replay_ok(r):
            bad.append((key, r.passed, r.vectors, r.first))
    assert not bad, bad


@pytest.mark.parametrize("fn", sorted(registered()))
def test_negctl_mode1_red_against_golden(sndlib, golden, fn):
    """Mode 1 of every function (one wrong detail, documented at its body): at least one vector of the function's own keys no longer
    hashes to the emulated original's digest."""
    _need_bundles()
    plan = R.negctl_plan(KEYS, 1, STEP)
    keys = plan[fn]
    sndlib.negctl(fn, 1)
    try:
        bad = sum(r.vectors - r.passed for r in (R.replay_key(sndlib, k, golden) for k in keys))
    finally:
        sndlib.negctl(fn, 0)
    assert bad > 0, f"{fn}: mode 1 reads GREEN on every vector of {keys}"


# ================================================================================================ the live oracle (opt-in)
@pytest.fixture(scope="module")
def vm():
    if not ORACLE:
        pytest.skip("SG_EMU_ORACLE=1 runs the emulator")
    with R.E.FixVM() as v:
        yield v


@pytest.mark.parametrize("key", KEYS)
def test_oracle_key(vm, sndlib, key):
    v, _, _ = R.oracle_key(vm, sndlib, key)
    assert v.verdict == "GREEN" and v.errors == 0 and not v.fallbacks, v
    g = R.load_golden(STEP).get(key)
    if g and not v.forgiven:
        assert v.golden == g["digests"], "the emulated original no longer gives the pinned digests"
