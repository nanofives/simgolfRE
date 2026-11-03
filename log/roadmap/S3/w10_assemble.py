"""S3 wave 10 stack: w10_base.py (= a2f1f3c4's port tree, I22 + I23) + the wave 10 layers in merge order, each through its own
assembler's --root <tree> --only-<k> form (every anchor exactly once, the assembler refuses otherwise):

    1. S3.31  31_assemble.py --only-31   (terrain_overlay.cpp / .h, test_terrain_overlay.py)
    2. S3.30  30_assemble.py --only-30   (trace_format.py, trace_rec.js, both trace schemas, tick.cpp TraceInput, live.cpp / .h,
                                          test_trace_call.py)
    3. S3.32  32_assemble.py --only-32   (when present: the boot's static vtable words)

    ~/bin/py -3.12 log/roadmap/S3/w10_assemble.py --dst S3_W10 [--skip 32]

The destination is rebuilt from the archive (w10_base.py); its port/build subdirectory (the builds) is kept."""
import argparse
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
LAYERS = ["31", "30", "32"]


def run(*args: str) -> None:
    print("+", " ".join(args), flush=True)
    subprocess.run([sys.executable, *args], cwd=ROOT, check=True)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_W10")
    ap.add_argument("--skip", nargs="*", default=[])
    a = ap.parse_args()
    assert a.dst not in ("I22", "x64"), a.dst
    m = ROOT / "port" / "build" / a.dst
    run(str(HERE / "w10_base.py"), "--dst", a.dst)
    for k in LAYERS:
        if k in a.skip:
            print("skip", k)
            continue
        asm = HERE / f"{k}_assemble.py"
        if not asm.exists():
            print("no", asm.name, "- layer", k, "not delivered")
            continue
        run(str(asm), "--root", str(m), f"--only-{k}")
    (m / "HEAD_COMMIT").write_text("a2f1f3c4 (port tree = w10_base) + S3 wave 10 layers " + ", ".join(
        k for k in LAYERS if k not in a.skip and (HERE / f"{k}_assemble.py").exists()) + "\n")


if __name__ == "__main__":
    main()
