"""S10.7 negative control: one flipped byte in the original's body must turn an identical function RED.
~/bin/py -3.12 log/roadmap/S10/7_tools/control.py > log/roadmap/S10/7_control.log
For each sampled identical row (every 25th of each module, plus every byte-identity row), the original's bytes come from a patched
match.original_bytes with the first byte of the body XOR 0x01 (an opcode byte, never inside a relocated field); the row is RED when no
variant and no byte identity reaches 100%."""
import pathlib, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libscore as S  # noqa: E402

orig = S.match.original_bytes
red = total = 0
for key, mod in S.MODS.items():
    idx = {t: S.index(p) for t, p in S.LIBS[key]}
    psz = {int(l.split("\t")[0], 16): int(l.split("\t")[3] or 0) for l in (S.ROOT / f"log/roadmap/S10/7_cand_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]}
    rows = [l.split("\t") for l in (S.ROOT / f"log/roadmap/S10/7_score_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:] if float(l.split("\t")[5]) == 100.0]
    for n, c in enumerate(rows):
        if n % 25 and not c[8].startswith("byte-identity"):
            continue
        va, sym, tag = int(c[0], 16), c[1], c[3]
        ext, static = idx[tag]
        member, secs, osyms = ([ext[sym]] if sym in ext else static[sym])[0]
        flip = {"va": va}

        def patched(m, a, size, _o=orig, _f=flip):
            d = bytearray(_o(m, a, size))
            if m == mod and a == _f["va"] and size >= 1:
                d[0] ^= 1
            return bytes(d)
        S.match.original_bytes = patched
        try:
            after = S.best_acc(mod, va, secs, osyms, sym, psz.get(va, 0))
        finally:
            S.match.original_bytes = orig
        total += 1
        red += after < 1
        print(f"{mod}\t0x{va:08x}\t{sym}\t{'RED' if after < 1 else 'STILL 100%'}\t{100 * after:.1f}%", flush=True)
print(f"# {red}/{total} RED", flush=True)
