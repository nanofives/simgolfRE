"""S4.2b private build (log/roadmap/S4/2_mirror.py pattern): mirror port/ into port/build/S4_2b_mirror/root/port with
  * every tracked port/ file a sibling has modified built at its committed version (HEAD),
  * the sound.dll port's files from the tree (the core, the ten audio modules and their headers: TREE below),
  * modules.inc = HEAD + the ten SG_MODULE lines (MODULES), registry.cpp touched,
  * portsim_c.h / portsim_c.cpp = HEAD + S4.2's C ABI addition (log/roadmap/S4/2_cabi.{h,cpp}.txt) + sg_state_free's
    audio_release (the only per-context sound state since S4.2b),
then build x64 (port/build/S4_2b_mirror/x64) and, with --w32, Win32 with the runner (w32).

(Until the wave-1 files could be written in the tree this script built them from log/roadmap/S4/2b_wave1/ copies, and a
--baseline mode built the accepted state; its binaries stay in port/build/S4_2b_mirror/x64_base and w32_base.)

    ~/bin/py -3.12 log/roadmap/S4/2b_mirror.py [--w32] [--no-build]
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "port"
MIR = ROOT / "port" / "build" / "S4_2b_mirror"
HERE = pathlib.Path(__file__).resolve().parent
MODEL = ROOT / "re" / "model" / "include"
MODULES = ("audio_core", "audio_mixer", "audio_chan", "audio_ds", "audio_dev", "audio_codec", "audio_seq", "audio_sound",
           "audio_wave", "audio_soundfile", "audio_crt", "audio_exe")
HEADERS = ("audio", "audio_sink", "soundstate", "audio_chan", "audio_dev", "audio_mixer", "audio_codec", "audio_seq",
           "audio_sound", "audio_wave", "audio_exe")
TREE = ({f"src/sim/{m}.cpp" for m in MODULES} | {f"include/portsim/sim/{h}.h" for h in HEADERS}
        | {"src/sim/audio_x80.inc"})
sys.path.insert(0, str(ROOT))
from port.test import build as B  # noqa: E402


def git(*a, binary=False):
    r = subprocess.run(["git", *a], cwd=ROOT, capture_output=True, check=True, text=not binary)
    return r.stdout


def head(rel):
    return git("show", f"HEAD:port/{rel}", binary=True)


def shared_port_files() -> dict:
    """The shared port files (modules.inc, portsim_c.h / .cpp, tick.cpp, tick_boot.inc) exactly as log/roadmap/S4/2b_apply_shared.py
    leaves them: the mirror builds with the edits the merge request applies."""
    import importlib.util
    spec = importlib.util.spec_from_file_location("s42b_apply", HERE / "2b_apply_shared.py")
    ap = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(ap)
    return {k[len("port/"):]: v for k, v in ap.shared_texts().items() if k.startswith("port/")}


def mirror(dst: pathlib.Path):
    modified = set(git("diff", "--name-only", "HEAD", "--", "port/").split())
    untracked = set(git("ls-files", "--others", "--exclude-standard", "--", "port/").split())
    seen = set()

    def put(rel: str, data: bytes):
        d = dst / rel
        seen.add(d)
        d.parent.mkdir(parents=True, exist_ok=True)
        if not d.exists() or d.read_bytes() != data:
            d.write_bytes(data)
            if rel == "src/sim/modules.inc":
                (dst / "src" / "registry.cpp").touch()

    shared = shared_port_files()
    files = []
    for dirpath, dirnames, filenames in os.walk(SRC):
        here = pathlib.Path(dirpath)
        if here == SRC:
            dirnames[:] = [d for d in dirnames if d not in ("build", "corpus")]
        dirnames[:] = [d for d in dirnames if d != "__pycache__"]
        files += [here / f for f in filenames]
    for p in files:
        r = p.relative_to(SRC).as_posix()
        full = "port/" + r
        if full in untracked:
            if r not in TREE:
                continue                         # a sibling's new file: not part of this build
            data = p.read_bytes()
        elif r in shared:
            data = shared[r]
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
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    a = ap.parse_args()
    dst = MIR / "root" / "port"
    mirror(dst)
    if a.no_build:
        return
    build(dst, "x64", MIR / "x64")
    print("SG_PORTSIM_DLL=" + str(MIR / "x64" / "Release" / "portsim_c.dll"))
    if a.w32:
        build(dst, "Win32", MIR / "w32")
        print("runner32=" + str(MIR / "w32" / "Release" / "portsim_runner32.exe"))


if __name__ == "__main__":
    main()
