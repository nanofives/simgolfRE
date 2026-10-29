"""S4.10g, U-1421: run the game on instances\\s4_u1421 with the audio device live and log the x87 control word at
sound.dll's codec entries (10g_probe.js). Usage (from SimGolf\\):
  SIMGOLF_ALLOW_MULTI=1 SIMGOLF_GAME_DIR=instances/s4_u1421 ~/bin/py -3.12 log/roadmap/S4/10g_run.py <scenario> <out.json>
scenarios: menu (menu + sandbox + 60 s), season (sandbox, tee+green, hole open, 120 s at TIMEWARP 8)."""
import json, os, pathlib, sys, time

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "re" / "tools"))
os.environ.setdefault("SIMGOLF_ALLOW_MULTI", "1")
os.environ["SIMGOLF_KEEP_SOUND"] = os.environ.get("SIMGOLF_KEEP_SOUND", "0")
os.environ.setdefault("SIMGOLF_GAME_DIR", str(ROOT / "instances" / "s4_u1421"))
assert "s4_u1421" in os.environ["SIMGOLF_GAME_DIR"], "never boot in original\\"
import game as G          # noqa: E402
import scenario as S      # noqa: E402

scn, out = sys.argv[1], sys.argv[2]
env = {"SIMGOLF_SKIP_INTRO": "0" if scn == "intro" else "1"}
if scn == "season":
    env["SIMGOLF_TIMEWARP"] = "8"
pre = G.ctypes.windll.winmm.waveOutGetNumDevs()
g = G.Game(extra_js=[str(pathlib.Path(__file__).with_name("10g_probe.js"))], env=env)
rows = []
g.on_message = lambda m, d: rows.append(m.get("payload", m))
try:
    g.start()
    pid = g.pid
    g.wait_window()
    time.sleep(6)
    if scn == "intro":
        time.sleep(40)
    else:
        S.to_sandbox(g)
        if scn == "season":
            S.build_first_hole(g)
            S.run_season(g, 120)
        else:
            S.run_season(g, 60)
    try:
        counts = g.scripts[-1].exports_sync.counts()
    except Exception as e:
        counts = {"error": str(e)}
finally:
    g.kill()
    r = G.purge_frida_temp(0)
json.dump({"scenario": scn, "pid": pid, "waveOutGetNumDevs": pre, "counts": counts, "rows": rows,
           "messages": [str(m)[:300] for m in g.messages if not isinstance(m, dict) or "probe" not in m][:20]},
          open(out, "w"), indent=1)
print(scn, "pid", pid, "devs", pre, "counts", counts, "rows", len(rows))
