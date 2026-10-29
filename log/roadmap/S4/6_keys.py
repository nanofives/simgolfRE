import sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(ROOT / "port" / "test")); sys.path.insert(0, str(ROOT / "re" / "tools"))
import sound_replay as SR
import sound_emu as E
keys = SR.keys_for("S4.6")
for k in keys:
    s = SR.spec_of(k)
    print(k, "|", SR.port_name(k), "|", {x: s.get(x) for x in ("abi", "ret", "fpcw", "module")}, "| nvec", len(s["vectors"]))
print(len(keys))
