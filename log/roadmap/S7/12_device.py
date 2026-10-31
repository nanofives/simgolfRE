"""S7.12: audio_stream's device run at sound.golfSfxAlias 1 and 0, and at 1 over a scratch install whose course-sfx directory
carries the exe's name (S7.11's 11_sfx_install.py layout: junctions and hard links into original\\, nothing written there).
Prints steps / lines / sha / mmioOpen counts and whether the switch-0 lines, alias notes removed, equal the renamed-install lines.

    cd port/build/S7_12_root && SG_PORTSIM_DLL=$PWD/port/build/x64/Release/portsim_c.dll \\
      ~/bin/py -3.12 ../../../log/roadmap/S7/12_device.py <scratch dir> [--backend win32]
"""
import importlib.util
import json
import os
import pathlib
import re
import sys

sys.path.insert(0, os.getcwd())
from port.test import audio_stream as AS  # noqa: E402
from port.test import segments as S  # noqa: E402

HERE = pathlib.Path(__file__).resolve().parent
ALIAS = re.compile(r" \(alias [^)]*\)")


def stats(out, lines):
    return {"steps": out, "lines": len(lines), "sha": AS.device_sha(lines), "mmioOpen": sum("mmioOpen" in x for x in lines),
            "aliased": sum(" (alias " in x for x in lines), "absent": sum("(absent)" in x for x in lines)}


def main():
    scratch = pathlib.Path(sys.argv[1])
    if "--backend" in sys.argv:
        os.environ["SG_BACKEND"] = sys.argv[sys.argv.index("--backend") + 1]
    from port.test import lib as L
    lib = L.load(os.environ.get("SG_PORTSIM_DLL") if L.backend() != "win32" else None)
    res = {"backend": L.backend()}
    out1, _, l1 = AS.device_run(lib)
    res["switch1"] = stats(out1, l1)
    lib.defect("sound.golfSfxAlias", False)
    try:
        out0, _, l0 = AS.device_run(lib)
    finally:
        lib.defect("sound.golfSfxAlias", True)
    res["switch0"] = stats(out0, l0)
    spec = importlib.util.spec_from_file_location("sfx", HERE / "11_sfx_install.py")
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    m.build(scratch / "inst")
    old = S.INSTALL
    S.INSTALL = scratch / "inst"
    try:
        outr, _, lr = AS.device_run(lib)
    finally:
        S.INSTALL = old
    res["renamed_install"] = stats(outr, lr)
    stripped = [ALIAS.sub("", x) for x in l0]
    res["switch0_equals_renamed_install_without_alias_notes"] = stripped == lr
    if stripped != lr:
        k = next((i for i, (a, b) in enumerate(zip(stripped, lr)) if a != b), min(len(stripped), len(lr)))
        res["first_difference"] = {"line": k, "switch0": stripped[k:k + 1], "renamed": lr[k:k + 1]}
    (scratch / f"switch0_{L.backend()}.txt").write_text("\n".join(l0) + "\n")
    print(json.dumps(res, indent=1))


if __name__ == "__main__":
    main()
