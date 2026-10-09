"""re/tools/pointer_callers.py against installers checked by hand in the image."""
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
import pointer_callers as pc  # noqa: E402

pytestmark = pytest.mark.static


def installers(module, addr):
    m = pc.mod(module)
    return {i for *_, inst in pc.references(m, pc.norm(m, addr)) for i in inst}


@pytest.mark.parametrize("module, addr, installer", [
    # STORE in code: `mov [edi], 0x4b5810` at 0x004b51fc in jinit_c_master_control (round 16)
    ("golf_clean.exe", "0x4b5810", 0x004b51e0),
    # STORE in a function created from undiscovered code: start_pass_coef installs compress_first_pass (2026-10-09)
    ("golf_clean.exe", "0x4b0b20", 0x004b07b0),
    # TABLE slot 2 of the vtable 0x4bb014 installed by NodeListC::ctor (closeHookC, round 19)
    ("golf_clean.exe", "0x487bd0", 0x00487b40),
    # NULL-delimited CRT initializer table: slot 1 of 0x4c1000, whose address __cinit loads at 0x4a50fb
    ("golf_clean.exe", "0x403f10", 0x004a50db),
])
def test_installer(module, addr, installer):
    assert installer in installers(module, addr)


def test_unreferenced_global_has_no_installer():
    # TextView::create's only reference is the global 0x4e44ec, which no instruction addresses
    refs = pc.references(pc.mod("golf_clean.exe"), 0x00491310)
    assert refs and not any(inst for *_, inst in refs)
