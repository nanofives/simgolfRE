r"""Record the ORIGINAL Terrain.dll's state side: every exported Terrain call from process start plus snapshots of the Terrain object
and the DLL's data (phase 5 step 3, re/PHASE5_TERRAIN_PLAN.md section 13; agent re/frida/js/terrain_calls.js).

    ~/bin/py -3.12 re/tools/terrain_calls.py --label t1

One boot with every shim hook off (SIMGOLF_HOOKS_OFF=all), driven through the sandbox like re/tools/gl_corpus.py: start, a built hole,
the Build Course palette, cart paths, terrain edits (raise / lower), the terrain tools, zoom. After each step a snapshot is taken on
the game thread at the next Terrain::render entry; around the first calls of each mutator a before / after pair is taken.
Output log/p5/terrain/<label>/: calls.txt (one line per exported call, see the agent), snaps/<name>/{tobj.bin, tdata.bin, collar.bin,
meta.json} with meta.seq = the log position the snapshot sits before.
"""
import argparse
import json
import pathlib
import sys
import threading
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import game  # noqa: E402
import scenario  # noqa: E402

OUT = ROOT / "log" / "p5" / "terrain"
NL = chr(10)
VK_X, VK_Z, VK_E, VK_ESC, VK_RIGHT, VK_LEFT, VK_PGUP = 0x58, 0x5A, 0x45, 0x1B, 0x27, 0x25, 0x21
DETAIL = {"setType": 4, "elevateCorner": 4, "lowerCorner": 4, "layPath": 3, "updatePath": 2, "setWall": 3, "lowerEdgeCorner": 2,
          "calcAllNormals": 2, "calcNormals": 2, "passCollarInfo": 1, "initTerrain": 1, "loadNewCourseType": 1, "resetTerrain": 1}


class Collector:
    def __init__(self):
        self.chunks = []
        self.done = 0
        self.snaps = []
        self.cur = None
        self.info = {}
        self.ev = threading.Event()

    def on_msg(self, m, d):
        p = m.get("payload") if m.get("type") == "send" else None
        if not isinstance(p, dict) or "tc" not in p:
            if m.get("type") == "error":
                print("  agent error:", m.get("description"), flush=True)
            return
        k = p["tc"]
        if k == "installed":
            self.info.update(p)
        elif k == "snap":
            self.cur = {"meta": p, "data": {}}
        elif k == "data":
            self.cur["data"][p["kind"]] = bytes(d)
        elif k == "snapend":
            self.snaps.append(self.cur)
            self.cur = None
        elif k == "chunk":
            self.chunks.append(p["lines"])
        elif k == "done":
            self.done += 1
            self.ev.set()


def save(root: pathlib.Path, s: dict, used: dict) -> str:
    m = s["meta"]
    name = m["label"] if m["where"] == "render" else f"{m['label']}.{m['where']}"
    name = name.replace("#", "_")
    if name in used:
        used[name] += 1
        name = f"{name}.{used[name]}"
    else:
        used[name] = 0
    d = root / "snaps" / name
    d.mkdir(parents=True, exist_ok=True)
    for k, v in s["data"].items():
        (d / f"{k}.bin").write_bytes(v)
    (d / "meta.json").write_text(json.dumps(m, indent=1))
    return name


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--label", required=True)
    ap.add_argument("--settle", type=float, default=1.6)
    a = ap.parse_args()
    root = OUT / a.label
    root.mkdir(parents=True, exist_ok=True)
    c = Collector()
    used = {}
    g = game.Game(extra_js=[str(ROOT / "re" / "frida" / "js" / "terrain_calls.js")],
                  env={"SIMGOLF_SKIP_INTRO": "1", "SIMGOLF_HOOKS_OFF": "all"})
    g.on_message = c.on_msg
    names = []
    with g:
        t0 = time.time()
        while not g.hwnd() and time.time() - t0 < 30:
            time.sleep(0.25)
        rec = g.scripts[-1].exports_sync

        def flush_snaps():
            n = rec.tc_take()
            t1 = time.time()
            while len(c.snaps) < n and time.time() - t1 < 30:
                time.sleep(0.1)
            got = c.snaps[:]
            c.snaps.clear()
            for s in got:
                names.append(save(root, s, used))
                print(f"    snap {names[-1]} seq {s['meta']['seq']}", flush=True)

        def snap(label):
            rec.tc_snap(label)
            t1 = time.time()
            while rec.tc_pending() and time.time() - t1 < 20:   # a render pass runs on the next camera move
                g.key(VK_RIGHT, hold=0.1); time.sleep(0.6); g.key(VK_LEFT, hold=0.1); time.sleep(0.6)
            if rec.tc_pending():
                print(f"  {label}: NO render pass", flush=True)
            flush_snaps()

        def step(name, fn, details=()):
            for dn in details:
                rec.tc_detail(dn, DETAIL.get(dn, 2))
            try:
                fn()
                time.sleep(a.settle)
            except Exception as e:   # a failed input must not lose the boot
                print(f"  {name}: ERROR {e!r}", flush=True)
            for dn in details:
                rec.tc_detail(dn, 0)
            snap(name)
            print(f"  {name}: {rec.tc_state()['seq']} calls logged", flush=True)

        # the boot itself: the constructor, initSystem, initTerrain, passCollarInfo and the title's course happen before the menu
        for dn in ("passCollarInfo", "initTerrain", "loadNewCourseType", "resetTerrain"):
            rec.tc_detail(dn, DETAIL[dn])
        scenario.imgcmp.wait_for(g, scenario.imgcmp.load(scenario.GOLDEN / "main_menu.png"), timeout=60)
        rec.tc_detail("setType", 3)
        rec.tc_detail("elevateCorner", 3)
        scenario.to_sandbox(g)
        g.key(VK_ESC)
        time.sleep(1.5)
        step("sandbox", lambda: None)
        step("hole", lambda: scenario.build_first_hole(g), ("setType", "calcAllNormals", "layPath"))

        def golf_tools():
            g.click(*scenario.GOLF_ICON); time.sleep(1.5)
            spots = [(300, 250), (360, 300), (420, 250), (480, 300), (540, 250), (600, 300), (330, 350), (400, 350), (470, 350)]
            for k, slot in enumerate(scenario.COV_B_GOLF_TOP + scenario.COV_B_GOLF_BOTTOM):
                g.click(*slot); time.sleep(0.8)
                x, y = spots[k % len(spots)]
                g.click(x, y); time.sleep(1.0)
                g.click(x, y + 60, right=True); time.sleep(0.6)
            g.click(*scenario.GOLF_ICON); time.sleep(1.0)

        def close_palette():
            g.key(VK_ESC); time.sleep(1.0)
            g.click(*scenario.GOLF_ICON); time.sleep(1.2)

        def paths():
            g.click(*scenario.GOLF_ICON); time.sleep(1.5)
            g.click(235, 527); time.sleep(1.5)
            g.click(*scenario.COV_B_PAGE2_ITEMS[0]); time.sleep(0.8)
            pts = [(360, 330), (430, 360), (500, 330), (560, 290), (520, 250), (450, 230)]
            for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
                g.drag(x0, y0, x1, y1, steps=10, step_time=0.08)
                time.sleep(0.5)
            g.click(330, 300, right=True); time.sleep(0.8)
            for k, item in enumerate(scenario.COV_B_PAGE2_ITEMS[1:]):
                g.click(*item); time.sleep(0.8)
                x, y = [(380, 260), (560, 240)][k]
                g.drag(x, y, x + 60, y + 30)
                time.sleep(0.8)
                g.click(x - 40, y, right=True); time.sleep(0.8)

        def edit():
            g.key(VK_E); time.sleep(1.5)
            for (x, y) in ((400, 300), (460, 260)):
                g.move(x, y); time.sleep(0.8)
                for _ in range(3):
                    g.key(0xBB, char="="); time.sleep(0.5)
            g.move(340, 340); time.sleep(0.6)
            for _ in range(4):
                g.key(0xBD, char="-"); time.sleep(0.5)

        def terrain_tools():
            g.click(118, 495); time.sleep(1.5)
            g.click(235, 527); time.sleep(1.5)
            for k, tool in enumerate(scenario.COV_B_TERRAIN_TOOLS):
                g.click(*tool); time.sleep(1.0)
                x, y = [(380, 260), (450, 300), (560, 240), (330, 330)][k]
                g.move(x, y); time.sleep(0.6)
                g.drag(x, y, x + 40, y + 20)
                time.sleep(0.8)
                g.click(x - 40, y + 20, right=True); time.sleep(0.8)

        step("golf_tools_prep", close_palette)
        step("golf_tools", golf_tools, ("setType", "setWall", "calcAllNormals"))
        step("paths_prep", close_palette)
        step("paths", paths, ("layPath", "updatePath", "setType"))
        step("edit_raise", edit, ("elevateCorner", "lowerCorner", "lowerEdgeCorner", "calcAllNormals", "calcNormals"))
        step("edit_off", lambda: (g.key(VK_E), time.sleep(1.0), g.key(VK_ESC)))
        step("terrain_tools", terrain_tools, ("setType", "setWall", "elevateCorner", "lowerCorner"))
        step("terrain_tools_off", lambda: (g.key(VK_ESC), time.sleep(0.8), g.click(118, 495)))
        step("zoom1", lambda: g.key(VK_X), ("calcAllNormals",))
        step("zoom2", lambda: g.key(VK_X), ("calcAllNormals",))
        step("zoomback", lambda: (g.key(VK_Z), time.sleep(1.0), g.key(VK_Z)))
        step("rotate", lambda: g.key(VK_PGUP))
        rec.tc_dump()
        t0 = time.time()
        while not c.done and time.time() - t0 < 60:
            time.sleep(0.2)
        st = rec.tc_state()
        g.shot().save(root / "last.png")
    lines = [l for ch in c.chunks for l in ch]
    (root / "calls.txt").write_text(NL.join(lines) + NL)
    (root / "index.json").write_text(json.dumps({"snaps": names, "calls": len(lines), "counts": st.get("counts"),
                                                 "base": c.info.get("base")}, indent=1))
    print(f"{len(lines)} calls, {len(names)} snapshots -> {root}")
    return 0 if names else 1


if __name__ == "__main__":
    sys.exit(main())
