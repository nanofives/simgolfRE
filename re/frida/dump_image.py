"""Dump golf.exe's in-memory (SafeDisc-decrypted) image.
Usage: py -3.12 re/frida/dump_image.py <out.bin> [--after SECONDS]
Without --after, dumps when the game calls ExitProcess."""
import frida, subprocess, sys, time, pathlib, argparse, threading
ROOT = pathlib.Path(__file__).resolve().parents[2]
GAME = ROOT / "original"
def spawn_via_injector():
    before = {p.pid for p in frida.get_local_device().enumerate_processes() if p.name.lower() == "golf.exe"}
    subprocess.Popen([str(GAME / "VersionInjector.exe"), "golf.exe"], cwd=GAME)
    t0 = time.time()
    while time.time() - t0 < 10:
        for p in frida.get_local_device().enumerate_processes():
            if p.name.lower() == "golf.exe" and p.pid not in before:
                return p.pid
        time.sleep(0.05)
    raise SystemExit("golf.exe never appeared")
def main():
    ap = argparse.ArgumentParser(); ap.add_argument("out"); ap.add_argument("--after", type=float)
    a = ap.parse_args()
    pid = spawn_via_injector(); print("pid", pid)
    sess = frida.attach(pid)
    sc = sess.create_script((pathlib.Path(__file__).parent / "js" / "dump_on_exit.js").read_text())
    state = {"buf": None, "done": threading.Event()}
    def on(m, data):
        if m["type"] != "send": print(m); return
        p = m["payload"]
        if p["type"] == "begin":
            print("dump begin", p); state["buf"] = bytearray(p["size"]); state["base"] = p["base"]
        elif p["type"] == "chunk":
            state["buf"][p["off"]:p["off"] + len(data)] = data
        elif p["type"] == "end":
            pathlib.Path(a.out).write_bytes(state["buf"]); print("wrote", a.out, len(state["buf"]))
            sc.post({"type": "ack"}); state["done"].set()
    sc.on("message", on); sc.load()
    if a.after:
        time.sleep(a.after); threading.Thread(target=sc.exports_sync.dump, daemon=True).start()
    state["done"].wait(120)
    try:
        if a.after: frida.kill(pid)
    except Exception: pass
if __name__ == "__main__":
    main()
