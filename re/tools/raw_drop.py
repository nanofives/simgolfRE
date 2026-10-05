"""Remove functions from re/match/*_raw_NN.cpp once a readable source matches them elsewhere.

    py -3.12 re/tools/raw_drop.py 0x10004590 0x100032f0 ...
Deletes each `namespace f_<addr> { ... }` block (blocks are brace-balanced and start at column 0)."""
import pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]


def drop(addrs):
    n = 0
    for f in sorted((ROOT / "re" / "match").glob("*_raw_*.cpp")):
        t = f.read_text()
        orig = t
        for a in addrs:
            m = re.search(rf"^namespace f_{a:08x} \{{\n", t, re.M)
            if not m:
                continue
            depth, i = 0, m.end() - 2
            while True:
                c = t[i]
                depth += c == "{"
                depth -= c == "}"
                i += 1
                if depth == 0:
                    break
            t = t[:m.start()] + t[i:].lstrip("\n")
            n += 1
        if t != orig:
            f.write_text(t)
    return n


if __name__ == "__main__":
    print(drop([int(x, 16) for x in sys.argv[1:]]), "blocks removed")
