"""S4.4: the emulated original's golden digests (layout A) of every S4.4 key with the x87 tags as Unicorn has them
(SG_S44_RAW_TAGS=1) and normalised empty (default); prints one row per key, writes log/roadmap/S4/4_tags_<mode>.json.
    ~/bin/py -3.12 log/roadmap/S4/4_tags_check.py raw|norm      then   ... compare
"""
import json, os, pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parents[3]
mode = sys.argv[1]
if mode == "compare":
    a = json.loads((ROOT / "log/roadmap/S4/4_tags_raw.json").read_text())
    b = json.loads((ROOT / "log/roadmap/S4/4_tags_norm.json").read_text())
    rows = ["key\tvectors\tdigests_equal"]
    bad = 0
    for k in sorted(set(a) | set(b)):
        eq = a.get(k) == b.get(k)
        bad += 0 if eq else 1
        rows.append(f"{k}\t{len(a.get(k) or [])}\t{'yes' if eq else 'NO'}")
    (ROOT / "log/roadmap/S4/4_tags_check.tsv").write_text("\n".join(rows) + "\n")
    print(f"keys {len(rows) - 1} differ {bad}")
    sys.exit(1 if bad else 0)
if mode == "raw":
    os.environ["SG_S44_RAW_TAGS"] = "1"
sys.path.insert(0, str(ROOT))
from port.test import test_audio_mixer as T
S, E = T.S, T.E
T.use_mirror()
lib = S.load_lib()
out = {}
with E.FixVM() as vm:
    keys = T.keys_for4()
    for n, k in enumerate(keys, 1):
        v, _, _ = S.oracle_key(vm, lib, k, ("A",), record=False)
        out[k] = v.golden
        print(f"[{n}/{len(keys)}] {k} {mode} {len(v.golden)} vectors", flush=True)
(ROOT / f"log/roadmap/S4/4_tags_{mode}.json").write_text(json.dumps(out, indent=0))
