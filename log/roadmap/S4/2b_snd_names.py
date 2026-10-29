"""S4.2b: the naming rule of log/roadmap/merges/S4-3.md: every sound.dll function is registered as `snd.<hooks.csv name>`.
Rewrites the FuncDef tables of the ten audio modules (a name is prefixed when it is the name of a row of
re/roadmap/S4_functions.tsv; harness entries `audio_core.*` and the CRT rows `audio_core.crt.*` keep theirs) and the core's
rva -> name lookup. Idempotent.

    ~/bin/py -3.12 log/roadmap/S4/2b_snd_names.py [--check]
"""
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SIM = ROOT / "port" / "src" / "sim"
MODULES = ("core", "mixer", "chan", "ds", "dev", "codec", "seq", "sound", "wave", "soundfile")
with open(ROOT / "re" / "roadmap" / "S4_functions.tsv", newline="") as f:
    NAMES = {r["name"] for r in csv.DictReader(f, delimiter="\t")}

out = {}
total = 0
for m in MODULES:
    p = SIM / f"audio_{m}.cpp"
    t = p.read_text(encoding="utf-8")
    i = t.index("const FuncDef kFuncs[] = {")
    j = t.index("\n};", i)
    tab = t[i:j]
    n0 = 0

    def lit(mo):
        global n0
        name = mo.group(2)
        if name in NAMES:
            return f'{mo.group(1)}"snd.{name}",'
        return mo.group(0)
    tab2, k1 = re.subn(r'(\{|F\()"([^"]+)",', lambda mo: lit(mo), tab)
    # S4.4's macro: F(name, ...) -> {#name, ...}
    tab2 = tab2.replace('#define F(name, abi, ret, n) {#name, abi, ret, n, 0, a_##name, &g_negctl_##name},',
                        '#define F(name, abi, ret, n) {"snd." #name, abi, ret, n, 0, a_##name, &g_negctl_##name},')
    t2 = t[:i] + tab2 + t[j:]
    if m == "core":
        old = "    const int k = registry_find(kSoundFunctions[i].name);\n"
        if old in t2:
            t2 = t2.replace(old, "    // every sound.dll function is registered as \"snd.<hooks.csv name>\" (log/roadmap/merges/S4-3.md)\n"
                                 "    const int k = registry_find((std::string(\"snd.\") + kSoundFunctions[i].name).c_str());\n")
    changed = sum(1 for a, b in zip(t.splitlines(), t2.splitlines()) if a != b)
    total += changed
    out[p] = t2
    print(f"audio_{m}: {changed} lines changed")
if "--check" not in sys.argv:
    for p, t2 in out.items():
        p.write_text(t2, encoding="utf-8")
print(f"total {total}")
