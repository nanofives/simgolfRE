"""I16: re_classify.norm_addr needs every UNCERTAINTIES.md addr cell to be one hex number or empty. For the S10 rows
(U-1850..U-1899) whose cell is not, keep the first 0x... token as the cell (else empty) and prefix the description with
the original cell text. Line endings kept.   [--write]"""
import pathlib, re, sys
R = pathlib.Path(__file__).resolve().parents[4]
P = R / "UNCERTAINTIES.md"
raw = P.read_bytes().decode("utf-8")
lines = raw.split("\r\n")
n = 0
for i, l in enumerate(lines):
    m = re.match(r"^\| (U-18[5-9]\d) \| (.*?) \| (.*?) \| (.*)$", l)
    if not m:
        continue
    uid, addr, typ, rest = m.groups()
    try:
        if addr.strip():
            int(addr.strip(), 16)
        continue
    except ValueError:
        pass
    tok = re.search(r"0x[0-9a-fA-F]+", addr)
    cell = tok.group(0) if tok else ""
    desc = "" if addr.strip() in ("-", "") else addr.strip() + ": "
    lines[i] = f"| {uid} | {cell} | {typ} | {desc}{rest}"
    n += 1
    print(uid, repr(addr), "->", repr(cell))
print(n, "rows fixed")
if "--write" in sys.argv:
    P.write_bytes("\r\n".join(lines).encode("utf-8"))
