"""S7.9 mirror: port/build/S7_9_head = `git archive 78ffb06d` + the S0 junctions (+ shim/build, re/replay, extracted, as I7) + S7's
files as S7.8 left them (port/build/S7_8_head: the live core, port/app/**, the app tests) + S7's shared edits in one set:
7_tick.patch, 8_tick.patch, 7_capi.patch, SG_MODULE(live), and the CMake edit I7 measured (add_subdirectory(app) before S5's
FreeType block, which gains a copy of freetype.dll beside simgolf_port). This step's own edits (test_live.py: the inflated test,
the Win32 equivalence runner) are made in the mirror afterwards and listed in 9_files.txt; create never runs twice.

    ~/bin/py -3.12 log/roadmap/S7/9_mirror.py                       # create (once)
    ~/bin/py -3.12 log/roadmap/S7/9_mirror.py --build [--w32] [--ft] # Release builds in the mirror's port/build/x64, Win32
    ~/bin/py -3.12 log/roadmap/S7/9_mirror.py --patches             # 9_tick.patch, 9_capi.patch, 9_cmake.patch (vs HEAD)
"""
from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
S7DIR = ROOT / "log" / "roadmap" / "S7"
S78 = ROOT / "port" / "build" / "S7_8_head"
MIRROR = ROOT / "port" / "build" / "S7_9_head"
HEAD = "78ffb06d"
JUNCTIONS = ["original", "log", "port/corpus", "tests/golden", "tests/fixtures", "re/versions", "tools/vc6", "shim/build",
             "re/replay", "extracted"]
LIVE = ["port/include/portsim/sim/live.h", "port/src/sim/live.cpp", "port/test/live.py", "port/test/test_live.py"]
TESTS = ["port/test/test_app_input.py", "port/test/test_app_shell.py", "port/test/test_app_live.py", "port/test/app_replay.py",
         "port/test/test_app_replay.py", "port/test/test_app_ingame.py", "port/test/test_app_present.py"]
PATCHES = ["7_tick.patch", "8_tick.patch", "7_capi.patch"]   # the create step; S7.9 then took 8_tick's hunks out again (9_notes.md section 3)
TICK_FILES = ["port/src/sim/tick.cpp", "port/src/sim/tick_hook.h", "port/include/portsim/sim/tick.h"]
CAPI_FILES = ["port/include/portsim_c.h", "port/src/portsim_c.cpp", "port/test/runner32/runner32.cpp"]
CMAKE_FILES = ["port/CMakeLists.txt", "port/src/sim/modules.inc"]
GFX_FILES = ["port/src/sim/gfx.cpp", "port/include/portsim/sim/gfx.h"]
CMAKE = (r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake"
         r"\CMake\bin\cmake.exe")
FT_ROOT = r"C:\Users\maria\vcpkg\installed"

S7_CMAKE = ("# Roadmap S7: the Windows host (input translation library and its trace checker; S7.3 adds the app executable).\n"
            "# It comes before S5's FreeType block, which copies freetype.dll beside simgolf_port too (roadmap S7.9, I7).\n"
            "add_subdirectory(app)\n\n")
FT_HEAD = "# Roadmap S5.1 (decision D4): FreeType, the system library"
FT_LOOP_END = ('                                     "$<TARGET_FILE_DIR:${_sg_ft_tgt}>")\n'
               '        endif()\n'
               '      endforeach()\n')
FT_APP_COPY = ('      if(TARGET simgolf_port)   # roadmap S7: the app links portsim; add_custom_command(TARGET) works only in its own directory\n'
               '        add_custom_target(simgolf_port_freetype ALL\n'
               '                          COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:freetype>" "$<TARGET_FILE_DIR:simgolf_port>")\n'
               '        add_dependencies(simgolf_port_freetype simgolf_port)\n'
               '      endif()\n')


def run(cmd, **kw):
    print("+", " ".join(str(c) for c in cmd), flush=True)
    return subprocess.run(cmd, check=True, **kw)


def edit(rel: str, old: str, new: str, what: str) -> None:
    p = MIRROR / rel
    t = p.read_bytes().decode("utf-8")
    if "\r\n" in t:
        old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
    if t.count(old) != 1:
        raise SystemExit(f"{what}: anchor found {t.count(old)} times")
    p.write_bytes(t.replace(old, new).encode("utf-8"))
    print("edit", what)


def apply_patches() -> None:
    # The mirror lies inside the repo (port/build is ignored): a plain `git apply` there finds the repo and SILENTLY skips
    # every path outside the current directory. GIT_CEILING_DIRECTORIES stops the discovery, so git applies as a plain patch.
    env = dict(os.environ, GIT_CEILING_DIRECTORIES=str(MIRROR.parent))
    for p in PATCHES:
        run(["git", "apply", "--verbose", "--whitespace=nowarn", str(S7DIR / p)], cwd=MIRROR, env=env)


def create() -> None:
    if (MIRROR / "port" / "CMakeLists.txt").exists():
        raise SystemExit(f"{MIRROR} exists")
    MIRROR.mkdir(parents=True, exist_ok=True)
    tar = subprocess.run(["git", "archive", HEAD], cwd=ROOT, capture_output=True, check=True).stdout
    run(["tar", "-xf", "-"], cwd=MIRROR, input=tar)
    full = subprocess.run(["git", "rev-parse", HEAD], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    (MIRROR / "HEAD_COMMIT").write_text(full + "\n")
    for j in JUNCTIONS:
        link = MIRROR / j
        if link.exists() and not os.path.isjunction(link):
            assert MIRROR in link.resolve().parents, link
            shutil.rmtree(link)
        if not link.exists() and (ROOT / j).exists():
            link.parent.mkdir(parents=True, exist_ok=True)
            run(["cmd", "/c", "mklink", "/J", str(link), str(ROOT / j)], capture_output=True)
    # S7's step-owned files as S7.8 accepted them
    for src in (S78 / "port" / "app").rglob("*"):
        if src.is_file() and "__pycache__" not in src.parts:
            d = MIRROR / src.relative_to(S78)
            d.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, d)
    for rel in LIVE + TESTS:
        (MIRROR / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(S78 / rel, MIRROR / rel)
    # S7's shared edits, one set
    apply_patches()
    mi = MIRROR / "port" / "src" / "sim" / "modules.inc"
    t = mi.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in t else "\n"
    assert "SG_MODULE(live)" not in t
    mi.write_bytes((t.rstrip("\r\n") + nl + "SG_MODULE(live)" + nl).encode("utf-8"))
    edit("port/CMakeLists.txt", FT_HEAD, S7_CMAKE + FT_HEAD, "CMake add_subdirectory(app) before S5's block")
    edit("port/CMakeLists.txt", FT_LOOP_END, FT_LOOP_END + FT_APP_COPY, "CMake freetype.dll beside simgolf_port")
    (MIRROR / "port" / "src" / "registry.cpp").touch()
    print("created", MIRROR)


def build(arch: str, ft: bool) -> int:
    """the mirror's own port/build/<arch> (what lib.py / lib32.py use and rebuild when stale), every target, through
    port/test/build.py (claude2 runs cmake only from Python)"""
    env = dict(os.environ)
    if ft:
        env["SG_FREETYPE_ROOT"] = FT_ROOT
    else:
        env.pop("SG_FREETYPE_ROOT", None)
    import sys as _s
    return subprocess.run([_s.executable, "port/test/build.py", "--arch", arch], cwd=MIRROR, env=env).returncode


def patches() -> None:
    """git-style patches of the mirror's shared files against HEAD: a/<rel> = HEAD's blob, b/<rel> = the mirror's file"""
    scratch = MIRROR.parent / "S7_9_patchwork"
    if scratch.exists():
        shutil.rmtree(scratch)
    for name, files in (("9_tick.patch", TICK_FILES), ("9_capi.patch", CAPI_FILES), ("9_cmake.patch", CMAKE_FILES),
                        ("9_gfx.patch", GFX_FILES)):
        out = []
        for rel in files:
            a, b = scratch / "a" / rel, scratch / "b" / rel
            a.parent.mkdir(parents=True, exist_ok=True)
            b.parent.mkdir(parents=True, exist_ok=True)
            a.write_bytes(subprocess.run(["git", "show", f"{HEAD}:{rel}"], cwd=ROOT, capture_output=True, check=True).stdout)
            b.write_bytes((MIRROR / rel).read_bytes())
            r = subprocess.run(["git", "diff", "--no-index", "--no-color", "--src-prefix=", "--dst-prefix=", f"a/{rel}", f"b/{rel}"],
                               cwd=scratch, capture_output=True)
            if r.returncode not in (0, 1):
                raise SystemExit(r.stderr.decode())
            out.append(r.stdout.decode("utf-8"))
        (S7DIR / name).write_bytes("".join(out).encode("utf-8"))
        print("wrote", name, sum(len(x) for x in out), "bytes")
    shutil.rmtree(scratch)


if __name__ == "__main__":
    a = sys.argv[1:]
    if "--patches" in a:
        patches()
    elif "--build" in a:
        ft = "--ft" in a
        rc = build("x64", ft)
        if rc == 0 and "--w32" in a:
            rc = build("Win32", ft)
        sys.exit(rc)
    else:
        create()
