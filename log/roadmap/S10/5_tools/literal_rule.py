"""S10.5 / U-1886: every golf_clean.exe MATCH line of re/match/*.cpp and re/match/wip/*.cpp scored with the new mask rule
and with the old one (literal_values patched to the empty set), one compile per file. No evidence written.

    ~/bin/py -3.12 log/roadmap/S10/5_tools/literal_rule.py [--out log/roadmap/S10/5_literal_rule.tsv]
"""
import argparse
import contextlib
import io
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402

_cache, _compile, _literals = {}, match.compile_obj, match.literal_values


def cached_compile(*a, **k):
    key = (str(a), str(sorted(k.items())))
    if key not in _cache:
        _cache[key] = _compile(*a, **k)
    return _cache[key]


def run(f):
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            return {(r[1], r[2]): r[4] for r in match.compare(f, "", False, quiet=True) if r[0] == "golf_clean.exe"}
    except SystemExit as e:
        return f"err: {str(e).splitlines()[-1][:80]}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="log/roadmap/S10/5_literal_rule.tsv")
    a = ap.parse_args()
    match.compile_obj = cached_compile
    files = [f for g in ("*.cpp", "wip/*.cpp") for f in sorted((ROOT / "re" / "match").glob(g))
             if "MATCH: golf_clean.exe" in f.read_text(errors="replace")]
    rows, moved = [], 0
    for k, f in enumerate(files, 1):
        _cache.clear()
        match.literal_values = _literals
        new = run(f)
        match.literal_values = lambda *_: set()
        old = run(f)
        rel = f.relative_to(ROOT).as_posix()
        if isinstance(new, str) or isinstance(old, str):
            rows.append([rel, "", "", str(old), str(new)])
        else:
            for (addr, sym), acc in new.items():
                o = old.get((addr, sym))
                rows.append([rel, f"0x{addr:08x}", sym, f"{100 * o:.1f}", f"{100 * acc:.1f}"])
                moved += o != acc
        print(f"[{k}/{len(files)}] {f.name} {'err' if isinstance(new, str) else 'ok'} moved={moved}", flush=True)
    (ROOT / a.out).write_text("file\taddr\tsymbol\told\tnew\n" + "".join("\t".join(r) + "\n" for r in rows))
    print(f"{len(rows)} rows, {moved} moved -> {a.out}")


if __name__ == "__main__":
    main()
