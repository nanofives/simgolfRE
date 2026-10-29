"""One MATCH line per address across re/match/smsg_*.cpp (S10.2-4).

smsg_import.py skips only addresses held by NON-smsg files, so an address that the linker folded (ICF: empty `ret`,
`return 0`, ...) gets a MATCH line in every smsg file that has a procedure there. All read 100 % (same code), but the rule is
one canonical line per address. This pass keeps the line in the first file by name and turns the others into
`// ALSO 100%, already in <file>:` comments. Run after a bulk run, before --evidence.

usage: smsg_dedupe.py <prefix, e.g. smsg_golf_> [--write]      (default: list only; only files with that prefix change)
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
PROTECT = {"smsg_golf_Geometry.cpp", "smsg_golf_Plot.cpp", "smsg_golf_InputPoll.cpp"}     # S10.1 files: first holders, never rewritten
LINE = re.compile(r"^// MATCH:\s*(\S+)\s+(0x[0-9a-fA-F]+)\s+(\S+)(.*)$")


def main(argv):
    write = "--write" in argv
    prefix = next(a for a in argv if not a.startswith("--"))
    files = sorted((ROOT / "re" / "match").glob("smsg_*.cpp"), key=lambda f: (f.name not in PROTECT, f.name))
    holder: dict[tuple[str, int], str] = {}
    for f in (ROOT / "re" / "match").glob("*.cpp"):         # non-smsg files win outright
        if not f.name.startswith("smsg_"):
            for m in re.finditer(r"^// MATCH:\s*(\S+)\s+(0x[0-9a-fA-F]+)", f.read_text(errors="replace"), re.M):
                holder.setdefault((m.group(1), int(m.group(2), 16)), f.name)
    changed = 0
    for f in files:
        if not f.name.startswith(prefix):
            continue
        lines = f.read_text().splitlines()
        for i, ln in enumerate(lines):
            m = LINE.match(ln)
            if not m:
                continue
            key = (m.group(1), int(m.group(2), 16))
            if key in holder and holder[key] != f.name:
                print(f"{f.name}: {m.group(1)} {m.group(2)} kept in {holder[key]}")
                lines[i] = f"// ALSO 100%, already in {holder[key]}: {m.group(1)} {m.group(2)} {m.group(3)}"
                changed += 1
            else:
                holder.setdefault(key, f.name)
        if write:
            if not any(LINE.match(x) for x in lines):     # nothing canonical left: the match tier wants a MATCH line
                f.unlink()
                print(f"{f.name}: no MATCH line left, file removed")
                continue
            f.write_text("\n".join(lines) + "\n")
    print(f"{changed} duplicate lines" + (" rewritten" if write else " (dry run)"))


if __name__ == "__main__":
    main(sys.argv[1:])
