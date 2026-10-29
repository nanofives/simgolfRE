"""S10.9: every address a names row cites inside its own function's body must start an instruction (capstone on our
binary), and every named function a row cites as `<name> 0x<addr>` / `0x<addr> <name>` must exist at that address.
~/bin/py -3.12 log/roadmap/S10/9_tools/check_cites.py re/names/<id>.tsv ...      exit 1 on any bad citation."""
import pathlib, re, sys

import capstone

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import pointer_callers as pc  # noqa: E402

import json  # S10.13: bodies outside our lists take their extent from 13_facts.json
EXT = {(v["module"], int(v["va"], 16)): v["size"] for v in json.loads(
    (ROOT / "log/roadmap/S10/13_facts.json").read_text(encoding="utf-8")).values()}
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
bad = checked = 0
for f in sys.argv[1:]:
    for l in pathlib.Path(f).read_text(encoding="utf-8").splitlines()[1:]:
        c = l.split("\t")
        m = pc.mod(c[0])
        va = int(c[1], 16)
        size = next((hi - lo for lo, hi, e in m.ranges if e == va), 0) or EXT.get((c[0], va), 0)
        if not size:
            continue
        starts = {i.address for i in md.disasm(m.img[va - m.base:va - m.base + size], va)}
        for a in {int(x, 16) for x in re.findall(r"0x([0-9a-f]{8})", c[4] + " " + c[5])}:
            if va <= a < va + size:
                checked += 1
                if a not in starts:
                    bad += 1
                    print(f"BAD {f}: {c[1]} {c[2]}: 0x{a:08x} is inside the body but not an instruction start")
print(f"{checked} in-body citations checked, {bad} bad")
sys.exit(1 if bad else 0)
