"""Every A/B registry entry's calling convention and argument count must agree with the original's `ret N`
(re/tools/check_ret_args.py). Round 28: panelSubSlot60 returned with `ret 4` while its detour and registry declared no
argument; its own A/B was GREEN and a C3 caller crashed with the hook on."""
import sys

import pytest

from conftest import ROOT

pytestmark = pytest.mark.static
sys.path.insert(0, str(ROOT / "re" / "tools"))
import check_ret_args  # noqa: E402


def test_registry_conventions_match_the_originals_ret():
    assert check_ret_args.main() == 0


def test_the_check_catches_the_round_28_mismatch():
    # panelSubSlot60 0x4a4870 as first declared: thiscall with `this` only expects ret 0; the original pops 4
    got = check_ret_args.rets("golf_clean.exe", 0x4a4870)
    assert check_ret_args.expected("thiscall", 1) not in got and 4 in got
