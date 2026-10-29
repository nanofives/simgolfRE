r"""Generate port/include/portsim/sim/soundstate.h: sound.dll's .data as portsim::SoundState, the RVAs of its named .rdata
symbols, and the table of every sound.dll function hooks.csv names (step S4.2, log/roadmap/S4/2_notes.md).

Inputs: re/names/sound_*_globals.tsv (module, addr, name, type, evidence), hooks.csv (sound.dll rows), the user's
original\sound.dll (section table only: the .data RVA / virtual size and a hash of .rdata). No byte of the DLL is copied:
the header holds offsets, sizes, RVAs, names and an sgh64 hash in decimal.

    struct SoundState    pack(1), sizeof == the .data VirtualSize; one member per named .data global at its offset, gaps as
                         uint8_t arrays; a global whose declared size runs into the next named one is cut at it (the
                         comment says so); a type whose size the names row does not give (`T[]`, `struct (...)`) is one
                         element and the rest is gap (NO-GUESSING: the size is not determined by the names row).
    namespace snd_rva    constexpr RVAs of the named globals outside .data (vtables, constant tables in .rdata).
    kSoundFunctions[]    {rva, hooks.csv name, collides} for every sound.dll row of hooks.csv; `collides` = the name is also
                         a row of another module (the registry is by name, so the dispatcher never maps such a row).

    ~/bin/py -3.12 re/tools/soundstate_gen.py           # write the header
    ~/bin/py -3.12 re/tools/soundstate_gen.py --check   # exit 1 when the committed header is stale
"""
from __future__ import annotations

import argparse
import csv
import pathlib
import re
import sys

import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import state_image as SI  # noqa: E402

NAMES = ROOT / "re" / "names"
HOOKS = ROOT / "hooks.csv"
DLL = ROOT / "original" / "sound.dll"
OUT = ROOT / "port" / "include" / "portsim" / "sim" / "soundstate.h"
IMAGE_BASE = 0x10000000

# element sizes of the names rows' types (bytes); a pointer type (contains '*') is 4
SIZES = {
    "int": 4, "uint": 4, "dword": 4, "handle": 4, "hwnd": 4, "fourcc": 4, "bool": 4, "float": 4, "double": 8,
    "critical_section": 0x18, "wndclassa": 0x28, "waveformatex": 0x12, "guid": 0x10, "char": 1, "short": 2,
    "uint16": 2, "uint8": 1, "byte": 1,
}
CTYPES = {"int": "int32_t", "float": "float", "double": "double", "char": "char", "short": "int16_t",
          "uint16": "uint16_t", "uint8": "uint8_t", "byte": "uint8_t"}


def section_info(path: pathlib.Path):
    pe = pefile.PE(str(path), fast_load=True)
    secs = {s.Name.rstrip(b"\0").decode(): s for s in pe.sections}
    d, r = secs[".data"], secs[".rdata"]
    rdata = r.get_data()[:r.Misc_VirtualSize]
    info = dict(data_rva=d.VirtualAddress, data_vsize=d.Misc_VirtualSize, data_raw=d.SizeOfRawData,
                rdata_rva=r.VirtualAddress, rdata_vsize=r.Misc_VirtualSize, rdata_hash=SI.sgh64(rdata),
                image_size=pe.OPTIONAL_HEADER.SizeOfImage)
    pe.close()
    return info


def parse_type(t: str):
    """(element C type, element size, count or None when not determined, note)"""
    s = t.strip()
    if not s.lower().startswith("struct"):
        s = re.sub(r"\s*\(.*\)\s*$", "", s)                 # `HANDLE (manual-reset event)` -> HANDLE
        if s.lower().startswith("pointer to"):
            return "uint32_t", 4, 1, ""
        if " " in s and "[" not in s and "*" not in s:      # `int flag` -> int
            s = s.split()[0]
    m = re.fullmatch(r"(.+?)\s*\[(\d*)\]", s)
    count = 1
    if m:
        s, n = m.group(1).strip(), m.group(2)
        count = int(n) if n else None
    key = s.lower().replace(" ", "")
    if "*" in s or key in ("pointer", "void*"):
        return "uint32_t", 4, count, ""
    if key.startswith("struct") or key.startswith("linked-list") or key.startswith("pointer"):
        return "uint8_t", 4 if key.startswith(("linked-list", "pointer")) else None, 1, s
    if key in SIZES:
        size = SIZES[key]
        ct = CTYPES.get(key)
        if ct is None:
            ct = "uint32_t" if size == 4 else "uint8_t"
            if size != 4:
                return "uint8_t", size, (None if count is None else count), s   # opaque record of `size` bytes
        return ct, size, count, ""
    return "uint8_t", None, 1, s


def load_globals():
    rows = []
    for p in sorted(NAMES.glob("sound_*_globals.tsv")):
        with p.open(newline="", encoding="utf-8") as f:
            for r in csv.DictReader(f, delimiter="\t"):
                if (r.get("module") or "").lower() != "sound.dll":
                    continue
                rows.append(dict(va=int(r["addr"], 16), name=r["name"].strip(), type=r["type"].strip(), src=p.name))
    rows.sort(key=lambda r: (r["va"], r["name"]))
    # one row per address (the first by file order wins; later ones are recorded as aliases)
    out, seen_va = [], {}
    for r in rows:
        if r["va"] in seen_va:
            seen_va[r["va"]]["aliases"].append(r["name"])
            continue
        r["aliases"] = []
        seen_va[r["va"]] = r
        out.append(r)
    return out


def cname(name: str) -> str:
    return re.sub(r"\W", "_", name)


def build(info):
    g = load_globals()
    lo = IMAGE_BASE + info["data_rva"]
    hi = lo + info["data_vsize"]
    data = [r for r in g if lo <= r["va"] < hi]
    other = [r for r in g if not (lo <= r["va"] < hi)]
    used = {}
    members, notes = [], []
    off = 0
    for i, r in enumerate(data):
        o = r["va"] - lo
        nxt = (data[i + 1]["va"] - lo) if i + 1 < len(data) else info["data_vsize"]
        ct, esz, cnt, note = parse_type(r["type"])
        name = cname(r["name"])
        if name in used:
            name = f"{name}_{o:x}"
        used[name] = o
        if o > off:
            members.append(("uint8_t", f"gap_{off:05x}", o - off, f""))
        avail = nxt - o
        if esz is None:
            size, decl_cnt = min(4, avail), None
            note = f"size not determined by the names row (`{r['type']}`)"
            ct = "uint8_t"
            members.append((ct, name, size, note))
        else:
            want = esz * (cnt if cnt is not None else 1)
            if cnt is None:
                note = (note + "; " if note else "") + f"count not determined by the names row (`{r['type']}`): one element"
            size = want
            if want > avail:
                size = avail
                note = (note + "; " if note else "") + f"declared `{r['type']}` ({want} bytes) cut at the next named global"
            members.append(("uint8_t" if size % esz else ct, name, size, note))   # size in bytes
        if r["aliases"]:
            notes.append(f"{name}: also named {', '.join(r['aliases'])}")
        off = o + members[-1][2]
    if off < info["data_vsize"]:
        members.append(("uint8_t", f"gap_{off:05x}", info["data_vsize"] - off, ""))
    return members, other, used


def esize(ct):
    return {"int32_t": 4, "uint32_t": 4, "float": 4, "double": 8, "char": 1, "int16_t": 2, "uint16_t": 2, "uint8_t": 1}[ct]


def load_functions():
    with HOOKS.open(newline="") as f:
        rows = list(csv.DictReader(f))
    other = {r["name"] for r in rows if r["module"] != "sound.dll"}
    fns = sorted(((int(r["addr"], 16), r["name"]) for r in rows if r["module"] == "sound.dll"))
    return [(a, n, n in other) for a, n in fns]


def render(info) -> str:
    members, other, used = build(info)
    fns = load_functions()
    L = []
    w = L.append
    w("// GENERATED by re/tools/soundstate_gen.py from re/names/sound_*_globals.tsv, hooks.csv and the section table of the")
    w("// user's sound.dll (step S4.2). Do not edit: rerun the generator. No byte of the DLL is in this file.")
    w("//")
    w("// SoundState is sound.dll's .data as the module sees it: the block of the context's 32-bit address space at")
    w("// image_base + kSoundDataRva (portsim/sim/audio.h maps or locates it). Members are the named module globals at their")
    w("// offsets; port code reaches them through snd_obj / snd_arr (audio.h), never through a host pointer.")
    w("#ifndef PORTSIM_SIM_SOUNDSTATE_H")
    w("#define PORTSIM_SIM_SOUNDSTATE_H")
    w("")
    w("#include <cstddef>")
    w("#include <cstdint>")
    w("")
    w("namespace portsim {")
    w("")
    w(f"constexpr uint32_t kSoundDataRva = 0x{info['data_rva']:x};          // .data VirtualAddress")
    w(f"constexpr uint32_t kSoundDataSize = 0x{info['data_vsize']:x};        // .data VirtualSize")
    w(f"constexpr uint32_t kSoundDataInit = 0x{info['data_raw']:x};          // .data SizeOfRawData (initialised bytes in the file)")
    w(f"constexpr uint32_t kSoundRdataRva = 0x{info['rdata_rva']:x};         // .rdata VirtualAddress")
    w(f"constexpr uint32_t kSoundRdataSize = 0x{info['rdata_vsize']:x};       // .rdata VirtualSize")
    w(f"constexpr uint64_t kSoundRdataHash = {info['rdata_hash']}ull;  // sgh64 of the file's .rdata bytes (decimal)")
    w(f"constexpr uint32_t kSoundImageSize = 0x{info['image_size']:x};        // SizeOfImage")
    w("")
    w("#pragma pack(push, 1)")
    w("struct SoundState {")
    for ct, name, size, note in members:
        es = esize(ct)
        n = size // es
        decl = f"  {ct} {name}" + (f"[{n}]" if (n != 1 or name.startswith("gap_")) else "") + ";"
        w(decl + (f"  // {note}" if note else ""))
    w("};")
    w("#pragma pack(pop)")
    w("static_assert(sizeof(SoundState) == kSoundDataSize, \"SoundState size\");")
    for name, o in sorted(used.items(), key=lambda x: x[1]):
        w(f"static_assert(offsetof(SoundState, {name}) == 0x{o:x}, \"{name}\");")
    w("")
    w("// RVAs of the named sound.dll globals outside .data (vtables and constants in .rdata). VA = image_base + rva.")
    w("namespace snd_rva {")
    seen = set()
    for r in other:
        n = cname(r["name"])
        if n in seen:
            n = f"{n}_{r['va'] - IMAGE_BASE:x}"
        seen.add(n)
        w(f"constexpr uint32_t {n} = 0x{r['va'] - IMAGE_BASE:x};  // {r['type']}")
    w("}  // namespace snd_rva")
    w("")
    w("// Every sound.dll function hooks.csv names: the dispatcher maps an rva to the registered port function of that name.")
    w("struct SoundFunction {")
    w("  uint32_t rva;")
    w("  const char *name;")
    w("  bool collides;  // the name is also another module's row: never dispatched by name")
    w("};")
    w("constexpr SoundFunction kSoundFunctions[] = {")
    for a, n, c in fns:
        w(f"    {{0x{a:x}, \"{n}\", {'true' if c else 'false'}}},")
    w("};")
    w("constexpr size_t kSoundFunctionCount = sizeof(kSoundFunctions) / sizeof(kSoundFunctions[0]);")
    w("")
    w("}  // namespace portsim")
    w("")
    w("#endif  // PORTSIM_SIM_SOUNDSTATE_H")
    return "\n".join(L) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--dll", default=str(DLL))
    a = ap.parse_args()
    text = render(section_info(pathlib.Path(a.dll)))
    if a.check:
        cur = OUT.read_text(encoding="utf-8") if OUT.exists() else ""
        if cur != text:
            print("soundstate.h is stale: rerun re/tools/soundstate_gen.py")
            return 1
        print("soundstate.h is current")
        return 0
    OUT.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
