"""S4.10 private build (copy of log/roadmap/S4/2b_mirror.py with its own port/build/S4_10_mirror): mirror port/ into
port/build/S4_10_mirror/root/port with
  * every tracked port/ file a sibling has modified built at its committed version (HEAD),
  * the S4 audio files from the tree (the accepted steps' modules as S4.2b left them; --modules picks which sound.dll modules
    are built: S4.2b is still migrating audio_seq / audio_sound / audio_wave / audio_codec / audio_soundfile),
  * S4.2b's wave-1 copies of audio.h / audio_core.cpp (log/roadmap/S4/2b_wave1/port/),
  * this step's files (audio_exe.cpp / audio_exe.h),
  * modules.inc = HEAD + SG_MODULE(<modules> + audio_exe), registry.cpp touched,
  * portsim_c.h / portsim_c.cpp = HEAD + S4.2's C ABI addition (log/roadmap/S4/2_cabi.{h,cpp}.txt) + audio_release in
    sg_state_free (S4.2b's form),
  * tick.cpp = HEAD with this step's merge edit (log/roadmap/S4/10_merge.md section 2: playSound / startSoundSlot /
    Snd::status leave kDrops) unless --keep-drops,
then build x64 (x64) and, with --w32, Win32 with the runner (w32).

    ~/bin/py -3.12 log/roadmap/S4/10_mirror.py [--w32] [--no-build] [--keep-drops] [--modules core,mixer,chan,ds,dev,...]
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "port"
MIR = ROOT / "port" / "build" / "S4_10_mirror"
HERE = pathlib.Path(__file__).resolve().parent
WAVE1 = HERE / "2b_wave1" / "port"
MODEL = ROOT / "re" / "model" / "include"
DEFAULT_MODULES = ("audio_core", "audio_mixer", "audio_chan", "audio_ds", "audio_dev")
ALL_SOUND = ("audio_core", "audio_mixer", "audio_chan", "audio_ds", "audio_dev", "audio_seq", "audio_sound", "audio_wave",
             "audio_codec", "audio_soundfile", "audio_crt")
USE_WAVE1_COPY = False
PATCH_WAVE = True       # the setRegionMs argument fix (patched_wave); --no-wave-fix builds the tree file as it is
OWN = {"src/sim/audio_exe.cpp", "include/portsim/sim/audio_exe.h"}
WAVE1_FILES = {"include/portsim/sim/audio.h", "src/sim/audio_core.cpp"}
SHARED_TREE = {"include/portsim/sim/audio_sink.h", "include/portsim/sim/soundstate.h", "src/sim/audio_x80.inc"}
# kDrops lines this step's merge request removes (10_merge.md section 2)
DROP_LINES = ('    {"playSound", "audio request"},\n', '    {"startSoundSlot", "audio request"},\n',
              '    {"Snd::status", "audio device status query; the frame discards it (ML+0xf3c)"},\n')
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
    return t.replace(anchor, (HERE / "2_cabi.h.txt").read_text() + anchor).encode()


def patched_c_cpp() -> bytes:
    t = head("src/portsim_c.cpp").decode()
    t = t.replace('#include "portsim/sim/tick.h"\n', '#include "portsim/sim/tick.h"\n#include "portsim/sim/audio.h"\n', 1)
    t = t.replace("#include <cstring>\n", "#include <cstddef>\n#include <cstring>\n", 1)
    old = "void sg_state_free(sg_ctx *ctx) { delete ctx; }"
    assert t.count(old) == 1
    t = t.replace(old, "void sg_state_free(sg_ctx *ctx) {\n  if (ctx) portsim::audio_release(ctx->ctx);  // S4.2\n"
                       "  delete ctx;\n}")
    end = '}  // extern "C"'
    i = t.rindex(end)
    return (t[:i] + (HERE / "2_cabi.cpp.txt").read_text() + "\n" + t[i:]).encode()


def patched_tick(keep_drops: bool) -> bytes:
    t = head("src/sim/tick.cpp").decode().replace("\r\n", "\n")
    if not keep_drops:
        for line in DROP_LINES:
            assert t.count(line) == 1, line
            t = t.replace(line, "")
    return t.encode()


NL = chr(10)
BOOT_EDITS = [
    # the CRT's static construction of g_sounds (dynInit_arrayStaticInit_448160 -> arrayStaticInit_448160), before gameMain
    ('    cc("staticInit_820f04");' + NL,
     '    cc("staticInit_820f04");' + NL +
     '    cc("arrayStaticInit_448160");   // the CRT initializer dynInit_arrayStaticInit_448160: the 300 g_sounds records' + NL),
    # gameMain+0x42e loadSoundTable (after showTitleScreen, gameMain+0x41f)
    ('    cc("showTitleScreen");' + NL,
     '    cc("showTitleScreen");' + NL + '    cc("loadSoundTable");                                             // gameMain+0x42e' + NL),
    # ML+0x2f playSound(0x80, 0x64, 0, 0, 0)
    ("    // ML+0x2f playSound(0x80, 0x64, 0, 0, 0): the audio backend's" + NL,
     '    cc("playSound", 0x80, 0x64, 0, 0, 0);                             // ML+0x2f' + NL),
]


def patched_boot() -> bytes:
    """tick_boot.inc: with playSound / startSoundSlot / Snd::status out of kDrops the frame calls the module, and its Snd
    records must be the constructed, table-filled ones the original has at the first frame (10_merge.md section 2b)."""
    t = head("src/sim/tick_boot.inc").decode().replace(chr(13) + chr(10), chr(10))
    for a, b in BOOT_EDITS:
        assert t.count(a) == 1, a
        t = t.replace(a, b)
    return t.encode()


def patched_core_crt(b: bytes) -> bytes:
    """S4.11's audio_crt.cpp exposes audio_crt_function_name(rva) for the core's CRT lookup; the core in the tree does not
    consult it yet (S4.11's merge): the mirror adds that one line so the CRT the module ports is reached by rva."""
    t = b.decode().replace("\r\n", "\n")
    old = '  if (rva == snd_crt::ftol_) return "audio_core.crt.ftol";\n'
    assert t.count(old) == 1
    t = t.replace(old, old + "  if (const char *n = audio_crt_function_name(rva)) return n;  // S4.11 (mirror only)\n")
    decl = "const char *audio_crt_name(uint32_t rva);  // part 2\n"
    assert t.count(decl) == 1
    t = t.replace(decl, decl + "const char *audio_crt_function_name(uint32_t rva);  // audio_crt.cpp (S4.11)\n")
    # this step's own sound.dll functions without a hooks.csv row (10_merge.md section 3)
    old2 = "  if (const char *n = audio_crt_function_name(rva)) return n;  // S4.11 (mirror only)\n"
    t = t.replace(old2, old2 + "  if (const char *n = audio_exe_sound_name(rva)) return n;  // S4.10 (mirror only)\n")
    t = t.replace(decl, decl + "const char *audio_exe_sound_name(uint32_t rva);  // audio_exe.cpp (S4.10)\n")
    return t.encode()


def patched_wave(b: bytes) -> bytes:
    """Wave_Device::open / start / restart (audio_wave.cpp, S4.9): in the original, slot 0x40 of the device (FUN_1000e830, a
    bare `ret`) leaves its pushed 0xe on the stack and DsBuffer::setRegionMs (`ret 8`, reads [esp + 4] and [esp + 8]) takes it
    as its second argument (open 0xb5b1..0xb5bf, 0xb680..0xb6ac; start 0xbb64..0xbb72, 0xbbfe..0xbc23; restart 0xc114..0xc122);
    the port passed one argument. Mirror-only fix, reported in 10_merge.md section 4."""
    t = b.decode().replace(chr(13) + chr(10), chr(10))
    n = 0
    for line in t.split(chr(10)):
        if "0x0000182f" in line and "ms})" in line:
            n += 1
    assert n == 7, n
    t = t.replace("), ms});", "), ms, 0xe});").replace(".va()), ms}).eax;", ".va()), ms, 0xe}).eax;")
    # slot 0x40 itself reads no argument (`ret`): the port calls it with none, the 0xe goes to setRegionMs above
    assert t.count("0x40, SndAbi::Thiscall, {0xe})") == 7, t.count("0x40, SndAbi::Thiscall, {0xe})")
    t = t.replace("0x40, SndAbi::Thiscall, {0xe})", "0x40, SndAbi::Thiscall, {})")
    # flushSoundList (thunk 0x1f0f, rva 0x8470) reads no argument and no ecx (the core registers it cdecl, arity 0); the five
    # call sites load ecx first (0x7480, 0x7452, 0x7a69, 0x7a31, 0xbe34), which the port passed as a thiscall slot
    import re as _re
    t, k = _re.subn(r"snd_call\(ctx, 0x00001f0f, SndAbi::Thiscall, \{[^;]*?\}\)", "snd_call(ctx, 0x00001f0f, SndAbi::Cdecl, {})", t)
    assert k == 5, k
    # 0x32fc0..0x32fca: `push 8; call [edx + 0x40]` (FUN_1000e830, `mov eax, [ecx + 0x1ac]; ret`: the 8 stays on the stack),
    # `push eax; call setRegionMs`: setRegionMs(this, [wd + 0x1ac], 8)
    old = ("                unsigned v = snd_vcall(ctx, wd, 0x40, SndAbi::Thiscall, {8}).eax;  // 0x32fc4\n"
           "                snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {ld<u32>(ctx, self + 0x54), v});  // 0x32fca\n")
    new = ("                unsigned v = snd_vcall(ctx, wd, 0x40, SndAbi::Thiscall, {}).eax;  // 0x32fc4 (`ret`: the pushed 8 stays)\n"
           "                snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {ld<u32>(ctx, self + 0x54), v, 8});  // 0x32fc9 / 0x32fca\n")
    assert t.count(old) == 1, t.count(old)
    t = t.replace(old, new)
    return t.encode()


def mirror(dst: pathlib.Path, modules, keep_drops: bool):
    modified = set(git("diff", "--name-only", "HEAD", "--", "port/").split())
    untracked = set(git("ls-files", "--others", "--exclude-standard", "--", "port/").split())
    tree = set(SHARED_TREE) | OWN
    for m in modules:
        if m == "audio_core":
            continue
        tree.add(f"src/sim/{m}.cpp")
        h = f"include/portsim/sim/{m}.h"
        if (SRC / h).exists():
            tree.add(h)
    # headers of every sound module (a module's .cpp may include a sibling's header)
    for m in ALL_SOUND:
        h = f"include/portsim/sim/{m}.h"
        if (SRC / h).exists():
            tree.add(h)
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
        if r in WAVE1_FILES and USE_WAVE1_COPY:
            data = (WAVE1 / r).read_bytes()
        elif r in WAVE1_FILES:
            data = p.read_bytes()                # S4.2b writes audio.h / audio_core.cpp in the tree since wave 1's refusal
            if r == "src/sim/audio_core.cpp" and "audio_crt" in modules:
                data = patched_core_crt(data)
        elif full in untracked:
            if r not in tree:
                continue                         # a sibling's new file: not part of this build
            data = p.read_bytes()
            if r == "src/sim/audio_wave.cpp" and PATCH_WAVE:
                data = patched_wave(data)
        elif r == "src/sim/modules.inc":
            data = (head(r).decode().rstrip("\n") + "\n" +
                    "".join(f"SG_MODULE({m})\n" for m in list(modules) + ["audio_exe"])).encode()
        elif r == "include/portsim_c.h":
            data = patched_c_h()
        elif r == "src/portsim_c.cpp":
            data = patched_c_cpp()
        elif r == "src/sim/tick.cpp":
            data = patched_tick(keep_drops)
        elif r == "src/sim/tick_boot.inc" and not keep_drops:
            data = patched_boot()
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
                            "/v:minimal"], capture_output=True, text=True, encoding="utf-8", errors="replace")
        if r.returncode:
            print(r.stdout[-12000:], r.stderr[-2000:])
            raise SystemExit(f"build of {t} ({arch}) failed")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--keep-drops", action="store_true", help="tick.cpp at HEAD (playSound etc. stay dropped)")
    ap.add_argument("--modules", default=",".join(DEFAULT_MODULES),
                    help="sound.dll modules to build (audio_ prefix optional; 'all' = every S4 module)")
    ap.add_argument("--tag", default="", help="suffix of the build directories (a second configuration)")
    ap.add_argument("--wave1-copy", action="store_true", help="audio.h / audio_core.cpp from log/roadmap/S4/2b_wave1")
    a = ap.parse_args()
    mods = ALL_SOUND if a.modules == "all" else tuple(m if m.startswith("audio_") else "audio_" + m
                                                      for m in a.modules.split(",") if m)
    global USE_WAVE1_COPY
    USE_WAVE1_COPY = a.wave1_copy
    sfx = a.tag
    dst = MIR / f"root{sfx}" / "port"
    mirror(dst, mods, a.keep_drops)
    if a.no_build:
        return
    build(dst, "x64", MIR / f"x64{sfx}")
    print("SG_PORTSIM_DLL=" + str(MIR / f"x64{sfx}" / "Release" / "portsim_c.dll"))
    if a.w32:
        build(dst, "Win32", MIR / f"w32{sfx}")
        print("runner32=" + str(MIR / f"w32{sfx}" / "Release" / "portsim_runner32.exe"))


if __name__ == "__main__":
    main()
