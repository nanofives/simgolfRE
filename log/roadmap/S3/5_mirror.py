"""S3.5 private build and test root.

    ~/bin/py -3.12 log/roadmap/S3/5_mirror.py [--w32] [--no-build] [--no-merge]

Source and test root port/build/S3_5_root, built from:
  - port/build/S3_2_root/port (S3.2's accepted root: the S3 mirror c6d7d78b + S2 wave 1 + S3 wave 1 + S3.2 with its merge edits),
    minus build / corpus;
  - S3 wave 2 (S3.4 + S3.4b, accepted): the port files of log/roadmap/S3/files_w2.txt from the working tree and the shared-file
    edits 1 (modules.inc SG_MODULE(flic)) and 2a..2c (menu.cpp) of log/roadmap/S3/MERGE_w2.md, applied here by exact match;
  - this step's own files (OWN) and, unless --no-merge, the shared-file edits of log/roadmap/S3/5_merge.md (5_merge_edits.py);
  - junctions: port/corpus, original, log -> the working tree; re, tests -> the S3 mirror's HEAD copies (as S3.2's root).
Builds: port/build/S3_head/port/build/S3_5 (x64) and S3_5_w32 (Win32, with portsim_runner32). Tests run from the root:
    cd port/build/S3_5_root && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m pytest port/test/test_gfx_scene.py
"""
import argparse
import importlib.util
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
MIRROR = ROOT / "port" / "build" / "S3_head"
BASE = ROOT / "port" / "build" / "S3_6_src"
# S3.2 (accepted): its own files and its shared-file edits (2_merge_edits.py), applied on top of the S3.6 base
S32_OWN = ["port/src/sim/terrain_live.cpp", "port/include/portsim/sim/terrain_live.h", "port/src/sim/terrain_state.cpp",
           "port/include/portsim/sim/terrain_state.h", "port/src/sim/terrain_dll.cpp", "port/src/sim/terrain_gl_setup.cpp",
           "port/include/portsim/sim/terrain_gl_setup.h", "port/src/sim/gfx_terrain.cpp", "port/test/terrainlive.py",
           "port/test/test_terrain_live.py", "port/test/test_terrain_gl_setup.py", "port/test/runner32/runner32.cpp", "port/test/lib32.py"]
DST = ROOT / "port" / "build" / "S3_5_root"
BUILD = {"x64": MIRROR / "port" / "build" / "S3_5", "Win32": MIRROR / "port" / "build" / "S3_5_w32"}
OWN = ["port/src/sim/gfx_scene.cpp", "port/include/portsim/sim/gfx_scene.h", "port/test/test_gfx_scene.py"]
JUNCTIONS = {"port/corpus": ROOT / "port" / "corpus", "original": ROOT / "original", "log": ROOT / "log",
             "tests": MIRROR / "tests", "re": MIRROR / "re"}
CMAKE = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

# MERGE_w2.md edits 2a..2c (menu.cpp)
W2_MENU = [
    ('#include "portsim/sim/gfx.h"\n', '#include "portsim/sim/gfx.h"\n#include "portsim/sim/flic.h"\n'),
    ("  const int32_t cols = flic_cols(ctx, read_str(ctx.mem, text));\n",
     "  const int32_t cols = flic_cols(ctx, flic::resolve_name(ctx.files(), read_str(ctx.mem, text)));\n"),
    ("    gs.g_sheetFirstCell[id] = base;\n"
     "    const int32_t cells = frames > 0 && cols > 0 ? (f.delay == -1 ? 1 : mul32(frames, cols)) : 0;\n"
     "    gs.g_cellCursor = add32(base, cells);\n"
     "  }\n",
     "    gs.g_sheetFirstCell[id] = base;\n"
     "    const int32_t cells = frames > 0 && cols > 0 ? (f.delay == -1 ? 1 : mul32(frames, cols)) : 0;\n"
     "    gs.g_cellCursor = add32(base, cells);\n"
     "    // the cells' pixels for an attached graphics backend (S3.4, portsim/sim/flic.h): loadFlic's frame-by-frame cut of the\n"
     "    // flic into the sheet's cells g_cellPool[base ..], judged against the original's sheet memory (port/test/test_flic.py)\n"
     "    if (g_gfx_attached.load(std::memory_order_relaxed) != 0)\n"
     "      flic::sheet_to_gfx(ctx, path, f.frames, f.delay, f.flag,\n"
     "                         state_va(ctx, &GameState::g_cellPool) + 0x2cu * static_cast<uint32_t>(base));\n"
     "  }\n"),
]


# S2 wave 5 (the orchestrator's decision: Surface_drawSprite has one body, S2's gfx_reports.cpp): S2's accepted files from the
# I6 integration tree and the shared-file lines they need (Sprite8's cut size in gfx_body.h, the family line)
I6 = ROOT / "port" / "build" / "I6"
S2W5_FILES = ["port/src/sim/gfx_reports.cpp", "port/src/sim/gfx_reports.h"]
S2W5_EDITS = [
    ("port/src/sim/gfx_body.h", "  int32_t w = 0, h = 0, left = 0, top = 0;\n",
     "  int32_t w = 0, h = 0, left = 0, top = 0;\n"
     "  int32_t cut_w = 0, cut_h = 0;   // the requested rectangle's size (the SpriteRef's +0x18 / +0x1c; roadmap S2.8)\n"),
    ("port/src/sim/gfx_families.inc", "SG_GFX_FAMILY(scene)\n", "SG_GFX_FAMILY(scene)\nSG_GFX_FAMILY(reports)\n"),
]


def apply_s2w5():
    for rel in S2W5_FILES:
        shutil.copyfile(I6 / rel, DST / rel)
    for rel, old, new in S2W5_EDITS:
        p = DST / rel
        b = p.read_bytes().decode()
        nn = new.replace(chr(10), chr(13) + chr(10)) if chr(13) + chr(10) in b else new
        if nn in b:
            continue
        p.write_bytes(patch(b, old, new, "S2 wave 5 " + rel).encode())
    me = _load("s35_merge", ROOT / "log/roadmap/S3/5_merge_edits.py")
    for rel, old, new in getattr(me, "EDITS_AFTER_S2W5", []):
        p = DST / rel
        b = p.read_bytes().decode()
        nn = new.replace(chr(10), chr(13) + chr(10)) if chr(13) + chr(10) in b else new
        if nn in b:
            continue
        p.write_bytes(patch(b, old, new, "S3.5 after S2 wave 5 " + rel).encode())


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def wave_files(listing: pathlib.Path) -> list[str]:
    return [ln.strip() for ln in listing.read_text().splitlines() if ln.strip().startswith("port/")]


def patch(text: str, old: str, new: str, what: str) -> str:
    if "\r\n" in text:
        old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
    if text.count(old) != 1:
        raise SystemExit(f"{what}: anchor found {text.count(old)} times")
    return text.replace(old, new)


def junction(link: pathlib.Path, target: pathlib.Path):
    if link.exists():
        return
    link.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)], check=True, capture_output=True)


def copy_tree(merge: bool = True):
    DST.mkdir(parents=True, exist_ok=True)
    port = DST / "port"
    if port.exists():
        cj = port / "corpus"
        if cj.exists():
            subprocess.run(["cmd", "/c", "rmdir", str(cj)], check=True, capture_output=True)
        shutil.rmtree(port)
    shutil.copytree(BASE / "port", port, ignore=shutil.ignore_patterns("build", "corpus", "__pycache__"))
    for name in ("pytest.ini", "CLAUDE.md"):
        shutil.copyfile(BASE / name, DST / name)
    for rel, tgt in JUNCTIONS.items():
        junction(DST / rel, tgt)
    for rel in S32_OWN + wave_files(ROOT / "log/roadmap/S3/files_w2.txt") + OWN:
        src = ROOT / rel
        if src.exists():
            (DST / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, DST / rel)
    s32 = _load("s32_merge", ROOT / "log/roadmap/S3/2_merge_edits.py")
    for rel, old, new in s32.EDITS:
        p = DST / rel
        p.write_bytes(patch(p.read_bytes().decode(), old, new, "S3.2 " + rel).encode())
    mods = port / "src" / "sim" / "modules.inc"
    t = mods.read_bytes().decode()
    nl = "\r\n" if "\r\n" in t else "\n"
    if "SG_MODULE(flic)" not in t:
        mods.write_bytes((t.rstrip("\r\n") + nl + "SG_MODULE(flic)" + nl).encode())
    menu = port / "src" / "sim" / "menu.cpp"
    m = menu.read_bytes().decode()
    for k, (old, new) in enumerate(W2_MENU):
        m = patch(m, old, new, f"menu.cpp MERGE_w2 2{'abc'[k]}")
    menu.write_bytes(m.encode())
    if merge:
        me = _load("s35_merge", ROOT / "log/roadmap/S3/5_merge_edits.py")
        for rel, old, new in me.EDITS + getattr(me, "EDITS_HUD", []):
            p = DST / rel
            b = p.read_bytes().decode()
            p.write_bytes(patch(b, old, new, rel).encode())
        apply_s2w5()
    (port / "src" / "registry.cpp").touch()


def update():
    """refresh the source copy in place (own files, edits not yet applied), without touching anything else: usable while a
    long run reads the root"""
    for rel in OWN:
        src = ROOT / rel
        if src.exists():
            (DST / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, DST / rel)
    me = _load("s35_merge", ROOT / "log/roadmap/S3/5_merge_edits.py")
    for rel, old, new in me.EDITS + getattr(me, "EDITS_HUD", []):
        p = DST / rel
        b = p.read_bytes().decode()
        nn = new.replace(chr(10), chr(13) + chr(10)) if chr(13) + chr(10) in b else new
        if nn in b:
            continue
        p.write_bytes(patch(b, old, new, rel).encode())
    apply_s2w5()
    (DST / "port" / "src" / "registry.cpp").touch()


def build(arch: str, suffix: str = ""):
    b = BUILD[arch]
    if suffix:
        b = b.parent / (b.name + suffix)
    subprocess.run([CMAKE, "-S", str(DST / "port"), "-B", str(b), "-A", arch], check=True)
    subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_c", "--", "/m"], check=True)
    if arch == "Win32":
        subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_runner32", "--", "/m"], check=True)
    return b / "Release" / "portsim_c.dll"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--no-merge", action="store_true", help="without this step's shared-file edits (the control build)")
    ap.add_argument("--update", action="store_true", help="refresh own files / new edits in place instead of a fresh copy")
    ap.add_argument("--suffix", default="", help="build directory suffix (a second build while the first is in use)")
    a = ap.parse_args()
    if a.update:
        update()
    else:
        copy_tree(merge=not a.no_merge)
    if a.no_build:
        return 0
    print("SG_PORTSIM_DLL=" + str(build("x64", a.suffix)))
    if a.w32:
        print("Win32:", build("Win32", a.suffix))
    return 0


if __name__ == "__main__":
    sys.exit(main())
