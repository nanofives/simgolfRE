"""Tool fixes of the b3 cleanup batch (re/C4_PLAN.md item 4): asm2inline.py kept the operands of movsx and read every
range of a multi-range function; match.py's normalize() compares backward branches as signed offsets; c3_verify.py names
the reason when a row's hooks.csv `file` column points at a source without its SG_HOOK."""
import subprocess
import sys

import pytest

from conftest import ROOT

pytestmark = pytest.mark.static
sys.path.insert(0, str(ROOT / "re" / "tools"))
import asm2inline  # noqa: E402
import c3_verify  # noqa: E402
import match  # noqa: E402


def _asm2inline(*args):
    r = subprocess.run([sys.executable, str(ROOT / "re" / "tools" / "asm2inline.py"), *args],
                       capture_output=True, text=True, check=True, cwd=ROOT)
    return r.stdout


def test_string_instructions_lose_their_operands_but_movsx_keeps_them():
    assert asm2inline.is_string_op("rep movsd", "dword ptr es:[edi], dword ptr [esi]")
    assert asm2inline.is_string_op("repne scasb", "al, byte ptr es:[edi]")
    assert asm2inline.is_string_op("stosd", "dword ptr es:[edi], eax")
    assert not asm2inline.is_string_op("movsx", "eax, cl")
    assert not asm2inline.is_string_op("movsx", "eax, word ptr [ecx + 0x57956e]")
    assert not asm2inline.is_string_op("movsd", "xmm0, qword ptr [eax]")


def test_movsx_operands_survive_in_the_block():
    # pickGolferName 0x00467600: `movsx eax, cl` at 0x0046762f
    out = _asm2inline("golf_clean.exe", "0x00467600", "--from", "0x0046762f", "--to", "0x0046763c")
    assert "movsx eax, cl" in out
    assert "repne" not in out


def test_listing_of_a_multi_range_function_reaches_its_last_range():
    # loadInterfaceArt 0x00442180: four ranges, the last ends at the `ret` 0x004480f7; the listing used to stop at 0x00447f79
    out = _asm2inline("golf_clean.exe", "0x00442180", "--list").splitlines()
    assert out[-1].startswith("0x004480f7")
    assert any(l.startswith("0x00445aa0") for l in out)


def test_backward_branch_before_the_obj_base_is_a_signed_offset():
    # `je` back by 0x10 at obj base 0 (target below 0): capstone's 0xfffffff0 must read -0x10, like the original side
    code = bytes([0x74, 0xee])                                   # je -0x12 -> target 2 - 0x12 = -0x10
    obj = match.normalize(code, 0, 0, lambda *a: False, obj=True)
    orig = match.normalize(code, 0x401000, 0x401000, lambda *a: False, obj=False)
    assert obj[0][1] == orig[0][1] == "je +0x-10"


def test_forward_branch_keys_are_unchanged():
    code = bytes([0x74, 0x04])
    assert match.normalize(code, 0, 0, lambda *a: False, obj=True)[0][1] == "je +0x6"


def test_hook_diagnosis_names_a_row_file_without_the_hook(tmp_path):
    batch = tmp_path / "batch.cpp"
    batch.write_text('SG_HOOK("golf_clean.exe", 0x0049eef0, flagSet, A, B);\n')
    # flagSet 0x0049eef0: hooks.csv once named c3x.cpp, which has no SG_HOOK there
    row_file = "shim/src/re/c3x.cpp"
    why = c3_verify.hook_diagnosis(row_file, batch, "golf_clean.exe", 0x0049eef0)
    assert why and "c3x.cpp" in why and "has no SG_HOOK at 0x0049eef0" in why and "--file" in why
    assert c3_verify.hook_diagnosis("", batch, "golf_clean.exe", 0x0049eef0) is None


def test_hook_diagnosis_reports_a_batch_without_the_hook(tmp_path):
    batch = tmp_path / "empty.cpp"
    batch.write_text("// nothing\n")
    why = c3_verify.hook_diagnosis("", batch, "sound.dll", 0x28600)
    assert why and "empty.cpp" in why and "sound.dll+0x00028600" in why


def test_source_hooks_reads_module_and_address(tmp_path):
    f = tmp_path / "s.cpp"
    f.write_text('SG_HOOK("jgld.dll", 0x72b0, find, a, b);\nSG_HOOK( "golf_clean.exe" , 0x4a29a0, f, a, b);\n')
    assert c3_verify.source_hooks(f) == {("jgld.dll", 0x72b0), ("golf_clean.exe", 0x4a29a0)}
    assert c3_verify.source_hooks(tmp_path / "missing.cpp") == set()
