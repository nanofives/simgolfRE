"""S11.4: generate log/roadmap/S11/4_edits.py (anchored edits, every old text exactly once) from a pristine tree (default the I22
mirror) by applying the rename policy of 4_notes.md section 2 in memory, then the hand edits of manual.py, then cutting the
differences into narrow hunks.

    ~/bin/py -3.12 log/roadmap/S11/4_tools/gen.py [--base port/build/I22] [--report]

Writes 4_edits.py and 4_gen_report.tsv (file, edits, renamed occurrences, kept lines). Never writes into the base tree.
"""
from __future__ import annotations

import csv
import difflib
import importlib.util
import pathlib
import pprint
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
S11 = HERE.parent
ROOT = S11.parents[2]
sys.path.insert(0, str(HERE))
import lexrename as LX  # noqa: E402

ROWS = list(csv.DictReader(open(S11 / "4_renames.tsv", encoding="utf-8"), delimiter="\t"))
TYPES = [("SgRecordBank", "SgHomeLayout"), ("Animal", "VipResident")]
# defect switch ids stay (recorded in session.json, defect_profiles.tsv, the app's compiled profile copy, integrator.py)
SWITCH_IDS = ["spawnObject.cellSearchBound", "placeAnimal.zeroFrameCount"]
# C++ identifiers built from a name in snake case (measured in port/src / port/include, 4_notes.md section 2)
SNAKE = [("spawn_object", "spawn_vip_resident"), ("place_animal", "update_vip_resident"),
         ("tick_animals", "update_vip_residents"), ("free_record_at_pos", "remove_vip_resident_at"),
         ("reset_record_bank", "reset_home_layout"), ("place_record", "place_home_layout_part"),
         ("init_records", "init_home_layouts"), ("draw_records", "draw_home_layout"),
         ("draw_flying_ball", "draw_walkers"), ("announce_building", "open_hole"),
         ("hole_magazine_event", "rate_course_holes"), ("rate_tile", "golfer_shot_reach"),
         ("tile_query449310", "corner_height_at")]
# method part of a `Class::method` name used alone as a C++ identifier (NEG / NG macro arguments), per file
BARE = {"port/src/sim/audio_exe.cpp": [("setField34", "setDelay"), ("set_field34", "set_delay")]}

GENERATED = {"port/include/gamestate.h", "port/include/gamestate_meta.inc", "port/include/gamestate_addresses.inc",
             "port/include/portsim/sim/soundstate.h", "re/model/include/simgolf_model.h", "re/model/model_ctypes.py"}

Q = [(r["old"], r["new"]) for r in ROWS]                                  # names as written (Class::method)
ID = [(r["old"].replace("::", "_"), r["new"].replace("::", "_")) for r in ROWS]
SOUND_Q = [(r["old"], r["new"]) for r in ROWS if r["module"] == "sound.dll"]

# Python lines that hold corpus keys (registry keys, raw-capture keys, bundle keys) or pytest ids: not renamed
PY_KEEP = {
    "port/test/test_events.py": [r'^\s+"announceBuilding", "announceBuilding_bi0"', r'^\s+"holeMagazineEvent", "holeMagazineEvent_busy"'],
    "port/test/audio_stream.py": [r'^\s+\("Snd::', r'^\s+\("VoiceRx::'],
}


def cpp_rename(rel: str, text: str) -> tuple[str, int]:
    ids = sorted(ID + SNAKE + BARE.get(rel, []) + TYPES, key=lambda p: -len(p[0]))
    exact = {"Animal"}
    out, n = [], 0
    for kind, seg in LX.cpp_segments(text):
        if kind == "code":
            new = LX.cpp_code_sub(seg, ids, exact)
        else:
            pairs = Q + (TYPES if kind == "comment" else [])
            new, _ = LX.word_sub(seg, pairs, SWITCH_IDS, columns=kind == "comment")
        n += new != seg
        out.append(new)
    return "".join(out), n


def shim_rename(rel: str, text: str) -> tuple[str, int]:
    out, n = [], 0
    for kind, seg in LX.cpp_segments(text):
        new = LX.word_sub(seg, Q, SWITCH_IDS, columns=True)[0] if kind == "comment" else seg
        n += new != seg
        out.append(new)
    return "".join(out), n


def model_header_rename(rel: str, text: str) -> tuple[str, int]:
    out, n = [], 0
    for kind, seg in LX.cpp_segments(text):
        if kind == "code":
            new = LX.cpp_code_sub(seg, TYPES, {"Animal"})
        elif kind == "comment":
            new = LX.word_sub(seg, Q + TYPES, SWITCH_IDS, columns=True)[0]
        else:
            new = seg
        n += new != seg
        out.append(new)
    return "".join(out), n


def py_rename(rel: str, text: str) -> tuple[str, int]:
    spans = LX.py_spans(text)
    out, pos, n = [], 0, 0
    for a, b, kind in spans:
        out.append(text[pos:a])
        seg = text[a:b]
        new = LX.word_sub(seg, Q, SWITCH_IDS, columns=kind == "comment")[0]
        n += new != seg
        out.append(new)
        pos = b
    out.append(text[pos:])
    new_text = "".join(out)
    keep = [re.compile(p) for p in PY_KEEP.get(rel, [])]
    if keep:
        ol, nl = text.split("\n"), new_text.split("\n")
        assert len(ol) == len(nl), rel
        for i, line in enumerate(ol):
            if any(k.search(line) for k in keep):
                nl[i] = line
        new_text = "\n".join(nl)
    return new_text, n


def golden_rename(rel: str, text: str) -> tuple[str, int]:
    n = 0
    for old, new in sorted(Q, key=lambda p: -len(p[0])):
        pat = re.compile(r'("name":\s*"(?:snd\.)?)' + re.escape(old) + r'"')
        text, c = pat.subn(lambda m, new=new: m.group(1) + new + '"', text)
        n += c
    return text, n


def tsv_word_rename(pairs):
    def f(rel, text):
        return LX.word_sub(text, pairs, SWITCH_IDS)
    return f


def addresses_rename(rel: str, text: str) -> tuple[str, int]:
    """re/model/addresses.tsv: symbol, element / c type and parent of the two renamed objects; evidence text stays."""
    lines = text.split("\n")
    head = lines[0].rstrip("\r").split("\t")
    ci = {c: i for i, c in enumerate(head)}
    sym = {"g_animalRecords": "g_vipResidents", "g_recordBanks": "g_homeLayouts"}
    ty = dict(TYPES)
    n = 0
    for k in range(1, len(lines)):
        cr = lines[k].endswith("\r")
        f = lines[k].rstrip("\r").split("\t")
        if len(f) != len(head):
            continue
        g = list(f)
        if f[ci["symbol"]] in sym:
            g[ci["symbol"]] = sym[f[ci["symbol"]]]
            for c in ("element_type", "c_type"):
                g[ci[c]] = ty.get(f[ci[c]], f[ci[c]])
        if f[ci["parent"]] in sym:
            g[ci["parent"]] = sym[f[ci["parent"]]]
        if g != f:
            n += 1
            lines[k] = "\t".join(g) + ("\r" if cr else "")
    return "\n".join(lines), n


def stream_rename(rel: str, text: str) -> tuple[str, int]:
    """re/model/<s>_addresses.tsv: the element type of the two objects (so model_merge reproduces addresses.tsv)."""
    lines = text.split("\n")
    head = lines[0].rstrip("\r").split("\t")
    ci = {c: i for i, c in enumerate(head)}
    ty = dict(TYPES)
    n = 0
    for k in range(1, len(lines)):
        cr = lines[k].endswith("\r")
        f = lines[k].rstrip("\r").split("\t")
        if len(f) != len(head):
            continue
        for c in ("element_type", "c_type", "type"):
            if c in ci and f[ci[c]] in ty:
                f[ci[c]] = ty[f[ci[c]]]
                n += 1
                lines[k] = "\t".join(f) + ("\r" if cr else "")
    return "\n".join(lines), n


def sve_rename(rel: str, text: str) -> tuple[str, int]:
    pairs = [("g_animalRecords", "g_vipResidents"), ("g_recordBanks", "g_homeLayouts")] + TYPES
    lines = text.split("\n")
    n = 0
    for k, line in enumerate(lines):
        if "g_animalRecords" in line or "g_recordBanks" in line:
            lines[k], c = LX.word_sub(line, pairs)
            n += c
    return "\n".join(lines), n


def targets(base: pathlib.Path) -> list[tuple[str, object]]:
    out = []
    for d in ("port/src", "port/include", "port/app"):
        for p in sorted((base / d).rglob("*")):
            rel = p.relative_to(base).as_posix()
            if p.suffix in (".cpp", ".h", ".inc", ".hpp", ".c") and rel not in GENERATED:
                out.append((rel, cpp_rename))
    for p in sorted((base / "port").glob("*.md")):
        out.append((p.relative_to(base).as_posix(), tsv_word_rename(Q)))
    for p in sorted((base / "port/test").rglob("*")):
        rel = p.relative_to(base).as_posix()
        if "__pycache__" in rel or not p.is_file():
            continue
        if p.suffix == ".py":
            out.append((rel, py_rename))
        elif p.suffix == ".json" and p.name.startswith("golden"):
            out.append((rel, golden_rename))
        elif p.suffix == ".md":
            out.append((rel, tsv_word_rename(Q)))
        elif p.suffix in (".cpp", ".h"):
            out.append((rel, cpp_rename))
    for p in sorted((base / "shim/src").rglob("*")):
        if p.suffix in (".cpp", ".h", ".inc", ".c"):
            out.append((p.relative_to(base).as_posix(), shim_rename))
    for s in ("b", "c", "d", "e1", "e2", "f"):
        out.append((f"re/model/include/sections/{s}.h", model_header_rename))
        out.append((f"re/model/{s}_addresses.tsv", stream_rename))
    out.append(("re/model/addresses.tsv", addresses_rename))
    out.append(("re/model/sve_fields.tsv", sve_rename))
    out.append(("re/roadmap/S4_functions.tsv", tsv_word_rename(SOUND_Q)))
    return out


def read(p: pathlib.Path) -> str:
    return p.read_bytes().decode("utf-8")


def hunks(rel: str, old: str, new: str) -> list[tuple[str, str, str]]:
    """Cut old -> new into (rel, old_text, new_text) edits, each old_text unique in the text it is applied to (in order)."""
    a = old.split("\n")
    b = new.split("\n")
    ops = [o for o in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes() if o[0] != "equal"]
    groups = []
    for tag, i1, i2, j1, j2 in ops:
        groups.append([i1, i2, j1, j2])
    edits = []
    cur = old
    lo_min = 0                                  # lines before this were already part of an earlier edit
    k = 0
    while k < len(groups):
        i1, i2, j1, j2 = groups[k]
        ctx = 0
        while True:
            lo, hi = max(lo_min, i1 - ctx), min(len(a), i2 + ctx)
            # merge with the next group if the context reaches it
            if k + 1 < len(groups) and hi >= groups[k + 1][0]:
                ni1, ni2, nj1, nj2 = groups.pop(k + 1)
                i2, j2 = ni2, nj2
                continue
            old_t = "\n".join(a[lo:hi])
            new_t = "\n".join(a[lo:i1] + b[j1:j2] + a[i2:hi])
            if i1 == i2 and lo == hi:          # pure insertion with no context yet
                ctx += 1
                continue
            if old_t and cur.count(old_t) == 1:
                break
            ctx += 1
            assert ctx < 400, (rel, i1)
        cur = cur.replace(old_t, new_t, 1)
        edits.append((rel, old_t, new_t))
        lo_min = hi
        k += 1
    assert cur == new, f"{rel}: hunks do not rebuild the edited text"
    return edits


def main() -> None:
    a = sys.argv
    base = ROOT / (a[a.index("--base") + 1] if "--base" in a else "port/build/I22")
    spec = importlib.util.spec_from_file_location("s114_manual", HERE / "manual.py")
    M = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(M)
    new_texts: dict[str, str] = {}
    orig: dict[str, str] = {}
    report = []
    for rel, fn in targets(base):
        p = base / rel
        if not p.exists():
            continue
        t = read(p)
        crlf = "\r\n" in t
        tn = t.replace("\r\n", "\n") if crlf else t
        nt, n = fn(rel, tn)
        if nt != tn:
            orig[rel] = tn
            new_texts[rel] = nt
            report.append((rel, n))
    # hand edits (manual.py): applied on top of the renamed text
    for rel, old, new in M.EDITS:
        if rel not in new_texts:
            t = read(base / rel)
            orig[rel] = t.replace("\r\n", "\n")
            new_texts[rel] = orig[rel]
        c = new_texts[rel].count(old)
        assert c == 1, f"manual edit anchor {c}x in {rel}: {old[:80]!r}"
        new_texts[rel] = new_texts[rel].replace(old, new, 1)
    edits = []
    for rel in sorted(new_texts):
        edits += hunks(rel, orig[rel], new_texts[rel])
    aliases = "kind\tmodule\taddr\tcorpus_name\tport_name\n" + "".join(
        f"{r['kind']}\t{r['module']}\t{r['addr']}\t{r['old']}\t{r['new']}\n" for r in ROWS)
    new_files = sorted({**M.NEW_FILES, "re/port/corpus_aliases.tsv": aliases}.items())
    out = S11 / "4_edits.py"
    with open(out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write('"""S11.4 edits (generated by log/roadmap/S11/4_tools/gen.py from port/build/I22; applied by 4_assemble.py).\n'
                 'EDITS: (path, old, new), each old text exactly once in the file when it is applied (texts use LF; a CRLF file is\n'
                 'matched and written in CRLF). NEW_FILES: path -> content, refused when the file exists."""\n')
        fh.write("EDITS = ")
        fh.write(pprint.pformat(edits, width=130))
        fh.write("\nNEW_FILES = ")
        fh.write(pprint.pformat(dict(new_files), width=130))
        fh.write("\n")
    with open(S11 / "4_gen_report.tsv", "w", encoding="utf-8", newline="\n") as fh:
        fh.write("file\tsegments_renamed\tedits\n")
        per = {}
        for e in edits:
            per[e[0]] = per.get(e[0], 0) + 1
        for rel, n in report:
            fh.write(f"{rel}\t{n}\t{per.get(rel, 0)}\n")
        for rel in sorted(set(per) - {r for r, _ in report}):
            fh.write(f"{rel}\tmanual\t{per[rel]}\n")
    print(f"{len(edits)} edits in {len(new_texts)} files, {len(new_files)} new files -> {out}")


if __name__ == "__main__":
    main()
