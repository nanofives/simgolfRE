"""S10.9: slot -> SMSG method names over every vtable SMSG's publics name (??_7...), read from OUR image.
slots(key) -> {slot_index: {method names}}, tables(key) -> {table_va: (public, [body_va per slot])}."""
import pathlib, struct, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import pointer_callers as pc  # noqa: E402

MODS = {"sound": "sound.dll", "jgld": "jgld.dll", "Terrain": "Terrain.dll"}


def procs(key):
    out = {}
    for l in (ROOT / f"log/roadmap/S10/pdb/{key}.procs.tsv").read_text(encoding="utf-8").splitlines():
        c = l.split("\t")
        if len(c) >= 3 and c[0].startswith("0x"):
            out[int(c[0], 16) + 0x10000000] = (c[2], int(c[1]))
    return out


def tables(key):
    m = pc.mod(MODS[key])
    rev = {t: b for b, ts in m.thunks.items() for t in ts}
    out = {}
    for l in (ROOT / f"log/roadmap/S10/pdb/{key}.publics.tsv").read_text(encoding="utf-8").splitlines():
        c = l.split("\t")
        if len(c) < 2 or not c[0].startswith("0x") or not c[1].startswith("??_7"):
            continue
        va, slots = int(c[0], 16) + m.base, []
        while True:
            d = m.dword(va + 4 * len(slots))
            if d is None or not m.in_code(d):
                break
            slots.append(rev.get(d, d))
        out[int(c[0], 16) + m.base] = (c[1], slots)
    return out


def slots(key):
    p, idx = procs(key), {}
    for pub, sl in tables(key).values():
        for k, b in enumerate(sl):
            if b in p:
                idx.setdefault(k, set()).add((p[b][0].split("::")[-1], b, p[b][1]))
    return idx


if __name__ == "__main__":
    t = tables(sys.argv[1])
    print(len(t), "tables;", sum(len(s) for _, s in t.values()), "slots")
