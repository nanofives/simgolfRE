"""S4.3: find each S4.3 row's detour in its hook source (SG_HOOK line) and print its line range."""
import csv, pathlib, re
ROOT = pathlib.Path(__file__).resolve().parents[3]
rows = [r for r in csv.DictReader(open(ROOT / "re/roadmap/S4_functions.tsv"), delimiter="\t") if r["step"] == "S4.3"]
def find(rva, src):
    t = (ROOT / "shim/src/re" / src).read_text().split("\n")
    for ln in t:
        m = re.match(r'SG_HOOK\("sound\.dll",\s*0x0*([0-9a-fA-F]+),\s*\w+,\s*(\w+)', ln)
        if m and int(m.group(1), 16) == rva:
            det = m.group(2)
            for i, l in enumerate(t):
                if re.match(rf"^\S.*\b{det}\(", l) and not l.startswith("typedef") and not l.rstrip().endswith(";"):
                    j = i
                    while t[j] != "}":
                        j += 1
                    k = i
                    while k > 0 and (t[k - 1].startswith("//") or t[k - 1].startswith("typedef") or re.match(r"^\w+_t \w+_orig;", t[k - 1])):
                        k -= 1
                    return det, k + 1, i + 1, j + 1
    return None
if __name__ == "__main__":
    for r in rows:
        f = find(int(r["rva"], 16), r["hook_source"])
        print(r["rva"], r["name"], r["hook_source"], f, (f[3] - f[2]) if f else None)
