"""Matching inventory per module: game functions (not thunks, not FidDb-identified library code), matched,
partial (re/match/wip), reached by the coverage census, by size bucket. Feeds re/match/PLAN.md.
    py -3.12 re/tools/match_inventory.py            # table
    py -3.12 re/tools/match_inventory.py --json     # machine output"""
import collections, csv, glob, json, pathlib, re, sys

import pefile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from match_queue import not_game  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
MODULES = {"golf_clean.exe": "functions_ghidra.tsv", "Terrain.dll": "functions_ghidra_Terrain.dll.tsv",
           "jgld.dll": "functions_ghidra_jgld.dll.tsv", "sound.dll": "functions_ghidra_sound.dll.tsv"}
BUCKETS = ("<=64", "65-256", "257-1024", ">1024")


def rows(path):
    return list(csv.DictReader(open(path), delimiter="\t"))


def bucket(size):
    return BUCKETS[0] if size <= 64 else BUCKETS[1] if size <= 256 else BUCKETS[2] if size <= 1024 else BUCKETS[3]


def main():
    bases = {m: pefile.PE(str(ROOT / "original" / m), fast_load=True).OPTIONAL_HEADER.ImageBase for m in MODULES}
    rva = lambda m, a: a - bases[m] if a >= bases[m] else a
    tags = {"matched": collections.defaultdict(set), "wip": collections.defaultdict(set)}
    for key, pattern in (("matched", "re/match/*.cpp"), ("wip", "re/match/wip/*.cpp")):
        for f in glob.glob(str(ROOT / pattern)):
            for m, a in re.findall(r"// MATCH: (\S+) 0x([0-9a-fA-F]+)", open(f).read()):
                tags[key][m].add(rva(m, int(a, 16)))
    reach = collections.defaultdict(set)
    for f in glob.glob(str(ROOT / "re" / "coverage" / "*.tsv")):
        for r in rows(f):
            reach[r["module"]].add(rva(r["module"], int(r["addr"], 16)))
    out = {}
    for m, tsv in MODULES.items():
        lib = {rva(m, int(r["entry"], 16)) for r in rows(ROOT / "re" / "fid" / f"{m}.fid.tsv") if r["unique"] == "1"}
        fns = [(rva(m, int(r["entry"], 16)), int(r["size"]), r["name"]) for r in rows(ROOT / "re" / tsv)]
        pe = pefile.PE(str(ROOT / "original" / m), fast_load=True)
        # not game code: incremental-linking thunks, Unwind@/Catch@ EH funclets, FID_conflict library code,
        # import stubs (match_queue.not_game)
        thunks = {a for a, s, n in fns if not_game(n, s, pe.get_data(a, 2))}
        # re/match/skip.tsv entries judged not to be game code (e.g. Terrain.dll's CRT fragments); "deferred ..."
        # entries are game functions put aside and stay in the counts
        skipped = {rva(m, int(r["addr"], 16)) for r in rows(ROOT / "re" / "match" / "skip.tsv")
                   if r["module"] == m and not r["reason"].startswith("deferred")}
        game = [(a, s) for a, s, n in fns if a not in thunks and a not in lib and a not in skipped]
        b = {k: dict(game=0, bytes=0, matched=0, wip=0, reached_left=0) for k in BUCKETS}
        for a, s in game:
            d = b[bucket(s)]
            d["game"] += 1; d["bytes"] += s
            if a in tags["matched"][m]:
                d["matched"] += 1
            else:
                d["wip"] += a in tags["wip"][m]
                d["reached_left"] += a in reach[m]
        out[m] = dict(functions=len(fns), thunks=len(thunks), library=len(lib), skipped=len(skipped), buckets=b)
    if "--json" in sys.argv:
        print(json.dumps(out, indent=1)); return
    for m, d in out.items():
        g = sum(v["game"] for v in d["buckets"].values()); mt = sum(v["matched"] for v in d["buckets"].values())
        print(f"{m}: {d['functions']} functions, {d['thunks']} thunks/funclets/stubs, {d['library']} library, "
              f"{d['skipped']} in skip.tsv; game {g}, matched {mt}")
        for k, v in d["buckets"].items():
            print(f"   {k:>9}: {v['game']:5} fns {v['bytes']:7} B  matched {v['matched']:4}  wip {v['wip']:3}  "
                  f"left {v['game'] - v['matched']:5} (reached {v['reached_left']})")


if __name__ == "__main__":
    main()
