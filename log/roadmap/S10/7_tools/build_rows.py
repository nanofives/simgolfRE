"""S10.7: names rows for identical library procedures with no names row.
~/bin/py -3.12 log/roadmap/S10/7_tools/build_rows.py   -> re/names/smsg_<exe|jgld|sound|terrain>_lib.tsv + 7_ledger_<key>.tsv
Inputs: 7_score_<key>.tsv (100% rows), 7_gaps_<key>.tsv (names_file column), 7_cand_<key>.tsv (PDB undecorated name),
re/functions_ghidra*.tsv (the address must start a function of ours), every re/names/*.tsv (names stay unique per module)."""
import glob, pathlib, re

ROOT = pathlib.Path(__file__).resolve().parents[4]
MODS = {"golf": ("golf_clean.exe", "smsg_exe_lib", "functions_ghidra.tsv"),
        "jgld": ("jgld.dll", "smsg_jgld_lib", "functions_ghidra_jgld.dll.tsv"),
        "sound": ("sound.dll", "smsg_sound_lib", "functions_ghidra_sound.dll.tsv"),
        "Terrain": ("Terrain.dll", "smsg_terrain_lib", "functions_ghidra_Terrain.dll.tsv")}
BASE = {"golf": 0, "jgld": 0x10000000, "sound": 0x10000000, "Terrain": 0x10000000}
SP = {"sp2": "VC6 SP2", "sp5": "VC6 SP5", "rtm": "VC6 RTM"}
FIXED = {"operator new": "operator_new", "operator delete": "operator_delete", "operator new[]": "operator_new_array",
         "operator delete[]": "operator_delete_array"}
OPS = {"2": "operator_new", "3": "operator_delete", "_U": "operator_new_array", "_V": "operator_delete_array"}


def clean(s):
    s = re.sub(r"[`']", "", s)
    return re.sub(r"[^A-Za-z0-9_:~]+", "_", s) or "unnamed"


SPECIAL_OPS = {"0": "ctor", "1": "dtor", "4": "operator_assign", "5": "operator_rshift", "6": "operator_lshift",
               "8": "operator_eq", "9": "operator_ne", "A": "operator_index", "_G": "scalar_deleting_destructor",
               "_E": "vector_deleting_destructor"}


def from_symbol(sym):
    """Name from a decorated PDB public: C names keep the PDB's leading underscores (the S10.8 convention), @N is dropped,
    C++ names lose the decoration: ?m@C@@ -> C::m, ??0C@@ -> C::ctor, ??6C@@ -> C::operator_lshift."""
    m = re.match(r"\?\?(_?[0-9A-Z])@@?", sym)
    if m and m.group(1) in OPS:
        return OPS[m.group(1)]
    m = re.match(r"\?\?(_?[0-9A-Z])([^@]+)@@", sym)
    if m and m.group(1) in SPECIAL_OPS:
        return f"{m.group(2)}::{SPECIAL_OPS[m.group(1)]}"
    m = re.match(r"\?([^@?]+)@([^@]*)@", sym)
    if m:
        return f"{m.group(2)}::{m.group(1)}" if m.group(2) else m.group(1)
    return clean(re.sub(r"@\d+$", "", sym))


def names_taken(skip):
    out = {}
    for f in glob.glob(str(ROOT / "re/names/*.tsv")):
        p = pathlib.Path(f)
        if p.stem.endswith("_globals") or p.stem.endswith("_lib") or p.stem == skip or p.stem == "withdrawn":
            continue
        for l in p.read_text(encoding="utf-8").splitlines()[1:]:
            c = l.split("\t")
            if len(c) >= 3:
                out.setdefault(c[0], set()).add(c[2])
    return out


def main():
    taken_all = names_taken("")
    for key, (mod, nid, fnfile) in MODS.items():
        fn = {}
        for l in (ROOT / "re" / fnfile).read_text(encoding="utf-8").splitlines()[1:]:
            c = l.split("\t")
            fn[int(c[0], 16)] = c[1]
        fnames = {v for v in fn.values()}
        taken = set(taken_all.get(mod, set()))
        pdbname = {}
        for l in (ROOT / f"log/roadmap/S10/7_cand_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]:
            c = l.split("\t")
            pdbname[int(c[0], 16)] = c[4].split(" | ")[0] if c[2] == "proc" else ""
        gaps = {int(l.split("\t")[0], 16): l.split("\t") for l in (ROOT / f"log/roadmap/S10/7_gaps_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]}
        out, led, done = [], [], set()
        for l in (ROOT / f"log/roadmap/S10/7_score_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]:
            c = l.split("\t")
            if float(c[5]) != 100.0:
                continue
            va = int(c[0], 16)
            k = va - BASE[key]
            if va in done:          # two PDB procedures at one address (alias entry)
                continue
            done.add(va)
            if gaps[va][5] != "-":
                led.append(f"{c[0]}\tHAS_ROW\t{c[1]}\t{gaps[va][5]}\t")
                continue
            if k not in fn:
                led.append(f"{c[0]}\tNOT_FUNCTION\t{c[1]}\t-\tno function of ours starts here (entry label inside another body or a thunk)")
                continue
            sym = c[1]
            base = FIXED.get(pdbname.get(va, ""), "") or (clean(pdbname[va]) if pdbname.get(va) and "$$$" not in pdbname[va] else from_symbol(sym))
            name = base
            if name in taken or (name in fnames and fn[k] != name):
                name = f"{base}_{c[0][2:]}"
            taken.add(name)
            tag = c[3]
            stem = f"{k:08x}_{key + '_' if key != 'golf' else ''}{re.sub(r'[^A-Za-z0-9_]', '_', sym)}.match.csv"
            rel = f"log/roadmap/S10/7_lib/{key}/{tag}/{stem}"
            how = {"": "the normalized instructions equal", "byte-identity (relocation sites masked)":
                   "the bytes equal, relocation sites masked,"}.get(c[8], f"the normalized instructions equal ({c[8]} extent)")
            ev = (f"library-object identity: {c[5]}% ({c[6]}/{c[7]}) {how} the body of member {c[4].split(chr(92))[-1]} of "
                  f"{tag.split('_')[1]} in {SP[tag.split('_')[0]]} (log/roadmap/S10/7_tools/libscore.py, CSV {rel}); "
                  f"symbol {sym}; address in our function list as {fn[k]}")
            out.append("\t".join([mod, c[0], name, "crt", ev, f"{SP[tag.split('_')[0]]} static CRT routine {sym} (object {c[4].split(chr(92))[-1]})."]))
            led.append(f"{c[0]}\tACCEPT\t{sym}\t{name}\t")
        rev = ROOT / f"log/roadmap/S10/7_reverse_{key}_amb.tsv"       # reverse.py --only pass: leftovers identical to ONE library function
        for l in (rev.read_text(encoding="utf-8").splitlines()[1:] if rev.exists() else []):
            c = l.split("	")
            if len(c) < 9 or c[7] == "-" or c[8] != "1" or c[0] != mod or int(c[1], 16) in done:
                continue
            va = int(c[1], 16)
            k = va - BASE[key]
            if k not in fn or (mod, va) in {(mod, a) for a in ()}:
                continue
            done.add(va)
            sym, tag = c[3], c[4]
            name = from_symbol(sym)
            if name in taken:
                name = f"{name}_{c[1][2:]}"
            taken.add(name)
            ev = (f"library-object identity of a function without a PDB name: {c[6]}% of the normalized instructions equal the body "
                  f"of symbol {sym} in member {c[5].split(chr(92))[-1]} of {tag.split('_')[1]} in {SP[tag.split('_')[0]]}, the only function of "
                  f"that length in the archives that is identical (log/roadmap/S10/7_tools/reverse.py, CSV {c[7]}); address in our "
                  f"function list as {fn[k]}")
            out.append("	".join([mod, c[1], name, "crt", ev, f"{SP[tag.split('_')[0]]} static CRT routine {sym} (object {c[5].split(chr(92))[-1]})."]))
            led.append(f"{c[1]}	ACCEPT_REV	{sym}	{name}	")
        (ROOT / f"re/names/{nid}.tsv").write_text("module\taddr\tname\tsubsystem\tevidence\tpurpose\n" + "\n".join(out) + "\n", encoding="utf-8")
        (ROOT / f"log/roadmap/S10/7_ledger_{key}.tsv").write_text("va\tverdict\tsymbol\tname\treason\n" + "\n".join(led) + "\n", encoding="utf-8")
        print(nid, len(out), "rows;", sum("NOT_FUNCTION" in x for x in led), "not functions;", sum("HAS_ROW" in x for x in led), "have a row")


main()
