"""Boot tracer: spawn the game suspended under Frida (hooks live before the first instruction),
log registry / drive / file / window / exit APIs.
Usage: py -3.12 re/frida/boot_trace.py [--exe golf_clean.exe] [--seconds 30] [--injector]"""
import argparse, frida, pathlib, subprocess, sys, time
ROOT = pathlib.Path(__file__).resolve().parents[2]
GAME = ROOT / "original"
JS = (pathlib.Path(__file__).parent / "js" / "boot_trace.js").read_text()

def spawn_via_injector(exe="golf.exe"):
    dev = frida.get_local_device()
    before = {p.pid for p in dev.enumerate_processes() if p.name.lower() == exe.lower()}
    subprocess.Popen([str(GAME / "VersionInjector.exe"), exe], cwd=GAME)
    t0 = time.time()
    while time.time() - t0 < 10:
        for p in dev.enumerate_processes():
            if p.name.lower() == exe.lower() and p.pid not in before:
                return p.pid
        time.sleep(0.05)
    raise SystemExit(f"{exe} never appeared")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default="golf_clean.exe")
    ap.add_argument("--seconds", type=float, default=30)
    ap.add_argument("--injector", action="store_true", help="launch through VersionInjector (SafeDisc build)")
    ap.add_argument("args", nargs="*")
    a = ap.parse_args()
    if a.injector:
        pid = spawn_via_injector(a.exe); spawned = False
    else:
        pid = frida.spawn([str(GAME / a.exe), *a.args], cwd=str(GAME)); spawned = True
    print("pid", pid, flush=True)
    sess = frida.attach(pid)
    sc = sess.create_script(JS)
    sc.on("message", lambda m, d: print(m.get("payload", m), flush=True))
    sess.on("detached", lambda r, c: print("detached:", r, flush=True))
    sc.load()
    if spawned:
        frida.resume(pid)
    time.sleep(a.seconds)
    try:
        frida.kill(pid)
    except Exception:
        pass
    return 0

if __name__ == "__main__":
    sys.exit(main())
