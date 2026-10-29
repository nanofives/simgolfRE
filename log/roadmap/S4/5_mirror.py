"""S4.5 private build (copy of 6_mirror.py: its own tree port/build/S4_5_mirror, S4.2 and S4.6 files as they are on disk plus audio_seq)
S4.2 private build (log/p3/p37b/mirror.py pattern): mirror port/ into port/build/S4_mirror/root/port with
  * every tracked port/ file a sibling has modified built at its committed version (HEAD),
  * this step's own files (audio_core.cpp, audio*.h, soundstate.h) as they are on disk,
  * modules.inc = HEAD + SG_MODULE(audio_core), registry.cpp touched when it changes,
  * portsim_c.h / portsim_c.cpp = HEAD + the C ABI addition of log/roadmap/S4/2_cabi.{h,cpp}.txt (the merge request),
then build x64 (port/build/S4_mirror/x64) and, with --w32, Win32 with the runner (port/build/S4_mirror/w32).

    ~/bin/py -3.12 log/roadmap/S4/2_mirror.py [--w32] [--no-build]
Prints the DLL paths: SG_PORTSIM_DLL=<x64 dll>; the Win32 runner + DLL for port/test/sound_replay.py --w32.
"""
import argparse
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "port"
MIR = ROOT / "port" / "build" / "S4_5_mirror"
DST = MIR / "root" / "port"
EXTRA = []      # (--extra) test-only module files added to src/sim with their SG_MODULE line
BASE_ONLY = False
NO_CHAN = False      # --no-chan: build S4.2's core without audio_chan (a reference build)
MODEL = ROOT / "re" / "model" / "include"
HERE = pathlib.Path(__file__).resolve().parent
OWN = {"src/sim/audio_core.cpp", "src/sim/audio_chan.cpp", "src/sim/audio_seq.cpp", "include/portsim/sim/audio_chan.h", "include/portsim/sim/audio_seq.h", "include/portsim/sim/audio.h", "include/portsim/sim/audio_sink.h",
       "include/portsim/sim/soundstate.h"}
sys.path.insert(0, str(ROOT))
from port.test import build as B  # noqa: E402


def git(*a, binary=False):
    r = subprocess.run(["git", *a], cwd=ROOT, capture_output=True, check=True, text=not binary)
    return r.stdout


def head(rel):
    return git("show", f"HEAD:port/{rel}", binary=True)


def patched_c_h() -> bytes:
    t = head("include/portsim_c.h").decode()
    anchor = "SG_API const char *sg_build_info(void);"
    assert t.count(anchor) == 1
    add = (HERE / "2_cabi.h.txt").read_text()
    return t.replace(anchor, add + anchor).encode()


def patched_c_cpp() -> bytes:
    t = head("src/portsim_c.cpp").decode()
    t = t.replace('#include "portsim/sim/tick.h"\n', '#include "portsim/sim/tick.h"\n#include "portsim/sim/audio.h"\n', 1)
    t = t.replace("#include <cstring>\n", "#include <cstddef>\n#include <cstring>\n", 1)
    old = "void sg_state_free(sg_ctx *ctx) { delete ctx; }"
    assert t.count(old) == 1
    t = t.replace(old, "void sg_state_free(sg_ctx *ctx) {\n  if (ctx) portsim::audio_release(ctx->ctx);  // S4.2: the audio "
                       "core's per-context state\n  delete ctx;\n}")
    end = '}  // extern "C"'
    i = t.rindex(end)
    t = t[:i] + (HERE / "2_cabi.cpp.txt").read_text() + "\n" + t[i:]
    return t.encode()


def mirror():
    modified = set(git("diff", "--name-only", "HEAD", "--", "port/").split())
    untracked = set(git("ls-files", "--others", "--exclude-standard", "--", "port/").split())
    seen = set()
    for p in SRC.rglob("*"):
        rel = p.relative_to(SRC)
        if p.is_dir() or rel.parts[0] in ("build", "corpus") or "__pycache__" in rel.parts:
            continue
        r = rel.as_posix()
        full = "port/" + r
        if full in untracked and (r not in OWN or BASE_ONLY):
            continue                              # a sibling's new file: not part of this build
        if BASE_ONLY:
            data = head(r) if (full in modified or r in ("src/sim/modules.inc", "include/portsim_c.h",
                                                         "src/portsim_c.cpp")) else p.read_bytes()
        elif r == "src/sim/modules.inc":
            data = (head(r).decode().rstrip("\n") + "\nSG_MODULE(audio_core)\n"
                    + ("SG_MODULE(audio_chan)\n" if (SRC / "src/sim/audio_chan.cpp").exists() and not NO_CHAN else "")
                    + ("SG_MODULE(audio_seq)\n" if (SRC / "src/sim/audio_seq.cpp").exists() else "")
                    +"".join(f"SG_MODULE({e.stem})\n" for e in EXTRA)).encode()
        elif r == "include/portsim_c.h":
            data = patched_c_h()
        elif r == "src/portsim_c.cpp":
            data = patched_c_cpp()
        elif full in modified and r not in OWN:
            data = head(r)
        else:
            data = p.read_bytes()
        d = DST / rel
        seen.add(d)
        d.parent.mkdir(parents=True, exist_ok=True)
        if not d.exists() or d.read_bytes() != data:
            d.write_bytes(data)
            if r == "src/sim/modules.inc":
                (DST / "src" / "registry.cpp").touch()
    for e in EXTRA:
        d = DST / "src" / "sim" / e.name
        seen.add(d)
        if not d.exists() or d.read_bytes() != e.read_bytes():
            d.write_bytes(e.read_bytes())
    for p in MODEL.rglob("*"):
        if p.is_file():
            d = DST.parent / "re" / "model" / "include" / p.relative_to(MODEL)
            d.parent.mkdir(parents=True, exist_ok=True)
            if not d.exists() or d.read_bytes() != p.read_bytes():
                shutil.copy2(p, d)
            seen.add(d)
    for d in list(DST.rglob("*")):
        if d.is_file() and d not in seen and "re" not in d.relative_to(DST).parts[:1]:
            d.unlink()


def build(arch, out):
    cm = B.cmake_exe()
    subprocess.run([cm, "-S", str(DST), "-B", str(out), "-A", arch], check=True, stdout=subprocess.DEVNULL)
    targets = ["portsim_c"] + (["portsim_runner32"] if arch == "Win32" else [])
    for t in targets:
        r = subprocess.run([cm, "--build", str(out), "--config", "Release", "--target", t, "--", "/m", "/nologo",
                            "/v:minimal"], capture_output=True, text=True)
        if r.returncode:
            print(r.stdout[-6000:], r.stderr[-2000:])
            raise SystemExit(f"build of {t} ({arch}) failed")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--only-w32", action="store_true", help="skip the x64 build (its DLL may be loaded by a running oracle)")
    ap.add_argument("--extra", action="append", default=[], help="a test-only module file (own tree root_<stem>, build x64_<stem>)")
    ap.add_argument("--base", action="store_true", help="HEAD only (no audio_core, no C ABI patch): the reference build")
    a = ap.parse_args()
    global DST, BASE_ONLY
    if a.base:
        BASE_ONLY = True
        DST = MIR / "root_base" / "port"
        mirror()
        build("x64", MIR / "x64_base")
        print("SG_PORTSIM_DLL=" + str(MIR / "x64_base" / "Release" / "portsim_c.dll"))
        return
    if a.extra:
        EXTRA.extend(pathlib.Path(x).resolve() for x in a.extra)
        tag = "_".join(e.stem for e in EXTRA)
        DST = MIR / f"root_{tag}" / "port"
        mirror()
        build("x64", MIR / f"x64_{tag}")
        print("SG_PORTSIM_DLL=" + str(MIR / f"x64_{tag}" / "Release" / "portsim_c.dll"))
        return
    mirror()
    if a.no_build:
        return
    if not a.only_w32:
        build("x64", MIR / "x64")
        print("SG_PORTSIM_DLL=" + str(MIR / "x64" / "Release" / "portsim_c.dll"))
    if a.w32 or a.only_w32:
        build("Win32", MIR / "w32")
        print("runner32=" + str(MIR / "w32" / "Release" / "portsim_runner32.exe"))
        print("dll32=" + str(MIR / "w32" / "Release" / "portsim_c.dll"))


if __name__ == "__main__":
    main()
