"""S4.11 private build (log/roadmap/S4/2b_mirror.py pattern): mirror port/ into port/build/S4_11_mirror/root/port with
  * every tracked port/ file a sibling has modified built at its committed version (HEAD),
  * every S4 file from the tree (S4.2b installed the consolidated core there: audio.h, audio_core.cpp, the ten modules, their
    headers, soundstate.h, audio_sink.h, audio_x80.inc) and this step's audio_crt.cpp,
  * modules.inc = HEAD + SG_MODULE of the eleven audio modules, registry.cpp touched,
  * portsim_c.h / portsim_c.cpp = HEAD + S4.2's C ABI addition (2_cabi.{h,cpp}.txt) + sg_state_free's audio_release,
then build x64 and, with --w32, Win32 with the runner.

    ~/bin/py -3.12 log/roadmap/S4/11_mirror.py [--w32] [--no-build] [--only-w32] [--no-crt]
`--no-crt` leaves audio_crt out (the state before this step: the baseline census).  --no-crt builds into root_nocrt / x64_nocrt.
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "port"
MIR = ROOT / "port" / "build" / "S4_11_mirror"
HERE = pathlib.Path(__file__).resolve().parent
MODEL = ROOT / "re" / "model" / "include"
MODULES = ("audio_core", "audio_mixer", "audio_chan", "audio_ds", "audio_dev", "audio_codec", "audio_seq", "audio_sound",
           "audio_wave", "audio_soundfile")
sys.path.insert(0, str(ROOT))
from port.test import build as B  # noqa: E402


def git(*a, binary=False):
    r = subprocess.run(["git", *a], cwd=ROOT, capture_output=True, check=True, text=not binary)
    return r.stdout


def head(rel):
    return git("show", f"HEAD:port/{rel}", binary=True)


def is_audio(rel: str) -> bool:
    n = pathlib.PurePosixPath(rel).name
    return n.startswith("audio_") or n in ("audio.h", "soundstate.h")


def patched_c_h() -> bytes:
    t = head("include/portsim_c.h").decode()
    anchor = "SG_API const char *sg_build_info(void);"
    assert t.count(anchor) == 1
    return t.replace(anchor, (HERE / "2_cabi.h.txt").read_text() + anchor).encode()


def patched_c_cpp() -> bytes:
    t = head("src/portsim_c.cpp").decode()
    t = t.replace('#include "portsim/sim/tick.h"\n', '#include "portsim/sim/tick.h"\n#include "portsim/sim/audio.h"\n', 1)
    t = t.replace("#include <cstring>\n", "#include <cstddef>\n#include <cstring>\n", 1)
    old = "void sg_state_free(sg_ctx *ctx) { delete ctx; }"
    assert t.count(old) == 1
    t = t.replace(old, "void sg_state_free(sg_ctx *ctx) {\n  if (ctx) portsim::audio_release(ctx->ctx);\n  delete ctx;\n}")
    end = '}  // extern "C"'
    i = t.rindex(end)
    return (t[:i] + (HERE / "2_cabi.cpp.txt").read_text() + "\n" + t[i:]).encode()


CORE_OLD = '  if (rva == snd_crt::ftol_) return "audio_core.crt.ftol";\n  return nullptr;\n}\n'
CORE_NEW = ('  if (rva == snd_crt::ftol_) return "audio_core.crt.ftol";\n'
            '  if (const char *n = audio_crt_function_name(rva)) return n;  // S4.11: audio_crt.cpp\n  return nullptr;\n}\n')
CORE_DECL_OLD = "const char *audio_crt_name(uint32_t rva);  // part 2\n"
CORE_DECL_NEW = CORE_DECL_OLD + "const char *audio_crt_function_name(uint32_t rva);  // S4.11: audio_crt.cpp\n"


def patched_core(b: bytes) -> bytes:
    """the merge request's audio_core.cpp lines (11_merge.md section 2): the core asks audio_crt for the rvas it has no name for"""
    t = b.decode("utf-8").replace("\r\n", "\n")
    assert t.count(CORE_OLD) == 1 and t.count(CORE_DECL_OLD) == 1, "audio_core.cpp anchor moved"
    return t.replace(CORE_OLD, CORE_NEW).replace(CORE_DECL_OLD, CORE_DECL_NEW).encode("utf-8")


def mirror(dst: pathlib.Path, with_crt: bool):
    modified = set(git("diff", "--name-only", "HEAD", "--", "port/").split())
    untracked = set(git("ls-files", "--others", "--exclude-standard", "--", "port/").split())
    mods = MODULES + (("audio_crt",) if with_crt else ())
    seen = set()

    def put(rel: str, data: bytes):
        d = dst / rel
        seen.add(d)
        d.parent.mkdir(parents=True, exist_ok=True)
        if not d.exists() or d.read_bytes() != data:
            d.write_bytes(data)
            if rel == "src/sim/modules.inc":
                (dst / "src" / "registry.cpp").touch()

    files = []
    for dirpath, dirnames, filenames in os.walk(SRC):
        here = pathlib.Path(dirpath)
        if here == SRC:
            dirnames[:] = [d for d in dirnames if d not in ("build", "corpus", "app")]
        dirnames[:] = [d for d in dirnames if d != "__pycache__"]
        files += [here / f for f in filenames]
    for p in files:
        r = p.relative_to(SRC).as_posix()
        full = "port/" + r
        if full in untracked:
            if not (is_audio(r) and (r.startswith("src/sim/") or r.startswith("include/portsim/sim/"))):
                continue                         # a sibling's new file: not part of this build
            if r == "src/sim/audio_crt.cpp" and not with_crt:
                continue
            data = p.read_bytes()
            if r == "src/sim/audio_core.cpp" and with_crt:
                data = patched_core(data)
        elif r == "src/sim/modules.inc":
            data = (head(r).decode().rstrip("\n") + "\n" + "".join(f"SG_MODULE({m})\n" for m in mods)).encode()
        elif r == "include/portsim_c.h":
            data = patched_c_h()
        elif r == "src/portsim_c.cpp":
            data = patched_c_cpp()
        elif full in modified:
            data = head(r)
        else:
            data = p.read_bytes()
        put(r, data)
    for p in MODEL.rglob("*"):
        if p.is_file():
            d = dst.parent / "re" / "model" / "include" / p.relative_to(MODEL)
            d.parent.mkdir(parents=True, exist_ok=True)
            if not d.exists() or d.read_bytes() != p.read_bytes():
                shutil.copy2(p, d)
            seen.add(d)
    for d in list(dst.rglob("*")):
        if d.is_file() and d not in seen:
            d.unlink()


def build(src: pathlib.Path, arch: str, out: pathlib.Path):
    cm = B.cmake_exe()
    subprocess.run([cm, "-S", str(src), "-B", str(out), "-A", arch], check=True, stdout=subprocess.DEVNULL)
    for t in ["portsim_c"] + (["portsim_runner32"] if arch == "Win32" else []):
        r = subprocess.run([cm, "--build", str(out), "--config", "Release", "--target", t, "--", "/m", "/nologo",
                            "/v:minimal"], capture_output=True, text=True)
        if r.returncode:
            print(r.stdout[-8000:], r.stderr[-2000:])
            raise SystemExit(f"build of {t} ({arch}) failed")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-crt", action="store_true")
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--only-w32", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    a = ap.parse_args()
    sfx = "_nocrt" if a.no_crt else ""
    dst = MIR / f"root{sfx}" / "port"
    mirror(dst, not a.no_crt)
    if a.no_build:
        return
    if not a.only_w32:
        build(dst, "x64", MIR / f"x64{sfx}")
        print("SG_PORTSIM_DLL=" + str(MIR / f"x64{sfx}" / "Release" / "portsim_c.dll"))
    if a.w32 or a.only_w32:
        build(dst, "Win32", MIR / f"w32{sfx}")
        print("runner32=" + str(MIR / f"w32{sfx}" / "Release" / "portsim_runner32.exe"))


if __name__ == "__main__":
    main()
