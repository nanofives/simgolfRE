"""S3.5 recorder: the ORIGINAL's root surface at the return of renderSpriteQueue in each checkpoint's frame (scene_rec.js), for
the sprite-pixel gate (log/roadmap/S3/5_sprite_gate.py --queue). A pinned scenario replayed by re/tools/scenario_refs.py under
the virtual clock with scene_rec.js appended to its agent (the pattern of re/tools/terrain_calls.py run_pinned); at every
checkpoint the main thread waits in the clock stub, outside every drawing call, and the last copy (and its renderSpriteQueue
call ordinal, counted from process start) is queued; everything is fetched at the scenario's last reply.

    SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 log/roadmap/S3/5_tools/scene_rec.py <scenario>... --instance s3_5

Output port/corpus/scene/<scenario>/<checkpoint>.z (zlib, RGB555 bits as the DIB holds them) + index.json; the run itself
(checkpoints compared with the pinned reference) is log/c3/alt/refs/<scenario>/s35_scene.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import sys
import time
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
OUT = ROOT / "port" / "corpus" / "scene"
JS = pathlib.Path(__file__).with_name("scene_rec.js")


def run(scenario: str, instance: str, label: str = "s35_scene") -> int:
    if "game" in sys.modules:
        raise RuntimeError("game.py was imported before scenario_refs: the game would boot in the wrong install dir")
    import scenario_refs as R
    got, done = [], [0]
    js = JS.read_text()
    orig_setup = getattr(R, '_s35_orig_setup', None) or R.setup
    R._s35_orig_setup = orig_setup

    def install(S):
        Base = S.Run

        class SceneRun(Base):
            def agent_source(self):
                p = super().agent_source()
                p.write_text(p.read_text() + chr(10) + js)
                return p

            def _on_message(self, m, data):
                pl = m.get("payload") if m.get("type") == "send" else None
                if isinstance(pl, dict) and "sr" in pl:
                    if pl["sr"] == "snap":
                        got.append((pl["meta"], bytes(data) if data else None))
                    elif pl["sr"] == "done":
                        done[0] = 1
                    return
                return super()._on_message(m, data)

            def _rec(self):
                return self.g.scripts[-1].exports_sync

            def take_dump(self, sc, name):
                if not self._rec().sr_snap_now(name):
                    print(f"  checkpoint {name}: no renderSpriteQueue copy yet", flush=True)
                return super().take_dump(sc, name)

            def post(self, sc, **kw):
                if "actions" in kw and not kw.get("next"):
                    self._rec().sr_take()
                    t0 = time.time()
                    while not done[0] and time.time() - t0 < 120:
                        time.sleep(0.05)
                return super().post(sc, **kw)

        S.Run = SceneRun

    def setup(scen, inst):
        S = orig_setup(scen, inst)
        install(S)
        return S

    R.setup = setup
    a = argparse.Namespace(instance=instance, scenario=scenario, label=label, schedule=None, record=False, on=None, env=None,
                           stop_after=0, clock_log=0, msg_log=0, trace=None, watch=None)
    rc = R.cmd_run(a)
    d = OUT / scenario
    d.mkdir(parents=True, exist_ok=True)
    index = {"scenario": scenario, "rc": rc, "run": str(R.REFS / scenario / label), "checkpoints": {}}
    for meta, data in got:
        index["checkpoints"][meta["label"]] = meta
        if data:
            (d / f"{meta['label']}.z").write_bytes(zlib.compress(data, 6))
    (d / "index.json").write_text(json.dumps(index, indent=1))
    n = sum(1 for m, dd in got if dd)
    print(f"{scenario}: {n} copies of {len(got)} checkpoints -> {d} (scenario_refs rc {rc})", flush=True)
    return 0 if rc == 0 and n else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scenarios", nargs="+")
    ap.add_argument("--instance", default="s3_5")
    a = ap.parse_args()
    bad = 0
    for s in a.scenarios:
        bad += run(s, a.instance) != 0
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
