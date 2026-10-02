"""Record/replay (re/frida/replay.py): offline Unicorn replay of recorded real calls.
Static layer: uses the committed recording if present (re/frida/record.py makes it)."""
import pathlib
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "frida"))
import replay  # noqa: E402

pytestmark = pytest.mark.static
BUNDLE = ROOT / "re" / "replay" / "TerrainDll_tileAt"
needs_bundle = pytest.mark.skipif(not (BUNDLE / "meta.json").exists(), reason="run re/frida/record.py TerrainDll_tileAt")


@needs_bundle
def test_replay_green_on_real_calls(tmp_path):
    assert replay.replay("TerrainDll_tileAt", out_dir=tmp_path, limit=300)


@needs_bundle
def test_replay_negative_control_is_red(tmp_path):
    xor_eax_ret8 = bytes([0x31, 0xC0, 0xC2, 0x08, 0x00])  # always NULL, same stack contract
    assert not replay.replay("TerrainDll_tileAt", out_dir=tmp_path, wrong_stub=xor_eax_ret8, limit=300)


@needs_bundle
def test_replay_refuses_to_write_negative_control_into_evidence():
    with pytest.raises(ValueError):
        replay.replay("TerrainDll_tileAt", wrong_stub=b"\xc3")
