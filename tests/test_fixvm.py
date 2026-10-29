"""Fixture VM + emulated original sound.dll (step S4.1; re/analysis/data/sound_replay.md).

No game: Node runs the A/B fixtures over a mock Frida API (re/tools/fixvm), Unicorn runs the user's original\\sound.dll
relocated (port/test/sound_emu.py). Every replay is compared with the live path-1 CSV in log/diff on the fields that do not
depend on run addresses (re/tools/sound_census.py). Needs Node (SG_NODE, PATH, or C:\\Program Files\\nodejs).
"""
from __future__ import annotations

import os
import pathlib
import struct
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "port" / "test"))
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(ROOT / "re" / "tools" / "fixvm"))

pytestmark = pytest.mark.static

import sound_emu as E  # noqa: E402

HAVE_NODE = os.path.exists(E.node_exe())
HAVE_DLL = E.SOUND_DLL.exists()
needs_vm = pytest.mark.skipif(not (HAVE_NODE and HAVE_DLL), reason="needs Node and original/sound.dll")

# gate (b): one key per fixture style. EXACT = every field of every vector compared and equal (no run address in any
# region); EQUAL_PI = every position-independent field equal, the state hashes hold run addresses (mmio: the block keeps
# absolute buffer pointers and the import log records caller addresses).
STYLE_KEYS = {
    "pure memory": ("Channel_setPanGains", "EXACT"),
    "sound.dll globals": ("codecProcessBlock", "EXACT"),
    "NativeCallback vtables": ("Midi_Device::setChannelVolume", "EXACT"),
    "heap stand-in": ("Channel_setFormat_oom", "EXACT"),
    "import stand-in": ("Midi_Device::flushMessages_h", "EXACT"),
    "mmio stand-in": ("parseRiffWave", "EQUAL_PI"),
    "Interceptor.replace stubs": ("Channel_start_null", "EXACT"),
    "frozen thread": ("Channel_setRateGlide", "EXACT"),
}


@pytest.fixture(scope="module")
def vm():
    if not (HAVE_NODE and HAVE_DLL):
        pytest.skip("needs Node and original/sound.dll")
    with E.FixVM() as v:
        yield v


# ---- pieces that need no VM ------------------------------------------------------------------------------------------
@pytest.mark.skipif(not HAVE_DLL, reason="needs original/sound.dll")
def test_relocation_applies_every_highlow_entry():
    data0, relocs, *_ = E._raw(str(E.SOUND_DLL))
    assert len(relocs) == 5769          # of 5,816 entries: 5,769 HIGHLOW (type 3) + 47 type-0 block pads
    img = E.load_image(0x04C00000)
    delta = (0x04C00000 - 0x10000000) & 0xFFFFFFFF
    for o in relocs[::97]:
        a, b = struct.unpack_from("<I", data0, o)[0], struct.unpack_from("<I", img.data, o)[0]
        assert b == (a + delta) & 0xFFFFFFFF
    same = E.load_image(0x10000000)
    assert same.data == data0


def test_js_number_formats():
    assert [E.js_number(x) for x in (0.0, -0.0, 0.5, 1e21, 1e-7, 123.5, 1e20, 0.3333333333333322)] == \
        ["0", "0", "0.5", "1e+21", "1e-7", "123.5", "100000000000000000000", "0.3333333333333322"]
    # the 2026-10-14 Frida runtime's printing, measured on c3fh's CSVs
    for v, s in ((0.8, "0.80000000000000004"), (1.1, "1.1000000000000000"), (0.5, "0.5"), (-1.7, "-1.6999999999999999"),
                 (1e308, "1.0000000000000000e+308"), (4.440892098500626e-16, "4.4408920985006261e-16")):
        assert E.js_number_trunc17(v) == s
    assert E.format_double(float("inf"), "shortest") == "NaN"


def test_fnv_matches_diff_hook():
    # diff_hook.js fnv(): FNV-1a 32 over the hex text
    assert E.fnv("") == "811c9dc5"
    assert E.fnv("00|ff") == "%08x" % _fnv_ref(b"00|ff")


def _fnv_ref(b):
    h = 0x811C9DC5
    for c in b:
        h = ((h ^ c) * 0x01000193) & 0xFFFFFFFF
    return h


def test_census_has_a_row_for_every_key():
    tsv = ROOT / "re" / "roadmap" / "S4_census.tsv"
    lines = tsv.read_text().splitlines()
    head = lines[0].split("\t")
    rows = {l.split("\t")[0]: dict(zip(head, l.split("\t"))) for l in lines[1:]}
    keys = E.sound_keys()
    assert len(keys) == 980
    assert set(keys) <= set(rows)
    for k in keys:
        r = rows[k]
        assert r["fixture_status"] in ("OK", "FIXTURE_ERR"), k
        if r["fixture_status"] != "OK" or r["tier"] in ("EMU_ERR", "PARTIAL_ERR"):
            assert r["missing_api"] or r["first_diff"], f"{k}: no missing API named"


def test_unicorn_x87_facts():
    """What part (d) measured: Unicorn 2.1.4 applies the precision control and sets no x87 exception flag. If an upgrade
    changes either, the replay's x87 notes in sound_replay.md must be revisited."""
    import x87_probe_unicorn as X
    p = X.probes()
    assert all("honours PC" in r[1] for r in p[:3])
    assert "PE=0" in p[3][2] and "IE=0" in p[4][2]


# ---- replays ---------------------------------------------------------------------------------------------------------
@needs_vm
def test_vm_mock_api(vm):
    run = E.KeyRun(vm, "randRange", "A", init=False)
    run.setup()
    ev = lambda src: vm.request({"op": "eval", "src": src})["value"]   # noqa: E731
    assert ev("ptr(0xffffffff).add(2).toString()") == "0x1"
    assert ev("ptr(0x10).sub(0x20).toString(16)") == "fffffff0"
    assert ev("(function(){ const p = Memory.alloc(8); p.writeDouble(Infinity); return [p.readU32(), p.add(4).readU32()]; })()") \
        == [0, 0x7FF80000]
    assert ev("Process.getModuleByName('sound.dll').base.toUInt32()") == E.LAYOUTS["A"]["base"]
    assert ev("Process.getCurrentThreadId()") == E.TID


@needs_vm
@pytest.mark.parametrize("style", sorted(STYLE_KEYS))
def test_style_replays_equal_csv(vm, style):
    import sound_census as SC
    key, want = STYLE_KEYS[style]
    r = SC.census_key(vm, key)["row"]
    assert r["tier"] == want, (style, key, r["tier"], r["first_diff"])
    assert int(r["emu_errors"]) == 0
    assert int(r["ret_cmp"]) + (int(r["state_cmp"]) if r["state_cmp"] != "-" else 0) > 0
    assert r["ret_cmp"] == r["ret_eq"]
    if r["state_cmp"] != "-":
        assert r["state_cmp"] == r["state_eq"] and r["changed_cmp"] == r["changed_eq"]


@needs_vm
def test_second_arm_hook(vm):
    """The S4.2 link: a second executor called where diff_hook.js called the reimplementation. The original run through the
    hook matches arm 1 on every vector; a body off by one in its return reads RED."""
    key = "Channel_setFormat_oom"
    spec = E.registry()[0][key]

    def orig(run, args):
        return run.m.native_call(run.img.base + spec["addr"], spec["abi"], args, spec["ret"])

    def wrong(run, args):
        r = orig(run, args)
        r["eax"] = (r["eax"] + 1) & 0xFFFFFFFF
        return r
    ok = E.KeyRun(vm, key, "A", second_arm=orig).run()
    assert ok.rows and all(r.match for r in ok.rows)
    bad = E.KeyRun(vm, key, "A", second_arm=wrong).run()
    assert not any(r.match for r in bad.rows)


@needs_vm
def test_controls_read_red(vm):
    import controls as C
    for kind, key, param in C.CONTROLS:
        r = C.run_control(vm, kind, key, param)
        assert r["verdict"] == "RED", (kind, key, r)
