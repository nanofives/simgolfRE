"""uivec: gather the APPROVED scene of every judged element into re/tools/uivec/scenes/<area>/<id>.json (P41).

The areas wrote their scenes to log/roadmap/P41/work{,2,3,4,5}/<area>/ (not committed: the comparison images beside them
hold game art). Mariano judged them over five rounds; the approved scene of an element is the one of the latest round that
has it (for the three elements approved as "version B" in round 3, that version). A scene is numbers only, no art.

py -3.12 re/tools/uivec/collect_scenes.py      writes the scenes and scenes/manifest.tsv (area, id, round, source file)
"""
import json
import os
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
P41 = os.path.join(ROOT, "log", "roadmap", "P41")
OUT = os.path.join(HERE, "scenes")
ROUNDS = [(6, "work6"), (5, "work5"), (4, "work4"), (3, "work3"), (2, "work2"), (1, "work")]


def main():
    first = json.load(open(os.path.join(P41, "judge", "decisions_final.json"), encoding="utf-8"))
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    rows, missing = [], []
    for key in sorted(first):
        area, eid = key.split("--", 1)
        if first[key]["verdict"] == "art":
            rows.append((area, eid, "-", "kept as art"))
            continue
        if area == "parent":
            rows.append((area, eid, "-", "title.py / diff.py / load.py build it (title_geom.json, diff_geom.json, load_geom.json)"))
            continue
        src = None
        for rnd, d in ROUNDS:
            for stem in (eid, eid + "--b"):
                p = os.path.join(P41, d, area, stem + ".json")
                if os.path.exists(p):
                    src = (rnd, p)
                    break
            if src:
                break
        if not src:
            missing.append(key)
            continue
        os.makedirs(os.path.join(OUT, area), exist_ok=True)
        shutil.copyfile(src[1], os.path.join(OUT, area, eid + ".json"))
        rows.append((area, eid, str(src[0]), os.path.relpath(src[1], ROOT).replace("\\", "/")))
    with open(os.path.join(OUT, "manifest.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("area\tid\tround\tsource\n")
        for r in rows:
            f.write("\t".join(r) + "\n")
    print(len(rows), "elements;", sum(1 for r in rows if r[2] != "-"), "scenes copied")
    for rnd, _ in ROUNDS:
        print("  round", rnd, sum(1 for r in rows if r[2] == str(rnd)))
    print("missing:", missing)


if __name__ == "__main__":
    main()
