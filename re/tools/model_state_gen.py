r"""Generate port/include/gamestate.h: the original data section as one C++17 struct (phase 2.2, 2026-10-18).

GameState reproduces golf_clean.exe's .data section (initialised data and .bss, 0x381174 bytes) byte for byte: one member
per top-level model object of re/model/addresses.tsv inside the section, at its own offset, typed with the merged model
header re/model/include/simgolf_model.h, and a `uint8_t gap_<offset>[n]` member for every run of bytes no model object
covers. A checkpoint or a state image of the original is then a GameState byte image, and a comparison is a per-object
memcmp (re/tools/state_image.py). The port's code takes `GameState &` and uses the members by name only.

Outputs (all generated, deterministic, committed):
  port/include/gamestate.h              the struct (#pragma pack(1): every member sits at the original offset, whatever
                                        its type's alignment), sizeof / offsetof / member-size static_asserts. No original
                                        address appears in it (offsets only; the test applies model_merge.ADDR_RE).
  port/include/gamestate_meta.inc       X-macro table SG_OBJECT(symbol, offset, size, class, pointer, hidden, saved),
                                        SG_POINTER_WORD(symbol, offset_in_object), SG_GAP(offset, size): what a comparison
                                        includes (class `sim` by default) and which dwords it masks.
  port/include/gamestate_addresses.inc  SG_DATA_SECTION(va, size) and SG_ADDRESS(symbol, va): for tooling only (the image
                                        parsers), never included by port code.

Classes (one per object, first rule that applies):
  1. the phase 2.1 hidden-state table (re/analysis/data/premise_check_hidden_state.tsv, matched by va and size):
     H0 / H1 / H2-rewritten / H7 -> sim, H2-stale / H5 / H8 -> hidden, H3 -> ui, H4 -> render, H6 -> audio;
  2. a .sve field (sve_field column) -> sim;
  3. audio objects (AUDIO_TYPES or AUDIO_NAME) -> audio;
  4. pointer column `type` (re/model/DECISIONS.md, phase 2.2) -> pointer;
  5. G1_CLASS for the rows stream G1 typed itself;
  6. by stream: b / c -> sim, d -> render, e1 -> ui, e2 -> engine, f -> const.
`pointer` (SG_POINTER_WORD) lists the `words:` dwords of the pointer column; `hidden` is the H code or `-`; `saved` is 1
for a .sve field.

    ~/bin/py -3.12 re/tools/model_state_gen.py            # write the three files, print the class counts
    ~/bin/py -3.12 re/tools/model_state_gen.py --check    # exit 1 when any of them is stale
"""
from __future__ import annotations

import argparse
import collections
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import model_ctypes_gen as G  # noqa: E402  (the C reader of the model sections)

ADDRESSES = ROOT / "re" / "model" / "addresses.tsv"
HIDDEN = ROOT / "re" / "analysis" / "data" / "premise_check_hidden_state.tsv"
PORT_INC = ROOT / "port" / "include"
OUT_H = PORT_INC / "gamestate.h"
OUT_META = PORT_INC / "gamestate_meta.inc"
OUT_ADDR = PORT_INC / "gamestate_addresses.inc"
DATA_VA, DATA_N = 0x004C1000, 0x381174     # golf_clean.exe .data (VirtualAddress + VirtualSize; = restart_ab.py DATA_VA/N)

CLASSES = ["sim", "hidden", "ui", "render", "audio", "pointer", "engine", "const"]
HIDDEN_CLASS = [  # prefix of the hidden-state table's class text -> GameState class
    ("H0", "sim"), ("H1", "sim"), ("H2 not saved, rewritten", "sim"), ("H2 not saved, stale", "hidden"),
    ("H3", "ui"), ("H4", "render"), ("H5", "hidden"), ("H6", "audio"), ("H7", "sim"), ("H8", "hidden"),
]
AUDIO_TYPES = {"Snd", "NodeList", "NodeListBucket", "VoiceRx"}
AUDIO_NAME = re.compile(r"sound|music|snd|voice|midi|wave|audio|resample", re.I)
STREAM_CLASS = {"b": "sim", "c": "sim", "d": "render", "e1": "ui", "e2": "engine", "f": "const"}
G1_CLASS = {  # rows stream G1 typed itself (re/model/DECISIONS.md); each from the row's evidence in addresses.tsv
    "g_moneyLabelX": "render", "g_moneyLabelY": "render",     # money-popup ring read by drawMoneyLabels
    "g_jpegSrcPitch": "engine",                                # libjpeg screenshot source pitch
    "g_pendingCourseInfo": "sim",                              # setPendingCourseInfo's course text (stream H)
    "g_keyTables": "ui", "g_sinTable": "const",
    "g_dataDirA": "engine", "g_dataDirB": "engine", "g_pathScratch": "engine", "g_dataDirC": "engine",
}


def model():
    m = G.Model()
    for p in sorted(G.SECTIONS.glob("*.h")):
        m.parse(p)
    return m


def header_types() -> set[str]:
    txt = (ROOT / "re" / "model" / "include" / "simgolf_model.h").read_text(encoding="utf-8")
    return set(re.findall(r"\}\s*(\w+)\s*;", txt)) | set(re.findall(r"typedef\s+\w+\s+(\w+)\s*\[", txt))


def load_rows():
    with ADDRESSES.open(newline="", encoding="utf-8") as f:
        return [r for r in csv.DictReader(f, delimiter="\t")
                if r["module"] == "golf_clean.exe" and r["relation"] == "top"
                and DATA_VA <= int(r["va"], 16) < DATA_VA + DATA_N]


def load_hidden():
    out = {}
    with HIDDEN.open(newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            if r["va"] in ("", "-"):
                continue                       # an unmodeled range: no object to classify
            out[(int(r["va"], 16), int(r["size"], 16))] = r
    return out


def member_type(r, m, htypes):
    """(C type, dims) of a row in the merged header's terms; ('uint8_t', [size]) when the type does not resolve."""
    size = int(r["size"], 16)
    for t in (r["c_type"], r["element_type"]):
        nt = G.norm_type(t)
        if nt is None:
            continue
        base, dims = nt
        if base not in G.SCALARS and base not in htypes:
            continue
        try:
            es = m.size_align(base)[0]
        except KeyError:
            continue
        n = 1
        for d in dims:
            n *= d
        if es * n == size:
            return base, dims, None
        if size % (es * n) == 0:
            return base, [size // (es * n)] + dims, None
        if size % es == 0:
            return base, [size // es], f"`{t}` reshaped to {base}[{size // es}]"
    return "uint8_t", [size], f"type `{r['c_type']}` not in the header: bytes"


def classify(r, hidden):
    h = hidden.get((int(r["va"], 16), int(r["size"], 16)))
    if h is not None:
        for pre, cls in HIDDEN_CLASS:
            if h["class"].startswith(pre):
                return cls, h["class"].split()[0]
        raise SystemExit(f"hidden-state class not mapped: {h['class']}")
    if r["sve_field"] not in ("", "-"):
        return "sim", "-"
    if r["c_type"] in AUDIO_TYPES or AUDIO_NAME.search(r["symbol"]):
        return "audio", "-"
    if r["pointer"] == "type":
        return "pointer", "-"
    if r["stream"] == "g1":
        if r["symbol"] not in G1_CLASS:
            raise SystemExit(f"G1 row without a class: {r['symbol']}")
        return G1_CLASS[r["symbol"]], "-"
    return STREAM_CLASS[r["stream"]], "-"


def objects():
    """The GameState layout: [{symbol, offset, size, type, dims, cls, hidden, saved, words, note}] incl. gaps, by
    offset, covering [0, DATA_N) exactly."""
    m, htypes, hidden = model(), header_types(), load_hidden()
    rows = sorted(load_rows(), key=lambda r: int(r["va"], 16))
    out, pos = [], 0
    matched = set()
    for r in rows:
        off, size = int(r["va"], 16) - DATA_VA, int(r["size"], 16)
        if off < pos:
            raise SystemExit(f"{r['symbol']}: overlaps the previous object")
        if off + size > DATA_N:
            raise SystemExit(f"{r['symbol']}: runs past the data section")
        if off > pos:
            out.append(gap(pos, off - pos))
        base, dims, note = member_type(r, m, htypes)
        cls, hcode = classify(r, hidden)
        if hcode != "-":
            matched.add((int(r["va"], 16), size))
        words = []
        if r["pointer"].startswith("words:"):
            words = [int(w, 16) for w in r["pointer"][6:].split(",")]
        out.append({"symbol": r["symbol"], "offset": off, "size": size, "type": base, "dims": dims, "cls": cls,
                    "hidden": hcode, "saved": int(r["sve_field"] not in ("", "-")), "words": words, "note": note,
                    "va": int(r["va"], 16), "pointer": r["pointer"]})
        pos = off + size
    if pos < DATA_N:
        out.append(gap(pos, DATA_N - pos))
    unmatched = [k for k in hidden if k not in matched]
    return out, unmatched


def gap(off, n):
    return {"symbol": f"gap_{off:06x}", "offset": off, "size": n, "type": "uint8_t", "dims": [n], "cls": "unmodeled",
            "hidden": "-", "saved": 0, "words": [], "note": None, "va": DATA_VA + off, "pointer": "-"}


def decl(o):
    t = {"char": "char"}.get(o["type"], o["type"])
    return t + " " + o["symbol"] + "".join(f"[{d}]" for d in o["dims"])


def emit_header(objs):
    n_obj = sum(1 for o in objs if o["cls"] != "unmodeled")
    L = ["// GENERATED by re/tools/model_state_gen.py from re/model/addresses.tsv, re/model/include/simgolf_model.h and",
         "// re/analysis/data/premise_check_hidden_state.tsv. Do not edit: regenerate (tests/test_gamestate_gen.py checks it).",
         "//",
         "// GameState: the original game's data section (initialised data and .bss) as one struct. Every model object is a",
         "// member at its original offset with its model type; bytes no model object covers are gap_<offset> members.",
         "// Pointers of the original 32-bit process are uint32_t (the model's convention), so the layout is the same on x86",
         "// and x64. Classes, pointer words and the hidden-state codes are in gamestate_meta.inc. Specification only: no",
         "// code or data of the original is in this file.",
         f"// {n_obj} objects, {len(objs) - n_obj} gaps, 0x{DATA_N:x} bytes.",
         "#ifndef PORTSIM_GAMESTATE_H", "#define PORTSIM_GAMESTATE_H", "",
         "#include <cstddef>", "#include <cstdint>", "", '#include "simgolf_model.h"', "",
         "namespace portsim {", "",
         "#pragma pack(push, 1)", "struct GameState {"]
    for o in objs:
        tag = o["cls"] + ("" if o["hidden"] == "-" else " " + o["hidden"]) + (" saved" if o["saved"] else "")
        if o["words"]:
            tag += " ptrwords " + ",".join(f"+0x{w:x}" for w in o["words"])
        L.append(f"    {decl(o)};".ljust(60) + f" // +0x{o['offset']:06x} {tag}")
    L += ["};", "#pragma pack(pop)", "",
          f"static_assert(sizeof(GameState) == 0x{DATA_N:x}, \"GameState size\");"]
    for o in objs:
        L.append(f"static_assert(offsetof(GameState, {o['symbol']}) == 0x{o['offset']:x} && "
                 f"sizeof(GameState::{o['symbol']}) == 0x{o['size']:x}, \"{o['symbol']}\");")
    L += ["", "}  // namespace portsim", "", "#endif  // PORTSIM_GAMESTATE_H", ""]
    return "\n".join(L)


def emit_meta(objs):
    L = ["// GENERATED by re/tools/model_state_gen.py. Do not edit.",
         "// X-macro table of GameState (gamestate.h). Define the macros you need before including; the others expand to",
         "// nothing. SG_OBJECT(symbol, offset, size, class, pointer, hidden, saved): class one of " + " / ".join(CLASSES) + ";",
         "// pointer `type` (the object's type holds pointers), `words` (only the SG_POINTER_WORD dwords do) or `none`;",
         "// hidden the phase 2.1 hidden-state code (H0..H8) or `none`; saved 1 when the object is a .sve field.",
         "// SG_POINTER_WORD(symbol, offset_in_object): a dword holding a process-specific pointer or handle.",
         "// SG_GAP(offset, size): bytes no model object covers.",
         "// Every macro is #undef'd at the end of this file.",
         "#ifndef SG_OBJECT", "#define SG_OBJECT(symbol, offset, size, cls, pointer, hidden, saved)", "#endif",
         "#ifndef SG_POINTER_WORD", "#define SG_POINTER_WORD(symbol, offset)", "#endif",
         "#ifndef SG_GAP", "#define SG_GAP(offset, size)", "#endif"]
    for o in objs:
        if o["cls"] == "unmodeled":
            L.append(f"SG_GAP(0x{o['offset']:06x}, 0x{o['size']:x})")
            continue
        ptr = "type" if o["pointer"] == "type" else ("words" if o["words"] else "none")
        hid = o["hidden"] if o["hidden"] != "-" else "none"
        L.append(f"SG_OBJECT({o['symbol']}, 0x{o['offset']:06x}, 0x{o['size']:x}, {o['cls']}, {ptr}, {hid}, {o['saved']})")
        for w in o["words"]:
            L.append(f"SG_POINTER_WORD({o['symbol']}, 0x{w:x})")
    L += ["#undef SG_OBJECT", "#undef SG_POINTER_WORD", "#undef SG_GAP"]
    return "\n".join(L) + "\n"


def emit_addresses(objs):
    L = ["// GENERATED by re/tools/model_state_gen.py. Do not edit. TOOLING ONLY: original addresses of the GameState",
         "// objects (golf_clean.exe v1.03). Port code never includes this file.",
         "// Every macro is #undef'd at the end of this file.",
         "#ifndef SG_DATA_SECTION", "#define SG_DATA_SECTION(va, size)", "#endif",
         "#ifndef SG_ADDRESS", "#define SG_ADDRESS(symbol, va)", "#endif",
         f"SG_DATA_SECTION(0x{DATA_VA:08x}, 0x{DATA_N:x})"]
    L += [f"SG_ADDRESS({o['symbol']}, 0x{o['va']:08x})" for o in objs if o["cls"] != "unmodeled"]
    L += ["#undef SG_DATA_SECTION", "#undef SG_ADDRESS"]
    return "\n".join(L) + "\n"


def build():
    objs, unmatched = objects()
    return {OUT_H: emit_header(objs), OUT_META: emit_meta(objs), OUT_ADDR: emit_addresses(objs)}, objs, unmatched


def summary(objs):
    c = collections.Counter()
    b = collections.Counter()
    for o in objs:
        c[o["cls"]] += 1
        b[o["cls"]] += o["size"]
    return {k: (c[k], b[k]) for k in CLASSES + ["unmodeled"] if c[k]}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    files, objs, unmatched = build()
    if a.check:
        stale = [p for p, t in files.items() if not p.exists() or p.read_text(encoding="utf-8") != t]
        for p in stale:
            print(f"stale: {p.relative_to(ROOT)}")
        return 1 if stale else 0
    PORT_INC.mkdir(parents=True, exist_ok=True)
    for p, t in files.items():
        p.write_text(t, encoding="utf-8", newline="\n")
        print(f"wrote {p.relative_to(ROOT)} ({t.count(chr(10))} lines)")
    for k, (n, nb) in summary(objs).items():
        print(f"  {k:9s} {n:5d} objects {nb:9,d} bytes")
    words = sum(len(o["words"]) for o in objs)
    print(f"  pointer words {words}; hidden-state rows not matched to an object: {len(unmatched)}")
    for o in objs:
        if o["note"]:
            print(f"  note {o['symbol']}: {o['note']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
