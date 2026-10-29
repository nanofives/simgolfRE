"""match.py's per-file toolchain selector (`// TOOLCHAIN sp5`, roadmap S10.5) and the exe literal rule (U-1886).
Static layer; skipped when a toolchain is not present (tools/vc6 and tools/vc6sp5 are local, not committed)."""
import pathlib
import subprocess
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402

HAVE_RTM = (ROOT / "tools" / "vc6" / "vc98" / "bin" / "cl.exe").exists()
HAVE_SP5 = (ROOT / "tools" / "vc6sp5" / "VC98" / "BIN" / "CL.EXE").exists()
pytestmark = [pytest.mark.static,
              pytest.mark.skipif(not (HAVE_RTM and HAVE_SP5), reason="VC6 RTM or SP5 toolchain not in tools/")]

DSB = ROOT / "re" / "match" / "smsg_sound_dsb.cpp"
EMU_WRITE = 0x10010920          # CDSB::get_emulated_write_info: SP5's C2 places one `jae` target differently


def comp_id(obj: pathlib.Path) -> tuple[int, int]:
    _, syms = match.coff(obj)
    v = next(s for s in syms if s["name"] == "@comp.id")["value"]
    return v >> 16, v & 0xFFFF


def test_toolchain_line_default_and_unknown():
    assert match.file_toolchain("// FLAGS sound.dll: /O2\n") == "8168"
    assert match.file_toolchain("// TOOLCHAIN sp5\n") == "sp5"
    with pytest.raises(SystemExit):
        match.file_toolchain("// TOOLCHAIN sp9\n")


@pytest.mark.parametrize("tc, banner, build", [("8168", "12.00.8168", 8168), ("sp5", "12.00.8804", 8966),
                                               ("sp5c2", "12.00.8168", 8966)])
def test_compiler_build_in_banner_and_obj(tmp_path, tc, banner, build):
    """@comp.id carries the back end's build: SP5's C2 12.00.8966 is sound.dll's Rich entry 11/8966 (x34)."""
    t = match.TOOLCHAINS[tc]
    env = {"PATH": ";".join(str(d) for d in [t["bin"], *t["path"]]), "SystemRoot": r"C:\Windows"}
    r = subprocess.run([str(t["bin"] / "cl.exe")], capture_output=True, text=True, env=env)
    assert f"Version {banner}" in r.stderr + r.stdout
    src = tmp_path / "t.cpp"
    src.write_text("int f(int a) { return a * 3 + 1; }\n")
    assert comp_id(match.compile_obj(src, "/O2", "c++", "", tc)) == (11, build)


@pytest.mark.skipif(not DSB.exists(), reason="smsg_sound_dsb.cpp not generated (re/tools/smsg_import.py)")
def test_sp5_only_match_and_its_8168_control():
    assert "// TOOLCHAIN sp5" in DSB.read_text()
    sp5 = match.compare(DSB, "", False, only=EMU_WRITE, quiet=True)
    assert [r[4] for r in sp5] == [1.0] and sp5[0][8] == "sp5"
    old = match.compare(DSB, "", False, only=EMU_WRITE, quiet=True, toolchain="8168")
    assert len(old) == 1 and 0.99 < old[0][4] < 1.0           # 157 / 158 instructions: the control reads RED
    back = match.compare(DSB, "", False, only=EMU_WRITE, quiet=True, toolchain="sp5c2")
    assert back[0][4] == 1.0                                  # the back end alone is enough


def _ycc_wrapper(tmp_path) -> pathlib.Path:
    """rgb_ycc_start from IJG libjpeg 6a's jccolor.c (vendored), /O2 C, like the golf_jpeg_*.cpp wrappers."""
    jc = (ROOT / "re" / "match" / "vendor" / "jpeg-6a" / "jccolor.c").as_posix()
    w = tmp_path / "ycc.cpp"
    w.write_text(f'// FLAGS golf_clean.exe: /O2\n// LANG c\n#include "{jc}"\n'
                 "// MATCH: golf_clean.exe 0x004b4ca0 _rgb_ycc_start\n")
    return w


def test_exe_literal_in_image_range_is_a_constant(tmp_path):
    """U-1886: `mov [esp+0x1c], 0x807fff` (CBCR_OFFSET + ONE_HALF - 1) is a literal in the obj and inside the exe's
    image range; it was masked on the original side only (56 / 57 instructions)."""
    res = match.compare(_ycc_wrapper(tmp_path), "", False, quiet=True)
    assert [r[4] for r in res] == [1.0]


def test_exe_literal_rule_control(tmp_path, monkeypatch):
    """Control: without the literal set (the old rule) the same function reads 56 / 57."""
    monkeypatch.setattr(match, "literal_values", lambda *_: set())
    res = match.compare(_ycc_wrapper(tmp_path), "", False, quiet=True)
    assert len(res) == 1 and round(100 * res[0][4], 1) == 98.2

