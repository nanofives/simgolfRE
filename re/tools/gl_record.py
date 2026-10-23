r"""Record the GL call stream Terrain.dll issues for rendered frames (Phase 5 step 3, re/PHASE5_TERRAIN_PLAN.md 2.3).

    ~/bin/py -3.12 re/tools/gl_record.py --label sandbox_a [--flushes 1] [--move]

Boots the game (hooks as deployed), goes to the sandbox, arms re/frida/js/gl_rec.js for N glFlush calls (one per
rendered terrain strip pass / frame), optionally nudges the camera so the terrain re-renders, and writes
log/p5/gl/<label>.gl.txt (one call per line) plus <label>.png (the window right after). Run it twice with the same
label prefix and `cmp` the two files to see whether the stream is reproducible.
"""
import argparse
import hashlib
import pathlib
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import game  # noqa: E402
import scenario  # noqa: E402

OUT = ROOT / "log" / "p5" / "gl"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--label", required=True)
    ap.add_argument("--flushes", type=int, default=1)
    ap.add_argument("--move", action="store_true", help="press an arrow key to force a terrain re-render")
    ap.add_argument("--timeout", type=float, default=30)
    ap.add_argument("--hooks-off", action="store_true", help="run the ORIGINAL Terrain.dll code (SIMGOLF_HOOKS_OFF=all)")
    ap.add_argument("--hole", action="store_true", help="build the first hole before recording")
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    chunks, done, info = {}, [], {}

    def on_msg(m, _d):
        p = m.get("payload") if m.get("type") == "send" else None
        if not isinstance(p, dict) or "glrec" not in p:
            return
        if p["glrec"] == "chunk":
            chunks[p["i"]] = p["lines"]
        elif p["glrec"] == "done":
            done.append(p)
        else:
            info.update(p)

    g = game.Game(extra_js=[str(ROOT / "re" / "frida" / "js" / "gl_rec.js")], env={"SIMGOLF_SKIP_INTRO": "1", **({"SIMGOLF_HOOKS_OFF": "all"} if a.hooks_off else {})})
    g.on_message = on_msg
    with g:
        t0 = time.time()
        while not g.hwnd() and time.time() - t0 < 30:
            time.sleep(0.25)
        scenario.to_sandbox(g)
        if a.hole:
            scenario.build_first_hole(g)
        rec = g.scripts[-1].exports_sync
        t0 = time.time()
        while "hooks" not in info and time.time() - t0 < 15:
            time.sleep(0.2)
        print("recorder:", info, flush=True)
        rec.glrec_arm(a.flushes)
        t0 = time.time()
        while not done and time.time() - t0 < a.timeout:
            if a.move:
                g.key(0x27, hold=0.1)   # VK_RIGHT: camera moves, terrain re-renders
            time.sleep(0.5)
        st = rec.glrec_state()
        print('seen (all callers):', dict(sorted(rec.glrec_seen().items(), key=lambda kv: -kv[1])[:12]))
        img = g.shot()
        img.save(OUT / f"{a.label}.png")
    lines = [l for i in sorted(chunks) for l in chunks[i]]
    (OUT / f"{a.label}.gl.txt").write_text("\n".join(lines) + "\n")
    names = {}
    for l in lines:
        n = l.split(" ", 1)[0]
        names[n] = names.get(n, 0) + 1
    print("done:", done, "state:", st)
    print("calls:", len(lines), "sha256:", hashlib.sha256("\n".join(lines).encode()).hexdigest()[:16])
    for n, c in sorted(names.items(), key=lambda kv: -kv[1]):
        print(f"  {c:8d} {n}")
    return 0 if done else 1


if __name__ == "__main__":
    sys.exit(main())
