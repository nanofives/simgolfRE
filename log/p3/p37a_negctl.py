"""Phase 3.7a: every negative-control mode of the tick module against the 35 TIME-FORWARD segments: how many read RED."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from port.test import lib as L, segments as S
dll = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "port/build/p37a/mb/Release/portsim_c.dll")
lib = L.load(dll, rebuild=False)
MODES = [("tick.frame", 1), ("tick.frame", 2), ("cellAt", 1), ("tileToScreen", 1), ("screenToTile", 1),
         ("drawQueuedSprite", 1), ("drawQueuedSprite", 2), ("snapshotTables", 1), ("buildSceneSprites", 1),
         ("drawWorldObjects", 1), ("drawFlyingBall", 1), ("flushCourseInfo", 1), ("startMessage", 1), ("appendCents", 1),
         ("logTick", 1), ("tickMessage", 1), ("drawWindow", 1), ("drawThoughtBubble", 1), ("updateGolfers", 1),
         ("View_toLocal", 1), ("View::fromParent", 1), ("saveGame", 1), ("drawLabels", 1), ("drawMoneyLabels", 1),
         ("matchUpdate", 1), ("membershipReport", 1), ("canAfford", 1), ("addScore", 1), ("hireEmployee", 1),
         ("setDirtyFlagA", 1), ("setDirtyFlagB", 1)]
rows = [r for r in S.manifest_rows() if r["status"] == "accepted" and r["class"] == "TIME-FORWARD"]
segs = [S.load_segment(S.segment_dir(r)) for r in rows]
runner = S.SegmentRunner(lib)
for k, (fn, mode) in enumerate(MODES, 1):
    lib.negctl(fn, mode)
    red = [s.name for s in segs if runner.run(s).status != "PASS"]
    lib.negctl(fn, 0)
    print("[%d/%d] %s mode %d %s %d/%d %s" % (k, len(MODES), fn, mode, "RED" if red else "BLIND", len(red), len(segs), ",".join(red[:3])))
