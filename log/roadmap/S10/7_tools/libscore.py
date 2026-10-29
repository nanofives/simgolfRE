"""S10.7: library-object identity of every library candidate of one module (7_cand_<key>.tsv, made by cands.py).
~/bin/py -3.12 log/roadmap/S10/7_tools/libscore.py <golf|jgld|sound|Terrain> [--limit N]
Same comparison as log/c3/m36e_lib_match.py (match.score_function on the library member's function object), but the
members of each archive are indexed once. Writes 7_score_<key>.tsv (va, symbol, lib, member, pct, same, n, extent_note)
and one CSV per candidate's best result to log/roadmap/S10/7_lib/<key>/<libtag>/ (never log/diff)."""
import csv, pathlib, re, struct, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(ROOT / "log" / "c3"))
import match  # noqa: E402
import m36e_lib_match as L  # noqa: E402

D2, D5, D0 = "tools/vc6sp2/VC98/lib/", "tools/vc6sp5/VC98/lib/", "tools/vc6/vc98/lib/"
DBG = [("sp2_cmtd", D2 + "lib/libcmtd.lib"), ("rtm_cmtd", D2 + "LIBCMTD.LIB"), ("sp2_cimtd", D2 + "LIBCIMTD.LIB"),
       ("sp2_cpmtd", D2 + "LIBCPMTD.LIB"),
       ("sp5_cmtd", D5 + "LIBCMTD.LIB"), ("sp5_cimtd", D5 + "LIBCIMTD.LIB")]
REL = [("sp5_cmt", D5 + "LIBCMT.LIB"), ("sp5_cimt", D5 + "LIBCIMT.LIB"), ("sp5_cpmt", D5 + "LIBCPMT.LIB"),
       ("rtm_cmt", D0 + "libcmt.lib"), ("rtm_cimt", D0 + "libcimt.lib"), ("rtm_cpmt", D0 + "libcpmt.lib"),
       ("sp2_cmt", D2 + "lib/libcmt.lib")]
LIBS = {"jgld": DBG, "Terrain": DBG, "sound": REL, "golf": REL[3:6] + REL[:3] + REL[6:]}
LIBS_PATH = {k: dict(v) for k, v in LIBS.items()}
MODS = {"golf": "golf_clean.exe", "jgld": "jgld.dll", "sound": "sound.dll", "Terrain": "Terrain.dll"}


def index(path):
    """symbol -> (member, secs, syms) for every external function symbol of an archive."""
    idx, static = {}, {}
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="s107_")) / "m.obj"
    for name, data in L.members((ROOT / path).read_bytes()):
        if name in ("/", "//") or len(data) < 20 or struct.unpack_from("<H", data, 0)[0] != 0x14c:
            continue
        tmp.write_bytes(data)
        secs, syms = match.coff(tmp)
        raw = match.coff(tmp)[1]            # untouched copy: C static functions (cls 3) keep their function type
        for s in raw:
            if s["sec"] > 0 and s["type"] == 0x20 and s["cls"] == 3:
                static.setdefault(s["name"], []).append((name, secs, raw))
        for s in syms:                      # MASM code labels are static function symbols (see m36e_lib_match.py)
            if s["cls"] == 3 and s["type"] == 0x20:
                s["type"] = 0
        for s in syms:
            if s["sec"] > 0 and s["type"] == 0x20 and s["cls"] == 2:
                idx.setdefault(s["name"], (name, secs, syms))
    return idx, static


def write_csv(p, libname, member, mod, full, a, b, acc):
    with p.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["instruction", "library_object", "original", "match"])
        for k in range(max(len(a), len(b))):
            x = a[k] if k < len(a) else ("", "")
            y = b[k] if k < len(b) else ("", "")
            w.writerow([k, x[0], y[0], x[1] == y[1]])
        w.writerow(["meta", f"library={libname} member={member} (library object, no compilation)",
                    f"symbol={full}", f"module={mod}"])
        w.writerow(["VERDICT", "GREEN" if acc == 1 else "RED", len(a), f"{100 * acc:.1f}%"])


def score(mod, va, secs, syms, sym, size=None, whole=False):
    """(acc, a, b, full, same) or None when our function list has no body here; size = a process-local extent."""
    orig = match.boundary_overrides
    if whole:       # the library body runs to the end of its section: other entry labels inside it do not end it
        syms = [dict(s, type=0) if s["name"] != sym and s["type"] == 0x20 else s for s in syms]
    if size:
        key = match.tracker_key(mod, va)
        match.boundary_overrides = lambda m: {**orig(m), **({key: size} if m == mod else {})}
    try:
        acc, a, b, full, same, _, _ = match.score_function(mod, va, secs, syms, sym)
        return acc, a, b, full, same
    except SystemExit:
        return None
    finally:
        match.boundary_overrides = orig


def trim_pad(mod, va, size):
    """size without the trailing int3 / nop fill of the original's extent."""
    d = match.original_bytes(mod, va, size)
    return size - (len(d) - len(d.rstrip(bytes([0xcc, 0x90]))))


def byte_identity(mod, va, secs, osyms, sym):
    """Library body to the end of its section against the original's bytes, relocation sites masked; a relocation to a label
    in the same section must hold va + label + the stored addend. Used where jump tables sit inside the body (memcpy /
    memmove) and match.py's table check reads the whole tail as one table. Returns (a, b, full) block rows or None."""
    syms = [dict(s, type=0) if s["name"] != sym and s["type"] == 0x20 else s for s in osyms]
    try:
        mine, relocs, full, local = match.function_from_obj(secs, syms, sym)
        orig = match.original_bytes(mod, va, len(mine))
    except SystemExit:
        return None
    mask = {i for r in list(relocs) + list(local) for i in range(r, r + 4)}
    if any(i not in mask and mine[i] != orig[i] for i in range(len(mine))):
        return None
    for site, target in local.items():
        addend = struct.unpack_from("<i", mine, site)[0]
        if (va + target + addend) & 0xffffffff != int.from_bytes(orig[site:site + 4], "little"):
            return None
    rows = []
    for o in range(0, len(mine), 16):
        blk = "".join("??" if o + k in mask else f"{mine[o + k]:02x}" for k in range(min(16, len(mine) - o)))
        rows.append((f"bytes +0x{o:x}: {blk}", blk))
    return rows, rows, full, len(mine)


def variants(mod, va, secs, osyms, sym, psize):
    """(our extent, library body to section end, label): ours as listed first, then ours without padding, the PDB's
    extent (and without padding), the extent of the library's own code without its jump tables, and the library body widened
    to the end of its section (an alias entry label inside it ends it otherwise)."""
    ours = match.ghidra_size(mod, va)
    out, seen = [(None, False, "")], {ours}
    try:
        mine, _, _, local = match.function_from_obj(secs, osyms, sym)
        cut = len(match.strip_switch_tables(mine, local))
    except SystemExit:
        cut = 0
    for size, label in ((ours and trim_pad(mod, va, ours), "trim-pad"), (psize, "pdb-size"),
                        (psize and trim_pad(mod, va, psize), "pdb-size trim-pad"), (cut, "lib-code-size"),
                        (cut and trim_pad(mod, va, cut), "lib-code-size trim-pad")):
        if size and size not in seen:
            seen.add(size)
            out.append((size, False, label))
    out.append((None, True, "lib-to-section-end"))
    out.append((ours and trim_pad(mod, va, ours), True, "trim-pad lib-to-section-end"))
    return out


def best_acc(mod, va, secs, osyms, sym, psize):
    """Best match fraction over every extent variant, 1.0 when the bytes are identical outside the relocation sites."""
    best = 0.0
    for size, whole, _ in variants(mod, va, secs, osyms, sym, psize):
        r = score(mod, va, secs, osyms, sym, size, whole)
        if r:
            best = max(best, r[0])
        if best == 1:
            return 1.0
    return 1.0 if byte_identity(mod, va, secs, osyms, sym) else best


def covered(mod, va, secs, osyms, sym, size, whole):
    """Bytes of the original an extent variant compares."""
    if size:
        return size
    if whole:
        syms = [dict(s, type=0) if s["name"] != sym and s["type"] == 0x20 else s for s in osyms]
        return len(match.function_from_obj(secs, syms, sym)[0])
    return match.ghidra_size(mod, va) or 0


def hits(idxs, syms):
    """(sym, libtag, path, member, secs, syms) for every external definition, then every C static of that name."""
    for sym in syms:
        for tag, path, (ext, static) in idxs:
            for member, secs, osyms in ([ext[sym]] if sym in ext else []) + static.get(sym, []):
                yield sym, tag, path, member, secs, osyms


def main():
    key = sys.argv[1]
    limit = int(sys.argv[sys.argv.index("--limit") + 1]) if "--limit" in sys.argv else 0
    mod = MODS[key]
    idxs = [(tag, path, index(path)) for tag, path in LIBS[key]]
    print("indexed", [(t, len(i[0]), len(i[1])) for t, _, i in idxs], flush=True)
    cand = [l.split("\t") for l in (ROOT / f"log/roadmap/S10/7_cand_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]]
    out_dir = ROOT / f"log/roadmap/S10/7_lib/{key}"
    rows = []
    for c in cand[:limit or None]:
        va, syms, src, psize = int(c[0], 16), [s for s in c[1].split(",") if s], c[2], int(c[3] or 0)
        if not syms and " | " in c[4]:       # no public at the address: the proc name, decorated the C way
            nm = c[4].split(" | ")[0]
            syms = ["_" + nm, nm, "__" + nm, "___" + nm]
            for _, _, (ext, static) in idxs:      # static stdcall functions are decorated _name@N
                syms += [k for k in list(ext) + list(static) if re.fullmatch("_?_?" + re.escape(nm) + r"@\d+", k)]
        best = None                  # (acc, covered bytes, tag, member, sym, same, n, label, a, b, full, path)
        for sym, tag, path, member, secs, osyms in hits(idxs, syms):
            cands = []
            for size, whole, label in variants(mod, va, secs, osyms, sym, psize):
                r = score(mod, va, secs, osyms, sym, size, whole)
                if r:
                    cands.append((r[0], covered(mod, va, secs, osyms, sym, size, whole), label, r))
            for acc, cov, label, (_, a, b, full, same) in cands:
                if best is None or (acc, cov) > best[:2]:
                    best = (acc, cov, tag, member, sym, same, max(len(a), len(b)), label, a, b, full, path)
            r = byte_identity(mod, va, secs, osyms, sym)
            if r and (best is None or (1.0, r[3]) > best[:2]):
                best = (1.0, r[3], tag, member, sym, len(r[0]), len(r[0]), "byte-identity (relocation sites masked)", r[0], r[1], r[2], path)
            if best and best[0] == 1:
                break
        if best:
            best = best[:1] + best[2:]

        if best is None:
            rows.append(f"0x{va:08x}\t{c[1]}\t{src}\t-\t-\t0.0\t0\t0\tno member defines it or no body in our list")
        else:
            acc, tag, member, sym, same, n, ext, a, b, full, path = best
            p = out_dir / tag
            p.mkdir(parents=True, exist_ok=True)
            fn = f"{match.tracker_key(mod, va):08x}_{key + '_' if key != 'golf' else ''}{re.sub(r'[^A-Za-z0-9_]', '_', sym)}.match.csv"
            write_csv(p / fn, path, member, mod, full, a, b, acc)
            rows.append(f"0x{va:08x}\t{sym}\t{src}\t{tag}\t{member}\t{100 * acc:.1f}\t{same}\t{n}\t{ext}")
        print(rows[-1], flush=True)
    (ROOT / f"log/roadmap/S10/7_score_{key}.tsv").write_text(
        "va\tsymbol\tsource\tlib\tmember\tpct\tsame\tn\tnote\n" + "\n".join(rows) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
