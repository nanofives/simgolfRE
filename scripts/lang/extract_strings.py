"""P24D: the English texts of the user's own exe (the string pools the port reads, port/include/portsim/execonst_desc.inc rows
of kind string_pool) as a TSV for translators: `va<TAB>pool<TAB>text` (UTF-8, \\n \\t \\\\ escaped). The output holds game
text, so it goes to the gitignored lang\\ folder, never into the repository.

    ~/bin/py -3.12 scripts/lang/extract_strings.py [--exe original/golf_clean.exe] [--out lang/source/exe_strings.tsv]"""
from __future__ import annotations

import argparse
import pathlib
import re
import struct

ROOT = pathlib.Path(__file__).resolve().parents[2]
DESC = ROOT / "port" / "include" / "portsim" / "execonst_desc.inc"
ROW = re.compile(r'SG_EXE_CONST\((\w+), (\d+)u, (\d+)u, "([^"]*)", (\d+)u, (\d+)u, "(\w+)"')


def file_to_va(pe: bytes, off: int) -> int:
    e = struct.unpack_from("<I", pe, 0x3C)[0]
    nsec = struct.unpack_from("<H", pe, e + 6)[0]
    opt = struct.unpack_from("<H", pe, e + 20)[0]
    base = struct.unpack_from("<I", pe, e + 24 + 28)[0]
    s = e + 24 + opt
    for i in range(nsec):
        vsz, va, rsz, raw = struct.unpack_from("<IIII", pe, s + 40 * i + 8)
        if raw <= off < raw + rsz:
            return base + va + (off - raw)
    return 0


def esc(s: str) -> str:
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=str(ROOT / "original" / "golf_clean.exe"))
    ap.add_argument("--out", default=str(ROOT / "lang" / "source" / "exe_strings.tsv"))
    a = ap.parse_args()
    pe = pathlib.Path(a.exe).read_bytes()
    out = pathlib.Path(a.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    n = 0
    seen = set()
    with out.open("w", encoding="utf-8", newline="\n") as f:
        f.write("# va\tpool\ttext (the exe's string pools; generated, game text: never commit)\n")
        for line in DESC.read_text().splitlines():
            m = ROW.match(line)
            if not m or m.group(7) != "string_pool":
                continue
            name, off, size = m.group(1), int(m.group(2)), int(m.group(3))
            blob = pe[off:off + size]
            i = 0
            while i < len(blob):
                j = blob.find(b"\0", i)
                if j < 0:
                    j = len(blob)
                s = blob[i:j]
                if len(s) >= 2 and re.search(rb"[A-Za-z]{2}", s):
                    va = file_to_va(pe, off + i)
                    f.write(f"{va:#x}\t{name}\t{esc(s.decode('cp1252', 'replace'))}\n")
                    seen.add(va)
                    n += 1
                i = j + 1
        # every other NUL-terminated printable run of .rdata / .data (tables the pool rows do not cover: months, hole types)
        e = struct.unpack_from("<I", pe, 0x3C)[0]
        nsec = struct.unpack_from("<H", pe, e + 6)[0]
        opt = struct.unpack_from("<H", pe, e + 20)[0]
        for k in range(nsec):
            sh = e + 24 + opt + 40 * k
            sname = pe[sh:sh + 8].rstrip(b"\0")
            if sname not in (b".rdata", b".data"):
                continue
            rsz, raw = struct.unpack_from("<II", pe, sh + 16)
            for m in re.finditer(rb"[\x20-\x7e\x91-\x97\xa0-\xff\t\n\r]{3,}(?=\0)", pe[raw:raw + rsz]):
                off = raw + m.start()
                va = file_to_va(pe, off)
                if va in seen or not re.search(rb"[A-Za-z]{2}", m.group(0)) or (m.start() > 0 and pe[off - 1] != 0):
                    continue
                f.write(f"{va:#x}\t{sname.decode()}\t{esc(m.group(0).decode('cp1252', 'replace'))}\n")
                seen.add(va)
                n += 1
    print(f"{n} strings -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
