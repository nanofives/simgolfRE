"""S10.12: score hand-written source variants of one function (no file is written except a temp copy).

    ~/bin/py -3.12 log/roadmap/S10/12_tools/variants.py <src> <addr> <variants.json>

variants.json: [{"name": "...", "old": "text in the function", "new": "replacement", "all": false}, ...]
`old` must occur in the function's span (the span the permuter finds for the MATCH line). Output: score per variant.
"""
import json, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match_permute as mp  # noqa: E402


def main():
    src, addr, vj = pathlib.Path(sys.argv[1]), int(sys.argv[2], 16), pathlib.Path(sys.argv[3])
    text = src.read_text()
    s, e = mp.function_span(text, addr)
    body = text[s:e]
    base = mp.score(text, src, addr)
    print(f"base {mp.fmt(base)}", flush=True)
    for v in json.loads(vj.read_text()):
        if v["old"] not in body:
            print(f"{v['name']}: old text not found", flush=True)
            continue
        nb = body.replace(v["old"], v["new"]) if v.get("all") else body.replace(v["old"], v["new"], 1)
        sc = mp.score(text[:s] + nb + text[e:], src, addr)
        print(f"{v['name']}: {mp.fmt(sc) if sc else 'no compile'}", flush=True)


if __name__ == "__main__":
    main()
