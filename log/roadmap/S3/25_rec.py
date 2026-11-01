"""S3.25 recorder driver: one pinned scenario replayed by re/tools/scenario_refs.py (OFF arm, hooks off, C3FT_AUDIO default) with
log/roadmap/S3/25_rec.js appended to its agent (the pattern of log/roadmap/S3/5_tools/scene_rec.py).

    SIMGOLF_ALLOW_MULTI=1 ~/bin/py -3.12 log/roadmap/S3/25_rec.py <preset> <scenario> --instance s3_w8

Presets (log/roadmap/S3/22_causes.tsv groups):
  inject  group 7: the 2D drawing calls after renderSpriteQueue with their arguments and a box watch on the 4x4 marker boxes
  camera  group 5: drawMapLine / Terrain::drawLine / the line helpers and mainLoop's worldToScreen calls with their out values
  queue   groups 3 / 4: the sprite-queue arrays at renderSpriteQueue's entry, the root at its return, every enqueueSprite /
          addSprite call (caller and arguments) and drawQueuedSprite's drawing calls

Output port/corpus/w8rec/<scenario>/<preset>/: <checkpoint>.rows.json.gz (meta, rows, box events), <checkpoint>.q.bin (the
qranges concatenated), <checkpoint>.rootq.z (zlib RGB555 root at renderSpriteQueue's return), index.json (hook table, ranges,
the run's SAME check against det_b). The run itself is log/c3/alt/refs/<scenario>/s325_<preset>.
"""
from __future__ import annotations

import argparse
import gzip
import json
import pathlib
import sys
import time
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "re" / "tools"))
OUT = ROOT / "port" / "corpus" / "w8rec"
JS = pathlib.Path(__file__).with_name("25_rec.js")

# exe drawing functions (hooks.csv names); 8 stack dwords each (extra dwords are the caller's, read only)
TEXT = "s,r"
PRIM = [
    (0x004049a0, "drawLineOnSurface", "r"), (0x004049d0, "drawText4049d0", TEXT), (0x00404a20, "drawTextScaled", TEXT),
    (0x00404ad0, "drawTextSurface", TEXT), (0x00404b70, "drawCenteredText", TEXT), (0x00404bc0, "drawCenteredText2", TEXT),
    (0x00406220, "drawMinimapSegment", "r"), (0x00407c60, "drawCircle", "r"), (0x0040bf00, "setLinePoint", "r"),
    (0x0040bf20, "lineTo", "r"), (0x0040c7a0, "drawLabels", "r"), (0x0040c910, "drawMoneyLabels", "r"),
    (0x0040ca10, "blitPanel", "r"), (0x0040cc00, "drawPanelS2", "r"), (0x0040cdd0, "drawWindowFrame", "r"),
    (0x0040cef0, "drawFrameR3", "r"), (0x0040d0b0, "drawFrameS2", "r"), (0x0040d320, "drawWindow", TEXT),
    (0x00421b60, "drawScaledSprite421b60", "r"), (0x00421b90, "drawScaledSprite421b90", "r"),
    (0x00432200, "drawAmenityRow", "r"), (0x00432620, "drawPanelBackdrop", "r"),
    (0x00433e50, "drawTerrainToolbar", "r"), (0x00434350, "drawBuildingPanel", "r"), (0x00434cf0, "drawAmenitiesPanel", "r"),
    (0x00435760, "drawEmployeeViewPanel", "r"), (0x004362f0, "drawGolferActionPanel", "r"),
    (0x00436e50, "drawEmployeeDetailPanel", "r"), (0x004493d0, "drawMapLine", "r"), (0x0045b0d0, "drawMessageBuffer", TEXT),
    (0x00461110, "drawMatchScoreboard", "r"), (0x00462be0, "drawQueuedSprite", "r"), (0x0046e710, "drawRectScaled", "r"),
    (0x00473bf0, "Surface_blit", "r"), (0x00473c60, "Surface_blitTakeDirty", "r"), (0x00473cb0, "Surface_drawScaled", "r"),
    (0x00473df0, "Surface_drawRegion", "r"), (0x00473e60, "Image_draw", "r"), (0x00473f60, "Surface_stretchBlit", "r"),
    (0x00473fc0, "Surface_blitMasked", "r"), (0x00474030, "Surface_drawScaled2", "r"), (0x004740f0, "Surface_blit2", "r"),
    (0x004741b0, "Sprite_draw", "r"), (0x00474260, "Sprite_drawScaled", "r"), (0x00474380, "Surface_draw", "r"),
    (0x00474440, "Surface_drawSprite", "r"), (0x00475b00, "Widget_fillRectR", "r"), (0x00475b20, "Surface_box", "r"),
    (0x00475c60, "Surface_blitThunk", "r"), (0x00475c90, "Surface_fillRegion", "r"), (0x00475d00, "Surface_blit3", "r"),
    (0x00475da0, "Surface_fill", "r"), (0x00475e10, "blitLeft", "r"), (0x00475fb0, "blitMid", "r"),
    (0x00476140, "blitRight", "r"), (0x004762d0, "setDrawTarget", "r"), (0x00476310, "Widget_setQuad", "r"),
    (0x00476650, "Widget_draw", TEXT), (0x004766a0, "Widget_draw2", TEXT), (0x00476700, "drawStringAt", TEXT),
    (0x00476750, "drawStringAt2", TEXT), (0x004767a0, "drawTextRun", TEXT), (0x00477280, "drawMarkupRun", TEXT),
    (0x004775b0, "drawRichText", TEXT), (0x00477c30, "drawText", TEXT), (0x00477cd0, "drawCentered", TEXT),
    (0x00477da0, "drawCentered2", TEXT), (0x00477e60, "drawBox", "r"), (0x00477eb0, "drawCentered3", TEXT),
    (0x00477fc0, "drawStringCentered", TEXT), (0x00478080, "drawRight", TEXT), (0x00478140, "drawStringRightAt", TEXT),
    (0x004781f0, "drawRight2", TEXT), (0x004782d0, "drawTextLines", TEXT), (0x00478430, "drawTextLinesAt", TEXT),
    (0x00478530, "drawCenteredTextLines", TEXT), (0x004787a0, "drawTextBlock", TEXT), (0x00478b30, "Widget_clearArea", "r"),
    (0x00478b50, "Widget_fillArea", "r"), (0x00478b80, "Widget_fillRect", "r"), (0x00478bb0, "Widget_drawHLine", "r"),
    (0x00478be0, "Widget_drawVLine", "r"), (0x00478c10, "Widget_drawHLineDepth", "r"), (0x00478c70, "Widget_drawVLineDepth", "r"),
    (0x00478ea0, "Widget_writePixel", "r"), (0x004791b0, "drawNinePatch", "r"), (0x00479560, "Widget_drawBevelBox", "r"),
    (0x004795d0, "Widget_drawBevelBoxR", "r"), (0x00479610, "Widget_drawBox", "r"), (0x00479670, "Widget_drawBoxR", "r"),
    (0x0047cdb0, "drawTooltip", "r"), (0x0047d130, "drawCursor", "r"), (0x00480b00, "fillWindowRect", "r"),
    (0x00480c20, "Window::refreshRect", "r"), (0x00480c80, "Window::refreshFull", "r"), (0x004810d0, "drawPanel4810d0", "r"),
    (0x00482990, "Surface::blitRLE", "r"), (0x00485d40, "Image::dottedHLine", "r"), (0x00485e80, "Image::dottedVLine", "r"),
    (0x004932d0, "fillConvexPoly16", "r"), (0x00493520, "fillConvexPoly", "r"), (0x00493630, "fill16", "r"),
    (0x00494d30, "dottedHLine16", "r"), (0x00494f00, "dottedVLine16", "r"), (0x004628d0, "addSprite", "r"),
    (0x00462a30, "enqueueSprite", "r"), (0x00433190, "showInfoCursorPanel", "r"),
]
TDRAWLINE = (0x48a0, "Terrain::drawLine", "r")        # ?drawLine@Terrain@@QAEXHHHHHHH@Z body (thunk at RVA 0x11b3)
ML = "ra:40f5c0-421615"                                # calls returning into mainLoop's body
DQS = "ra:462be0-463100"                               # calls returning into drawQueuedSprite
PRESETS = {
    "inject": {"hooks": PRIM, "thooks": [TDRAWLINE], "postOnly": True, "rootq": True},
    "camera": {"hooks": [(0x004493d0, "drawMapLine", "r"), (0x0040bf00, "setLinePoint", "r"), (0x0040bf20, "lineTo", "r"),
                         (0x00407c60, "drawCircle", "r"), (0x00406220, "drawMinimapSegment", "r"),
                         (0x0042fb90, "worldToScreen", "o,r," + ML), (0x0042f940, "tileToScreen", "o,r," + ML),
                         (0x00434cf0, "drawAmenitiesPanel", "r"), (0x00433190, "showInfoCursorPanel", "r"),
                         (0x00404bc0, "drawCenteredText2", TEXT), (0x0047cdb0, "drawTooltip", "r")],
               "thooks": [TDRAWLINE], "postOnly": False, "rootq": True},
    "queue": {"hooks": [(0x00462a30, "enqueueSprite", "r"), (0x004628d0, "addSprite", "r,w"), (0x00462be0, "drawQueuedSprite", "r"),
                        (0x004740f0, "Surface_blit2", "r,w"), (0x004741b0, "Sprite_draw", "r,w"),
                        (0x00474260, "Sprite_drawScaled", "r,w"), (0x00474440, "Surface_drawSprite", "r,w"),
                        (0x00473e60, "Image_draw", "r,w"), (0x00473cb0, "Surface_drawScaled", "r,w"),
                        (0x00474030, "Surface_drawScaled2", "r,w"), (0x004762d0, "setDrawTarget", "r," + DQS),
                        (0x00404b70, "drawCenteredText", TEXT + ",w," + DQS), (0x00478b80, "Widget_fillRect", "r,w," + DQS),
                        (0x00462020, "composeGolferSprite", "r"), (0x00463180, "buildSceneSprites", "r,w"),
                        (0x004012d0, "drawRecords", "r,w"), (0x00430360, "drawWorldObjects", "r,w")],
              "thooks": [], "postOnly": False, "rootq": True,
              # the sprite-queue arrays 0x0082415c..0x0083820b (g_spriteSortKey .. g_key838208), g_sheetFirstCell, g_sheetCols,
              # g_images (190 x 0x58)
              "qranges": [[0x0082415C, 0x0083820C - 0x0082415C], [0x005A9370, 0x938], [0x0053F3E8, 0x938], [0x0081CA10, 0x4150]]},
}
# the 4x4 marker boxes of log/roadmap/S3/22_causes.tsv group 7 (x0, y0, x1, y1 inclusive)
BOXES = {"cov_b_inject": [[142, 89, 145, 92], [568, 355, 571, 358]],
         "cov_b_inject2": [[142, 89, 145, 92], [576, 365, 579, 368], [272, 61, 275, 64]],
         # groups 3 / 4 (queue preset, watched from renderSpriteQueue's entry): the ellipse, the fountain / ripple / flag boxes
         "golfer_stories": [[60, 70, 102, 90]],
         "cov_b_holes18": [[5, 131, 100, 185]],
         "cov_k3_camera2": [[65, 413, 97, 451]]}


FOCUS = {"cov_b_inject": {"zoom_box", "forced_events", "end"}, "cov_b_inject2": {"zoom_box", "forced_events", "end"},
         "golfer_stories": {"season_end"}, "cov_b_holes18": {"holes"}, "cov_k3_camera2": {"y_inj4"}}


def run(preset: str, scenario: str, instance: str, suffix: str = "") -> int:
    if "game" in sys.modules:
        raise RuntimeError("game.py was imported before scenario_refs: the game would boot in the wrong install dir")
    import scenario_refs as R
    P = PRESETS[preset]
    cfg = {"hooks": [[va, 8, n, f] for va, n, f in P["hooks"]], "thooks": [[rva, 7, n, f] for rva, n, f in P["thooks"]],
           "boxes": BOXES.get(scenario, []), "qranges": P.get("qranges", []), "watchFrom": "frame" if preset == "queue" else "",
           "rootq": P["rootq"], "postOnly": P["postOnly"], "maxRows": 200000}
    if cfg["boxes"]:
        # the box watch costs a read of every box at every watched return: only in the frames up to 2.5 virtual s before the focus
        # checkpoints (their T in det_b's log, printed as T() - t0)
        import re as _re
        ts = [int(m.group(2)) for m in _re.finditer(r"checkpoint (\S+) T=(\d+)", (R.REFS / scenario / "det_b.log").read_text())
              if m.group(1) in FOCUS.get(scenario, ())]
        cfg["watchT"] = [[t - 2500, t + 200] for t in ts]
    got, done = [], [0]
    js = "globalThis.R25 = " + json.dumps(cfg) + ";" + chr(10) + JS.read_text()
    label = f"s325_{preset}{suffix}"

    def install(S):
        Base = S.Run

        class RecRun(Base):
            def agent_source(self):
                p = super().agent_source()
                p.write_text(p.read_text() + chr(10) + js)
                return p

            def _on_message(self, m, data):
                pl = m.get("payload") if m.get("type") == "send" else None
                if isinstance(pl, dict) and "r25" in pl:
                    if pl["r25"] == "snap":
                        got.append((pl["meta"], bytes(data) if data else b""))
                    elif pl["r25"] == "done":
                        done[0] = 1
                    return
                return super()._on_message(m, data)

            def _rec(self):
                return self.g.scripts[-1].exports_sync

            def take_dump(self, sc, name):
                self._rec().r25_snap_now(name)
                return super().take_dump(sc, name)

            def post(self, sc, **kw):
                if "actions" in kw and not kw.get("next"):
                    self._rec().r25_take()
                    t0 = time.time()
                    while not done[0] and time.time() - t0 < 300:
                        time.sleep(0.05)
                return super().post(sc, **kw)

        S.Run = RecRun

    orig_setup = R.setup

    def setup(scen, inst):
        S = orig_setup(scen, inst)
        install(S)
        return S

    R.setup = setup
    a = argparse.Namespace(instance=instance, scenario=scenario, label=label, schedule=None, record=False, on=None, env=None,
                           stop_after=0, clock_log=0, msg_log=0, trace=None, watch=None)
    rc = R.cmd_run(a)
    d = OUT / scenario / (preset + suffix)
    d.mkdir(parents=True, exist_ok=True)
    run_dir, ref = R.REFS / scenario / label, R.REFS / scenario / "det_b"
    ok, why = R.run_ok(run_dir)
    verdict = R.summarize(R.compare(run_dir, ref)) if ok else f"not scorable: {why}"
    index = {"scenario": scenario, "preset": preset, "rc": rc, "run": str(run_dir), "vs_det_b": verdict, "cfg": cfg,
             "hook_ids": [h[2] for h in cfg["hooks"]] + [h[2] for h in cfg["thooks"]], "checkpoints": {}}
    for meta, data in got:
        q, rq = meta.get("qsize", 0), meta.get("rqsize", 0)
        lab = meta["label"]
        if q:
            (d / f"{lab}.q.bin").write_bytes(data[:q])
        if rq:
            (d / f"{lab}.rootq.z").write_bytes(zlib.compress(data[q:q + rq], 6))
        rows, boxev = meta.pop("rows"), meta.pop("boxEv")
        with gzip.open(d / f"{lab}.rows.json.gz", "wt") as f:
            json.dump({"meta": meta, "rows": rows, "boxEv": boxev}, f)
        index["checkpoints"][lab] = dict(meta, nrows=len(rows), nbox=len(boxev))
    (d / "index.json").write_text(json.dumps(index, indent=1))
    print(f"{scenario}/{preset}: {len(got)} checkpoints -> {d}; run rc {rc}; vs det_b: {verdict}", flush=True)
    return 0 if rc == 0 and got and verdict.startswith("SAME") else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("preset", choices=sorted(PRESETS))
    ap.add_argument("scenario")
    ap.add_argument("--instance", default="s3_w8")
    ap.add_argument("--suffix", default="", help="appended to the run label and the output folder (a second recording)")
    a = ap.parse_args()
    return run(a.preset, a.scenario, a.instance, a.suffix)


if __name__ == "__main__":
    sys.exit(main())
