"""S3.7: summarise a framegate census directory (<out>.tsv.d/*.json): per unsupported reason the in-game rows that carry it,
and per in-game row the hud_message region's px / beyond px.

    ~/bin/py -3.12 log/roadmap/S3/7_reasons.py <census.tsv.d> [--rows] [--grep <substring>]
"""
import collections
import json
import pathlib
import sys


def regions(s):
    out = {}
    for part in s.split(";"):
        if "=" not in part:
            continue
        k, v = part.split("=", 1)
        f = v.split("/")
        out[k] = (int(f[0]), int(f[3]) if len(f) > 3 else 0)
    return out


def main():
    d = pathlib.Path(sys.argv[1])
    show_rows = "--rows" in sys.argv
    pat = sys.argv[sys.argv.index("--grep") + 1] if "--grep" in sys.argv else None
    cnt = collections.Counter()
    msg = []
    for p in sorted(d.glob("*.json")):
        rows = json.loads(p.read_text())["rows"]
        for r in rows:
            if r.get("kind") != "ingame" or r["status"] == "NOT_RUN":
                continue
            reasons = [x for x in r.get("unsupported_before", "").split("|") if x]
            for x in reasons:
                cnt[x] += 1
            rg = regions(r.get("regions", ""))
            hm = rg.get("hud_message", (0, 0))
            if pat is None or any(pat in x for x in reasons):
                msg.append((r["scenario"], r["checkpoint"], hm[0], hm[1], [x for x in reasons if pat is None or pat in x]))
    for k, v in cnt.most_common():
        print(f"{v:5d}  {k}")
    if show_rows:
        print()
        for s, c, px, beyond, rs in msg:
            print(f"{s}\t{c}\thud_message px {px} beyond {beyond}\t{'|'.join(rs)[:300]}")


if __name__ == "__main__":
    main()
