"""S11.2 negative control for 2_compare.py: plant 4 differences in a copy of the names files, expect exactly 4 more rows.

usage: 2_control.py <names_dir> <export_dir> <scratch_dir>
Plants: (1) exe_0.tsv placeRecord 0x00401040 renamed placeRecordX (fn_name), (2) exe_0_globals.tsv g_placed_objects
0x0058bcb8 -> g_placed_objectsX (global_name), (3) a withdrawn row for 0x00401040 removed from naming by dropping its
function row is not needed: a withdrawn row for an address no file names but the master names (jgld 0x10001000-range
first USER_DEFINED function of the export) (withdrawn_named), (4) a function row at an address with no function
(0x00401041, fn_missing).
"""
import csv
import os
import shutil
import subprocess
import sys

names, export, scratch = sys.argv[1:4]
d = os.path.join(scratch, "names")
shutil.rmtree(d, ignore_errors=True)
shutil.copytree(names, d)


def edit(fn, old, new):
    p = os.path.join(d, fn)
    t = open(p, encoding="utf-8", newline="").read()
    assert t.count(old) == 1, (fn, old)
    open(p, "w", encoding="utf-8", newline="").write(t.replace(old, new))


edit("exe_0.tsv", "\t0x00401040\tplaceRecord\t", "\t0x00401040\tplaceRecordX\t")
edit("exe_0_globals.tsv", "\t0x0058bcb8\tg_placed_objects\t", "\t0x0058bcb8\tg_placed_objectsX\t")
named = set()
for f in os.listdir(d):
    if f.endswith(".tsv") and not f.endswith("_globals.tsv") and f != "withdrawn.tsv":
        for ln in open(os.path.join(d, f), encoding="utf-8"):
            c = ln.split("\t")
            if len(c) > 2 and c[0] == "jgld.dll":
                named.add(int(c[1], 16))
target = None
with open(os.path.join(export, "jgld.dll.tsv"), encoding="utf-8") as h:
    for row in csv.reader(h, delimiter="\t", quoting=csv.QUOTE_NONE):
        if row[0] == "FN" and row[3] != "DEFAULT" and int(row[1], 16) not in named:
            target = row
            break
assert target, "no unbacked non-default jgld function"
with open(os.path.join(d, "withdrawn.tsv"), "a", encoding="utf-8", newline="") as h:
    h.write("jgld.dll\t0x%s\tS11.2 control\n" % target[1])
with open(os.path.join(d, "withdrawn.tsv"), "a", encoding="utf-8", newline="") as h:
    # (5) a withdrawn label still in the master -> withdrawn_label_present; (6) a withdrawn label a globals row names
    # again (g_walkers, exe_0_globals.tsv) -> no row
    h.write("golf_clean.exe\t0x0041088f\tlabel mainLoop::override::jmp_0041088f::switch withdrawn: S11.2 control\n")
    h.write("golf_clean.exe\t0x00585850\tlabel g_walkers withdrawn: S11.2 control\n")
with open(os.path.join(d, "exe_0.tsv"), "a", encoding="utf-8", newline="") as h:
    h.write("golf_clean.exe\t0x00401041\tcontrolNoFunction\tsim\tS11.2 control\tS11.2 control\n")
out = os.path.join(scratch, "control_diff.tsv")
r = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), "2_compare.py"), d, export, out],
                   capture_output=True, text=True)
print(r.stdout)
cats = [ln.split("\t")[6] for ln in open(out, encoding="utf-8").read().splitlines()[1:]]
want = {"fn_name": 5, "global_name": 1, "withdrawn_named": 1, "fn_missing": 1, "withdrawn_label_present": 1}
got = {k: cats.count(k) for k in set(cats)}
print("withdrawn target", target[1], target[2])
print("expected", want, "got", got, "RED" if got == want else "BLIND/WRONG")
