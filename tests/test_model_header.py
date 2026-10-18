"""The merged data-model header re/model/include/simgolf_model.h (phase 1, stream G1).

- it is current: regenerating it from the six section headers (re/tools/model_merge.py header) gives the same text;
- it holds no original address (decision 2);
- it compiles with MSVC (cl) as C and C++, x86 and x64, /W4 /WX, and every static_assert holds; a wrong size assert
  fails, so the asserts are evaluated (skipped with the reason when Visual Studio Build Tools are not installed);
- the clang 19 front end shipped with the Build Tools (clang-tidy) parses it as C99 / C11 / C++17 for i686 and x86_64
  Linux (System V layouts) with -pedantic -Werror (skipped when clang-tidy is not installed);
- the merged address map re/model/addresses.tsv has no overlap, no duplicate symbol and no struct size disagreement.
"""
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
import model_merge as mm  # noqa: E402

pytestmark = pytest.mark.static

HEADER = ROOT / "re" / "model" / "include" / "simgolf_model.h"
VS = pathlib.Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools")
VCVARS = VS / "VC" / "Auxiliary" / "Build"
CLANG_TIDY = VS / "VC" / "Tools" / "Llvm" / "bin" / "clang-tidy.exe"

PROBE = '#include "{h}"\nint sg_model_header_probe(void) {{ return (int)sizeof(Golfer); }}\n'
BAD = '#include "{h}"\nSG_MODEL_STATIC_ASSERT(sizeof(Snd) == 0x6d, g1_negative_control);\n'


def test_header_is_current():
    assert HEADER.read_text(encoding="utf-8") == mm.build_header(), \
        "re/model/include/simgolf_model.h is stale: run `py -3.12 re/tools/model_merge.py header`"


def test_header_has_no_original_address():
    text = HEADER.read_text(encoding="utf-8")
    assert not mm.ADDR_RE.findall(text)


def test_address_map_has_no_overlap():
    rows, _ = mm.merged_rows()
    bad, _ = mm.overlaps(rows)
    assert not bad, [(a["symbol"], b["symbol"]) for a, b in bad]
    syms = [r["symbol"] for r in rows]
    assert len(syms) == len(set(syms))
    assert not mm.consistency(rows)


def _write(tmp, name, text):
    p = pathlib.Path(tmp) / name
    p.write_text(text.format(h=str(HEADER).replace("\\", "/")), encoding="ascii")
    return p


def _cl(arch, flags, src, tmp):
    bat = pathlib.Path(tmp) / "b.bat"
    bat.write_text(f'@echo off\nset CL=\nset _CL_=\ncall "{VCVARS / ("vcvars32.bat" if arch == "x86" else "vcvars64.bat")}"'
                   f' >nul 2>nul\ncd /d "{tmp}"\ncl /nologo {flags} /c "{src}" /Fo"{tmp}\\o.obj"\n', encoding="ascii")
    return subprocess.run(["cmd", "/c", str(bat)], capture_output=True, text=True, errors="replace", timeout=300)


@pytest.mark.skipif(not (VCVARS / "vcvars32.bat").exists(), reason="Visual Studio 2022 Build Tools (cl) not installed")
@pytest.mark.parametrize("arch,flags", [
    ("x86", "/W4 /WX /TC"), ("x86", "/W4 /WX /TC /std:c11"), ("x86", "/W4 /WX /TC /Za"),
    ("x86", "/W4 /WX /TP /std:c++17 /permissive-"), ("x64", "/W4 /WX /TC /std:c11"),
    ("x64", "/W4 /WX /TP /std:c++17 /permissive-"),
])
def test_header_compiles_with_msvc(arch, flags):
    with tempfile.TemporaryDirectory() as tmp:
        r = _cl(arch, flags, _write(tmp, "t.c", PROBE), tmp)
        assert r.returncode == 0, r.stdout[-3000:] + r.stderr[-2000:]


@pytest.mark.skipif(not (VCVARS / "vcvars32.bat").exists(), reason="Visual Studio 2022 Build Tools (cl) not installed")
def test_wrong_size_assert_fails_with_msvc():
    with tempfile.TemporaryDirectory() as tmp:
        r = _cl("x86", "/W4 /TC /std:c11", _write(tmp, "bad.c", BAD), tmp)
        assert r.returncode != 0 and "g1_negative_control" in r.stdout


SYSINC = {
    "stdint.h": "#ifndef G1_STDINT_H\n#define G1_STDINT_H\ntypedef __INT8_TYPE__ int8_t; typedef __UINT8_TYPE__ uint8_t;\n"
                "typedef __INT16_TYPE__ int16_t; typedef __UINT16_TYPE__ uint16_t;\ntypedef __INT32_TYPE__ int32_t; "
                "typedef __UINT32_TYPE__ uint32_t;\ntypedef __INT64_TYPE__ int64_t; typedef __UINT64_TYPE__ uint64_t;\n"
                "#endif\n",
    "stddef.h": "#ifndef G1_STDDEF_H\n#define G1_STDDEF_H\ntypedef __SIZE_TYPE__ size_t;\ntypedef __PTRDIFF_TYPE__ ptrdiff_t;\n"
                "#define NULL ((void *)0)\n#define offsetof(t, m) __builtin_offsetof(t, m)\n#endif\n",
}


def _clang(src, lang, std, target, tmp):
    inc = pathlib.Path(tmp) / "sysinc"
    inc.mkdir(exist_ok=True)
    for k, v in SYSINC.items():
        (inc / k).write_text(v, encoding="ascii")
    cmd = [str(CLANG_TIDY), "-checks=-*,bugprone-assert-side-effect", "-warnings-as-errors=*", "--header-filter=.*",
           "--quiet", str(src), "--", "-x", lang, f"-std={std}", "-pedantic", "-Wall", "-Wextra", "-Werror",
           "-ffreestanding", "-nostdinc", "-isystem", str(inc), f"--target={target}"]
    return subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=300)


@pytest.mark.skipif(not CLANG_TIDY.exists(), reason="clang-tidy (clang 19 front end) not installed with the Build Tools")
@pytest.mark.parametrize("lang,std,target", [
    ("c", "c99", "i686-unknown-linux-gnu"), ("c", "c11", "x86_64-unknown-linux-gnu"),
    ("c++", "c++17", "i686-unknown-linux-gnu"),
])
def test_header_parses_with_clang(lang, std, target):
    with tempfile.TemporaryDirectory() as tmp:
        r = _clang(_write(tmp, "t.c", PROBE), lang, std, target, tmp)
        assert r.returncode == 0, r.stdout[-3000:] + r.stderr[-2000:]


@pytest.mark.skipif(not CLANG_TIDY.exists(), reason="clang-tidy (clang 19 front end) not installed with the Build Tools")
def test_wrong_size_assert_fails_with_clang():
    with tempfile.TemporaryDirectory() as tmp:
        r = _clang(_write(tmp, "bad.c", BAD), "c", "c11", "i686-unknown-linux-gnu", tmp)
        assert r.returncode != 0 and "g1_negative_control" in (r.stdout + r.stderr)
