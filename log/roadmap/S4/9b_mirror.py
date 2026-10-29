"""S4.9b private build (pattern of 9_mirror.py; MIR port/build/S4_9b_mirror).

The live tree's S4 files are being rewritten by S4.2b (audio.h / the SndFrame stack helper) while this step runs, so the mirror is
NOT taken from the tree. It is a frozen copy of the accepted S4 builds:
  * port/build/S4_9_mirror/root (S4.9's final, accepted tree: audio_core, audio_mixer, audio_chan, audio_seq, audio_ds, audio_dev,
    audio_wave, the C ABI patch of 2_cabi.*, modules.inc with those modules),
  * audio_sound.cpp / audio_sound.h (S4.7) and audio_codec.cpp / audio_codec.h (S4.3) from port/build/S4_3_mirror/root (both
    accepted; the base files of S4_3_mirror and S4_9_mirror are byte-identical, checked below),
  * this step's port/src/sim/audio_soundfile.cpp as it is on disk,
  * modules.inc = S4.9's + SG_MODULE(audio_sound) SG_MODULE(audio_codec) SG_MODULE(audio_soundfile).
The frozen base is copied once into MIR/base (BASE_SHA lists its files' sha1) and rebuilt from there.

    ~/bin/py -3.12 log/roadmap/S4/9b_mirror.py [--w32] [--only-w32] [--no-build] [--refreeze]
Prints SG_PORTSIM_DLL=<x64 dll>, and runner32= / dll32= for --w32.
"""
import argparse
import hashlib
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
MIR = ROOT / "port" / "build" / "S4_9b_mirror"
BASE = MIR / "base"                 # frozen copy of the accepted tree (port/ and re/model/include)
DST = MIR / "root" / "port"
S49 = ROOT / "port" / "build" / "S4_9_mirror" / "root"
S43 = ROOT / "port" / "build" / "S4_3_mirror" / "root"
FROM_S43 = ("src/sim/audio_sound.cpp", "include/portsim/sim/audio_sound.h", "src/sim/audio_codec.cpp",
            "include/portsim/sim/audio_codec.h")
SAME_IN_BOTH = ("src/sim/audio_core.cpp", "src/sim/audio_chan.cpp", "src/sim/audio_dev.cpp", "src/sim/audio_ds.cpp",
                "src/sim/audio_mixer.cpp", "src/sim/audio_seq.cpp", "src/sim/audio_x80.inc", "include/portsim/sim/audio.h",
                "include/portsim/sim/soundstate.h", "include/portsim/sim/audio_sink.h", "include/portsim/sim/audio_dev.h",
                "include/portsim/sim/audio_chan.h", "include/portsim/sim/audio_seq.h", "include/portsim/sim/audio_mixer.h",
                "include/portsim_c.h")
OWN = ("src/sim/audio_soundfile.cpp",)
EXTRA_MODULES = ("audio_sound", "audio_codec", "audio_soundfile")
sys.path.insert(0, str(ROOT))
from port.test import build as B  # noqa: E402


def sha1(p: pathlib.Path) -> str:
    return hashlib.sha1(p.read_bytes()).hexdigest()


def freeze():
    for rel in SAME_IN_BOTH:
        a, b = S49 / "port" / rel, S43 / "port" / rel
        if sha1(a) != sha1(b):
            raise SystemExit(f"base differs between S4_9_mirror and S4_3_mirror: {rel}")
    if BASE.exists():
        shutil.rmtree(BASE)
    shutil.copytree(S49 / "port", BASE / "port", ignore=shutil.ignore_patterns("__pycache__"))
    shutil.copytree(S49 / "re", BASE / "re")
    for rel in FROM_S43:
        d = BASE / "port" / rel
        d.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(S43 / "port" / rel, d)
    with open(MIR / "BASE_SHA.txt", "w") as f:
        for p in sorted(BASE.rglob("*")):
            if p.is_file():
                f.write(f"{sha1(p)}  {p.relative_to(BASE).as_posix()}\n")


def mirror():
    if not (BASE / "port").exists():
        freeze()
    seen = set()

    def put(rel_dst: pathlib.Path, data: bytes):
        d = DST.parent / rel_dst
        seen.add(d)
        d.parent.mkdir(parents=True, exist_ok=True)
        if not d.exists() or d.read_bytes() != data:
            d.write_bytes(data)
            if rel_dst.as_posix() == "port/src/sim/modules.inc":
                (DST / "src" / "registry.cpp").touch()

    for p in BASE.rglob("*"):
        if p.is_dir():
            continue
        rel = p.relative_to(BASE)
        data = p.read_bytes()
        if rel.as_posix() == "port/src/sim/modules.inc":
            data = (data.decode().rstrip("\n") + "\n" + "".join(f"SG_MODULE({m})\n" for m in EXTRA_MODULES
                                                                if m != "audio_soundfile" or (ROOT / "port" / OWN[0]).exists())).encode()
        put(rel, data)
    for rel in OWN:
        src = ROOT / "port" / rel
        if src.exists():
            put(pathlib.Path("port") / rel, src.read_bytes())
    for d in list(DST.parent.rglob("*")):
        if d.is_file() and d not in seen:
            d.unlink()


def build(arch, out):
    cm = B.cmake_exe()
    subprocess.run([cm, "-S", str(DST), "-B", str(out), "-A", arch], check=True, stdout=subprocess.DEVNULL)
    targets = ["portsim_c"] + (["portsim_runner32"] if arch == "Win32" else [])
    for t in targets:
        r = subprocess.run([cm, "--build", str(out), "--config", "Release", "--target", t, "--", "/m", "/nologo",
                            "/v:minimal"], capture_output=True, text=True)
        if r.returncode:
            print(r.stdout[-8000:], r.stderr[-2000:])
            raise SystemExit(f"build of {t} ({arch}) failed")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--only-w32", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--refreeze", action="store_true", help="copy the accepted base again")
    a = ap.parse_args()
    MIR.mkdir(parents=True, exist_ok=True)
    if a.refreeze or not (BASE / "port").exists():
        freeze()
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
