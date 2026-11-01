"""S4.12: block until a file contains a regex (or a timeout), then print the matching lines.
Usage: ~/bin/py -3.12 log/roadmap/merges/I14/waitfor.py <file> <regex> [timeout s, default 540]"""
import pathlib
import re
import sys
import time

p, rx = pathlib.Path(sys.argv[1]), re.compile(sys.argv[2])
end = time.time() + float(sys.argv[3]) if len(sys.argv) > 3 else time.time() + 540
while time.time() < end:
    t = p.read_text(errors="replace") if p.exists() else ""
    hits = [l for l in t.splitlines() if rx.search(l)]
    if hits:
        print("\n".join(hits))
        sys.exit(0)
    time.sleep(10)
print("timeout")
sys.exit(1)
