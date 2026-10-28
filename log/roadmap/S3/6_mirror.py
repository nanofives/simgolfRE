"""S3.6 private build: S2's latest accepted mirror (port/build/S2_4_head = HEAD c6d7d78b + S2.1 + S2.3 + S2.4) plus S3 wave 1
(S3.1 + S3.1b files and their portsim_c.cpp / .h edit, S3.3's 3_tick.patch and test_gfx_eoi.py) plus this step's own files
and its shared-file edits (log/roadmap/S3/6_merge.md, applied here with exact-match anchors).

    ~/bin/py -3.12 log/roadmap/S3/6_mirror.py [--w32] [--no-build] [--no-hud]

Source root: port/build/S3_6_src (port/ is a copy; every other top-level entry of S2_4_head is a junction to it; port/corpus
is a junction to the shared corpus). Builds: port/build/S3_head/port/build/S3_6 (x64) and S3_6_w32 (Win32). --no-hud builds
the same tree without this step's edits (the baseline the gates compare with) into S3_6_base.

Tests run from the source root:
    cd port/build/S3_6_src && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m pytest port/test/test_gfx_hud.py
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
S2 = ROOT / "port" / "build" / "S2_4_head"
S3 = ROOT / "port" / "build" / "S3_head"
S31B = ROOT / "port" / "build" / "S3_1b_src"
SRC = ROOT / "port" / "build" / "S3_6_src"
BUILD = {"x64": S3 / "port" / "build" / "S3_6", "Win32": S3 / "port" / "build" / "S3_6_w32",
         "base": S3 / "port" / "build" / "S3_6_base"}
CMAKE = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

# S3 wave 1 (log/roadmap/S3/MERGE.md, files.txt): port files as the S3 mirror holds them
WAVE1_FROM_S3HEAD = ["src/sim/terrain_render.cpp", "include/portsim/sim/terrain_render.h", "test/test_terrain_render.py",
                     "test/terraincorpus.py", "test/glcorpus.py", "src/sim/gl_raster.cpp", "include/portsim/sim/gl_raster.h",
                     "test/test_gl_raster.py", "test/glraster.py", "test/glprobe.py", "test/test_gfx_eoi.py"]
WAVE1_FROM_S31B = ["src/portsim_c.cpp", "include/portsim_c.h"]   # S3.1's C ABI routing edit (MERGE.md 1a / 1b) applied
OWN = ["src/sim/gfx_hud.cpp", "include/portsim/sim/gfx_hud.h", "test/test_gfx_hud.py", "src/sim/gfx_pixel.cpp",
       "include/portsim/sim/gfx_pixel.h", "test/test_gfx_pixel.py"]

TICK_OLD_1 = """        FrameExit ex;
        try {
          ex = tk::tick_frame_io(ctx, in, io);
        } catch (...) {
          after_frame(f, k, io);
          throw;
        }
        after_frame(f, k, io);
        if (detail)
          *detail = ex == FrameExit::Quit ? "the last frame ran to the end of the trace: mainLoop returned (the title menu's quit)"
                                          : "the last frame ran to the end of the trace";"""
TICK_NEW_1 = """        FrameExit ex = FrameExit::Wait;
        bool out_of_input = false;
        try {
          ex = tk::tick_frame_io(ctx, in, io);
        } catch (const NeedInput &) {
          // the last frame's Blocking pump finds no recorded input left: the end of the recording (roadmap S3.3, U-1120).
          // Every snapshot not yet taken is past the last item the run could consume
          out_of_input = true;
          gfx_end_of_input(ctx);
        } catch (...) {
          after_frame(f, k, io);
          throw;
        }
        after_frame(f, k, io);
        if (detail)
          *detail = out_of_input ? "the last frame ran to the end of the trace: a Blocking pump with no input left"
                    : ex == FrameExit::Quit ? "the last frame ran to the end of the trace: mainLoop returned (the title menu's quit)"
                                            : "the last frame ran to the end of the trace";"""
TICK_OLD_2 = """      stop = std::string("a Blocking pump with no input left (") + (e.what ? e.what : "?") + ")";
    }"""
TICK_NEW_2 = """      stop = std::string("a Blocking pump with no input left (") + (e.what ? e.what : "?") + ")";
      gfx_end_of_input(ctx);  // snapshots still pending lie past the last recorded item (S3.3)
    }"""

# this step's shared-file edits (6_merge.md section 1)
FAM_OLD = "SG_GFX_FAMILY(routing)\n"
FAM_NEW = "SG_GFX_FAMILY(routing)\nSG_GFX_FAMILY(hud)\nSG_GFX_FAMILY(pixel)\n"
EDITS_PATH = ROOT / "log" / "roadmap" / "S3" / "6_edits.py"   # OLD / NEW pairs for gfx.cpp (exec'd: EDITS = [(file, old, new)])


def patch(text: str, old: str, new: str, what: str) -> str:
    crlf = "\r\n" in text
    if crlf:
        old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
    if text.count(old) != 1:
        raise SystemExit(f"{what}: anchor found {text.count(old)} times")
    return text.replace(old, new)


def patch_file(p: pathlib.Path, old: str, new: str, what: str):
    t = p.read_bytes().decode()
    p.write_bytes(patch(t, old, new, what).encode())


def junction(link: pathlib.Path, target: pathlib.Path):
    if link.exists():
        return
    subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)], check=True, capture_output=True)


def copy_tree(hud: bool):
    SRC.mkdir(parents=True, exist_ok=True)
    for e in S2.iterdir():                       # every top-level entry but port/: a junction (dirs) or a copy (files)
        if e.name == "port":
            continue
        if e.is_dir():
            junction(SRC / e.name, e)
        else:
            shutil.copyfile(e, SRC / e.name)
    dst = SRC / "port"
    dst.mkdir(exist_ok=True)
    for e in dst.iterdir():                      # empty port/ in place; the corpus junction is unlinked, never entered
        if e.is_junction() or e.is_symlink():
            os.rmdir(e)
        elif e.is_dir():
            shutil.rmtree(e)
        else:
            e.unlink()
    shutil.copytree(S2 / "port", dst, ignore=shutil.ignore_patterns("build", "corpus", "__pycache__"), dirs_exist_ok=True)
    junction(dst / "corpus", ROOT / "port" / "corpus")
    for rel in WAVE1_FROM_S3HEAD:
        shutil.copyfile(S3 / "port" / rel, dst / rel)
    for rel in WAVE1_FROM_S31B:
        shutil.copyfile(S31B / "port" / rel, dst / rel)
    t = dst / "src" / "sim" / "tick.cpp"
    patch_file(t, TICK_OLD_1, TICK_NEW_1, "tick.cpp hunk 1 (S3.3)")
    patch_file(t, TICK_OLD_2, TICK_NEW_2, "tick.cpp hunk 2 (S3.3)")
    if not hud:
        return
    for rel in OWN:
        src = ROOT / "port" / rel
        if src.exists():
            shutil.copyfile(src, dst / rel)
    patch_file(dst / "src" / "sim" / "gfx_families.inc", FAM_OLD, FAM_NEW, "gfx_families.inc")
    if EDITS_PATH.exists():
        env = {}
        exec(EDITS_PATH.read_text(encoding="utf-8"), env)
        for rel, old, new in env["EDITS"]:
            patch_file(dst / rel, old, new, rel)
    (dst / "src" / "registry.cpp").touch()


def build(arch: str, key: str):
    b = BUILD[key]
    subprocess.run([CMAKE, "-S", str(SRC / "port"), "-B", str(b), "-A", arch], check=True)
    subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_c", "--", "/m"], check=True)
    if arch == "Win32":
        subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_runner32", "--", "/m"], check=True)
    return b / "Release" / "portsim_c.dll"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--no-hud", action="store_true")
    a = ap.parse_args()
    copy_tree(not a.no_hud)
    if a.no_build:
        return 0
    if a.no_hud:
        print("SG_PORTSIM_DLL=" + str(build("x64", "base")))
        return 0
    print("SG_PORTSIM_DLL=" + str(build("x64", "x64")))
    if a.w32:
        print("Win32:", build("Win32", "Win32"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
