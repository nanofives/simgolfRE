"""Compare every port/test/golden_*.json of HEAD with the mirror's, old keys mapped to new through rename_map.json.

    ~/bin/py -3.12 log/roadmap/P1/golden_keys.py [mirror root]
A key or nested key holding an old FUN_ name is renamed (whole token) before comparing; every value must be equal.
"""
import io, json, pathlib, re, subprocess, sys, tarfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
MIRROR = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "port" / "build" / "P1"
REN = json.loads((ROOT / "log/roadmap/P1/rename_map.json").read_text())
PAT = re.compile(r"\b(" + "|".join(map(re.escape, REN)) + r")\b")


def head_files():
    tar = subprocess.run(["git", "archive", "--format=tar", "HEAD", "port/test"], cwd=ROOT, capture_output=True,
                         check=True).stdout
    with tarfile.open(fileobj=io.BytesIO(tar)) as t:
        return {m.name: t.extractfile(m).read() for m in t.getmembers()
                if re.fullmatch(r"port/test/golden_[^/]*\.json", m.name)}


def remap(v, hits):
    if isinstance(v, dict):
        out = {}
        for k, x in v.items():
            k2 = PAT.sub(lambda m: REN[m.group(1)], k)
            hits[0] += k2 != k
            out[k2] = remap(x, hits)
        return out
    if isinstance(v, list):
        return [remap(x, hits) for x in v]
    return v


bad = 0
for name, data in sorted(head_files().items()):
    hits = [0]
    old = remap(json.loads(data), hits)
    new = json.loads((MIRROR / name).read_bytes())
    same = old == new
    bad += not same
    print(f"{'SAME' if same else 'DIFF'} {name} keys={len(new)} renamed_keys={hits[0]}")
print("golden files differing:", bad)
sys.exit(1 if bad else 0)
